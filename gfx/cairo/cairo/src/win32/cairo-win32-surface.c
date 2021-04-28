/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Owen Taylor <otaylor@redhat.com>
 *	Stuart Parmenter <stuart@mozilla.com>
 *	Vladimir Vukicevic <vladimir@pobox.com>
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

#include "cairo-backend-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-paginated-private.h"
#include "cairo-pattern-private.h"
#include "cairo-win32-private.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-surface-fallback-private.h"
#include "cairo-surface-backend-private.h"

#include <wchar.h>
#include <windows.h>

#if defined(__MINGW32__) && !defined(ETO_PDY)
# define ETO_PDY 0x2000
#endif

/**
 * SECTION:cairo-win32
 * @Title: Win32 Surfaces
 * @Short_Description: Microsoft Windows surface support
 * @See_Also: #cairo_surface_t
 *
 * The Microsoft Windows surface is used to render cairo graphics to
 * Microsoft Windows windows, bitmaps, and printing device contexts.
 *
 * The surface returned by cairo_win32_printing_surface_create() is of surface
 * type %CAIRO_SURFACE_TYPE_WIN32_PRINTING and is a multi-page vector surface
 * type.
 *
 * The surface returned by the other win32 constructors is of surface type
 * %CAIRO_SURFACE_TYPE_WIN32 and is a raster surface type.
 **/

/**
 * CAIRO_HAS_WIN32_SURFACE:
 *
 * Defined if the Microsoft Windows surface backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 *
 * Since: 1.0
 **/

/**
 * _cairo_win32_print_gdi_error:
 * @context: context string to display along with the error
 *
 * Helper function to dump out a human readable form of the
 * current error code.
 *
 * Return value: A cairo status code for the error code
 **/
cairo_status_t
_cairo_win32_print_gdi_error (const char *context)
{
    void *lpMsgBuf;
    DWORD last_error = GetLastError ();

    if (!FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER |
			 FORMAT_MESSAGE_FROM_SYSTEM,
			 NULL,
			 last_error,
			 MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
			 (LPWSTR) &lpMsgBuf,
			 0, NULL)) {
	fprintf (stderr, "%s: Unknown GDI error", context);
    } else {
	fprintf (stderr, "%s: %S", context, (wchar_t *)lpMsgBuf);

	LocalFree (lpMsgBuf);
    }

    fflush (stderr);

    return _cairo_error (CAIRO_STATUS_WIN32_GDI_ERROR);
}

cairo_bool_t
_cairo_win32_surface_get_extents (void		          *abstract_surface,
				  cairo_rectangle_int_t   *rectangle)
{
    cairo_win32_surface_t *surface = abstract_surface;

    *rectangle = surface->extents;
    return TRUE;
}

/**
 * cairo_win32_surface_get_dc:
 * @surface: a #cairo_surface_t
 *
 * Returns the HDC associated with this surface, or %NULL if none.
 * Also returns %NULL if the surface is not a win32 surface.
 *
 * A call to cairo_surface_flush() is required before using the HDC to
 * ensure that all pending drawing operations are finished and to
 * restore any temporary modification cairo has made to its state. A
 * call to cairo_surface_mark_dirty() is required after the state or
 * the content of the HDC has been modified.
 *
 * Return value: HDC or %NULL if no HDC available.
 *
 * Since: 1.2
 **/
HDC
cairo_win32_surface_get_dc (cairo_surface_t *surface)
{
    if (surface->backend == NULL)
	return NULL;

    if (surface->backend->type == CAIRO_SURFACE_TYPE_WIN32)
	return to_win32_surface(surface)->dc;

    if (_cairo_surface_is_paginated (surface)) {
	cairo_surface_t *target = _cairo_paginated_surface_get_target (surface);
	if (target->backend->type == CAIRO_SURFACE_TYPE_WIN32_PRINTING)
	    return to_win32_surface(target)->dc;
    }

    return NULL;
}

HDC
cairo_win32_get_dc_with_clip (cairo_t *cr)
{
    cairo_surface_t *surface = cairo_get_target (cr);
    if (cr->backend->type == CAIRO_TYPE_DEFAULT) {
        cairo_default_context_t *c = (cairo_default_context_t *) cr;
        cairo_clip_t *clip = _cairo_clip_copy (_cairo_gstate_get_clip (c->gstate));
        if (_cairo_surface_is_win32 (surface)) {
            cairo_win32_display_surface_t *winsurf = (cairo_win32_display_surface_t *) surface;

            _cairo_win32_display_surface_set_clip (winsurf, clip);

            _cairo_clip_destroy (clip);
            return winsurf->win32.dc;
        }

        if (_cairo_surface_is_paginated (surface)) {
            cairo_surface_t *target;

            target = _cairo_paginated_surface_get_target (surface);

#ifndef CAIRO_OMIT_WIN32_PRINTING
            if (_cairo_surface_is_win32_printing (target)) {
                cairo_status_t status;
                cairo_win32_printing_surface_t *psurf = (cairo_win32_printing_surface_t *) target;

                status = _cairo_surface_clipper_set_clip (&psurf->clipper, clip);

                _cairo_clip_destroy (clip);

                if (status)
                    return NULL;

                return psurf->win32.dc;
            }
#endif
        }
        _cairo_clip_destroy (clip);
    }
    return NULL;
}

/**
 * _cairo_surface_is_win32:
 * @surface: a #cairo_surface_t
 *
 * Checks if a surface is an #cairo_win32_surface_t
 *
 * Return value: %TRUE if the surface is an win32 surface
 **/
cairo_bool_t
_cairo_surface_is_win32 (const cairo_surface_t *surface)
{
    /* _cairo_surface_nil sets a NULL backend so be safe */
    return surface->backend && surface->backend->type == CAIRO_SURFACE_TYPE_WIN32;
}

/**
 * cairo_win32_surface_get_image:
 * @surface: a #cairo_surface_t
 *
 * Returns a #cairo_surface_t image surface that refers to the same bits
 * as the DIB of the Win32 surface.  If the passed-in win32 surface
 * is not a DIB surface, %NULL is returned.
 *
 * Return value: a #cairo_surface_t (owned by the win32 #cairo_surface_t),
 * or %NULL if the win32 surface is not a DIB.
 *
 * Since: 1.4
 **/
cairo_surface_t *
cairo_win32_surface_get_image (cairo_surface_t *surface)
{

    if (! _cairo_surface_is_win32 (surface)) {
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));
    }

    GdiFlush();
    return to_win32_display_surface(surface)->image;
}

#define STACK_GLYPH_SIZE 256
cairo_int_status_t
_cairo_win32_surface_emit_glyphs (cairo_win32_surface_t *dst,
				  const cairo_pattern_t *source,
				  cairo_glyph_t	 *glyphs,
				  int			  num_glyphs,
				  cairo_scaled_font_t	 *scaled_font,
				  cairo_bool_t		  glyph_indexing)
{
#if CAIRO_HAS_DWRITE_FONT
    if (scaled_font->backend->type == CAIRO_FONT_TYPE_DWRITE) {
        if (!glyph_indexing) return CAIRO_INT_STATUS_UNSUPPORTED;

        // FIXME: fake values for params that aren't currently passed in here
        cairo_operator_t op = CAIRO_OPERATOR_SOURCE;
        cairo_clip_t *clip = NULL;
        return _cairo_dwrite_show_glyphs_on_surface (dst, op, source, glyphs, num_glyphs, scaled_font, clip /* , glyph_indexing */ );
    }
#endif
#if CAIRO_HAS_WIN32_FONT
    WORD glyph_buf_stack[STACK_GLYPH_SIZE];
    WORD *glyph_buf = glyph_buf_stack;
    int dxy_buf_stack[2 * STACK_GLYPH_SIZE];
    int *dxy_buf = dxy_buf_stack;

    BOOL win_result = 0;
    int i, j;

    cairo_solid_pattern_t *solid_pattern;
    COLORREF color;

    cairo_matrix_t device_to_logical;

    int start_x, start_y;
    double user_x, user_y;
    int logical_x, logical_y;
    unsigned int glyph_index_option;

    /* We can only handle win32 fonts */
    assert (cairo_scaled_font_get_type (scaled_font) == CAIRO_FONT_TYPE_WIN32);

    /* We can only handle opaque solid color sources and destinations */
    assert (_cairo_pattern_is_opaque_solid(source));
    assert (dst->format == CAIRO_FORMAT_RGB24);

    solid_pattern = (cairo_solid_pattern_t *)source;
    color = RGB(((int)solid_pattern->color.red_short) >> 8,
		((int)solid_pattern->color.green_short) >> 8,
		((int)solid_pattern->color.blue_short) >> 8);

    cairo_win32_scaled_font_get_device_to_logical(scaled_font, &device_to_logical);

    SaveDC(dst->dc);

    cairo_win32_scaled_font_select_font(scaled_font, dst->dc);
    SetTextColor(dst->dc, color);
    SetTextAlign(dst->dc, TA_BASELINE | TA_LEFT);
    SetBkMode(dst->dc, TRANSPARENT);

    if (num_glyphs > STACK_GLYPH_SIZE) {
	glyph_buf = (WORD *) _cairo_malloc_ab (num_glyphs, sizeof(WORD));
        dxy_buf = (int *) _cairo_malloc_abc (num_glyphs, sizeof(int), 2);
    }

    /* It is vital that dx values for dxy_buf are calculated from the delta of
     * _logical_ x coordinates (not user x coordinates) or else the sum of all
     * previous dx values may start to diverge from the current glyph's x
     * coordinate due to accumulated rounding error. As a result strings could
     * be painted shorter or longer than expected. */

    user_x = glyphs[0].x;
    user_y = glyphs[0].y;

    cairo_matrix_transform_point(&device_to_logical,
                                 &user_x, &user_y);

    logical_x = _cairo_lround (user_x);
    logical_y = _cairo_lround (user_y);

    start_x = logical_x;
    start_y = logical_y;

    for (i = 0, j = 0; i < num_glyphs; ++i, j = 2 * i) {
        glyph_buf[i] = (WORD) glyphs[i].index;
        if (i == num_glyphs - 1) {
            dxy_buf[j] = 0;
            dxy_buf[j+1] = 0;
        } else {
            double next_user_x = glyphs[i+1].x;
            double next_user_y = glyphs[i+1].y;
            int next_logical_x, next_logical_y;

            cairo_matrix_transform_point(&device_to_logical,
                                         &next_user_x, &next_user_y);

            next_logical_x = _cairo_lround (next_user_x);
            next_logical_y = _cairo_lround (next_user_y);

            dxy_buf[j] = _cairo_lround (next_logical_x - logical_x);
            dxy_buf[j+1] = _cairo_lround (next_logical_y - logical_y);

            logical_x = next_logical_x;
            logical_y = next_logical_y;
        }
    }

    if (glyph_indexing)
	glyph_index_option = ETO_GLYPH_INDEX;
    else
	glyph_index_option = 0;

    win_result = ExtTextOutW(dst->dc,
                             start_x,
                             start_y,
                             glyph_index_option | ETO_PDY,
                             NULL,
                             glyph_buf,
                             num_glyphs,
                             dxy_buf);
    if (!win_result) {
        _cairo_win32_print_gdi_error("_cairo_win32_surface_show_glyphs(ExtTextOutW failed)");
    }

    RestoreDC(dst->dc, -1);

    if (glyph_buf != glyph_buf_stack) {
	free(glyph_buf);
        free(dxy_buf);
    }
    return (win_result) ? CAIRO_STATUS_SUCCESS : CAIRO_INT_STATUS_UNSUPPORTED;
#else
    return CAIRO_INT_STATUS_UNSUPPORTED;
#endif
}
#undef STACK_GLYPH_SIZE

cairo_status_t
cairo_win32_surface_get_size (const cairo_surface_t *surface, int *width, int *height)
{
    if (surface->type != CAIRO_SURFACE_TYPE_WIN32)
        return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    const cairo_win32_surface_t *winsurface = (const cairo_win32_surface_t *) surface;

    *width = winsurface->extents.width;
    *height = winsurface->extents.height;

    return CAIRO_STATUS_SUCCESS;
}

