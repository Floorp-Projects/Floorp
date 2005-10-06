/*
 * Copyright © 2005 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "glitzint.h"

typedef struct _glitz_fbo_drawable {
    glitz_drawable_t base;
    glitz_drawable_t *other;
    int		     width;
    int		     height;
    glitz_gl_uint_t  fb;
    glitz_gl_uint_t  front;
    glitz_gl_uint_t  back;
    glitz_gl_uint_t  depth;
    glitz_gl_uint_t  stencil;
    glitz_gl_uint_t  front_texture;
    glitz_gl_uint_t  back_texture;
    glitz_gl_enum_t  internal_format;
} glitz_fbo_drawable_t;

static glitz_bool_t
_glitz_fbo_bind (glitz_fbo_drawable_t *drawable)
{
    glitz_bool_t    update = 0;
    glitz_gl_enum_t status;

    GLITZ_GL_DRAWABLE (drawable->other);

    if (!drawable->fb)
    {
	gl->gen_framebuffers (1, &drawable->fb);

	drawable->width  = drawable->base.width;
	drawable->height = drawable->base.height;
	update = 1;
    }
    else if (drawable->width  != drawable->base.width ||
	     drawable->height != drawable->base.height)
    {
	drawable->width  = drawable->base.width;
	drawable->height = drawable->base.height;
	update = 1;
    }

    gl->bind_framebuffer (GLITZ_GL_FRAMEBUFFER, drawable->fb);

    if (drawable->base.front &&
	drawable->front_texture != drawable->base.front->texture.name)
    {
	gl->framebuffer_texture_2d (GLITZ_GL_FRAMEBUFFER,
				    GLITZ_GL_COLOR_ATTACHMENT0,
				    drawable->base.front->texture.target,
				    drawable->base.front->texture.name,
				    0);

	drawable->front_texture = drawable->base.front->texture.name;

	if (drawable->front)
	{
	    gl->delete_renderbuffers (1, &drawable->front);
	    drawable->front = 0;
	}
    }

    if (!drawable->front_texture && !drawable->front)
    {
	gl->gen_renderbuffers (1, &drawable->front);
	gl->bind_renderbuffer (GLITZ_GL_RENDERBUFFER, drawable->front);
	gl->renderbuffer_storage (GLITZ_GL_RENDERBUFFER,
				  drawable->internal_format,
				  drawable->base.width,
				  drawable->base.height);
	gl->bind_renderbuffer (GLITZ_GL_RENDERBUFFER, 0);
	gl->framebuffer_renderbuffer (GLITZ_GL_FRAMEBUFFER,
				      GLITZ_GL_COLOR_ATTACHMENT0,
				      GLITZ_GL_RENDERBUFFER,
				      drawable->front);
    }

    if (drawable->base.format->d.doublebuffer)
    {
	if (drawable->base.back &&
	    drawable->back_texture != drawable->base.back->texture.name)
	{
	    gl->framebuffer_texture_2d (GLITZ_GL_FRAMEBUFFER,
					GLITZ_GL_COLOR_ATTACHMENT1,
					drawable->base.back->texture.target,
					drawable->base.back->texture.name,
					0);

	    drawable->back_texture = drawable->base.back->texture.name;

	    if (drawable->back)
	    {
		gl->delete_renderbuffers (1, &drawable->back);
		drawable->back = 0;
	    }
	}

	if (!drawable->back_texture && !drawable->back)
	{
	    gl->gen_renderbuffers (1, &drawable->back);
	    gl->bind_renderbuffer (GLITZ_GL_RENDERBUFFER,
				   drawable->back);
	    gl->renderbuffer_storage (GLITZ_GL_RENDERBUFFER,
				      drawable->internal_format,
				      drawable->base.width,
				      drawable->base.height);
	    gl->bind_renderbuffer (GLITZ_GL_RENDERBUFFER, 0);
	    gl->framebuffer_renderbuffer (GLITZ_GL_FRAMEBUFFER,
					  GLITZ_GL_COLOR_ATTACHMENT1,
					  GLITZ_GL_RENDERBUFFER,
					  drawable->back);
	}
    }

    if (update)
    {
	if (drawable->base.format->d.depth_size)
	{
	    if (!drawable->depth)
		gl->gen_renderbuffers (1, &drawable->depth);

	    gl->bind_renderbuffer (GLITZ_GL_RENDERBUFFER, drawable->depth);
	    gl->renderbuffer_storage (GLITZ_GL_RENDERBUFFER,
				      GLITZ_GL_DEPTH_COMPONENT,
				      drawable->base.width,
				      drawable->base.height);
	    gl->bind_renderbuffer (GLITZ_GL_RENDERBUFFER, 0);

	    gl->framebuffer_renderbuffer (GLITZ_GL_FRAMEBUFFER,
					  GLITZ_GL_DEPTH_ATTACHMENT,
					  GLITZ_GL_RENDERBUFFER,
					  drawable->depth);
	}

	if (drawable->base.format->d.stencil_size)
	{
	    if (!drawable->stencil)
		gl->gen_renderbuffers (1, &drawable->stencil);

	    gl->bind_renderbuffer (GLITZ_GL_RENDERBUFFER,
				   drawable->stencil);
	    gl->renderbuffer_storage (GLITZ_GL_RENDERBUFFER,
				      GLITZ_GL_STENCIL_INDEX,
				      drawable->base.width,
				      drawable->base.height);
	    gl->bind_renderbuffer (GLITZ_GL_RENDERBUFFER, 0);

	    gl->framebuffer_renderbuffer (GLITZ_GL_FRAMEBUFFER,
					  GLITZ_GL_STENCIL_ATTACHMENT,
					  GLITZ_GL_RENDERBUFFER,
					  drawable->stencil);
	}
    }

    status = gl->check_framebuffer_status (GLITZ_GL_FRAMEBUFFER);
    if (status == GLITZ_GL_FRAMEBUFFER_COMPLETE)
	return 1;

    return 0;
}

static void
_glitz_fbo_attach_notify (void            *abstract_drawable,
			  glitz_surface_t *surface)
{
    glitz_fbo_drawable_t *drawable = (glitz_fbo_drawable_t *)
	abstract_drawable;
    glitz_texture_t      *texture;

    GLITZ_GL_DRAWABLE (drawable->other);

    texture = &surface->texture;
    if (!TEXTURE_ALLOCATED (texture))
    {
	drawable->other->backend->push_current (drawable->other, NULL,
						GLITZ_ANY_CONTEXT_CURRENT);
	glitz_texture_allocate (gl, texture);
	drawable->other->backend->pop_current (drawable->other);

	if (!TEXTURE_ALLOCATED (texture))
	    return;
    }

    REGION_EMPTY (&surface->drawable_damage);
}

static void
_glitz_fbo_detach_notify (void            *abstract_drawable,
			  glitz_surface_t *surface)
{
    glitz_fbo_drawable_t *drawable = (glitz_fbo_drawable_t *)
	abstract_drawable;

    if (surface->texture.name == drawable->front_texture ||
	surface->texture.name == drawable->back_texture)
    {
	GLITZ_GL_DRAWABLE (drawable->other);

	drawable->other->backend->push_current (drawable->other, NULL,
						GLITZ_ANY_CONTEXT_CURRENT);

	gl->bind_framebuffer (GLITZ_GL_FRAMEBUFFER, drawable->fb);

	if (surface->texture.name == drawable->front_texture)
	{
	    gl->framebuffer_texture_2d (GLITZ_GL_FRAMEBUFFER,
					GLITZ_GL_COLOR_ATTACHMENT0,
					surface->texture.target,
					0, 0);
	    drawable->front_texture = 0;
	}

	if (surface->texture.name == drawable->back_texture)
	{
	    gl->framebuffer_texture_2d (GLITZ_GL_FRAMEBUFFER,
					GLITZ_GL_COLOR_ATTACHMENT1,
					surface->texture.target,
					0, 0);
	    drawable->back_texture = 0;
	}

	gl->bind_framebuffer (GLITZ_GL_FRAMEBUFFER, 0);

	surface->fb = 0;

	drawable->other->backend->pop_current (drawable->other);
    }
}

static glitz_bool_t
_glitz_fbo_push_current (void               *abstract_drawable,
			 glitz_surface_t    *surface,
			 glitz_constraint_t constraint)
{
    glitz_fbo_drawable_t *drawable = (glitz_fbo_drawable_t *)
	abstract_drawable;

    drawable->other->backend->push_current (drawable->other, surface,
					    constraint);

    if (constraint == GLITZ_DRAWABLE_CURRENT)
    {
	if (_glitz_fbo_bind (drawable))
	{
	    drawable->base.update_all = drawable->other->update_all = 1;
	    surface->fb = drawable->fb;
	    return 1;
	}
    }

    return 0;
}

static glitz_surface_t *
_glitz_fbo_pop_current (void *abstract_drawable)
{
    glitz_fbo_drawable_t *drawable = (glitz_fbo_drawable_t *)
	abstract_drawable;

    GLITZ_GL_DRAWABLE (drawable->other);

    gl->bind_framebuffer (GLITZ_GL_FRAMEBUFFER, 0);

    return drawable->other->backend->pop_current (drawable->other);
}

static void
_glitz_fbo_make_current (void *abstract_drawable,
			 void *abstract_context)
{
    glitz_fbo_drawable_t *drawable = (glitz_fbo_drawable_t *)
	abstract_drawable;

    drawable->other->backend->make_current (drawable->other, abstract_context);

    _glitz_fbo_bind (drawable);
}

static glitz_bool_t
_glitz_fbo_swap_buffers (void *abstract_drawable)
{
    glitz_fbo_drawable_t *drawable = (glitz_fbo_drawable_t *)
	abstract_drawable;

    if (!drawable->fb)
	return 1;

    return 0;
}

static void
_glitz_fbo_destroy (void *abstract_drawable)
{
    glitz_fbo_drawable_t *drawable = (glitz_fbo_drawable_t *)
	abstract_drawable;

    if (drawable->fb)
    {
	GLITZ_GL_DRAWABLE (drawable->other);

	drawable->other->backend->push_current (drawable->other, NULL,
						GLITZ_ANY_CONTEXT_CURRENT);

	gl->delete_framebuffers (1, &drawable->fb);

	if (drawable->front)
	    gl->delete_renderbuffers (1, &drawable->front);

	if (drawable->back)
	    gl->delete_renderbuffers (1, &drawable->back);

	if (drawable->depth)
	    gl->delete_renderbuffers (1, &drawable->depth);

	if (drawable->stencil)
	    gl->delete_renderbuffers (1, &drawable->stencil);

	drawable->other->backend->pop_current (drawable->other);
    }

    glitz_drawable_destroy (drawable->other);

    free (drawable);
}

glitz_drawable_t *
_glitz_fbo_drawable_create (glitz_drawable_t	        *other,
			    glitz_int_drawable_format_t *format,
			    int	                        width,
			    int	                        height)
{
    glitz_fbo_drawable_t *drawable;
    glitz_backend_t	 *backend;

    drawable = malloc (sizeof (glitz_fbo_drawable_t) +
		       sizeof (glitz_backend_t));
    if (!drawable)
	return NULL;

    glitz_drawable_reference (other);
    drawable->other = other;
    backend = (glitz_backend_t *) (drawable + 1);
    *backend = *other->backend;

    backend->destroy       = _glitz_fbo_destroy;
    backend->push_current  = _glitz_fbo_push_current;
    backend->pop_current   = _glitz_fbo_pop_current;
    backend->attach_notify = _glitz_fbo_attach_notify;
    backend->detach_notify = _glitz_fbo_detach_notify;
    backend->swap_buffers  = _glitz_fbo_swap_buffers;
    backend->make_current  = _glitz_fbo_make_current;

    drawable->fb = 0;

    drawable->width  = 0;
    drawable->height = 0;

    drawable->front   = 0;
    drawable->back    = 0;
    drawable->depth   = 0;
    drawable->stencil = 0;

    drawable->front_texture = 0;
    drawable->back_texture  = 0;

    /* XXX: temporary solution until we have proper format validation */
    if (format->d.color.alpha_size)
	drawable->internal_format = GLITZ_GL_RGBA;
    else
	drawable->internal_format = GLITZ_GL_RGB;

    _glitz_drawable_init (&drawable->base, format, backend, width, height);

    return &drawable->base;
}
