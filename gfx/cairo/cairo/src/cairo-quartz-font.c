/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright � 2008 Mozilla Corporation
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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 *
 * Contributor(s):
 *	Vladimir Vukicevic <vladimir@mozilla.com>
 */

#include "cairoint.h"

#include <dlfcn.h>

#include "cairo-quartz.h"
#include "cairo-quartz-private.h"

#include "cairo-error-private.h"

/**
 * SECTION:cairo-quartz-fonts
 * @Title: Quartz (CGFont) Fonts
 * @Short_Description: Font support via CGFont on OS X
 * @See_Also: #cairo_font_face_t
 *
 * The Quartz font backend is primarily used to render text on Apple
 * MacOS X systems.  The CGFont API is used for the internal
 * implementation of the font backend methods.
 */

/**
 * CAIRO_HAS_QUARTZ_FONT:
 *
 * Defined if the Quartz font backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 */

/* CreateWithFontName exists in 10.5, but not in 10.4; CreateWithName isn't public in 10.4 */
static CGFontRef (*CGFontCreateWithFontNamePtr) (CFStringRef) = NULL;
static CGFontRef (*CGFontCreateWithNamePtr) (const char *) = NULL;

/* These aren't public before 10.5, and some have different names in 10.4 */
static int (*CGFontGetUnitsPerEmPtr) (CGFontRef) = NULL;
static bool (*CGFontGetGlyphAdvancesPtr) (CGFontRef, const CGGlyph[], size_t, int[]) = NULL;
static bool (*CGFontGetGlyphBBoxesPtr) (CGFontRef, const CGGlyph[], size_t, CGRect[]) = NULL;
static CGRect (*CGFontGetFontBBoxPtr) (CGFontRef) = NULL;

/* Not public, but present */
static void (*CGFontGetGlyphsForUnicharsPtr) (CGFontRef, const UniChar[], const CGGlyph[], size_t) = NULL;
static void (*CGContextSetAllowsFontSmoothingPtr) (CGContextRef, bool) = NULL;
static bool (*CGContextGetAllowsFontSmoothingPtr) (CGContextRef) = NULL;

/* CGFontGetHMetrics isn't public, but the other functions are public/present in 10.5 */
typedef struct {
    int ascent;
    int descent;
    int leading;
} quartz_CGFontMetrics;
static quartz_CGFontMetrics* (*CGFontGetHMetricsPtr) (CGFontRef fontRef) = NULL;
static int (*CGFontGetAscentPtr) (CGFontRef fontRef) = NULL;
static int (*CGFontGetDescentPtr) (CGFontRef fontRef) = NULL;
static int (*CGFontGetLeadingPtr) (CGFontRef fontRef) = NULL;

/* CTFontCreateWithGraphicsFont is not public until 10.5. */
typedef const struct __CTFontDescriptor *CTFontDescriptorRef;
static CTFontRef (*CTFontCreateWithGraphicsFontPtr) (CGFontRef, CGFloat, const CGAffineTransform *, CTFontDescriptorRef) = NULL;

static cairo_bool_t _cairo_quartz_font_symbol_lookup_done = FALSE;
static cairo_bool_t _cairo_quartz_font_symbols_present = FALSE;

static void
quartz_font_ensure_symbols(void)
{
    if (_cairo_quartz_font_symbol_lookup_done)
	return;

    /* Look for the 10.5 versions first */
    CGFontGetGlyphBBoxesPtr = dlsym(RTLD_DEFAULT, "CGFontGetGlyphBBoxes");
    if (!CGFontGetGlyphBBoxesPtr)
	CGFontGetGlyphBBoxesPtr = dlsym(RTLD_DEFAULT, "CGFontGetGlyphBoundingBoxes");

    CGFontGetGlyphsForUnicharsPtr = dlsym(RTLD_DEFAULT, "CGFontGetGlyphsForUnichars");
    if (!CGFontGetGlyphsForUnicharsPtr)
	CGFontGetGlyphsForUnicharsPtr = dlsym(RTLD_DEFAULT, "CGFontGetGlyphsForUnicodes");

    CGFontGetFontBBoxPtr = dlsym(RTLD_DEFAULT, "CGFontGetFontBBox");

    /* We just need one of these two */
    CGFontCreateWithFontNamePtr = dlsym(RTLD_DEFAULT, "CGFontCreateWithFontName");
    CGFontCreateWithNamePtr = dlsym(RTLD_DEFAULT, "CGFontCreateWithName");

    /* These have the same name in 10.4 and 10.5 */
    CGFontGetUnitsPerEmPtr = dlsym(RTLD_DEFAULT, "CGFontGetUnitsPerEm");
    CGFontGetGlyphAdvancesPtr = dlsym(RTLD_DEFAULT, "CGFontGetGlyphAdvances");

    CGFontGetHMetricsPtr = dlsym(RTLD_DEFAULT, "CGFontGetHMetrics");
    CGFontGetAscentPtr = dlsym(RTLD_DEFAULT, "CGFontGetAscent");
    CGFontGetDescentPtr = dlsym(RTLD_DEFAULT, "CGFontGetDescent");
    CGFontGetLeadingPtr = dlsym(RTLD_DEFAULT, "CGFontGetLeading");

    CGContextGetAllowsFontSmoothingPtr = dlsym(RTLD_DEFAULT, "CGContextGetAllowsFontSmoothing");
    CGContextSetAllowsFontSmoothingPtr = dlsym(RTLD_DEFAULT, "CGContextSetAllowsFontSmoothing");

    CTFontCreateWithGraphicsFontPtr = dlsym(RTLD_DEFAULT, "CTFontCreateWithGraphicsFont");

    if ((CGFontCreateWithFontNamePtr || CGFontCreateWithNamePtr) &&
	CGFontGetGlyphBBoxesPtr &&
	CGFontGetGlyphsForUnicharsPtr &&
	CGFontGetUnitsPerEmPtr &&
	CGFontGetGlyphAdvancesPtr &&
	(CGFontGetHMetricsPtr || (CGFontGetAscentPtr && CGFontGetDescentPtr && CGFontGetLeadingPtr)))
	_cairo_quartz_font_symbols_present = TRUE;

    _cairo_quartz_font_symbol_lookup_done = TRUE;
}

typedef struct _cairo_quartz_font_face cairo_quartz_font_face_t;
typedef struct _cairo_quartz_scaled_font cairo_quartz_scaled_font_t;

struct _cairo_quartz_scaled_font {
    cairo_scaled_font_t base;
};

struct _cairo_quartz_font_face {
    cairo_font_face_t base;

    CGFontRef cgFont;
    CTFontRef ctFont;
};

/*
 * font face backend
 */

static cairo_status_t
_cairo_quartz_font_face_create_for_toy (cairo_toy_font_face_t   *toy_face,
					cairo_font_face_t      **font_face)
{
    const char *family;
    char *full_name;
    CFStringRef cgFontName = NULL;
    CGFontRef cgFont = NULL;
    int loop;

    quartz_font_ensure_symbols();
    if (! _cairo_quartz_font_symbols_present)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    family = toy_face->family;
    full_name = malloc (strlen (family) + 64); // give us a bit of room to tack on Bold, Oblique, etc.
    /* handle CSS-ish faces */
    if (!strcmp(family, "serif") || !strcmp(family, "Times Roman"))
	family = "Times";
    else if (!strcmp(family, "sans-serif") || !strcmp(family, "sans"))
	family = "Helvetica";
    else if (!strcmp(family, "cursive"))
	family = "Apple Chancery";
    else if (!strcmp(family, "fantasy"))
	family = "Papyrus";
    else if (!strcmp(family, "monospace") || !strcmp(family, "mono"))
	family = "Courier";

    /* Try to build up the full name, e.g. "Helvetica Bold Oblique" first,
     * then drop the bold, then drop the slant, then drop both.. finally
     * just use "Helvetica".  And if Helvetica doesn't exist, give up.
     */
    for (loop = 0; loop < 5; loop++) {
	if (loop == 4)
	    family = "Helvetica";

	strcpy (full_name, family);

	if (loop < 3 && (loop & 1) == 0) {
	    if (toy_face->weight == CAIRO_FONT_WEIGHT_BOLD)
		strcat (full_name, " Bold");
	}

	if (loop < 3 && (loop & 2) == 0) {
	    if (toy_face->slant == CAIRO_FONT_SLANT_ITALIC)
		strcat (full_name, " Italic");
	    else if (toy_face->slant == CAIRO_FONT_SLANT_OBLIQUE)
		strcat (full_name, " Oblique");
	}

	if (CGFontCreateWithFontNamePtr) {
	    cgFontName = CFStringCreateWithCString (NULL, full_name, kCFStringEncodingASCII);
	    cgFont = CGFontCreateWithFontNamePtr (cgFontName);
	    CFRelease (cgFontName);
	} else {
	    cgFont = CGFontCreateWithNamePtr (full_name);
	}

	if (cgFont)
	    break;
    }

    if (!cgFont) {
	/* Give up */
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    *font_face = cairo_quartz_font_face_create_for_cgfont (cgFont);
    CGFontRelease (cgFont);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_quartz_font_face_destroy (void *abstract_face)
{
    cairo_quartz_font_face_t *font_face = (cairo_quartz_font_face_t*) abstract_face;

    if (font_face->ctFont) {
        CFRelease (font_face->ctFont);
    }

    CGFontRelease (font_face->cgFont);
}

static const cairo_scaled_font_backend_t _cairo_quartz_scaled_font_backend;

static cairo_status_t
_cairo_quartz_font_face_scaled_font_create (void *abstract_face,
					    const cairo_matrix_t *font_matrix,
					    const cairo_matrix_t *ctm,
					    const cairo_font_options_t *options,
					    cairo_scaled_font_t **font_out)
{
    cairo_quartz_font_face_t *font_face = abstract_face;
    cairo_quartz_scaled_font_t *font = NULL;
    cairo_status_t status;
    cairo_font_extents_t fs_metrics;
    double ems;
    CGRect bbox;

    quartz_font_ensure_symbols();
    if (!_cairo_quartz_font_symbols_present)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font = malloc(sizeof(cairo_quartz_scaled_font_t));
    if (font == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    memset (font, 0, sizeof(cairo_quartz_scaled_font_t));

    status = _cairo_scaled_font_init (&font->base,
				      &font_face->base, font_matrix, ctm, options,
				      &_cairo_quartz_scaled_font_backend);
    if (status)
	goto FINISH;

    ems = CGFontGetUnitsPerEmPtr (font_face->cgFont);

    /* initialize metrics */
    if (CGFontGetFontBBoxPtr && CGFontGetAscentPtr) {
	fs_metrics.ascent = (CGFontGetAscentPtr (font_face->cgFont) / ems);
	fs_metrics.descent = - (CGFontGetDescentPtr (font_face->cgFont) / ems);
	fs_metrics.height = fs_metrics.ascent + fs_metrics.descent +
	    (CGFontGetLeadingPtr (font_face->cgFont) / ems);

	bbox = CGFontGetFontBBoxPtr (font_face->cgFont);
	fs_metrics.max_x_advance = CGRectGetMaxX(bbox) / ems;
	fs_metrics.max_y_advance = 0.0;
    } else {
	CGGlyph wGlyph;
	UniChar u;

	quartz_CGFontMetrics *m;
	m = CGFontGetHMetricsPtr (font_face->cgFont);

	/* On OX 10.4, GetHMetricsPtr sometimes returns NULL for unknown reasons */
	if (!m) {
	    status = _cairo_error(CAIRO_STATUS_NULL_POINTER);
	    goto FINISH;
	}

	fs_metrics.ascent = (m->ascent / ems);
	fs_metrics.descent = - (m->descent / ems);
	fs_metrics.height = fs_metrics.ascent + fs_metrics.descent + (m->leading / ems);

	/* We kind of have to guess here; W's big, right? */
	u = (UniChar) 'W';
	CGFontGetGlyphsForUnicharsPtr (font_face->cgFont, &u, &wGlyph, 1);
	if (wGlyph && CGFontGetGlyphBBoxesPtr (font_face->cgFont, &wGlyph, 1, &bbox)) {
	    fs_metrics.max_x_advance = CGRectGetMaxX(bbox) / ems;
	    fs_metrics.max_y_advance = 0.0;
	} else {
	    fs_metrics.max_x_advance = 0.0;
	    fs_metrics.max_y_advance = 0.0;
	}
    }

    status = _cairo_scaled_font_set_metrics (&font->base, &fs_metrics);

FINISH:
    if (status != CAIRO_STATUS_SUCCESS) {
	free (font);
    } else {
	*font_out = (cairo_scaled_font_t*) font;
    }

    return status;
}

const cairo_font_face_backend_t _cairo_quartz_font_face_backend = {
    CAIRO_FONT_TYPE_QUARTZ,
    _cairo_quartz_font_face_create_for_toy,
    _cairo_quartz_font_face_destroy,
    _cairo_quartz_font_face_scaled_font_create
};

// Helper to create a CTFont from a CGFont, copying any variations that were
// set on the original CGFont.
static CTFontRef
CreateCTFontFromCGFontWithVariations(CGFontRef aCGFont, CGFloat aSize)
{
    // Declare helper provided by widget/cocoa/nsCocoaFeatures.mm
    extern bool Gecko_OnSierraExactly();

    // Avoid calling potentially buggy variation APIs on pre-Sierra macOS
    // versions (see bug 1331683).
    //
    // And on HighSierra, CTFontCreateWithGraphicsFont properly carries over
    // variation settings from the CGFont to CTFont, so we don't need to do
    // the extra work here -- and this seems to avoid Core Text crashiness
    // seen in bug 1454094.
    //
    // So we only need to do this "the hard way" on Sierra; on other releases,
    // just let the standard CTFont function do its thing.
    //
    // NOTE in case this ever needs further adjustment: there is similar logic
    // in four places in the tree (sadly):
    //    CreateCTFontFromCGFontWithVariations in gfxMacFont.cpp
    //    CreateCTFontFromCGFontWithVariations in ScaledFontMac.cpp
    //    CreateCTFontFromCGFontWithVariations in cairo-quartz-font.c
    //    ctfont_create_exact_copy in SkFontHost_mac.cpp
    //
    // XXX Does this need to behave differently for installed fonts on High
    // Sierra, as in other similar places (bug 1455569)?

    if (!Gecko_OnSierraExactly()) {
        return CTFontCreateWithGraphicsFont(aCGFont, aSize, NULL, NULL);
    }

    CFDictionaryRef vars = CGFontCopyVariations(aCGFont);
    CTFontRef ctFont;
    if (vars) {
        CFDictionaryRef varAttr =
            CFDictionaryCreate(NULL,
                               (const void**)&kCTFontVariationAttribute,
                               (const void**)&vars, 1,
                               &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);
        CFRelease(vars);

        CTFontDescriptorRef varDesc = CTFontDescriptorCreateWithAttributes(varAttr);
        CFRelease(varAttr);

        ctFont = CTFontCreateWithGraphicsFont(aCGFont, aSize, NULL, varDesc);
        CFRelease(varDesc);
    } else {
        ctFont = CTFontCreateWithGraphicsFont(aCGFont, aSize, NULL, NULL);
    }
    return ctFont;
}

/**
 * cairo_quartz_font_face_create_for_cgfont
 * @font: a #CGFontRef obtained through a method external to cairo.
 *
 * Creates a new font for the Quartz font backend based on a
 * #CGFontRef.  This font can then be used with
 * cairo_set_font_face() or cairo_scaled_font_create().
 *
 * Return value: a newly created #cairo_font_face_t. Free with
 *  cairo_font_face_destroy() when you are done using it.
 *
 * Since: 1.6
 */
cairo_font_face_t *
cairo_quartz_font_face_create_for_cgfont (CGFontRef font)
{
    cairo_quartz_font_face_t *font_face;

    quartz_font_ensure_symbols();

    font_face = malloc (sizeof (cairo_quartz_font_face_t));
    if (!font_face) {
	cairo_status_t ignore_status;
	ignore_status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_font_face_t *)&_cairo_font_face_nil;
    }

    font_face->cgFont = CGFontRetain (font);

    if (CTFontCreateWithGraphicsFontPtr) {
        font_face->ctFont = CreateCTFontFromCGFontWithVariations (font, 1.0);
    } else {
        font_face->ctFont = NULL;
    }

    _cairo_font_face_init (&font_face->base, &_cairo_quartz_font_face_backend);

    return &font_face->base;
}

/*
 * scaled font backend
 */

static cairo_quartz_font_face_t *
_cairo_quartz_scaled_to_face (void *abstract_font)
{
    cairo_quartz_scaled_font_t *sfont = (cairo_quartz_scaled_font_t*) abstract_font;
    cairo_font_face_t *font_face = sfont->base.font_face;
    assert (font_face->backend->type == CAIRO_FONT_TYPE_QUARTZ);
    return (cairo_quartz_font_face_t*) font_face;
}

static void
_cairo_quartz_scaled_font_fini(void *abstract_font)
{
}

#define INVALID_GLYPH 0x00

static inline CGGlyph
_cairo_quartz_scaled_glyph_index (cairo_scaled_glyph_t *scaled_glyph) {
    unsigned long index = _cairo_scaled_glyph_index (scaled_glyph);
    if (index > 0xffff)
	return INVALID_GLYPH;
    return (CGGlyph) index;
}

static cairo_int_status_t
_cairo_quartz_init_glyph_metrics (cairo_quartz_scaled_font_t *font,
				  cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    cairo_quartz_font_face_t *font_face = _cairo_quartz_scaled_to_face(font);
    cairo_text_extents_t extents = {0, 0, 0, 0, 0, 0};
    CGGlyph glyph = _cairo_quartz_scaled_glyph_index (scaled_glyph);
    int advance;
    CGRect bbox;
    double emscale = CGFontGetUnitsPerEmPtr (font_face->cgFont);
    double xscale, yscale;
    double xmin, ymin, xmax, ymax;

    if (glyph == INVALID_GLYPH)
	goto FAIL;

    if (!CGFontGetGlyphAdvancesPtr (font_face->cgFont, &glyph, 1, &advance) ||
	!CGFontGetGlyphBBoxesPtr (font_face->cgFont, &glyph, 1, &bbox))
	goto FAIL;

    /* broken fonts like Al Bayan return incorrect bounds for some null characters,
       see https://bugzilla.mozilla.org/show_bug.cgi?id=534260 */
    if (unlikely (bbox.origin.x == -32767 &&
                  bbox.origin.y == -32767 &&
                  bbox.size.width == 65534 &&
                  bbox.size.height == 65534)) {
        bbox.origin.x = bbox.origin.y = 0;
        bbox.size.width = bbox.size.height = 0;
    }

    status = _cairo_matrix_compute_basis_scale_factors (&font->base.scale,
						  &xscale, &yscale, 1);
    if (status)
	goto FAIL;

    bbox = CGRectMake (bbox.origin.x / emscale,
		       bbox.origin.y / emscale,
		       bbox.size.width / emscale,
		       bbox.size.height / emscale);

    /* Should we want to always integer-align glyph extents, we can do so in this way */
#if 0
    {
	CGAffineTransform textMatrix;
	textMatrix = CGAffineTransformMake (font->base.scale.xx,
					    -font->base.scale.yx,
					    -font->base.scale.xy,
					    font->base.scale.yy,
					    0.0f, 0.0f);

	bbox = CGRectApplyAffineTransform (bbox, textMatrix);
	bbox = CGRectIntegral (bbox);
	bbox = CGRectApplyAffineTransform (bbox, CGAffineTransformInvert (textMatrix));
    }
#endif

#if 0
    fprintf (stderr, "[0x%04x] bbox: %f %f %f %f\n", glyph,
	     bbox.origin.x / emscale, bbox.origin.y / emscale,
	     bbox.size.width / emscale, bbox.size.height / emscale);
#endif

    xmin = CGRectGetMinX(bbox);
    ymin = CGRectGetMinY(bbox);
    xmax = CGRectGetMaxX(bbox);
    ymax = CGRectGetMaxY(bbox);

    extents.x_bearing = xmin;
    extents.y_bearing = - ymax;
    extents.width = xmax - xmin;
    extents.height = ymax - ymin;

    extents.x_advance = (double) advance / emscale;
    extents.y_advance = 0.0;

#if 0
    fprintf (stderr, "[0x%04x] extents: bearings: %f %f dim: %f %f adv: %f\n\n", glyph,
	     extents.x_bearing, extents.y_bearing, extents.width, extents.height, extents.x_advance);
#endif

  FAIL:
    _cairo_scaled_glyph_set_metrics (scaled_glyph,
				     &font->base,
				     &extents);

    return status;
}

static void
_cairo_quartz_path_apply_func (void *info, const CGPathElement *el)
{
    cairo_path_fixed_t *path = (cairo_path_fixed_t *) info;
    cairo_status_t status;

    switch (el->type) {
	case kCGPathElementMoveToPoint:
	    status = _cairo_path_fixed_move_to (path,
						_cairo_fixed_from_double(el->points[0].x),
						_cairo_fixed_from_double(el->points[0].y));
	    assert(!status);
	    break;
	case kCGPathElementAddLineToPoint:
	    status = _cairo_path_fixed_line_to (path,
						_cairo_fixed_from_double(el->points[0].x),
						_cairo_fixed_from_double(el->points[0].y));
	    assert(!status);
	    break;
	case kCGPathElementAddQuadCurveToPoint: {
	    cairo_fixed_t fx, fy;
	    double x, y;
	    if (!_cairo_path_fixed_get_current_point (path, &fx, &fy))
		fx = fy = 0;
	    x = _cairo_fixed_to_double (fx);
	    y = _cairo_fixed_to_double (fy);

	    status = _cairo_path_fixed_curve_to (path,
						 _cairo_fixed_from_double((x + el->points[0].x * 2.0) / 3.0),
						 _cairo_fixed_from_double((y + el->points[0].y * 2.0) / 3.0),
						 _cairo_fixed_from_double((el->points[0].x * 2.0 + el->points[1].x) / 3.0),
						 _cairo_fixed_from_double((el->points[0].y * 2.0 + el->points[1].y) / 3.0),
						 _cairo_fixed_from_double(el->points[1].x),
						 _cairo_fixed_from_double(el->points[1].y));
	}
	    assert(!status);
	    break;
	case kCGPathElementAddCurveToPoint:
	    status = _cairo_path_fixed_curve_to (path,
						 _cairo_fixed_from_double(el->points[0].x),
						 _cairo_fixed_from_double(el->points[0].y),
						 _cairo_fixed_from_double(el->points[1].x),
						 _cairo_fixed_from_double(el->points[1].y),
						 _cairo_fixed_from_double(el->points[2].x),
						 _cairo_fixed_from_double(el->points[2].y));
	    assert(!status);	    
	    break;
	case kCGPathElementCloseSubpath:
	    status = _cairo_path_fixed_close_path (path);
	    assert(!status);
	    break;
    }
}

static cairo_int_status_t
_cairo_quartz_init_glyph_path (cairo_quartz_scaled_font_t *font,
			       cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_quartz_font_face_t *font_face = _cairo_quartz_scaled_to_face(font);
    CGGlyph glyph = _cairo_quartz_scaled_glyph_index (scaled_glyph);
    CGAffineTransform textMatrix;
    CGPathRef glyphPath;
    CTFontRef ctFont;
    cairo_path_fixed_t *path;

    if (glyph == INVALID_GLYPH) {
	_cairo_scaled_glyph_set_path (scaled_glyph, &font->base, _cairo_path_fixed_create());
	return CAIRO_STATUS_SUCCESS;
    }

    /* scale(1,-1) * font->base.scale */
    textMatrix = CGAffineTransformMake (font->base.scale.xx,
					font->base.scale.yx,
					-font->base.scale.xy,
					-font->base.scale.yy,
					0, 0);

    ctFont = CreateCTFontFromCGFontWithVariations (font_face->cgFont, 1.0);
    glyphPath = CTFontCreatePathForGlyph (ctFont, glyph, &textMatrix);
    CFRelease (ctFont);
    if (!glyphPath)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    path = _cairo_path_fixed_create ();
    if (!path) {
	CGPathRelease (glyphPath);
	return _cairo_error(CAIRO_STATUS_NO_MEMORY);
    }

    CGPathApply (glyphPath, path, _cairo_quartz_path_apply_func);

    CGPathRelease (glyphPath);

    _cairo_scaled_glyph_set_path (scaled_glyph, &font->base, path);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartz_init_glyph_surface (cairo_quartz_scaled_font_t *font,
				  cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    cairo_quartz_font_face_t *font_face = _cairo_quartz_scaled_to_face(font);

    cairo_image_surface_t *surface = NULL;

    CGGlyph glyph = _cairo_quartz_scaled_glyph_index (scaled_glyph);

    int advance;
    CGRect bbox;
    double width, height;
    double xscale, yscale;
    double emscale = CGFontGetUnitsPerEmPtr (font_face->cgFont);

    CGContextRef cgContext = NULL;
    CGAffineTransform textMatrix;
    CGRect glyphRect, glyphRectInt;
    CGPoint glyphOrigin;

    //fprintf (stderr, "scaled_glyph: %p surface: %p\n", scaled_glyph, scaled_glyph->surface);

    /* Create blank 2x2 image if we don't have this character.
     * Maybe we should draw a better missing-glyph slug or something,
     * but this is ok for now.
     */
    if (glyph == INVALID_GLYPH) {
	surface = (cairo_image_surface_t*) cairo_image_surface_create (CAIRO_FORMAT_A8, 2, 2);
	status = cairo_surface_status ((cairo_surface_t *) surface);
	if (status)
	    return status;

	_cairo_scaled_glyph_set_surface (scaled_glyph,
					 &font->base,
					 surface);
	return CAIRO_STATUS_SUCCESS;
    }

    if (!CGFontGetGlyphAdvancesPtr (font_face->cgFont, &glyph, 1, &advance) ||
	!CGFontGetGlyphBBoxesPtr (font_face->cgFont, &glyph, 1, &bbox))
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = _cairo_matrix_compute_basis_scale_factors (&font->base.scale,
						  &xscale, &yscale, 1);
    if (status)
	return status;

    /* scale(1,-1) * font->base.scale * scale(1,-1) */
    textMatrix = CGAffineTransformMake (font->base.scale.xx,
					-font->base.scale.yx,
					-font->base.scale.xy,
					font->base.scale.yy,
					0, -0);
    glyphRect = CGRectMake (bbox.origin.x / emscale,
			    bbox.origin.y / emscale,
			    bbox.size.width / emscale,
			    bbox.size.height / emscale);

    glyphRect = CGRectApplyAffineTransform (glyphRect, textMatrix);

    /* Round the rectangle outwards, so that we don't have to deal
     * with non-integer-pixel origins or dimensions.
     */
    glyphRectInt = CGRectIntegral (glyphRect);

#if 0
    fprintf (stderr, "glyphRect[o]: %f %f %f %f\n",
	     glyphRect.origin.x, glyphRect.origin.y, glyphRect.size.width, glyphRect.size.height);
    fprintf (stderr, "glyphRectInt: %f %f %f %f\n",
	     glyphRectInt.origin.x, glyphRectInt.origin.y, glyphRectInt.size.width, glyphRectInt.size.height);
#endif

    glyphOrigin = glyphRectInt.origin;

    //textMatrix = CGAffineTransformConcat (textMatrix, CGAffineTransformInvert (ctm));

    width = glyphRectInt.size.width;
    height = glyphRectInt.size.height;

    //fprintf (stderr, "glyphRect[n]: %f %f %f %f\n", glyphRect.origin.x, glyphRect.origin.y, glyphRect.size.width, glyphRect.size.height);

    surface = (cairo_image_surface_t*) cairo_image_surface_create (CAIRO_FORMAT_A8, width, height);
    if (surface->base.status)
	return surface->base.status;

    if (surface->width != 0 && surface->height != 0) {
	cgContext = CGBitmapContextCreate (surface->data,
					   surface->width,
					   surface->height,
					   8,
					   surface->stride,
					   NULL,
					   kCGImageAlphaOnly);

	if (cgContext == NULL) {
	    cairo_surface_destroy (&surface->base);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	CGContextSetFont (cgContext, font_face->cgFont);
	CGContextSetFontSize (cgContext, 1.0);
	CGContextSetTextMatrix (cgContext, textMatrix);

	switch (font->base.options.antialias) {
	case CAIRO_ANTIALIAS_SUBPIXEL:
	    CGContextSetShouldAntialias (cgContext, TRUE);
	    CGContextSetShouldSmoothFonts (cgContext, TRUE);
	    if (CGContextSetAllowsFontSmoothingPtr &&
		!CGContextGetAllowsFontSmoothingPtr (cgContext))
		CGContextSetAllowsFontSmoothingPtr (cgContext, TRUE);
	    break;
	case CAIRO_ANTIALIAS_NONE:
	    CGContextSetShouldAntialias (cgContext, FALSE);
	    break;
	case CAIRO_ANTIALIAS_GRAY:
	    CGContextSetShouldAntialias (cgContext, TRUE);
	    CGContextSetShouldSmoothFonts (cgContext, FALSE);
	    break;
	case CAIRO_ANTIALIAS_DEFAULT:
	default:
	    /* Don't do anything */
	    break;
	}

	CGContextSetAlpha (cgContext, 1.0);
	CGContextShowGlyphsAtPoint (cgContext, - glyphOrigin.x, - glyphOrigin.y, &glyph, 1);

	CGContextRelease (cgContext);
    }

    cairo_surface_set_device_offset (&surface->base,
				     - glyphOrigin.x,
				     height + glyphOrigin.y);

    _cairo_scaled_glyph_set_surface (scaled_glyph, &font->base, surface);

    return status;
}

static cairo_int_status_t
_cairo_quartz_scaled_glyph_init (void *abstract_font,
				 cairo_scaled_glyph_t *scaled_glyph,
				 cairo_scaled_glyph_info_t info)
{
    cairo_quartz_scaled_font_t *font = (cairo_quartz_scaled_font_t *) abstract_font;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    if (!status && (info & CAIRO_SCALED_GLYPH_INFO_METRICS))
	status = _cairo_quartz_init_glyph_metrics (font, scaled_glyph);

    if (!status && (info & CAIRO_SCALED_GLYPH_INFO_PATH))
	status = _cairo_quartz_init_glyph_path (font, scaled_glyph);

    if (!status && (info & CAIRO_SCALED_GLYPH_INFO_SURFACE))
	status = _cairo_quartz_init_glyph_surface (font, scaled_glyph);

    return status;
}

static unsigned long
_cairo_quartz_ucs4_to_index (void *abstract_font,
			     uint32_t ucs4)
{
    cairo_quartz_scaled_font_t *font = (cairo_quartz_scaled_font_t*) abstract_font;
    cairo_quartz_font_face_t *ffont = _cairo_quartz_scaled_to_face(font);
    UniChar u = (UniChar) ucs4;
    CGGlyph glyph;

    CGFontGetGlyphsForUnicharsPtr (ffont->cgFont, &u, &glyph, 1);

    return glyph;
}

static const cairo_scaled_font_backend_t _cairo_quartz_scaled_font_backend = {
    CAIRO_FONT_TYPE_QUARTZ,
    _cairo_quartz_scaled_font_fini,
    _cairo_quartz_scaled_glyph_init,
    NULL, /* text_to_glyphs */
    _cairo_quartz_ucs4_to_index,
    NULL, /* show_glyphs */
    NULL, /* load_truetype_table */
    NULL, /* map_glyphs_to_unicode */
};

/*
 * private methods that the quartz surface uses
 */

CGFontRef
_cairo_quartz_scaled_font_get_cg_font_ref (cairo_scaled_font_t *abstract_font)
{
    cairo_quartz_font_face_t *ffont = _cairo_quartz_scaled_to_face(abstract_font);

    return ffont->cgFont;
}

CTFontRef
_cairo_quartz_scaled_font_get_ct_font_ref (cairo_scaled_font_t *abstract_font)
{
    cairo_quartz_font_face_t *ffont = _cairo_quartz_scaled_to_face(abstract_font);

    return ffont->ctFont;
}

#if !defined(__LP64__) && !TARGET_OS_IPHONE
/*
 * compat with old ATSUI backend
 */

/**
 * cairo_quartz_font_face_create_for_atsu_font_id
 * @font_id: an ATSUFontID for the font.
 *
 * Creates a new font for the Quartz font backend based on an
 * #ATSUFontID. This font can then be used with
 * cairo_set_font_face() or cairo_scaled_font_create().
 *
 * Return value: a newly created #cairo_font_face_t. Free with
 *  cairo_font_face_destroy() when you are done using it.
 *
 * Since: 1.6
 **/
cairo_font_face_t *
cairo_quartz_font_face_create_for_atsu_font_id (ATSUFontID font_id)
{
    ATSFontRef atsFont = FMGetATSFontRefFromFont (font_id);
    CGFontRef cgFont = CGFontCreateWithPlatformFont (&atsFont);
    cairo_font_face_t *ff;

    ff = cairo_quartz_font_face_create_for_cgfont (cgFont);

    CGFontRelease (cgFont);

    return ff;
}

/* This is the old name for the above function, exported for compat purposes */
cairo_font_face_t *cairo_atsui_font_face_create_for_atsu_font_id (ATSUFontID font_id);

cairo_font_face_t *
cairo_atsui_font_face_create_for_atsu_font_id (ATSUFontID font_id)
{
    return cairo_quartz_font_face_create_for_atsu_font_id (font_id);
}
#endif
