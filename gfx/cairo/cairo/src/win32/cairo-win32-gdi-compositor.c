/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2012 Intel Corporation
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

#include "cairo-win32-private.h"

#include "cairo-boxes-private.h"
#include "cairo-clip-inline.h"
#include "cairo-compositor-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-pattern-private.h"
#include "cairo-region-private.h"
#include "cairo-surface-inline.h"
#include "cairo-surface-offset-private.h"

#if !defined(AC_SRC_OVER)
#define AC_SRC_OVER                 0x00
#pragma pack(1)
typedef struct {
    BYTE   BlendOp;
    BYTE   BlendFlags;
    BYTE   SourceConstantAlpha;
    BYTE   AlphaFormat;
}BLENDFUNCTION;
#pragma pack()
#endif

/* for compatibility with VC++ 6 */
#ifndef AC_SRC_ALPHA
#define AC_SRC_ALPHA                0x01
#endif

#define PELS_72DPI  ((LONG)(72. / 0.0254))

/* the low-level interface */

struct fill_box {
    HDC dc;
    HBRUSH brush;
};

static cairo_bool_t fill_box (cairo_box_t *box, void *closure)
{
    struct fill_box *fb = closure;
    RECT rect;

    rect.left = _cairo_fixed_integer_part (box->p1.x);
    rect.top = _cairo_fixed_integer_part (box->p1.y);
    rect.right = _cairo_fixed_integer_part (box->p2.x);
    rect.bottom = _cairo_fixed_integer_part (box->p2.y);

    TRACE ((stderr, "%s\n", __FUNCTION__));
    return FillRect (fb->dc, &rect, fb->brush);
}

struct check_box {
    cairo_rectangle_int_t limit;
    int tx, ty;
};

struct copy_box {
    cairo_rectangle_int_t limit;
    int tx, ty;
    HDC dst, src;
    BLENDFUNCTION bf;
    cairo_win32_alpha_blend_func_t alpha_blend;
};

static cairo_bool_t copy_box (cairo_box_t *box, void *closure)
{
    const struct copy_box *cb = closure;
    int x = _cairo_fixed_integer_part (box->p1.x);
    int y = _cairo_fixed_integer_part (box->p1.y);
    int width  = _cairo_fixed_integer_part (box->p2.x - box->p1.x);
    int height = _cairo_fixed_integer_part (box->p2.y - box->p1.y);

    TRACE ((stderr, "%s\n", __FUNCTION__));
    return BitBlt (cb->dst, x, y, width, height,
		   cb->src, x + cb->tx, y + cb->ty,
		   SRCCOPY);
}

static cairo_bool_t alpha_box (cairo_box_t *box, void *closure)
{
    const struct copy_box *cb = closure;
    int x = _cairo_fixed_integer_part (box->p1.x);
    int y = _cairo_fixed_integer_part (box->p1.y);
    int width  = _cairo_fixed_integer_part (box->p2.x - box->p1.x);
    int height = _cairo_fixed_integer_part (box->p2.y - box->p1.y);

    TRACE ((stderr, "%s\n", __FUNCTION__));
    return cb->alpha_blend (cb->dst, x, y, width, height,
			    cb->src, x + cb->tx, y + cb->ty, width, height,
			    cb->bf);
}

struct upload_box {
    cairo_rectangle_int_t limit;
    int tx, ty;
    HDC dst;
    BITMAPINFO bi;
    void *data;
};

static cairo_bool_t upload_box (cairo_box_t *box, void *closure)
{
    const struct upload_box *cb = closure;
    int x = _cairo_fixed_integer_part (box->p1.x);
    int y = _cairo_fixed_integer_part (box->p1.y);
    int width  = _cairo_fixed_integer_part (box->p2.x - box->p1.x);
    int height = _cairo_fixed_integer_part (box->p2.y - box->p1.y);
    int src_height = -cb->bi.bmiHeader.biHeight;

    TRACE ((stderr, "%s\n", __FUNCTION__));
    return StretchDIBits (cb->dst, x, y + height - 1, width, -height,
			  x + cb->tx,  src_height - (y + cb->ty - 1),
			  width, -height,
			  cb->data, &cb->bi,
			  DIB_RGB_COLORS, SRCCOPY);
}

/* the mid-level: converts boxes into drawing operations */

static COLORREF color_to_rgb(const cairo_color_t *c)
{
    return RGB (c->red_short >> 8, c->green_short >> 8, c->blue_short >> 8);
}

static cairo_int_status_t
fill_boxes (cairo_win32_display_surface_t	*dst,
	    const cairo_pattern_t		*src,
	    cairo_boxes_t			*boxes)
{
    const cairo_color_t *color = &((cairo_solid_pattern_t *) src)->color;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    struct fill_box fb;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    if ((dst->win32.flags & CAIRO_WIN32_SURFACE_CAN_RGB_BRUSH) == 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    fb.dc = dst->win32.dc;
    fb.brush = CreateSolidBrush (color_to_rgb(color));
    if (!fb.brush)
	return _cairo_win32_print_gdi_error (__FUNCTION__);

    if (! _cairo_boxes_for_each_box (boxes, fill_box, &fb))
	status = CAIRO_INT_STATUS_UNSUPPORTED;

    DeleteObject (fb.brush);

    return status;
}

static cairo_bool_t source_contains_box (cairo_box_t *box, void *closure)
{
    struct check_box *data = closure;

    /* The box is pixel-aligned so the truncation is safe. */
    return
	_cairo_fixed_integer_part (box->p1.x) + data->tx >= data->limit.x &&
	_cairo_fixed_integer_part (box->p1.y) + data->ty >= data->limit.y &&
	_cairo_fixed_integer_part (box->p2.x) + data->tx <= data->limit.x + data->limit.width &&
	_cairo_fixed_integer_part (box->p2.y) + data->ty <= data->limit.y + data->limit.height;
}

static cairo_status_t
copy_boxes (cairo_win32_display_surface_t *dst,
	    const cairo_pattern_t *source,
	    cairo_boxes_t *boxes)
{
    const cairo_surface_pattern_t *pattern;
    struct copy_box cb;
    cairo_surface_t *surface;
    cairo_status_t status;
    cairo_win32_surface_t *src;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    pattern = (const cairo_surface_pattern_t *) source;
    surface = _cairo_surface_get_source (pattern->surface, &cb.limit);
    if (surface->type == CAIRO_SURFACE_TYPE_IMAGE) {
	surface = to_image_surface(surface)->parent;
	if (surface == NULL)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    if (surface->type != CAIRO_SURFACE_TYPE_WIN32)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_matrix_is_integer_translation (&source->matrix,
						&cb.tx, &cb.ty))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    src = to_win32_surface(surface);

    if (src->format != dst->win32.format &&
	!(src->format == CAIRO_FORMAT_ARGB32 && dst->win32.format == CAIRO_FORMAT_RGB24))
    {
	/* forbid copy different surfaces unless it is from argb32 to
	 * rgb (dropping alpha) */
        return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    cb.dst = dst->win32.dc;
    cb.src = src->dc;

    /* First check that the data is entirely within the image */
    if (! _cairo_boxes_for_each_box (boxes, source_contains_box, &cb))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = __cairo_surface_flush (surface, 0);
    if (status)
	return status;

    cb.tx += cb.limit.x;
    cb.ty += cb.limit.y;
    status = CAIRO_STATUS_SUCCESS;
    if (! _cairo_boxes_for_each_box (boxes, copy_box, &cb))
	status = CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_win32_display_surface_discard_fallback (dst);
    return status;
}

static cairo_status_t
upload_boxes (cairo_win32_display_surface_t *dst,
	      const cairo_pattern_t *source,
	      cairo_boxes_t *boxes)
{
    const cairo_surface_pattern_t *pattern;
    struct upload_box cb;
    cairo_surface_t *surface;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    if ((dst->win32.flags & CAIRO_WIN32_SURFACE_CAN_STRETCHDIB) == 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_matrix_is_integer_translation (&source->matrix,
						&cb.tx, &cb.ty))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    pattern = (const cairo_surface_pattern_t *) source;
    surface = _cairo_surface_get_source (pattern->surface, &cb.limit);

    /* First check that the data is entirely within the image */
    if (! _cairo_boxes_for_each_box (boxes, source_contains_box, &cb))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (surface->type != CAIRO_SURFACE_TYPE_IMAGE) {
	status = _cairo_surface_acquire_source_image (surface,
						      &image, &image_extra);
	if (status)
	    return status;
    } else
	image = to_image_surface(surface);

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (!(image->format == CAIRO_FORMAT_ARGB32 ||
	  image->format == CAIRO_FORMAT_RGB24))
	goto err;
    if (image->stride != 4*image->width)
	goto err;

    cb.dst = dst->win32.dc;
    cb.data = image->data;

    cb.bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    cb.bi.bmiHeader.biWidth = image->width;
    cb.bi.bmiHeader.biHeight = -image->height;
    cb.bi.bmiHeader.biSizeImage = 0;
    cb.bi.bmiHeader.biXPelsPerMeter = PELS_72DPI;
    cb.bi.bmiHeader.biYPelsPerMeter = PELS_72DPI;
    cb.bi.bmiHeader.biPlanes = 1;
    cb.bi.bmiHeader.biBitCount = 32;
    cb.bi.bmiHeader.biCompression = BI_RGB;
    cb.bi.bmiHeader.biClrUsed = 0;
    cb.bi.bmiHeader.biClrImportant = 0;

    cb.tx += cb.limit.x;
    cb.ty += cb.limit.y;
    status = CAIRO_STATUS_SUCCESS;
    if (! _cairo_boxes_for_each_box (boxes, upload_box, &cb))
	status = CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_win32_display_surface_discard_fallback (dst);
err:
    if (&image->base != surface)
	_cairo_surface_release_source_image (surface, image, image_extra);

    return status;
}

static cairo_status_t
alpha_blend_boxes (cairo_win32_display_surface_t *dst,
		   const cairo_pattern_t *source,
		   cairo_boxes_t *boxes,
		   uint8_t alpha)
{
    const cairo_surface_pattern_t *pattern;
    struct copy_box cb;
    cairo_surface_t *surface;
    cairo_win32_display_surface_t *src;
    cairo_status_t status;

    TRACE ((stderr, "%s\n", __FUNCTION__));
    if (source->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    pattern = (const cairo_surface_pattern_t *) source;
    surface = _cairo_surface_get_source (pattern->surface, &cb.limit);
    if (surface->type == CAIRO_SURFACE_TYPE_IMAGE) {
	surface = to_image_surface(surface)->parent;
	if (surface == NULL)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    if (surface->type != CAIRO_SURFACE_TYPE_WIN32)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_matrix_is_integer_translation (&source->matrix,
						&cb.tx, &cb.ty))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    src = to_win32_display_surface (surface);
    cb.dst = dst->win32.dc;
    cb.src = src->win32.dc;

    /* First check that the data is entirely within the image */
    if (! _cairo_boxes_for_each_box (boxes, source_contains_box, &cb))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = __cairo_surface_flush (&src->win32.base, 0);
    if (status)
	return status;

    cb.bf.BlendOp = AC_SRC_OVER;
    cb.bf.BlendFlags = 0;
    cb.bf.SourceConstantAlpha = alpha;
    cb.bf.AlphaFormat = (src->win32.format == CAIRO_FORMAT_ARGB32) ? AC_SRC_ALPHA : 0;
    cb.alpha_blend = to_win32_device(dst->win32.base.device)->alpha_blend;

    cb.tx += cb.limit.x;
    cb.ty += cb.limit.y;
    status = CAIRO_STATUS_SUCCESS;
    if (! _cairo_boxes_for_each_box (boxes, alpha_box, &cb))
	status = CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_win32_display_surface_discard_fallback (dst);
    return status;
}

static cairo_bool_t
can_alpha_blend (cairo_win32_display_surface_t *dst)
{
    if ((dst->win32.flags & CAIRO_WIN32_SURFACE_CAN_ALPHABLEND) == 0)
	return FALSE;

    return to_win32_device(dst->win32.base.device)->alpha_blend != NULL;
}

static cairo_status_t
draw_boxes (cairo_composite_rectangles_t *composite,
	    cairo_boxes_t *boxes)
{
    cairo_win32_display_surface_t *dst = to_win32_display_surface(composite->surface);
    cairo_operator_t op = composite->op;
    const cairo_pattern_t *src = &composite->source_pattern.base;
    cairo_int_status_t status;

    TRACE ((stderr, "%s\n", __FUNCTION__));
    if (boxes->num_boxes == 0 && composite->is_bounded)
	return CAIRO_STATUS_SUCCESS;

    if (!boxes->is_pixel_aligned)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (op == CAIRO_OPERATOR_CLEAR)
	op = CAIRO_OPERATOR_SOURCE;

    if (op == CAIRO_OPERATOR_OVER &&
	_cairo_pattern_is_opaque (src, &composite->bounded))
	op = CAIRO_OPERATOR_SOURCE;

    if (dst->win32.base.is_clear &&
	(op == CAIRO_OPERATOR_OVER || op == CAIRO_OPERATOR_ADD))
	op = CAIRO_OPERATOR_SOURCE;

    if (op == CAIRO_OPERATOR_SOURCE) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	if (src->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    status = copy_boxes (dst, src, boxes);
	    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
		status = upload_boxes (dst, src, boxes);
	} else if (src->type == CAIRO_PATTERN_TYPE_SOLID) {
	    status = fill_boxes (dst, src, boxes);
	}
	return status;
    }

    if (op == CAIRO_OPERATOR_OVER && can_alpha_blend (dst))
	return alpha_blend_boxes (dst, src, boxes, 255);

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
opacity_boxes (cairo_composite_rectangles_t *composite,
	       cairo_boxes_t *boxes)
{
    cairo_win32_display_surface_t *dst = to_win32_display_surface(composite->surface);
    cairo_operator_t op = composite->op;
    const cairo_pattern_t *src = &composite->source_pattern.base;

    TRACE ((stderr, "%s\n", __FUNCTION__));
    if (composite->mask_pattern.base.type != CAIRO_PATTERN_TYPE_SOLID)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (boxes->num_boxes == 0 && composite->is_bounded)
	return CAIRO_STATUS_SUCCESS;

    if (!boxes->is_pixel_aligned)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (op != CAIRO_OPERATOR_OVER)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (!can_alpha_blend (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return alpha_blend_boxes (dst, src, boxes,
			      composite->mask_pattern.solid.color.alpha_short >> 8);
}

/* high-level compositor interface */

static cairo_bool_t check_blit (cairo_composite_rectangles_t *composite)
{
    cairo_win32_display_surface_t *dst;

    if (composite->clip->path)
	return FALSE;

    dst = to_win32_display_surface (composite->surface);
    if (dst->fallback)
	return FALSE;

    if (dst->win32.format != CAIRO_FORMAT_RGB24
	&& dst->win32.format != CAIRO_FORMAT_ARGB32)
	return FALSE;

    if (dst->win32.flags & CAIRO_WIN32_SURFACE_CAN_BITBLT)
	return TRUE;

    return dst->image == NULL;
}

static cairo_int_status_t
_cairo_win32_gdi_compositor_paint (const cairo_compositor_t	*compositor,
				   cairo_composite_rectangles_t *composite)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;

    if (check_blit (composite)) {
	cairo_boxes_t boxes;

	TRACE ((stderr, "%s\n", __FUNCTION__));
	_cairo_clip_steal_boxes (composite->clip, &boxes);
	status = draw_boxes (composite, &boxes);
	_cairo_clip_unsteal_boxes (composite->clip, &boxes);
    }

    return status;
}

static cairo_int_status_t
_cairo_win32_gdi_compositor_mask (const cairo_compositor_t	*compositor,
				  cairo_composite_rectangles_t *composite)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;

    if (check_blit (composite)) {
	cairo_boxes_t boxes;

	TRACE ((stderr, "%s\n", __FUNCTION__));
	_cairo_clip_steal_boxes (composite->clip, &boxes);
	status = opacity_boxes (composite, &boxes);
	_cairo_clip_unsteal_boxes (composite->clip, &boxes);
    }

    return status;
}

static cairo_int_status_t
_cairo_win32_gdi_compositor_stroke (const cairo_compositor_t	*compositor,
				    cairo_composite_rectangles_t *composite,
				    const cairo_path_fixed_t	*path,
				    const cairo_stroke_style_t	*style,
				    const cairo_matrix_t	*ctm,
				    const cairo_matrix_t	*ctm_inverse,
				    double			 tolerance,
				    cairo_antialias_t		 antialias)
{
    cairo_int_status_t status;

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (check_blit (composite) &&
	_cairo_path_fixed_stroke_is_rectilinear (path)) {
	cairo_boxes_t boxes;

	TRACE ((stderr, "%s\n", __FUNCTION__));
	_cairo_boxes_init_with_clip (&boxes, composite->clip);
	status = _cairo_path_fixed_stroke_rectilinear_to_boxes (path,
								style,
								ctm,
								antialias,
								&boxes);
	if (likely (status == CAIRO_INT_STATUS_SUCCESS))
	    status = draw_boxes (composite, &boxes);
	_cairo_boxes_fini (&boxes);
    }

    return status;
}

static cairo_int_status_t
_cairo_win32_gdi_compositor_fill (const cairo_compositor_t	*compositor,
				  cairo_composite_rectangles_t	*composite,
				  const cairo_path_fixed_t	*path,
				  cairo_fill_rule_t		 fill_rule,
				  double			 tolerance,
				  cairo_antialias_t		 antialias)
{
    cairo_int_status_t status;

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (check_blit (composite) &&
	_cairo_path_fixed_fill_is_rectilinear (path)) {
	cairo_boxes_t boxes;

	TRACE ((stderr, "%s\n", __FUNCTION__));
	_cairo_boxes_init_with_clip (&boxes, composite->clip);
	status = _cairo_path_fixed_fill_rectilinear_to_boxes (path,
							      fill_rule,
							      antialias,
							      &boxes);
	if (likely (status == CAIRO_INT_STATUS_SUCCESS))
	    status = draw_boxes (composite, &boxes);
	_cairo_boxes_fini (&boxes);
    }

    return status;
}

static cairo_bool_t check_glyphs (cairo_composite_rectangles_t *composite,
				  cairo_scaled_font_t *scaled_font)
{
    if (! _cairo_clip_is_region (composite->clip))
	return FALSE;

    cairo_font_type_t type = cairo_scaled_font_get_type (scaled_font);
    if (type != CAIRO_FONT_TYPE_WIN32 && type != CAIRO_FONT_TYPE_DWRITE)
	return FALSE;

    if (! _cairo_pattern_is_opaque_solid (&composite->source_pattern.base))
	return FALSE;

    return (composite->op == CAIRO_OPERATOR_CLEAR ||
	    composite->op == CAIRO_OPERATOR_SOURCE ||
	    composite->op == CAIRO_OPERATOR_OVER);
}

static cairo_int_status_t
_cairo_win32_gdi_compositor_glyphs (const cairo_compositor_t	*compositor,
				    cairo_composite_rectangles_t*composite,
				    cairo_scaled_font_t		*scaled_font,
				    cairo_glyph_t		*glyphs,
				    int				 num_glyphs,
				    cairo_bool_t		 overlap)
{
    cairo_int_status_t status;

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (check_blit (composite) && check_glyphs (composite, scaled_font)) {
	cairo_win32_display_surface_t *dst = to_win32_display_surface (composite->surface);

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if ((dst->win32.flags & CAIRO_WIN32_SURFACE_CAN_RGB_BRUSH) == 0)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	status = _cairo_win32_display_surface_set_clip(dst, composite->clip);
	if (status)
	    return status;

	status = _cairo_win32_surface_emit_glyphs (&dst->win32,
						   &composite->source_pattern.base,
						   glyphs,
						   num_glyphs,
						   scaled_font,
						   TRUE);

	_cairo_win32_display_surface_unset_clip (dst);
    }

    return status;
}

const cairo_compositor_t *
_cairo_win32_gdi_compositor_get (void)
{
    static cairo_atomic_once_t once = CAIRO_ATOMIC_ONCE_INIT;
    static cairo_compositor_t compositor;

    if (_cairo_atomic_init_once_enter(&once)) {
	compositor.delegate = &_cairo_fallback_compositor;

	compositor.paint  = _cairo_win32_gdi_compositor_paint;
	compositor.mask   = _cairo_win32_gdi_compositor_mask;
	compositor.fill   = _cairo_win32_gdi_compositor_fill;
	compositor.stroke = _cairo_win32_gdi_compositor_stroke;
	compositor.glyphs = _cairo_win32_gdi_compositor_glyphs;

	_cairo_atomic_init_once_leave(&once);
    }

    return &compositor;
}
