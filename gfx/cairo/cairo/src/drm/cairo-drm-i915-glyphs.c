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
#include "cairo-drm-i915-private.h"
#include "cairo-error-private.h"
#include "cairo-rtree-private.h"
#include "cairo-clip-inline.h"

static void
i915_emit_glyph_rectangle_zero (i915_device_t *device,
				i915_shader_t *shader,
				int x1, int y1,
				int x2, int y2,
				intel_glyph_t *glyph)
{
    float *v;

    /* Each vertex is:
     *   2 vertex coordinates
     */

    v = i915_add_rectangle (device);
    *v++ = x2; *v++ = y2;
    *v++ = x1; *v++ = y2;
    *v++ = x1; *v++ = y1;
}

static void
i915_emit_glyph_rectangle_constant (i915_device_t *device,
				    i915_shader_t *shader,
				    int x1, int y1,
				    int x2, int y2,
				    intel_glyph_t *glyph)
{
    float *v;

    /* Each vertex is:
     *   2 vertex coordinates
     *   2 glyph texture coordinates
     */

    v = i915_add_rectangle (device);

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

static void
i915_emit_glyph_rectangle_general (i915_device_t *device,
				   i915_shader_t *shader,
				   int x1, int y1,
				   int x2, int y2,
				   intel_glyph_t *glyph)
{
    double s, t;
    float *v;

    /* Each vertex is:
     *   2 vertex coordinates
     *   [0-2] source texture coordinates
     *   2 glyph texture coordinates
     */

    v = i915_add_rectangle (device);

    /* bottom right */
    *v++ = x2; *v++ = y2;
    s = x2, t = y2;
    switch (shader->source.type.vertex) {
    case VS_ZERO:
    case VS_CONSTANT:
	break;
    case VS_LINEAR:
	*v++ = i915_shader_linear_texcoord (&shader->source.linear, s, t);
	break;
    case VS_TEXTURE:
	cairo_matrix_transform_point (&shader->source.base.matrix, &s, &t);
	*v++ = s; *v++ = t;
	break;
    case VS_TEXTURE_16:
	cairo_matrix_transform_point (&shader->source.base.matrix, &s, &t);
	*v++ = texcoord_2d_16 (s, t);
	break;
    }
    *v++ = glyph->texcoord[0];

    /* bottom left */
    *v++ = x1; *v++ = y2;
    s = x1, t = y2;
    switch (shader->source.type.vertex) {
    case VS_ZERO:
    case VS_CONSTANT:
	break;
    case VS_LINEAR:
	*v++ = i915_shader_linear_texcoord (&shader->source.linear, s, t);
	break;
    case VS_TEXTURE:
	cairo_matrix_transform_point (&shader->source.base.matrix, &s, &t);
	*v++ = s; *v++ = t;
	break;
    case VS_TEXTURE_16:
	cairo_matrix_transform_point (&shader->source.base.matrix, &s, &t);
	*v++ = texcoord_2d_16 (s, t);
	break;
    }
    *v++ = glyph->texcoord[1];

    /* top left */
    *v++ = x1; *v++ = y1;
    s = x1, t = y2;
    switch (shader->source.type.vertex) {
    case VS_ZERO:
    case VS_CONSTANT:
	break;
    case VS_LINEAR:
	*v++ = i915_shader_linear_texcoord (&shader->source.linear, s, t);
	break;
    case VS_TEXTURE:
	cairo_matrix_transform_point (&shader->source.base.matrix, &s, &t);
	*v++ = s; *v++ = t;
	break;
    case VS_TEXTURE_16:
	cairo_matrix_transform_point (&shader->source.base.matrix, &s, &t);
	*v++ = texcoord_2d_16 (s, t);
	break;
    }
    *v++ = glyph->texcoord[2];
}

typedef void
(*i915_emit_glyph_rectangle_func_t) (i915_device_t *device,
				     i915_shader_t *shader,
				     int x1, int y1,
				     int x2, int y2,
				     intel_glyph_t *glyph);

static cairo_status_t
i915_surface_mask_internal (i915_surface_t *dst,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*source,
			    i915_surface_t *mask,
			    cairo_clip_t		*clip,
			    const cairo_composite_rectangles_t *extents)
{
    i915_device_t *device;
    i915_shader_t shader;
    cairo_region_t *clip_region = NULL;
    cairo_status_t status;

    i915_shader_init (&shader, dst, op, 1.);

    status = i915_shader_acquire_pattern (&shader, &shader.source,
					  source, &extents->bounded);
    if (unlikely (status))
	return status;

    shader.mask.type.vertex = VS_TEXTURE_16;
    shader.mask.type.pattern = PATTERN_TEXTURE;
    shader.mask.type.fragment = FS_TEXTURE;
    shader.mask.base.content = mask->intel.drm.base.content;
    shader.mask.base.texfmt = TEXCOORDFMT_2D_16;
    shader.mask.base.n_samplers = 1;
    shader.mask.base.sampler[0] =
	(MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT) |
	i915_texture_filter (CAIRO_FILTER_NEAREST);
    shader.mask.base.sampler[1] =
	SS3_NORMALIZED_COORDS |
	i915_texture_extend (CAIRO_EXTEND_NONE);

    cairo_matrix_init_translate (&shader.mask.base.matrix,
				 -extents->bounded.x,
				 -extents->bounded.y);
    cairo_matrix_scale (&shader.mask.base.matrix,
			1. / mask->intel.drm.width,
			1. / mask->intel.drm.height);

    shader.mask.base.bo = intel_bo_reference (to_intel_bo (mask->intel.drm.bo));
    shader.mask.base.offset[0] = 0;
    shader.mask.base.map[0] = mask->map0;
    shader.mask.base.map[1] = mask->map1;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);

	if (clip_region != NULL && cairo_region_num_rectangles (clip_region) == 1)
	    clip_region = NULL;

	if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	    i915_shader_set_clip (&shader, clip);
    }

    status = cairo_device_acquire (dst->intel.drm.base.device);
    if (unlikely (status))
	goto CLEANUP_SHADER;

    device = i915_device (dst);

    status = i915_shader_commit (&shader, device);
    if (unlikely (status))
	goto CLEANUP_DEVICE;

    if (clip_region != NULL) {
	unsigned int n, num_rectangles;

	num_rectangles = cairo_region_num_rectangles (clip_region);
	for (n = 0; n < num_rectangles; n++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (clip_region, n, &rect);

	    shader.add_rectangle (&shader,
				  rect.x, rect.y,
				  rect.x + rect.width, rect.y + rect.height);
	}
    } else {
	shader.add_rectangle (&shader,
			      extents->bounded.x, extents->bounded.y,
			      extents->bounded.x + extents->bounded.width,
			      extents->bounded.y + extents->bounded.height);
    }

    if (! extents->is_bounded)
	status = i915_fixup_unbounded (dst, extents, clip);

CLEANUP_DEVICE:
    cairo_device_release (&device->intel.base.base);
CLEANUP_SHADER:
    i915_shader_fini (&shader);
    return status;
}

cairo_int_status_t
i915_surface_glyphs (void			*abstract_surface,
		     cairo_operator_t		 op,
		     const cairo_pattern_t	*source,
		     cairo_glyph_t		*glyphs,
		     int			 num_glyphs,
		     cairo_scaled_font_t	*scaled_font,
		     cairo_clip_t		*clip,
		     int *num_remaining)
{
    i915_surface_t *surface = abstract_surface;
    i915_surface_t *mask = NULL;
    i915_device_t *device;
    i915_shader_t shader;
    cairo_composite_rectangles_t extents;
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    cairo_bool_t overlap;
    cairo_region_t *clip_region = NULL;
    intel_bo_t *last_bo = NULL;
    i915_emit_glyph_rectangle_func_t emit_func;
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
							  glyphs, num_glyphs,
							  clip,
							  &overlap);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_rectangle (clip, &extents.mask))
	clip = NULL;

    if (clip != NULL && extents.is_bounded) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	status = _cairo_clip_rectangle (clip, &extents.bounded);
	if (unlikely (status))
	    return status;

	have_clip = TRUE;
    }

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (_cairo_status_is_error (status) ||
		      status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	{
	    if (have_clip)
		_cairo_clip_fini (&local_clip);
	    return status;
	}
    }

    if (i915_surface_needs_tiling (surface)) {
	ASSERT_NOT_REACHED;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (overlap || ! extents.is_bounded) {
	cairo_format_t format;

	format = CAIRO_FORMAT_A8;
	if (scaled_font->options.antialias == CAIRO_ANTIALIAS_SUBPIXEL)
	    format = CAIRO_FORMAT_ARGB32;

	mask = (i915_surface_t *)
	    i915_surface_create_internal (&i915_device (surface)->intel.base,
					  format,
					  extents.bounded.width,
					  extents.bounded.height,
					  I915_TILING_DEFAULT,
					  TRUE);
	if (unlikely (mask->intel.drm.base.status))
	    return mask->intel.drm.base.status;

	status = i915_surface_clear (mask);
	if (unlikely (status)) {
	    cairo_surface_destroy (&mask->intel.drm.base);
	    return status;
	}

	i915_shader_init (&shader, mask, CAIRO_OPERATOR_ADD, 1.);

	status = i915_shader_acquire_pattern (&shader, &shader.source,
					      &_cairo_pattern_white.base,
					      &extents.bounded);
	if (unlikely (status)) {
	    cairo_surface_destroy (&mask->intel.drm.base);
	    return status;
	}

	mask_x = -extents.bounded.x;
	mask_y = -extents.bounded.y;
    } else {
	i915_shader_init (&shader, surface, op, 1.);

	status = i915_shader_acquire_pattern (&shader, &shader.source,
					      source, &extents.bounded);
	if (unlikely (status))
	    return status;

	if (clip != NULL) {
	    status = _cairo_clip_get_region (clip, &clip_region);

	    if (clip_region != NULL && cairo_region_num_rectangles (clip_region) == 1)
		clip_region = NULL;

	    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
		i915_shader_set_clip (&shader, clip);
	}
    }

    shader.mask.type.fragment = FS_TEXTURE;
    shader.mask.base.content = CAIRO_CONTENT_ALPHA; /* XXX */
    shader.mask.base.texfmt = TEXCOORDFMT_2D_16;
    shader.mask.base.n_samplers = 1;
    shader.mask.base.sampler[0] =
	(MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT) |
	i915_texture_filter (CAIRO_FILTER_NEAREST);
    shader.mask.base.sampler[1] =
	SS3_NORMALIZED_COORDS |
	i915_texture_extend (CAIRO_EXTEND_NONE);

    switch (shader.source.type.vertex) {
    case VS_ZERO:
	emit_func = i915_emit_glyph_rectangle_zero;
	break;
    case VS_CONSTANT:
	emit_func = i915_emit_glyph_rectangle_constant;
	break;
    default:
    case VS_LINEAR:
    case VS_TEXTURE:
    case VS_TEXTURE_16:
	emit_func = i915_emit_glyph_rectangle_general;
	break;
    }

    status = cairo_device_acquire (surface->intel.drm.base.device);
    if (unlikely (status))
	goto CLEANUP_SHADER;

    device = i915_device (surface);

    _cairo_scaled_font_freeze_cache (scaled_font);
    if (scaled_font->surface_private == NULL) {
	scaled_font->surface_private = device;
	scaled_font->surface_backend = surface->intel.drm.base.backend;
	cairo_list_add (&scaled_font->link, &device->intel.fonts);
    }

    memset (glyph_cache, 0, sizeof (glyph_cache));

    for (i = 0; i < num_glyphs; i++) {
	cairo_scaled_glyph_t *scaled_glyph;
	int x, y, x1, x2, y1, y2;
	int cache_index = glyphs[i].index % ARRAY_LENGTH (glyph_cache);
	intel_glyph_t *glyph;

	scaled_glyph = glyph_cache[cache_index];
	if (scaled_glyph == NULL ||
	    _cairo_scaled_glyph_index (scaled_glyph) != glyphs[i].index)
	{
	    status = _cairo_scaled_glyph_lookup (scaled_font,
						 glyphs[i].index,
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
	x = _cairo_lround (glyphs[i].x);
	y = _cairo_lround (glyphs[i].y);

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

	    shader.mask.base.bo = cache->buffer.bo;
	    shader.mask.base.offset[0] = cache->buffer.offset;
	    shader.mask.base.map[0] = cache->buffer.map0;
	    shader.mask.base.map[1] = cache->buffer.map1;
	    shader.mask.base.content = CAIRO_CONTENT_ALPHA; /* XXX */

	    status = i915_shader_commit (&shader, device);
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

	/* XXX clip glyph */
	emit_func (device, &shader, x1, y1, x2, y2, glyph);
    }

    status = CAIRO_STATUS_SUCCESS;
  FINISH:
    _cairo_scaled_font_thaw_cache (scaled_font);
    cairo_device_release (surface->intel.drm.base.device);
  CLEANUP_SHADER:
    i915_shader_fini (&shader);

    if (unlikely (status == CAIRO_INT_STATUS_UNSUPPORTED)) {
	cairo_path_fixed_t path;

	_cairo_path_fixed_init (&path);
	status = _cairo_scaled_font_glyph_path (scaled_font,
						glyphs + i, num_glyphs - i,
						&path);
	if (mask_x | mask_y) {
	    _cairo_path_fixed_translate (&path,
					 _cairo_fixed_from_int (mask_x),
					 _cairo_fixed_from_int (mask_y));
	}
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    status = surface->intel.drm.base.backend->fill (shader.target,
							    shader.op,
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
	    status = i915_surface_mask_internal (surface, op, source, mask,
					         clip, &extents);
	}
	cairo_surface_finish (&mask->intel.drm.base);
	cairo_surface_destroy (&mask->intel.drm.base);
    }

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}
