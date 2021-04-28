/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* glitter-paths - polygon scan converter
 *
 * Copyright (c) 2008  M Joonas Pihlaja
 * Copyright (c) 2007  David Turner
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/* This is the Glitter paths scan converter incorporated into cairo.
 * The source is from commit 734c53237a867a773640bd5b64816249fa1730f8
 * of
 *
 *   http://gitweb.freedesktop.org/?p=users/joonas/glitter-paths
 */
/* Glitter-paths is a stand alone polygon rasteriser derived from
 * David Turner's reimplementation of Tor Anderssons's 15x17
 * supersampling rasteriser from the Apparition graphics library.  The
 * main new feature here is cheaply choosing per-scan line between
 * doing fully analytical coverage computation for an entire row at a
 * time vs. using a supersampling approach.
 *
 * David Turner's code can be found at
 *
 *   http://david.freetype.org/rasterizer-shootout/raster-comparison-20070813.tar.bz2
 *
 * In particular this file incorporates large parts of ftgrays_tor10.h
 * from raster-comparison-20070813.tar.bz2
 */
/* Overview
 *
 * A scan converter's basic purpose to take polygon edges and convert
 * them into an RLE compressed A8 mask.  This one works in two phases:
 * gathering edges and generating spans.
 *
 * 1) As the user feeds the scan converter edges they are vertically
 * clipped and bucketted into a _polygon_ data structure.  The edges
 * are also snapped from the user's coordinates to the subpixel grid
 * coordinates used during scan conversion.
 *
 *     user
 *      |
 *      | edges
 *      V
 *    polygon buckets
 *
 * 2) Generating spans works by performing a vertical sweep of pixel
 * rows from top to bottom and maintaining an _active_list_ of edges
 * that intersect the row.  From the active list the fill rule
 * determines which edges are the left and right edges of the start of
 * each span, and their contribution is then accumulated into a pixel
 * coverage list (_cell_list_) as coverage deltas.  Once the coverage
 * deltas of all edges are known we can form spans of constant pixel
 * coverage by summing the deltas during a traversal of the cell list.
 * At the end of a pixel row the cell list is sent to a coverage
 * blitter for rendering to some target surface.
 *
 * The pixel coverages are computed by either supersampling the row
 * and box filtering a mono rasterisation, or by computing the exact
 * coverages of edges in the active list.  The supersampling method is
 * used whenever some edge starts or stops within the row or there are
 * edge intersections in the row.
 *
 *   polygon bucket for       \
 *   current pixel row        |
 *      |                     |
 *      | activate new edges  |  Repeat GRID_Y times if we
 *      V                     \  are supersampling this row,
 *   active list              /  or just once if we're computing
 *      |                     |  analytical coverage.
 *      | coverage deltas     |
 *      V                     |
 *   pixel coverage list     /
 *      |
 *      V
 *   coverage blitter
 */
#include "cairoint.h"
#include "cairo-spans-private.h"
#include "cairo-error-private.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/*-------------------------------------------------------------------------
 * cairo specific config
 */
#define I static

/* Prefer cairo's status type. */
#define GLITTER_HAVE_STATUS_T 1
#define GLITTER_STATUS_SUCCESS CAIRO_STATUS_SUCCESS
#define GLITTER_STATUS_NO_MEMORY CAIRO_STATUS_NO_MEMORY
typedef cairo_status_t glitter_status_t;

/* The input coordinate scale and the rasterisation grid scales. */
#define GLITTER_INPUT_BITS CAIRO_FIXED_FRAC_BITS
#define GRID_X_BITS CAIRO_FIXED_FRAC_BITS
#define GRID_Y 15

/* Set glitter up to use a cairo span renderer to do the coverage
 * blitting. */
struct pool;
struct cell_list;

static glitter_status_t
blit_with_span_renderer(
    struct cell_list		*coverages,
    cairo_span_renderer_t	*span_renderer,
    struct pool			*span_pool,
    int				 y,
    int				 height,
    int				 xmin,
    int				 xmax);

static glitter_status_t
blit_empty_with_span_renderer (cairo_span_renderer_t *renderer, int y, int height);

#define GLITTER_BLIT_COVERAGES_ARGS \
	cairo_span_renderer_t *span_renderer, \
	struct pool *span_pool

#define GLITTER_BLIT_COVERAGES(cells, y, height,xmin, xmax) do {	\
    cairo_status_t status = blit_with_span_renderer (cells,		\
						     span_renderer,	\
						     span_pool,		\
						     y, height,		\
						     xmin, xmax);	\
    if (unlikely (status))						\
	return status;							\
} while (0)

#define GLITTER_BLIT_COVERAGES_EMPTY(y, height, xmin, xmax) do {		\
    cairo_status_t status = blit_empty_with_span_renderer (span_renderer, y, height); \
    if (unlikely (status))						\
	return status;							\
} while (0)

/*-------------------------------------------------------------------------
 * glitter-paths.h
 */

/* "Input scaled" numbers are fixed precision reals with multiplier
 * 2**GLITTER_INPUT_BITS.  Input coordinates are given to glitter as
 * pixel scaled numbers.  These get converted to the internal grid
 * scaled numbers as soon as possible. Internal overflow is possible
 * if GRID_X/Y inside glitter-paths.c is larger than
 * 1<<GLITTER_INPUT_BITS. */
#ifndef GLITTER_INPUT_BITS
#  define GLITTER_INPUT_BITS 8
#endif
#define GLITTER_INPUT_SCALE (1<<GLITTER_INPUT_BITS)
typedef int glitter_input_scaled_t;

#if !GLITTER_HAVE_STATUS_T
typedef enum {
    GLITTER_STATUS_SUCCESS = 0,
    GLITTER_STATUS_NO_MEMORY
} glitter_status_t;
#endif

#ifndef I
# define I /*static*/
#endif

/* Opaque type for scan converting. */
typedef struct glitter_scan_converter glitter_scan_converter_t;

/* Reset a scan converter to accept polygon edges and set the clip box
 * in pixels.  Allocates O(ymax-ymin) bytes of memory.	The clip box
 * is set to integer pixel coordinates xmin <= x < xmax, ymin <= y <
 * ymax. */
I glitter_status_t
glitter_scan_converter_reset(
    glitter_scan_converter_t *converter,
    int xmin, int ymin,
    int xmax, int ymax);

/* Add a new polygon edge from pixel (x1,y1) to (x2,y2) to the scan
 * converter.  The coordinates represent pixel positions scaled by
 * 2**GLITTER_PIXEL_BITS.  If this function fails then the scan
 * converter should be reset or destroyed.  Dir must be +1 or -1,
 * with the latter reversing the orientation of the edge. */
I glitter_status_t
glitter_scan_converter_add_edge (glitter_scan_converter_t *converter,
				 const cairo_edge_t *edge);

/* Render the polygon in the scan converter to the given A8 format
 * image raster.  Only the pixels accessible as pixels[y*stride+x] for
 * x,y inside the clip box are written to, where xmin <= x < xmax,
 * ymin <= y < ymax.  The image is assumed to be clear on input.
 *
 * If nonzero_fill is true then the interior of the polygon is
 * computed with the non-zero fill rule.  Otherwise the even-odd fill
 * rule is used.
 *
 * The scan converter must be reset or destroyed after this call. */
#ifndef GLITTER_BLIT_COVERAGES_ARGS
# define GLITTER_BLIT_COVERAGES_ARGS unsigned char *raster_pixels, long raster_stride
#endif
I glitter_status_t
glitter_scan_converter_render(
    glitter_scan_converter_t *converter,
    int nonzero_fill,
    GLITTER_BLIT_COVERAGES_ARGS);

/*-------------------------------------------------------------------------
 * glitter-paths.c: Implementation internal types
 */
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* All polygon coordinates are snapped onto a subsample grid. "Grid
 * scaled" numbers are fixed precision reals with multiplier GRID_X or
 * GRID_Y. */
typedef int grid_scaled_t;
typedef int grid_scaled_x_t;
typedef int grid_scaled_y_t;

/* Default x/y scale factors.
 *  You can either define GRID_X/Y_BITS to get a power-of-two scale
 *  or define GRID_X/Y separately. */
#if !defined(GRID_X) && !defined(GRID_X_BITS)
#  define GRID_X_BITS 8
#endif
#if !defined(GRID_Y) && !defined(GRID_Y_BITS)
#  define GRID_Y 15
#endif

/* Use GRID_X/Y_BITS to define GRID_X/Y if they're available. */
#ifdef GRID_X_BITS
#  define GRID_X (1 << GRID_X_BITS)
#endif
#ifdef GRID_Y_BITS
#  define GRID_Y (1 << GRID_Y_BITS)
#endif

/* The GRID_X_TO_INT_FRAC macro splits a grid scaled coordinate into
 * integer and fractional parts. The integer part is floored. */
#if defined(GRID_X_TO_INT_FRAC)
  /* do nothing */
#elif defined(GRID_X_BITS)
#  define GRID_X_TO_INT_FRAC(x, i, f) \
	_GRID_TO_INT_FRAC_shift(x, i, f, GRID_X_BITS)
#else
#  define GRID_X_TO_INT_FRAC(x, i, f) \
	_GRID_TO_INT_FRAC_general(x, i, f, GRID_X)
#endif

#define _GRID_TO_INT_FRAC_general(t, i, f, m) do {	\
    (i) = (t) / (m);					\
    (f) = (t) % (m);					\
    if ((f) < 0) {					\
	--(i);						\
	(f) += (m);					\
    }							\
} while (0)

#define _GRID_TO_INT_FRAC_shift(t, i, f, b) do {	\
    (f) = (t) & ((1 << (b)) - 1);			\
    (i) = (t) >> (b);					\
} while (0)

/* A grid area is a real in [0,1] scaled by 2*GRID_X*GRID_Y.  We want
 * to be able to represent exactly areas of subpixel trapezoids whose
 * vertices are given in grid scaled coordinates.  The scale factor
 * comes from needing to accurately represent the area 0.5*dx*dy of a
 * triangle with base dx and height dy in grid scaled numbers. */
typedef int grid_area_t;
#define GRID_XY (2*GRID_X*GRID_Y) /* Unit area on the grid. */

/* GRID_AREA_TO_ALPHA(area): map [0,GRID_XY] to [0,255]. */
#if GRID_XY == 510
#  define GRID_AREA_TO_ALPHA(c)	  (((c)+1) >> 1)
#elif GRID_XY == 255
#  define  GRID_AREA_TO_ALPHA(c)  (c)
#elif GRID_XY == 64
#  define  GRID_AREA_TO_ALPHA(c)  (((c) << 2) | -(((c) & 0x40) >> 6))
#elif GRID_XY == 128
#  define  GRID_AREA_TO_ALPHA(c)  ((((c) << 1) | -((c) >> 7)) & 255)
#elif GRID_XY == 256
#  define  GRID_AREA_TO_ALPHA(c)  (((c) | -((c) >> 8)) & 255)
#elif GRID_XY == 15
#  define  GRID_AREA_TO_ALPHA(c)  (((c) << 4) + (c))
#elif GRID_XY == 2*256*15
#  define  GRID_AREA_TO_ALPHA(c)  (((c) + ((c)<<4) + 256) >> 9)
#else
#  define  GRID_AREA_TO_ALPHA(c)  (((c)*255 + GRID_XY/2) / GRID_XY)
#endif

#define UNROLL3(x) x x x

struct quorem {
    int32_t quo;
    int32_t rem;
};

/* Header for a chunk of memory in a memory pool. */
struct _pool_chunk {
    /* # bytes used in this chunk. */
    size_t size;

    /* # bytes total in this chunk */
    size_t capacity;

    /* Pointer to the previous chunk or %NULL if this is the sentinel
     * chunk in the pool header. */
    struct _pool_chunk *prev_chunk;

    /* Actual data starts here.	 Well aligned for pointers. */
};

/* A memory pool.  This is supposed to be embedded on the stack or
 * within some other structure.	 It may optionally be followed by an
 * embedded array from which requests are fulfilled until
 * malloc needs to be called to allocate a first real chunk. */
struct pool {
    /* Chunk we're allocating from. */
    struct _pool_chunk *current;

    /* Free list of previously allocated chunks.  All have >= default
     * capacity. */
    struct _pool_chunk *first_free;

    /* The default capacity of a chunk. */
    size_t default_capacity;

    /* Header for the sentinel chunk.  Directly following the pool
     * struct should be some space for embedded elements from which
     * the sentinel chunk allocates from. */
    struct _pool_chunk sentinel[1];
};

/* A polygon edge. */
struct edge {
    /* Next in y-bucket or active list. */
    struct edge *next;

    /* Current x coordinate while the edge is on the active
     * list. Initialised to the x coordinate of the top of the
     * edge. The quotient is in grid_scaled_x_t units and the
     * remainder is mod dy in grid_scaled_y_t units.*/
    struct quorem x;

    /* Advance of the current x when moving down a subsample line. */
    struct quorem dxdy;

    /* Advance of the current x when moving down a full pixel
     * row. Only initialised when the height of the edge is large
     * enough that there's a chance the edge could be stepped by a
     * full row's worth of subsample rows at a time. */
    struct quorem dxdy_full;

    /* The clipped y of the top of the edge. */
    grid_scaled_y_t ytop;

    /* y2-y1 after orienting the edge downwards.  */
    grid_scaled_y_t dy;

    /* Number of subsample rows remaining to scan convert of this
     * edge. */
    grid_scaled_y_t height_left;

    /* Original sign of the edge: +1 for downwards, -1 for upwards
     * edges.  */
    int dir;
    int vertical;
};

/* Number of subsample rows per y-bucket. Must be GRID_Y. */
#define EDGE_Y_BUCKET_HEIGHT GRID_Y

#define EDGE_Y_BUCKET_INDEX(y, ymin) (((y) - (ymin))/EDGE_Y_BUCKET_HEIGHT)

struct bucket {
    /* Unsorted list of edges starting within this bucket. */
    struct edge *edges;

    /* Set to non-zero if there are edges starting strictly within the
     * bucket. */
    unsigned     have_inside_edges;
};

/* A collection of sorted and vertically clipped edges of the polygon.
 * Edges are moved from the polygon to an active list while scan
 * converting. */
struct polygon {
    /* The clip extents. */
    grid_scaled_x_t xmin, xmax;
    grid_scaled_y_t ymin, ymax;

    /* Array of edges all starting in the same bucket.	An edge is put
     * into bucket EDGE_BUCKET_INDEX(edge->ytop, polygon->ymin) when
     * it is added to the polygon. */
    struct bucket *y_buckets;
    struct bucket  y_buckets_embedded[64];

    struct {
	struct pool base[1];
	struct edge embedded[32];
    } edge_pool;
};

/* A cell records the effect on pixel coverage of polygon edges
 * passing through a pixel.  It contains two accumulators of pixel
 * coverage.
 *
 * Consider the effects of a polygon edge on the coverage of a pixel
 * it intersects and that of the following one.  The coverage of the
 * following pixel is the height of the edge multiplied by the width
 * of the pixel, and the coverage of the pixel itself is the area of
 * the trapezoid formed by the edge and the right side of the pixel.
 *
 * +-----------------------+-----------------------+
 * |                       |                       |
 * |                       |                       |
 * |_______________________|_______________________|
 * |   \...................|.......................|\
 * |    \..................|.......................| |
 * |     \.................|.......................| |
 * |      \....covered.....|.......................| |
 * |       \....area.......|.......................| } covered height
 * |        \..............|.......................| |
 * |uncovered\.............|.......................| |
 * |  area    \............|.......................| |
 * |___________\...........|.......................|/
 * |                       |                       |
 * |                       |                       |
 * |                       |                       |
 * +-----------------------+-----------------------+
 *
 * Since the coverage of the following pixel will always be a multiple
 * of the width of the pixel, we can store the height of the covered
 * area instead.  The coverage of the pixel itself is the total
 * coverage minus the area of the uncovered area to the left of the
 * edge.  As it's faster to compute the uncovered area we only store
 * that and subtract it from the total coverage later when forming
 * spans to blit.
 *
 * The heights and areas are signed, with left edges of the polygon
 * having positive sign and right edges having negative sign.  When
 * two edges intersect they swap their left/rightness so their
 * contribution above and below the intersection point must be
 * computed separately. */
struct cell {
    struct cell		*next;
    int			 x;
    grid_area_t		 uncovered_area;
    grid_scaled_y_t	 covered_height;
};

/* A cell list represents the scan line sparsely as cells ordered by
 * ascending x.  It is geared towards scanning the cells in order
 * using an internal cursor. */
struct cell_list {
    /* Points to the left-most cell in the scan line. */
    struct cell *head;
    /* Sentinel node */
    struct cell tail;

    /* Cursor state for iterating through the cell list.  Points to
     * a pointer to the current cell: either &cell_list->head or the next
     * field of the previous cell. */
    struct cell **cursor;

    /* Cells in the cell list are owned by the cell list and are
     * allocated from this pool.  */
    struct {
	struct pool base[1];
	struct cell embedded[32];
    } cell_pool;
};

struct cell_pair {
    struct cell *cell1;
    struct cell *cell2;
};

/* The active list contains edges in the current scan line ordered by
 * the x-coordinate of the intercept of the edge and the scan line. */
struct active_list {
    /* Leftmost edge on the current scan line. */
    struct edge *head;

    /* A lower bound on the height of the active edges is used to
     * estimate how soon some active edge ends.	 We can't advance the
     * scan conversion by a full pixel row if an edge ends somewhere
     * within it. */
    grid_scaled_y_t min_height;
};

struct glitter_scan_converter {
    struct polygon	polygon[1];
    struct active_list	active[1];
    struct cell_list	coverages[1];

    /* Clip box. */
    grid_scaled_x_t xmin, xmax;
    grid_scaled_y_t ymin, ymax;
};

/* Compute the floored division a/b. Assumes / and % perform symmetric
 * division. */
inline static struct quorem
floored_divrem(int a, int b)
{
    struct quorem qr;
    qr.quo = a/b;
    qr.rem = a%b;
    if ((a^b)<0 && qr.rem) {
	qr.quo -= 1;
	qr.rem += b;
    }
    return qr;
}

/* Compute the floored division (x*a)/b. Assumes / and % perform symmetric
 * division. */
static struct quorem
floored_muldivrem(int x, int a, int b)
{
    struct quorem qr;
    long long xa = (long long)x*a;
    qr.quo = xa/b;
    qr.rem = xa%b;
    if ((xa>=0) != (b>=0) && qr.rem) {
	qr.quo -= 1;
	qr.rem += b;
    }
    return qr;
}

static void
_pool_chunk_init(
    struct _pool_chunk *p,
    struct _pool_chunk *prev_chunk,
    size_t capacity)
{
    p->prev_chunk = prev_chunk;
    p->size = 0;
    p->capacity = capacity;
}

static struct _pool_chunk *
_pool_chunk_create(
    struct _pool_chunk *prev_chunk,
    size_t size)
{
    struct _pool_chunk *p;
    size_t size_with_head = size + sizeof(struct _pool_chunk);
    if (size_with_head < size)
	return NULL;
    p = malloc(size_with_head);
    if (p)
	_pool_chunk_init(p, prev_chunk, size);
    return p;
}

static void
pool_init(
    struct pool *pool,
    size_t default_capacity,
    size_t embedded_capacity)
{
    pool->current = pool->sentinel;
    pool->first_free = NULL;
    pool->default_capacity = default_capacity;
    _pool_chunk_init(pool->sentinel, NULL, embedded_capacity);
}

static void
pool_fini(struct pool *pool)
{
    struct _pool_chunk *p = pool->current;
    do {
	while (NULL != p) {
	    struct _pool_chunk *prev = p->prev_chunk;
	    if (p != pool->sentinel)
		free(p);
	    p = prev;
	}
	p = pool->first_free;
	pool->first_free = NULL;
    } while (NULL != p);
    pool_init(pool, 0, 0);
}

/* Satisfy an allocation by first allocating a new large enough chunk
 * and adding it to the head of the pool's chunk list. This function
 * is called as a fallback if pool_alloc() couldn't do a quick
 * allocation from the current chunk in the pool. */
static void *
_pool_alloc_from_new_chunk(
    struct pool *pool,
    size_t size)
{
    struct _pool_chunk *chunk;
    void *obj;
    size_t capacity;

    /* If the allocation is smaller than the default chunk size then
     * try getting a chunk off the free list.  Force alloc of a new
     * chunk for large requests. */
    capacity = size;
    chunk = NULL;
    if (size < pool->default_capacity) {
	capacity = pool->default_capacity;
	chunk = pool->first_free;
	if (chunk) {
	    pool->first_free = chunk->prev_chunk;
	    _pool_chunk_init(chunk, pool->current, chunk->capacity);
	}
    }

    if (NULL == chunk) {
	chunk = _pool_chunk_create (pool->current, capacity);
	if (unlikely (NULL == chunk))
	    return NULL;
    }
    pool->current = chunk;

    obj = ((unsigned char*)chunk + sizeof(*chunk) + chunk->size);
    chunk->size += size;
    return obj;
}

/* Allocate size bytes from the pool.  The first allocated address
 * returned from a pool is aligned to sizeof(void*).  Subsequent
 * addresses will maintain alignment as long as multiples of void* are
 * allocated.  Returns the address of a new memory area or %NULL on
 * allocation failures.	 The pool retains ownership of the returned
 * memory. */
inline static void *
pool_alloc (struct pool *pool, size_t size)
{
    struct _pool_chunk *chunk = pool->current;

    if (size <= chunk->capacity - chunk->size) {
	void *obj = ((unsigned char*)chunk + sizeof(*chunk) + chunk->size);
	chunk->size += size;
	return obj;
    } else {
	return _pool_alloc_from_new_chunk(pool, size);
    }
}

/* Relinquish all pool_alloced memory back to the pool. */
static void
pool_reset (struct pool *pool)
{
    /* Transfer all used chunks to the chunk free list. */
    struct _pool_chunk *chunk = pool->current;
    if (chunk != pool->sentinel) {
	while (chunk->prev_chunk != pool->sentinel) {
	    chunk = chunk->prev_chunk;
	}
	chunk->prev_chunk = pool->first_free;
	pool->first_free = pool->current;
    }
    /* Reset the sentinel as the current chunk. */
    pool->current = pool->sentinel;
    pool->sentinel->size = 0;
}

/* Rewinds the cell list's cursor to the beginning.  After rewinding
 * we're good to cell_list_find() the cell any x coordinate. */
inline static void
cell_list_rewind (struct cell_list *cells)
{
    cells->cursor = &cells->head;
}

/* Rewind the cell list if its cursor has been advanced past x. */
inline static void
cell_list_maybe_rewind (struct cell_list *cells, int x)
{
    struct cell *tail = *cells->cursor;
    if (tail->x > x)
	cell_list_rewind (cells);
}

static void
cell_list_init(struct cell_list *cells)
{
    pool_init(cells->cell_pool.base,
	      256*sizeof(struct cell),
	      sizeof(cells->cell_pool.embedded));
    cells->tail.next = NULL;
    cells->tail.x = INT_MAX;
    cells->head = &cells->tail;
    cell_list_rewind (cells);
}

static void
cell_list_fini(struct cell_list *cells)
{
    pool_fini (cells->cell_pool.base);
}

/* Empty the cell list.  This is called at the start of every pixel
 * row. */
inline static void
cell_list_reset (struct cell_list *cells)
{
    cell_list_rewind (cells);
    cells->head = &cells->tail;
    pool_reset (cells->cell_pool.base);
}

static struct cell *
cell_list_alloc (struct cell_list *cells,
		 struct cell **cursor,
		 struct cell *tail,
		 int x)
{
    struct cell *cell;

    cell = pool_alloc (cells->cell_pool.base, sizeof (struct cell));
    if (unlikely (NULL == cell))
	return NULL;

    *cursor = cell;
    cell->next = tail;
    cell->x = x;
    cell->uncovered_area = 0;
    cell->covered_height = 0;
    return cell;
}

/* Find a cell at the given x-coordinate.  Returns %NULL if a new cell
 * needed to be allocated but couldn't be.  Cells must be found with
 * non-decreasing x-coordinate until the cell list is rewound using
 * cell_list_rewind(). Ownership of the returned cell is retained by
 * the cell list. */
inline static struct cell *
cell_list_find (struct cell_list *cells, int x)
{
    struct cell **cursor = cells->cursor;
    struct cell *tail;

    while (1) {
	UNROLL3({
	    tail = *cursor;
	    if (tail->x >= x) {
		break;
	    }
	    cursor = &tail->next;
	});
    }
    cells->cursor = cursor;

    if (tail->x == x)
	return tail;

    return cell_list_alloc (cells, cursor, tail, x);
}

/* Find two cells at x1 and x2.	 This is exactly equivalent
 * to
 *
 *   pair.cell1 = cell_list_find(cells, x1);
 *   pair.cell2 = cell_list_find(cells, x2);
 *
 * except with less function call overhead. */
inline static struct cell_pair
cell_list_find_pair(struct cell_list *cells, int x1, int x2)
{
    struct cell_pair pair;
    struct cell **cursor = cells->cursor;
    struct cell *cell1;
    struct cell *cell2;
    struct cell *newcell;

    /* Find first cell at x1. */
    while (1) {
	UNROLL3({
	    cell1 = *cursor;
	    if (cell1->x > x1)
		break;

	    if (cell1->x == x1)
		goto found_first;

	    cursor = &cell1->next;
	});
    }

    /* New first cell at x1. */
    newcell = pool_alloc (cells->cell_pool.base,
			  sizeof (struct cell));
    if (likely (NULL != newcell)) {
	*cursor = newcell;
	newcell->next = cell1;
	newcell->x = x1;
	newcell->uncovered_area = 0;
	newcell->covered_height = 0;
    }
    cell1 = newcell;
 found_first:

    /* Find second cell at x2. */
    while (1) {
	UNROLL3({
	    cell2 = *cursor;
	    if (cell2->x > x2)
		break;
	    if (cell2->x == x2)
		goto found_second;
	    cursor = &cell2->next;
	});
    }

    /* New second cell at x2. */
    newcell = pool_alloc (cells->cell_pool.base,
			 sizeof (struct cell));
    if (likely (NULL != newcell)) {
	*cursor = newcell;
	newcell->next = cell2;
	newcell->x = x2;
	newcell->uncovered_area = 0;
	newcell->covered_height = 0;
    }
    cell2 = newcell;
 found_second:

    cells->cursor = cursor;
    pair.cell1 = cell1;
    pair.cell2 = cell2;
    return pair;
}

/* Add an unbounded subpixel span covering subpixels >= x to the
 * coverage cells. */
static glitter_status_t
cell_list_add_unbounded_subspan (struct cell_list *cells,
				 grid_scaled_x_t x)
{
    struct cell *cell;
    int ix, fx;

    GRID_X_TO_INT_FRAC(x, ix, fx);

    cell = cell_list_find (cells, ix);
    if (likely (cell != NULL)) {
	cell->uncovered_area += 2*fx;
	cell->covered_height++;
	return GLITTER_STATUS_SUCCESS;
    }

    return GLITTER_STATUS_NO_MEMORY;
}

/* Add a subpixel span covering [x1, x2) to the coverage cells. */
inline static glitter_status_t
cell_list_add_subspan(
    struct cell_list *cells,
    grid_scaled_x_t x1,
    grid_scaled_x_t x2)
{
    int ix1, fx1;
    int ix2, fx2;

    GRID_X_TO_INT_FRAC(x1, ix1, fx1);
    GRID_X_TO_INT_FRAC(x2, ix2, fx2);

    if (ix1 != ix2) {
	struct cell_pair p;
	p = cell_list_find_pair(cells, ix1, ix2);
	if (likely (p.cell1 != NULL && p.cell2 != NULL)) {
	    p.cell1->uncovered_area += 2*fx1;
	    ++p.cell1->covered_height;
	    p.cell2->uncovered_area -= 2*fx2;
	    --p.cell2->covered_height;
	    return GLITTER_STATUS_SUCCESS;
	}
    } else {
	struct cell *cell = cell_list_find(cells, ix1);
	if (likely (cell != NULL)) {
	    cell->uncovered_area += 2*(fx1-fx2);
	    return GLITTER_STATUS_SUCCESS;
	}
    }
    return GLITTER_STATUS_NO_MEMORY;
}

/* Adds the analytical coverage of an edge crossing the current pixel
 * row to the coverage cells and advances the edge's x position to the
 * following row.
 *
 * This function is only called when we know that during this pixel row:
 *
 * 1) The relative order of all edges on the active list doesn't
 * change.  In particular, no edges intersect within this row to pixel
 * precision.
 *
 * 2) No new edges start in this row.
 *
 * 3) No existing edges end mid-row.
 *
 * This function depends on being called with all edges from the
 * active list in the order they appear on the list (i.e. with
 * non-decreasing x-coordinate.)  */
static glitter_status_t
cell_list_render_edge(
    struct cell_list *cells,
    struct edge *edge,
    int sign)
{
    grid_scaled_y_t y1, y2, dy;
    grid_scaled_x_t dx;
    int ix1, ix2;
    grid_scaled_x_t fx1, fx2;

    struct quorem x1 = edge->x;
    struct quorem x2 = x1;

    if (! edge->vertical) {
	x2.quo += edge->dxdy_full.quo;
	x2.rem += edge->dxdy_full.rem;
	if (x2.rem >= 0) {
	    ++x2.quo;
	    x2.rem -= edge->dy;
	}

	edge->x = x2;
    }

    GRID_X_TO_INT_FRAC(x1.quo, ix1, fx1);
    GRID_X_TO_INT_FRAC(x2.quo, ix2, fx2);

    /* Edge is entirely within a column? */
    if (ix1 == ix2) {
	/* We always know that ix1 is >= the cell list cursor in this
	 * case due to the no-intersections precondition.  */
	struct cell *cell = cell_list_find(cells, ix1);
	if (unlikely (NULL == cell))
	    return GLITTER_STATUS_NO_MEMORY;

	cell->covered_height += sign*GRID_Y;
	cell->uncovered_area += sign*(fx1 + fx2)*GRID_Y;
	return GLITTER_STATUS_SUCCESS;
    }

    /* Orient the edge left-to-right. */
    dx = x2.quo - x1.quo;
    if (dx >= 0) {
	y1 = 0;
	y2 = GRID_Y;
    } else {
	int tmp;
	tmp = ix1; ix1 = ix2; ix2 = tmp;
	tmp = fx1; fx1 = fx2; fx2 = tmp;
	dx = -dx;
	sign = -sign;
	y1 = GRID_Y;
	y2 = 0;
    }
    dy = y2 - y1;

    /* Add coverage for all pixels [ix1,ix2] on this row crossed
     * by the edge. */
    {
	struct cell_pair pair;
	struct quorem y = floored_divrem((GRID_X - fx1)*dy, dx);

	/* When rendering a previous edge on the active list we may
	 * advance the cell list cursor past the leftmost pixel of the
	 * current edge even though the two edges don't intersect.
	 * e.g. consider two edges going down and rightwards:
	 *
	 *  --\_+---\_+-----+-----+----
	 *      \_    \_    |     |
	 *      | \_  | \_  |     |
	 *      |   \_|   \_|     |
	 *      |     \_    \_    |
	 *  ----+-----+-\---+-\---+----
	 *
	 * The left edge touches cells past the starting cell of the
	 * right edge.  Fortunately such cases are rare.
	 *
	 * The rewinding is never necessary if the current edge stays
	 * within a single column because we've checked before calling
	 * this function that the active list order won't change. */
	cell_list_maybe_rewind(cells, ix1);

	pair = cell_list_find_pair(cells, ix1, ix1+1);
	if (unlikely (!pair.cell1 || !pair.cell2))
	    return GLITTER_STATUS_NO_MEMORY;

	pair.cell1->uncovered_area += sign*y.quo*(GRID_X + fx1);
	pair.cell1->covered_height += sign*y.quo;
	y.quo += y1;

	if (ix1+1 < ix2) {
	    struct quorem dydx_full = floored_divrem(GRID_X*dy, dx);
	    struct cell *cell = pair.cell2;

	    ++ix1;
	    do {
		grid_scaled_y_t y_skip = dydx_full.quo;
		y.rem += dydx_full.rem;
		if (y.rem >= dx) {
		    ++y_skip;
		    y.rem -= dx;
		}

		y.quo += y_skip;

		y_skip *= sign;
		cell->uncovered_area += y_skip*GRID_X;
		cell->covered_height += y_skip;

		++ix1;
		cell = cell_list_find(cells, ix1);
		if (unlikely (NULL == cell))
		    return GLITTER_STATUS_NO_MEMORY;
	    } while (ix1 != ix2);

	    pair.cell2 = cell;
	}
	pair.cell2->uncovered_area += sign*(y2 - y.quo)*fx2;
	pair.cell2->covered_height += sign*(y2 - y.quo);
    }

    return GLITTER_STATUS_SUCCESS;
}

static void
polygon_init (struct polygon *polygon)
{
    polygon->ymin = polygon->ymax = 0;
    polygon->xmin = polygon->xmax = 0;
    polygon->y_buckets = polygon->y_buckets_embedded;
    pool_init (polygon->edge_pool.base,
	       8192 - sizeof (struct _pool_chunk),
	       sizeof (polygon->edge_pool.embedded));
}

static void
polygon_fini (struct polygon *polygon)
{
    if (polygon->y_buckets != polygon->y_buckets_embedded)
	free (polygon->y_buckets);

    pool_fini (polygon->edge_pool.base);
}

/* Empties the polygon of all edges. The polygon is then prepared to
 * receive new edges and clip them to the vertical range
 * [ymin,ymax). */
static glitter_status_t
polygon_reset (struct polygon *polygon,
	       grid_scaled_x_t xmin,
	       grid_scaled_x_t xmax,
	       grid_scaled_y_t ymin,
	       grid_scaled_y_t ymax)
{
    unsigned h = ymax - ymin;
    unsigned num_buckets = EDGE_Y_BUCKET_INDEX(ymax + EDGE_Y_BUCKET_HEIGHT-1,
					       ymin);

    pool_reset(polygon->edge_pool.base);

    if (unlikely (h > 0x7FFFFFFFU - EDGE_Y_BUCKET_HEIGHT))
	goto bail_no_mem; /* even if you could, you wouldn't want to. */

    if (polygon->y_buckets != polygon->y_buckets_embedded)
	free (polygon->y_buckets);

    polygon->y_buckets =  polygon->y_buckets_embedded;
    if (num_buckets > ARRAY_LENGTH (polygon->y_buckets_embedded)) {
	polygon->y_buckets = _cairo_malloc_ab (num_buckets,
					       sizeof (struct bucket));
	if (unlikely (NULL == polygon->y_buckets))
	    goto bail_no_mem;
    }
    memset (polygon->y_buckets, 0, num_buckets * sizeof (struct bucket));

    polygon->ymin = ymin;
    polygon->ymax = ymax;
    polygon->xmin = xmin;
    polygon->xmax = xmax;
    return GLITTER_STATUS_SUCCESS;

 bail_no_mem:
    polygon->ymin = 0;
    polygon->ymax = 0;
    return GLITTER_STATUS_NO_MEMORY;
}

static void
_polygon_insert_edge_into_its_y_bucket(
    struct polygon *polygon,
    struct edge *e)
{
    unsigned j = e->ytop - polygon->ymin;
    unsigned ix = j / EDGE_Y_BUCKET_HEIGHT;
    unsigned offset = j % EDGE_Y_BUCKET_HEIGHT;
    struct edge **ptail = &polygon->y_buckets[ix].edges;
    e->next = *ptail;
    *ptail = e;
    polygon->y_buckets[ix].have_inside_edges |= offset;
}

inline static glitter_status_t
polygon_add_edge (struct polygon *polygon,
		  const cairo_edge_t *edge)
{
    struct edge *e;
    grid_scaled_x_t dx;
    grid_scaled_y_t dy;
    grid_scaled_y_t ytop, ybot;
    grid_scaled_y_t ymin = polygon->ymin;
    grid_scaled_y_t ymax = polygon->ymax;

    assert (edge->bottom > edge->top);

    if (unlikely (edge->top >= ymax || edge->bottom <= ymin))
	return GLITTER_STATUS_SUCCESS;

    e = pool_alloc (polygon->edge_pool.base, sizeof (struct edge));
    if (unlikely (NULL == e))
	return GLITTER_STATUS_NO_MEMORY;

    dx = edge->line.p2.x - edge->line.p1.x;
    dy = edge->line.p2.y - edge->line.p1.y;
    e->dy = dy;
    e->dir = edge->dir;

    ytop = edge->top >= ymin ? edge->top : ymin;
    ybot = edge->bottom <= ymax ? edge->bottom : ymax;
    e->ytop = ytop;
    e->height_left = ybot - ytop;

    if (dx == 0) {
	e->vertical = TRUE;
	e->x.quo = edge->line.p1.x;
	e->x.rem = 0;
	e->dxdy.quo = 0;
	e->dxdy.rem = 0;
	e->dxdy_full.quo = 0;
	e->dxdy_full.rem = 0;

	/* Drop edges to the right of the clip extents. */
	if (e->x.quo >= polygon->xmax)
	    return GLITTER_STATUS_SUCCESS;

	/* Offset vertical edges at the left side of the clip extents
	 * to just shy of the left side.  We depend on this when
	 * checking for possible intersections within the clip
	 * rectangle. */
	if (e->x.quo <= polygon->xmin) {
	    e->x.quo = polygon->xmin - 1;
	}
    } else {
	e->vertical = FALSE;
	e->dxdy = floored_divrem (dx, dy);
	if (ytop == edge->line.p1.y) {
	    e->x.quo = edge->line.p1.x;
	    e->x.rem = 0;
	} else {
	    e->x = floored_muldivrem (ytop - edge->line.p1.y, dx, dy);
	    e->x.quo += edge->line.p1.x;
	}

	if (e->x.quo >= polygon->xmax && e->dxdy.quo >= 0)
	    return GLITTER_STATUS_SUCCESS;

	if (e->height_left >= GRID_Y) {
	    e->dxdy_full = floored_muldivrem (GRID_Y, dx, dy);
	} else {
	    e->dxdy_full.quo = 0;
	    e->dxdy_full.rem = 0;
	}
    }

    _polygon_insert_edge_into_its_y_bucket (polygon, e);

    e->x.rem -= dy;		/* Bias the remainder for faster
				 * edge advancement. */
    return GLITTER_STATUS_SUCCESS;
}

static void
active_list_reset (struct active_list *active)
{
    active->head = NULL;
    active->min_height = 0;
}

static void
active_list_init(struct active_list *active)
{
    active_list_reset(active);
}

/*
 * Merge two sorted edge lists.
 * Input:
 *  - head_a: The head of the first list.
 *  - head_b: The head of the second list; head_b cannot be NULL.
 * Output:
 * Returns the head of the merged list.
 *
 * Implementation notes:
 * To make it fast (in particular, to reduce to an insertion sort whenever
 * one of the two input lists only has a single element) we iterate through
 * a list until its head becomes greater than the head of the other list,
 * then we switch their roles. As soon as one of the two lists is empty, we
 * just attach the other one to the current list and exit.
 * Writes to memory are only needed to "switch" lists (as it also requires
 * attaching to the output list the list which we will be iterating next) and
 * to attach the last non-empty list.
 */
static struct edge *
merge_sorted_edges (struct edge *head_a, struct edge *head_b)
{
    struct edge *head, **next;

    head = head_a;
    next = &head;

    while (1) {
	while (head_a != NULL && head_a->x.quo <= head_b->x.quo) {
	    next = &head_a->next;
	    head_a = head_a->next;
	}

	*next = head_b;
	if (head_a == NULL)
	    return head;

	while (head_b != NULL && head_b->x.quo <= head_a->x.quo) {
	    next = &head_b->next;
	    head_b = head_b->next;
	}

	*next = head_a;
	if (head_b == NULL)
	    return head;
    }
}

/*
 * Sort (part of) a list.
 * Input:
 *  - list: The list to be sorted; list cannot be NULL.
 *  - limit: Recursion limit.
 * Output:
 *  - head_out: The head of the sorted list containing the first 2^(level+1) elements of the
 *              input list; if the input list has fewer elements, head_out be a sorted list
 *              containing all the elements of the input list.
 * Returns the head of the list of unprocessed elements (NULL if the sorted list contains
 * all the elements of the input list).
 *
 * Implementation notes:
 * Special case single element list, unroll/inline the sorting of the first two elements.
 * Some tail recursion is used since we iterate on the bottom-up solution of the problem
 * (we start with a small sorted list and keep merging other lists of the same size to it).
 */
static struct edge *
sort_edges (struct edge  *list,
	    unsigned int  level,
	    struct edge **head_out)
{
    struct edge *head_other, *remaining;
    unsigned int i;

    head_other = list->next;

    /* Single element list -> return */
    if (head_other == NULL) {
	*head_out = list;
	return NULL;
    }

    /* Unroll the first iteration of the following loop (halves the number of calls to merge_sorted_edges):
     *  - Initialize remaining to be the list containing the elements after the second in the input list.
     *  - Initialize *head_out to be the sorted list containing the first two element.
     */
    remaining = head_other->next;
    if (list->x.quo <= head_other->x.quo) {
	*head_out = list;
	/* list->next = head_other; */ /* The input list is already like this. */
	head_other->next = NULL;
    } else {
	*head_out = head_other;
	head_other->next = list;
	list->next = NULL;
    }

    for (i = 0; i < level && remaining; i++) {
	/* Extract a sorted list of the same size as *head_out
	 * (2^(i+1) elements) from the list of remaining elements. */
	remaining = sort_edges (remaining, i, &head_other);
	*head_out = merge_sorted_edges (*head_out, head_other);
    }

    /* *head_out now contains (at most) 2^(level+1) elements. */

    return remaining;
}

/* Test if the edges on the active list can be safely advanced by a
 * full row without intersections or any edges ending. */
inline static int
active_list_can_step_full_row (struct active_list *active,
			       grid_scaled_x_t     xmin)
{
    const struct edge *e;
    grid_scaled_x_t prev_x = INT_MIN;

    /* Recomputes the minimum height of all edges on the active
     * list if we have been dropping edges. */
    if (active->min_height <= 0) {
	int min_height = INT_MAX;

	e = active->head;
	while (NULL != e) {
	    if (e->height_left < min_height)
		min_height = e->height_left;
	    e = e->next;
	}

	active->min_height = min_height;
    }

    if (active->min_height < GRID_Y)
	return 0;

    /* Check for intersections as no edges end during the next row. */
    e = active->head;
    while (NULL != e) {
	struct quorem x = e->x;

	if (! e->vertical) {
	    x.quo += e->dxdy_full.quo;
	    x.rem += e->dxdy_full.rem;
	    if (x.rem >= 0)
		++x.quo;
	}

	/* There's may be an intersection if the edge sort order might
	 * change. */
	if (x.quo <= prev_x) {
	    /* Ignore intersections to the left of the clip extents.
	     * This assumes that all vertical edges on or at the left
	     * side of the clip rectangle have been shifted slightly
	     * to the left in polygon_add_edge(). */
	    if (prev_x >= xmin || x.quo >= xmin || e->x.quo >= xmin)
		return 0;
	}
	else {
	    prev_x = x.quo;
	}
	e = e->next;
    }

    return 1;
}

/* Merges edges on the given subpixel row from the polygon to the
 * active_list. */
inline static void
active_list_merge_edges_from_polygon(
    struct active_list *active,
    grid_scaled_y_t y,
    struct polygon *polygon)
{
    /* Split off the edges on the current subrow and merge them into
     * the active list. */
    unsigned ix = EDGE_Y_BUCKET_INDEX(y, polygon->ymin);
    int min_height = active->min_height;
    struct edge *subrow_edges = NULL;
    struct edge **ptail = &polygon->y_buckets[ix].edges;

    while (1) {
	struct edge *tail = *ptail;
	if (NULL == tail) break;

	if (y == tail->ytop) {
	    *ptail = tail->next;
	    tail->next = subrow_edges;
	    subrow_edges = tail;
	    if (tail->height_left < min_height)
		min_height = tail->height_left;
	} else {
	    ptail = &tail->next;
	}
    }
    if (subrow_edges) {
	sort_edges (subrow_edges, UINT_MAX, &subrow_edges);
	active->head = merge_sorted_edges (active->head, subrow_edges);
	active->min_height = min_height;
    }
}

/* Advance the edges on the active list by one subsample row by
 * updating their x positions.  Drop edges from the list that end. */
inline static void
active_list_substep_edges(
    struct active_list *active)
{
    struct edge **cursor = &active->head;
    grid_scaled_x_t prev_x = INT_MIN;
    struct edge *unsorted = NULL;

    while (1) {
	struct edge *edge;

	UNROLL3({
	    edge = *cursor;
	    if (NULL == edge)
		break;

	    if (0 != --edge->height_left) {
		edge->x.quo += edge->dxdy.quo;
		edge->x.rem += edge->dxdy.rem;
		if (edge->x.rem >= 0) {
		    ++edge->x.quo;
		    edge->x.rem -= edge->dy;
		}

		if (edge->x.quo < prev_x) {
		    *cursor = edge->next;
		    edge->next = unsorted;
		    unsorted = edge;
		} else {
		    prev_x = edge->x.quo;
		    cursor = &edge->next;
		}

	    } else {
		*cursor = edge->next;
	    }
	});
    }

    if (unsorted) {
	sort_edges (unsorted, UINT_MAX, &unsorted);
	active->head = merge_sorted_edges (active->head, unsorted);
    }
}

inline static glitter_status_t
apply_nonzero_fill_rule_for_subrow (struct active_list *active,
				    struct cell_list *coverages)
{
    struct edge *edge = active->head;
    int winding = 0;
    int xstart;
    int xend;
    int status;

    cell_list_rewind (coverages);

    while (NULL != edge) {
	xstart = edge->x.quo;
	winding = edge->dir;
	while (1) {
	    edge = edge->next;
	    if (NULL == edge)
		return cell_list_add_unbounded_subspan (coverages, xstart);

	    winding += edge->dir;
	    if (0 == winding) {
		if (edge->next == NULL || edge->next->x.quo != edge->x.quo)
		    break;
	    }
	}

	xend = edge->x.quo;
	status = cell_list_add_subspan (coverages, xstart, xend);
	if (unlikely (status))
	    return status;

	edge = edge->next;
    }

    return GLITTER_STATUS_SUCCESS;
}

static glitter_status_t
apply_evenodd_fill_rule_for_subrow (struct active_list *active,
				    struct cell_list *coverages)
{
    struct edge *edge = active->head;
    int xstart;
    int xend;
    int status;

    cell_list_rewind (coverages);

    while (NULL != edge) {
	xstart = edge->x.quo;

	while (1) {
	    edge = edge->next;
	    if (NULL == edge)
		return cell_list_add_unbounded_subspan (coverages, xstart);

	    if (edge->next == NULL || edge->next->x.quo != edge->x.quo)
		break;

	    edge = edge->next;
	}

	xend = edge->x.quo;
	status = cell_list_add_subspan (coverages, xstart, xend);
	if (unlikely (status))
	    return status;

	edge = edge->next;
    }

    return GLITTER_STATUS_SUCCESS;
}

static glitter_status_t
apply_nonzero_fill_rule_and_step_edges (struct active_list *active,
					struct cell_list *coverages)
{
    struct edge **cursor = &active->head;
    struct edge *left_edge;
    int status;

    left_edge = *cursor;
    while (NULL != left_edge) {
	struct edge *right_edge;
	int winding = left_edge->dir;

	left_edge->height_left -= GRID_Y;
	if (left_edge->height_left)
	    cursor = &left_edge->next;
	else
	    *cursor = left_edge->next;

	while (1) {
	    right_edge = *cursor;
	    if (NULL == right_edge)
		return cell_list_render_edge (coverages, left_edge, +1);

	    right_edge->height_left -= GRID_Y;
	    if (right_edge->height_left)
		cursor = &right_edge->next;
	    else
		*cursor = right_edge->next;

	    winding += right_edge->dir;
	    if (0 == winding) {
		if (right_edge->next == NULL ||
		    right_edge->next->x.quo != right_edge->x.quo)
		{
		    break;
		}
	    }

	    if (! right_edge->vertical) {
		right_edge->x.quo += right_edge->dxdy_full.quo;
		right_edge->x.rem += right_edge->dxdy_full.rem;
		if (right_edge->x.rem >= 0) {
		    ++right_edge->x.quo;
		    right_edge->x.rem -= right_edge->dy;
		}
	    }
	}

	status = cell_list_render_edge (coverages, left_edge, +1);
	if (unlikely (status))
	    return status;

	status = cell_list_render_edge (coverages, right_edge, -1);
	if (unlikely (status))
	    return status;

	left_edge = *cursor;
    }

    return GLITTER_STATUS_SUCCESS;
}

static glitter_status_t
apply_evenodd_fill_rule_and_step_edges (struct active_list *active,
					struct cell_list *coverages)
{
    struct edge **cursor = &active->head;
    struct edge *left_edge;
    int status;

    left_edge = *cursor;
    while (NULL != left_edge) {
	struct edge *right_edge;
	int winding = left_edge->dir;

	left_edge->height_left -= GRID_Y;
	if (left_edge->height_left)
	    cursor = &left_edge->next;
	else
	    *cursor = left_edge->next;

	while (1) {
	    right_edge = *cursor;
	    if (NULL == right_edge)
		return cell_list_render_edge (coverages, left_edge, +1);

	    right_edge->height_left -= GRID_Y;
	    if (right_edge->height_left)
		cursor = &right_edge->next;
	    else
		*cursor = right_edge->next;

	    winding += right_edge->dir;
	    if ((winding & 1) == 0) {
		if (right_edge->next == NULL ||
		    right_edge->next->x.quo != right_edge->x.quo)
		{
		    break;
		}
	    }

	    if (! right_edge->vertical) {
		right_edge->x.quo += right_edge->dxdy_full.quo;
		right_edge->x.rem += right_edge->dxdy_full.rem;
		if (right_edge->x.rem >= 0) {
		    ++right_edge->x.quo;
		    right_edge->x.rem -= right_edge->dy;
		}
	    }
	}

	status = cell_list_render_edge (coverages, left_edge, +1);
	if (unlikely (status))
	    return status;

	status = cell_list_render_edge (coverages, right_edge, -1);
	if (unlikely (status))
	    return status;

	left_edge = *cursor;
    }

    return GLITTER_STATUS_SUCCESS;
}

/* If the user hasn't configured a coverage blitter, use a default one
 * that blits spans directly to an A8 raster. */
#ifndef GLITTER_BLIT_COVERAGES

inline static void
blit_span(
    unsigned char *row_pixels,
    int x, unsigned len,
    grid_area_t coverage)
{
    int alpha = GRID_AREA_TO_ALPHA(coverage);
    if (1 == len) {
	row_pixels[x] = alpha;
    }
    else {
	memset(row_pixels + x, alpha, len);
    }
}

#define GLITTER_BLIT_COVERAGES(coverages, y, height, xmin, xmax) \
    do { \
	int __y = y; \
	int __h = height; \
	do { \
	    blit_cells(coverages, raster_pixels + (__y)*raster_stride, xmin, xmax); \
	} while (--__h); \
    } while (0)

static void
blit_cells(
    struct cell_list *cells,
    unsigned char *row_pixels,
    int xmin, int xmax)
{
    struct cell *cell = cells->head;
    int prev_x = xmin;
    int coverage = 0;
    if (NULL == cell)
	return;

    while (NULL != cell && cell->x < xmin) {
	coverage += cell->covered_height;
	cell = cell->next;
    }
    coverage *= GRID_X*2;

    for (; NULL != cell; cell = cell->next) {
	int x = cell->x;
	int area;
	if (x >= xmax)
	    break;
	if (x > prev_x && 0 != coverage) {
	    blit_span(row_pixels, prev_x, x - prev_x, coverage);
	}

	coverage += cell->covered_height * GRID_X*2;
	area = coverage - cell->uncovered_area;
	if (area) {
	    blit_span(row_pixels, x, 1, area);
	}
	prev_x = x+1;
    }

    if (0 != coverage && prev_x < xmax) {
	blit_span(row_pixels, prev_x, xmax - prev_x, coverage);
    }
}
#endif /* GLITTER_BLIT_COVERAGES */

static void
_glitter_scan_converter_init(glitter_scan_converter_t *converter)
{
    polygon_init(converter->polygon);
    active_list_init(converter->active);
    cell_list_init(converter->coverages);
    converter->xmin=0;
    converter->ymin=0;
    converter->xmax=0;
    converter->ymax=0;
}

static void
_glitter_scan_converter_fini(glitter_scan_converter_t *converter)
{
    polygon_fini(converter->polygon);
    cell_list_fini(converter->coverages);
    converter->xmin=0;
    converter->ymin=0;
    converter->xmax=0;
    converter->ymax=0;
}

static grid_scaled_t
int_to_grid_scaled(int i, int scale)
{
    /* Clamp to max/min representable scaled number. */
    if (i >= 0) {
	if (i >= INT_MAX/scale)
	    i = INT_MAX/scale;
    }
    else {
	if (i <= INT_MIN/scale)
	    i = INT_MIN/scale;
    }
    return i*scale;
}

#define int_to_grid_scaled_x(x) int_to_grid_scaled((x), GRID_X)
#define int_to_grid_scaled_y(x) int_to_grid_scaled((x), GRID_Y)

I glitter_status_t
glitter_scan_converter_reset(
    glitter_scan_converter_t *converter,
    int xmin, int ymin,
    int xmax, int ymax)
{
    glitter_status_t status;

    converter->xmin = 0; converter->xmax = 0;
    converter->ymin = 0; converter->ymax = 0;

    xmin = int_to_grid_scaled_x(xmin);
    ymin = int_to_grid_scaled_y(ymin);
    xmax = int_to_grid_scaled_x(xmax);
    ymax = int_to_grid_scaled_y(ymax);

    active_list_reset(converter->active);
    cell_list_reset(converter->coverages);
    status = polygon_reset(converter->polygon, xmin, xmax, ymin, ymax);
    if (status)
	return status;

    converter->xmin = xmin;
    converter->xmax = xmax;
    converter->ymin = ymin;
    converter->ymax = ymax;
    return GLITTER_STATUS_SUCCESS;
}

/* INPUT_TO_GRID_X/Y (in_coord, out_grid_scaled, grid_scale)
 *   These macros convert an input coordinate in the client's
 *   device space to the rasterisation grid.
 */
/* Gah.. this bit of ugly defines INPUT_TO_GRID_X/Y so as to use
 * shifts if possible, and something saneish if not.
 */
#if !defined(INPUT_TO_GRID_Y) && defined(GRID_Y_BITS) && GRID_Y_BITS <= GLITTER_INPUT_BITS
#  define INPUT_TO_GRID_Y(in, out) (out) = (in) >> (GLITTER_INPUT_BITS - GRID_Y_BITS)
#else
#  define INPUT_TO_GRID_Y(in, out) INPUT_TO_GRID_general(in, out, GRID_Y)
#endif

#if !defined(INPUT_TO_GRID_X) && defined(GRID_X_BITS) && GRID_X_BITS <= GLITTER_INPUT_BITS
#  define INPUT_TO_GRID_X(in, out) (out) = (in) >> (GLITTER_INPUT_BITS - GRID_X_BITS)
#else
#  define INPUT_TO_GRID_X(in, out) INPUT_TO_GRID_general(in, out, GRID_X)
#endif

#define INPUT_TO_GRID_general(in, out, grid_scale) do {		\
	long long tmp__ = (long long)(grid_scale) * (in);	\
	tmp__ >>= GLITTER_INPUT_BITS;				\
	(out) = tmp__;						\
} while (0)

I glitter_status_t
glitter_scan_converter_add_edge (glitter_scan_converter_t *converter,
				 const cairo_edge_t *edge)
{
    cairo_edge_t e;

    INPUT_TO_GRID_Y (edge->top, e.top);
    INPUT_TO_GRID_Y (edge->bottom, e.bottom);
    if (e.top >= e.bottom)
	return GLITTER_STATUS_SUCCESS;

    /* XXX: possible overflows if GRID_X/Y > 2**GLITTER_INPUT_BITS */
    INPUT_TO_GRID_Y (edge->line.p1.y, e.line.p1.y);
    INPUT_TO_GRID_Y (edge->line.p2.y, e.line.p2.y);
    if (e.line.p1.y == e.line.p2.y)
	return GLITTER_STATUS_SUCCESS;

    INPUT_TO_GRID_X (edge->line.p1.x, e.line.p1.x);
    INPUT_TO_GRID_X (edge->line.p2.x, e.line.p2.x);

    e.dir = edge->dir;

    return polygon_add_edge (converter->polygon, &e);
}

#ifndef GLITTER_BLIT_COVERAGES_BEGIN
# define GLITTER_BLIT_COVERAGES_BEGIN
#endif

#ifndef GLITTER_BLIT_COVERAGES_END
# define GLITTER_BLIT_COVERAGES_END
#endif

#ifndef GLITTER_BLIT_COVERAGES_EMPTY
# define GLITTER_BLIT_COVERAGES_EMPTY(y0, y1, xmin, xmax)
#endif

static cairo_bool_t
active_list_is_vertical (struct active_list *active)
{
    struct edge *e;

    for (e = active->head; e != NULL; e = e->next) {
	if (! e->vertical)
	    return FALSE;
    }

    return TRUE;
}

static void
step_edges (struct active_list *active, int count)
{
    struct edge **cursor = &active->head;
    struct edge *edge;

    for (edge = *cursor; edge != NULL; edge = *cursor) {
	edge->height_left -= GRID_Y * count;
	if (edge->height_left)
	    cursor = &edge->next;
	else
	    *cursor = edge->next;
    }
}

I glitter_status_t
glitter_scan_converter_render(
    glitter_scan_converter_t *converter,
    int nonzero_fill,
    GLITTER_BLIT_COVERAGES_ARGS)
{
    int i, j;
    int ymax_i = converter->ymax / GRID_Y;
    int ymin_i = converter->ymin / GRID_Y;
    int xmin_i, xmax_i;
    grid_scaled_x_t xmin = converter->xmin;
    int h = ymax_i - ymin_i;
    struct polygon *polygon = converter->polygon;
    struct cell_list *coverages = converter->coverages;
    struct active_list *active = converter->active;

    xmin_i = converter->xmin / GRID_X;
    xmax_i = converter->xmax / GRID_X;
    if (xmin_i >= xmax_i)
	return GLITTER_STATUS_SUCCESS;

    /* Let the coverage blitter initialise itself. */
    GLITTER_BLIT_COVERAGES_BEGIN;

    /* Render each pixel row. */
    for (i = 0; i < h; i = j) {
	int do_full_step = 0;
	glitter_status_t status = 0;

	j = i + 1;

	/* Determine if we can ignore this row or use the full pixel
	 * stepper. */
	if (polygon->y_buckets[i].edges == NULL) {
	    if (! active->head) {
		for (; j < h && ! polygon->y_buckets[j].edges; j++)
		    ;
		GLITTER_BLIT_COVERAGES_EMPTY (i+ymin_i, j-i, xmin_i, xmax_i);
		continue;
	    }
	    do_full_step = active_list_can_step_full_row (active, xmin);
	}
	else if (! polygon->y_buckets[i].have_inside_edges) {
	    grid_scaled_y_t y = (i+ymin_i)*GRID_Y;
	    active_list_merge_edges_from_polygon (active, y, polygon);
	    do_full_step = active_list_can_step_full_row (active, xmin);
	}

	if (do_full_step) {
	    /* Step by a full pixel row's worth. */
	    if (nonzero_fill) {
		status = apply_nonzero_fill_rule_and_step_edges (active,
								 coverages);
	    } else {
		status = apply_evenodd_fill_rule_and_step_edges (active,
								 coverages);
	    }

	    if (active_list_is_vertical (active)) {
		while (j < h &&
		       polygon->y_buckets[j].edges == NULL &&
		       active->min_height >= 2*GRID_Y)
		{
		    active->min_height -= GRID_Y;
		    j++;
		}
		if (j != i + 1)
		    step_edges (active, j - (i + 1));
	    }
	} else {
	    /* Supersample this row. */
	    grid_scaled_y_t suby;
	    for (suby = 0; suby < GRID_Y; suby++) {
		grid_scaled_y_t y = (i+ymin_i)*GRID_Y + suby;

		active_list_merge_edges_from_polygon (active, y, polygon);

		if (nonzero_fill) {
		    status |= apply_nonzero_fill_rule_for_subrow (active,
								  coverages);
		} else {
		    status |= apply_evenodd_fill_rule_for_subrow (active,
								  coverages);
		}

		active_list_substep_edges(active);
	    }
	}

	if (unlikely (status))
	    return status;

	GLITTER_BLIT_COVERAGES(coverages, i+ymin_i, j-i, xmin_i, xmax_i);
	cell_list_reset (coverages);

	if (! active->head)
	    active->min_height = INT_MAX;
	else
	    active->min_height -= GRID_Y;
    }

    /* Clean up the coverage blitter. */
    GLITTER_BLIT_COVERAGES_END;

    return GLITTER_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------
 * cairo specific implementation: the coverage blitter and
 * scan converter subclass. */

static glitter_status_t
blit_with_span_renderer (struct cell_list *cells,
			 cairo_span_renderer_t *renderer,
			 struct pool *span_pool,
			 int y, int height,
			 int xmin, int xmax)
{
    struct cell *cell = cells->head;
    int prev_x = xmin;
    int cover = 0;
    cairo_half_open_span_t *spans;
    unsigned num_spans;

    if (cell == NULL)
	return blit_empty_with_span_renderer (renderer, y, height);

    /* Skip cells to the left of the clip region. */
    while (cell != NULL && cell->x < xmin) {
	cover += cell->covered_height;
	cell = cell->next;
    }
    cover *= GRID_X*2;

    /* Count number of cells remaining. */
    {
	struct cell *next = cell;
	num_spans = 1;
	while (next != NULL) {
	    next = next->next;
	    ++num_spans;
	}
	num_spans = 2*num_spans;
    }

    /* Allocate enough spans for the row. */
    pool_reset (span_pool);
    spans = pool_alloc (span_pool, sizeof(spans[0])*num_spans);
    if (unlikely (spans == NULL))
	return GLITTER_STATUS_NO_MEMORY;

    num_spans = 0;

    /* Form the spans from the coverages and areas. */
    for (; cell != NULL; cell = cell->next) {
	int x = cell->x;
	int area;

	if (x >= xmax)
	    break;

	if (x > prev_x) {
	    spans[num_spans].x = prev_x;
	    spans[num_spans].coverage = GRID_AREA_TO_ALPHA (cover);
	    ++num_spans;
	}

	cover += cell->covered_height*GRID_X*2;
	area = cover - cell->uncovered_area;

	spans[num_spans].x = x;
	spans[num_spans].coverage = GRID_AREA_TO_ALPHA (area);
	++num_spans;

	prev_x = x+1;
    }

    if (prev_x <= xmax) {
	spans[num_spans].x = prev_x;
	spans[num_spans].coverage = GRID_AREA_TO_ALPHA (cover);
	++num_spans;
    }

    if (prev_x < xmax && cover) {
	spans[num_spans].x = xmax;
	spans[num_spans].coverage = 0;
	++num_spans;
    }

    /* Dump them into the renderer. */
    return renderer->render_rows (renderer, y, height, spans, num_spans);
}

static glitter_status_t
blit_empty_with_span_renderer (cairo_span_renderer_t *renderer, int y, int height)
{
    return renderer->render_rows (renderer, y, height, NULL, 0);
}

struct _cairo_tor_scan_converter {
    cairo_scan_converter_t base;

    glitter_scan_converter_t converter[1];
    cairo_fill_rule_t fill_rule;

    struct {
	struct pool base[1];
	cairo_half_open_span_t embedded[32];
    } span_pool;
};

typedef struct _cairo_tor_scan_converter cairo_tor_scan_converter_t;

static void
_cairo_tor_scan_converter_destroy (void *converter)
{
    cairo_tor_scan_converter_t *self = converter;
    if (self == NULL) {
	return;
    }
    _glitter_scan_converter_fini (self->converter);
    pool_fini (self->span_pool.base);
    free(self);
}

static cairo_status_t
_cairo_tor_scan_converter_add_edge (void		*converter,
				    const cairo_point_t *p1,
				    const cairo_point_t *p2,
				    int top, int bottom,
				    int dir)
{
    cairo_tor_scan_converter_t *self = converter;
    cairo_status_t status;
    cairo_edge_t edge;

    edge.line.p1 = *p1;
    edge.line.p2 = *p2;
    edge.top = top;
    edge.bottom = bottom;
    edge.dir = dir;

    status = glitter_scan_converter_add_edge (self->converter, &edge);
    if (unlikely (status))
	return _cairo_scan_converter_set_error (self, _cairo_error (status));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_tor_scan_converter_add_polygon (void		*converter,
				       const cairo_polygon_t *polygon)
{
    cairo_tor_scan_converter_t *self = converter;
    cairo_status_t status;
    int i;

    for (i = 0; i < polygon->num_edges; i++) {
	status = glitter_scan_converter_add_edge (self->converter,
						  &polygon->edges[i]);
	if (unlikely (status)) {
	    return _cairo_scan_converter_set_error (self,
						    _cairo_error (status));
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_tor_scan_converter_generate (void			*converter,
				    cairo_span_renderer_t	*renderer)
{
    cairo_tor_scan_converter_t *self = converter;
    cairo_status_t status;

   status = glitter_scan_converter_render (self->converter,
					   self->fill_rule == CAIRO_FILL_RULE_WINDING,
					   renderer,
					   self->span_pool.base);
    if (unlikely (status))
	return _cairo_scan_converter_set_error (self, _cairo_error (status));

    return CAIRO_STATUS_SUCCESS;
}

cairo_scan_converter_t *
_cairo_tor_scan_converter_create (int			xmin,
				  int			ymin,
				  int			xmax,
				  int			ymax,
				  cairo_fill_rule_t	fill_rule)
{
    cairo_tor_scan_converter_t *self;
    cairo_status_t status;

    self = calloc (1, sizeof(struct _cairo_tor_scan_converter));
    if (unlikely (self == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto bail_nomem;
    }

    self->base.destroy = _cairo_tor_scan_converter_destroy;
    self->base.add_edge = _cairo_tor_scan_converter_add_edge;
    self->base.add_polygon = _cairo_tor_scan_converter_add_polygon;
    self->base.generate = _cairo_tor_scan_converter_generate;

    pool_init (self->span_pool.base,
	      250 * sizeof(self->span_pool.embedded[0]),
	      sizeof(self->span_pool.embedded));

    _glitter_scan_converter_init (self->converter);
    status = glitter_scan_converter_reset (self->converter,
					   xmin, ymin, xmax, ymax);
    if (unlikely (status))
	goto bail;

    self->fill_rule = fill_rule;

    return &self->base;

 bail:
    self->base.destroy(&self->base);
 bail_nomem:
    return _cairo_scan_converter_create_in_error (status);
}
