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
#include "cairo-surface-clipper-private.h"

#ifndef SHADEBLENDCAPS
#define SHADEBLENDCAPS 120
#endif
#ifndef SB_NONE
#define SB_NONE 0
#endif

#define WIN32_FONT_LOGICAL_SCALE 1


CAIRO_BEGIN_DECLS

typedef struct _cairo_win32_surface {
    cairo_surface_t base;

    cairo_format_t format;

    HDC dc;

    struct IDirect3DSurface9 *d3d9surface;

    /* We create off-screen surfaces as DIBs or DDBs, based on what we created
     * originally*/
    HBITMAP bitmap;
    cairo_bool_t is_dib;

    /* Used to save the initial 1x1 monochrome bitmap for the DC to
     * select back into the DC before deleting the DC and our
     * bitmap. For Windows XP, this doesn't seem to be necessary
     * ... we can just delete the DC and that automatically unselects
     * out bitmap. But it's standard practice so apparently is needed
     * on some versions of Windows.
     */
    HBITMAP saved_dc_bitmap;

    cairo_surface_t *image;

    cairo_rectangle_int_t extents;

    /* Initial clip bits
     * We need these kept around so that we maintain
     * whatever clip was set on the original DC at creation
     * time when cairo is asked to reset the surface clip.
     */
    cairo_rectangle_int_t clip_rect;
    HRGN initial_clip_rgn;
    cairo_bool_t had_simple_clip;
    cairo_region_t *clip_region;

    /* For path clipping to the printing-surface */
    cairo_surface_clipper_t clipper;

    /* Surface DC flags */
    uint32_t flags;

    /* printing surface bits */
    cairo_paginated_mode_t paginated_mode;
    cairo_content_t content;
    cairo_bool_t path_empty;
    cairo_bool_t has_ctm;
    cairo_matrix_t ctm;
    cairo_bool_t has_gdi_ctm;
    cairo_matrix_t gdi_ctm;
    HBRUSH brush, old_brush;
    cairo_scaled_font_subsets_t *font_subsets;
} cairo_win32_surface_t;

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

    /* Whether we can use the CHECKJPEGFORMAT escape function */
    CAIRO_WIN32_SURFACE_CAN_CHECK_PNG = (1<<8),

    /* if this DDB surface can be converted to a DIB if necessary */
    CAIRO_WIN32_SURFACE_CAN_CONVERT_TO_DIB = (1<<9),
};

cairo_status_t
_cairo_win32_print_gdi_error (const char *context);

cairo_bool_t
_cairo_surface_is_win32 (cairo_surface_t *surface);

cairo_bool_t
_cairo_surface_is_win32_printing (cairo_surface_t *surface);

cairo_status_t
_cairo_win32_surface_finish (void *abstract_surface);

cairo_bool_t
_cairo_win32_surface_get_extents (void		          *abstract_surface,
				  cairo_rectangle_int_t   *rectangle);

uint32_t
_cairo_win32_flags_for_dc (HDC dc);

cairo_status_t
_cairo_win32_surface_set_clip_region (void           *abstract_surface,
				      cairo_region_t *region);

cairo_int_status_t
_cairo_win32_surface_show_glyphs_internal (void			 *surface,
					   cairo_operator_t	  op,
					   const cairo_pattern_t *source,
					   cairo_glyph_t	 *glyphs,
					   int			  num_glyphs,
					   cairo_scaled_font_t	 *scaled_font,
					   cairo_clip_t		 *clip,
					   int			 *remaining_glyphs,
					   cairo_bool_t		  glyph_indices);

cairo_int_status_t
_cairo_win32_surface_show_glyphs (void			*surface,
				  cairo_operator_t	 op,
				  const cairo_pattern_t	*source,
				  cairo_glyph_t		*glyphs,
				  int			 num_glyphs,
				  cairo_scaled_font_t	*scaled_font,
				  cairo_clip_t		*clip,
				  int			*remaining_glyphs);

cairo_surface_t *
_cairo_win32_surface_create_similar (void	    *abstract_src,
				     cairo_content_t content,
				     int	     width,
				     int	     height);

cairo_status_t
_cairo_win32_surface_clone_similar (void *abstract_surface,
				    cairo_surface_t *src,
				    cairo_content_t content,
				    int src_x,
				    int src_y,
				    int width,
				    int height,
				    int *clone_offset_x,
				    int *clone_offset_y,
				    cairo_surface_t **clone_out);

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

cairo_int_status_t
_cairo_win32_save_initial_clip (HDC dc, cairo_win32_surface_t *surface);

cairo_int_status_t
_cairo_win32_restore_initial_clip (cairo_win32_surface_t *surface);

void
_cairo_win32_debug_dump_hrgn (HRGN rgn, char *header);

cairo_bool_t
_cairo_win32_scaled_font_is_type1 (cairo_scaled_font_t *scaled_font);

cairo_bool_t
_cairo_win32_scaled_font_is_bitmap (cairo_scaled_font_t *scaled_font);

#ifdef CAIRO_HAS_DWRITE_FONT

cairo_int_status_t
_cairo_dwrite_show_glyphs_on_surface(void			*surface,
				     cairo_operator_t		 op,
				     const cairo_pattern_t	*source,
				     cairo_glyph_t		*glyphs,
				     int			 num_glyphs,
				     cairo_scaled_font_t	*scaled_font,
				     cairo_clip_t               *clip);

cairo_int_status_t
_cairo_dwrite_scaled_font_create_win32_scaled_font(cairo_scaled_font_t *scaled_font,
                                                   cairo_scaled_font_t **new_font);

#endif /* CAIRO_HAS_DWRITE_FONT */
CAIRO_END_DECLS
#endif /* CAIRO_WIN32_PRIVATE_H */
