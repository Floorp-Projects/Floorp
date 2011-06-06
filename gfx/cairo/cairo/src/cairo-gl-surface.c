/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005,2010 Red Hat, Inc
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
 *	Benjamin Otte <otte@gnome.org>
 *	Carl Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Eric Anholt <eric@anholt.net>
 */

#include "cairoint.h"

#include "cairo-composite-rectangles-private.h"
#include "cairo-error-private.h"
#include "cairo-gl-private.h"

static cairo_int_status_t
_cairo_gl_surface_fill_rectangles (void			   *abstract_dst,
				   cairo_operator_t	    op,
				   const cairo_color_t     *color,
				   cairo_rectangle_int_t   *rects,
				   int			    num_rects);

static cairo_int_status_t
_cairo_gl_surface_composite (cairo_operator_t		  op,
			     const cairo_pattern_t	 *src,
			     const cairo_pattern_t	 *mask,
			     void			 *abstract_dst,
			     int			  src_x,
			     int			  src_y,
			     int			  mask_x,
			     int			  mask_y,
			     int			  dst_x,
			     int			  dst_y,
			     unsigned int		  width,
			     unsigned int		  height,
			     cairo_region_t		 *clip_region);

static cairo_status_t
_cairo_gl_surface_flush (void *abstract_surface);

static cairo_bool_t _cairo_surface_is_gl (cairo_surface_t *surface)
{
    return surface->backend == &_cairo_gl_surface_backend;
}

cairo_bool_t
_cairo_gl_get_image_format_and_type (pixman_format_code_t pixman_format,
				     GLenum *internal_format, GLenum *format,
				     GLenum *type, cairo_bool_t *has_alpha)
{
    *has_alpha = TRUE;

    switch (pixman_format) {
    case PIXMAN_a8r8g8b8:
	*internal_format = GL_RGBA;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_INT_8_8_8_8_REV;
	return TRUE;
    case PIXMAN_x8r8g8b8:
	*internal_format = GL_RGB;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_INT_8_8_8_8_REV;
	*has_alpha = FALSE;
	return TRUE;
    case PIXMAN_a8b8g8r8:
	*internal_format = GL_RGBA;
	*format = GL_RGBA;
	*type = GL_UNSIGNED_INT_8_8_8_8_REV;
	return TRUE;
    case PIXMAN_x8b8g8r8:
	*internal_format = GL_RGB;
	*format = GL_RGBA;
	*type = GL_UNSIGNED_INT_8_8_8_8_REV;
	*has_alpha = FALSE;
	return TRUE;
    case PIXMAN_b8g8r8a8:
	*internal_format = GL_RGBA;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_INT_8_8_8_8;
	return TRUE;
    case PIXMAN_b8g8r8x8:
	*internal_format = GL_RGB;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_INT_8_8_8_8;
	*has_alpha = FALSE;
	return TRUE;
    case PIXMAN_r8g8b8:
	*internal_format = GL_RGB;
	*format = GL_RGB;
	*type = GL_UNSIGNED_BYTE;
	return TRUE;
    case PIXMAN_b8g8r8:
	*internal_format = GL_RGB;
	*format = GL_BGR;
	*type = GL_UNSIGNED_BYTE;
	return TRUE;
    case PIXMAN_r5g6b5:
	*internal_format = GL_RGB;
	*format = GL_RGB;
	*type = GL_UNSIGNED_SHORT_5_6_5;
	return TRUE;
    case PIXMAN_b5g6r5:
	*internal_format = GL_RGB;
	*format = GL_RGB;
	*type = GL_UNSIGNED_SHORT_5_6_5_REV;
	return TRUE;
    case PIXMAN_a1r5g5b5:
	*internal_format = GL_RGBA;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	return TRUE;
    case PIXMAN_x1r5g5b5:
	*internal_format = GL_RGB;
	*format = GL_BGRA;
	*type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	*has_alpha = FALSE;
	return TRUE;
    case PIXMAN_a1b5g5r5:
	*internal_format = GL_RGBA;
	*format = GL_RGBA;
	*type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	return TRUE;
    case PIXMAN_x1b5g5r5:
	*internal_format = GL_RGB;
	*format = GL_RGBA;
	*type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	*has_alpha = FALSE;
	return TRUE;
    case PIXMAN_a8:
	*internal_format = GL_ALPHA;
	*format = GL_ALPHA;
	*type = GL_UNSIGNED_BYTE;
	return TRUE;

    case PIXMAN_a2b10g10r10:
    case PIXMAN_x2b10g10r10:
    case PIXMAN_a4r4g4b4:
    case PIXMAN_x4r4g4b4:
    case PIXMAN_a4b4g4r4:
    case PIXMAN_x4b4g4r4:
    case PIXMAN_r3g3b2:
    case PIXMAN_b2g3r3:
    case PIXMAN_a2r2g2b2:
    case PIXMAN_a2b2g2r2:
    case PIXMAN_c8:
    case PIXMAN_x4a4:
    /* case PIXMAN_x4c4: */
    case PIXMAN_x4g4:
    case PIXMAN_a4:
    case PIXMAN_r1g2b1:
    case PIXMAN_b1g2r1:
    case PIXMAN_a1r1g1b1:
    case PIXMAN_a1b1g1r1:
    case PIXMAN_c4:
    case PIXMAN_g4:
    case PIXMAN_a1:
    case PIXMAN_g1:
    case PIXMAN_yuy2:
    case PIXMAN_yv12:
    case PIXMAN_x2r10g10b10:
    case PIXMAN_a2r10g10b10:
    default:
	return FALSE;
    }
}

cairo_bool_t
_cairo_gl_operator_is_supported (cairo_operator_t op)
{
    return op < CAIRO_OPERATOR_SATURATE;
}

void
_cairo_gl_surface_init (cairo_device_t *device,
			cairo_gl_surface_t *surface,
			cairo_content_t content,
			int width, int height)
{
    _cairo_surface_init (&surface->base,
			 &_cairo_gl_surface_backend,
			 device,
			 content);

    surface->width = width;
    surface->height = height;
}

static cairo_surface_t *
_cairo_gl_surface_create_scratch_for_texture (cairo_gl_context_t   *ctx,
					      cairo_content_t	    content,
					      GLuint		    tex,
					      int		    width,
					      int		    height)
{
    cairo_gl_surface_t *surface;

    assert (width <= ctx->max_framebuffer_size && height <= ctx->max_framebuffer_size);

    surface = calloc (1, sizeof (cairo_gl_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_gl_surface_init (&ctx->base, surface, content, width, height);
    surface->tex = tex;

    /* Create the texture used to store the surface's data. */
    _cairo_gl_context_activate (ctx, CAIRO_GL_TEX_TEMP);
    glBindTexture (ctx->tex_target, surface->tex);
    glTexParameteri (ctx->tex_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (ctx->tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return &surface->base;
}

static cairo_surface_t *
_cairo_gl_surface_create_scratch (cairo_gl_context_t   *ctx,
				  cairo_content_t	content,
				  int			width,
				  int			height)
{
    cairo_gl_surface_t *surface;
    GLenum format;
    GLuint tex;

    glGenTextures (1, &tex);
    surface = (cairo_gl_surface_t *)
	_cairo_gl_surface_create_scratch_for_texture (ctx, content,
						      tex, width, height);
    if (unlikely (surface->base.status))
	return &surface->base;

    surface->owns_tex = TRUE;

    /* adjust the texture size after setting our real extents */
    if (width < 1)
	width = 1;
    if (height < 1)
	height = 1;

    switch (content) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_CONTENT_COLOR_ALPHA:
	format = GL_RGBA;
	break;
    case CAIRO_CONTENT_ALPHA:
	/* We want to be trying GL_ALPHA framebuffer objects here. */
	format = GL_RGBA;
	break;
    case CAIRO_CONTENT_COLOR:
	/* GL_RGB is almost what we want here -- sampling 1 alpha when
	 * texturing, using 1 as destination alpha factor in blending,
	 * etc.  However, when filtering with GL_CLAMP_TO_BORDER, the
	 * alpha channel of the border color will also be clamped to
	 * 1, when we actually want the border color we explicitly
	 * specified.  So, we have to store RGBA, and fill the alpha
	 * channel with 1 when blending.
	 */
	format = GL_RGBA;
	break;
    }

    glTexImage2D (ctx->tex_target, 0, format, width, height, 0,
		  format, GL_UNSIGNED_BYTE, NULL);

    return &surface->base;
}

static cairo_status_t
_cairo_gl_surface_clear (cairo_gl_surface_t  *surface,
                         const cairo_color_t *color)
{
    cairo_gl_context_t *ctx;
    cairo_status_t status;
    double r, g, b, a;

    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
	return status;

    _cairo_gl_context_set_destination (ctx, surface);
    if (surface->base.content & CAIRO_CONTENT_COLOR) {
        r = color->red   * color->alpha;
        g = color->green * color->alpha;
        b = color->blue  * color->alpha;
    } else {
        r = g = b = 0;
    }
    if (surface->base.content & CAIRO_CONTENT_ALPHA) {
        a = color->alpha;
    } else {
        a = 1.0;
    }

    glDisable (GL_SCISSOR_TEST);
    glClearColor (r, g, b, a);
    glClear (GL_COLOR_BUFFER_BIT);

    return _cairo_gl_context_release (ctx, status);
}

cairo_surface_t *
cairo_gl_surface_create (cairo_device_t		*abstract_device,
			 cairo_content_t	 content,
			 int			 width,
			 int			 height)
{
    cairo_gl_context_t *ctx;
    cairo_gl_surface_t *surface;
    cairo_status_t status;

    if (! CAIRO_CONTENT_VALID (content))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_CONTENT));

    if (abstract_device == NULL) {
	return cairo_image_surface_create (_cairo_format_from_content (content),
					   width, height);
    }

    if (abstract_device->status)
	return _cairo_surface_create_in_error (abstract_device->status);

    if (abstract_device->backend->type != CAIRO_DEVICE_TYPE_GL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));

    status = _cairo_gl_context_acquire (abstract_device, &ctx);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    surface = (cairo_gl_surface_t *)
	_cairo_gl_surface_create_scratch (ctx, content, width, height);
    if (unlikely (surface->base.status)) {
	status = _cairo_gl_context_release (ctx, surface->base.status);
	cairo_surface_destroy (&surface->base);
	return _cairo_surface_create_in_error (status);
    }

    /* Cairo surfaces start out initialized to transparent (black) */
    status = _cairo_gl_surface_clear (surface, CAIRO_COLOR_TRANSPARENT);

    status = _cairo_gl_context_release (ctx, status);
    if (unlikely (status)) {
	cairo_surface_destroy (&surface->base);
	return _cairo_surface_create_in_error (status);
    }

    return &surface->base;
}
slim_hidden_def (cairo_gl_surface_create);


/**
 * cairo_gl_surface_create_for_texture:
 * @content: type of content in the surface
 * @tex: name of texture to use for storage of surface pixels
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a GL surface for the specified texture with the specified
 * content and dimensions.  The texture must be kept around until the
 * #cairo_surface_t is destroyed or cairo_surface_finish() is called
 * on the surface.  The initial contents of @tex will be used as the
 * initial image contents; you must explicitly clear the buffer,
 * using, for example, cairo_rectangle() and cairo_fill() if you want
 * it cleared.  The format of @tex should be compatible with @content,
 * in the sense that it must have the color components required by
 * @content.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 **/
cairo_surface_t *
cairo_gl_surface_create_for_texture (cairo_device_t	*abstract_device,
				     cairo_content_t	 content,
				     unsigned int	 tex,
				     int		 width,
				     int		 height)
{
    cairo_gl_context_t *ctx;
    cairo_gl_surface_t *surface;
    cairo_status_t status;

    if (! CAIRO_CONTENT_VALID (content))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_CONTENT));

    if (abstract_device == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NULL_POINTER));

    if (abstract_device->status)
	return _cairo_surface_create_in_error (abstract_device->status);

    if (abstract_device->backend->type != CAIRO_DEVICE_TYPE_GL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));

    status = _cairo_gl_context_acquire (abstract_device, &ctx);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    surface = (cairo_gl_surface_t *)
	_cairo_gl_surface_create_scratch_for_texture (ctx, content,
						      tex, width, height);
    status = _cairo_gl_context_release (ctx, status);

    return &surface->base;
}
slim_hidden_def (cairo_gl_surface_create_for_texture);


void
cairo_gl_surface_set_size (cairo_surface_t *abstract_surface,
			   int              width,
			   int              height)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;
    cairo_status_t status;

    if (unlikely (abstract_surface->status))
	return;
    if (unlikely (abstract_surface->finished)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
        return;
    }

    if (! _cairo_surface_is_gl (abstract_surface) ||
        ! _cairo_gl_surface_is_texture (surface)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));
	return;
    }

    surface->width = width;
    surface->height = height;
}

int
cairo_gl_surface_get_width (cairo_surface_t *abstract_surface)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;

    if (! _cairo_surface_is_gl (abstract_surface))
	return 0;

    return surface->width;
}

int
cairo_gl_surface_get_height (cairo_surface_t *abstract_surface)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;

    if (! _cairo_surface_is_gl (abstract_surface))
	return 0;

    return surface->height;
}

void
cairo_gl_surface_swapbuffers (cairo_surface_t *abstract_surface)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;
    cairo_status_t status;

    if (unlikely (abstract_surface->status))
	return;
    if (unlikely (abstract_surface->finished)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
        return;
    }

    if (! _cairo_surface_is_gl (abstract_surface)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    if (! _cairo_gl_surface_is_texture (surface)) {
	cairo_gl_context_t *ctx;
        cairo_status_t status;

        status = _cairo_gl_context_acquire (surface->base.device, &ctx);
        if (unlikely (status))
            return;

        cairo_surface_flush (abstract_surface);

	ctx->swap_buffers (ctx, surface);

        status = _cairo_gl_context_release (ctx, status);
        if (status)
            status = _cairo_surface_set_error (abstract_surface, status);         
    }
}

static cairo_surface_t *
_cairo_gl_surface_create_similar (void		 *abstract_surface,
				  cairo_content_t  content,
				  int		  width,
				  int		  height)
{
    cairo_surface_t *surface = abstract_surface;
    cairo_gl_context_t *ctx;
    cairo_status_t status;

    if (width < 1 || height < 1)
        return cairo_image_surface_create (_cairo_format_from_content (content),
                                           width, height);

    status = _cairo_gl_context_acquire (surface->device, &ctx);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    if (width > ctx->max_framebuffer_size ||
	height > ctx->max_framebuffer_size)
    {
	surface = NULL;
        goto RELEASE;
    }

    surface = _cairo_gl_surface_create_scratch (ctx, content, width, height);

RELEASE:
    status = _cairo_gl_context_release (ctx, status);
    if (unlikely (status)) {
        cairo_surface_destroy (surface);
        return _cairo_surface_create_in_error (status);
    }

    return surface;
}

cairo_status_t
_cairo_gl_surface_draw_image (cairo_gl_surface_t *dst,
			      cairo_image_surface_t *src,
			      int src_x, int src_y,
			      int width, int height,
			      int dst_x, int dst_y)
{
    GLenum internal_format, format, type;
    cairo_bool_t has_alpha;
    cairo_image_surface_t *clone = NULL;
    cairo_gl_context_t *ctx;
    int cpp;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (! _cairo_gl_get_image_format_and_type (src->pixman_format,
					       &internal_format,
					       &format,
					       &type,
					       &has_alpha))
    {
	cairo_bool_t is_supported;

	clone = _cairo_image_surface_coerce (src);
	if (unlikely (clone->base.status))
	    return clone->base.status;

	is_supported =
	    _cairo_gl_get_image_format_and_type (clone->pixman_format,
		                                 &internal_format,
						 &format,
						 &type,
						 &has_alpha);
	assert (is_supported);
	src = clone;
    }

    cpp = PIXMAN_FORMAT_BPP (src->pixman_format) / 8;

    status = _cairo_gl_context_acquire (dst->base.device, &ctx);
    if (unlikely (status))
	return status;

    status = _cairo_gl_surface_flush (&dst->base);
    if (unlikely (status))
	goto FAIL;

    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, src->stride / cpp);
    if (_cairo_gl_surface_is_texture (dst)) {
        _cairo_gl_context_activate (ctx, CAIRO_GL_TEX_TEMP);
	glBindTexture (ctx->tex_target, dst->tex);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexSubImage2D (ctx->tex_target, 0,
			 dst_x, dst_y, width, height,
			 format, type,
			 src->data + src_y * src->stride + src_x * cpp);

	/* If we just treated some rgb-only data as rgba, then we have to
	 * go back and fix up the alpha channel where we filled in this
	 * texture data.
	 */
	if (!has_alpha) {
	    cairo_rectangle_int_t rect;

	    rect.x = dst_x;
	    rect.y = dst_y;
	    rect.width = width;
	    rect.height = height;

            _cairo_gl_composite_flush (ctx);
	    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
	    _cairo_gl_surface_fill_rectangles (dst,
					       CAIRO_OPERATOR_SOURCE,
					       CAIRO_COLOR_BLACK,
					       &rect, 1);
            _cairo_gl_composite_flush (ctx);
	    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
    } else {
        cairo_surface_t *tmp;
        
        tmp = _cairo_gl_surface_create_scratch (ctx,
                                                dst->base.content,
                                                width, height);
        if (unlikely (tmp->status)) {
            cairo_surface_destroy (tmp);
            goto FAIL;
        }
        status = _cairo_gl_surface_draw_image ((cairo_gl_surface_t *) tmp,
                                               src,
                                               src_x, src_y,
                                               width, height,
                                               0, 0);
        if (status == CAIRO_STATUS_SUCCESS) {
            cairo_surface_pattern_t tmp_pattern;

            _cairo_pattern_init_for_surface (&tmp_pattern, tmp);
            _cairo_gl_surface_composite (CAIRO_OPERATOR_SOURCE,
                                         &tmp_pattern.base,
                                         NULL,
                                         dst,
                                         0, 0,
                                         0, 0,
                                         dst_x, dst_y,
                                         width, height,
                                         NULL);
            _cairo_pattern_fini (&tmp_pattern.base);
        }

        cairo_surface_destroy (tmp);
    }

FAIL:
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);

    status = _cairo_gl_context_release (ctx, status);

    if (clone)
        cairo_surface_destroy (&clone->base);

    return status;
}

static cairo_status_t
_cairo_gl_surface_get_image (cairo_gl_surface_t      *surface,
			     cairo_rectangle_int_t   *interest,
			     cairo_image_surface_t  **image_out,
			     cairo_rectangle_int_t   *rect_out)
{
    cairo_image_surface_t *image;
    cairo_gl_context_t *ctx;
    GLenum format, type;
    cairo_format_t cairo_format;
    unsigned int cpp;
    cairo_status_t status;

    /* Want to use a switch statement here but the compiler gets whiny. */
    if (surface->base.content == CAIRO_CONTENT_COLOR_ALPHA) {
	format = GL_BGRA;
	cairo_format = CAIRO_FORMAT_ARGB32;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	cpp = 4;
    } else if (surface->base.content == CAIRO_CONTENT_COLOR) {
	format = GL_BGRA;
	cairo_format = CAIRO_FORMAT_RGB24;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	cpp = 4;
    } else if (surface->base.content == CAIRO_CONTENT_ALPHA) {
	format = GL_ALPHA;
	cairo_format = CAIRO_FORMAT_A8;
	type = GL_UNSIGNED_BYTE;
	cpp = 1;
    } else {
	ASSERT_NOT_REACHED;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    image = (cairo_image_surface_t*)
	cairo_image_surface_create (cairo_format,
				    interest->width, interest->height);
    if (unlikely (image->base.status))
	return image->base.status;

    /* This is inefficient, as we'd rather just read the thing without making
     * it the destination.  But then, this is the fallback path, so let's not
     * fall back instead.
     */
    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
        return status;

    _cairo_gl_composite_flush (ctx);
    _cairo_gl_context_set_destination (ctx, surface);

    glPixelStorei (GL_PACK_ALIGNMENT, 1);
    glPixelStorei (GL_PACK_ROW_LENGTH, image->stride / cpp);
    if (! _cairo_gl_surface_is_texture (surface) && GLEW_MESA_pack_invert)
	glPixelStorei (GL_PACK_INVERT_MESA, 1);
    glReadPixels (interest->x, interest->y,
		  interest->width, interest->height,
		  format, type, image->data);
    if (! _cairo_gl_surface_is_texture (surface) && GLEW_MESA_pack_invert)
	glPixelStorei (GL_PACK_INVERT_MESA, 0);

    status = _cairo_gl_context_release (ctx, status);
    if (unlikely (status)) {
	cairo_surface_destroy (&image->base);
	return status;
    }

    *image_out = image;
    if (rect_out != NULL)
	*rect_out = *interest;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_finish (void *abstract_surface)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_status_t status;
    cairo_gl_context_t *ctx;

    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
        return status;

    if (ctx->operands[CAIRO_GL_TEX_SOURCE].type == CAIRO_GL_OPERAND_TEXTURE &&
        ctx->operands[CAIRO_GL_TEX_SOURCE].texture.surface == surface)
        _cairo_gl_context_destroy_operand (ctx, CAIRO_GL_TEX_SOURCE);
    if (ctx->operands[CAIRO_GL_TEX_MASK].type == CAIRO_GL_OPERAND_TEXTURE &&
        ctx->operands[CAIRO_GL_TEX_MASK].texture.surface == surface)
        _cairo_gl_context_destroy_operand (ctx, CAIRO_GL_TEX_MASK);
    if (ctx->current_target == surface)
	ctx->current_target = NULL;

    if (surface->depth)
        glDeleteFramebuffersEXT (1, &surface->depth);
    if (surface->fb)
        glDeleteFramebuffersEXT (1, &surface->fb);
    if (surface->owns_tex)
	glDeleteTextures (1, &surface->tex);

    return _cairo_gl_context_release (ctx, status);
}

static cairo_status_t
_cairo_gl_surface_acquire_source_image (void		       *abstract_surface,
					cairo_image_surface_t **image_out,
					void		      **image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_rectangle_int_t extents;

    *image_extra = NULL;

    extents.x = extents.y = 0;
    extents.width = surface->width;
    extents.height = surface->height;
    return _cairo_gl_surface_get_image (surface, &extents, image_out, NULL);
}

static void
_cairo_gl_surface_release_source_image (void		      *abstract_surface,
					cairo_image_surface_t *image,
					void		      *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_gl_surface_acquire_dest_image (void		      *abstract_surface,
				      cairo_rectangle_int_t   *interest_rect,
				      cairo_image_surface_t  **image_out,
				      cairo_rectangle_int_t   *image_rect_out,
				      void		     **image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;

    *image_extra = NULL;
    return _cairo_gl_surface_get_image (surface, interest_rect, image_out,
					image_rect_out);
}

static void
_cairo_gl_surface_release_dest_image (void		      *abstract_surface,
				      cairo_rectangle_int_t   *interest_rect,
				      cairo_image_surface_t   *image,
				      cairo_rectangle_int_t   *image_rect,
				      void		      *image_extra)
{
    cairo_status_t status;

    status = _cairo_gl_surface_draw_image (abstract_surface, image,
					   0, 0,
					   image->width, image->height,
					   image_rect->x, image_rect->y);
    /* as we created the image, its format should be directly applicable */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_gl_surface_clone_similar (void		     *abstract_surface,
				 cairo_surface_t     *src,
				 int                  src_x,
				 int                  src_y,
				 int                  width,
				 int                  height,
				 int                 *clone_offset_x,
				 int                 *clone_offset_y,
				 cairo_surface_t    **clone_out)
{
    cairo_gl_surface_t *surface = abstract_surface;

    /* XXX: Use GLCopyTexImage2D to clone non-texture-surfaces */
    if (src->device == surface->base.device &&
        _cairo_gl_surface_is_texture ((cairo_gl_surface_t *) src)) {
	*clone_offset_x = 0;
	*clone_offset_y = 0;
	*clone_out = cairo_surface_reference (src);

	return CAIRO_STATUS_SUCCESS;
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;
	cairo_gl_surface_t *clone;
	cairo_status_t status;

	clone = (cairo_gl_surface_t *)
	    _cairo_gl_surface_create_similar (&surface->base,
		                              src->content,
					      width, height);
	if (clone == NULL)
	    return UNSUPPORTED ("create_similar failed");
	if (clone->base.status)
	    return clone->base.status;

	status = _cairo_gl_surface_draw_image (clone, image_src,
					       src_x, src_y,
					       width, height,
					       0, 0);
	if (status) {
	    cairo_surface_destroy (&clone->base);
	    return status;
	}

	*clone_out = &clone->base;
	*clone_offset_x = src_x;
	*clone_offset_y = src_y;

	return CAIRO_STATUS_SUCCESS;
    }

    return UNSUPPORTED ("unknown src surface type in clone_similar");
}

/** Creates a cairo-gl pattern surface for the given trapezoids */
static cairo_status_t
_cairo_gl_get_traps_pattern (cairo_gl_surface_t *dst,
			     int dst_x, int dst_y,
			     int width, int height,
			     cairo_trapezoid_t *traps,
			     int num_traps,
			     cairo_antialias_t antialias,
			     cairo_surface_pattern_t *pattern)
{
    pixman_format_code_t pixman_format;
    pixman_image_t *image;
    cairo_surface_t *surface;
    int i;

    pixman_format = antialias != CAIRO_ANTIALIAS_NONE ? PIXMAN_a8 : PIXMAN_a1,
    image = pixman_image_create_bits (pixman_format, width, height, NULL, 0);
    if (unlikely (image == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    for (i = 0; i < num_traps; i++) {
	pixman_trapezoid_t trap;

	trap.top = _cairo_fixed_to_16_16 (traps[i].top);
	trap.bottom = _cairo_fixed_to_16_16 (traps[i].bottom);

	trap.left.p1.x = _cairo_fixed_to_16_16 (traps[i].left.p1.x);
	trap.left.p1.y = _cairo_fixed_to_16_16 (traps[i].left.p1.y);
	trap.left.p2.x = _cairo_fixed_to_16_16 (traps[i].left.p2.x);
	trap.left.p2.y = _cairo_fixed_to_16_16 (traps[i].left.p2.y);

	trap.right.p1.x = _cairo_fixed_to_16_16 (traps[i].right.p1.x);
	trap.right.p1.y = _cairo_fixed_to_16_16 (traps[i].right.p1.y);
	trap.right.p2.x = _cairo_fixed_to_16_16 (traps[i].right.p2.x);
	trap.right.p2.y = _cairo_fixed_to_16_16 (traps[i].right.p2.y);

	pixman_rasterize_trapezoid (image, &trap, -dst_x, -dst_y);
    }

    surface = _cairo_image_surface_create_for_pixman_image (image,
							    pixman_format);
    if (unlikely (surface->status)) {
	pixman_image_unref (image);
	return surface->status;
    }

    _cairo_pattern_init_for_surface (pattern, surface);
    cairo_surface_destroy (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_surface_composite (cairo_operator_t		  op,
			     const cairo_pattern_t	 *src,
			     const cairo_pattern_t	 *mask,
			     void			 *abstract_dst,
			     int			  src_x,
			     int			  src_y,
			     int			  mask_x,
			     int			  mask_y,
			     int			  dst_x,
			     int			  dst_y,
			     unsigned int		  width,
			     unsigned int		  height,
			     cairo_region_t		 *clip_region)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_gl_context_t *ctx;
    cairo_status_t status;
    cairo_gl_composite_t setup;
    cairo_rectangle_int_t rect = { dst_x, dst_y, width, height };
    int dx, dy;

    if (op == CAIRO_OPERATOR_SOURCE &&
        mask == NULL &&
        src->type == CAIRO_PATTERN_TYPE_SURFACE &&
        _cairo_surface_is_image (((cairo_surface_pattern_t *) src)->surface) &&
        _cairo_matrix_is_integer_translation (&src->matrix, &dx, &dy)) {
        cairo_image_surface_t *image = (cairo_image_surface_t *)
            ((cairo_surface_pattern_t *) src)->surface;
        dx += src_x;
        dy += src_y;
        if (dx >= 0 &&
            dy >= 0 &&
            dx + width <= (unsigned int) image->width &&
            dy + height <= (unsigned int) image->height) {
            status = _cairo_gl_surface_draw_image (dst, image,
                                                   dx, dy,
                                                   width, height,
                                                   dst_x, dst_y);
            if (status != CAIRO_INT_STATUS_UNSUPPORTED)
                return status;
        }
    }

    status = _cairo_gl_composite_init (&setup, op, dst,
                                       mask && mask->has_component_alpha,
                                       &rect);
    if (unlikely (status))
        goto CLEANUP;

    status = _cairo_gl_composite_set_source (&setup, src,
                                             src_x, src_y,
                                             dst_x, dst_y,
                                             width, height);
    if (unlikely (status))
        goto CLEANUP;

    status = _cairo_gl_composite_set_mask (&setup, mask,
                                           mask_x, mask_y,
                                           dst_x, dst_y,
                                           width, height);
    if (unlikely (status))
        goto CLEANUP;

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto CLEANUP;

    if (clip_region != NULL) {
        int i, num_rectangles;

        num_rectangles = cairo_region_num_rectangles (clip_region);

	for (i = 0; i < num_rectangles; i++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (clip_region, i, &rect);
            _cairo_gl_composite_emit_rect (ctx,
                                           rect.x,              rect.y,
                                           rect.x + rect.width, rect.y + rect.height,
                                           0);
	}
    } else {
        _cairo_gl_composite_emit_rect (ctx,
                                       dst_x,         dst_y,
                                       dst_x + width, dst_y + height,
                                       0);
    }

    status = _cairo_gl_context_release (ctx, status);

  CLEANUP:
    _cairo_gl_composite_fini (&setup);

    return status;
}

static cairo_int_status_t
_cairo_gl_surface_composite_trapezoids (cairo_operator_t op,
					const cairo_pattern_t *pattern,
					void *abstract_dst,
					cairo_antialias_t antialias,
					int src_x, int src_y,
					int dst_x, int dst_y,
					unsigned int width,
					unsigned int height,
					cairo_trapezoid_t *traps,
					int num_traps,
					cairo_region_t *clip_region)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_surface_pattern_t traps_pattern;
    cairo_int_status_t status;

    if (! _cairo_gl_operator_is_supported (op))
	return UNSUPPORTED ("unsupported operator");

    status = _cairo_gl_get_traps_pattern (dst,
					  dst_x, dst_y, width, height,
					  traps, num_traps, antialias,
					  &traps_pattern);
    if (unlikely (status))
	return status;

    status = _cairo_gl_surface_composite (op,
					  pattern, &traps_pattern.base, dst,
					  src_x, src_y,
					  0, 0,
					  dst_x, dst_y,
					  width, height,
					  clip_region);

    _cairo_pattern_fini (&traps_pattern.base);

    assert (status != CAIRO_INT_STATUS_UNSUPPORTED);
    return status;
}

static cairo_int_status_t
_cairo_gl_surface_fill_rectangles (void			   *abstract_dst,
				   cairo_operator_t	    op,
				   const cairo_color_t     *color,
				   cairo_rectangle_int_t   *rects,
				   int			    num_rects)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_solid_pattern_t solid;
    cairo_gl_context_t *ctx;
    cairo_status_t status;
    cairo_gl_composite_t setup;
    int i;

    status = _cairo_gl_composite_init (&setup, op, dst,
                                       FALSE,
                                       /* XXX */ NULL);
    if (unlikely (status))
        goto CLEANUP;

    _cairo_pattern_init_solid (&solid, color);
    status = _cairo_gl_composite_set_source (&setup, &solid.base,
                                             0, 0,
                                             0, 0,
                                             0, 0);
    if (unlikely (status))
        goto CLEANUP;

    status = _cairo_gl_composite_set_mask (&setup, NULL,
                                           0, 0,
                                           0, 0,
                                           0, 0);
    if (unlikely (status))
        goto CLEANUP;

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
        goto CLEANUP;

    for (i = 0; i < num_rects; i++) {
        _cairo_gl_composite_emit_rect (ctx,
                                       rects[i].x,
                                       rects[i].y,
                                       rects[i].x + rects[i].width,
                                       rects[i].y + rects[i].height,
                                       0);
    }

    status = _cairo_gl_context_release (ctx, status);

  CLEANUP:
    _cairo_gl_composite_fini (&setup);

    return status;
}

typedef struct _cairo_gl_surface_span_renderer {
    cairo_span_renderer_t base;

    cairo_gl_composite_t setup;

    int xmin, xmax;
    int ymin, ymax;

    cairo_gl_context_t *ctx;
} cairo_gl_surface_span_renderer_t;

static cairo_status_t
_cairo_gl_render_bounded_spans (void *abstract_renderer,
				int y, int height,
				const cairo_half_open_span_t *spans,
				unsigned num_spans)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	if (spans[0].coverage) {
            _cairo_gl_composite_emit_rect (renderer->ctx,
                                           spans[0].x, y,
                                           spans[1].x, y + height,
                                           spans[0].coverage);
	}

	spans++;
    } while (--num_spans > 1);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_render_unbounded_spans (void *abstract_renderer,
				  int y, int height,
				  const cairo_half_open_span_t *spans,
				  unsigned num_spans)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (y > renderer->ymin) {
        _cairo_gl_composite_emit_rect (renderer->ctx,
                                       renderer->xmin, renderer->ymin,
                                       renderer->xmax, y,
                                       0);
    }

    if (num_spans == 0) {
        _cairo_gl_composite_emit_rect (renderer->ctx,
                                       renderer->xmin, y,
                                       renderer->xmax, y + height,
                                       0);
    } else {
        if (spans[0].x != renderer->xmin) {
            _cairo_gl_composite_emit_rect (renderer->ctx,
                                           renderer->xmin, y,
                                           spans[0].x,     y + height,
                                           0);
        }

        do {
            _cairo_gl_composite_emit_rect (renderer->ctx,
                                           spans[0].x, y,
                                           spans[1].x, y + height,
                                           spans[0].coverage);
            spans++;
        } while (--num_spans > 1);

        if (spans[0].x != renderer->xmax) {
            _cairo_gl_composite_emit_rect (renderer->ctx,
                                           spans[0].x,     y,
                                           renderer->xmax, y + height,
                                           0);
        }
    }

    renderer->ymin = y + height;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_finish_unbounded_spans (void *abstract_renderer)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (renderer->ymax > renderer->ymin) {
        _cairo_gl_composite_emit_rect (renderer->ctx,
                                       renderer->xmin, renderer->ymin,
                                       renderer->xmax, renderer->ymax,
                                       0);
    }

    return _cairo_gl_context_release (renderer->ctx, CAIRO_STATUS_SUCCESS);
}

static cairo_status_t
_cairo_gl_finish_bounded_spans (void *abstract_renderer)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    return _cairo_gl_context_release (renderer->ctx, CAIRO_STATUS_SUCCESS);
}

static void
_cairo_gl_surface_span_renderer_destroy (void *abstract_renderer)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (!renderer)
	return;

    _cairo_gl_composite_fini (&renderer->setup);

    free (renderer);
}

static cairo_bool_t
_cairo_gl_surface_check_span_renderer (cairo_operator_t	       op,
				       const cairo_pattern_t  *pattern,
				       void		      *abstract_dst,
				       cairo_antialias_t       antialias)
{
    if (! _cairo_gl_operator_is_supported (op))
	return FALSE;

    return TRUE;

    (void) pattern;
    (void) abstract_dst;
    (void) antialias;
}

static cairo_span_renderer_t *
_cairo_gl_surface_create_span_renderer (cairo_operator_t	 op,
					const cairo_pattern_t	*src,
					void			*abstract_dst,
					cairo_antialias_t	 antialias,
					const cairo_composite_rectangles_t *rects,
					cairo_region_t		*clip_region)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_gl_surface_span_renderer_t *renderer;
    cairo_status_t status;
    const cairo_rectangle_int_t *extents;

    renderer = calloc (1, sizeof (*renderer));
    if (unlikely (renderer == NULL))
	return _cairo_span_renderer_create_in_error (CAIRO_STATUS_NO_MEMORY);

    renderer->base.destroy = _cairo_gl_surface_span_renderer_destroy;
    if (rects->is_bounded) {
	renderer->base.render_rows = _cairo_gl_render_bounded_spans;
        renderer->base.finish =      _cairo_gl_finish_bounded_spans;
	extents = &rects->bounded;
    } else {
	renderer->base.render_rows = _cairo_gl_render_unbounded_spans;
        renderer->base.finish =      _cairo_gl_finish_unbounded_spans;
	extents = &rects->unbounded;
    }
    renderer->xmin = extents->x;
    renderer->xmax = extents->x + extents->width;
    renderer->ymin = extents->y;
    renderer->ymax = extents->y + extents->height;

    status = _cairo_gl_composite_init (&renderer->setup,
                                       op, dst,
                                       FALSE, extents);
    if (unlikely (status))
        goto FAIL;

    status = _cairo_gl_composite_set_source (&renderer->setup, src,
                                             extents->x, extents->y,
                                             extents->x, extents->y,
                                             extents->width, extents->height);
    if (unlikely (status))
        goto FAIL;

    _cairo_gl_composite_set_mask_spans (&renderer->setup);
    _cairo_gl_composite_set_clip_region (&renderer->setup, clip_region);

    status = _cairo_gl_composite_begin (&renderer->setup, &renderer->ctx);
    if (unlikely (status))
        goto FAIL;

    return &renderer->base;

FAIL:
    _cairo_gl_composite_fini (&renderer->setup);
    free (renderer);
    return _cairo_span_renderer_create_in_error (status);
}

static cairo_bool_t
_cairo_gl_surface_get_extents (void		     *abstract_surface,
			       cairo_rectangle_int_t *rectangle)
{
    cairo_gl_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return TRUE;
}

static void
_cairo_gl_surface_get_font_options (void                  *abstract_surface,
				    cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_ON);
    _cairo_font_options_set_round_glyph_positions (options, CAIRO_ROUND_GLYPH_POS_ON);
}

static cairo_status_t
_cairo_gl_surface_flush (void *abstract_surface)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_status_t status;
    cairo_gl_context_t *ctx;

    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
        return status;

    if ((ctx->operands[CAIRO_GL_TEX_SOURCE].type == CAIRO_GL_OPERAND_TEXTURE &&
         ctx->operands[CAIRO_GL_TEX_SOURCE].texture.surface == surface) ||
        (ctx->operands[CAIRO_GL_TEX_MASK].type == CAIRO_GL_OPERAND_TEXTURE &&
         ctx->operands[CAIRO_GL_TEX_MASK].texture.surface == surface) ||
        (ctx->current_target == surface))
      _cairo_gl_composite_flush (ctx);

    return _cairo_gl_context_release (ctx, status);
}

static cairo_int_status_t
_cairo_gl_surface_paint (void *abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t *source,
			 cairo_clip_t	    *clip)
{
    /* simplify the common case of clearing the surface */
    if (clip == NULL) {
        if (op == CAIRO_OPERATOR_CLEAR)
            return _cairo_gl_surface_clear (abstract_surface, CAIRO_COLOR_TRANSPARENT);
       else if (source->type == CAIRO_PATTERN_TYPE_SOLID &&
                (op == CAIRO_OPERATOR_SOURCE ||
                 (op == CAIRO_OPERATOR_OVER && _cairo_pattern_is_opaque_solid (source)))) {
            return _cairo_gl_surface_clear (abstract_surface,
                                            &((cairo_solid_pattern_t *) source)->color);
        }
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_gl_surface_polygon (cairo_gl_surface_t *dst,
                           cairo_operator_t op,
                           const cairo_pattern_t *src,
                           cairo_polygon_t *polygon,
                           cairo_fill_rule_t fill_rule,
                           cairo_antialias_t antialias,
                           const cairo_composite_rectangles_t *extents,
                           cairo_clip_t *clip)
{
    cairo_status_t status;
    cairo_region_t *clip_region = NULL;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	    return CAIRO_STATUS_SUCCESS;
	if (unlikely (_cairo_status_is_error (status)))
	    return status;

	if (status == CAIRO_INT_STATUS_UNSUPPORTED)
            return UNSUPPORTED ("a clip surface would be required");
    }

    if (! _cairo_surface_check_span_renderer (op, src, &dst->base, antialias))
        return UNSUPPORTED ("no span renderer");

    if (op == CAIRO_OPERATOR_SOURCE)
        return UNSUPPORTED ("SOURCE compositing doesn't work in GL");
    if (op == CAIRO_OPERATOR_CLEAR) {
        op = CAIRO_OPERATOR_DEST_OUT;
        src = &_cairo_pattern_white.base;
    }

    status = _cairo_surface_composite_polygon (&dst->base,
                                               op,
                                               src,
                                               fill_rule,
                                               antialias,
                                               extents,
                                               polygon,
                                               clip_region);
    return status;
}

static cairo_int_status_t
_cairo_gl_surface_stroke (void			        *abstract_surface,
                          cairo_operator_t		 op,
                          const cairo_pattern_t	        *source,
                          cairo_path_fixed_t		*path,
                          const cairo_stroke_style_t	*style,
                          const cairo_matrix_t	        *ctm,
                          const cairo_matrix_t	        *ctm_inverse,
                          double			 tolerance,
                          cairo_antialias_t		 antialias,
                          cairo_clip_t		        *clip)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    cairo_polygon_t polygon;
    cairo_status_t status;

    status = _cairo_composite_rectangles_init_for_stroke (&extents,
							  surface->width,
							  surface->height,
							  op, source,
							  path, style, ctm,
							  clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (clip != NULL) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	have_clip = TRUE;
    }

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status)) {
	if (have_clip)
	    _cairo_clip_fini (&local_clip);

	return status;
    }

    _cairo_polygon_init (&polygon);
    _cairo_polygon_limit (&polygon, clip_boxes, num_boxes);

    status = _cairo_path_fixed_stroke_to_polygon (path,
                                                  style,
                                                  ctm, ctm_inverse,
                                                  tolerance,
                                                  &polygon);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
        status = _cairo_gl_surface_polygon (surface, op, source, &polygon,
                                            CAIRO_FILL_RULE_WINDING, antialias,
                                            &extents, clip);
    }

    _cairo_polygon_fini (&polygon);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

static cairo_int_status_t
_cairo_gl_surface_fill (void			*abstract_surface,
                        cairo_operator_t	 op,
                        const cairo_pattern_t	*source,
                        cairo_path_fixed_t	*path,
                        cairo_fill_rule_t	 fill_rule,
                        double			 tolerance,
                        cairo_antialias_t	 antialias,
                        cairo_clip_t		*clip)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_polygon_t polygon;
    cairo_status_t status;

    status = _cairo_composite_rectangles_init_for_fill (&extents,
							surface->width,
							surface->height,
							op, source, path,
							clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

#if 0
    if (extents.is_bounded && clip != NULL) {
	cairo_clip_path_t *clip_path;

	if (((clip_path = _clip_get_single_path (clip)) != NULL) &&
	    _cairo_path_fixed_equal (&clip_path->path, path))
	{
	    clip = NULL;
	}
    }
#endif

    if (clip != NULL) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	have_clip = TRUE;
    }

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status)) {
	if (have_clip)
	    _cairo_clip_fini (&local_clip);

	return status;
    }

    _cairo_polygon_init (&polygon);
    _cairo_polygon_limit (&polygon, clip_boxes, num_boxes);

    status = _cairo_path_fixed_fill_to_polygon (path, tolerance, &polygon);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
        status = _cairo_gl_surface_polygon (surface, op, source, &polygon,
                                            fill_rule, antialias,
                                            &extents, clip);
    }

    _cairo_polygon_fini (&polygon);

    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

const cairo_surface_backend_t _cairo_gl_surface_backend = {
    CAIRO_SURFACE_TYPE_GL,
    _cairo_gl_surface_create_similar,
    _cairo_gl_surface_finish,

    _cairo_gl_surface_acquire_source_image,
    _cairo_gl_surface_release_source_image,
    _cairo_gl_surface_acquire_dest_image,
    _cairo_gl_surface_release_dest_image,

    _cairo_gl_surface_clone_similar,
    _cairo_gl_surface_composite,
    _cairo_gl_surface_fill_rectangles,
    _cairo_gl_surface_composite_trapezoids,
    _cairo_gl_surface_create_span_renderer,
    _cairo_gl_surface_check_span_renderer,

    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_gl_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_gl_surface_get_font_options,
    _cairo_gl_surface_flush,
    NULL, /* mark_dirty_rectangle */
    _cairo_gl_surface_scaled_font_fini,
    _cairo_gl_surface_scaled_glyph_fini,
    _cairo_gl_surface_paint,
    NULL, /* mask */
    _cairo_gl_surface_stroke,
    _cairo_gl_surface_fill,
    _cairo_gl_surface_show_glyphs, /* show_glyphs */
    NULL  /* snapshot */
};
