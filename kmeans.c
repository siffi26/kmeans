/*

ASSUMPTIONS:

- data points are 2D
- data points are positive integers
- data points are sorted

*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

//#define DEBUG
#define BIG_NUM          999999999999999999
#define LINE_SIZE        80
#define FALSE            0
#define TRUE             1

#define uint             unsigned int
#define uint64           unsigned long
#define datax(i)         data_points[i * 2]
#define datay(i)         data_points[i * 2 + 1]
#define distance(i, j)   (datax(j) - datax(i)) * (datax(j) - datax(i)) + (datay(j) - datay(i)) * (datay(j) - datay(i))
#define is_assigned(i)   point_assignments[i].assigned
#define set_assigned(i)  point_assignments[i].assigned = TRUE
#define assign(i, c)     point_assignments[i].centroid_id = c
#define assigned_to(i)   point_assignments[i].centroid_id


typedef struct {
  uint    point_id;
  uint    num_points;
  uint64  sum_x;
  uint64  sum_y;

  // for calculating new centroid
  double  target_x;
  double  target_y;
  uint64  prev_distance;
  uint64  best_distance;
} centroid;

typedef struct {
  uint  centroid_id;
  uint  distance;
  char  assigned;
} assignment;

char        *input_file_name;
uint        num_points;
uint        num_clusters;
uint        max_iterations = 10;
centroid    *centroids;
uint        *data_points;
assignment  *point_assignments;
uint        improved = 0;
float       improvement = 0.0;


void setup_data_points();
void setup_centroids();
void setup_assignments();

void update_assignments();
void update_centroids();
void dump_state();
void dbg(const char *fmt, ...);

int main(int argc, char **argv)
{
  uint      i, a, b, d, x, y;
  float     average_improvement;
  centroid  *cp;

  // usage: ./kmeans /path/to/file num_points num_clusters max_iterations
  input_file_name = argv[1];
  num_points = atoi(argv[2]);
  num_clusters = atoi(argv[3]);
  max_iterations = atoi(argv[4]);
  dbg("file: %s, num_points: %d, num_clusters: %d\n\n", argv[1], num_points, num_clusters);

  setup_data_points();
  setup_assignments();
  setup_centroids();

  dump_state();

  for (uint i = 0; i < max_iterations; i++) {
    dbg("================ ITERATION %d ================\n\n", i);
    update_assignments();
    update_centroids();
    if (improved) {
      average_improvement = improvement / improved;
      dbg("average improvement: %.1f\n\n", 100.0 * average_improvement);

      // done if average improvement is less than 0.1%
      if (average_improvement < 0.001)
        break;
    }
  }

  dbg("================ DONE! ================\n\n");
  dump_state();

  // dump final centroids
  // format: x, y, num points, distance to mean
  for (i = 0, cp = centroids; i < num_clusters; i++, cp++) {
    // calculate distance to mean
    a = cp->sum_x / cp->num_points;
    b = cp->sum_y / cp->num_points;
    x = datax(cp->point_id);
    y = datay(cp->point_id);
    d = (a - x) * (a - x) + (b - y) * (b - y);

    printf("%d\t%d\t%d\t%.1lf\n", x, y, cp->num_points, sqrt((double)d));
  }

  return 0;
}

/**
 * assign data points to current centroids
 */
void update_assignments()
{
  uint      i, j, d;
  uint      nearest_centroid;
  uint64    min_dist;
  centroid  *cp, *cp_last;

  dbg("*** assigning points to centroids...\n\n");

  // clear out centroids
  for (cp = centroids, cp_last = centroids + num_clusters; cp < cp_last; cp++) {
    cp->num_points = 0;
    cp->sum_x = 0;
    cp->sum_y = 0;
  }

  // for each data point...
  for (i = 0; i < num_points; i++) {
    // find nearest centroid
    for (j = 0; j < num_clusters; j++) {
      d = distance(i, centroids[j].point_id);
      if (j == 0 || min_dist > d) {
        min_dist = d;
        nearest_centroid = j;
      }
    }

    // assign point to centroid
    point_assignments[i].centroid_id = nearest_centroid;
    point_assignments[i].distance = min_dist;

    // update centroid stats
    centroids[nearest_centroid].sum_x += datax(i);
    centroids[nearest_centroid].sum_y += datay(i);
    centroids[nearest_centroid].num_points++;
  }

  dump_state();
}

/**
 * pick new centroids based on mean coordinates in the cluster
 */
void update_centroids()
{
  uint        i, a, b;
  uint        *dp;
  uint64      d;
  float       k;
  assignment  *ap;
  centroid    *cp, *cp_last;

  dbg("*** updating centroids...\n\n");

  // initialize
  for (cp = centroids, cp_last = centroids + num_clusters; cp < cp_last; cp++) {
    cp->target_x = cp->sum_x / cp->num_points;
    cp->target_y = cp->sum_y / cp->num_points;
    cp->prev_distance = cp->best_distance;
    cp->best_distance = BIG_NUM;
  }

  #ifdef DEBUG
  for (i = 0, cp = centroids, cp_last = centroids + num_clusters; cp < cp_last; i++, cp++) {
    a = datax(cp->point_id) - cp->target_x;
    b = datay(cp->point_id) - cp->target_y;
    dbg("old centroid %d (%d, %d), target = (%.1lf, %.1lf), distance = %d\n", i, datax(cp->point_id), datay(cp->point_id), cp->target_x, cp->target_y, a * a + b * b);
  }
  #endif

  // go through points; for each respective centroid, find the
  // point closest to the target new centroid
  for (i = 0, ap = point_assignments, dp = data_points; i < num_points; i++, ap++, dp += 2) {
    // get my centroid
    cp = centroids + ap->centroid_id;

    // calculate distance to target
    a = *dp - cp->target_x;
    b = *(dp + 1) - cp->target_y;
    d = a * a + b * b;

    // new closest point?
    if (cp->best_distance > d) {
      cp->best_distance = d;
      cp->point_id = i;
    }
  }

  // check improvement
  improved = 0;
  improvement = 0.0;
  for (i = 0, cp = centroids; i < num_clusters; i++, cp++) {
    dbg("new centroid %d (%d, %d), distance: %ld => %ld", i, datax(cp->point_id), datay(cp->point_id), cp->prev_distance, cp->best_distance);
    if (cp->prev_distance) {
      k = ((float)cp->best_distance - (float)cp->prev_distance) / (float)cp->prev_distance;
      improvement += fabs(k);
      improved++;
      dbg(" (%.3f%%)", 100.0 * k);
    }
    dbg("\n");
  }
  dbg("\n");

  dump_state();
}

/**
 * allocate the array for data points and read them in from the input file
 */
void setup_data_points()
{
  FILE *file;
  char line[LINE_SIZE];

  // allocate data array
  data_points = (uint *)calloc(num_points * 2, sizeof(uint));

  // read data from file into array
  file = fopen(input_file_name, "rt");
  for (uint i = 0; i < num_points; i++) {
    fgets(line, LINE_SIZE, file);
    sscanf(line, "%d\t%d", &datax(i), &datay(i));
  }
  fclose(file);
}

/**
 * allocate the array for data point to centroid assignments
 */
void setup_assignments()
{
  point_assignments = (assignment *)calloc(num_points, sizeof(assignment));
}

/**
 * allocate the array for centroids, then pick the initial centroids
 */
void setup_centroids()
{
  uint i, j, k, x, y;

  // allocate centroids array
  centroids = (centroid *)calloc(num_clusters, sizeof(centroid));

  // pick starting centroids, ensuring uniqueness
  srand((unsigned)time(NULL));
  for (i = 0; i < num_clusters; ) {
    // grab a random data point
    j = random() % num_points;

    // take if first one or unassigned
    if (i == 0 || !is_assigned(j)) {
      centroids[i].point_id = j;

      // mark this point (and all points like it) as assigned;
      // this traversal works because we know the data set is sorted
      x = datax(j);
      y = datay(j);
      for (k = j; k < num_points && x == datax(k) && y == datay(k); k++)
        set_assigned(k);
      for (k = j; k-- > 0 && x == datax(k) && y == datay(k); )
        set_assigned(k);

      // on to next centroid
      i++;
    }
  }

  // it's possible that the actual number of clusters is less than asked for
  if (i < num_clusters)
    num_clusters = i;
}

/**
 * dump the state of things: centroids with assigned data points
 */
void dump_state()
{
#ifdef DEBUG
  uint        i, j, k, pid;
  assignment  *ap;
  centroid    *cp;

  #if 0
  // data points
  for (uint i = 0, *dp = data_points; i < num_points; i++, dp += 2)
    dbg("data[%d]: %d, %d\n", i, *dp, *(dp + 1));
  #endif

  // centroids
  for (i = 0, cp = centroids; i < num_clusters; i++, cp++) {
    pid = cp->point_id;
    dbg("centroids[%d]: point_id = %d (%d, %d), %d points, sum_x = %ld, sum_y = %ld\n", i, pid, datax(pid), datay(pid), cp->num_points, cp->sum_x, cp->sum_y);
    dbg("centroids[%d]:", i);
    for (j = 0, k = 0, ap = point_assignments; j < num_points; j++, ap++) {
      if (ap->centroid_id == i) {
        if (k && k % 10 == 0)
          dbg("\n             ");
        dbg(" %d,%d", datax(j), datay(j));
        k++;
      }
    }
    dbg("\n");
  }
  dbg("\n");
#endif
}

/**
 * same as printf except does nothing when not in DEBUG mode
 */
void dbg(const char *fmt, ...)
{
#ifdef DEBUG
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
#endif
}
