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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 */

#include <string.h>
#include <stdio.h>

#include "cairo-win32-private.h"

#ifndef SPI_GETFONTSMOOTHINGTYPE 
#define SPI_GETFONTSMOOTHINGTYPE 0x200a
#endif
#ifndef FE_FONTSMOOTHINGCLEARTYPE
#define FE_FONTSMOOTHINGCLEARTYPE 2
#endif
#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY 5
#endif

const cairo_font_backend_t cairo_win32_font_backend;

#define LOGICAL_SCALE 32

typedef struct {
    cairo_font_t base;

    LOGFONTW logfont;

    BYTE quality;

    /* We do drawing and metrics computation in a "logical space" which
     * is similar to font space, except that it is scaled by a factor
     * of the (desired font size) * (LOGICAL_SCALE). The multiplication
     * by LOGICAL_SCALE allows for sub-pixel precision.
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

    HFONT scaled_font;
    HFONT unscaled_font;
    
} cairo_win32_font_t;

#define NEARLY_ZERO(d) (fabs(d) < (1. / 65536.))

static void
_compute_transform (cairo_win32_font_t *font,
		    cairo_font_scale_t *sc)
{
    if (NEARLY_ZERO (sc->matrix[0][1]) && NEARLY_ZERO (sc->matrix[1][0])) {
	font->preserve_axes = TRUE;
	font->x_scale = sc->matrix[0][0];
	font->swap_x = (sc->matrix[0][0] < 0);
	font->y_scale = sc->matrix[1][1];
	font->swap_y = (sc->matrix[1][1] < 0);
	font->swap_axes = FALSE;
	
    } else if (NEARLY_ZERO (sc->matrix[0][0]) && NEARLY_ZERO (sc->matrix[1][1])) {
	font->preserve_axes = TRUE;
	font->x_scale = sc->matrix[0][1];
	font->swap_x = (sc->matrix[0][1] < 0);
	font->y_scale = sc->matrix[1][0];
	font->swap_y = (sc->matrix[1][0] < 0);
	font->swap_axes = TRUE;

    } else {
	font->preserve_axes = FALSE;
	font->swap_x = font->swap_y = font->swap_axes = FALSE;
    }

    if (font->preserve_axes) {
	if (font->swap_x)
	    font->x_scale = - font->x_scale;
	if (font->swap_y)
	    font->y_scale = - font->y_scale;
	
	font->logical_scale = LOGICAL_SCALE * font->y_scale;
	font->logical_size = LOGICAL_SCALE * floor (font->y_scale + 0.5);
    }

    /* The font matrix has x and y "scale" components which we extract and
     * use as character scale values.
     */
    cairo_matrix_set_affine (&font->logical_to_device,
			     sc->matrix[0][0],
			     sc->matrix[0][1],
			     sc->matrix[1][0],
			     sc->matrix[1][1], 
			     0, 0);

    if (!font->preserve_axes) {
	_cairo_matrix_compute_scale_factors (&font->logical_to_device,
					     &font->x_scale, &font->y_scale,
					     TRUE);	/* XXX: Handle vertical text */

	font->logical_size = floor (LOGICAL_SCALE * font->y_scale + 0.5);
	font->logical_scale = LOGICAL_SCALE * font->y_scale;
    }

    cairo_matrix_scale (&font->logical_to_device,
			1.0 / font->logical_scale, 1.0 / font->logical_scale);

    font->device_to_logical = font->logical_to_device;
    if (!CAIRO_OK (cairo_matrix_invert (&font->device_to_logical)))
      cairo_matrix_set_identity (&font->device_to_logical);
}

static BYTE
_get_system_quality (void)
{
    BOOL font_smoothing;

    if (!SystemParametersInfo (SPI_GETFONTSMOOTHING, 0, &font_smoothing, 0)) {
	_cairo_win32_print_gdi_error ("_get_system_quality");
	return FALSE;
    }

    if (font_smoothing) {
	OSVERSIONINFO version_info;

	version_info.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
	
	if (!GetVersionEx (&version_info)) {
	    _cairo_win32_print_gdi_error ("_get_system_quality");
	    return FALSE;
	}

	if (version_info.dwMajorVersion > 5 ||
	    (version_info.dwMajorVersion == 5 &&
	     version_info.dwMinorVersion >= 1))	{ /* XP or newer */
	    UINT smoothing_type;

	    if (!SystemParametersInfo (SPI_GETFONTSMOOTHINGTYPE,
				       0, &smoothing_type, 0)) {
		_cairo_win32_print_gdi_error ("_get_system_quality");
		return FALSE;
	    }

	    if (smoothing_type == FE_FONTSMOOTHINGCLEARTYPE)
		return CLEARTYPE_QUALITY;
	}

	return ANTIALIASED_QUALITY;
    } else
	return DEFAULT_QUALITY;
}

static cairo_font_t *
_win32_font_create (LOGFONTW           *logfont,
		    cairo_font_scale_t *scale)
{
    cairo_win32_font_t *f;

    f = malloc (sizeof(cairo_win32_font_t));
    if (f == NULL) 
      return NULL;

    f->logfont = *logfont;
    f->quality = _get_system_quality ();
    f->em_square = 0;
    f->scaled_font = NULL;
    f->unscaled_font = NULL;

    _compute_transform (f, scale);
    
    _cairo_font_init ((cairo_font_t *)f, scale, &cairo_win32_font_backend);

    return (cairo_font_t *)f;
}

static cairo_status_t
_win32_font_set_world_transform (cairo_win32_font_t *font,
				 HDC                 hdc)
{
    XFORM xform;
    
    xform.eM11 = font->logical_to_device.m[0][0];
    xform.eM21 = font->logical_to_device.m[1][0];
    xform.eM12 = font->logical_to_device.m[0][1];
    xform.eM22 = font->logical_to_device.m[1][1];
    xform.eDx = font->logical_to_device.m[2][0];
    xform.eDy = font->logical_to_device.m[2][1];

    if (!SetWorldTransform (hdc, &xform))
	return _cairo_win32_print_gdi_error ("_win32_font_set_world_transform");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_win32_font_set_identity_transform (HDC hdc)
{
    if (!ModifyWorldTransform (hdc, NULL, MWT_IDENTITY))
	return _cairo_win32_print_gdi_error ("_win32_font_set_identity_transform");

    return CAIRO_STATUS_SUCCESS;
}

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

static HFONT
_win32_font_get_scaled_font (cairo_win32_font_t *font)
{
    if (!font->scaled_font) {
	LOGFONTW logfont = font->logfont;
	logfont.lfHeight = font->logical_size;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 0;
	logfont.lfQuality = font->quality;

	font->scaled_font = CreateFontIndirectW (&logfont);
	if (!font->scaled_font) {
	    _cairo_win32_print_gdi_error ("_win32_font_get_scaled_font");
	    return NULL;
	}
    }

    return font->scaled_font;
}

static HFONT
_win32_font_get_unscaled_font (cairo_win32_font_t *font,
			       HDC                 hdc)
{
    if (!font->unscaled_font) {
	OUTLINETEXTMETRIC *otm;
	unsigned int otm_size;
	HFONT scaled_font;
	LOGFONTW logfont;

	scaled_font = _win32_font_get_scaled_font (font);
	if (!scaled_font)
	    return NULL;

	if (!SelectObject (hdc, scaled_font)) {
	    _cairo_win32_print_gdi_error ("_win32_font_get_unscaled_font:SelectObject");
	    return NULL;
	}

	otm_size = GetOutlineTextMetrics (hdc, 0, NULL);
	if (!otm_size) {
	    _cairo_win32_print_gdi_error ("_win32_font_get_unscaled_font:GetOutlineTextMetrics");
	    return NULL;
	}
	
	otm = malloc (otm_size);
	if (!otm)
	    return NULL;

	if (!GetOutlineTextMetrics (hdc, otm_size, otm)) {
	    _cairo_win32_print_gdi_error ("_win32_font_get_unscaled_font:GetOutlineTextMetrics");
	    free (otm);
	    return NULL;
	}

	font->em_square = otm->otmEMSquare;
	free (otm);
	
	logfont = font->logfont;
	logfont.lfHeight = font->em_square;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 0;
	logfont.lfQuality = font->quality;
	
	font->unscaled_font = CreateFontIndirectW (&logfont);
	if (!font->unscaled_font) {
	    _cairo_win32_print_gdi_error ("_win32_font_get_unscaled_font:CreateIndirect");
	    return NULL;
	}
    }

    return font->unscaled_font;
}

static cairo_status_t
_cairo_win32_font_select_unscaled_font (cairo_font_t *font,
					HDC           hdc)
{
    cairo_status_t status;
    HFONT hfont;
    HFONT old_hfont = NULL;

    hfont = _win32_font_get_unscaled_font ((cairo_win32_font_t *)font, hdc);
    if (!hfont)
	return CAIRO_STATUS_NO_MEMORY;

    old_hfont = SelectObject (hdc, hfont);
    if (!old_hfont)
	return _cairo_win32_print_gdi_error ("_cairo_win32_font_select_unscaled_font");

    status = _win32_font_set_identity_transform (hdc);
    if (!CAIRO_OK (status)) {
	SelectObject (hdc, old_hfont);
	return status;
    }

    SetMapMode (hdc, MM_TEXT);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_font_done_unscaled_font (cairo_font_t *font)
{
}

/* implement the font backend interface */

static cairo_status_t
_cairo_win32_font_create (const char          *family, 
			  cairo_font_slant_t   slant, 
			  cairo_font_weight_t  weight,
			  cairo_font_scale_t  *scale,
			  cairo_font_t       **font_out)
{
    LOGFONTW logfont;
    cairo_font_t *font;
    uint16_t *face_name;
    int face_name_len;
    cairo_status_t status;

    status = _cairo_utf8_to_utf16 (family, -1, &face_name, &face_name_len);
    if (!CAIRO_OK (status))
	return status;

    if (face_name_len > LF_FACESIZE - 1) {
	free (face_name);
	return CAIRO_STATUS_INVALID_STRING;
    }

    memcpy (logfont.lfFaceName, face_name, sizeof (uint16_t) * (face_name_len + 1));
    free (face_name);

    logfont.lfHeight = 0;	/* filled in later */
    logfont.lfWidth = 0;	/* filled in later */
    logfont.lfEscapement = 0;	/* filled in later */
    logfont.lfOrientation = 0;	/* filled in later */

    switch (weight) {
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
	logfont.lfWeight = FW_NORMAL;
	break;
    case CAIRO_FONT_WEIGHT_BOLD:
	logfont.lfWeight = FW_BOLD;
	break;
    }

    switch (slant) {
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

    if (!logfont.lfFaceName)
	return CAIRO_STATUS_NO_MEMORY;
    
    font = _win32_font_create (&logfont, scale);
    if (!font)
	return CAIRO_STATUS_NO_MEMORY;

    *font_out = font;

    return CAIRO_STATUS_SUCCESS;
}

static void 
_cairo_win32_font_destroy_font (void *abstract_font)
{
    cairo_win32_font_t *font = abstract_font;

    if (font->scaled_font)
	DeleteObject (font->scaled_font);

    if (font->unscaled_font)
	DeleteObject (font->unscaled_font);

    free (font);
}

static void 
_cairo_win32_font_destroy_unscaled_font (void *abstract_font)
{
}

static void
_cairo_win32_font_get_glyph_cache_key (void                    *abstract_font,
				    cairo_glyph_cache_key_t *key)
{
}

static cairo_status_t 
_cairo_win32_font_text_to_glyphs (void			*abstract_font,
				  const unsigned char	*utf8,
				  cairo_glyph_t	       **glyphs, 
				  int			*num_glyphs)
{
    cairo_win32_font_t *font = abstract_font;
    uint16_t *utf16;
    int n16;
    GCP_RESULTSW gcp_results;
    unsigned int buffer_size, i;
    WCHAR *glyph_indices = NULL;
    int *dx = NULL;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    double x_pos;
    HDC hdc = NULL;

    status = _cairo_utf8_to_utf16 (utf8, -1, &utf16, &n16);
    if (!CAIRO_OK (status))
	return status;

    gcp_results.lStructSize = sizeof (GCP_RESULTS);
    gcp_results.lpOutString = NULL;
    gcp_results.lpOrder = NULL;
    gcp_results.lpCaretPos = NULL;
    gcp_results.lpClass = NULL;
    
    buffer_size = MAX (n16 * 1.2, 16);		/* Initially guess number of chars plus a few */
    if (buffer_size > INT_MAX) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL1;
    }
    
    hdc = _get_global_font_dc ();
    if (!hdc) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL1;
    }

    status = cairo_win32_font_select_font (&font->base, hdc);
    if (!CAIRO_OK (status))
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
	
	glyph_indices = malloc (sizeof (WCHAR) * buffer_size);
	dx = malloc (sizeof (int) * buffer_size);
	if (!glyph_indices || !dx) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL2;
	}

	gcp_results.nGlyphs = buffer_size;
	gcp_results.lpDx = dx;
	gcp_results.lpGlyphs = glyph_indices;

	if (!GetCharacterPlacementW (hdc, utf16, n16,
				     0,
				     &gcp_results, 
				     GCP_DIACRITIC | GCP_LIGATE | GCP_GLYPHSHAPE | GCP_REORDER)) {
	    status = _cairo_win32_print_gdi_error ("_cairo_win32_font_text_to_glyphs");
	    goto FAIL2;
	}

	if (gcp_results.lpDx && gcp_results.lpGlyphs)
	    break;

	/* Too small a buffer, try again */
	
	buffer_size *= 1.5;
	if (buffer_size > INT_MAX) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL2;
	}
    }

    *num_glyphs = gcp_results.nGlyphs;
    *glyphs = malloc (sizeof (cairo_glyph_t) * gcp_results.nGlyphs);
    if (!*glyphs) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL2;
    }

    x_pos = 0;
    for (i = 0; i < gcp_results.nGlyphs; i++) {
	(*glyphs)[i].index = glyph_indices[i];
	(*glyphs)[i].x = x_pos ;
	(*glyphs)[i].y = 0;

	x_pos += dx[i] / font->logical_scale;
    }

 FAIL2:
    if (glyph_indices)
	free (glyph_indices);
    if (dx)
	free (dx);
   
    cairo_win32_font_done_font (&font->base);
    
 FAIL1:
    free (utf16);
   
    return status;
}

static cairo_status_t 
_cairo_win32_font_font_extents (void			*abstract_font,
				cairo_font_extents_t	*extents)
{
    cairo_win32_font_t *font = abstract_font;
    cairo_status_t status;
    TEXTMETRIC metrics;
    HDC hdc;

    hdc = _get_global_font_dc ();
    if (!hdc)
	return CAIRO_STATUS_NO_MEMORY;

    if (font->preserve_axes) {
	/* For 90-degree rotations (including 0), we get the metrics
	 * from the GDI in logical space, then convert back to font space
	 */
	status = cairo_win32_font_select_font (&font->base, hdc);
	if (!CAIRO_OK (status))
	    return status;
	GetTextMetrics (hdc, &metrics);
	cairo_win32_font_done_font (&font->base);

	extents->ascent = metrics.tmAscent / font->logical_scale;
	extents->descent = metrics.tmDescent / font->logical_scale;

	extents->height = (metrics.tmHeight + metrics.tmExternalLeading) / font->logical_scale;
	extents->max_x_advance = metrics.tmMaxCharWidth / font->logical_scale;
	extents->max_y_advance = 0;

    } else {
	/* For all other transformations, we use the design metrics
	 * of the font. The GDI results from GetTextMetrics() on a
	 * transformed font are inexplicably large and we want to
	 * avoid them.
	 */
	status = _cairo_win32_font_select_unscaled_font (&font->base, hdc);
	if (!CAIRO_OK (status))
	    return status;
	GetTextMetrics (hdc, &metrics);
	_cairo_win32_font_done_unscaled_font (&font->base);

	extents->ascent = (double)metrics.tmAscent / font->em_square;
	extents->descent = metrics.tmDescent * font->em_square;
	extents->height = (double)(metrics.tmHeight + metrics.tmExternalLeading) / font->em_square;
	extents->max_x_advance = (double)(metrics.tmMaxCharWidth) / font->em_square;
	extents->max_y_advance = 0;
	
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_font_glyph_extents (void			*abstract_font,
				 cairo_glyph_t		*glyphs, 
				 int			 num_glyphs,
				 cairo_text_extents_t	*extents)
{
    cairo_win32_font_t *font = abstract_font;
    static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    GLYPHMETRICS metrics;
    cairo_status_t status;
    HDC hdc;

    hdc = _get_global_font_dc ();
    if (!hdc)
	return CAIRO_STATUS_NO_MEMORY;

    /* We handle only the case num_glyphs == 1, glyphs[i].x == glyphs[0].y == 0.
     * This is all that the calling code triggers, and the backend interface
     * will eventually be changed to match
     */
    assert (num_glyphs == 1);

    if (font->preserve_axes) {
	/* If we aren't rotating / skewing the axes, then we get the metrics
	 * from the GDI in device space and convert to font space.
	 */
	status = cairo_win32_font_select_font (&font->base, hdc);
	if (!CAIRO_OK (status))
	    return status;
	GetGlyphOutlineW (hdc, glyphs[0].index, GGO_METRICS | GGO_GLYPH_INDEX,
			  &metrics, 0, NULL, &matrix);
	cairo_win32_font_done_font (&font->base);

	if (font->swap_axes) {
	    extents->x_bearing = - metrics.gmptGlyphOrigin.y / font->y_scale;
	    extents->y_bearing = metrics.gmptGlyphOrigin.x / font->x_scale;
	    extents->width = metrics.gmBlackBoxY / font->y_scale;
	    extents->height = metrics.gmBlackBoxX / font->x_scale;
	    extents->x_advance = metrics.gmCellIncY / font->x_scale;
	    extents->y_advance = metrics.gmCellIncX / font->y_scale;
	} else {
	    extents->x_bearing = metrics.gmptGlyphOrigin.x / font->x_scale;
	    extents->y_bearing = - metrics.gmptGlyphOrigin.y / font->y_scale;
	    extents->width = metrics.gmBlackBoxX / font->x_scale;
	    extents->height = metrics.gmBlackBoxY / font->y_scale;
	    extents->x_advance = metrics.gmCellIncX / font->x_scale;
	    extents->y_advance = metrics.gmCellIncY / font->y_scale;
	}

	if (font->swap_x) {
	    extents->x_bearing = (- extents->x_bearing - extents->width);
	    extents->x_advance = - extents->x_advance;
	}

	if (font->swap_y) {
	    extents->y_bearing = (- extents->y_bearing - extents->height);
	    extents->y_advance = - extents->y_advance;
	}
	
    } else {
	/* For all other transformations, we use the design metrics
	 * of the font.
	 */
	status = _cairo_win32_font_select_unscaled_font (&font->base, hdc);
	GetGlyphOutlineW (hdc, glyphs[0].index, GGO_METRICS | GGO_GLYPH_INDEX,
			  &metrics, 0, NULL, &matrix);
	_cairo_win32_font_done_unscaled_font (&font->base);

	extents->x_bearing = (double)metrics.gmptGlyphOrigin.x / font->em_square;
	extents->y_bearing = (double)metrics.gmptGlyphOrigin.y / font->em_square;
	extents->width = (double)metrics.gmBlackBoxX / font->em_square;
	extents->height = (double)metrics.gmBlackBoxY / font->em_square;
	extents->x_advance = (double)metrics.gmCellIncX / font->em_square;
	extents->y_advance = (double)metrics.gmCellIncY / font->em_square;
    }

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_win32_font_glyph_bbox (void		       		*abstract_font,
			      const cairo_glyph_t 		*glyphs,
			      int                 		 num_glyphs,
			      cairo_box_t         		*bbox)
{
    static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    cairo_win32_font_t *font = abstract_font;
    int x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    if (num_glyphs > 0) {
	HDC hdc = _get_global_font_dc ();
	GLYPHMETRICS metrics;
	cairo_status_t status;
	int i;

	if (!hdc)
	    return CAIRO_STATUS_NO_MEMORY;

	status = cairo_win32_font_select_font (&font->base, hdc);
	if (!CAIRO_OK (status))
	    return status;

	for (i = 0; i < num_glyphs; i++) {
	    int x = floor (0.5 + glyphs[i].x);
	    int y = floor (0.5 + glyphs[i].y);

	    GetGlyphOutlineW (hdc, glyphs[i].index, GGO_METRICS | GGO_GLYPH_INDEX,
			     &metrics, 0, NULL, &matrix);

	    if (i == 0 || x1 > x + metrics.gmptGlyphOrigin.x)
		x1 = x + metrics.gmptGlyphOrigin.x;
	    if (i == 0 || y1 > y - metrics.gmptGlyphOrigin.y)
		y1 = y - metrics.gmptGlyphOrigin.y;
	    if (i == 0 || x2 < x + metrics.gmptGlyphOrigin.x + metrics.gmBlackBoxX)
		x2 = x + metrics.gmptGlyphOrigin.x + metrics.gmBlackBoxX;
	    if (i == 0 || y2 < y - metrics.gmptGlyphOrigin.y + metrics.gmBlackBoxY)
		y2 = y - metrics.gmptGlyphOrigin.y + metrics.gmBlackBoxY;
	}

	cairo_win32_font_done_font (&font->base);
    }

    bbox->p1.x = _cairo_fixed_from_int (x1);
    bbox->p1.y = _cairo_fixed_from_int (y1);
    bbox->p2.x = _cairo_fixed_from_int (x2);
    bbox->p2.y = _cairo_fixed_from_int (y2);

    return CAIRO_STATUS_SUCCESS;
}

typedef struct {
    cairo_win32_font_t *font;
    HDC hdc;
    
    cairo_array_t glyphs;
    cairo_array_t dx;

    int start_x;
    int last_x;
    int last_y;
} cairo_glyph_state_t;

static void
_start_glyphs (cairo_glyph_state_t *state,
	       cairo_win32_font_t  *font,
	       HDC                  hdc)
{
    state->hdc = hdc;
    state->font = font;

    _cairo_array_init (&state->glyphs, sizeof (WCHAR));
    _cairo_array_init (&state->dx, sizeof (int));
}

static cairo_status_t
_flush_glyphs (cairo_glyph_state_t *state)
{
    int dx = 0;
    if (!_cairo_array_append (&state->dx, &dx, 1))
	return CAIRO_STATUS_NO_MEMORY;
    
    if (!ExtTextOutW (state->hdc,
		      state->start_x, state->last_y,
		      ETO_GLYPH_INDEX,
		      NULL,
		      (WCHAR *)state->glyphs.elements,
		      state->glyphs.num_elements,
		      (int *)state->dx.elements)) {
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
    double user_x = device_x;
    double user_y = device_y;
    WCHAR glyph_index = index;
    int logical_x, logical_y;

    cairo_matrix_transform_point (&state->font->device_to_logical, &user_x, &user_y);

    logical_x = floor (user_x + 0.5);
    logical_y = floor (user_y + 0.5);

    if (state->glyphs.num_elements > 0) {
	int dx;
	
	if (logical_y != state->last_y) {
	    cairo_status_t status = _flush_glyphs (state);
	    if (!CAIRO_OK (status))
		return status;
	    state->start_x = logical_x;
	}
	
	dx = logical_x - state->last_x;
	if (!_cairo_array_append (&state->dx, &dx, 1))
	    return CAIRO_STATUS_NO_MEMORY;
    } else {
	state->start_x = logical_x;
    }

    state->last_x = logical_x;
    state->last_y = logical_y;
    
    _cairo_array_append (&state->glyphs, &glyph_index, 1);

    return CAIRO_STATUS_SUCCESS;
}

static void
_finish_glyphs (cairo_glyph_state_t *state)
{
    _flush_glyphs (state);

    _cairo_array_fini (&state->glyphs);
    _cairo_array_fini (&state->dx);
}

static cairo_status_t
_draw_glyphs_on_surface (cairo_win32_surface_t *surface,
			 cairo_win32_font_t    *font,
			 COLORREF               color,
			 int                    x_offset,
			 int                    y_offset,
			 const cairo_glyph_t   *glyphs,
			 int                 	num_glyphs)
{
    cairo_glyph_state_t state;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    int i;

    if (!SaveDC (surface->dc))
	return _cairo_win32_print_gdi_error ("_draw_glyphs_on_surface:SaveDC");

    status = cairo_win32_font_select_font (&font->base, surface->dc);
    if (!CAIRO_OK (status))
	goto FAIL1;

    SetTextColor (surface->dc, color);
    SetTextAlign (surface->dc, TA_BASELINE | TA_LEFT);
    SetBkMode (surface->dc, TRANSPARENT);
    
    _start_glyphs (&state, font, surface->dc);

    for (i = 0; i < num_glyphs; i++) {
	status = _add_glyph (&state, glyphs[i].index,
			     glyphs[i].x - x_offset, glyphs[i].y - y_offset);
	if (!CAIRO_OK (status))
	    goto FAIL2;
    }

 FAIL2:
    _finish_glyphs (&state);
    cairo_win32_font_done_font (&font->base);
 FAIL1:
    RestoreDC (surface->dc, 1);
    
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

    image8 = (cairo_image_surface_t *)cairo_image_surface_create (CAIRO_FORMAT_A8,
								  image24->width, image24->height);
    if (!image8)
	return NULL;

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

static cairo_status_t 
_cairo_win32_font_show_glyphs (void		       *abstract_font,
			       cairo_operator_t    	operator,
			       cairo_pattern_t     	*pattern,
			       cairo_surface_t     	*generic_surface,
			       int                 	source_x,
			       int                 	source_y,
			       int			dest_x,
			       int			dest_y,
			       unsigned int		width,
			       unsigned int		height,
			       const cairo_glyph_t 	*glyphs,
			       int                 	num_glyphs)
{
    cairo_win32_font_t *font = abstract_font;
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)generic_surface;
    cairo_status_t status;

    if (width == 0 || height == 0)
	return CAIRO_STATUS_SUCCESS;

    if (_cairo_surface_is_win32 (generic_surface) &&
	surface->format == CAIRO_FORMAT_RGB24 &&
	operator == CAIRO_OPERATOR_OVER &&
	pattern->type == CAIRO_PATTERN_SOLID &&
	_cairo_pattern_is_opaque (pattern)) {

	cairo_solid_pattern_t *solid_pattern = (cairo_solid_pattern_t *)pattern;

	/* When compositing OVER on a GDI-understood surface, with a
	 * solid opaque color, we can just call ExtTextOut directly.
	 */
	COLORREF new_color;
	
	new_color = RGB (((int)(0xffff * solid_pattern->red)) >> 8,
			 ((int)(0xffff * solid_pattern->green)) >> 8,
			 ((int)(0xffff * solid_pattern->blue)) >> 8);

	status = _draw_glyphs_on_surface (surface, font, new_color,
					  0, 0,
					  glyphs, num_glyphs);
	
	return status;
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
	RECT r;

	tmp_surface = (cairo_win32_surface_t *)_cairo_win32_surface_create_dib (CAIRO_FORMAT_ARGB32, width, height);
	if (!tmp_surface)
	    return CAIRO_STATUS_NO_MEMORY;

	r.left = 0;
	r.top = 0;
	r.right = width;
	r.bottom = height;
	FillRect (tmp_surface->dc, &r, GetStockObject (WHITE_BRUSH));

	_draw_glyphs_on_surface (tmp_surface, font, RGB (0, 0, 0),
				 dest_x, dest_y,
				 glyphs, num_glyphs);

	if (font->quality == CLEARTYPE_QUALITY) {
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

	    /* XXX: Hacky, should expose this in cairo_image_surface */
	    pixman_image_set_component_alpha (((cairo_image_surface_t *)tmp_surface->image)->pixman_image, TRUE);
	    
	} else {
	    mask_surface = _compute_a8_mask (tmp_surface);
	    cairo_surface_destroy (&tmp_surface->base);
	    if (!mask_surface)
		return CAIRO_STATUS_NO_MEMORY;
	}

	/* For operator == OVER, no-cleartype, a possible optimization here is to
	 * draw onto an intermediate ARGB32 surface and alpha-blend that with the
	 * destination
	 */
	_cairo_pattern_init_for_surface (&mask, mask_surface);

	status = _cairo_surface_composite (operator, pattern, 
					   &mask.base,
					   &surface->base,
					   source_x, source_y,
					   0, 0,
					   dest_x, dest_y,
					   width, height);

	_cairo_pattern_fini (&mask.base);
	
	cairo_surface_destroy (mask_surface);

	return status;
    }
}

static cairo_status_t 
_cairo_win32_font_glyph_path (void			*abstract_font,
			      cairo_glyph_t		*glyphs, 
			      int			num_glyphs,
			      cairo_path_t		*path)
{
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_font_create_glyph (cairo_image_glyph_cache_entry_t *val)    
{
    return CAIRO_STATUS_NO_MEMORY;
}

const cairo_font_backend_t cairo_win32_font_backend = {
    _cairo_win32_font_create,
    _cairo_win32_font_destroy_font,
    _cairo_win32_font_destroy_unscaled_font,
    _cairo_win32_font_font_extents,
    _cairo_win32_font_text_to_glyphs,
    _cairo_win32_font_glyph_extents,
    _cairo_win32_font_glyph_bbox,
    _cairo_win32_font_show_glyphs,
    _cairo_win32_font_glyph_path,
    _cairo_win32_font_get_glyph_cache_key,
    _cairo_win32_font_create_glyph
};

/* implement the platform-specific interface */

/**
 * cairo_win32_font_create_for_logfontw:
 * @logfont: A #LOGFONTW structure specifying the font to use.
 *   The lfHeight, lfWidth, lfOrientation and lfEscapement
 *   fields of this structure are ignored; information from
 *   @scale will be used instead.
 * @scale: The scale at which this font will be used. The
 *   scale is given by multiplying the font matrix (see
 *   cairo_transform_font()) by the current transformation matrix.
 *   The translation elements of the resulting matrix are ignored.
 * 
 * Creates a new font for the Win32 font backend based on a
 * #LOGFONT. This font can then be used with
 * cairo_set_font(), cairo_font_glyph_extents(), or FreeType backend
 * specific functions like cairo_win32_font_select_font().
 *
 * Return value: a newly created #cairo_font_t. Free with
 *  cairo_font_destroy() when you are done using it.
 **/
cairo_font_t *
cairo_win32_font_create_for_logfontw (LOGFONTW       *logfont,
				      cairo_matrix_t *scale)
{
    cairo_font_scale_t sc;
    
    cairo_matrix_get_affine (scale,
			     &sc.matrix[0][0], &sc.matrix[0][1],
			     &sc.matrix[1][0], &sc.matrix[1][1],
			     NULL, NULL);

    return _win32_font_create (logfont, &sc);
}

/**
 * cairo_win32_font_select_font:
 * @font: A #cairo_font_t from the Win32 font backend. Such an
 *   object can be created with cairo_win32_font_create_for_logfontw().
 * @hdc: a device context
 *
 * Selects the font into the given device context and changes the
 * map mode and world transformation of the device context to match
 * that of the font. This function is intended for use when using
 * layout APIs such as Uniscribe to do text layout with the
 * Cairo font. After finishing using the device context, you must call
 * cairo_win32_font_done_font() to release any resources allocated
 * by this function.
 *
 * See cairo_win32_font_get_scale_factor() for converting logical
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
cairo_win32_font_select_font (cairo_font_t *font,
			      HDC           hdc)
{
    cairo_status_t status;
    HFONT hfont;
    HFONT old_hfont = NULL;
    int old_mode;

    hfont = _win32_font_get_scaled_font ((cairo_win32_font_t *)font);
    if (!hfont)
	return CAIRO_STATUS_NO_MEMORY;

    old_hfont = SelectObject (hdc, hfont);
    if (!old_hfont)
	return _cairo_win32_print_gdi_error ("cairo_win32_font_select_font");

    old_mode = SetGraphicsMode (hdc, GM_ADVANCED);
    if (!old_mode) {
	status = _cairo_win32_print_gdi_error ("cairo_win32_font_select_font");
	SelectObject (hdc, old_hfont);
	return status;
    }

    status = _win32_font_set_world_transform ((cairo_win32_font_t *)font, hdc);
    if (!CAIRO_OK (status)) {
	SetGraphicsMode (hdc, old_mode);
	SelectObject (hdc, old_hfont);
	return status;
    }

    SetMapMode (hdc, MM_TEXT);

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_win32_font_done_font:
 * @font: A #cairo_font_t from the Win32 font backend.
 * 
 * Releases any resources allocated by cairo_win32_font_select_font()
 **/
void
cairo_win32_font_done_font (cairo_font_t *font)
{
}

/**
 * cairo_win32_font_get_scale_factor:
 * @font: a #cairo_font_t from the Win32 font backend
 * 
 * Gets a scale factor between logical coordinates in the coordinate
 * space used by cairo_win32_font_select_font() and font space coordinates.
 * 
 * Return value: factor to multiply logical units by to get font space
 *               coordinates.
 **/
double
cairo_win32_font_get_scale_factor (cairo_font_t *font)
{
    return 1. / ((cairo_win32_font_t *)font)->logical_scale;
}
