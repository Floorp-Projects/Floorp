/*
 * Copyright © 2004 Carl Worth
 * Copyright © 2006 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Carl Worth
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

/* Provide definitions for standalone compilation */
#include "cairoint.h"

#include "cairo-skiplist-private.h"
#include "cairo-freelist-private.h"

#define DEBUG_VALIDATE 0
#define DEBUG_PRINT_STATE 0

typedef cairo_point_t cairo_bo_point32_t;

typedef struct _cairo_bo_point128 {
    cairo_int128_t x;
    cairo_int128_t y;
} cairo_bo_point128_t;

typedef struct _cairo_bo_intersect_ordinate {
    int32_t ordinate;
    enum { EXACT, INEXACT } exactness;
} cairo_bo_intersect_ordinate_t;

typedef struct _cairo_bo_intersect_point {
    cairo_bo_intersect_ordinate_t x;
    cairo_bo_intersect_ordinate_t y;
} cairo_bo_intersect_point_t;

typedef struct _cairo_bo_edge cairo_bo_edge_t;
typedef struct _sweep_line_elt sweep_line_elt_t;
typedef struct _cairo_bo_trap cairo_bo_trap_t;
typedef struct _cairo_bo_traps cairo_bo_traps_t;

/* A deferred trapezoid of an edge. */
struct _cairo_bo_trap {
    cairo_bo_edge_t *right;
    int32_t top;
};

struct _cairo_bo_traps {
    cairo_traps_t *traps;
    cairo_freelist_t freelist;

    /* These form the closed bounding box of the original input
     * points. */
    cairo_fixed_t xmin;
    cairo_fixed_t ymin;
    cairo_fixed_t xmax;
    cairo_fixed_t ymax;
};

struct _cairo_bo_edge {
    cairo_bo_point32_t top;
    cairo_bo_point32_t middle;
    cairo_bo_point32_t bottom;
    cairo_bool_t reversed;
    cairo_bo_edge_t *prev;
    cairo_bo_edge_t *next;
    cairo_bo_trap_t *deferred_trap;
    sweep_line_elt_t *sweep_line_elt;
};

struct _sweep_line_elt {
    cairo_bo_edge_t *edge;
    skip_elt_t elt;
};

#define SKIP_ELT_TO_EDGE_ELT(elt)	SKIP_LIST_ELT_TO_DATA (sweep_line_elt_t, (elt))
#define SKIP_ELT_TO_EDGE(elt) 		(SKIP_ELT_TO_EDGE_ELT (elt)->edge)

typedef enum {
    CAIRO_BO_STATUS_INTERSECTION,
    CAIRO_BO_STATUS_PARALLEL,
    CAIRO_BO_STATUS_NO_INTERSECTION
} cairo_bo_status_t;

typedef enum {
    CAIRO_BO_EVENT_TYPE_START,
    CAIRO_BO_EVENT_TYPE_STOP,
    CAIRO_BO_EVENT_TYPE_INTERSECTION
} cairo_bo_event_type_t;

typedef struct _cairo_bo_event {
    cairo_bo_event_type_t type;
    cairo_bo_edge_t *e1;
    cairo_bo_edge_t *e2;
    cairo_bo_point32_t point;
    skip_elt_t elt;
} cairo_bo_event_t;

#define SKIP_ELT_TO_EVENT(elt) SKIP_LIST_ELT_TO_DATA (cairo_bo_event_t, (elt))

typedef struct _cairo_bo_event_queue {
    cairo_skip_list_t intersection_queue;

    cairo_bo_event_t *startstop_events;
    cairo_bo_event_t **sorted_startstop_event_ptrs;
    unsigned next_startstop_event_index;
    unsigned num_startstop_events;
} cairo_bo_event_queue_t;

/* This structure extends #cairo_skip_list_t, which must come first. */
typedef struct _cairo_bo_sweep_line {
    cairo_skip_list_t active_edges;
    cairo_bo_edge_t *head;
    cairo_bo_edge_t *tail;
    int32_t current_y;
} cairo_bo_sweep_line_t;


static inline int
_cairo_bo_point32_compare (cairo_bo_point32_t const *a,
			   cairo_bo_point32_t const *b)
{
    int cmp = a->y - b->y;
    if (cmp) return cmp;
    return a->x - b->x;
}

/* Compare the slope of a to the slope of b, returning 1, 0, -1 if the
 * slope a is respectively greater than, equal to, or less than the
 * slope of b.
 *
 * For each edge, consider the direction vector formed from:
 *
 *	top -> bottom
 *
 * which is:
 *
 *	(dx, dy) = (bottom.x - top.x, bottom.y - top.y)
 *
 * We then define the slope of each edge as dx/dy, (which is the
 * inverse of the slope typically used in math instruction). We never
 * compute a slope directly as the value approaches infinity, but we
 * can derive a slope comparison without division as follows, (where
 * the ? represents our compare operator).
 *
 * 1.	   slope(a) ? slope(b)
 * 2.	    adx/ady ? bdx/bdy
 * 3.	(adx * bdy) ? (bdx * ady)
 *
 * Note that from step 2 to step 3 there is no change needed in the
 * sign of the result since both ady and bdy are guaranteed to be
 * greater than or equal to 0.
 *
 * When using this slope comparison to sort edges, some care is needed
 * when interpreting the results. Since the slope compare operates on
 * distance vectors from top to bottom it gives a correct left to
 * right sort for edges that have a common top point, (such as two
 * edges with start events at the same location). On the other hand,
 * the sense of the result will be exactly reversed for two edges that
 * have a common stop point.
 */
static int
_slope_compare (cairo_bo_edge_t *a,
		cairo_bo_edge_t *b)
{
    /* XXX: We're assuming here that dx and dy will still fit in 32
     * bits. That's not true in general as there could be overflow. We
     * should prevent that before the tessellation algorithm
     * begins.
     */
    int32_t adx = a->bottom.x - a->top.x;
    int32_t bdx = b->bottom.x - b->top.x;

    /* Since the dy's are all positive by construction we can fast
     * path the case where the two edges point in different directions
     * with respect to x. */
    if ((adx ^ bdx) < 0) {
	return adx < 0 ? -1 : +1;
    } else {
	int32_t ady = a->bottom.y - a->top.y;
	int32_t bdy = b->bottom.y - b->top.y;
	cairo_int64_t adx_bdy = _cairo_int32x32_64_mul (adx, bdy);
	cairo_int64_t bdx_ady = _cairo_int32x32_64_mul (bdx, ady);

	return _cairo_int64_cmp (adx_bdy, bdx_ady);
    }
}

/*
 * We need to compare the x-coordinates of a pair of lines for a particular y,
 * without loss of precision.
 *
 * The x-coordinate along an edge for a given y is:
 *   X = A_x + (Y - A_y) * A_dx / A_dy
 *
 * So the inequality we wish to test is:
 *   A_x + (Y - A_y) * A_dx / A_dy -?- B_x + (Y - B_y) * B_dx / B_dy,
 * where -?- is our inequality operator.
 *
 * By construction, we know that A_dy and B_dy (and (Y - A_y), (Y - B_y)) are
 * all positive, so we can rearrange it thus without causing a sign change:
 *   A_dy * B_dy * (A_x - B_x) -?- (Y - B_y) * B_dx * A_dy
 *                                 - (Y - A_y) * A_dx * B_dy
 *
 * Given the assumption that all the deltas fit within 32 bits, we can compute
 * this comparison directly using 128 bit arithmetic.
 *
 * (And put the burden of the work on developing fast 128 bit ops, which are
 * required throughout the tessellator.)
 *
 * See the similar discussion for _slope_compare().
 */
static int
edges_compare_x_for_y_general (const cairo_bo_edge_t *a,
			       const cairo_bo_edge_t *b,
			       int32_t y)
{
    /* XXX: We're assuming here that dx and dy will still fit in 32
     * bits. That's not true in general as there could be overflow. We
     * should prevent that before the tessellation algorithm
     * begins.
     */
    int32_t adx, ady;
    int32_t bdx, bdy;
    cairo_int128_t L, R;

    adx = a->bottom.x - a->top.x;
    ady = a->bottom.y - a->top.y;

    bdx = b->bottom.x - b->top.x;
    bdy = b->bottom.y - b->top.y;

    L = _cairo_int64x32_128_mul (_cairo_int32x32_64_mul (ady, bdy),
				 a->top.x - b->top.x);

    R = _cairo_int128_sub (_cairo_int64x32_128_mul (_cairo_int32x32_64_mul (bdx,
									    ady),
						    y - b->top.y),
			   _cairo_int64x32_128_mul (_cairo_int32x32_64_mul (adx,
									    bdy),
						    y - a->top.y));

    /* return _cairo_int128_cmp (L, R); */
    if (_cairo_int128_lt (L, R))
	return -1;
    if (_cairo_int128_gt (L, R))
	return 1;
    return 0;
}

/*
 * We need to compare the x-coordinate of a line for a particular y wrt to a
 * given x, without loss of precision.
 *
 * The x-coordinate along an edge for a given y is:
 *   X = A_x + (Y - A_y) * A_dx / A_dy
 *
 * So the inequality we wish to test is:
 *   A_x + (Y - A_y) * A_dx / A_dy -?- X
 * where -?- is our inequality operator.
 *
 * By construction, we know that A_dy (and (Y - A_y)) are
 * all positive, so we can rearrange it thus without causing a sign change:
 *   (Y - A_y) * A_dx -?- (X - A_x) * A_dy
 *
 * Given the assumption that all the deltas fit within 32 bits, we can compute
 * this comparison directly using 64 bit arithmetic.
 *
 * See the similar discussion for _slope_compare() and
 * edges_compare_x_for_y_general().
 */
static int
edge_compare_for_y_against_x (const cairo_bo_edge_t *a,
			      int32_t y,
			      int32_t x)
{
    int32_t adx, ady;
    int32_t dx, dy;
    cairo_int64_t L, R;

    adx = a->bottom.x - a->top.x;
    ady = a->bottom.y - a->top.y;

    dy = y - a->top.y;
    dx = x - a->top.x;

    L = _cairo_int32x32_64_mul (dy, adx);
    R = _cairo_int32x32_64_mul (dx, ady);

    return _cairo_int64_cmp (L, R);
}

static int
edges_compare_x_for_y (const cairo_bo_edge_t *a,
		       const cairo_bo_edge_t *b,
		       int32_t y)
{
    /* If the sweep-line is currently on an end-point of a line,
     * then we know its precise x value (and considering that we often need to
     * compare events at end-points, this happens frequently enough to warrant
     * special casing).
     */
    enum {
       HAVE_NEITHER = 0x0,
       HAVE_AX      = 0x1,
       HAVE_BX      = 0x2,
       HAVE_BOTH    = HAVE_AX | HAVE_BX
    } have_ax_bx = HAVE_BOTH;
    int32_t ax, bx;

    if (y == a->top.y)
	ax = a->top.x;
    else if (y == a->bottom.y)
	ax = a->bottom.x;
    else
	have_ax_bx &= ~HAVE_AX;

    if (y == b->top.y)
	bx = b->top.x;
    else if (y == b->bottom.y)
	bx = b->bottom.x;
    else
	have_ax_bx &= ~HAVE_BX;

    switch (have_ax_bx) {
    default:
    case HAVE_NEITHER:
	return edges_compare_x_for_y_general (a, b, y);
    case HAVE_AX:
	return - edge_compare_for_y_against_x (b, y, ax);
    case HAVE_BX:
	return edge_compare_for_y_against_x (a, y, bx);
    case HAVE_BOTH:
	return ax - bx;
    }
}

static int
_cairo_bo_sweep_line_compare_edges (cairo_bo_sweep_line_t	*sweep_line,
				    cairo_bo_edge_t		*a,
				    cairo_bo_edge_t		*b)
{
    int cmp;

    if (a == b)
	return 0;

    /* don't bother solving for abscissa if the edges' bounding boxes
     * can be used to order them. */
    {
           int32_t amin, amax;
           int32_t bmin, bmax;
           if (a->middle.x < a->bottom.x) {
                   amin = a->middle.x;
                   amax = a->bottom.x;
           } else {
                   amin = a->bottom.x;
                   amax = a->middle.x;
           }
           if (b->middle.x < b->bottom.x) {
                   bmin = b->middle.x;
                   bmax = b->bottom.x;
           } else {
                   bmin = b->bottom.x;
                   bmax = b->middle.x;
           }
           if (amax < bmin) return -1;
           if (amin > bmax) return +1;
    }

    cmp = edges_compare_x_for_y (a, b, sweep_line->current_y);
    if (cmp)
	return cmp;

    /* The two edges intersect exactly at y, so fall back on slope
     * comparison. We know that this compare_edges function will be
     * called only when starting a new edge, (not when stopping an
     * edge), so we don't have to worry about conditionally inverting
     * the sense of _slope_compare. */
    cmp = _slope_compare (a, b);
    if (cmp)
	return cmp;

    /* We've got two collinear edges now. */

    /* Since we're dealing with start events, prefer comparing top
     * edges before bottom edges. */
    cmp = _cairo_bo_point32_compare (&a->top, &b->top);
    if (cmp)
	return cmp;

    cmp = _cairo_bo_point32_compare (&a->bottom, &b->bottom);
    if (cmp)
	return cmp;

    /* Finally, we've got two identical edges. Let's finally
     * discriminate by a simple pointer comparison, (which works only
     * because we "know" the edges are all in a single array and don't
     * move. */
    if (a > b)
	return 1;
    else
	return -1;
}

static int
_sweep_line_elt_compare (void	*list,
			 void	*a,
			 void	*b)
{
    cairo_bo_sweep_line_t *sweep_line = list;
    sweep_line_elt_t *edge_elt_a = a;
    sweep_line_elt_t *edge_elt_b = b;

    return _cairo_bo_sweep_line_compare_edges (sweep_line,
					       edge_elt_a->edge,
					       edge_elt_b->edge);
}

static inline int
cairo_bo_event_compare (cairo_bo_event_t const *a,
			cairo_bo_event_t const *b)
{
    int cmp;

    /* The major motion of the sweep line is vertical (top-to-bottom),
     * and the minor motion is horizontal (left-to-right), dues to the
     * infinitesimal tilt rule.
     *
     * Our point comparison function respects these rules.
     */
    cmp = _cairo_bo_point32_compare (&a->point, &b->point);
    if (cmp)
	return cmp;

    /* The events share a common point, so further discrimination is
     * determined by the event type. Due to the infinitesimal
     * shortening rule, stop events come first, then intersection
     * events, then start events.
     */
    if (a->type != b->type) {
	if (a->type == CAIRO_BO_EVENT_TYPE_STOP)
	    return -1;
	if (a->type == CAIRO_BO_EVENT_TYPE_START)
	    return 1;

	if (b->type == CAIRO_BO_EVENT_TYPE_STOP)
	    return 1;
	if (b->type == CAIRO_BO_EVENT_TYPE_START)
	    return -1;
    }

    /* At this stage we are looking at two events of the same type at
     * the same point. The final sort key is a slope comparison. We
     * need a different sense for start and stop events based on the
     * shortening rule.
     *
     * Note: Fortunately, we get to ignore errors in the relative
     * ordering of intersection events. This means we don't even have
     * to look at e2 here, nor worry about which sense of the slope
     * comparison test is used for intersection events.
     */
    cmp = _slope_compare (a->e1, b->e1);
    if (cmp) {
	if (a->type == CAIRO_BO_EVENT_TYPE_START)
	    return cmp;
	else
	    return - cmp;
    }

    /* Next look at the opposite point. This leaves ambiguities only
     * for identical edges. */
    if (a->type == CAIRO_BO_EVENT_TYPE_START) {
	cmp = _cairo_bo_point32_compare (&b->e1->bottom,
					 &a->e1->bottom);
	if (cmp)
	    return cmp;
    }
    else if (a->type == CAIRO_BO_EVENT_TYPE_STOP) {
	cmp = _cairo_bo_point32_compare (&a->e1->top,
					 &b->e1->top);
	if (cmp)
	    return cmp;
    }
    else { /* CAIRO_BO_EVENT_TYPE_INTERSECT */
	/* For two intersection events at the identical point, we
	 * don't care what order they sort in, but we do care that we
	 * have a stable sort. In particular intersections between
	 * different pairs of edges must never return 0. */
	cmp = _cairo_bo_point32_compare (&a->e2->top, &b->e2->top);
	if (cmp)
	    return cmp;
	cmp = _cairo_bo_point32_compare (&a->e2->bottom, &b->e2->bottom);
	if (cmp)
	    return cmp;
	cmp = _cairo_bo_point32_compare (&a->e1->top, &b->e1->top);
	if (cmp)
	    return cmp;
	cmp = _cairo_bo_point32_compare (&a->e1->bottom, &b->e1->bottom);
	if (cmp)
	    return cmp;
     }

    /* Discrimination based on the edge pointers. */
    if (a->e1 < b->e1)
	return -1;
    if (a->e1 > b->e1)
	return +1;
    if (a->e2 < b->e2)
	return -1;
    if (a->e2 > b->e2)
	return +1;
    return 0;
}

static int
cairo_bo_event_compare_abstract (void		*list,
				 void	*a,
				 void	*b)
{
    cairo_bo_event_t *event_a = a;
    cairo_bo_event_t *event_b = b;

    return cairo_bo_event_compare (event_a, event_b);
}

static int
cairo_bo_event_compare_pointers (void const *voida,
				 void const *voidb)
{
    cairo_bo_event_t const * const *a = voida;
    cairo_bo_event_t const * const *b = voidb;
    if (*a != *b) {
	int cmp = cairo_bo_event_compare (*a, *b);
	if (cmp)
	    return cmp;
	if (*a < *b)
	    return -1;
	if (*a > *b)
	    return +1;
    }
    return 0;
}

static inline cairo_int64_t
det32_64 (int32_t a,
	  int32_t b,
	  int32_t c,
	  int32_t d)
{
    cairo_int64_t ad;
    cairo_int64_t bc;

    /* det = a * d - b * c */
    ad = _cairo_int32x32_64_mul (a, d);
    bc = _cairo_int32x32_64_mul (b, c);

    return _cairo_int64_sub (ad, bc);
}

static inline cairo_int128_t
det64x32_128 (cairo_int64_t a,
	      int32_t       b,
	      cairo_int64_t c,
	      int32_t       d)
{
    cairo_int128_t ad;
    cairo_int128_t bc;

    /* det = a * d - b * c */
    ad = _cairo_int64x32_128_mul (a, d);
    bc = _cairo_int64x32_128_mul (c, b);

    return _cairo_int128_sub (ad, bc);
}

/* Compute the intersection of two lines as defined by two edges. The
 * result is provided as a coordinate pair of 128-bit integers.
 *
 * Returns %CAIRO_BO_STATUS_INTERSECTION if there is an intersection or
 * %CAIRO_BO_STATUS_PARALLEL if the two lines are exactly parallel.
 */
static cairo_bo_status_t
intersect_lines (cairo_bo_edge_t		*a,
		 cairo_bo_edge_t		*b,
		 cairo_bo_intersect_point_t	*intersection)
{
    cairo_int64_t a_det, b_det;

    /* XXX: We're assuming here that dx and dy will still fit in 32
     * bits. That's not true in general as there could be overflow. We
     * should prevent that before the tessellation algorithm begins.
     * What we're doing to mitigate this is to perform clamping in
     * cairo_bo_tessellate_polygon().
     */
    int32_t dx1 = a->top.x - a->bottom.x;
    int32_t dy1 = a->top.y - a->bottom.y;

    int32_t dx2 = b->top.x - b->bottom.x;
    int32_t dy2 = b->top.y - b->bottom.y;

    cairo_int64_t den_det = det32_64 (dx1, dy1, dx2, dy2);
    cairo_quorem64_t qr;

    if (_cairo_int64_is_zero (den_det))
	return CAIRO_BO_STATUS_PARALLEL;

    a_det = det32_64 (a->top.x, a->top.y,
		      a->bottom.x, a->bottom.y);
    b_det = det32_64 (b->top.x, b->top.y,
		      b->bottom.x, b->bottom.y);

    /* x = det (a_det, dx1, b_det, dx2) / den_det */
    qr = _cairo_int_96by64_32x64_divrem (det64x32_128 (a_det, dx1,
						       b_det, dx2),
					 den_det);
    if (_cairo_int64_eq (qr.rem, den_det))
	return CAIRO_BO_STATUS_NO_INTERSECTION;
    intersection->x.ordinate = _cairo_int64_to_int32 (qr.quo);
    intersection->x.exactness = _cairo_int64_is_zero (qr.rem) ? EXACT : INEXACT;

    /* y = det (a_det, dy1, b_det, dy2) / den_det */
    qr = _cairo_int_96by64_32x64_divrem (det64x32_128 (a_det, dy1,
						       b_det, dy2),
					 den_det);
    if (_cairo_int64_eq (qr.rem, den_det))
	return CAIRO_BO_STATUS_NO_INTERSECTION;
    intersection->y.ordinate = _cairo_int64_to_int32 (qr.quo);
    intersection->y.exactness = _cairo_int64_is_zero (qr.rem) ? EXACT : INEXACT;

    return CAIRO_BO_STATUS_INTERSECTION;
}

static int
_cairo_bo_intersect_ordinate_32_compare (cairo_bo_intersect_ordinate_t	a,
					 int32_t			b)
{
    /* First compare the quotient */
    if (a.ordinate > b)
	return +1;
    if (a.ordinate < b)
	return -1;
    /* With quotient identical, if remainder is 0 then compare equal */
    /* Otherwise, the non-zero remainder makes a > b */
    return INEXACT == a.exactness;
}

/* Does the given edge contain the given point. The point must already
 * be known to be contained within the line determined by the edge,
 * (most likely the point results from an intersection of this edge
 * with another).
 *
 * If we had exact arithmetic, then this function would simply be a
 * matter of examining whether the y value of the point lies within
 * the range of y values of the edge. But since intersection points
 * are not exact due to being rounded to the nearest integer within
 * the available precision, we must also examine the x value of the
 * point.
 *
 * The definition of "contains" here is that the given intersection
 * point will be seen by the sweep line after the start event for the
 * given edge and before the stop event for the edge. See the comments
 * in the implementation for more details.
 */
static cairo_bool_t
_cairo_bo_edge_contains_intersect_point (cairo_bo_edge_t		*edge,
					 cairo_bo_intersect_point_t	*point)
{
    int cmp_top, cmp_bottom;

    /* XXX: When running the actual algorithm, we don't actually need to
     * compare against edge->top at all here, since any intersection above
     * top is eliminated early via a slope comparison. We're leaving these
     * here for now only for the sake of the quadratic-time intersection
     * finder which needs them.
     */

    cmp_top = _cairo_bo_intersect_ordinate_32_compare (point->y, edge->top.y);
    cmp_bottom = _cairo_bo_intersect_ordinate_32_compare (point->y, edge->bottom.y);

    if (cmp_top < 0 || cmp_bottom > 0)
    {
	return FALSE;
    }

    if (cmp_top > 0 && cmp_bottom < 0)
    {
	return TRUE;
    }

    /* At this stage, the point lies on the same y value as either
     * edge->top or edge->bottom, so we have to examine the x value in
     * order to properly determine containment. */

    /* If the y value of the point is the same as the y value of the
     * top of the edge, then the x value of the point must be greater
     * to be considered as inside the edge. Similarly, if the y value
     * of the point is the same as the y value of the bottom of the
     * edge, then the x value of the point must be less to be
     * considered as inside. */

    if (cmp_top == 0)
	return (_cairo_bo_intersect_ordinate_32_compare (point->x, edge->top.x) > 0);
    else /* cmp_bottom == 0 */
	return (_cairo_bo_intersect_ordinate_32_compare (point->x, edge->bottom.x) < 0);
}

/* Compute the intersection of two edges. The result is provided as a
 * coordinate pair of 128-bit integers.
 *
 * Returns %CAIRO_BO_STATUS_INTERSECTION if there is an intersection
 * that is within both edges, %CAIRO_BO_STATUS_NO_INTERSECTION if the
 * intersection of the lines defined by the edges occurs outside of
 * one or both edges, and %CAIRO_BO_STATUS_PARALLEL if the two edges
 * are exactly parallel.
 *
 * Note that when determining if a candidate intersection is "inside"
 * an edge, we consider both the infinitesimal shortening and the
 * infinitesimal tilt rules described by John Hobby. Specifically, if
 * the intersection is exactly the same as an edge point, it is
 * effectively outside (no intersection is returned). Also, if the
 * intersection point has the same
 */
static cairo_bo_status_t
_cairo_bo_edge_intersect (cairo_bo_edge_t	*a,
			  cairo_bo_edge_t	*b,
			  cairo_bo_point32_t	*intersection)
{
    cairo_bo_status_t status;
    cairo_bo_intersect_point_t quorem;

    status = intersect_lines (a, b, &quorem);
    if (status)
	return status;

    if (! _cairo_bo_edge_contains_intersect_point (a, &quorem))
	return CAIRO_BO_STATUS_NO_INTERSECTION;

    if (! _cairo_bo_edge_contains_intersect_point (b, &quorem))
	return CAIRO_BO_STATUS_NO_INTERSECTION;

    /* Now that we've correctly compared the intersection point and
     * determined that it lies within the edge, then we know that we
     * no longer need any more bits of storage for the intersection
     * than we do for our edge coordinates. We also no longer need the
     * remainder from the division. */
    intersection->x = quorem.x.ordinate;
    intersection->y = quorem.y.ordinate;

    return CAIRO_BO_STATUS_INTERSECTION;
}

static void
_cairo_bo_event_init (cairo_bo_event_t		*event,
		      cairo_bo_event_type_t	 type,
		      cairo_bo_edge_t	*e1,
		      cairo_bo_edge_t	*e2,
		      cairo_bo_point32_t	 point)
{
    event->type = type;
    event->e1 = e1;
    event->e2 = e2;
    event->point = point;
}

static cairo_status_t
_cairo_bo_event_queue_insert (cairo_bo_event_queue_t *queue,
			      cairo_bo_event_t	     *event)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    /* Don't insert if there's already an equivalent intersection event in the queue. */
    if (_cairo_skip_list_insert (&queue->intersection_queue, event,
		      event->type == CAIRO_BO_EVENT_TYPE_INTERSECTION) == NULL)
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
    return status;
}

static void
_cairo_bo_event_queue_delete (cairo_bo_event_queue_t *queue,
			      cairo_bo_event_t	     *event)
{
    if (CAIRO_BO_EVENT_TYPE_INTERSECTION == event->type)
	_cairo_skip_list_delete_given ( &queue->intersection_queue, &event->elt );
}

static cairo_bo_event_t *
_cairo_bo_event_dequeue (cairo_bo_event_queue_t *event_queue)
{
    skip_elt_t *elt = event_queue->intersection_queue.chains[0];
    cairo_bo_event_t *intersection = elt ? SKIP_ELT_TO_EVENT (elt) : NULL;
    cairo_bo_event_t *startstop;

    if (event_queue->next_startstop_event_index == event_queue->num_startstop_events)
	return intersection;
    startstop = event_queue->sorted_startstop_event_ptrs[
	event_queue->next_startstop_event_index];

    if (!intersection || cairo_bo_event_compare (startstop, intersection) <= 0)
    {
	event_queue->next_startstop_event_index++;
	return startstop;
    }
    return intersection;
}

static cairo_status_t
_cairo_bo_event_queue_init (cairo_bo_event_queue_t	*event_queue,
			    cairo_bo_edge_t	*edges,
			    int				 num_edges)
{
    int i;
    cairo_bo_event_t *events, **sorted_event_ptrs;
    unsigned num_events = 2*num_edges;

    memset (event_queue, 0, sizeof(*event_queue));

    _cairo_skip_list_init (&event_queue->intersection_queue,
		    cairo_bo_event_compare_abstract,
		    sizeof (cairo_bo_event_t));
    if (0 == num_edges)
	return CAIRO_STATUS_SUCCESS;

    /* The skip_elt_t field of a cairo_bo_event_t isn't used for start
     * or stop events, so this allocation is safe.  XXX: make the
     * event type a union so it doesn't always contain the skip
     * elt? */
    events = _cairo_malloc_ab (num_events, sizeof (cairo_bo_event_t) + sizeof(cairo_bo_event_t*));
    if (events == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    sorted_event_ptrs = (cairo_bo_event_t **) (events + num_events);
    event_queue->startstop_events = events;
    event_queue->sorted_startstop_event_ptrs = sorted_event_ptrs;
    event_queue->num_startstop_events = num_events;
    event_queue->next_startstop_event_index = 0;

    for (i = 0; i < num_edges; i++) {
	sorted_event_ptrs[2*i] = &events[2*i];
	sorted_event_ptrs[2*i+1] = &events[2*i+1];

	/* Initialize "middle" to top */
	edges[i].middle = edges[i].top;

	_cairo_bo_event_init (&events[2*i],
			      CAIRO_BO_EVENT_TYPE_START,
			      &edges[i], NULL,
			      edges[i].top);

	_cairo_bo_event_init (&events[2*i+1],
			      CAIRO_BO_EVENT_TYPE_STOP,
			      &edges[i], NULL,
			      edges[i].bottom);
    }

    qsort (sorted_event_ptrs, num_events,
	   sizeof(cairo_bo_event_t *),
	   cairo_bo_event_compare_pointers);
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_bo_event_queue_fini (cairo_bo_event_queue_t *event_queue)
{
    _cairo_skip_list_fini (&event_queue->intersection_queue);
    if (event_queue->startstop_events)
	free (event_queue->startstop_events);
}

static cairo_status_t
_cairo_bo_event_queue_insert_if_intersect_below_current_y (cairo_bo_event_queue_t	*event_queue,
							   cairo_bo_edge_t	*left,
							   cairo_bo_edge_t	*right)
{
    cairo_bo_status_t status;
    cairo_bo_point32_t intersection;
    cairo_bo_event_t event;

    if (left == NULL || right == NULL)
	return CAIRO_STATUS_SUCCESS;

    /* The names "left" and "right" here are correct descriptions of
     * the order of the two edges within the active edge list. So if a
     * slope comparison also puts left less than right, then we know
     * that the intersection of these two segments has oalready
     * occurred before the current sweep line position. */
    if (_slope_compare (left, right) < 0)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_bo_edge_intersect (left, right, &intersection);
    if (status == CAIRO_BO_STATUS_PARALLEL ||
	status == CAIRO_BO_STATUS_NO_INTERSECTION)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    _cairo_bo_event_init (&event,
			  CAIRO_BO_EVENT_TYPE_INTERSECTION,
			  left, right,
			  intersection);

    return _cairo_bo_event_queue_insert (event_queue, &event);
}

static void
_cairo_bo_sweep_line_init (cairo_bo_sweep_line_t *sweep_line)
{
    _cairo_skip_list_init (&sweep_line->active_edges,
		    _sweep_line_elt_compare,
		    sizeof (sweep_line_elt_t));
    sweep_line->head = NULL;
    sweep_line->tail = NULL;
    sweep_line->current_y = 0;
}

static void
_cairo_bo_sweep_line_fini (cairo_bo_sweep_line_t *sweep_line)
{
    _cairo_skip_list_fini (&sweep_line->active_edges);
}

static cairo_status_t
_cairo_bo_sweep_line_insert (cairo_bo_sweep_line_t	*sweep_line,
			     cairo_bo_edge_t		*edge)
{
    skip_elt_t *next_elt;
    sweep_line_elt_t *sweep_line_elt;
    cairo_bo_edge_t **prev_of_next, **next_of_prev;

    sweep_line_elt = _cairo_skip_list_insert (&sweep_line->active_edges, &edge,
				       1 /* unique inserts*/);
    if (sweep_line_elt == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    next_elt = sweep_line_elt->elt.next[0];
    if (next_elt)
	prev_of_next = & (SKIP_ELT_TO_EDGE (next_elt)->prev);
    else
	prev_of_next = &sweep_line->tail;

    if (*prev_of_next)
	next_of_prev = &(*prev_of_next)->next;
    else
	next_of_prev = &sweep_line->head;

    edge->prev = *prev_of_next;
    edge->next = *next_of_prev;
    *prev_of_next = edge;
    *next_of_prev = edge;

    edge->sweep_line_elt = sweep_line_elt;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_bo_sweep_line_delete (cairo_bo_sweep_line_t	*sweep_line,
			     cairo_bo_edge_t	*edge)
{
    cairo_bo_edge_t **left_next, **right_prev;

    _cairo_skip_list_delete_given (&sweep_line->active_edges, &edge->sweep_line_elt->elt);

    left_next = &sweep_line->head;
    if (edge->prev)
	left_next = &edge->prev->next;

    right_prev = &sweep_line->tail;
    if (edge->next)
	right_prev = &edge->next->prev;

    *left_next = edge->next;
    *right_prev = edge->prev;
}

static void
_cairo_bo_sweep_line_swap (cairo_bo_sweep_line_t	*sweep_line,
			   cairo_bo_edge_t		*left,
			   cairo_bo_edge_t		*right)
{
    sweep_line_elt_t *left_elt, *right_elt;
    cairo_bo_edge_t **before_left, **after_right;

    /* Within the skip list we can do the swap simply by swapping the
     * pointers to the edge elements and leaving all of the skip list
     * elements and pointers unchanged. */
    left_elt = left->sweep_line_elt;
    right_elt = SKIP_ELT_TO_EDGE_ELT (left_elt->elt.next[0]);

    left_elt->edge = right;
    right->sweep_line_elt = left_elt;

    right_elt->edge = left;
    left->sweep_line_elt = right_elt;

    /* Within the doubly-linked list of edges, there's a bit more
     * bookkeeping involved with the swap. */
    before_left = &sweep_line->head;
    if (left->prev)
	before_left = &left->prev->next;
    *before_left = right;

    after_right = &sweep_line->tail;
    if (right->next)
	after_right = &right->next->prev;
    *after_right = left;

    left->next = right->next;
    right->next = left;

    right->prev = left->prev;
    left->prev = right;
}

#if DEBUG_PRINT_STATE
static void
_cairo_bo_edge_print (cairo_bo_edge_t *edge)
{
    printf ("(0x%x, 0x%x)-(0x%x, 0x%x)",
	    edge->top.x, edge->top.y,
	    edge->bottom.x, edge->bottom.y);
}

static void
_cairo_bo_event_print (cairo_bo_event_t *event)
{
    switch (event->type) {
    case CAIRO_BO_EVENT_TYPE_START:
	printf ("Start: ");
	break;
    case CAIRO_BO_EVENT_TYPE_STOP:
	printf ("Stop: ");
	break;
    case CAIRO_BO_EVENT_TYPE_INTERSECTION:
	printf ("Intersection: ");
	break;
    }
    printf ("(%d, %d)\t", event->point.x, event->point.y);
    _cairo_bo_edge_print (event->e1);
    if (event->type == CAIRO_BO_EVENT_TYPE_INTERSECTION) {
	printf (" X ");
	_cairo_bo_edge_print (event->e2);
    }
    printf ("\n");
}

static void
_cairo_bo_event_queue_print (cairo_bo_event_queue_t *event_queue)
{
    skip_elt_t *elt;
    /* XXX: fixme to print the start/stop array too. */
    cairo_skip_list_t *queue = &event_queue->intersection_queue;
    cairo_bo_event_t *event;

    printf ("Event queue:\n");

    for (elt = queue->chains[0];
	 elt;
	 elt = elt->next[0])
    {
	event = SKIP_ELT_TO_EVENT (elt);
	_cairo_bo_event_print (event);
    }
}

static void
_cairo_bo_sweep_line_print (cairo_bo_sweep_line_t *sweep_line)
{
    cairo_bool_t first = TRUE;
    skip_elt_t *elt;
    cairo_bo_edge_t *edge;

    printf ("Sweep line (reversed):     ");

    for (edge = sweep_line->tail;
	 edge;
	 edge = edge->prev)
    {
	if (!first)
	    printf (", ");
	_cairo_bo_edge_print (edge);
	first = FALSE;
    }
    printf ("\n");


    printf ("Sweep line from edge list: ");
    first = TRUE;
    for (edge = sweep_line->head;
	 edge;
	 edge = edge->next)
    {
	if (!first)
	    printf (", ");
	_cairo_bo_edge_print (edge);
	first = FALSE;
    }
    printf ("\n");

    printf ("Sweep line from skip list: ");
    first = TRUE;
    for (elt = sweep_line->active_edges.chains[0];
	 elt;
	 elt = elt->next[0])
    {
	if (!first)
	    printf (", ");
	_cairo_bo_edge_print (SKIP_ELT_TO_EDGE (elt));
	first = FALSE;
    }
    printf ("\n");
}

static void
print_state (const char			*msg,
	     cairo_bo_event_queue_t	*event_queue,
	     cairo_bo_sweep_line_t	*sweep_line)
{
    printf ("%s\n", msg);
    _cairo_bo_event_queue_print (event_queue);
    _cairo_bo_sweep_line_print (sweep_line);
    printf ("\n");
}
#endif

/* Adds the trapezoid, if any, of the left edge to the #cairo_traps_t
 * of bo_traps. */
static cairo_status_t
_cairo_bo_edge_end_trap (cairo_bo_edge_t	*left,
			 int32_t		bot,
			 cairo_bo_traps_t	*bo_traps)
{
    cairo_fixed_t fixed_top, fixed_bot;
    cairo_bo_trap_t *trap = left->deferred_trap;
    cairo_bo_edge_t *right;

    if (!trap)
	return CAIRO_STATUS_SUCCESS;

     /* If the right edge of the trapezoid stopped earlier than the
      * left edge, then cut the trapezoid bottom early. */
    right = trap->right;
    if (right->bottom.y < bot)
	bot = right->bottom.y;

    fixed_top = trap->top;
    fixed_bot = bot;

    /* Only emit trapezoids with positive height. */
    if (fixed_top < fixed_bot) {
	cairo_line_t left_line;
	cairo_line_t right_line;
	cairo_fixed_t xmin = bo_traps->xmin;
	cairo_fixed_t ymin = bo_traps->ymin;
	fixed_top += ymin;
	fixed_bot += ymin;

	left_line.p1.x  = left->top.x + xmin;
	left_line.p1.y  = left->top.y + ymin;
	right_line.p1.x = right->top.x + xmin;
	right_line.p1.y = right->top.y + ymin;

	left_line.p2.x  = left->bottom.x + xmin;
	left_line.p2.y  = left->bottom.y + ymin;
	right_line.p2.x = right->bottom.x + xmin;
	right_line.p2.y = right->bottom.y + ymin;

	/* Avoid emitting the trapezoid if it is obviously degenerate.
	 * TODO: need a real collinearity test here for the cases
	 * where the trapezoid is degenerate, yet the top and bottom
	 * coordinates aren't equal.  */
	if (left_line.p1.x != right_line.p1.x ||
	    left_line.p1.y != right_line.p1.y ||
	    left_line.p2.x != right_line.p2.x ||
	    left_line.p2.y != right_line.p2.y)
	{
	    _cairo_traps_add_trap (bo_traps->traps,
				   fixed_top, fixed_bot,
				   &left_line, &right_line);

#if DEBUG_PRINT_STATE
	    printf ("Deferred trap: left=(%08x, %08x)-(%08x,%08x) "
		    "right=(%08x,%08x)-(%08x,%08x) top=%08x, bot=%08x\n",
		    left->top.x, left->top.y, left->bottom.x, left->bottom.y,
		    right->top.x, right->top.y, right->bottom.x, right->bottom.y,
		    trap->top, bot);
#endif
	}
    }

    _cairo_freelist_free (&bo_traps->freelist, trap);
    left->deferred_trap = NULL;

    return _cairo_traps_status (bo_traps->traps);
}

/* Start a new trapezoid at the given top y coordinate, whose edges
 * are `edge' and `edge->next'. If `edge' already has a trapezoid,
 * then either add it to the traps in `bo_traps', if the trapezoid's
 * right edge differs from `edge->next', or do nothing if the new
 * trapezoid would be a continuation of the existing one. */
static cairo_status_t
_cairo_bo_edge_start_or_continue_trap (cairo_bo_edge_t	*edge,
				       int32_t		top,
				       cairo_bo_traps_t	*bo_traps)
{
    cairo_status_t status;
    cairo_bo_trap_t *trap = edge->deferred_trap;

    if (trap) {
	if (trap->right == edge->next) return CAIRO_STATUS_SUCCESS;
	status = _cairo_bo_edge_end_trap (edge, top, bo_traps);
	if (status)
	    return status;
    }

    if (edge->next) {
	trap = edge->deferred_trap = _cairo_freelist_alloc (&bo_traps->freelist);
	if (!edge->deferred_trap)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	trap->right = edge->next;
	trap->top = top;
    }
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_bo_traps_init (cairo_bo_traps_t	*bo_traps,
		      cairo_traps_t	*traps,
		      cairo_fixed_t	 xmin,
		      cairo_fixed_t	 ymin,
		      cairo_fixed_t	 xmax,
		      cairo_fixed_t	 ymax)
{
    bo_traps->traps = traps;
    _cairo_freelist_init (&bo_traps->freelist, sizeof(cairo_bo_trap_t));
    bo_traps->xmin = xmin;
    bo_traps->ymin = ymin;
    bo_traps->xmax = xmax;
    bo_traps->ymax = ymax;
}

static void
_cairo_bo_traps_fini (cairo_bo_traps_t *bo_traps)
{
    _cairo_freelist_fini (&bo_traps->freelist);
}

#if DEBUG_VALIDATE
static void
_cairo_bo_sweep_line_validate (cairo_bo_sweep_line_t *sweep_line)
{
    cairo_bo_edge_t *edge;
    skip_elt_t *elt;

    /* March through both the skip list's singly-linked list and the
     * sweep line's own list through pointers in the edges themselves
     * and make sure they agree at every point. */

    for (edge = sweep_line->head, elt = sweep_line->active_edges.chains[0];
	 edge && elt;
	 edge = edge->next, elt = elt->next[0])
    {
	if (SKIP_ELT_TO_EDGE (elt) != edge) {
	    fprintf (stderr, "*** Error: Sweep line fails to validate: Inconsistent data in the two lists.\n");
	    abort ();
	}
    }

    if (edge || elt) {
	fprintf (stderr, "*** Error: Sweep line fails to validate: One list ran out before the other.\n");
	abort ();
    }
}
#endif


static cairo_status_t
_active_edges_to_traps (cairo_bo_edge_t		*head,
			int32_t			 top,
			cairo_fill_rule_t	 fill_rule,
			cairo_bo_traps_t	*bo_traps)
{
    cairo_status_t status;
    int in_out = 0;
    cairo_bo_edge_t *edge;

    for (edge = head; edge; edge = edge->next) {
	if (fill_rule == CAIRO_FILL_RULE_WINDING) {
	    if (edge->reversed)
		in_out++;
	    else
		in_out--;
	    if (in_out == 0) {
		status = _cairo_bo_edge_end_trap (edge, top, bo_traps);
		if (status)
		    return status;
		continue;
	    }
	} else {
	    in_out++;
	    if ((in_out & 1) == 0) {
		status = _cairo_bo_edge_end_trap (edge, top, bo_traps);
		if (status)
		    return status;
		continue;
	    }
	}

	status = _cairo_bo_edge_start_or_continue_trap (edge, top, bo_traps);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Execute a single pass of the Bentley-Ottmann algorithm on edges,
 * generating trapezoids according to the fill_rule and appending them
 * to traps. */
static cairo_status_t
_cairo_bentley_ottmann_tessellate_bo_edges (cairo_bo_edge_t	*edges,
					    int			 num_edges,
					    cairo_fill_rule_t	 fill_rule,
					    cairo_traps_t	*traps,
					    cairo_fixed_t	xmin,
					    cairo_fixed_t	ymin,
					    cairo_fixed_t	xmax,
					    cairo_fixed_t	ymax,
					    int			*num_intersections)
{
    cairo_status_t status;
    int intersection_count = 0;
    cairo_bo_event_queue_t event_queue;
    cairo_bo_sweep_line_t sweep_line;
    cairo_bo_traps_t bo_traps;
    cairo_bo_event_t *event, event_saved;
    cairo_bo_edge_t *edge;
    cairo_bo_edge_t *left, *right;
    cairo_bo_edge_t *edge1, *edge2;

    status = _cairo_bo_event_queue_init (&event_queue, edges, num_edges);
    if (status)
	return status;

    _cairo_bo_sweep_line_init (&sweep_line);
    _cairo_bo_traps_init (&bo_traps, traps, xmin, ymin, xmax, ymax);

#if DEBUG_PRINT_STATE
    print_state ("After initializing", &event_queue, &sweep_line);
#endif

    while (1)
    {
	event = _cairo_bo_event_dequeue (&event_queue);
	if (!event)
	    break;

	if (event->point.y != sweep_line.current_y) {
	    status = _active_edges_to_traps (sweep_line.head,
					     sweep_line.current_y,
					     fill_rule, &bo_traps);
	    if (status)
		goto unwind;

	    sweep_line.current_y = event->point.y;
	}

	event_saved = *event;
	_cairo_bo_event_queue_delete (&event_queue, event);
	event = &event_saved;

	switch (event->type) {
	case CAIRO_BO_EVENT_TYPE_START:
	    edge = event->e1;

	    status = _cairo_bo_sweep_line_insert (&sweep_line, edge);
	    if (status)
		goto unwind;
	    /* Cache the insert position for use in pass 2.
	    event->e2 = Sortlist::prev (sweep_line, edge);
	    */

	    left = edge->prev;
	    right = edge->next;

	    status = _cairo_bo_event_queue_insert_if_intersect_below_current_y (&event_queue, left, edge);
	    if (status)
		goto unwind;

	    status = _cairo_bo_event_queue_insert_if_intersect_below_current_y (&event_queue, edge, right);
	    if (status)
		goto unwind;

#if DEBUG_PRINT_STATE
	    print_state ("After processing start", &event_queue, &sweep_line);
#endif
	    break;

	case CAIRO_BO_EVENT_TYPE_STOP:
	    edge = event->e1;

	    left = edge->prev;
	    right = edge->next;

	    _cairo_bo_sweep_line_delete (&sweep_line, edge);

	    status = _cairo_bo_edge_end_trap (edge, edge->bottom.y, &bo_traps);
	    if (status)
		goto unwind;

	    status = _cairo_bo_event_queue_insert_if_intersect_below_current_y (&event_queue, left, right);
	    if (status)
		goto unwind;

#if DEBUG_PRINT_STATE
	    print_state ("After processing stop", &event_queue, &sweep_line);
#endif
	    break;

	case CAIRO_BO_EVENT_TYPE_INTERSECTION:
	    edge1 = event->e1;
	    edge2 = event->e2;

	    /* skip this intersection if its edges are not adjacent */
	    if (edge2 != edge1->next)
		break;

	    intersection_count++;

	    edge1->middle = event->point;
	    edge2->middle = event->point;

	    left = edge1->prev;
	    right = edge2->next;

	    _cairo_bo_sweep_line_swap (&sweep_line, edge1, edge2);

	    /* after the swap e2 is left of e1 */

	    status = _cairo_bo_event_queue_insert_if_intersect_below_current_y (&event_queue,
								       left, edge2);
	    if (status)
		goto unwind;

	    status = _cairo_bo_event_queue_insert_if_intersect_below_current_y (&event_queue,
								       edge1, right);
	    if (status)
		goto unwind;

#if DEBUG_PRINT_STATE
	    print_state ("After processing intersection", &event_queue, &sweep_line);
#endif
	    break;
	}
#if DEBUG_VALIDATE
	_cairo_bo_sweep_line_validate (&sweep_line);
#endif
    }

    *num_intersections = intersection_count;
 unwind:
    for (edge = sweep_line.head; edge; edge = edge->next) {
	cairo_status_t status2 = _cairo_bo_edge_end_trap (edge,
							  sweep_line.current_y,
							  &bo_traps);
	if (!status)
	    status = status2;
    }
    _cairo_bo_traps_fini (&bo_traps);
    _cairo_bo_sweep_line_fini (&sweep_line);
    _cairo_bo_event_queue_fini (&event_queue);
    return status;
}

static void
update_minmax(cairo_fixed_t *inout_min,
	      cairo_fixed_t *inout_max,
	      cairo_fixed_t v)
{
    if (v < *inout_min)
	*inout_min = v;
    if (v > *inout_max)
	*inout_max = v;
}

cairo_status_t
_cairo_bentley_ottmann_tessellate_polygon (cairo_traps_t	 *traps,
					   const cairo_polygon_t *polygon,
					   cairo_fill_rule_t	  fill_rule)
{
    int intersections;
    cairo_status_t status;
    cairo_bo_edge_t stack_edges[CAIRO_STACK_ARRAY_LENGTH (cairo_bo_edge_t)];
    cairo_bo_edge_t *edges;
    cairo_fixed_t xmin = 0x7FFFFFFF;
    cairo_fixed_t ymin = 0x7FFFFFFF;
    cairo_fixed_t xmax = -0x80000000;
    cairo_fixed_t ymax = -0x80000000;
    cairo_box_t limit;
    cairo_bool_t has_limits;
    int num_bo_edges;
    int i;

    if (0 == polygon->num_edges)
	return CAIRO_STATUS_SUCCESS;

    has_limits = _cairo_traps_get_limit (traps, &limit);

    if (polygon->num_edges < ARRAY_LENGTH (stack_edges)) {
	edges = stack_edges;
    } else {
	edges = _cairo_malloc_ab (polygon->num_edges, sizeof (cairo_bo_edge_t));
	if (edges == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    /* Figure out the bounding box of the input coordinates and
     * validate that we're not given invalid polygon edges. */
    for (i = 0; i < polygon->num_edges; i++) {
	update_minmax (&xmin, &xmax, polygon->edges[i].edge.p1.x);
	update_minmax (&ymin, &ymax, polygon->edges[i].edge.p1.y);
	update_minmax (&xmin, &xmax, polygon->edges[i].edge.p2.x);
	update_minmax (&ymin, &ymax, polygon->edges[i].edge.p2.y);
	assert (polygon->edges[i].edge.p1.y <= polygon->edges[i].edge.p2.y &&
		"BUG: tessellator given upside down or horizontal edges");
    }

    /* The tessellation functions currently assume that no line
     * segment extends more than 2^31-1 in either dimension.  We
     * guarantee this by offsetting the internal coordinates to the
     * range [0,2^31-1], and clamping to 2^31-1 if a coordinate
     * exceeds the range (and yes, this generates an incorrect
     * result).  First we have to clamp the bounding box itself. */
    /* XXX: Rather than changing the input values, a better approach
     * would be to detect out-of-bounds input and return a
     * CAIRO_STATUS_OVERFLOW value to the user. */
    if (xmax - xmin < 0)
	xmax = xmin + 0x7FFFFFFF;
    if (ymax - ymin < 0)
	ymax = ymin + 0x7FFFFFFF;

    for (i = 0, num_bo_edges = 0; i < polygon->num_edges; i++) {
	cairo_bo_edge_t *edge = &edges[num_bo_edges];
	cairo_point_t top = polygon->edges[i].edge.p1;
	cairo_point_t bot = polygon->edges[i].edge.p2;

	/* Discard the edge if it lies outside the limits of traps. */
	if (has_limits) {
	    /* Strictly above or below the limits? */
	    if (bot.y <= limit.p1.y || top.y >= limit.p2.y)
		continue;
	}

	/* Offset coordinates into the non-negative range. */
	top.x -= xmin;
	top.y -= ymin;
	bot.x -= xmin;
	bot.y -= ymin;

	/* If the coordinates are still negative, then their extent is
	 * overflowing 2^31-1.  We're going to kludge it and clamp the
	 * coordinates into the clamped bounding box.  */
	if (top.x < 0) top.x = xmax - xmin;
	if (top.y < 0) top.y = ymax - ymin;
	if (bot.x < 0) bot.x = xmax - xmin;
	if (bot.y < 0) bot.y = ymax - ymin;

	if (top.y == bot.y) {
	    /* Clamping might have produced horizontal edges.  Ignore
	     * those. */
	    continue;
	}
	assert (top.y < bot.y &&
		"BUG: clamping the input range flipped the "
		"orientation of an edge");

	edge->top.x = top.x;
	edge->top.y = top.y;
	edge->bottom.x = bot.x;
	edge->bottom.y = bot.y;
	/* XXX: The 'clockWise' name that cairo_polygon_t uses is
	 * totally bogus. It's really a (negated!) description of
	 * whether the edge is reversed. */
	edge->reversed = (! polygon->edges[i].clockWise);
	edge->deferred_trap = NULL;
	edge->prev = NULL;
	edge->next = NULL;
	edge->sweep_line_elt = NULL;

	num_bo_edges++;
    }

    /* XXX: This would be the convenient place to throw in multiple
     * passes of the Bentley-Ottmann algorithm. It would merely
     * require storing the results of each pass into a temporary
     * cairo_traps_t. */
    status = _cairo_bentley_ottmann_tessellate_bo_edges (edges, num_bo_edges,
							 fill_rule, traps,
							 xmin, ymin, xmax, ymax,
							 &intersections);

    if (edges != stack_edges)
	free (edges);

    return status;
}

#if 0
static cairo_bool_t
edges_have_an_intersection_quadratic (cairo_bo_edge_t	*edges,
				      int		 num_edges)

{
    int i, j;
    cairo_bo_edge_t *a, *b;
    cairo_bo_point32_t intersection;
    cairo_bo_status_t status;

    /* We must not be given any upside-down edges. */
    for (i = 0; i < num_edges; i++) {
	assert (_cairo_bo_point32_compare (&edges[i].top, &edges[i].bottom) < 0);
	edges[i].top.x <<= CAIRO_BO_GUARD_BITS;
	edges[i].top.y <<= CAIRO_BO_GUARD_BITS;
	edges[i].bottom.x <<= CAIRO_BO_GUARD_BITS;
	edges[i].bottom.y <<= CAIRO_BO_GUARD_BITS;
    }

    for (i = 0; i < num_edges; i++) {
	for (j = 0; j < num_edges; j++) {
	    if (i == j)
		continue;

	    a = &edges[i];
	    b = &edges[j];

	    status = _cairo_bo_edge_intersect (a, b, &intersection);
	    if (status == CAIRO_BO_STATUS_PARALLEL ||
		status == CAIRO_BO_STATUS_NO_INTERSECTION)
	    {
		continue;
	    }

	    printf ("Found intersection (%d,%d) between (%d,%d)-(%d,%d) and (%d,%d)-(%d,%d)\n",
		    intersection.x,
		    intersection.y,
		    a->top.x, a->top.y,
		    a->bottom.x, a->bottom.y,
		    b->top.x, b->top.y,
		    b->bottom.x, b->bottom.y);

	    return TRUE;
	}
    }
    return FALSE;
}

#define TEST_MAX_EDGES 10

typedef struct test {
    const char *name;
    const char *description;
    int num_edges;
    cairo_bo_edge_t edges[TEST_MAX_EDGES];
} test_t;

static test_t
tests[] = {
    {
	"3 near misses",
	"3 edges all intersecting very close to each other",
	3,
	{
	    { { 4, 2}, {0, 0}, { 9, 9}, NULL, NULL },
	    { { 7, 2}, {0, 0}, { 2, 3}, NULL, NULL },
	    { { 5, 2}, {0, 0}, { 1, 7}, NULL, NULL }
	}
    },
    {
	"inconsistent data",
	"Derived from random testing---was leading to skip list and edge list disagreeing.",
	2,
	{
	    { { 2, 3}, {0, 0}, { 8, 9}, NULL, NULL },
	    { { 2, 3}, {0, 0}, { 6, 7}, NULL, NULL }
	}
    },
    {
	"failed sort",
	"A test derived from random testing that leads to an inconsistent sort --- looks like we just can't attempt to validate the sweep line with edge_compare?",
	3,
	{
	    { { 6, 2}, {0, 0}, { 6, 5}, NULL, NULL },
	    { { 3, 5}, {0, 0}, { 5, 6}, NULL, NULL },
	    { { 9, 2}, {0, 0}, { 5, 6}, NULL, NULL },
	}
    },
    {
	"minimal-intersection",
	"Intersection of a two from among the smallest possible edges.",
	2,
	{
	    { { 0, 0}, {0, 0}, { 1, 1}, NULL, NULL },
	    { { 1, 0}, {0, 0}, { 0, 1}, NULL, NULL }
	}
    },
    {
	"simple",
	"A simple intersection of two edges at an integer (2,2).",
	2,
	{
	    { { 1, 1}, {0, 0}, { 3, 3}, NULL, NULL },
	    { { 2, 1}, {0, 0}, { 2, 3}, NULL, NULL }
	}
    },
    {
	"bend-to-horizontal",
	"With intersection truncation one edge bends to horizontal",
	2,
	{
	    { { 9, 1}, {0, 0}, {3, 7}, NULL, NULL },
	    { { 3, 5}, {0, 0}, {9, 9}, NULL, NULL }
	}
    }
};

/*
    {
	"endpoint",
	"An intersection that occurs at the endpoint of a segment.",
	{
	    { { 4, 6}, { 5, 6}, NULL, { { NULL }} },
	    { { 4, 5}, { 5, 7}, NULL, { { NULL }} },
	    { { 0, 0}, { 0, 0}, NULL, { { NULL }} },
	}
    }
    {
	name = "overlapping",
	desc = "Parallel segments that share an endpoint, with different slopes.",
	edges = {
	    { top = { x = 2, y = 0}, bottom = { x = 1, y = 1}},
	    { top = { x = 2, y = 0}, bottom = { x = 0, y = 2}},
	    { top = { x = 0, y = 3}, bottom = { x = 1, y = 3}},
	    { top = { x = 0, y = 3}, bottom = { x = 2, y = 3}},
	    { top = { x = 0, y = 4}, bottom = { x = 0, y = 6}},
	    { top = { x = 0, y = 5}, bottom = { x = 0, y = 6}}
	}
    },
    {
	name = "hobby_stage_3",
	desc = "A particularly tricky part of the 3rd stage of the 'hobby' test below.",
	edges = {
	    { top = { x = -1, y = -2}, bottom = { x =  4, y = 2}},
	    { top = { x =  5, y =  3}, bottom = { x =  9, y = 5}},
	    { top = { x =  5, y =  3}, bottom = { x =  6, y = 3}},
	}
    },
    {
	name = "hobby",
	desc = "Example from John Hobby's paper. Requires 3 passes of the iterative algorithm.",
	edges = {
	    { top = { x =   0, y =   0}, bottom = { x =   9, y =   5}},
	    { top = { x =   0, y =   0}, bottom = { x =  13, y =   6}},
	    { top = { x =  -1, y =  -2}, bottom = { x =   9, y =   5}}
	}
    },
    {
	name = "slope",
	desc = "Edges with same start/stop points but different slopes",
	edges = {
	    { top = { x = 4, y = 1}, bottom = { x = 6, y = 3}},
	    { top = { x = 4, y = 1}, bottom = { x = 2, y = 3}},
	    { top = { x = 2, y = 4}, bottom = { x = 4, y = 6}},
	    { top = { x = 6, y = 4}, bottom = { x = 4, y = 6}}
	}
    },
    {
	name = "horizontal",
	desc = "Test of a horizontal edge",
	edges = {
	    { top = { x = 1, y = 1}, bottom = { x = 6, y = 6}},
	    { top = { x = 2, y = 3}, bottom = { x = 5, y = 3}}
	}
    },
    {
	name = "vertical",
	desc = "Test of a vertical edge",
	edges = {
	    { top = { x = 5, y = 1}, bottom = { x = 5, y = 7}},
	    { top = { x = 2, y = 4}, bottom = { x = 8, y = 5}}
	}
    },
    {
	name = "congruent",
	desc = "Two overlapping edges with the same slope",
	edges = {
	    { top = { x = 5, y = 1}, bottom = { x = 5, y = 7}},
	    { top = { x = 5, y = 2}, bottom = { x = 5, y = 6}},
	    { top = { x = 2, y = 4}, bottom = { x = 8, y = 5}}
	}
    },
    {
	name = "multi",
	desc = "Several segments with a common intersection point",
	edges = {
	    { top = { x = 1, y = 2}, bottom = { x = 5, y = 4} },
	    { top = { x = 1, y = 1}, bottom = { x = 5, y = 5} },
	    { top = { x = 2, y = 1}, bottom = { x = 4, y = 5} },
	    { top = { x = 4, y = 1}, bottom = { x = 2, y = 5} },
	    { top = { x = 5, y = 1}, bottom = { x = 1, y = 5} },
	    { top = { x = 5, y = 2}, bottom = { x = 1, y = 4} }
	}
    }
};
*/

static int
run_test (const char		*test_name,
          cairo_bo_edge_t	*test_edges,
          int			 num_edges)
{
    int i, intersections, passes;
    cairo_bo_edge_t *edges;
    cairo_array_t intersected_edges;

    printf ("Testing: %s\n", test_name);

    _cairo_array_init (&intersected_edges, sizeof (cairo_bo_edge_t));

    intersections = _cairo_bentley_ottmann_intersect_edges (test_edges, num_edges, &intersected_edges);
    if (intersections)
	printf ("Pass 1 found %d intersections:\n", intersections);


    /* XXX: Multi-pass Bentley-Ottmmann. Preferable would be to add a
     * pass of Hobby's tolerance-square algorithm instead. */
    passes = 1;
    while (intersections) {
	int num_edges = _cairo_array_num_elements (&intersected_edges);
	passes++;
	edges = _cairo_malloc_ab (num_edges, sizeof (cairo_bo_edge_t));
	assert (edges != NULL);
	memcpy (edges, _cairo_array_index (&intersected_edges, 0), num_edges * sizeof (cairo_bo_edge_t));
	_cairo_array_fini (&intersected_edges);
	_cairo_array_init (&intersected_edges, sizeof (cairo_bo_edge_t));
	intersections = _cairo_bentley_ottmann_intersect_edges (edges, num_edges, &intersected_edges);
	free (edges);

	if (intersections){
	    printf ("Pass %d found %d remaining intersections:\n", passes, intersections);
	} else {
	    if (passes > 3)
		for (i = 0; i < passes; i++)
		    printf ("*");
	    printf ("No remainining intersections found after pass %d\n", passes);
	}
    }

    if (edges_have_an_intersection_quadratic (_cairo_array_index (&intersected_edges, 0),
					      _cairo_array_num_elements (&intersected_edges)))
	printf ("*** FAIL ***\n");
    else
	printf ("PASS\n");

    _cairo_array_fini (&intersected_edges);

    return 0;
}

#define MAX_RANDOM 300

int
main (void)
{
    char random_name[] = "random-XX";
    cairo_bo_edge_t random_edges[MAX_RANDOM], *edge;
    unsigned int i, num_random;
    test_t *test;

    for (i = 0; i < ARRAY_LENGTH (tests); i++) {
	test = &tests[i];
	run_test (test->name, test->edges, test->num_edges);
    }

    for (num_random = 0; num_random < MAX_RANDOM; num_random++) {
	srand (0);
	for (i = 0; i < num_random; i++) {
	    do {
		edge = &random_edges[i];
		edge->top.x = (int32_t) (10.0 * (rand() / (RAND_MAX + 1.0)));
		edge->top.y = (int32_t) (10.0 * (rand() / (RAND_MAX + 1.0)));
		edge->bottom.x = (int32_t) (10.0 * (rand() / (RAND_MAX + 1.0)));
		edge->bottom.y = (int32_t) (10.0 * (rand() / (RAND_MAX + 1.0)));
		if (edge->top.y > edge->bottom.y) {
		    int32_t tmp = edge->top.y;
		    edge->top.y = edge->bottom.y;
		    edge->bottom.y = tmp;
		}
	    } while (edge->top.y == edge->bottom.y);
	}

	sprintf (random_name, "random-%02d", num_random);

	run_test (random_name, random_edges, num_random);
    }

    return 0;
}
#endif

