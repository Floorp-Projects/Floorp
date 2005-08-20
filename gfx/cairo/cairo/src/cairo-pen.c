/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"

#include "cairo-gstate-private.h"

static int
_cairo_pen_vertices_needed (double tolerance, double radius, cairo_matrix_t *matrix);

static void
_cairo_pen_compute_slopes (cairo_pen_t *pen);

static cairo_status_t
_cairo_pen_stroke_spline_half (cairo_pen_t *pen, cairo_spline_t *spline, cairo_direction_t dir, cairo_polygon_t *polygon);

cairo_status_t
_cairo_pen_init_empty (cairo_pen_t *pen)
{
    pen->radius = 0;
    pen->tolerance = 0;
    pen->vertices = NULL;
    pen->num_vertices = 0;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_pen_init (cairo_pen_t *pen, double radius, cairo_gstate_t *gstate)
{
    int i;
    int reflect;
    double  det;

    if (pen->num_vertices) {
	/* XXX: It would be nice to notice that the pen is already properly constructed.
	   However, this test would also have to account for possible changes in the transformation
	   matrix.
	   if (pen->radius == radius && pen->tolerance == tolerance)
	   return CAIRO_STATUS_SUCCESS;
	*/
	_cairo_pen_fini (pen);
    }

    pen->radius = radius;
    pen->tolerance = gstate->tolerance;

    _cairo_matrix_compute_determinant (&gstate->ctm, &det);
    if (det >= 0) {
	reflect = 0;
    } else {
	reflect = 1;
    }

    pen->num_vertices = _cairo_pen_vertices_needed (gstate->tolerance,
						    radius,
						    &gstate->ctm);
    
    pen->vertices = malloc (pen->num_vertices * sizeof (cairo_pen_vertex_t));
    if (pen->vertices == NULL) {
	return CAIRO_STATUS_NO_MEMORY;
    }

    /*
     * Compute pen coordinates.  To generate the right ellipse, compute points around
     * a circle in user space and transform them to device space.  To get a consistent
     * orientation in device space, flip the pen if the transformation matrix
     * is reflecting
     */
    for (i=0; i < pen->num_vertices; i++) {
	double theta = 2 * M_PI * i / (double) pen->num_vertices;
	double dx = radius * cos (reflect ? -theta : theta);
	double dy = radius * sin (reflect ? -theta : theta);
	cairo_pen_vertex_t *v = &pen->vertices[i];
	cairo_matrix_transform_distance (&gstate->ctm, &dx, &dy);
	v->point.x = _cairo_fixed_from_double (dx);
	v->point.y = _cairo_fixed_from_double (dy);
    }

    _cairo_pen_compute_slopes (pen);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pen_fini (cairo_pen_t *pen)
{
    free (pen->vertices);
    pen->vertices = NULL;

    _cairo_pen_init_empty (pen);
}

cairo_status_t
_cairo_pen_init_copy (cairo_pen_t *pen, cairo_pen_t *other)
{
    *pen = *other;

    if (pen->num_vertices) {
	pen->vertices = malloc (pen->num_vertices * sizeof (cairo_pen_vertex_t));
	if (pen->vertices == NULL) {
	    return CAIRO_STATUS_NO_MEMORY;
	}
	memcpy (pen->vertices, other->vertices, pen->num_vertices * sizeof (cairo_pen_vertex_t));
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_pen_add_points (cairo_pen_t *pen, cairo_point_t *point, int num_points)
{
    cairo_pen_vertex_t *vertices;
    int num_vertices;
    int i;

    num_vertices = pen->num_vertices + num_points;
    vertices = realloc (pen->vertices, num_vertices * sizeof (cairo_pen_vertex_t));
    if (vertices == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    pen->vertices = vertices;
    pen->num_vertices = num_vertices;

    /* initialize new vertices */
    for (i=0; i < num_points; i++)
	pen->vertices[pen->num_vertices-num_points+i].point = point[i];

    _cairo_hull_compute (pen->vertices, &pen->num_vertices);

    _cairo_pen_compute_slopes (pen);

    return CAIRO_STATUS_SUCCESS;
}

/*
The circular pen in user space is transformed into an ellipse in
device space.

We construct the pen by computing points along the circumference
using equally spaced angles.

We show below that this approximation to the ellipse has
maximum error at the major axis of the ellipse.  
So, we need to compute the length of the major axis and then 
use that to compute the number of sides needed in our pen.

Thanks to Walter Brisken <wbrisken@aoc.nrao.edu> for this
derivation:

1.  First some notation:

All capital letters represent vectors in two dimensions.  A prime ' 
represents a transformed coordinate.  Matrices are written in underlined
form, ie _R_.  Lowercase letters represent scalar real values.

The letter t is used to represent the greek letter theta.

2.  The question has been posed:  What is the maximum expansion factor 
achieved by the linear transformation

X' = _R_ X

where _R_ is a real-valued 2x2 matrix with entries:

_R_ = [a b]
      [c d]  .

In other words, what is the maximum radius, MAX[ |X'| ], reached for any 
X on the unit circle ( |X| = 1 ) ?


3.  Some useful formulae

(A) through (C) below are standard double-angle formulae.  (D) is a lesser
known result and is derived below:

(A)  sin^2(t) = (1 - cos(2*t))/2
(B)  cos^2(t) = (1 + cos(2*t))/2
(C)  sin(t)*cos(t) = sin(2*t)/2
(D)  MAX[a*cos(t) + b*sin(t)] = sqrt(a^2 + b^2)

Proof of (D):

find the maximum of the function by setting the derivative to zero:

     -a*sin(t)+b*cos(t) = 0

From this it follows that 

     tan(t) = b/a 

and hence 

     sin(t) = b/sqrt(a^2 + b^2)

and 

     cos(t) = a/sqrt(a^2 + b^2)

Thus the maximum value is

     MAX[a*cos(t) + b*sin(t)] = (a^2 + b^2)/sqrt(a^2 + b^2)
                                 = sqrt(a^2 + b^2)


4.  Derivation of maximum expansion

To find MAX[ |X'| ] we search brute force method using calculus.  The unit
circle on which X is constrained is to be parameterized by t:

     X(t) = (cos(t), sin(t))

Thus 

     X'(t) = (a*cos(t) + b*sin(t), c*cos(t) + d*sin(t)) .

Define 

     r(t) = |X'(t)|

Thus

     r^2(t) = (a*cos(t) + b*sin(t))^2 + (c*cos(t) + d*sin(t))^2
            = (a^2 + c^2)*cos^2(t) + (b^2 + d^2)*sin^2(t) 
               + 2*(a*b + c*d)*cos(t)*sin(t) 

Now apply the double angle formulae (A) to (C) from above:

     r^2(t) = (a^2 + b^2 + c^2 + d^2)/2 
	  + (a^2 - b^2 + c^2 - d^2)*cos(2*t)/2
	  + (a*b + c*d)*sin(2*t)
            = f + g*cos(u) + h*sin(u)

Where

     f = (a^2 + b^2 + c^2 + d^2)/2
     g = (a^2 - b^2 + c^2 - d^2)/2
     h = (a*b + c*d)
     u = 2*t

It is clear that MAX[ |X'| ] = sqrt(MAX[ r^2 ]).  Here we determine MAX[ r^2 ]
using (D) from above:

     MAX[ r^2 ] = f + sqrt(g^2 + h^2)

And finally

     MAX[ |X'| ] = sqrt( f + sqrt(g^2 + h^2) )

Which is the solution to this problem.


Walter Brisken
2004/10/08

(Note that the minor axis length is at the minimum of the above solution,
which is just sqrt (f - sqrt (g^2 + h^2)) given the symmetry of (D)).

Now to compute how many sides to use for the pen formed by
a regular polygon.

Set

	    M = major axis length (computed by above formula)
	    m = minor axis length (computed by above formula)

Align 'M' along the X axis and 'm' along the Y axis and draw
an ellipse parameterized by angle 't':

	    x = M cos t			y = m sin t

Perturb t by ± d and compute two new points (x+,y+), (x-,y-).
The distance from the average of these two points to (x,y) represents
the maximum error in approximating the ellipse with a polygon formed
from vertices 2∆ radians apart.

	    x+ = M cos (t+∆)		y+ = m sin (t+∆)
	    x- = M cos (t-∆)		y- = m sin (t-∆)

Now compute the approximation error, E:

	Ex = (x - (x+ + x-) / 2)
	Ex = (M cos(t) - (Mcos(t+∆) + Mcos(t-∆))/2)
	   = M (cos(t) - (cos(t)cos(∆) + sin(t)sin(∆) +
			  cos(t)cos(∆) - sin(t)sin(∆))/2)
	   = M(cos(t) - cos(t)cos(∆))
	   = M cos(t) (1 - cos(∆))

	Ey = y - (y+ - y-) / 2
	   = m sin (t) - (m sin(t+∆) + m sin(t-∆)) / 2
	   = m (sin(t) - (sin(t)cos(∆) + cos(t)sin(∆) +
			  sin(t)cos(∆) - cos(t)sin(∆))/2)
	   = m (sin(t) - sin(t)cos(∆))
	   = m sin(t) (1 - cos(∆))

	E² = Ex² + Ey²
	   = (M cos(t) (1 - cos (∆)))² + (m sin(t) (1-cos(∆)))²
	   = (1 - cos(∆))² (M² cos²(t) + m² sin²(t))
	   = (1 - cos(∆))² ((m² + M² - m²) cos² (t) + m² sin²(t))
	   = (1 - cos(∆))² (M² - m²) cos² (t) + (1 - cos(∆))² m²

Find the extremum by differentiation wrt t and setting that to zero

∂(E²)/∂(t) = (1-cos(∆))² (M² - m²) (-2 cos(t) sin(t))

         0 = 2 cos (t) sin (t)
	 0 = sin (2t)
	 t = nπ

Which is to say that the maximum and minimum errors occur on the
axes of the ellipse at 0 and π radians:

	E²(0) = (1-cos(∆))² (M² - m²) + (1-cos(∆))² m²
	      = (1-cos(∆))² M²
	E²(π) = (1-cos(∆))² m²

maximum error = M (1-cos(∆))
minimum error = m (1-cos(∆))

We must make maximum error ≤ tolerance, so compute the ∆ needed:

	    tolerance = M (1-cos(∆))
	tolerance / M = 1 - cos (∆)
	       cos(∆) = 1 - tolerance/M
                    ∆ = acos (1 - tolerance / M);

Remembering that ∆ is half of our angle between vertices,
the number of vertices is then

vertices = ceil(2π/2∆).
		 = ceil(π/∆).

Note that this also equation works for M == m (a circle) as it
doesn't matter where on the circle the error is computed.

*/

static int
_cairo_pen_vertices_needed (double	    tolerance,
			    double	    radius,
			    cairo_matrix_t  *matrix)
{
    double  a = matrix->xx, b = matrix->yx;
    double  c = matrix->xy, d = matrix->yy;

    double  i = a*a + c*c;
    double  j = b*b + d*d;

    double  f = 0.5 * (i + j);
    double  g = 0.5 * (i - j);
    double  h = a*b + c*d;
    
    /* 
     * compute major and minor axes lengths for 
     * a pen with the specified radius 
     */
    
    double  major_axis = radius * sqrt (f + sqrt (g*g+h*h));

    /*
     * we don't need the minor axis length, which is
     * double min = radius * sqrt (f - sqrt (g*g+h*h));
     */
    
    /*
     * compute number of vertices needed
     */
    int	    num_vertices;
    
    /* Where tolerance / M is > 1, we use 4 points */
    if (tolerance >= major_axis) {
	num_vertices = 4;
    } else {
	double delta = acos (1 - tolerance / major_axis);
	num_vertices = ceil (M_PI / delta);
	
	/* number of vertices must be even */
	if (num_vertices % 2)
	    num_vertices++;
    }
    return num_vertices;
}

static void
_cairo_pen_compute_slopes (cairo_pen_t *pen)
{
    int i, i_prev;
    cairo_pen_vertex_t *prev, *v, *next;

    for (i=0, i_prev = pen->num_vertices - 1;
	 i < pen->num_vertices;
	 i_prev = i++) {
	prev = &pen->vertices[i_prev];
	v = &pen->vertices[i];
	next = &pen->vertices[(i + 1) % pen->num_vertices];

	_cairo_slope_init (&v->slope_cw, &prev->point, &v->point);
	_cairo_slope_init (&v->slope_ccw, &v->point, &next->point);
    }
}
/*
 * Find active pen vertex for clockwise edge of stroke at the given slope.
 *
 * NOTE: The behavior of this function is sensitive to the sense of
 * the inequality within _cairo_slope_clockwise/_cairo_slope_counter_clockwise.
 *
 * The issue is that the slope_ccw member of one pen vertex will be
 * equivalent to the slope_cw member of the next pen vertex in a
 * counterclockwise order. However, for this function, we care
 * strongly about which vertex is returned.
 */
cairo_status_t
_cairo_pen_find_active_cw_vertex_index (cairo_pen_t *pen,
					cairo_slope_t *slope,
					int *active)
{
    int i;

    for (i=0; i < pen->num_vertices; i++) {
	if (_cairo_slope_clockwise (slope, &pen->vertices[i].slope_ccw)
	    && _cairo_slope_counter_clockwise (slope, &pen->vertices[i].slope_cw))
	    break;
    }

    *active = i;

    return CAIRO_STATUS_SUCCESS;
}

/* Find active pen vertex for counterclockwise edge of stroke at the given slope.
 *
 * NOTE: The behavior of this function is sensitive to the sense of
 * the inequality within _cairo_slope_clockwise/_cairo_slope_counter_clockwise.
 */
cairo_status_t
_cairo_pen_find_active_ccw_vertex_index (cairo_pen_t *pen,
					 cairo_slope_t *slope,
					 int *active)
{
    int i;
    cairo_slope_t slope_reverse;

    slope_reverse = *slope;
    slope_reverse.dx = -slope_reverse.dx;
    slope_reverse.dy = -slope_reverse.dy;

    for (i=pen->num_vertices-1; i >= 0; i--) {
	if (_cairo_slope_counter_clockwise (&pen->vertices[i].slope_ccw, &slope_reverse)
	    && _cairo_slope_clockwise (&pen->vertices[i].slope_cw, &slope_reverse))
	    break;
    }

    *active = i;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pen_stroke_spline_half (cairo_pen_t *pen,
			       cairo_spline_t *spline,
			       cairo_direction_t dir,
			       cairo_polygon_t *polygon)
{
    int i;
    cairo_status_t status;
    int start, stop, step;
    int active = 0;
    cairo_point_t hull_point;
    cairo_slope_t slope, initial_slope, final_slope;
    cairo_point_t *point = spline->points;
    int num_points = spline->num_points;

    if (dir == CAIRO_DIRECTION_FORWARD) {
	start = 0;
	stop = num_points;
	step = 1;
	initial_slope = spline->initial_slope;
	final_slope = spline->final_slope;
    } else {
	start = num_points - 1;
	stop = -1;
	step = -1;
	initial_slope = spline->final_slope;
	initial_slope.dx = -initial_slope.dx;
	initial_slope.dy = -initial_slope.dy;
	final_slope = spline->initial_slope;
	final_slope.dx = -final_slope.dx; 
	final_slope.dy = -final_slope.dy; 
    }

    _cairo_pen_find_active_cw_vertex_index (pen, &initial_slope, &active);

    i = start;
    while (i != stop) {
	hull_point.x = point[i].x + pen->vertices[active].point.x;
	hull_point.y = point[i].y + pen->vertices[active].point.y;
	status = _cairo_polygon_line_to (polygon, &hull_point);
	if (status)
	    return status;

	if (i + step == stop)
	    slope = final_slope;
	else
	    _cairo_slope_init (&slope, &point[i], &point[i+step]);
	if (_cairo_slope_counter_clockwise (&slope, &pen->vertices[active].slope_ccw)) {
	    if (++active == pen->num_vertices)
		active = 0;
	} else if (_cairo_slope_clockwise (&slope, &pen->vertices[active].slope_cw)) {
	    if (--active == -1)
		active = pen->num_vertices - 1;
	} else {
	    i += step;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Compute outline of a given spline using the pen.
   The trapezoids needed to fill that outline will be added to traps
*/
cairo_status_t
_cairo_pen_stroke_spline (cairo_pen_t		*pen,
			  cairo_spline_t	*spline,
			  double		tolerance,
			  cairo_traps_t		*traps)
{
    cairo_status_t status;
    cairo_polygon_t polygon;

    /* If the line width is so small that the pen is reduced to a
       single point, then we have nothing to do. */
    if (pen->num_vertices <= 1)
	return CAIRO_STATUS_SUCCESS;

    _cairo_polygon_init (&polygon);

    status = _cairo_spline_decompose (spline, tolerance);
    if (status)
	return status;

    status = _cairo_pen_stroke_spline_half (pen, spline, CAIRO_DIRECTION_FORWARD, &polygon);
    if (status)
	return status;

    status = _cairo_pen_stroke_spline_half (pen, spline, CAIRO_DIRECTION_REVERSE, &polygon);
    if (status)
	return status;

    _cairo_polygon_close (&polygon);
    _cairo_traps_tessellate_polygon (traps, &polygon, CAIRO_FILL_RULE_WINDING);
    _cairo_polygon_fini (&polygon);
    
    return CAIRO_STATUS_SUCCESS;
}
