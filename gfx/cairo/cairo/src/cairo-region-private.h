/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2007 Mozilla Corporation
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
 * The Initial Developer of the Original Code is Mozilla Corporation
 *
 * Contributor(s):
 *	Vladimir Vukicevic <vladimir@pobox.com>
 */

#ifndef CAIRO_REGION_PRIVATE_H
#define CAIRO_REGION_PRIVATE_H

#include <pixman.h>

#include "cairo-compiler-private.h"

/* #cairo_region_t is defined in cairoint.h */

struct _cairo_region {
    pixman_region16_t rgn;
};

cairo_private void
_cairo_region_init (cairo_region_t *region);

cairo_private void
_cairo_region_init_rect (cairo_region_t *region,
			 cairo_rectangle_int_t *rect);

cairo_private cairo_int_status_t
_cairo_region_init_boxes (cairo_region_t *region,
			  cairo_box_int_t *boxes,
			  int count);

cairo_private void
_cairo_region_fini (cairo_region_t *region);

cairo_private cairo_int_status_t
_cairo_region_copy (cairo_region_t *dst,
		    cairo_region_t *src);

cairo_private int
_cairo_region_num_boxes (cairo_region_t *region);

cairo_private cairo_int_status_t
_cairo_region_get_boxes (cairo_region_t *region,
			 int *num_boxes,
			 cairo_box_int_t **boxes);

cairo_private void
_cairo_region_boxes_fini (cairo_region_t *region,
			  cairo_box_int_t *boxes);

cairo_private void
_cairo_region_get_extents (cairo_region_t *region,
			   cairo_rectangle_int_t *extents);

cairo_private cairo_int_status_t
_cairo_region_subtract (cairo_region_t *dst,
			cairo_region_t *a,
			cairo_region_t *b);

cairo_private cairo_int_status_t
_cairo_region_intersect (cairo_region_t *dst,
			 cairo_region_t *a,
			 cairo_region_t *b);

cairo_private cairo_int_status_t
_cairo_region_union_rect (cairo_region_t *dst,
			  cairo_region_t *src,
			  cairo_rectangle_int_t *rect);

cairo_private cairo_bool_t
_cairo_region_not_empty (cairo_region_t *region);

cairo_private void
_cairo_region_translate (cairo_region_t *region,
			 int x, int y);

cairo_private pixman_region_overlap_t
_cairo_region_contains_rectangle (cairo_region_t *region, cairo_rectangle_int_t *box);


#endif /* CAIRO_REGION_PRIVATE_H */
