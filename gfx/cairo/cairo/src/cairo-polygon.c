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

#include <stdlib.h>
#include "cairoint.h"

/* private functions */

static cairo_status_t
_cairo_polygon_grow (cairo_polygon_t *polygon);

void
_cairo_polygon_init (cairo_polygon_t *polygon)
{
    polygon->num_edges = 0;

    polygon->edges_size = 0;
    polygon->edges = NULL;

    polygon->has_current_point = FALSE;
}

void
_cairo_polygon_fini (cairo_polygon_t *polygon)
{
    if (polygon->edges && polygon->edges != polygon->edges_embedded)
	free (polygon->edges);

    polygon->edges = NULL;
    polygon->edges_size = 0;
    polygon->num_edges = 0;

    polygon->has_current_point = FALSE;
}

/* make room for at least one more edge */
static cairo_status_t
_cairo_polygon_grow (cairo_polygon_t *polygon)
{
    cairo_edge_t *new_edges;
    int old_size = polygon->edges_size;
    int embedded_size = sizeof (polygon->edges_embedded) / sizeof (polygon->edges_embedded[0]);
    int new_size = 2 * MAX (old_size, 16);

    /* we have a local buffer at polygon->edges_embedded.  try to fulfill the request
     * from there. */
    if (old_size < embedded_size) {
	polygon->edges = polygon->edges_embedded;
	polygon->edges_size = embedded_size;
	return CAIRO_STATUS_SUCCESS;
    }

    assert (polygon->num_edges <= polygon->edges_size);

    if (polygon->edges == polygon->edges_embedded) {
	new_edges = malloc (new_size * sizeof (cairo_edge_t));
	if (new_edges)
	    memcpy (new_edges, polygon->edges, old_size * sizeof (cairo_edge_t));
    } else {
	new_edges = realloc (polygon->edges, new_size * sizeof (cairo_edge_t));
    }

    if (new_edges == NULL) {
	return CAIRO_STATUS_NO_MEMORY;
    }

    polygon->edges = new_edges;
    polygon->edges_size = new_size;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_polygon_add_edge (cairo_polygon_t *polygon, cairo_point_t *p1, cairo_point_t *p2)
{
    cairo_status_t status;
    cairo_edge_t *edge;

    /* drop horizontal edges */
    if (p1->y == p2->y) {
	goto DONE;
    }

    if (polygon->num_edges >= polygon->edges_size) {
	status = _cairo_polygon_grow (polygon);
	if (status) {
	    return status;
	}
    }

    edge = &polygon->edges[polygon->num_edges];
    if (p1->y < p2->y) {
	edge->edge.p1 = *p1;
	edge->edge.p2 = *p2;
	edge->clockWise = 1;
    } else {
	edge->edge.p1 = *p2;
	edge->edge.p2 = *p1;
	edge->clockWise = 0;
    }

    polygon->num_edges++;

  DONE:
    _cairo_polygon_move_to (polygon, p2);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_polygon_move_to (cairo_polygon_t *polygon, cairo_point_t *point)
{
    if (! polygon->has_current_point)
	polygon->first_point = *point;
    polygon->current_point = *point;
    polygon->has_current_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_polygon_line_to (cairo_polygon_t *polygon, cairo_point_t *point)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (polygon->has_current_point) {
	status = _cairo_polygon_add_edge (polygon, &polygon->current_point, point);
    } else {
	_cairo_polygon_move_to (polygon, point);
    }

    return status;
}

cairo_status_t
_cairo_polygon_close (cairo_polygon_t *polygon)
{
    cairo_status_t status;

    if (polygon->has_current_point) {
	status = _cairo_polygon_add_edge (polygon,
					  &polygon->current_point,
					  &polygon->first_point);
	if (status)
	    return status;

	polygon->has_current_point = FALSE;
    }

    return CAIRO_STATUS_SUCCESS;
}
