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
 *	Calum Robinson <calumr@mac.com>
 */

#include <stdlib.h>
#include <math.h>

#include "cairo-atsui.h"
#include "cairoint.h"





#pragma mark Types





typedef struct {
	cairo_unscaled_font_t	base;
	
	ATSUStyle				style;
	ATSUFontID				fontID;
} cairo_atsui_font_t;


typedef struct cairo_ATSUI_glyph_path_callback_info_t {
	cairo_path_t		*path;
	cairo_matrix_t		scale;
} cairo_ATSUI_glyph_path_callback_info_t;





#pragma mark Private Functions





static CGAffineTransform CGAffineTransformMakeWithCairoFontScale(cairo_font_scale_t scale)
{
	return CGAffineTransformMake(	scale.matrix[0][0],	scale.matrix[0][1],
									scale.matrix[1][0],	scale.matrix[1][1],
									0, 0);
}


static ATSUStyle CreateSizedCopyOfStyle(ATSUStyle inStyle, cairo_font_scale_t	*scale)
{
	ATSUStyle				style;
	OSStatus				err;
	
	
	// Set the style's size
	CGAffineTransform		theTransform = CGAffineTransformMakeWithCairoFontScale(*scale);
	Fixed					theSize = FloatToFixed(CGSizeApplyAffineTransform(CGSizeMake(1.0, 1.0), theTransform).height);
	const ATSUAttributeTag  theFontStyleTags[] = { kATSUSizeTag };
	const ByteCount         theFontStyleSizes[] = { sizeof(Fixed) };
	ATSUAttributeValuePtr   theFontStyleValues[] = { &theSize };

	err = ATSUCreateAndCopyStyle(inStyle, &style);
	
	err = ATSUSetAttributes(	style,
								sizeof(theFontStyleTags) / sizeof(ATSUAttributeTag),
								theFontStyleTags, theFontStyleSizes, theFontStyleValues);
	
	
	return style;
}





#pragma mark Public Functions





static cairo_unscaled_font_t *
_cairo_atsui_font_create(	const char   	*family, 
							cairo_font_slant_t   slant, 
							cairo_font_weight_t  weight)
{
	cairo_atsui_font_t  *font = NULL;
	ATSUStyle			style;
	ATSUFontID			fontID;
	OSStatus			err;
	Boolean				isItalic, isBold;
	
	
	err = ATSUCreateStyle(&style);
	
	
	switch (weight)
	{
		case CAIRO_FONT_WEIGHT_BOLD:
			isBold = true;
			break;
		case CAIRO_FONT_WEIGHT_NORMAL:
		default:
			isBold = false;
			break;
	}

	switch (slant)
	{
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
	
	err = ATSUFindFontFromName( family, strlen(family),
								kFontFamilyName,
								kFontNoPlatformCode,
								kFontRomanScript,
								kFontNoLanguageCode,
								&fontID);
	
	
	ATSUAttributeTag		styleTags[] = {kATSUQDItalicTag, kATSUQDBoldfaceTag, kATSUFontTag};
	ATSUAttributeValuePtr   styleValues[] = {&isItalic, &isBold, &fontID};
	ByteCount				styleSizes[] = {sizeof(Boolean), sizeof(Boolean), sizeof(ATSUFontID)};


	err = ATSUSetAttributes(	style,
								sizeof(styleTags) / sizeof(styleTags[0]),
								styleTags,
								styleSizes,
								styleValues);
	
	
	
	font = malloc(sizeof(cairo_atsui_font_t));
	
	if (_cairo_unscaled_font_init(&font->base, &cairo_atsui_font_backend) == CAIRO_STATUS_SUCCESS)
	{
		font->style		= style;
		font->fontID	= fontID;
		
		
		return &font->base;
	}
	
	
	free(font);
	
	return NULL;
}


static void 
_cairo_atsui_font_destroy(void *abstract_font)
{
	cairo_atsui_font_t *font = abstract_font;
	
	
	if (font == NULL)
	    return;
	
	if (font->style)
		ATSUDisposeStyle(font->style);
	
	free(font);
}


static cairo_status_t 
_cairo_atsui_font_text_to_glyphs(	void				*abstract_font,
									cairo_font_scale_t	*sc,
									const unsigned char	*utf8,
									cairo_glyph_t		**glyphs, 
									int					*nglyphs)
{
	cairo_atsui_font_t	*font = abstract_font;
	size_t				i;
	OSStatus			err;
	ATSUTextLayout		textLayout;
	ATSLayoutRecord		*layoutRecords;
	ItemCount			glyphCount, charCount;
	UniChar				*theText;
	ATSUStyle			style;
	
	
	charCount = strlen(utf8);
	
	
	err = ATSUCreateTextLayout(&textLayout);
	
	
	// Set the text in the text layout object, so we can measure it
	theText = (UniChar *)malloc(charCount * sizeof(UniChar));
	
	for (i = 0; i < charCount; i++)
	{
		theText[i] = utf8[i];
	}
	
	err = ATSUSetTextPointerLocation(   textLayout, 
										theText, 
										0, 
										charCount, 
										charCount);
	
	
	style = CreateSizedCopyOfStyle(font->style, sc);
	
	
	// Set the style for all of the text
	err = ATSUSetRunStyle(	textLayout, 
							style, 
							kATSUFromTextBeginning, 
							kATSUToTextEnd);
	
	
	
	// Get the glyphs from the text layout object
	err = ATSUDirectGetLayoutDataArrayPtrFromTextLayout(	textLayout,
															0,
															kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, 
															(void *)&layoutRecords,  
															&glyphCount); 
	
	*nglyphs = glyphCount;
	
	
	*glyphs = (cairo_glyph_t *)malloc(glyphCount * (sizeof(cairo_glyph_t)));
	if (*glyphs == NULL)
	{
	    return CAIRO_STATUS_NO_MEMORY;
	}

	for (i = 0; i < glyphCount; i++)
	{
		(*glyphs)[i].index = layoutRecords[i].glyphID;
		(*glyphs)[i].x = FixedToFloat(layoutRecords[i].realPos);
		(*glyphs)[i].y = 0;
	}
	
	
	free(theText);
	
	ATSUDirectReleaseLayoutDataArrayPtr(	NULL, 
											kATSUDirectDataLayoutRecordATSLayoutRecordCurrent,
											(void *)&layoutRecords); 
	
	ATSUDisposeTextLayout(textLayout);
	
	ATSUDisposeStyle(style);
	
	
	return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_atsui_font_font_extents(	void					*abstract_font,
								cairo_font_scale_t   	*sc,
								cairo_font_extents_t	*extents)
{
	cairo_atsui_font_t	*font = abstract_font;
	ATSFontRef			atsFont;
	ATSFontMetrics		metrics;
	OSStatus			err;
	
	
	// TODO - test this
	
	atsFont = FMGetATSFontRefFromFont(font->fontID);
	
	if (atsFont)
	{
		err = ATSFontGetHorizontalMetrics(atsFont, kATSOptionFlagsDefault, &metrics);
		
		if (err == noErr)
		{
			extents->ascent			= metrics.ascent;
			extents->descent		= metrics.descent;
			extents->height			= metrics.capHeight;
			extents->max_x_advance	= metrics.maxAdvanceWidth;
			
			// The FT backend doesn't handle max_y_advance either, so we'll ignore it for now. 
			extents->max_y_advance	= 0.0;
			

			return CAIRO_STATUS_SUCCESS;
		}
	}
	
	
	return CAIRO_STATUS_NULL_POINTER;
}


static cairo_status_t 
_cairo_atsui_font_glyph_extents(	void					*abstract_font,
									cairo_font_scale_t		*sc,
									cairo_glyph_t			*glyphs, 
									int						num_glyphs,
									cairo_text_extents_t	*extents)
{
	cairo_atsui_font_t		*font = abstract_font;
	cairo_point_double_t	origin;
	cairo_point_double_t	glyph_min, glyph_max;
	cairo_point_double_t	total_min, total_max;
	OSStatus				err;
	ATSUStyle				style;
	int						i;
	

	if (num_glyphs == 0)
	{
		extents->x_bearing = 0.0;
		extents->y_bearing = 0.0;
		extents->width  = 0.0;
		extents->height = 0.0;
		extents->x_advance = 0.0;
		extents->y_advance = 0.0;

		return CAIRO_STATUS_SUCCESS;
	}

	origin.x = glyphs[0].x;
	origin.y = glyphs[0].y;
	
	
	style = CreateSizedCopyOfStyle(font->style, sc);
	

	for (i = 0; i < num_glyphs; i++)
	{
		GlyphID					theGlyph = glyphs[i].index;
		double					minX, maxX, ascent, descent;
		ATSGlyphIdealMetrics	metricsH, metricsV;
		
		
		err = ATSUGlyphGetIdealMetrics(	style,
										1,
										&theGlyph,
										0,
										&metricsH);
		
		
		ATSUVerticalCharacterType	verticalType = kATSUStronglyVertical;
		ATSUAttributeTag			theTag = kATSUVerticalCharacterTag;
		ByteCount					theSize = sizeof(ATSUVerticalCharacterType);
		
		err = ATSUSetAttributes(style, 1, &theTag, &theSize, (ATSUAttributeValuePtr)&verticalType);
		
		err = ATSUGlyphGetIdealMetrics(	style,
										1,
										&theGlyph,
										0,
										&metricsV);
		
		minX = metricsH.otherSideBearing.x;
		maxX = metricsH.advance.x;
		
		ascent = metricsV.advance.x;
		descent = metricsV.otherSideBearing.x;
		
		glyph_min.x = glyphs[i].x + minX;
		glyph_min.y = glyphs[i].y + descent;
		glyph_max.x = glyphs[i].x + maxX;
		glyph_max.y = glyphs[i].y + ascent;
		
		if (i==0)
		{
			total_min = glyph_min;
			total_max = glyph_max;
		}
		else
		{
			if (glyph_min.x < total_min.x)
				total_min.x = glyph_min.x;
			if (glyph_min.y < total_min.y)
				total_min.y = glyph_min.y;

			if (glyph_max.x > total_max.x)
				total_max.x = glyph_max.x;
			if (glyph_max.y > total_max.y)
				total_max.y = glyph_max.y;
		}
	}
	

	extents->x_bearing = total_min.x - origin.x;
	extents->y_bearing = total_min.y - origin.y;
	extents->width 	= total_max.x - total_min.x;
	extents->height	= total_max.y - total_min.y;
	extents->x_advance = glyphs[i-1].x - origin.x;
	extents->y_advance = glyphs[i-1].y - origin.y;
	
	
	return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_atsui_font_glyph_bbox(	void				*abstract_font,
								cairo_font_scale_t 	*sc,
								const cairo_glyph_t	*glyphs,
								int					num_glyphs,
								cairo_box_t 		*bbox)
{
	cairo_atsui_font_t	*font = abstract_font;
	cairo_fixed_t		x1, y1, x2, y2;
	int					i;
	OSStatus			err;
	ATSUStyle			style;
	

	bbox->p1.x = bbox->p1.y = CAIRO_MAXSHORT << 16;
	bbox->p2.x = bbox->p2.y = CAIRO_MINSHORT << 16;
	
	
	style = CreateSizedCopyOfStyle(font->style, sc);
	
	
	for (i = 0; i < num_glyphs; i++)
	{
		GlyphID					theGlyph = glyphs[i].index;
		ATSGlyphIdealMetrics	metrics;
		
		
		err = ATSUGlyphGetIdealMetrics(	style,
										1,
										&theGlyph,
										0,
										&metrics);
		
		x1 = _cairo_fixed_from_double(glyphs[i].x);
		y1 = _cairo_fixed_from_double(glyphs[i].y);
		x2 = x1 + _cairo_fixed_from_double(metrics.advance.x);
		y2 = y1 + _cairo_fixed_from_double(metrics.advance.y);
		
		if (x1 < bbox->p1.x)
			bbox->p1.x = x1;
		
		if (y1 < bbox->p1.y)
			bbox->p1.y = y1;
		
		if (x2 > bbox->p2.x)
			bbox->p2.x = x2;
		
		if (y2 > bbox->p2.y)
			bbox->p2.y = y2;
	}
	
	
	ATSUDisposeStyle(style);
	

	return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_atsui_font_show_glyphs(	void				*abstract_font,
								cairo_font_scale_t 	*sc,
								cairo_operator_t	operator,
								cairo_surface_t 	*source,
								cairo_surface_t 	*surface,
								int					source_x,
								int					source_y,
								const cairo_glyph_t	*glyphs,
								int					num_glyphs)
{
	cairo_atsui_font_t		*font = abstract_font;
	CGContextRef			myBitmapContext;
    CGColorSpaceRef			colorSpace;
	cairo_image_surface_t	*destImageSurface;
	int						i;
	
	
	destImageSurface = _cairo_surface_get_image(surface);
    
	
	// Create a CGBitmapContext for the dest surface for drawing into
    colorSpace = CGColorSpaceCreateDeviceRGB();
	
	myBitmapContext = CGBitmapContextCreate(	destImageSurface->data,
												destImageSurface->width,
												destImageSurface->height,
												destImageSurface->depth / 4,
												destImageSurface->stride,
												colorSpace,
												kCGImageAlphaPremultipliedFirst);
	
	
	ATSFontRef	atsFont	= FMGetATSFontRefFromFont(font->fontID);
	CGFontRef	cgFont	= CGFontCreateWithPlatformFont(&atsFont);
	
	CGContextSetFont(myBitmapContext, cgFont);
	
	
	CGAffineTransform	textTransform = CGAffineTransformMakeWithCairoFontScale(*sc);
    CGSize				textSize = CGSizeMake(1.0, 1.0);
	
	textSize = CGSizeApplyAffineTransform(textSize, textTransform);
	
	CGContextSetFontSize(myBitmapContext, textSize.width);
	
	
	// TODO - bold and italic text
	//
	// We could draw the text using ATSUI and get bold, italics
	// etc. for free, but ATSUI does a lot of text layout work
	// that we don't really need...
	
	
	for (i = 0; i < num_glyphs; i++)
	{
		CGGlyph		theGlyph = glyphs[i].index;
		
		CGContextShowGlyphsAtPoint(myBitmapContext, source_x + glyphs[i].x, destImageSurface->height - (source_y + glyphs[i].y), &theGlyph, 1);
	}
	
	
	CGColorSpaceRelease(colorSpace);
	CGContextRelease(myBitmapContext);
	
	
	return CAIRO_STATUS_SUCCESS;
}


#pragma mark -


static OSStatus MyATSCubicMoveToCallback(const Float32Point *pt, void *callBackDataPtr)
{
    cairo_ATSUI_glyph_path_callback_info_t	*info = callBackDataPtr;
    double									scaledPt[2];
	cairo_point_t							point;
	
	
	scaledPt[0] = pt->x;
	scaledPt[1] = pt->y;
	
	cairo_matrix_transform_point(&info->scale, &scaledPt[0], &scaledPt[1]);

    point.x = _cairo_fixed_from_double(scaledPt[0]);
    point.y = _cairo_fixed_from_double(scaledPt[1]);

    _cairo_path_move_to(info->path, &point);
    
	
    return noErr;
}


static OSStatus MyATSCubicLineToCallback(const Float32Point *pt, void *callBackDataPtr)
{
    cairo_ATSUI_glyph_path_callback_info_t	*info = callBackDataPtr;
    cairo_point_t							point;
    double									scaledPt[2];
	
	
	scaledPt[0] = pt->x;
	scaledPt[1] = pt->y;
	
	cairo_matrix_transform_point(&info->scale, &scaledPt[0], &scaledPt[1]);
	
    point.x = _cairo_fixed_from_double(scaledPt[0]);
    point.y = _cairo_fixed_from_double(scaledPt[1]);

    _cairo_path_line_to(info->path, &point);
	
	
    return noErr; 
}


static OSStatus MyATSCubicCurveToCallback( const Float32Point *pt1, 
									const Float32Point *pt2, 
									const Float32Point *pt3, 
									void *callBackDataPtr)
{
    cairo_ATSUI_glyph_path_callback_info_t	*info = callBackDataPtr;
    cairo_point_t p0, p1, p2;
	double	scaledPt[2];
	
	
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


static OSStatus MyCubicClosePathProc(void * callBackDataPtr) 
{
    cairo_ATSUI_glyph_path_callback_info_t	*info = callBackDataPtr;
    
	
    _cairo_path_close_path(info->path);
    
	
    return noErr; 
}


static cairo_status_t 
_cairo_atsui_font_glyph_path(	void				*abstract_font,
								cairo_font_scale_t 	*sc,
								cairo_glyph_t		*glyphs, 
								int					num_glyphs,
								cairo_path_t		*path)
{
	int										i;
	cairo_atsui_font_t						*font = abstract_font;
	OSStatus								err;
	cairo_ATSUI_glyph_path_callback_info_t	info;
	ATSUStyle								style;
	
	
	static ATSCubicMoveToUPP				moveProc		= NULL;
	static ATSCubicLineToUPP				lineProc		= NULL;
	static ATSCubicCurveToUPP				curveProc		= NULL;
	static ATSCubicClosePathUPP				closePathProc	= NULL;
	
	
	if (moveProc == NULL)
	{
		moveProc		= NewATSCubicMoveToUPP(MyATSCubicMoveToCallback);
		lineProc		= NewATSCubicLineToUPP(MyATSCubicLineToCallback);
		curveProc       = NewATSCubicCurveToUPP(MyATSCubicCurveToCallback);
		closePathProc   = NewATSCubicClosePathUPP(MyCubicClosePathProc);
	}
	
	
	info.path = path;
	
	
	style = CreateSizedCopyOfStyle(font->style, sc);
		

	for (i = 0; i < num_glyphs; i++)
	{
		GlyphID theGlyph = glyphs[i].index;
		
		
		cairo_matrix_set_affine(	&info.scale,
									1.0, 0.0,
									0.0, 1.0,
									glyphs[i].x, glyphs[i].y);
		
		
		err = ATSUGlyphGetCubicPaths(   style, 
										theGlyph, 
										moveProc,
										lineProc,
										curveProc,
										closePathProc,
										(void *)&info, 
										&err);
	}
	
	
	err = ATSUDisposeStyle(style);
	
	
	return CAIRO_STATUS_SUCCESS;
}


#pragma mark -


static cairo_status_t 
_cairo_atsui_font_create_glyph(cairo_image_glyph_cache_entry_t 	*val)	
{
	// TODO
	printf("_cairo_atsui_font_create_glyph is unimplemented\n");
	
	// I'm not sure if we need this, given that the ATSUI backend does no caching(?)
	
	
	return CAIRO_STATUS_SUCCESS;
}


cairo_font_t *
cairo_atsui_font_create(ATSUStyle style)
{	
	cairo_font_scale_t scale;
	cairo_font_t *scaled;
	cairo_atsui_font_t *f = NULL;
	
	
	scaled = malloc(sizeof(cairo_font_t));
	if (scaled == NULL)
		return NULL;
	
	
	f = malloc(sizeof(cairo_atsui_font_t));
	if (f)
	{
		if (_cairo_unscaled_font_init(&f->base, &cairo_atsui_font_backend) == CAIRO_STATUS_SUCCESS)
		{
			f->style = style;

			_cairo_font_init(scaled, &scale, &f->base);

			return scaled;
		}
	}
	
	
	free(scaled);
	
	
	return NULL;
}





#pragma mark Backend





const cairo_font_backend_t cairo_atsui_font_backend = {
	_cairo_atsui_font_create,
	_cairo_atsui_font_destroy,
	_cairo_atsui_font_font_extents,
	_cairo_atsui_font_text_to_glyphs,
	_cairo_atsui_font_glyph_extents,
	_cairo_atsui_font_glyph_bbox,
	_cairo_atsui_font_show_glyphs,
	_cairo_atsui_font_glyph_path,
	_cairo_atsui_font_create_glyph
};
