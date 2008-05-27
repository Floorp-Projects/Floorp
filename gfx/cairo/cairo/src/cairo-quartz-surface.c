/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright � 2006, 2007 Mozilla Corporation
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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 *
 * Contributor(s):
 *	Vladimir Vukicevic <vladimir@mozilla.com>
 */

#include "cairoint.h"

#include "cairo-quartz-private.h"

#include <dlfcn.h>

/* The 10.5 SDK includes a funky new definition of FloatToFixed which
 * causes all sorts of breakage; so reset to old-style definition
 */
#ifdef FloatToFixed
#undef FloatToFixed
#define FloatToFixed(a)     ((Fixed)((float)(a) * fixed1))
#endif

#include <limits.h>

#undef QUARTZ_DEBUG

#ifdef QUARTZ_DEBUG
#define ND(_x)	fprintf _x
#else
#define ND(_x)	do {} while(0)
#endif

#define IS_EMPTY(s) ((s)->extents.width == 0 || (s)->extents.height == 0)

/* This method is private, but it exists.  Its params are are exposed
 * as args to the NS* method, but not as CG.
 */
enum PrivateCGCompositeMode {
    kPrivateCGCompositeClear		= 0,
    kPrivateCGCompositeCopy		= 1,
    kPrivateCGCompositeSourceOver	= 2,
    kPrivateCGCompositeSourceIn		= 3,
    kPrivateCGCompositeSourceOut	= 4,
    kPrivateCGCompositeSourceAtop	= 5,
    kPrivateCGCompositeDestinationOver	= 6,
    kPrivateCGCompositeDestinationIn	= 7,
    kPrivateCGCompositeDestinationOut	= 8,
    kPrivateCGCompositeDestinationAtop	= 9,
    kPrivateCGCompositeXOR		= 10,
    kPrivateCGCompositePlusDarker	= 11, // (max (0, (1-d) + (1-s)))
    kPrivateCGCompositePlusLighter	= 12, // (min (1, s + d))
};
typedef enum PrivateCGCompositeMode PrivateCGCompositeMode;
CG_EXTERN void CGContextSetCompositeOperation (CGContextRef, PrivateCGCompositeMode);
CG_EXTERN void CGContextResetCTM (CGContextRef);
CG_EXTERN void CGContextSetCTM (CGContextRef, CGAffineTransform);
CG_EXTERN void CGContextResetClip (CGContextRef);
CG_EXTERN CGSize CGContextGetPatternPhase (CGContextRef);

/* We need to work with the 10.3 SDK as well (and 10.3 machines; luckily, 10.3.9
 * has all the stuff we care about, just some of it isn't exported in the SDK.
 */
#ifndef kCGBitmapByteOrder32Host
#define USE_10_3_WORKAROUNDS
#define kCGBitmapAlphaInfoMask 0x1F
#define kCGBitmapByteOrderMask 0x7000
#define kCGBitmapByteOrder32Host 0

typedef uint32_t CGBitmapInfo;

/* public in 10.4, present in 10.3.9 */
CG_EXTERN void CGContextReplacePathWithStrokedPath (CGContextRef);
CG_EXTERN CGImageRef CGBitmapContextCreateImage (CGContextRef);
#endif

/* Some of these are present in earlier versions of the OS than where
 * they are public; others are not public at all (CGContextCopyPath,
 * CGContextReplacePathWithClipPath, many of the getters, etc.)
 */
static void (*CGContextClipToMaskPtr) (CGContextRef, CGRect, CGImageRef) = NULL;
static void (*CGContextDrawTiledImagePtr) (CGContextRef, CGRect, CGImageRef) = NULL;
static unsigned int (*CGContextGetTypePtr) (CGContextRef) = NULL;
static void (*CGContextSetShouldAntialiasFontsPtr) (CGContextRef, bool) = NULL;
static bool (*CGContextGetShouldAntialiasFontsPtr) (CGContextRef) = NULL;
static bool (*CGContextGetShouldSmoothFontsPtr) (CGContextRef) = NULL;
static void (*CGContextSetAllowsFontSmoothingPtr) (CGContextRef, bool) = NULL;
static bool (*CGContextGetAllowsFontSmoothingPtr) (CGContextRef) = NULL;
static CGPathRef (*CGContextCopyPathPtr) (CGContextRef) = NULL;
static void (*CGContextReplacePathWithClipPathPtr) (CGContextRef) = NULL;

static SInt32 _cairo_quartz_osx_version = 0x0;

static cairo_bool_t _cairo_quartz_symbol_lookup_done = FALSE;

/*
 * Utility functions
 */

#ifdef QUARTZ_DEBUG
static void quartz_surface_to_png (cairo_quartz_surface_t *nq, char *dest);
static void quartz_image_to_png (CGImageRef, char *dest);
#endif

static cairo_quartz_surface_t *
_cairo_quartz_surface_create_internal (CGContextRef cgContext,
				       cairo_content_t content,
				       unsigned int width,
				       unsigned int height);

/* Load all extra symbols */
static void quartz_ensure_symbols(void)
{
    if (_cairo_quartz_symbol_lookup_done)
	return;

    CGContextClipToMaskPtr = dlsym(RTLD_DEFAULT, "CGContextClipToMask");
    CGContextDrawTiledImagePtr = dlsym(RTLD_DEFAULT, "CGContextDrawTiledImage");
    CGContextGetTypePtr = dlsym(RTLD_DEFAULT, "CGContextGetType");
    CGContextSetShouldAntialiasFontsPtr = dlsym(RTLD_DEFAULT, "CGContextSetShouldAntialiasFonts");
    CGContextGetShouldAntialiasFontsPtr = dlsym(RTLD_DEFAULT, "CGContextGetShouldAntialiasFonts");
    CGContextGetShouldSmoothFontsPtr = dlsym(RTLD_DEFAULT, "CGContextGetShouldSmoothFonts");
    CGContextCopyPathPtr = dlsym(RTLD_DEFAULT, "CGContextCopyPath");
    CGContextReplacePathWithClipPathPtr = dlsym(RTLD_DEFAULT, "CGContextReplacePathWithClipPath");
    CGContextGetAllowsFontSmoothingPtr = dlsym(RTLD_DEFAULT, "CGContextGetAllowsFontSmoothing");
    CGContextSetAllowsFontSmoothingPtr = dlsym(RTLD_DEFAULT, "CGContextSetAllowsFontSmoothing");

    if (Gestalt(gestaltSystemVersion, &_cairo_quartz_osx_version) != noErr) {
	// assume 10.4
	_cairo_quartz_osx_version = 0x1040;
    }

    _cairo_quartz_symbol_lookup_done = TRUE;
}

static inline cairo_bool_t
_cairo_quartz_is_cgcontext_bitmap_context (CGContextRef cgc) {
    if (cgc == NULL)
	return FALSE;

    if (CGContextGetTypePtr) {
	/* 4 is the type value of a bitmap context */
	if (CGContextGetTypePtr(cgc) == 4)
	    return TRUE;
	return FALSE;
    }

    /* This will cause a (harmless) warning to be printed if called on a non-bitmap context */
    return CGBitmapContextGetBitsPerPixel(cgc) != 0;
}

/* CoreGraphics limitation with flipped CTM surfaces: height must be less than signed 16-bit max */

#define CG_MAX_HEIGHT   SHRT_MAX
#define CG_MAX_WIDTH    USHRT_MAX

/* is the desired size of the surface within bounds? */
cairo_bool_t
_cairo_quartz_verify_surface_size(int width, int height)
{
    /* hmmm, allow width, height == 0 ? */
    if (width < 0 || height < 0) {
	return FALSE;
    }

    if (width > CG_MAX_WIDTH || height > CG_MAX_HEIGHT) {
	return FALSE;
    }

    return TRUE;
}

/*
 * Cairo path -> Quartz path conversion helpers
 */

typedef struct _quartz_stroke {
    CGContextRef cgContext;
    cairo_matrix_t *ctm_inverse;
} quartz_stroke_t;

/* cairo path -> execute in context */
static cairo_status_t
_cairo_path_to_quartz_context_move_to (void *closure, cairo_point_t *point)
{
    //ND((stderr, "moveto: %f %f\n", _cairo_fixed_to_double(point->x), _cairo_fixed_to_double(point->y)));
    quartz_stroke_t *stroke = (quartz_stroke_t *)closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (stroke->ctm_inverse)
	cairo_matrix_transform_point (stroke->ctm_inverse, &x, &y);

    CGContextMoveToPoint (stroke->cgContext, x, y);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_context_line_to (void *closure, cairo_point_t *point)
{
    //ND((stderr, "lineto: %f %f\n",  _cairo_fixed_to_double(point->x), _cairo_fixed_to_double(point->y)));
    quartz_stroke_t *stroke = (quartz_stroke_t *)closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (stroke->ctm_inverse)
	cairo_matrix_transform_point (stroke->ctm_inverse, &x, &y);

    if (CGContextIsPathEmpty (stroke->cgContext))
	CGContextMoveToPoint (stroke->cgContext, x, y);
    else
	CGContextAddLineToPoint (stroke->cgContext, x, y);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_context_curve_to (void *closure, cairo_point_t *p0, cairo_point_t *p1, cairo_point_t *p2)
{
    //ND( (stderr, "curveto: %f,%f %f,%f %f,%f\n",
    //		   _cairo_fixed_to_double(p0->x), _cairo_fixed_to_double(p0->y),
    //		   _cairo_fixed_to_double(p1->x), _cairo_fixed_to_double(p1->y),
    //		   _cairo_fixed_to_double(p2->x), _cairo_fixed_to_double(p2->y)));
    quartz_stroke_t *stroke = (quartz_stroke_t *)closure;
    double x0 = _cairo_fixed_to_double (p0->x);
    double y0 = _cairo_fixed_to_double (p0->y);
    double x1 = _cairo_fixed_to_double (p1->x);
    double y1 = _cairo_fixed_to_double (p1->y);
    double x2 = _cairo_fixed_to_double (p2->x);
    double y2 = _cairo_fixed_to_double (p2->y);

    if (stroke->ctm_inverse) {
	cairo_matrix_transform_point (stroke->ctm_inverse, &x0, &y0);
	cairo_matrix_transform_point (stroke->ctm_inverse, &x1, &y1);
	cairo_matrix_transform_point (stroke->ctm_inverse, &x2, &y2);
    }

    CGContextAddCurveToPoint (stroke->cgContext,
			      x0, y0, x1, y1, x2, y2);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_context_close_path (void *closure)
{
    //ND((stderr, "closepath\n"));
    quartz_stroke_t *stroke = (quartz_stroke_t *)closure;
    CGContextClosePath (stroke->cgContext);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_cairo_path_to_quartz_context (cairo_path_fixed_t *path,
					    quartz_stroke_t *stroke)
{
    return _cairo_path_fixed_interpret (path,
					CAIRO_DIRECTION_FORWARD,
					_cairo_path_to_quartz_context_move_to,
					_cairo_path_to_quartz_context_line_to,
					_cairo_path_to_quartz_context_curve_to,
					_cairo_path_to_quartz_context_close_path,
					stroke);
}

/*
 * Misc helpers/callbacks
 */

static PrivateCGCompositeMode
_cairo_quartz_cairo_operator_to_quartz (cairo_operator_t op)
{
    switch (op) {
	case CAIRO_OPERATOR_CLEAR:
	    return kPrivateCGCompositeClear;
	case CAIRO_OPERATOR_SOURCE:
	    return kPrivateCGCompositeCopy;
	case CAIRO_OPERATOR_OVER:
	    return kPrivateCGCompositeSourceOver;
	case CAIRO_OPERATOR_IN:
	    /* XXX This doesn't match image output */
	    return kPrivateCGCompositeSourceIn;
	case CAIRO_OPERATOR_OUT:
	    /* XXX This doesn't match image output */
	    return kPrivateCGCompositeSourceOut;
	case CAIRO_OPERATOR_ATOP:
	    return kPrivateCGCompositeSourceAtop;

	case CAIRO_OPERATOR_DEST:
	    /* XXX this is handled specially (noop)! */
	    return kPrivateCGCompositeCopy;
	case CAIRO_OPERATOR_DEST_OVER:
	    return kPrivateCGCompositeDestinationOver;
	case CAIRO_OPERATOR_DEST_IN:
	    /* XXX This doesn't match image output */
	    return kPrivateCGCompositeDestinationIn;
	case CAIRO_OPERATOR_DEST_OUT:
	    return kPrivateCGCompositeDestinationOut;
	case CAIRO_OPERATOR_DEST_ATOP:
	    /* XXX This doesn't match image output */
	    return kPrivateCGCompositeDestinationAtop;

	case CAIRO_OPERATOR_XOR:
	    return kPrivateCGCompositeXOR; /* This will generate strange results */
	case CAIRO_OPERATOR_ADD:
	    return kPrivateCGCompositePlusLighter;
	case CAIRO_OPERATOR_SATURATE:
	    /* XXX This doesn't match image output for SATURATE; there's no equivalent */
	    return kPrivateCGCompositePlusDarker;  /* ??? */
    }


    return kPrivateCGCompositeCopy;
}

static inline CGLineCap
_cairo_quartz_cairo_line_cap_to_quartz (cairo_line_cap_t ccap)
{
    switch (ccap) {
	case CAIRO_LINE_CAP_BUTT: return kCGLineCapButt; break;
	case CAIRO_LINE_CAP_ROUND: return kCGLineCapRound; break;
	case CAIRO_LINE_CAP_SQUARE: return kCGLineCapSquare; break;
    }

    return kCGLineCapButt;
}

static inline CGLineJoin
_cairo_quartz_cairo_line_join_to_quartz (cairo_line_join_t cjoin)
{
    switch (cjoin) {
	case CAIRO_LINE_JOIN_MITER: return kCGLineJoinMiter; break;
	case CAIRO_LINE_JOIN_ROUND: return kCGLineJoinRound; break;
	case CAIRO_LINE_JOIN_BEVEL: return kCGLineJoinBevel; break;
    }

    return kCGLineJoinMiter;
}

static inline CGInterpolationQuality
_cairo_quartz_filter_to_quartz (cairo_filter_t filter)
{
    switch (filter) {
	case CAIRO_FILTER_NEAREST:
	    return kCGInterpolationNone;

	case CAIRO_FILTER_FAST:
	    return kCGInterpolationLow;

	case CAIRO_FILTER_BEST:
	case CAIRO_FILTER_GOOD:
	case CAIRO_FILTER_BILINEAR:
	case CAIRO_FILTER_GAUSSIAN:
	    return kCGInterpolationDefault;
    }

    return kCGInterpolationDefault;
}

static inline void
_cairo_quartz_cairo_matrix_to_quartz (const cairo_matrix_t *src,
				      CGAffineTransform *dst)
{
    dst->a = src->xx;
    dst->b = src->yx;
    dst->c = src->xy;
    dst->d = src->yy;
    dst->tx = src->x0;
    dst->ty = src->y0;
}

typedef struct {
    bool isClipping;
    CGGlyph *cg_glyphs;
    CGSize *cg_advances;
    size_t nglyphs;
    CGAffineTransform textTransform;
    CGFontRef font;
    CGPoint origin;
} unbounded_show_glyphs_t;

typedef struct {
    CGPathRef cgPath;
    cairo_fill_rule_t fill_rule;
} unbounded_stroke_fill_t;

typedef struct {
    CGImageRef mask;
    CGAffineTransform maskTransform;
} unbounded_mask_t;

typedef enum {
    UNBOUNDED_STROKE_FILL,
    UNBOUNDED_SHOW_GLYPHS,
    UNBOUNDED_MASK
} unbounded_op_t;

typedef struct {
    unbounded_op_t op;
    union {
	unbounded_stroke_fill_t stroke_fill;
	unbounded_show_glyphs_t show_glyphs;
	unbounded_mask_t mask;
    } u;
} unbounded_op_data_t;

static void
_cairo_quartz_fixup_unbounded_operation (cairo_quartz_surface_t *surface,
					 unbounded_op_data_t *op,
					 cairo_antialias_t antialias)
{
    CGColorSpaceRef gray;
    CGRect clipBox, clipBoxRound;
    CGContextRef cgc;
    CGImageRef maskImage;

    if (!CGContextClipToMaskPtr)
	return;

    clipBox = CGContextGetClipBoundingBox (surface->cgContext);
    clipBoxRound = CGRectIntegral (clipBox);

    gray = CGColorSpaceCreateDeviceGray ();
    cgc = CGBitmapContextCreate (NULL,
				 clipBoxRound.size.width,
				 clipBoxRound.size.height,
				 8,
				 clipBoxRound.size.width,
				 gray,
				 kCGImageAlphaNone);
    CGColorSpaceRelease (gray);

    if (!cgc)
	return;

    /* We want to mask out whatever we just rendered, so we fill the
     * surface with white, and then we'll render with black.
     */
    CGContextSetRGBFillColor (cgc, 1.0f, 1.0f, 1.0f, 1.0f);
    CGContextFillRect (cgc, CGRectMake (0, 0, clipBoxRound.size.width, clipBoxRound.size.height));

    CGContextSetRGBFillColor (cgc, 0.0f, 0.0f, 0.0f, 1.0f);
    CGContextSetShouldAntialias (cgc, (antialias != CAIRO_ANTIALIAS_NONE));

    CGContextTranslateCTM (cgc, -clipBoxRound.origin.x, -clipBoxRound.origin.y);

    /* We need to either render the path that was given to us, or the glyph op */
    if (op->op == UNBOUNDED_STROKE_FILL) {
	CGContextBeginPath (cgc);
	CGContextAddPath (cgc, op->u.stroke_fill.cgPath);

	if (op->u.stroke_fill.fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextFillPath (cgc);
	else
	    CGContextEOFillPath (cgc);
    } else if (op->op == UNBOUNDED_SHOW_GLYPHS) {
	CGContextSetFont (cgc, op->u.show_glyphs.font);
	CGContextSetFontSize (cgc, 1.0);
	CGContextSetTextMatrix (cgc, op->u.show_glyphs.textTransform);
	CGContextTranslateCTM (cgc, op->u.show_glyphs.origin.x, op->u.show_glyphs.origin.y);

	if (op->u.show_glyphs.isClipping) {
	    /* Note that the comment in show_glyphs about kCGTextClip
	     * and the text transform still applies here; however, the
	     * cg_advances we have were already transformed, so we
	     * don't have to do anything. */
	    CGContextSetTextDrawingMode (cgc, kCGTextClip);
	    CGContextSaveGState (cgc);
	}

	CGContextShowGlyphsWithAdvances (cgc,
					 op->u.show_glyphs.cg_glyphs,
					 op->u.show_glyphs.cg_advances,
					 op->u.show_glyphs.nglyphs);

	if (op->u.show_glyphs.isClipping) {
	    CGContextFillRect (cgc, CGRectMake (0.0, 0.0, clipBoxRound.size.width, clipBoxRound.size.height));
	    CGContextRestoreGState (cgc);
	}
    } else if (op->op == UNBOUNDED_MASK) {
	CGContextSaveGState (cgc);
	CGContextConcatCTM (cgc, op->u.mask.maskTransform);
	CGContextClipToMask (cgc, CGRectMake (0.0f, 0.0f,
					      CGImageGetWidth(op->u.mask.mask), CGImageGetHeight(op->u.mask.mask)),
			     op->u.mask.mask);
	CGContextFillRect (cgc, CGRectMake (0.0, 0.0, clipBoxRound.size.width, clipBoxRound.size.height));
	CGContextRestoreGState (cgc);
    }

    /* Also mask out the portion of the clipbox that we rounded out, if any */
    if (!CGRectEqualToRect (clipBox, clipBoxRound)) {
	CGContextBeginPath (cgc);
	CGContextAddRect (cgc, CGRectMake (0.0, 0.0, clipBoxRound.size.width, clipBoxRound.size.height));
	CGContextAddRect (cgc, CGRectMake (clipBoxRound.origin.x - clipBox.origin.x,
					   clipBoxRound.origin.y - clipBox.origin.y,
					   clipBox.size.width,
					   clipBox.size.height));
	CGContextEOFillPath (cgc);
    }

    maskImage = CGBitmapContextCreateImage (cgc);
    CGContextRelease (cgc);

    if (!maskImage)
	return;

    /* Then render with the mask */
    CGContextSaveGState (surface->cgContext);

    CGContextSetCompositeOperation (surface->cgContext, kPrivateCGCompositeCopy);
    CGContextClipToMaskPtr (surface->cgContext, clipBoxRound, maskImage);
    CGImageRelease (maskImage);

    /* Finally, clear out the entire clipping region through our mask */
    CGContextClearRect (surface->cgContext, clipBoxRound);

    CGContextRestoreGState (surface->cgContext);
}

/*
 * Source -> Quartz setup and finish functions
 */

static void
ComputeGradientValue (void *info, const float *in, float *out)
{
    double fdist = *in;
    cairo_gradient_pattern_t *grad = (cairo_gradient_pattern_t*) info;
    unsigned int i;

    /* Put fdist back in the 0.0..1.0 range if we're doing
     * REPEAT/REFLECT
     */
    if (grad->base.extend == CAIRO_EXTEND_REPEAT) {
	fdist = fdist - floor(fdist);
    } else if (grad->base.extend == CAIRO_EXTEND_REFLECT) {
	fdist = fmod(fabs(fdist), 2.0);
	if (fdist > 1.0) {
	    fdist = 2.0 - fdist;
	}
    }

    for (i = 0; i < grad->n_stops; i++) {
	if (grad->stops[i].offset > fdist)
	    break;
    }

    if (i == 0 || i == grad->n_stops) {
	if (i == grad->n_stops)
	    --i;
	out[0] = grad->stops[i].color.red;
	out[1] = grad->stops[i].color.green;
	out[2] = grad->stops[i].color.blue;
	out[3] = grad->stops[i].color.alpha;
    } else {
	float ax = grad->stops[i-1].offset;
	float bx = grad->stops[i].offset - ax;
	float bp = (fdist - ax)/bx;
	float ap = 1.0 - bp;

	out[0] =
	    grad->stops[i-1].color.red * ap +
	    grad->stops[i].color.red * bp;
	out[1] =
	    grad->stops[i-1].color.green * ap +
	    grad->stops[i].color.green * bp;
	out[2] =
	    grad->stops[i-1].color.blue * ap +
	    grad->stops[i].color.blue * bp;
	out[3] =
	    grad->stops[i-1].color.alpha * ap +
	    grad->stops[i].color.alpha * bp;
    }
}

static CGFunctionRef
CreateGradientFunction (cairo_gradient_pattern_t *gpat)
{
    float input_value_range[2] = { 0.f, 1.f };
    float output_value_ranges[8] = { 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f };
    CGFunctionCallbacks callbacks = {
	0, ComputeGradientValue, (CGFunctionReleaseInfoCallback) cairo_pattern_destroy
    };

    return CGFunctionCreate (gpat,
			     1,
			     input_value_range,
			     4,
			     output_value_ranges,
			     &callbacks);
}

static CGFunctionRef
CreateRepeatingGradientFunction (cairo_quartz_surface_t *surface,
				 cairo_gradient_pattern_t *gpat,
				 CGPoint *start, CGPoint *end,
				 CGAffineTransform matrix)
{
    float input_value_range[2];
    float output_value_ranges[8] = { 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f };
    CGFunctionCallbacks callbacks = {
	0, ComputeGradientValue, (CGFunctionReleaseInfoCallback) cairo_pattern_destroy
    };

    CGPoint mstart, mend;

    double dx, dy;
    int x_rep_start = 0, x_rep_end = 0;
    int y_rep_start = 0, y_rep_end = 0;

    int rep_start, rep_end;

    // figure out how many times we'd need to repeat the gradient pattern
    // to cover the whole (transformed) surface area
    mstart = CGPointApplyAffineTransform (*start, matrix);
    mend = CGPointApplyAffineTransform (*end, matrix);

    dx = fabs (mend.x - mstart.x);
    dy = fabs (mend.y - mstart.y);

    if (dx > 1e-6) {
	x_rep_start = (int) ceil(MIN(mstart.x, mend.x) / dx);
	x_rep_end = (int) ceil((surface->extents.width - MAX(mstart.x, mend.x)) / dx);

	if (mend.x < mstart.x) {
	    int swap = x_rep_end;
	    x_rep_end = x_rep_start;
	    x_rep_start = swap;
	}
    }

    if (dy > 1e-6) {
	y_rep_start = (int) ceil(MIN(mstart.y, mend.y) / dy);
	y_rep_end = (int) ceil((surface->extents.width - MAX(mstart.y, mend.y)) / dy);

	if (mend.y < mstart.y) {
	    int swap = y_rep_end;
	    y_rep_end = y_rep_start;
	    y_rep_start = swap;
	}
    }

    rep_start = MAX(x_rep_start, y_rep_start);
    rep_end = MAX(x_rep_end, y_rep_end);

    // extend the line between start and end by rep_start times from the start
    // and rep_end times from the end

    dx = end->x - start->x;
    dy = end->y - start->y;

    start->x = start->x - dx * rep_start;
    start->y = start->y - dy * rep_start;

    end->x = end->x + dx * rep_end;
    end->y = end->y + dy * rep_end;

    // set the input range for the function -- the function knows how to
    // map values outside of 0.0 .. 1.0 to that range for REPEAT/REFLECT.
    input_value_range[0] = 0.0 - 1.0 * rep_start;
    input_value_range[1] = 1.0 + 1.0 * rep_end;

    return CGFunctionCreate (gpat,
			     1,
			     input_value_range,
			     4,
			     output_value_ranges,
			     &callbacks);
}

/* Obtain a CGImageRef from a #cairo_surface_t * */

static cairo_status_t
_cairo_surface_to_cgimage (cairo_surface_t *target,
			   cairo_surface_t *source,
			   CGImageRef *image_out)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_surface_type_t stype = cairo_surface_get_type (source);
    cairo_image_surface_t *isurf;
    CGImageRef image;
    void *image_extra;

    if (stype == CAIRO_SURFACE_TYPE_QUARTZ_IMAGE) {
	cairo_quartz_image_surface_t *surface = (cairo_quartz_image_surface_t *) source;
	*image_out = CGImageRetain (surface->image);
	return CAIRO_STATUS_SUCCESS;
    }

    if (stype == CAIRO_SURFACE_TYPE_QUARTZ) {
	cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) source;
	if (IS_EMPTY(surface)) {
	    *image_out = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}

	if (_cairo_quartz_is_cgcontext_bitmap_context (surface->cgContext)) {
	    *image_out = CGBitmapContextCreateImage (surface->cgContext);
	    if (*image_out)
		return CAIRO_STATUS_SUCCESS;
	}
    }

    if (stype != CAIRO_SURFACE_TYPE_IMAGE) {
	status = _cairo_surface_acquire_source_image (source, &isurf, &image_extra);
	if (status)
	    return status;
    } else {
	isurf = (cairo_image_surface_t *) source;
    }

    if (isurf->width == 0 || isurf->height == 0) {
	*image_out = NULL;
    } else {
	image = _cairo_quartz_create_cgimage (isurf->format,
					      isurf->width,
					      isurf->height,
					      isurf->stride,
					      isurf->data,
					      TRUE,
					      NULL, NULL, NULL);

	/* Create a copy to ensure that the CGImageRef doesn't depend on the image surface's backing store */
	*image_out = CGImageCreateCopy (image);
	CGImageRelease (image);
    }

    if ((cairo_surface_t*) isurf != source)
	_cairo_surface_release_source_image (source, isurf, image_extra);

    return status;
}

/* Generic #cairo_pattern_t -> CGPattern function */

typedef struct {
    CGImageRef image;
    CGRect imageBounds;
    cairo_bool_t do_reflect;
} SurfacePatternDrawInfo;

static void
SurfacePatternDrawFunc (void *ainfo, CGContextRef context)
{
    SurfacePatternDrawInfo *info = (SurfacePatternDrawInfo*) ainfo;

    CGContextTranslateCTM (context, 0, info->imageBounds.size.height);
    CGContextScaleCTM (context, 1, -1);

    CGContextDrawImage (context, info->imageBounds, info->image);
    if (info->do_reflect) {
	/* draw 3 more copies of the image, flipped.
	 * DrawImage draws the image according to the current Y-direction into the rectangle given
	 * (imageBounds); at the time of the first DrawImage above, the origin is at the bottom left
	 * of the base image position, and the Y axis is extending upwards.
	 */

	/* Make the y axis extend downwards, and draw a flipped image below */
	CGContextScaleCTM (context, 1, -1);
	CGContextDrawImage (context, info->imageBounds, info->image);

	/* Shift over to the right, and flip vertically (translation is 2x,
	 * since we'll be flipping and thus rendering the rectangle "backwards"
	 */
	CGContextTranslateCTM (context, 2 * info->imageBounds.size.width, 0);
	CGContextScaleCTM (context, -1, 1);
	CGContextDrawImage (context, info->imageBounds, info->image);

	/* Then unflip the Y-axis again, and draw the image above the point. */
	CGContextScaleCTM (context, 1, -1);
	CGContextDrawImage (context, info->imageBounds, info->image);

    }
}

static void
SurfacePatternReleaseInfoFunc (void *ainfo)
{
    SurfacePatternDrawInfo *info = (SurfacePatternDrawInfo*) ainfo;

    CGImageRelease (info->image);
    free (info);
}

static cairo_int_status_t
_cairo_quartz_cairo_repeating_surface_pattern_to_quartz (cairo_quartz_surface_t *dest,
							 cairo_pattern_t *apattern,
							 CGPatternRef *cgpat)
{
    cairo_surface_pattern_t *spattern;
    cairo_surface_t *pat_surf;
    cairo_rectangle_int_t extents;

    CGImageRef image;
    CGRect pbounds;
    CGAffineTransform ptransform, stransform;
    CGPatternCallbacks cb = { 0,
			      SurfacePatternDrawFunc,
			      SurfacePatternReleaseInfoFunc };
    SurfacePatternDrawInfo *info;
    float rw, rh;
    cairo_status_t status;

    cairo_matrix_t m;

    /* SURFACE is the only type we'll handle here */
    if (apattern->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    spattern = (cairo_surface_pattern_t *) apattern;
    pat_surf = spattern->surface;

    status = _cairo_surface_get_extents (pat_surf, &extents);
    if (status)
	return status;

    status = _cairo_surface_to_cgimage ((cairo_surface_t*) dest, pat_surf, &image);
    if (status != CAIRO_STATUS_SUCCESS)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (image == NULL)
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    info = malloc(sizeof(SurfacePatternDrawInfo));
    if (!info)
	return CAIRO_STATUS_NO_MEMORY;

    /* XXX -- if we're printing, we may need to call CGImageCreateCopy to make sure
     * that the data will stick around for this image when the printer gets to it.
     * Otherwise, the underlying data store may disappear from under us!
     *
     * _cairo_surface_to_cgimage will copy when it converts non-Quartz surfaces,
     * since the Quartz surfaces have a higher chance of sticking around.  If the
     * source is a quartz image surface, then it's set up to retain a ref to the
     * image surface that it's backed by.
     */
    info->image = image;

    info->imageBounds = CGRectMake (0, 0, extents.width, extents.height);

    pbounds.origin.x = 0;
    pbounds.origin.y = 0;

    if (spattern->base.extend == CAIRO_EXTEND_REFLECT) {
	pbounds.size.width = 2.0 * extents.width;
	pbounds.size.height = 2.0 * extents.height;
	info->do_reflect = TRUE;
    } else {
	pbounds.size.width = extents.width;
	pbounds.size.height = extents.height;
    }
    rw = pbounds.size.width;
    rh = pbounds.size.height;

    m = spattern->base.matrix;
    cairo_matrix_invert(&m);
    _cairo_quartz_cairo_matrix_to_quartz (&m, &stransform);

    /* The pattern matrix is relative to the bottom left, again; the
     * incoming cairo pattern matrix is relative to the upper left.
     * So we take the pattern matrix and the original context matrix,
     * which gives us the correct base translation/y flip.
     */
    ptransform = CGAffineTransformConcat(stransform, dest->cgContextBaseCTM);

#ifdef QUARTZ_DEBUG
    ND((stderr, "  pbounds: %f %f %f %f\n", pbounds.origin.x, pbounds.origin.y, pbounds.size.width, pbounds.size.height));
    ND((stderr, "  pattern xform: t: %f %f xx: %f xy: %f yx: %f yy: %f\n", ptransform.tx, ptransform.ty, ptransform.a, ptransform.b, ptransform.c, ptransform.d));
    CGAffineTransform xform = CGContextGetCTM(dest->cgContext);
    ND((stderr, "  context xform: t: %f %f xx: %f xy: %f yx: %f yy: %f\n", xform.tx, xform.ty, xform.a, xform.b, xform.c, xform.d));
#endif

    *cgpat = CGPatternCreate (info,
			      pbounds,
			      ptransform,
			      rw, rh,
			      kCGPatternTilingConstantSpacing, /* kCGPatternTilingNoDistortion, */
			      TRUE,
			      &cb);

    return CAIRO_STATUS_SUCCESS;
}

typedef enum {
    DO_SOLID,
    DO_SHADING,
    DO_PATTERN,
    DO_IMAGE,
    DO_UNSUPPORTED,
    DO_NOTHING,
    DO_TILED_IMAGE
} cairo_quartz_action_t;

static cairo_quartz_action_t
_cairo_quartz_setup_fallback_source (cairo_quartz_surface_t *surface,
				     cairo_pattern_t *source)
{
    CGRect clipBox = CGContextGetClipBoundingBox (surface->cgContext);
    CGAffineTransform ctm;
    double x0, y0, w, h;

    cairo_surface_t *fallback;
    cairo_t *fallback_cr;
    CGImageRef img;

    cairo_status_t status;

    if (clipBox.size.width == 0.0f ||
	clipBox.size.height == 0.0f)
	return DO_NOTHING;

    // the clipBox is in userspace, so:
    ctm = CGContextGetCTM (surface->cgContext);
    ctm = CGAffineTransformInvert (ctm);
    clipBox = CGRectApplyAffineTransform (clipBox, ctm);

    // get the Y flip right -- the CTM will always have a Y flip in place
    clipBox.origin.y = surface->extents.height - (clipBox.origin.y + clipBox.size.height);

    x0 = floor(clipBox.origin.x);
    y0 = floor(clipBox.origin.y);
    w = ceil(clipBox.origin.x + clipBox.size.width) - x0;
    h = ceil(clipBox.origin.y + clipBox.size.height) - y0;

    /* Create a temporary the size of the clip surface, and position
     * it so that the device origin coincides with the original surface */
    fallback = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, (int) w, (int) h);
    cairo_surface_set_device_offset (fallback, -x0, -y0);

    /* Paint the source onto our temporary */
    fallback_cr = cairo_create (fallback);
    cairo_set_operator (fallback_cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source (fallback_cr, source);
    cairo_paint (fallback_cr);
    cairo_destroy (fallback_cr);

    status = _cairo_surface_to_cgimage ((cairo_surface_t*) surface, fallback, &img);
    if (status == CAIRO_STATUS_SUCCESS && img == NULL)
	return DO_NOTHING;
    if (status)
	return DO_UNSUPPORTED;

    surface->sourceImageRect = CGRectMake (0.0, 0.0, w, h);
    surface->sourceImage = img;
    surface->sourceImageSurface = fallback;
    surface->sourceTransform = CGAffineTransformMakeTranslation (x0, y0);

    return DO_IMAGE;
}

static cairo_quartz_action_t
_cairo_quartz_setup_linear_source (cairo_quartz_surface_t *surface,
				   cairo_linear_pattern_t *lpat)
{
    cairo_pattern_t *abspat = (cairo_pattern_t *) lpat;
    cairo_matrix_t mat;
    CGPoint start, end;
    CGFunctionRef gradFunc;
    CGColorSpaceRef rgb;
    bool extend = abspat->extend == CAIRO_EXTEND_PAD;

    if (lpat->base.n_stops == 0) {
	CGContextSetRGBStrokeColor (surface->cgContext, 0., 0., 0., 0.);
	CGContextSetRGBFillColor (surface->cgContext, 0., 0., 0., 0.);
	return DO_SOLID;
    }

    cairo_pattern_get_matrix (abspat, &mat);
    cairo_matrix_invert (&mat);
    _cairo_quartz_cairo_matrix_to_quartz (&mat, &surface->sourceTransform);

    rgb = CGColorSpaceCreateDeviceRGB();

    start = CGPointMake (_cairo_fixed_to_double (lpat->p1.x),
			 _cairo_fixed_to_double (lpat->p1.y));
    end = CGPointMake (_cairo_fixed_to_double (lpat->p2.x),
		       _cairo_fixed_to_double (lpat->p2.y));

    // ref will be released by the CGShading's destructor
    cairo_pattern_reference ((cairo_pattern_t*) lpat);

    if (abspat->extend == CAIRO_EXTEND_NONE ||
	abspat->extend == CAIRO_EXTEND_PAD)
    {
	gradFunc = CreateGradientFunction ((cairo_gradient_pattern_t*) lpat);
    } else {
	gradFunc = CreateRepeatingGradientFunction (surface,
						    (cairo_gradient_pattern_t*) lpat,
						    &start, &end, surface->sourceTransform);
    }

    surface->sourceShading = CGShadingCreateAxial (rgb,
						   start, end,
						   gradFunc,
						   extend, extend);

    CGColorSpaceRelease(rgb);
    CGFunctionRelease(gradFunc);

    return DO_SHADING;
}

static cairo_quartz_action_t
_cairo_quartz_setup_radial_source (cairo_quartz_surface_t *surface,
				   cairo_radial_pattern_t *rpat)
{
    cairo_pattern_t *abspat = (cairo_pattern_t *)rpat;
    cairo_matrix_t mat;
    CGPoint start, end;
    CGFunctionRef gradFunc;
    CGColorSpaceRef rgb;
    bool extend = abspat->extend == CAIRO_EXTEND_PAD;

    if (rpat->base.n_stops == 0) {
	CGContextSetRGBStrokeColor (surface->cgContext, 0., 0., 0., 0.);
	CGContextSetRGBFillColor (surface->cgContext, 0., 0., 0., 0.);
	return DO_SOLID;
    }

    if (abspat->extend == CAIRO_EXTEND_REPEAT ||
	abspat->extend == CAIRO_EXTEND_REFLECT)
    {
	/* I started trying to map these to Quartz, but it's much harder
	 * then the linear case (I think it would involve doing multiple
	 * Radial shadings).  So, instead, let's just render an image
	 * for pixman to draw the shading into, and use that.
	 */
	return _cairo_quartz_setup_fallback_source (surface, (cairo_pattern_t*) rpat);
    }

    cairo_pattern_get_matrix (abspat, &mat);
    cairo_matrix_invert (&mat);
    _cairo_quartz_cairo_matrix_to_quartz (&mat, &surface->sourceTransform);

    rgb = CGColorSpaceCreateDeviceRGB();

    start = CGPointMake (_cairo_fixed_to_double (rpat->c1.x),
			 _cairo_fixed_to_double (rpat->c1.y));
    end = CGPointMake (_cairo_fixed_to_double (rpat->c2.x),
		       _cairo_fixed_to_double (rpat->c2.y));

    // ref will be released by the CGShading's destructor
    cairo_pattern_reference ((cairo_pattern_t*) rpat);

    gradFunc = CreateGradientFunction ((cairo_gradient_pattern_t*) rpat);

    surface->sourceShading = CGShadingCreateRadial (rgb,
						    start,
						    _cairo_fixed_to_double (rpat->r1),
						    end,
						    _cairo_fixed_to_double (rpat->r2),
						    gradFunc,
						    extend, extend);

    CGColorSpaceRelease(rgb);
    CGFunctionRelease(gradFunc);

    return DO_SHADING;
}

static cairo_quartz_action_t
_cairo_quartz_setup_source (cairo_quartz_surface_t *surface,
			    cairo_pattern_t *source)
{
    assert (!(surface->sourceImage || surface->sourceShading || surface->sourcePattern));

    surface->oldInterpolationQuality = CGContextGetInterpolationQuality (surface->cgContext);
    CGContextSetInterpolationQuality (surface->cgContext, _cairo_quartz_filter_to_quartz (source->filter));

    if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) source;

	CGContextSetRGBStrokeColor (surface->cgContext,
				    solid->color.red,
				    solid->color.green,
				    solid->color.blue,
				    solid->color.alpha);
	CGContextSetRGBFillColor (surface->cgContext,
				  solid->color.red,
				  solid->color.green,
				  solid->color.blue,
				  solid->color.alpha);

	return DO_SOLID;
    }

    if (source->type == CAIRO_PATTERN_TYPE_LINEAR) {
	cairo_linear_pattern_t *lpat = (cairo_linear_pattern_t *)source;
	return _cairo_quartz_setup_linear_source (surface, lpat);

    }

    if (source->type == CAIRO_PATTERN_TYPE_RADIAL) {
	cairo_radial_pattern_t *rpat = (cairo_radial_pattern_t *)source;
	return _cairo_quartz_setup_radial_source (surface, rpat);

    }

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE &&
	(source->extend == CAIRO_EXTEND_NONE || (CGContextDrawTiledImagePtr && source->extend == CAIRO_EXTEND_REPEAT)))
    {
	cairo_surface_pattern_t *spat = (cairo_surface_pattern_t *) source;
	cairo_surface_t *pat_surf = spat->surface;
	CGImageRef img;
	cairo_matrix_t m = spat->base.matrix;
	cairo_rectangle_int_t extents;
	cairo_status_t status;
	CGAffineTransform xform;
	CGRect srcRect;
	cairo_fixed_t fw, fh;

	status = _cairo_surface_to_cgimage ((cairo_surface_t *) surface, pat_surf, &img);
	if (status == CAIRO_STATUS_SUCCESS && img == NULL)
	    return DO_NOTHING;
	if (status)
	    return DO_UNSUPPORTED;

	surface->sourceImage = img;

	cairo_matrix_invert(&m);
	_cairo_quartz_cairo_matrix_to_quartz (&m, &surface->sourceTransform);

	status = _cairo_surface_get_extents (pat_surf, &extents);
	if (status)
	    return DO_UNSUPPORTED;

	if (source->extend == CAIRO_EXTEND_NONE) {
	    surface->sourceImageRect = CGRectMake (0, 0, extents.width, extents.height);
	    return DO_IMAGE;
	}

	/* Quartz seems to tile images at pixel-aligned regions only -- this
	 * leads to seams if the image doesn't end up scaling to fill the
	 * space exactly.  The CGPattern tiling approach doesn't have this
	 * problem.  Check if we're going to fill up the space (within some
	 * epsilon), and if not, fall back to the CGPattern type.
	 */

	xform = CGAffineTransformConcat (CGContextGetCTM (surface->cgContext),
					 surface->sourceTransform);

	srcRect = CGRectMake (0, 0, extents.width, extents.height);
	srcRect = CGRectApplyAffineTransform (srcRect, xform);

	fw = _cairo_fixed_from_double (srcRect.size.width);
	fh = _cairo_fixed_from_double (srcRect.size.height);

	if ((fw & CAIRO_FIXED_FRAC_MASK) <= CAIRO_FIXED_EPSILON &&
	    (fh & CAIRO_FIXED_FRAC_MASK) <= CAIRO_FIXED_EPSILON)
	{
	    /* We're good to use DrawTiledImage, but ensure that
	     * the math works out */

	    srcRect.size.width = round(srcRect.size.width);
	    srcRect.size.height = round(srcRect.size.height);

	    xform = CGAffineTransformInvert (xform);

	    srcRect = CGRectApplyAffineTransform (srcRect, xform);

	    surface->sourceImageRect = srcRect;

	    return DO_TILED_IMAGE;
	}

	/* Fall through to generic SURFACE case */
    }

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	float patternAlpha = 1.0f;
	CGColorSpaceRef patternSpace;
	CGPatternRef pattern;
	cairo_int_status_t status;

	status = _cairo_quartz_cairo_repeating_surface_pattern_to_quartz (surface, source, &pattern);
	if (status == CAIRO_INT_STATUS_NOTHING_TO_DO)
	    return DO_NOTHING;
	if (status)
	    return DO_UNSUPPORTED;

	// Save before we change the pattern, colorspace, etc. so that
	// we can restore and make sure that quartz releases our
	// pattern (which may be stack allocated)
	CGContextSaveGState(surface->cgContext);

	patternSpace = CGColorSpaceCreatePattern(NULL);
	CGContextSetFillColorSpace (surface->cgContext, patternSpace);
	CGContextSetFillPattern (surface->cgContext, pattern, &patternAlpha);
	CGContextSetStrokeColorSpace (surface->cgContext, patternSpace);
	CGContextSetStrokePattern (surface->cgContext, pattern, &patternAlpha);
	CGColorSpaceRelease (patternSpace);

	/* Quartz likes to munge the pattern phase (as yet unexplained
	 * why); force it to 0,0 as we've already baked in the correct
	 * pattern translation into the pattern matrix
	 */
	CGContextSetPatternPhase (surface->cgContext, CGSizeMake(0,0));

	surface->sourcePattern = pattern;

	return DO_PATTERN;
    }

    return DO_UNSUPPORTED;
}

static void
_cairo_quartz_teardown_source (cairo_quartz_surface_t *surface,
				cairo_pattern_t *source)
{
    CGContextSetInterpolationQuality (surface->cgContext, surface->oldInterpolationQuality);

    if (surface->sourceImage) {
	CGImageRelease(surface->sourceImage);
	surface->sourceImage = NULL;

	cairo_surface_destroy(surface->sourceImageSurface);
	surface->sourceImageSurface = NULL;
    }

    if (surface->sourceShading) {
	CGShadingRelease(surface->sourceShading);
	surface->sourceShading = NULL;
    }

    if (surface->sourcePattern) {
	CGPatternRelease(surface->sourcePattern);
	// To tear down the pattern and colorspace
	CGContextRestoreGState(surface->cgContext);

	surface->sourcePattern = NULL;
    }
}

/*
 * get source/dest image implementation
 */

/* Read the image from the surface's front buffer */
static cairo_int_status_t
_cairo_quartz_get_image (cairo_quartz_surface_t *surface,
			 cairo_image_surface_t **image_out)
{
    unsigned char *imageData;
    cairo_image_surface_t *isurf;

    if (IS_EMPTY(surface)) {
	*image_out = (cairo_image_surface_t*) cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0);
	return CAIRO_STATUS_SUCCESS;
    }

    if (surface->imageSurfaceEquiv) {
	*image_out = (cairo_image_surface_t*) cairo_surface_reference(surface->imageSurfaceEquiv);
	return CAIRO_STATUS_SUCCESS;
    }

    if (_cairo_quartz_is_cgcontext_bitmap_context(surface->cgContext)) {
	unsigned int stride;
	unsigned int bitinfo;
	unsigned int bpc, bpp;
	CGColorSpaceRef colorspace;
	unsigned int color_comps;

	imageData = (unsigned char *) CGBitmapContextGetData(surface->cgContext);

#ifdef USE_10_3_WORKAROUNDS
	bitinfo = CGBitmapContextGetAlphaInfo (surface->cgContext);
#else
	bitinfo = CGBitmapContextGetBitmapInfo (surface->cgContext);
#endif
	stride = CGBitmapContextGetBytesPerRow (surface->cgContext);
	bpp = CGBitmapContextGetBitsPerPixel (surface->cgContext);
	bpc = CGBitmapContextGetBitsPerComponent (surface->cgContext);

	// let's hope they don't add YUV under us
	colorspace = CGBitmapContextGetColorSpace (surface->cgContext);
	color_comps = CGColorSpaceGetNumberOfComponents(colorspace);

	// XXX TODO: We can handle all of these by converting to
	// pixman masks, including non-native-endian masks
	if (bpc != 8)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (bpp != 32 && bpp != 8)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (color_comps != 3 && color_comps != 1)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (bpp == 32 && color_comps == 3 &&
	    (bitinfo & kCGBitmapAlphaInfoMask) == kCGImageAlphaPremultipliedFirst &&
	    (bitinfo & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Host)
	{
	    isurf = (cairo_image_surface_t *)
		cairo_image_surface_create_for_data (imageData,
						     CAIRO_FORMAT_ARGB32,
						     surface->extents.width,
						     surface->extents.height,
						     stride);
	} else if (bpp == 32 && color_comps == 3 &&
		   (bitinfo & kCGBitmapAlphaInfoMask) == kCGImageAlphaNoneSkipFirst &&
		   (bitinfo & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Host)
	{
	    isurf = (cairo_image_surface_t *)
		cairo_image_surface_create_for_data (imageData,
						     CAIRO_FORMAT_RGB24,
						     surface->extents.width,
						     surface->extents.height,
						     stride);
	} else if (bpp == 8 && color_comps == 1)
	{
	    isurf = (cairo_image_surface_t *)
		cairo_image_surface_create_for_data (imageData,
						     CAIRO_FORMAT_A8,
						     surface->extents.width,
						     surface->extents.height,
						     stride);
	} else {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    } else {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    *image_out = isurf;
    return CAIRO_STATUS_SUCCESS;
}

/*
 * Cairo surface backend implementations
 */

static cairo_status_t
_cairo_quartz_surface_finish (void *abstract_surface)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    ND((stderr, "_cairo_quartz_surface_finish[%p] cgc: %p\n", surface, surface->cgContext));

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    /* Restore our saved gstate that we use to reset clipping */
    CGContextRestoreGState (surface->cgContext);

    CGContextRelease (surface->cgContext);

    surface->cgContext = NULL;

    if (surface->imageSurfaceEquiv) {
	cairo_surface_destroy (surface->imageSurfaceEquiv);
	surface->imageSurfaceEquiv = NULL;
    }

    if (surface->imageData) {
	free (surface->imageData);
	surface->imageData = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_surface_acquire_source_image (void *abstract_surface,
					     cairo_image_surface_t **image_out,
					     void **image_extra)
{
    cairo_int_status_t status;
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    //ND((stderr, "%p _cairo_quartz_surface_acquire_source_image\n", surface));

    status = _cairo_quartz_get_image (surface, image_out);
    if (status)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_quartz_surface_release_source_image (void *abstract_surface,
					     cairo_image_surface_t *image,
					     void *image_extra)
{
    cairo_surface_destroy ((cairo_surface_t *) image);
}


static cairo_status_t
_cairo_quartz_surface_acquire_dest_image (void *abstract_surface,
					  cairo_rectangle_int_t *interest_rect,
					  cairo_image_surface_t **image_out,
					  cairo_rectangle_int_t *image_rect,
					  void **image_extra)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t status;

    ND((stderr, "%p _cairo_quartz_surface_acquire_dest_image\n", surface));

    status = _cairo_quartz_get_image (surface, image_out);
    if (status)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    *image_rect = surface->extents;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_quartz_surface_release_dest_image (void *abstract_surface,
					  cairo_rectangle_int_t *interest_rect,
					  cairo_image_surface_t *image,
					  cairo_rectangle_int_t *image_rect,
					  void *image_extra)
{
    //cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    //ND((stderr, "%p _cairo_quartz_surface_release_dest_image\n", surface));

    cairo_surface_destroy ((cairo_surface_t *) image);
}

static cairo_surface_t *
_cairo_quartz_surface_create_similar (void *abstract_surface,
				       cairo_content_t content,
				       int width,
				       int height)
{
    /*cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;*/

    cairo_format_t format;

    if (content == CAIRO_CONTENT_COLOR_ALPHA)
	format = CAIRO_FORMAT_ARGB32;
    else if (content == CAIRO_CONTENT_COLOR)
	format = CAIRO_FORMAT_RGB24;
    else if (content == CAIRO_CONTENT_ALPHA)
	format = CAIRO_FORMAT_A8;
    else
	return NULL;

    // verify width and height of surface
    if (!_cairo_quartz_verify_surface_size(width, height)) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    return cairo_quartz_surface_create (format, width, height);
}

static cairo_status_t
_cairo_quartz_surface_clone_similar (void *abstract_surface,
				     cairo_surface_t *src,
				     int              src_x,
				     int              src_y,
				     int              width,
				     int              height,
				     cairo_surface_t **clone_out)
{
    cairo_quartz_surface_t *new_surface = NULL;
    cairo_format_t new_format;
    CGImageRef quartz_image = NULL;
    cairo_status_t status;

    *clone_out = NULL;

    // verify width and height of surface
    if (!_cairo_quartz_verify_surface_size(width, height)) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (width == 0 || height == 0) {
	*clone_out = (cairo_surface_t*)
	    _cairo_quartz_surface_create_internal (NULL, CAIRO_CONTENT_COLOR_ALPHA,
						   width, height);
	return CAIRO_STATUS_SUCCESS;
    }

    if (src->backend->type == CAIRO_SURFACE_TYPE_QUARTZ) {
	cairo_quartz_surface_t *qsurf = (cairo_quartz_surface_t *) src;

	if (IS_EMPTY(qsurf)) {
	    *clone_out = (cairo_surface_t*)
		_cairo_quartz_surface_create_internal (NULL, CAIRO_CONTENT_COLOR_ALPHA,
						       qsurf->extents.width, qsurf->extents.height);
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    status = _cairo_surface_to_cgimage ((cairo_surface_t*) abstract_surface, src, &quartz_image);
    if (status)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    new_format = CAIRO_FORMAT_ARGB32;  /* assumed */
    if (_cairo_surface_is_image (src)) {
	new_format = ((cairo_image_surface_t *) src)->format;
    }

    new_surface = (cairo_quartz_surface_t *)
	cairo_quartz_surface_create (new_format, width, height);

    if (quartz_image == NULL)
	goto FINISH;

    if (!new_surface || new_surface->base.status) {
	CGImageRelease (quartz_image);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    CGContextSaveGState (new_surface->cgContext);

    CGContextSetCompositeOperation (new_surface->cgContext,
				    kPrivateCGCompositeCopy);

    CGContextTranslateCTM (new_surface->cgContext, -src_x, -src_y);
    CGContextDrawImage (new_surface->cgContext,
			CGRectMake (0, 0, CGImageGetWidth(quartz_image), CGImageGetHeight(quartz_image)),
			quartz_image);

    CGContextRestoreGState (new_surface->cgContext);

    CGImageRelease (quartz_image);

FINISH:    
    *clone_out = (cairo_surface_t*) new_surface;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartz_surface_get_extents (void *abstract_surface,
				   cairo_rectangle_int_t *extents)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    *extents = surface->extents;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartz_surface_paint (void *abstract_surface,
			     cairo_operator_t op,
			     cairo_pattern_t *source)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartz_action_t action;

    ND((stderr, "%p _cairo_quartz_surface_paint op %d source->type %d\n", surface, op, source->type));

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    CGContextSetCompositeOperation (surface->cgContext, _cairo_quartz_cairo_operator_to_quartz (op));

    action = _cairo_quartz_setup_source (surface, source);

    if (action == DO_SOLID || action == DO_PATTERN) {
	CGContextFillRect (surface->cgContext, CGRectMake(surface->extents.x,
							  surface->extents.y,
							  surface->extents.width,
							  surface->extents.height));
    } else if (action == DO_SHADING) {
	CGContextConcatCTM (surface->cgContext, surface->sourceTransform);
	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    } else if (action == DO_IMAGE || action == DO_TILED_IMAGE) {
	CGContextSaveGState (surface->cgContext);

	CGContextConcatCTM (surface->cgContext, surface->sourceTransform);
	CGContextTranslateCTM (surface->cgContext, 0, surface->sourceImageRect.size.height);
	CGContextScaleCTM (surface->cgContext, 1, -1);

	if (action == DO_IMAGE)
	    CGContextDrawImage (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
	else
	    CGContextDrawTiledImagePtr (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
	CGContextRestoreGState (surface->cgContext);
    } else if (action != DO_NOTHING) {
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
    }

    _cairo_quartz_teardown_source (surface, source);

    ND((stderr, "-- paint\n"));
    return rv;
}

static cairo_int_status_t
_cairo_quartz_surface_fill (void *abstract_surface,
			     cairo_operator_t op,
			     cairo_pattern_t *source,
			     cairo_path_fixed_t *path,
			     cairo_fill_rule_t fill_rule,
			     double tolerance,
			     cairo_antialias_t antialias)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartz_action_t action;
    quartz_stroke_t stroke;
    cairo_box_t box;
    CGPathRef path_for_unbounded = NULL;

    ND((stderr, "%p _cairo_quartz_surface_fill op %d source->type %d\n", surface, op, source->type));

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    /* Check whether the path would be a no-op */
    /* XXX handle unbounded ops */
    if (_cairo_path_fixed_is_empty(path) ||
	(_cairo_path_fixed_is_box(path, &box) &&
	 box.p1.x == box.p2.x &&
	 box.p1.y == box.p2.y))
    {
	return CAIRO_STATUS_SUCCESS;
    }

    CGContextSaveGState (surface->cgContext);

    CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));
    CGContextSetCompositeOperation (surface->cgContext, _cairo_quartz_cairo_operator_to_quartz (op));

    action = _cairo_quartz_setup_source (surface, source);

    CGContextBeginPath (surface->cgContext);

    stroke.cgContext = surface->cgContext;
    stroke.ctm_inverse = NULL;
    rv = _cairo_quartz_cairo_path_to_quartz_context (path, &stroke);
    if (rv)
        goto BAIL;

    if (!_cairo_operator_bounded_by_mask(op) && CGContextCopyPathPtr)
	path_for_unbounded = CGContextCopyPathPtr (surface->cgContext);

    if (action == DO_SOLID || action == DO_PATTERN) {
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextFillPath (surface->cgContext);
	else
	    CGContextEOFillPath (surface->cgContext);
    } else if (action == DO_SHADING) {

	// we have to clip and then paint the shading; we can't fill
	// with the shading
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextClip (surface->cgContext);
	else
	    CGContextEOClip (surface->cgContext);

	CGContextConcatCTM (surface->cgContext, surface->sourceTransform);
	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    } else if (action == DO_IMAGE || action == DO_TILED_IMAGE) {
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextClip (surface->cgContext);
	else
	    CGContextEOClip (surface->cgContext);

	CGContextConcatCTM (surface->cgContext, surface->sourceTransform);
	CGContextTranslateCTM (surface->cgContext, 0, surface->sourceImageRect.size.height);
	CGContextScaleCTM (surface->cgContext, 1, -1);

	if (action == DO_IMAGE)
	    CGContextDrawImage (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
	else
	    CGContextDrawTiledImagePtr (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
    } else if (action != DO_NOTHING) {
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
    }

  BAIL:
    _cairo_quartz_teardown_source (surface, source);

    CGContextRestoreGState (surface->cgContext);

    if (path_for_unbounded) {
	unbounded_op_data_t ub;
	ub.op = UNBOUNDED_STROKE_FILL;
	ub.u.stroke_fill.cgPath = path_for_unbounded;
	ub.u.stroke_fill.fill_rule = fill_rule;

	_cairo_quartz_fixup_unbounded_operation (surface, &ub, antialias);
	CGPathRelease (path_for_unbounded);
    }

    ND((stderr, "-- fill\n"));
    return rv;
}

static cairo_int_status_t
_cairo_quartz_surface_stroke (void *abstract_surface,
			      cairo_operator_t op,
			      cairo_pattern_t *source,
			      cairo_path_fixed_t *path,
			      cairo_stroke_style_t *style,
			      cairo_matrix_t *ctm,
			      cairo_matrix_t *ctm_inverse,
			      double tolerance,
			      cairo_antialias_t antialias)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartz_action_t action;
    quartz_stroke_t stroke;
    CGAffineTransform origCTM, strokeTransform;
    CGPathRef path_for_unbounded = NULL;

    ND((stderr, "%p _cairo_quartz_surface_stroke op %d source->type %d\n", surface, op, source->type));

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    CGContextSaveGState (surface->cgContext);

    // Turning antialiasing off used to cause misrendering with
    // single-pixel lines (e.g. 20,10.5 -> 21,10.5 end up being rendered as 2 pixels).
    // That's been since fixed in at least 10.5, and in the latest 10.4 dot releases.
    CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));
    CGContextSetLineWidth (surface->cgContext, style->line_width);
    CGContextSetLineCap (surface->cgContext, _cairo_quartz_cairo_line_cap_to_quartz (style->line_cap));
    CGContextSetLineJoin (surface->cgContext, _cairo_quartz_cairo_line_join_to_quartz (style->line_join));
    CGContextSetMiterLimit (surface->cgContext, style->miter_limit);

    origCTM = CGContextGetCTM (surface->cgContext);

    _cairo_quartz_cairo_matrix_to_quartz (ctm, &strokeTransform);
    CGContextConcatCTM (surface->cgContext, strokeTransform);

    if (style->dash && style->num_dashes) {
#define STATIC_DASH 32
	float sdash[STATIC_DASH];
	float *fdash = sdash;
	unsigned int max_dashes = style->num_dashes;
	unsigned int k;

	if (style->num_dashes%2)
	    max_dashes *= 2;
	if (max_dashes > STATIC_DASH)
	    fdash = _cairo_malloc_ab (max_dashes, sizeof (float));
	if (fdash == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	for (k = 0; k < max_dashes; k++)
	    fdash[k] = (float) style->dash[k % style->num_dashes];

	CGContextSetLineDash (surface->cgContext, style->dash_offset, fdash, max_dashes);
	if (fdash != sdash)
	    free (fdash);
    }

    CGContextSetCompositeOperation (surface->cgContext, _cairo_quartz_cairo_operator_to_quartz (op));

    action = _cairo_quartz_setup_source (surface, source);

    CGContextBeginPath (surface->cgContext);

    stroke.cgContext = surface->cgContext;
    stroke.ctm_inverse = ctm_inverse;
    rv = _cairo_quartz_cairo_path_to_quartz_context (path, &stroke);
    if (rv)
	goto BAIL;

    if (!_cairo_operator_bounded_by_mask (op) && CGContextCopyPathPtr)
	path_for_unbounded = CGContextCopyPathPtr (surface->cgContext);

    if (action == DO_SOLID || action == DO_PATTERN) {
	CGContextStrokePath (surface->cgContext);
    } else if (action == DO_IMAGE || action == DO_TILED_IMAGE) {
	CGContextReplacePathWithStrokedPath (surface->cgContext);
	CGContextClip (surface->cgContext);

	CGContextSetCTM (surface->cgContext, origCTM);

	CGContextConcatCTM (surface->cgContext, surface->sourceTransform);
	CGContextTranslateCTM (surface->cgContext, 0, surface->sourceImageRect.size.height);
	CGContextScaleCTM (surface->cgContext, 1, -1);

	if (action == DO_IMAGE)
	    CGContextDrawImage (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
	else
	    CGContextDrawTiledImagePtr (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
    } else if (action == DO_SHADING) {
	CGContextReplacePathWithStrokedPath (surface->cgContext);
	CGContextClip (surface->cgContext);

	CGContextSetCTM (surface->cgContext, origCTM);

	CGContextConcatCTM (surface->cgContext, surface->sourceTransform);
	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    } else if (action != DO_NOTHING) {
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
    }

  BAIL:
    _cairo_quartz_teardown_source (surface, source);

    CGContextRestoreGState (surface->cgContext);

    if (path_for_unbounded) {
	unbounded_op_data_t ub;

	CGContextBeginPath (surface->cgContext);

	/* recreate the stroke state, but without the CTM, as it's been already baked
	 * into the path.
	 */
	CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));
	CGContextSetLineWidth (surface->cgContext, style->line_width);
	CGContextSetLineCap (surface->cgContext, _cairo_quartz_cairo_line_cap_to_quartz (style->line_cap));
	CGContextSetLineJoin (surface->cgContext, _cairo_quartz_cairo_line_join_to_quartz (style->line_join));
	CGContextSetMiterLimit (surface->cgContext, style->miter_limit);

	CGContextAddPath (surface->cgContext, path_for_unbounded);
	CGPathRelease (path_for_unbounded);

	CGContextReplacePathWithStrokedPath (surface->cgContext);
	path_for_unbounded = CGContextCopyPathPtr (surface->cgContext);

	ub.op = UNBOUNDED_STROKE_FILL;
	ub.u.stroke_fill.cgPath = path_for_unbounded;
	ub.u.stroke_fill.fill_rule = CAIRO_FILL_RULE_WINDING;

	_cairo_quartz_fixup_unbounded_operation (surface, &ub, antialias);

	CGPathRelease (path_for_unbounded);
    }

    ND((stderr, "-- stroke\n"));
    return rv;
}

#if CAIRO_HAS_QUARTZ_FONT
static cairo_int_status_t
_cairo_quartz_surface_show_glyphs (void *abstract_surface,
				   cairo_operator_t op,
				   cairo_pattern_t *source,
				   cairo_glyph_t *glyphs,
				   int num_glyphs,
				   cairo_scaled_font_t *scaled_font)
{
    CGAffineTransform textTransform, ctm;
#define STATIC_BUF_SIZE 64
    CGGlyph glyphs_static[STATIC_BUF_SIZE];
    CGSize cg_advances_static[STATIC_BUF_SIZE];
    CGGlyph *cg_glyphs = &glyphs_static[0];
    CGSize *cg_advances = &cg_advances_static[0];

    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartz_action_t action;
    float xprev, yprev;
    int i;
    CGFontRef cgfref = NULL;

    cairo_bool_t isClipping = FALSE;
    cairo_bool_t didForceFontSmoothing = FALSE;

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    if (num_glyphs <= 0)
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    if (cairo_scaled_font_get_type (scaled_font) != CAIRO_FONT_TYPE_QUARTZ)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    CGContextSaveGState (surface->cgContext);

    action = _cairo_quartz_setup_source (surface, source);
    if (action == DO_SOLID || action == DO_PATTERN) {
	CGContextSetTextDrawingMode (surface->cgContext, kCGTextFill);
    } else if (action == DO_IMAGE || action == DO_TILED_IMAGE || action == DO_SHADING) {
	CGContextSetTextDrawingMode (surface->cgContext, kCGTextClip);
	isClipping = TRUE;
    } else {
	if (action != DO_NOTHING)
	    rv = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL;
    }

    CGContextSetCompositeOperation (surface->cgContext, _cairo_quartz_cairo_operator_to_quartz (op));

    /* this doesn't addref */
    cgfref = _cairo_quartz_scaled_font_get_cg_font_ref (scaled_font);
    CGContextSetFont (surface->cgContext, cgfref);
    CGContextSetFontSize (surface->cgContext, 1.0);

    switch (scaled_font->options.antialias) {
	case CAIRO_ANTIALIAS_SUBPIXEL:
	    CGContextSetShouldAntialias (surface->cgContext, TRUE);
	    CGContextSetShouldSmoothFonts (surface->cgContext, TRUE);
	    if (CGContextSetAllowsFontSmoothingPtr &&
		!CGContextGetAllowsFontSmoothingPtr (surface->cgContext))
	    {
		didForceFontSmoothing = TRUE;
		CGContextSetAllowsFontSmoothingPtr (surface->cgContext, TRUE);
	    }
	    break;
	case CAIRO_ANTIALIAS_NONE:
	    CGContextSetShouldAntialias (surface->cgContext, FALSE);
	    break;
	case CAIRO_ANTIALIAS_GRAY:
	    CGContextSetShouldAntialias (surface->cgContext, TRUE);
	    CGContextSetShouldSmoothFonts (surface->cgContext, FALSE);
	    break;
	case CAIRO_ANTIALIAS_DEFAULT:
	    /* Don't do anything */
	    break;
    }

    if (num_glyphs > STATIC_BUF_SIZE) {
	cg_glyphs = (CGGlyph*) _cairo_malloc_ab (num_glyphs, sizeof(CGGlyph));
	if (cg_glyphs == NULL) {
	    rv = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto BAIL;
	}

	cg_advances = (CGSize*) _cairo_malloc_ab (num_glyphs, sizeof(CGSize));
	if (cg_advances == NULL) {
	    rv = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto BAIL;
	}
    }

    textTransform = CGAffineTransformMake (scaled_font->font_matrix.xx,
					   scaled_font->font_matrix.yx,
					   scaled_font->font_matrix.xy,
					   scaled_font->font_matrix.yy,
					   0., 0.);
    textTransform = CGAffineTransformScale (textTransform, 1.0, -1.0);
    textTransform = CGAffineTransformConcat (CGAffineTransformMake(scaled_font->ctm.xx,
								   -scaled_font->ctm.yx,
								   -scaled_font->ctm.xy,
								   scaled_font->ctm.yy,
								   0., 0.),
					     textTransform);

    CGContextSetTextMatrix (surface->cgContext, textTransform);

    /* Convert our glyph positions to glyph advances.  We need n-1 advances,
     * since the advance at index 0 is applied after glyph 0. */
    xprev = glyphs[0].x;
    yprev = glyphs[0].y;

    cg_glyphs[0] = glyphs[0].index;

    for (i = 1; i < num_glyphs; i++) {
	float xf = glyphs[i].x;
	float yf = glyphs[i].y;
	cg_glyphs[i] = glyphs[i].index;
	cg_advances[i-1].width = xf - xprev;
	cg_advances[i-1].height = yf - yprev;
	xprev = xf;
	yprev = yf;
    }

    if (_cairo_quartz_osx_version >= 0x1050 && isClipping) {
	/* If we're clipping, OSX 10.5 (at least as of 10.5.2) has a
	 * bug (apple bug ID #5834794) where the glyph
	 * advances/positions are not transformed by the text matrix
	 * if kCGTextClip is being used.  So, we pre-transform here.
	 * 10.4 does not have this problem (as of 10.4.11).
	 */
	for (i = 0; i < num_glyphs - 1; i++)
	    cg_advances[i] = CGSizeApplyAffineTransform(cg_advances[i], textTransform);
    }

#if 0
    for (i = 0; i < num_glyphs; i++) {
	ND((stderr, "[%d: %d %f,%f]\n", i, cg_glyphs[i], cg_advances[i].width, cg_advances[i].height));
    }
#endif

    /* Translate to the first glyph's position before drawing */
    ctm = CGContextGetCTM (surface->cgContext);
    CGContextTranslateCTM (surface->cgContext, glyphs[0].x, glyphs[0].y);

    CGContextShowGlyphsWithAdvances (surface->cgContext,
				     cg_glyphs,
				     cg_advances,
				     num_glyphs);

    CGContextSetCTM (surface->cgContext, ctm);

    if (action == DO_IMAGE || action == DO_TILED_IMAGE) {
	CGContextConcatCTM (surface->cgContext, surface->sourceTransform);
	CGContextTranslateCTM (surface->cgContext, 0, surface->sourceImageRect.size.height);
	CGContextScaleCTM (surface->cgContext, 1, -1);

	if (action == DO_IMAGE)
	    CGContextDrawImage (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
	else
	    CGContextDrawTiledImagePtr (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
    } else if (action == DO_SHADING) {
	CGContextConcatCTM (surface->cgContext, surface->sourceTransform);
	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    }

BAIL:
    _cairo_quartz_teardown_source (surface, source);

    if (didForceFontSmoothing)
	CGContextSetAllowsFontSmoothingPtr (surface->cgContext, FALSE);

    CGContextRestoreGState (surface->cgContext);

    if (rv == CAIRO_STATUS_SUCCESS &&
	cgfref &&
	!_cairo_operator_bounded_by_mask (op))
    {
	unbounded_op_data_t ub;
	ub.op = UNBOUNDED_SHOW_GLYPHS;

	ub.u.show_glyphs.isClipping = isClipping;
	ub.u.show_glyphs.cg_glyphs = cg_glyphs;
	ub.u.show_glyphs.cg_advances = cg_advances;
	ub.u.show_glyphs.nglyphs = num_glyphs;
	ub.u.show_glyphs.textTransform = textTransform;
	ub.u.show_glyphs.font = cgfref;
	ub.u.show_glyphs.origin = CGPointMake (glyphs[0].x, glyphs[0].y);

	_cairo_quartz_fixup_unbounded_operation (surface, &ub, scaled_font->options.antialias);
    }


    if (cg_advances != &cg_advances_static[0]) {
	free (cg_advances);
    }

    if (cg_glyphs != &glyphs_static[0]) {
	free (cg_glyphs);
    }

    return rv;
}
#endif /* CAIRO_HAS_QUARTZ_FONT */

static cairo_int_status_t
_cairo_quartz_surface_mask_with_surface (cairo_quartz_surface_t *surface,
                                         cairo_operator_t op,
                                         cairo_pattern_t *source,
                                         cairo_surface_pattern_t *mask)
{
    cairo_rectangle_int_t extents;
    CGRect rect;
    CGImageRef img;
    cairo_surface_t *pat_surf = mask->surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    CGAffineTransform ctm, mask_matrix;

    status = _cairo_surface_get_extents (pat_surf, &extents);
    if (status)
	return status;

    // everything would be masked out, so do nothing
    if (extents.width == 0 || extents.height == 0)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_surface_to_cgimage ((cairo_surface_t *) surface, pat_surf, &img);
    if (status == CAIRO_STATUS_SUCCESS && img == NULL)
	return CAIRO_STATUS_SUCCESS;
    if (status)
	return status;

    rect = CGRectMake (0.0f, 0.0f, extents.width, extents.height);

    CGContextSaveGState (surface->cgContext);

    /* ClipToMask is essentially drawing an image, so we need to flip the CTM
     * to get the image to appear oriented the right way */
    ctm = CGContextGetCTM (surface->cgContext);

    _cairo_quartz_cairo_matrix_to_quartz (&mask->base.matrix, &mask_matrix);
    CGContextConcatCTM (surface->cgContext, CGAffineTransformInvert(mask_matrix));
    CGContextTranslateCTM (surface->cgContext, 0.0f, rect.size.height);
    CGContextScaleCTM (surface->cgContext, 1.0f, -1.0f);

    CGContextClipToMaskPtr (surface->cgContext, rect, img);

    CGContextSetCTM (surface->cgContext, ctm);

    status = _cairo_quartz_surface_paint (surface, op, source);

    CGContextRestoreGState (surface->cgContext);

    if (!_cairo_operator_bounded_by_mask (op)) {
	unbounded_op_data_t ub;
	ub.op = UNBOUNDED_MASK;
	ub.u.mask.mask = img;
	ub.u.mask.maskTransform = CGAffineTransformInvert(mask_matrix);
	_cairo_quartz_fixup_unbounded_operation (surface, &ub, CAIRO_ANTIALIAS_NONE);
    }

    CGImageRelease (img);

    return status;
}

/* This is somewhat less than ideal, but it gets the job done;
 * it would be better to avoid calling back into cairo.  This
 * creates a temporary surface to use as the mask.
 */
static cairo_int_status_t
_cairo_quartz_surface_mask_with_generic (cairo_quartz_surface_t *surface,
					 cairo_operator_t op,
					 cairo_pattern_t *source,
					 cairo_pattern_t *mask)
{
    int width = surface->extents.width - surface->extents.x;
    int height = surface->extents.height - surface->extents.y;

    cairo_surface_t *gradient_surf = NULL;
    cairo_t *gradient_surf_cr = NULL;

    cairo_pattern_union_t surface_pattern;
    cairo_int_status_t status;

    /* Render the gradient to a surface */
    gradient_surf = cairo_quartz_surface_create (CAIRO_FORMAT_ARGB32,
						 width,
						 height);
    gradient_surf_cr = cairo_create(gradient_surf);
    cairo_set_source (gradient_surf_cr, mask);
    cairo_set_operator (gradient_surf_cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (gradient_surf_cr);
    status = cairo_status (gradient_surf_cr);
    cairo_destroy (gradient_surf_cr);

    if (status)
	goto BAIL;

    _cairo_pattern_init_for_surface (&surface_pattern.surface, gradient_surf);

    status = _cairo_quartz_surface_mask_with_surface (surface, op, source, &surface_pattern.surface);

    _cairo_pattern_fini (&surface_pattern.base);

  BAIL:
    if (gradient_surf)
	cairo_surface_destroy (gradient_surf);

    return status;
}

static cairo_int_status_t
_cairo_quartz_surface_mask (void *abstract_surface,
			    cairo_operator_t op,
			    cairo_pattern_t *source,
			    cairo_pattern_t *mask)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;

    ND((stderr, "%p _cairo_quartz_surface_mask op %d source->type %d mask->type %d\n", surface, op, source->type, mask->type));

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	/* This is easy; we just need to paint with the alpha. */
	cairo_solid_pattern_t *solid_mask = (cairo_solid_pattern_t *) mask;

	CGContextSetAlpha (surface->cgContext, solid_mask->color.alpha);
	rv = _cairo_quartz_surface_paint (surface, op, source);
	CGContextSetAlpha (surface->cgContext, 1.0);

	return rv;
    }

    /* If we have CGContextClipToMask, we can do more complex masks */
    if (CGContextClipToMaskPtr) {
	/* For these, we can skip creating a temporary surface, since we already have one */
	if (mask->type == CAIRO_PATTERN_TYPE_SURFACE && mask->extend == CAIRO_EXTEND_NONE)
	    return _cairo_quartz_surface_mask_with_surface (surface, op, source, (cairo_surface_pattern_t *) mask);

	return _cairo_quartz_surface_mask_with_generic (surface, op, source, mask);
    }

    /* So, CGContextClipToMask is not present in 10.3.9, so we're
     * doomed; if we have imageData, we can do fallback, otherwise
     * just pretend success.
     */
    if (surface->imageData)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartz_surface_intersect_clip_path (void *abstract_surface,
					    cairo_path_fixed_t *path,
					    cairo_fill_rule_t fill_rule,
					    double tolerance,
					    cairo_antialias_t antialias)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    quartz_stroke_t stroke;
    cairo_status_t status;

    ND((stderr, "%p _cairo_quartz_surface_intersect_clip_path path: %p\n", surface, path));

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    if (path == NULL) {
	/* If we're being asked to reset the clip, we can only do it
	 * by restoring the gstate to our previous saved one, and
	 * saving it again.
	 *
	 * Note that this assumes that ALL quartz surface creation
	 * functions will do a SaveGState first; we do this in create_internal.
	 */
	CGContextRestoreGState (surface->cgContext);
	CGContextSaveGState (surface->cgContext);
    } else {
	CGContextBeginPath (surface->cgContext);
	stroke.cgContext = surface->cgContext;
	stroke.ctm_inverse = NULL;

	CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));

	/* path must not be empty. */
	CGContextMoveToPoint (surface->cgContext, 0, 0);
	status = _cairo_quartz_cairo_path_to_quartz_context (path, &stroke);
	if (status)
	    return status;

	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextClip (surface->cgContext);
	else
	    CGContextEOClip (surface->cgContext);
    }

    ND((stderr, "-- intersect_clip_path\n"));

    return CAIRO_STATUS_SUCCESS;
}

// XXXtodo implement show_page; need to figure out how to handle begin/end

static const struct _cairo_surface_backend cairo_quartz_surface_backend = {
    CAIRO_SURFACE_TYPE_QUARTZ,
    _cairo_quartz_surface_create_similar,
    _cairo_quartz_surface_finish,
    _cairo_quartz_surface_acquire_source_image,
    _cairo_quartz_surface_release_source_image,
    _cairo_quartz_surface_acquire_dest_image,
    _cairo_quartz_surface_release_dest_image,
    _cairo_quartz_surface_clone_similar,
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    _cairo_quartz_surface_intersect_clip_path,
    _cairo_quartz_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    _cairo_quartz_surface_paint,
    _cairo_quartz_surface_mask,
    _cairo_quartz_surface_stroke,
    _cairo_quartz_surface_fill,
#if CAIRO_HAS_QUARTZ_FONT
    _cairo_quartz_surface_show_glyphs,
#else
    NULL, /* show_glyphs */
#endif

    NULL, /* snapshot */
    NULL, /* is_similar */
    NULL, /* reset */
    NULL  /* fill_stroke */
};

cairo_quartz_surface_t *
_cairo_quartz_surface_create_internal (CGContextRef cgContext,
					cairo_content_t content,
					unsigned int width,
					unsigned int height)
{
    cairo_quartz_surface_t *surface;

    quartz_ensure_symbols();

    /* Init the base surface */
    surface = malloc(sizeof(cairo_quartz_surface_t));
    if (surface == NULL)
	return (cairo_quartz_surface_t*) _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    memset(surface, 0, sizeof(cairo_quartz_surface_t));

    _cairo_surface_init(&surface->base, &cairo_quartz_surface_backend,
			content);

    /* Save our extents */
    surface->extents.x = surface->extents.y = 0;
    surface->extents.width = width;
    surface->extents.height = height;

    if (IS_EMPTY(surface)) {
	surface->cgContext = NULL;
	surface->cgContextBaseCTM = CGAffineTransformIdentity;
	surface->imageData = NULL;
	return surface;
    }

    /* Save so we can always get back to a known-good CGContext -- this is
     * required for proper behaviour of intersect_clip_path(NULL)
     */
    CGContextSaveGState (cgContext);

    surface->cgContext = cgContext;
    surface->cgContextBaseCTM = CGContextGetCTM (cgContext);

    surface->imageData = NULL;
    surface->imageSurfaceEquiv = NULL;

    return surface;
}

/**
 * cairo_quartz_surface_create_for_cg_context
 * @cgContext: the existing CGContext for which to create the surface
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a Quartz surface that wraps the given CGContext.  The
 * CGContext is assumed to be in the standard Cairo coordinate space
 * (that is, with the origin at the upper left and the Y axis
 * increasing downward).  If the CGContext is in the Quartz coordinate
 * space (with the origin at the bottom left), then it should be
 * flipped before this function is called.  The flip can be accomplished
 * using a translate and a scale; for example:
 *
 * <informalexample><programlisting>
 * CGContextTranslateCTM (cgContext, 0.0, height);
 * CGContextScaleCTM (cgContext, 1.0, -1.0);
 * </programlisting></informalexample>
 *
 * All Cairo operations are implemented in terms of Quartz operations,
 * as long as Quartz-compatible elements are used (such as Quartz fonts).
 *
 * Return value: the newly created Cairo surface.
 *
 * Since: 1.4
 **/

cairo_surface_t *
cairo_quartz_surface_create_for_cg_context (CGContextRef cgContext,
					    unsigned int width,
					    unsigned int height)
{
    cairo_quartz_surface_t *surf;

    CGContextRetain (cgContext);

    surf = _cairo_quartz_surface_create_internal (cgContext, CAIRO_CONTENT_COLOR_ALPHA,
						  width, height);
    if (surf->base.status) {
	CGContextRelease (cgContext);
	// create_internal will have set an error
	return (cairo_surface_t*) surf;
    }

    return (cairo_surface_t *) surf;
}

/**
 * cairo_quartz_surface_create
 * @format: format of pixels in the surface to create
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a Quartz surface backed by a CGBitmap.  The surface is
 * created using the Device RGB (or Device Gray, for A8) color space.
 * All Cairo operations, including those that require software
 * rendering, will succeed on this surface.
 *
 * Return value: the newly created surface.
 *
 * Since: 1.4
 **/
cairo_surface_t *
cairo_quartz_surface_create (cairo_format_t format,
			     unsigned int width,
			     unsigned int height)
{
    cairo_quartz_surface_t *surf;
    CGContextRef cgc;
    CGColorSpaceRef cgColorspace;
    CGBitmapInfo bitinfo;
    void *imageData;
    int stride;
    int bitsPerComponent;

    // verify width and height of surface
    if (!_cairo_quartz_verify_surface_size(width, height))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    if (width == 0 || height == 0) {
	return (cairo_surface_t*) _cairo_quartz_surface_create_internal (NULL, _cairo_content_from_format (format),
									 width, height);
    }

    if (format == CAIRO_FORMAT_ARGB32 ||
	format == CAIRO_FORMAT_RGB24)
    {
	cgColorspace = CGColorSpaceCreateDeviceRGB();
	bitinfo = kCGBitmapByteOrder32Host;
	if (format == CAIRO_FORMAT_ARGB32)
	    bitinfo |= kCGImageAlphaPremultipliedFirst;
	else
	    bitinfo |= kCGImageAlphaNoneSkipFirst;
	bitsPerComponent = 8;

	/* The Apple docs say that for best performance, the stride and the data
	 * pointer should be 16-byte aligned.  malloc already aligns to 16-bytes,
	 * so we don't have to anything special on allocation.
	 */
	stride = width * 4;
	stride += (16 - (stride & 15)) & 15;
    } else if (format == CAIRO_FORMAT_A8) {
	cgColorspace = CGColorSpaceCreateDeviceGray();
	if (width % 4 == 0)
	    stride = width;
	else
	    stride = (width & ~3) + 4;
	bitinfo = kCGImageAlphaNone;
	bitsPerComponent = 8;
    } else if (format == CAIRO_FORMAT_A1) {
	/* I don't think we can usefully support this, as defined by
	 * cairo_format_t -- these are 1-bit pixels stored in 32-bit
	 * quantities.
	 */
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    } else {
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    }

    imageData = _cairo_malloc_ab (height, stride);
    if (!imageData) {
	CGColorSpaceRelease (cgColorspace);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    /* zero the memory to match the image surface behaviour */
    memset (imageData, 0, height * stride);

    cgc = CGBitmapContextCreate (imageData,
				 width,
				 height,
				 bitsPerComponent,
				 stride,
				 cgColorspace,
				 bitinfo);
    CGColorSpaceRelease (cgColorspace);

    if (!cgc) {
	free (imageData);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    /* flip the Y axis */
    CGContextTranslateCTM (cgc, 0.0, height);
    CGContextScaleCTM (cgc, 1.0, -1.0);

    surf = _cairo_quartz_surface_create_internal (cgc, _cairo_content_from_format (format),
						  width, height);
    if (surf->base.status) {
	CGContextRelease (cgc);
	free (imageData);
	// create_internal will have set an error
	return (cairo_surface_t*) surf;
    }

    surf->imageData = imageData;
    surf->imageSurfaceEquiv = cairo_image_surface_create_for_data (imageData, format, width, height, stride);

    return (cairo_surface_t *) surf;
}

/**
 * cairo_quartz_surface_get_cg_context
 * @surface: the Cairo Quartz surface
 *
 * Returns the CGContextRef that the given Quartz surface is backed
 * by.
 *
 * Return value: the CGContextRef for the given surface.
 *
 * Since: 1.4
 **/
CGContextRef
cairo_quartz_surface_get_cg_context (cairo_surface_t *surface)
{
    cairo_quartz_surface_t *quartz = (cairo_quartz_surface_t*)surface;

    if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_QUARTZ)
	return NULL;

    return quartz->cgContext;
}


/* Debug stuff */

#ifdef QUARTZ_DEBUG

#include <Movies.h>

void ExportCGImageToPNGFile(CGImageRef inImageRef, char* dest)
{
    Handle  dataRef = NULL;
    OSType  dataRefType;
    CFStringRef inPath = CFStringCreateWithCString(NULL, dest, kCFStringEncodingASCII);

    GraphicsExportComponent grex = 0;
    unsigned long sizeWritten;

    ComponentResult result;

    // create the data reference
    result = QTNewDataReferenceFromFullPathCFString(inPath, kQTNativeDefaultPathStyle,
						    0, &dataRef, &dataRefType);

    if (NULL != dataRef && noErr == result) {
	// get the PNG exporter
	result = OpenADefaultComponent(GraphicsExporterComponentType, kQTFileTypePNG,
				       &grex);

	if (grex) {
	    // tell the exporter where to find its source image
	    result = GraphicsExportSetInputCGImage(grex, inImageRef);

	    if (noErr == result) {
		// tell the exporter where to save the exporter image
		result = GraphicsExportSetOutputDataReference(grex, dataRef,
							      dataRefType);

		if (noErr == result) {
		    // write the PNG file
		    result = GraphicsExportDoExport(grex, &sizeWritten);
		}
	    }

	    // remember to close the component
	    CloseComponent(grex);
	}

	// remember to dispose of the data reference handle
	DisposeHandle(dataRef);
    }
}

void
quartz_image_to_png (CGImageRef imgref, char *dest)
{
    static int sctr = 0;
    char sptr[] = "/Users/vladimir/Desktop/barXXXXX.png";

    if (dest == NULL) {
	fprintf (stderr, "** Writing %p to bar%d\n", imgref, sctr);
	sprintf (sptr, "/Users/vladimir/Desktop/bar%d.png", sctr);
	sctr++;
	dest = sptr;
    }

    ExportCGImageToPNGFile(imgref, dest);
}

void
quartz_surface_to_png (cairo_quartz_surface_t *nq, char *dest)
{
    static int sctr = 0;
    char sptr[] = "/Users/vladimir/Desktop/fooXXXXX.png";

    if (nq->base.type != CAIRO_SURFACE_TYPE_QUARTZ) {
	fprintf (stderr, "** quartz_surface_to_png: surface %p isn't quartz!\n", nq);
	return;
    }

    if (dest == NULL) {
	fprintf (stderr, "** Writing %p to foo%d\n", nq, sctr);
	sprintf (sptr, "/Users/vladimir/Desktop/foo%d.png", sctr);
	sctr++;
	dest = sptr;
    }

    CGImageRef imgref = CGBitmapContextCreateImage (nq->cgContext);
    if (imgref == NULL) {
	fprintf (stderr, "quartz surface at %p is not a bitmap context!\n", nq);
	return;
    }

    ExportCGImageToPNGFile(imgref, dest);

    CGImageRelease(imgref);
}

#endif /* QUARTZ_DEBUG */
