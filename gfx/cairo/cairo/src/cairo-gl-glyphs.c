/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Chris Wilson
 * Copyright © 2010 Intel Corporation
 * Copyright © 2010 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Chris Wilson.
 *
 * Contributors:
 *      Benjamin Otte <otte@gnome.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-gl-private.h"

#include "cairo-error-private.h"
#include "cairo-rtree-private.h"

#define GLYPH_CACHE_WIDTH 1024
#define GLYPH_CACHE_HEIGHT 1024
#define GLYPH_CACHE_MIN_SIZE 4
#define GLYPH_CACHE_MAX_SIZE 128

typedef struct _cairo_gl_glyph_private {
    cairo_rtree_node_t node;
    cairo_gl_glyph_cache_t *cache;
    struct { float x, y; } p1, p2;
} cairo_gl_glyph_private_t;

static cairo_status_t
_cairo_gl_glyph_cache_add_glyph (cairo_gl_context_t *ctx,
				 cairo_gl_glyph_cache_t *cache,
				 cairo_scaled_glyph_t  *scaled_glyph)
{
    cairo_image_surface_t *glyph_surface = scaled_glyph->surface;
    cairo_gl_surface_t *cache_surface;
    cairo_gl_glyph_private_t *glyph_private;
    cairo_rtree_node_t *node = NULL;
    cairo_status_t status;
    int width, height;

    width = glyph_surface->width;
    if (width < GLYPH_CACHE_MIN_SIZE)
	width = GLYPH_CACHE_MIN_SIZE;
    height = glyph_surface->height;
    if (height < GLYPH_CACHE_MIN_SIZE)
	height = GLYPH_CACHE_MIN_SIZE;

    /* search for an available slot */
    status = _cairo_rtree_insert (&cache->rtree, width, height, &node);
    /* search for an unlocked slot */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	status = _cairo_rtree_evict_random (&cache->rtree,
				            width, height, &node);
	if (status == CAIRO_STATUS_SUCCESS) {
	    status = _cairo_rtree_node_insert (&cache->rtree,
		                               node, width, height, &node);
	}
    }
    if (status)
	return status;

    cache_surface = (cairo_gl_surface_t *) cache->pattern.surface;

    /* XXX: Make sure we use the mask texture. This should work automagically somehow */
    glActiveTexture (GL_TEXTURE1);
    status = _cairo_gl_surface_draw_image (cache_surface,
                                           glyph_surface,
                                           0, 0,
                                           glyph_surface->width, glyph_surface->height,
                                           node->x, node->y);
    if (unlikely (status))
	return status;

    scaled_glyph->surface_private = node;
    node->owner = &scaled_glyph->surface_private;

    glyph_private = (cairo_gl_glyph_private_t *) node;
    glyph_private->cache = cache;

    /* compute tex coords */
    glyph_private->p1.x = node->x;
    glyph_private->p1.y = node->y;
    glyph_private->p2.x = node->x + glyph_surface->width;
    glyph_private->p2.y = node->y + glyph_surface->height;
    if (! _cairo_gl_device_requires_power_of_two_textures (&ctx->base)) {
	glyph_private->p1.x /= cache_surface->width;
	glyph_private->p1.y /= cache_surface->height;
	glyph_private->p2.x /= cache_surface->width;
	glyph_private->p2.y /= cache_surface->height;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_gl_glyph_private_t *
_cairo_gl_glyph_cache_lock (cairo_gl_glyph_cache_t *cache,
			    cairo_scaled_glyph_t *scaled_glyph)
{
    return _cairo_rtree_pin (&cache->rtree, scaled_glyph->surface_private);
}

static cairo_status_t
cairo_gl_context_get_glyph_cache (cairo_gl_context_t *ctx,
				  cairo_format_t format,
                                  cairo_gl_glyph_cache_t **cache_out)
{
    cairo_gl_glyph_cache_t *cache;
    cairo_content_t content;

    switch (format) {
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
	cache = &ctx->glyph_cache[0];
        content = CAIRO_CONTENT_COLOR_ALPHA;
	break;
    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
	cache = &ctx->glyph_cache[1];
        content = CAIRO_CONTENT_ALPHA;
	break;
    case CAIRO_FORMAT_INVALID:
	ASSERT_NOT_REACHED;
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT);
    }

    if (unlikely (cache->pattern.surface == NULL)) {
        cairo_surface_t *surface;
        surface = cairo_gl_surface_create (&ctx->base,
                                           content,
                                           GLYPH_CACHE_WIDTH,
                                           GLYPH_CACHE_HEIGHT);
        if (unlikely (surface->status)) {
            cairo_status_t status = surface->status;
            cairo_surface_destroy (surface);
            return status;
        }
        _cairo_surface_release_device_reference (surface);
        _cairo_pattern_init_for_surface (&cache->pattern, surface);
        cairo_surface_destroy (surface);
        cache->pattern.base.has_component_alpha = (content == CAIRO_CONTENT_COLOR_ALPHA);
    }

    *cache_out = cache;
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_gl_glyph_cache_unlock (cairo_gl_glyph_cache_t *cache)
{
    _cairo_rtree_unpin (&cache->rtree);
}

static cairo_bool_t
_cairo_gl_surface_owns_font (cairo_gl_surface_t *surface,
			     cairo_scaled_font_t *scaled_font)
{
    cairo_device_t *font_private;

    font_private = scaled_font->surface_private;
    if ((scaled_font->surface_backend != NULL &&
	 scaled_font->surface_backend != &_cairo_gl_surface_backend) ||
	(font_private != NULL && font_private != surface->base.device))
    {
	return FALSE;
    }

    return TRUE;
}

void
_cairo_gl_surface_scaled_font_fini (cairo_scaled_font_t  *scaled_font)
{
    cairo_list_del (&scaled_font->link);
}

void
_cairo_gl_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
				     cairo_scaled_font_t  *scaled_font)
{
    cairo_gl_glyph_private_t *glyph_private;

    glyph_private = scaled_glyph->surface_private;
    if (glyph_private != NULL) {
	glyph_private->node.owner = NULL;
	if (! glyph_private->node.pinned) {
	    /* XXX thread-safety? Probably ok due to the frozen scaled-font. */
	    _cairo_rtree_node_remove (&glyph_private->cache->rtree,
		                      &glyph_private->node);
	}
    }
}

static cairo_status_t
_render_glyphs (cairo_gl_surface_t	*dst,
	        int dst_x, int dst_y,
	        cairo_operator_t	 op,
		const cairo_pattern_t	*source,
		cairo_glyph_t		*glyphs,
		int			 num_glyphs,
		const cairo_rectangle_int_t *glyph_extents,
		cairo_scaled_font_t	*scaled_font,
		cairo_bool_t		*has_component_alpha,
		cairo_region_t		*clip_region,
		int			*remaining_glyphs)
{
    cairo_format_t last_format = CAIRO_FORMAT_INVALID;
    cairo_gl_glyph_cache_t *cache = NULL;
    cairo_gl_context_t *ctx;
    cairo_gl_composite_t setup;
    cairo_status_t status;
    int i = 0;

    *has_component_alpha = FALSE;

    status = _cairo_gl_context_acquire (dst->base.device, &ctx);
    if (unlikely (status))
	return status;

    _cairo_scaled_font_freeze_cache (scaled_font);

    status = _cairo_gl_composite_init (&setup, op, dst,
                                       TRUE, glyph_extents);

    if (unlikely (status))
	goto FINISH;

    if (! _cairo_gl_surface_owns_font (dst, scaled_font)) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto FINISH;
    }

    status = _cairo_gl_composite_set_source (&setup, source,
                                             glyph_extents->x, glyph_extents->y,
                                             dst_x, dst_y,
                                             glyph_extents->width,
                                             glyph_extents->height);
    if (unlikely (status))
	goto FINISH;

    if (scaled_font->surface_private == NULL) {
	scaled_font->surface_private = ctx;
	scaled_font->surface_backend = &_cairo_gl_surface_backend;
	cairo_list_add (&scaled_font->link, &ctx->fonts);
    }

    _cairo_gl_composite_set_clip_region (&setup, clip_region);

    for (i = 0; i < num_glyphs; i++) {
	cairo_scaled_glyph_t *scaled_glyph;
	cairo_gl_glyph_private_t *glyph;
	double x_offset, y_offset;
	double x1, x2, y1, y2;

	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph);
	if (unlikely (status))
	    goto FINISH;

	if (scaled_glyph->surface->width  == 0 ||
	    scaled_glyph->surface->height == 0)
	{
	    continue;
	}
	if (scaled_glyph->surface->width  > GLYPH_CACHE_MAX_SIZE ||
	    scaled_glyph->surface->height > GLYPH_CACHE_MAX_SIZE)
	{
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
	    goto FINISH;
	}

	if (scaled_glyph->surface->format != last_format) {
	    status = cairo_gl_context_get_glyph_cache (ctx,
						       scaled_glyph->surface->format,
                                                       &cache);
            if (unlikely (status))
                goto FINISH;

	    last_format = scaled_glyph->surface->format;

            status = _cairo_gl_composite_set_mask (&setup,
                                                   &cache->pattern.base,
                                                   0, 0, 0, 0, 0, 0);
            if (unlikely (status))
                goto FINISH;

	    *has_component_alpha |= cache->pattern.base.has_component_alpha;

            /* XXX: _cairo_gl_composite_begin() acquires the context a
             * second time. Need to refactor this loop so this doesn't happen.
             */
            status = _cairo_gl_composite_begin (&setup, &ctx);

            status = _cairo_gl_context_release (ctx, status);
	    if (unlikely (status))
		goto FINISH;
	}

	if (scaled_glyph->surface_private == NULL) {
	    status = _cairo_gl_glyph_cache_add_glyph (ctx, cache, scaled_glyph);

	    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
		/* Cache is full, so flush existing prims and try again. */
                _cairo_gl_composite_flush (ctx);
		_cairo_gl_glyph_cache_unlock (cache);
		status = _cairo_gl_glyph_cache_add_glyph (ctx, cache, scaled_glyph);
	    }

	    if (unlikely (_cairo_status_is_error (status)))
		goto FINISH;
	}

	x_offset = scaled_glyph->surface->base.device_transform.x0;
	y_offset = scaled_glyph->surface->base.device_transform.y0;

	x1 = _cairo_lround (glyphs[i].x - x_offset);
	y1 = _cairo_lround (glyphs[i].y - y_offset);
	x2 = x1 + scaled_glyph->surface->width;
	y2 = y1 + scaled_glyph->surface->height;

	glyph = _cairo_gl_glyph_cache_lock (cache, scaled_glyph);
	_cairo_gl_composite_emit_glyph (ctx,
					x1, y1, x2, y2,
                                        glyph->p1.x, glyph->p1.y,
                                        glyph->p2.x, glyph->p2.y);
    }

    status = CAIRO_STATUS_SUCCESS;
  FINISH:
    _cairo_scaled_font_thaw_cache (scaled_font);

    status = _cairo_gl_context_release (ctx, status);

    _cairo_gl_composite_fini (&setup);

    *remaining_glyphs = num_glyphs - i;
    return status;
}

static cairo_int_status_t
_cairo_gl_surface_show_glyphs_via_mask (cairo_gl_surface_t	*dst,
			                cairo_operator_t	 op,
					const cairo_pattern_t	*source,
					cairo_glyph_t		*glyphs,
					int			 num_glyphs,
					const cairo_rectangle_int_t *glyph_extents,
					cairo_scaled_font_t	*scaled_font,
					cairo_clip_t		*clip,
					int			*remaining_glyphs)
{
    cairo_surface_t *mask;
    cairo_status_t status;
    cairo_bool_t has_component_alpha;
    int i;

    /* XXX: For non-CA, this should be CAIRO_CONTENT_ALPHA to save memory */
    mask = cairo_gl_surface_create (dst->base.device,
                                    CAIRO_CONTENT_COLOR_ALPHA,
                                    glyph_extents->width,
                                    glyph_extents->height);
    if (unlikely (mask->status))
        return mask->status;

    for (i = 0; i < num_glyphs; i++) {
	glyphs[i].x -= glyph_extents->x;
	glyphs[i].y -= glyph_extents->y;
    }

    status = _render_glyphs ((cairo_gl_surface_t *) mask, 0, 0,
	                     CAIRO_OPERATOR_ADD,
			     &_cairo_pattern_white.base,
	                     glyphs, num_glyphs, glyph_extents,
			     scaled_font, &has_component_alpha,
			     NULL, remaining_glyphs);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	cairo_surface_pattern_t mask_pattern;

        mask->is_clear = FALSE;
	_cairo_pattern_init_for_surface (&mask_pattern, mask);
	mask_pattern.base.has_component_alpha = has_component_alpha;
	cairo_matrix_init_translate (&mask_pattern.base.matrix,
		                     -glyph_extents->x, -glyph_extents->y);
	status = _cairo_surface_mask (&dst->base, op,
		                      source, &mask_pattern.base, clip);
	_cairo_pattern_fini (&mask_pattern.base);
    } else {
	for (i = 0; i < num_glyphs; i++) {
	    glyphs[i].x += glyph_extents->x;
	    glyphs[i].y += glyph_extents->y;
	}
	*remaining_glyphs = num_glyphs;
    }

    cairo_surface_destroy (mask);

    return status;
}

cairo_int_status_t
_cairo_gl_surface_show_glyphs (void			*abstract_dst,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       cairo_glyph_t		*glyphs,
			       int			 num_glyphs,
			       cairo_scaled_font_t	*scaled_font,
			       cairo_clip_t		*clip,
			       int			*remaining_glyphs)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_rectangle_int_t surface_extents;
    cairo_rectangle_int_t extents;
    cairo_region_t *clip_region = NULL;
    cairo_bool_t overlap, use_mask = FALSE;
    cairo_bool_t has_component_alpha;
    cairo_status_t status;
    int i;

    if (! _cairo_gl_operator_is_supported (op))
	return UNSUPPORTED ("unsupported operator");

    if (! _cairo_operator_bounded_by_mask (op))
	use_mask |= TRUE;

    /* If any of the glyphs are component alpha, we have to go through a mask,
     * since only _cairo_gl_surface_composite() currently supports component
     * alpha.
     */
    if (!use_mask && op != CAIRO_OPERATOR_OVER) {
	for (i = 0; i < num_glyphs; i++) {
	    cairo_scaled_glyph_t *scaled_glyph;

	    status = _cairo_scaled_glyph_lookup (scaled_font,
						 glyphs[i].index,
						 CAIRO_SCALED_GLYPH_INFO_SURFACE,
						 &scaled_glyph);
	    if (!_cairo_status_is_error (status) &&
		scaled_glyph->surface->format == CAIRO_FORMAT_ARGB32)
	    {
		use_mask = TRUE;
		break;
	    }
	}
    }

    /* For CLEAR, cairo's rendering equation (quoting Owen's description in:
     * http://lists.cairographics.org/archives/cairo/2005-August/004992.html)
     * is:
     *     mask IN clip ? src OP dest : dest
     * or more simply:
     *     mask IN CLIP ? 0 : dest
     *
     * where the ternary operator A ? B : C is (A * B) + ((1 - A) * C).
     *
     * The model we use in _cairo_gl_set_operator() is Render's:
     *     src IN mask IN clip OP dest
     * which would boil down to:
     *     0 (bounded by the extents of the drawing).
     *
     * However, we can do a Render operation using an opaque source
     * and DEST_OUT to produce:
     *    1 IN mask IN clip DEST_OUT dest
     * which is
     *    mask IN clip ? 0 : dest
     */
    if (op == CAIRO_OPERATOR_CLEAR) {
	source = &_cairo_pattern_white.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    /* For SOURCE, cairo's rendering equation is:
     *     (mask IN clip) ? src OP dest : dest
     * or more simply:
     *     (mask IN clip) ? src : dest.
     *
     * If we just used the Render equation, we would get:
     *     (src IN mask IN clip) OP dest
     * or:
     *     (src IN mask IN clip) bounded by extents of the drawing.
     *
     * The trick is that for GL blending, we only get our 4 source values
     * into the blender, and since we need all 4 components of source, we
     * can't also get the mask IN clip into the blender.  But if we did
     * two passes we could make it work:
     *     dest = (mask IN clip) DEST_OUT dest
     *     dest = src IN mask IN clip ADD dest
     *
     * But for now, composite via an intermediate mask.
     */
    if (op == CAIRO_OPERATOR_SOURCE)
	use_mask |= TRUE;

    /* XXX we don't need ownership of the font as we use a global
     * glyph cache -- but we do need scaled_glyph eviction notification. :-(
     */
    if (! _cairo_gl_surface_owns_font (dst, scaled_font))
	return UNSUPPORTED ("do not control font");

    /* If the glyphs overlap, we need to build an intermediate mask rather
     * then perform the compositing directly.
     */
    status = _cairo_scaled_font_glyph_device_extents (scaled_font,
						      glyphs, num_glyphs,
						      &extents,
						      &overlap);
    if (unlikely (status))
	return status;

    use_mask |= overlap;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	/* the empty clip should never be propagated this far */
	assert (status != CAIRO_INT_STATUS_NOTHING_TO_DO);
	if (unlikely (_cairo_status_is_error (status)))
	    return status;

	use_mask |= status == CAIRO_INT_STATUS_UNSUPPORTED;

	if (! _cairo_rectangle_intersect (&extents,
		                          _cairo_clip_get_extents (clip)))
	    goto EMPTY;
    }

    surface_extents.x = surface_extents.y = 0;
    surface_extents.width = dst->width;
    surface_extents.height = dst->height;
    if (! _cairo_rectangle_intersect (&extents, &surface_extents))
	goto EMPTY;

    if (use_mask) {
	return _cairo_gl_surface_show_glyphs_via_mask (dst, op,
			                               source,
			                               glyphs, num_glyphs,
						       &extents,
						       scaled_font,
						       clip,
						       remaining_glyphs);
    }

    return _render_glyphs (dst, extents.x, extents.y,
	                   op, source,
			   glyphs, num_glyphs, &extents,
			   scaled_font, &has_component_alpha,
			   clip_region, remaining_glyphs);

EMPTY:
    *remaining_glyphs = 0;
    if (! _cairo_operator_bounded_by_mask (op))
	return _cairo_surface_paint (&dst->base, op, source, clip);
    else
	return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gl_glyph_cache_init (cairo_gl_glyph_cache_t *cache)
{
    _cairo_rtree_init (&cache->rtree,
		       GLYPH_CACHE_WIDTH,
		       GLYPH_CACHE_HEIGHT,
		       GLYPH_CACHE_MIN_SIZE,
		       sizeof (cairo_gl_glyph_private_t));
}

void
_cairo_gl_glyph_cache_fini (cairo_gl_context_t *ctx,
			    cairo_gl_glyph_cache_t *cache)
{
    _cairo_rtree_fini (&cache->rtree);

    if (cache->pattern.surface) {
        _cairo_pattern_fini (&cache->pattern.base);
        cache->pattern.surface = NULL;
    }
}

