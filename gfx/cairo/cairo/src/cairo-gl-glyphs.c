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

#include "cairo-compositor-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-rtree-private.h"

#define GLYPH_CACHE_WIDTH 1024
#define GLYPH_CACHE_HEIGHT 1024
#define GLYPH_CACHE_MIN_SIZE 4
#define GLYPH_CACHE_MAX_SIZE 128

typedef struct _cairo_gl_glyph {
    cairo_rtree_node_t node;
    cairo_scaled_glyph_private_t base;
    cairo_scaled_glyph_t *glyph;
    cairo_gl_glyph_cache_t *cache;
    struct { float x, y; } p1, p2;
} cairo_gl_glyph_t;

static void
_cairo_gl_node_destroy (cairo_rtree_node_t *node)
{
    cairo_gl_glyph_t *priv = cairo_container_of (node, cairo_gl_glyph_t, node);
    cairo_scaled_glyph_t *glyph;

    glyph = priv->glyph;
    if (glyph == NULL)
	    return;

    if (glyph->dev_private_key == priv->cache) {
	    glyph->dev_private = NULL;
	    glyph->dev_private_key = NULL;
    }
    cairo_list_del (&priv->base.link);
    priv->glyph = NULL;
}

static void
_cairo_gl_glyph_fini (cairo_scaled_glyph_private_t *glyph_private,
		      cairo_scaled_glyph_t *scaled_glyph,
		      cairo_scaled_font_t  *scaled_font)
{
    cairo_gl_glyph_t *priv = cairo_container_of (glyph_private,
						 cairo_gl_glyph_t,
						 base);

    assert (priv->glyph);

    _cairo_gl_node_destroy (&priv->node);

    /* XXX thread-safety? Probably ok due to the frozen scaled-font. */
    if (! priv->node.pinned)
	_cairo_rtree_node_remove (&priv->cache->rtree, &priv->node);

    assert (priv->glyph == NULL);
}

static cairo_int_status_t
_cairo_gl_glyph_cache_add_glyph (cairo_gl_context_t *ctx,
				 cairo_gl_glyph_cache_t *cache,
				 cairo_scaled_glyph_t  *scaled_glyph)
{
    cairo_image_surface_t *glyph_surface = scaled_glyph->surface;
    cairo_gl_glyph_t *glyph_private;
    cairo_rtree_node_t *node = NULL;
    cairo_int_status_t status;
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
	if (status == CAIRO_INT_STATUS_SUCCESS) {
	    status = _cairo_rtree_node_insert (&cache->rtree,
					       node, width, height, &node);
	}
    }
    if (status)
	return status;

    /* XXX: Make sure we use the mask texture. This should work automagically somehow */
    glActiveTexture (GL_TEXTURE1);
    status = _cairo_gl_surface_draw_image (cache->surface, glyph_surface,
					   0, 0,
					   glyph_surface->width, glyph_surface->height,
					   node->x, node->y, FALSE);
    if (unlikely (status))
	return status;

    glyph_private = (cairo_gl_glyph_t *) node;
    glyph_private->cache = cache;
    glyph_private->glyph = scaled_glyph;
    _cairo_scaled_glyph_attach_private (scaled_glyph,
					&glyph_private->base,
					cache,
					_cairo_gl_glyph_fini);

    scaled_glyph->dev_private = glyph_private;
    scaled_glyph->dev_private_key = cache;

    /* compute tex coords */
    glyph_private->p1.x = node->x;
    glyph_private->p1.y = node->y;
    glyph_private->p2.x = node->x + glyph_surface->width;
    glyph_private->p2.y = node->y + glyph_surface->height;
    if (! _cairo_gl_device_requires_power_of_two_textures (&ctx->base)) {
	glyph_private->p1.x /= GLYPH_CACHE_WIDTH;
	glyph_private->p2.x /= GLYPH_CACHE_WIDTH;
	glyph_private->p1.y /= GLYPH_CACHE_HEIGHT;
	glyph_private->p2.y /= GLYPH_CACHE_HEIGHT;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_gl_glyph_t *
_cairo_gl_glyph_cache_lock (cairo_gl_glyph_cache_t *cache,
			    cairo_scaled_glyph_t *scaled_glyph)
{
    return _cairo_rtree_pin (&cache->rtree, scaled_glyph->dev_private);
}

static cairo_status_t
cairo_gl_context_get_glyph_cache (cairo_gl_context_t *ctx,
				  cairo_format_t format,
				  cairo_gl_glyph_cache_t **cache_out)
{
    cairo_gl_glyph_cache_t *cache;
    cairo_content_t content;

    switch (format) {
    case CAIRO_FORMAT_RGB30:
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
    default:
    case CAIRO_FORMAT_INVALID:
	ASSERT_NOT_REACHED;
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT);
    }

    if (unlikely (cache->surface == NULL)) {
	cairo_surface_t *surface;

	surface = _cairo_gl_surface_create_scratch_for_caching (ctx,
								content,
								GLYPH_CACHE_WIDTH,
								GLYPH_CACHE_HEIGHT);
	if (unlikely (surface->status))
	    return surface->status;

	_cairo_surface_release_device_reference (surface);

	cache->surface = (cairo_gl_surface_t *)surface;
	cache->surface->operand.texture.attributes.has_component_alpha =
	    content == CAIRO_CONTENT_COLOR_ALPHA;
    }

    *cache_out = cache;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
render_glyphs (cairo_gl_surface_t *dst,
	       int dst_x, int dst_y,
	       cairo_operator_t op,
	       cairo_surface_t *source,
	       cairo_composite_glyphs_info_t *info,
	       cairo_bool_t *has_component_alpha,
	       cairo_clip_t *clip)
{
    cairo_format_t last_format = CAIRO_FORMAT_INVALID;
    cairo_gl_glyph_cache_t *cache = NULL;
    cairo_gl_context_t *ctx;
    cairo_gl_emit_glyph_t emit = NULL;
    cairo_gl_composite_t setup;
    cairo_int_status_t status;
    int i = 0;

    TRACE ((stderr, "%s (%d, %d)x(%d, %d)\n", __FUNCTION__,
	    info->extents.x, info->extents.y,
	    info->extents.width, info->extents.height));

    *has_component_alpha = FALSE;

    status = _cairo_gl_context_acquire (dst->base.device, &ctx);
    if (unlikely (status))
	return status;

    status = _cairo_gl_composite_init (&setup, op, dst, TRUE);
    if (unlikely (status))
	goto FINISH;

    if (source == NULL) {
	    _cairo_gl_composite_set_solid_source (&setup, CAIRO_COLOR_WHITE);
    } else {
	    _cairo_gl_composite_set_source_operand (&setup,
						    source_to_operand (source));

    }

    _cairo_gl_composite_set_clip (&setup, clip);

    for (i = 0; i < info->num_glyphs; i++) {
	cairo_scaled_glyph_t *scaled_glyph;
	cairo_gl_glyph_t *glyph;
	double x_offset, y_offset;
	double x1, x2, y1, y2;

	status = _cairo_scaled_glyph_lookup (info->font,
					     info->glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph);
	if (unlikely (status))
	    goto FINISH;

	if (scaled_glyph->surface->width  == 0 ||
	    scaled_glyph->surface->height == 0)
	{
	    continue;
	}
	if (scaled_glyph->surface->format != last_format) {
	    status = cairo_gl_context_get_glyph_cache (ctx,
						       scaled_glyph->surface->format,
						       &cache);
	    if (unlikely (status))
		goto FINISH;

	    last_format = scaled_glyph->surface->format;

	    _cairo_gl_composite_set_mask_operand (&setup, &cache->surface->operand);
	    *has_component_alpha |= cache->surface->operand.texture.attributes.has_component_alpha;

	    /* XXX Shoot me. */
	    status = _cairo_gl_composite_begin (&setup, &ctx);
	    status = _cairo_gl_context_release (ctx, status);
	    if (unlikely (status))
		goto FINISH;

	    emit = _cairo_gl_context_choose_emit_glyph (ctx);
	}

	if (scaled_glyph->dev_private_key != cache) {
	    cairo_scaled_glyph_private_t *priv;

	    priv = _cairo_scaled_glyph_find_private (scaled_glyph, cache);
	    if (priv) {
		scaled_glyph->dev_private_key = cache;
		scaled_glyph->dev_private = cairo_container_of (priv,
								cairo_gl_glyph_t,
								base);
	    } else {
		status = _cairo_gl_glyph_cache_add_glyph (ctx, cache, scaled_glyph);

		if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
		    /* Cache is full, so flush existing prims and try again. */
		    _cairo_gl_composite_flush (ctx);
		    _cairo_gl_glyph_cache_unlock (cache);
		    status = _cairo_gl_glyph_cache_add_glyph (ctx, cache, scaled_glyph);
		}

		if (unlikely (_cairo_int_status_is_error (status)))
		    goto FINISH;
	    }
	}

	x_offset = scaled_glyph->surface->base.device_transform.x0;
	y_offset = scaled_glyph->surface->base.device_transform.y0;

	x1 = _cairo_lround (info->glyphs[i].x - x_offset - dst_x);
	y1 = _cairo_lround (info->glyphs[i].y - y_offset - dst_y);
	x2 = x1 + scaled_glyph->surface->width;
	y2 = y1 + scaled_glyph->surface->height;

	glyph = _cairo_gl_glyph_cache_lock (cache, scaled_glyph);
	assert (emit);
	emit (ctx,
	      x1, y1, x2, y2,
	      glyph->p1.x, glyph->p1.y,
	      glyph->p2.x, glyph->p2.y);
    }

    status = CAIRO_STATUS_SUCCESS;
  FINISH:
    status = _cairo_gl_context_release (ctx, status);

    _cairo_gl_composite_fini (&setup);
    return status;
}

static cairo_int_status_t
render_glyphs_via_mask (cairo_gl_surface_t *dst,
			int dst_x, int dst_y,
			cairo_operator_t  op,
			cairo_surface_t *source,
			cairo_composite_glyphs_info_t *info,
			cairo_clip_t *clip)
{
    cairo_surface_t *mask;
    cairo_status_t status;
    cairo_bool_t has_component_alpha;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    /* XXX: For non-CA, this should be CAIRO_CONTENT_ALPHA to save memory */
    mask = cairo_gl_surface_create (dst->base.device,
				    CAIRO_CONTENT_COLOR_ALPHA,
				    info->extents.width,
				    info->extents.height);
    if (unlikely (mask->status))
	return mask->status;

    status = render_glyphs ((cairo_gl_surface_t *) mask,
			    info->extents.x, info->extents.y,
			    CAIRO_OPERATOR_ADD, NULL,
			    info, &has_component_alpha, NULL);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	cairo_surface_pattern_t mask_pattern;
	cairo_surface_pattern_t source_pattern;
	cairo_rectangle_int_t clip_extents;

	mask->is_clear = FALSE;
	_cairo_pattern_init_for_surface (&mask_pattern, mask);
	mask_pattern.base.has_component_alpha = has_component_alpha;
	mask_pattern.base.filter = CAIRO_FILTER_NEAREST;
	mask_pattern.base.extend = CAIRO_EXTEND_NONE;

	cairo_matrix_init_translate (&mask_pattern.base.matrix,
				     dst_x-info->extents.x, dst_y-info->extents.y);

	_cairo_pattern_init_for_surface (&source_pattern, source);
	cairo_matrix_init_translate (&source_pattern.base.matrix,
				     dst_x-info->extents.x, dst_y-info->extents.y);

	clip = _cairo_clip_copy (clip);
	clip_extents.x = info->extents.x - dst_x;
	clip_extents.y = info->extents.y - dst_y;
	clip_extents.width = info->extents.width;
	clip_extents.height = info->extents.height;
	clip = _cairo_clip_intersect_rectangle (clip, &clip_extents);

	status = _cairo_surface_mask (&dst->base, op,
				      &source_pattern.base,
				      &mask_pattern.base,
				      clip);

	_cairo_clip_destroy (clip);

	_cairo_pattern_fini (&mask_pattern.base);
	_cairo_pattern_fini (&source_pattern.base);
    }

    cairo_surface_destroy (mask);

    return status;
}

cairo_int_status_t
_cairo_gl_check_composite_glyphs (const cairo_composite_rectangles_t *extents,
				  cairo_scaled_font_t *scaled_font,
				  cairo_glyph_t *glyphs,
				  int *num_glyphs)
{
    if (! _cairo_gl_operator_is_supported (extents->op))
	return UNSUPPORTED ("unsupported operator");

    /* XXX use individual masks for large glyphs? */
    if (ceil (scaled_font->max_scale) >= GLYPH_CACHE_MAX_SIZE)
	return UNSUPPORTED ("glyphs too large");

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_gl_composite_glyphs_with_clip (void			    *_dst,
				      cairo_operator_t		     op,
				      cairo_surface_t		    *_src,
				      int			     src_x,
				      int			     src_y,
				      int			     dst_x,
				      int			     dst_y,
				      cairo_composite_glyphs_info_t *info,
				      cairo_clip_t		    *clip)
{
    cairo_gl_surface_t *dst = _dst;
    cairo_bool_t has_component_alpha;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    /* If any of the glyphs require component alpha, we have to go through
     * a mask, since only _cairo_gl_surface_composite() currently supports
     * component alpha.
     */
    if (!dst->base.is_clear && ! info->use_mask && op != CAIRO_OPERATOR_OVER &&
	(info->font->options.antialias == CAIRO_ANTIALIAS_SUBPIXEL ||
	 info->font->options.antialias == CAIRO_ANTIALIAS_BEST))
    {
	info->use_mask = TRUE;
    }

    if (info->use_mask) {
	return render_glyphs_via_mask (dst, dst_x, dst_y,
				       op, _src, info, clip);
    } else {
	return render_glyphs (dst, dst_x, dst_y,
			      op, _src, info,
			      &has_component_alpha,
			      clip);
    }

}

cairo_int_status_t
_cairo_gl_composite_glyphs (void			*_dst,
			    cairo_operator_t		 op,
			    cairo_surface_t		*_src,
			    int				 src_x,
			    int				 src_y,
			    int				 dst_x,
			    int				 dst_y,
			    cairo_composite_glyphs_info_t *info)
{
    return _cairo_gl_composite_glyphs_with_clip (_dst, op, _src, src_x, src_y,
						 dst_x, dst_y, info, NULL);
}

void
_cairo_gl_glyph_cache_init (cairo_gl_glyph_cache_t *cache)
{
    _cairo_rtree_init (&cache->rtree,
		       GLYPH_CACHE_WIDTH,
		       GLYPH_CACHE_HEIGHT,
		       GLYPH_CACHE_MIN_SIZE,
		       sizeof (cairo_gl_glyph_t),
		       _cairo_gl_node_destroy);
}

void
_cairo_gl_glyph_cache_fini (cairo_gl_context_t *ctx,
			    cairo_gl_glyph_cache_t *cache)
{
    _cairo_rtree_fini (&cache->rtree);
    cairo_surface_destroy (&cache->surface->base);
}
