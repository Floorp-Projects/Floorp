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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 *
 * Contributor(s):
 *	Vladimir Vukicevic <vladimir@mozilla.com>
 */

#define _GNU_SOURCE /* required for RTLD_DEFAULT */
#include "cairoint.h"

#include "cairo-quartz-private.h"
#include "cairo-surface-clipper-private.h"
#include "cairo-gstate-private.h"
#include "cairo-private.h"

#include <dlfcn.h>

#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT ((void *) 0)
#endif

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

/* Here are some of the differences between cairo and CoreGraphics
   - cairo has only a single source active at once vs. CoreGraphics having
     separate sources for stroke and fill
*/

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
 * they are public; others are not public at all
 * (CGContextReplacePathWithClipPath, many of the getters, etc.)
 */
static void (*CGContextClipToMaskPtr) (CGContextRef, CGRect, CGImageRef) = NULL;
static void (*CGContextDrawTiledImagePtr) (CGContextRef, CGRect, CGImageRef) = NULL;
static unsigned int (*CGContextGetTypePtr) (CGContextRef) = NULL;
static void (*CGContextSetShouldAntialiasFontsPtr) (CGContextRef, bool) = NULL;
static bool (*CGContextGetShouldAntialiasFontsPtr) (CGContextRef) = NULL;
static bool (*CGContextGetShouldSmoothFontsPtr) (CGContextRef) = NULL;
static void (*CGContextSetAllowsFontSmoothingPtr) (CGContextRef, bool) = NULL;
static bool (*CGContextGetAllowsFontSmoothingPtr) (CGContextRef) = NULL;
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
    CGContextReplacePathWithClipPathPtr = dlsym(RTLD_DEFAULT, "CGContextReplacePathWithClipPath");
    CGContextGetAllowsFontSmoothingPtr = dlsym(RTLD_DEFAULT, "CGContextGetAllowsFontSmoothing");
    CGContextSetAllowsFontSmoothingPtr = dlsym(RTLD_DEFAULT, "CGContextSetAllowsFontSmoothing");

    if (Gestalt(gestaltSystemVersion, &_cairo_quartz_osx_version) != noErr) {
	// assume 10.4
	_cairo_quartz_osx_version = 0x1040;
    }

    _cairo_quartz_symbol_lookup_done = TRUE;
}

CGImageRef
_cairo_quartz_create_cgimage (cairo_format_t format,
			      unsigned int width,
			      unsigned int height,
			      unsigned int stride,
			      void *data,
			      cairo_bool_t interpolate,
			      CGColorSpaceRef colorSpaceOverride,
			      CGDataProviderReleaseDataCallback releaseCallback,
			      void *releaseInfo)
{
    CGImageRef image = NULL;
    CGDataProviderRef dataProvider = NULL;
    CGColorSpaceRef colorSpace = colorSpaceOverride;
    CGBitmapInfo bitinfo;
    int bitsPerComponent, bitsPerPixel;

    switch (format) {
	case CAIRO_FORMAT_ARGB32:
	    if (colorSpace == NULL)
		colorSpace = CGColorSpaceCreateDeviceRGB();
	    bitinfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
	    bitsPerComponent = 8;
	    bitsPerPixel = 32;
	    break;

	case CAIRO_FORMAT_RGB24:
	    if (colorSpace == NULL)
		colorSpace = CGColorSpaceCreateDeviceRGB();
	    bitinfo = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
	    bitsPerComponent = 8;
	    bitsPerPixel = 32;
	    break;

	case CAIRO_FORMAT_A8:
	    bitsPerComponent = 8;
	    bitsPerPixel = 8;
	    break;

	case CAIRO_FORMAT_A1:
	    bitsPerComponent = 1;
	    bitsPerPixel = 1;
	    break;

	default:
	    return NULL;
    }

    dataProvider = CGDataProviderCreateWithData (releaseInfo,
						 data,
						 height * stride,
						 releaseCallback);

    if (!dataProvider) {
	// manually release
	if (releaseCallback)
	    releaseCallback (releaseInfo, data, height * stride);
	goto FINISH;
    }

    if (format == CAIRO_FORMAT_A8 || format == CAIRO_FORMAT_A1) {
	CGFloat decode[] = {1.0, 0.0};
	image = CGImageMaskCreate (width, height,
				   bitsPerComponent,
				   bitsPerPixel,
				   stride,
				   dataProvider,
				   decode,
				   interpolate);
    } else
	image = CGImageCreate (width, height,
			       bitsPerComponent,
			       bitsPerPixel,
			       stride,
			       colorSpace,
			       bitinfo,
			       dataProvider,
			       NULL,
			       interpolate,
			       kCGRenderingIntentDefault);

FINISH:

    CGDataProviderRelease (dataProvider);

    if (colorSpace != colorSpaceOverride)
	CGColorSpaceRelease (colorSpace);

    return image;
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
_cairo_path_to_quartz_context_move_to (void *closure,
				       const cairo_point_t *point)
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
_cairo_path_to_quartz_context_line_to (void *closure,
				       const cairo_point_t *point)
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
_cairo_path_to_quartz_context_curve_to (void *closure,
					const cairo_point_t *p0,
					const cairo_point_t *p1,
					const cairo_point_t *p2)
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
_cairo_quartz_cairo_operator_to_quartz_composite (cairo_operator_t op)
{
    switch (op) {
	case CAIRO_OPERATOR_CLEAR:
	    return kPrivateCGCompositeClear;
	case CAIRO_OPERATOR_SOURCE:
	    return kPrivateCGCompositeCopy;
	case CAIRO_OPERATOR_OVER:
	    return kPrivateCGCompositeSourceOver;
	case CAIRO_OPERATOR_IN:
	    return kPrivateCGCompositeSourceIn;
	case CAIRO_OPERATOR_OUT:
	    return kPrivateCGCompositeSourceOut;
	case CAIRO_OPERATOR_ATOP:
	    return kPrivateCGCompositeSourceAtop;
	case CAIRO_OPERATOR_DEST_OVER:
	    return kPrivateCGCompositeDestinationOver;
	case CAIRO_OPERATOR_DEST_IN:
	    return kPrivateCGCompositeDestinationIn;
	case CAIRO_OPERATOR_DEST_OUT:
	    return kPrivateCGCompositeDestinationOut;
	case CAIRO_OPERATOR_DEST_ATOP:
	    return kPrivateCGCompositeDestinationAtop;
	case CAIRO_OPERATOR_XOR:
	    return kPrivateCGCompositeXOR;
	case CAIRO_OPERATOR_ADD:
	    return kPrivateCGCompositePlusLighter;

	case CAIRO_OPERATOR_DEST:
	case CAIRO_OPERATOR_SATURATE:
	case CAIRO_OPERATOR_MULTIPLY:
	case CAIRO_OPERATOR_SCREEN:
	case CAIRO_OPERATOR_OVERLAY:
	case CAIRO_OPERATOR_DARKEN:
	case CAIRO_OPERATOR_LIGHTEN:
	case CAIRO_OPERATOR_COLOR_DODGE:
	case CAIRO_OPERATOR_COLOR_BURN:
	case CAIRO_OPERATOR_HARD_LIGHT:
	case CAIRO_OPERATOR_SOFT_LIGHT:
	case CAIRO_OPERATOR_DIFFERENCE:
	case CAIRO_OPERATOR_EXCLUSION:
	case CAIRO_OPERATOR_HSL_HUE:
	case CAIRO_OPERATOR_HSL_SATURATION:
	case CAIRO_OPERATOR_HSL_COLOR:
	case CAIRO_OPERATOR_HSL_LUMINOSITY:
        default:
	    assert (0);
    }
}

static cairo_int_status_t
_cairo_quartz_surface_set_cairo_operator (cairo_quartz_surface_t *surface, cairo_operator_t op)
{
    ND((stderr, "%p _cairo_quartz_surface_set_cairo_operator %d\n", surface, op));

    if (surface->base.content == CAIRO_CONTENT_ALPHA) {
	/* For some weird reason, some compositing operators are
	   swapped when operating on masks */
	switch (op) {
	    case CAIRO_OPERATOR_CLEAR:
	    case CAIRO_OPERATOR_SOURCE:
	    case CAIRO_OPERATOR_OVER:
	    case CAIRO_OPERATOR_DEST_IN:
	    case CAIRO_OPERATOR_DEST_OUT:
	    case CAIRO_OPERATOR_ADD:
		CGContextSetCompositeOperation (surface->cgContext, _cairo_quartz_cairo_operator_to_quartz_composite (op));
		return CAIRO_STATUS_SUCCESS;

	    case CAIRO_OPERATOR_IN:
		CGContextSetCompositeOperation (surface->cgContext, kPrivateCGCompositeDestinationAtop);
		return CAIRO_STATUS_SUCCESS;

	    case CAIRO_OPERATOR_DEST_OVER:
	    case CAIRO_OPERATOR_MULTIPLY:
	    case CAIRO_OPERATOR_SCREEN:
	    case CAIRO_OPERATOR_OVERLAY:
	    case CAIRO_OPERATOR_DARKEN:
	    case CAIRO_OPERATOR_LIGHTEN:
	    case CAIRO_OPERATOR_COLOR_DODGE:
	    case CAIRO_OPERATOR_COLOR_BURN:
	    case CAIRO_OPERATOR_HARD_LIGHT:
	    case CAIRO_OPERATOR_SOFT_LIGHT:
	    case CAIRO_OPERATOR_DIFFERENCE:
	    case CAIRO_OPERATOR_EXCLUSION:
	    case CAIRO_OPERATOR_HSL_HUE:
	    case CAIRO_OPERATOR_HSL_SATURATION:
	    case CAIRO_OPERATOR_HSL_COLOR:
	    case CAIRO_OPERATOR_HSL_LUMINOSITY:
		CGContextSetCompositeOperation (surface->cgContext, kPrivateCGCompositeSourceOver);
		return CAIRO_STATUS_SUCCESS;

	    case CAIRO_OPERATOR_DEST_ATOP:
		CGContextSetCompositeOperation (surface->cgContext, kPrivateCGCompositeSourceIn);
		return CAIRO_STATUS_SUCCESS;

	    case CAIRO_OPERATOR_SATURATE:
		CGContextSetCompositeOperation (surface->cgContext, kPrivateCGCompositePlusLighter);
		return CAIRO_STATUS_SUCCESS;


	    case CAIRO_OPERATOR_ATOP:
		/*
		CGContextSetCompositeOperation (surface->cgContext, kPrivateCGCompositeDestinationOver);
		return CAIRO_STATUS_SUCCESS;
		*/
	    case CAIRO_OPERATOR_DEST:
		return CAIRO_INT_STATUS_NOTHING_TO_DO;

	    case CAIRO_OPERATOR_OUT:
	    case CAIRO_OPERATOR_XOR:
	    default:
		return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    } else {
	switch (op) {
	    case CAIRO_OPERATOR_CLEAR:
	    case CAIRO_OPERATOR_SOURCE:
	    case CAIRO_OPERATOR_OVER:
	    case CAIRO_OPERATOR_IN:
	    case CAIRO_OPERATOR_OUT:
	    case CAIRO_OPERATOR_ATOP:
	    case CAIRO_OPERATOR_DEST_OVER:
	    case CAIRO_OPERATOR_DEST_IN:
	    case CAIRO_OPERATOR_DEST_OUT:
	    case CAIRO_OPERATOR_DEST_ATOP:
	    case CAIRO_OPERATOR_XOR:
	    case CAIRO_OPERATOR_ADD:
		CGContextSetCompositeOperation (surface->cgContext, _cairo_quartz_cairo_operator_to_quartz_composite (op));
		return CAIRO_STATUS_SUCCESS;

	    case CAIRO_OPERATOR_DEST:
		return CAIRO_INT_STATUS_NOTHING_TO_DO;

	    case CAIRO_OPERATOR_SATURATE:
	    /* TODO: the following are mostly supported by CGContextSetBlendMode*/
	    case CAIRO_OPERATOR_MULTIPLY:
	    case CAIRO_OPERATOR_SCREEN:
	    case CAIRO_OPERATOR_OVERLAY:
	    case CAIRO_OPERATOR_DARKEN:
	    case CAIRO_OPERATOR_LIGHTEN:
	    case CAIRO_OPERATOR_COLOR_DODGE:
	    case CAIRO_OPERATOR_COLOR_BURN:
	    case CAIRO_OPERATOR_HARD_LIGHT:
	    case CAIRO_OPERATOR_SOFT_LIGHT:
	    case CAIRO_OPERATOR_DIFFERENCE:
	    case CAIRO_OPERATOR_EXCLUSION:
	    case CAIRO_OPERATOR_HSL_HUE:
	    case CAIRO_OPERATOR_HSL_SATURATION:
	    case CAIRO_OPERATOR_HSL_COLOR:
	    case CAIRO_OPERATOR_HSL_LUMINOSITY:
	    default:
		return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    }
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

/*
 * Source -> Quartz setup and finish functions
 */

static void
ComputeGradientValue (void *info, const CGFloat *in, CGFloat *out)
{
    double fdist = *in;
    const cairo_gradient_pattern_t *grad = (cairo_gradient_pattern_t*) info;
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

static const CGFloat gradient_output_value_ranges[8] = {
    0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f
};
static const CGFunctionCallbacks gradient_callbacks = {
    0, ComputeGradientValue, (CGFunctionReleaseInfoCallback) cairo_pattern_destroy
};
/* Quartz will clamp input values to the input range.

   Our stops are all in the range 0.0 to 1.0. However, the color before the
   beginning of the gradient line is obtained by Quartz computing a negative
   position on the gradient line, clamping it to the input range we specified
   for our color function, and then calling our color function (actually it
   pre-samples the color function into an array, but that doesn't matter just
   here). Therefore if we set the lower bound to 0.0, a negative position
   on the gradient line will pass 0.0 to ComputeGradientValue, which will
   select the last color stop with position 0, although it should select
   the first color stop (this matters when there are multiple color stops with
   position 0). 
   
   Therefore we pass a small negative number as the lower bound of the input
   range, so this value gets passed into ComputeGradientValue, which will
   return the color of the first stop. The number should be small because
   as far as I can tell, Quartz pre-samples the entire input range of the color
   function into an array of fixed size, so if the input range is larger
   than needed, the resolution of the gradient will be unnecessarily low.
*/
static const CGFloat nonrepeating_gradient_input_value_range[2] = { -0.001f, 1.f };

static CGFunctionRef
CreateGradientFunction (const cairo_gradient_pattern_t *gpat)
{
    cairo_pattern_t *pat;

    if (_cairo_pattern_create_copy (&pat, &gpat->base))
	/* quartz doesn't deal very well with malloc failing, so there's
	 * not much point in us trying either */
	return NULL;

    return CGFunctionCreate (pat,
			     1,
			     nonrepeating_gradient_input_value_range,
			     4,
			     gradient_output_value_ranges,
			     &gradient_callbacks);
}

static void
UpdateLinearParametersToIncludePoint(double *min_t, double *max_t, CGPoint *start,
                                     double dx, double dy,
                                     double x, double y)
{
    /* Compute a parameter t such that a line perpendicular to the (dx,dy)
       vector, passing through (start->x + dx*t, start->y + dy*t), also
       passes through (x,y).

       Let px = x - start->x, py = y - start->y.
       t is given by
         (px - dx*t)*dx + (py - dy*t)*dy = 0

       Solving for t we get
         numerator = dx*px + dy*py
         denominator = dx^2 + dy^2
         t = numerator/denominator

       In CreateRepeatingLinearGradientFunction we know the length of (dx,dy)
       is not zero. (This is checked in _cairo_quartz_setup_linear_source.)
    */
    double px = x - start->x;
    double py = y - start->y;
    double numerator = dx*px + dy*py;
    double denominator = dx*dx + dy*dy;
    double t = numerator/denominator;

    if (*min_t > t) {
        *min_t = t;
    }
    if (*max_t < t) {
        *max_t = t;
    }
}

static CGFunctionRef
CreateRepeatingLinearGradientFunction (cairo_quartz_surface_t *surface,
				       const cairo_gradient_pattern_t *gpat,
				       CGPoint *start, CGPoint *end,
				       cairo_rectangle_int_t *extents)
{
    cairo_pattern_t *pat;
    CGFloat input_value_range[2];
    double t_min = 0.;
    double t_max = 0.;
    double dx = end->x - start->x;
    double dy = end->y - start->y;
    double bounds_x1, bounds_x2, bounds_y1, bounds_y2;

    if (!extents) {
        extents = &surface->extents;
    }
    bounds_x1 = extents->x;
    bounds_y1 = extents->y;
    bounds_x2 = extents->x + extents->width;
    bounds_y2 = extents->y + extents->height;
    _cairo_matrix_transform_bounding_box (&gpat->base.matrix,
                                          &bounds_x1, &bounds_y1,
                                          &bounds_x2, &bounds_y2,
                                          NULL);

    UpdateLinearParametersToIncludePoint(&t_min, &t_max, start, dx, dy,
                                         bounds_x1, bounds_y1);
    UpdateLinearParametersToIncludePoint(&t_min, &t_max, start, dx, dy,
                                         bounds_x2, bounds_y1);
    UpdateLinearParametersToIncludePoint(&t_min, &t_max, start, dx, dy,
                                         bounds_x2, bounds_y2);
    UpdateLinearParametersToIncludePoint(&t_min, &t_max, start, dx, dy,
                                         bounds_x1, bounds_y2);

    /* Move t_min and t_max to the nearest usable integer to try to avoid
       subtle variations due to numerical instability, especially accidentally
       cutting off a pixel. Extending the gradient repetitions is always safe. */
    t_min = floor (t_min);
    t_max = ceil (t_max);
    end->x = start->x + dx*t_max;
    end->y = start->y + dy*t_max;
    start->x = start->x + dx*t_min;
    start->y = start->y + dy*t_min;

    // set the input range for the function -- the function knows how to
    // map values outside of 0.0 .. 1.0 to that range for REPEAT/REFLECT.
    input_value_range[0] = t_min;
    input_value_range[1] = t_max;

    if (_cairo_pattern_create_copy (&pat, &gpat->base))
	/* quartz doesn't deal very well with malloc failing, so there's
	 * not much point in us trying either */
	return NULL;

    return CGFunctionCreate (pat,
			     1,
			     input_value_range,
			     4,
			     gradient_output_value_ranges,
			     &gradient_callbacks);
}

static void
UpdateRadialParameterToIncludePoint(double *max_t, CGPoint *center,
                                    double dr, double dx, double dy,
                                    double x, double y)
{
    /* Compute a parameter t such that a circle centered at
       (center->x + dx*t, center->y + dy*t) with radius dr*t contains the
       point (x,y).

       Let px = x - center->x, py = y - center->y.
       Parameter values for which t is on the circle are given by
         (px - dx*t)^2 + (py - dy*t)^2 = (t*dr)^2

       Solving for t using the quadratic formula, and simplifying, we get
         numerator = dx*px + dy*py +-
                     sqrt( dr^2*(px^2 + py^2) - (dx*py - dy*px)^2 )
         denominator = dx^2 + dy^2 - dr^2
         t = numerator/denominator

       In CreateRepeatingRadialGradientFunction we know the outer circle
       contains the inner circle. Therefore the distance between the circle
       centers plus the radius of the inner circle is less than the radius of
       the outer circle. (This is checked in _cairo_quartz_setup_radial_source.)
       Therefore
         dx^2 + dy^2 < dr^2
       So the denominator is negative and the larger solution for t is given by
         numerator = dx*px + dy*py -
                     sqrt( dr^2*(px^2 + py^2) - (dx*py - dy*px)^2 )
         denominator = dx^2 + dy^2 - dr^2
         t = numerator/denominator
       dx^2 + dy^2 < dr^2 also ensures that the operand of sqrt is positive.
    */
    double px = x - center->x;
    double py = y - center->y;
    double dx_py_minus_dy_px = dx*py - dy*px;
    double numerator = dx*px + dy*py -
        sqrt (dr*dr*(px*px + py*py) - dx_py_minus_dy_px*dx_py_minus_dy_px);
    double denominator = dx*dx + dy*dy - dr*dr;
    double t = numerator/denominator;

    if (*max_t < t) {
        *max_t = t;
    }
}

/* This must only be called when one of the circles properly contains the other */
static CGFunctionRef
CreateRepeatingRadialGradientFunction (cairo_quartz_surface_t *surface,
                                       const cairo_gradient_pattern_t *gpat,
                                       CGPoint *start, double *start_radius,
                                       CGPoint *end, double *end_radius,
                                       cairo_rectangle_int_t *extents)
{
    cairo_pattern_t *pat;
    CGFloat input_value_range[2];
    CGPoint *inner;
    double *inner_radius;
    CGPoint *outer;
    double *outer_radius;
    /* minimum and maximum t-parameter values that will make our gradient
       cover the clipBox */
    double t_min, t_max, t_temp;
    /* outer minus inner */
    double dr, dx, dy;
    double bounds_x1, bounds_x2, bounds_y1, bounds_y2;

    if (!extents) {
        extents = &surface->extents;
    }
    bounds_x1 = extents->x;
    bounds_y1 = extents->y;
    bounds_x2 = extents->x + extents->width;
    bounds_y2 = extents->y + extents->height;
    _cairo_matrix_transform_bounding_box (&gpat->base.matrix,
                                          &bounds_x1, &bounds_y1,
                                          &bounds_x2, &bounds_y2,
                                          NULL);

    if (*start_radius < *end_radius) {
        /* end circle contains start circle */
        inner = start;
        outer = end;
        inner_radius = start_radius;
        outer_radius = end_radius;
    } else {
        /* start circle contains end circle */
        inner = end;
        outer = start;
        inner_radius = end_radius;
        outer_radius = start_radius;
    }

    dr = *outer_radius - *inner_radius;
    dx = outer->x - inner->x;
    dy = outer->y - inner->y;

    /* We can't round or fudge t_min here, it has to be as accurate as possible. */
    t_min = -(*inner_radius/dr);
    inner->x += t_min*dx;
    inner->y += t_min*dy;
    *inner_radius = 0.;

    t_temp = 0.;
    UpdateRadialParameterToIncludePoint(&t_temp, inner, dr, dx, dy,
                                        bounds_x1, bounds_y1);
    UpdateRadialParameterToIncludePoint(&t_temp, inner, dr, dx, dy,
                                        bounds_x2, bounds_y1);
    UpdateRadialParameterToIncludePoint(&t_temp, inner, dr, dx, dy,
                                        bounds_x2, bounds_y2);
    UpdateRadialParameterToIncludePoint(&t_temp, inner, dr, dx, dy,
                                        bounds_x1, bounds_y2);
    /* UpdateRadialParameterToIncludePoint assumes t=0 means radius 0.
       But for the parameter values we use with Quartz, t_min means radius 0.
       Since the circles are alway expanding and contain the earlier circles,
       it's safe to extend t_max/t_temp as much as we want, so round t_temp up
       to the nearest integer. This may help us give stable results. */
    t_temp = ceil (t_temp);
    t_max = t_min + t_temp;
    outer->x = inner->x + t_temp*dx;
    outer->y = inner->y + t_temp*dy;
    *outer_radius = t_temp*dr;

    /* set the input range for the function -- the function knows how to
       map values outside of 0.0 .. 1.0 to that range for REPEAT/REFLECT. */
    if (*start_radius < *end_radius) {
        input_value_range[0] = t_min;
        input_value_range[1] = t_max;
    } else {
        input_value_range[0] = -t_max;
        input_value_range[1] = -t_min;
    }

    if (_cairo_pattern_create_copy (&pat, &gpat->base))
  /* quartz doesn't deal very well with malloc failing, so there's
   * not much point in us trying either */
  return NULL;

    return CGFunctionCreate (pat,
           1,
           input_value_range,
           4,
           gradient_output_value_ranges,
           &gradient_callbacks);
}

/* Obtain a CGImageRef from a #cairo_surface_t * */

static void
DataProviderReleaseCallback (void *info, const void *data, size_t size)
{
    cairo_surface_t *surface = (cairo_surface_t *) info;
    cairo_surface_destroy (surface);
}

static cairo_status_t
_cairo_surface_to_cgimage (cairo_surface_t *source,
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
	    if (!surface->bitmapContextImage) {
	        surface->bitmapContextImage =
	            CGBitmapContextCreateImage (surface->cgContext);
	    }
	    if (surface->bitmapContextImage) {
                *image_out = CGImageRetain (surface->bitmapContextImage);
                return CAIRO_STATUS_SUCCESS;
            }
	}
    }

    if (stype != CAIRO_SURFACE_TYPE_IMAGE) {
	status = _cairo_surface_acquire_source_image (source,
						      &isurf, &image_extra);
	if (status)
	    return status;
    } else {
	isurf = (cairo_image_surface_t *) source;
    }

    if (isurf->width == 0 || isurf->height == 0) {
	*image_out = NULL;
    } else {
	cairo_image_surface_t *isurf_snap = NULL;

	isurf_snap = (cairo_image_surface_t*)
	    _cairo_surface_snapshot (&isurf->base);
	if (isurf_snap->base.status)
	    return isurf_snap->base.status;

	image = _cairo_quartz_create_cgimage (isurf_snap->format,
					      isurf_snap->width,
					      isurf_snap->height,
					      isurf_snap->stride,
					      isurf_snap->data,
					      TRUE,
					      NULL,
					      DataProviderReleaseCallback,
					      isurf_snap);

	*image_out = image;
	if (image == NULL)
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (&isurf->base != source)
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
							 const cairo_pattern_t *apattern,
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
    cairo_bool_t is_bounded;

    cairo_matrix_t m;

    /* SURFACE is the only type we'll handle here */
    if (apattern->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    spattern = (cairo_surface_pattern_t *) apattern;
    pat_surf = spattern->surface;

    is_bounded = _cairo_surface_get_extents (pat_surf, &extents);
    assert (is_bounded);

    status = _cairo_surface_to_cgimage (pat_surf, &image);
    if (status)
	return status;
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
    info->do_reflect = FALSE;

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
    DO_TILED_IMAGE,
    DO_LAYER,
    DO_UNSUPPORTED,
    DO_NOTHING
} cairo_quartz_action_t;

/* State used during a drawing operation. */
typedef struct {
    CGContextRef context;
    cairo_quartz_action_t action;

    // Used with DO_SHADING, DO_IMAGE, DO_TILED_IMAGE and DO_LAYER
    CGAffineTransform transform;

    // Used with DO_IMAGE and DO_TILED_IMAGE
    CGImageRef image;
    cairo_surface_t *imageSurface;

    // Used with DO_IMAGE, DO_TILED_IMAGE and DO_LAYER
    CGRect imageRect;

    // Used with DO_LAYER
    CGLayerRef layer;

    // Used with DO_SHADING
    CGShadingRef shading;

    // Used with DO_PATTERN
    CGPatternRef pattern;

    // Used for handling unbounded operators
    CGLayerRef unboundedLayer;
    CGPoint unboundedLayerOffset;
    CGContextRef unboundedDestination;
} cairo_quartz_drawing_state_t;

static void
_cairo_quartz_setup_fallback_source (cairo_quartz_surface_t *surface,
				     const cairo_pattern_t *source,
				     cairo_quartz_drawing_state_t *state)
{
    CGRect clipBox = CGContextGetClipBoundingBox (state->context);
    double x0, y0, w, h;

    cairo_surface_t *fallback;
    CGImageRef img;

    cairo_status_t status;

    if (clipBox.size.width == 0.0f ||
	clipBox.size.height == 0.0f) {
	state->action = DO_NOTHING;
	return;
    }

    x0 = floor(clipBox.origin.x);
    y0 = floor(clipBox.origin.y);
    w = ceil(clipBox.origin.x + clipBox.size.width) - x0;
    h = ceil(clipBox.origin.y + clipBox.size.height) - y0;

    /* Create a temporary the size of the clip surface, and position
     * it so that the device origin coincides with the original surface */
    fallback = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, (int) w, (int) h);
    cairo_surface_set_device_offset (fallback, -x0, -y0);

#if 0
    {
	cairo_t *fallback_cr;
	cairo_pattern_t *source_copy;

	/* Paint the source onto our temporary */
	fallback_cr = cairo_create (fallback);
	cairo_set_operator (fallback_cr, CAIRO_OPERATOR_SOURCE);

	/* Use a copy of the pattern because it is const and could be allocated
	 * on the stack */
	status = _cairo_pattern_create_copy (&source_copy, source);
	cairo_set_source (fallback_cr, source_copy);
	cairo_pattern_destroy (source_copy);

	cairo_paint (fallback_cr);
	cairo_destroy (fallback_cr);
    }
#else
    {
	cairo_pattern_union_t pattern;

	_cairo_pattern_init_static_copy (&pattern.base, source);
	_cairo_pattern_transform (&pattern.base,
				  &fallback->device_transform_inverse);
	status = _cairo_surface_paint (fallback,
				       CAIRO_OPERATOR_SOURCE,
				       &pattern.base, NULL);
    }
#endif

    status = _cairo_surface_to_cgimage (fallback, &img);
    if (status) {
        state->action = DO_UNSUPPORTED;
	return;
    }
    if (img == NULL) {
        state->action = DO_NOTHING;
	return;
    }

    state->imageRect = CGRectMake (0.0, 0.0, w, h);
    state->image = img;
    state->imageSurface = fallback;
    state->transform = CGAffineTransformMakeTranslation (x0, y0);
    state->action = DO_IMAGE;
}

/*
Quartz does not support repeating radients. We handle repeating gradients
by manually extending the gradient and repeating color stops. We need to
minimize the number of repetitions since Quartz seems to sample our color
function across the entire range, even if part of that range is not needed
for the visible area of the gradient, and it samples with some fixed resolution,
so if the gradient range is too large it samples with very low resolution and
the gradient is very coarse. CreateRepeatingLinearGradientFunction and
CreateRepeatingRadialGradientFunction compute the number of repetitions needed
based on the extents of the object (the clip region cannot be used here since
we don't want the rasterization of the entire gradient to depend on the
clip region).
*/
static void
_cairo_quartz_setup_linear_source (cairo_quartz_surface_t *surface,
				   const cairo_linear_pattern_t *lpat,
				   cairo_rectangle_int_t *extents,
				   cairo_quartz_drawing_state_t *state)
{
    const cairo_pattern_t *abspat = &lpat->base.base;
    cairo_matrix_t mat;
    CGPoint start, end;
    CGFunctionRef gradFunc;
    CGColorSpaceRef rgb;
    bool extend = abspat->extend == CAIRO_EXTEND_PAD;

    if (lpat->base.n_stops == 0) {
	CGContextSetRGBStrokeColor (state->context, 0., 0., 0., 0.);
	CGContextSetRGBFillColor (state->context, 0., 0., 0., 0.);
	state->action = DO_SOLID;
	return;
    }

    if (lpat->p1.x == lpat->p2.x &&
        lpat->p1.y == lpat->p2.y) {
	/* Quartz handles cases where the vector has no length very
	 * differently from pixman.
	 * Whatever the correct behaviour is, let's at least have only pixman's
	 * implementation to worry about.
	 */
	_cairo_quartz_setup_fallback_source (surface, abspat, state);
	return;
    }

    mat = abspat->matrix;
    cairo_matrix_invert (&mat);
    _cairo_quartz_cairo_matrix_to_quartz (&mat, &state->transform);

    rgb = CGColorSpaceCreateDeviceRGB();

    start = CGPointMake (_cairo_fixed_to_double (lpat->p1.x),
			 _cairo_fixed_to_double (lpat->p1.y));
    end = CGPointMake (_cairo_fixed_to_double (lpat->p2.x),
		       _cairo_fixed_to_double (lpat->p2.y));

    if (abspat->extend == CAIRO_EXTEND_NONE ||
        abspat->extend == CAIRO_EXTEND_PAD) 
    {
	gradFunc = CreateGradientFunction (&lpat->base);
    } else {
	gradFunc = CreateRepeatingLinearGradientFunction (surface,
						          &lpat->base,
						          &start, &end,
						          extents);
    }

    state->shading = CGShadingCreateAxial (rgb,
					   start, end,
					   gradFunc,
					   extend, extend);

    CGColorSpaceRelease(rgb);
    CGFunctionRelease(gradFunc);

    state->action = DO_SHADING;
}

static void
_cairo_quartz_setup_radial_source (cairo_quartz_surface_t *surface,
				   const cairo_radial_pattern_t *rpat,
				   cairo_rectangle_int_t *extents,
				   cairo_quartz_drawing_state_t *state)
{
    const cairo_pattern_t *abspat = &rpat->base.base;
    cairo_matrix_t mat;
    CGPoint start, end;
    CGFunctionRef gradFunc;
    CGColorSpaceRef rgb;
    bool extend = abspat->extend == CAIRO_EXTEND_PAD;
    double c1x = _cairo_fixed_to_double (rpat->c1.x);
    double c1y = _cairo_fixed_to_double (rpat->c1.y);
    double c2x = _cairo_fixed_to_double (rpat->c2.x);
    double c2y = _cairo_fixed_to_double (rpat->c2.y);
    double r1 = _cairo_fixed_to_double (rpat->r1);
    double r2 = _cairo_fixed_to_double (rpat->r2);
    double dx = c1x - c2x;
    double dy = c1y - c2y;
    double centerDistance = sqrt (dx*dx + dy*dy);

    if (rpat->base.n_stops == 0) {
	CGContextSetRGBStrokeColor (state->context, 0., 0., 0., 0.);
	CGContextSetRGBFillColor (state->context, 0., 0., 0., 0.);
	state->action = DO_SOLID;
	return;
    }

    if (r2 <= centerDistance + r1 + 1e-6 && /* circle 2 doesn't contain circle 1 */
        r1 <= centerDistance + r2 + 1e-6) { /* circle 1 doesn't contain circle 2 */
	/* Quartz handles cases where neither circle contains the other very
	 * differently from pixman.
	 * Whatever the correct behaviour is, let's at least have only pixman's
	 * implementation to worry about.
	 * Note that this also catches the cases where r1 == r2.
	 */
	_cairo_quartz_setup_fallback_source (surface, abspat, state);
	return;
    }

    mat = abspat->matrix;
    cairo_matrix_invert (&mat);
    _cairo_quartz_cairo_matrix_to_quartz (&mat, &state->transform);

    rgb = CGColorSpaceCreateDeviceRGB();

    start = CGPointMake (c1x, c1y);
    end = CGPointMake (c2x, c2y);

    if (abspat->extend == CAIRO_EXTEND_NONE ||
        abspat->extend == CAIRO_EXTEND_PAD)
    {
	gradFunc = CreateGradientFunction (&rpat->base);
    } else {
	gradFunc = CreateRepeatingRadialGradientFunction (surface,
						          &rpat->base,
						          &start, &r1,
						          &end, &r2,
						          extents);
    }

    state->shading = CGShadingCreateRadial (rgb,
					    start,
					    r1,
					    end,
					    r2,
					    gradFunc,
					    extend, extend);

    CGColorSpaceRelease(rgb);
    CGFunctionRelease(gradFunc);

    state->action = DO_SHADING;
}

static void
_cairo_quartz_setup_surface_source (cairo_quartz_surface_t *surface,
				    const cairo_surface_pattern_t *spat,
				    cairo_rectangle_int_t *extents,
				    cairo_quartz_drawing_state_t *state)
{
    const cairo_pattern_t *source = &spat->base;
    CGContextRef context = state->context;

    if (source->extend == CAIRO_EXTEND_NONE || source->extend == CAIRO_EXTEND_PAD ||
        (CGContextDrawTiledImagePtr && source->extend == CAIRO_EXTEND_REPEAT))
    {
	cairo_surface_t *pat_surf = spat->surface;
	CGImageRef img;
	cairo_matrix_t m = spat->base.matrix;
	cairo_rectangle_int_t extents;
	CGAffineTransform xform;
	CGRect srcRect;
	cairo_fixed_t fw, fh;
	cairo_bool_t is_bounded;
	cairo_bool_t repeat = source->extend == CAIRO_EXTEND_REPEAT;
        cairo_status_t status;

        cairo_matrix_invert(&m);
        _cairo_quartz_cairo_matrix_to_quartz (&m, &state->transform);

        /* Draw nonrepeating CGLayer surface using DO_LAYER */
        if (!repeat && cairo_surface_get_type (pat_surf) == CAIRO_SURFACE_TYPE_QUARTZ) {
            cairo_quartz_surface_t *quartz_surf = (cairo_quartz_surface_t *) pat_surf;
            if (quartz_surf->cgLayer) {
         	state->imageRect = CGRectMake (0, 0, quartz_surf->extents.width, quartz_surf->extents.height);
                state->layer = quartz_surf->cgLayer;
                state->action = DO_LAYER;
                return;
            }
        }

	status = _cairo_surface_to_cgimage (pat_surf, &img);
        if (status) {
            state->action = DO_UNSUPPORTED;
	    return;
        }
        if (img == NULL) {
            state->action = DO_NOTHING;
	    return;
        }

        /* XXXroc what is this for? */
	CGContextSetRGBFillColor (surface->cgContext, 0, 0, 0, 1);

	state->image = img;

	is_bounded = _cairo_surface_get_extents (pat_surf, &extents);
	assert (is_bounded);

	if (!repeat) {
	    state->imageRect = CGRectMake (0, 0, extents.width, extents.height);
	    state->action = DO_IMAGE;
	    return;
	}

	/* Quartz seems to tile images at pixel-aligned regions only -- this
	 * leads to seams if the image doesn't end up scaling to fill the
	 * space exactly.  The CGPattern tiling approach doesn't have this
	 * problem.  Check if we're going to fill up the space (within some
	 * epsilon), and if not, fall back to the CGPattern type.
	 */

	xform = CGAffineTransformConcat (CGContextGetCTM (context),
					 state->transform);

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

	    state->imageRect = srcRect;
            state->action = DO_TILED_IMAGE;
            return;
	}

	/* Fall through to generic SURFACE case */
    }

    CGFloat patternAlpha = 1.0f;
    CGColorSpaceRef patternSpace;
    CGPatternRef pattern;
    cairo_int_status_t status;

    status = _cairo_quartz_cairo_repeating_surface_pattern_to_quartz (surface, source, &pattern);
    if (status == CAIRO_INT_STATUS_NOTHING_TO_DO) {
        state->action = DO_NOTHING;
	return;
    }
    if (status) {
	state->action = DO_UNSUPPORTED;
	return;
    }

    patternSpace = CGColorSpaceCreatePattern (NULL);
    CGContextSetFillColorSpace (context, patternSpace);
    CGContextSetFillPattern (context, pattern, &patternAlpha);
    CGContextSetStrokeColorSpace (context, patternSpace); 
    CGContextSetStrokePattern (context, pattern, &patternAlpha);
    CGColorSpaceRelease (patternSpace);

    /* Quartz likes to munge the pattern phase (as yet unexplained
     * why); force it to 0,0 as we've already baked in the correct
     * pattern translation into the pattern matrix
     */
    CGContextSetPatternPhase (context, CGSizeMake(0,0));

    state->pattern = pattern;
    state->action = DO_PATTERN;
    return;
}

/**
 * Call this before any operation that can modify the contents of a
 * cairo_quartz_surface_t.
 */
static void
_cairo_quartz_surface_will_change (cairo_quartz_surface_t *surface)
{
    if (surface->bitmapContextImage) {
        CGImageRelease (surface->bitmapContextImage);
        surface->bitmapContextImage = NULL;
    }
}

/**
 * Sets up internal state to be used to draw the source mask, stored in
 * cairo_quartz_state_t. Guarantees to call CGContextSaveGState on
 * surface->cgContext.
 */
static cairo_quartz_drawing_state_t
_cairo_quartz_setup_state (cairo_quartz_surface_t *surface,
			   const cairo_pattern_t *source,
			   cairo_operator_t op,
			   cairo_rectangle_int_t *extents)
{
    CGContextRef context = surface->cgContext;
    cairo_quartz_drawing_state_t state;
    cairo_bool_t bounded_op = _cairo_operator_bounded_by_mask (op);
    cairo_status_t status;

    state.context = context;
    state.image = NULL;
    state.imageSurface = NULL;
    state.layer = NULL;
    state.shading = NULL;
    state.pattern = NULL;
    state.unboundedLayer = NULL;

    _cairo_quartz_surface_will_change (surface);

    // Save before we change the pattern, colorspace, etc. so that
    // we can restore and make sure that quartz releases our
    // pattern (which may be stack allocated)
    CGContextSaveGState(context);

    status = _cairo_quartz_surface_set_cairo_operator (surface, op);
    if (status == CAIRO_INT_STATUS_NOTHING_TO_DO) {
        state.action = DO_NOTHING;
        return state;
    }
    if (status) {
        state.action = DO_UNSUPPORTED;
        return state;
    }

    if (!bounded_op) {
        state.unboundedDestination = context;

        CGRect clipBox = CGContextGetClipBoundingBox (context);
        CGRect clipBoxRound = CGRectIntegral (CGRectInset (clipBox, -1, -1));
        state.unboundedLayer = CGLayerCreateWithContext (context, clipBoxRound.size, NULL);
        if (!state.unboundedLayer) {
            state.action = DO_UNSUPPORTED;
            return state;
        }

        context = CGLayerGetContext (state.unboundedLayer);
        if (!context) {
            state.action = DO_UNSUPPORTED;
            return state;
        }
        state.context = context;
        // No need to save state here, since this context won't be used later
        CGContextTranslateCTM (context, -clipBoxRound.origin.x, -clipBoxRound.origin.y);

        state.unboundedLayerOffset = clipBoxRound.origin;

        CGContextSetCompositeOperation (context, kPrivateCGCompositeCopy);
    }

    CGContextSetInterpolationQuality (context, _cairo_quartz_filter_to_quartz (source->filter));

    if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) source;

	CGContextSetRGBStrokeColor (context,
				    solid->color.red,
				    solid->color.green,
				    solid->color.blue,
				    solid->color.alpha);
	CGContextSetRGBFillColor (context,
				  solid->color.red,
				  solid->color.green,
				  solid->color.blue,
				  solid->color.alpha);

        state.action = DO_SOLID;
        return state;
    }

    if (source->type == CAIRO_PATTERN_TYPE_LINEAR) {
	const cairo_linear_pattern_t *lpat = (const cairo_linear_pattern_t *)source;
	_cairo_quartz_setup_linear_source (surface, lpat, extents, &state);
	return state;
    }

    if (source->type == CAIRO_PATTERN_TYPE_RADIAL) {
	const cairo_radial_pattern_t *rpat = (const cairo_radial_pattern_t *)source;
	_cairo_quartz_setup_radial_source (surface, rpat, extents, &state);
	return state;
    }

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	const cairo_surface_pattern_t *spat = (const cairo_surface_pattern_t *) source;
        _cairo_quartz_setup_surface_source (surface, spat, extents, &state);
        return state;
    }

    state.action = DO_UNSUPPORTED;
    return state;
}

/**
 * 1) Tears down internal state used to draw the source
 * 2) Does CGContextRestoreGState on the state saved by _cairo_quartz_setup_state
 */
static void
_cairo_quartz_teardown_state (cairo_quartz_drawing_state_t *state)
{
    if (state->image) {
	CGImageRelease (state->image);
    }

    if (state->imageSurface) {
	cairo_surface_destroy (state->imageSurface);
    }

    if (state->shading) {
	CGShadingRelease (state->shading);
    }

    if (state->pattern) {
	CGPatternRelease (state->pattern);
    }

    if (state->unboundedLayer) {
        // Copy the layer back to the destination
        CGContextDrawLayerAtPoint (state->unboundedDestination,
                                   state->unboundedLayerOffset,
                                   state->unboundedLayer);
        CGContextRestoreGState (state->unboundedDestination);
        CGLayerRelease (state->unboundedLayer);
    } else {
        CGContextRestoreGState (state->context);
    }
}


static void
_cairo_quartz_draw_image (cairo_quartz_drawing_state_t *state)
{
    assert (state &&
            ((state->image && (state->action == DO_IMAGE || state->action == DO_TILED_IMAGE)) ||
             (state->layer && state->action == DO_LAYER)));

    CGContextConcatCTM (state->context, state->transform);
    CGContextTranslateCTM (state->context, 0, state->imageRect.size.height);
    CGContextScaleCTM (state->context, 1, -1);

    if (state->action == DO_TILED_IMAGE) {
	CGContextDrawTiledImagePtr (state->context, state->imageRect, state->image);
	/* no need to worry about unbounded operators, since tiled images
	   fill the entire clip region */
    } else {
        if (state->action == DO_LAYER) {
            /* Note that according to Apple docs it's completely legal
             * to draw a CGLayer to any CGContext, even one it wasn't
             * created for.
             */
            CGContextDrawLayerAtPoint (state->context, state->imageRect.origin,
                                       state->layer);
        } else {
            CGContextDrawImage (state->context, state->imageRect, state->image);
        }
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

    CGContextFlush(surface->cgContext);

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
    _cairo_surface_clipper_reset (&surface->clipper);

    CGContextRelease (surface->cgContext);

    surface->cgContext = NULL;

    if (surface->bitmapContextImage) {
        CGImageRelease (surface->bitmapContextImage);
        surface->bitmapContextImage = NULL;
    }

    if (surface->imageSurfaceEquiv) {
	_cairo_image_surface_assume_ownership_of_data (surface->imageSurfaceEquiv);
	cairo_surface_destroy (surface->imageSurfaceEquiv);
	surface->imageSurfaceEquiv = NULL;
    } else if (surface->imageData) {
        free (surface->imageData);
    }

    surface->imageData = NULL;

    if (surface->cgLayer) {
        CGLayerRelease (surface->cgLayer);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_surface_acquire_image (void *abstract_surface,
                                     cairo_image_surface_t **image_out,
                                     void **image_extra)
{
    cairo_int_status_t status;
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    *image_extra = NULL;

    /* ND((stderr, "%p _cairo_quartz_surface_acquire_image\n", surface)); */

    status = _cairo_quartz_get_image (surface, image_out);

    if (status == CAIRO_INT_STATUS_UNSUPPORTED && surface->cgLayer) {
        /* copy the layer into a Quartz bitmap context so we can get the data */
        cairo_surface_t *tmp =
            cairo_quartz_surface_create (CAIRO_FORMAT_ARGB32,
                                         surface->extents.width,
                                         surface->extents.height);
        cairo_quartz_surface_t *tmp_surface = (cairo_quartz_surface_t *) tmp;

        /* if surface creation failed, we won't have a Quartz surface here */
        if (cairo_surface_get_type (tmp) == CAIRO_SURFACE_TYPE_QUARTZ &&
            tmp_surface->imageSurfaceEquiv) {
            CGContextSaveGState (tmp_surface->cgContext);
            CGContextTranslateCTM (tmp_surface->cgContext, 0, surface->extents.height);
            CGContextScaleCTM (tmp_surface->cgContext, 1, -1);
            /* Note that according to Apple docs it's completely legal
             * to draw a CGLayer to any CGContext, even one it wasn't
             * created for.
             */
            CGContextDrawLayerAtPoint (tmp_surface->cgContext,
                                       CGPointMake (0.0, 0.0),
                                       surface->cgLayer);
            CGContextRestoreGState (tmp_surface->cgContext);

            *image_out = (cairo_image_surface_t*)
                cairo_surface_reference(tmp_surface->imageSurfaceEquiv);
            *image_extra = tmp;
            status = CAIRO_STATUS_SUCCESS;
        } else {
            cairo_surface_destroy (tmp);
        }
    }

    if (status)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_quartz_surface_release_source_image (void *abstract_surface,
					     cairo_image_surface_t *image,
					     void *image_extra)
{
    cairo_surface_destroy ((cairo_surface_t *) image);

    if (image_extra) {
        cairo_surface_destroy ((cairo_surface_t *) image_extra);
    }
}


static cairo_status_t
_cairo_quartz_surface_acquire_dest_image (void *abstract_surface,
					  cairo_rectangle_int_t *interest_rect,
					  cairo_image_surface_t **image_out,
					  cairo_rectangle_int_t *image_rect,
					  void **image_extra)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    ND((stderr, "%p _cairo_quartz_surface_acquire_dest_image\n", surface));

    *image_rect = surface->extents;
    *image_extra = NULL;

    _cairo_quartz_surface_will_change (surface);

    return _cairo_quartz_surface_acquire_image (abstract_surface,
        image_out, image_extra);
}

static void
_cairo_quartz_surface_release_dest_image (void *abstract_surface,
					  cairo_rectangle_int_t *interest_rect,
					  cairo_image_surface_t *image,
					  cairo_rectangle_int_t *image_rect,
					  void *image_extra)
{
    /* ND((stderr, "%p _cairo_quartz_surface_release_dest_image\n", surface)); */

    cairo_surface_destroy ((cairo_surface_t *) image);

    if (image_extra) {
        /* we need to write the data from the temp surface back to the layer */
        cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
        cairo_quartz_surface_t *tmp_surface = (cairo_quartz_surface_t *) image_extra;
        CGImageRef img;
        cairo_status_t status = _cairo_surface_to_cgimage (&tmp_surface->base, &img);
        if (status) {
            cairo_surface_destroy (&tmp_surface->base);
            return;
        }

        CGContextSaveGState (surface->cgContext);
        CGContextTranslateCTM (surface->cgContext, 0, surface->extents.height);
        CGContextScaleCTM (surface->cgContext, 1, -1);
        CGContextDrawImage (surface->cgContext,
                            CGRectMake (0.0, 0.0, surface->extents.width, surface->extents.height),
                            img);
        CGContextRestoreGState (surface->cgContext);

        cairo_surface_destroy (&tmp_surface->base);
    }
}

static cairo_surface_t *
_cairo_quartz_surface_create_similar (void *abstract_surface,
				       cairo_content_t content,
				       int width,
				       int height)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_format_t format;

    if (surface->cgLayer)
        return cairo_quartz_surface_create_cg_layer (abstract_surface, width, height);

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
	return _cairo_surface_create_in_error (_cairo_error
					       (CAIRO_STATUS_INVALID_SIZE));
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
				     int             *clone_offset_x,
				     int             *clone_offset_y,
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
	*clone_offset_x = 0;
	*clone_offset_y = 0;
	return CAIRO_STATUS_SUCCESS;
    }

    if (src->backend->type == CAIRO_SURFACE_TYPE_QUARTZ) {
	cairo_quartz_surface_t *qsurf = (cairo_quartz_surface_t *) src;

	if (IS_EMPTY(qsurf)) {
	    *clone_out = (cairo_surface_t*)
		_cairo_quartz_surface_create_internal (NULL, CAIRO_CONTENT_COLOR_ALPHA,
						       qsurf->extents.width, qsurf->extents.height);
	    *clone_offset_x = 0;
	    *clone_offset_y = 0;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    status = _cairo_surface_to_cgimage (src, &quartz_image);
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
    *clone_offset_x = src_x;
    *clone_offset_y = src_y;
    *clone_out = (cairo_surface_t*) new_surface;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_quartz_surface_get_extents (void *abstract_surface,
				   cairo_rectangle_int_t *extents)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    *extents = surface->extents;
    return TRUE;
}

static cairo_int_status_t
_cairo_quartz_surface_paint_internal (cairo_quartz_surface_t *surface,
			              cairo_quartz_drawing_state_t *state)
{
    if (state->action == DO_SOLID || state->action == DO_PATTERN) {
	CGContextFillRect (state->context, CGRectMake(surface->extents.x,
						      surface->extents.y,
						      surface->extents.width,
						      surface->extents.height));
    } else if (state->action == DO_SHADING) {
	CGContextConcatCTM (state->context, state->transform);
	CGContextDrawShading (state->context, state->shading);
    } else if (state->action == DO_IMAGE || state->action == DO_TILED_IMAGE ||
               state->action == DO_LAYER) {
	_cairo_quartz_draw_image (state);
    } else if (state->action != DO_NOTHING) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartz_surface_paint (void *abstract_surface,
			     cairo_operator_t op,
			     const cairo_pattern_t *source,
			     cairo_clip_t *clip)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartz_drawing_state_t state;

    ND((stderr, "%p _cairo_quartz_surface_paint op %d source->type %d\n", surface, op, source->type));

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    rv = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (rv))
	return rv;

    state = _cairo_quartz_setup_state (surface, source, op, NULL);

    rv = _cairo_quartz_surface_paint_internal (surface, &state);

    _cairo_quartz_teardown_state (&state);

    ND((stderr, "-- paint\n"));
    return rv;
}

static cairo_bool_t
_cairo_quartz_source_needs_extents (const cairo_pattern_t *source)
{
    /* For repeating gradients we need to manually extend the gradient and
       repeat stops, since Quartz doesn't support repeating gradients natively.
       We need to minimze the number of repeated stops, and since rasterization
       depends on the number of repetitions we use (even if some of the
       repetitions go beyond the extents of the object or outside the clip
       region), it's important to use the same number of repetitions when
       rendering an object no matter what the clip region is. So the
       computation of the repetition count cannot depended on the clip region,
       and should only depend on the object extents, so we need to compute
       the object extents for repeating gradients. */
    return (source->type == CAIRO_PATTERN_TYPE_LINEAR ||
            source->type == CAIRO_PATTERN_TYPE_RADIAL) &&
           (source->extend == CAIRO_EXTEND_REPEAT ||
            source->extend == CAIRO_EXTEND_REFLECT);
}

static cairo_int_status_t
_cairo_quartz_surface_fill (void *abstract_surface,
			     cairo_operator_t op,
			     const cairo_pattern_t *source,
			     cairo_path_fixed_t *path,
			     cairo_fill_rule_t fill_rule,
			     double tolerance,
			     cairo_antialias_t antialias,
			     cairo_clip_t *clip)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartz_drawing_state_t state;
    quartz_stroke_t stroke;

    ND((stderr, "%p _cairo_quartz_surface_fill op %d source->type %d\n", surface, op, source->type));

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    rv = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (rv))
	return rv;

    if (_cairo_quartz_source_needs_extents (source))
    {
        /* We don't need precise extents since these are only used to
           compute the number of gradient reptitions needed to cover the
           object. */
        cairo_rectangle_int_t path_extents;
        _cairo_path_fixed_approximate_fill_extents (path, &path_extents);
        state = _cairo_quartz_setup_state (surface, source, op, &path_extents);
    } else {
        state = _cairo_quartz_setup_state (surface, source, op, NULL);
    }

    CGContextSetShouldAntialias (state.context, (antialias != CAIRO_ANTIALIAS_NONE));

    CGContextBeginPath (state.context);

    stroke.cgContext = state.context;
    stroke.ctm_inverse = NULL;
    rv = _cairo_quartz_cairo_path_to_quartz_context (path, &stroke);
    if (rv)
        goto BAIL;

    if (state.action == DO_SOLID || state.action == DO_PATTERN) {
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextFillPath (state.context);
	else
	    CGContextEOFillPath (state.context);
    } else if (state.action == DO_SHADING) {

	// we have to clip and then paint the shading; we can't fill
	// with the shading
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextClip (state.context);
	else
            CGContextEOClip (state.context);

	CGContextConcatCTM (state.context, state.transform);
	CGContextDrawShading (state.context, state.shading);
    } else if (state.action == DO_IMAGE || state.action == DO_TILED_IMAGE ||
               state.action == DO_LAYER) {
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextClip (state.context);
	else
	    CGContextEOClip (state.context);

	_cairo_quartz_draw_image (&state);
    } else if (state.action != DO_NOTHING) {
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
    }

  BAIL:
    _cairo_quartz_teardown_state (&state);

    ND((stderr, "-- fill\n"));
    return rv;
}

static cairo_int_status_t
_cairo_quartz_surface_stroke (void *abstract_surface,
			      cairo_operator_t op,
			      const cairo_pattern_t *source,
			      cairo_path_fixed_t *path,
			      cairo_stroke_style_t *style,
			      cairo_matrix_t *ctm,
			      cairo_matrix_t *ctm_inverse,
			      double tolerance,
			      cairo_antialias_t antialias,
			      cairo_clip_t *clip)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartz_drawing_state_t state;
    quartz_stroke_t stroke;
    CGAffineTransform origCTM, strokeTransform;

    ND((stderr, "%p _cairo_quartz_surface_stroke op %d source->type %d\n", surface, op, source->type));

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    rv = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (rv))
	return rv;

    if (_cairo_quartz_source_needs_extents (source))
    {
        cairo_rectangle_int_t path_extents;
        _cairo_path_fixed_approximate_stroke_extents (path, style, ctm, &path_extents);
        state = _cairo_quartz_setup_state (surface, source, op, &path_extents);
    } else {
        state = _cairo_quartz_setup_state (surface, source, op, NULL);
    }

    // Turning antialiasing off used to cause misrendering with
    // single-pixel lines (e.g. 20,10.5 -> 21,10.5 end up being rendered as 2 pixels).
    // That's been since fixed in at least 10.5, and in the latest 10.4 dot releases.
    CGContextSetShouldAntialias (state.context, (antialias != CAIRO_ANTIALIAS_NONE));
    CGContextSetLineWidth (state.context, style->line_width);
    CGContextSetLineCap (state.context, _cairo_quartz_cairo_line_cap_to_quartz (style->line_cap));
    CGContextSetLineJoin (state.context, _cairo_quartz_cairo_line_join_to_quartz (style->line_join));
    CGContextSetMiterLimit (state.context, style->miter_limit);

    origCTM = CGContextGetCTM (state.context);

    if (style->dash && style->num_dashes) {
#define STATIC_DASH 32
	CGFloat sdash[STATIC_DASH];
	CGFloat *fdash = sdash;
	double offset = style->dash_offset;
	unsigned int max_dashes = style->num_dashes;
	unsigned int k;

	if (_cairo_stroke_style_dash_can_approximate (style, ctm, tolerance)) {
	    double approximate_dashes[2];
	    _cairo_stroke_style_dash_approximate (style, ctm, tolerance,
						  &offset,
						  approximate_dashes,
						  &max_dashes);
	    sdash[0] = approximate_dashes[0];
	    sdash[1] = approximate_dashes[1];
	} else {
	    if (style->num_dashes%2)
		max_dashes *= 2;
	    if (max_dashes > STATIC_DASH)
		fdash = _cairo_malloc_ab (max_dashes, sizeof (CGFloat));
	    if (fdash == NULL)
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	    for (k = 0; k < max_dashes; k++)
		fdash[k] = (CGFloat) style->dash[k % style->num_dashes];
	}
	CGContextSetLineDash (state.context, offset, fdash, max_dashes);
	if (fdash != sdash)
	    free (fdash);
    } else
	CGContextSetLineDash (state.context, 0, NULL, 0);

    _cairo_quartz_cairo_matrix_to_quartz (ctm, &strokeTransform);
    CGContextConcatCTM (state.context, strokeTransform);

    CGContextBeginPath (state.context);

    stroke.cgContext = state.context;
    stroke.ctm_inverse = ctm_inverse;
    rv = _cairo_quartz_cairo_path_to_quartz_context (path, &stroke);
    if (rv)
	goto BAIL;

    if (state.action == DO_SOLID || state.action == DO_PATTERN) {
	CGContextStrokePath (state.context);
    } else if (state.action == DO_IMAGE || state.action == DO_TILED_IMAGE ||
               state.action == DO_LAYER) {
	CGContextReplacePathWithStrokedPath (state.context);
	CGContextClip (state.context);

	CGContextSetCTM (state.context, origCTM);
	_cairo_quartz_draw_image (&state);
    } else if (state.action == DO_SHADING) {
	CGContextReplacePathWithStrokedPath (state.context);
	CGContextClip (state.context);

	CGContextSetCTM (state.context, origCTM);

	CGContextConcatCTM (state.context, state.transform);
	CGContextDrawShading (state.context, state.shading);
    } else if (state.action != DO_NOTHING) {
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL;
    }

  BAIL:
    _cairo_quartz_teardown_state (&state);

    ND((stderr, "-- stroke\n"));
    return rv;
}

#if CAIRO_HAS_QUARTZ_FONT
static cairo_int_status_t
_cairo_quartz_surface_show_glyphs (void *abstract_surface,
				   cairo_operator_t op,
				   const cairo_pattern_t *source,
				   cairo_glyph_t *glyphs,
				   int num_glyphs,
				   cairo_scaled_font_t *scaled_font,
				   cairo_clip_t *clip,
				   int *remaining_glyphs)
{
    CGAffineTransform textTransform, ctm;
#define STATIC_BUF_SIZE 64
    CGGlyph glyphs_static[STATIC_BUF_SIZE];
    CGSize cg_advances_static[STATIC_BUF_SIZE];
    CGGlyph *cg_glyphs = &glyphs_static[0];
    CGSize *cg_advances = &cg_advances_static[0];

    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartz_drawing_state_t state;
    float xprev, yprev;
    int i;
    CGFontRef cgfref = NULL;

    cairo_bool_t isClipping = FALSE;
    cairo_bool_t didForceFontSmoothing = FALSE;

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    if (num_glyphs <= 0)
	return CAIRO_STATUS_SUCCESS;

    if (cairo_scaled_font_get_type (scaled_font) != CAIRO_FONT_TYPE_QUARTZ)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    rv = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (rv))
	return rv;

    if (_cairo_quartz_source_needs_extents (source))
    {
        cairo_rectangle_int_t glyph_extents;
        _cairo_scaled_font_glyph_device_extents (scaled_font, glyphs, num_glyphs,
                                                 &glyph_extents, NULL);
        state = _cairo_quartz_setup_state (surface, source, op, &glyph_extents);
    } else {
        state = _cairo_quartz_setup_state (surface, source, op, NULL);
    }

    if (state.action == DO_SOLID || state.action == DO_PATTERN) {
	CGContextSetTextDrawingMode (state.context, kCGTextFill);
    } else if (state.action == DO_IMAGE || state.action == DO_TILED_IMAGE ||
               state.action == DO_SHADING || state.action == DO_LAYER) {
	CGContextSetTextDrawingMode (state.context, kCGTextClip);
	isClipping = TRUE;
    } else {
	if (state.action != DO_NOTHING)
	    rv = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL;
    }

    /* this doesn't addref */
    cgfref = _cairo_quartz_scaled_font_get_cg_font_ref (scaled_font);
    CGContextSetFont (state.context, cgfref);
    CGContextSetFontSize (state.context, 1.0);

    switch (scaled_font->options.antialias) {
	case CAIRO_ANTIALIAS_SUBPIXEL:
	    CGContextSetShouldAntialias (state.context, TRUE);
	    CGContextSetShouldSmoothFonts (state.context, TRUE);
	    if (CGContextSetAllowsFontSmoothingPtr &&
		!CGContextGetAllowsFontSmoothingPtr (state.context))
	    {
		didForceFontSmoothing = TRUE;
		CGContextSetAllowsFontSmoothingPtr (state.context, TRUE);
	    }
	    break;
	case CAIRO_ANTIALIAS_NONE:
	    CGContextSetShouldAntialias (state.context, FALSE);
	    break;
	case CAIRO_ANTIALIAS_GRAY:
	    CGContextSetShouldAntialias (state.context, TRUE);
	    CGContextSetShouldSmoothFonts (state.context, FALSE);
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

    CGContextSetTextMatrix (state.context, textTransform);

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
    ctm = CGContextGetCTM (state.context);
    CGContextTranslateCTM (state.context, glyphs[0].x, glyphs[0].y);

    CGContextShowGlyphsWithAdvances (state.context,
				     cg_glyphs,
				     cg_advances,
				     num_glyphs);

    CGContextSetCTM (state.context, ctm);

    if (state.action == DO_IMAGE || state.action == DO_TILED_IMAGE ||
        state.action == DO_LAYER) {
	_cairo_quartz_draw_image (&state);
    } else if (state.action == DO_SHADING) {
	CGContextConcatCTM (state.context, state.transform);
	CGContextDrawShading (state.context, state.shading);
    }

BAIL:
    if (didForceFontSmoothing)
        CGContextSetAllowsFontSmoothingPtr (state.context, FALSE);

    _cairo_quartz_teardown_state (&state);

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
                                         const cairo_pattern_t *source,
                                         const cairo_surface_pattern_t *mask,
					 cairo_clip_t *clip)
{
    CGRect rect;
    CGImageRef img;
    cairo_surface_t *pat_surf = mask->surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    CGAffineTransform ctm, mask_matrix;
    cairo_quartz_drawing_state_t state;

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_surface_to_cgimage (pat_surf, &img);
    if (status)
	return status;
    if (img == NULL) {
	if (!_cairo_operator_bounded_by_mask (op))
	    CGContextClearRect (surface->cgContext, CGContextGetClipBoundingBox (surface->cgContext));
	return CAIRO_STATUS_SUCCESS;
    }

    rect = CGRectMake (0.0f, 0.0f, CGImageGetWidth (img) , CGImageGetHeight (img));

    state = _cairo_quartz_setup_state (surface, source, op, NULL);

    /* ClipToMask is essentially drawing an image, so we need to flip the CTM
     * to get the image to appear oriented the right way */
    ctm = CGContextGetCTM (state.context);

    _cairo_quartz_cairo_matrix_to_quartz (&mask->base.matrix, &mask_matrix);
    mask_matrix = CGAffineTransformInvert(mask_matrix);
    mask_matrix = CGAffineTransformTranslate (mask_matrix, 0.0, CGImageGetHeight (img));
    mask_matrix = CGAffineTransformScale (mask_matrix, 1.0, -1.0);

    CGContextConcatCTM (state.context, mask_matrix);
    CGContextClipToMaskPtr (state.context, rect, img);

    CGContextSetCTM (state.context, ctm);

    status = _cairo_quartz_surface_paint_internal (surface, &state);

    _cairo_quartz_teardown_state (&state);

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
					 const cairo_pattern_t *source,
					 const cairo_pattern_t *mask,
					 cairo_clip_t *clip)
{
    int width = surface->extents.width;
    int height = surface->extents.height;

    cairo_surface_t *gradient_surf = NULL;
    cairo_surface_pattern_t surface_pattern;
    cairo_int_status_t status;

    /* Render the gradient to a surface */
    gradient_surf = cairo_quartz_surface_create (CAIRO_FORMAT_A8,
						 width,
						 height);

    status = _cairo_quartz_surface_paint (gradient_surf, CAIRO_OPERATOR_SOURCE, mask, NULL);
    if (status)
	goto BAIL;

    _cairo_pattern_init_for_surface (&surface_pattern, gradient_surf);

    status = _cairo_quartz_surface_mask_with_surface (surface, op, source, &surface_pattern, clip);

    _cairo_pattern_fini (&surface_pattern.base);

  BAIL:
    if (gradient_surf)
	cairo_surface_destroy (gradient_surf);

    return status;
}

static cairo_int_status_t
_cairo_quartz_surface_mask (void *abstract_surface,
			    cairo_operator_t op,
			    const cairo_pattern_t *source,
			    const cairo_pattern_t *mask,
			    cairo_clip_t *clip)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;

    ND((stderr, "%p _cairo_quartz_surface_mask op %d source->type %d mask->type %d\n", surface, op, source->type, mask->type));

    if (IS_EMPTY(surface))
	return CAIRO_STATUS_SUCCESS;

    rv = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (rv))
	return rv;

    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	/* This is easy; we just need to paint with the alpha. */
	cairo_solid_pattern_t *solid_mask = (cairo_solid_pattern_t *) mask;

	CGContextSetAlpha (surface->cgContext, solid_mask->color.alpha);
	rv = _cairo_quartz_surface_paint (surface, op, source, clip);
	CGContextSetAlpha (surface->cgContext, 1.0);

	return rv;
    }

    /* If we have CGContextClipToMask, we can do more complex masks */
    if (CGContextClipToMaskPtr) {
	/* For these, we can skip creating a temporary surface, since we already have one */
	if (mask->type == CAIRO_PATTERN_TYPE_SURFACE && mask->extend == CAIRO_EXTEND_NONE)
	    return _cairo_quartz_surface_mask_with_surface (surface, op, source, (cairo_surface_pattern_t *) mask, clip);

	return _cairo_quartz_surface_mask_with_generic (surface, op, source, mask, clip);
    }

    /* So, CGContextClipToMask is not present in 10.3.9, so we're
     * doomed; if we have imageData, we can do fallback, otherwise
     * just pretend success.
     */
    if (surface->imageData)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_surface_clipper_intersect_clip_path (cairo_surface_clipper_t *clipper,
						   cairo_path_fixed_t *path,
						   cairo_fill_rule_t fill_rule,
						   double tolerance,
						   cairo_antialias_t antialias)
{
    cairo_quartz_surface_t *surface =
	cairo_container_of (clipper, cairo_quartz_surface_t, clipper);
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
    _cairo_quartz_surface_acquire_image,
    _cairo_quartz_surface_release_source_image,
    _cairo_quartz_surface_acquire_dest_image,
    _cairo_quartz_surface_release_dest_image,
    _cairo_quartz_surface_clone_similar,
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
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

    _cairo_surface_clipper_init (&surface->clipper,
				 _cairo_quartz_surface_clipper_intersect_clip_path);

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
    surface->bitmapContextImage = NULL;
    surface->cgLayer = NULL;

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
 * cairo_quartz_cglayer_surface_create_similar
 * @surface: The returned surface can be efficiently drawn into this
 * destination surface (if tiling is not used)."
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a Quartz surface backed by a CGLayer, if the given surface
 * is a Quartz surface; the CGLayer is created to match the surface's
 * Quartz context. Otherwise just calls cairo_surface_create_similar
 * with CAIRO_CONTENT_COLOR_ALPHA.
 * The returned surface can be efficiently blitted to the given surface,
 * but tiling and 'extend' modes other than NONE are not so efficient.
 *
 * Return value: the newly created surface.
 *
 * Since: 1.10
 **/
cairo_surface_t *
cairo_quartz_surface_create_cg_layer (cairo_surface_t *surface,
                                      unsigned int width,
                                      unsigned int height)
{
    cairo_quartz_surface_t *surf;
    CGLayerRef layer;
    CGContextRef ctx;
    CGContextRef cgContext;

    cgContext = cairo_quartz_surface_get_cg_context (surface);
    if (!cgContext)
        return cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR_ALPHA,
                                             width, height);

    if (!_cairo_quartz_verify_surface_size(width, height))
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

    /* If we pass zero width or height into CGLayerCreateWithContext below,
     * it will fail.
     */
    if (width == 0 || height == 0) {
        return (cairo_surface_t*)
            _cairo_quartz_surface_create_internal (NULL, CAIRO_CONTENT_COLOR_ALPHA,
                                                   width, height);
    }

    layer = CGLayerCreateWithContext (cgContext,
                                      CGSizeMake (width, height),
                                      NULL);
    if (!layer)
      return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    ctx = CGLayerGetContext (layer);
    /* Flip it when we draw into it, so that when we finally composite it
     * to a flipped target, the directions match and Quartz will optimize
     * the composition properly
     */
    CGContextTranslateCTM (ctx, 0, height);
    CGContextScaleCTM (ctx, 1, -1);

    CGContextRetain (ctx);
    surf = _cairo_quartz_surface_create_internal (ctx, CAIRO_CONTENT_COLOR_ALPHA,
              width, height);
    if (surf->base.status) {
        CGLayerRelease (layer);
        // create_internal will have set an error
        return (cairo_surface_t*) surf;
    }
    surf->cgLayer = layer;

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
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

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
	stride = width * 4;
    } else if (format == CAIRO_FORMAT_A8) {
	cgColorspace = NULL;
	stride = width;
	bitinfo = kCGImageAlphaOnly;
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

    /* The Apple docs say that for best performance, the stride and the data
     * pointer should be 16-byte aligned.  malloc already aligns to 16-bytes,
     * so we don't have to anything special on allocation.
     */
    stride = (stride + 15) & ~15;

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

CGContextRef
cairo_quartz_get_cg_context_with_clip (cairo_t *cr)
{

    cairo_surface_t *surface = cr->gstate->target;
    cairo_clip_t *clip = &cr->gstate->clip;
    cairo_status_t status;

    cairo_quartz_surface_t *quartz = (cairo_quartz_surface_t*)surface;

    if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_QUARTZ)
	return NULL;

    if (!clip->path) {
	if (clip->all_clipped) {
	    /* Save the state before we set an empty clip rect so that
	     * our previous clip will be restored */
	    CGContextSaveGState (quartz->cgContext);

	    /* _cairo_surface_clipper_set_clip doesn't deal with
	     * clip->all_clipped because drawing is normally discarded earlier */
	    CGRect empty = {{0,0}, {0,0}};
	    CGContextClipToRect (quartz->cgContext, empty);

	    return quartz->cgContext;
	}

	/* an empty clip is represented by NULL */
	clip = NULL;
    }

    status = _cairo_surface_clipper_set_clip (&quartz->clipper, clip);

    /* Save the state after we set the clip so that it persists
     * after we restore */
    CGContextSaveGState (quartz->cgContext);

    if (unlikely (status))
	return NULL;

    return quartz->cgContext;
}

void
cairo_quartz_finish_cg_context_with_clip (cairo_t *cr)
{
    cairo_surface_t *surface = cr->gstate->target;

    cairo_quartz_surface_t *quartz = (cairo_quartz_surface_t*)surface;

    if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_QUARTZ)
	return;

    CGContextRestoreGState (quartz->cgContext);
}

cairo_surface_t *
cairo_quartz_surface_get_image (cairo_surface_t *surface)
{
    cairo_quartz_surface_t *quartz = (cairo_quartz_surface_t *)surface;
    cairo_image_surface_t *image;

    if (_cairo_quartz_get_image(quartz, &image))
        return NULL;

    return (cairo_surface_t *)image;
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
