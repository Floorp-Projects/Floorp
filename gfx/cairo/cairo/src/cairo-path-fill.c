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
 */

#include "cairoint.h"
#include "cairo-boxes-private.h"
#include "cairo-error-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-region-private.h"

typedef struct cairo_filler {
    double tolerance;
    cairo_polygon_t *polygon;
} cairo_filler_t;

static void
_cairo_filler_init (cairo_filler_t *filler,
		    double tolerance,
		    cairo_polygon_t *polygon)
{
    filler->tolerance = tolerance;
    filler->polygon = polygon;
}

static void
_cairo_filler_fini (cairo_filler_t *filler)
{
}

static cairo_status_t
_cairo_filler_move_to (void *closure,
		       const cairo_point_t *point)
{
    cairo_filler_t *filler = closure;
    cairo_polygon_t *polygon = filler->polygon;

    return _cairo_polygon_close (polygon) ||
           _cairo_polygon_move_to (polygon, point);
}

static cairo_status_t
_cairo_filler_line_to (void *closure,
		       const cairo_point_t *point)
{
    cairo_filler_t *filler = closure;
    return _cairo_polygon_line_to (filler->polygon, point);
}

static cairo_status_t
_cairo_filler_curve_to (void *closure,
			const cairo_point_t *b,
			const cairo_point_t *c,
			const cairo_point_t *d)
{
    cairo_filler_t *filler = closure;
    cairo_spline_t spline;

    if (! _cairo_spline_init (&spline,
			      _cairo_filler_line_to, filler,
			      &filler->polygon->current_point, b, c, d))
    {
	return _cairo_filler_line_to (closure, d);
    }

    return _cairo_spline_decompose (&spline, filler->tolerance);
}

static cairo_status_t
_cairo_filler_close_path (void *closure)
{
    cairo_filler_t *filler = closure;
    return _cairo_polygon_close (filler->polygon);
}

cairo_status_t
_cairo_path_fixed_fill_to_polygon (const cairo_path_fixed_t *path,
				   double tolerance,
				   cairo_polygon_t *polygon)
{
    cairo_filler_t filler;
    cairo_status_t status;

    _cairo_filler_init (&filler, tolerance, polygon);

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_filler_move_to,
					  _cairo_filler_line_to,
					  _cairo_filler_curve_to,
					  _cairo_filler_close_path,
					  &filler);
    if (unlikely (status))
	return status;

    status = _cairo_polygon_close (polygon);
    _cairo_filler_fini (&filler);

    return status;
}

cairo_status_t
_cairo_path_fixed_fill_to_traps (const cairo_path_fixed_t *path,
				 cairo_fill_rule_t fill_rule,
				 double tolerance,
				 cairo_traps_t *traps)
{
    cairo_polygon_t polygon;
    cairo_status_t status;

    if (path->is_empty_fill)
	return CAIRO_STATUS_SUCCESS;

    _cairo_polygon_init (&polygon);
    if (traps->num_limits)
	_cairo_polygon_limit (&polygon, traps->limits, traps->num_limits);

    status = _cairo_path_fixed_fill_to_polygon (path,
						tolerance,
						&polygon);
    if (unlikely (status || polygon.num_edges == 0))
	goto CLEANUP;

    if (path->is_rectilinear) {
	status = _cairo_bentley_ottmann_tessellate_rectilinear_polygon (traps,
									&polygon,
									fill_rule);
    } else {
	status = _cairo_bentley_ottmann_tessellate_polygon (traps,
							    &polygon,
							    fill_rule);
    }

  CLEANUP:
    _cairo_polygon_fini (&polygon);
    return status;
}

static cairo_region_t *
_cairo_path_fixed_fill_rectilinear_tessellate_to_region (const cairo_path_fixed_t	*path,
							 cairo_fill_rule_t	 fill_rule,
							 const cairo_rectangle_int_t *extents)
{
    cairo_box_t box;
    cairo_polygon_t polygon;
    cairo_traps_t traps;
    cairo_status_t status;
    cairo_region_t *region;

    /* first try to bypass fill-to-polygon */
    _cairo_traps_init (&traps);
    status = _cairo_path_fixed_fill_rectilinear_to_traps (path,
							  fill_rule,
							  &traps);
    if (_cairo_status_is_error (status))
	goto CLEANUP_TRAPS;

    if (status == CAIRO_STATUS_SUCCESS) {
	status = _cairo_traps_extract_region (&traps, &region);
	goto CLEANUP_TRAPS;
    }

    /* path is not rectangular, try extracting clipped rectilinear edges */
    _cairo_polygon_init (&polygon);
    if (extents != NULL) {
	_cairo_box_from_rectangle (&box, extents);
	_cairo_polygon_limit (&polygon, &box, 1);
    }

    /* tolerance will be ignored as the path is rectilinear */
    status = _cairo_path_fixed_fill_to_polygon (path, 0., &polygon);
    if (unlikely (status))
	goto CLEANUP_POLYGON;

    if (polygon.num_edges == 0) {
	region = cairo_region_create ();
    } else {
	status =
	    _cairo_bentley_ottmann_tessellate_rectilinear_polygon (&traps,
								   &polygon,
								   fill_rule);
	if (likely (status == CAIRO_STATUS_SUCCESS))
	    status = _cairo_traps_extract_region (&traps, &region);
    }

  CLEANUP_POLYGON:
    _cairo_polygon_fini (&polygon);

  CLEANUP_TRAPS:
    _cairo_traps_fini (&traps);

    if (unlikely (status))
	region = _cairo_region_create_in_error (status);

    return region;
}

/* This special-case filler supports only a path that describes a
 * device-axis aligned rectangle. It exists to avoid the overhead of
 * the general tessellator when drawing very common rectangles.
 *
 * If the path described anything but a device-axis aligned rectangle,
 * this function will abort.
 */
cairo_region_t *
_cairo_path_fixed_fill_rectilinear_to_region (const cairo_path_fixed_t	*path,
					      cairo_fill_rule_t	 fill_rule,
					      const cairo_rectangle_int_t *extents)
{
    cairo_rectangle_int_t rectangle_stack[CAIRO_STACK_ARRAY_LENGTH (cairo_rectangle_int_t)];
    cairo_box_t box;
    cairo_region_t *region = NULL;

    assert (path->maybe_fill_region);
    assert (! path->is_empty_fill);

    if (_cairo_path_fixed_is_box (path, &box)) {
	rectangle_stack[0].x = _cairo_fixed_integer_part (box.p1.x);
	rectangle_stack[0].y = _cairo_fixed_integer_part (box.p1.y);
	rectangle_stack[0].width = _cairo_fixed_integer_part (box.p2.x) -
	                            rectangle_stack[0].x;
	rectangle_stack[0].height = _cairo_fixed_integer_part (box.p2.y) -
	                            rectangle_stack[0].y;
	if (! _cairo_rectangle_intersect (&rectangle_stack[0], extents))
	    region = cairo_region_create ();
	else
	    region = cairo_region_create_rectangle (&rectangle_stack[0]);
    } else if (fill_rule == CAIRO_FILL_RULE_WINDING) {
	cairo_rectangle_int_t *rects = rectangle_stack;
	cairo_path_fixed_iter_t iter;
	int last_cw = -1;
	int size = ARRAY_LENGTH (rectangle_stack);
	int count = 0;

	/* Support a series of rectangles as can be expected to describe a
	 * GdkRegion clip region during exposes.
	 */
	_cairo_path_fixed_iter_init (&iter, path);
	while (_cairo_path_fixed_iter_is_fill_box (&iter, &box)) {
	    int cw = 0;

	    if (box.p1.x > box.p2.x) {
		cairo_fixed_t t;

		t = box.p1.x;
		box.p1.x = box.p2.x;
		box.p2.x = t;

		cw = ! cw;
	    }

	    if (box.p1.y > box.p2.y) {
		cairo_fixed_t t;

		t = box.p1.y;
		box.p1.y = box.p2.y;
		box.p2.y = t;

		cw = ! cw;
	    }

	    if (last_cw < 0)
		last_cw = cw;
	    else if (last_cw != cw)
		goto TESSELLATE;

	    if (count == size) {
		cairo_rectangle_int_t *new_rects;

		size *= 4;
		if (rects == rectangle_stack) {
		    new_rects = _cairo_malloc_ab (size,
						  sizeof (cairo_rectangle_int_t));
		    if (unlikely (new_rects == NULL)) {
			/* XXX _cairo_region_nil */
			break;
		    }
		    memcpy (new_rects, rects, sizeof (rectangle_stack));
		} else {
		    new_rects = _cairo_realloc_ab (rects, size,
						   sizeof (cairo_rectangle_int_t));
		    if (unlikely (new_rects == NULL)) {
			/* XXX _cairo_region_nil */
			break;
		    }
		}
		rects = new_rects;
	    }

	    rects[count].x = _cairo_fixed_integer_part (box.p1.x);
	    rects[count].y = _cairo_fixed_integer_part (box.p1.y);
	    rects[count].width = _cairo_fixed_integer_part (box.p2.x) - rects[count].x;
	    rects[count].height = _cairo_fixed_integer_part (box.p2.y) - rects[count].y;
	    if (_cairo_rectangle_intersect (&rects[count], extents))
		count++;
	}

	if (_cairo_path_fixed_iter_at_end (&iter))
	    region = cairo_region_create_rectangles (rects, count);

TESSELLATE:
	if (rects != rectangle_stack)
	    free (rects);
    }

    if (region == NULL) {
	/* Hmm, complex polygon */
	region = _cairo_path_fixed_fill_rectilinear_tessellate_to_region (path,
									  fill_rule,
									  extents);


    }

    return region;
}

cairo_int_status_t
_cairo_path_fixed_fill_rectilinear_to_traps (const cairo_path_fixed_t *path,
					     cairo_fill_rule_t fill_rule,
					     cairo_traps_t *traps)
{
    cairo_box_t box;
    cairo_status_t status;

    traps->is_rectilinear = TRUE;
    traps->is_rectangular = TRUE;

    if (_cairo_path_fixed_is_box (path, &box)) {
	return _cairo_traps_tessellate_rectangle (traps, &box.p1, &box.p2);
    } else {
	cairo_path_fixed_iter_t iter;

	_cairo_path_fixed_iter_init (&iter, path);
	while (_cairo_path_fixed_iter_is_fill_box (&iter, &box)) {
	    if (box.p1.y > box.p2.y) {
		cairo_fixed_t t;

		t = box.p1.y;
		box.p1.y = box.p2.y;
		box.p2.y = t;

		t = box.p1.x;
		box.p1.x = box.p2.x;
		box.p2.x = t;
	    }

	    status = _cairo_traps_tessellate_rectangle (traps,
							&box.p1, &box.p2);
	    if (unlikely (status)) {
		_cairo_traps_clear (traps);
		return status;
	    }
	}

	if (_cairo_path_fixed_iter_at_end (&iter))
	    return _cairo_bentley_ottmann_tessellate_rectangular_traps (traps, fill_rule);

	_cairo_traps_clear (traps);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
}

static cairo_status_t
_cairo_path_fixed_fill_rectilinear_tessellate_to_boxes (const cairo_path_fixed_t *path,
							cairo_fill_rule_t fill_rule,
							cairo_boxes_t *boxes)
{
    cairo_polygon_t polygon;
    cairo_status_t status;

    _cairo_polygon_init (&polygon);
    if (boxes->num_limits) {
	_cairo_polygon_limit (&polygon, boxes->limits, boxes->num_limits);
	boxes->num_limits = 0;
    }

    /* tolerance will be ignored as the path is rectilinear */
    status = _cairo_path_fixed_fill_to_polygon (path, 0., &polygon);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	status =
	    _cairo_bentley_ottmann_tessellate_rectilinear_polygon_to_boxes (&polygon,
									    fill_rule,
									    boxes);
    }

    _cairo_polygon_fini (&polygon);

    return status;
}

cairo_status_t
_cairo_path_fixed_fill_rectilinear_to_boxes (const cairo_path_fixed_t *path,
					     cairo_fill_rule_t fill_rule,
					     cairo_boxes_t *boxes)
{
    cairo_path_fixed_iter_t iter;
    cairo_status_t status;
    cairo_box_t box;

    if (_cairo_path_fixed_is_box (path, &box))
	return _cairo_boxes_add (boxes, &box);

    _cairo_path_fixed_iter_init (&iter, path);
    while (_cairo_path_fixed_iter_is_fill_box (&iter, &box)) {
	if (box.p1.y == box.p2.y || box.p1.x == box.p2.x)
	    continue;

	if (box.p1.y > box.p2.y) {
	    cairo_fixed_t t;

	    t = box.p1.y;
	    box.p1.y = box.p2.y;
	    box.p2.y = t;

	    t = box.p1.x;
	    box.p1.x = box.p2.x;
	    box.p2.x = t;
	}

	status = _cairo_boxes_add (boxes, &box);
	if (unlikely (status))
	    return status;
    }

    if (_cairo_path_fixed_iter_at_end (&iter))
	return _cairo_bentley_ottmann_tessellate_boxes (boxes, fill_rule, boxes);

    /* path is not rectangular, try extracting clipped rectilinear edges */
    _cairo_boxes_clear (boxes);
    return _cairo_path_fixed_fill_rectilinear_tessellate_to_boxes (path,
								   fill_rule,
								   boxes);
}
