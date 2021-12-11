/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2008 Opened Hand Ltd.
 * Copyright © 2009 Chris Wilson
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
 * http://www.mozilla.og/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * Contributor(s):
 *      Pierre Tardy      <tardyp@gmail.com>
 *      Øyvind Kolås      <pippin@gimp.org>
 *      Vladimi Vukicevic <vladimir@mozilla.com> (stubbed out base backend)
 *      Chris Wilson      <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-vg.h"

#include "cairo-cache-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-recording-surface-inline.h"
#include "cairo-surface-clipper-private.h"

#include <pixman.h>
#include <VG/openvg.h>

//#define OPENVG_DEBUG

/*
 * Work that needs to be done:
 *  - Glyph cache / proper font support
 *
 *  - First-class paths
 *    Paths are expensive for OpenVG, reuse paths whenever possible.
 *    So add a path cache, and first class paths!
 */

typedef struct _cairo_vg_surface cairo_vg_surface_t;

/* XXX need GL specific context control. :( */
struct _cairo_vg_context {
    cairo_status_t status;
    cairo_reference_count_t ref_count;

    unsigned long target_id;

    VGPaint		paint;
    cairo_vg_surface_t *source;
    double		alpha;

    cairo_cache_t snapshot_cache;

    void *display;
    void *context;

    cairo_status_t (*create_target) (cairo_vg_context_t *,
				     cairo_vg_surface_t *);
    cairo_status_t (*set_target) (cairo_vg_context_t *,
				  cairo_vg_surface_t *);
    void (*destroy_target) (cairo_vg_context_t *, cairo_vg_surface_t *);
};

struct _cairo_vg_surface {
    cairo_surface_t base;

    cairo_vg_context_t *context;

    VGImage	    image;
    VGImageFormat   format;
    int             width;
    int             height;
    cairo_bool_t    own_image;

    cairo_cache_entry_t snapshot_cache_entry;

    cairo_surface_clipper_t clipper;

    unsigned long target_id;
};

static const cairo_surface_backend_t cairo_vg_surface_backend;

slim_hidden_proto (cairo_vg_surface_create);

static cairo_surface_t *
_vg_surface_create_internal (cairo_vg_context_t *context,
			     VGImage image,
			     VGImageFormat format,
			     int width, int height);

static cairo_vg_context_t *
_vg_context_reference (cairo_vg_context_t *context)
{
    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&context->ref_count));

    _cairo_reference_count_inc (&context->ref_count);

    return context;
}

static cairo_vg_context_t *
_vg_context_lock (cairo_vg_context_t *context)
{
    /* XXX if we need to add locking, then it has to be recursive */
    return context;
}

static cairo_int_status_t
_vg_context_set_target (cairo_vg_context_t *context,
			cairo_vg_surface_t *surface)
{
    cairo_status_t status;

    if (surface->target_id == 0) {
	status = context->create_target (context, surface);
	if (unlikely (status))
	    return status;
    }

    if (context->target_id == surface->target_id)
	return CAIRO_STATUS_SUCCESS;

    context->target_id = surface->target_id;

    return context->set_target (context, surface);
}

static void
_vg_context_destroy_target (cairo_vg_context_t *context,
			    cairo_vg_surface_t *surface)
{
    if (surface->target_id == 0)
	return;

    if (context->target_id == surface->target_id)
	context->set_target (context, NULL);

    context->destroy_target (context, surface);
}

static cairo_bool_t
_vg_snapshot_cache_can_remove (const void *entry)
{
    return TRUE;
}

static void
_vg_snapshot_cache_remove (void *cache_entry)
{
    cairo_vg_surface_t *surface = cairo_container_of (cache_entry,
						      cairo_vg_surface_t,
						      snapshot_cache_entry);
    surface->snapshot_cache_entry.hash = 0;
    cairo_surface_destroy (&surface->base);
}

static cairo_status_t
_vg_context_init (cairo_vg_context_t *context)
{
    cairo_status_t status;

    context->status = CAIRO_STATUS_SUCCESS;
    CAIRO_REFERENCE_COUNT_INIT (&context->ref_count, 1);

    status = _cairo_cache_init (&context->snapshot_cache,
				NULL,
				_vg_snapshot_cache_can_remove,
				_vg_snapshot_cache_remove,
				16*1024*1024);
    if (unlikely (status))
	return status;

    context->target_id = 0;
    context->source = NULL;
    context->alpha = 1.0;

    context->paint = vgCreatePaint ();
    vgLoadIdentity ();

    return CAIRO_STATUS_SUCCESS;
}

static void
_vg_context_destroy (cairo_vg_context_t *context)
{
    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&context->ref_count));

    if (! _cairo_reference_count_dec_and_test (&context->ref_count))
	return;

    if (context->paint != VG_INVALID_HANDLE)
	vgDestroyPaint (context->paint);

    _cairo_cache_fini (&context->snapshot_cache);
    free (context);
}

static void
_vg_context_unlock (cairo_vg_context_t *context)
{
}

#ifdef OPENVG_DEBUG
static void check_vg_errors(const char*function,int line)
{
    int err = vgGetError();
    if (err != VG_NO_ERROR){
	printf("%s+%d:vgError detected: 0x%08x.\n",function, line,err);
	assert(err == VG_NO_ERROR);
    }

}
#define CHECK_VG_ERRORS() check_vg_errors(__FILE__,__LINE__)
#else
#define CHECK_VG_ERRORS() do{}while(0)
#endif //OPENVG_DEBUG

static pixman_format_code_t
_vg_format_to_pixman (VGImageFormat format,
		      cairo_bool_t *needs_premult_fixup)
{
    *needs_premult_fixup = FALSE;
    switch (format) {
	/* RGB{A,X} channel ordering */
    case VG_sRGBX_8888: return PIXMAN_r8g8b8x8;
    case VG_sRGBA_8888: *needs_premult_fixup = TRUE; return PIXMAN_r8g8b8a8;
    case VG_sRGBA_8888_PRE: return PIXMAN_r8g8b8a8;
    case VG_sRGB_565: return PIXMAN_r5g6b5;
    case VG_sRGBA_5551: return 0;
    case VG_sRGBA_4444: return 0;
    case VG_sL_8: return PIXMAN_g8;
    case VG_lRGBX_8888: return 0;
    case VG_lRGBA_8888: return 0;
    case VG_lRGBA_8888_PRE: return 0;
    case VG_lL_8: return 0;
    case VG_A_8: return PIXMAN_a8;
    case VG_BW_1: return PIXMAN_a1;
    case VG_A_1: return PIXMAN_a1;
    case VG_A_4: return PIXMAN_a4;

	/* {A,X}RGB channel ordering */
    case VG_sXRGB_8888: return PIXMAN_x8r8g8b8;
    case VG_sARGB_8888: *needs_premult_fixup = TRUE; return PIXMAN_a8r8g8b8;
    case VG_sARGB_8888_PRE: return PIXMAN_a8r8g8b8;
    case VG_sARGB_1555: return 0;
    case VG_sARGB_4444: return 0;
    case VG_lXRGB_8888: return 0;
    case VG_lARGB_8888: return 0;
    case VG_lARGB_8888_PRE: return 0;

	/* BGR{A,X} channel ordering */
    case VG_sBGRX_8888: return PIXMAN_b8g8r8x8;
    case VG_sBGRA_8888: *needs_premult_fixup = TRUE; return PIXMAN_b8g8r8a8;
    case VG_sBGRA_8888_PRE: return PIXMAN_b8g8r8a8;
    case VG_sBGR_565: return PIXMAN_b5g6r5;
    case VG_sBGRA_5551: return 0;
    case VG_sBGRA_4444: return 0;
    case VG_lBGRX_8888: return 0;
    case VG_lBGRA_8888: return 0;
    case VG_lBGRA_8888_PRE: return 0;

	/* {A,X}BGR channel ordering */
    case VG_sXBGR_8888: return PIXMAN_x8b8g8r8;
    case VG_sABGR_8888: *needs_premult_fixup = TRUE; return PIXMAN_a8b8g8r8;
    case VG_sABGR_8888_PRE: return PIXMAN_a8b8g8r8;
    case VG_sABGR_1555: return 0;
    case VG_sABGR_4444: return 0;
    case VG_lXBGR_8888: return 0;
    case VG_lABGR_8888: return 0;
    case VG_lABGR_8888_PRE: return 0;
    default: return 0;
    }
}

static pixman_format_code_t
_vg_format_to_content (VGImageFormat format)
{
    /* XXX could use more simple bit tests */
    switch (format) {
	/* RGB{A,X} channel ordering */
    case VG_sRGBX_8888: return CAIRO_CONTENT_COLOR;
    case VG_sRGBA_8888: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sRGBA_8888_PRE: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sRGB_565: return CAIRO_CONTENT_COLOR;
    case VG_sRGBA_5551: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sRGBA_4444: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sL_8: return CAIRO_CONTENT_ALPHA;
    case VG_lRGBX_8888: return CAIRO_CONTENT_COLOR;
    case VG_lRGBA_8888: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_lRGBA_8888_PRE: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_lL_8: return CAIRO_CONTENT_ALPHA;
    case VG_A_8: return CAIRO_CONTENT_ALPHA;
    case VG_A_4: return CAIRO_CONTENT_ALPHA;
    case VG_A_1: return CAIRO_CONTENT_ALPHA;
    case VG_BW_1: return CAIRO_CONTENT_ALPHA;

	/* {A,X}RGB channel ordering */
    case VG_sXRGB_8888: return CAIRO_CONTENT_COLOR;
    case VG_sARGB_8888: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sARGB_8888_PRE: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sARGB_1555: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sARGB_4444: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_lXRGB_8888: return CAIRO_CONTENT_COLOR;
    case VG_lARGB_8888: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_lARGB_8888_PRE: return CAIRO_CONTENT_COLOR_ALPHA;

	/* BGR{A,X} channel ordering */
    case VG_sBGRX_8888: return CAIRO_CONTENT_COLOR;
    case VG_sBGRA_8888: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sBGRA_8888_PRE: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sBGR_565: return CAIRO_CONTENT_COLOR;
    case VG_sBGRA_5551: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sBGRA_4444: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_lBGRX_8888: return CAIRO_CONTENT_COLOR;
    case VG_lBGRA_8888: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_lBGRA_8888_PRE: return CAIRO_CONTENT_COLOR_ALPHA;

	/* {A,X}BGR channel ordering */
    case VG_sXBGR_8888: return CAIRO_CONTENT_COLOR;
    case VG_sABGR_8888: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sABGR_8888_PRE: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sABGR_1555: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_sABGR_4444: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_lXBGR_8888: return CAIRO_CONTENT_COLOR;
    case VG_lABGR_8888: return CAIRO_CONTENT_COLOR_ALPHA;
    case VG_lABGR_8888_PRE: return CAIRO_CONTENT_COLOR_ALPHA;
    default: return 0;
    }
}

static VGImageFormat
_vg_format_from_pixman (pixman_format_code_t format)
{
    /* XXX _PRE needs fixup */
    switch ((int) format) {
    case PIXMAN_r5g6b5: return VG_sRGB_565;
    case PIXMAN_g8: return VG_sL_8;
    case PIXMAN_a8: return VG_A_8;
    case PIXMAN_a1: return VG_BW_1;
    case PIXMAN_x8r8g8b8: return VG_sXRGB_8888;
    case PIXMAN_a8r8g8b8: return VG_sARGB_8888; // _PRE
    case PIXMAN_b8g8r8x8: return VG_sBGRX_8888;
    case PIXMAN_b8g8r8a8: return VG_sBGRA_8888; // _PRE
    case PIXMAN_b5g6r5: return VG_sBGR_565;
    case PIXMAN_x8b8g8r8: return VG_sXBGR_8888;
    case PIXMAN_a8b8g8r8: return VG_sABGR_8888; // _PRE
    default: return 0;
    }
}

static VGImageFormat
_vg_format_for_content (cairo_content_t content)
{
    switch (content) {
    case CAIRO_CONTENT_ALPHA: return VG_A_8;
    case CAIRO_CONTENT_COLOR: return VG_sXRGB_8888;
    default: ASSERT_NOT_REACHED;
    case CAIRO_CONTENT_COLOR_ALPHA: return VG_sARGB_8888; // _PRE
    }
}

static cairo_surface_t *
_vg_surface_create_similar (void            *abstract_surface,
			    cairo_content_t  content,
			    int              width,
			    int              height)
{
    cairo_vg_surface_t *surface = abstract_surface;

    if (width > vgGeti (VG_MAX_IMAGE_WIDTH) ||
	height > vgGeti (VG_MAX_IMAGE_HEIGHT))
    {
	return NULL;
    }

    return cairo_vg_surface_create (surface->context, content, width, height);
}

static cairo_status_t
_vg_surface_clipper_intersect_clip_path (cairo_surface_clipper_t *clipper,
					 cairo_path_fixed_t *path,
					 cairo_fill_rule_t   fill_rule,
					 double              tolerance,
					 cairo_antialias_t   antialias)
{
    cairo_vg_surface_t *surface = cairo_container_of (clipper,
						      cairo_vg_surface_t,
						      clipper);
    cairo_vg_surface_t *mask;
    cairo_status_t status;

    if (path == NULL) {
	vgMask (VG_INVALID_HANDLE,
		VG_FILL_MASK, 0, 0, surface->width, surface->height);
	vgSeti (VG_MASKING, VG_FALSE);
	CHECK_VG_ERRORS();
	return CAIRO_STATUS_SUCCESS;
    }

    mask = (cairo_vg_surface_t *)
	_vg_surface_create_similar (surface, CAIRO_CONTENT_ALPHA,
				    surface->width, surface->height);
    if (unlikely (mask == NULL))
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (unlikely (mask->base.status))
	return mask->base.status;

    status = _cairo_surface_fill (&mask->base,
				  CAIRO_OPERATOR_SOURCE,
				  &_cairo_pattern_white.base,
				  path, fill_rule, tolerance, antialias,
				  NULL);
    if (status) {
	cairo_surface_destroy (&mask->base);
	return status;
    }

    vgSeti (VG_MASKING, VG_TRUE);
    vgMask (mask->image, VG_INTERSECT_MASK, 0, 0, mask->width, mask->height);

    cairo_surface_destroy (&mask->base);

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_vg_surface_get_extents (void                  *abstract_surface,
			 cairo_rectangle_int_t *extents)
{
    cairo_vg_surface_t *surface = abstract_surface;

    extents->x = 0;
    extents->y = 0;
    extents->width  = surface->width;
    extents->height = surface->height;

    return TRUE;
}

#define MAX_SEG  16  /* max number of knots to upload in a batch */

typedef struct _vg_path {
    VGPath path;
    const cairo_matrix_t *ctm_inverse;

    VGubyte gseg[MAX_SEG];
    VGfloat gdata[MAX_SEG*3*2];
    int dcount;
    int scount;
} vg_path_t;

static cairo_status_t
_vg_move_to (void          *closure,
	     const cairo_point_t *point)
{
    vg_path_t *path = closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (path->ctm_inverse)
	cairo_matrix_transform_point (path->ctm_inverse, &x, &y);

    path->gseg[path->scount++] = VG_MOVE_TO;
    path->gdata[path->dcount++] = x;
    path->gdata[path->dcount++] = y;

    if (path->scount >= MAX_SEG-1) {
	vgAppendPathData (path->path, path->scount, path->gseg, path->gdata);
	path->scount = 0;
	path->dcount = 0;
    }

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_vg_line_to (void          *closure,
	     const cairo_point_t *point)
{
    vg_path_t *path = closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (path->ctm_inverse)
	cairo_matrix_transform_point (path->ctm_inverse, &x, &y);

    path->gseg[path->scount++] = VG_LINE_TO;
    path->gdata[path->dcount++] = x;
    path->gdata[path->dcount++] = y;

    if (path->scount >= MAX_SEG-1) {
	vgAppendPathData (path->path, path->scount, path->gseg, path->gdata);
	path->scount = 0;
	path->dcount = 0;
    }

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_vg_curve_to (void          *closure,
	      const cairo_point_t *p0,
	      const cairo_point_t *p1,
	      const cairo_point_t *p2)
{
    vg_path_t *path = closure;
    double x0 = _cairo_fixed_to_double (p0->x);
    double y0 = _cairo_fixed_to_double (p0->y);
    double x1 = _cairo_fixed_to_double (p1->x);
    double y1 = _cairo_fixed_to_double (p1->y);
    double x2 = _cairo_fixed_to_double (p2->x);
    double y2 = _cairo_fixed_to_double (p2->y);

    if (path->ctm_inverse) {
	cairo_matrix_transform_point (path->ctm_inverse, &x0, &y0);
	cairo_matrix_transform_point (path->ctm_inverse, &x1, &y1);
	cairo_matrix_transform_point (path->ctm_inverse, &x2, &y2);
    }

    path->gseg[path->scount++] = VG_CUBIC_TO;
    path->gdata[path->dcount++] = x0;
    path->gdata[path->dcount++] = y0;
    path->gdata[path->dcount++] = x1;
    path->gdata[path->dcount++] = y1;
    path->gdata[path->dcount++] = x2;
    path->gdata[path->dcount++] = y2;

    if (path->scount >= MAX_SEG-1) {
	vgAppendPathData(path->path, path->scount, path->gseg, path->gdata);
	path->scount = 0;
	path->dcount = 0;
    }

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_vg_close_path (void *closure)
{
    vg_path_t *path = closure;

    path->gseg[path->scount++] = VG_CLOSE_PATH;

    if (path->scount >= MAX_SEG-1) {
	vgAppendPathData (path->path, path->scount, path->gseg, path->gdata);
	path->scount = 0;
	path->dcount = 0;
    }

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static void
_vg_path_from_cairo (vg_path_t    *vg_path,
		     const cairo_path_fixed_t *path)
{
    cairo_status_t status;

    vg_path->scount = 0;
    vg_path->dcount = 0;

    status = _cairo_path_fixed_interpret (path,
					  _vg_move_to,
					  _vg_line_to,
					  _vg_curve_to,
					  _vg_close_path,
					  vg_path);
    assert (status == CAIRO_STATUS_SUCCESS);

    vgAppendPathData (vg_path->path,
		      vg_path->scount, vg_path->gseg, vg_path->gdata);
    CHECK_VG_ERRORS();
}

static cairo_bool_t
_vg_is_supported_operator (cairo_operator_t op)
{
    switch ((int) op) {
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_ADD:
	return TRUE;

    default:
	return FALSE;
    }
}

static VGBlendMode
_vg_operator (cairo_operator_t op)
{
    switch ((int) op) {
    case CAIRO_OPERATOR_SOURCE:
	return VG_BLEND_SRC;
    case CAIRO_OPERATOR_OVER:
	return VG_BLEND_SRC_OVER;
    case CAIRO_OPERATOR_IN:
	return VG_BLEND_SRC_IN;
    case CAIRO_OPERATOR_DEST_OVER:
	return VG_BLEND_DST_OVER;
    case CAIRO_OPERATOR_DEST_IN:
	return VG_BLEND_DST_IN;
    case CAIRO_OPERATOR_ADD:
	return VG_BLEND_ADDITIVE;
    default:
	ASSERT_NOT_REACHED;
	return VG_BLEND_SRC_OVER;
    }
}

static VGFillRule
_vg_fill_rule_from_cairo (cairo_fill_rule_t rule)
{
    switch (rule) {
    case CAIRO_FILL_RULE_EVEN_ODD: return VG_EVEN_ODD;
    case CAIRO_FILL_RULE_WINDING: return VG_NON_ZERO;
    }

    ASSERT_NOT_REACHED;
    return VG_NON_ZERO;
}

static VGRenderingQuality
_vg_rendering_quality_from_cairo (cairo_antialias_t aa)
{
    switch (aa) {
    case CAIRO_ANTIALIAS_DEFAULT:
    case CAIRO_ANTIALIAS_SUBPIXEL:
    case CAIRO_ANTIALIAS_GOOD:
    case CAIRO_ANTIALIAS_BEST:
	return VG_RENDERING_QUALITY_BETTER;

    case CAIRO_ANTIALIAS_GRAY:
    case CAIRO_ANTIALIAS_FAST:
	return VG_RENDERING_QUALITY_FASTER;

    case CAIRO_ANTIALIAS_NONE:
	return VG_RENDERING_QUALITY_NONANTIALIASED;
    }

    ASSERT_NOT_REACHED;
    return VG_RENDERING_QUALITY_BETTER;
}

static VGCapStyle
_vg_line_cap_from_cairo (cairo_line_cap_t cap)
{
    switch (cap) {
    case CAIRO_LINE_CAP_BUTT:   return VG_CAP_BUTT;
    case CAIRO_LINE_CAP_ROUND:  return VG_CAP_ROUND;
    case CAIRO_LINE_CAP_SQUARE: return VG_CAP_SQUARE;
    }

    ASSERT_NOT_REACHED;
    return VG_CAP_BUTT;
}

static VGJoinStyle
_vg_line_join_from_cairo (cairo_line_join_t join)
{
    switch (join) {
    case CAIRO_LINE_JOIN_MITER: return VG_JOIN_MITER;
    case CAIRO_LINE_JOIN_ROUND: return VG_JOIN_ROUND;
    case CAIRO_LINE_JOIN_BEVEL: return VG_JOIN_BEVEL;
    }

    ASSERT_NOT_REACHED;
    return VG_JOIN_MITER;
}

static void
_vg_matrix_from_cairo (VGfloat *dst, const cairo_matrix_t *src)
{
    dst[0] = /* sx  */ src->xx;
    dst[1] = /* shy */ src->yx;
    dst[2] = /* w0  */ 0;
    dst[3] = /* shx */ src->xy;
    dst[4] = /* sy  */ src->yy;
    dst[5] = /* w1  */ 0;
    dst[6] = /* tx  */ src->x0;
    dst[7] = /* ty  */ src->y0;
    dst[8] = /* w2  */ 0;
}

static cairo_status_t
_vg_setup_gradient_stops (cairo_vg_context_t *context,
			  const cairo_gradient_pattern_t *pattern)
{
    VGint numstops = pattern->n_stops;
    VGfloat *stops, stack_stops[CAIRO_STACK_ARRAY_LENGTH (VGfloat)];
    int i;

    if (numstops*5 < ARRAY_LENGTH (stack_stops)) {
	stops = stack_stops;
    } else {
	stops = _cairo_malloc_ab (numstops, 5*sizeof (VGfloat));
	if (unlikely (stops == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < numstops; i++) {
	stops[i*5 + 0] = pattern->stops[i].offset;
	stops[i*5 + 1] = pattern->stops[i].color.red;
	stops[i*5 + 2] = pattern->stops[i].color.green;
	stops[i*5 + 3] = pattern->stops[i].color.blue;
	stops[i*5 + 4] = pattern->stops[i].color.alpha * context->alpha;
    }

    vgSetParameterfv (context->paint,
		      VG_PAINT_COLOR_RAMP_STOPS, numstops * 5, stops);

    if (stops != stack_stops)
	free (stops);

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static void
_vg_set_source_matrix (const cairo_pattern_t *pat)
{
    cairo_matrix_t mat;
    cairo_status_t status;
    VGfloat vmat[9];

    mat = pat->matrix;
    status = cairo_matrix_invert (&mat);
    assert (status == CAIRO_STATUS_SUCCESS);

    _vg_matrix_from_cairo (vmat, &mat);

    vgSeti (VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER);
    vgLoadMatrix (vmat);
    vgSeti (VG_MATRIX_MODE, VG_MATRIX_STROKE_PAINT_TO_USER);
    vgLoadMatrix (vmat);
    vgSeti (VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);

    CHECK_VG_ERRORS();
}

static cairo_status_t
_vg_setup_linear_source (cairo_vg_context_t *context,
			 const cairo_linear_pattern_t *lpat)
{
    VGfloat linear[4];

    linear[0] = lpat->pd1.x;
    linear[1] = lpat->pd1.y;
    linear[2] = lpat->pd2.x;
    linear[3] = lpat->pd2.y;

    vgSetParameteri (context->paint,
		     VG_PAINT_COLOR_RAMP_SPREAD_MODE,
		     VG_COLOR_RAMP_SPREAD_PAD);
    vgSetParameteri (context->paint,
		     VG_PAINT_TYPE,
		     VG_PAINT_TYPE_LINEAR_GRADIENT);
    vgSetParameterfv (context->paint,
		      VG_PAINT_LINEAR_GRADIENT, 4, linear);

    _vg_set_source_matrix (&lpat->base.base);

    CHECK_VG_ERRORS();
    return _vg_setup_gradient_stops (context, &lpat->base);

}

static cairo_status_t
_vg_setup_radial_source (cairo_vg_context_t *context,
			 const cairo_radial_pattern_t *rpat)
{
    VGfloat radial[5];

    radial[0] = rpat->cd1.center.x;
    radial[1] = rpat->cd1.center.y;
    radial[2] = rpat->cd2.center.x;
    radial[3] = rpat->cd2.center.y;
    radial[4] = rpat->cd2.radius;

    vgSetParameteri (context->paint,
		     VG_PAINT_COLOR_RAMP_SPREAD_MODE, VG_COLOR_RAMP_SPREAD_PAD);
    vgSetParameteri (context->paint,
		     VG_PAINT_TYPE, VG_PAINT_TYPE_RADIAL_GRADIENT);
    vgSetParameterfv (context->paint,
		      VG_PAINT_RADIAL_GRADIENT, 5, radial);

    _vg_set_source_matrix (&rpat->base.base);

    /* FIXME: copy/adapt fixes from SVG backend to add inner radius */

    CHECK_VG_ERRORS();
    return _vg_setup_gradient_stops (context, &rpat->base);
}

static cairo_status_t
_vg_setup_solid_source (cairo_vg_context_t *context,
			const cairo_solid_pattern_t *spat)
{
    VGfloat color[] = {
	spat->color.red,
	spat->color.green,
	spat->color.blue,
	spat->color.alpha * context->alpha
    };

    vgSetParameteri (context->paint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
    vgSetParameterfv (context->paint, VG_PAINT_COLOR, 4, color);

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static cairo_vg_surface_t *
_vg_clone_recording_surface (cairo_vg_context_t *context,
			cairo_surface_t *surface)
{
    VGImage vg_image;
    VGImageFormat format;
    cairo_status_t status;
    cairo_rectangle_int_t extents;
    cairo_vg_surface_t *clone;

    status = _cairo_surface_get_extents (surface, &extents);
    if (status)
	return NULL;

    if (extents.width > vgGeti (VG_MAX_IMAGE_WIDTH) ||
	extents.height > vgGeti (VG_MAX_IMAGE_HEIGHT))
    {
	return NULL;
    }

    format = _vg_format_for_content (surface->content);

    /* NONALIASED, FASTER, BETTER */
    vg_image = vgCreateImage (format,
			      extents.width, extents.height,
			      VG_IMAGE_QUALITY_FASTER);
    clone = (cairo_vg_surface_t *)
	_vg_surface_create_internal (context, vg_image, format,
				     extents.width, extents.height);
    cairo_surface_set_device_offset (&clone->base, -extents.x, -extents.y);

    status = _cairo_recording_surface_replay (surface, &clone->base);
    if (unlikely (status)) {
	cairo_surface_destroy (&clone->base);
	return (cairo_vg_surface_t *) _cairo_surface_create_in_error (status);
    }

    return clone;
}

static cairo_vg_surface_t *
_vg_clone_image_surface (cairo_vg_context_t *context,
			 cairo_surface_t *surface)
{
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status;
    VGImage vg_image;
    VGImageFormat format;
    cairo_rectangle_int_t extents;
    cairo_vg_surface_t *clone;

    if (surface->backend->acquire_source_image == NULL)
	return NULL;

    status = _cairo_surface_get_extents (surface, &extents);
    if (status)
	return NULL;

    if (extents.width > vgGeti (VG_MAX_IMAGE_WIDTH) ||
	extents.height > vgGeti (VG_MAX_IMAGE_HEIGHT))
    {
	return NULL;
    }

    status = _cairo_surface_acquire_source_image (surface,
						  &image, &image_extra);
    if (unlikely (status))
	return (cairo_vg_surface_t *) _cairo_surface_create_in_error (status);

    format = _vg_format_from_pixman (image->pixman_format);
    if (format == 0)
	format = _vg_format_for_content (image->base.content);

    /* NONALIASED, FASTER, BETTER */
    vg_image = vgCreateImage (format,
			      image->width, image->height,
			      VG_IMAGE_QUALITY_FASTER);
    clone = (cairo_vg_surface_t *)
	_vg_surface_create_internal (context, vg_image, format,
				    image->width, image->height);
    if (unlikely (clone->base.status))
	return clone;

    vgImageSubData (clone->image,
		    image->data, image->stride,
		    format, 0, 0, image->width, image->height);

    _cairo_surface_release_source_image (surface, image, image_extra);

    return clone;
}

static void
_vg_surface_remove_from_cache (cairo_surface_t *abstract_surface)
{
    cairo_vg_surface_t *surface = (cairo_vg_surface_t *) abstract_surface;

    if (surface->snapshot_cache_entry.hash) {
	cairo_vg_context_t *context;

	context = _vg_context_lock (surface->context);
	_cairo_cache_remove (&context->snapshot_cache,
			     &surface->snapshot_cache_entry);
	_vg_context_unlock (context);

	surface->snapshot_cache_entry.hash = 0;
    }
}

static cairo_status_t
_vg_setup_surface_source (cairo_vg_context_t *context,
			  const cairo_surface_pattern_t *spat)
{
    cairo_surface_t *snapshot;
    cairo_vg_surface_t *clone;
    cairo_status_t status;

    snapshot = _cairo_surface_has_snapshot (spat->surface,
					    &cairo_vg_surface_backend);
    if (snapshot != NULL) {
	clone = (cairo_vg_surface_t *) cairo_surface_reference (snapshot);
	goto DONE;
    }

    if (_cairo_surface_is_recording (spat->surface))
	clone = _vg_clone_recording_surface (context, spat->surface);
    else
	clone = _vg_clone_image_surface (context, spat->surface);
    if (clone == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (unlikely (clone->base.status))
	return clone->base.status;

    clone->snapshot_cache_entry.hash = clone->base.unique_id;
    status = _cairo_cache_insert (&context->snapshot_cache,
				  &clone->snapshot_cache_entry);
    if (unlikely (status)) {
	clone->snapshot_cache_entry.hash = 0;
	cairo_surface_destroy (&clone->base);
	return status;
    }

    _cairo_surface_attach_snapshot (spat->surface, &clone->base,
				    _vg_surface_remove_from_cache);

DONE:
    cairo_surface_destroy (&context->source->base);
    context->source = clone;

    vgSetParameteri (context->paint, VG_PAINT_TYPE, VG_PAINT_TYPE_PATTERN);

    switch (spat->base.extend) {
    case CAIRO_EXTEND_PAD:
	vgSetParameteri (context->paint,
			 VG_PAINT_PATTERN_TILING_MODE,
			 VG_TILE_PAD);
	break;

    case CAIRO_EXTEND_NONE:
	vgSetParameteri (context->paint,
			 VG_PAINT_PATTERN_TILING_MODE,
			 VG_TILE_FILL);
	{
	    VGfloat color[] = {0,0,0,0};
	    vgSetfv (VG_TILE_FILL_COLOR, 4, color);
	}
	break;

    case CAIRO_EXTEND_REPEAT:
	vgSetParameteri (context->paint,
			 VG_PAINT_PATTERN_TILING_MODE,
			 VG_TILE_REPEAT);
	break;

    default:
	ASSERT_NOT_REACHED;
    case CAIRO_EXTEND_REFLECT:
	vgSetParameteri (context->paint,
			 VG_PAINT_PATTERN_TILING_MODE,
			 VG_TILE_REFLECT);
	break;
    }
    vgPaintPattern (context->paint, context->source->image);

    _vg_set_source_matrix (&spat->base);

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
setup_source (cairo_vg_context_t *context,
	      const cairo_pattern_t *source)
{
    switch (source->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return _vg_setup_solid_source (context,
				       (cairo_solid_pattern_t *) source);
    case CAIRO_PATTERN_TYPE_LINEAR:
	return _vg_setup_linear_source (context,
					(cairo_linear_pattern_t *) source);
    case CAIRO_PATTERN_TYPE_RADIAL:
	return _vg_setup_radial_source (context,
					(cairo_radial_pattern_t *) source);
    case CAIRO_PATTERN_TYPE_SURFACE:
	return _vg_setup_surface_source (context,
					 (cairo_surface_pattern_t *) source);
    default:
	ASSERT_NOT_REACHED;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
}

static cairo_int_status_t
_vg_surface_stroke (void                       *abstract_surface,
		    cairo_operator_t            op,
		    const cairo_pattern_t      *source,
		    const cairo_path_fixed_t   *path,
		    const cairo_stroke_style_t *style,
		    const cairo_matrix_t       *ctm,
		    const cairo_matrix_t       *ctm_inverse,
		    double                      tolerance,
		    cairo_antialias_t           antialias,
		    const cairo_clip_t         *clip)
{
    cairo_vg_surface_t *surface = abstract_surface;
    cairo_vg_context_t *context;
    cairo_status_t status;
    VGfloat state[9];
    VGfloat strokeTransform[9];
    vg_path_t vg_path;

    if (! _vg_is_supported_operator (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    context = _vg_context_lock (surface->context);
    status = _vg_context_set_target (context, surface);
    if (status) {
	_vg_context_unlock (context);
	return status;
    }

    status = setup_source (context, source);
    if (status) {
	_vg_context_unlock (context);
	return status;
    }

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status)) {
	_vg_context_unlock (context);
	return status;
    }

    vg_path.path = vgCreatePath (VG_PATH_FORMAT_STANDARD,
				 VG_PATH_DATATYPE_F,
				 1, 0, 0, 0,
				 VG_PATH_CAPABILITY_ALL);

    vgGetMatrix (state);
    _vg_matrix_from_cairo (strokeTransform, ctm);
    vgMultMatrix (strokeTransform);

    vg_path.ctm_inverse = ctm_inverse;

    _vg_path_from_cairo (&vg_path, path);

    /* XXX DASH_PATTERN, DASH_PHASE */
    vgSetf (VG_STROKE_LINE_WIDTH, style->line_width);
    vgSetf (VG_STROKE_MITER_LIMIT, style->miter_limit);
    vgSetf (VG_STROKE_JOIN_STYLE, _vg_line_join_from_cairo (style->line_join));
    vgSetf (VG_STROKE_CAP_STYLE, _vg_line_cap_from_cairo (style->line_cap));

    vgSeti (VG_BLEND_MODE, _vg_operator (op));

    vgSetPaint (context->paint, VG_STROKE_PATH);

    vgDrawPath (vg_path.path, VG_STROKE_PATH);

    vgDestroyPath (vg_path.path);

    vgLoadMatrix (state);

    CHECK_VG_ERRORS();
    _vg_context_unlock (context);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_vg_surface_fill (void                     *abstract_surface,
		  cairo_operator_t          op,
		  const cairo_pattern_t    *source,
		  const cairo_path_fixed_t *path,
		  cairo_fill_rule_t         fill_rule,
		  double                    tolerance,
		  cairo_antialias_t         antialias,
		  const cairo_clip_t       *clip)
{
    cairo_vg_surface_t *surface = abstract_surface;
    cairo_vg_context_t *context;
    cairo_status_t status;
    vg_path_t vg_path;

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    if (! _vg_is_supported_operator (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    context = _vg_context_lock (surface->context);
    status = _vg_context_set_target (context, surface);
    if (status) {
	_vg_context_unlock (context);
	return status;
    }

    status = setup_source (context, source);
    if (status) {
	_vg_context_unlock (context);
	return status;
    }

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status)) {
	_vg_context_unlock (context);
	return status;
    }

    vg_path.path = vgCreatePath (VG_PATH_FORMAT_STANDARD,
				 VG_PATH_DATATYPE_F,
				 1, 0,
				 0, 0,
				 VG_PATH_CAPABILITY_ALL);
    vg_path.ctm_inverse = NULL;

    _vg_path_from_cairo (&vg_path, path);

    /* XXX tolerance */

    vgSeti (VG_BLEND_MODE, _vg_operator (op));
    vgSetf (VG_FILL_RULE, _vg_fill_rule_from_cairo (fill_rule));
    vgSetf (VG_RENDERING_QUALITY, _vg_rendering_quality_from_cairo (antialias));

    vgSetPaint (context->paint, VG_FILL_PATH);

    vgDrawPath (vg_path.path, VG_FILL_PATH);

    vgDestroyPath (vg_path.path);

    _vg_context_unlock (context);

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_vg_surface_paint (void                  *abstract_surface,
		   cairo_operator_t       op,
		   const cairo_pattern_t *source,
		   const cairo_clip_t    *clip)
{
    cairo_vg_surface_t *surface = abstract_surface;
    cairo_vg_context_t *context;
    cairo_status_t status;

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    if (! _vg_is_supported_operator (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    context = _vg_context_lock (surface->context);
    status = _vg_context_set_target (context, surface);
    if (status) {
	_vg_context_unlock (context);
	return status;
    }

    status = setup_source (context, source);
    if (status) {
	_vg_context_unlock (context);
	return status;
    }

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status)) {
	_vg_context_unlock (context);
	return status;
    }

    vgSeti (VG_BLEND_MODE, _vg_operator (op));
    vgSetPaint (context->paint, VG_FILL_PATH);

    { /* creating a rectangular path that should cover the extent */
	VGubyte segs[] = {
	    VG_MOVE_TO_ABS, VG_LINE_TO_ABS,
	    VG_LINE_TO_ABS, VG_LINE_TO_ABS,
	    VG_CLOSE_PATH
	};
	VGfloat data[] = {
	    0, 0,
	    surface->width, 0,
	    surface->width, surface->height,
	    0, surface->height
	};
	VGPath fullext;

	fullext = vgCreatePath (VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
				1,0,0,0, VG_PATH_CAPABILITY_ALL);
	vgAppendPathData (fullext, sizeof(segs), segs, data);

	vgDrawPath (fullext, VG_FILL_PATH);

	vgDestroyPath (fullext);
    }

    _vg_context_unlock (context);

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_vg_surface_mask (void                   *abstract_surface,
		  cairo_operator_t        op,
		  const cairo_pattern_t  *source,
		  const cairo_pattern_t  *mask,
		  const cairo_clip_t     *clip)
{
    cairo_vg_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (! _vg_is_supported_operator (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Handle paint-with-alpha to do fades cheaply */
    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) mask;
	cairo_vg_context_t *context = _vg_context_lock (surface->context);
	double alpha = context->alpha;

	context->alpha = solid->color.alpha;
	status = _vg_surface_paint (abstract_surface, op, source, clip);
	context->alpha = alpha;

	_vg_context_unlock (context);

	return status;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static void
_vg_surface_get_font_options (void                  *abstract_surface,
			      cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_ON);
    _cairo_font_options_set_round_glyph_positions (options, CAIRO_ROUND_GLYPH_POS_OFF);
}

static cairo_int_status_t
_vg_surface_show_glyphs (void			*abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 cairo_glyph_t		*glyphs,
			 int			 num_glyphs,
			 cairo_scaled_font_t	*scaled_font,
			 const cairo_clip_t     *clip)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_path_fixed_t path;

    if (num_glyphs <= 0)
	return CAIRO_STATUS_SUCCESS;

    _cairo_path_fixed_init (&path);

    /* XXX Glyph cache! OpenVG font support in 1.1? */

    status = _cairo_scaled_font_glyph_path (scaled_font,
					    glyphs, num_glyphs,
					    &path);
    if (unlikely (status))
	goto BAIL;

    status = _vg_surface_fill (abstract_surface,
			       op, source, &path,
			       CAIRO_FILL_RULE_WINDING,
			       CAIRO_GSTATE_TOLERANCE_DEFAULT,
			       CAIRO_ANTIALIAS_DEFAULT,
			       clip);
BAIL:
    _cairo_path_fixed_fini (&path);
    return status;
}

static inline int
multiply_alpha (int alpha, int color)
{
    int temp = alpha * color + 0x80;
    return (temp + (temp >> 8)) >> 8;
}

static void
premultiply_argb (uint8_t   *data,
		  int	     width,
		  int	     height,
		  int	     stride)
{
    int i;

    while (height --) {
	uint32_t *row = (uint32_t *) data;

	for (i = 0; i < width; i++) {
	    uint32_t p = row[i];
	    uint8_t  alpha;

	    alpha = p >> 24;
	    if (alpha == 0) {
		 row[i] = 0;
	    } else if (alpha != 0xff) {
		uint8_t r = multiply_alpha (alpha, (p >> 16) & 0xff);
		uint8_t g = multiply_alpha (alpha, (p >>  8) & 0xff);
		uint8_t b = multiply_alpha (alpha, (p >>  0) & 0xff);
		row[i] = ((uint32_t)alpha << 24) | (r << 16) | (g << 8) | (b << 0);
	    }
	}

	data += stride;
    }
}

static cairo_int_status_t
_vg_get_image (cairo_vg_surface_t *surface,
	       int x, int y,
	       int width, int height,
	       cairo_image_surface_t **image_out)
{
    cairo_image_surface_t *image;
    pixman_image_t *pixman_image;
    pixman_format_code_t pixman_format;
    cairo_bool_t needs_premultiply;

    pixman_format = _vg_format_to_pixman (surface->format,
					  &needs_premultiply);
    if (pixman_format == 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    pixman_image = pixman_image_create_bits (pixman_format,
					     width, height,
					     NULL, 0);
    if (unlikely (pixman_image == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    vgFinish ();
    CHECK_VG_ERRORS();

    vgGetImageSubData (surface->image,
		       pixman_image_get_data (pixman_image),
		       pixman_image_get_stride (pixman_image),
		       surface->format,
		       x, y, width, height);

    image = (cairo_image_surface_t *)
	_cairo_image_surface_create_for_pixman_image (pixman_image,
						      pixman_format);
    if (unlikely (image->base.status)) {
	pixman_image_unref (pixman_image);
	return image->base.status;
    }

    if (needs_premultiply)
	premultiply_argb (image->data, width, height, image->stride);

    *image_out = image;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_vg_surface_acquire_source_image (void *abstract_surface,
				  cairo_image_surface_t **image_out,
				  void                  **image_extra)
{
    cairo_vg_surface_t *surface = abstract_surface;

    CHECK_VG_ERRORS();
    *image_extra = NULL;
    return _vg_get_image (surface,
			  0, 0, surface->width, surface->height,
			  image_out);
}

static void
_vg_surface_release_source_image (void                    *abstract_surface,
				  cairo_image_surface_t   *image,
				  void                    *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_vg_surface_finish (void *abstract_surface)
{
    cairo_vg_surface_t *surface = abstract_surface;
    cairo_vg_context_t *context = _vg_context_lock (surface->context);

    if (surface->snapshot_cache_entry.hash) {
	_cairo_cache_remove (&context->snapshot_cache,
			     &surface->snapshot_cache_entry);

	surface->snapshot_cache_entry.hash = 0;
    }

    _cairo_surface_clipper_reset (&surface->clipper);

    if (surface->own_image)
	vgDestroyImage (surface->image);

    _vg_context_destroy_target (context, surface);

    _vg_context_unlock (context);
    _vg_context_destroy (context);

    CHECK_VG_ERRORS();
    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_vg_surface_backend = {
    CAIRO_SURFACE_TYPE_VG,
    _vg_surface_finish,

    _cairo_default_context_create, /* XXX */

    _vg_surface_create_similar,
    NULL, /* create similar image */
    NULL, /* map to image */
    NULL, /* unmap image */

    _cairo_surface_default_source,
    _vg_surface_acquire_source_image,
    _vg_surface_release_source_image,
    NULL, /* snapshot */

    NULL, /* copy_page */
    NULL, /* show_page */

    _vg_surface_get_extents,
    _vg_surface_get_font_options, /* get_font_options */

    NULL, /* flush */
    NULL, /* mark dirty */

    _vg_surface_paint,
    _vg_surface_mask,
    _vg_surface_stroke,
    _vg_surface_fill,
    NULL, /* fill-stroke */
    _vg_surface_show_glyphs,
};

static cairo_surface_t *
_vg_surface_create_internal (cairo_vg_context_t *context,
			     VGImage image,
			     VGImageFormat format,
			     int width, int height)
{
    cairo_vg_surface_t *surface;

    surface = _cairo_malloc (sizeof (cairo_vg_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    surface->context = _vg_context_reference (context);

    surface->image  = image;
    surface->format = format;

    _cairo_surface_init (&surface->base,
			 &cairo_vg_surface_backend,
			 NULL, /* device */
			 _vg_format_to_content (format),
			 FALSE); /* is_vector */

    surface->width  = width;
    surface->height = height;

    _cairo_surface_clipper_init (&surface->clipper,
				 _vg_surface_clipper_intersect_clip_path);

    surface->snapshot_cache_entry.hash = 0;

    surface->target_id = 0;

    CHECK_VG_ERRORS();
    return &surface->base;
}

cairo_surface_t *
cairo_vg_surface_create_for_image (cairo_vg_context_t *context,
				   VGImage image,
				   VGImageFormat format,
				   int width, int height)
{
    cairo_bool_t premult;

    if (context->status)
	return _cairo_surface_create_in_error (context->status);

    if (image == VG_INVALID_HANDLE)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    if (_vg_format_to_pixman (format, &premult) == 0)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    return _vg_surface_create_internal (context, image, format, width, height);
}

cairo_surface_t *
cairo_vg_surface_create (cairo_vg_context_t *context,
			 cairo_content_t  content,
			 int              width,
			 int              height)
{
    VGImage image;
    VGImageFormat format;
    cairo_surface_t *surface;

    if (context->status)
	return _cairo_surface_create_in_error (context->status);

    if (! CAIRO_CONTENT_VALID (content))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_CONTENT));

    if (width > vgGeti (VG_MAX_IMAGE_WIDTH) ||
	height > vgGeti (VG_MAX_IMAGE_HEIGHT))
    {
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
    }


    format = _vg_format_for_content (content);
    image = vgCreateImage (format, width, height, VG_IMAGE_QUALITY_BETTER);
    if (image == VG_INVALID_HANDLE)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    surface = _vg_surface_create_internal (context,
					   image, format, width, height);
    if (unlikely (surface->status))
	return surface;

    ((cairo_vg_surface_t *) surface)->own_image = TRUE;
    return surface;
}
slim_hidden_def (cairo_vg_surface_create);

VGImage
cairo_vg_surface_get_image (cairo_surface_t *abstract_surface)
{
    cairo_vg_surface_t *surface;

    if (abstract_surface->backend != &cairo_vg_surface_backend) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return VG_INVALID_HANDLE;
    }

    surface = (cairo_vg_surface_t *) abstract_surface;
    return surface->image;
}

int
cairo_vg_surface_get_width (cairo_surface_t *abstract_surface)
{
    cairo_vg_surface_t *surface;

    if (abstract_surface->backend != &cairo_vg_surface_backend) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    surface = (cairo_vg_surface_t *) abstract_surface;
    return surface->width;
}

int
cairo_vg_surface_get_height (cairo_surface_t *abstract_surface)
{
    cairo_vg_surface_t *surface;

    if (abstract_surface->backend != &cairo_vg_surface_backend) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    surface = (cairo_vg_surface_t *) abstract_surface;
    return surface->height;
}

VGImageFormat
cairo_vg_surface_get_format (cairo_surface_t *abstract_surface)
{
    cairo_vg_surface_t *surface;

    if (abstract_surface->backend != &cairo_vg_surface_backend) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    surface = (cairo_vg_surface_t *) abstract_surface;
    return surface->format;
}

/* GL specific context support :-(
 *
 * OpenVG like cairo defers creation of surface (and the necessary
 * paraphernalia to the application.
 */

static const cairo_vg_context_t _vg_context_nil = {
    CAIRO_STATUS_NO_MEMORY,
    CAIRO_REFERENCE_COUNT_INVALID
};

static const cairo_vg_context_t _vg_context_nil_invalid_visual = {
    CAIRO_STATUS_INVALID_VISUAL,
    CAIRO_REFERENCE_COUNT_INVALID
};

#if CAIRO_HAS_GLX_FUNCTIONS
#include <GL/glx.h>

static cairo_status_t
glx_create_target (cairo_vg_context_t *context,
		   cairo_vg_surface_t *surface)
{
    /* XXX hmm, magic required for creating an FBO points to VGImage! */
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
glx_set_target (cairo_vg_context_t *context,
		cairo_vg_surface_t *surface)
{
#if 0
    glXMakeContextCurrent (context->display,
			   (GLXDrawable) surface->target_id,
			   (GLXDrawable) surface->target_id,
			   context->context);
#else
    return CAIRO_INT_STATUS_UNSUPPORTED;
#endif
}

static void
glx_destroy_target (cairo_vg_context_t *context,
		    cairo_vg_surface_t *surface)
{
}

cairo_vg_context_t *
cairo_vg_context_create_for_glx (Display *dpy, GLXContext ctx)
{
    cairo_vg_context_t *context;
    cairo_status_t status;

    context = _cairo_malloc (sizeof (*context));
    if (unlikely (context == NULL))
	return (cairo_vg_context_t *) &_vg_context_nil;

    context->display = dpy;
    context->context = ctx;

    context->create_target  = glx_create_target;
    context->set_target     = glx_set_target;
    context->destroy_target = glx_destroy_target;

    status = _vg_context_init (context);
    if (unlikely (status)) {
	free (context);
	return (cairo_vg_context_t *) &_vg_context_nil;
    }

    return context;
}
#endif

#if CAIRO_HAS_EGL_FUNCTIONS
static cairo_status_t
egl_create_target (cairo_vg_context_t *context,
		   cairo_vg_surface_t *surface)
{
    EGLSurface *egl_surface;
#define RED 1
#define GREEN 3
#define BLUE 5
#define ALPHA 7
    int attribs[] = {
	EGL_RED_SIZE, 0,
	EGL_GREEN_SIZE, 0,
	EGL_BLUE_SIZE, 0,
	EGL_ALPHA_SIZE, 0,
	EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
	EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
	EGL_NONE
    };
    pixman_format_code_t pixman_format;
    EGLConfig config;
    int num_configs = 0;
    cairo_bool_t needs_premultiply;

    pixman_format = _vg_format_to_pixman (surface->format, &needs_premultiply);
    if (pixman_format == 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX no control over pixel ordering! */
    attribs[RED]   = PIXMAN_FORMAT_R (pixman_format);
    attribs[GREEN] = PIXMAN_FORMAT_G (pixman_format);
    attribs[BLUE]  = PIXMAN_FORMAT_B (pixman_format);
    attribs[ALPHA] = PIXMAN_FORMAT_A (pixman_format);

    if (! eglChooseConfig (context->display,
			   attribs,
			   &config, 1, &num_configs) ||
	num_configs != 1)
    {
	fprintf(stderr, "Error: eglChooseConfig() failed.\n");
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    egl_surface =
	eglCreatePbufferFromClientBuffer (context->display,
					  EGL_OPENVG_IMAGE,
					  (EGLClientBuffer) surface->image,
					  config,
					  NULL);
    surface->target_id = (unsigned long) egl_surface;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
egl_set_target (cairo_vg_context_t *context,
		cairo_vg_surface_t *surface)
{
    if (! eglMakeCurrent (context->display,
			  (EGLSurface *) surface->target_id,
			  (EGLSurface *) surface->target_id,
			  context->context))
    {
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
egl_destroy_target (cairo_vg_context_t *context,
		    cairo_vg_surface_t *surface)
{
    eglDestroySurface (context->display,
		       (EGLSurface *) surface->target_id);
}

cairo_vg_context_t *
cairo_vg_context_create_for_egl (EGLDisplay egl_display,
				 EGLContext egl_context)
{
    cairo_vg_context_t *context;
    cairo_status_t status;

    context = _cairo_malloc (sizeof (*context));
    if (unlikely (context == NULL))
	return (cairo_vg_context_t *) &_vg_context_nil;

    status = _vg_context_init (context);
    if (unlikely (status)) {
	free (context);
	return (cairo_vg_context_t *) &_vg_context_nil;
    }

    context->display = egl_display;
    context->context = egl_context;

    context->create_target  = egl_create_target;
    context->set_target     = egl_set_target;
    context->destroy_target = egl_destroy_target;

    return context;
}
#endif

cairo_status_t
cairo_vg_context_status (cairo_vg_context_t *context)
{
    return context->status;
}

void
cairo_vg_context_destroy (cairo_vg_context_t *context)
{
    if (context == NULL || CAIRO_REFERENCE_COUNT_IS_INVALID (&context->ref_count))
	return;

    _vg_context_destroy (context);
}
