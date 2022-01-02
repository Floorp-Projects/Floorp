/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
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
 */

#ifndef CAIRO_WIN32_PRIVATE_H
#define CAIRO_WIN32_PRIVATE_H

#include "cairo-win32.h"

#include "cairoint.h"

#include "cairo-device-private.h"
#include "cairo-surface-clipper-private.h"
#include "cairo-surface-private.h"

#ifndef SHADEBLENDCAPS
#define SHADEBLENDCAPS 120
#endif
#ifndef SB_NONE
#define SB_NONE 0
#endif

#define WIN32_FONT_LOGICAL_SCALE 32

CAIRO_BEGIN_DECLS

/* Surface DC flag values */
enum {
    /* If this is a surface created for printing or not */
    CAIRO_WIN32_SURFACE_FOR_PRINTING = (1<<0),

    /* Whether the DC is a display DC or not */
    CAIRO_WIN32_SURFACE_IS_DISPLAY = (1<<1),

    /* Whether we can use BitBlt with this surface */
    CAIRO_WIN32_SURFACE_CAN_BITBLT = (1<<2),

    /* Whether we can use AlphaBlend with this surface */
    CAIRO_WIN32_SURFACE_CAN_ALPHABLEND = (1<<3),

    /* Whether we can use StretchBlt with this surface */
    CAIRO_WIN32_SURFACE_CAN_STRETCHBLT = (1<<4),

    /* Whether we can use StretchDIBits with this surface */
    CAIRO_WIN32_SURFACE_CAN_STRETCHDIB = (1<<5),

    /* Whether we can use GradientFill rectangles with this surface */
    CAIRO_WIN32_SURFACE_CAN_RECT_GRADIENT = (1<<6),

    /* Whether we can use the CHECKJPEGFORMAT escape function */
    CAIRO_WIN32_SURFACE_CAN_CHECK_JPEG = (1<<7),

    /* Whether we can use the CHECKPNGFORMAT escape function */
    CAIRO_WIN32_SURFACE_CAN_CHECK_PNG = (1<<8),

    /* Whether we can use gdi drawing with solid rgb brush with this surface */
    CAIRO_WIN32_SURFACE_CAN_RGB_BRUSH = (1<<9),
};

typedef struct _cairo_win32_surface {
    cairo_surface_t base;

    cairo_format_t format;
    HDC dc;

    /* Surface DC flags */
    unsigned flags;

    /* We use the x and y parts of extents for situations where
     * we're not supposed to draw to the entire surface.
     * For example, during a paint event a program will get
     * a DC that has been clipped to the dirty region.
     * A cairo surface constructed for that DC will have extents
     * that match bounds of the clipped region.
     */
    cairo_rectangle_int_t extents;

    /* Offset added to extents, used when the extents start with a negative
     * offset, which can occur on Windows for, and only for, desktop DC.  This
     * occurs when you have multiple monitors, and at least one monitor
     * extends to the left, or above, the primaty monitor.  The primary
     * monitor on Windows always starts with offset (0,0), and any other points
     * to the left, or above, have negative offsets.  So the 'desktop DC' is
     * in fact a 'virtual desktop' which can start with extents in the negative
     * range.
     *
     * Why use new variables, and not the device transform?  Simply because since
     * the device transform functions are exposed, a lot of 3rd party libraries
     * simply overwrite those, disregarding the prior content, instead of actually
     * adding the offset.  GTK for example simply resets the device transform of the
     * desktop cairo surface to zero.  So make some private member variables for
     * this, which will not be fiddled with externally.
     */
    int x_ofs, y_ofs;
} cairo_win32_surface_t;
#define to_win32_surface(S) ((cairo_win32_surface_t *)(S))

typedef struct _cairo_win32_display_surface {
    cairo_win32_surface_t win32;

    /* We create off-screen surfaces as DIBs or DDBs, based on what we created
     * originally */
    HBITMAP bitmap;
    cairo_bool_t is_dib;

    /* Used to save the initial 1x1 monochrome bitmap for the DC to
     * select back into the DC before deleting the DC and our
     * bitmap. For Windows XP, this doesn't seem to be necessary
     * ... we can just delete the DC and that automatically unselects
     * our bitmap. But it's standard practice so apparently is needed
     * on some versions of Windows.
     */
    HBITMAP saved_dc_bitmap;
    cairo_surface_t *image;
    cairo_surface_t *fallback;

    HRGN initial_clip_rgn;
    cairo_bool_t had_simple_clip;
} cairo_win32_display_surface_t;
#define to_win32_display_surface(S) ((cairo_win32_display_surface_t *)(S))

typedef struct _cairo_win32_printing_surface {
    cairo_win32_surface_t win32;

    cairo_surface_clipper_t clipper;

    cairo_paginated_mode_t paginated_mode;
    cairo_content_t content;
    cairo_bool_t path_empty;
    cairo_bool_t has_ctm;
    cairo_matrix_t ctm;
    cairo_bool_t has_gdi_ctm;
    cairo_matrix_t gdi_ctm;
    cairo_bool_t extents_valid;
    HBRUSH brush, old_brush;
    cairo_scaled_font_subsets_t *font_subsets;
} cairo_win32_printing_surface_t;
#define to_win32_printing_surface(S) ((cairo_win32_printing_surface_t *)(S))

typedef BOOL (WINAPI *cairo_win32_alpha_blend_func_t) (HDC hdcDest,
						       int nXOriginDest,
						       int nYOriginDest,
						       int nWidthDest,
						       int hHeightDest,
						       HDC hdcSrc,
						       int nXOriginSrc,
						       int nYOriginSrc,
						       int nWidthSrc,
						       int nHeightSrc,
						       BLENDFUNCTION blendFunction);

typedef struct _cairo_win32_device {
    cairo_device_t base;

    HMODULE msimg32_dll;

    const cairo_compositor_t *compositor;

    cairo_win32_alpha_blend_func_t alpha_blend;
} cairo_win32_device_t;
#define to_win32_device(D) ((cairo_win32_device_t *)(D))
#define to_win32_device_from_surface(S) to_win32_device(((cairo_surface_t *)(S))->device)

cairo_private cairo_device_t *
_cairo_win32_device_get (void);

const cairo_compositor_t *
_cairo_win32_gdi_compositor_get (void);

cairo_status_t
_cairo_win32_print_gdi_error (const char *context);

cairo_bool_t
_cairo_surface_is_win32 (const cairo_surface_t *surface);

cairo_bool_t
_cairo_surface_is_win32_printing (const cairo_surface_t *surface);

cairo_private void
_cairo_win32_display_surface_discard_fallback (cairo_win32_display_surface_t *surface);

cairo_bool_t
_cairo_win32_surface_get_extents (void			  *abstract_surface,
				  cairo_rectangle_int_t   *rectangle);

uint32_t
_cairo_win32_flags_for_dc (HDC dc, cairo_format_t format);

cairo_int_status_t
_cairo_win32_surface_emit_glyphs (cairo_win32_surface_t *dst,
				  const cairo_pattern_t *source,
				  cairo_glyph_t	 *glyphs,
				  int			  num_glyphs,
				  cairo_scaled_font_t	 *scaled_font,
				  cairo_bool_t		  glyph_indexing);

static inline void
_cairo_matrix_to_win32_xform (const cairo_matrix_t *m,
			      XFORM *xform)
{
    xform->eM11 = (FLOAT) m->xx;
    xform->eM21 = (FLOAT) m->xy;
    xform->eM12 = (FLOAT) m->yx;
    xform->eM22 = (FLOAT) m->yy;
    xform->eDx = (FLOAT) m->x0;
    xform->eDy = (FLOAT) m->y0;
}

cairo_status_t
_cairo_win32_display_surface_set_clip (cairo_win32_display_surface_t *surface,
				       cairo_clip_t *clip);

void
_cairo_win32_display_surface_unset_clip (cairo_win32_display_surface_t *surface);

void
_cairo_win32_debug_dump_hrgn (HRGN rgn, char *header);

cairo_bool_t
_cairo_win32_scaled_font_is_type1 (cairo_scaled_font_t *scaled_font);

cairo_bool_t
_cairo_win32_scaled_font_is_bitmap (cairo_scaled_font_t *scaled_font);

#ifdef CAIRO_HAS_DWRITE_FONT

cairo_int_status_t
_cairo_dwrite_show_glyphs_on_surface (void *surface,
                                      cairo_operator_t op,
                                      const cairo_pattern_t *source,
                                      cairo_glyph_t *glyphs,
                                      int num_glyphs,
                                      cairo_scaled_font_t *scaled_font,
                                      cairo_clip_t *clip);

cairo_int_status_t
_cairo_dwrite_scaled_font_create_win32_scaled_font (cairo_scaled_font_t *scaled_font,
                                                    cairo_scaled_font_t **new_font);

#endif /* CAIRO_HAS_DWRITE_FONT */

CAIRO_END_DECLS

#endif /* CAIRO_WIN32_PRIVATE_H */
