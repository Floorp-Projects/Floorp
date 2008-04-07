/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2007 Adrian Johnson
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
 * The Initial Developer of the Original Code is Adrian Johnson.
 *
 * Contributor(s):
 *      Adrian Johnson <ajohnson@redneon.com>
 *      Vladimir Vukicevic <vladimir@pobox.com>
 */

#define WIN32_LEAN_AND_MEAN
/* We require Windows 2000 features such as ETO_PDY */
#if !defined(WINVER) || (WINVER < 0x0500)
# define WINVER 0x0500
#endif
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0500)
# define _WIN32_WINNT 0x0500
#endif

#include "cairoint.h"

#include "cairo-paginated-private.h"

#include "cairo-clip-private.h"
#include "cairo-win32-private.h"
#include "cairo-meta-surface-private.h"

#include <windows.h>

#if !defined(POSTSCRIPT_IDENTIFY)
# define POSTSCRIPT_IDENTIFY 0x1015
#endif

#if !defined(PSIDENT_GDICENTRIC)
# define PSIDENT_GDICENTRIC 0x0000
#endif

#if !defined(GET_PS_FEATURESETTING)
# define GET_PS_FEATURESETTING 0x1019
#endif

#if !defined(FEATURESETTING_PSLEVEL)
# define FEATURESETTING_PSLEVEL 0x0002
#endif

#if !defined(GRADIENT_FILL_RECT_H)
# define GRADIENT_FILL_RECT_H 0x00
#endif

#define PELS_72DPI  ((LONG)(72. / 0.0254))

static const cairo_surface_backend_t cairo_win32_printing_surface_backend;
static const cairo_paginated_surface_backend_t cairo_win32_surface_paginated_backend;

static void
_cairo_win32_printing_surface_init_ps_mode (cairo_win32_surface_t *surface)
{
    DWORD word;
    INT ps_feature, ps_level;

    word = PSIDENT_GDICENTRIC;
    if (ExtEscape (surface->dc, POSTSCRIPT_IDENTIFY, sizeof(DWORD), (char *)&word, 0, (char *)NULL) <= 0)
	return;

    ps_feature = FEATURESETTING_PSLEVEL;
    if (ExtEscape (surface->dc, GET_PS_FEATURESETTING, sizeof(INT),
		   (char *)&ps_feature, sizeof(INT), (char *)&ps_level) <= 0)
	return;

    if (ps_level >= 3)
	surface->flags |= CAIRO_WIN32_SURFACE_CAN_RECT_GRADIENT;
}

static cairo_int_status_t
analyze_surface_pattern_transparency (cairo_surface_pattern_t *pattern)
{
    cairo_image_surface_t  *image;
    void		   *image_extra;
    cairo_int_status_t      status;
    int x, y;

    status = _cairo_surface_acquire_source_image (pattern->surface,
						  &image,
						  &image_extra);
    if (status)
	return status;

    if (image->base.status)
	return image->base.status;

    if (image->format == CAIRO_FORMAT_RGB24) {
	status = CAIRO_STATUS_SUCCESS;
	goto RELEASE_SOURCE;
    }

    if (image->format != CAIRO_FORMAT_ARGB32) {
	/* If the surface does not support the image format, assume
	 * that it does have alpha. The image will be converted to
	 * rgb24 when the surface blends the image into the page
	 * color to remove the transparency. */
	status = CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY;
	goto RELEASE_SOURCE;
    }

    for (y = 0; y < image->height; y++) {
	int a;
	uint32_t *pixel = (uint32_t *) (image->data + y * image->stride);

	for (x = 0; x < image->width; x++, pixel++) {
	    a = (*pixel & 0xff000000) >> 24;
	    if (a != 255) {
		status = CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY;
		goto RELEASE_SOURCE;
	    }
	}
    }
    status = CAIRO_STATUS_SUCCESS;

RELEASE_SOURCE:
    _cairo_surface_release_source_image (pattern->surface, image, image_extra);

    return status;
}

static cairo_bool_t
surface_pattern_supported (const cairo_surface_pattern_t *pattern)
{
    cairo_extend_t extend;

    if (_cairo_surface_is_meta (pattern->surface))
	return TRUE;

    if (cairo_surface_get_type (pattern->surface) != CAIRO_SURFACE_TYPE_WIN32 &&
	pattern->surface->backend->acquire_source_image == NULL)
    {
	return FALSE;
    }

    extend = cairo_pattern_get_extend ((cairo_pattern_t*)&pattern->base);
    switch (extend) {
    case CAIRO_EXTEND_NONE:
    case CAIRO_EXTEND_REPEAT:
    case CAIRO_EXTEND_REFLECT:
    /* There's no point returning FALSE for EXTEND_PAD, as the image
     * surface does not currently implement it either */
    case CAIRO_EXTEND_PAD:
	return TRUE;
    }

    ASSERT_NOT_REACHED;
    return FALSE;
}

static cairo_bool_t
pattern_supported (cairo_win32_surface_t *surface, const cairo_pattern_t *pattern)
{
    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID)
	return TRUE;

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE)
	return surface_pattern_supported ((const cairo_surface_pattern_t *) pattern);

    if (pattern->type == CAIRO_PATTERN_TYPE_LINEAR)
	return surface->flags & CAIRO_WIN32_SURFACE_CAN_RECT_GRADIENT;

    return FALSE;
}

static cairo_int_status_t
_cairo_win32_printing_surface_analyze_operation (cairo_win32_surface_t *surface,
                                                 cairo_operator_t       op,
                                                 const cairo_pattern_t *pattern)
{
    if (! pattern_supported (surface, pattern))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (!(op == CAIRO_OPERATOR_SOURCE ||
	  op == CAIRO_OPERATOR_OVER ||
	  op == CAIRO_OPERATOR_CLEAR))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) pattern;

	if ( _cairo_surface_is_meta (surface_pattern->surface))
	    return CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN;
    }

    if (op == CAIRO_OPERATOR_SOURCE ||
	op == CAIRO_OPERATOR_CLEAR)
	return CAIRO_STATUS_SUCCESS;

    /* CAIRO_OPERATOR_OVER is only supported for opaque patterns. If
     * the pattern contains transparency, we return
     * CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY to the analysis
     * surface. If the analysis surface determines that there is
     * anything drawn under this operation, a fallback image will be
     * used. Otherwise the operation will be replayed during the
     * render stage and we blend the transarency into the white
     * background to convert the pattern to opaque.
     */

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) pattern;

	return analyze_surface_pattern_transparency (surface_pattern);
    }

    if (_cairo_pattern_is_opaque (pattern))
	return CAIRO_STATUS_SUCCESS;
    else
	return CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY;
}

static cairo_bool_t
_cairo_win32_printing_surface_operation_supported (cairo_win32_surface_t *surface,
                                                   cairo_operator_t       op,
                                                   const cairo_pattern_t *pattern)
{
    if (_cairo_win32_printing_surface_analyze_operation (surface, op, pattern) != CAIRO_INT_STATUS_UNSUPPORTED)
	return TRUE;
    else
	return FALSE;
}

static void
_cairo_win32_printing_surface_init_clear_color (cairo_win32_surface_t *surface,
						cairo_solid_pattern_t *color)
{
    if (surface->content == CAIRO_CONTENT_COLOR_ALPHA)
	_cairo_pattern_init_solid (color, CAIRO_COLOR_WHITE, CAIRO_CONTENT_COLOR);
    else
	_cairo_pattern_init_solid (color, CAIRO_COLOR_BLACK, CAIRO_CONTENT_COLOR);
}

static COLORREF
_cairo_win32_printing_surface_flatten_transparency (cairo_win32_surface_t *surface,
						    const cairo_color_t   *color)
{
    COLORREF c;
    BYTE red, green, blue;

    red   = color->red_short   >> 8;
    green = color->green_short >> 8;
    blue  = color->blue_short  >> 8;

    if (!CAIRO_COLOR_IS_OPAQUE(color)) {
	if (surface->content == CAIRO_CONTENT_COLOR_ALPHA) {
	    /* Blend into white */
	    uint8_t one_minus_alpha = 255 - (color->alpha_short >> 8);

	    red   = (color->red_short   >> 8) + one_minus_alpha;
	    green = (color->green_short >> 8) + one_minus_alpha;
	    blue  = (color->blue_short  >> 8) + one_minus_alpha;
	} else {
	    /* Blend into black */
	    red   = (color->red_short   >> 8);
	    green = (color->green_short >> 8);
	    blue  = (color->blue_short  >> 8);
	}
    }
    c = RGB (red, green, blue);

    return c;
}

static cairo_status_t
_cairo_win32_printing_surface_select_solid_brush (cairo_win32_surface_t *surface,
                                                  cairo_pattern_t       *source)
{
    cairo_solid_pattern_t *pattern = (cairo_solid_pattern_t *) source;
    COLORREF color;

    color = _cairo_win32_printing_surface_flatten_transparency (surface,
								&pattern->color);
    surface->brush = CreateSolidBrush (color);
    if (!surface->brush)
	return _cairo_win32_print_gdi_error ("_cairo_win32_surface_select_solid_brush(CreateSolidBrush)");
    surface->old_brush = SelectObject (surface->dc, surface->brush);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_printing_surface_done_solid_brush (cairo_win32_surface_t *surface)
{
    if (surface->old_brush) {
	SelectObject (surface->dc, surface->old_brush);
	DeleteObject (surface->brush);
	surface->old_brush = NULL;
    }
}

static cairo_status_t
_cairo_win32_printing_surface_get_ctm_clip_box (cairo_win32_surface_t *surface,
						RECT                  *clip)
{
    XFORM xform;

    _cairo_matrix_to_win32_xform (&surface->ctm, &xform);
    if (!SetWorldTransform (surface->dc, &xform))
	return _cairo_win32_print_gdi_error ("_cairo_win32_printing_surface_get_clip_box:SetWorldTransform");
    GetClipBox (surface->dc, clip);
    if (!ModifyWorldTransform (surface->dc, &xform, MWT_IDENTITY))
	return _cairo_win32_print_gdi_error ("_cairo_win32_printing_surface_get_clip_box:ModifyWorldTransform");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_paint_solid_pattern (cairo_win32_surface_t *surface,
                                                   cairo_pattern_t       *pattern)
{
    RECT clip;
    cairo_status_t status;

    GetClipBox (surface->dc, &clip);
    status = _cairo_win32_printing_surface_select_solid_brush (surface, pattern);
    if (status)
	return status;

    FillRect (surface->dc, &clip, surface->brush);
    _cairo_win32_printing_surface_done_solid_brush (surface);

    return 0;
}

static cairo_status_t
_cairo_win32_printing_surface_paint_meta_pattern (cairo_win32_surface_t   *surface,
						  cairo_surface_pattern_t *pattern)
{
    cairo_content_t old_content;
    cairo_matrix_t old_ctm;
    cairo_bool_t old_has_ctm;
    cairo_rectangle_int_t meta_extents;
    cairo_status_t status;
    cairo_extend_t extend;
    cairo_matrix_t p2d;
    XFORM xform;
    int x_tile, y_tile, left, right, top, bottom;
    RECT clip;
    cairo_surface_t *meta_surface = pattern->surface;

    extend = cairo_pattern_get_extend (&pattern->base);

    p2d = pattern->base.matrix;
    status = cairo_matrix_invert (&p2d);
    /* _cairo_pattern_set_matrix guarantees invertibility */
    assert (status == CAIRO_STATUS_SUCCESS);

    old_ctm = surface->ctm;
    old_has_ctm = surface->has_ctm;
    cairo_matrix_multiply (&p2d, &p2d, &surface->ctm);
    surface->ctm = p2d;
    SaveDC (surface->dc);
    _cairo_matrix_to_win32_xform (&p2d, &xform);

    status = _cairo_surface_get_extents (meta_surface, &meta_extents);
    if (status)
	return status;

    status = _cairo_win32_printing_surface_get_ctm_clip_box (surface, &clip);
    if (status)
	return status;

    if (extend == CAIRO_EXTEND_REPEAT || extend == CAIRO_EXTEND_REFLECT) {
	left = (int) floor((double)clip.left/meta_extents.width);
	right = (int) ceil((double)clip.right/meta_extents.width);
	top = (int) floor((double)clip.top/meta_extents.height);
	bottom = (int) ceil((double)clip.bottom/meta_extents.height);
    } else {
	left = 0;
	right = 1;
	top = 0;
	bottom = 1;
    }

    old_content = surface->content;
    if (cairo_surface_get_content (meta_surface) == CAIRO_CONTENT_COLOR) {
	cairo_pattern_t  *source;
	cairo_solid_pattern_t black;

	surface->content = CAIRO_CONTENT_COLOR;
	_cairo_pattern_init_solid (&black, CAIRO_COLOR_BLACK, CAIRO_CONTENT_COLOR);
	source = (cairo_pattern_t*) &black;
	_cairo_win32_printing_surface_paint_solid_pattern (surface, source);
    }

    for (y_tile = top; y_tile < bottom; y_tile++) {
	for (x_tile = left; x_tile < right; x_tile++) {
	    cairo_matrix_t m;
	    double x, y;

	    SaveDC (surface->dc);
	    m = p2d;
	    cairo_matrix_translate (&m,
				    x_tile*meta_extents.width,
				    y_tile*meta_extents.height);
	    if (extend == CAIRO_EXTEND_REFLECT) {
		if (x_tile % 2) {
		    cairo_matrix_translate (&m, meta_extents.width, 0);
		    cairo_matrix_scale (&m, -1, 1);
		}
		if (y_tile % 2) {
		    cairo_matrix_translate (&m, 0, meta_extents.height);
		    cairo_matrix_scale (&m, 1, -1);
		}
	    }
	    surface->ctm = m;
	    surface->has_ctm = !_cairo_matrix_is_identity (&surface->ctm);

	    /* Set clip path around bbox of the pattern. */
	    BeginPath (surface->dc);

	    x = 0;
	    y = 0;
	    cairo_matrix_transform_point (&surface->ctm, &x, &y);
	    MoveToEx (surface->dc, (int) x, (int) y, NULL);

	    x = meta_extents.width;
	    y = 0;
	    cairo_matrix_transform_point (&surface->ctm, &x, &y);
	    LineTo (surface->dc, (int) x, (int) y);

	    x = meta_extents.width;
	    y = meta_extents.height;
	    cairo_matrix_transform_point (&surface->ctm, &x, &y);
	    LineTo (surface->dc, (int) x, (int) y);

	    x = 0;
	    y = meta_extents.height;
	    cairo_matrix_transform_point (&surface->ctm, &x, &y);
	    LineTo (surface->dc, (int) x, (int) y);

	    CloseFigure (surface->dc);
	    EndPath (surface->dc);
	    SelectClipPath (surface->dc, RGN_AND);

	    SaveDC (surface->dc); /* Allow clip path to be reset during replay */
	    status = _cairo_meta_surface_replay_region (meta_surface, &surface->base,
							CAIRO_META_REGION_NATIVE);
	    assert (status != CAIRO_INT_STATUS_UNSUPPORTED);
	    /* Restore both the clip save and our earlier path SaveDC */
	    RestoreDC (surface->dc, -2);

	    if (status)
		return status;
	}
    }

    surface->content = old_content;
    surface->ctm = old_ctm;
    surface->has_ctm = old_has_ctm;
    RestoreDC (surface->dc, -1);

    return status;
}

static cairo_status_t
_cairo_win32_printing_surface_paint_image_pattern (cairo_win32_surface_t   *surface,
						   cairo_surface_pattern_t *pattern)
{
    cairo_status_t status;
    cairo_extend_t extend;
    cairo_surface_t *pat_surface;
    cairo_surface_attributes_t pat_attr;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_surface_t *opaque_surface;
    cairo_pattern_union_t opaque_pattern;
    cairo_image_surface_t *opaque_image = NULL;
    BITMAPINFO bi;
    cairo_matrix_t m;
    int oldmode;
    XFORM xform;
    int x_tile, y_tile, left, right, top, bottom;
    RECT clip;
    const cairo_color_t *background_color;

    /* If we can't use StretchDIBits with this surface, we can't do anything
     * here.
     */
    if (!(surface->flags & CAIRO_WIN32_SURFACE_CAN_STRETCHDIB))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (surface->content == CAIRO_CONTENT_COLOR_ALPHA)
	background_color = CAIRO_COLOR_WHITE;
    else
	background_color = CAIRO_COLOR_BLACK;

    extend = cairo_pattern_get_extend (&pattern->base);
    status = _cairo_pattern_acquire_surface ((cairo_pattern_t *)pattern,
					     (cairo_surface_t *)surface,
					     0, 0, -1, -1,
					     &pat_surface, &pat_attr);
    if (status)
	return status;

    status = _cairo_surface_acquire_source_image (pat_surface, &image, &image_extra);
    if (status)
	goto FINISH;

    if (image->base.status) {
	status = image->base.status;
	goto FINISH2;
    }

    if (image->width == 0 || image->height == 0) {
	status = CAIRO_STATUS_SUCCESS;
	goto FINISH2;
    }

    if (image->format != CAIRO_FORMAT_RGB24) {
	opaque_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
						     image->width,
						     image->height);
	if (opaque_surface->status) {
	    status = opaque_surface->status;
	    goto FINISH3;
	}

	_cairo_pattern_init_for_surface (&opaque_pattern.surface, &image->base);

	status = _cairo_surface_fill_rectangle (opaque_surface,
				                CAIRO_OPERATOR_SOURCE,
						background_color,
						0, 0,
						image->width, image->height);
	if (status) {
	    _cairo_pattern_fini (&opaque_pattern.base);
	    goto FINISH3;
	}

	status = _cairo_surface_composite (CAIRO_OPERATOR_OVER,
				           &opaque_pattern.base,
					   NULL,
					   opaque_surface,
					   0, 0,
					   0, 0,
					   0, 0,
					   image->width,
					   image->height);
	if (status) {
	    _cairo_pattern_fini (&opaque_pattern.base);
	    goto FINISH3;
	}

	_cairo_pattern_fini (&opaque_pattern.base);
	opaque_image = (cairo_image_surface_t *) opaque_surface;
    } else {
	opaque_surface = &image->base;
	opaque_image = image;
    }

    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = opaque_image->width;
    bi.bmiHeader.biHeight = -opaque_image->height;
    bi.bmiHeader.biSizeImage = 0;
    bi.bmiHeader.biXPelsPerMeter = PELS_72DPI;
    bi.bmiHeader.biYPelsPerMeter = PELS_72DPI;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biClrUsed = 0;
    bi.bmiHeader.biClrImportant = 0;

    m = pattern->base.matrix;
    status = cairo_matrix_invert (&m);
    /* _cairo_pattern_set_matrix guarantees invertibility */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_matrix_multiply (&m, &m, &surface->ctm);
    SaveDC (surface->dc);
    _cairo_matrix_to_win32_xform (&m, &xform);

    if (!SetWorldTransform (surface->dc, &xform))
	return _cairo_win32_print_gdi_error ("_win32_scaled_font_set_world_transform");

    oldmode = SetStretchBltMode(surface->dc, HALFTONE);

    GetClipBox (surface->dc, &clip);
    if (extend == CAIRO_EXTEND_REPEAT || extend == CAIRO_EXTEND_REFLECT) {
	left = (int) floor((double)clip.left/opaque_image->width);
	right = (int) ceil((double)clip.right/opaque_image->width);
	top = (int) floor((double)clip.top/opaque_image->height);
	bottom = (int) ceil((double)clip.bottom/opaque_image->height);
    } else {
	left = 0;
	right = 1;
	top = 0;
	bottom = 1;
    }

    for (y_tile = top; y_tile < bottom; y_tile++) {
	for (x_tile = left; x_tile < right; x_tile++) {
	    if (!StretchDIBits (surface->dc,
				x_tile*opaque_image->width,
				y_tile*opaque_image->height,
				opaque_image->width,
				opaque_image->height,
				0,
				0,
				opaque_image->width,
				opaque_image->height,
				opaque_image->data,
				&bi,
				DIB_RGB_COLORS,
				SRCCOPY))
		return _cairo_win32_print_gdi_error ("_cairo_win32_printing_surface_paint(StretchDIBits)");
	}
    }
    SetStretchBltMode(surface->dc, oldmode);
    RestoreDC (surface->dc, -1);

FINISH3:
    if (opaque_image != image)
	cairo_surface_destroy (opaque_surface);
FINISH2:
    _cairo_surface_release_source_image (pat_surface, image, image_extra);
FINISH:
    _cairo_pattern_release_surface ((cairo_pattern_t *)pattern, pat_surface, &pat_attr);

    return status;
}

static cairo_status_t
_cairo_win32_printing_surface_paint_surface_pattern (cairo_win32_surface_t   *surface,
                                                     cairo_surface_pattern_t *pattern)
{
    if (_cairo_surface_is_meta (pattern->surface)) {
	return _cairo_win32_printing_surface_paint_meta_pattern (surface,
								 pattern);
    } else {
	return _cairo_win32_printing_surface_paint_image_pattern (surface,
								  pattern);
    }
}

static void
vertex_set_color (TRIVERTEX *vert, cairo_color_t *color)
{
    /* MSDN says that the range here is 0x0000 .. 0xff00;
     * that may well be a typo, but just chop the low bits
     * here. */
    vert->Alpha = 0xff00;
    vert->Red   = color->red_short & 0xff00;
    vert->Green = color->green_short & 0xff00;
    vert->Blue  = color->blue_short & 0xff00;
}

static cairo_int_status_t
_cairo_win32_printing_surface_paint_linear_pattern (cairo_win32_surface_t *surface,
                                                    cairo_linear_pattern_t *pattern)
{
    TRIVERTEX *vert;
    GRADIENT_RECT *rect;
    RECT clip;
    XFORM xform;
    int i, num_stops;
    cairo_matrix_t mat, rot;
    double p1x, p1y, p2x, p2y, xd, yd, d, sn, cs;
    cairo_extend_t extend;
    int range_start, range_stop, num_ranges, num_rects, stop;
    int total_verts, total_rects;
    cairo_status_t status;

    extend = cairo_pattern_get_extend (&pattern->base.base);
    SaveDC (surface->dc);

    mat = pattern->base.base.matrix;
    status = cairo_matrix_invert (&mat);
    /* _cairo_pattern_set_matrix guarantees invertibility */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_matrix_multiply (&mat, &surface->ctm, &mat);

    p1x = _cairo_fixed_to_double (pattern->p1.x);
    p1y = _cairo_fixed_to_double (pattern->p1.y);
    p2x = _cairo_fixed_to_double (pattern->p2.x);
    p2y = _cairo_fixed_to_double (pattern->p2.y);
    cairo_matrix_translate (&mat, p1x, p1y);

    xd = p2x - p1x;
    yd = p2y - p1y;
    d = sqrt (xd*xd + yd*yd);
    sn = yd/d;
    cs = xd/d;
    cairo_matrix_init (&rot,
		       cs, sn,
		       -sn, cs,
		        0, 0);
    cairo_matrix_multiply (&mat, &rot, &mat);

    _cairo_matrix_to_win32_xform (&mat, &xform);

    if (!SetWorldTransform (surface->dc, &xform))
	return _cairo_win32_print_gdi_error ("_win32_printing_surface_paint_linear_pattern:SetWorldTransform2");

    GetClipBox (surface->dc, &clip);

    if (extend == CAIRO_EXTEND_REPEAT || extend == CAIRO_EXTEND_REFLECT) {
	range_start = (int) floor(clip.left/d);
	range_stop = (int) ceil(clip.right/d);
    } else {
	range_start = 0;
	range_stop = 1;
    }
    num_ranges = range_stop - range_start;
    num_stops = pattern->base.n_stops;
    num_rects = num_stops - 1;

    /* Add an extra four points and two rectangles for EXTEND_PAD */
    vert = malloc (sizeof (TRIVERTEX) * (num_rects*2*num_ranges + 4));
    rect = malloc (sizeof (GRADIENT_RECT) * (num_rects*num_ranges + 2));

    for (i = 0; i < num_ranges*num_rects; i++) {
	vert[i*2].y = (LONG) clip.top;
	if (i%num_rects == 0) {
	    stop = 0;
	    if (extend == CAIRO_EXTEND_REFLECT && (range_start+(i/num_rects))%2)
		stop = num_rects;
	    vert[i*2].x = (LONG)(d*(range_start + i/num_rects));
	    vertex_set_color (&vert[i*2], &pattern->base.stops[stop].color);
	} else {
	    vert[i*2].x = vert[i*2-1].x;
	    vert[i*2].Red = vert[i*2-1].Red;
	    vert[i*2].Green = vert[i*2-1].Green;
	    vert[i*2].Blue = vert[i*2-1].Blue;
	    vert[i*2].Alpha = vert[i*2-1].Alpha;
	}

	stop = i%num_rects + 1;
	vert[i*2+1].x = (LONG)(d*(range_start + i/num_rects + pattern->base.stops[stop].offset));
	vert[i*2+1].y = (LONG) clip.bottom;
	if (extend == CAIRO_EXTEND_REFLECT && (range_start+(i/num_rects))%2)
	    stop = num_rects - stop;
	vertex_set_color (&vert[i*2+1], &pattern->base.stops[stop].color);

	rect[i].UpperLeft = i*2;
	rect[i].LowerRight = i*2 + 1;
    }
    total_verts = 2*num_ranges*num_rects;
    total_rects = num_ranges*num_rects;

    if (extend == CAIRO_EXTEND_PAD) {
	vert[i*2].x = vert[i*2-1].x;
	vert[i*2].y = (LONG) clip.top;
	vert[i*2].Red = vert[i*2-1].Red;
	vert[i*2].Green = vert[i*2-1].Green;
	vert[i*2].Blue = vert[i*2-1].Blue;
	vert[i*2].Alpha = 0xff00;
	vert[i*2+1].x = clip.right;
	vert[i*2+1].y = (LONG) clip.bottom;
	vert[i*2+1].Red = vert[i*2-1].Red;
	vert[i*2+1].Green = vert[i*2-1].Green;
	vert[i*2+1].Blue = vert[i*2-1].Blue;
	vert[i*2+1].Alpha = 0xff00;
	rect[i].UpperLeft = i*2;
	rect[i].LowerRight = i*2 + 1;

	i++;

	vert[i*2].x = clip.left;
	vert[i*2].y = (LONG) clip.top;
	vert[i*2].Red = vert[0].Red;
	vert[i*2].Green = vert[0].Green;
	vert[i*2].Blue = vert[0].Blue;
	vert[i*2].Alpha = 0xff00;
	vert[i*2+1].x = vert[0].x;
	vert[i*2+1].y = (LONG) clip.bottom;
	vert[i*2+1].Red = vert[0].Red;
	vert[i*2+1].Green = vert[0].Green;
	vert[i*2+1].Blue = vert[0].Blue;
	vert[i*2+1].Alpha = 0xff00;
	rect[i].UpperLeft = i*2;
	rect[i].LowerRight = i*2 + 1;

	total_verts += 4;
	total_rects += 2;
    }

    if (!GradientFill (surface->dc,
		       vert, total_verts,
		       rect, total_rects,
		       GRADIENT_FILL_RECT_H))
	return _cairo_win32_print_gdi_error ("_win32_printing_surface_paint_linear_pattern:GradientFill");

    free (rect);
    free (vert);
    RestoreDC (surface->dc, -1);

    return 0;
}

static cairo_int_status_t
_cairo_win32_printing_surface_paint_pattern (cairo_win32_surface_t *surface,
                                             cairo_pattern_t       *pattern)
{
    cairo_status_t status;

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	status = _cairo_win32_printing_surface_paint_solid_pattern (surface, pattern);
	if (status)
	    return status;
	break;

    case CAIRO_PATTERN_TYPE_SURFACE:
	status = _cairo_win32_printing_surface_paint_surface_pattern (surface,
                                                                      (cairo_surface_pattern_t *) pattern);
	if (status)
	    return status;
	break;

    case CAIRO_PATTERN_TYPE_LINEAR:
	status = _cairo_win32_printing_surface_paint_linear_pattern (surface, (cairo_linear_pattern_t *) pattern);
	if (status)
	    return status;
	break;

    case CAIRO_PATTERN_TYPE_RADIAL:
	return CAIRO_INT_STATUS_UNSUPPORTED;
	break;
    }

    return CAIRO_STATUS_SUCCESS;
}

typedef struct _win32_print_path_info {
    cairo_win32_surface_t *surface;
} win32_path_info_t;

static cairo_status_t
_cairo_win32_printing_surface_path_move_to (void *closure, cairo_point_t *point)
{
    win32_path_info_t *path_info = closure;

    if (path_info->surface->has_ctm) {
	double x, y;

	x = _cairo_fixed_to_double (point->x);
	y = _cairo_fixed_to_double (point->y);
	cairo_matrix_transform_point (&path_info->surface->ctm, &x, &y);
	MoveToEx (path_info->surface->dc, (int) x, (int) y, NULL);
    } else {
	MoveToEx (path_info->surface->dc,
		  _cairo_fixed_integer_part (point->x),
		  _cairo_fixed_integer_part (point->y),
		  NULL);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_path_line_to (void *closure, cairo_point_t *point)
{
    win32_path_info_t *path_info = closure;

    path_info->surface->path_empty = FALSE;
    if (path_info->surface->has_ctm) {
	double x, y;

	x = _cairo_fixed_to_double (point->x);
	y = _cairo_fixed_to_double (point->y);
	cairo_matrix_transform_point (&path_info->surface->ctm, &x, &y);
	LineTo (path_info->surface->dc, (int) x, (int) y);
    } else {
	LineTo (path_info->surface->dc,
		_cairo_fixed_integer_part (point->x),
		_cairo_fixed_integer_part (point->y));
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_path_curve_to (void          *closure,
                                             cairo_point_t *b,
                                             cairo_point_t *c,
                                             cairo_point_t *d)
{
    win32_path_info_t *path_info = closure;
    POINT points[3];

    path_info->surface->path_empty = FALSE;
    if (path_info->surface->has_ctm) {
	double x, y;

	x = _cairo_fixed_to_double (b->x);
	y = _cairo_fixed_to_double (b->y);
	cairo_matrix_transform_point (&path_info->surface->ctm, &x, &y);
	points[0].x = (LONG) x;
	points[0].y = (LONG) y;

	x = _cairo_fixed_to_double (c->x);
	y = _cairo_fixed_to_double (c->y);
	cairo_matrix_transform_point (&path_info->surface->ctm, &x, &y);
	points[1].x = (LONG) x;
	points[1].y = (LONG) y;

	x = _cairo_fixed_to_double (d->x);
	y = _cairo_fixed_to_double (d->y);
	cairo_matrix_transform_point (&path_info->surface->ctm, &x, &y);
	points[2].x = (LONG) x;
	points[2].y = (LONG) y;
    } else {
	points[0].x = _cairo_fixed_integer_part (b->x);
	points[0].y = _cairo_fixed_integer_part (b->y);
	points[1].x = _cairo_fixed_integer_part (c->x);
	points[1].y = _cairo_fixed_integer_part (c->y);
	points[2].x = _cairo_fixed_integer_part (d->x);
	points[2].y = _cairo_fixed_integer_part (d->y);
    }
    PolyBezierTo (path_info->surface->dc, points, 3);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_path_close_path (void *closure)
{
    win32_path_info_t *path_info = closure;

    CloseFigure (path_info->surface->dc);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_emit_path (cairo_win32_surface_t *surface,
                                         cairo_path_fixed_t    *path)
{
    win32_path_info_t path_info;
    cairo_status_t status;

    path_info.surface = surface;
    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_win32_printing_surface_path_move_to,
					  _cairo_win32_printing_surface_path_line_to,
					  _cairo_win32_printing_surface_path_curve_to,
					  _cairo_win32_printing_surface_path_close_path,
					  &path_info);
    return status;
}

static cairo_int_status_t
_cairo_win32_printing_surface_show_page (void *abstract_surface)
{
    cairo_win32_surface_t *surface = abstract_surface;

    /* Undo both SaveDC's that we did in start_page */
    RestoreDC (surface->dc, -2);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_win32_printing_surface_intersect_clip_path (void		      *abstract_surface,
                                                   cairo_path_fixed_t *path,
                                                   cairo_fill_rule_t   fill_rule,
                                                   double	       tolerance,
                                                   cairo_antialias_t   antialias)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_STATUS_SUCCESS;

    if (path == NULL) {
	RestoreDC (surface->dc, -1);
	SaveDC (surface->dc);

	return CAIRO_STATUS_SUCCESS;
    }

    BeginPath (surface->dc);
    status = _cairo_win32_printing_surface_emit_path (surface, path);
    EndPath (surface->dc);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	SetPolyFillMode (surface->dc, WINDING);
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	SetPolyFillMode (surface->dc, ALTERNATE);
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    SelectClipPath (surface->dc, RGN_AND);

    return status;
}

static void
_cairo_win32_printing_surface_get_font_options (void                  *abstract_surface,
                                                cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_antialias (options, CAIRO_ANTIALIAS_GRAY);
}

static cairo_int_status_t
_cairo_win32_printing_surface_paint (void             *abstract_surface,
                                     cairo_operator_t  op,
                                     cairo_pattern_t  *source)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_solid_pattern_t clear;

    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_win32_printing_surface_init_clear_color (surface, &clear);
	source = (cairo_pattern_t*) &clear;
	op = CAIRO_OPERATOR_SOURCE;
    }

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_win32_printing_surface_analyze_operation (surface, op, source);

    assert (_cairo_win32_printing_surface_operation_supported (surface, op, source));

    return _cairo_win32_printing_surface_paint_pattern (surface, source);
}

static int
_cairo_win32_line_cap (cairo_line_cap_t cap)
{
    switch (cap) {
    case CAIRO_LINE_CAP_BUTT:
	return PS_ENDCAP_FLAT;
    case CAIRO_LINE_CAP_ROUND:
	return PS_ENDCAP_ROUND;
    case CAIRO_LINE_CAP_SQUARE:
	return PS_ENDCAP_SQUARE;
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static int
_cairo_win32_line_join (cairo_line_join_t join)
{
    switch (join) {
    case CAIRO_LINE_JOIN_MITER:
	return PS_JOIN_MITER;
    case CAIRO_LINE_JOIN_ROUND:
	return PS_JOIN_ROUND;
    case CAIRO_LINE_JOIN_BEVEL:
	return PS_JOIN_BEVEL;
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static void
_cairo_matrix_factor_out_scale (cairo_matrix_t *m, double *scale)
{
    double s;

    s = fabs (m->xx);
    if (fabs (m->xy) > s)
	s = fabs (m->xy);
    if (fabs (m->yx) > s)
	s = fabs (m->yx);
    if (fabs (m->yy) > s)
	s = fabs (m->yy);
    *scale = s;
    s = 1.0/s;
    cairo_matrix_scale (m, s, s);
}

static cairo_int_status_t
_cairo_win32_printing_surface_stroke (void                 *abstract_surface,
                                      cairo_operator_t	    op,
                                      cairo_pattern_t	   *source,
                                      cairo_path_fixed_t   *path,
                                      cairo_stroke_style_t *style,
                                      cairo_matrix_t       *stroke_ctm,
                                      cairo_matrix_t       *stroke_ctm_inverse,
                                      double       	    tolerance,
                                      cairo_antialias_t     antialias)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_int_status_t status;
    HPEN pen;
    LOGBRUSH brush;
    COLORREF color;
    XFORM xform;
    DWORD pen_style;
    DWORD *dash_array;
    HGDIOBJ obj;
    unsigned int i;
    cairo_solid_pattern_t clear;
    cairo_matrix_t mat;
    double scale;

    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_win32_printing_surface_init_clear_color (surface, &clear);
	source = (cairo_pattern_t*) &clear;
	op = CAIRO_OPERATOR_SOURCE;
    }

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	/* Win32 does not support a dash offset. */
	if (style->num_dashes > 0 && style->dash_offset != 0.0)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	return _cairo_win32_printing_surface_analyze_operation (surface, op, source);
    }

    assert (_cairo_win32_printing_surface_operation_supported (surface, op, source));
    assert (!(style->num_dashes > 0 && style->dash_offset != 0.0));

    cairo_matrix_multiply (&mat, stroke_ctm, &surface->ctm);
    _cairo_matrix_factor_out_scale (&mat, &scale);

    pen_style = PS_GEOMETRIC;
    dash_array = NULL;
    if (style->num_dashes) {
	pen_style |= PS_USERSTYLE;
	dash_array = calloc (sizeof (DWORD), style->num_dashes);
	for (i = 0; i < style->num_dashes; i++) {
	    dash_array[i] = (DWORD) (scale * style->dash[i]);
	}
    } else {
	pen_style |= PS_SOLID;
    }

    SetMiterLimit (surface->dc, (FLOAT) (style->miter_limit), NULL);
    if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) source;


	color = _cairo_win32_printing_surface_flatten_transparency (surface,
								    &solid->color);
    } else {
	/* Color not used as the pen will only be used by WidenPath() */
	color = RGB (0,0,0);
    }
    brush.lbStyle = BS_SOLID;
    brush.lbColor = color;
    brush.lbHatch = 0;
    pen_style |= _cairo_win32_line_cap (style->line_cap);
    pen_style |= _cairo_win32_line_join (style->line_join);
    pen = ExtCreatePen(pen_style,
		       scale * style->line_width,
		       &brush,
		       style->num_dashes,
		       dash_array);
    if (pen == NULL)
	return _cairo_win32_print_gdi_error ("_win32_surface_stroke:ExtCreatePen");
    obj = SelectObject (surface->dc, pen);
    if (obj == NULL)
	return _cairo_win32_print_gdi_error ("_win32_surface_stroke:SelectObject");

    BeginPath (surface->dc);
    status = _cairo_win32_printing_surface_emit_path (surface, path);
    EndPath (surface->dc);
    if (status)
	return status;

    /*
     * Switch to user space to set line parameters
     */
    SaveDC (surface->dc);

    _cairo_matrix_to_win32_xform (&mat, &xform);
    xform.eDx = 0.0f;
    xform.eDy = 0.0f;

    if (!SetWorldTransform (surface->dc, &xform))
	return _cairo_win32_print_gdi_error ("_win32_surface_stroke:SetWorldTransform");

    if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	StrokePath (surface->dc);
    } else {
	if (!WidenPath (surface->dc))
	    return _cairo_win32_print_gdi_error ("_win32_surface_stroke:WidenPath");
	if (!SelectClipPath (surface->dc, RGN_AND))
	    return _cairo_win32_print_gdi_error ("_win32_surface_stroke:SelectClipPath");

	/* Return to device space to paint the pattern */
	if (!ModifyWorldTransform (surface->dc, &xform, MWT_IDENTITY))
	    return _cairo_win32_print_gdi_error ("_win32_surface_stroke:ModifyWorldTransform");
	status = _cairo_win32_printing_surface_paint_pattern (surface, source);
    }
    RestoreDC (surface->dc, -1);
    DeleteObject (pen);
    if (dash_array)
	free (dash_array);

    return status;
}

static cairo_int_status_t
_cairo_win32_printing_surface_fill (void		        *abstract_surface,
				    cairo_operator_t	 op,
				    cairo_pattern_t	*source,
				    cairo_path_fixed_t	*path,
				    cairo_fill_rule_t	 fill_rule,
				    double		 tolerance,
				    cairo_antialias_t	 antialias)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_int_status_t status;
    cairo_solid_pattern_t clear;

    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_win32_printing_surface_init_clear_color (surface, &clear);
	source = (cairo_pattern_t*) &clear;
	op = CAIRO_OPERATOR_SOURCE;
    }

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_win32_printing_surface_analyze_operation (surface, op, source);

    assert (_cairo_win32_printing_surface_operation_supported (surface, op, source));

    surface->path_empty = TRUE;
    BeginPath (surface->dc);
    status = _cairo_win32_printing_surface_emit_path (surface, path);
    EndPath (surface->dc);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	SetPolyFillMode (surface->dc, WINDING);
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	SetPolyFillMode (surface->dc, ALTERNATE);
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	status = _cairo_win32_printing_surface_select_solid_brush (surface, source);
	if (status)
	    return status;

	FillPath (surface->dc);
	_cairo_win32_printing_surface_done_solid_brush (surface);
    } else if (surface->path_empty == FALSE) {
	SaveDC (surface->dc);
	SelectClipPath (surface->dc, RGN_AND);
	status = _cairo_win32_printing_surface_paint_pattern (surface, source);
	RestoreDC (surface->dc, -1);
    }

    fflush(stderr);

    return status;
}

static cairo_int_status_t
_cairo_win32_printing_surface_show_glyphs (void                 *abstract_surface,
                                           cairo_operator_t	 op,
                                           cairo_pattern_t	*source,
                                           cairo_glyph_t        *glyphs,
                                           int			 num_glyphs,
                                           cairo_scaled_font_t  *scaled_font)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_pattern_t *opaque = NULL;
    int i;
    cairo_matrix_t old_ctm;
    cairo_bool_t old_has_ctm;
    cairo_solid_pattern_t clear;

    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_win32_printing_surface_init_clear_color (surface, &clear);
	source = (cairo_pattern_t*) &clear;
	op = CAIRO_OPERATOR_SOURCE;
    }

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	if (!(cairo_scaled_font_get_type (scaled_font) == CAIRO_FONT_TYPE_WIN32 &&
	      source->type == CAIRO_PATTERN_TYPE_SOLID)) {
	    for (i = 0; i < num_glyphs; i++) {
		status = _cairo_scaled_glyph_lookup (scaled_font,
						     glyphs[i].index,
						     CAIRO_SCALED_GLYPH_INFO_PATH,
						     &scaled_glyph);
		if (status)
		    return status;
	    }
	}

	return _cairo_win32_printing_surface_analyze_operation (surface, op, source);
    }

    if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) source;
	COLORREF color;

	color = _cairo_win32_printing_surface_flatten_transparency (surface,
								    &solid->color);
	opaque = cairo_pattern_create_rgb (GetRValue (color) / 255.0,
					   GetGValue (color) / 255.0,
					   GetBValue (color) / 255.0);
	if (opaque->status)
	    return opaque->status;
	source = opaque;
    }

    if (cairo_scaled_font_get_type (scaled_font) == CAIRO_FONT_TYPE_WIN32 &&
	source->type == CAIRO_PATTERN_TYPE_SOLID)
    {
	cairo_matrix_t ctm;

	if (surface->has_ctm) {
	    for (i = 0; i < num_glyphs; i++)
		cairo_matrix_transform_point (&surface->ctm, &glyphs[i].x, &glyphs[i].y);
	    cairo_matrix_multiply (&ctm, &scaled_font->ctm, &surface->ctm);
	    scaled_font = cairo_scaled_font_create (scaled_font->font_face,
						    &scaled_font->font_matrix,
						    &ctm,
						    &scaled_font->options);
	}
	status = _cairo_win32_surface_show_glyphs (surface, op,
						   source, glyphs,
						   num_glyphs, scaled_font);
	if (surface->has_ctm)
	    cairo_scaled_font_destroy (scaled_font);

	return status;
    }

    SaveDC (surface->dc);
    old_ctm = surface->ctm;
    old_has_ctm = surface->has_ctm;
    surface->has_ctm = TRUE;
    surface->path_empty = TRUE;
    BeginPath (surface->dc);
    for (i = 0; i < num_glyphs; i++) {
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_PATH,
					     &scaled_glyph);
	if (status)
	    break;
	surface->ctm = old_ctm;
	cairo_matrix_translate (&surface->ctm, glyphs[i].x, glyphs[i].y);
	status = _cairo_win32_printing_surface_emit_path (surface, scaled_glyph->path);
    }
    EndPath (surface->dc);
    surface->ctm = old_ctm;
    surface->has_ctm = old_has_ctm;
    if (status == CAIRO_STATUS_SUCCESS && surface->path_empty == FALSE) {
	if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	    status = _cairo_win32_printing_surface_select_solid_brush (surface, source);
	    if (status)
		return status;

	    SetPolyFillMode (surface->dc, WINDING);
	    FillPath (surface->dc);
	    _cairo_win32_printing_surface_done_solid_brush (surface);
	} else {
	    SelectClipPath (surface->dc, RGN_AND);
	    status = _cairo_win32_printing_surface_paint_pattern (surface, source);
	}
    }
    RestoreDC (surface->dc, -1);

    if (opaque)
	cairo_pattern_destroy (opaque);

    return status;
}

static cairo_surface_t *
_cairo_win32_printing_surface_create_similar (void		*abstract_surface,
					      cairo_content_t	 content,
					      int		 width,
					      int		 height)
{
    return _cairo_meta_surface_create (content, width, height);
}

static cairo_int_status_t
_cairo_win32_printing_surface_start_page (void *abstract_surface)
{
    cairo_win32_surface_t *surface = abstract_surface;
    XFORM xform;
    double x_res, y_res;
    cairo_matrix_t inverse_ctm;
    cairo_status_t status;

    SaveDC (surface->dc); /* Save application context first, before doing MWT */

    SetGraphicsMode (surface->dc, GM_ADVANCED);
    GetWorldTransform(surface->dc, &xform);
    surface->ctm.xx = xform.eM11;
    surface->ctm.xy = xform.eM21;
    surface->ctm.yx = xform.eM12;
    surface->ctm.yy = xform.eM22;
    surface->ctm.x0 = xform.eDx;
    surface->ctm.y0 = xform.eDy;
    surface->has_ctm = !_cairo_matrix_is_identity (&surface->ctm);

    inverse_ctm = surface->ctm;
    status = cairo_matrix_invert (&inverse_ctm);
    if (status)
	return status;

    x_res = (double) GetDeviceCaps(surface->dc, LOGPIXELSX);
    y_res = (double) GetDeviceCaps(surface->dc, LOGPIXELSY);
    cairo_matrix_transform_distance (&inverse_ctm, &x_res, &y_res);
    _cairo_surface_set_resolution (&surface->base, x_res, y_res);

    if (!ModifyWorldTransform (surface->dc, NULL, MWT_IDENTITY))
	return _cairo_win32_print_gdi_error ("_cairo_win32_printing_surface_start_page:ModifyWorldTransform");

    SaveDC (surface->dc); /* Then save Cairo's known-good clip state, so the clip path can be reset */

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_printing_surface_set_paginated_mode (void *abstract_surface,
                                                  cairo_paginated_mode_t paginated_mode)
{
    cairo_win32_surface_t *surface = abstract_surface;

    surface->paginated_mode = paginated_mode;
}

/**
 * cairo_win32_printing_surface_create:
 * @hdc: the DC to create a surface for
 *
 * Creates a cairo surface that targets the given DC.  The DC will be
 * queried for its initial clip extents, and this will be used as the
 * size of the cairo surface.  The DC should be a printing DC;
 * antialiasing will be ignored, and GDI will be used as much as
 * possible to draw to the surface.
 *
 * The returned surface will be wrapped using the paginated surface to
 * provide correct complex rendering behaviour; show_page() and
 * associated methods must be used for correct output.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_win32_printing_surface_create (HDC hdc)
{
    cairo_win32_surface_t *surface;
    RECT rect;

    surface = malloc (sizeof (cairo_win32_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    if (_cairo_win32_save_initial_clip (hdc, surface) != CAIRO_STATUS_SUCCESS) {
	free (surface);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    surface->image = NULL;
    surface->format = CAIRO_FORMAT_RGB24;
    surface->content = CAIRO_CONTENT_COLOR_ALPHA;

    surface->dc = hdc;
    surface->bitmap = NULL;
    surface->is_dib = FALSE;
    surface->saved_dc_bitmap = NULL;
    surface->brush = NULL;
    surface->old_brush = NULL;

    GetClipBox(hdc, &rect);
    surface->extents.x = rect.left;
    surface->extents.y = rect.top;
    surface->extents.width = rect.right - rect.left;
    surface->extents.height = rect.bottom - rect.top;

    surface->flags = _cairo_win32_flags_for_dc (surface->dc);
    surface->flags |= CAIRO_WIN32_SURFACE_FOR_PRINTING;

    _cairo_win32_printing_surface_init_ps_mode (surface);
    _cairo_surface_init (&surface->base, &cairo_win32_printing_surface_backend,
                         CAIRO_CONTENT_COLOR_ALPHA);

    return _cairo_paginated_surface_create (&surface->base,
                                            CAIRO_CONTENT_COLOR_ALPHA,
					    surface->extents.width,
					    surface->extents.height,
                                            &cairo_win32_surface_paginated_backend);
}

cairo_bool_t
_cairo_surface_is_win32_printing (cairo_surface_t *surface)
{
    return surface->backend == &cairo_win32_printing_surface_backend;
}

static const cairo_surface_backend_t cairo_win32_printing_surface_backend = {
    CAIRO_SURFACE_TYPE_WIN32_PRINTING,
    _cairo_win32_printing_surface_create_similar,
    _cairo_win32_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    _cairo_win32_surface_clone_similar,
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    _cairo_win32_printing_surface_show_page,
    NULL, /* set_clip_region */
    _cairo_win32_printing_surface_intersect_clip_path,
    _cairo_win32_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_win32_printing_surface_get_font_options,
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    _cairo_win32_printing_surface_paint,
    NULL, /* mask */
    _cairo_win32_printing_surface_stroke,
    _cairo_win32_printing_surface_fill,
    _cairo_win32_printing_surface_show_glyphs,
    NULL, /* snapshot */
    NULL, /* is_similar */
    NULL, /* reset */
    NULL, /* fill_stroke */
};

static const cairo_paginated_surface_backend_t cairo_win32_surface_paginated_backend = {
    _cairo_win32_printing_surface_start_page,
    _cairo_win32_printing_surface_set_paginated_mode,
    NULL, /* set_bounding_box */
};
