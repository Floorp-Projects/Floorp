/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
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
 */

#define WIN32_LEAN_AND_MEAN
/* We require Windows 2000 features such as GetGlyphIndices */
#if !defined(WINVER) || (WINVER < 0x0500)
# define WINVER 0x0500
#endif
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0500)
# define _WIN32_WINNT 0x0500
#endif

#include "cairoint.h"

#include "cairo-win32-private.h"
#include "cairo-error-private.h"

#include <wchar.h>

#ifndef SPI_GETFONTSMOOTHINGTYPE
#define SPI_GETFONTSMOOTHINGTYPE 0x200a
#endif
#ifndef FE_FONTSMOOTHINGCLEARTYPE
#define FE_FONTSMOOTHINGCLEARTYPE 2
#endif
#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY 5
#endif
#ifndef TT_PRIM_CSPLINE
#define TT_PRIM_CSPLINE 3
#endif

#define CMAP_TAG 0x70616d63

/**
 * SECTION:cairo-win32-fonts
 * @Title: Win32 Fonts
 * @Short_Description: Font support for Microsoft Windows
 * @See_Also: #cairo_font_face_t
 *
 * The Microsoft Windows font backend is primarily used to render text on
 * Microsoft Windows systems.
 */

/**
 * CAIRO_HAS_WIN32_FONT:
 *
 * Defined if the Microsoft Windows font backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 */

const cairo_scaled_font_backend_t _cairo_win32_scaled_font_backend;

typedef struct {
    cairo_scaled_font_t base;

    LOGFONTW logfont;

    BYTE quality;

    /* We do drawing and metrics computation in a "logical space" which
     * is similar to font space, except that it is scaled by a factor
     * of the (desired font size) * (WIN32_FONT_LOGICAL_SCALE). The multiplication
     * by WIN32_FONT_LOGICAL_SCALE allows for sub-pixel precision.
     */
    double logical_scale;

    /* The size we should actually request the font at from Windows; differs
     * from the logical_scale because it is quantized for orthogonal
     * transformations
     */
    double logical_size;

    /* Transformations from device <=> logical space
     */
    cairo_matrix_t logical_to_device;
    cairo_matrix_t device_to_logical;

    /* We special case combinations of 90-degree-rotations, scales and
     * flips ... that is transformations that take the axes to the
     * axes. If preserve_axes is true, then swap_axes/swap_x/swap_y
     * encode the 8 possibilities for orientation (4 rotation angles with
     * and without a flip), and scale_x, scale_y the scale components.
     */
    cairo_bool_t preserve_axes;
    cairo_bool_t swap_axes;
    cairo_bool_t swap_x;
    cairo_bool_t swap_y;
    double x_scale;
    double y_scale;

    /* The size of the design unit of the font
     */
    int em_square;

    HFONT scaled_hfont;
    HFONT unscaled_hfont;

    cairo_bool_t is_bitmap;
    cairo_bool_t is_type1;
    cairo_bool_t delete_scaled_hfont;
} cairo_win32_scaled_font_t;

static cairo_status_t
_cairo_win32_scaled_font_set_metrics (cairo_win32_scaled_font_t *scaled_font);

static cairo_status_t
_cairo_win32_scaled_font_init_glyph_metrics (cairo_win32_scaled_font_t *scaled_font,
					     cairo_scaled_glyph_t      *scaled_glyph);

static cairo_status_t
_cairo_win32_scaled_font_init_glyph_surface (cairo_win32_scaled_font_t *scaled_font,
                                             cairo_scaled_glyph_t      *scaled_glyph);

static cairo_status_t
_cairo_win32_scaled_font_init_glyph_path (cairo_win32_scaled_font_t *scaled_font,
					  cairo_scaled_glyph_t      *scaled_glyph);

#define NEARLY_ZERO(d) (fabs(d) < (1. / 65536.))

static HDC
_get_global_font_dc (void)
{
    static HDC hdc;

    if (!hdc) {
	hdc = CreateCompatibleDC (NULL);
	if (!hdc) {
	    _cairo_win32_print_gdi_error ("_get_global_font_dc");
	    return NULL;
	}

	if (!SetGraphicsMode (hdc, GM_ADVANCED)) {
	    _cairo_win32_print_gdi_error ("_get_global_font_dc");
	    DeleteDC (hdc);
	    return NULL;
	}
    }

    return hdc;
}

static cairo_status_t
_compute_transform (cairo_win32_scaled_font_t *scaled_font,
		    cairo_matrix_t            *sc)
{
    cairo_status_t status;

    if (NEARLY_ZERO (sc->yx) && NEARLY_ZERO (sc->xy) &&
	    !NEARLY_ZERO(sc->xx) && !NEARLY_ZERO(sc->yy)) {
	scaled_font->preserve_axes = TRUE;
	scaled_font->x_scale = sc->xx;
	scaled_font->swap_x = (sc->xx < 0);
	scaled_font->y_scale = sc->yy;
	scaled_font->swap_y = (sc->yy < 0);
	scaled_font->swap_axes = FALSE;

    } else if (NEARLY_ZERO (sc->xx) && NEARLY_ZERO (sc->yy) &&
	    !NEARLY_ZERO(sc->yx) && !NEARLY_ZERO(sc->xy)) {
	scaled_font->preserve_axes = TRUE;
	scaled_font->x_scale = sc->yx;
	scaled_font->swap_x = (sc->yx < 0);
	scaled_font->y_scale = sc->xy;
	scaled_font->swap_y = (sc->xy < 0);
	scaled_font->swap_axes = TRUE;

    } else {
	scaled_font->preserve_axes = FALSE;
	scaled_font->swap_x = scaled_font->swap_y = scaled_font->swap_axes = FALSE;
    }

    if (scaled_font->preserve_axes) {
	if (scaled_font->swap_x)
	    scaled_font->x_scale = - scaled_font->x_scale;
	if (scaled_font->swap_y)
	    scaled_font->y_scale = - scaled_font->y_scale;

	scaled_font->logical_scale = WIN32_FONT_LOGICAL_SCALE * scaled_font->y_scale;
	scaled_font->logical_size = WIN32_FONT_LOGICAL_SCALE *
                                    _cairo_lround (scaled_font->y_scale);
    }

    /* The font matrix has x and y "scale" components which we extract and
     * use as character scale values.
     */
    cairo_matrix_init (&scaled_font->logical_to_device,
		       sc->xx, sc->yx, sc->xy, sc->yy, 0, 0);

    if (!scaled_font->preserve_axes) {
	status = _cairo_matrix_compute_basis_scale_factors (&scaled_font->logical_to_device,
						      &scaled_font->x_scale, &scaled_font->y_scale,
						      TRUE);	/* XXX: Handle vertical text */
	if (status)
	    return status;

	scaled_font->logical_size = _cairo_lround (WIN32_FONT_LOGICAL_SCALE *
                                                   scaled_font->y_scale);
	scaled_font->logical_scale = WIN32_FONT_LOGICAL_SCALE * scaled_font->y_scale;
    }

    cairo_matrix_scale (&scaled_font->logical_to_device,
			1.0 / scaled_font->logical_scale, 1.0 / scaled_font->logical_scale);

    scaled_font->device_to_logical = scaled_font->logical_to_device;

    status = cairo_matrix_invert (&scaled_font->device_to_logical);
    if (status)
	cairo_matrix_init_identity (&scaled_font->device_to_logical);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_have_cleartype_quality (void)
{
    OSVERSIONINFO version_info;

    version_info.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);

    if (!GetVersionEx (&version_info)) {
	_cairo_win32_print_gdi_error ("_have_cleartype_quality");
	return FALSE;
    }

    return (version_info.dwMajorVersion > 5 ||
	    (version_info.dwMajorVersion == 5 &&
	     version_info.dwMinorVersion >= 1));	/* XP or newer */
}

BYTE
cairo_win32_get_system_text_quality (void)
{
    BOOL font_smoothing;
    UINT smoothing_type;

    if (!SystemParametersInfo (SPI_GETFONTSMOOTHING, 0, &font_smoothing, 0)) {
	_cairo_win32_print_gdi_error ("_cairo_win32_get_system_text_quality");
	return DEFAULT_QUALITY;
    }

    if (font_smoothing) {
	if (_have_cleartype_quality ()) {
	    if (!SystemParametersInfo (SPI_GETFONTSMOOTHINGTYPE,
				       0, &smoothing_type, 0)) {
		_cairo_win32_print_gdi_error ("_cairo_win32_get_system_text_quality");
		return DEFAULT_QUALITY;
	    }

	    if (smoothing_type == FE_FONTSMOOTHINGCLEARTYPE)
		return CLEARTYPE_QUALITY;
	}

	return ANTIALIASED_QUALITY;
    } else {
	return DEFAULT_QUALITY;
    }
}

/* If face_hfont is non-%NULL then font_matrix must be a simple scale by some
 * factor S, ctm must be the identity, logfont->lfHeight must be -S,
 * logfont->lfWidth, logfont->lfEscapement, logfont->lfOrientation must
 * all be 0, and face_hfont is the result of calling CreateFontIndirectW on
 * logfont.
 */
static cairo_status_t
_win32_scaled_font_create (LOGFONTW                   *logfont,
			   HFONT                      face_hfont,
			   cairo_font_face_t	      *font_face,
			   const cairo_matrix_t       *font_matrix,
			   const cairo_matrix_t       *ctm,
			   const cairo_font_options_t *options,
			   cairo_scaled_font_t       **font_out)
{
    HDC hdc;
    cairo_win32_scaled_font_t *f;
    cairo_matrix_t scale;
    cairo_status_t status;

    hdc = _get_global_font_dc ();
    if (hdc == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    f = malloc (sizeof(cairo_win32_scaled_font_t));
    if (f == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    f->logfont = *logfont;

    /* We don't have any control over the hinting style or subpixel
     * order in the Win32 font API, so we ignore those parts of
     * cairo_font_options_t. We use the 'antialias' field to set
     * the 'quality'.
     *
     * XXX: The other option we could pay attention to, but don't
     *      here is the hint_metrics options.
     */
    if (options->antialias == CAIRO_ANTIALIAS_DEFAULT)
	f->quality = cairo_win32_get_system_text_quality ();
    else {
	switch (options->antialias) {
	case CAIRO_ANTIALIAS_NONE:
	    f->quality = NONANTIALIASED_QUALITY;
	    break;
	case CAIRO_ANTIALIAS_GRAY:
	    f->quality = ANTIALIASED_QUALITY;
	    break;
	case CAIRO_ANTIALIAS_SUBPIXEL:
	    if (_have_cleartype_quality ())
		f->quality = CLEARTYPE_QUALITY;
	    else
		f->quality = ANTIALIASED_QUALITY;
	    break;
	case CAIRO_ANTIALIAS_DEFAULT:
	    ASSERT_NOT_REACHED;
	}
    }

    f->em_square = 0;
    f->scaled_hfont = NULL;
    f->unscaled_hfont = NULL;

    if (f->quality == logfont->lfQuality ||
        (logfont->lfQuality == DEFAULT_QUALITY &&
         options->antialias == CAIRO_ANTIALIAS_DEFAULT)) {
        /* If face_hfont is non-NULL, then we can use it to avoid creating our
         * own --- because the constraints on face_hfont mentioned above
         * guarantee it was created in exactly the same way that
         * _win32_scaled_font_get_scaled_hfont would create it.
         */
        f->scaled_hfont = face_hfont;
    }
    /* don't delete the hfont if we're using the one passed in to us */
    f->delete_scaled_hfont = !f->scaled_hfont;

    cairo_matrix_multiply (&scale, font_matrix, ctm);
    status = _compute_transform (f, &scale);
    if (status)
	goto FAIL;

    status = _cairo_scaled_font_init (&f->base, font_face,
				      font_matrix, ctm, options,
				      &_cairo_win32_scaled_font_backend);
    if (status)
	goto FAIL;

    status = _cairo_win32_scaled_font_set_metrics (f);
    if (status) {
	_cairo_scaled_font_fini (&f->base);
	goto FAIL;
    }

    *font_out = &f->base;
    return CAIRO_STATUS_SUCCESS;

 FAIL:
    free (f);
    return status;
}

static cairo_status_t
_win32_scaled_font_set_world_transform (cairo_win32_scaled_font_t *scaled_font,
					HDC                        hdc)
{
    XFORM xform;

    _cairo_matrix_to_win32_xform (&scaled_font->logical_to_device, &xform);

    if (!SetWorldTransform (hdc, &xform))
	return _cairo_win32_print_gdi_error ("_win32_scaled_font_set_world_transform");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_win32_scaled_font_set_identity_transform (HDC hdc)
{
    if (!ModifyWorldTransform (hdc, NULL, MWT_IDENTITY))
	return _cairo_win32_print_gdi_error ("_win32_scaled_font_set_identity_transform");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_win32_scaled_font_get_scaled_hfont (cairo_win32_scaled_font_t *scaled_font,
				     HFONT *hfont_out)
{
    if (!scaled_font->scaled_hfont) {
	LOGFONTW logfont = scaled_font->logfont;
	logfont.lfHeight = -scaled_font->logical_size;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 0;
	logfont.lfQuality = scaled_font->quality;

	scaled_font->scaled_hfont = CreateFontIndirectW (&logfont);
	if (!scaled_font->scaled_hfont)
	    return _cairo_win32_print_gdi_error ("_win32_scaled_font_get_scaled_hfont");
    }

    *hfont_out = scaled_font->scaled_hfont;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_win32_scaled_font_get_unscaled_hfont (cairo_win32_scaled_font_t *scaled_font,
				       HDC                        hdc,
				       HFONT			 *hfont_out)
{
    if (scaled_font->unscaled_hfont == NULL) {
	OUTLINETEXTMETRIC *otm;
	unsigned int otm_size;
	HFONT scaled_hfont;
	LOGFONTW logfont;
	cairo_status_t status;

	status = _win32_scaled_font_get_scaled_hfont (scaled_font,
						      &scaled_hfont);
	if (status)
	    return status;

	if (! SelectObject (hdc, scaled_hfont))
	    return _cairo_win32_print_gdi_error ("_win32_scaled_font_get_unscaled_hfont:SelectObject");

	otm_size = GetOutlineTextMetrics (hdc, 0, NULL);
	if (! otm_size)
	    return _cairo_win32_print_gdi_error ("_win32_scaled_font_get_unscaled_hfont:GetOutlineTextMetrics");

	otm = malloc (otm_size);
	if (otm == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	if (! GetOutlineTextMetrics (hdc, otm_size, otm)) {
	    status = _cairo_win32_print_gdi_error ("_win32_scaled_font_get_unscaled_hfont:GetOutlineTextMetrics");
	    free (otm);
	    return status;
	}

	scaled_font->em_square = otm->otmEMSquare;
	free (otm);

	logfont = scaled_font->logfont;
	logfont.lfHeight = -scaled_font->em_square;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 0;
	logfont.lfQuality = scaled_font->quality;

	scaled_font->unscaled_hfont = CreateFontIndirectW (&logfont);
	if (! scaled_font->unscaled_hfont)
	    return _cairo_win32_print_gdi_error ("_win32_scaled_font_get_unscaled_hfont:CreateIndirect");
    }

    *hfont_out = scaled_font->unscaled_hfont;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_scaled_font_select_unscaled_font (cairo_scaled_font_t *scaled_font,
					       HDC                  hdc)
{
    cairo_status_t status;
    HFONT hfont;
    HFONT old_hfont = NULL;

    status = _win32_scaled_font_get_unscaled_hfont ((cairo_win32_scaled_font_t *)scaled_font, hdc, &hfont);
    if (status)
	return status;

    old_hfont = SelectObject (hdc, hfont);
    if (!old_hfont)
	return _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_select_unscaled_font");

    status = _win32_scaled_font_set_identity_transform (hdc);
    if (status) {
	SelectObject (hdc, old_hfont);
	return status;
    }

    SetMapMode (hdc, MM_TEXT);

    return CAIRO_STATUS_SUCCESS;
}

cairo_bool_t
_cairo_win32_scaled_font_is_type1 (cairo_scaled_font_t *scaled_font)
{
    cairo_win32_scaled_font_t *win32_scaled_font;

    win32_scaled_font = (cairo_win32_scaled_font_t *) scaled_font;

    return win32_scaled_font->is_type1;
}

cairo_bool_t
_cairo_win32_scaled_font_is_bitmap (cairo_scaled_font_t *scaled_font)
{
    cairo_win32_scaled_font_t *win32_scaled_font;

    win32_scaled_font = (cairo_win32_scaled_font_t *) scaled_font;

    return win32_scaled_font->is_bitmap;
}

static void
_cairo_win32_scaled_font_done_unscaled_font (cairo_scaled_font_t *scaled_font)
{
}

/* implement the font backend interface */

static cairo_status_t
_cairo_win32_font_face_create_for_toy (cairo_toy_font_face_t   *toy_face,
				       cairo_font_face_t      **font_face)
{
    LOGFONTW logfont;
    uint16_t *face_name;
    int face_name_len;
    cairo_status_t status;

    status = _cairo_utf8_to_utf16 (toy_face->family, -1,
				   &face_name, &face_name_len);
    if (status)
	return status;

    if (face_name_len > LF_FACESIZE - 1)
	face_name_len = LF_FACESIZE - 1;

    memcpy (logfont.lfFaceName, face_name, sizeof (uint16_t) * face_name_len);
    logfont.lfFaceName[face_name_len] = 0;
    free (face_name);

    logfont.lfHeight = 0;	/* filled in later */
    logfont.lfWidth = 0;	/* filled in later */
    logfont.lfEscapement = 0;	/* filled in later */
    logfont.lfOrientation = 0;	/* filled in later */

    switch (toy_face->weight) {
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
	logfont.lfWeight = FW_NORMAL;
	break;
    case CAIRO_FONT_WEIGHT_BOLD:
	logfont.lfWeight = FW_BOLD;
	break;
    }

    switch (toy_face->slant) {
    case CAIRO_FONT_SLANT_NORMAL:
    default:
	logfont.lfItalic = FALSE;
	break;
    case CAIRO_FONT_SLANT_ITALIC:
    case CAIRO_FONT_SLANT_OBLIQUE:
	logfont.lfItalic = TRUE;
	break;
    }

    logfont.lfUnderline = FALSE;
    logfont.lfStrikeOut = FALSE;
    /* The docs for LOGFONT discourage using this, since the
     * interpretation is locale-specific, but it's not clear what
     * would be a better alternative.
     */
    logfont.lfCharSet = DEFAULT_CHARSET;
    logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    logfont.lfQuality = DEFAULT_QUALITY; /* filled in later */
    logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    *font_face = cairo_win32_font_face_create_for_logfontw (&logfont);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_scaled_font_fini (void *abstract_font)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;

    if (scaled_font == NULL)
	return;

    if (scaled_font->scaled_hfont && scaled_font->delete_scaled_hfont)
	DeleteObject (scaled_font->scaled_hfont);

    if (scaled_font->unscaled_hfont)
	DeleteObject (scaled_font->unscaled_hfont);
}

static cairo_int_status_t
_cairo_win32_scaled_font_type1_text_to_glyphs (cairo_win32_scaled_font_t *scaled_font,
					       double		          x,
					       double		          y,
					       const char	         *utf8,
					       cairo_glyph_t            **glyphs,
					       int		         *num_glyphs)
{
    uint16_t *utf16;
    int n16;
    int i;
    WORD *glyph_indices = NULL;
    cairo_status_t status;
    double x_pos, y_pos;
    HDC hdc = NULL;
    cairo_matrix_t mat;

    status = _cairo_utf8_to_utf16 (utf8, -1, &utf16, &n16);
    if (status)
	return status;

    glyph_indices = _cairo_malloc_ab (n16 + 1, sizeof (WORD));
    if (!glyph_indices) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto FAIL1;
    }

    hdc = _get_global_font_dc ();
    assert (hdc != NULL);

    status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
    if (status)
	goto FAIL2;

    if (GetGlyphIndicesW (hdc, utf16, n16, glyph_indices, 0) == GDI_ERROR) {
	status = _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_type1_text_to_glyphs:GetGlyphIndicesW");
	goto FAIL3;
    }

    *num_glyphs = n16;
    *glyphs = _cairo_malloc_ab (n16, sizeof (cairo_glyph_t));
    if (!*glyphs) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto FAIL3;
    }

    x_pos = x;
    y_pos = y;

    mat = scaled_font->base.ctm;
    status = cairo_matrix_invert (&mat);
    assert (status == CAIRO_STATUS_SUCCESS);

    _cairo_scaled_font_freeze_cache (&scaled_font->base);

    for (i = 0; i < n16; i++) {
	cairo_scaled_glyph_t *scaled_glyph;

	(*glyphs)[i].index = glyph_indices[i];
	(*glyphs)[i].x = x_pos;
	(*glyphs)[i].y = y_pos;

	status = _cairo_scaled_glyph_lookup (&scaled_font->base,
					     glyph_indices[i],
					     CAIRO_SCALED_GLYPH_INFO_METRICS,
					     &scaled_glyph);
	if (status) {
	    free (*glyphs);
	    *glyphs = NULL;
	    break;
	}

	x = scaled_glyph->x_advance;
	y = scaled_glyph->y_advance;
	cairo_matrix_transform_distance (&mat, &x, &y);
	x_pos += x;
	y_pos += y;
    }

    _cairo_scaled_font_thaw_cache (&scaled_font->base);

FAIL3:
    cairo_win32_scaled_font_done_font (&scaled_font->base);
FAIL2:
    free (glyph_indices);
FAIL1:
    free (utf16);

    return status;
}

static cairo_int_status_t
_cairo_win32_scaled_font_text_to_glyphs (void		*abstract_font,
					 double		x,
					 double		y,
					 const char	*utf8,
					 cairo_glyph_t **glyphs,
					 int		*num_glyphs)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    uint16_t *utf16;
    int n16;
    GCP_RESULTSW gcp_results;
    unsigned int buffer_size, i;
    WCHAR *glyph_indices = NULL;
    int *dx = NULL;
    cairo_status_t status;
    double x_pos, y_pos;
    double x_incr, y_incr;
    HDC hdc = NULL;

    /* GetCharacterPlacement() returns utf16 instead of glyph indices
     * for Type 1 fonts. Use GetGlyphIndices for Type 1 fonts. */
    if (scaled_font->is_type1)
	 return _cairo_win32_scaled_font_type1_text_to_glyphs (scaled_font,
							       x,
							       y,
							       utf8,
							       glyphs,
							       num_glyphs);

    /* Compute a vector in user space along the baseline of length one logical space unit */
    x_incr = 1;
    y_incr = 0;
    cairo_matrix_transform_distance (&scaled_font->base.font_matrix, &x_incr, &y_incr);
    x_incr /= scaled_font->logical_scale;
    y_incr /= scaled_font->logical_scale;

    status = _cairo_utf8_to_utf16 (utf8, -1, &utf16, &n16);
    if (status)
	return status;

    gcp_results.lStructSize = sizeof (GCP_RESULTS);
    gcp_results.lpOutString = NULL;
    gcp_results.lpOrder = NULL;
    gcp_results.lpCaretPos = NULL;
    gcp_results.lpClass = NULL;

    buffer_size = MAX (n16 * 1.2, 16);		/* Initially guess number of chars plus a few */
    if (buffer_size > INT_MAX) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto FAIL1;
    }

    hdc = _get_global_font_dc ();
    assert (hdc != NULL);

    status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
    if (status)
	goto FAIL1;

    while (TRUE) {
	if (glyph_indices) {
	    free (glyph_indices);
	    glyph_indices = NULL;
	}
	if (dx) {
	    free (dx);
	    dx = NULL;
	}

	glyph_indices = _cairo_malloc_ab (buffer_size, sizeof (WCHAR));
	dx = _cairo_malloc_ab (buffer_size, sizeof (int));
	if (!glyph_indices || !dx) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto FAIL2;
	}

	gcp_results.nGlyphs = buffer_size;
	gcp_results.lpDx = dx;
	gcp_results.lpGlyphs = glyph_indices;

	if (!GetCharacterPlacementW (hdc, utf16, n16,
				     0,
				     &gcp_results,
				     GCP_DIACRITIC | GCP_LIGATE | GCP_GLYPHSHAPE | GCP_REORDER)) {
	    status = _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_text_to_glyphs");
	    goto FAIL2;
	}

	if (gcp_results.lpDx && gcp_results.lpGlyphs)
	    break;

	/* Too small a buffer, try again */

	buffer_size += buffer_size / 2;
	if (buffer_size > INT_MAX) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto FAIL2;
	}
    }

    *num_glyphs = gcp_results.nGlyphs;
    *glyphs = _cairo_malloc_ab (gcp_results.nGlyphs, sizeof (cairo_glyph_t));
    if (!*glyphs) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto FAIL2;
    }

    x_pos = x;
    y_pos = y;

    for (i = 0; i < gcp_results.nGlyphs; i++) {
	(*glyphs)[i].index = glyph_indices[i];
	(*glyphs)[i].x = x_pos ;
	(*glyphs)[i].y = y_pos;

	x_pos += x_incr * dx[i];
	y_pos += y_incr * dx[i];
    }

 FAIL2:
    if (glyph_indices)
	free (glyph_indices);
    if (dx)
	free (dx);

    cairo_win32_scaled_font_done_font (&scaled_font->base);

 FAIL1:
    free (utf16);

    return status;
}

static unsigned long
_cairo_win32_scaled_font_ucs4_to_index (void		*abstract_font,
					uint32_t	 ucs4)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    wchar_t unicode[2];
    WORD glyph_index;
    HDC hdc = NULL;
    cairo_status_t status;

    hdc = _get_global_font_dc ();
    assert (hdc != NULL);

    status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
    if (status)
	return 0;

    unicode[0] = ucs4;
    unicode[1] = 0;
    if (GetGlyphIndicesW (hdc, unicode, 1, &glyph_index, 0) == GDI_ERROR) {
	_cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_ucs4_to_index:GetGlyphIndicesW");
	glyph_index = 0;
    }

    cairo_win32_scaled_font_done_font (&scaled_font->base);

    return glyph_index;
}

static cairo_status_t
_cairo_win32_scaled_font_set_metrics (cairo_win32_scaled_font_t *scaled_font)
{
    cairo_status_t status;
    cairo_font_extents_t extents;

    TEXTMETRIC metrics;
    HDC hdc;

    hdc = _get_global_font_dc ();
    assert (hdc != NULL);

    if (scaled_font->preserve_axes || scaled_font->base.options.hint_metrics == CAIRO_HINT_METRICS_OFF) {
	/* For 90-degree rotations (including 0), we get the metrics
	 * from the GDI in logical space, then convert back to font space
	 */
	status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
	if (status)
	    return status;
	GetTextMetrics (hdc, &metrics);
	cairo_win32_scaled_font_done_font (&scaled_font->base);

	extents.ascent = metrics.tmAscent / scaled_font->logical_scale;
	extents.descent = metrics.tmDescent / scaled_font->logical_scale;

	extents.height = (metrics.tmHeight + metrics.tmExternalLeading) / scaled_font->logical_scale;
	extents.max_x_advance = metrics.tmMaxCharWidth / scaled_font->logical_scale;
	extents.max_y_advance = 0;

    } else {
	/* For all other transformations, we use the design metrics
	 * of the font. The GDI results from GetTextMetrics() on a
	 * transformed font are inexplicably large and we want to
	 * avoid them.
	 */
	status = _cairo_win32_scaled_font_select_unscaled_font (&scaled_font->base, hdc);
	if (status)
	    return status;
	GetTextMetrics (hdc, &metrics);
	_cairo_win32_scaled_font_done_unscaled_font (&scaled_font->base);

	extents.ascent = (double)metrics.tmAscent / scaled_font->em_square;
	extents.descent = (double)metrics.tmDescent / scaled_font->em_square;
	extents.height = (double)(metrics.tmHeight + metrics.tmExternalLeading) / scaled_font->em_square;
	extents.max_x_advance = (double)(metrics.tmMaxCharWidth) / scaled_font->em_square;
	extents.max_y_advance = 0;

    }

    scaled_font->is_bitmap = !(metrics.tmPitchAndFamily & TMPF_VECTOR);

    /* Need to determine if this is a Type 1 font for the special
     * handling in _text_to_glyphs.  Unlike TrueType or OpenType,
     * Type1 fonts do not have a "cmap" table (or any other table).
     * However GetFontData() will retrieve a Type1 font when
     * requesting that GetFontData() retrieve data from the start of
     * the file. This is to distinguish Type1 from stroke fonts such
     * as "Script" and "Modern". The TMPF_TRUETYPE test is redundant
     * but improves performance for the most common fonts.
     */
    scaled_font->is_type1 = FALSE;
    if (!(metrics.tmPitchAndFamily & TMPF_TRUETYPE) &&
	(metrics.tmPitchAndFamily & TMPF_VECTOR))
    {
	 if ((GetFontData (hdc, CMAP_TAG, 0, NULL, 0) == GDI_ERROR) &&
	     (GetFontData (hdc, 0, 0, NULL, 0) != GDI_ERROR))
	 {
	      scaled_font->is_type1 = TRUE;
	 }
    }

    return _cairo_scaled_font_set_metrics (&scaled_font->base, &extents);
}

static cairo_status_t
_cairo_win32_scaled_font_init_glyph_metrics (cairo_win32_scaled_font_t *scaled_font,
					     cairo_scaled_glyph_t      *scaled_glyph)
{
    static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    GLYPHMETRICS metrics;
    cairo_status_t status;
    cairo_text_extents_t extents;
    HDC hdc;

    hdc = _get_global_font_dc ();
    assert (hdc != NULL);

    if (scaled_font->is_bitmap) {
	/* GetGlyphOutline will not work. Assume that the glyph does not extend outside the font box. */
	cairo_font_extents_t font_extents;
	INT width = 0;
	UINT charIndex = _cairo_scaled_glyph_index (scaled_glyph);

	cairo_scaled_font_extents (&scaled_font->base, &font_extents);

	status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
	if (status)
	    return status;

	if (!GetCharWidth32(hdc, charIndex, charIndex, &width)) {
	    status = _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_init_glyph_metrics:GetCharWidth32");
	    width = 0;
	}
	cairo_win32_scaled_font_done_font (&scaled_font->base);
	if (status)
	    return status;

	extents.x_bearing = 0;
	extents.y_bearing = scaled_font->base.ctm.yy * (-font_extents.ascent / scaled_font->y_scale);
	extents.width = width / (WIN32_FONT_LOGICAL_SCALE * scaled_font->x_scale);
	extents.height = scaled_font->base.ctm.yy * (font_extents.ascent + font_extents.descent) / scaled_font->y_scale;
	extents.x_advance = extents.width;
	extents.y_advance = 0;
    } else if (scaled_font->preserve_axes && scaled_font->base.options.hint_metrics != CAIRO_HINT_METRICS_OFF) {
	/* If we aren't rotating / skewing the axes, then we get the metrics
	 * from the GDI in device space and convert to font space.
	 */
	status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
	if (status)
	    return status;

	if (GetGlyphOutlineW (hdc, _cairo_scaled_glyph_index (scaled_glyph),
			      GGO_METRICS | GGO_GLYPH_INDEX,
			      &metrics, 0, NULL, &matrix) == GDI_ERROR) {
	    memset (&metrics, 0, sizeof (GLYPHMETRICS));
	} else {
	    if (metrics.gmBlackBoxX > 0 && scaled_font->base.options.antialias != CAIRO_ANTIALIAS_NONE) {
		/* The bounding box reported by Windows supposedly contains the glyph's "black" area;
		 * however, antialiasing (especially with ClearType) means that the actual image that
		 * needs to be rendered may "bleed" into the adjacent pixels, mainly on the right side.
		 * To avoid clipping the glyphs when drawn by _cairo_surface_fallback_show_glyphs,
		 * for example, or other code that uses glyph extents to determine the area to update,
		 * we add a pixel of "slop" to left side of the nominal "black" area returned by GDI,
		 * and two pixels to the right (as tests show some glyphs bleed into this column).
		 */
		metrics.gmptGlyphOrigin.x -= 1;
		metrics.gmBlackBoxX += 3;
	    }
	}
	cairo_win32_scaled_font_done_font (&scaled_font->base);

	if (scaled_font->swap_axes) {
	    extents.x_bearing = - metrics.gmptGlyphOrigin.y / scaled_font->y_scale;
	    extents.y_bearing = metrics.gmptGlyphOrigin.x / scaled_font->x_scale;
	    extents.width = metrics.gmBlackBoxY / scaled_font->y_scale;
	    extents.height = metrics.gmBlackBoxX / scaled_font->x_scale;
	    extents.x_advance = metrics.gmCellIncY / scaled_font->x_scale;
	    extents.y_advance = metrics.gmCellIncX / scaled_font->y_scale;
	} else {
	    extents.x_bearing = metrics.gmptGlyphOrigin.x / scaled_font->x_scale;
	    extents.y_bearing = - metrics.gmptGlyphOrigin.y / scaled_font->y_scale;
	    extents.width = metrics.gmBlackBoxX / scaled_font->x_scale;
	    extents.height = metrics.gmBlackBoxY / scaled_font->y_scale;
	    extents.x_advance = metrics.gmCellIncX / scaled_font->x_scale;
	    extents.y_advance = metrics.gmCellIncY / scaled_font->y_scale;
	}

	if (scaled_font->swap_x) {
	    extents.x_bearing = (- extents.x_bearing - extents.width);
	    extents.x_advance = - extents.x_advance;
	}

	if (scaled_font->swap_y) {
	    extents.y_bearing = (- extents.y_bearing - extents.height);
	    extents.y_advance = - extents.y_advance;
	}

    } else {
	/* For all other transformations, we use the design metrics
	 * of the font.
	 */
	status = _cairo_win32_scaled_font_select_unscaled_font (&scaled_font->base, hdc);
	if (status)
	    return status;

	if (GetGlyphOutlineW (hdc, _cairo_scaled_glyph_index (scaled_glyph),
	                      GGO_METRICS | GGO_GLYPH_INDEX,
			      &metrics, 0, NULL, &matrix) == GDI_ERROR) {
	    memset (&metrics, 0, sizeof (GLYPHMETRICS));
	}
	_cairo_win32_scaled_font_done_unscaled_font (&scaled_font->base);

	extents.x_bearing = (double)metrics.gmptGlyphOrigin.x / scaled_font->em_square;
	extents.y_bearing = - (double)metrics.gmptGlyphOrigin.y / scaled_font->em_square;
	extents.width = (double)metrics.gmBlackBoxX / scaled_font->em_square;
	extents.height = (double)metrics.gmBlackBoxY / scaled_font->em_square;
	extents.x_advance = (double)metrics.gmCellIncX / scaled_font->em_square;
	extents.y_advance = (double)metrics.gmCellIncY / scaled_font->em_square;
    }

    _cairo_scaled_glyph_set_metrics (scaled_glyph,
				     &scaled_font->base,
				     &extents);

    return CAIRO_STATUS_SUCCESS;
}

/* Not currently used code, but may be useful in the future if we add
 * back the capability to the scaled font backend interface to get the
 * actual device space bbox rather than computing it from the
 * font-space metrics.
 */
#if 0
static cairo_status_t
_cairo_win32_scaled_font_glyph_bbox (void		 *abstract_font,
				     const cairo_glyph_t *glyphs,
				     int                  num_glyphs,
				     cairo_box_t         *bbox)
{
    static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    int x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    if (num_glyphs > 0) {
	HDC hdc;
	GLYPHMETRICS metrics;
	cairo_status_t status;
	int i;

	hdc = _get_global_font_dc ();
	assert (hdc != NULL);

	status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
	if (status)
	    return status;

	for (i = 0; i < num_glyphs; i++) {
	    int x = _cairo_lround (glyphs[i].x);
	    int y = _cairo_lround (glyphs[i].y);

	    GetGlyphOutlineW (hdc, glyphs[i].index, GGO_METRICS | GGO_GLYPH_INDEX,
			     &metrics, 0, NULL, &matrix);

	    if (i == 0 || x1 > x + metrics.gmptGlyphOrigin.x)
		x1 = x + metrics.gmptGlyphOrigin.x;
	    if (i == 0 || y1 > y - metrics.gmptGlyphOrigin.y)
		y1 = y - metrics.gmptGlyphOrigin.y;
	    if (i == 0 || x2 < x + metrics.gmptGlyphOrigin.x + (int)metrics.gmBlackBoxX)
		x2 = x + metrics.gmptGlyphOrigin.x + (int)metrics.gmBlackBoxX;
	    if (i == 0 || y2 < y - metrics.gmptGlyphOrigin.y + (int)metrics.gmBlackBoxY)
		y2 = y - metrics.gmptGlyphOrigin.y + (int)metrics.gmBlackBoxY;
	}

	cairo_win32_scaled_font_done_font (&scaled_font->base);
    }

    bbox->p1.x = _cairo_fixed_from_int (x1);
    bbox->p1.y = _cairo_fixed_from_int (y1);
    bbox->p2.x = _cairo_fixed_from_int (x2);
    bbox->p2.y = _cairo_fixed_from_int (y2);

    return CAIRO_STATUS_SUCCESS;
}
#endif

typedef struct {
    cairo_win32_scaled_font_t *scaled_font;
    HDC hdc;

    cairo_array_t glyphs;
    cairo_array_t dx;

    int start_x;
    int last_x;
    int last_y;
} cairo_glyph_state_t;

static void
_start_glyphs (cairo_glyph_state_t        *state,
	       cairo_win32_scaled_font_t  *scaled_font,
	       HDC                         hdc)
{
    state->hdc = hdc;
    state->scaled_font = scaled_font;

    _cairo_array_init (&state->glyphs, sizeof (WCHAR));
    _cairo_array_init (&state->dx, sizeof (int));
}

static cairo_status_t
_flush_glyphs (cairo_glyph_state_t *state)
{
    cairo_status_t status;
    int dx = 0;
    WCHAR * elements;
    int * dx_elements;

    status = _cairo_array_append (&state->dx, &dx);
    if (status)
	return status;

    elements = _cairo_array_index (&state->glyphs, 0);
    dx_elements = _cairo_array_index (&state->dx, 0);
    if (!ExtTextOutW (state->hdc,
		      state->start_x, state->last_y,
		      ETO_GLYPH_INDEX,
		      NULL,
		      elements,
		      state->glyphs.num_elements,
		      dx_elements)) {
	return _cairo_win32_print_gdi_error ("_flush_glyphs");
    }

    _cairo_array_truncate (&state->glyphs, 0);
    _cairo_array_truncate (&state->dx, 0);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_add_glyph (cairo_glyph_state_t *state,
	    unsigned long        index,
	    double               device_x,
	    double               device_y)
{
    cairo_status_t status;
    double user_x = device_x;
    double user_y = device_y;
    WCHAR glyph_index = index;
    int logical_x, logical_y;

    cairo_matrix_transform_point (&state->scaled_font->device_to_logical, &user_x, &user_y);

    logical_x = _cairo_lround (user_x);
    logical_y = _cairo_lround (user_y);

    if (state->glyphs.num_elements > 0) {
	int dx;

	if (logical_y != state->last_y) {
	    status = _flush_glyphs (state);
	    if (status)
		return status;
	    state->start_x = logical_x;
	} else {
	    dx = logical_x - state->last_x;
	    status = _cairo_array_append (&state->dx, &dx);
	    if (status)
		return status;
	}
    } else {
	state->start_x = logical_x;
    }

    state->last_x = logical_x;
    state->last_y = logical_y;

    status = _cairo_array_append (&state->glyphs, &glyph_index);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_finish_glyphs (cairo_glyph_state_t *state)
{
    cairo_status_t status;

    status = _flush_glyphs (state);

    _cairo_array_fini (&state->glyphs);
    _cairo_array_fini (&state->dx);

    return status;
}

static cairo_status_t
_draw_glyphs_on_surface (cairo_win32_surface_t     *surface,
			 cairo_win32_scaled_font_t *scaled_font,
			 COLORREF                   color,
			 int                        x_offset,
			 int                        y_offset,
			 const cairo_glyph_t       *glyphs,
			 int			    num_glyphs)
{
    cairo_glyph_state_t state;
    cairo_status_t status, status2;
    int i;

    if (!SaveDC (surface->dc))
	return _cairo_win32_print_gdi_error ("_draw_glyphs_on_surface:SaveDC");

    status = cairo_win32_scaled_font_select_font (&scaled_font->base, surface->dc);
    if (status)
	goto FAIL1;

    SetTextColor (surface->dc, color);
    SetTextAlign (surface->dc, TA_BASELINE | TA_LEFT);
    SetBkMode (surface->dc, TRANSPARENT);

    _start_glyphs (&state, scaled_font, surface->dc);

    for (i = 0; i < num_glyphs; i++) {
	status = _add_glyph (&state, glyphs[i].index,
			     glyphs[i].x - x_offset, glyphs[i].y - y_offset);
	if (status)
	    goto FAIL2;
    }

 FAIL2:
    status2 = _finish_glyphs (&state);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    cairo_win32_scaled_font_done_font (&scaled_font->base);
 FAIL1:
    RestoreDC (surface->dc, -1);

    return status;
}

/* Duplicate the green channel of a 4-channel mask in the alpha channel, then
 * invert the whole mask.
 */
static void
_compute_argb32_mask_alpha (cairo_win32_surface_t *mask_surface)
{
    cairo_image_surface_t *image = (cairo_image_surface_t *)mask_surface->image;
    int i, j;

    for (i = 0; i < image->height; i++) {
	uint32_t *p = (uint32_t *) (image->data + i * image->stride);
	for (j = 0; j < image->width; j++) {
	    *p = 0xffffffff ^ (*p | ((*p & 0x0000ff00) << 16));
	    p++;
	}
    }
}

/* Invert a mask
 */
static void
_invert_argb32_mask (cairo_win32_surface_t *mask_surface)
{
    cairo_image_surface_t *image = (cairo_image_surface_t *)mask_surface->image;
    int i, j;

    for (i = 0; i < image->height; i++) {
	uint32_t *p = (uint32_t *) (image->data + i * image->stride);
	for (j = 0; j < image->width; j++) {
	    *p = 0xffffffff ^ *p;
	    p++;
	}
    }
}

/* Compute an alpha-mask from a monochrome RGB24 image
 */
static cairo_surface_t *
_compute_a8_mask (cairo_win32_surface_t *mask_surface)
{
    cairo_image_surface_t *image24 = (cairo_image_surface_t *)mask_surface->image;
    cairo_image_surface_t *image8;
    int i, j;

    if (image24->base.status)
	return cairo_surface_reference (&image24->base);

    image8 = (cairo_image_surface_t *)cairo_image_surface_create (CAIRO_FORMAT_A8,
								  image24->width, image24->height);
    if (image8->base.status)
	return &image8->base;

    for (i = 0; i < image24->height; i++) {
	uint32_t *p = (uint32_t *) (image24->data + i * image24->stride);
	unsigned char *q = (unsigned char *) (image8->data + i * image8->stride);

	for (j = 0; j < image24->width; j++) {
	    *q = 255 - ((*p & 0x0000ff00) >> 8);
	    p++;
	    q++;
	}
    }

    return &image8->base;
}

static cairo_int_status_t
_cairo_win32_scaled_font_glyph_init (void		       *abstract_font,
				     cairo_scaled_glyph_t      *scaled_glyph,
				     cairo_scaled_glyph_info_t  info)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    cairo_status_t status;

    if ((info & CAIRO_SCALED_GLYPH_INFO_METRICS) != 0) {
	status = _cairo_win32_scaled_font_init_glyph_metrics (scaled_font, scaled_glyph);
	if (status)
	    return status;
    }

    if (info & CAIRO_SCALED_GLYPH_INFO_SURFACE) {
	status = _cairo_win32_scaled_font_init_glyph_surface (scaled_font, scaled_glyph);
	if (status)
	    return status;
    }

    if ((info & CAIRO_SCALED_GLYPH_INFO_PATH) != 0) {
	status = _cairo_win32_scaled_font_init_glyph_path (scaled_font, scaled_glyph);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_win32_scaled_font_show_glyphs (void			*abstract_font,
				      cairo_operator_t		 op,
				      const cairo_pattern_t	*pattern,
				      cairo_surface_t		*generic_surface,
				      int			 source_x,
				      int			 source_y,
				      int			 dest_x,
				      int			 dest_y,
				      unsigned int		 width,
				      unsigned int		 height,
				      cairo_glyph_t		*glyphs,
				      int			 num_glyphs,
				      cairo_region_t		*clip_region,
				      int			*remaining_glyphs)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)generic_surface;
    cairo_status_t status;

    if (width == 0 || height == 0)
	return CAIRO_STATUS_SUCCESS;

    if (_cairo_surface_is_win32 (generic_surface) &&
	surface->format == CAIRO_FORMAT_RGB24 &&
	(generic_surface->permit_subpixel_antialiasing || scaled_font->quality != CLEARTYPE_QUALITY) &&
	op == CAIRO_OPERATOR_OVER &&
	_cairo_pattern_is_opaque_solid (pattern)) {

	cairo_solid_pattern_t *solid_pattern = (cairo_solid_pattern_t *)pattern;

	/* When compositing OVER on a GDI-understood surface, with a
	 * solid opaque color, we can just call ExtTextOut directly.
	 */
	COLORREF new_color;

	status = _cairo_win32_surface_set_clip_region (surface, clip_region);
	if (unlikely (status))
	    return status;

	new_color = RGB (((int)solid_pattern->color.red_short) >> 8,
			 ((int)solid_pattern->color.green_short) >> 8,
			 ((int)solid_pattern->color.blue_short) >> 8);

	return _draw_glyphs_on_surface (surface, scaled_font, new_color,
					0, 0,
					glyphs, num_glyphs);
    } else {
	/* Otherwise, we need to draw using software fallbacks. We create a mask
	 * surface by drawing the the glyphs onto a DIB, black-on-white then
	 * inverting. GDI outputs gamma-corrected images so inverted black-on-white
	 * is very different from white-on-black. We favor the more common
	 * case where the final output is dark-on-light.
	 */
	cairo_win32_surface_t *tmp_surface;
	cairo_surface_t *mask_surface;
	cairo_surface_pattern_t mask;
	cairo_bool_t use_subpixel_antialiasing =
	    scaled_font->quality == CLEARTYPE_QUALITY && generic_surface->permit_subpixel_antialiasing;
	RECT r;

	tmp_surface = (cairo_win32_surface_t *)cairo_win32_surface_create_with_dib (CAIRO_FORMAT_ARGB32, width, height);
	if (tmp_surface->base.status)
	    return tmp_surface->base.status;

	r.left = 0;
	r.top = 0;
	r.right = width;
	r.bottom = height;
	FillRect (tmp_surface->dc, &r, GetStockObject (WHITE_BRUSH));

	status = _draw_glyphs_on_surface (tmp_surface,
		                          scaled_font, RGB (0, 0, 0),
					  dest_x, dest_y,
					  glyphs, num_glyphs);
	if (status) {
	    cairo_surface_destroy (&tmp_surface->base);
	    return status;
	}

	if (use_subpixel_antialiasing) {
	    /* For ClearType, we need a 4-channel mask. If we are compositing on
	     * a surface with alpha, we need to compute the alpha channel of
	     * the mask (we just copy the green channel). But for a destination
	     * surface without alpha the alpha channel of the mask is ignored
	     */

	    if (surface->format != CAIRO_FORMAT_RGB24)
		_compute_argb32_mask_alpha (tmp_surface);
	    else
		_invert_argb32_mask (tmp_surface);

	    mask_surface = &tmp_surface->base;
	} else {
	    mask_surface = _compute_a8_mask (tmp_surface);
	    cairo_surface_destroy (&tmp_surface->base);
	    status = mask_surface->status;
	    if (status)
		return status;
	}

	/* For op == OVER, no-cleartype, a possible optimization here is to
	 * draw onto an intermediate ARGB32 surface and alpha-blend that with the
	 * destination
	 */
	_cairo_pattern_init_for_surface (&mask, mask_surface);
	cairo_surface_destroy (mask_surface);

	if (use_subpixel_antialiasing)
	    mask.base.has_component_alpha = TRUE;

	status = _cairo_surface_composite (op, pattern,
					   &mask.base,
					   &surface->base,
					   source_x, source_y,
					   0, 0,
					   dest_x, dest_y,
					   width, height,
					   clip_region);

	_cairo_pattern_fini (&mask.base);

	return status;
    }
}

static cairo_int_status_t
_cairo_win32_scaled_font_load_truetype_table (void	       *abstract_font,
                                             unsigned long      tag,
                                             long               offset,
                                             unsigned char     *buffer,
                                             unsigned long     *length)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    HDC hdc;
    cairo_status_t status;

    hdc = _get_global_font_dc ();
    assert (hdc != NULL);

    tag = (tag&0x000000ff)<<24 | (tag&0x0000ff00)<<8 | (tag&0x00ff0000)>>8 | (tag&0xff000000)>>24;
    status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
    if (status)
	return status;

    *length = GetFontData (hdc, tag, offset, buffer, *length);
    if (*length == GDI_ERROR)
        status = CAIRO_INT_STATUS_UNSUPPORTED;

    cairo_win32_scaled_font_done_font (&scaled_font->base);

    return status;
}

static cairo_int_status_t
_cairo_win32_scaled_font_index_to_ucs4 (void		*abstract_font,
					unsigned long    index,
					uint32_t	*ucs4)
{
    cairo_win32_scaled_font_t *scaled_font = abstract_font;
    GLYPHSET *glyph_set;
    uint16_t *utf16 = NULL;
    WORD *glyph_indices = NULL;
    HDC hdc = NULL;
    int res;
    unsigned int i, j, num_glyphs;
    cairo_status_t status;

    hdc = _get_global_font_dc ();
    assert (hdc != NULL);

    status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
    if (status)
	return status;

    res = GetFontUnicodeRanges(hdc, NULL);
    if (res == 0) {
	status = _cairo_win32_print_gdi_error (
	    "_cairo_win32_scaled_font_index_to_ucs4:GetFontUnicodeRanges");
	goto exit1;
    }

    glyph_set = malloc (res);
    if (glyph_set == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto exit1;
    }

    res = GetFontUnicodeRanges(hdc, glyph_set);
    if (res == 0) {
	status = _cairo_win32_print_gdi_error (
	    "_cairo_win32_scaled_font_index_to_ucs4:GetFontUnicodeRanges");
	goto exit1;
    }

    *ucs4 = (uint32_t) -1;
    for (i = 0; i < glyph_set->cRanges; i++) {
	num_glyphs = glyph_set->ranges[i].cGlyphs;

	utf16 = _cairo_malloc_ab (num_glyphs + 1, sizeof (uint16_t));
	if (utf16 == NULL) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto exit1;
	}

	glyph_indices = _cairo_malloc_ab (num_glyphs + 1, sizeof (WORD));
	if (glyph_indices == NULL) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto exit2;
	}

	for (j = 0; j < num_glyphs; j++)
	    utf16[j] = glyph_set->ranges[i].wcLow + j;
	utf16[j] = 0;

	if (GetGlyphIndicesW (hdc, utf16, num_glyphs, glyph_indices, 0) == GDI_ERROR) {
	    status = _cairo_win32_print_gdi_error (
		"_cairo_win32_scaled_font_index_to_ucs4:GetGlyphIndicesW");
	    goto exit2;
	}

	for (j = 0; j < num_glyphs; j++) {
	    if (glyph_indices[j] == index) {
		*ucs4 = utf16[j];
		goto exit2;
	    }
	}

	free (glyph_indices);
	glyph_indices = NULL;
	free (utf16);
	utf16 = NULL;
    }

exit2:
    if (glyph_indices)
	free (glyph_indices);
    if (utf16)
	free (utf16);
    free (glyph_set);
exit1:
    cairo_win32_scaled_font_done_font (&scaled_font->base);

    return status;
}

static cairo_status_t
_cairo_win32_scaled_font_init_glyph_surface (cairo_win32_scaled_font_t *scaled_font,
                                             cairo_scaled_glyph_t      *scaled_glyph)
{
    cairo_status_t status;
    cairo_glyph_t glyph;
    cairo_win32_surface_t *surface;
    cairo_t *cr;
    cairo_surface_t *image;
    int width, height;
    int x1, y1, x2, y2;

    x1 = _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.x);
    y1 = _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.y);
    x2 = _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.x);
    y2 = _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.y);
    width = x2 - x1;
    height = y2 - y1;

    surface = (cairo_win32_surface_t *)
	cairo_win32_surface_create_with_dib (CAIRO_FORMAT_RGB24, width, height);

    cr = cairo_create (&surface->base);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_paint (cr);
    status = cairo_status (cr);
    cairo_destroy(cr);
    if (status)
	goto FAIL;

    glyph.index = _cairo_scaled_glyph_index (scaled_glyph);
    glyph.x = -x1;
    glyph.y = -y1;
    status = _draw_glyphs_on_surface (surface, scaled_font, RGB(0,0,0),
                                      0, 0, &glyph, 1);
    if (status)
	goto FAIL;

    GdiFlush();

    image = _compute_a8_mask (surface);
    status = image->status;
    if (status)
	goto FAIL;

    cairo_surface_set_device_offset (image, -x1, -y1);
    _cairo_scaled_glyph_set_surface (scaled_glyph,
                                     &scaled_font->base,
                                     (cairo_image_surface_t *) image);

  FAIL:
    cairo_surface_destroy (&surface->base);

    return status;
}

static void
_cairo_win32_transform_FIXED_to_fixed (cairo_matrix_t *matrix,
                                       FIXED Fx, FIXED Fy,
                                       cairo_fixed_t *fx, cairo_fixed_t *fy)
{
    double x = Fx.value + Fx.fract / 65536.0;
    double y = Fy.value + Fy.fract / 65536.0;
    cairo_matrix_transform_point (matrix, &x, &y);
    *fx = _cairo_fixed_from_double (x);
    *fy = _cairo_fixed_from_double (y);
}

static cairo_status_t
_cairo_win32_scaled_font_init_glyph_path (cairo_win32_scaled_font_t *scaled_font,
					  cairo_scaled_glyph_t      *scaled_glyph)
{
    static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, -1 } };
    cairo_status_t status;
    GLYPHMETRICS metrics;
    HDC hdc;
    DWORD bytesGlyph;
    unsigned char *buffer, *ptr;
    cairo_path_fixed_t *path;
    cairo_matrix_t transform;
    cairo_fixed_t x, y;

    if (scaled_font->is_bitmap)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    hdc = _get_global_font_dc ();
    assert (hdc != NULL);

    path = _cairo_path_fixed_create ();
    if (!path)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (scaled_font->base.options.hint_style == CAIRO_HINT_STYLE_NONE) {
        status = _cairo_win32_scaled_font_select_unscaled_font (&scaled_font->base, hdc);
        transform = scaled_font->base.scale;
        cairo_matrix_scale (&transform, 1.0/scaled_font->em_square, 1.0/scaled_font->em_square);
    } else {
        status = cairo_win32_scaled_font_select_font (&scaled_font->base, hdc);
        cairo_matrix_init_identity(&transform);
    }
    if (status)
        goto CLEANUP_PATH;

    bytesGlyph = GetGlyphOutlineW (hdc, _cairo_scaled_glyph_index (scaled_glyph),
				   GGO_NATIVE | GGO_GLYPH_INDEX,
				   &metrics, 0, NULL, &matrix);

    if (bytesGlyph == GDI_ERROR) {
	status = _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_glyph_path");
	goto CLEANUP_FONT;
    }

    ptr = buffer = malloc (bytesGlyph);
    if (!buffer) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_FONT;
    }

    if (GetGlyphOutlineW (hdc, _cairo_scaled_glyph_index (scaled_glyph),
			  GGO_NATIVE | GGO_GLYPH_INDEX,
			  &metrics, bytesGlyph, buffer, &matrix) == GDI_ERROR) {
	status = _cairo_win32_print_gdi_error ("_cairo_win32_scaled_font_glyph_path");
	goto CLEANUP_BUFFER;
    }

    while (ptr < buffer + bytesGlyph) {
	TTPOLYGONHEADER *header = (TTPOLYGONHEADER *)ptr;
	unsigned char *endPoly = ptr + header->cb;

	ptr += sizeof (TTPOLYGONHEADER);

        _cairo_win32_transform_FIXED_to_fixed (&transform,
                                               header->pfxStart.x,
                                               header->pfxStart.y,
                                               &x, &y);
        status = _cairo_path_fixed_move_to (path, x, y);
	if (status)
	    goto CLEANUP_BUFFER;

	while (ptr < endPoly) {
	    TTPOLYCURVE *curve = (TTPOLYCURVE *)ptr;
	    POINTFX *points = curve->apfx;
	    int i;
	    switch (curve->wType) {
	    case TT_PRIM_LINE:
		for (i = 0; i < curve->cpfx; i++) {
                    _cairo_win32_transform_FIXED_to_fixed (&transform,
                                                           points[i].x,
                                                           points[i].y,
                                                           &x, &y);
		    status = _cairo_path_fixed_line_to (path, x, y);
		    if (status)
			goto CLEANUP_BUFFER;
		}
		break;
	    case TT_PRIM_QSPLINE:
		for (i = 0; i < curve->cpfx - 1; i++) {
		    cairo_fixed_t p1x, p1y, p2x, p2y, cx, cy, c1x, c1y, c2x, c2y;
		    if (! _cairo_path_fixed_get_current_point (path, &p1x, &p1y))
			goto CLEANUP_BUFFER;
                    _cairo_win32_transform_FIXED_to_fixed (&transform,
                                                           points[i].x,
                                                           points[i].y,
                                                           &cx, &cy);

		    if (i + 1 == curve->cpfx - 1) {
                        _cairo_win32_transform_FIXED_to_fixed (&transform,
                                                               points[i + 1].x,
                                                               points[i + 1].y,
                                                               &p2x, &p2y);
		    } else {
			/* records with more than one curve use interpolation for
			   control points, per http://support.microsoft.com/kb/q87115/ */
                        _cairo_win32_transform_FIXED_to_fixed (&transform,
                                                               points[i + 1].x,
                                                               points[i + 1].y,
                                                               &x, &y);
                        p2x = (cx + x) / 2;
			p2y = (cy + y) / 2;
		    }

		    c1x = 2 * cx / 3 + p1x / 3;
		    c1y = 2 * cy / 3 + p1y / 3;
		    c2x = 2 * cx / 3 + p2x / 3;
		    c2y = 2 * cy / 3 + p2y / 3;

		    status = _cairo_path_fixed_curve_to (path, c1x, c1y, c2x, c2y, p2x, p2y);
		    if (status)
			goto CLEANUP_BUFFER;
		}
		break;
	    case TT_PRIM_CSPLINE:
		for (i = 0; i < curve->cpfx - 2; i += 2) {
		    cairo_fixed_t x1, y1, x2, y2;
                    _cairo_win32_transform_FIXED_to_fixed (&transform,
                                                           points[i].x,
                                                           points[i].y,
                                                           &x, &y);
                    _cairo_win32_transform_FIXED_to_fixed (&transform,
                                                           points[i + 1].x,
                                                           points[i + 1].y,
                                                           &x1, &y1);
                    _cairo_win32_transform_FIXED_to_fixed (&transform,
                                                           points[i + 2].x,
                                                           points[i + 2].y,
                                                           &x2, &y2);
		    status = _cairo_path_fixed_curve_to (path, x, y, x1, y1, x2, y2);
		    if (status)
			goto CLEANUP_BUFFER;
		}
		break;
	    }
	    ptr += sizeof(TTPOLYCURVE) + sizeof (POINTFX) * (curve->cpfx - 1);
	}
	status = _cairo_path_fixed_close_path (path);
	if (status)
	    goto CLEANUP_BUFFER;
    }

    _cairo_scaled_glyph_set_path (scaled_glyph,
				  &scaled_font->base,
				  path);

 CLEANUP_BUFFER:
    free (buffer);

 CLEANUP_FONT:
    if (scaled_font->base.options.hint_style == CAIRO_HINT_STYLE_NONE)
	_cairo_win32_scaled_font_done_unscaled_font (&scaled_font->base);
    else
	cairo_win32_scaled_font_done_font (&scaled_font->base);

 CLEANUP_PATH:
    if (status != CAIRO_STATUS_SUCCESS)
	_cairo_path_fixed_destroy (path);

    return status;
}

const cairo_scaled_font_backend_t _cairo_win32_scaled_font_backend = {
    CAIRO_FONT_TYPE_WIN32,
    _cairo_win32_scaled_font_fini,
    _cairo_win32_scaled_font_glyph_init,
    NULL, /* _cairo_win32_scaled_font_text_to_glyphs, FIXME */
    _cairo_win32_scaled_font_ucs4_to_index,
    _cairo_win32_scaled_font_show_glyphs,
    _cairo_win32_scaled_font_load_truetype_table,
    _cairo_win32_scaled_font_index_to_ucs4,
};

/* #cairo_win32_font_face_t */

typedef struct _cairo_win32_font_face cairo_win32_font_face_t;

/* If hfont is non-%NULL then logfont->lfHeight must be -S for some S,
 * logfont->lfWidth, logfont->lfEscapement, logfont->lfOrientation must
 * all be 0, and hfont is the result of calling CreateFontIndirectW on
 * logfont.
 */
struct _cairo_win32_font_face {
    cairo_font_face_t base;
    LOGFONTW logfont;
    HFONT hfont;
};

/* implement the platform-specific interface */

static void
_cairo_win32_font_face_destroy (void *abstract_face);

static cairo_bool_t
_is_scale (const cairo_matrix_t *matrix, double scale)
{
    return matrix->xx == scale && matrix->yy == scale &&
           matrix->xy == 0. && matrix->yx == 0. &&
           matrix->x0 == 0. && matrix->y0 == 0.;
}

static cairo_status_t
_cairo_win32_font_face_scaled_font_create (void			*abstract_face,
					   const cairo_matrix_t	*font_matrix,
					   const cairo_matrix_t	*ctm,
					   const cairo_font_options_t *options,
					   cairo_scaled_font_t **font)
{
    HFONT hfont = NULL;

    cairo_win32_font_face_t *font_face = abstract_face;

    if (font_face->hfont) {
        /* Check whether it's OK to go ahead and use the font-face's HFONT. */
        if (_is_scale (ctm, 1.) &&
            _is_scale (font_matrix, -font_face->logfont.lfHeight)) {
            hfont = font_face->hfont;
        }
    }

    return _win32_scaled_font_create (&font_face->logfont,
				      hfont,
				      &font_face->base,
				      font_matrix, ctm, options,
				      font);
}

const cairo_font_face_backend_t _cairo_win32_font_face_backend = {
    CAIRO_FONT_TYPE_WIN32,
    _cairo_win32_font_face_create_for_toy,
    _cairo_win32_font_face_destroy,
    _cairo_win32_font_face_scaled_font_create
};

/* We maintain a hash table from LOGFONT,HFONT => #cairo_font_face_t.
 * The primary purpose of this mapping is to provide unique
 * #cairo_font_face_t values so that our cache and mapping from
 * #cairo_font_face_t => #cairo_scaled_font_t works. Once the
 * corresponding #cairo_font_face_t objects fall out of downstream
 * caches, we don't need them in this hash table anymore.
 *
 * Modifications to this hash table are protected by
 * _cairo_win32_font_face_mutex.
 */

static cairo_hash_table_t *cairo_win32_font_face_hash_table = NULL;

static int
_cairo_win32_font_face_keys_equal (const void *key_a,
				   const void *key_b);

static void
_cairo_win32_font_face_hash_table_destroy (void)
{
    cairo_hash_table_t *hash_table;

    /* We manually acquire the lock rather than calling
     * _cairo_win32_font_face_hash_table_lock simply to avoid creating
     * the table only to destroy it again. */
    CAIRO_MUTEX_LOCK (_cairo_win32_font_face_mutex);
    hash_table = cairo_win32_font_face_hash_table;
    cairo_win32_font_face_hash_table = NULL;
    CAIRO_MUTEX_UNLOCK (_cairo_win32_font_face_mutex);

    if (hash_table != NULL)
	_cairo_hash_table_destroy (hash_table);
}

static cairo_hash_table_t *
_cairo_win32_font_face_hash_table_lock (void)
{
    CAIRO_MUTEX_LOCK (_cairo_win32_font_face_mutex);

    if (unlikely (cairo_win32_font_face_hash_table == NULL))
    {
	cairo_win32_font_face_hash_table =
	_cairo_hash_table_create (_cairo_win32_font_face_keys_equal);

	if (unlikely (cairo_win32_font_face_hash_table == NULL)) {
	    CAIRO_MUTEX_UNLOCK (_cairo_win32_font_face_mutex);
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return NULL;
	}
    }

    return cairo_win32_font_face_hash_table;
}

static void
_cairo_win32_font_face_hash_table_unlock (void)
{
    CAIRO_MUTEX_UNLOCK (_cairo_win32_font_face_mutex);
}

static void
_cairo_win32_font_face_init_key (cairo_win32_font_face_t *key,
				 LOGFONTW                *logfont,
				 HFONT                    font)
{
    unsigned long hash = _CAIRO_HASH_INIT_VALUE;

    key->logfont = *logfont;
    key->hfont = font;

    hash = _cairo_hash_bytes (0, logfont->lfFaceName, 2*wcslen(logfont->lfFaceName));
    hash = _cairo_hash_bytes (hash, &logfont->lfWeight, sizeof(logfont->lfWeight));
    hash = _cairo_hash_bytes (hash, &logfont->lfItalic, sizeof(logfont->lfItalic));

    key->base.hash_entry.hash = hash;
}

static int
_cairo_win32_font_face_keys_equal (const void *key_a,
				   const void *key_b)
{
    const cairo_win32_font_face_t *face_a = key_a;
    const cairo_win32_font_face_t *face_b = key_b;

    if (face_a->logfont.lfWeight         == face_b->logfont.lfWeight &&
	face_a->logfont.lfItalic         == face_b->logfont.lfItalic &&
	face_a->logfont.lfUnderline      == face_b->logfont.lfUnderline &&
	face_a->logfont.lfStrikeOut      == face_b->logfont.lfStrikeOut &&
	face_a->logfont.lfCharSet        == face_b->logfont.lfCharSet &&
	face_a->logfont.lfOutPrecision   == face_b->logfont.lfOutPrecision &&
	face_a->logfont.lfClipPrecision  == face_b->logfont.lfClipPrecision &&
	face_a->logfont.lfPitchAndFamily == face_b->logfont.lfPitchAndFamily &&
	(wcscmp (face_a->logfont.lfFaceName, face_b->logfont.lfFaceName) == 0))
	return TRUE;
    else
	return FALSE;
}

static void
_cairo_win32_font_face_destroy (void *abstract_face)
{
    cairo_hash_table_t *hash_table;
    cairo_win32_font_face_t *font_face = abstract_face;

    hash_table = _cairo_win32_font_face_hash_table_lock ();
    if (hash_table) {
        _cairo_hash_table_remove (hash_table, &font_face->base.hash_entry);
        _cairo_win32_font_face_hash_table_unlock ();
    }

    if (font_face->hfont) {
        DeleteObject (font_face->hfont);
    }
}

/**
 * cairo_win32_font_face_create_for_logfontw_hfont:
 * @logfont: A #LOGFONTW structure specifying the font to use.
 *   If @font is %NULL then the lfHeight, lfWidth, lfOrientation and lfEscapement
 *   fields of this structure are ignored. Otherwise lfWidth, lfOrientation and
 *   lfEscapement must be zero.
 * @font: An #HFONT that can be used when the font matrix is a scale by
 *   -lfHeight and the CTM is identity. The cairo_font_face_t takes ownership
 *   of 'font' and is responsible for destroying it.
 *
 * Creates a new font for the Win32 font backend based on a
 * #LOGFONT. This font can then be used with
 * cairo_set_font_face() or cairo_scaled_font_create().
 * The #cairo_scaled_font_t
 * returned from cairo_scaled_font_create() is also for the Win32 backend
 * and can be used with functions such as cairo_win32_scaled_font_select_font().
 *
 * Return value: a newly created #cairo_font_face_t. Free with
 *  cairo_font_face_destroy() when you are done using it.
 **/
cairo_font_face_t *
cairo_win32_font_face_create_for_logfontw_hfont (LOGFONTW *logfont, HFONT font)
{
    cairo_win32_font_face_t *font_face, key;
    cairo_hash_table_t *hash_table;
    cairo_status_t status;

    hash_table = _cairo_win32_font_face_hash_table_lock ();
    if (unlikely (hash_table == NULL)) {
        _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
        if (font) {
            /* We are supposed to take ownership of this font. Since we don't
             * need to use it, delete it now.
             */
            DeleteObject (font);
        }
	return (cairo_font_face_t *)&_cairo_font_face_nil;
    }

    _cairo_win32_font_face_init_key (&key, logfont, font);

    /* Return existing unscaled font if it exists in the hash table. */
    font_face = _cairo_hash_table_lookup (hash_table,
					 &key.base.hash_entry);
    if (font_face != NULL) {
        if (font) {
            /* We are supposed to take ownership of this font. Since we don't
             * need to use it, delete it now.
             */
            DeleteObject (font);
        }
	cairo_font_face_reference (&font_face->base);
	goto DONE;
    }

    /* Otherwise create it and insert into hash table. */
    font_face = malloc (sizeof (cairo_win32_font_face_t));
    if (!font_face) {
        _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	goto FAIL;
    }

    _cairo_win32_font_face_init_key (font_face, logfont, font);
    _cairo_font_face_init (&font_face->base, &_cairo_win32_font_face_backend);

    assert (font_face->base.hash_entry.hash == key.base.hash_entry.hash);
    status = _cairo_hash_table_insert (hash_table,
				       &font_face->base.hash_entry);
    if (unlikely (status))
	goto FAIL;

DONE:
    _cairo_win32_font_face_hash_table_unlock ();

    return &font_face->base;

FAIL:
    _cairo_win32_font_face_hash_table_unlock ();

    if (font) {
        /* We are supposed to take ownership of this font. Since we don't
         * need to use it, delete it now.
         */
        DeleteObject (font);
    }

    return (cairo_font_face_t *)&_cairo_font_face_nil;
}

/**
 * cairo_win32_font_face_create_for_logfontw:
 * @logfont: A #LOGFONTW structure specifying the font to use.
 *   The lfHeight, lfWidth, lfOrientation and lfEscapement
 *   fields of this structure are ignored.
 *
 * Creates a new font for the Win32 font backend based on a
 * #LOGFONT. This font can then be used with
 * cairo_set_font_face() or cairo_scaled_font_create().
 * The #cairo_scaled_font_t
 * returned from cairo_scaled_font_create() is also for the Win32 backend
 * and can be used with functions such as cairo_win32_scaled_font_select_font().
 *
 * Return value: a newly created #cairo_font_face_t. Free with
 *  cairo_font_face_destroy() when you are done using it.
 **/
cairo_font_face_t *
cairo_win32_font_face_create_for_logfontw (LOGFONTW *logfont)
{
    return cairo_win32_font_face_create_for_logfontw_hfont (logfont, NULL);
}

/**
 * cairo_win32_font_face_create_for_hfont:
 * @font: An #HFONT structure specifying the font to use.
 *
 * Creates a new font for the Win32 font backend based on a
 * #HFONT. This font can then be used with
 * cairo_set_font_face() or cairo_scaled_font_create().
 * The #cairo_scaled_font_t
 * returned from cairo_scaled_font_create() is also for the Win32 backend
 * and can be used with functions such as cairo_win32_scaled_font_select_font().
 *
 * Return value: a newly created #cairo_font_face_t. Free with
 *  cairo_font_face_destroy() when you are done using it.
 **/
cairo_font_face_t *
cairo_win32_font_face_create_for_hfont (HFONT font)
{
    LOGFONTW logfont;
    GetObjectW (font, sizeof(logfont), &logfont);

    if (logfont.lfEscapement != 0 || logfont.lfOrientation != 0 ||
        logfont.lfWidth != 0) {
        /* We can't use this font because that optimization requires that
         * lfEscapement, lfOrientation and lfWidth be zero. */
        font = NULL;
    }

    return cairo_win32_font_face_create_for_logfontw_hfont (&logfont, font);
}

static cairo_bool_t
_cairo_scaled_font_is_win32 (cairo_scaled_font_t *scaled_font)
{
    return scaled_font->backend == &_cairo_win32_scaled_font_backend;
}

/**
 * cairo_win32_scaled_font_select_font:
 * @scaled_font: A #cairo_scaled_font_t from the Win32 font backend. Such an
 *   object can be created with cairo_win32_scaled_font_create_for_logfontw().
 * @hdc: a device context
 *
 * Selects the font into the given device context and changes the
 * map mode and world transformation of the device context to match
 * that of the font. This function is intended for use when using
 * layout APIs such as Uniscribe to do text layout with the
 * cairo font. After finishing using the device context, you must call
 * cairo_win32_scaled_font_done_font() to release any resources allocated
 * by this function.
 *
 * See cairo_win32_scaled_font_get_metrics_factor() for converting logical
 * coordinates from the device context to font space.
 *
 * Normally, calls to SaveDC() and RestoreDC() would be made around
 * the use of this function to preserve the original graphics state.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if the operation succeeded.
 *   otherwise an error such as %CAIRO_STATUS_NO_MEMORY and
 *   the device context is unchanged.
 **/
cairo_status_t
cairo_win32_scaled_font_select_font (cairo_scaled_font_t *scaled_font,
				     HDC                  hdc)
{
    cairo_status_t status;
    HFONT hfont;
    HFONT old_hfont = NULL;
    int old_mode;

    if (! _cairo_scaled_font_is_win32 (scaled_font)) {
	return _cairo_error (CAIRO_STATUS_FONT_TYPE_MISMATCH);
    }

    if (scaled_font->status)
	return scaled_font->status;

    status = _win32_scaled_font_get_scaled_hfont ((cairo_win32_scaled_font_t *)scaled_font, &hfont);
    if (status)
	return status;

    old_hfont = SelectObject (hdc, hfont);
    if (!old_hfont)
	return _cairo_win32_print_gdi_error ("cairo_win32_scaled_font_select_font:SelectObject");

    old_mode = SetGraphicsMode (hdc, GM_ADVANCED);
    if (!old_mode) {
	status = _cairo_win32_print_gdi_error ("cairo_win32_scaled_font_select_font:SetGraphicsMode");
	SelectObject (hdc, old_hfont);
	return status;
    }

    status = _win32_scaled_font_set_world_transform ((cairo_win32_scaled_font_t *)scaled_font, hdc);
    if (status) {
	SetGraphicsMode (hdc, old_mode);
	SelectObject (hdc, old_hfont);
	return status;
    }

    SetMapMode (hdc, MM_TEXT);

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_win32_scaled_font_done_font:
 * @scaled_font: A scaled font from the Win32 font backend.
 *
 * Releases any resources allocated by cairo_win32_scaled_font_select_font()
 **/
void
cairo_win32_scaled_font_done_font (cairo_scaled_font_t *scaled_font)
{
    if (! _cairo_scaled_font_is_win32 (scaled_font)) {
	_cairo_error_throw (CAIRO_STATUS_FONT_TYPE_MISMATCH);
    }
}

/**
 * cairo_win32_scaled_font_get_metrics_factor:
 * @scaled_font: a scaled font from the Win32 font backend
 *
 * Gets a scale factor between logical coordinates in the coordinate
 * space used by cairo_win32_scaled_font_select_font() (that is, the
 * coordinate system used by the Windows functions to return metrics) and
 * font space coordinates.
 *
 * Return value: factor to multiply logical units by to get font space
 *               coordinates.
 **/
double
cairo_win32_scaled_font_get_metrics_factor (cairo_scaled_font_t *scaled_font)
{
    if (! _cairo_scaled_font_is_win32 (scaled_font)) {
	_cairo_error_throw (CAIRO_STATUS_FONT_TYPE_MISMATCH);
	return 1.;
    }
    return 1. / ((cairo_win32_scaled_font_t *)scaled_font)->logical_scale;
}

/**
 * cairo_win32_scaled_font_get_logical_to_device:
 * @scaled_font: a scaled font from the Win32 font backend
 * @logical_to_device: matrix to return
 *
 * Gets the transformation mapping the logical space used by @scaled_font
 * to device space.
 *
 * Since: 1.4
 **/
void
cairo_win32_scaled_font_get_logical_to_device (cairo_scaled_font_t *scaled_font,
					       cairo_matrix_t *logical_to_device)
{
    cairo_win32_scaled_font_t *win_font = (cairo_win32_scaled_font_t *)scaled_font;
    if (! _cairo_scaled_font_is_win32 (scaled_font)) {
	_cairo_error_throw (CAIRO_STATUS_FONT_TYPE_MISMATCH);
	cairo_matrix_init_identity (logical_to_device);
	return;
    }
    *logical_to_device = win_font->logical_to_device;
}

/**
 * cairo_win32_scaled_font_get_device_to_logical:
 * @scaled_font: a scaled font from the Win32 font backend
 * @device_to_logical: matrix to return
 *
 * Gets the transformation mapping device space to the logical space
 * used by @scaled_font.
 *
 * Since: 1.4
 **/
void
cairo_win32_scaled_font_get_device_to_logical (cairo_scaled_font_t *scaled_font,
					       cairo_matrix_t *device_to_logical)
{
    cairo_win32_scaled_font_t *win_font = (cairo_win32_scaled_font_t *)scaled_font;
    if (! _cairo_scaled_font_is_win32 (scaled_font)) {
	_cairo_error_throw (CAIRO_STATUS_FONT_TYPE_MISMATCH);
	cairo_matrix_init_identity (device_to_logical);
	return;
    }
    *device_to_logical = win_font->device_to_logical;
}

void
_cairo_win32_font_reset_static_data (void)
{
    _cairo_win32_font_face_hash_table_destroy ();
}
