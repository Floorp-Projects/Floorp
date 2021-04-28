/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2007, 2008 Adrian Johnson
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

#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-paginated-private.h"

#include "cairo-clip-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-win32-private.h"
#include "cairo-recording-surface-inline.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-image-info-private.h"
#include "cairo-image-surface-inline.h"
#include "cairo-image-surface-private.h"
#include "cairo-surface-backend-private.h"
#include "cairo-surface-clipper-private.h"
#include "cairo-surface-snapshot-inline.h"
#include "cairo-surface-subsurface-private.h"

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

#if !defined(CHECKJPEGFORMAT)
# define CHECKJPEGFORMAT 0x1017
#endif

#if !defined(CHECKPNGFORMAT)
# define CHECKPNGFORMAT 0x1018
#endif

#define PELS_72DPI  ((LONG)(72. / 0.0254))

static const char *_cairo_win32_printing_supported_mime_types[] =
{
    CAIRO_MIME_TYPE_JPEG,
    CAIRO_MIME_TYPE_PNG,
    NULL
};

static const cairo_surface_backend_t cairo_win32_printing_surface_backend;
static const cairo_paginated_surface_backend_t cairo_win32_surface_paginated_backend;

static void
_cairo_win32_printing_surface_init_ps_mode (cairo_win32_printing_surface_t *surface)
{
    DWORD word;
    INT ps_feature, ps_level;

    word = PSIDENT_GDICENTRIC;
    if (ExtEscape (surface->win32.dc, POSTSCRIPT_IDENTIFY, sizeof(DWORD), (char *)&word, 0, (char *)NULL) <= 0)
	return;

    ps_feature = FEATURESETTING_PSLEVEL;
    if (ExtEscape (surface->win32.dc, GET_PS_FEATURESETTING, sizeof(INT),
		   (char *)&ps_feature, sizeof(INT), (char *)&ps_level) <= 0)
	return;

    if (ps_level >= 3)
	surface->win32.flags |= CAIRO_WIN32_SURFACE_CAN_RECT_GRADIENT;
}

static void
_cairo_win32_printing_surface_init_image_support (cairo_win32_printing_surface_t *surface)
{
    DWORD word;

    word = CHECKJPEGFORMAT;
    if (ExtEscape(surface->win32.dc, QUERYESCSUPPORT, sizeof(word), (char *)&word, 0, (char *)NULL) > 0)
	surface->win32.flags |= CAIRO_WIN32_SURFACE_CAN_CHECK_JPEG;

    word = CHECKPNGFORMAT;
    if (ExtEscape(surface->win32.dc, QUERYESCSUPPORT, sizeof(word), (char *)&word, 0, (char *)NULL) > 0)
	surface->win32.flags |= CAIRO_WIN32_SURFACE_CAN_CHECK_PNG;
}

/* When creating an EMF file, ExtTextOut with ETO_GLYPH_INDEX does not
 * work unless the GDI function GdiInitializeLanguagePack() has been
 * called.
 *
 *   http://m-a-tech.blogspot.com/2009/04/emf-buffer-idiocracy.html
 *
 * The only information I could find on the how to use this
 * undocumented function is the use in:
 *
 * http://src.chromium.org/viewvc/chrome/trunk/src/chrome/renderer/render_process.cc?view=markup
 *
 * to solve the same problem. The above code first checks if LPK.DLL
 * is already loaded. If it is not it calls
 * GdiInitializeLanguagePack() using the prototype
 *   BOOL GdiInitializeLanguagePack (int)
 * and argument 0.
 */
static void
_cairo_win32_printing_surface_init_language_pack (cairo_win32_printing_surface_t *surface)
{
    typedef BOOL (WINAPI *gdi_init_lang_pack_func_t)(int);
    gdi_init_lang_pack_func_t gdi_init_lang_pack;
    HMODULE module;

    if (GetModuleHandleW (L"LPK.DLL"))
	return;

    module = GetModuleHandleW (L"GDI32.DLL");
    if (module) {
	gdi_init_lang_pack = (gdi_init_lang_pack_func_t)
	    GetProcAddress (module, "GdiInitializeLanguagePack");
	if (gdi_init_lang_pack)
	    gdi_init_lang_pack (0);
    }
}

/**
 * _cairo_win32_printing_surface_acquire_image_pattern:
 * @surface: the win32 printing surface
 * @pattern: A #cairo_pattern_t of type SURFACE or RASTER_SOURCE to use as the source
 * @extents: extents of the operation that is using this source
 * @image_pattern: returns pattern containing acquired image. The matrix (adjusted for
 * the device offset of raster source) is copied from the pattern.
 * @width: returns width of the pattern
 * @height: returns height of  pattern
 * @image_extra: returns image extra for image type surface
 *
 * Acquire source surface or raster source pattern.
 **/
static cairo_status_t
_cairo_win32_printing_surface_acquire_image_pattern (
    cairo_win32_printing_surface_t  *surface,
    const cairo_pattern_t           *pattern,
    const cairo_rectangle_int_t     *extents,
    cairo_surface_pattern_t         *image_pattern,
    int                             *width,
    int                             *height,
    void                           **image_extra)
{
    cairo_status_t          status;
    cairo_image_surface_t  *image;
    cairo_matrix_t tm;
    double x = 0;
    double y = 0;

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SURFACE: {
	cairo_surface_t *surf = ((cairo_surface_pattern_t *) pattern)->surface;

	status =  _cairo_surface_acquire_source_image (surf, &image, image_extra);
	if (unlikely (status))
	    return status;

	*width = image->width;
	*height = image->height;
    } break;

    case CAIRO_PATTERN_TYPE_RASTER_SOURCE: {
	cairo_surface_t *surf;
	cairo_box_t box;
	cairo_rectangle_int_t rect;
	cairo_raster_source_pattern_t *raster;

	/* get the operation extents in pattern space */
	_cairo_box_from_rectangle (&box, extents);
	_cairo_matrix_transform_bounding_box_fixed (&pattern->matrix, &box, NULL);
	_cairo_box_round_to_rectangle (&box, &rect);
	surf = _cairo_raster_source_pattern_acquire (pattern, &surface->win32.base, &rect);
	if (!surf)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	assert (_cairo_surface_is_image (surf));
	image = (cairo_image_surface_t *) surf;
	cairo_surface_get_device_offset (surf, &x, &y);

	raster = (cairo_raster_source_pattern_t *) pattern;
	*width = raster->extents.width;
	*height = raster->extents.height;
    } break;

    case CAIRO_PATTERN_TYPE_SOLID:
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
    case CAIRO_PATTERN_TYPE_MESH:
    default:
	ASSERT_NOT_REACHED;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    _cairo_pattern_init_for_surface (image_pattern, &image->base);
    image_pattern->base.extend = pattern->extend;
    cairo_matrix_init_translate (&tm, x, y);
    status = cairo_matrix_invert (&tm);
    /* translation matrices are invertibile */
    assert (status == CAIRO_STATUS_SUCCESS);

    image_pattern->base.matrix = pattern->matrix;
    cairo_matrix_multiply (&image_pattern->base.matrix, &image_pattern->base.matrix, &tm);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_printing_surface_release_image_pattern (cairo_win32_printing_surface_t *surface,
						     const cairo_pattern_t          *pattern,
						     cairo_surface_pattern_t        *image_pattern,
						     void                           *image_extra)
{
    cairo_surface_t *surf = image_pattern->surface;

    _cairo_pattern_fini (&image_pattern->base);
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SURFACE: {
	cairo_surface_pattern_t *surf_pat = (cairo_surface_pattern_t *) pattern;
	cairo_image_surface_t *image = (cairo_image_surface_t *) surf;
	_cairo_surface_release_source_image (surf_pat->surface, image, image_extra);
    } break;

    case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
	_cairo_raster_source_pattern_release (pattern, surf);
	break;

    case CAIRO_PATTERN_TYPE_SOLID:
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
    case CAIRO_PATTERN_TYPE_MESH:
    default:
	ASSERT_NOT_REACHED;
	break;
    }
}

static cairo_int_status_t
analyze_surface_pattern_transparency (cairo_win32_printing_surface_t *surface,
				      const cairo_pattern_t          *pattern,
				      const cairo_rectangle_int_t    *extents)
{
    cairo_surface_pattern_t image_pattern;
    cairo_image_surface_t  *image;
    void		   *image_extra;
    cairo_int_status_t      status;
    cairo_image_transparency_t transparency;
    int pattern_width, pattern_height;

    status = _cairo_win32_printing_surface_acquire_image_pattern (surface,
								  pattern,
								  extents,
								  &image_pattern,
								  &pattern_width,
								  &pattern_height,
								  &image_extra);
    if (status)
	return status;

    image = (cairo_image_surface_t *)(image_pattern.surface);
    transparency = _cairo_image_analyze_transparency (image);
    switch (transparency) {
    case CAIRO_IMAGE_UNKNOWN:
	ASSERT_NOT_REACHED;
    case CAIRO_IMAGE_IS_OPAQUE:
	status = CAIRO_STATUS_SUCCESS;
	break;

    case CAIRO_IMAGE_HAS_BILEVEL_ALPHA:
    case CAIRO_IMAGE_HAS_ALPHA:
	status = CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY;
	break;
    }

    _cairo_win32_printing_surface_release_image_pattern (surface, pattern, &image_pattern, image_extra);

    return status;
}

static cairo_bool_t
surface_pattern_supported (const cairo_surface_pattern_t *pattern)
{
    if (_cairo_surface_is_recording (pattern->surface))
	return TRUE;

    if (pattern->surface->backend->acquire_source_image == NULL)
    {
	return FALSE;
    }

    return TRUE;
}

static cairo_bool_t
pattern_supported (cairo_win32_printing_surface_t *surface, const cairo_pattern_t *pattern)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return TRUE;

    case CAIRO_PATTERN_TYPE_LINEAR:
	return surface->win32.flags & CAIRO_WIN32_SURFACE_CAN_RECT_GRADIENT;

    case CAIRO_PATTERN_TYPE_RADIAL:
    case CAIRO_PATTERN_TYPE_MESH:
	return FALSE;

    case CAIRO_PATTERN_TYPE_SURFACE:
	return surface_pattern_supported ((cairo_surface_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
	return TRUE;

    default:
	ASSERT_NOT_REACHED;
	return FALSE;
    }
}

static cairo_int_status_t
_cairo_win32_printing_surface_analyze_operation (cairo_win32_printing_surface_t *surface,
                                                 cairo_operator_t                op,
                                                 const cairo_pattern_t          *pattern,
						 const cairo_rectangle_int_t    *extents)
{
    if (! pattern_supported (surface, pattern))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (!(op == CAIRO_OPERATOR_SOURCE ||
	  op == CAIRO_OPERATOR_OVER ||
	  op == CAIRO_OPERATOR_CLEAR))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) pattern;

	if (surface_pattern->surface->type == CAIRO_SURFACE_TYPE_RECORDING)
	    return CAIRO_INT_STATUS_ANALYZE_RECORDING_SURFACE_PATTERN;
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

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE || pattern->type == CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return analyze_surface_pattern_transparency (surface, pattern, extents);

    if (_cairo_pattern_is_opaque (pattern, NULL))
	return CAIRO_STATUS_SUCCESS;
    else
	return CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY;
}

static cairo_bool_t
_cairo_win32_printing_surface_operation_supported (cairo_win32_printing_surface_t *surface,
                                                   cairo_operator_t                op,
                                                   const cairo_pattern_t          *pattern,
						   const cairo_rectangle_int_t    *extents)
{
    if (_cairo_win32_printing_surface_analyze_operation (surface, op, pattern, extents) != CAIRO_INT_STATUS_UNSUPPORTED)
	return TRUE;
    else
	return FALSE;
}

static void
_cairo_win32_printing_surface_init_clear_color (cairo_win32_printing_surface_t *surface,
						cairo_solid_pattern_t *color)
{
    if (surface->content == CAIRO_CONTENT_COLOR_ALPHA)
	_cairo_pattern_init_solid (color, CAIRO_COLOR_WHITE);
    else
	_cairo_pattern_init_solid (color, CAIRO_COLOR_BLACK);
}

static COLORREF
_cairo_win32_printing_surface_flatten_transparency (cairo_win32_printing_surface_t *surface,
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
_cairo_win32_printing_surface_select_solid_brush (cairo_win32_printing_surface_t *surface,
                                                  const cairo_pattern_t *source)
{
    cairo_solid_pattern_t *pattern = (cairo_solid_pattern_t *) source;
    COLORREF color;

    color = _cairo_win32_printing_surface_flatten_transparency (surface,
								&pattern->color);
    surface->brush = CreateSolidBrush (color);
    if (!surface->brush)
	return _cairo_win32_print_gdi_error ("_cairo_win32_surface_select_solid_brush(CreateSolidBrush)");
    surface->old_brush = SelectObject (surface->win32.dc, surface->brush);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_printing_surface_done_solid_brush (cairo_win32_printing_surface_t *surface)
{
    if (surface->old_brush) {
	SelectObject (surface->win32.dc, surface->old_brush);
	DeleteObject (surface->brush);
	surface->old_brush = NULL;
    }
}

static cairo_status_t
_cairo_win32_printing_surface_get_ctm_clip_box (cairo_win32_printing_surface_t *surface,
						RECT                  *clip)
{
    XFORM xform;

    _cairo_matrix_to_win32_xform (&surface->ctm, &xform);
    if (!ModifyWorldTransform (surface->win32.dc, &xform, MWT_LEFTMULTIPLY))
	return _cairo_win32_print_gdi_error ("_cairo_win32_printing_surface_get_clip_box:ModifyWorldTransform");
    GetClipBox (surface->win32.dc, clip);

    _cairo_matrix_to_win32_xform (&surface->gdi_ctm, &xform);
    if (!SetWorldTransform (surface->win32.dc, &xform))
	return _cairo_win32_print_gdi_error ("_cairo_win32_printing_surface_get_clip_box:SetWorldTransform");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_paint_solid_pattern (cairo_win32_printing_surface_t *surface,
                                                   const cairo_pattern_t *pattern)
{
    RECT clip;
    cairo_status_t status;

    GetClipBox (surface->win32.dc, &clip);
    status = _cairo_win32_printing_surface_select_solid_brush (surface, pattern);
    if (status)
	return status;

    FillRect (surface->win32.dc, &clip, surface->brush);
    _cairo_win32_printing_surface_done_solid_brush (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_paint_recording_pattern (cairo_win32_printing_surface_t   *surface,
						       cairo_surface_pattern_t *pattern)
{
    cairo_content_t old_content;
    cairo_matrix_t old_ctm;
    cairo_bool_t old_has_ctm;
    cairo_rectangle_int_t recording_extents;
    cairo_int_status_t status;
    cairo_extend_t extend;
    cairo_matrix_t p2d;
    XFORM xform;
    int x_tile, y_tile, left, right, top, bottom;
    RECT clip;
    cairo_recording_surface_t *recording_surface = (cairo_recording_surface_t *) pattern->surface;
    cairo_box_t bbox;
    cairo_surface_t *free_me = NULL;
    cairo_bool_t is_subsurface;

    extend = cairo_pattern_get_extend (&pattern->base);

    p2d = pattern->base.matrix;
    status = cairo_matrix_invert (&p2d);
    /* _cairo_pattern_set_matrix guarantees invertibility */
    assert (status == CAIRO_INT_STATUS_SUCCESS);

    old_ctm = surface->ctm;
    old_has_ctm = surface->has_ctm;
    cairo_matrix_multiply (&p2d, &p2d, &surface->ctm);
    surface->ctm = p2d;
    SaveDC (surface->win32.dc);
    _cairo_matrix_to_win32_xform (&p2d, &xform);

    if (_cairo_surface_is_snapshot (&recording_surface->base)) {
	free_me = _cairo_surface_snapshot_get_target (&recording_surface->base);
	recording_surface = (cairo_recording_surface_t *) free_me;
    }

    if (recording_surface->base.backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	cairo_surface_subsurface_t *sub = (cairo_surface_subsurface_t *) recording_surface;

	recording_surface = (cairo_recording_surface_t *) (sub->target);
	recording_extents = sub->extents;
	is_subsurface = TRUE;
    } else {
	status = _cairo_recording_surface_get_bbox (recording_surface, &bbox, NULL);
	if (status)
	    goto err;

	_cairo_box_round_to_rectangle (&bbox, &recording_extents);
    }

    status = _cairo_win32_printing_surface_get_ctm_clip_box (surface, &clip);
    if (status)
	goto err;

    if (extend == CAIRO_EXTEND_REPEAT || extend == CAIRO_EXTEND_REFLECT) {
	left = floor (clip.left / _cairo_fixed_to_double (bbox.p2.x - bbox.p1.x));
	right = ceil (clip.right / _cairo_fixed_to_double (bbox.p2.x - bbox.p1.x));
	top = floor (clip.top / _cairo_fixed_to_double (bbox.p2.y - bbox.p1.y));
	bottom = ceil (clip.bottom / _cairo_fixed_to_double (bbox.p2.y - bbox.p1.y));
    } else {
	left = 0;
	right = 1;
	top = 0;
	bottom = 1;
    }

    old_content = surface->content;
    if (recording_surface->base.content == CAIRO_CONTENT_COLOR) {
	surface->content = CAIRO_CONTENT_COLOR;
	status = _cairo_win32_printing_surface_paint_solid_pattern (surface,
								    &_cairo_pattern_black.base);
	if (status)
	    goto err;
    }

    for (y_tile = top; y_tile < bottom; y_tile++) {
	for (x_tile = left; x_tile < right; x_tile++) {
	    cairo_matrix_t m;
	    double x, y;

	    SaveDC (surface->win32.dc);
	    m = p2d;
	    cairo_matrix_translate (&m,
				    x_tile*recording_extents.width,
				    y_tile*recording_extents.height);
	    if (extend == CAIRO_EXTEND_REFLECT) {
		if (x_tile % 2) {
		    cairo_matrix_translate (&m, recording_extents.width, 0);
		    cairo_matrix_scale (&m, -1, 1);
		}
		if (y_tile % 2) {
		    cairo_matrix_translate (&m, 0, recording_extents.height);
		    cairo_matrix_scale (&m, 1, -1);
		}
	    }
	    surface->ctm = m;
	    surface->has_ctm = !_cairo_matrix_is_identity (&surface->ctm);

	    /* Set clip path around bbox of the pattern. */
	    BeginPath (surface->win32.dc);

	    x = 0;
	    y = 0;
	    cairo_matrix_transform_point (&surface->ctm, &x, &y);
	    MoveToEx (surface->win32.dc, (int) x, (int) y, NULL);

	    x = recording_extents.width;
	    y = 0;
	    cairo_matrix_transform_point (&surface->ctm, &x, &y);
	    LineTo (surface->win32.dc, (int) x, (int) y);

	    x = recording_extents.width;
	    y = recording_extents.height;
	    cairo_matrix_transform_point (&surface->ctm, &x, &y);
	    LineTo (surface->win32.dc, (int) x, (int) y);

	    x = 0;
	    y = recording_extents.height;
	    cairo_matrix_transform_point (&surface->ctm, &x, &y);
	    LineTo (surface->win32.dc, (int) x, (int) y);

	    CloseFigure (surface->win32.dc);
	    EndPath (surface->win32.dc);
	    SelectClipPath (surface->win32.dc, RGN_AND);

	    SaveDC (surface->win32.dc); /* Allow clip path to be reset during replay */
	    status = _cairo_recording_surface_replay_region (&recording_surface->base,
							     is_subsurface ? &recording_extents : NULL,
							     &surface->win32.base,
							     CAIRO_RECORDING_REGION_NATIVE);
	    assert (status != CAIRO_INT_STATUS_UNSUPPORTED);
	    /* Restore both the clip save and our earlier path SaveDC */
	    RestoreDC (surface->win32.dc, -2);

	    if (status)
		goto err;
	}
    }

    surface->content = old_content;
    surface->ctm = old_ctm;
    surface->has_ctm = old_has_ctm;
    RestoreDC (surface->win32.dc, -1);

  err:
    cairo_surface_destroy (free_me);
    return status;
}

static cairo_int_status_t
_cairo_win32_printing_surface_check_jpeg (cairo_win32_printing_surface_t   *surface,
					  cairo_surface_t         *source,
					  const unsigned char    **data,
					  unsigned long           *length,
					  cairo_image_info_t      *info)
{
    const unsigned char *mime_data;
    unsigned long mime_data_length;
    cairo_int_status_t status;
    DWORD result;

    if (!(surface->win32.flags & CAIRO_WIN32_SURFACE_CAN_CHECK_JPEG))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    cairo_surface_get_mime_data (source, CAIRO_MIME_TYPE_JPEG,
				 &mime_data, &mime_data_length);
    if (mime_data == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_image_info_get_jpeg_info (info, mime_data, mime_data_length);
    if (status)
	return status;

    result = 0;
    if (ExtEscape(surface->win32.dc, CHECKJPEGFORMAT, mime_data_length, (char *) mime_data,
		  sizeof(result), (char *) &result) <= 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (result != 1)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    *data = mime_data;
    *length = mime_data_length;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_win32_printing_surface_check_png (cairo_win32_printing_surface_t   *surface,
					 cairo_surface_t         *source,
					 const unsigned char    **data,
					 unsigned long           *length,
					 cairo_image_info_t      *info)
{
    const unsigned char *mime_data;
    unsigned long mime_data_length;

    cairo_int_status_t status;
    DWORD result;

    if (!(surface->win32.flags & CAIRO_WIN32_SURFACE_CAN_CHECK_PNG))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    cairo_surface_get_mime_data (source, CAIRO_MIME_TYPE_PNG,
				 &mime_data, &mime_data_length);
    if (mime_data == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_image_info_get_png_info (info, mime_data, mime_data_length);
    if (status)
	return status;

    result = 0;
    if (ExtEscape(surface->win32.dc, CHECKPNGFORMAT, mime_data_length, (char *) mime_data,
		  sizeof(result), (char *) &result) <= 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (result != 1)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    *data = mime_data;
    *length = mime_data_length;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_paint_image_pattern (cairo_win32_printing_surface_t *surface,
						   const cairo_pattern_t          *pattern,
						   const cairo_rectangle_int_t    *extents)
{
    cairo_int_status_t status;
    cairo_surface_pattern_t image_pattern;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_image_surface_t *opaque_image = NULL;
    BITMAPINFO bi;
    cairo_matrix_t m;
    int oldmode;
    XFORM xform;
    int x_tile, y_tile, left, right, top, bottom;
    int pattern_width, pattern_height;
    RECT clip;
    const cairo_color_t *background_color;
    const unsigned char *mime_data;
    unsigned long mime_size;
    cairo_image_info_t mime_info;
    cairo_bool_t use_mime;
    DWORD mime_type;

    /* If we can't use StretchDIBits with this surface, we can't do anything
     * here.
     */
    if (!(surface->win32.flags & CAIRO_WIN32_SURFACE_CAN_STRETCHDIB))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (surface->content == CAIRO_CONTENT_COLOR_ALPHA)
	background_color = CAIRO_COLOR_WHITE;
    else
	background_color = CAIRO_COLOR_BLACK;

    status = _cairo_win32_printing_surface_acquire_image_pattern (surface,
								  pattern,
								  extents,
								  &image_pattern,
								  &pattern_width,
								  &pattern_height,
								  &image_extra);
    if (status)
	return status;

    image = (cairo_image_surface_t *)(image_pattern.surface);
    if (image->base.status) {
	status = image->base.status;
	goto CLEANUP_IMAGE;
    }

    if (image->width == 0 || image->height == 0) {
	status = CAIRO_STATUS_SUCCESS;
	goto CLEANUP_IMAGE;
    }

    mime_type = BI_JPEG;
    status = _cairo_win32_printing_surface_check_jpeg (surface,
						       image_pattern.surface,
						       &mime_data,
						       &mime_size,
						       &mime_info);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	mime_type = BI_PNG;
	status = _cairo_win32_printing_surface_check_png (surface,
							  image_pattern.surface,
							  &mime_data,
							  &mime_size,
							  &mime_info);
    }
    if (_cairo_int_status_is_error (status))
	return status;

    use_mime = (status == CAIRO_INT_STATUS_SUCCESS);

    if (!use_mime && image->format != CAIRO_FORMAT_RGB24) {
	cairo_surface_t *opaque_surface;
	cairo_surface_pattern_t image_pattern;
	cairo_solid_pattern_t background_pattern;

	opaque_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
						     image->width,
						     image->height);
	if (opaque_surface->status) {
	    status = opaque_surface->status;
	    goto CLEANUP_OPAQUE_IMAGE;
	}

	_cairo_pattern_init_solid (&background_pattern,
				   background_color);
	status = _cairo_surface_paint (opaque_surface,
				       CAIRO_OPERATOR_SOURCE,
				       &background_pattern.base,
				       NULL);
	if (status)
	    goto CLEANUP_OPAQUE_IMAGE;

	_cairo_pattern_init_for_surface (&image_pattern, &image->base);
	status = _cairo_surface_paint (opaque_surface,
				       CAIRO_OPERATOR_OVER,
				       &image_pattern.base,
				       NULL);
	_cairo_pattern_fini (&image_pattern.base);
	if (status)
	    goto CLEANUP_OPAQUE_IMAGE;

	opaque_image = (cairo_image_surface_t *) opaque_surface;
    } else {
	opaque_image = image;
    }

    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = use_mime ? mime_info.width : opaque_image->width;
    bi.bmiHeader.biHeight = use_mime ? - mime_info.height : -opaque_image->height;
    bi.bmiHeader.biSizeImage = use_mime ? mime_size : 0;
    bi.bmiHeader.biXPelsPerMeter = PELS_72DPI;
    bi.bmiHeader.biYPelsPerMeter = PELS_72DPI;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = use_mime ? mime_type : BI_RGB;
    bi.bmiHeader.biClrUsed = 0;
    bi.bmiHeader.biClrImportant = 0;

    m = image_pattern.base.matrix;
    status = cairo_matrix_invert (&m);
    /* _cairo_pattern_set_matrix guarantees invertibility */
    assert (status == CAIRO_INT_STATUS_SUCCESS);

    cairo_matrix_multiply (&m, &m, &surface->ctm);
    cairo_matrix_multiply (&m, &m, &surface->gdi_ctm);
    SaveDC (surface->win32.dc);
    _cairo_matrix_to_win32_xform (&m, &xform);

    if (! SetWorldTransform (surface->win32.dc, &xform)) {
	status = _cairo_win32_print_gdi_error ("_cairo_win32_printing_surface_paint_image_pattern");
	goto CLEANUP_OPAQUE_IMAGE;
    }

    oldmode = SetStretchBltMode(surface->win32.dc, HALFTONE);

    GetClipBox (surface->win32.dc, &clip);
    if (pattern->extend == CAIRO_EXTEND_REPEAT || pattern->extend == CAIRO_EXTEND_REFLECT) {
	left = floor ( clip.left / (double) opaque_image->width);
	right = ceil (clip.right / (double) opaque_image->width);
	top = floor (clip.top / (double) opaque_image->height);
	bottom = ceil (clip.bottom / (double) opaque_image->height);
    } else {
	left = 0;
	right = 1;
	top = 0;
	bottom = 1;
    }

    for (y_tile = top; y_tile < bottom; y_tile++) {
	for (x_tile = left; x_tile < right; x_tile++) {
	    if (!StretchDIBits (surface->win32.dc,
				x_tile*opaque_image->width,
				y_tile*opaque_image->height,
				opaque_image->width,
				opaque_image->height,
				0,
				0,
				use_mime ? mime_info.width : opaque_image->width,
				use_mime ? mime_info.height : opaque_image->height,
				use_mime ? mime_data : opaque_image->data,
				&bi,
				DIB_RGB_COLORS,
				SRCCOPY))
	    {
		status = _cairo_win32_print_gdi_error ("_cairo_win32_printing_surface_paint(StretchDIBits)");
		goto CLEANUP_OPAQUE_IMAGE;
	    }
	}
    }
    SetStretchBltMode(surface->win32.dc, oldmode);
    RestoreDC (surface->win32.dc, -1);

CLEANUP_OPAQUE_IMAGE:
    if (opaque_image != image)
	cairo_surface_destroy (&opaque_image->base);
CLEANUP_IMAGE:
    _cairo_win32_printing_surface_release_image_pattern (surface, pattern, &image_pattern, image_extra);

    return status;
}

static void
vertex_set_color (TRIVERTEX *vert, cairo_color_stop_t *color)
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
_cairo_win32_printing_surface_paint_linear_pattern (cairo_win32_printing_surface_t *surface,
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
    SaveDC (surface->win32.dc);

    mat = pattern->base.base.matrix;
    status = cairo_matrix_invert (&mat);
    /* _cairo_pattern_set_matrix guarantees invertibility */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_matrix_multiply (&mat, &surface->ctm, &mat);

    p1x = pattern->pd1.x;
    p1y = pattern->pd1.y;
    p2x = pattern->pd2.x;
    p2y = pattern->pd2.y;
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

    if (!SetWorldTransform (surface->win32.dc, &xform))
	return _cairo_win32_print_gdi_error ("_win32_printing_surface_paint_linear_pattern:SetWorldTransform2");

    GetClipBox (surface->win32.dc, &clip);

    if (extend == CAIRO_EXTEND_REPEAT || extend == CAIRO_EXTEND_REFLECT) {
	range_start = floor (clip.left / d);
	range_stop = ceil (clip.right / d);
    } else {
	range_start = 0;
	range_stop = 1;
    }
    num_ranges = range_stop - range_start;
    num_stops = pattern->base.n_stops;
    num_rects = num_stops - 1;

    /* Add an extra four points and two rectangles for EXTEND_PAD */
    vert = _cairo_malloc (sizeof (TRIVERTEX) * (num_rects*2*num_ranges + 4));
    rect = _cairo_malloc (sizeof (GRADIENT_RECT) * (num_rects*num_ranges + 2));

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

    if (!GradientFill (surface->win32.dc,
		       vert, total_verts,
		       rect, total_rects,
		       GRADIENT_FILL_RECT_H))
	return _cairo_win32_print_gdi_error ("_win32_printing_surface_paint_linear_pattern:GradientFill");

    free (rect);
    free (vert);
    RestoreDC (surface->win32.dc, -1);

    return 0;
}

static cairo_int_status_t
_cairo_win32_printing_surface_paint_pattern (cairo_win32_printing_surface_t *surface,
                                             const cairo_pattern_t          *pattern,
					     const cairo_rectangle_int_t    *extents)
{
    cairo_status_t status;

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	status = _cairo_win32_printing_surface_paint_solid_pattern (surface, pattern);
	if (status)
	    return status;
	break;

    case CAIRO_PATTERN_TYPE_SURFACE: {
	cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) pattern;

	if ( _cairo_surface_is_recording (surface_pattern->surface))
	    status = _cairo_win32_printing_surface_paint_recording_pattern (surface, surface_pattern);
	else
	    status = _cairo_win32_printing_surface_paint_image_pattern (surface, pattern, extents);

	if (status)
	    return status;
	break;
    }
    case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
	status = _cairo_win32_printing_surface_paint_image_pattern (surface, pattern, extents);
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

    case CAIRO_PATTERN_TYPE_MESH:
	ASSERT_NOT_REACHED;
    }

    return CAIRO_STATUS_SUCCESS;
}

typedef struct _win32_print_path_info {
    cairo_win32_printing_surface_t *surface;
} win32_path_info_t;

static cairo_status_t
_cairo_win32_printing_surface_path_move_to (void *closure,
					    const cairo_point_t *point)
{
    win32_path_info_t *path_info = closure;

    if (path_info->surface->has_ctm) {
	double x, y;

	x = _cairo_fixed_to_double (point->x);
	y = _cairo_fixed_to_double (point->y);
	cairo_matrix_transform_point (&path_info->surface->ctm, &x, &y);
	MoveToEx (path_info->surface->win32.dc, (int) x, (int) y, NULL);
    } else {
	MoveToEx (path_info->surface->win32.dc,
		  _cairo_fixed_integer_part (point->x),
		  _cairo_fixed_integer_part (point->y),
		  NULL);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_path_line_to (void *closure,
					    const cairo_point_t *point)
{
    win32_path_info_t *path_info = closure;

    path_info->surface->path_empty = FALSE;
    if (path_info->surface->has_ctm) {
	double x, y;

	x = _cairo_fixed_to_double (point->x);
	y = _cairo_fixed_to_double (point->y);
	cairo_matrix_transform_point (&path_info->surface->ctm, &x, &y);
	LineTo (path_info->surface->win32.dc, (int) x, (int) y);
    } else {
	LineTo (path_info->surface->win32.dc,
		_cairo_fixed_integer_part (point->x),
		_cairo_fixed_integer_part (point->y));
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_path_curve_to (void          *closure,
                                             const cairo_point_t *b,
                                             const cairo_point_t *c,
                                             const cairo_point_t *d)
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
    PolyBezierTo (path_info->surface->win32.dc, points, 3);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_path_close_path (void *closure)
{
    win32_path_info_t *path_info = closure;

    CloseFigure (path_info->surface->win32.dc);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_emit_path (cairo_win32_printing_surface_t    *surface,
					 const cairo_path_fixed_t *path)
{
    win32_path_info_t path_info;

    path_info.surface = surface;
    return _cairo_path_fixed_interpret (path,
					_cairo_win32_printing_surface_path_move_to,
					_cairo_win32_printing_surface_path_line_to,
					_cairo_win32_printing_surface_path_curve_to,
					_cairo_win32_printing_surface_path_close_path,
					&path_info);
}

static cairo_int_status_t
_cairo_win32_printing_surface_show_page (void *abstract_surface)
{
    cairo_win32_printing_surface_t *surface = abstract_surface;

    /* Undo both SaveDC's that we did in start_page */
    RestoreDC (surface->win32.dc, -2);

    /* Invalidate extents since the size of the next page is not known at
     * this point.
     */
    surface->extents_valid = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_printing_surface_clipper_intersect_clip_path (cairo_surface_clipper_t *clipper,
                                                   cairo_path_fixed_t *path,
                                                   cairo_fill_rule_t   fill_rule,
                                                   double	       tolerance,
                                                   cairo_antialias_t   antialias)
{
    cairo_win32_printing_surface_t *surface = cairo_container_of (clipper,
							 cairo_win32_printing_surface_t,
							 clipper);
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_STATUS_SUCCESS;

    if (path == NULL) {
	RestoreDC (surface->win32.dc, -1);
	SaveDC (surface->win32.dc);

	return CAIRO_STATUS_SUCCESS;
    }

    BeginPath (surface->win32.dc);
    status = _cairo_win32_printing_surface_emit_path (surface, path);
    EndPath (surface->win32.dc);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	SetPolyFillMode (surface->win32.dc, WINDING);
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	SetPolyFillMode (surface->win32.dc, ALTERNATE);
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    SelectClipPath (surface->win32.dc, RGN_AND);

    return status;
}

static cairo_bool_t
_cairo_win32_printing_surface_get_extents (void		          *abstract_surface,
					   cairo_rectangle_int_t  *rectangle)
{
    cairo_win32_printing_surface_t *surface = abstract_surface;

    if (surface->extents_valid)
	*rectangle = surface->win32.extents;

    return surface->extents_valid;
}

static void
_cairo_win32_printing_surface_get_font_options (void                  *abstract_surface,
                                                cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_antialias (options, CAIRO_ANTIALIAS_GRAY);
    _cairo_font_options_set_round_glyph_positions (options, CAIRO_ROUND_GLYPH_POS_ON);
}

static cairo_int_status_t
_cairo_win32_printing_surface_paint (void			*abstract_surface,
                                     cairo_operator_t		 op,
                                     const cairo_pattern_t	*source,
				     const cairo_clip_t      *clip)
{
    cairo_win32_printing_surface_t *surface = abstract_surface;
    cairo_solid_pattern_t clear;
    cairo_composite_rectangles_t extents;
    cairo_status_t status;

    status = _cairo_composite_rectangles_init_for_paint (&extents,
							 &surface->win32.base,
							 op, source, clip);
    if (unlikely (status))
	return status;

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	goto cleanup_composite;

    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_win32_printing_surface_init_clear_color (surface, &clear);
	source = (cairo_pattern_t*) &clear;
	op = CAIRO_OPERATOR_SOURCE;
    }

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	status = _cairo_win32_printing_surface_analyze_operation (surface, op, source, &extents.bounded);
	goto cleanup_composite;
    }

    assert (_cairo_win32_printing_surface_operation_supported (surface, op, source, &extents.bounded));

    status = _cairo_win32_printing_surface_paint_pattern (surface, source, &extents.bounded);

  cleanup_composite:
    _cairo_composite_rectangles_fini (&extents);
    return status;
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
_cairo_win32_printing_surface_stroke (void			*abstract_surface,
                                      cairo_operator_t		 op,
                                      const cairo_pattern_t	*source,
                                      const cairo_path_fixed_t	*path,
                                      const cairo_stroke_style_t *style,
                                      const cairo_matrix_t	*stroke_ctm,
                                      const cairo_matrix_t	*stroke_ctm_inverse,
                                      double			tolerance,
                                      cairo_antialias_t		antialias,
				      const cairo_clip_t    *clip)
{
    cairo_win32_printing_surface_t *surface = abstract_surface;
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
    cairo_composite_rectangles_t extents;

    status = _cairo_composite_rectangles_init_for_stroke (&extents,
							  &surface->win32.base,
							  op, source,
							  path, style, stroke_ctm,
							  clip);
    if (unlikely (status))
	return status;

    /* use the more accurate extents */
    {
	cairo_rectangle_int_t r;
	cairo_box_t b;

	status = _cairo_path_fixed_stroke_extents (path, style,
						   stroke_ctm, stroke_ctm_inverse,
						   tolerance,
						   &r);
	if (unlikely (status))
	    goto cleanup_composite;

	_cairo_box_from_rectangle (&b, &r);
	status = _cairo_composite_rectangles_intersect_mask_extents (&extents, &b);
	if (unlikely (status))
	    goto cleanup_composite;
    }

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	goto cleanup_composite;

    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_win32_printing_surface_init_clear_color (surface, &clear);
	source = (cairo_pattern_t*) &clear;
	op = CAIRO_OPERATOR_SOURCE;
    }

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	/* Win32 does not support a dash offset. */
	if (style->num_dashes > 0 && style->dash_offset != 0.0)
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
	else
	    status = _cairo_win32_printing_surface_analyze_operation (surface, op, source, &extents.bounded);

	goto cleanup_composite;
    }

    assert (_cairo_win32_printing_surface_operation_supported (surface, op, source, &extents.bounded));
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

    SetMiterLimit (surface->win32.dc, (FLOAT) (style->miter_limit), NULL);
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
    if (pen == NULL) {
	status = _cairo_win32_print_gdi_error ("_win32_surface_stroke:ExtCreatePen");
	goto cleanup_composite;
    }

    obj = SelectObject (surface->win32.dc, pen);
    if (obj == NULL) {
	status = _cairo_win32_print_gdi_error ("_win32_surface_stroke:SelectObject");
	goto cleanup_composite;
    }

    BeginPath (surface->win32.dc);
    status = _cairo_win32_printing_surface_emit_path (surface, path);
    EndPath (surface->win32.dc);
    if (unlikely (status))
	goto cleanup_composite;

    /*
     * Switch to user space to set line parameters
     */
    SaveDC (surface->win32.dc);

    _cairo_matrix_to_win32_xform (&mat, &xform);
    xform.eDx = 0.0f;
    xform.eDy = 0.0f;

    if (!ModifyWorldTransform (surface->win32.dc, &xform, MWT_LEFTMULTIPLY)) {
	status = _cairo_win32_print_gdi_error ("_win32_surface_stroke:SetWorldTransform");
	goto cleanup_composite;
    }

    if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	StrokePath (surface->win32.dc);
    } else {
	if (!WidenPath (surface->win32.dc)) {
	    status = _cairo_win32_print_gdi_error ("_win32_surface_stroke:WidenPath");
	    goto cleanup_composite;
	}
	if (!SelectClipPath (surface->win32.dc, RGN_AND)) {
	    status = _cairo_win32_print_gdi_error ("_win32_surface_stroke:SelectClipPath");
	    goto cleanup_composite;
	}

	/* Return to device space to paint the pattern */
	_cairo_matrix_to_win32_xform (&surface->gdi_ctm, &xform);
	if (!SetWorldTransform (surface->win32.dc, &xform)) {
	    status = _cairo_win32_print_gdi_error ("_win32_surface_stroke:ModifyWorldTransform");
	    goto cleanup_composite;
	}
	status = _cairo_win32_printing_surface_paint_pattern (surface, source, &extents.bounded);
    }
    RestoreDC (surface->win32.dc, -1);
    DeleteObject (pen);
    free (dash_array);

cleanup_composite:
    _cairo_composite_rectangles_fini (&extents);
    return status;
}

static cairo_int_status_t
_cairo_win32_printing_surface_fill (void		        *abstract_surface,
				    cairo_operator_t		 op,
				    const cairo_pattern_t	*source,
				    const cairo_path_fixed_t	*path,
				    cairo_fill_rule_t		 fill_rule,
				    double			 tolerance,
				    cairo_antialias_t		 antialias,
				    const cairo_clip_t		*clip)
{
    cairo_win32_printing_surface_t *surface = abstract_surface;
    cairo_int_status_t status;
    cairo_solid_pattern_t clear;
    cairo_composite_rectangles_t extents;

    status = _cairo_composite_rectangles_init_for_fill (&extents,
							&surface->win32.base,
							op, source, path,
							clip);
    if (unlikely (status))
	return status;

    /* use the more accurate extents */
    {
	cairo_rectangle_int_t r;
	cairo_box_t b;

	_cairo_path_fixed_fill_extents (path,
					fill_rule,
					tolerance,
					&r);

	_cairo_box_from_rectangle (&b, &r);
	status = _cairo_composite_rectangles_intersect_mask_extents (&extents, &b);
	if (unlikely (status))
	    goto cleanup_composite;
    }

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	goto cleanup_composite;

    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_win32_printing_surface_init_clear_color (surface, &clear);
	source = (cairo_pattern_t*) &clear;
	op = CAIRO_OPERATOR_SOURCE;
    }

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	status = _cairo_win32_printing_surface_analyze_operation (surface, op, source, &extents.bounded);
	goto cleanup_composite;
    }

    assert (_cairo_win32_printing_surface_operation_supported (surface, op, source, &extents.bounded));

    surface->path_empty = TRUE;
    BeginPath (surface->win32.dc);
    status = _cairo_win32_printing_surface_emit_path (surface, path);
    EndPath (surface->win32.dc);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	SetPolyFillMode (surface->win32.dc, WINDING);
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	SetPolyFillMode (surface->win32.dc, ALTERNATE);
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	status = _cairo_win32_printing_surface_select_solid_brush (surface, source);
	if (unlikely (status))
	    goto cleanup_composite;

	FillPath (surface->win32.dc);
	_cairo_win32_printing_surface_done_solid_brush (surface);
    } else if (surface->path_empty == FALSE) {
	SaveDC (surface->win32.dc);
	SelectClipPath (surface->win32.dc, RGN_AND);
	status = _cairo_win32_printing_surface_paint_pattern (surface, source, &extents.bounded);
	RestoreDC (surface->win32.dc, -1);
    }

    fflush(stderr);

cleanup_composite:
    _cairo_composite_rectangles_fini (&extents);
    return status;
}


static cairo_int_status_t
_cairo_win32_printing_surface_emit_win32_glyphs (cairo_win32_printing_surface_t	*surface,
						 cairo_operator_t	 op,
						 const cairo_pattern_t  *source,
						 cairo_glyph_t		 *glyphs,
						 int			 num_glyphs,
						 cairo_scaled_font_t	*scaled_font,
						 const cairo_clip_t	*clip)
{
    cairo_matrix_t ctm;
    cairo_glyph_t  *unicode_glyphs;
    cairo_scaled_font_subsets_glyph_t subset_glyph;
    int i, first;
    cairo_bool_t sequence_is_unicode;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    /* Where possible reverse the glyph indices back to unicode
     * characters. Strings of glyphs that could not be reversed to
     * unicode will be printed with ETO_GLYPH_INDEX.
     *
     * As _cairo_win32_scaled_font_index_to_ucs4() is a slow
     * operation, the font subsetting function
     * _cairo_scaled_font_subsets_map_glyph() is used to obtain
     * the unicode value because it caches the reverse mapping in
     * the subsets.
     */

    if (surface->has_ctm) {
	for (i = 0; i < num_glyphs; i++)
	    cairo_matrix_transform_point (&surface->ctm, &glyphs[i].x, &glyphs[i].y);
	cairo_matrix_multiply (&ctm, &scaled_font->ctm, &surface->ctm);
	scaled_font = cairo_scaled_font_create (scaled_font->font_face,
						&scaled_font->font_matrix,
						&ctm,
						&scaled_font->options);
    }

    unicode_glyphs = _cairo_malloc_ab (num_glyphs, sizeof (cairo_glyph_t));
    if (unicode_glyphs == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    memcpy (unicode_glyphs, glyphs, num_glyphs * sizeof (cairo_glyph_t));
    for (i = 0; i < num_glyphs; i++) {
	status = _cairo_scaled_font_subsets_map_glyph (surface->font_subsets,
						       scaled_font,
						       glyphs[i].index,
						       NULL, 0,
						       &subset_glyph);
	if (status)
	    goto fail;

	unicode_glyphs[i].index = subset_glyph.unicode;
    }

    i = 0;
    first = 0;
    sequence_is_unicode = unicode_glyphs[0].index <= 0xffff;
    while (i < num_glyphs) {
	if (i == num_glyphs - 1 ||
	    ((unicode_glyphs[i + 1].index < 0xffff) != sequence_is_unicode))
	{
	    status = _cairo_win32_surface_emit_glyphs (&surface->win32,
						       source,
						       sequence_is_unicode ? &unicode_glyphs[first] : &glyphs[first],
						       i - first + 1,
						       scaled_font,
						       ! sequence_is_unicode);
	    first = i + 1;
	    if (i < num_glyphs - 1)
		sequence_is_unicode = unicode_glyphs[i + 1].index <= 0xffff;
	}
	i++;
    }

fail:
    if (surface->has_ctm)
	cairo_scaled_font_destroy (scaled_font);

    free (unicode_glyphs);

    return status;
}

static cairo_int_status_t
_cairo_win32_printing_surface_show_glyphs (void                 *abstract_surface,
                                           cairo_operator_t	 op,
                                           const cairo_pattern_t *source,
                                           cairo_glyph_t        *glyphs,
                                           int			 num_glyphs,
                                           cairo_scaled_font_t  *scaled_font,
					   const cairo_clip_t	*clip)
{
    cairo_win32_printing_surface_t *surface = abstract_surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_pattern_t *opaque = NULL;
    int i;
    cairo_matrix_t old_ctm;
    cairo_bool_t old_has_ctm;
    cairo_solid_pattern_t clear;
    cairo_composite_rectangles_t extents;
    cairo_bool_t overlap;
    cairo_scaled_font_t *local_scaled_font = NULL;

    status = _cairo_composite_rectangles_init_for_glyphs (&extents,
							  &surface->win32.base,
							  op, source,
							  scaled_font,
							  glyphs, num_glyphs,
							  clip,
							  &overlap);
    if (unlikely (status))
	return status;

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	goto cleanup_composite;

    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_win32_printing_surface_init_clear_color (surface, &clear);
	source = (cairo_pattern_t*) &clear;
	op = CAIRO_OPERATOR_SOURCE;
    }

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	/* When printing bitmap fonts to a printer DC, Windows may
	 * substitute an outline font for bitmap font. As the win32
	 * font backend always uses a screen DC when obtaining the
	 * font metrics the metrics of the substituted font will not
	 * match the metrics that the win32 font backend returns.
	 *
	 * If we are printing a bitmap font, use fallback images to
	 * ensure the font is not substituted.
	 */
#if CAIRO_HAS_WIN32_FONT
	if (cairo_scaled_font_get_type (scaled_font) == CAIRO_FONT_TYPE_WIN32) {
	    if (_cairo_win32_scaled_font_is_bitmap (scaled_font)) {
		status = CAIRO_INT_STATUS_UNSUPPORTED;
		goto cleanup_composite;
	    } else {
		status = _cairo_win32_printing_surface_analyze_operation (surface, op, source, &extents.bounded);
		goto cleanup_composite;
	    }
	}
#endif

#if CAIRO_HAS_DWRITE_FONT
        if (cairo_scaled_font_get_type (scaled_font) == CAIRO_FONT_TYPE_DWRITE) {
            status = _cairo_win32_printing_surface_analyze_operation (surface, op, source, &extents.bounded);
            goto cleanup_composite;
        }
#endif

	/* For non win32 fonts we need to check that each glyph has a
	 * path available. If a path is not available,
	 * _cairo_scaled_glyph_lookup() will return
	 * CAIRO_INT_STATUS_UNSUPPORTED and a fallback image will be
	 * used.
	 */
        _cairo_scaled_font_freeze_cache (scaled_font);
	for (i = 0; i < num_glyphs; i++) {
	    status = _cairo_scaled_glyph_lookup (scaled_font,
						 glyphs[i].index,
						 CAIRO_SCALED_GLYPH_INFO_PATH,
						 &scaled_glyph);
	    if (status)
                break;
	}
        _cairo_scaled_font_thaw_cache (scaled_font);
	if (unlikely (status))
	    goto cleanup_composite;

	status = _cairo_win32_printing_surface_analyze_operation (surface, op, source, &extents.bounded);
	goto cleanup_composite;
    }

    if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) source;
	COLORREF color;

	color = _cairo_win32_printing_surface_flatten_transparency (surface,
								    &solid->color);
	opaque = cairo_pattern_create_rgb (GetRValue (color) / 255.0,
					   GetGValue (color) / 255.0,
					   GetBValue (color) / 255.0);
	if (unlikely (opaque->status)) {
	    status = opaque->status;
	    goto cleanup_composite;
	}
	source = opaque;
    }

#if CAIRO_HAS_DWRITE_FONT
    /* For a printer, the dwrite path is not desirable as it goes through the
     * bitmap-blitting GDI interop route. Better to create a win32 (GDI) font
     * so that ExtTextOut can be used, giving the printer driver the chance
     * to do the right thing with the text.
     */
    if (cairo_scaled_font_get_type (scaled_font) == CAIRO_FONT_TYPE_DWRITE) {
        status = _cairo_dwrite_scaled_font_create_win32_scaled_font (scaled_font, &local_scaled_font);
        if (status == CAIRO_STATUS_SUCCESS) {
            scaled_font = local_scaled_font;
        } else {
            /* Reset status; we'll fall back to drawing glyphs as paths */
            status = CAIRO_STATUS_SUCCESS;
        }
    }
#endif

#if CAIRO_HAS_WIN32_FONT
    if (cairo_scaled_font_get_type (scaled_font) == CAIRO_FONT_TYPE_WIN32 &&
	source->type == CAIRO_PATTERN_TYPE_SOLID)
    {
	status = _cairo_win32_printing_surface_emit_win32_glyphs (surface,
								  op,
								  source,
								  glyphs,
								  num_glyphs,
								  scaled_font,
								  clip);
	goto cleanup_composite;
    }
#endif

    SaveDC (surface->win32.dc);
    old_ctm = surface->ctm;
    old_has_ctm = surface->has_ctm;
    surface->has_ctm = TRUE;
    surface->path_empty = TRUE;
    _cairo_scaled_font_freeze_cache (scaled_font);
    BeginPath (surface->win32.dc);
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
    EndPath (surface->win32.dc);
    _cairo_scaled_font_thaw_cache (scaled_font);
    surface->ctm = old_ctm;
    surface->has_ctm = old_has_ctm;
    if (status == CAIRO_STATUS_SUCCESS && surface->path_empty == FALSE) {
	if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	    status = _cairo_win32_printing_surface_select_solid_brush (surface, source);
	    if (unlikely (status))
		goto cleanup_composite;

	    SetPolyFillMode (surface->win32.dc, WINDING);
	    FillPath (surface->win32.dc);
	    _cairo_win32_printing_surface_done_solid_brush (surface);
	} else {
	    SelectClipPath (surface->win32.dc, RGN_AND);
	    status = _cairo_win32_printing_surface_paint_pattern (surface, source, &extents.bounded);
	}
    }
    RestoreDC (surface->win32.dc, -1);

    if (opaque)
	cairo_pattern_destroy (opaque);

cleanup_composite:
    _cairo_composite_rectangles_fini (&extents);

    if (local_scaled_font)
        cairo_scaled_font_destroy (local_scaled_font);

    return status;
}

static const char **
_cairo_win32_printing_surface_get_supported_mime_types (void	  *abstract_surface)
{
    return _cairo_win32_printing_supported_mime_types;
}

static cairo_status_t
_cairo_win32_printing_surface_finish (void *abstract_surface)
{
    cairo_win32_printing_surface_t *surface = abstract_surface;

    if (surface->font_subsets != NULL)
	_cairo_scaled_font_subsets_destroy (surface->font_subsets);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_win32_printing_surface_create_similar (void		*abstract_surface,
					      cairo_content_t	 content,
					      int		 width,
					      int		 height)
{
    cairo_rectangle_t extents;

    extents.x = extents.y = 0;
    extents.width  = width;
    extents.height = height;
    return cairo_recording_surface_create (content, &extents);
}

static cairo_int_status_t
_cairo_win32_printing_surface_start_page (void *abstract_surface)
{
    cairo_win32_printing_surface_t *surface = abstract_surface;
    XFORM xform;
    double x_res, y_res;
    cairo_matrix_t inverse_ctm;
    cairo_status_t status;
    RECT rect;

    /* Since the page size may be changed after _show_page() and before the
     * next drawing command, the extents are set in _start_page() and invalidated
     * in _show_page(). The paginated surface will obtain the extents immediately
     * after calling _show_page() and before any drawing commands. At this point
     * the next page will not have been setup on the DC so we return invalid
     * extents and the paginated surface will create an unbounded recording surface.
     * Prior to replay of the record surface, the paginated surface will call
     * _start_page and we setup the correct extents.
     *
     * Note that we always set the extents x,y to 0 so prevent replay from translating
     * the coordinates of objects. Windows will clip anything outside of the page clip
     * area.
     */
    GetClipBox(surface->win32.dc, &rect);
    surface->win32.extents.x = 0;
    surface->win32.extents.y = 0;
    surface->win32.extents.width = rect.right;
    surface->win32.extents.height = rect.bottom;
    surface->extents_valid = TRUE;

    SaveDC (surface->win32.dc); /* Save application context first, before doing MWT */

    /* As the logical coordinates used by GDI functions (eg LineTo)
     * are integers we need to do some additional work to prevent
     * rounding errors. For example the obvious way to paint a recording
     * pattern is to:
     *
     *   SaveDC()
     *   transform the device context DC by the pattern to device matrix
     *   replay the recording surface
     *   RestoreDC()
     *
     * The problem here is that if the pattern to device matrix is
     * [100 0 0 100 0 0], coordinates in the recording pattern such as
     * (1.56, 2.23) which correspond to (156, 223) in device space
     * will be rounded to (100, 200) due to (1.56, 2.23) being
     * truncated to integers.
     *
     * This is solved by saving the current GDI CTM in surface->ctm,
     * switch the GDI CTM to identity, and transforming all
     * coordinates by surface->ctm before passing them to GDI. When
     * painting a recording pattern, surface->ctm is transformed by the
     * pattern to device matrix.
     *
     * For printing device contexts where 1 unit is 1 dpi, switching
     * the GDI CTM to identity maximises the possible resolution of
     * coordinates.
     *
     * If the device context is an EMF file, using an identity
     * transform often provides insufficient resolution. The workaround
     * is to set the GDI CTM to a scale < 1 eg [1.0/16 0 0 1/0/16 0 0]
     * and scale the cairo CTM by [16 0 0 16 0 0]. The
     * SetWorldTransform function call to scale the GDI CTM by 1.0/16
     * will be recorded in the EMF followed by all the graphics
     * functions by their coordinateds multiplied by 16.
     *
     * To support allowing the user to set a GDI CTM with scale < 1,
     * we avoid switching to an identity CTM if the CTM xx and yy is < 1.
     */
    SetGraphicsMode (surface->win32.dc, GM_ADVANCED);
    GetWorldTransform(surface->win32.dc, &xform);
    if (xform.eM11 < 1 && xform.eM22 < 1) {
	cairo_matrix_init_identity (&surface->ctm);
	surface->gdi_ctm.xx = xform.eM11;
	surface->gdi_ctm.xy = xform.eM21;
	surface->gdi_ctm.yx = xform.eM12;
	surface->gdi_ctm.yy = xform.eM22;
	surface->gdi_ctm.x0 = xform.eDx;
	surface->gdi_ctm.y0 = xform.eDy;
    } else {
	surface->ctm.xx = xform.eM11;
	surface->ctm.xy = xform.eM21;
	surface->ctm.yx = xform.eM12;
	surface->ctm.yy = xform.eM22;
	surface->ctm.x0 = xform.eDx;
	surface->ctm.y0 = xform.eDy;
	cairo_matrix_init_identity (&surface->gdi_ctm);
	if (!ModifyWorldTransform (surface->win32.dc, NULL, MWT_IDENTITY))
	    return _cairo_win32_print_gdi_error ("_cairo_win32_printing_surface_start_page:ModifyWorldTransform");
    }

    surface->has_ctm = !_cairo_matrix_is_identity (&surface->ctm);
    surface->has_gdi_ctm = !_cairo_matrix_is_identity (&surface->gdi_ctm);
    inverse_ctm = surface->ctm;
    status = cairo_matrix_invert (&inverse_ctm);
    if (status)
	return status;

    x_res = GetDeviceCaps (surface->win32.dc, LOGPIXELSX);
    y_res = GetDeviceCaps (surface->win32.dc, LOGPIXELSY);
    cairo_matrix_transform_distance (&inverse_ctm, &x_res, &y_res);
    _cairo_surface_set_resolution (&surface->win32.base, x_res, y_res);

    SaveDC (surface->win32.dc); /* Then save Cairo's known-good clip state, so the clip path can be reset */

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_win32_printing_surface_set_paginated_mode (void *abstract_surface,
                                                  cairo_paginated_mode_t paginated_mode)
{
    cairo_win32_printing_surface_t *surface = abstract_surface;

    surface->paginated_mode = paginated_mode;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_win32_printing_surface_supports_fine_grained_fallbacks (void *abstract_surface)
{
    return TRUE;
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
 * provide correct complex rendering behaviour; cairo_surface_show_page() and
 * associated methods must be used for correct output.
 *
 * Return value: the newly created surface
 *
 * Since: 1.6
 **/
cairo_surface_t *
cairo_win32_printing_surface_create (HDC hdc)
{
    cairo_win32_printing_surface_t *surface;
    cairo_surface_t *paginated;

    surface = _cairo_malloc (sizeof (cairo_win32_printing_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

#if 0
    if (_cairo_win32_save_initial_clip (hdc, surface) != CAIRO_STATUS_SUCCESS) {
	free (surface);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }
#endif

    _cairo_surface_clipper_init (&surface->clipper,
				 _cairo_win32_printing_surface_clipper_intersect_clip_path);

    surface->win32.format = CAIRO_FORMAT_RGB24;
    surface->content = CAIRO_CONTENT_COLOR_ALPHA;

    surface->win32.dc = hdc;
    surface->extents_valid = FALSE;

    surface->brush = NULL;
    surface->old_brush = NULL;
    surface->font_subsets = _cairo_scaled_font_subsets_create_scaled ();
    if (surface->font_subsets == NULL) {
	free (surface);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    surface->win32.flags = _cairo_win32_flags_for_dc (surface->win32.dc, CAIRO_FORMAT_RGB24);
    surface->win32.flags |= CAIRO_WIN32_SURFACE_FOR_PRINTING;

    _cairo_win32_printing_surface_init_ps_mode (surface);
    _cairo_win32_printing_surface_init_image_support (surface);
    _cairo_win32_printing_surface_init_language_pack (surface);
    _cairo_surface_init (&surface->win32.base,
			 &cairo_win32_printing_surface_backend,
			 NULL, /* device */
                         CAIRO_CONTENT_COLOR_ALPHA,
			 TRUE); /* is_vector */

    paginated = _cairo_paginated_surface_create (&surface->win32.base,
						 CAIRO_CONTENT_COLOR_ALPHA,
						 &cairo_win32_surface_paginated_backend);

    /* paginated keeps the only reference to surface now, drop ours */
    cairo_surface_destroy (&surface->win32.base);

    return paginated;
}

cairo_bool_t
_cairo_surface_is_win32_printing (const cairo_surface_t *surface)
{
    return surface->backend && surface->backend->type == CAIRO_SURFACE_TYPE_WIN32_PRINTING;
}

static const cairo_surface_backend_t cairo_win32_printing_surface_backend = {
    CAIRO_SURFACE_TYPE_WIN32_PRINTING,
    _cairo_win32_printing_surface_finish,

    _cairo_default_context_create,

    _cairo_win32_printing_surface_create_similar,
    NULL, /* create similar image */
    NULL, /* map to image */
    NULL, /* unmap image */

    _cairo_surface_default_source,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* snapshot */

    NULL, /* copy_page */
    _cairo_win32_printing_surface_show_page,

    _cairo_win32_printing_surface_get_extents,
    _cairo_win32_printing_surface_get_font_options,

    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */

    _cairo_win32_printing_surface_paint,
    NULL, /* mask */
    _cairo_win32_printing_surface_stroke,
    _cairo_win32_printing_surface_fill,
    NULL, /* fill/stroke */
    _cairo_win32_printing_surface_show_glyphs,
    NULL, /* has_show_text_glyphs */
    NULL, /* show_text_glyphs */
    _cairo_win32_printing_surface_get_supported_mime_types,
};

static const cairo_paginated_surface_backend_t cairo_win32_surface_paginated_backend = {
    _cairo_win32_printing_surface_start_page,
    _cairo_win32_printing_surface_set_paginated_mode,
    NULL, /* set_bounding_box */
    NULL, /* _cairo_win32_printing_surface_has_fallback_images, */
    _cairo_win32_printing_surface_supports_fine_grained_fallbacks,
};
