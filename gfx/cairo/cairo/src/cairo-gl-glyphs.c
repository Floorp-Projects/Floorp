/* Cairo - a vector graphics library with display and print output
 *
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
 * The Initial Developer of the Original Code is Chris Wilson.
 */

#include "cairoint.h"

#include "cairo-gl-private.h"
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
_cairo_gl_glyph_cache_add_glyph (cairo_gl_glyph_cache_t *cache,
				 cairo_scaled_glyph_t  *scaled_glyph)
{
    cairo_image_surface_t *glyph_surface = scaled_glyph->surface;
    cairo_gl_glyph_private_t *glyph_private;
    cairo_rtree_node_t *node = NULL;
    cairo_image_surface_t *clone = NULL;
    cairo_status_t status;
    int width, height;
    GLenum internal_format, format, type;
    cairo_bool_t has_alpha;

    if (! _cairo_gl_get_image_format_and_type (glyph_surface->pixman_format,
					       &internal_format,
					       &format,
					       &type,
					       &has_alpha))
    {
	cairo_bool_t is_supported;

	clone = _cairo_image_surface_coerce (glyph_surface,
		                             _cairo_format_from_content (glyph_surface->base.content));
	if (unlikely (clone->base.status))
	    return clone->base.status;

	is_supported =
	    _cairo_gl_get_image_format_and_type (clone->pixman_format,
					         &internal_format,
						 &format,
						 &type,
						 &has_alpha);
	assert (is_supported);
	glyph_surface = clone;
    }

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

    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei (GL_UNPACK_ROW_LENGTH,
		   glyph_surface->stride /
		   (PIXMAN_FORMAT_BPP (glyph_surface->pixman_format) / 8));
    glTexSubImage2D (GL_TEXTURE_2D, 0,
		     node->x, node->y,
		     glyph_surface->width, glyph_surface->height,
		     format, type,
		     glyph_surface->data);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);

    scaled_glyph->surface_private = node;
    node->owner = &scaled_glyph->surface_private;

    glyph_private = (cairo_gl_glyph_private_t *) node;
    glyph_private->cache = cache;

    /* compute tex coords */
    glyph_private->p1.x = node->x / (double) cache->width;
    glyph_private->p1.y = node->y / (double) cache->height;
    glyph_private->p2.x =
	(node->x + glyph_surface->width) / (double) cache->width;
    glyph_private->p2.y =
	(node->y + glyph_surface->height) / (double) cache->height;

    cairo_surface_destroy (&clone->base);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_gl_glyph_private_t *
_cairo_gl_glyph_cache_lock (cairo_gl_glyph_cache_t *cache,
			    cairo_scaled_glyph_t *scaled_glyph)
{
    return _cairo_rtree_pin (&cache->rtree, scaled_glyph->surface_private);
}

static cairo_gl_glyph_cache_t *
cairo_gl_context_get_glyph_cache (cairo_gl_context_t *ctx,
				  cairo_format_t format)
{
    cairo_gl_glyph_cache_t *cache;

    switch (format) {
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
	cache = &ctx->glyph_cache[0];
	format = CAIRO_FORMAT_ARGB32;
	break;
    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
	cache = &ctx->glyph_cache[1];
	format = CAIRO_FORMAT_A8;
	break;
    }

    if (unlikely (cache->tex == 0)) {
	GLenum internal_format;

	cache->width = GLYPH_CACHE_WIDTH;
	cache->height = GLYPH_CACHE_HEIGHT;

	switch (format) {
	    case CAIRO_FORMAT_A1:
	    case CAIRO_FORMAT_RGB24:
		ASSERT_NOT_REACHED;
	    case CAIRO_FORMAT_ARGB32:
		internal_format = GL_RGBA;
		break;
	    case CAIRO_FORMAT_A8:
		internal_format = GL_ALPHA;
		break;
	}

	glGenTextures (1, &cache->tex);
	glBindTexture (GL_TEXTURE_2D, cache->tex);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D (GL_TEXTURE_2D, 0, internal_format,
		      GLYPH_CACHE_WIDTH, GLYPH_CACHE_HEIGHT, 0,
		      internal_format, GL_FLOAT, NULL);
    }

    return cache;
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
    cairo_gl_context_t *font_private;

    font_private = scaled_font->surface_private;
    if ((scaled_font->surface_backend != NULL &&
	 scaled_font->surface_backend != &_cairo_gl_surface_backend) ||
	(font_private != NULL && font_private != surface->ctx))
    {
	return FALSE;
    }

    return TRUE;
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

typedef struct _cairo_gl_glyphs_setup
{
    unsigned int vbo_size; /* units of floats */
    unsigned int vb_offset; /* units of floats */
    unsigned int vertex_size; /* units of floats */
    unsigned int num_prims;
    float *vb;
    cairo_gl_composite_setup_t *composite;
    cairo_region_t *clip;
    cairo_gl_surface_t *dst;
} cairo_gl_glyphs_setup_t;

static void
_cairo_gl_flush_glyphs (cairo_gl_context_t *ctx,
			cairo_gl_glyphs_setup_t *setup)
{
    int i;

    if (setup->vb != NULL) {
	glUnmapBufferARB (GL_ARRAY_BUFFER_ARB);
	setup->vb = NULL;

	if (setup->num_prims != 0) {
	    if (setup->clip) {
		int num_rectangles = cairo_region_num_rectangles (setup->clip);

		glEnable (GL_SCISSOR_TEST);
		for (i = 0; i < num_rectangles; i++) {
		    cairo_rectangle_int_t rect;

		    cairo_region_get_rectangle (setup->clip, i, &rect);

		    glScissor (rect.x,
			       _cairo_gl_y_flip (setup->dst, rect.y),
			       rect.x + rect.width,
			       _cairo_gl_y_flip (setup->dst,
						 rect.y + rect.height));
		    glDrawArrays (GL_QUADS, 0, 4 * setup->num_prims);
		}
		glDisable (GL_SCISSOR_TEST);
	    } else {
		glDrawArrays (GL_QUADS, 0, 4 * setup->num_prims);
	    }
	    setup->num_prims = 0;
	}
    }
}

static void
_cairo_gl_glyphs_emit_vertex (cairo_gl_glyphs_setup_t *setup,
			      int x, int y, float glyph_x, float glyph_y)
{
    int i = 0;
    float *vb = &setup->vb[setup->vb_offset];
    cairo_surface_attributes_t *src_attributes;

    src_attributes = &setup->composite->src.operand.texture.attributes;

    vb[i++] = x;
    vb[i++] = y;

    vb[i++] = glyph_x;
    vb[i++] = glyph_y;

    if (setup->composite->src.type == OPERAND_TEXTURE) {
	double s = x;
	double t = y;
	cairo_matrix_transform_point (&src_attributes->matrix, &s, &t);
	vb[i++] = s;
	vb[i++] = t;
    }

    setup->vb_offset += setup->vertex_size;
}


static void
_cairo_gl_emit_glyph_rectangle (cairo_gl_context_t *ctx,
				cairo_gl_glyphs_setup_t *setup,
				int x1, int y1,
				int x2, int y2,
				cairo_gl_glyph_private_t *glyph)
{
    if (setup->vb != NULL &&
	setup->vb_offset + 4 * setup->vertex_size > setup->vbo_size) {
	_cairo_gl_flush_glyphs (ctx, setup);
    }

    if (setup->vb == NULL) {
	glBufferDataARB (GL_ARRAY_BUFFER_ARB,
			 setup->vbo_size * sizeof (GLfloat),
			 NULL, GL_STREAM_DRAW_ARB);
	setup->vb = glMapBufferARB (GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	setup->vb_offset = 0;
    }

    _cairo_gl_glyphs_emit_vertex (setup, x1, y1, glyph->p1.x, glyph->p1.y);
    _cairo_gl_glyphs_emit_vertex (setup, x2, y1, glyph->p2.x, glyph->p1.y);
    _cairo_gl_glyphs_emit_vertex (setup, x2, y2, glyph->p2.x, glyph->p2.y);
    _cairo_gl_glyphs_emit_vertex (setup, x1, y2, glyph->p1.x, glyph->p2.y);
    setup->num_prims++;
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
    cairo_format_t last_format = (cairo_format_t) -1;
    cairo_gl_glyph_cache_t *cache = NULL;
    cairo_gl_context_t *ctx;
    cairo_gl_glyphs_setup_t setup;
    cairo_gl_composite_setup_t composite_setup;
    cairo_status_t status;
    int i = 0;
    GLuint vbo = 0;

    *has_component_alpha = FALSE;

    status = _cairo_gl_operand_init (&composite_setup.src, source, dst,
				     glyph_extents->x, glyph_extents->y,
				     dst_x, dst_y,
				     glyph_extents->width,
				     glyph_extents->height);
    if (unlikely (status))
	return status;

    ctx = _cairo_gl_context_acquire (dst->ctx);

    /* Set up the IN operator for source IN mask.
     *
     * IN (normal, any op): dst.argb = src.argb * mask.aaaa
     * IN (component, ADD): dst.argb = src.argb * mask.argb
     *
     * The mask channel selection for component alpha ADD will be updated in
     * the loop over glyphs below.
     */
    glActiveTexture (GL_TEXTURE1);
    glEnable (GL_TEXTURE_2D);
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);

    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE1);
    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE1);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

    glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PREVIOUS);
    glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PREVIOUS);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

    _cairo_gl_set_destination (dst);
    /* If we're doing some CA glyphs, we're only doing it for ADD,
     * which doesn't require the source alpha value in blending.
     */
    _cairo_gl_set_operator (dst, op, FALSE);
    _cairo_gl_set_src_operand (ctx, &composite_setup);

    _cairo_scaled_font_freeze_cache (scaled_font);
    if (! _cairo_gl_surface_owns_font (dst, scaled_font)) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto CLEANUP_FONT;
    }

    if (scaled_font->surface_private == NULL) {
	/* XXX couple into list to remove on context destruction */
	scaled_font->surface_private = ctx;
	scaled_font->surface_backend = &_cairo_gl_surface_backend;
    }

    /* Create our VBO so that we can accumulate a bunch of glyph primitives
     * into one giant DrawArrays.
     */
    memset(&setup, 0, sizeof(setup));
    setup.composite = &composite_setup;
    setup.clip = clip_region;
    setup.dst = dst;
    setup.vertex_size = 4;
    if (composite_setup.src.type == OPERAND_TEXTURE)
	setup.vertex_size += 2;
    setup.vbo_size = num_glyphs * 4 * setup.vertex_size;
    if (setup.vbo_size > 4096)
	setup.vbo_size = 4096;

    glGenBuffersARB (1, &vbo);
    glBindBufferARB (GL_ARRAY_BUFFER_ARB, vbo);

    glVertexPointer (2, GL_FLOAT, setup.vertex_size * sizeof (GLfloat),
		     (void *)(uintptr_t)(0));
    glEnableClientState (GL_VERTEX_ARRAY);
    if (composite_setup.src.type == OPERAND_TEXTURE) {
	/* Note that we're packing texcoord 0 after texcoord 1, for
	 * convenience.
	 */
	glClientActiveTexture (GL_TEXTURE0);
	glTexCoordPointer (2, GL_FLOAT, setup.vertex_size * sizeof (GLfloat),
			   (void *)(uintptr_t)(4 * sizeof (GLfloat)));
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    }
    glClientActiveTexture (GL_TEXTURE1);
    glTexCoordPointer (2, GL_FLOAT, setup.vertex_size * sizeof (GLfloat),
		       (void *)(uintptr_t)(2 * sizeof (GLfloat)));
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);

    for (i = 0; i < num_glyphs; i++) {
	cairo_scaled_glyph_t *scaled_glyph;
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
	    /* Switching textures, so flush any queued prims. */
	    _cairo_gl_flush_glyphs (ctx, &setup);

	    glActiveTexture (GL_TEXTURE1);
	    cache = cairo_gl_context_get_glyph_cache (ctx,
						      scaled_glyph->surface->format);

	    glBindTexture (GL_TEXTURE_2D, cache->tex);

	    last_format = scaled_glyph->surface->format;
	    /* If we're doing component alpha in this function, it should
	     * only be in the case of CAIRO_OPERATOR_ADD.  In that case, we just
	     * need to make sure we send the rgb bits down to the destination.
	     */
	    if (last_format == CAIRO_FORMAT_ARGB32) {
		assert (op == CAIRO_OPERATOR_ADD);
		glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		*has_component_alpha = TRUE;
	    } else {
		glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
	    }

	    /* XXX component alpha */
	}

	if (scaled_glyph->surface_private == NULL) {
	    status = _cairo_gl_glyph_cache_add_glyph (cache, scaled_glyph);

	    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
		/* Cache is full, so flush existing prims and try again. */
		_cairo_gl_flush_glyphs (ctx, &setup);
		_cairo_gl_glyph_cache_unlock (cache);
		status = _cairo_gl_glyph_cache_add_glyph (cache, scaled_glyph);
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

	_cairo_gl_emit_glyph_rectangle (ctx, &setup,
					x1, y1, x2, y2,
					_cairo_gl_glyph_cache_lock (cache, scaled_glyph));
    }

    status = CAIRO_STATUS_SUCCESS;
  FINISH:
    _cairo_gl_flush_glyphs (ctx, &setup);
  CLEANUP_FONT:
    _cairo_scaled_font_thaw_cache (scaled_font);

    glDisable (GL_BLEND);
    glDisable (GL_SCISSOR_TEST);

    glDisableClientState (GL_VERTEX_ARRAY);

    glClientActiveTexture (GL_TEXTURE0);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glActiveTexture (GL_TEXTURE0);
    glDisable (GL_TEXTURE_2D);

    glClientActiveTexture (GL_TEXTURE1);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glActiveTexture (GL_TEXTURE1);
    glDisable (GL_TEXTURE_2D);

    if (vbo != 0) {
	glBindBufferARB (GL_ARRAY_BUFFER_ARB, 0);
	glDeleteBuffersARB (1, &vbo);
    }

    _cairo_gl_context_release (ctx);

    _cairo_gl_operand_destroy (&composite_setup.src);

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
    cairo_solid_pattern_t solid;
    cairo_bool_t has_component_alpha;
    int i;

    /* XXX: For non-CA, this should be CAIRO_CONTENT_ALPHA to save memory */
    mask = cairo_gl_surface_create (dst->ctx,
	                            CAIRO_CONTENT_COLOR_ALPHA,
				    glyph_extents->width,
				    glyph_extents->height);
    if (unlikely (mask->status))
	return mask->status;

    for (i = 0; i < num_glyphs; i++) {
	glyphs[i].x -= glyph_extents->x;
	glyphs[i].y -= glyph_extents->y;
    }

    _cairo_pattern_init_solid (&solid, CAIRO_COLOR_WHITE, CAIRO_CONTENT_COLOR_ALPHA);
    status = _render_glyphs ((cairo_gl_surface_t *) mask, 0, 0,
	                     CAIRO_OPERATOR_ADD, &solid.base,
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
    cairo_solid_pattern_t solid_pattern;
    cairo_bool_t overlap, use_mask = FALSE;
    cairo_bool_t has_component_alpha;
    cairo_status_t status;
    int i;

    if (! GLEW_ARB_vertex_buffer_object)
	return UNSUPPORTED ("requires ARB_vertex_buffer_object");

    if (! _cairo_gl_operator_is_supported (op))
	return UNSUPPORTED ("unsupported operator");

    if (! _cairo_operator_bounded_by_mask (op))
	use_mask |= TRUE;

    /* If any of the glyphs are component alpha, we have to go through a mask,
     * since only _cairo_gl_surface_composite() currently supports component
     * alpha.
     */
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
	_cairo_pattern_init_solid (&solid_pattern, CAIRO_COLOR_WHITE,
				   CAIRO_CONTENT_COLOR);
	source = &solid_pattern.base;
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
_cairo_gl_glyph_cache_fini (cairo_gl_glyph_cache_t *cache)
{
    if (cache->tex == 0)
	return;

    glDeleteTextures (1, &cache->tex);

    _cairo_rtree_fini (&cache->rtree);
}

void
_cairo_gl_glyph_cache_init (cairo_gl_glyph_cache_t *cache)
{
    cache->tex = 0;

    _cairo_rtree_init (&cache->rtree,
		       GLYPH_CACHE_WIDTH,
		       GLYPH_CACHE_HEIGHT,
		       GLYPH_CACHE_MIN_SIZE,
		       sizeof (cairo_gl_glyph_private_t),
		       NULL);
}
