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
#include "cairo-drm-i965-private.h"

/* Operates in either immediate or retained mode.
 * When given a clip region we record the sequence of vbo and then
 * replay them for each clip rectangle, otherwise we simply emit
 * the vbo straight into the command stream.
 */

typedef struct _i965_spans i965_spans_t;

typedef float *
(*i965_get_rectangle_func_t) (i965_spans_t *spans);

struct _i965_spans {
    cairo_span_renderer_t renderer;

    i965_device_t *device;

    int xmin, xmax;
    cairo_bool_t is_bounded;
    const cairo_rectangle_int_t *extents;

    i965_get_rectangle_func_t get_rectangle;
    i965_shader_t shader;

    cairo_region_t *clip_region;

    struct i965_vbo head, *tail;

    unsigned int vbo_offset;
    float *vbo_base;
};

static float *
i965_spans_emit_rectangle (i965_spans_t *spans)
{
    return i965_add_rectangle (spans->device);
}

static float *
i965_spans_accumulate_rectangle (i965_spans_t *spans)
{
    float *vertices;
    uint32_t size;

    size = spans->device->rectangle_size;
    if (unlikely (spans->vbo_offset + size > I965_VERTEX_SIZE)) {
	struct i965_vbo *vbo;

	vbo = _cairo_malloc (sizeof (struct i965_vbo));
	if (unlikely (vbo == NULL)) {
	    /* throw error! */
	}

	spans->tail->next = vbo;
	spans->tail = vbo;

	vbo->next = NULL;
	vbo->bo = intel_bo_create (&spans->device->intel,
				   I965_VERTEX_SIZE, I965_VERTEX_SIZE,
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
i965_span_rectangle (i965_spans_t *spans,
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

static cairo_status_t
i965_bounded_spans_mono (void *abstract_renderer,
			 int y, int height,
			 const cairo_half_open_span_t *half,
			 unsigned num_spans)
{
    i965_spans_t *spans = abstract_renderer;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	if (half[0].coverage >= 128) {
	    i965_span_rectangle (spans,
				 half[0].x, half[1].x,
				 y, y + height,
				 255);
	}
	half++;
    } while (--num_spans > 1);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i965_bounded_spans (void *abstract_renderer,
		    int y, int height,
		    const cairo_half_open_span_t *half,
		    unsigned num_spans)
{
    i965_spans_t *spans = abstract_renderer;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	if (half[0].coverage) {
	    i965_span_rectangle (spans,
				 half[0].x, half[1].x,
				 y, y + height,
				 half[0].coverage);
	}
	half++;
    } while (--num_spans > 1);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i965_unbounded_spans (void *abstract_renderer,
		      int y, int height,
		      const cairo_half_open_span_t *half,
		      unsigned num_spans)
{
    i965_spans_t *spans = abstract_renderer;

    if (num_spans == 0) {
	i965_span_rectangle (spans,
			     spans->xmin, spans->xmax,
			     y, y + height,
			     0);
	return CAIRO_STATUS_SUCCESS;
    }

    if (half[0].x != spans->xmin) {
	i965_span_rectangle (spans,
			     spans->xmin, half[0].x,
			     y, y + height,
			     0);
    }

    do {
	i965_span_rectangle (spans,
			     half[0].x, half[1].x,
			     y, y + height,
			     half[0].coverage);
	half++;
    } while (--num_spans > 1);

    if (half[0].x != spans->xmax) {
	i965_span_rectangle (spans,
			     half[0].x, spans->xmax,
			     y, y + height,
			     0);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i965_unbounded_spans_mono (void *abstract_renderer,
			   int y, int height,
			   const cairo_half_open_span_t *half,
			   unsigned num_spans)
{
    i965_spans_t *spans = abstract_renderer;

    if (num_spans == 0) {
	i965_span_rectangle (spans,
			     spans->xmin, spans->xmax,
			     y, y + height,
			     0);
	return CAIRO_STATUS_SUCCESS;
    }

    if (half[0].x != spans->xmin) {
	i965_span_rectangle (spans,
			     spans->xmin, half[0].x,
			     y, y + height,
			     0);
    }

    do {
	int alpha = 0;
	if (half[0].coverage >= 128)
	    alpha = 255;
	i965_span_rectangle (spans,
			     half[0].x, half[1].x,
			     y, y + height,
			     alpha);
	half++;
    } while (--num_spans > 1);

    if (half[0].x != spans->xmax) {
	i965_span_rectangle (spans,
			     half[0].x, spans->xmax,
			     y, y + height,
			     0);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i965_spans_init (i965_spans_t *spans,
		 i965_surface_t *dst,
		 cairo_operator_t op,
		 const cairo_pattern_t *pattern,
		 cairo_antialias_t antialias,
		 cairo_clip_t *clip,
		 const cairo_composite_rectangles_t *extents)
{
    cairo_status_t status;

    spans->device = i965_device (dst);
    i965_shader_init (&spans->shader, dst, op);

    spans->is_bounded = extents->is_bounded;
    if (extents->is_bounded) {
	if (antialias == CAIRO_ANTIALIAS_NONE)
	    spans->renderer.render_rows = i965_bounded_spans_mono;
	else
	    spans->renderer.render_rows = i965_bounded_spans;

	spans->extents = &extents->bounded;
    } else {
	if (antialias == CAIRO_ANTIALIAS_NONE)
	    spans->renderer.render_rows = i965_unbounded_spans_mono;
	else
	    spans->renderer.render_rows = i965_unbounded_spans;

	spans->extents = &extents->unbounded;
    }
    spans->xmin = spans->extents->x;
    spans->xmax = spans->extents->x + spans->extents->width;

    spans->clip_region = NULL;
    if (clip != NULL) {
	cairo_region_t *clip_region = NULL;

	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status == CAIRO_STATUS_SUCCESS || status == CAIRO_INT_STATUS_UNSUPPORTED);

	if (clip_region != NULL && cairo_region_num_rectangles (clip_region) == 1)
	    clip_region = NULL;

	spans->clip_region = clip_region;
	if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	    i965_shader_set_clip (&spans->shader, clip);
    }

    spans->head.next  = NULL;
    spans->head.bo    = NULL;
    spans->head.count = 0;
    spans->tail = &spans->head;

    if (spans->clip_region == NULL) {
	spans->get_rectangle = i965_spans_emit_rectangle;
    } else {
	spans->get_rectangle = i965_spans_accumulate_rectangle;
	spans->head.bo = intel_bo_create (&spans->device->intel,
					  I965_VERTEX_SIZE, I965_VERTEX_SIZE,
					  FALSE, I915_TILING_NONE, 0);
	if (unlikely (spans->head.bo == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	spans->vbo_base = intel_bo_map (&spans->device->intel, spans->head.bo);
    }
    spans->vbo_offset = 0;

    return i965_shader_acquire_pattern (&spans->shader,
					&spans->shader.source,
					pattern, &extents->bounded);
}

static void
i965_spans_fini (i965_spans_t *spans)
{
    i965_shader_fini (&spans->shader);

    if (spans->head.bo != NULL) {
	struct i965_vbo *vbo, *next;

	intel_bo_destroy (&spans->device->intel, spans->head.bo);
	for (vbo = spans->head.next; vbo != NULL; vbo = next) {
	    next = vbo->next;
	    intel_bo_destroy (&spans->device->intel, vbo->bo);
	    free (vbo);
	}
    }
}

cairo_status_t
i965_clip_and_composite_spans (i965_surface_t		*dst,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*pattern,
			       cairo_antialias_t	 antialias,
			       i965_spans_func_t	 draw_func,
			       void			*draw_closure,
			       const cairo_composite_rectangles_t*extents,
			       cairo_clip_t		*clip)
{
    i965_spans_t spans;
    i965_device_t *device;
    cairo_status_t status;

    if (op == CAIRO_OPERATOR_CLEAR) {
	pattern = &_cairo_pattern_white.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    status = i965_spans_init (&spans, dst, op, pattern, antialias, clip, extents);
    if (unlikely (status))
	return status;

    spans.shader.mask.base.content  = CAIRO_CONTENT_ALPHA;
    spans.shader.mask.type.fragment = FS_SPANS;
    spans.shader.mask.type.vertex   = VS_SPANS;
    spans.shader.mask.type.pattern  = PATTERN_BASE;

    status = cairo_device_acquire (dst->intel.drm.base.device);
    if (unlikely (status))
	goto CLEANUP_SPANS;

    device = i965_device (dst);
    status = i965_shader_commit (&spans.shader, device);
    if (unlikely (status))
	goto CLEANUP_DEVICE;

    status = draw_func (draw_closure, &spans.renderer, spans.extents);
    if (spans.clip_region != NULL && status == CAIRO_STATUS_SUCCESS)
	i965_clipped_vertices (device, &spans.head, spans.clip_region);

  CLEANUP_DEVICE:
    cairo_device_release (dst->intel.drm.base.device);
  CLEANUP_SPANS:
    i965_spans_fini (&spans);

    return status;
}
