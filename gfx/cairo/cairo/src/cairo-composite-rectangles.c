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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *      Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-error-private.h"
#include "cairo-composite-rectangles-private.h"

/* A collection of routines to facilitate writing compositors. */

static inline cairo_bool_t
_cairo_composite_rectangles_init (cairo_composite_rectangles_t *extents,
				  const cairo_rectangle_int_t *surface_extents,
				  cairo_operator_t op,
				  const cairo_pattern_t *source,
				  cairo_clip_t *clip)
{
    extents->unbounded = *surface_extents;

    if (clip != NULL) {
	const cairo_rectangle_int_t *clip_extents;

	clip_extents = _cairo_clip_get_extents (clip);
	if (clip_extents == NULL)
	    return FALSE;

	if (! _cairo_rectangle_intersect (&extents->unbounded, clip_extents))
	    return FALSE;
    }

    extents->bounded = extents->unbounded;
    extents->is_bounded = _cairo_operator_bounded_by_either (op);

    _cairo_pattern_get_extents (source, &extents->source);
    if (extents->is_bounded & CAIRO_OPERATOR_BOUND_BY_SOURCE) {
	if (! _cairo_rectangle_intersect (&extents->bounded, &extents->source))
	    return FALSE;
    }

    return TRUE;
}

cairo_int_status_t
_cairo_composite_rectangles_init_for_paint (cairo_composite_rectangles_t *extents,
					    const cairo_rectangle_int_t *surface_extents,
					    cairo_operator_t		 op,
					    const cairo_pattern_t	*source,
					    cairo_clip_t		*clip)
{
    if (! _cairo_composite_rectangles_init (extents,
					    surface_extents,
					    op, source, clip))
    {
	return CAIRO_INT_STATUS_NOTHING_TO_DO;
    }

    extents->mask = extents->bounded;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_composite_rectangles_intersect (cairo_composite_rectangles_t *extents)
{
    cairo_bool_t ret;

    ret = _cairo_rectangle_intersect (&extents->bounded, &extents->mask);
    if (! ret && extents->is_bounded & CAIRO_OPERATOR_BOUND_BY_MASK)
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_composite_rectangles_init_for_mask (cairo_composite_rectangles_t *extents,
					   const cairo_rectangle_int_t *surface_extents,
					   cairo_operator_t		 op,
					   const cairo_pattern_t	*source,
					   const cairo_pattern_t	*mask,
					   cairo_clip_t			*clip)
{
    if (! _cairo_composite_rectangles_init (extents,
					    surface_extents,
					    op, source, clip))
    {
	return CAIRO_INT_STATUS_NOTHING_TO_DO;
    }

    _cairo_pattern_get_extents (mask, &extents->mask);

    return _cairo_composite_rectangles_intersect (extents);
}

cairo_int_status_t
_cairo_composite_rectangles_init_for_stroke (cairo_composite_rectangles_t *extents,
					     const cairo_rectangle_int_t *surface_extents,
					     cairo_operator_t		 op,
					     const cairo_pattern_t	*source,
					     cairo_path_fixed_t		*path,
					     const cairo_stroke_style_t	*style,
					     const cairo_matrix_t	*ctm,
					     cairo_clip_t		*clip)
{
    if (! _cairo_composite_rectangles_init (extents,
					    surface_extents,
					    op, source, clip))
    {
	return CAIRO_INT_STATUS_NOTHING_TO_DO;
    }

    _cairo_path_fixed_approximate_stroke_extents (path, style, ctm, &extents->mask);

    return _cairo_composite_rectangles_intersect (extents);
}

cairo_int_status_t
_cairo_composite_rectangles_init_for_fill (cairo_composite_rectangles_t *extents,
					   const cairo_rectangle_int_t *surface_extents,
					   cairo_operator_t		 op,
					   const cairo_pattern_t	*source,
					   cairo_path_fixed_t		*path,
					   cairo_clip_t			*clip)
{
    if (! _cairo_composite_rectangles_init (extents,
					    surface_extents,
					    op, source, clip))
    {
	return CAIRO_INT_STATUS_NOTHING_TO_DO;
    }

    _cairo_path_fixed_approximate_fill_extents (path, &extents->mask);

    return _cairo_composite_rectangles_intersect (extents);
}

cairo_int_status_t
_cairo_composite_rectangles_init_for_glyphs (cairo_composite_rectangles_t *extents,
					     const cairo_rectangle_int_t *surface_extents,
					     cairo_operator_t		 op,
					     const cairo_pattern_t	*source,
					     cairo_scaled_font_t	*scaled_font,
					     cairo_glyph_t		*glyphs,
					     int			 num_glyphs,
					     cairo_clip_t		*clip,
					     cairo_bool_t		*overlap)
{
    cairo_status_t status;

    if (! _cairo_composite_rectangles_init (extents,
					    surface_extents,
					    op, source, clip))
    {
	return CAIRO_INT_STATUS_NOTHING_TO_DO;
    }

    status = _cairo_scaled_font_glyph_device_extents (scaled_font,
						      glyphs, num_glyphs,
						      &extents->mask,
						      overlap);
    if (unlikely (status))
	return status;

    return _cairo_composite_rectangles_intersect (extents);
}
