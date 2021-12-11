/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
 * Copyright © 2011 Intel Corporation
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
 *	Behdad Esfahbod <behdad@behdad.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 */

#include "cairoint.h"

#if !CAIRO_HAS_XLIB_XCB_FUNCTIONS

#include "cairo-xlib-private.h"

#include "cairo-compositor-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-surface-offset-private.h"

static const cairo_compositor_t *
_get_compositor (cairo_surface_t *surface)
{
    return ((cairo_image_surface_t *)surface)->compositor;
}

static cairo_bool_t
unclipped (cairo_xlib_surface_t *xlib, cairo_clip_t *clip)
{
    cairo_rectangle_int_t r;

    r.x = r.y = 0;
    r.width = xlib->width;
    r.height = xlib->height;
    return _cairo_clip_contains_rectangle (clip, &r);
}

static cairo_int_status_t
_cairo_xlib_shm_compositor_paint (const cairo_compositor_t	*_compositor,
				  cairo_composite_rectangles_t	*extents)
{
    cairo_xlib_surface_t *xlib = (cairo_xlib_surface_t *)extents->surface;
    cairo_int_status_t status;
    cairo_surface_t *shm;
    cairo_bool_t overwrite;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    overwrite =
	extents->op <= CAIRO_OPERATOR_SOURCE && unclipped (xlib, extents->clip);

    shm = _cairo_xlib_surface_get_shm (xlib, overwrite);
    if (shm == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_compositor_paint (_get_compositor (shm), shm,
				      extents->op,
				      &extents->source_pattern.base,
				      extents->clip);
    if (unlikely (status))
	return status;

    xlib->base.is_clear =
	extents->op == CAIRO_OPERATOR_CLEAR && unclipped (xlib, extents->clip);
    xlib->base.serial++;
    xlib->fallback++;
    return CAIRO_INT_STATUS_NOTHING_TO_DO;
}

static cairo_int_status_t
_cairo_xlib_shm_compositor_mask (const cairo_compositor_t	*_compositor,
				 cairo_composite_rectangles_t	*extents)
{
    cairo_xlib_surface_t *xlib = (cairo_xlib_surface_t *)extents->surface;
    cairo_int_status_t status;
    cairo_surface_t *shm;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    shm = _cairo_xlib_surface_get_shm (xlib, FALSE);
    if (shm == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_compositor_mask (_get_compositor (shm), shm,
				     extents->op,
				     &extents->source_pattern.base,
				     &extents->mask_pattern.base,
				     extents->clip);
    if (unlikely (status))
	return status;

    xlib->base.is_clear = FALSE;
    xlib->base.serial++;
    xlib->fallback++;
    return CAIRO_INT_STATUS_NOTHING_TO_DO;
}

static cairo_int_status_t
_cairo_xlib_shm_compositor_stroke (const cairo_compositor_t	*_compositor,
				   cairo_composite_rectangles_t *extents,
				   const cairo_path_fixed_t	*path,
				   const cairo_stroke_style_t	*style,
				   const cairo_matrix_t		*ctm,
				   const cairo_matrix_t		*ctm_inverse,
				   double			 tolerance,
				   cairo_antialias_t		 antialias)
{
    cairo_xlib_surface_t *xlib = (cairo_xlib_surface_t *)extents->surface;
    cairo_int_status_t status;
    cairo_surface_t *shm;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    shm = _cairo_xlib_surface_get_shm (xlib, FALSE);
    if (shm == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_compositor_stroke (_get_compositor (shm), shm,
				       extents->op,
				       &extents->source_pattern.base,
				       path, style,
				       ctm, ctm_inverse,
				       tolerance,
				       antialias,
				       extents->clip);
    if (unlikely (status))
	return status;

    xlib->base.is_clear = FALSE;
    xlib->base.serial++;
    xlib->fallback++;
    return CAIRO_INT_STATUS_NOTHING_TO_DO;
}

static cairo_int_status_t
_cairo_xlib_shm_compositor_fill (const cairo_compositor_t	*_compositor,
				 cairo_composite_rectangles_t *extents,
				 const cairo_path_fixed_t	*path,
				 cairo_fill_rule_t		 fill_rule,
				 double				 tolerance,
				 cairo_antialias_t		 antialias)
{
    cairo_xlib_surface_t *xlib = (cairo_xlib_surface_t *)extents->surface;
    cairo_int_status_t status;
    cairo_surface_t *shm;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    shm = _cairo_xlib_surface_get_shm (xlib, FALSE);
    if (shm == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_compositor_fill (_get_compositor (shm), shm,
				     extents->op,
				     &extents->source_pattern.base,
				     path,
				     fill_rule, tolerance, antialias,
				     extents->clip);
    if (unlikely (status))
	return status;

    xlib->base.is_clear = FALSE;
    xlib->base.serial++;
    xlib->fallback++;
    return CAIRO_INT_STATUS_NOTHING_TO_DO;
}

static cairo_int_status_t
_cairo_xlib_shm_compositor_glyphs (const cairo_compositor_t	*_compositor,
				   cairo_composite_rectangles_t *extents,
				   cairo_scaled_font_t		*scaled_font,
				   cairo_glyph_t		*glyphs,
				   int				 num_glyphs,
				   cairo_bool_t			 overlap)
{
    cairo_xlib_surface_t *xlib = (cairo_xlib_surface_t *)extents->surface;
    cairo_int_status_t status;
    cairo_surface_t *shm;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    shm = _cairo_xlib_surface_get_shm (xlib, FALSE);
    if (shm == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_compositor_glyphs (_get_compositor (shm), shm,
				       extents->op,
				       &extents->source_pattern.base,
				       glyphs, num_glyphs, scaled_font,
				       extents->clip);
    if (unlikely (status))
	return status;

    xlib->base.is_clear = FALSE;
    xlib->base.serial++;
    xlib->fallback++;
    return CAIRO_INT_STATUS_NOTHING_TO_DO;
}

static const cairo_compositor_t _cairo_xlib_shm_compositor = {
     &_cairo_fallback_compositor,

     _cairo_xlib_shm_compositor_paint,
     _cairo_xlib_shm_compositor_mask,
     _cairo_xlib_shm_compositor_stroke,
     _cairo_xlib_shm_compositor_fill,
     _cairo_xlib_shm_compositor_glyphs,
};

const cairo_compositor_t *
_cairo_xlib_fallback_compositor_get (void)
{
    return &_cairo_xlib_shm_compositor;
}

#endif /* !CAIRO_HAS_XLIB_XCB_FUNCTIONS */
