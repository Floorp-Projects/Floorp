/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Intel Corporation
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
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-composite-rectangles-private.h"
#include "cairo-boxes-private.h"
#include "cairo-error-private.h"
#include "cairo-drm-i915-private.h"

/* Operates in either immediate or retained mode.
 * When given a clip region we record the sequence of vbo and then
 * replay them for each clip rectangle, otherwise we simply emit
 * the vbo straight into the command stream.
 */

typedef struct _i915_spans i915_spans_t;

typedef float *
(*i915_get_rectangle_func_t) (i915_spans_t *spans);

typedef void
(*i915_span_func_t) (i915_spans_t *spans,
		     int x0, int x1, int y0, int y1,
		     int alpha);

struct _i915_spans {
    cairo_span_renderer_t renderer;

    i915_device_t *device;

    int xmin, xmax;
    cairo_bool_t is_bounded;
    const cairo_rectangle_int_t *extents;

    i915_get_rectangle_func_t get_rectangle;
    i915_span_func_t span;
    i915_shader_t shader;

    cairo_region_t *clip_region;
    cairo_bool_t need_clip_surface;

    struct vbo {
	struct vbo *next;
	intel_bo_t *bo;
	unsigned int count;
    } head, *tail;

    unsigned int vbo_offset;
    float *vbo_base;
};

static float *
i915_emit_rectangle (i915_spans_t *spans)
{
    return i915_add_rectangle (spans->device);
}

static float *
i915_accumulate_rectangle (i915_spans_t *spans)
{
    float *vertices;
    uint32_t size;

    size = spans->device->rectangle_size;
    if (unlikely (spans->vbo_offset + size > I915_VBO_SIZE)) {
	struct vbo *vbo;

	vbo = _cairo_malloc (sizeof (struct vbo));
	if (unlikely (vbo == NULL)) {
	    /* throw error! */
	}

	spans->tail->next = vbo;
	spans->tail = vbo;

	vbo->next = NULL;
	vbo->bo = intel_bo_create (&spans->device->intel,
				   I915_VBO_SIZE, I915_VBO_SIZE,
				   FALSE, I915_TILING_NONE, 0);
	vbo->count = 0;

	spans->vbo_offset = 0;
	spans->vbo_base = intel_bo_map (&spans->device->intel, vbo->bo);
    }

    vertices = spans->vbo_base + spans->vbo_offset;
    spans->vbo_offset += size;
    spans->tail->count += 3;

    return vertices;
}

static void
i915_span_zero (i915_spans_t *spans,
		int x0, int x1, int y0, int y1,
		int alpha)
{
    float *vertices;

    vertices = spans->get_rectangle (spans);

    *vertices++ = x1;
    *vertices++ = y1;

    *vertices++ = x0;
    *vertices++ = y1;

    *vertices++ = x0;
    *vertices++ = y0;
}

static void
i915_span_constant (i915_spans_t *spans,
		    int x0, int x1, int y0, int y1,
		    int alpha)
{
    float *vertices;
    float a = alpha / 255.;

    vertices = spans->get_rectangle (spans);

    *vertices++ = x1;
    *vertices++ = y1;
    *vertices++ = a;

    *vertices++ = x0;
    *vertices++ = y1;
    *vertices++ = a;

    *vertices++ = x0;
    *vertices++ = y0;
    *vertices++ = a;
}

static void
i915_span_linear (i915_spans_t *spans,
		  int x0, int x1, int y0, int y1,
		  int alpha)
{
    float *vertices;
    float a = alpha / 255.;
    double s, t;

    vertices = spans->get_rectangle (spans);

    *vertices++ = x1;
    *vertices++ = y1;
    s = x0, t = y0;
    *vertices++ = i915_shader_linear_texcoord (&spans->shader.source.linear, s, t);
    *vertices++ = a;

    *vertices++ = x0;
    *vertices++ = y1;
    s = x1, t = y0;
    *vertices++ = i915_shader_linear_texcoord (&spans->shader.source.linear, s, t);
    *vertices++ = a;

    *vertices++ = x0;
    *vertices++ = y0;
    s = x1, t = y1;
    *vertices++ = i915_shader_linear_texcoord (&spans->shader.source.linear, s, t);
    *vertices++ = a;
}

static void
i915_span_texture (i915_spans_t *spans,
		   int x0, int x1, int y0, int y1,
		   int alpha)
{
    float *vertices;
    float a = alpha / 255.;
    double s, t;

    vertices = spans->get_rectangle (spans);

    *vertices++ = x1;
    *vertices++ = y1;
    s = x0, t = y0;
    cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
    *vertices++ = s; *vertices++ = t;
    *vertices++ = a;

    *vertices++ = x0;
    *vertices++ = y1;
    s = x1, t = y0;
    cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
    *vertices++ = s; *vertices++ = t;
    *vertices++ = a;

    *vertices++ = x0;
    *vertices++ = y0;
    s = x1, t = y1;
    cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
    *vertices++ = s; *vertices++ = t;
    *vertices++ = a;
}

static void
i915_span_texture16 (i915_spans_t *spans,
		     int x0, int x1, int y0, int y1, int alpha)
{
    float *vertices;
    float a = alpha / 255.;
    double s, t;

    vertices = spans->get_rectangle (spans);

    *vertices++ = x1;
    *vertices++ = y1;
    s = x0, t = y0;
    cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
    *vertices++ = texcoord_2d_16 (s, t);
    *vertices++ = a;

    *vertices++ = x0;
    *vertices++ = y1;
    s = x1, t = y0;
    cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
    *vertices++ = texcoord_2d_16 (s, t);
    *vertices++ = a;

    *vertices++ = x0;
    *vertices++ = y0;
    s = x1, t = y1;
    cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
    *vertices++ = texcoord_2d_16 (s, t);
    *vertices++ = a;
}

static void
i915_span_generic (i915_spans_t *spans,
		   int x0, int x1, int y0, int y1, int alpha)
{
    double s, t;
    float *vertices;
    float a = alpha / 255.;

    /* Each vertex is:
     *   2 vertex coordinates
     *   [0-2] source texture coordinates
     *   1 alpha value.
     *   [0,2] clip mask coordinates
     */

    vertices = spans->get_rectangle (spans);

    /* bottom right */
    *vertices++ = x1; *vertices++ = y1;
    s = x1, t = y1;
    switch (spans->shader.source.type.vertex) {
    case VS_ZERO:
    case VS_CONSTANT:
	break;
    case VS_LINEAR:
	*vertices++ = i915_shader_linear_texcoord (&spans->shader.source.linear, s, t);
	break;
    case VS_TEXTURE:
	cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
	*vertices++ = s; *vertices++ = t;
	break;
    case VS_TEXTURE_16:
	cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
	*vertices++ = texcoord_2d_16 (s, t);
	break;
    }
    *vertices++ = a;
    if (spans->need_clip_surface) {
	s = x1, t = y1;
	cairo_matrix_transform_point (&spans->shader.clip.base.matrix, &s, &t);
	*vertices++ = texcoord_2d_16 (s, t);
    }
    if (spans->shader.need_combine) {
	s = x1, t = y1;
	cairo_matrix_transform_point (&spans->shader.dst.base.matrix, &s, &t);
	*vertices++ = texcoord_2d_16 (s, t);
    }

    /* bottom left */
    *vertices++ = x0; *vertices++ = y1;
    s = x0, t = y1;
    switch (spans->shader.source.type.vertex) {
    case VS_ZERO:
    case VS_CONSTANT:
	break;
    case VS_LINEAR:
	*vertices++ = i915_shader_linear_texcoord (&spans->shader.source.linear, s, t);
	break;
    case VS_TEXTURE:
	cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
	*vertices++ = s; *vertices++ = t;
	break;
    case VS_TEXTURE_16:
	cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
	*vertices++ = texcoord_2d_16 (s, t);
	break;
    }
    *vertices++ = a;
    if (spans->need_clip_surface) {
	s = x0, t = y1;
	cairo_matrix_transform_point (&spans->shader.clip.base.matrix, &s, &t);
	*vertices++ = texcoord_2d_16 (s, t);
    }
    if (spans->shader.need_combine) {
	s = x0, t = y1;
	cairo_matrix_transform_point (&spans->shader.dst.base.matrix, &s, &t);
	*vertices++ = texcoord_2d_16 (s, t);
    }

    /* top left */
    *vertices++ = x0; *vertices++ = y0;
    s = x0, t = y0;
    switch (spans->shader.source.type.vertex) {
    case VS_ZERO:
    case VS_CONSTANT:
	break;
    case VS_LINEAR:
	*vertices++ = i915_shader_linear_texcoord (&spans->shader.source.linear, s, t);
	break;
    case VS_TEXTURE:
	cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
	*vertices++ = s; *vertices++ = t;
	break;
    case VS_TEXTURE_16:
	cairo_matrix_transform_point (&spans->shader.source.base.matrix, &s, &t);
	*vertices++ = texcoord_2d_16 (s, t);
	break;
    }
    *vertices++ = a;
    if (spans->need_clip_surface) {
	s = x0, t = y0;
	cairo_matrix_transform_point (&spans->shader.clip.base.matrix, &s, &t);
	*vertices++ = texcoord_2d_16 (s, t);
    }
    if (spans->shader.need_combine) {
	s = x0, t = y0;
	cairo_matrix_transform_point (&spans->shader.dst.base.matrix, &s, &t);
	*vertices++ = texcoord_2d_16 (s, t);
    }
}

static cairo_status_t
i915_zero_spans_mono (void *abstract_renderer,
		      int y, int height,
		      const cairo_half_open_span_t *half,
		      unsigned num_spans)
{
    i915_spans_t *spans = abstract_renderer;
    int x0, x1;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	while (num_spans && half[0].coverage < 128)
	    half++, num_spans--;
	if (num_spans == 0)
	    break;

	x0 = x1 = half[0].x;
	while (num_spans--) {
	    half++;

	    x1 = half[0].x;
	    if (half[0].coverage < 128)
		break;
	}

	i915_span_zero (spans,
			x0, x1,
			y, y + height,
			0);
    } while (num_spans);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_zero_spans (void *abstract_renderer,
		 int y, int height,
		 const cairo_half_open_span_t *half,
		 unsigned num_spans)
{
    i915_spans_t *spans = abstract_renderer;
    int x0, x1;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	while (num_spans && half[0].coverage == 0)
	    half++, num_spans--;
	if (num_spans == 0)
	    break;

	x0 = x1 = half[0].x;
	while (num_spans--) {
	    half++;

	    x1 = half[0].x;
	    if (half[0].coverage == 0)
		break;
	}

	i915_span_zero (spans,
			x0, x1,
			y, y + height,
			0);
    } while (num_spans);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_bounded_spans_mono (void *abstract_renderer,
			 int y, int height,
			 const cairo_half_open_span_t *half,
			 unsigned num_spans)
{
    i915_spans_t *spans = abstract_renderer;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	if (half[0].coverage >= 128) {
	    spans->span (spans,
			 half[0].x, half[1].x,
			 y, y + height,
			 255);
	}
	half++;
    } while (--num_spans > 1);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_bounded_spans (void *abstract_renderer,
		    int y, int height,
		    const cairo_half_open_span_t *half,
		    unsigned num_spans)
{
    i915_spans_t *spans = abstract_renderer;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	if (half[0].coverage) {
	    spans->span (spans,
			 half[0].x, half[1].x,
			 y, y + height,
			 half[0].coverage);
	}
	half++;
    } while (--num_spans > 1);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_unbounded_spans (void *abstract_renderer,
		      int y, int height,
		      const cairo_half_open_span_t *half,
		      unsigned num_spans)
{
    i915_spans_t *spans = abstract_renderer;

    if (num_spans == 0) {
	spans->span (spans,
		     spans->xmin, spans->xmax,
		     y, y + height,
		     0);
	return CAIRO_STATUS_SUCCESS;
    }

    if (half[0].x != spans->xmin) {
	spans->span (spans,
		     spans->xmin, half[0].x,
		     y, y + height,
		     0);
    }

    do {
	spans->span (spans,
		     half[0].x, half[1].x,
		     y, y + height,
		     half[0].coverage);
	half++;
    } while (--num_spans > 1);

    if (half[0].x != spans->xmax) {
	spans->span (spans,
		     half[0].x, spans->xmax,
		     y, y + height,
		     0);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_unbounded_spans_mono (void *abstract_renderer,
			   int y, int height,
			   const cairo_half_open_span_t *half,
			   unsigned num_spans)
{
    i915_spans_t *spans = abstract_renderer;

    if (num_spans == 0) {
	spans->span (spans,
		     spans->xmin, spans->xmax,
		     y, y + height,
		     0);
	return CAIRO_STATUS_SUCCESS;
    }

    if (half[0].x != spans->xmin) {
	spans->span (spans,
		     spans->xmin, half[0].x,
		     y, y + height,
		     0);
    }

    do {
	int alpha = 0;
	if (half[0].coverage >= 128)
	    alpha = 255;
	spans->span (spans,
		     half[0].x, half[1].x,
		     y, y + height,
		     alpha);
	half++;
    } while (--num_spans > 1);

    if (half[0].x != spans->xmax) {
	spans->span (spans,
		     half[0].x, spans->xmax,
		     y, y + height,
		     0);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_spans_init (i915_spans_t *spans,
		 i915_surface_t *dst,
		 cairo_operator_t op,
		 const cairo_pattern_t *pattern,
		 cairo_antialias_t antialias,
		 cairo_clip_t *clip,
		 double opacity,
		 const cairo_composite_rectangles_t *extents)
{
    cairo_status_t status;

    spans->device = (i915_device_t *) dst->intel.drm.base.device;

    spans->is_bounded = extents->is_bounded;
    if (extents->is_bounded) {
	if (antialias == CAIRO_ANTIALIAS_NONE)
	    spans->renderer.render_rows = i915_bounded_spans_mono;
	else
	    spans->renderer.render_rows = i915_bounded_spans;

	spans->extents = &extents->bounded;
    } else {
	if (antialias == CAIRO_ANTIALIAS_NONE)
	    spans->renderer.render_rows = i915_unbounded_spans_mono;
	else
	    spans->renderer.render_rows = i915_unbounded_spans;

	spans->extents = &extents->unbounded;
    }
    spans->xmin = spans->extents->x;
    spans->xmax = spans->extents->x + spans->extents->width;

    spans->clip_region = NULL;
    spans->need_clip_surface = FALSE;
    if (clip != NULL) {
	cairo_region_t *clip_region = NULL;

	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status == CAIRO_STATUS_SUCCESS || status == CAIRO_INT_STATUS_UNSUPPORTED);

	if (clip_region != NULL && cairo_region_num_rectangles (clip_region) == 1)
	    clip_region = NULL;

	spans->clip_region = clip_region;
	spans->need_clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
    }

    spans->head.next = NULL;
    spans->head.bo = NULL;
    spans->head.count = 0;
    spans->tail = &spans->head;

    if (spans->clip_region == NULL) {
	spans->get_rectangle = i915_emit_rectangle;
    } else {
	assert (! extents->is_bounded);
	spans->get_rectangle = i915_accumulate_rectangle;
	spans->head.bo = intel_bo_create (&spans->device->intel,
					  I915_VBO_SIZE, I915_VBO_SIZE,
					  FALSE, I915_TILING_NONE, 0);
	if (unlikely (spans->head.bo == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	spans->vbo_base = intel_bo_map (&spans->device->intel, spans->head.bo);
    }
    spans->vbo_offset = 0;

    i915_shader_init (&spans->shader, dst, op, opacity);
    if (spans->need_clip_surface)
	i915_shader_set_clip (&spans->shader, clip);

    status = i915_shader_acquire_pattern (&spans->shader, &spans->shader.source,
					  pattern, &extents->bounded);
    if (unlikely (status))
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static void
i915_spans_fini (i915_spans_t *spans)
{
    i915_shader_fini (&spans->shader);

    if (spans->head.bo != NULL) {
	struct vbo *vbo, *next;

	intel_bo_destroy (&spans->device->intel, spans->head.bo);
	for (vbo = spans->head.next; vbo != NULL; vbo = next) {
	    next = vbo->next;
	    intel_bo_destroy (&spans->device->intel, vbo->bo);
	    free (vbo);
	}
    }
}

cairo_status_t
i915_clip_and_composite_spans (i915_surface_t		*dst,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*pattern,
			       cairo_antialias_t	 antialias,
			       i915_spans_func_t	 draw_func,
			       void			*draw_closure,
			       const cairo_composite_rectangles_t*extents,
			       cairo_clip_t		*clip,
			       double opacity)
{
    i915_spans_t spans;
    i915_device_t *device;
    cairo_status_t status;
    struct vbo *vbo;

    if (i915_surface_needs_tiling (dst)) {
	ASSERT_NOT_REACHED;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (op == CAIRO_OPERATOR_CLEAR) {
	pattern = &_cairo_pattern_white.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    status = i915_spans_init (&spans, dst, op, pattern, antialias, clip, opacity, extents);
    if (unlikely (status))
	return status;

    spans.shader.mask.base.texfmt = TEXCOORDFMT_1D;
    spans.shader.mask.base.content = CAIRO_CONTENT_ALPHA;
    spans.shader.mask.type.fragment = FS_SPANS;

    status = cairo_device_acquire (dst->intel.drm.base.device);
    if (unlikely (status))
	goto CLEANUP_SPANS;

    if (dst->deferred_clear) {
	status = i915_surface_clear (dst);
	if (unlikely (status))
	    goto CLEANUP_SPANS;
    }

    device = i915_device (dst);
    status = i915_shader_commit (&spans.shader, device);
    if (unlikely (status))
	goto CLEANUP_DEVICE;

    if (! spans.shader.need_combine && ! spans.need_clip_surface) {
	switch (spans.shader.source.type.vertex) {
	case VS_ZERO:
	    spans.span = i915_span_zero;
	    if (extents->is_bounded) {
		if (antialias == CAIRO_ANTIALIAS_NONE)
		    spans.renderer.render_rows = i915_zero_spans_mono;
		else
		    spans.renderer.render_rows = i915_zero_spans;
	    }
	    break;
	case VS_CONSTANT:
	    spans.span = i915_span_constant;
	    break;
	case VS_LINEAR:
	    spans.span = i915_span_linear;
	    break;
	case VS_TEXTURE:
	    spans.span = i915_span_texture;
	    break;
	case VS_TEXTURE_16:
	    spans.span = i915_span_texture16;
	    break;
	default:
	    spans.span = i915_span_generic;
	    break;
	}
    } else {
	spans.span = i915_span_generic;
    }

    status = draw_func (draw_closure, &spans.renderer, spans.extents);
    if (spans.clip_region != NULL && status == CAIRO_STATUS_SUCCESS) {
	i915_vbo_finish (device);

	OUT_DWORD (_3DSTATE_SCISSOR_ENABLE_CMD | ENABLE_SCISSOR_RECT);
	for (vbo = &spans.head; vbo != NULL; vbo = vbo->next) {
	    int i, num_rectangles;

	    /* XXX require_space & batch_flush */

	    OUT_DWORD (_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S (0) | I1_LOAD_S (1) | 1);
	    i915_batch_emit_reloc (device, vbo->bo, 0,
				   I915_GEM_DOMAIN_VERTEX, 0,
				   FALSE);
	    OUT_DWORD ((device->floats_per_vertex << S1_VERTEX_WIDTH_SHIFT) |
		       (device->floats_per_vertex << S1_VERTEX_PITCH_SHIFT) |
		       vbo->count);

	    num_rectangles = cairo_region_num_rectangles (spans.clip_region);
	    for (i = 0; i < num_rectangles; i++) {
		cairo_rectangle_int_t rect;

		cairo_region_get_rectangle (spans.clip_region, i, &rect);

		OUT_DWORD (_3DSTATE_SCISSOR_RECT_0_CMD);
		OUT_DWORD (SCISSOR_RECT_0_XMIN (rect.x) |
			   SCISSOR_RECT_0_YMIN (rect.y));
		OUT_DWORD (SCISSOR_RECT_0_XMAX (rect.x + rect.width) |
			   SCISSOR_RECT_0_YMAX (rect.y + rect.height));

		OUT_DWORD (PRIM3D_RECTLIST | PRIM3D_INDIRECT_SEQUENTIAL | vbo->count);
		OUT_DWORD (0);
	    }
	}
	OUT_DWORD (_3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT);
    }

CLEANUP_DEVICE:
    cairo_device_release (dst->intel.drm.base.device);
CLEANUP_SPANS:
    i915_spans_fini (&spans);

    return status;
}
