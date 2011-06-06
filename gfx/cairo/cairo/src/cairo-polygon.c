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

#include "cairo-error-private.h"
#include "cairo-slope-private.h"

void
_cairo_polygon_init (cairo_polygon_t *polygon)
{
    VG (VALGRIND_MAKE_MEM_UNDEFINED (polygon, sizeof (cairo_polygon_t)));

    polygon->status = CAIRO_STATUS_SUCCESS;

    polygon->num_edges = 0;

    polygon->edges = polygon->edges_embedded;
    polygon->edges_size = ARRAY_LENGTH (polygon->edges_embedded);

    polygon->has_current_point = FALSE;
    polygon->has_current_edge = FALSE;
    polygon->num_limits = 0;

    polygon->extents.p1.x = polygon->extents.p1.y = INT32_MAX;
    polygon->extents.p2.x = polygon->extents.p2.y = INT32_MIN;
}

void
_cairo_polygon_limit (cairo_polygon_t	*polygon,
		      const cairo_box_t *limits,
		      int num_limits)
{
    int n;

    polygon->limits = limits;
    polygon->num_limits = num_limits;

    if (polygon->num_limits) {
	polygon->limit = limits[0];
	for (n = 1; n < num_limits; n++) {
	    if (limits[n].p1.x < polygon->limit.p1.x)
		polygon->limit.p1.x = limits[n].p1.x;

	    if (limits[n].p1.y < polygon->limit.p1.y)
		polygon->limit.p1.y = limits[n].p1.y;

	    if (limits[n].p2.x > polygon->limit.p2.x)
		polygon->limit.p2.x = limits[n].p2.x;

	    if (limits[n].p2.y > polygon->limit.p2.y)
		polygon->limit.p2.y = limits[n].p2.y;
	}
    }
}

void
_cairo_polygon_fini (cairo_polygon_t *polygon)
{
    if (polygon->edges != polygon->edges_embedded)
	free (polygon->edges);

    VG (VALGRIND_MAKE_MEM_NOACCESS (polygon, sizeof (cairo_polygon_t)));
}

/* make room for at least one more edge */
static cairo_bool_t
_cairo_polygon_grow (cairo_polygon_t *polygon)
{
    cairo_edge_t *new_edges;
    int old_size = polygon->edges_size;
    int new_size = 4 * old_size;

    if (CAIRO_INJECT_FAULT ()) {
	polygon->status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	return FALSE;
    }

    if (polygon->edges == polygon->edges_embedded) {
	new_edges = _cairo_malloc_ab (new_size, sizeof (cairo_edge_t));
	if (new_edges != NULL)
	    memcpy (new_edges, polygon->edges, old_size * sizeof (cairo_edge_t));
    } else {
	new_edges = _cairo_realloc_ab (polygon->edges,
		                       new_size, sizeof (cairo_edge_t));
    }

    if (unlikely (new_edges == NULL)) {
	polygon->status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	return FALSE;
    }

    polygon->edges = new_edges;
    polygon->edges_size = new_size;

    return TRUE;
}

static void
_add_edge (cairo_polygon_t *polygon,
	   const cairo_point_t *p1,
	   const cairo_point_t *p2,
	   int top, int bottom,
	   int dir)
{
    cairo_edge_t *edge;

    assert (top < bottom);

    if (unlikely (polygon->num_edges == polygon->edges_size)) {
	if (! _cairo_polygon_grow (polygon))
	    return;
    }

    edge = &polygon->edges[polygon->num_edges++];
    edge->line.p1 = *p1;
    edge->line.p2 = *p2;
    edge->top = top;
    edge->bottom = bottom;
    edge->dir = dir;

    if (top < polygon->extents.p1.y)
	polygon->extents.p1.y = top;
    if (bottom > polygon->extents.p2.y)
	polygon->extents.p2.y = bottom;

    if (p1->x < polygon->extents.p1.x || p1->x > polygon->extents.p2.x) {
	cairo_fixed_t x = p1->x;
	if (top != p1->y)
	    x = _cairo_edge_compute_intersection_x_for_y (p1, p2, top);
	if (x < polygon->extents.p1.x)
	    polygon->extents.p1.x = x;
	if (x > polygon->extents.p2.x)
	    polygon->extents.p2.x = x;
    }

    if (p2->x < polygon->extents.p1.x || p2->x > polygon->extents.p2.x) {
	cairo_fixed_t x = p2->x;
	if (bottom != p2->y)
	    x = _cairo_edge_compute_intersection_x_for_y (p1, p2, bottom);
	if (x < polygon->extents.p1.x)
	    polygon->extents.p1.x = x;
	if (x > polygon->extents.p2.x)
	    polygon->extents.p2.x = x;
    }
}

static void
_add_clipped_edge (cairo_polygon_t *polygon,
		   const cairo_point_t *p1,
		   const cairo_point_t *p2,
		   const int top, const int bottom,
		   const int dir)
{
    cairo_point_t p[2];
    int top_y, bot_y;
    int n;

    for (n = 0; n < polygon->num_limits; n++) {
	const cairo_box_t *limits = &polygon->limits[n];

	if (top >= limits->p2.y)
	    continue;
	if (bottom <= limits->p1.y)
	    continue;

	if (p1->x >= limits->p1.x && p2->x >= limits->p1.x &&
	    p1->x <= limits->p2.x && p2->x <= limits->p2.x)
	{
	    top_y = top;
	    if (top_y < limits->p1.y)
		top_y = limits->p1.y;

	    bot_y = bottom;
	    if (bot_y > limits->p2.y)
		bot_y = limits->p2.y;

	    _add_edge (polygon, p1, p2, top_y, bot_y, dir);
	}
	else if (p1->x <= limits->p1.x && p2->x <= limits->p1.x)
	{
	    p[0].x = limits->p1.x;
	    p[0].y = limits->p1.y;
	    top_y = top;
	    if (top_y < p[0].y)
		top_y = p[0].y;

	    p[1].x = limits->p1.x;
	    p[1].y = limits->p2.y;
	    bot_y = bottom;
	    if (bot_y > p[1].y)
		bot_y = p[1].y;

	    _add_edge (polygon, &p[0], &p[1], top_y, bot_y, dir);
	}
	else if (p1->x >= limits->p2.x && p2->x >= limits->p2.x)
	{
	    p[0].x = limits->p2.x;
	    p[0].y = limits->p1.y;
	    top_y = top;
	    if (top_y < p[0].y)
		top_y = p[0].y;

	    p[1].x = limits->p2.x;
	    p[1].y = limits->p2.y;
	    bot_y = bottom;
	    if (bot_y > p[1].y)
		bot_y = p[1].y;

	    _add_edge (polygon, &p[0], &p[1], top_y, bot_y, dir);
	}
	else
	{
	    int left_y, right_y;
	    int p1_y, p2_y;

	    left_y = _cairo_edge_compute_intersection_y_for_x (p1, p2,
							       limits->p1.x);
	    right_y = _cairo_edge_compute_intersection_y_for_x (p1, p2,
								limits->p2.x);

	    if (left_y == right_y) /* horizontal within bounds */
		continue;

	    p1_y = top;
	    p2_y = bottom;

	    if (left_y < right_y) {
		if (p1->x < limits->p1.x && left_y > limits->p1.y) {
		    p[0].x = limits->p1.x;
		    p[0].y = limits->p1.y;
		    top_y = p1_y;
		    if (top_y < p[0].y)
			top_y = p[0].y;

		    p[1].x = limits->p1.x;
		    p[1].y = limits->p2.y;
		    bot_y = left_y;
		    if (bot_y > p[1].y)
			bot_y = p[1].y;

		    if (bot_y > top_y)
			_add_edge (polygon, &p[0], &p[1], top_y, bot_y, dir);
		    p1_y = bot_y;
		}

		if (p2->x > limits->p2.x && right_y < limits->p2.y) {
		    p[0].x = limits->p2.x;
		    p[0].y = limits->p1.y;
		    top_y = right_y;
		    if (top_y < p[0].y)
			top_y = p[0].y;

		    p[1].x = limits->p2.x;
		    p[1].y = limits->p2.y;
		    bot_y = p2_y;
		    if (bot_y > p[1].y)
			bot_y = p[1].y;

		    if (bot_y > top_y)
			_add_edge (polygon, &p[0], &p[1], top_y, bot_y, dir);
		    p2_y = top_y;
		}
	    } else {
		if (p1->x > limits->p2.x && right_y > limits->p1.y) {
		    p[0].x = limits->p2.x;
		    p[0].y = limits->p1.y;
		    top_y = p1_y;
		    if (top_y < p[0].y)
			top_y = p[0].y;

		    p[1].x = limits->p2.x;
		    p[1].y = limits->p2.y;
		    bot_y = right_y;
		    if (bot_y > p[1].y)
			bot_y = p[1].y;

		    if (bot_y > top_y)
			_add_edge (polygon, &p[0], &p[1], top_y, bot_y, dir);
		    p1_y = bot_y;
		}

		if (p2->x < limits->p1.x && left_y < limits->p2.y) {
		    p[0].x = limits->p1.x;
		    p[0].y = limits->p1.y;
		    top_y = left_y;
		    if (top_y < p[0].y)
			top_y = p[0].y;

		    p[1].x = limits->p1.x;
		    p[1].y = limits->p2.y;
		    bot_y = p2_y;
		    if (bot_y > p[1].y)
			bot_y = p[1].y;

		    if (bot_y > top_y)
			_add_edge (polygon, &p[0], &p[1], top_y, bot_y, dir);
		    p2_y = top_y;
		}
	    }

	    if (p1_y < limits->p1.y)
		p1_y = limits->p1.y;
	    if (p2_y > limits->p2.y)
		p2_y = limits->p2.y;
	    if (p2_y > p1_y)
		_add_edge (polygon, p1, p2, p1_y, p2_y, dir);
	}
    }
}

static void
_cairo_polygon_add_edge (cairo_polygon_t *polygon,
			 const cairo_point_t *p1,
			 const cairo_point_t *p2)
{
    int dir;

    /* drop horizontal edges */
    if (p1->y == p2->y)
	return;

    if (p1->y < p2->y) {
	dir = 1;
    } else {
	const cairo_point_t *t;
	t = p1, p1 = p2, p2 = t;
	dir = -1;
    }

    if (polygon->num_limits) {
	if (p2->y <= polygon->limit.p1.y)
	    return;

	if (p1->y >= polygon->limit.p2.y)
	    return;

	_add_clipped_edge (polygon, p1, p2, p1->y, p2->y, dir);
    } else
	_add_edge (polygon, p1, p2, p1->y, p2->y, dir);
}

cairo_status_t
_cairo_polygon_add_external_edge (void *polygon,
				  const cairo_point_t *p1,
				  const cairo_point_t *p2)
{
    _cairo_polygon_add_edge (polygon, p1, p2);
    return _cairo_polygon_status (polygon);
}

cairo_status_t
_cairo_polygon_add_line (cairo_polygon_t *polygon,
			 const cairo_line_t *line,
			 int top, int bottom,
			 int dir)
{
    /* drop horizontal edges */
    if (line->p1.y == line->p2.y)
	return CAIRO_STATUS_SUCCESS;

    if (bottom <= top)
	return CAIRO_STATUS_SUCCESS;

    if (polygon->num_limits) {
	if (line->p2.y <= polygon->limit.p1.y)
	    return CAIRO_STATUS_SUCCESS;

	if (line->p1.y >= polygon->limit.p2.y)
	    return CAIRO_STATUS_SUCCESS;

	_add_clipped_edge (polygon, &line->p1, &line->p2, top, bottom, dir);
    } else
	_add_edge (polygon, &line->p1, &line->p2, top, bottom, dir);

    return polygon->status;
}

/* flattened path operations */

cairo_status_t
_cairo_polygon_move_to (cairo_polygon_t *polygon,
			const cairo_point_t *point)
{
    if (polygon->has_current_edge) {
	_cairo_polygon_add_edge (polygon,
				 &polygon->last_point,
				 &polygon->current_point);
	polygon->has_current_edge = FALSE;
    }

    if (! polygon->has_current_point) {
	polygon->first_point = *point;
	polygon->has_current_point = TRUE;
    }

    polygon->current_point = *point;
    return polygon->status;
}

cairo_status_t
_cairo_polygon_line_to (cairo_polygon_t *polygon,
			const cairo_point_t *point)
{
    /* squash collinear edges */
    if (polygon->has_current_edge) {
	if (polygon->current_point.x != point->x ||
	    polygon->current_point.y != point->y)
	{
	    cairo_slope_t this;

	    _cairo_slope_init (&this, &polygon->current_point, point);
	    if (_cairo_slope_equal (&polygon->current_edge, &this)) {
		polygon->current_point = *point;
		return CAIRO_STATUS_SUCCESS;
	    }

	    _cairo_polygon_add_edge (polygon,
				     &polygon->last_point,
				     &polygon->current_point);

	    polygon->last_point = polygon->current_point;
	    polygon->current_edge = this;
	}
    } else if (polygon->has_current_point) {
	if (polygon->current_point.x != point->x ||
	    polygon->current_point.y != point->y)
	{
	    polygon->last_point = polygon->current_point;
	    _cairo_slope_init (&polygon->current_edge,
			       &polygon->last_point,
			       point);
	    polygon->has_current_edge = TRUE;
	}
    } else {
	polygon->first_point = *point;
	polygon->has_current_point = TRUE;
    }

    polygon->current_point = *point;
    return polygon->status;
}

cairo_status_t
_cairo_polygon_close (cairo_polygon_t *polygon)
{
    cairo_status_t status;

    if (polygon->has_current_point) {
	status = _cairo_polygon_line_to (polygon, &polygon->first_point);
	polygon->has_current_point = FALSE;
    }

    if (polygon->has_current_edge) {
	_cairo_polygon_add_edge (polygon,
				 &polygon->last_point,
				 &polygon->current_point);
	polygon->has_current_edge = FALSE;
    }

    return polygon->status;
}
