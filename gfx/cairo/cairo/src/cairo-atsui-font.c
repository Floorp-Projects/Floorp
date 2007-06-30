/* -*- Mode: c; c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Calum Robinson
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
 * The Initial Developer of the Original Code is Calum Robinson
 *
 * Contributor(s):
 *  Calum Robinson <calumr@mac.com>
 */

#include <stdlib.h>
#include <math.h>
#include "cairo-atsui.h"
#include "cairoint.h"
#include "cairo.h"
#include "cairo-quartz-private.h"

/*
 * FixedToFloat/FloatToFixed are 10.3+ SDK items - include definitions
 * here so we can use older SDKs.
 */
#ifndef FixedToFloat
#define fixed1              ((Fixed) 0x00010000L)
#define FixedToFloat(a)     ((float)(a) / fixed1)
#define FloatToFixed(a)     ((Fixed)((float)(a) * fixed1))
#endif

/* If this isn't defined, we must be building on non-intel,
 * hence it will be 0.
 */
#ifdef __BIG_ENDIAN__
#define CG_BITMAP_BYTE_ORDER_FLAG 0
#else    /* Little endian. */
/* x86, and will be a 10.4u SDK; ByteOrder32Host will be defined */
#define CG_BITMAP_BYTE_ORDER_FLAG kCGBitmapByteOrder32Host
#endif

/* Public in 10.4, present in 10.3.9 */
CG_EXTERN CGRect CGRectApplyAffineTransform (CGRect, CGAffineTransform);

typedef struct _cairo_atsui_font_face cairo_atsui_font_face_t;
typedef struct _cairo_atsui_font cairo_atsui_font_t;
typedef struct _cairo_atsui_scaled_path cairo_atsui_scaled_path_t;

static cairo_status_t _cairo_atsui_font_create_scaled (cairo_font_face_t *font_face,
						       ATSUFontID font_id,
						       ATSUStyle style,
						       const cairo_matrix_t *font_matrix,
						       const cairo_matrix_t *ctm,
						       const cairo_font_options_t *options,
						       cairo_scaled_font_t **font_out);

struct _cairo_atsui_font {
    cairo_scaled_font_t base;

    ATSUStyle style;
    ATSUStyle unscaled_style;
    ATSUFontID fontID;

    Fixed size;
    CGAffineTransform font_matrix;
};

struct _cairo_atsui_font_face {
  cairo_font_face_t base;
  ATSUFontID font_id;
};

struct _cairo_atsui_scaled_path {
    cairo_path_fixed_t *path;
    cairo_matrix_t *scale;
};

static void
_cairo_atsui_font_face_destroy (void *abstract_face)
{
}

static cairo_status_t
_cairo_atsui_font_face_scaled_font_create (void	*abstract_face,
					   const cairo_matrix_t	*font_matrix,
					   const cairo_matrix_t	*ctm,
					   const cairo_font_options_t *options,
					   cairo_scaled_font_t **font)
{
    cairo_atsui_font_face_t *font_face = abstract_face;
    OSStatus err;
    ATSUAttributeTag styleTags[] = { kATSUFontTag };
    ATSUAttributeValuePtr styleValues[] = { &font_face->font_id };
    ByteCount styleSizes[] = {  sizeof(ATSUFontID) };
    ATSUStyle style;

    err = ATSUCreateStyle (&style);
    err = ATSUSetAttributes(style,
                            sizeof(styleTags) / sizeof(styleTags[0]),
                            styleTags, styleSizes, styleValues);

    return _cairo_atsui_font_create_scaled (&font_face->base, font_face->font_id, style,
					    font_matrix, ctm, options, font);
}

static const cairo_font_face_backend_t _cairo_atsui_font_face_backend = {
    CAIRO_FONT_TYPE_ATSUI,
    _cairo_atsui_font_face_destroy,
    _cairo_atsui_font_face_scaled_font_create
};

cairo_font_face_t *
cairo_atsui_font_face_create_for_atsu_font_id (ATSUFontID font_id)
{
  cairo_atsui_font_face_t *font_face;

  font_face = malloc (sizeof (cairo_atsui_font_face_t));
  if (!font_face) {
    _cairo_error (CAIRO_STATUS_NO_MEMORY);
    return (cairo_font_face_t *)&_cairo_font_face_nil;
  }

  font_face->font_id = font_id;

    _cairo_font_face_init (&font_face->base, &_cairo_atsui_font_face_backend);

    return &font_face->base;
}

static CGAffineTransform
CGAffineTransformMakeWithCairoFontScale(const cairo_matrix_t *scale)
{
    return CGAffineTransformMake(scale->xx, scale->yx,
                                 scale->xy, scale->yy,
                                 0, 0);
}

static ATSUStyle
CreateSizedCopyOfStyle(ATSUStyle inStyle, 
		       const Fixed *theSize, 
		       const CGAffineTransform *theTransform)
{
    ATSUStyle style;
    OSStatus err;
    const ATSUAttributeTag theFontStyleTags[] = { kATSUSizeTag, 
						  kATSUFontMatrixTag };
    const ByteCount theFontStyleSizes[] = { sizeof(Fixed), 
					    sizeof(CGAffineTransform) };
    ATSUAttributeValuePtr theFontStyleValues[] = { (Fixed *)theSize, 
						   (CGAffineTransform *)theTransform };

    err = ATSUCreateAndCopyStyle(inStyle, &style);

    err = ATSUSetAttributes(style,
                            sizeof(theFontStyleTags) /
                            sizeof(ATSUAttributeTag), theFontStyleTags,
                            theFontStyleSizes, theFontStyleValues);

    return style;
}

static cairo_status_t
_cairo_atsui_font_set_metrics (cairo_atsui_font_t *font)
{
    ATSFontRef atsFont;
    ATSFontMetrics metrics;
    OSStatus err;

    atsFont = FMGetATSFontRefFromFont(font->fontID);

    if (atsFont) {
        err = ATSFontGetHorizontalMetrics(atsFont, kATSOptionFlagsDefault, &metrics);

        if (err == noErr) {
	    cairo_font_extents_t extents;

            extents.ascent = metrics.ascent;
            extents.descent = -metrics.descent;
            extents.height = metrics.capHeight;
            extents.max_x_advance = metrics.maxAdvanceWidth;

            /* The FT backend doesn't handle max_y_advance either, so we'll ignore it for now. */
            extents.max_y_advance = 0.0;

	    _cairo_scaled_font_set_metrics (&font->base, &extents);

            return CAIRO_STATUS_SUCCESS;
        }
    }

    return CAIRO_STATUS_NULL_POINTER;
}

static cairo_status_t
_cairo_atsui_font_create_scaled (cairo_font_face_t *font_face,
				 ATSUFontID font_id,
				 ATSUStyle style,
				 const cairo_matrix_t *font_matrix,
				 const cairo_matrix_t *ctm,
				 const cairo_font_options_t *options,
				 cairo_scaled_font_t **font_out)
{
    cairo_atsui_font_t *font = NULL;
    OSStatus err;
    cairo_status_t status;
    double xscale = 1.0;
    double yscale = 1.0;

    font = malloc(sizeof(cairo_atsui_font_t));
    if (font == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_scaled_font_init(&font->base, font_face, font_matrix, ctm, options,
			    &cairo_atsui_scaled_font_backend);

    _cairo_matrix_compute_scale_factors (&font->base.scale, 
					 &xscale, &yscale, 1);
    font->font_matrix = CGAffineTransformMake (1., 0.,
					       0., yscale/xscale,
					       0., 0.);
    font->size = FloatToFixed (xscale);

    font->style = CreateSizedCopyOfStyle (style, &font->size, &font->font_matrix);

    {
	Fixed theSize = FloatToFixed(1.0);
	const ATSUAttributeTag theFontStyleTags[] = { kATSUSizeTag };
	const ByteCount theFontStyleSizes[] = { sizeof(Fixed) };
	ATSUAttributeValuePtr theFontStyleValues[] = { &theSize };

	err = ATSUSetAttributes(style,
				sizeof(theFontStyleTags) /
				sizeof(ATSUAttributeTag), theFontStyleTags,
				theFontStyleSizes, theFontStyleValues);
	if (err != noErr) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL;
	}
    }

    font->unscaled_style = style;
    font->fontID = font_id;

    *font_out = &font->base;

    status = _cairo_atsui_font_set_metrics (font);

  FAIL:
    if (status) {
	cairo_scaled_font_destroy (&font->base);
	return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_atsui_font_create_toy(cairo_toy_font_face_t *toy_face,
			     const cairo_matrix_t *font_matrix,
			     const cairo_matrix_t *ctm,
			     const cairo_font_options_t *options,
			     cairo_scaled_font_t **font_out)
{
    ATSUStyle style;
    ATSUFontID fontID;
    OSStatus err;
    Boolean isItalic, isBold;
    const char *family = toy_face->family;
    const char *full_name;

    err = ATSUCreateStyle(&style);

    switch (toy_face->weight) {
    case CAIRO_FONT_WEIGHT_BOLD:
        isBold = true;
        break;
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
        isBold = false;
        break;
    }

    switch (toy_face->slant) {
    case CAIRO_FONT_SLANT_ITALIC:
        isItalic = true;
        break;
    case CAIRO_FONT_SLANT_OBLIQUE:
        isItalic = false;
        break;
    case CAIRO_FONT_SLANT_NORMAL:
    default:
        isItalic = false;
        break;
    }

    /* The APIs for resolving a font family to a regular
     * font face are all broken or deprecated in 10.4, so
     * just try our best to find a sensible font.
     * 
     * First we try to map the CSS generic font families
     * to fonts that should always be available. 
     * If the family isn't a CSS family, guess that the 
     * font family name is the same as the full name of the 
     * regular font instance, which works in many cases. 
     * 'Times' is one common exception to this rule, so it's
     * handled specially.
     */
    if (!strcmp(family, "serif") || !strcmp(family, "Times"))
	full_name = "Times Roman";
    else if (!strcmp(family, "sans-serif") || !strcmp(family, "sans"))
	full_name = "Helvetica";
    else if (!strcmp(family, "cursive"))
	full_name = "Apple Chancery";
    else if (!strcmp(family, "fantasy"))
	full_name = "American Typewriter";
    else if (!strcmp(family, "monospace") || !strcmp(family, "mono"))
	full_name = "Courier";
    else
	full_name = family;

    err = ATSUFindFontFromName(family, strlen(family),
                               kFontFullName,
                               kFontNoPlatformCode,
                               kFontRomanScript,
                               kFontNoLanguageCode, &fontID);

    if (err != noErr) {
	/* Look for any font instance in the family. This may 
	 * succeed, but select the bold or italic face. It only
	 * selects the first loaded font instance in the family.
	 */
	err = ATSUFindFontFromName(family, strlen(family),
				   kFontFamilyName,
				   kFontNoPlatformCode,
				   kFontRomanScript,
				   kFontNoLanguageCode, &fontID);
	if (err != noErr) {
	    /* Last chance - try Courier. */
	    full_name = "Courier";
	    err = ATSUFindFontFromName(full_name, strlen(full_name),
				       kFontFullName,
				       kFontNoPlatformCode,
				       kFontRomanScript,
				       kFontNoLanguageCode, &fontID);
	}
    }

    {
	ATSUAttributeTag styleTags[] =
	    { kATSUQDItalicTag, kATSUQDBoldfaceTag, kATSUFontTag };
	ATSUAttributeValuePtr styleValues[] = { &isItalic, &isBold, &fontID };
	ByteCount styleSizes[] =
	    { sizeof(Boolean), sizeof(Boolean), sizeof(ATSUFontID) };

	err = ATSUSetAttributes(style,
				sizeof(styleTags) / sizeof(styleTags[0]),
				styleTags, styleSizes, styleValues);
    }

    return _cairo_atsui_font_create_scaled (&toy_face->base, fontID, style,
					    font_matrix, ctm, options, font_out);
}

static void
_cairo_atsui_font_fini(void *abstract_font)
{
    cairo_atsui_font_t *font = abstract_font;

    if (font == NULL)
        return;

    if (font->style)
        ATSUDisposeStyle(font->style);
    if (font->unscaled_style)
        ATSUDisposeStyle(font->unscaled_style);
}

static
OSStatus _move_to_for_metrics (const Float32Point *point, void *callback_data)
{
    CGMutablePathRef path = callback_data;

    CGPathMoveToPoint (path, &CGAffineTransformIdentity,
			   point->x, point->y);
    return noErr;
}

static
OSStatus _line_to_for_metrics(const Float32Point *point, void *callback_data)
{
    CGMutablePathRef path = callback_data;

    CGPathAddLineToPoint (path, &CGAffineTransformIdentity,
			   point->x, point->y);
    return noErr;
}

static
OSStatus _curve_to_for_metrics (const Float32Point *point1,
				const Float32Point *point2,
				const Float32Point *point3,
				void *callback_data)
{
    CGMutablePathRef path = callback_data;

    CGPathAddCurveToPoint (path, &CGAffineTransformIdentity,
			   point1->x, point1->y,
			   point2->x, point2->y,
			   point3->x, point3->y);
    return noErr;
}

static
OSStatus _close_path_for_metrics(void *callback_data)
{
    CGMutablePathRef path = callback_data;

    CGPathCloseSubpath (path);
    return noErr;
}

static GlyphID 
_cairo_atsui_scaled_glyph_index (cairo_scaled_glyph_t *scaled_glyph) {
    unsigned long index = _cairo_scaled_glyph_index (scaled_glyph);
    if (index > 0xffff) 
	return kATSDeletedGlyphcode;
    return index;
}

static cairo_status_t
_cairo_atsui_font_init_glyph_metrics (cairo_atsui_font_t *scaled_font,
				      cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_text_extents_t extents = {0, 0, 0, 0, 0, 0};
    OSStatus err, callback_err;
    ATSGlyphScreenMetrics metricsH;
    static ATSCubicMoveToUPP moveProc = NULL;
    static ATSCubicLineToUPP lineProc = NULL;
    static ATSCubicCurveToUPP curveProc = NULL;
    static ATSCubicClosePathUPP closePathProc = NULL;
    CGMutablePathRef path;
    GlyphID theGlyph = _cairo_atsui_scaled_glyph_index (scaled_glyph);
    double xscale, yscale;
    CGRect rect;

    if (theGlyph == kATSDeletedGlyphcode) {
	_cairo_scaled_glyph_set_metrics (scaled_glyph,
					 &scaled_font->base,
					 &extents);
	return CAIRO_STATUS_SUCCESS;
    }

    /* We calculate the advance from the screen metrics. We
     * could probably take this from the glyph path.
     */
    err = ATSUGlyphGetScreenMetrics (scaled_font->style,
				     1, &theGlyph, 0, false,
				     false, &metricsH);
    if (err != noErr)
	return CAIRO_STATUS_NO_MEMORY;

    /* Scale down to font units.*/
    _cairo_matrix_compute_scale_factors (&scaled_font->base.scale,
					 &xscale, &yscale, 1);
    xscale = 1.0/xscale;
    yscale = 1.0/yscale;

    extents.x_advance = metricsH.deviceAdvance.x * xscale;
    extents.y_advance = 0;

    if (moveProc == NULL) {
        moveProc = NewATSCubicMoveToUPP (_move_to_for_metrics);
        lineProc = NewATSCubicLineToUPP (_line_to_for_metrics);
        curveProc = NewATSCubicCurveToUPP (_curve_to_for_metrics);
        closePathProc = NewATSCubicClosePathUPP (_close_path_for_metrics);
    }

    path = CGPathCreateMutable ();

    /* The callback error contains any error our functions returned.
     * Its only meaningful if err != noErr, and we don't currently
     * use it for anything.
     */
    err = ATSUGlyphGetCubicPaths (scaled_font->style, theGlyph,
				  moveProc, lineProc, curveProc, closePathProc,
				  (void *)path, &callback_err);

    if (err != noErr) {
	CGPathRelease (path);
	return CAIRO_STATUS_NO_MEMORY;
    }

    rect = CGPathGetBoundingBox (path);

    extents.x_bearing = rect.origin.x * xscale;
    extents.y_bearing = rect.origin.y * yscale;
    extents.width = rect.size.width * xscale;
    extents.height = rect.size.height * yscale;

    _cairo_scaled_glyph_set_metrics (scaled_glyph,
				     &scaled_font->base,
				     &extents);
    CGPathRelease (path);

    return CAIRO_STATUS_SUCCESS;
}

static OSStatus
_move_to (const Float32Point *point,
	  void *callback_data)
{
    cairo_atsui_scaled_path_t *scaled_path = callback_data;
    double x = point->x;
    double y = point->y;
    
    cairo_matrix_transform_point (scaled_path->scale, &x, &y);
    _cairo_path_fixed_close_path (scaled_path->path);
    _cairo_path_fixed_move_to (scaled_path->path,
			       _cairo_fixed_from_double (x),
			       _cairo_fixed_from_double (y));

    return noErr;
}

static OSStatus
_line_to (const Float32Point *point,
	  void *callback_data)
{
    cairo_atsui_scaled_path_t *scaled_path = callback_data;
    double x = point->x;
    double y = point->y;
    
    cairo_matrix_transform_point (scaled_path->scale, &x, &y);

    _cairo_path_fixed_line_to (scaled_path->path,
			       _cairo_fixed_from_double (x),
			       _cairo_fixed_from_double (y));

    return noErr;
}

static OSStatus
_curve_to (const Float32Point *point1,
	   const Float32Point *point2,
	   const Float32Point *point3,
	   void *callback_data)
{
    cairo_atsui_scaled_path_t *scaled_path = callback_data;
    double x1 = point1->x;
    double y1 = point1->y;
    double x2 = point2->x;
    double y2 = point2->y;
    double x3 = point3->x;
    double y3 = point3->y;
    
    cairo_matrix_transform_point (scaled_path->scale, &x1, &y1);
    cairo_matrix_transform_point (scaled_path->scale, &x2, &y2);
    cairo_matrix_transform_point (scaled_path->scale, &x3, &y3);

    _cairo_path_fixed_curve_to (scaled_path->path,
				_cairo_fixed_from_double (x1),
				_cairo_fixed_from_double (y1),
				_cairo_fixed_from_double (x2),
				_cairo_fixed_from_double (y2),
				_cairo_fixed_from_double (x3),
				_cairo_fixed_from_double (y3));

    return noErr;
}

static OSStatus
_close_path (void *callback_data)

{
    cairo_atsui_scaled_path_t *scaled_path = callback_data;

    _cairo_path_fixed_close_path (scaled_path->path);

    return noErr;
}

static cairo_status_t
_cairo_atsui_scaled_font_init_glyph_path (cairo_atsui_font_t *scaled_font,
					  cairo_scaled_glyph_t *scaled_glyph)
{
    static ATSCubicMoveToUPP moveProc = NULL;
    static ATSCubicLineToUPP lineProc = NULL;
    static ATSCubicCurveToUPP curveProc = NULL;
    static ATSCubicClosePathUPP closePathProc = NULL;
    GlyphID theGlyph = _cairo_atsui_scaled_glyph_index (scaled_glyph);
    OSStatus err;
    cairo_atsui_scaled_path_t scaled_path;
    cairo_matrix_t *font_to_device = &scaled_font->base.scale;
    cairo_matrix_t unscaled_font_to_device;
    double xscale;
    double yscale;
    
    scaled_path.path = _cairo_path_fixed_create ();
    if (!scaled_path.path)
	return CAIRO_STATUS_NO_MEMORY;

    if (theGlyph == kATSDeletedGlyphcode) {
	_cairo_scaled_glyph_set_path (scaled_glyph, &scaled_font->base, 
				      scaled_path.path);

	return CAIRO_STATUS_SUCCESS;
    }

    /* extract the rotation/shear component of the scale matrix. */
    _cairo_matrix_compute_scale_factors (font_to_device, &xscale, &yscale, 1);
    cairo_matrix_init (&unscaled_font_to_device, 
		      font_to_device->xx, 
		      font_to_device->yx, 
		      font_to_device->xy, 
		      font_to_device->yy, 0., 0.);
    cairo_matrix_scale (&unscaled_font_to_device, 1.0/xscale, 1.0/yscale);

    scaled_path.scale = &unscaled_font_to_device;

    if (moveProc == NULL) {
        moveProc = NewATSCubicMoveToUPP(_move_to);
        lineProc = NewATSCubicLineToUPP(_line_to);
        curveProc = NewATSCubicCurveToUPP(_curve_to);
        closePathProc = NewATSCubicClosePathUPP(_close_path);
    }

    err = ATSUGlyphGetCubicPaths(scaled_font->style,
				 theGlyph,
				 moveProc,
				 lineProc,
				 curveProc,
				 closePathProc, (void *)&scaled_path, &err);

    _cairo_scaled_glyph_set_path (scaled_glyph, &scaled_font->base, 
				  scaled_path.path);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_atsui_scaled_font_init_glyph_surface (cairo_atsui_font_t *scaled_font,
					     cairo_scaled_glyph_t *scaled_glyph)
{
    OSStatus err;
    CGContextRef drawingContext;
    cairo_image_surface_t *surface;
    cairo_format_t format;

    ATSFontRef atsFont;
    CGFontRef cgFont;
    cairo_scaled_font_t base = scaled_font->base;
    cairo_font_extents_t extents = base.extents;

    GlyphID theGlyph = _cairo_atsui_scaled_glyph_index (scaled_glyph);
    ATSGlyphScreenMetrics metricsH;
    double left, bottom, width, height;
    double xscale, yscale;
    CGRect bbox;
    CGAffineTransform transform;


    if (theGlyph == kATSDeletedGlyphcode) {
	surface = (cairo_image_surface_t *)cairo_image_surface_create (CAIRO_FORMAT_A8, 2, 2);
	if (!surface)
	    return CAIRO_STATUS_NO_MEMORY;
	_cairo_scaled_glyph_set_surface (scaled_glyph,
					 &base,
					 surface);
	return CAIRO_STATUS_SUCCESS;
    }

    /* Compute a box to contain the glyph mask. The vertical
     * sizes come from the font extents; extra pixels are 
     * added to account for fractional sizes.
     */
    height = extents.ascent + extents.descent + 2.0;
    bottom = -extents.descent - 1.0;

    /* Horizontal sizes come from the glyph typographic metrics.
     * It is possible that this might result in clipped text
     * in fonts where the typographic bounds don't cover the ink.
     * The width is recalculated, since metricsH.width is rounded.
     */
    err = ATSUGlyphGetScreenMetrics (scaled_font->style,
				     1, &theGlyph, 0, false, 
				     false, &metricsH);    
    left = metricsH.sideBearing.x - 1.0;
    width = metricsH.deviceAdvance.x 
	- metricsH.sideBearing.x 
	+ metricsH.otherSideBearing.x + 2.0;

    /* The xy and yx components are negated because the y-axis
     * is flipped into the cairo system then flipped back, ie:
     * [1  0][xx yx][1  0]
     * [0 -1][xy yy][0 -1]
     */
    transform = CGAffineTransformMake (base.scale.xx, 
				      -base.scale.yx, 
				      -base.scale.xy, 
				      base.scale.yy, 
				      0., 0.);
    _cairo_matrix_compute_scale_factors (&base.scale, 
					&xscale, &yscale, 1);
    transform = CGAffineTransformScale (transform, 1.0/xscale, 1.0/yscale);

    /* Rotate the bounding box. This computes the smallest CGRect
     * that would contain the bounding box after rotation.
     */
    bbox = CGRectApplyAffineTransform (CGRectMake (left, bottom, 
						   width, height), transform);
    /* Compute the smallest CGRect with  integer coordinates
     * that contains the bounding box.
     */
    bbox = CGRectIntegral (bbox);

    left = CGRectGetMinX (bbox);
    bottom = CGRectGetMinY (bbox);

    /* XXX should we select format based on antialiasing flags, as ft does? */
    format = CAIRO_FORMAT_A8;

    /* create the glyph mask surface */
    surface = (cairo_image_surface_t *)cairo_image_surface_create (format, bbox.size.width, bbox.size.height);
    if (!surface)
	return CAIRO_STATUS_NO_MEMORY;

    /* Create a CGBitmapContext for the dest surface for drawing into */
    {
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray ();
    
	drawingContext = CGBitmapContextCreate (surface->data,
						surface->width,
						surface->height,
						8,
						surface->stride,
						colorSpace,
						kCGImageAlphaNone);
	CGColorSpaceRelease (colorSpace);
    }

    if (!drawingContext) {
	cairo_surface_destroy ((cairo_surface_t *)surface);
	return CAIRO_STATUS_NO_MEMORY;
    }
    
    atsFont = FMGetATSFontRefFromFont (scaled_font->fontID);
    cgFont = CGFontCreateWithPlatformFont (&atsFont);

    CGContextSetFont (drawingContext, cgFont);

    if (base.options.antialias ==  CAIRO_ANTIALIAS_NONE) {
    	CGContextSetShouldAntialias (drawingContext, false);
    }

    /* solid white */
    CGContextSetRGBFillColor (drawingContext, 1.0, 1.0, 1.0, 1.0);

    CGContextSetFontSize (drawingContext, 1.0);
    CGContextTranslateCTM (drawingContext, -left, -bottom);
    CGContextScaleCTM (drawingContext, xscale, yscale);
    CGContextSetTextMatrix (drawingContext, transform);
    CGContextShowGlyphsAtPoint (drawingContext, 0, 0,  
				&theGlyph, 1);

    CGContextRelease (drawingContext);

    /* correct for difference between cairo and quartz 
     * coordinate systems.
     */
    cairo_surface_set_device_offset ((cairo_surface_t *)surface, left, 
				    -bbox.size.height - bottom);
    _cairo_scaled_glyph_set_surface (scaled_glyph,
				     &base,
				     surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_atsui_font_scaled_glyph_init (void			*abstract_font,
				     cairo_scaled_glyph_t	*scaled_glyph,
				     cairo_scaled_glyph_info_t	 info)
{
    cairo_atsui_font_t *scaled_font = abstract_font;
    cairo_status_t status;

    if ((info & CAIRO_SCALED_GLYPH_INFO_METRICS) != 0) {
      status = _cairo_atsui_font_init_glyph_metrics (scaled_font, scaled_glyph);
      if (status)
	return status;
    }

    if ((info & CAIRO_SCALED_GLYPH_INFO_PATH) != 0) {
	status = _cairo_atsui_scaled_font_init_glyph_path (scaled_font, scaled_glyph);
	if (status)
	    return status;
    }

    if ((info & CAIRO_SCALED_GLYPH_INFO_SURFACE) != 0) {
	status = _cairo_atsui_scaled_font_init_glyph_surface (scaled_font, scaled_glyph);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_atsui_font_text_to_glyphs (void		*abstract_font,
				  double	 x,
				  double	 y,
				  const char	*utf8,
				  cairo_glyph_t **glyphs,
				  int		*num_glyphs)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    uint16_t *utf16;
    int n16;
    OSStatus err;
    ATSUTextLayout textLayout;
    ATSLayoutRecord *layoutRecords;
    cairo_atsui_font_t *font = abstract_font;
    ItemCount glyphCount;
    int i;
    CGPoint point;
    double xscale, yscale;
    CGAffineTransform device_to_user_scale;

    status = _cairo_utf8_to_utf16 ((unsigned char *)utf8, -1, &utf16, &n16);
    if (status)
	return status;

    err = ATSUCreateTextLayout(&textLayout);

    err = ATSUSetTextPointerLocation(textLayout, utf16, 0, n16, n16);

    /* Set the style for all of the text */
    err = ATSUSetRunStyle(textLayout,
			  font->style, kATSUFromTextBeginning, kATSUToTextEnd);

    err = ATSUDirectGetLayoutDataArrayPtrFromTextLayout(textLayout,
							0,
							kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
							(void *)&layoutRecords,
							&glyphCount);

    *num_glyphs = glyphCount - 1;
    *glyphs =
	(cairo_glyph_t *) malloc(*num_glyphs * (sizeof (cairo_glyph_t)));
    if (*glyphs == NULL) {
	return CAIRO_STATUS_NO_MEMORY;
    }

    _cairo_matrix_compute_scale_factors (&font->base.ctm, &xscale, &yscale, 1);
    device_to_user_scale = 
	CGAffineTransformInvert (CGAffineTransformMake (xscale, 0,
							0, yscale,
							0, 0));
    for (i = 0; i < *num_glyphs; i++) {
	(*glyphs)[i].index = layoutRecords[i].glyphID;
	/* ATSLayoutRecord.realPos is in device units, convert to user units */
	point = CGPointMake (FixedToFloat (layoutRecords[i].realPos), 0);
	point = CGPointApplyAffineTransform (point, device_to_user_scale);

	(*glyphs)[i].x = x + point.x;
	(*glyphs)[i].y = y;
    }

    free (utf16);

    ATSUDirectReleaseLayoutDataArrayPtr(NULL,
					kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
					(void *) &layoutRecords);
    ATSUDisposeTextLayout(textLayout);

    return CAIRO_STATUS_SUCCESS;
}

ATSUStyle
_cairo_atsui_scaled_font_get_atsu_style (cairo_scaled_font_t *sfont)
{
    cairo_atsui_font_t *afont = (cairo_atsui_font_t *) sfont;

    return afont->style;
}

ATSUFontID
_cairo_atsui_scaled_font_get_atsu_font_id (cairo_scaled_font_t *sfont)
{
    cairo_atsui_font_t *afont = (cairo_atsui_font_t *) sfont;

    return afont->fontID;
}

static cairo_int_status_t
_cairo_atsui_load_truetype_table (void	           *abstract_font,
				  unsigned long     tag,
				  long              offset,
				  unsigned char    *buffer,
				  unsigned long    *length)
{
    cairo_atsui_font_t *scaled_font = abstract_font;
    ATSFontRef atsFont;
    OSStatus err;
	
    atsFont = FMGetATSFontRefFromFont (scaled_font->fontID);
    err = ATSFontGetTable ( atsFont, tag,
			    (ByteOffset) offset,
			    (ByteCount) *length,
			    buffer,
			    length);
    if (err != noErr) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_STATUS_SUCCESS;
}

const cairo_scaled_font_backend_t cairo_atsui_scaled_font_backend = {
    CAIRO_FONT_TYPE_ATSUI,
    _cairo_atsui_font_create_toy,
    _cairo_atsui_font_fini,
    _cairo_atsui_font_scaled_glyph_init,
    _cairo_atsui_font_text_to_glyphs,
    NULL, /* ucs4_to_index */
    NULL, /* show_glyphs */
    _cairo_atsui_load_truetype_table,
    NULL, /* map_glyphs_to_unicode */
};

