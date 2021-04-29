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
#include "cairo-drm-i965-private.h"
#include "cairo-error-private.h"
#include "cairo-rtree-private.h"

typedef struct _i965_glyphs i965_glyphs_t;

typedef float *
(*i965_get_rectangle_func_t) (i965_glyphs_t *glyphs);

struct _i965_glyphs {
    i965_get_rectangle_func_t get_rectangle;
    i965_shader_t shader;

    struct i965_vbo head, *tail;

    unsigned int vbo_offset;
    float *vbo_base;
};

static float *
i965_glyphs_emit_rectangle (i965_glyphs_t *glyphs)
{
    return i965_add_rectangle (glyphs->shader.device);
}

static float *
i965_glyphs_accumulate_rectangle (i965_glyphs_t *glyphs)
{
    float *vertices;
    uint32_t size;

    size = glyphs->shader.device->rectangle_size;
    if (unlikely (glyphs->vbo_offset + size > I965_VERTEX_SIZE)) {
	struct i965_vbo *vbo;

	vbo = _cairo_malloc (sizeof (struct i965_vbo));
	if (unlikely (vbo == NULL)) {
	    /* throw error! */
	}

	glyphs->tail->next = vbo;
	glyphs->tail = vbo;

	vbo->next = NULL;
	vbo->bo = intel_bo_create (&glyphs->shader.device->intel,
				   I965_VERTEX_SIZE, I965_VERTEX_SIZE,
				   FALSE, I915_TILING_NONE, 0);
	vbo->count = 0;

	glyphs->vbo_offset = 0;
	glyphs->vbo_base = intel_bo_map (&glyphs->shader.device->intel, vbo->bo);
    }

    vertices = glyphs->vbo_base + glyphs->vbo_offset;
    glyphs->vbo_offset += size;
    glyphs->tail->count += 3;

    return vertices;
}

static void
i965_add_glyph_rectangle (i965_glyphs_t *glyphs,
			  int x1, int y1,
			  int x2, int y2,
			  intel_glyph_t *glyph)
{
    float *v;

    /* Each vertex is:
     *   2 vertex coordinates
     *   1 glyph texture coordinate
     */

    v = glyphs->get_rectangle (glyphs);

    /* bottom right */
    *v++ = x2; *v++ = y2;
    *v++ = glyph->texcoord[0];

    /* bottom left */
    *v++ = x1; *v++ = y2;
    *v++ = glyph->texcoord[1];

    /* top left */
    *v++ = x1; *v++ = y1;
    *v++ = glyph->texcoord[2];
}

static cairo_status_t
i965_surface_mask_internal (i965_surface_t *dst,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*source,
			    i965_surface_t *mask,
			    cairo_clip_t		*clip,
			    const cairo_composite_rectangles_t *extents)
{
    i965_device_t *device;
    i965_shader_t shader;
    cairo_region_t *clip_region = NULL;
    cairo_status_t status;

    i965_shader_init (&shader, dst, op);

    status = i965_shader_acquire_pattern (&shader, &shader.source,
					  source, &extents->bounded);
    if (unlikely (status))
	return status;

    shader.mask.type.vertex = VS_NONE;
    shader.mask.type.fragment = FS_SURFACE;
    shader.mask.base.content = mask->intel.drm.base.content;
    shader.mask.base.filter = i965_filter (CAIRO_FILTER_NEAREST);
    shader.mask.base.extend = i965_extend (CAIRO_EXTEND_NONE);

    cairo_matrix_init_translate (&shader.mask.base.matrix,
				 -extents->bounded.x,
				 -extents->bounded.y);
    cairo_matrix_scale (&shader.mask.base.matrix,
			1. / mask->intel.drm.width,
			1. / mask->intel.drm.height);

    shader.mask.base.bo = to_intel_bo (mask->intel.drm.bo);
    shader.mask.base.format = mask->intel.drm.format;
    shader.mask.base.width = mask->intel.drm.width;
    shader.mask.base.height = mask->intel.drm.height;
    shader.mask.base.stride = mask->intel.drm.stride;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status == CAIRO_STATUS_SUCCESS || status == CAIRO_INT_STATUS_UNSUPPORTED);

	if (clip_region != NULL && cairo_region_num_rectangles (clip_region) == 1)
	    clip_region = NULL;

	if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	    i965_shader_set_clip (&shader, clip);
    }

    status = cairo_device_acquire (dst->intel.drm.base.device);
    if (unlikely (status))
	goto CLEANUP_SHADER;

    device = i965_device (dst);

    status = i965_shader_commit (&shader, device);
    if (unlikely (status))
	goto CLEANUP_DEVICE;

    if (clip_region != NULL) {
	unsigned int n, num_rectangles;

	num_rectangles = cairo_region_num_rectangles (clip_region);
	for (n = 0; n < num_rectangles; n++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (clip_region, n, &rect);

	    i965_shader_add_rectangle (&shader,
				       rect.x, rect.y,
				       rect.width, rect.height);
	}
    } else {
	i965_shader_add_rectangle (&shader,
				   extents->bounded.x,
				   extents->bounded.y,
				   extents->bounded.width,
				   extents->bounded.height);
    }

    if (! extents->is_bounded)
	status = i965_fixup_unbounded (dst, extents, clip);

  CLEANUP_DEVICE:
    cairo_device_release (&device->intel.base.base);
  CLEANUP_SHADER:
    i965_shader_fini (&shader);
    return status;
}

cairo_int_status_t
i965_surface_glyphs (void			*abstract_surface,
		     cairo_operator_t		 op,
		     const cairo_pattern_t	*source,
		     cairo_glyph_t		*g,
		     int			 num_glyphs,
		     cairo_scaled_font_t	*scaled_font,
		     cairo_clip_t		*clip,
		     int *num_remaining)
{
    i965_surface_t *surface = abstract_surface;
    i965_surface_t *mask = NULL;
    i965_device_t *device;
    i965_glyphs_t glyphs;
    cairo_composite_rectangles_t extents;
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    cairo_bool_t overlap;
    cairo_region_t *clip_region = NULL;
    intel_bo_t *last_bo = NULL;
    cairo_scaled_glyph_t *glyph_cache[64];
    cairo_status_t status;
    int mask_x = 0, mask_y = 0;
    int i = 0;

    *num_remaining = 0;
    status = _cairo_composite_rectangles_init_for_glyphs (&extents,
							  surface->intel.drm.width,
							  surface->intel.drm.height,
							  op, source,
							  scaled_font,
							  g, num_glyphs,
							  clip,
							  &overlap);
    if (unlikely (status))
	return status;

    if (clip != NULL && _cairo_clip_contains_rectangle (clip, &extents.mask))
	clip = NULL;

    if (clip != NULL && extents.is_bounded) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	status = _cairo_clip_rectangle (clip, &extents.bounded);
	if (unlikely (status))
	    return status;

	have_clip = TRUE;
    }

    if (overlap || ! extents.is_bounded) {
	cairo_format_t format;

	format = CAIRO_FORMAT_A8;
	if (scaled_font->options.antialias == CAIRO_ANTIALIAS_SUBPIXEL)
	    format = CAIRO_FORMAT_ARGB32;

	mask = (i965_surface_t *)
	    i965_surface_create_internal (&i965_device (surface)->intel.base,
					  format,
					  extents.bounded.width,
					  extents.bounded.height,
					  I965_TILING_DEFAULT,
					  TRUE);
	if (unlikely (mask->intel.drm.base.status))
	    return mask->intel.drm.base.status;

	status = _cairo_surface_paint (&mask->intel.drm.base,
				       CAIRO_OPERATOR_CLEAR,
				       &_cairo_pattern_clear.base,
				       NULL);
	if (unlikely (status)) {
	    cairo_surface_destroy (&mask->intel.drm.base);
	    return status;
	}

	i965_shader_init (&glyphs.shader, mask, CAIRO_OPERATOR_ADD);

	status = i965_shader_acquire_pattern (&glyphs.shader, &glyphs.shader.source,
					      &_cairo_pattern_white.base,
					      &extents.bounded);
	if (unlikely (status)) {
	    cairo_surface_destroy (&mask->intel.drm.base);
	    return status;
	}

	mask_x = -extents.bounded.x;
	mask_y = -extents.bounded.y;
    } else {
	i965_shader_init (&glyphs.shader, surface, op);

	status = i965_shader_acquire_pattern (&glyphs.shader, &glyphs.shader.source,
					      source, &extents.bounded);
	if (unlikely (status))
	    return status;

	if (clip != NULL) {
	    status = _cairo_clip_get_region (clip, &clip_region);
	    assert (status == CAIRO_STATUS_SUCCESS || status == CAIRO_INT_STATUS_UNSUPPORTED);

	    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
		i965_shader_set_clip (&glyphs.shader, clip);
	}
    }

    glyphs.head.next = NULL;
    glyphs.head.bo = NULL;
    glyphs.head.count = 0;
    glyphs.tail = &glyphs.head;

    device = i965_device (surface);
    if (mask != NULL || clip_region == NULL) {
	glyphs.get_rectangle = i965_glyphs_emit_rectangle;
    } else {
	glyphs.get_rectangle = i965_glyphs_accumulate_rectangle;
	glyphs.head.bo = intel_bo_create (&device->intel,
					  I965_VERTEX_SIZE, I965_VERTEX_SIZE,
					  FALSE, I915_TILING_NONE, 0);
	if (unlikely (glyphs.head.bo == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	glyphs.vbo_base = intel_bo_map (&device->intel, glyphs.head.bo);
    }
    glyphs.vbo_offset = 0;

    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	goto CLEANUP_GLYPHS;

    _cairo_scaled_font_freeze_cache (scaled_font);
    //private = _cairo_scaled_font_get_device (scaled_font, device);
    if (scaled_font->surface_private == NULL) {
	scaled_font->surface_private = device;
	scaled_font->surface_backend = surface->intel.drm.base.backend;
	cairo_list_add (&scaled_font->link, &device->intel.fonts);
    }

    memset (glyph_cache, 0, sizeof (glyph_cache));

    for (i = 0; i < num_glyphs; i++) {
	cairo_scaled_glyph_t *scaled_glyph;
	int x, y, x1, x2, y1, y2;
	int cache_index = g[i].index % ARRAY_LENGTH (glyph_cache);
	intel_glyph_t *glyph;

	scaled_glyph = glyph_cache[cache_index];
	if (scaled_glyph == NULL ||
	    _cairo_scaled_glyph_index (scaled_glyph) != g[i].index)
	{
	    status = _cairo_scaled_glyph_lookup (scaled_font,
						 g[i].index,
						 CAIRO_SCALED_GLYPH_INFO_METRICS,
						 &scaled_glyph);
	    if (unlikely (status))
		goto FINISH;

	    glyph_cache[cache_index] = scaled_glyph;
	}

	if (unlikely (scaled_glyph->metrics.width  == 0 ||
		      scaled_glyph->metrics.height == 0))
	{
	    continue;
	}

	/* XXX glyph images are snapped to pixel locations */
	x = _cairo_lround (g[i].x);
	y = _cairo_lround (g[i].y);

	x1 = x + _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.x);
	y1 = y + _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.y);
	x2 = x + _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.x);
	y2 = y + _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.y);

	if (x2 < extents.bounded.x ||
	    y2 < extents.bounded.y ||
	    x1 > extents.bounded.x + extents.bounded.width ||
	    y1 > extents.bounded.y + extents.bounded.height)
	{
	    continue;
	}

	if (scaled_glyph->surface_private == NULL) {
	    status = intel_get_glyph (&device->intel, scaled_font, scaled_glyph);
	    if (unlikely (status == CAIRO_INT_STATUS_NOTHING_TO_DO)) {
		status = CAIRO_STATUS_SUCCESS;
		continue;
	    }
	    if (unlikely (status))
		goto FINISH;
	}
	glyph = intel_glyph_pin (scaled_glyph->surface_private);

	if (glyph->cache->buffer.bo != last_bo) {
	    intel_buffer_cache_t *cache = glyph->cache;

	    glyphs.shader.mask.type.vertex   = VS_GLYPHS;
	    glyphs.shader.mask.type.fragment = FS_GLYPHS;
	    glyphs.shader.mask.type.pattern  = PATTERN_BASE;

	    glyphs.shader.mask.base.bo = cache->buffer.bo;
	    glyphs.shader.mask.base.format = cache->buffer.format;
	    glyphs.shader.mask.base.width  = cache->buffer.width;
	    glyphs.shader.mask.base.height = cache->buffer.height;
	    glyphs.shader.mask.base.stride = cache->buffer.stride;
	    glyphs.shader.mask.base.filter = i965_filter (CAIRO_FILTER_NEAREST);
	    glyphs.shader.mask.base.extend = i965_extend (CAIRO_EXTEND_NONE);
	    glyphs.shader.mask.base.content = CAIRO_CONTENT_ALPHA; /* XXX */

	    glyphs.shader.committed = FALSE;
	    status = i965_shader_commit (&glyphs.shader, device);
	    if (unlikely (status))
		goto FINISH;

	    last_bo = cache->buffer.bo;
	}

	x2 = x1 + glyph->width;
	y2 = y1 + glyph->height;

	if (mask_x)
	    x1 += mask_x, x2 += mask_x;
	if (mask_y)
	    y1 += mask_y, y2 += mask_y;

	i965_add_glyph_rectangle (&glyphs, x1, y1, x2, y2, glyph);
    }

    if (mask != NULL && clip_region != NULL)
	i965_clipped_vertices (device, &glyphs.head, clip_region);

    status = CAIRO_STATUS_SUCCESS;
  FINISH:
    _cairo_scaled_font_thaw_cache (scaled_font);
    cairo_device_release (surface->intel.drm.base.device);
  CLEANUP_GLYPHS:
    i965_shader_fini (&glyphs.shader);

    if (glyphs.head.bo != NULL) {
	struct i965_vbo *vbo, *next;

	intel_bo_destroy (&device->intel, glyphs.head.bo);
	for (vbo = glyphs.head.next; vbo != NULL; vbo = next) {
	    next = vbo->next;
	    intel_bo_destroy (&device->intel, vbo->bo);
	    free (vbo);
	}
    }

    if (unlikely (status == CAIRO_INT_STATUS_UNSUPPORTED)) {
	cairo_path_fixed_t path;

	_cairo_path_fixed_init (&path);
	status = _cairo_scaled_font_glyph_path (scaled_font,
						g + i, num_glyphs - i,
						&path);
	if (mask_x | mask_y) {
	    _cairo_path_fixed_translate (&path,
					 _cairo_fixed_from_int (mask_x),
					 _cairo_fixed_from_int (mask_y));
	}
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    status = surface->intel.drm.base.backend->fill (glyphs.shader.target,
							    glyphs.shader.op,
							    mask != NULL ? &_cairo_pattern_white.base : source,
							    &path,
							    CAIRO_FILL_RULE_WINDING,
							    0,
							    scaled_font->options.antialias,
							    clip);
	}
	_cairo_path_fixed_fini (&path);
    }

    if (mask != NULL) {
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    status = i965_surface_mask_internal (surface, op, source, mask,
					         clip, &extents);
	}
	cairo_surface_finish (&mask->intel.drm.base);
	cairo_surface_destroy (&mask->intel.drm.base);
    }

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}
