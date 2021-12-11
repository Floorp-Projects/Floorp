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

/* The original X drawing API was very restrictive in what it could handle,
 * pixel-aligned fill/blits are all that map into Cairo's drawing model.
 */

#include "cairoint.h"

#if !CAIRO_HAS_XLIB_XCB_FUNCTIONS

#include "cairo-xlib-private.h"
#include "cairo-xlib-surface-private.h"

#include "cairo-boxes-private.h"
#include "cairo-clip-inline.h"
#include "cairo-compositor-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-pattern-private.h"
#include "cairo-region-private.h"
#include "cairo-surface-offset-private.h"

/* the low-level interface */

static cairo_int_status_t
acquire (void *abstract_dst)
{
    cairo_xlib_surface_t *dst = abstract_dst;
    return _cairo_xlib_display_acquire (dst->base.device, &dst->display);
}

static cairo_int_status_t
release (void *abstract_dst)
{
    cairo_xlib_surface_t *dst = abstract_dst;

    cairo_device_release (&dst->display->base);
    dst->display = NULL;

    return CAIRO_STATUS_SUCCESS;
}

struct _fill_box {
    Display *dpy;
    Drawable drawable;
    GC gc;
    //cairo_surface_t *dither = NULL;
};

static cairo_bool_t fill_box (cairo_box_t *box, void *closure)
{
    struct _fill_box *data = closure;
    int x = _cairo_fixed_integer_part (box->p1.x);
    int y = _cairo_fixed_integer_part (box->p1.y);
    int width  = _cairo_fixed_integer_part (box->p2.x - box->p1.x);
    int height = _cairo_fixed_integer_part (box->p2.y - box->p1.y);

    XFillRectangle (data->dpy, data->drawable, data->gc, x, y, width, height);
    return TRUE;
}

static void
_characterize_field (uint32_t mask, int *width, int *shift)
{
    *width = _cairo_popcount (mask);
    /* The final '& 31' is to force a 0 mask to result in 0 shift. */
    *shift = _cairo_popcount ((mask - 1) & ~mask) & 31;
}

static uint32_t
color_to_pixel (cairo_xlib_surface_t    *dst,
		const cairo_color_t     *color)
{
    uint32_t rgba = 0;
    int width, shift;

    _characterize_field (dst->a_mask, &width, &shift);
    rgba |= color->alpha_short >> (16 - width) << shift;

    _characterize_field (dst->r_mask, &width, &shift);
    rgba |= color->red_short >> (16 - width) << shift;

    _characterize_field (dst->g_mask, &width, &shift);
    rgba |= color->green_short >> (16 - width) << shift;

    _characterize_field (dst->b_mask, &width, &shift);
    rgba |= color->blue_short >> (16 - width) << shift;

    return rgba;
}

static cairo_int_status_t
_fill_box_init (struct _fill_box *fb,
		cairo_xlib_surface_t *dst,
		const cairo_color_t *color)
{
    cairo_int_status_t status;

    status = _cairo_xlib_surface_get_gc (dst->display, dst, &fb->gc);
    if (unlikely (status))
        return status;

    fb->dpy = dst->display->display;
    fb->drawable = dst->drawable;

    if (dst->visual && dst->visual->class != TrueColor && 0) {
#if 0
	cairo_solid_pattern_t solid;
	cairo_surface_attributes_t attrs;

	_cairo_pattern_init_solid (&solid, color);
	status = _cairo_pattern_acquire_surface (&solid.base, &dst->base,
						 0, 0,
						 ARRAY_LENGTH (dither_pattern[0]),
						 ARRAY_LENGTH (dither_pattern),
						 CAIRO_PATTERN_ACQUIRE_NONE,
						 &dither,
						 &attrs);
	if (unlikely (status)) {
	    _cairo_xlib_surface_put_gc (dst->display, dst, fb.gc);
	    return status;
	}

	XSetTSOrigin (fb->dpy, fb->gc,
		      - (dst->base.device_transform.x0 + attrs.x_offset),
		      - (dst->base.device_transform.y0 + attrs.y_offset));
	XSetTile (fb->dpy, fb->gc, ((cairo_xlib_surface_t *) dither)->drawable);
#endif
    } else {
	XGCValues gcv;

	gcv.foreground = color_to_pixel (dst, color);
	gcv.fill_style = FillSolid;

	XChangeGC (fb->dpy, fb->gc, GCFillStyle | GCForeground, &gcv);
    }

    return CAIRO_INT_STATUS_SUCCESS;
}

static void
_fill_box_fini (struct _fill_box *fb,
		cairo_xlib_surface_t *dst)
{
    _cairo_xlib_surface_put_gc (dst->display, dst, fb->gc);
    //cairo_surface_destroy (fb->dither);
}

cairo_int_status_t
_cairo_xlib_core_fill_boxes (cairo_xlib_surface_t    *dst,
			     const cairo_color_t     *color,
			     cairo_boxes_t	    *boxes)
{
    cairo_int_status_t status;
    struct _fill_box fb;

    status = _fill_box_init (&fb, dst, color);
    if (unlikely (status))
        return status;

    _cairo_boxes_for_each_box (boxes, fill_box, &fb);

    _fill_box_fini (&fb, dst);
    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_xlib_core_fill_rectangles (cairo_xlib_surface_t    *dst,
				  const cairo_color_t     *color,
				  int num_rects,
				  cairo_rectangle_int_t *rects)
{
    cairo_int_status_t status;
    struct _fill_box fb;
    int i;

    status = _fill_box_init (&fb, dst, color);
    if (unlikely (status))
        return status;

    for (i = 0; i < num_rects; i++)
	XFillRectangle (fb.dpy, fb.drawable, fb.gc,
			rects[i].x, rects[i].y,
			rects[i].width, rects[i].height);

    _fill_box_fini (&fb, dst);
    return CAIRO_STATUS_SUCCESS;
}

struct _fallback_box {
    cairo_xlib_surface_t	*dst;
    cairo_format_t		 format;
    const cairo_pattern_t	*pattern;
};

static cairo_bool_t fallback_box (cairo_box_t *box, void *closure)
{
    struct _fallback_box *data = closure;
    int x = _cairo_fixed_integer_part (box->p1.x);
    int y = _cairo_fixed_integer_part (box->p1.y);
    int width  = _cairo_fixed_integer_part (box->p2.x - box->p1.x);
    int height = _cairo_fixed_integer_part (box->p2.y - box->p1.y);
    cairo_surface_t *image;
    cairo_status_t status;

    /* XXX for EXTEND_NONE and if the box is wholly outside we can just fill */

    image = cairo_surface_create_similar_image (&data->dst->base, data->format,
						width, height);
    status = _cairo_surface_offset_paint (image, x, y,
					  CAIRO_OPERATOR_SOURCE,
					  data->pattern, NULL);
    if (status == CAIRO_STATUS_SUCCESS) {
	status = _cairo_xlib_surface_draw_image (data->dst,
						 (cairo_image_surface_t *)image,
						 0, 0,
						 width, height,
						 x, y);
    }
    cairo_surface_destroy (image);

    return status == CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
fallback_boxes (cairo_xlib_surface_t	*dst,
		const cairo_pattern_t	*pattern,
		cairo_boxes_t		*boxes)
{
    struct _fallback_box fb;

    /* XXX create_similar_image using pixman_format? */
    switch (dst->depth) {
    case 8: fb.format = CAIRO_FORMAT_A8; break;
    case 16: fb.format = CAIRO_FORMAT_RGB16_565; break;
    case 24: fb.format = CAIRO_FORMAT_RGB24; break;
    case 30: fb.format = CAIRO_FORMAT_RGB30; break;
    case 32: fb.format = CAIRO_FORMAT_ARGB32; break;
    default: return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    fb.dst = dst;
    fb.pattern = pattern;

    if (! _cairo_boxes_for_each_box (boxes, fallback_box, &fb))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
render_boxes (cairo_xlib_surface_t	*dst,
	      const cairo_pattern_t	*pattern,
	      cairo_boxes_t		*boxes)
{
    if (pattern->filter != CAIRO_FILTER_NEAREST)
	return fallback_boxes (dst, pattern, boxes);

    switch (pattern->extend) {
    default:
    case CAIRO_EXTEND_NONE:
    case CAIRO_EXTEND_REFLECT:
    case CAIRO_EXTEND_PAD:
	return fallback_boxes (dst, pattern, boxes);

    case CAIRO_EXTEND_REPEAT: /* XXX Use tiling */
	return fallback_boxes (dst, pattern, boxes);
    }
}

/* the mid-level: converts boxes into drawing operations */

struct _box_data {
    Display *dpy;
    cairo_xlib_surface_t *dst;
    cairo_surface_t *src;
    GC gc;
    int tx, ty;
    int width, height;
};

static cairo_bool_t source_contains_box (cairo_box_t *box, void *closure)
{
    struct _box_data *data = closure;

    /* The box is pixel-aligned so the truncation is safe. */
    return
	_cairo_fixed_integer_part (box->p1.x) + data->tx >= 0 &&
	_cairo_fixed_integer_part (box->p1.y) + data->ty >= 0 &&
	_cairo_fixed_integer_part (box->p2.x) + data->tx <= data->width &&
	_cairo_fixed_integer_part (box->p2.y) + data->ty <= data->height;
}

static cairo_bool_t image_upload_box (cairo_box_t *box, void *closure)
{
    const struct _box_data *iub = closure;
    int x = _cairo_fixed_integer_part (box->p1.x);
    int y = _cairo_fixed_integer_part (box->p1.y);
    int width  = _cairo_fixed_integer_part (box->p2.x - box->p1.x);
    int height = _cairo_fixed_integer_part (box->p2.y - box->p1.y);

    return _cairo_xlib_surface_draw_image (iub->dst,
					   (cairo_image_surface_t *)iub->src,
					   x + iub->tx, y + iub->ty,
					   width, height,
					   x, y) == CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
surface_matches_image_format (cairo_xlib_surface_t *surface,
			      cairo_image_surface_t *image)
{
    cairo_format_masks_t format;

    return (_pixman_format_to_masks (image->pixman_format, &format) &&
	    (format.alpha_mask == surface->a_mask || surface->a_mask == 0) &&
	    (format.red_mask   == surface->r_mask || surface->r_mask == 0) &&
	    (format.green_mask == surface->g_mask || surface->g_mask == 0) &&
	    (format.blue_mask  == surface->b_mask || surface->b_mask == 0));
}

static cairo_status_t
upload_image_inplace (cairo_xlib_surface_t *dst,
		      const cairo_pattern_t *source,
		      cairo_boxes_t *boxes)
{
    const cairo_surface_pattern_t *pattern;
    struct _box_data iub;
    cairo_image_surface_t *image;

    if (source->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    pattern = (const cairo_surface_pattern_t *) source;
    if (pattern->surface->type != CAIRO_SURFACE_TYPE_IMAGE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    image = (cairo_image_surface_t *) pattern->surface;
    if (image->format == CAIRO_FORMAT_INVALID)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (image->depth != dst->depth)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! surface_matches_image_format (dst, image))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX subsurface */

    if (! _cairo_matrix_is_integer_translation (&source->matrix,
						&iub.tx, &iub.ty))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    iub.dst = dst;
    iub.src = &image->base;
    iub.width  = image->width;
    iub.height = image->height;

    /* First check that the data is entirely within the image */
    if (! _cairo_boxes_for_each_box (boxes, source_contains_box, &iub))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_boxes_for_each_box (boxes, image_upload_box, &iub))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t copy_box (cairo_box_t *box, void *closure)
{
    const struct _box_data *cb = closure;
    int x = _cairo_fixed_integer_part (box->p1.x);
    int y = _cairo_fixed_integer_part (box->p1.y);
    int width  = _cairo_fixed_integer_part (box->p2.x - box->p1.x);
    int height = _cairo_fixed_integer_part (box->p2.y - box->p1.y);

    XCopyArea (cb->dpy,
	       ((cairo_xlib_surface_t *)cb->src)->drawable,
	       cb->dst->drawable,
	       cb->gc,
	       x + cb->tx, y + cb->ty,
	       width, height,
	       x, y);
    return TRUE;
}

static cairo_status_t
copy_boxes (cairo_xlib_surface_t *dst,
	    const cairo_pattern_t *source,
	    cairo_boxes_t *boxes)
{
    const cairo_surface_pattern_t *pattern;
    struct _box_data cb;
    cairo_xlib_surface_t *src;
    cairo_status_t status;

    if (source->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX subsurface */

    pattern = (const cairo_surface_pattern_t *) source;
    if (pattern->surface->backend->type != CAIRO_SURFACE_TYPE_XLIB)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    src = (cairo_xlib_surface_t *) pattern->surface;
    if (src->depth != dst->depth)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* We can only have a single control for subwindow_mode on the
     * GC. If we have a Window destination, we need to set ClipByChildren,
     * but if we have a Window source, we need IncludeInferiors. If we have
     * both a Window destination and source, we must fallback. There is
     * no convenient way to detect if a drawable is a Pixmap or Window,
     * therefore we can only rely on those surfaces that we created
     * ourselves to be Pixmaps, and treat everything else as a potential
     * Window.
     */
    if (! src->owns_pixmap && ! dst->owns_pixmap)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_xlib_surface_same_screen (dst, src))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_matrix_is_integer_translation (&source->matrix,
						&cb.tx, &cb.ty))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    cb.dpy = dst->display->display;
    cb.dst = dst;
    cb.src = &src->base;
    cb.width  = src->width;
    cb.height = src->height;

    /* First check that the data is entirely within the image */
    if (! _cairo_boxes_for_each_box (boxes, source_contains_box, &cb))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_xlib_surface_get_gc (dst->display, dst, &cb.gc);
    if (unlikely (status))
	return status;

    if (! src->owns_pixmap) {
	XGCValues gcv;

	gcv.subwindow_mode = IncludeInferiors;
	XChangeGC (dst->display->display, cb.gc, GCSubwindowMode, &gcv);
    }

    status = CAIRO_STATUS_SUCCESS;
    if (! _cairo_boxes_for_each_box (boxes, copy_box, &cb))
	status = CAIRO_INT_STATUS_UNSUPPORTED;

    if (! src->owns_pixmap) {
	XGCValues gcv;

	gcv.subwindow_mode = ClipByChildren;
	XChangeGC (dst->display->display, cb.gc, GCSubwindowMode, &gcv);
    }

    _cairo_xlib_surface_put_gc (dst->display, dst, cb.gc);

    return status;
}

static cairo_status_t
draw_boxes (cairo_composite_rectangles_t *extents,
	    cairo_boxes_t *boxes)
{
    cairo_xlib_surface_t *dst = (cairo_xlib_surface_t *)extents->surface;
    cairo_operator_t op = extents->op;
    const cairo_pattern_t *src = &extents->source_pattern.base;
    cairo_int_status_t status;

    if (boxes->num_boxes == 0 && extents->is_bounded)
	return CAIRO_STATUS_SUCCESS;

    if (! boxes->is_pixel_aligned)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (op == CAIRO_OPERATOR_CLEAR)
	op = CAIRO_OPERATOR_SOURCE;

    if (op == CAIRO_OPERATOR_OVER &&
	_cairo_pattern_is_opaque (src, &extents->bounded))
	op = CAIRO_OPERATOR_SOURCE;

    if (dst->base.is_clear && op == CAIRO_OPERATOR_OVER)
	op = CAIRO_OPERATOR_SOURCE;

    if (op != CAIRO_OPERATOR_SOURCE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = acquire (dst);
    if (unlikely (status))
	return status;

    if (src->type == CAIRO_PATTERN_TYPE_SOLID) {
	status = _cairo_xlib_core_fill_boxes
	    (dst, &((cairo_solid_pattern_t *) src)->color, boxes);
    } else {
	status = upload_image_inplace (dst, src, boxes);
	if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	    status = copy_boxes (dst, src, boxes);
	if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	    status = render_boxes (dst, src, boxes);
    }

    release (dst);

    return status;
}

/* high-level compositor interface */

static cairo_int_status_t
_cairo_xlib_core_compositor_paint (const cairo_compositor_t	*compositor,
				   cairo_composite_rectangles_t *extents)
{
    cairo_int_status_t status;

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (_cairo_clip_is_region (extents->clip)) {
	cairo_boxes_t boxes;

	 _cairo_clip_steal_boxes (extents->clip, &boxes);
	 status = draw_boxes (extents, &boxes);
	 _cairo_clip_unsteal_boxes (extents->clip, &boxes);
    }

    return status;
}

static cairo_int_status_t
_cairo_xlib_core_compositor_stroke (const cairo_compositor_t	*compositor,
				    cairo_composite_rectangles_t *extents,
				    const cairo_path_fixed_t	*path,
				    const cairo_stroke_style_t	*style,
				    const cairo_matrix_t	*ctm,
				    const cairo_matrix_t	*ctm_inverse,
				    double			 tolerance,
				    cairo_antialias_t		 antialias)
{
    cairo_int_status_t status;

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (extents->clip->path == NULL &&
	_cairo_path_fixed_stroke_is_rectilinear (path)) {
	cairo_boxes_t boxes;

	_cairo_boxes_init_with_clip (&boxes, extents->clip);
	status = _cairo_path_fixed_stroke_rectilinear_to_boxes (path,
								style,
								ctm,
								antialias,
								&boxes);
	if (likely (status == CAIRO_INT_STATUS_SUCCESS))
	    status = draw_boxes (extents, &boxes);
	_cairo_boxes_fini (&boxes);
    }

    return status;
}

static cairo_int_status_t
_cairo_xlib_core_compositor_fill (const cairo_compositor_t	*compositor,
				  cairo_composite_rectangles_t	*extents,
				  const cairo_path_fixed_t	*path,
				  cairo_fill_rule_t		 fill_rule,
				  double			 tolerance,
				  cairo_antialias_t		 antialias)
{
    cairo_int_status_t status;

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (extents->clip->path == NULL &&
	_cairo_path_fixed_fill_is_rectilinear (path)) {
	cairo_boxes_t boxes;

	_cairo_boxes_init_with_clip (&boxes, extents->clip);
	status = _cairo_path_fixed_fill_rectilinear_to_boxes (path,
							      fill_rule,
							      antialias,
							      &boxes);
	if (likely (status == CAIRO_INT_STATUS_SUCCESS))
	    status = draw_boxes (extents, &boxes);
	_cairo_boxes_fini (&boxes);
    }

    return status;
}

const cairo_compositor_t *
_cairo_xlib_core_compositor_get (void)
{
    static cairo_atomic_once_t once = CAIRO_ATOMIC_ONCE_INIT;
    static cairo_compositor_t compositor;

    if (_cairo_atomic_init_once_enter(&once)) {
	compositor.delegate = _cairo_xlib_fallback_compositor_get ();

	compositor.paint = _cairo_xlib_core_compositor_paint;
	compositor.mask  = NULL;
	compositor.fill  = _cairo_xlib_core_compositor_fill;
	compositor.stroke = _cairo_xlib_core_compositor_stroke;
	compositor.glyphs = NULL; /* XXX PolyGlyph? */

	_cairo_atomic_init_once_leave(&once);
    }

    return &compositor;
}

#endif /* !CAIRO_HAS_XLIB_XCB_FUNCTIONS */
