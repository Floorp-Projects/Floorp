/*
 * Copyright © 2002 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * 2002-07-15: Converted from XRenderCompositeDoublePoly to cairo_trap. Carl D. Worth
 */

#include "cairoint.h"

#define CAIRO_TRAPS_GROWTH_INC 10

/* private functions */

static cairo_status_t
_cairo_traps_grow_by (cairo_traps_t *traps, int additional);

static cairo_status_t
_cairo_traps_add_trap (cairo_traps_t *traps, cairo_fixed_t top, cairo_fixed_t bottom,
		       cairo_line_t *left, cairo_line_t *right);

static cairo_status_t
_cairo_traps_add_trap_from_points (cairo_traps_t *traps, cairo_fixed_t top, cairo_fixed_t bottom,
				   cairo_point_t left_p1, cairo_point_t left_p2,
				   cairo_point_t right_p1, cairo_point_t right_p2);

static int
_compare_point_fixed_by_y (const void *av, const void *bv);

static int
_compare_cairo_edge_by_top (const void *av, const void *bv);

static int
_compare_cairo_edge_by_slope (const void *av, const void *bv);

static cairo_fixed_16_16_t
_compute_x (cairo_line_t *line, cairo_fixed_t y);

static double
_compute_inverse_slope (cairo_line_t *l);

static double
_compute_x_intercept (cairo_line_t *l, double inverse_slope);

static int
_line_segs_intersect_ceil (cairo_line_t *left, cairo_line_t *right, cairo_fixed_t *y_ret);

void
_cairo_traps_init (cairo_traps_t *traps)
{
    traps->num_traps = 0;

    traps->traps_size = 0;
    traps->traps = NULL;
}

void
_cairo_traps_fini (cairo_traps_t *traps)
{
    if (traps->traps_size) {
	free (traps->traps);
	traps->traps = NULL;
	traps->traps_size = 0;
	traps->num_traps = 0;
    }
}

static cairo_status_t
_cairo_traps_add_trap (cairo_traps_t *traps, cairo_fixed_t top, cairo_fixed_t bottom,
		       cairo_line_t *left, cairo_line_t *right)
{
    cairo_status_t status;
    cairo_trapezoid_t *trap;

    if (top == bottom) {
	return CAIRO_STATUS_SUCCESS;
    }

    if (traps->num_traps >= traps->traps_size) {
	status = _cairo_traps_grow_by (traps, CAIRO_TRAPS_GROWTH_INC);
	if (status)
	    return status;
    }

    trap = &traps->traps[traps->num_traps];
    trap->top = top;
    trap->bottom = bottom;
    trap->left = *left;
    trap->right = *right;

    traps->num_traps++;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_traps_add_trap_from_points (cairo_traps_t *traps, cairo_fixed_t top, cairo_fixed_t bottom,
				   cairo_point_t left_p1, cairo_point_t left_p2,
				   cairo_point_t right_p1, cairo_point_t right_p2)
{
    cairo_line_t left;
    cairo_line_t right;

    left.p1 = left_p1;
    left.p2 = left_p2;

    right.p1 = right_p1;
    right.p2 = right_p2;

    return _cairo_traps_add_trap (traps, top, bottom, &left, &right);
}

static cairo_status_t
_cairo_traps_grow_by (cairo_traps_t *traps, int additional)
{
    cairo_trapezoid_t *new_traps;
    int old_size = traps->traps_size;
    int new_size = traps->num_traps + additional;

    if (new_size <= traps->traps_size) {
	return CAIRO_STATUS_SUCCESS;
    }

    traps->traps_size = new_size;
    new_traps = realloc (traps->traps, traps->traps_size * sizeof (cairo_trapezoid_t));

    if (new_traps == NULL) {
	traps->traps_size = old_size;
	return CAIRO_STATUS_NO_MEMORY;
    }

    traps->traps = new_traps;

    return CAIRO_STATUS_SUCCESS;
}

static int
_compare_point_fixed_by_y (const void *av, const void *bv)
{
    const cairo_point_t	*a = av, *b = bv;

    int ret = a->y - b->y;
    if (ret == 0) {
	ret = a->x - b->x;
    }
    return ret;
}

cairo_status_t
_cairo_traps_tessellate_triangle (cairo_traps_t *traps, cairo_point_t t[3])
{
    cairo_status_t status;
    cairo_line_t line;
    cairo_fixed_16_16_t intersect;
    cairo_point_t tsort[3];

    memcpy (tsort, t, 3 * sizeof (cairo_point_t));
    qsort (tsort, 3, sizeof (cairo_point_t), _compare_point_fixed_by_y);

    /* horizontal top edge requires special handling */
    if (tsort[0].y == tsort[1].y) {
	if (tsort[0].x < tsort[1].x)
	    status = _cairo_traps_add_trap_from_points (traps,
							tsort[1].y, tsort[2].y,
							tsort[0], tsort[2],
							tsort[1], tsort[2]);
	else
	    status = _cairo_traps_add_trap_from_points (traps,
							tsort[1].y, tsort[2].y,
							tsort[1], tsort[2],
							tsort[0], tsort[2]);
	return status;
    }

    line.p1 = tsort[0];
    line.p2 = tsort[1];

    intersect = _compute_x (&line, tsort[2].y);

    if (intersect < tsort[2].x) {
	status = _cairo_traps_add_trap_from_points (traps,
						    tsort[0].y, tsort[1].y,
						    tsort[0], tsort[1],
						    tsort[0], tsort[2]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    tsort[1].y, tsort[2].y,
						    tsort[1], tsort[2],
						    tsort[0], tsort[2]);
	if (status)
	    return status;
    } else {
	status = _cairo_traps_add_trap_from_points (traps,
						    tsort[0].y, tsort[1].y,
						    tsort[0], tsort[2],
						    tsort[0], tsort[1]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    tsort[1].y, tsort[2].y,
						    tsort[0], tsort[2],
						    tsort[1], tsort[2]);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Warning: This function reorders the elements of the array provided. */
cairo_status_t
_cairo_traps_tessellate_rectangle (cairo_traps_t *traps, cairo_point_t q[4])
{
    cairo_status_t status;

    qsort (q, 4, sizeof (cairo_point_t), _compare_point_fixed_by_y);

    if (q[1].x > q[2].x) {
	status = _cairo_traps_add_trap_from_points (traps,
						    q[0].y, q[1].y, q[0], q[2], q[0], q[1]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    q[1].y, q[2].y, q[0], q[2], q[1], q[3]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    q[2].y, q[3].y, q[2], q[3], q[1], q[3]);
	if (status)
	    return status;
    } else {
	status = _cairo_traps_add_trap_from_points (traps,
						    q[0].y, q[1].y, q[0], q[1], q[0], q[2]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    q[1].y, q[2].y, q[1], q[3], q[0], q[2]);
	if (status)
	    return status;
	status = _cairo_traps_add_trap_from_points (traps,
						    q[2].y, q[3].y, q[1], q[3], q[2], q[3]);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static int
_compare_cairo_edge_by_top (const void *av, const void *bv)
{
    const cairo_edge_t *a = av, *b = bv;

    return a->edge.p1.y - b->edge.p1.y;
}

/* Return value is:
   > 0 if a is "clockwise" from b, (in a mathematical, not a graphical sense)
   == 0 if slope (a) == slope (b)
   < 0 if a is "counter-clockwise" from b
*/
static int
_compare_cairo_edge_by_slope (const void *av, const void *bv)
{
    const cairo_edge_t *a = av, *b = bv;
    cairo_fixed_32_32_t d;

    cairo_fixed_48_16_t a_dx = a->edge.p2.x - a->edge.p1.x;
    cairo_fixed_48_16_t a_dy = a->edge.p2.y - a->edge.p1.y;
    cairo_fixed_48_16_t b_dx = b->edge.p2.x - b->edge.p1.x;
    cairo_fixed_48_16_t b_dy = b->edge.p2.y - b->edge.p1.y;

    d = b_dy * a_dx - a_dy * b_dx;

    if (d > 0)
	return 1;
    else if (d == 0)
	return 0;
    else
	return -1;
}

static int
_compare_cairo_edge_by_current_x_slope (const void *av, const void *bv)
{
    const cairo_edge_t *a = av, *b = bv;
    int ret;

    ret = a->current_x - b->current_x;
    if (ret == 0)
	ret = _compare_cairo_edge_by_slope (a, b);
    return ret;
}

/* XXX: Both _compute_x and _compute_inverse_slope will divide by zero
   for horizontal lines. Now, we "know" that when we are tessellating
   polygons that the polygon data structure discards all horizontal
   edges, but there's nothing here to guarantee that. I suggest the
   following:

   A) Move all of the polygon tessellation code out of xrtraps.c and
      into xrpoly.c, (in order to be in the same module as the code
      discarding horizontal lines).

   OR

   B) Re-implement the line intersection in a way that avoids all
      division by zero. Here's one approach. The only disadvantage
      might be that that there are not meaningful names for all of the
      sub-computations -- just a bunch of determinants. I haven't
      looked at complexity, (both are probably similar and it probably
      doesn't matter much anyway).

static double
_det (double a, double b, double c, double d)
{
    return a * d - b * c;
}

static int
_lines_intersect (cairo_line_t *l1, cairo_line_t *l2, cairo_fixed_t *y_intersection)
{
    double dx1 = cairo_fixed_to_double (l1->p1.x - l1->p2.x);
    double dy1 = cairo_fixed_to_double (l1->p1.y - l1->p2.y);

    double dx2 = cairo_fixed_to_double (l2->p1.x - l2->p2.x);
    double dy2 = cairo_fixed_to_double (l2->p1.y - l2->p2.y);

    double l1_det, l2_det;

    double den_det = _det (dx1, dy1, dx2, dy2);

    if (den_det == 0)
	return 0;

    l1_det = _det (l1->p1.x, l1->p1.y,
		  l1->p2.x, l1->p2.y);
    l2_det = _det (l2->p1.x, l2->p1.y,
		  l2->p2.x, l2->p2.y);

    *y_intersection = _det (l1_det, dy1,
			   l2_det, dy2) / den_det;

    return 1;
}
*/
static cairo_fixed_16_16_t
_compute_x (cairo_line_t *line, cairo_fixed_t y)
{
    cairo_fixed_16_16_t dx = line->p2.x - line->p1.x;
    cairo_fixed_32_32_t ex = (cairo_fixed_48_16_t) (y - line->p1.y) * (cairo_fixed_48_16_t) dx;
    cairo_fixed_16_16_t dy = line->p2.y - line->p1.y;

    return line->p1.x + (ex / dy);
}

static double
_compute_inverse_slope (cairo_line_t *l)
{
    return (_cairo_fixed_to_double (l->p2.x - l->p1.x) / 
	    _cairo_fixed_to_double (l->p2.y - l->p1.y));
}

static double
_compute_x_intercept (cairo_line_t *l, double inverse_slope)
{
    return _cairo_fixed_to_double (l->p1.x) - inverse_slope * _cairo_fixed_to_double (l->p1.y);
}

static int
_line_segs_intersect_ceil (cairo_line_t *l1, cairo_line_t *l2, cairo_fixed_t *y_ret)
{
    /*
     * x = m1y + b1
     * x = m2y + b2
     * m1y + b1 = m2y + b2
     * y * (m1 - m2) = b2 - b1
     * y = (b2 - b1) / (m1 - m2)
     */
    cairo_fixed_16_16_t y_intersect;
    double  m1 = _compute_inverse_slope (l1);
    double  b1 = _compute_x_intercept (l1, m1);
    double  m2 = _compute_inverse_slope (l2);
    double  b2 = _compute_x_intercept (l2, m2);

    if (m1 == m2)
	return 0;

    y_intersect = _cairo_fixed_from_double ((b2 - b1) / (m1 - m2));

    if (m1 < m2) {
	cairo_line_t *t;
	t = l1;
	l1 = l2;
	l2 = t;
    }

    /* Assuming 56 bits of floating point precision, the intersection
       is accurate within one sub-pixel coordinate. We must ensure
       that we return a value that is at or after the intersection. At
       most, we must increment once. */
    if (_compute_x (l2, y_intersect) > _compute_x (l1, y_intersect))
	y_intersect++;
    /* XXX: Hmm... Keith's error calculations said we'd at most be off
       by one sub-pixel. But, I found that the paint-fill-BE-01.svg
       test from the W3C SVG conformance suite definitely requires two
       increments.

       It could be that we need one to overcome the error, and another
       to round up.

       It would be nice to be sure this code is correct, (but we can't
       do the while loop as it will work for way to long on
       exceedingly distant intersections with large errors that we
       really don't care about anyway as they will be ignored by the
       calling function.
    */
    if (_compute_x (l2, y_intersect) > _compute_x (l1, y_intersect))
	y_intersect++;
    /* XXX: hmm... now I found "intersection_killer" inside xrspline.c
       that requires 3 increments. Clearly, we haven't characterized
       this completely yet. */
    if (_compute_x (l2, y_intersect) > _compute_x (l1, y_intersect))
	y_intersect++;
    /* I think I've found the answer to our problems. The insight is
       that everytime we round we are changing the slopes of the
       relevant lines, so we may be introducing new intersections that
       we miss, so everything breaks apart. John Hobby wrote a paper
       on how to fix this:

       [Hobby93c] John D. Hobby, Practical Segment Intersection with
       Finite Precision Output, Computation Geometry Theory and
       Applications, 13(4), 1999.

       Available online (2003-08017):

       http://cm.bell-labs.com/cm/cs/doc/93/2-27.ps.gz

       Now we just need to go off and implement that.
    */

    *y_ret = y_intersect;

    return 1;
}

/* The algorithm here is pretty simple:

   inactive = [edges]
   y = min_p1_y (inactive)

   while (num_active || num_inactive) {
   	active = all edges containing y

	next_y = min ( min_p2_y (active), min_p1_y (inactive), min_intersection (active) )

	fill_traps (active, y, next_y, fill_rule)

	y = next_y
   }

   The invariants that hold during fill_traps are:

   	All edges in active contain both y and next_y
	No edges in active intersect within y and next_y

   These invariants mean that fill_traps is as simple as sorting the
   active edges, forming a trapezoid between each adjacent pair. Then,
   either the even-odd or winding rule is used to determine whether to
   emit each of these trapezoids.

   Warning: This function obliterates the edges of the polygon provided.
*/
cairo_status_t
_cairo_traps_tessellate_polygon (cairo_traps_t		*traps,
				 cairo_polygon_t	*poly,
				 cairo_fill_rule_t	fill_rule)
{
    cairo_status_t	status;
    int 		i, active, inactive;
    cairo_fixed_t	y, y_next, intersect;
    int			in_out, num_edges = poly->num_edges;
    cairo_edge_t	*edges = poly->edges;

    if (num_edges == 0)
	return CAIRO_STATUS_SUCCESS;

    qsort (edges, num_edges, sizeof (cairo_edge_t), _compare_cairo_edge_by_top);
    
    y = edges[0].edge.p1.y;
    active = 0;
    inactive = 0;
    while (active < num_edges) {
	while (inactive < num_edges && edges[inactive].edge.p1.y <= y)
	    inactive++;

	for (i = active; i < inactive; i++)
	    edges[i].current_x = _compute_x (&edges[i].edge, y);

	qsort (&edges[active], inactive - active,
	       sizeof (cairo_edge_t), _compare_cairo_edge_by_current_x_slope);

	/* find next inflection point */
	y_next = edges[active].edge.p2.y;

	for (i = active; i < inactive; i++) {
	    if (edges[i].edge.p2.y < y_next)
		y_next = edges[i].edge.p2.y;
	    /* check intersect */
	    if (i != inactive - 1 && edges[i].current_x != edges[i+1].current_x)
		if (_line_segs_intersect_ceil (&edges[i].edge, &edges[i+1].edge,
					       &intersect))
		    if (intersect > y && intersect < y_next)
			y_next = intersect;
	}
	/* check next inactive point */
	if (inactive < num_edges && edges[inactive].edge.p1.y < y_next)
	    y_next = edges[inactive].edge.p1.y;

	/* walk the active edges generating trapezoids */
	in_out = 0;
	for (i = active; i < inactive - 1; i++) {
	    if (fill_rule == CAIRO_FILL_RULE_WINDING) {
		if (edges[i].clockWise)
		    in_out++;
		else
		    in_out--;
		if (in_out == 0)
		    continue;
	    } else {
		in_out++;
		if ((in_out & 1) == 0)
		    continue;
	    }
	    status = _cairo_traps_add_trap (traps, y, y_next, &edges[i].edge, &edges[i+1].edge);
	    if (status)
		return status;
	}

	/* delete inactive edges */
	for (i = active; i < inactive; i++) {
	    if (edges[i].edge.p2.y <= y_next) {
		memmove (&edges[active+1], &edges[active], (i - active) * sizeof (cairo_edge_t));
		active++;
	    }
	}

	y = y_next;
    }
    return CAIRO_STATUS_SUCCESS;
}

static int
_cairo_trap_contains (cairo_trapezoid_t *t, cairo_point_t *pt)
{
    cairo_slope_t slope_left, slope_pt, slope_right;
    
    if (t->top > pt->y)
	return 0;
    if (t->bottom < pt->y)
	return 0;
    
    _cairo_slope_init (&slope_left, &t->left.p1, &t->left.p2);
    _cairo_slope_init (&slope_pt, &t->left.p1, pt);

    if (_cairo_slope_compare (&slope_left, &slope_pt) < 0)
	return 0;

    _cairo_slope_init (&slope_right, &t->right.p1, &t->right.p2);
    _cairo_slope_init (&slope_pt, &t->right.p1, pt);

    if (_cairo_slope_compare (&slope_pt, &slope_right) < 0)
	return 0;

    return 1;
}

int
_cairo_traps_contain (cairo_traps_t *traps, double x, double y)
{
    int i;
    cairo_point_t point;

    point.x = _cairo_fixed_from_double (x);
    point.y = _cairo_fixed_from_double (y);

    for (i = 0; i < traps->num_traps; i++) {
	if (_cairo_trap_contains (&traps->traps[i], &point))
	    return 1;
    }

    return 0;
}

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static void
_cairo_trap_extents (cairo_trapezoid_t *t, cairo_box_t *extents)
{
    cairo_fixed_t x;
    
    if (t->top < extents->p1.y)
	extents->p1.y = t->top;
    
    if (t->bottom > extents->p2.y)
	extents->p2.y = t->bottom;
    
    x = MIN (_compute_x (&t->left, t->top),
	     _compute_x (&t->left, t->bottom));
    if (x < extents->p1.x)
	extents->p1.x = x;
    
    x = MAX (_compute_x (&t->right, t->top),
	     _compute_x (&t->right, t->bottom));
    if (x > extents->p2.x)
	extents->p2.x = x;
}

void
_cairo_traps_extents (cairo_traps_t *traps, cairo_box_t *extents)
{
    int i;
  
    extents->p1.x = extents->p1.y = CAIRO_MAXSHORT << 16;
    extents->p2.x = extents->p2.y = CAIRO_MINSHORT << 16;
    
    for (i = 0; i < traps->num_traps; i++)
	_cairo_trap_extents (&traps->traps[i], extents);
}
