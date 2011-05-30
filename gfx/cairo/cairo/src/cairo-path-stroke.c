/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#define _BSD_SOURCE /* for hypot() */
#include "cairoint.h"

#include "cairo-boxes-private.h"
#include "cairo-error-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-slope-private.h"

typedef struct _cairo_stroker_dash {
    cairo_bool_t dashed;
    unsigned int dash_index;
    cairo_bool_t dash_on;
    cairo_bool_t dash_starts_on;
    double dash_remain;

    double dash_offset;
    const double *dashes;
    unsigned int num_dashes;
} cairo_stroker_dash_t;

typedef struct cairo_stroker {
    cairo_stroke_style_t style;

    const cairo_matrix_t *ctm;
    const cairo_matrix_t *ctm_inverse;
    double tolerance;
    double ctm_determinant;
    cairo_bool_t ctm_det_positive;

    void *closure;
    cairo_status_t (*add_external_edge) (void *closure,
					 const cairo_point_t *p1,
					 const cairo_point_t *p2);
    cairo_status_t (*add_triangle) (void *closure,
				    const cairo_point_t triangle[3]);
    cairo_status_t (*add_triangle_fan) (void *closure,
					const cairo_point_t *midpt,
					const cairo_point_t *points,
					int npoints);
    cairo_status_t (*add_convex_quad) (void *closure,
				       const cairo_point_t quad[4]);

    cairo_pen_t	  pen;

    cairo_point_t current_point;
    cairo_point_t first_point;

    cairo_bool_t has_initial_sub_path;

    cairo_bool_t has_current_face;
    cairo_stroke_face_t current_face;

    cairo_bool_t has_first_face;
    cairo_stroke_face_t first_face;

    cairo_stroker_dash_t dash;

    cairo_bool_t has_bounds;
    cairo_box_t bounds;
} cairo_stroker_t;

static void
_cairo_stroker_dash_start (cairo_stroker_dash_t *dash)
{
    double offset;
    cairo_bool_t on = TRUE;
    unsigned int i = 0;

    if (! dash->dashed)
	return;

    offset = dash->dash_offset;

    /* We stop searching for a starting point as soon as the
       offset reaches zero.  Otherwise when an initial dash
       segment shrinks to zero it will be skipped over. */
    while (offset > 0.0 && offset >= dash->dashes[i]) {
	offset -= dash->dashes[i];
	on = !on;
	if (++i == dash->num_dashes)
	    i = 0;
    }

    dash->dash_index = i;
    dash->dash_on = dash->dash_starts_on = on;
    dash->dash_remain = dash->dashes[i] - offset;
}

static void
_cairo_stroker_dash_step (cairo_stroker_dash_t *dash, double step)
{
    dash->dash_remain -= step;
    if (dash->dash_remain <= 0.) {
	if (++dash->dash_index == dash->num_dashes)
	    dash->dash_index = 0;

	dash->dash_on = ! dash->dash_on;
	dash->dash_remain = dash->dashes[dash->dash_index];
    }
}

static void
_cairo_stroker_dash_init (cairo_stroker_dash_t *dash,
			  const cairo_stroke_style_t *style)
{
    dash->dashed = style->dash != NULL;
    if (! dash->dashed)
	return;

    dash->dashes = style->dash;
    dash->num_dashes = style->num_dashes;
    dash->dash_offset = style->dash_offset;

    _cairo_stroker_dash_start (dash);
}

static cairo_status_t
_cairo_stroker_init (cairo_stroker_t		*stroker,
		     const cairo_stroke_style_t	*stroke_style,
		     const cairo_matrix_t	*ctm,
		     const cairo_matrix_t	*ctm_inverse,
		     double			 tolerance)
{
    cairo_status_t status;

    stroker->style = *stroke_style;
    stroker->ctm = ctm;
    stroker->ctm_inverse = ctm_inverse;
    stroker->tolerance = tolerance;

    stroker->ctm_determinant = _cairo_matrix_compute_determinant (stroker->ctm);
    stroker->ctm_det_positive = stroker->ctm_determinant >= 0.0;

    status = _cairo_pen_init (&stroker->pen,
		              stroke_style->line_width / 2.0,
			      tolerance, ctm);
    if (unlikely (status))
	return status;

    stroker->has_bounds = FALSE;

    stroker->has_current_face = FALSE;
    stroker->has_first_face = FALSE;
    stroker->has_initial_sub_path = FALSE;

    _cairo_stroker_dash_init (&stroker->dash, stroke_style);

    stroker->add_external_edge = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_stroker_limit (cairo_stroker_t *stroker,
		      const cairo_box_t *boxes,
		      int num_boxes)
{
    double dx, dy;
    cairo_fixed_t fdx, fdy;

    stroker->has_bounds = TRUE;
    _cairo_boxes_get_extents (boxes, num_boxes, &stroker->bounds);

    /* Extend the bounds in each direction to account for the maximum area
     * we might generate trapezoids, to capture line segments that are outside
     * of the bounds but which might generate rendering that's within bounds.
     */

    _cairo_stroke_style_max_distance_from_path (&stroker->style, stroker->ctm,
						&dx, &dy);

    fdx = _cairo_fixed_from_double (dx);
    fdy = _cairo_fixed_from_double (dy);

    stroker->bounds.p1.x -= fdx;
    stroker->bounds.p2.x += fdx;

    stroker->bounds.p1.y -= fdy;
    stroker->bounds.p2.y += fdy;
}

static void
_cairo_stroker_fini (cairo_stroker_t *stroker)
{
    _cairo_pen_fini (&stroker->pen);
}

static void
_translate_point (cairo_point_t *point, const cairo_point_t *offset)
{
    point->x += offset->x;
    point->y += offset->y;
}

static int
_cairo_stroker_join_is_clockwise (const cairo_stroke_face_t *in,
				  const cairo_stroke_face_t *out)
{
    cairo_slope_t in_slope, out_slope;

    _cairo_slope_init (&in_slope, &in->point, &in->cw);
    _cairo_slope_init (&out_slope, &out->point, &out->cw);

    return _cairo_slope_compare (&in_slope, &out_slope) < 0;
}

/**
 * _cairo_slope_compare_sgn
 *
 * Return -1, 0 or 1 depending on the relative slopes of
 * two lines.
 */
static int
_cairo_slope_compare_sgn (double dx1, double dy1, double dx2, double dy2)
{
    double  c = (dx1 * dy2 - dx2 * dy1);

    if (c > 0) return 1;
    if (c < 0) return -1;
    return 0;
}

static inline int
_range_step (int i, int step, int max)
{
    i += step;
    if (i < 0)
	i = max - 1;
    if (i >= max)
	i = 0;
    return i;
}

/*
 * Construct a fan around the midpoint using the vertices from pen between
 * inpt and outpt.
 */
static cairo_status_t
_tessellate_fan (cairo_stroker_t *stroker,
		 const cairo_slope_t *in_vector,
		 const cairo_slope_t *out_vector,
		 const cairo_point_t *midpt,
		 const cairo_point_t *inpt,
		 const cairo_point_t *outpt,
		 cairo_bool_t clockwise)
{
    cairo_point_t stack_points[64], *points = stack_points;
    int start, stop, step, i, npoints;
    cairo_status_t status;

    if (clockwise) {
	step  = -1;

	start = _cairo_pen_find_active_ccw_vertex_index (&stroker->pen,
							 in_vector);
	if (_cairo_slope_compare (&stroker->pen.vertices[start].slope_ccw,
				  in_vector) < 0)
	    start = _range_step (start, -1, stroker->pen.num_vertices);

	stop  = _cairo_pen_find_active_ccw_vertex_index (&stroker->pen,
							 out_vector);
	if (_cairo_slope_compare (&stroker->pen.vertices[stop].slope_cw,
				  out_vector) > 0)
	{
	    stop = _range_step (stop, 1, stroker->pen.num_vertices);
	    if (_cairo_slope_compare (&stroker->pen.vertices[stop].slope_ccw,
				      in_vector) < 0)
	    {
		goto BEVEL;
	    }
	}

	npoints = start - stop;
    } else {
	step  = 1;

	start = _cairo_pen_find_active_cw_vertex_index (&stroker->pen,
							in_vector);
	if (_cairo_slope_compare (&stroker->pen.vertices[start].slope_cw,
				  in_vector) < 0)
	    start = _range_step (start, 1, stroker->pen.num_vertices);

	stop  = _cairo_pen_find_active_cw_vertex_index (&stroker->pen,
							out_vector);
	if (_cairo_slope_compare (&stroker->pen.vertices[stop].slope_ccw,
				  out_vector) > 0)
	{
	    stop = _range_step (stop, -1, stroker->pen.num_vertices);
	    if (_cairo_slope_compare (&stroker->pen.vertices[stop].slope_cw,
				      in_vector) < 0)
	    {
		goto BEVEL;
	    }
	}

	npoints = stop - start;
    }
    stop = _range_step (stop, step, stroker->pen.num_vertices);

    if (npoints < 0)
	npoints += stroker->pen.num_vertices;
    npoints += 3;

    if (npoints <= 1)
	goto BEVEL;

    if (npoints > ARRAY_LENGTH (stack_points)) {
	points = _cairo_malloc_ab (npoints, sizeof (cairo_point_t));
	if (unlikely (points == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }


    /* Construct the fan. */
    npoints = 0;
    points[npoints++] = *inpt;
    for (i = start;
	 i != stop;
	i = _range_step (i, step, stroker->pen.num_vertices))
    {
	points[npoints] = *midpt;
	_translate_point (&points[npoints], &stroker->pen.vertices[i].point);
	npoints++;
    }
    points[npoints++] = *outpt;

    if (stroker->add_external_edge != NULL) {
	for (i = 0; i < npoints - 1; i++) {
	    if (clockwise) {
		status = stroker->add_external_edge (stroker->closure,
						     &points[i], &points[i+1]);
	    } else {
		status = stroker->add_external_edge (stroker->closure,
						     &points[i+1], &points[i]);
	    }
	    if (unlikely (status))
		break;
	}
    } else {
	status = stroker->add_triangle_fan (stroker->closure,
					    midpt, points, npoints);
    }

    if (points != stack_points)
	free (points);

    return status;

BEVEL:
    /* Ensure a leak free connection... */
    if (stroker->add_external_edge != NULL) {
	if (clockwise)
	    return stroker->add_external_edge (stroker->closure, inpt, outpt);
	else
	    return stroker->add_external_edge (stroker->closure, outpt, inpt);
    } else {
	stack_points[0] = *midpt;
	stack_points[1] = *inpt;
	stack_points[2] = *outpt;
	return stroker->add_triangle (stroker->closure, stack_points);
    }
}

static cairo_status_t
_cairo_stroker_join (cairo_stroker_t *stroker,
		     const cairo_stroke_face_t *in,
		     const cairo_stroke_face_t *out)
{
    int	 clockwise = _cairo_stroker_join_is_clockwise (out, in);
    const cairo_point_t	*inpt, *outpt;
    cairo_point_t points[4];
    cairo_status_t status;

    if (in->cw.x  == out->cw.x  && in->cw.y  == out->cw.y &&
	in->ccw.x == out->ccw.x && in->ccw.y == out->ccw.y)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    if (clockwise) {
	if (stroker->add_external_edge != NULL) {
	    status = stroker->add_external_edge (stroker->closure,
						 &out->cw, &in->point);
	    if (unlikely (status))
		return status;

	    status = stroker->add_external_edge (stroker->closure,
						 &in->point, &in->cw);
	    if (unlikely (status))
		return status;
	}

	inpt = &in->ccw;
	outpt = &out->ccw;
    } else {
	if (stroker->add_external_edge != NULL) {
	    status = stroker->add_external_edge (stroker->closure,
						 &in->ccw, &in->point);
	    if (unlikely (status))
		return status;

	    status = stroker->add_external_edge (stroker->closure,
						 &in->point, &out->ccw);
	    if (unlikely (status))
		return status;
	}

	inpt = &in->cw;
	outpt = &out->cw;
    }

    switch (stroker->style.line_join) {
    case CAIRO_LINE_JOIN_ROUND:
	/* construct a fan around the common midpoint */
	return _tessellate_fan (stroker,
				&in->dev_vector,
				&out->dev_vector,
				&in->point, inpt, outpt,
				clockwise);

    case CAIRO_LINE_JOIN_MITER:
    default: {
	/* dot product of incoming slope vector with outgoing slope vector */
	double	in_dot_out = -in->usr_vector.x * out->usr_vector.x +
			     -in->usr_vector.y * out->usr_vector.y;
	double	ml = stroker->style.miter_limit;

	/* Check the miter limit -- lines meeting at an acute angle
	 * can generate long miters, the limit converts them to bevel
	 *
	 * Consider the miter join formed when two line segments
	 * meet at an angle psi:
	 *
	 *	   /.\
	 *	  /. .\
	 *	 /./ \.\
	 *	/./psi\.\
	 *
	 * We can zoom in on the right half of that to see:
	 *
	 *	    |\
	 *	    | \ psi/2
	 *	    |  \
	 *	    |   \
	 *	    |    \
	 *	    |     \
	 *	  miter    \
	 *	 length     \
	 *	    |        \
	 *	    |        .\
	 *	    |    .     \
	 *	    |.   line   \
	 *	     \    width  \
	 *	      \           \
	 *
	 *
	 * The right triangle in that figure, (the line-width side is
	 * shown faintly with three '.' characters), gives us the
	 * following expression relating miter length, angle and line
	 * width:
	 *
	 *	1 /sin (psi/2) = miter_length / line_width
	 *
	 * The right-hand side of this relationship is the same ratio
	 * in which the miter limit (ml) is expressed. We want to know
	 * when the miter length is within the miter limit. That is
	 * when the following condition holds:
	 *
	 *	1/sin(psi/2) <= ml
	 *	1 <= ml sin(psi/2)
	 *	1 <= ml² sin²(psi/2)
	 *	2 <= ml² 2 sin²(psi/2)
	 *				2·sin²(psi/2) = 1-cos(psi)
	 *	2 <= ml² (1-cos(psi))
	 *
	 *				in · out = |in| |out| cos (psi)
	 *
	 * in and out are both unit vectors, so:
	 *
	 *				in · out = cos (psi)
	 *
	 *	2 <= ml² (1 - in · out)
	 *
	 */
	if (2 <= ml * ml * (1 - in_dot_out)) {
	    double		x1, y1, x2, y2;
	    double		mx, my;
	    double		dx1, dx2, dy1, dy2;
	    double		ix, iy;
	    double		fdx1, fdy1, fdx2, fdy2;
	    double		mdx, mdy;

	    /*
	     * we've got the points already transformed to device
	     * space, but need to do some computation with them and
	     * also need to transform the slope from user space to
	     * device space
	     */
	    /* outer point of incoming line face */
	    x1 = _cairo_fixed_to_double (inpt->x);
	    y1 = _cairo_fixed_to_double (inpt->y);
	    dx1 = in->usr_vector.x;
	    dy1 = in->usr_vector.y;
	    cairo_matrix_transform_distance (stroker->ctm, &dx1, &dy1);

	    /* outer point of outgoing line face */
	    x2 = _cairo_fixed_to_double (outpt->x);
	    y2 = _cairo_fixed_to_double (outpt->y);
	    dx2 = out->usr_vector.x;
	    dy2 = out->usr_vector.y;
	    cairo_matrix_transform_distance (stroker->ctm, &dx2, &dy2);

	    /*
	     * Compute the location of the outer corner of the miter.
	     * That's pretty easy -- just the intersection of the two
	     * outer edges.  We've got slopes and points on each
	     * of those edges.  Compute my directly, then compute
	     * mx by using the edge with the larger dy; that avoids
	     * dividing by values close to zero.
	     */
	    my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2) /
		  (dx1 * dy2 - dx2 * dy1));
	    if (fabs (dy1) >= fabs (dy2))
		mx = (my - y1) * dx1 / dy1 + x1;
	    else
		mx = (my - y2) * dx2 / dy2 + x2;

	    /*
	     * When the two outer edges are nearly parallel, slight
	     * perturbations in the position of the outer points of the lines
	     * caused by representing them in fixed point form can cause the
	     * intersection point of the miter to move a large amount. If
	     * that moves the miter intersection from between the two faces,
	     * then draw a bevel instead.
	     */

	    ix = _cairo_fixed_to_double (in->point.x);
	    iy = _cairo_fixed_to_double (in->point.y);

	    /* slope of one face */
	    fdx1 = x1 - ix; fdy1 = y1 - iy;

	    /* slope of the other face */
	    fdx2 = x2 - ix; fdy2 = y2 - iy;

	    /* slope from the intersection to the miter point */
	    mdx = mx - ix; mdy = my - iy;

	    /*
	     * Make sure the miter point line lies between the two
	     * faces by comparing the slopes
	     */
	    if (_cairo_slope_compare_sgn (fdx1, fdy1, mdx, mdy) !=
		_cairo_slope_compare_sgn (fdx2, fdy2, mdx, mdy))
	    {
		if (stroker->add_external_edge != NULL) {
		    points[0].x = _cairo_fixed_from_double (mx);
		    points[0].y = _cairo_fixed_from_double (my);

		    if (clockwise) {
			status = stroker->add_external_edge (stroker->closure,
							     inpt, &points[0]);
			if (unlikely (status))
			    return status;

			status = stroker->add_external_edge (stroker->closure,
							     &points[0], outpt);
			if (unlikely (status))
			    return status;
		    } else {
			status = stroker->add_external_edge (stroker->closure,
							     outpt, &points[0]);
			if (unlikely (status))
			    return status;

			status = stroker->add_external_edge (stroker->closure,
							     &points[0], inpt);
			if (unlikely (status))
			    return status;
		    }

		    return CAIRO_STATUS_SUCCESS;
		} else {
		    points[0] = in->point;
		    points[1] = *inpt;
		    points[2].x = _cairo_fixed_from_double (mx);
		    points[2].y = _cairo_fixed_from_double (my);
		    points[3] = *outpt;

		    return stroker->add_convex_quad (stroker->closure, points);
		}
	    }
	}
    }

    /* fall through ... */

    case CAIRO_LINE_JOIN_BEVEL:
	if (stroker->add_external_edge != NULL) {
	    if (clockwise) {
		return stroker->add_external_edge (stroker->closure,
						   inpt, outpt);
	    } else {
		return stroker->add_external_edge (stroker->closure,
						   outpt, inpt);
	    }
	} else {
	    points[0] = in->point;
	    points[1] = *inpt;
	    points[2] = *outpt;

	    return stroker->add_triangle (stroker->closure, points);
	}
    }
}

static cairo_status_t
_cairo_stroker_add_cap (cairo_stroker_t *stroker,
			const cairo_stroke_face_t *f)
{
    switch (stroker->style.line_cap) {
    case CAIRO_LINE_CAP_ROUND: {
	cairo_slope_t slope;

	slope.dx = -f->dev_vector.dx;
	slope.dy = -f->dev_vector.dy;

	return _tessellate_fan (stroker,
				&f->dev_vector,
				&slope,
				&f->point, &f->cw, &f->ccw,
				FALSE);

    }

    case CAIRO_LINE_CAP_SQUARE: {
	double dx, dy;
	cairo_slope_t	fvector;
	cairo_point_t	quad[4];

	dx = f->usr_vector.x;
	dy = f->usr_vector.y;
	dx *= stroker->style.line_width / 2.0;
	dy *= stroker->style.line_width / 2.0;
	cairo_matrix_transform_distance (stroker->ctm, &dx, &dy);
	fvector.dx = _cairo_fixed_from_double (dx);
	fvector.dy = _cairo_fixed_from_double (dy);

	quad[0] = f->ccw;
	quad[1].x = f->ccw.x + fvector.dx;
	quad[1].y = f->ccw.y + fvector.dy;
	quad[2].x = f->cw.x + fvector.dx;
	quad[2].y = f->cw.y + fvector.dy;
	quad[3] = f->cw;

	if (stroker->add_external_edge != NULL) {
	    cairo_status_t status;

	    status = stroker->add_external_edge (stroker->closure,
						 &quad[0], &quad[1]);
	    if (unlikely (status))
		return status;

	    status = stroker->add_external_edge (stroker->closure,
						 &quad[1], &quad[2]);
	    if (unlikely (status))
		return status;

	    status = stroker->add_external_edge (stroker->closure,
						 &quad[2], &quad[3]);
	    if (unlikely (status))
		return status;

	    return CAIRO_STATUS_SUCCESS;
	} else {
	    return stroker->add_convex_quad (stroker->closure, quad);
	}
    }

    case CAIRO_LINE_CAP_BUTT:
    default:
	if (stroker->add_external_edge != NULL) {
	    return stroker->add_external_edge (stroker->closure,
					       &f->ccw, &f->cw);
	} else {
	    return CAIRO_STATUS_SUCCESS;
	}
    }
}

static cairo_status_t
_cairo_stroker_add_leading_cap (cairo_stroker_t     *stroker,
				const cairo_stroke_face_t *face)
{
    cairo_stroke_face_t reversed;
    cairo_point_t t;

    reversed = *face;

    /* The initial cap needs an outward facing vector. Reverse everything */
    reversed.usr_vector.x = -reversed.usr_vector.x;
    reversed.usr_vector.y = -reversed.usr_vector.y;
    reversed.dev_vector.dx = -reversed.dev_vector.dx;
    reversed.dev_vector.dy = -reversed.dev_vector.dy;
    t = reversed.cw;
    reversed.cw = reversed.ccw;
    reversed.ccw = t;

    return _cairo_stroker_add_cap (stroker, &reversed);
}

static cairo_status_t
_cairo_stroker_add_trailing_cap (cairo_stroker_t     *stroker,
				 const cairo_stroke_face_t *face)
{
    return _cairo_stroker_add_cap (stroker, face);
}

static inline cairo_bool_t
_compute_normalized_device_slope (double *dx, double *dy,
				  const cairo_matrix_t *ctm_inverse,
				  double *mag_out)
{
    double dx0 = *dx, dy0 = *dy;
    double mag;

    cairo_matrix_transform_distance (ctm_inverse, &dx0, &dy0);

    if (dx0 == 0.0 && dy0 == 0.0) {
	if (mag_out)
	    *mag_out = 0.0;
	return FALSE;
    }

    if (dx0 == 0.0) {
	*dx = 0.0;
	if (dy0 > 0.0) {
	    mag = dy0;
	    *dy = 1.0;
	} else {
	    mag = -dy0;
	    *dy = -1.0;
	}
    } else if (dy0 == 0.0) {
	*dy = 0.0;
	if (dx0 > 0.0) {
	    mag = dx0;
	    *dx = 1.0;
	} else {
	    mag = -dx0;
	    *dx = -1.0;
	}
    } else {
	mag = hypot (dx0, dy0);
	*dx = dx0 / mag;
	*dy = dy0 / mag;
    }

    if (mag_out)
	*mag_out = mag;

    return TRUE;
}

static void
_compute_face (const cairo_point_t *point, cairo_slope_t *dev_slope,
	       double slope_dx, double slope_dy,
	       cairo_stroker_t *stroker, cairo_stroke_face_t *face)
{
    double face_dx, face_dy;
    cairo_point_t offset_ccw, offset_cw;

    /*
     * rotate to get a line_width/2 vector along the face, note that
     * the vector must be rotated the right direction in device space,
     * but by 90° in user space. So, the rotation depends on
     * whether the ctm reflects or not, and that can be determined
     * by looking at the determinant of the matrix.
     */
    if (stroker->ctm_det_positive)
    {
	face_dx = - slope_dy * (stroker->style.line_width / 2.0);
	face_dy = slope_dx * (stroker->style.line_width / 2.0);
    }
    else
    {
	face_dx = slope_dy * (stroker->style.line_width / 2.0);
	face_dy = - slope_dx * (stroker->style.line_width / 2.0);
    }

    /* back to device space */
    cairo_matrix_transform_distance (stroker->ctm, &face_dx, &face_dy);

    offset_ccw.x = _cairo_fixed_from_double (face_dx);
    offset_ccw.y = _cairo_fixed_from_double (face_dy);
    offset_cw.x = -offset_ccw.x;
    offset_cw.y = -offset_ccw.y;

    face->ccw = *point;
    _translate_point (&face->ccw, &offset_ccw);

    face->point = *point;

    face->cw = *point;
    _translate_point (&face->cw, &offset_cw);

    face->usr_vector.x = slope_dx;
    face->usr_vector.y = slope_dy;

    face->dev_vector = *dev_slope;
}

static cairo_status_t
_cairo_stroker_add_caps (cairo_stroker_t *stroker)
{
    cairo_status_t status;

    /* check for a degenerative sub_path */
    if (stroker->has_initial_sub_path
	&& ! stroker->has_first_face
	&& ! stroker->has_current_face
	&& stroker->style.line_cap == CAIRO_LINE_JOIN_ROUND)
    {
	/* pick an arbitrary slope to use */
	double dx = 1.0, dy = 0.0;
	cairo_slope_t slope = { CAIRO_FIXED_ONE, 0 };
	cairo_stroke_face_t face;

	_compute_normalized_device_slope (&dx, &dy,
					  stroker->ctm_inverse, NULL);

	/* arbitrarily choose first_point
	 * first_point and current_point should be the same */
	_compute_face (&stroker->first_point, &slope, dx, dy, stroker, &face);

	status = _cairo_stroker_add_leading_cap (stroker, &face);
	if (unlikely (status))
	    return status;

	status = _cairo_stroker_add_trailing_cap (stroker, &face);
	if (unlikely (status))
	    return status;
    }

    if (stroker->has_first_face) {
	status = _cairo_stroker_add_leading_cap (stroker,
						 &stroker->first_face);
	if (unlikely (status))
	    return status;
    }

    if (stroker->has_current_face) {
	status = _cairo_stroker_add_trailing_cap (stroker,
						  &stroker->current_face);
	if (unlikely (status))
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_stroker_add_sub_edge (cairo_stroker_t *stroker,
			     const cairo_point_t *p1,
			     const cairo_point_t *p2,
			     cairo_slope_t *dev_slope,
			     double slope_dx, double slope_dy,
			     cairo_stroke_face_t *start,
			     cairo_stroke_face_t *end)
{
    _compute_face (p1, dev_slope, slope_dx, slope_dy, stroker, start);
    *end = *start;

    if (p1->x == p2->x && p1->y == p2->y)
	return CAIRO_STATUS_SUCCESS;

    end->point = *p2;
    end->ccw.x += p2->x - p1->x;
    end->ccw.y += p2->y - p1->y;
    end->cw.x += p2->x - p1->x;
    end->cw.y += p2->y - p1->y;

    if (stroker->add_external_edge != NULL) {
	cairo_status_t status;

	status = stroker->add_external_edge (stroker->closure,
					     &end->cw, &start->cw);
	if (unlikely (status))
	    return status;

	status = stroker->add_external_edge (stroker->closure,
					     &start->ccw, &end->ccw);
	if (unlikely (status))
	    return status;

	return CAIRO_STATUS_SUCCESS;
    } else {
	cairo_point_t quad[4];

	quad[0] = start->cw;
	quad[1] = end->cw;
	quad[2] = end->ccw;
	quad[3] = start->ccw;

	return stroker->add_convex_quad (stroker->closure, quad);
    }
}

static cairo_status_t
_cairo_stroker_move_to (void *closure,
			const cairo_point_t *point)
{
    cairo_stroker_t *stroker = closure;
    cairo_status_t status;

    /* reset the dash pattern for new sub paths */
    _cairo_stroker_dash_start (&stroker->dash);

    /* Cap the start and end of the previous sub path as needed */
    status = _cairo_stroker_add_caps (stroker);
    if (unlikely (status))
	return status;

    stroker->first_point = *point;
    stroker->current_point = *point;

    stroker->has_first_face = FALSE;
    stroker->has_current_face = FALSE;
    stroker->has_initial_sub_path = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_stroker_line_to (void *closure,
			const cairo_point_t *point)
{
    cairo_stroker_t *stroker = closure;
    cairo_stroke_face_t start, end;
    cairo_point_t *p1 = &stroker->current_point;
    cairo_slope_t dev_slope;
    double slope_dx, slope_dy;
    cairo_status_t status;

    stroker->has_initial_sub_path = TRUE;

    if (p1->x == point->x && p1->y == point->y)
	return CAIRO_STATUS_SUCCESS;

    _cairo_slope_init (&dev_slope, p1, point);
    slope_dx = _cairo_fixed_to_double (point->x - p1->x);
    slope_dy = _cairo_fixed_to_double (point->y - p1->y);
    _compute_normalized_device_slope (&slope_dx, &slope_dy,
				      stroker->ctm_inverse, NULL);

    status = _cairo_stroker_add_sub_edge (stroker,
					  p1, point,
					  &dev_slope,
					  slope_dx, slope_dy,
					  &start, &end);
    if (unlikely (status))
	return status;

    if (stroker->has_current_face) {
	/* Join with final face from previous segment */
	status = _cairo_stroker_join (stroker,
				      &stroker->current_face,
				      &start);
	if (unlikely (status))
	    return status;
    } else if (! stroker->has_first_face) {
	/* Save sub path's first face in case needed for closing join */
	stroker->first_face = start;
	stroker->has_first_face = TRUE;
    }
    stroker->current_face = end;
    stroker->has_current_face = TRUE;

    stroker->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

/*
 * Dashed lines.  Cap each dash end, join around turns when on
 */
static cairo_status_t
_cairo_stroker_line_to_dashed (void *closure,
			       const cairo_point_t *p2)
{
    cairo_stroker_t *stroker = closure;
    double mag, remain, step_length = 0;
    double slope_dx, slope_dy;
    double dx2, dy2;
    cairo_stroke_face_t sub_start, sub_end;
    cairo_point_t *p1 = &stroker->current_point;
    cairo_slope_t dev_slope;
    cairo_line_t segment;
    cairo_bool_t fully_in_bounds;
    cairo_status_t status;

    stroker->has_initial_sub_path = stroker->dash.dash_starts_on;

    if (p1->x == p2->x && p1->y == p2->y)
	return CAIRO_STATUS_SUCCESS;

    fully_in_bounds = TRUE;
    if (stroker->has_bounds &&
	(! _cairo_box_contains_point (&stroker->bounds, p1) ||
	 ! _cairo_box_contains_point (&stroker->bounds, p2)))
    {
	fully_in_bounds = FALSE;
    }

    _cairo_slope_init (&dev_slope, p1, p2);

    slope_dx = _cairo_fixed_to_double (p2->x - p1->x);
    slope_dy = _cairo_fixed_to_double (p2->y - p1->y);

    if (! _compute_normalized_device_slope (&slope_dx, &slope_dy,
					    stroker->ctm_inverse, &mag))
    {
	return CAIRO_STATUS_SUCCESS;
    }

    remain = mag;
    segment.p1 = *p1;
    while (remain) {
	step_length = MIN (stroker->dash.dash_remain, remain);
	remain -= step_length;
	dx2 = slope_dx * (mag - remain);
	dy2 = slope_dy * (mag - remain);
	cairo_matrix_transform_distance (stroker->ctm, &dx2, &dy2);
	segment.p2.x = _cairo_fixed_from_double (dx2) + p1->x;
	segment.p2.y = _cairo_fixed_from_double (dy2) + p1->y;

	if (stroker->dash.dash_on &&
	    (fully_in_bounds ||
	     (! stroker->has_first_face && stroker->dash.dash_starts_on) ||
	     _cairo_box_intersects_line_segment (&stroker->bounds, &segment)))
	{
	    status = _cairo_stroker_add_sub_edge (stroker,
						  &segment.p1, &segment.p2,
						  &dev_slope,
						  slope_dx, slope_dy,
						  &sub_start, &sub_end);
	    if (unlikely (status))
		return status;

	    if (stroker->has_current_face)
	    {
		/* Join with final face from previous segment */
		status = _cairo_stroker_join (stroker,
					      &stroker->current_face,
					      &sub_start);
		if (unlikely (status))
		    return status;

		stroker->has_current_face = FALSE;
	    }
	    else if (! stroker->has_first_face &&
		       stroker->dash.dash_starts_on)
	    {
		/* Save sub path's first face in case needed for closing join */
		stroker->first_face = sub_start;
		stroker->has_first_face = TRUE;
	    }
	    else
	    {
		/* Cap dash start if not connecting to a previous segment */
		status = _cairo_stroker_add_leading_cap (stroker, &sub_start);
		if (unlikely (status))
		    return status;
	    }

	    if (remain) {
		/* Cap dash end if not at end of segment */
		status = _cairo_stroker_add_trailing_cap (stroker, &sub_end);
		if (unlikely (status))
		    return status;
	    } else {
		stroker->current_face = sub_end;
		stroker->has_current_face = TRUE;
	    }
	} else {
	    if (stroker->has_current_face) {
		/* Cap final face from previous segment */
		status = _cairo_stroker_add_trailing_cap (stroker,
							  &stroker->current_face);
		if (unlikely (status))
		    return status;

		stroker->has_current_face = FALSE;
	    }
	}

	_cairo_stroker_dash_step (&stroker->dash, step_length);
	segment.p1 = segment.p2;
    }

    if (stroker->dash.dash_on && ! stroker->has_current_face) {
	/* This segment ends on a transition to dash_on, compute a new face
	 * and add cap for the beginning of the next dash_on step.
	 *
	 * Note: this will create a degenerate cap if this is not the last line
	 * in the path. Whether this behaviour is desirable or not is debatable.
	 * On one side these degenerate caps can not be reproduced with regular
	 * path stroking.
	 * On the other hand, Acroread 7 also produces the degenerate caps.
	 */
	_compute_face (p2, &dev_slope,
		       slope_dx, slope_dy,
		       stroker,
		       &stroker->current_face);

	status = _cairo_stroker_add_leading_cap (stroker,
						 &stroker->current_face);
	if (unlikely (status))
	    return status;

	stroker->has_current_face = TRUE;
    }

    stroker->current_point = *p2;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_stroker_curve_to (void *closure,
			 const cairo_point_t *b,
			 const cairo_point_t *c,
			 const cairo_point_t *d)
{
    cairo_stroker_t *stroker = closure;
    cairo_spline_t spline;
    cairo_line_join_t line_join_save;
    cairo_stroke_face_t face;
    double slope_dx, slope_dy;
    cairo_path_fixed_line_to_func_t *line_to;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    line_to = stroker->dash.dashed ?
	_cairo_stroker_line_to_dashed :
	_cairo_stroker_line_to;

    if (! _cairo_spline_init (&spline,
			      line_to, stroker,
			      &stroker->current_point, b, c, d))
    {
	return line_to (closure, d);
    }

    /* If the line width is so small that the pen is reduced to a
       single point, then we have nothing to do. */
    if (stroker->pen.num_vertices <= 1)
	return CAIRO_STATUS_SUCCESS;

    /* Compute the initial face */
    if (! stroker->dash.dashed || stroker->dash.dash_on) {
	slope_dx = _cairo_fixed_to_double (spline.initial_slope.dx);
	slope_dy = _cairo_fixed_to_double (spline.initial_slope.dy);
	if (_compute_normalized_device_slope (&slope_dx, &slope_dy,
					      stroker->ctm_inverse, NULL))
	{
	    _compute_face (&stroker->current_point,
			   &spline.initial_slope,
			   slope_dx, slope_dy,
			   stroker, &face);
	}
	if (stroker->has_current_face) {
	    status = _cairo_stroker_join (stroker,
					  &stroker->current_face, &face);
	    if (unlikely (status))
		return status;
	} else if (! stroker->has_first_face) {
	    stroker->first_face = face;
	    stroker->has_first_face = TRUE;
	}

	stroker->current_face = face;
	stroker->has_current_face = TRUE;
    }

    /* Temporarily modify the stroker to use round joins to guarantee
     * smooth stroked curves. */
    line_join_save = stroker->style.line_join;
    stroker->style.line_join = CAIRO_LINE_JOIN_ROUND;

    status = _cairo_spline_decompose (&spline, stroker->tolerance);
    if (unlikely (status))
	return status;

    /* And join the final face */
    if (! stroker->dash.dashed || stroker->dash.dash_on) {
	slope_dx = _cairo_fixed_to_double (spline.final_slope.dx);
	slope_dy = _cairo_fixed_to_double (spline.final_slope.dy);
	if (_compute_normalized_device_slope (&slope_dx, &slope_dy,
					      stroker->ctm_inverse, NULL))
	{
	    _compute_face (&stroker->current_point,
			   &spline.final_slope,
			   slope_dx, slope_dy,
			   stroker, &face);
	}

	status = _cairo_stroker_join (stroker, &stroker->current_face, &face);
	if (unlikely (status))
	    return status;

	stroker->current_face = face;
    }

    stroker->style.line_join = line_join_save;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_stroker_close_path (void *closure)
{
    cairo_stroker_t *stroker = closure;
    cairo_status_t status;

    if (stroker->dash.dashed)
	status = _cairo_stroker_line_to_dashed (stroker, &stroker->first_point);
    else
	status = _cairo_stroker_line_to (stroker, &stroker->first_point);
    if (unlikely (status))
	return status;

    if (stroker->has_first_face && stroker->has_current_face) {
	/* Join first and final faces of sub path */
	status = _cairo_stroker_join (stroker,
				      &stroker->current_face,
				      &stroker->first_face);
	if (unlikely (status))
	    return status;
    } else {
	/* Cap the start and end of the sub path as needed */
	status = _cairo_stroker_add_caps (stroker);
	if (unlikely (status))
	    return status;
    }

    stroker->has_initial_sub_path = FALSE;
    stroker->has_first_face = FALSE;
    stroker->has_current_face = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_stroke_to_shaper (cairo_path_fixed_t	*path,
				    const cairo_stroke_style_t	*stroke_style,
				    const cairo_matrix_t	*ctm,
				    const cairo_matrix_t	*ctm_inverse,
				    double		 tolerance,
				    cairo_status_t (*add_triangle) (void *closure,
								    const cairo_point_t triangle[3]),
				    cairo_status_t (*add_triangle_fan) (void *closure,
									const cairo_point_t *midpt,
									const cairo_point_t *points,
									int npoints),
				    cairo_status_t (*add_convex_quad) (void *closure,
								       const cairo_point_t quad[4]),
				    void *closure)
{
    cairo_stroker_t stroker;
    cairo_status_t status;

    status = _cairo_stroker_init (&stroker, stroke_style,
			          ctm, ctm_inverse, tolerance);
    if (unlikely (status))
	return status;

    stroker.add_triangle = add_triangle;
    stroker.add_triangle_fan = add_triangle_fan;
    stroker.add_convex_quad = add_convex_quad;
    stroker.closure = closure;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_stroker_move_to,
					  stroker.dash.dashed ?
					  _cairo_stroker_line_to_dashed :
					  _cairo_stroker_line_to,
					  _cairo_stroker_curve_to,
					  _cairo_stroker_close_path,
					  &stroker);

    if (unlikely (status))
	goto BAIL;

    /* Cap the start and end of the final sub path as needed */
    status = _cairo_stroker_add_caps (&stroker);

BAIL:
    _cairo_stroker_fini (&stroker);

    return status;
}

cairo_status_t
_cairo_path_fixed_stroke_to_polygon (const cairo_path_fixed_t	*path,
				     const cairo_stroke_style_t	*stroke_style,
				     const cairo_matrix_t	*ctm,
				     const cairo_matrix_t	*ctm_inverse,
				     double		 tolerance,
				     cairo_polygon_t *polygon)
{
    cairo_stroker_t stroker;
    cairo_status_t status;

    status = _cairo_stroker_init (&stroker, stroke_style,
			          ctm, ctm_inverse, tolerance);
    if (unlikely (status))
	return status;

    stroker.add_external_edge = _cairo_polygon_add_external_edge,
    stroker.closure = polygon;

    if (polygon->num_limits)
	_cairo_stroker_limit (&stroker, polygon->limits, polygon->num_limits);

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_stroker_move_to,
					  stroker.dash.dashed ?
					  _cairo_stroker_line_to_dashed :
					  _cairo_stroker_line_to,
					  _cairo_stroker_curve_to,
					  _cairo_stroker_close_path,
					  &stroker);

    if (unlikely (status))
	goto BAIL;

    /* Cap the start and end of the final sub path as needed */
    status = _cairo_stroker_add_caps (&stroker);

BAIL:
    _cairo_stroker_fini (&stroker);

    return status;
}

cairo_status_t
_cairo_path_fixed_stroke_to_traps (const cairo_path_fixed_t	*path,
				   const cairo_stroke_style_t	*stroke_style,
				   const cairo_matrix_t	*ctm,
				   const cairo_matrix_t	*ctm_inverse,
				   double		 tolerance,
				   cairo_traps_t	*traps)
{
    cairo_status_t status;
    cairo_polygon_t polygon;

    /* Before we do anything else, we attempt the rectilinear
     * stroker. It's careful to generate trapezoids that align to
     * device-pixel boundaries when possible. Many backends can render
     * those much faster than non-aligned trapezoids, (by using clip
     * regions, etc.) */
    if (path->is_rectilinear) {
	status = _cairo_path_fixed_stroke_rectilinear_to_traps (path,
								stroke_style,
								ctm,
								traps);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    _cairo_polygon_init (&polygon);
    if (traps->num_limits)
	_cairo_polygon_limit (&polygon, traps->limits, traps->num_limits);

    status = _cairo_path_fixed_stroke_to_polygon (path,
						  stroke_style,
						  ctm,
						  ctm_inverse,
						  tolerance,
						  &polygon);
    if (unlikely (status))
	goto BAIL;

    status = _cairo_polygon_status (&polygon);
    if (unlikely (status))
	goto BAIL;

    status = _cairo_bentley_ottmann_tessellate_polygon (traps, &polygon,
							CAIRO_FILL_RULE_WINDING);

BAIL:
    _cairo_polygon_fini (&polygon);

    return status;
}

typedef struct _segment_t {
    cairo_point_t p1, p2;
    cairo_bool_t is_horizontal;
    cairo_bool_t has_join;
} segment_t;

typedef struct _cairo_rectilinear_stroker {
    const cairo_stroke_style_t *stroke_style;
    const cairo_matrix_t *ctm;

    cairo_fixed_t half_line_width;
    cairo_bool_t do_traps;
    void *container;
    cairo_point_t current_point;
    cairo_point_t first_point;
    cairo_bool_t open_sub_path;

    cairo_stroker_dash_t dash;

    cairo_bool_t has_bounds;
    cairo_box_t bounds;

    int num_segments;
    int segments_size;
    segment_t *segments;
    segment_t segments_embedded[8]; /* common case is a single rectangle */
} cairo_rectilinear_stroker_t;

static void
_cairo_rectilinear_stroker_limit (cairo_rectilinear_stroker_t *stroker,
				  const cairo_box_t *boxes,
				  int num_boxes)
{
    stroker->has_bounds = TRUE;
    _cairo_boxes_get_extents (boxes, num_boxes, &stroker->bounds);

    stroker->bounds.p1.x -= stroker->half_line_width;
    stroker->bounds.p2.x += stroker->half_line_width;

    stroker->bounds.p1.y -= stroker->half_line_width;
    stroker->bounds.p2.y += stroker->half_line_width;
}

static cairo_bool_t
_cairo_rectilinear_stroker_init (cairo_rectilinear_stroker_t	*stroker,
				 const cairo_stroke_style_t	*stroke_style,
				 const cairo_matrix_t		*ctm,
				 cairo_bool_t			 do_traps,
				 void				*container)
{
    /* This special-case rectilinear stroker only supports
     * miter-joined lines (not curves) and a translation-only matrix
     * (though it could probably be extended to support a matrix with
     * uniform, integer scaling).
     *
     * It also only supports horizontal and vertical line_to
     * elements. But we don't catch that here, but instead return
     * UNSUPPORTED from _cairo_rectilinear_stroker_line_to if any
     * non-rectilinear line_to is encountered.
     */
    if (stroke_style->line_join	!= CAIRO_LINE_JOIN_MITER)
	return FALSE;

    /* If the miter limit turns right angles into bevels, then we
     * can't use this optimization. Remember, the ratio is
     * 1/sin(ɸ/2). So the cutoff is 1/sin(π/4.0) or ⎷2,
     * which we round for safety. */
    if (stroke_style->miter_limit < M_SQRT2)
	return FALSE;

    if (! (stroke_style->line_cap == CAIRO_LINE_CAP_BUTT ||
	   stroke_style->line_cap == CAIRO_LINE_CAP_SQUARE))
    {
	return FALSE;
    }

    if (! _cairo_matrix_has_unity_scale (ctm))
	return FALSE;

    stroker->stroke_style = stroke_style;
    stroker->ctm = ctm;

    stroker->half_line_width =
	_cairo_fixed_from_double (stroke_style->line_width / 2.0);
    stroker->open_sub_path = FALSE;
    stroker->segments = stroker->segments_embedded;
    stroker->segments_size = ARRAY_LENGTH (stroker->segments_embedded);
    stroker->num_segments = 0;

    _cairo_stroker_dash_init (&stroker->dash, stroke_style);

    stroker->has_bounds = FALSE;

    stroker->do_traps = do_traps;
    stroker->container = container;

    return TRUE;
}

static void
_cairo_rectilinear_stroker_fini (cairo_rectilinear_stroker_t	*stroker)
{
    if (stroker->segments != stroker->segments_embedded)
	free (stroker->segments);
}

static cairo_status_t
_cairo_rectilinear_stroker_add_segment (cairo_rectilinear_stroker_t *stroker,
					const cairo_point_t	*p1,
					const cairo_point_t	*p2,
					cairo_bool_t		 is_horizontal,
					cairo_bool_t		 has_join)
{
    if (CAIRO_INJECT_FAULT ())
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (stroker->num_segments == stroker->segments_size) {
	int new_size = stroker->segments_size * 2;
	segment_t *new_segments;

	if (stroker->segments == stroker->segments_embedded) {
	    new_segments = _cairo_malloc_ab (new_size, sizeof (segment_t));
	    if (unlikely (new_segments == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	    memcpy (new_segments, stroker->segments,
		    stroker->num_segments * sizeof (segment_t));
	} else {
	    new_segments = _cairo_realloc_ab (stroker->segments,
					      new_size, sizeof (segment_t));
	    if (unlikely (new_segments == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	stroker->segments_size = new_size;
	stroker->segments = new_segments;
    }

    stroker->segments[stroker->num_segments].p1 = *p1;
    stroker->segments[stroker->num_segments].p2 = *p2;
    stroker->segments[stroker->num_segments].has_join = has_join;
    stroker->segments[stroker->num_segments].is_horizontal = is_horizontal;
    stroker->num_segments++;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_rectilinear_stroker_emit_segments (cairo_rectilinear_stroker_t *stroker)
{
    cairo_status_t status;
    cairo_line_cap_t line_cap = stroker->stroke_style->line_cap;
    cairo_fixed_t half_line_width = stroker->half_line_width;
    int i;

    for (i = 0; i < stroker->num_segments; i++) {
	cairo_point_t *a, *b;
	cairo_bool_t lengthen_initial, shorten_final, lengthen_final;

	a = &stroker->segments[i].p1;
	b = &stroker->segments[i].p2;

	/* For each segment we generate a single rectangular
	 * trapezoid. This rectangle is based on a perpendicular
	 * extension (by half the line width) of the segment endpoints
	 * after some adjustments of the endpoints to account for caps
	 * and joins.
	 */

	/* We adjust the initial point of the segment to extend the
	 * rectangle to include the previous cap or join, (this
	 * adjustment applies to all segments except for the first
	 * segment of open, butt-capped paths).
	 */
	lengthen_initial = TRUE;
	if (i == 0 && stroker->open_sub_path && line_cap == CAIRO_LINE_CAP_BUTT)
	    lengthen_initial = FALSE;

	/* The adjustment of the final point is trickier. For all but
	 * the last segment we shorten the segment at the final
	 * endpoint to not overlap with the subsequent join. For the
	 * last segment we do the same shortening if the path is
	 * closed. If the path is open and butt-capped we do no
	 * adjustment, while if it's open and square-capped we do a
	 * lengthening adjustment instead to include the cap.
	 */
	shorten_final = TRUE;
	lengthen_final = FALSE;
	if (i == stroker->num_segments - 1 && stroker->open_sub_path) {
	    shorten_final = FALSE;
	    if (line_cap == CAIRO_LINE_CAP_SQUARE)
		lengthen_final = TRUE;
	}

	/* Perform the adjustments of the endpoints. */
	if (a->y == b->y) {
	    if (a->x < b->x) {
		if (lengthen_initial)
		    a->x -= half_line_width;
		if (shorten_final)
		    b->x -= half_line_width;
		else if (lengthen_final)
		    b->x += half_line_width;
	    } else {
		if (lengthen_initial)
		    a->x += half_line_width;
		if (shorten_final)
		    b->x += half_line_width;
		else if (lengthen_final)
		    b->x -= half_line_width;
	    }

	    if (a->x > b->x) {
		cairo_point_t *t;

		t = a;
		a = b;
		b = t;
	    }
	} else {
	    if (a->y < b->y) {
		if (lengthen_initial)
		    a->y -= half_line_width;
		if (shorten_final)
		    b->y -= half_line_width;
		else if (lengthen_final)
		    b->y += half_line_width;
	    } else {
		if (lengthen_initial)
		    a->y += half_line_width;
		if (shorten_final)
		    b->y += half_line_width;
		else if (lengthen_final)
		    b->y -= half_line_width;
	    }

	    if (a->y > b->y) {
		cairo_point_t *t;

		t = a;
		a = b;
		b = t;
	    }
	}

	/* Form the rectangle by expanding by half the line width in
	 * either perpendicular direction. */
	if (a->y == b->y) {
	    a->y -= half_line_width;
	    b->y += half_line_width;
	} else {
	    a->x -= half_line_width;
	    b->x += half_line_width;
	}

	if (stroker->do_traps) {
	    status = _cairo_traps_tessellate_rectangle (stroker->container, a, b);
	} else {
	    cairo_box_t box;

	    box.p1 = *a;
	    box.p2 = *b;

	    status = _cairo_boxes_add (stroker->container, &box);
	}
	if (unlikely (status))
	    return status;
    }

    stroker->num_segments = 0;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_rectilinear_stroker_emit_segments_dashed (cairo_rectilinear_stroker_t *stroker)
{
    cairo_status_t status;
    cairo_line_cap_t line_cap = stroker->stroke_style->line_cap;
    cairo_fixed_t half_line_width = stroker->half_line_width;
    int i;

    for (i = 0; i < stroker->num_segments; i++) {
	cairo_point_t *a, *b;
	cairo_bool_t is_horizontal;

	a = &stroker->segments[i].p1;
	b = &stroker->segments[i].p2;

	is_horizontal = stroker->segments[i].is_horizontal;

	/* Handle the joins for a potentially degenerate segment. */
	if (line_cap == CAIRO_LINE_CAP_BUTT &&
	    stroker->segments[i].has_join &&
	    (i != stroker->num_segments - 1 ||
	     (! stroker->open_sub_path && stroker->dash.dash_starts_on)))
	{
	    cairo_point_t p1 = stroker->segments[i].p1;
	    cairo_point_t p2 = stroker->segments[i].p2;
	    cairo_slope_t out_slope;
	    int j = (i + 1) % stroker->num_segments;

	    _cairo_slope_init (&out_slope,
			       &stroker->segments[j].p1,
			       &stroker->segments[j].p2);

	    if (is_horizontal) {
		if (p1.x <= p2.x) {
		    p1.x = p2.x;
		    p2.x += half_line_width;
		} else {
		    p1.x = p2.x - half_line_width;
		}
		if (out_slope.dy >= 0)
		    p1.y -= half_line_width;
		if (out_slope.dy <= 0)
		    p2.y += half_line_width;
	    } else {
		if (p1.y <= p2.y) {
		    p1.y = p2.y;
		    p2.y += half_line_width;
		} else {
		    p1.y = p2.y - half_line_width;
		}
		if (out_slope.dx >= 0)
		    p1.x -= half_line_width;
		if (out_slope.dx <= 0)
		    p2.x += half_line_width;
	    }

	    if (stroker->do_traps) {
		status = _cairo_traps_tessellate_rectangle (stroker->container, &p1, &p2);
	    } else {
		cairo_box_t box;

		box.p1 = p1;
		box.p2 = p2;

		status = _cairo_boxes_add (stroker->container, &box);
	    }
	    if (unlikely (status))
		return status;
	}

	/* Perform the adjustments of the endpoints. */
	if (is_horizontal) {
	    if (line_cap == CAIRO_LINE_CAP_SQUARE) {
		if (a->x <= b->x) {
		    a->x -= half_line_width;
		    b->x += half_line_width;
		} else {
		    a->x += half_line_width;
		    b->x -= half_line_width;
		}
	    }

	    if (a->x > b->x) {
		cairo_point_t *t;

		t = a;
		a = b;
		b = t;
	    }

	    a->y -= half_line_width;
	    b->y += half_line_width;
	} else {
	    if (line_cap == CAIRO_LINE_CAP_SQUARE) {
		if (a->y <= b->y) {
		    a->y -= half_line_width;
		    b->y += half_line_width;
		} else {
		    a->y += half_line_width;
		    b->y -= half_line_width;
		}
	    }

	    if (a->y > b->y) {
		cairo_point_t *t;

		t = a;
		a = b;
		b = t;
	    }

	    a->x -= half_line_width;
	    b->x += half_line_width;
	}

	if (a->x == b->x && a->y == b->y)
	    continue;

	if (stroker->do_traps) {
	    status = _cairo_traps_tessellate_rectangle (stroker->container, a, b);
	} else {
	    cairo_box_t box;

	    box.p1 = *a;
	    box.p2 = *b;

	    status = _cairo_boxes_add (stroker->container, &box);
	}
	if (unlikely (status))
	    return status;
    }

    stroker->num_segments = 0;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_rectilinear_stroker_move_to (void		*closure,
				    const cairo_point_t	*point)
{
    cairo_rectilinear_stroker_t *stroker = closure;
    cairo_status_t status;

    if (stroker->dash.dashed)
	status = _cairo_rectilinear_stroker_emit_segments_dashed (stroker);
    else
	status = _cairo_rectilinear_stroker_emit_segments (stroker);
    if (unlikely (status))
	return status;

    /* reset the dash pattern for new sub paths */
    _cairo_stroker_dash_start (&stroker->dash);

    stroker->current_point = *point;
    stroker->first_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_rectilinear_stroker_line_to (void		*closure,
				    const cairo_point_t	*b)
{
    cairo_rectilinear_stroker_t *stroker = closure;
    cairo_point_t *a = &stroker->current_point;
    cairo_status_t status;

    /* We only support horizontal or vertical elements. */
    assert (a->x == b->x || a->y == b->y);

    /* We don't draw anything for degenerate paths. */
    if (a->x == b->x && a->y == b->y)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_rectilinear_stroker_add_segment (stroker, a, b,
						     a->y == b->y,
						     TRUE);

    stroker->current_point = *b;
    stroker->open_sub_path = TRUE;

    return status;
}

static cairo_status_t
_cairo_rectilinear_stroker_line_to_dashed (void		*closure,
					   const cairo_point_t	*point)
{
    cairo_rectilinear_stroker_t *stroker = closure;
    const cairo_point_t *a = &stroker->current_point;
    const cairo_point_t *b = point;
    cairo_bool_t fully_in_bounds;
    double sign, remain;
    cairo_fixed_t mag;
    cairo_status_t status;
    cairo_line_t segment;
    cairo_bool_t dash_on = FALSE;
    cairo_bool_t is_horizontal;

    /* We don't draw anything for degenerate paths. */
    if (a->x == b->x && a->y == b->y)
	return CAIRO_STATUS_SUCCESS;

    /* We only support horizontal or vertical elements. */
    assert (a->x == b->x || a->y == b->y);

    fully_in_bounds = TRUE;
    if (stroker->has_bounds &&
	(! _cairo_box_contains_point (&stroker->bounds, a) ||
	 ! _cairo_box_contains_point (&stroker->bounds, b)))
    {
	fully_in_bounds = FALSE;
    }

    is_horizontal = a->y == b->y;
    if (is_horizontal)
	mag = b->x - a->x;
    else
	mag = b->y - a->y;
    if (mag < 0) {
	remain = _cairo_fixed_to_double (-mag);
	sign = 1.;
    } else {
	remain = _cairo_fixed_to_double (mag);
	sign = -1.;
    }

    segment.p2 = segment.p1 = *a;
    while (remain > 0.) {
	double step_length;

	step_length = MIN (stroker->dash.dash_remain, remain);
	remain -= step_length;

	mag = _cairo_fixed_from_double (sign*remain);
	if (is_horizontal)
	    segment.p2.x = b->x + mag;
	else
	    segment.p2.y = b->y + mag;

	if (stroker->dash.dash_on &&
	    (fully_in_bounds ||
	     _cairo_box_intersects_line_segment (&stroker->bounds, &segment)))
	{
	    status = _cairo_rectilinear_stroker_add_segment (stroker,
							     &segment.p1,
							     &segment.p2,
							     is_horizontal,
							     remain <= 0.);
	    if (unlikely (status))
		return status;

	    dash_on = TRUE;
	}
	else
	{
	    dash_on = FALSE;
	}

	_cairo_stroker_dash_step (&stroker->dash, step_length);
	segment.p1 = segment.p2;
    }

    if (stroker->dash.dash_on && ! dash_on &&
	(fully_in_bounds ||
	 _cairo_box_intersects_line_segment (&stroker->bounds, &segment)))
    {

	/* This segment ends on a transition to dash_on, compute a new face
	 * and add cap for the beginning of the next dash_on step.
	 */

	status = _cairo_rectilinear_stroker_add_segment (stroker,
							 &segment.p1,
							 &segment.p1,
							 is_horizontal,
							 TRUE);
	if (unlikely (status))
	    return status;
    }

    stroker->current_point = *point;
    stroker->open_sub_path = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_rectilinear_stroker_close_path (void *closure)
{
    cairo_rectilinear_stroker_t *stroker = closure;
    cairo_status_t status;

    /* We don't draw anything for degenerate paths. */
    if (! stroker->open_sub_path)
	return CAIRO_STATUS_SUCCESS;

    if (stroker->dash.dashed) {
	status = _cairo_rectilinear_stroker_line_to_dashed (stroker,
							    &stroker->first_point);
    } else {
	status = _cairo_rectilinear_stroker_line_to (stroker,
						     &stroker->first_point);
    }
    if (unlikely (status))
	return status;

    stroker->open_sub_path = FALSE;

    if (stroker->dash.dashed)
	status = _cairo_rectilinear_stroker_emit_segments_dashed (stroker);
    else
	status = _cairo_rectilinear_stroker_emit_segments (stroker);
    if (unlikely (status))
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_path_fixed_stroke_rectilinear_to_traps (const cairo_path_fixed_t	*path,
					       const cairo_stroke_style_t	*stroke_style,
					       const cairo_matrix_t	*ctm,
					       cairo_traps_t		*traps)
{
    cairo_rectilinear_stroker_t rectilinear_stroker;
    cairo_int_status_t status;

    assert (path->is_rectilinear);

    if (! _cairo_rectilinear_stroker_init (&rectilinear_stroker,
					   stroke_style, ctm,
					   TRUE, traps))
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (traps->num_limits) {
	_cairo_rectilinear_stroker_limit (&rectilinear_stroker,
					  traps->limits,
					  traps->num_limits);
    }

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_rectilinear_stroker_move_to,
					  rectilinear_stroker.dash.dashed ?
					  _cairo_rectilinear_stroker_line_to_dashed :
					  _cairo_rectilinear_stroker_line_to,
					  NULL,
					  _cairo_rectilinear_stroker_close_path,
					  &rectilinear_stroker);
    if (unlikely (status))
	goto BAIL;

    if (rectilinear_stroker.dash.dashed)
	status = _cairo_rectilinear_stroker_emit_segments_dashed (&rectilinear_stroker);
    else
	status = _cairo_rectilinear_stroker_emit_segments (&rectilinear_stroker);

    traps->is_rectilinear = 1;
    traps->is_rectangular = 1;
    /* As we incrementally tessellate, we do not eliminate self-intersections */
    traps->has_intersections = traps->num_traps > 1;
BAIL:
    _cairo_rectilinear_stroker_fini (&rectilinear_stroker);

    if (unlikely (status))
	_cairo_traps_clear (traps);

    return status;
}

cairo_int_status_t
_cairo_path_fixed_stroke_rectilinear_to_boxes (const cairo_path_fixed_t	*path,
					       const cairo_stroke_style_t	*stroke_style,
					       const cairo_matrix_t	*ctm,
					       cairo_boxes_t		*boxes)
{
    cairo_rectilinear_stroker_t rectilinear_stroker;
    cairo_int_status_t status;

    assert (path->is_rectilinear);

    if (! _cairo_rectilinear_stroker_init (&rectilinear_stroker,
					   stroke_style, ctm,
					   FALSE, boxes))
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (boxes->num_limits) {
	_cairo_rectilinear_stroker_limit (&rectilinear_stroker,
					  boxes->limits,
					  boxes->num_limits);
    }

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_rectilinear_stroker_move_to,
					  rectilinear_stroker.dash.dashed ?
					  _cairo_rectilinear_stroker_line_to_dashed :
					  _cairo_rectilinear_stroker_line_to,
					  NULL,
					  _cairo_rectilinear_stroker_close_path,
					  &rectilinear_stroker);
    if (unlikely (status))
	goto BAIL;

    if (rectilinear_stroker.dash.dashed)
	status = _cairo_rectilinear_stroker_emit_segments_dashed (&rectilinear_stroker);
    else
	status = _cairo_rectilinear_stroker_emit_segments (&rectilinear_stroker);
    if (unlikely (status))
	goto BAIL;

    /* As we incrementally tessellate, we do not eliminate self-intersections */
    status = _cairo_bentley_ottmann_tessellate_boxes (boxes,
						      CAIRO_FILL_RULE_WINDING,
						      boxes);
    if (unlikely (status))
	goto BAIL;

    _cairo_rectilinear_stroker_fini (&rectilinear_stroker);

    return CAIRO_STATUS_SUCCESS;

BAIL:
    _cairo_rectilinear_stroker_fini (&rectilinear_stroker);
    _cairo_boxes_clear (boxes);
    return status;
}
