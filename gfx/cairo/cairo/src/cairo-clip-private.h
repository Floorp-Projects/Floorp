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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 */

#ifndef CAIRO_CLIP_PRIVATE_H
#define CAIRO_CLIP_PRIVATE_H

#include "cairo-types-private.h"
#include "cairo-compiler-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-reference-count-private.h"

extern const cairo_private cairo_rectangle_list_t _cairo_rectangles_nil;

enum {
    CAIRO_CLIP_PATH_HAS_REGION = 0x1,
    CAIRO_CLIP_PATH_REGION_IS_UNSUPPORTED = 0x2,
    CAIRO_CLIP_PATH_IS_BOX = 0x4
};

struct _cairo_clip_path {
    cairo_reference_count_t	 ref_count;
    cairo_path_fixed_t		 path;
    cairo_fill_rule_t		 fill_rule;
    double			 tolerance;
    cairo_antialias_t		 antialias;
    cairo_clip_path_t		*prev;

    cairo_rectangle_int_t extents;

    /* partial caches */
    unsigned int flags;
    cairo_region_t *region;
    cairo_surface_t *surface;
};

struct _cairo_clip {
    /* can be used as a cairo_hash_entry_t for live clips */
    cairo_clip_path_t *path;

    cairo_bool_t all_clipped;

};

cairo_private void
_cairo_clip_init (cairo_clip_t *clip);

cairo_private_no_warn cairo_clip_t *
_cairo_clip_init_copy (cairo_clip_t *clip, cairo_clip_t *other);

cairo_private cairo_status_t
_cairo_clip_init_copy_transformed (cairo_clip_t    *clip,
				   cairo_clip_t    *other,
				   const cairo_matrix_t *matrix);

cairo_private void
_cairo_clip_reset (cairo_clip_t *clip);

cairo_private cairo_bool_t
_cairo_clip_equal (const cairo_clip_t *clip_a,
		   const cairo_clip_t *clip_b);

#define _cairo_clip_fini(clip) _cairo_clip_reset (clip)

cairo_private cairo_status_t
_cairo_clip_rectangle (cairo_clip_t       *clip,
		       const cairo_rectangle_int_t *rectangle);

cairo_private cairo_status_t
_cairo_clip_clip (cairo_clip_t       *clip,
		  const cairo_path_fixed_t *path,
		  cairo_fill_rule_t   fill_rule,
		  double              tolerance,
		  cairo_antialias_t   antialias);

cairo_private cairo_status_t
_cairo_clip_apply_clip (cairo_clip_t *clip,
			const cairo_clip_t *other);

cairo_private const cairo_rectangle_int_t *
_cairo_clip_get_extents (const cairo_clip_t *clip);

cairo_private cairo_surface_t *
_cairo_clip_get_surface (cairo_clip_t *clip, cairo_surface_t *dst, int *tx, int *ty);

cairo_private cairo_status_t
_cairo_clip_combine_with_surface (cairo_clip_t *clip,
				  cairo_surface_t *dst,
				  int dst_x, int dst_y);

cairo_private cairo_int_status_t
_cairo_clip_get_region (cairo_clip_t *clip,
			cairo_region_t **region);

cairo_private cairo_int_status_t
_cairo_clip_get_boxes (cairo_clip_t *clip,
		       cairo_box_t **boxes,
		       int *count);

cairo_private cairo_status_t
_cairo_clip_to_boxes (cairo_clip_t **clip,
		      cairo_composite_rectangles_t *extents,
		      cairo_box_t **boxes,
		      int *num_boxes);

cairo_private cairo_bool_t
_cairo_clip_contains_rectangle (cairo_clip_t *clip,
				const cairo_rectangle_int_t *rect);

cairo_private cairo_bool_t
_cairo_clip_contains_extents (cairo_clip_t *clip,
				const cairo_composite_rectangles_t *extents);

cairo_private void
_cairo_clip_drop_cache (cairo_clip_t  *clip);

cairo_private cairo_rectangle_list_t*
_cairo_clip_copy_rectangle_list (cairo_clip_t *clip, cairo_gstate_t *gstate);

#endif /* CAIRO_CLIP_PRIVATE_H */
