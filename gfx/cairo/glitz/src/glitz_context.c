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

void
_glitz_context_init (glitz_context_t  *context,
		     glitz_drawable_t *drawable)
{
    glitz_drawable_reference (drawable);

    context->ref_count    = 1;
    context->drawable     = drawable;
    context->closure      = NULL;
    context->lose_current = NULL;
}

void
_glitz_context_fini (glitz_context_t *context)
{
    glitz_drawable_destroy (context->drawable);
}

glitz_context_t *
glitz_context_create (glitz_drawable_t        *drawable,
		      glitz_drawable_format_t *format)
{
    return drawable->backend->create_context (drawable, format);
}
slim_hidden_def(glitz_context_create);

void
glitz_context_destroy (glitz_context_t *context)
{
    if (!context)
	return;

    context->ref_count--;
    if (context->ref_count)
	return;

    context->drawable->backend->destroy_context (context);
}
slim_hidden_def(glitz_context_destroy);

void
glitz_context_reference (glitz_context_t *context)
{
    if (!context)
	return;

    context->ref_count++;
}
slim_hidden_def(glitz_context_reference);

void
glitz_context_copy (glitz_context_t *src,
		    glitz_context_t *dst,
		    unsigned long   mask)
{
    src->drawable->backend->copy_context (src, dst, mask);
}
slim_hidden_def(glitz_context_copy);

void
glitz_context_set_user_data (glitz_context_t               *context,
			     void                          *closure,
			     glitz_lose_current_function_t lose_current)
{
    context->closure = closure;
    context->lose_current = lose_current;
}
slim_hidden_def(glitz_context_set_user_data);

glitz_function_pointer_t
glitz_context_get_proc_address (glitz_context_t *context,
				const char      *name)
{
    return context->drawable->backend->get_proc_address (context, name);
}
slim_hidden_def(glitz_context_get_proc_address);

void
glitz_context_make_current (glitz_context_t  *context,
			    glitz_drawable_t *drawable)
{
    glitz_lose_current_function_t lose_current;

    lose_current = context->lose_current;
    context->lose_current = 0;

    if (drawable != context->drawable)
    {
	glitz_drawable_reference (drawable);
	glitz_drawable_destroy (context->drawable);
	context->drawable = drawable;
    }

    if (drawable->front)
    {
	if (REGION_NOTEMPTY (&drawable->front->drawable_damage))
	{
	    glitz_surface_push_current (drawable->front,
					GLITZ_DRAWABLE_CURRENT);
	    glitz_surface_pop_current (drawable->front);
	}

	glitz_surface_damage (drawable->front, NULL,
			      GLITZ_DAMAGE_TEXTURE_MASK |
			      GLITZ_DAMAGE_SOLID_MASK);
    }

    if (drawable->back)
    {
	if (REGION_NOTEMPTY (&drawable->back->drawable_damage))
	{
	    glitz_surface_push_current (drawable->back,
					GLITZ_DRAWABLE_CURRENT);
	    glitz_surface_pop_current (drawable->back);
	}

	glitz_surface_damage (drawable->back, NULL,
			      GLITZ_DAMAGE_TEXTURE_MASK |
			      GLITZ_DAMAGE_SOLID_MASK);
    }

    context->lose_current = lose_current;

    drawable->backend->make_current (drawable, context);
}
slim_hidden_def(glitz_context_make_current);

void
glitz_context_bind_texture (glitz_context_t	   *context,
			    glitz_texture_object_t *texture)
{
    glitz_gl_proc_address_list_t *gl = context->drawable->backend->gl;

    if (REGION_NOTEMPTY (&texture->surface->texture_damage))
    {
	glitz_lose_current_function_t lose_current;

	lose_current = context->lose_current;
	context->lose_current = 0;

	glitz_surface_push_current (texture->surface, GLITZ_CONTEXT_CURRENT);
	_glitz_surface_sync_texture (texture->surface);
	glitz_surface_pop_current (texture->surface);

	context->lose_current = lose_current;

	glitz_context_make_current (context, context->drawable);
    }

    gl->bind_texture (texture->surface->texture.target,
		      texture->surface->texture.name);

    glitz_texture_ensure_parameters (gl,
				     &texture->surface->texture,
				     &texture->param);
}
slim_hidden_def(glitz_context_bind_texture);

void
glitz_context_draw_buffers (glitz_context_t	          *context,
			    const glitz_drawable_buffer_t *buffers,
			    int				  n)
{
    unsigned int mask = 0;

#define FRONT_BIT (1 << 0)
#define BACK_BIT  (1 << 1)

    while (n--)
    {
	switch (*buffers++) {
	case GLITZ_DRAWABLE_BUFFER_FRONT_COLOR:
	    mask |= FRONT_BIT;
	    break;
	case GLITZ_DRAWABLE_BUFFER_BACK_COLOR:
	    mask |= BACK_BIT;
	default:
	    break;
	}
    }

    if (mask)
    {
	static const glitz_gl_enum_t mode[] = {
	    GLITZ_GL_FRONT,
	    GLITZ_GL_BACK,
	    GLITZ_GL_FRONT_AND_BACK
	};

	context->drawable->backend->draw_buffer (context->drawable,
						 mode[mask - 1]);
    }

#undef FRONT_BIT
#undef BACK_BIT

}
slim_hidden_def(glitz_context_draw_buffers);

void
glitz_context_read_buffer (glitz_context_t		 *context,
			   const glitz_drawable_buffer_t buffer)
{
    switch (buffer) {
    case GLITZ_DRAWABLE_BUFFER_FRONT_COLOR:
	context->drawable->backend->read_buffer (context->drawable,
						 GLITZ_GL_FRONT);
	break;
    case GLITZ_DRAWABLE_BUFFER_BACK_COLOR:
	context->drawable->backend->read_buffer (context->drawable,
						 GLITZ_GL_BACK);
    default:
	break;
    }
}
slim_hidden_def(glitz_context_read_buffer);
