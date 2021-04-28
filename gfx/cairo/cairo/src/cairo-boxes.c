/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Intel Corporation
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
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-boxes-private.h"
#include "cairo-error-private.h"

void
_cairo_boxes_init (cairo_boxes_t *boxes)
{
    boxes->status = CAIRO_STATUS_SUCCESS;
    boxes->num_limits = 0;
    boxes->num_boxes = 0;

    boxes->tail = &boxes->chunks;
    boxes->chunks.next = NULL;
    boxes->chunks.base = boxes->boxes_embedded;
    boxes->chunks.size = ARRAY_LENGTH (boxes->boxes_embedded);
    boxes->chunks.count = 0;

    boxes->is_pixel_aligned = TRUE;
}

void
_cairo_boxes_init_for_array (cairo_boxes_t *boxes,
			     cairo_box_t *array,
			     int num_boxes)
{
    int n;

    boxes->status = CAIRO_STATUS_SUCCESS;
    boxes->num_limits = 0;
    boxes->num_boxes = num_boxes;

    boxes->tail = &boxes->chunks;
    boxes->chunks.next = NULL;
    boxes->chunks.base = array;
    boxes->chunks.size = num_boxes;
    boxes->chunks.count = num_boxes;

    for (n = 0; n < num_boxes; n++) {
	if (! _cairo_fixed_is_integer (array[n].p1.x) ||
	    ! _cairo_fixed_is_integer (array[n].p1.y) ||
	    ! _cairo_fixed_is_integer (array[n].p2.x) ||
	    ! _cairo_fixed_is_integer (array[n].p2.y))
	{
	    break;
	}
    }

    boxes->is_pixel_aligned = n == num_boxes;
}

void
_cairo_boxes_limit (cairo_boxes_t	*boxes,
		    const cairo_box_t	*limits,
		    int			 num_limits)
{
    int n;

    boxes->limits = limits;
    boxes->num_limits = num_limits;

    if (boxes->num_limits) {
	boxes->limit = limits[0];
	for (n = 1; n < num_limits; n++) {
	    if (limits[n].p1.x < boxes->limit.p1.x)
		boxes->limit.p1.x = limits[n].p1.x;

	    if (limits[n].p1.y < boxes->limit.p1.y)
		boxes->limit.p1.y = limits[n].p1.y;

	    if (limits[n].p2.x > boxes->limit.p2.x)
		boxes->limit.p2.x = limits[n].p2.x;

	    if (limits[n].p2.y > boxes->limit.p2.y)
		boxes->limit.p2.y = limits[n].p2.y;
	}
    }
}

static void
_cairo_boxes_add_internal (cairo_boxes_t *boxes,
			   const cairo_box_t *box)
{
    struct _cairo_boxes_chunk *chunk;

    if (unlikely (boxes->status))
	return;

    chunk = boxes->tail;
    if (unlikely (chunk->count == chunk->size)) {
	int size;

	size = chunk->size * 2;
	chunk->next = _cairo_malloc_ab_plus_c (size,
					       sizeof (cairo_box_t),
					       sizeof (struct _cairo_boxes_chunk));

	if (unlikely (chunk->next == NULL)) {
	    boxes->status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    return;
	}

	chunk = chunk->next;
	boxes->tail = chunk;

	chunk->next = NULL;
	chunk->count = 0;
	chunk->size = size;
	chunk->base = (cairo_box_t *) (chunk + 1);
    }

    chunk->base[chunk->count++] = *box;
    boxes->num_boxes++;

    if (boxes->is_pixel_aligned) {
	boxes->is_pixel_aligned =
	    _cairo_fixed_is_integer (box->p1.x) &&
	    _cairo_fixed_is_integer (box->p1.y) &&
	    _cairo_fixed_is_integer (box->p2.x) &&
	    _cairo_fixed_is_integer (box->p2.y);
    }
}

cairo_status_t
_cairo_boxes_add (cairo_boxes_t *boxes,
		  const cairo_box_t *box)
{
    if (box->p1.y == box->p2.y)
	return CAIRO_STATUS_SUCCESS;

    if (box->p1.x == box->p2.x)
	return CAIRO_STATUS_SUCCESS;

    if (boxes->num_limits) {
	cairo_point_t p1, p2;
	cairo_bool_t reversed = FALSE;
	int n;

	/* support counter-clockwise winding for rectangular tessellation */
	if (box->p1.x < box->p2.x) {
	    p1.x = box->p1.x;
	    p2.x = box->p2.x;
	} else {
	    p2.x = box->p1.x;
	    p1.x = box->p2.x;
	    reversed = ! reversed;
	}

	if (p1.x >= boxes->limit.p2.x || p2.x <= boxes->limit.p1.x)
	    return CAIRO_STATUS_SUCCESS;

	if (box->p1.y < box->p2.y) {
	    p1.y = box->p1.y;
	    p2.y = box->p2.y;
	} else {
	    p2.y = box->p1.y;
	    p1.y = box->p2.y;
	    reversed = ! reversed;
	}

	if (p1.y >= boxes->limit.p2.y || p2.y <= boxes->limit.p1.y)
	    return CAIRO_STATUS_SUCCESS;

	for (n = 0; n < boxes->num_limits; n++) {
	    const cairo_box_t *limits = &boxes->limits[n];
	    cairo_box_t _box;
	    cairo_point_t _p1, _p2;

	    if (p1.x >= limits->p2.x || p2.x <= limits->p1.x)
		continue;
	    if (p1.y >= limits->p2.y || p2.y <= limits->p1.y)
		continue;

	    /* Otherwise, clip the box to the limits. */
	    _p1 = p1;
	    if (_p1.x < limits->p1.x)
		_p1.x = limits->p1.x;
	    if (_p1.y < limits->p1.y)
		_p1.y = limits->p1.y;

	    _p2 = p2;
	    if (_p2.x > limits->p2.x)
		_p2.x = limits->p2.x;
	    if (_p2.y > limits->p2.y)
		_p2.y = limits->p2.y;

	    if (_p2.y <= _p1.y || _p2.x <= _p1.x)
		continue;

	    _box.p1.y = _p1.y;
	    _box.p2.y = _p2.y;
	    if (reversed) {
		_box.p1.x = _p2.x;
		_box.p2.x = _p1.x;
	    } else {
		_box.p1.x = _p1.x;
		_box.p2.x = _p2.x;
	    }

	    _cairo_boxes_add_internal (boxes, &_box);
	}
    } else {
	_cairo_boxes_add_internal (boxes, box);
    }

    return boxes->status;
}

void
_cairo_boxes_extents (const cairo_boxes_t *boxes,
		      cairo_rectangle_int_t *extents)
{
    const struct _cairo_boxes_chunk *chunk;
    cairo_box_t box;
    int i;

    box.p1.y = box.p1.x = INT_MAX;
    box.p2.y = box.p2.x = INT_MIN;

    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	const cairo_box_t *b = chunk->base;
	for (i = 0; i < chunk->count; i++) {
	    if (b[i].p1.x < box.p1.x)
		box.p1.x = b[i].p1.x;

	    if (b[i].p1.y < box.p1.y)
		box.p1.y = b[i].p1.y;

	    if (b[i].p2.x > box.p2.x)
		box.p2.x = b[i].p2.x;

	    if (b[i].p2.y > box.p2.y)
		box.p2.y = b[i].p2.y;
	}
    }

    _cairo_box_round_to_rectangle (&box, extents);
}

void
_cairo_boxes_clear (cairo_boxes_t *boxes)
{
    struct _cairo_boxes_chunk *chunk, *next;

    for (chunk = boxes->chunks.next; chunk != NULL; chunk = next) {
	next = chunk->next;
	free (chunk);
    }

    boxes->tail = &boxes->chunks;
    boxes->chunks.next = 0;
    boxes->chunks.count = 0;
    boxes->num_boxes = 0;

    boxes->is_pixel_aligned = TRUE;
}

void
_cairo_boxes_fini (cairo_boxes_t *boxes)
{
    struct _cairo_boxes_chunk *chunk, *next;

    for (chunk = boxes->chunks.next; chunk != NULL; chunk = next) {
	next = chunk->next;
	free (chunk);
    }
}
