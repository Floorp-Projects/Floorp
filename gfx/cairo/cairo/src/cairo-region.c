/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Owen Taylor <otaylor@redhat.com>
 *      Vladimir Vukicevic <vladimir@pobox.com>
 */

#include "cairoint.h"

void
_cairo_region_init (cairo_region_t *region)
{
    pixman_region_init (&region->rgn);
}

void
_cairo_region_init_rect (cairo_region_t *region,
			 cairo_rectangle_int_t *rect)
{
    pixman_region_init_rect (&region->rgn,
			     rect->x, rect->y,
			     rect->width, rect->height);
}

cairo_int_status_t
_cairo_region_init_boxes (cairo_region_t *region,
			  cairo_box_int_t *boxes,
			  int count)
{
    pixman_box16_t stack_pboxes[CAIRO_STACK_ARRAY_LENGTH (pixman_box16_t)];
    pixman_box16_t *pboxes = stack_pboxes;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    int i;

    if (count > ARRAY_LENGTH(stack_pboxes)) {
	pboxes = _cairo_malloc_ab (count, sizeof(pixman_box16_t));
	if (pboxes == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < count; i++) {
	pboxes[i].x1 = boxes[i].p1.x;
	pboxes[i].y1 = boxes[i].p1.y;
	pboxes[i].x2 = boxes[i].p2.x;
	pboxes[i].y2 = boxes[i].p2.y;
    }

    if (!pixman_region_init_rects (&region->rgn, pboxes, count))
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (pboxes != stack_pboxes)
	free (pboxes);

    return status;
}

void
_cairo_region_fini (cairo_region_t *region)
{
    pixman_region_fini (&region->rgn);
}

cairo_int_status_t
_cairo_region_copy (cairo_region_t *dst, cairo_region_t *src)
{
    if (!pixman_region_copy (&dst->rgn, &src->rgn))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

int
_cairo_region_num_boxes (cairo_region_t *region)
{
    return pixman_region_n_rects (&region->rgn);
}

cairo_int_status_t
_cairo_region_get_boxes (cairo_region_t *region, int *num_boxes, cairo_box_int_t **boxes)
{
    int nboxes;
    pixman_box16_t *pboxes;
    cairo_box_int_t *cboxes;
    int i;

    pboxes = pixman_region_rectangles (&region->rgn, &nboxes);

    if (nboxes == 0) {
	*num_boxes = 0;
	*boxes = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    cboxes = _cairo_malloc_ab (nboxes, sizeof(cairo_box_int_t));
    if (cboxes == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    for (i = 0; i < nboxes; i++) {
	cboxes[i].p1.x = pboxes[i].x1;
	cboxes[i].p1.y = pboxes[i].y1;
	cboxes[i].p2.x = pboxes[i].x2;
	cboxes[i].p2.y = pboxes[i].y2;
    }

    *num_boxes = nboxes;
    *boxes = cboxes;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_region_boxes_fini (cairo_region_t *region, cairo_box_int_t *boxes)
{
    free (boxes);
}

/**
 * _cairo_region_get_extents:
 * @region: a #cairo_region_t
 * @rect: rectangle into which to store the extents
 *
 * Gets the bounding box of a region as a #cairo_rectangle_int_t
 **/
void
_cairo_region_get_extents (cairo_region_t *region, cairo_rectangle_int_t *extents)
{
    pixman_box16_t *pextents = pixman_region_extents (&region->rgn);

    extents->x = pextents->x1;
    extents->y = pextents->y1;
    extents->width = pextents->x2 - pextents->x1;
    extents->height = pextents->y2 - pextents->y1;
}

cairo_int_status_t
_cairo_region_subtract (cairo_region_t *dst, cairo_region_t *a, cairo_region_t *b)
{
    if (!pixman_region_subtract (&dst->rgn, &a->rgn, &b->rgn))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_region_intersect (cairo_region_t *dst, cairo_region_t *a, cairo_region_t *b)
{
    if (!pixman_region_intersect (&dst->rgn, &a->rgn, &b->rgn))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_region_union_rect (cairo_region_t *dst,
			  cairo_region_t *src,
			  cairo_rectangle_int_t *rect)
{
    if (!pixman_region_union_rect (&dst->rgn, &src->rgn,
				   rect->x, rect->y,
				   rect->width, rect->height))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

cairo_bool_t
_cairo_region_not_empty (cairo_region_t *region)
{
    return (cairo_bool_t) pixman_region_not_empty (&region->rgn);
}

void
_cairo_region_translate (cairo_region_t *region,
			 int x, int y)
{
    pixman_region_translate (&region->rgn, x, y);
}

pixman_region_overlap_t
_cairo_region_contains_rectangle (cairo_region_t *region, cairo_rectangle_int_t *rect)
{
    pixman_box16_t pbox;

    pbox.x1 = rect->x;
    pbox.y1 = rect->y;
    pbox.x2 = rect->x + rect->width;
    pbox.y2 = rect->y + rect->height;

    return pixman_region_contains_rectangle (&region->rgn, &pbox);
}
