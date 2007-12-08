/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006, 2007 Mozilla Corporation
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

/* The 10.5 SDK includes a funky new definition of FloatToFixed which
 * causes all sorts of breakage; so reset to old-style definition
 */
#ifdef FloatToFixed
#undef FloatToFixed
#define FloatToFixed(a)     ((Fixed)((float)(a) * fixed1))
#endif

#include <Carbon/Carbon.h>
#include <limits.h>

#undef QUARTZ_DEBUG

#ifdef QUARTZ_DEBUG
#define ND(_x)	fprintf _x
#else
#define ND(_x)	do {} while(0)
#endif

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

/* missing in 10.3.9 */
extern void CGContextClipToMask (CGContextRef, CGRect, CGImageRef) __attribute__((weak_import));

/*
 * Utility functions
 */

static void quartz_surface_to_png (cairo_quartz_surface_t *nq, char *dest);
static void quartz_image_to_png (CGImageRef, char *dest);

/* CoreGraphics limitation with flipped CTM surfaces: height must be less than signed 16-bit max */

#define CG_MAX_HEIGHT   SHRT_MAX
#define CG_MAX_WIDTH    USHRT_MAX

/* is the desired size of the surface within bounds? */
static cairo_bool_t verify_surface_size(int width, int height)
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

static CGLineCap
_cairo_quartz_cairo_line_cap_to_quartz (cairo_line_cap_t ccap)
{
    switch (ccap) {
	case CAIRO_LINE_CAP_BUTT: return kCGLineCapButt; break;
	case CAIRO_LINE_CAP_ROUND: return kCGLineCapRound; break;
	case CAIRO_LINE_CAP_SQUARE: return kCGLineCapSquare; break;
    }

    return kCGLineCapButt;
}

static CGLineJoin
_cairo_quartz_cairo_line_join_to_quartz (cairo_line_join_t cjoin)
{
    switch (cjoin) {
	case CAIRO_LINE_JOIN_MITER: return kCGLineJoinMiter; break;
	case CAIRO_LINE_JOIN_ROUND: return kCGLineJoinRound; break;
	case CAIRO_LINE_JOIN_BEVEL: return kCGLineJoinBevel; break;
    }

    return kCGLineJoinMiter;
}

static void
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
ComputeGradientValue (void *info, const float *in, float *out)
{
    float fdist = *in; /* 0.0 .. 1.0 */
    cairo_fixed_t fdist_fix = _cairo_fixed_from_double(*in);
    cairo_gradient_pattern_t *grad = (cairo_gradient_pattern_t*) info;
    unsigned int i;

    for (i = 0; i < grad->n_stops; i++) {
	if (grad->stops[i].x > fdist_fix)
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
	float ax = _cairo_fixed_to_double(grad->stops[i-1].x);
	float bx = _cairo_fixed_to_double(grad->stops[i].x) - ax;
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
    static const float input_value_range[2] = { 0.f, 1.f };
    static const float output_value_ranges[8] = { 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f };
    static const CGFunctionCallbacks callbacks = {
	0, ComputeGradientValue, (CGFunctionReleaseInfoCallback) cairo_pattern_destroy
    };

    return CGFunctionCreate (gpat,
			     1,
			     input_value_range,
			     4,
			     output_value_ranges,
			     &callbacks);
}

static cairo_int_status_t
_cairo_quartz_cairo_gradient_pattern_to_quartz (cairo_pattern_t *abspat,
						CGShadingRef *shading)
{
    cairo_matrix_t mat;
    double x0, y0;

    if (abspat->type != CAIRO_PATTERN_TYPE_LINEAR &&
	abspat->type != CAIRO_PATTERN_TYPE_RADIAL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* bandaid for mozilla bug 379321, also visible in the
     * linear-gradient-reflect test. 
     */
    if (abspat->extend == CAIRO_EXTEND_REFLECT ||
	abspat->extend == CAIRO_EXTEND_REPEAT)
	return CAIRO_INT_STATUS_UNSUPPORTED;
	
    /* We can only do this if we have an identity pattern matrix;
     * otherwise fall back through to the generic pattern case.
     * XXXperf we could optimize this by creating a pattern with the shading;
     * but we'd need to know the extents to do that.
     * ... but we don't care; we can use the surface extents for it
     * XXXtodo - implement gradients with non-identity pattern matrices
     */
    cairo_pattern_get_matrix (abspat, &mat);
    if (mat.xx != 1.0 || mat.yy != 1.0 || mat.xy != 0.0 || mat.yx != 0.0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    x0 = mat.x0;
    y0 = mat.y0;

    if (abspat->type == CAIRO_PATTERN_TYPE_LINEAR) {
	cairo_linear_pattern_t *lpat = (cairo_linear_pattern_t*) abspat;
	CGPoint start, end;
	CGFunctionRef gradFunc;
	CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
	bool extend = abspat->extend == CAIRO_EXTEND_PAD;

	start = CGPointMake (_cairo_fixed_to_double (lpat->p1.x) - x0,
			     _cairo_fixed_to_double (lpat->p1.y) - y0);
	end = CGPointMake (_cairo_fixed_to_double (lpat->p2.x) - x0,
			   _cairo_fixed_to_double (lpat->p2.y) - y0);

	cairo_pattern_reference (abspat);
	gradFunc = CreateGradientFunction ((cairo_gradient_pattern_t*) lpat);
	*shading = CGShadingCreateAxial (rgb,
					start, end,
					gradFunc,
					extend, extend);
	CGColorSpaceRelease(rgb);
	CGFunctionRelease(gradFunc);

	return CAIRO_STATUS_SUCCESS;
    }

    if (abspat->type == CAIRO_PATTERN_TYPE_RADIAL) {
	cairo_radial_pattern_t *rpat = (cairo_radial_pattern_t*) abspat;
	CGPoint start, end;
	CGFunctionRef gradFunc;
	CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
	bool extend = abspat->extend == CAIRO_EXTEND_PAD;

	start = CGPointMake (_cairo_fixed_to_double (rpat->c1.x) - x0,
			     _cairo_fixed_to_double (rpat->c1.y) - y0);
	end = CGPointMake (_cairo_fixed_to_double (rpat->c2.x) - x0,
			   _cairo_fixed_to_double (rpat->c2.y) - y0);

	cairo_pattern_reference (abspat);
	gradFunc = CreateGradientFunction ((cairo_gradient_pattern_t*) rpat);
	*shading = CGShadingCreateRadial (rgb,
					 start,
					 _cairo_fixed_to_double (rpat->r1),
					 end,
					 _cairo_fixed_to_double (rpat->r2),
					 gradFunc,
					 extend, extend);
	CGColorSpaceRelease(rgb);
	CGFunctionRelease(gradFunc);

	return CAIRO_STATUS_SUCCESS;
    }

    /* Shouldn't be reached */
    ASSERT_NOT_REACHED;
    return CAIRO_STATUS_SUCCESS;
}

/* generic cairo surface -> cairo_quartz_surface_t function */
static cairo_int_status_t
_cairo_quartz_surface_to_quartz (cairo_surface_t *target,
				 cairo_surface_t *pat_surf,
				 cairo_quartz_surface_t **quartz_surf)
{

    if (cairo_surface_get_type(pat_surf) != CAIRO_SURFACE_TYPE_QUARTZ) {
	/* XXXtodo/perf don't use clone if the source surface is an image surface!  Instead,
	 * just create the CGImage directly!
	 */

	cairo_surface_t *ref_type = target;
	cairo_surface_t *new_surf = NULL;
	cairo_rectangle_int_t rect;
	cairo_status_t status;

	if (ref_type == NULL)
	    ref_type = cairo_quartz_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);

	status = _cairo_surface_get_extents (pat_surf, &rect);
	if (status)
	    return status;

	status = _cairo_surface_clone_similar (ref_type, pat_surf, rect.x, rect.y,
				      rect.width, rect.height, &new_surf);
	if (target == NULL)
	    cairo_surface_destroy(ref_type);

        if (status)
	    return status;

 	if (new_surf &&
	    cairo_surface_get_type (new_surf) != CAIRO_SURFACE_TYPE_QUARTZ)
	{
	    ND((stderr, "got a non-quartz surface, format=%d width=%u height=%u type=%d\n", cairo_surface_get_type (pat_surf), rect.width, rect.height, cairo_surface_get_type (new_surf)));
	    cairo_surface_destroy (new_surf);
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	*quartz_surf = (cairo_quartz_surface_t *) new_surf;
    } else {
	/* If it's a quartz surface, we can try to see if it's a CGBitmapContext;
	 * we do this when we call CGBitmapContextCreateImage below.
	 */
	cairo_surface_reference (pat_surf);
	*quartz_surf = (cairo_quartz_surface_t*) pat_surf;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Generic cairo_pattern -> CGPattern function */
static void
SurfacePatternDrawFunc (void *info, CGContextRef context)
{
    cairo_surface_pattern_t *spat = (cairo_surface_pattern_t *) info;
    cairo_surface_t *pat_surf = spat->surface;
    cairo_int_status_t status;

    cairo_quartz_surface_t *quartz_surf;
    CGImageRef img;
    CGRect imageBounds;

    status = _cairo_quartz_surface_to_quartz (NULL, pat_surf, &quartz_surf);
    if (status)
	return;

    img = CGBitmapContextCreateImage (quartz_surf->cgContext);
    if (!img) {
	// ... give up.
	ND((stderr, "CGBitmapContextCreateImage failed\n"));
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	cairo_surface_destroy ((cairo_surface_t*)quartz_surf);
	return;
    }

    /* XXXtodo WHY does this need to be flipped?  Writing this stuff
     * to disk shows that in both this path and the path above the source image
     * has an identical orientation, and the destination context at all times has a Y
     * flip.  So why do we need to flip in this case?
     */
    if (cairo_surface_get_type(pat_surf) == CAIRO_SURFACE_TYPE_QUARTZ) {
	CGContextTranslateCTM (context, 0, CGImageGetHeight(img));
	CGContextScaleCTM (context, 1, -1);
    }

    imageBounds.size = CGSizeMake (CGImageGetWidth(img), CGImageGetHeight(img));
    imageBounds.origin.x = 0;
    imageBounds.origin.y = 0;

    CGContextDrawImage (context, imageBounds, img);
    if (spat->base.extend == CAIRO_EXTEND_REFLECT) {
	/* draw 3 more copies of the image, flipped. */
	CGContextTranslateCTM (context, 0, 2 * imageBounds.size.height);
	CGContextScaleCTM (context, 1, -1);
	CGContextDrawImage (context, imageBounds, img);
	CGContextTranslateCTM (context, 2 * imageBounds.size.width, 0);
	CGContextScaleCTM (context, -1, 1);
	CGContextDrawImage (context, imageBounds, img);
	CGContextTranslateCTM (context, 0, 2 * imageBounds.size.height);
	CGContextScaleCTM (context, 1, -1);
	CGContextDrawImage (context, imageBounds, img);
    }

    CGImageRelease (img);

    cairo_surface_destroy ((cairo_surface_t*) quartz_surf);
}

/* Borrowed from cairo-meta-surface */
static cairo_status_t
_init_pattern_with_snapshot (cairo_pattern_t *pattern,
			     const cairo_pattern_t *other)
{
    cairo_status_t status;

    status = _cairo_pattern_init_copy (pattern, other);
    if (status)
	return status;

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *surface_pattern =
	    (cairo_surface_pattern_t *) pattern;
	cairo_surface_t *surface = surface_pattern->surface;

	surface_pattern->surface = _cairo_surface_snapshot (surface);

	cairo_surface_destroy (surface);

	if (surface_pattern->surface->status)
	    return surface_pattern->surface->status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartz_cairo_repeating_surface_pattern_to_quartz (cairo_quartz_surface_t *dest,
							 cairo_pattern_t *abspat,
							 CGPatternRef *cgpat)
{
    cairo_surface_pattern_t *spat;
    cairo_surface_t *pat_surf;
    cairo_rectangle_int_t extents;

    CGRect pbounds;
    CGAffineTransform ptransform, stransform;
    CGPatternCallbacks cb = { 0,
			      SurfacePatternDrawFunc,
			      (CGFunctionReleaseInfoCallback) cairo_pattern_destroy };
    float rw, rh;
    cairo_status_t status;

    cairo_pattern_union_t *snap_pattern = NULL;
    cairo_pattern_t *target_pattern = abspat;

    cairo_matrix_t m;
    /* SURFACE is the only type we'll handle here */
    if (abspat->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    spat = (cairo_surface_pattern_t *) abspat;
    pat_surf = spat->surface;

    status = _cairo_surface_get_extents (pat_surf, &extents);
    if (status)
	return status;

    pbounds.origin.x = 0;
    pbounds.origin.y = 0;

    // kjs seems to indicate this should work (setting to 0,0 to avoid
    // tiling); however, the pattern CTM scaling ends up being NaN in
    // the pattern draw function if either rw or rh are 0.
    // XXXtodo get pattern drawing working with extend options
    // XXXtodo/perf optimize CAIRO_EXTEND_NONE to a single DrawImage instead of a pattern
    if (spat->base.extend == CAIRO_EXTEND_REFLECT) {
	/* XXX broken; need to emulate by reflecting the image into 4 quadrants
	 * and then tiling that
	 */
	pbounds.size.width = 2 * extents.width;
	pbounds.size.height = 2 * extents.height;
    } else {
	pbounds.size.width = extents.width;
	pbounds.size.height = extents.height;
    }
    rw = pbounds.size.width;
    rh = pbounds.size.height;

    m = spat->base.matrix;
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


    /* XXX fixme: only do snapshots if the context is for printing, or get rid of the
       other block if it doesn't fafect performance */
    if (1 /* context is for printing */) {
	snap_pattern = (cairo_pattern_union_t*) malloc(sizeof(cairo_pattern_union_t));
	target_pattern = (cairo_pattern_t*) snap_pattern;
	_init_pattern_with_snapshot (target_pattern, abspat);
    } else {
	cairo_pattern_reference (abspat);
	target_pattern = abspat;
    }

    *cgpat = CGPatternCreate (target_pattern,
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
    DO_UNSUPPORTED
} cairo_quartz_action_t;

static cairo_quartz_action_t
_cairo_quartz_setup_source (cairo_quartz_surface_t *surface,
			    cairo_pattern_t *source)
{
    assert (!(surface->sourceImage || surface->sourceShading || surface->sourcePattern));

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
    } else if (source->type == CAIRO_PATTERN_TYPE_LINEAR ||
	       source->type == CAIRO_PATTERN_TYPE_RADIAL)
    {
	CGShadingRef shading = NULL;
	cairo_int_status_t status;

	status = _cairo_quartz_cairo_gradient_pattern_to_quartz (source, &shading);
	if (status)
	    return DO_UNSUPPORTED;

	surface->sourceShading = shading;

	return DO_SHADING;
    } else if (source->type == CAIRO_PATTERN_TYPE_SURFACE &&
	       source->extend == CAIRO_EXTEND_NONE)
    {
	    cairo_surface_pattern_t *spat = (cairo_surface_pattern_t *) source;
	    cairo_surface_t *pat_surf = spat->surface;
	    cairo_quartz_surface_t *quartz_surf;
	    CGImageRef img;
	    cairo_matrix_t m = spat->base.matrix;
	    cairo_rectangle_int_t extents;
	    cairo_status_t status;

	    status = _cairo_quartz_surface_to_quartz ((cairo_surface_t *) surface, pat_surf, &quartz_surf);
	    if (status)
		return DO_UNSUPPORTED;

	    img = CGBitmapContextCreateImage (quartz_surf->cgContext);
	    if (!img)
		return DO_UNSUPPORTED;

	    surface->sourceImage = img;

	    cairo_matrix_invert(&m);
	    _cairo_quartz_cairo_matrix_to_quartz (&m, &surface->sourceImageTransform);

	    status = _cairo_surface_get_extents (pat_surf, &extents);
	    if (status)
		return DO_UNSUPPORTED;

	    surface->sourceImageRect = CGRectMake (0, 0, extents.width, extents.height);

	    surface->sourceImageSurface = (cairo_surface_t *)quartz_surf;

	    return DO_IMAGE;
    } else if (source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	float patternAlpha = 1.0f;
	CGColorSpaceRef patternSpace;
	CGPatternRef pattern;
	cairo_int_status_t status;

	status = _cairo_quartz_cairo_repeating_surface_pattern_to_quartz (surface, source, &pattern);
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
    } else {
	return DO_UNSUPPORTED;
    }

    ASSERT_NOT_REACHED;
}

static void
_cairo_quartz_teardown_source (cairo_quartz_surface_t *surface,
				cairo_pattern_t *source)
{
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

static void
ImageDataReleaseFunc(void *info, const void *data, size_t size)
{
    if (data != NULL) {
	free((void *) data);
    }
}

/* Read the image from the surface's front buffer */
static cairo_int_status_t
_cairo_quartz_get_image (cairo_quartz_surface_t *surface,
			  cairo_image_surface_t **image_out,
			  unsigned char **data_out)
{
    unsigned char *imageData;
    cairo_image_surface_t *isurf;

    if (CGBitmapContextGetBitsPerPixel(surface->cgContext) != 0) {
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

    /* Restore our saved gstate that we use to reset clipping */
    CGContextRestoreGState (surface->cgContext);

    CGContextRelease (surface->cgContext);

    surface->cgContext = NULL;

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

    *image_extra = NULL;

    status = _cairo_quartz_get_image (surface, image_out, NULL);
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
    unsigned char *data;

    ND((stderr, "%p _cairo_quartz_surface_acquire_dest_image\n", surface));

    *image_rect = surface->extents;

    status = _cairo_quartz_get_image (surface, image_out, &data);
    if (status)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    *image_extra = data;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_quartz_surface_release_dest_image (void *abstract_surface,
					  cairo_rectangle_int_t *interest_rect,
					  cairo_image_surface_t *image,
					  cairo_rectangle_int_t *image_rect,
					  void *image_extra)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    unsigned char *imageData = (unsigned char *) image_extra;

    //ND((stderr, "%p _cairo_quartz_surface_release_dest_image\n", surface));

    if (!CGBitmapContextGetData (surface->cgContext)) {
	CGDataProviderRef dataProvider;
	CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
	CGImageRef img;

	dataProvider = CGDataProviderCreateWithData (NULL, imageData,
						     surface->extents.width * surface->extents.height * 4,
						     ImageDataReleaseFunc);

	img = CGImageCreate (surface->extents.width, surface->extents.height,
			     8, 32,
			     surface->extents.width * 4,
			     rgb,
			     kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
			     dataProvider,
			     NULL,
			     false,
			     kCGRenderingIntentDefault);
	CGColorSpaceRelease (rgb);

	CGContextSetCompositeOperation (surface->cgContext, kPrivateCGCompositeCopy);

	CGContextDrawImage (surface->cgContext,
			    CGRectMake (0, 0, surface->extents.width, surface->extents.height),
			    img);

	CGImageRelease (img);
	CGDataProviderRelease (dataProvider);

	ND((stderr, "Image for surface %p was recovered from a bitmap\n", surface));
    }

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
    if (!verify_surface_size(width, height)) {
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
    
    *clone_out = NULL;

    // verify width and height of surface
    if (!verify_surface_size(width, height)) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    CGImageRef quartz_image = NULL;

    if (cairo_surface_get_type(src) == CAIRO_SURFACE_TYPE_QUARTZ) {
	cairo_quartz_surface_t *qsurf = (cairo_quartz_surface_t *) src;
	quartz_image = CGBitmapContextCreateImage (qsurf->cgContext);
	new_format = CAIRO_FORMAT_ARGB32;  /* XXX bogus; recover a real format from the image */
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *isurf = (cairo_image_surface_t *) src;
	CGDataProviderRef dataProvider;
	CGColorSpaceRef cgColorspace;
	CGBitmapInfo bitinfo;
	int bitsPerComponent, bitsPerPixel;

	if (isurf->format == CAIRO_FORMAT_ARGB32) {
	    cgColorspace = CGColorSpaceCreateDeviceRGB();
	    bitinfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
	    bitsPerComponent = 8;
	    bitsPerPixel = 32;
	} else if (isurf->format == CAIRO_FORMAT_RGB24) {
	    cgColorspace = CGColorSpaceCreateDeviceRGB();
	    bitinfo = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
	    bitsPerComponent = 8;
	    bitsPerPixel = 32;
	} else if (isurf->format == CAIRO_FORMAT_A8) {
	    cgColorspace = CGColorSpaceCreateDeviceGray();
	    bitinfo = kCGImageAlphaNone;
	    bitsPerComponent = 8;
	    bitsPerPixel = 8;
	} else {
	    /* SUPPORT A1, maybe */
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	new_format = isurf->format;

	dataProvider = CGDataProviderCreateWithData (NULL,
						     isurf->data,
						     isurf->height * isurf->stride,
						     NULL);

	quartz_image = CGImageCreate (isurf->width, isurf->height,
				      bitsPerComponent,
				      bitsPerPixel,
				      isurf->stride,
				      cgColorspace,
				      bitinfo,
				      dataProvider,
				      NULL,
				      false,
				      kCGRenderingIntentDefault);
	CGDataProviderRelease (dataProvider);
	CGColorSpaceRelease (cgColorspace);
    } else {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (!quartz_image)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    new_surface = (cairo_quartz_surface_t *)
	cairo_quartz_surface_create (new_format,
				     CGImageGetWidth (quartz_image),
				     CGImageGetHeight (quartz_image));
    if (!new_surface || new_surface->base.status) {
	CGImageRelease (quartz_image);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    CGContextSetCompositeOperation (new_surface->cgContext,
				    kPrivateCGCompositeCopy);

    quartz_image_to_png (quartz_image, NULL);

    CGContextDrawImage (new_surface->cgContext,
			CGRectMake (src_x, src_y, width, height),
			quartz_image);
    CGImageRelease (quartz_image);

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
	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    } else if (action == DO_IMAGE) {
	cairo_surface_pattern_t *surface_pattern =
	    (cairo_surface_pattern_t *) source;
	cairo_surface_t *pat_surf = surface_pattern->surface;
	CGContextSaveGState (surface->cgContext);
	CGContextConcatCTM (surface->cgContext, surface->sourceImageTransform);
	if (cairo_surface_get_type(pat_surf) == CAIRO_SURFACE_TYPE_QUARTZ) {
	    CGContextTranslateCTM (surface->cgContext, 0, CGImageGetHeight(surface->sourceImage));
	    CGContextScaleCTM (surface->cgContext, 1, -1);
	}

	CGContextDrawImage (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
	CGContextRestoreGState (surface->cgContext);
    } else {
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

    ND((stderr, "%p _cairo_quartz_surface_fill op %d source->type %d\n", surface, op, source->type));

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    CGContextSaveGState (surface->cgContext);

    CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));
    CGContextSetCompositeOperation (surface->cgContext, _cairo_quartz_cairo_operator_to_quartz (op));

    action = _cairo_quartz_setup_source (surface, source);
    if (action == DO_UNSUPPORTED) {
	CGContextRestoreGState (surface->cgContext);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    CGContextBeginPath (surface->cgContext);

    stroke.cgContext = surface->cgContext;
    stroke.ctm_inverse = NULL;
    rv = _cairo_quartz_cairo_path_to_quartz_context (path, &stroke);
    if (rv)
        goto BAIL;

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

	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    } else if (action == DO_IMAGE) {
	cairo_surface_pattern_t *surface_pattern =
	    (cairo_surface_pattern_t *) source;
	cairo_surface_t *pat_surf = surface_pattern->surface;
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextClip (surface->cgContext);
	else
	    CGContextEOClip (surface->cgContext);
	CGContextConcatCTM (surface->cgContext, surface->sourceImageTransform);
	if (cairo_surface_get_type(pat_surf) == CAIRO_SURFACE_TYPE_QUARTZ) {
	    CGContextTranslateCTM (surface->cgContext, 0, CGImageGetHeight(surface->sourceImage));
	    CGContextScaleCTM (surface->cgContext, 1, -1);
	}

	CGContextDrawImage (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
    } else {
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
    }

  BAIL:
    _cairo_quartz_teardown_source (surface, source);

    CGContextRestoreGState (surface->cgContext);

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
    CGAffineTransform strokeTransform;

    ND((stderr, "%p _cairo_quartz_surface_stroke op %d source->type %d\n", surface, op, source->type));

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    CGContextSaveGState (surface->cgContext);

    // Turning antialiasing off causes misrendering with
    // single-pixel lines (e.g. 20,10.5 -> 21,10.5 end up being rendered as 2 pixels)
    //CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));
    CGContextSetLineWidth (surface->cgContext, style->line_width);
    CGContextSetLineCap (surface->cgContext, _cairo_quartz_cairo_line_cap_to_quartz (style->line_cap));
    CGContextSetLineJoin (surface->cgContext, _cairo_quartz_cairo_line_join_to_quartz (style->line_join));
    CGContextSetMiterLimit (surface->cgContext, style->miter_limit);
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
    if (action == DO_UNSUPPORTED) {
	CGContextRestoreGState (surface->cgContext);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    CGContextBeginPath (surface->cgContext);

    stroke.cgContext = surface->cgContext;
    stroke.ctm_inverse = ctm_inverse;
    rv = _cairo_quartz_cairo_path_to_quartz_context (path, &stroke);
    if (rv)
	goto BAIL;

    if (action == DO_SOLID || action == DO_PATTERN) {
	CGContextStrokePath (surface->cgContext);
    } else if (action == DO_IMAGE) {
	CGContextReplacePathWithStrokedPath (surface->cgContext);
	CGContextClip (surface->cgContext);

	CGContextConcatCTM (surface->cgContext, surface->sourceImageTransform);
	if (cairo_surface_get_type(((cairo_surface_pattern_t*)source)->surface) == CAIRO_SURFACE_TYPE_QUARTZ) {
	    CGContextTranslateCTM (surface->cgContext, 0, CGImageGetHeight(surface->sourceImage));
	    CGContextScaleCTM (surface->cgContext, 1, -1);
	}

	CGContextDrawImage (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
    } else if (action == DO_SHADING) {
	CGContextReplacePathWithStrokedPath (surface->cgContext);
	CGContextClip (surface->cgContext);

	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    } else {
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
    }

  BAIL:
    _cairo_quartz_teardown_source (surface, source);

    CGContextRestoreGState (surface->cgContext);

    ND((stderr, "-- stroke\n"));
    return rv;
}

#if CAIRO_HAS_ATSUI_FONT
static cairo_int_status_t
_cairo_quartz_surface_show_glyphs (void *abstract_surface,
				    cairo_operator_t op,
				    cairo_pattern_t *source,
				    cairo_glyph_t *glyphs,
				    int num_glyphs,
				    cairo_scaled_font_t *scaled_font)
{
    CGAffineTransform cairoTextTransform, textTransform, ctm;
    // XXXtodo/perf: stack storage for glyphs/sizes
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

    if (num_glyphs <= 0)
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    if (cairo_scaled_font_get_type (scaled_font) != CAIRO_FONT_TYPE_ATSUI)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    CGContextSaveGState (surface->cgContext);

    action = _cairo_quartz_setup_source (surface, source);
    if (action == DO_SOLID || action == DO_PATTERN) {
	CGContextSetTextDrawingMode (surface->cgContext, kCGTextFill);
    } else if (action == DO_IMAGE || action == DO_SHADING) {
	CGContextSetTextDrawingMode (surface->cgContext, kCGTextClip);
    } else {
	/* Unsupported */
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL;
    }

    CGContextSetCompositeOperation (surface->cgContext, _cairo_quartz_cairo_operator_to_quartz (op));

    CGFontRef cgfref = _cairo_atsui_scaled_font_get_cg_font_ref (scaled_font);
    CGContextSetFont (surface->cgContext, cgfref);

    /* So this should include the size; I don't know if I need to extract the
     * size from this and call CGContextSetFontSize.. will I get crappy hinting
     * with this 1.0 size business?  Or will CG just multiply that size into the
     * text matrix?
     */
    //ND((stderr, "show_glyphs: glyph 0 at: %f, %f\n", glyphs[0].x, glyphs[0].y));
    cairoTextTransform = CGAffineTransformMake (scaled_font->font_matrix.xx, 
						scaled_font->font_matrix.yx,
						scaled_font->font_matrix.xy, 
						scaled_font->font_matrix.yy,
						0., 0.);

    textTransform = CGAffineTransformMakeTranslation (glyphs[0].x, glyphs[0].y);
    textTransform = CGAffineTransformScale (textTransform, 1.0, -1.0);
    textTransform = CGAffineTransformConcat (cairoTextTransform, textTransform);

    ctm = CGAffineTransformMake (scaled_font->ctm.xx,
				 -scaled_font->ctm.yx,
				 -scaled_font->ctm.xy,
				 scaled_font->ctm.yy,
				 0., 0.);
    textTransform = CGAffineTransformConcat (ctm, textTransform);

    CGContextSetTextMatrix (surface->cgContext, textTransform);
    CGContextSetFontSize (surface->cgContext, 1.0);

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

    xprev = glyphs[0].x;
    yprev = glyphs[0].y;

    cg_glyphs[0] = glyphs[0].index;
    cg_advances[0].width = 0;
    cg_advances[0].height = 0;

    for (i = 1; i < num_glyphs; i++) {
	float xf = glyphs[i].x;
	float yf = glyphs[i].y;
	cg_glyphs[i] = glyphs[i].index;
	cg_advances[i-1].width = xf - xprev;
	cg_advances[i-1].height = yf - yprev;
	xprev = xf;
	yprev = yf;
    }

#if 0
    for (i = 0; i < num_glyphs; i++) {
	ND((stderr, "[%d: %d %f,%f]\n", i, cg_glyphs[i], cg_advances[i].width, cg_advances[i].height));
    }
#endif

    CGContextShowGlyphsWithAdvances (surface->cgContext,
				     cg_glyphs,
				     cg_advances,
				     num_glyphs);

    if (action == DO_IMAGE) {
	CGContextConcatCTM (surface->cgContext, surface->sourceImageTransform);
	if (cairo_surface_get_type(((cairo_surface_pattern_t*)source)->surface) == CAIRO_SURFACE_TYPE_QUARTZ) {
	    CGContextTranslateCTM (surface->cgContext, 0, CGImageGetHeight(surface->sourceImage));
	    CGContextScaleCTM (surface->cgContext, 1, -1);
	}

	CGContextDrawImage (surface->cgContext, surface->sourceImageRect, surface->sourceImage);
    } else if (action == DO_SHADING) {
	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    }

BAIL:
    if (cg_advances != &cg_advances_static[0]) {
	free (cg_advances);
    }

    if (cg_glyphs != &glyphs_static[0]) {
	free (cg_glyphs);
    }

    _cairo_quartz_teardown_source (surface, source);
    
    CGContextRestoreGState (surface->cgContext);

    return rv;
}
#endif /* CAIRO_HAS_ATSUI_FONT */

static cairo_int_status_t
_cairo_quartz_surface_mask_with_surface (cairo_quartz_surface_t *surface,
                                         cairo_operator_t op,
                                         cairo_pattern_t *source,
                                         cairo_surface_pattern_t *mask)
{
    cairo_rectangle_int16_t extents;
    cairo_quartz_surface_t *quartz_surf;
    CGRect rect;
    CGImageRef img;
    cairo_surface_t *pat_surf = mask->surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    status = _cairo_surface_get_extents (pat_surf, &extents);
    if (status)
	return status;

    status = _cairo_quartz_surface_to_quartz (NULL, pat_surf, &quartz_surf);
    if (status)
	return status;

    img = CGBitmapContextCreateImage (quartz_surf->cgContext);
    if (!img) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto BAIL;
    }

    rect = CGRectMake (-mask->base.matrix.x0, -mask->base.matrix.y0, extents.width, extents.height);
    CGContextSaveGState (surface->cgContext);
    CGContextClipToMask (surface->cgContext, rect, img);
    status = _cairo_quartz_surface_paint (surface, op, source);

    CGContextRestoreGState (surface->cgContext);
    CGImageRelease (img);
  BAIL:
    cairo_surface_destroy ((cairo_surface_t*) quartz_surf);
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

    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	/* This is easy; we just need to paint with the alpha. */
	cairo_solid_pattern_t *solid_mask = (cairo_solid_pattern_t *) mask;

	CGContextSetAlpha (surface->cgContext, solid_mask->color.alpha);
    } else if (CGContextClipToMask &&
               mask->type == CAIRO_PATTERN_TYPE_SURFACE &&
	       mask->extend == CAIRO_EXTEND_NONE) {
	return _cairo_quartz_surface_mask_with_surface (surface, op, source, (cairo_surface_pattern_t *) mask);
    } else {
	/* So, CGContextClipToMask is not present in 10.3.9, so we're
	 * doomed; if we have imageData, we can do fallback, otherwise
	 * just pretend success.
	 */
	if (surface->imageData)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	return CAIRO_STATUS_SUCCESS;
    }

    rv = _cairo_quartz_surface_paint (surface, op, source);

    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	CGContextSetAlpha (surface->cgContext, 1.0);
    }

    ND((stderr, "-- mask\n"));

    return rv;
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
#if CAIRO_HAS_ATSUI_FONT
    _cairo_quartz_surface_show_glyphs,
#else 
    NULL, /* surface_show_glyphs */
#endif /* CAIRO_HAS_ATSUI_FONT */

    NULL, /* snapshot */
    NULL, /* is_similar */
    NULL, /* reset */
    NULL  /* fill_stroke */
};

static cairo_quartz_surface_t *
_cairo_quartz_surface_create_internal (CGContextRef cgContext,
					cairo_content_t content,
					unsigned int width,
					unsigned int height)
{
    cairo_quartz_surface_t *surface;

    /* Init the base surface */
    surface = malloc(sizeof(cairo_quartz_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    memset(surface, 0, sizeof(cairo_quartz_surface_t));

    _cairo_surface_init(&surface->base, &cairo_quartz_surface_backend,
			content);

    /* Save our extents */
    surface->extents.x = surface->extents.y = 0;
    surface->extents.width = width;
    surface->extents.height = height;

    /* Save so we can always get back to a known-good CGContext -- this is
     * required for proper behaviour of intersect_clip_path(NULL)
     */
    CGContextSaveGState (cgContext);

    surface->cgContext = cgContext;
    surface->cgContextBaseCTM = CGContextGetCTM (cgContext);

    surface->imageData = NULL;

    return surface;
}
					 
/**
 * cairo_quartz_surface_create_for_cg_context
 * @cgContext: the existing CGContext for which to create the surface
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a Quartz surface that wraps the given CGContext.  The
 * CGContext is assumed to be in the QuickDraw coordinate space (that
 * is, with the origin at the upper left and the Y axis increasing
 * downward.)  If the CGContext is in the Quartz coordinate space (with
 * the origin at the bottom left), then it should be flipped before
 * this function is called:
 *
 * <informalexample><programlisting>
 * GContextTranslateCTM (cgContext, 0.0, height);
 * CGContextScaleCTM (cgContext, 1.0, -1.0);
 * </programlisting></informalexample>
 *
 * A very small number of Cairo operations cannot be translated to
 * Quartz operations; those operations will fail on this surface.
 * If all Cairo operations are required to succeed, consider rendering
 * to a surface created by cairo_quartz_surface_create() and then copying
 * the result to the CGContext.
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
    if (!surf) {
	CGContextRelease (cgContext);
	// create_internal will have set an error
	return (cairo_surface_t*) &_cairo_surface_nil;
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
    if (!verify_surface_size(width, height)) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    if (format == CAIRO_FORMAT_ARGB32) {
	cgColorspace = CGColorSpaceCreateDeviceRGB();
	stride = width * 4;
	bitinfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
	bitsPerComponent = 8;
    } else if (format == CAIRO_FORMAT_RGB24) {
	cgColorspace = CGColorSpaceCreateDeviceRGB();
	stride = width * 4;
	bitinfo = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
	bitsPerComponent = 8;
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
	_cairo_error (CAIRO_STATUS_INVALID_FORMAT);
	return (cairo_surface_t*) &_cairo_surface_nil;
    } else {
	_cairo_error (CAIRO_STATUS_INVALID_FORMAT);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    imageData = _cairo_malloc_ab (height, stride);
    if (!imageData) {
	CGColorSpaceRelease (cgColorspace);
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    cgc = CGBitmapContextCreate (imageData,
				 width,
				 height,
				 bitsPerComponent,
				 stride,
				 cgColorspace,
				 bitinfo);
    CGColorSpaceRelease (cgColorspace);

    if (!cgc) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	free (imageData);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    /* flip the Y axis */
    CGContextTranslateCTM (cgc, 0.0, height);
    CGContextScaleCTM (cgc, 1.0, -1.0);

    surf = _cairo_quartz_surface_create_internal (cgc, _cairo_content_from_format (format),
						   width, height);
    if (!surf) {
	CGContextRelease (cgc);
	free (imageData);
	// create_internal will have set an error
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    surf->imageData = imageData;

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
#endif

void
quartz_image_to_png (CGImageRef imgref, char *dest)
{
#if 0
    static int sctr = 0;
    char sptr[] = "/Users/vladimir/Desktop/barXXXXX.png";

    if (dest == NULL) {
	fprintf (stderr, "** Writing %p to bar%d\n", imgref, sctr);
	sprintf (sptr, "/Users/vladimir/Desktop/bar%d.png", sctr);
	sctr++;
	dest = sptr;
    }

    ExportCGImageToPNGFile(imgref, dest);
#endif
}

void
quartz_surface_to_png (cairo_quartz_surface_t *nq, char *dest)
{
#if 0
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
#endif
}

