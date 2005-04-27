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
#include <iconv.h>

// FixedToFloat/FloatToFixed are 10.3 SDK items - include definitions
// here so we can use older SDKs
#ifndef FixedToFloat
#define fixed1              ((Fixed) 0x00010000L)
#define FixedToFloat(a)     ((float)(a) / fixed1)
#define FloatToFixed(a)     ((Fixed)((float)(a) * fixed1))
#endif

typedef struct {
    cairo_font_t base;

    cairo_font_scale_t scale;
    ATSUStyle style;
    ATSUStyle unscaled_style;
    ATSUFontID fontID;
} cairo_atsui_font_t;

typedef struct cairo_ATSUI_glyph_path_callback_info_t {
    cairo_path_t *path;
    cairo_matrix_t scale;
} cairo_ATSUI_glyph_path_callback_info_t;



static CGAffineTransform
CGAffineTransformMakeWithCairoFontScale(cairo_font_scale_t scale)
{
    return CGAffineTransformMake(scale.matrix[0][0], scale.matrix[0][1],
                                 scale.matrix[1][0], scale.matrix[1][1],
                                 0, 0);
}


static ATSUStyle
CreateSizedCopyOfStyle(ATSUStyle inStyle, cairo_font_scale_t * scale)
{
    ATSUStyle style;
    OSStatus err;


    // Set the style's size
    CGAffineTransform theTransform =
        CGAffineTransformMakeWithCairoFontScale(*scale);
    Fixed theSize =
        FloatToFixed(CGSizeApplyAffineTransform
                     (CGSizeMake(1.0, 1.0), theTransform).height);
    const ATSUAttributeTag theFontStyleTags[] = { kATSUSizeTag };
    const ByteCount theFontStyleSizes[] = { sizeof(Fixed) };
    ATSUAttributeValuePtr theFontStyleValues[] = { &theSize };

    err = ATSUCreateAndCopyStyle(inStyle, &style);

    err = ATSUSetAttributes(style,
                            sizeof(theFontStyleTags) /
                            sizeof(ATSUAttributeTag), theFontStyleTags,
                            theFontStyleSizes, theFontStyleValues);


    return style;
}


static cairo_status_t
_cairo_atsui_font_create(const char *family,
			 cairo_font_slant_t slant,
			 cairo_font_weight_t weight,
			 cairo_font_scale_t *scale,
			 cairo_font_t **font_out)
{
    cairo_atsui_font_t *font = NULL;
    ATSUStyle style;
    ATSUFontID fontID;
    OSStatus err;
    Boolean isItalic, isBold;


    err = ATSUCreateStyle(&style);


    switch (weight) {
    case CAIRO_FONT_WEIGHT_BOLD:
        isBold = true;
        break;
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
        isBold = false;
        break;
    }

    switch (slant) {
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

    err = ATSUFindFontFromName(family, strlen(family),
                               kFontFamilyName,
                               kFontNoPlatformCode,
                               kFontRomanScript,
                               kFontNoLanguageCode, &fontID);

    if (err != noErr) {
	// couldn't get the font - remap css names and try again

	if (!strcmp(family, "serif"))
	    family = "Times";
	else if (!strcmp(family, "sans-serif"))
	    family = "Helvetica";
	else if (!strcmp(family, "cursive"))
	    family = "Apple Chancery";
	else if (!strcmp(family, "fantasy"))
	    family = "Gadget";
	else if (!strcmp(family, "monospace"))
	    family = "Courier";
	else // anything else - return error instead?
	    family = "Courier";

	err = ATSUFindFontFromName(family, strlen(family),
				   kFontFamilyName,
				   kFontNoPlatformCode,
				   kFontRomanScript,
				   kFontNoLanguageCode, &fontID);
    }


    ATSUAttributeTag styleTags[] =
        { kATSUQDItalicTag, kATSUQDBoldfaceTag, kATSUFontTag };
    ATSUAttributeValuePtr styleValues[] = { &isItalic, &isBold, &fontID };
    ByteCount styleSizes[] =
        { sizeof(Boolean), sizeof(Boolean), sizeof(ATSUFontID) };


    err = ATSUSetAttributes(style,
                            sizeof(styleTags) / sizeof(styleTags[0]),
                            styleTags, styleSizes, styleValues);

    font = malloc(sizeof(cairo_atsui_font_t));

    _cairo_font_init(&font->base, scale, &cairo_atsui_font_backend);

    font->style = CreateSizedCopyOfStyle(style, scale);


    Fixed theSize = FloatToFixed(1.0);
    const ATSUAttributeTag theFontStyleTags[] = { kATSUSizeTag };
    const ByteCount theFontStyleSizes[] = { sizeof(Fixed) };
    ATSUAttributeValuePtr theFontStyleValues[] = { &theSize };
    err = ATSUSetAttributes(style,
                            sizeof(theFontStyleTags) /
                            sizeof(ATSUAttributeTag), theFontStyleTags,
                            theFontStyleSizes, theFontStyleValues);

    font->unscaled_style = style;

    font->fontID = fontID;
    font->scale = *scale;

    *font_out = (cairo_font_t *)font;

    return CAIRO_STATUS_SUCCESS;
}


static void
_cairo_atsui_font_destroy_font(void *abstract_font)
{
    cairo_atsui_font_t *font = abstract_font;


    if (font == NULL)
        return;

    if (font->style)
        ATSUDisposeStyle(font->style);
    if (font->unscaled_style)
        ATSUDisposeStyle(font->unscaled_style);

    free(font);
}


static void 
_cairo_atsui_font_destroy_unscaled_font(void *abstract_font)
{
}


static void
_cairo_atsui_font_get_glyph_cache_key(void *abstract_font,
				      cairo_glyph_cache_key_t *key)
{
}


static cairo_status_t
_cairo_atsui_font_text_to_glyphs(void *abstract_font,
                                 const unsigned char *utf8,
                                 cairo_glyph_t ** glyphs, int *nglyphs)
{
    cairo_atsui_font_t *font = abstract_font;
    size_t i;
    OSStatus err;
    ATSUTextLayout textLayout;
    ATSLayoutRecord *layoutRecords;
    ItemCount glyphCount, charCount;
    UniChar *theText;

    err = ATSUCreateTextLayout(&textLayout);

#if 1
    charCount = strlen(utf8);

    // Set the text in the text layout object, so we can measure it
    theText = (UniChar *) malloc(charCount * sizeof(UniChar));

    for (i = 0; i < charCount; i++) {
        theText[i] = utf8[i];
    }
#endif

#if 0
    // Set the text in the text layout object, so we can measure it
    charCount = strlen(utf8);
    theText = (UniChar *) malloc(charCount * sizeof(UniChar));

    size_t inBytes = charCount, outBytes = charCount;
    iconv_t converter = iconv_open("UTF-8", "UTF-16");
    charCount = iconv(converter, utf8, &inBytes, theText, &outBytes);
#endif

    err = ATSUSetTextPointerLocation(textLayout,
                                     theText, 0, charCount, charCount);


    // Set the style for all of the text
    err = ATSUSetRunStyle(textLayout,
                          font->unscaled_style, kATSUFromTextBeginning, kATSUToTextEnd);

    // Get the glyphs from the text layout object
    err = ATSUDirectGetLayoutDataArrayPtrFromTextLayout(textLayout,
                                                        0,
                                                        kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
                                                        (void *)
                                                        &layoutRecords,
                                                        &glyphCount);

    *nglyphs = glyphCount - 1;


    *glyphs =
        (cairo_glyph_t *) malloc(*nglyphs * (sizeof(cairo_glyph_t)));
    if (*glyphs == NULL) {
        return CAIRO_STATUS_NO_MEMORY;
    }

    for (i = 0; i < *nglyphs; i++) {
        (*glyphs)[i].index = layoutRecords[i].glyphID;
        (*glyphs)[i].x = FixedToFloat(layoutRecords[i].realPos);
        (*glyphs)[i].y = 0;
    }


    free(theText);

    ATSUDirectReleaseLayoutDataArrayPtr(NULL,
                                        kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
                                        (void *) &layoutRecords);

    ATSUDisposeTextLayout(textLayout);


    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t
_cairo_atsui_font_font_extents(void *abstract_font,
                               cairo_font_extents_t * extents)
{
    cairo_atsui_font_t *font = abstract_font;
    ATSFontRef atsFont;
    ATSFontMetrics metrics;
    OSStatus err;

    // TODO - test this

    atsFont = FMGetATSFontRefFromFont(font->fontID);

    if (atsFont) {
        err =
            ATSFontGetHorizontalMetrics(atsFont, kATSOptionFlagsDefault,
                                        &metrics);

        if (err == noErr) {
            extents->ascent = metrics.ascent;
            extents->descent = metrics.descent;
            extents->height = metrics.capHeight;
            extents->max_x_advance = metrics.maxAdvanceWidth;

            // The FT backend doesn't handle max_y_advance either, so we'll ignore it for now. 
            extents->max_y_advance = 0.0;

            return CAIRO_STATUS_SUCCESS;
        }
    }


    return CAIRO_STATUS_NULL_POINTER;
}


static cairo_status_t
_cairo_atsui_font_glyph_extents(void *abstract_font,
                                cairo_glyph_t * glyphs,
                                int num_glyphs,
                                cairo_text_extents_t * extents)
{
    cairo_atsui_font_t *font = abstract_font;
    OSStatus err;

    assert(num_glyphs == 1);

    GlyphID theGlyph = glyphs[0].index;

    ATSGlyphIdealMetrics metricsH, metricsV;
    ATSUStyle style;

    ATSUCreateAndCopyStyle(font->unscaled_style, &style);

    err = ATSUGlyphGetIdealMetrics(style,
				   1, &theGlyph, 0, &metricsH);

    ATSUVerticalCharacterType verticalType = kATSUStronglyVertical;
    const ATSUAttributeTag theTag[] = { kATSUVerticalCharacterTag };
    const ByteCount theSizes[] = { sizeof(verticalType) };
    ATSUAttributeValuePtr theValues[] = { &verticalType };
    
    err = ATSUSetAttributes(style, 1, theTag, theSizes, theValues);

    err = ATSUGlyphGetIdealMetrics(style,
				   1, &theGlyph, 0, &metricsV);

    extents->x_bearing = metricsH.sideBearing.x;
    extents->y_bearing = metricsV.advance.y;
    extents->width = 
	metricsH.advance.x - metricsH.sideBearing.x - metricsH.otherSideBearing.x;
    extents->height = 
	-metricsV.advance.y - metricsV.sideBearing.y - metricsV.otherSideBearing.y;
    extents->x_advance = metricsH.advance.x;
    extents->y_advance = 0;

    ATSUDisposeStyle(style);

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t
_cairo_atsui_font_glyph_bbox(void *abstract_font,
                             const cairo_glyph_t *glyphs,
                             int num_glyphs, cairo_box_t *bbox)
{
    cairo_atsui_font_t *font = abstract_font;
    cairo_fixed_t x1, y1, x2, y2;
    int i;
    OSStatus err;

    bbox->p1.x = bbox->p1.y = CAIRO_MAXSHORT << 16;
    bbox->p2.x = bbox->p2.y = CAIRO_MINSHORT << 16;


    for (i = 0; i < num_glyphs; i++) {
        GlyphID theGlyph = glyphs[i].index;

	ATSGlyphScreenMetrics metrics;
	ATSUGlyphGetScreenMetrics(font->style, 
				  1, &theGlyph, 0, true, true, &metrics);

	x1 = _cairo_fixed_from_double(glyphs[i].x + metrics.topLeft.x);
	y1 = _cairo_fixed_from_double(glyphs[i].y - metrics.topLeft.y);
	x2 = x1 + _cairo_fixed_from_double(metrics.height);
	y2 = y1 + _cairo_fixed_from_double(metrics.width);

        if (x1 < bbox->p1.x)
            bbox->p1.x = x1;

        if (y1 < bbox->p1.y)
            bbox->p1.y = y1;

        if (x2 > bbox->p2.x)
            bbox->p2.x = x2;

        if (y2 > bbox->p2.y)
            bbox->p2.y = y2;
    }

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t
_cairo_atsui_font_show_glyphs(void *abstract_font,
                              cairo_operator_t operator,
                              cairo_pattern_t *pattern,
                              cairo_surface_t *generic_surface,
                              int source_x,
                              int source_y,
                              int dest_x,
                              int dest_y,
			      unsigned int width,
			      unsigned int height,
                              const cairo_glyph_t *glyphs,
			      int num_glyphs)
{
    cairo_atsui_font_t *font = abstract_font;
    CGContextRef myBitmapContext;
    CGColorSpaceRef colorSpace;
    cairo_image_surface_t *destImageSurface;
    int i;

    cairo_rectangle_t rect = {dest_x, dest_y, width, height};
    _cairo_surface_acquire_dest_image(generic_surface,
				      &rect,
				      &destImageSurface,
				      &rect,
				      NULL);

    // Create a CGBitmapContext for the dest surface for drawing into
    colorSpace = CGColorSpaceCreateDeviceRGB();

    myBitmapContext = CGBitmapContextCreate(destImageSurface->data,
                                            destImageSurface->width,
                                            destImageSurface->height,
                                            destImageSurface->depth / 4,
                                            destImageSurface->stride,
                                            colorSpace,
                                            kCGImageAlphaPremultipliedFirst);
    CGContextTranslateCTM(myBitmapContext, 0, destImageSurface->height);
    CGContextScaleCTM(myBitmapContext, 1.0f, -1.0f);

    ATSFontRef atsFont = FMGetATSFontRefFromFont(font->fontID);
    CGFontRef cgFont = CGFontCreateWithPlatformFont(&atsFont);

    CGContextSetFont(myBitmapContext, cgFont);

    CGAffineTransform textTransform =
        CGAffineTransformMakeWithCairoFontScale(font->scale);

    textTransform = CGAffineTransformScale(textTransform, 1.0f, -1.0f);

    CGContextSetFontSize(myBitmapContext, 1.0);
    CGContextSetTextMatrix(myBitmapContext, textTransform);

    if (pattern->type == CAIRO_PATTERN_SOLID &&
	_cairo_pattern_is_opaque(pattern)) {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *)pattern;
	CGContextSetRGBFillColor(myBitmapContext,
				 solid->red, solid->green, solid->blue, 1.0f);
    } else
	CGContextSetRGBFillColor(myBitmapContext, 0.0f, 0.0f, 0.0f, 0.0f);

    // TODO - bold and italic text
    //
    // We could draw the text using ATSUI and get bold, italics
    // etc. for free, but ATSUI does a lot of text layout work
    // that we don't really need...


    for (i = 0; i < num_glyphs; i++) {
        CGGlyph theGlyph = glyphs[i].index;

        CGContextShowGlyphsAtPoint(myBitmapContext,
				   glyphs[i].x,
                                   glyphs[i].y,
                                   &theGlyph, 1);
    }


    CGColorSpaceRelease(colorSpace);
    CGContextRelease(myBitmapContext);

    _cairo_surface_release_dest_image(generic_surface,
				      &rect,
				      destImageSurface,
				      &rect,
				      NULL);

    return CAIRO_STATUS_SUCCESS;
}


static OSStatus MyATSCubicMoveToCallback(const Float32Point * pt,
                                         void *callBackDataPtr)
{
    cairo_ATSUI_glyph_path_callback_info_t *info = callBackDataPtr;
    double scaledPt[2];
    cairo_point_t point;


    scaledPt[0] = pt->x;
    scaledPt[1] = pt->y;

    cairo_matrix_transform_point(&info->scale, &scaledPt[0], &scaledPt[1]);

    point.x = _cairo_fixed_from_double(scaledPt[0]);
    point.y = _cairo_fixed_from_double(scaledPt[1]);

    _cairo_path_move_to(info->path, &point);


    return noErr;
}


static OSStatus MyATSCubicLineToCallback(const Float32Point * pt,
                                         void *callBackDataPtr)
{
    cairo_ATSUI_glyph_path_callback_info_t *info = callBackDataPtr;
    cairo_point_t point;
    double scaledPt[2];


    scaledPt[0] = pt->x;
    scaledPt[1] = pt->y;

    cairo_matrix_transform_point(&info->scale, &scaledPt[0], &scaledPt[1]);

    point.x = _cairo_fixed_from_double(scaledPt[0]);
    point.y = _cairo_fixed_from_double(scaledPt[1]);

    _cairo_path_line_to(info->path, &point);


    return noErr;
}


static OSStatus MyATSCubicCurveToCallback(const Float32Point * pt1,
                                          const Float32Point * pt2,
                                          const Float32Point * pt3,
                                          void *callBackDataPtr)
{
    cairo_ATSUI_glyph_path_callback_info_t *info = callBackDataPtr;
    cairo_point_t p0, p1, p2;
    double scaledPt[2];


    scaledPt[0] = pt1->x;
    scaledPt[1] = pt1->y;

    cairo_matrix_transform_point(&info->scale, &scaledPt[0], &scaledPt[1]);

    p0.x = _cairo_fixed_from_double(scaledPt[0]);
    p0.y = _cairo_fixed_from_double(scaledPt[1]);


    scaledPt[0] = pt2->x;
    scaledPt[1] = pt2->y;

    cairo_matrix_transform_point(&info->scale, &scaledPt[0], &scaledPt[1]);

    p1.x = _cairo_fixed_from_double(scaledPt[0]);
    p1.y = _cairo_fixed_from_double(scaledPt[1]);


    scaledPt[0] = pt3->x;
    scaledPt[1] = pt3->y;

    cairo_matrix_transform_point(&info->scale, &scaledPt[0], &scaledPt[1]);

    p2.x = _cairo_fixed_from_double(scaledPt[0]);
    p2.y = _cairo_fixed_from_double(scaledPt[1]);


    _cairo_path_curve_to(info->path, &p0, &p1, &p2);


    return noErr;
}


static OSStatus MyCubicClosePathProc(void *callBackDataPtr)
{
    cairo_ATSUI_glyph_path_callback_info_t *info = callBackDataPtr;


    _cairo_path_close_path(info->path);


    return noErr;
}


static cairo_status_t
_cairo_atsui_font_glyph_path(void *abstract_font,
                             cairo_glyph_t *glyphs, int num_glyphs,
			     cairo_path_t *path)
{
    int i;
    cairo_atsui_font_t *font = abstract_font;
    OSStatus err;
    cairo_ATSUI_glyph_path_callback_info_t info;


    static ATSCubicMoveToUPP moveProc = NULL;
    static ATSCubicLineToUPP lineProc = NULL;
    static ATSCubicCurveToUPP curveProc = NULL;
    static ATSCubicClosePathUPP closePathProc = NULL;


    if (moveProc == NULL) {
        moveProc = NewATSCubicMoveToUPP(MyATSCubicMoveToCallback);
        lineProc = NewATSCubicLineToUPP(MyATSCubicLineToCallback);
        curveProc = NewATSCubicCurveToUPP(MyATSCubicCurveToCallback);
        closePathProc = NewATSCubicClosePathUPP(MyCubicClosePathProc);
    }


    info.path = path;


    for (i = 0; i < num_glyphs; i++) {
        GlyphID theGlyph = glyphs[i].index;


        cairo_matrix_set_affine(&info.scale,
                                1.0, 0.0,
                                0.0, 1.0, glyphs[i].x, glyphs[i].y);


        err = ATSUGlyphGetCubicPaths(font->style,
                                     theGlyph,
                                     moveProc,
                                     lineProc,
                                     curveProc,
                                     closePathProc, (void *) &info, &err);
    }

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t
_cairo_atsui_font_create_glyph(cairo_image_glyph_cache_entry_t * val)
{
    return CAIRO_STATUS_NO_MEMORY;
}


const cairo_font_backend_t cairo_atsui_font_backend = {
    _cairo_atsui_font_create,
    _cairo_atsui_font_destroy_font,
    _cairo_atsui_font_destroy_unscaled_font,
    _cairo_atsui_font_font_extents,
    _cairo_atsui_font_text_to_glyphs,
    _cairo_atsui_font_glyph_extents,
    _cairo_atsui_font_glyph_bbox,
    _cairo_atsui_font_show_glyphs,
    _cairo_atsui_font_glyph_path,
    _cairo_atsui_font_get_glyph_cache_key,
    _cairo_atsui_font_create_glyph
};


cairo_font_t *cairo_atsui_font_create(ATSUStyle style)
{
#if 0
    cairo_font_scale_t scale;
    cairo_font_t *scaled;
    cairo_atsui_font_t *f = NULL;


    scaled = malloc(sizeof(cairo_font_t));
    if (scaled == NULL)
        return NULL;


    f = malloc(sizeof(cairo_atsui_font_t));
    if (f) {
        if (_cairo_unscaled_font_init(&f->base, &cairo_atsui_font_backend)
            == CAIRO_STATUS_SUCCESS) {
            f->style = style;

            _cairo_font_init(scaled, &scale, &f->base);

            return scaled;
        }
    }


    free(scaled);

#endif

    return NULL;
}
