/*
 * Copyright © 2004 David Reveman
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the names of
 * David Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * David Reveman makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Based on glx code by David Reveman
 *
 * Contributors:
 *  Tor Lillqvist <tml@iki.fi>
 *  Vladimir Vukicevic <vladimir@pobox.com>
 *
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "glitz_wglint.h"

#include <stdio.h>

glitz_status_t
glitz_wgl_make_current_read (void *abstract_surface)
{
    return GLITZ_STATUS_NOT_SUPPORTED;
}

static glitz_wgl_drawable_t *
_glitz_wgl_create_drawable (glitz_wgl_screen_info_t *screen_info,
			    glitz_wgl_context_t     *context,
			    glitz_drawable_format_t *format,
			    HWND                    window,
			    HDC                     dc,
			    HPBUFFERARB             pbuffer,
			    int                     width,
			    int                     height)
{
    glitz_wgl_drawable_t *drawable;

    if (width <= 0 || height <= 0)
	return NULL;

    drawable = (glitz_wgl_drawable_t *) malloc (sizeof (glitz_wgl_drawable_t));
    if (drawable == NULL)
	return NULL;

    drawable->base.ref_count = 1;
    drawable->screen_info = screen_info;
    drawable->context = context;
    drawable->window = window;
    drawable->dc = dc;
    drawable->pbuffer = pbuffer;

    _glitz_drawable_init (&drawable->base,
			  &screen_info->formats[format->id],
			  &context->backend,
			  width,
			  height);

    if (!context->initialized) {
	glitz_wgl_push_current (drawable, NULL, GLITZ_CONTEXT_CURRENT, NULL);
	glitz_wgl_pop_current (drawable);
    }

    if (width > context->max_viewport_dims[0] ||
	height > context->max_viewport_dims[1]) {
	free (drawable);
	return NULL;
    }

    screen_info->drawables++;

    return drawable;
}

static glitz_drawable_t *
_glitz_wgl_create_pbuffer_drawable (glitz_wgl_screen_info_t    *screen_info,
				    glitz_drawable_format_t    *format,
				    unsigned int                width,
				    unsigned int                height)
{
    glitz_wgl_drawable_t *drawable;
    glitz_wgl_context_t *context;
    glitz_int_drawable_format_t *iformat = &screen_info->formats[format->id];
    HPBUFFERARB pbuffer;
    HDC dc;

    if (!(iformat->types & GLITZ_DRAWABLE_TYPE_PBUFFER_MASK))
	return NULL;

    context = glitz_wgl_context_get (screen_info, dc, format);
    if (!context)
        return NULL;
    
    pbuffer = glitz_wgl_pbuffer_create (screen_info, screen_info->format_ids[format->id],
					width, height,
					&dc);
    if (!pbuffer)
	return NULL;

    drawable = _glitz_wgl_create_drawable (screen_info, context, format,
					   NULL, dc, pbuffer,
					   width, height);
    if (!drawable) {
	glitz_wgl_pbuffer_destroy (screen_info, pbuffer, dc);
	return NULL;
    }

    return &drawable->base;
}

glitz_drawable_t *
glitz_wgl_create_pbuffer (void                       *abstract_templ,
			  glitz_drawable_format_t    *format,
			  unsigned int                width,
			  unsigned int                height)
{
    glitz_wgl_drawable_t *templ = (glitz_wgl_drawable_t *) abstract_templ;

    return _glitz_wgl_create_pbuffer_drawable (templ->screen_info, format,
					       width, height);
}

glitz_drawable_t *
glitz_wgl_create_drawable_for_window (glitz_drawable_format_t *format,
				      HWND                    window,
				      unsigned int            width,
				      unsigned int            height)
{
    glitz_wgl_drawable_t *drawable;
    glitz_wgl_screen_info_t *screen_info;
    glitz_wgl_context_t *context;
    HDC dc;

    screen_info = glitz_wgl_screen_info_get ();
    if (!screen_info)
	return NULL;

    dc = GetDC (window);

    context = glitz_wgl_context_get (screen_info, dc, format);
    if (!context) {
	ReleaseDC (window, dc);
	return NULL;
    }

    drawable = _glitz_wgl_create_drawable (screen_info, context, format,
					   window, dc, NULL,
					   width, height);
    if (!drawable)
	return NULL;

    return &drawable->base;
}

glitz_drawable_t *
glitz_wgl_create_pbuffer_drawable (glitz_drawable_format_t    *format,
				   unsigned int                width,
				   unsigned int                height)
{
    glitz_wgl_screen_info_t *screen_info;

    screen_info = glitz_wgl_screen_info_get ();
    if (!screen_info)
	return NULL;

    return _glitz_wgl_create_pbuffer_drawable (screen_info, format,
					       width, height);
}

void
glitz_wgl_destroy (void *abstract_drawable)
{
    glitz_wgl_drawable_t *drawable = (glitz_wgl_drawable_t *) abstract_drawable;

    drawable->screen_info->drawables--;
    if (drawable->screen_info->drawables == 0) {
	/*
	 * Last drawable? We have to destroy all fragment programs as this may
	 * be our last chance to have a context current.
	 */
	glitz_wgl_push_current (abstract_drawable, NULL, GLITZ_CONTEXT_CURRENT,
				NULL);
	glitz_program_map_fini (drawable->base.backend->gl,
				&drawable->screen_info->program_map);
	glitz_program_map_init (&drawable->screen_info->program_map);
	glitz_wgl_pop_current (abstract_drawable);
    }

    if (wglGetCurrentDC () == drawable->dc)
    {
	wglMakeCurrent (NULL, NULL);
    }

    if (drawable->pbuffer)
	glitz_wgl_pbuffer_destroy (drawable->screen_info, drawable->pbuffer, drawable->dc);

    free (drawable);
}

glitz_bool_t
glitz_wgl_swap_buffers (void *abstract_drawable)
{
    glitz_wgl_drawable_t *drawable = (glitz_wgl_drawable_t *) abstract_drawable;

    SwapBuffers (drawable->dc);

    return 1;
}

glitz_bool_t
glitz_wgl_copy_sub_buffer (void *abstract_drawable,
			   int  x,
			   int  y,
			   int  width,
			   int  height)
{
    return 0;
}
