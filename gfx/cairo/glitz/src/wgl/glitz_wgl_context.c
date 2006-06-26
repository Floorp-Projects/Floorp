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

#include <stdlib.h>
#include <stdio.h>

extern glitz_gl_proc_address_list_t _glitz_wgl_gl_proc_address;

static void
_glitz_wgl_context_create (glitz_wgl_screen_info_t *screen_info,
			   HDC                     dc,
			   int                     format_id,
			   glitz_wgl_context_t     *context)
{
    PIXELFORMATDESCRIPTOR dummy_pfd;

    dummy_pfd.nSize = sizeof (PIXELFORMATDESCRIPTOR);
    dummy_pfd.nVersion = 1;

    SetPixelFormat (dc, format_id, &dummy_pfd);

    context->context = wglCreateContext (dc);

    context->pixel_format = format_id;
}

static glitz_context_t *
_glitz_wgl_create_context (void                    *abstract_drawable,
			   glitz_drawable_format_t *format)
{
    glitz_wgl_drawable_t *drawable = (glitz_wgl_drawable_t *) abstract_drawable;
    glitz_wgl_screen_info_t *screen_info = drawable->screen_info;
    int format_id = screen_info->format_ids[format->id];
    glitz_wgl_context_t *context;

    context = malloc (sizeof (glitz_wgl_context_t));
    if (!context)
	return NULL;

    _glitz_context_init (&context->base, &drawable->base);

    _glitz_wgl_context_create (screen_info,
			       drawable->dc,
			       format_id,
			       context);

    wglShareLists (screen_info->root_context, context->context);

    return (glitz_context_t *) context;
}

static void
_glitz_wgl_destroy_context (void *abstract_context)
{
    glitz_wgl_context_t *context = (glitz_wgl_context_t *) abstract_context;

    wglDeleteContext (context->context);

    _glitz_context_fini (&context->base);

    free (context);
}

static void
_glitz_wgl_copy_context (void          *abstract_src,
			 void          *abstract_dst,
			 unsigned long  mask)
{
    glitz_wgl_context_t  *src = (glitz_wgl_context_t *) abstract_src;
    glitz_wgl_context_t  *dst = (glitz_wgl_context_t *) abstract_dst;

    wglCopyContext (src->context, dst->context, mask);
}

static void
_glitz_wgl_make_current (void *abstract_drawable,
                         void *abstract_context)
{
    glitz_wgl_context_t  *context = (glitz_wgl_context_t *) abstract_context;
    glitz_wgl_drawable_t *drawable = (glitz_wgl_drawable_t *) abstract_drawable;

    if (wglGetCurrentContext() != context->context ||
	wglGetCurrentDC() != drawable->dc)
    {
        if (drawable->screen_info->thread_info->cctx) {
	    glitz_context_t *ctx = drawable->screen_info->thread_info->cctx;

            if (ctx->lose_current)
                ctx->lose_current (ctx->closure);

            drawable->screen_info->thread_info->cctx = NULL;
        }

	wglMakeCurrent (drawable->dc, context->context);
    }

    drawable->screen_info->thread_info->cctx = &context->base;
}

static void
_glitz_wgl_notify_dummy (void            *abstract_drawable,
			 glitz_surface_t *surface) {}

static glitz_function_pointer_t
_glitz_wgl_context_get_proc_address (void       *abstract_context,
				     const char *name)
{
    glitz_wgl_context_t  *context = (glitz_wgl_context_t *) abstract_context;
    glitz_wgl_drawable_t *drawable = (glitz_wgl_drawable_t *)
	context->base.drawable;

    /* _glitz_wgl_make_current (drawable, context, NULL); */

    return glitz_wgl_get_proc_address (name, drawable->screen_info);
}

glitz_wgl_context_t *
glitz_wgl_context_get (glitz_wgl_screen_info_t          *screen_info,
		       HDC	        dc,
		       glitz_drawable_format_t          *format)
{
    glitz_wgl_context_t *context;
    glitz_wgl_context_t **contexts = screen_info->contexts;
    int index, n_contexts = screen_info->n_contexts;
    int format_id;

    for (; n_contexts; n_contexts--, contexts++)
	if ((*contexts)->pixel_format == screen_info->format_ids[format->id])
	    return *contexts;

    index = screen_info->n_contexts++;

    screen_info->contexts =
	realloc (screen_info->contexts,
		 sizeof (glitz_wgl_context_t *) * screen_info->n_contexts);

    context = malloc (sizeof (glitz_wgl_context_t));

    screen_info->contexts[index] = context;

    format_id = screen_info->format_ids[format->id];

    _glitz_wgl_context_create (screen_info,
			       dc,
			       format_id,
			       context);

    wglShareLists (screen_info->root_context, context->context);

#if 0
    /* Not needed as the root_context is created already when the
     * singleton screen_info is set up.
     */
    if (!screen_info->root_context)
	screen_info->root_context = context->context;
#endif

    context->backend.gl = &_glitz_wgl_gl_proc_address;

    context->backend.create_pbuffer = glitz_wgl_create_pbuffer;
    context->backend.destroy = glitz_wgl_destroy;
    context->backend.push_current = glitz_wgl_push_current;
    context->backend.pop_current = glitz_wgl_pop_current;
    context->backend.attach_notify = _glitz_wgl_notify_dummy;
    context->backend.detach_notify = _glitz_wgl_notify_dummy;
    context->backend.swap_buffers = glitz_wgl_swap_buffers;
    context->backend.copy_sub_buffer = glitz_wgl_copy_sub_buffer;

    context->backend.create_context = _glitz_wgl_create_context;
    context->backend.destroy_context = _glitz_wgl_destroy_context;
    context->backend.copy_context = _glitz_wgl_copy_context;
    context->backend.make_current = _glitz_wgl_make_current;
    context->backend.get_proc_address = _glitz_wgl_context_get_proc_address;

    context->backend.draw_buffer = _glitz_drawable_draw_buffer;
    context->backend.read_buffer = _glitz_drawable_read_buffer;

    context->backend.drawable_formats = screen_info->formats;
    context->backend.n_drawable_formats = screen_info->n_formats;

    context->backend.texture_formats = NULL;
    context->backend.formats = NULL;
    context->backend.n_formats = 0;

    context->backend.program_map = &screen_info->program_map;
    context->backend.feature_mask = 0;

    context->initialized = 0;

    return context;
}

void
glitz_wgl_context_destroy (glitz_wgl_screen_info_t *screen_info,
			   glitz_wgl_context_t     *context)
{
    if (context->backend.formats)
	free (context->backend.formats);

    if (context->backend.texture_formats)
	free (context->backend.texture_formats);

    wglDeleteContext (context->context);

    free (context);
}

static void
_glitz_wgl_context_initialize (glitz_wgl_screen_info_t *screen_info,
			       glitz_wgl_context_t     *context)
{
    glitz_backend_init (&context->backend,
			glitz_wgl_get_proc_address,
			(void *) screen_info);

    context->backend.gl->get_integer_v (GLITZ_GL_MAX_VIEWPORT_DIMS,
					context->max_viewport_dims);

    glitz_initiate_state (&_glitz_wgl_gl_proc_address);

    context->initialized = 1;
}

static void
_glitz_wgl_context_make_current (glitz_wgl_drawable_t *drawable,
				 glitz_bool_t         flush)
{
    if (flush)
    {
	glFlush ();
    }

    if (drawable->screen_info->thread_info->cctx) {
        glitz_context_t *ctx = drawable->screen_info->thread_info->cctx;

        if (ctx->lose_current)
            ctx->lose_current (ctx->closure);

        drawable->screen_info->thread_info->cctx = NULL;
    }

    wglMakeCurrent (drawable->dc, drawable->context->context);

    drawable->base.update_all = 1;

    if (!drawable->context->initialized)
	_glitz_wgl_context_initialize (drawable->screen_info, drawable->context);
}

static void
_glitz_wgl_context_update (glitz_wgl_drawable_t *drawable,
			   glitz_constraint_t   constraint)
{
    HGLRC context;

    drawable->base.flushed = drawable->base.finished = 0;

    switch (constraint) {
    case GLITZ_NONE:
	break;
    case GLITZ_ANY_CONTEXT_CURRENT:
	context = wglGetCurrentContext ();
	if (context == NULL)
	    _glitz_wgl_context_make_current (drawable, 0);
	break;
    case GLITZ_CONTEXT_CURRENT:
	context = wglGetCurrentContext ();
	if (context != drawable->context->context)
	    _glitz_wgl_context_make_current (drawable, (context)? 1: 0);
	break;
    case GLITZ_DRAWABLE_CURRENT:
	context = wglGetCurrentContext ();
	if ((context != drawable->context->context) ||
	    (wglGetCurrentDC () != drawable->dc))
	    _glitz_wgl_context_make_current (drawable, (context)? 1: 0);
	break;
    }
}

glitz_bool_t
glitz_wgl_push_current (void               *abstract_drawable,
			glitz_surface_t    *surface,
			glitz_constraint_t constraint,
			glitz_bool_t       *restore_state)
{
    glitz_wgl_drawable_t *drawable = (glitz_wgl_drawable_t *) abstract_drawable;
    glitz_wgl_context_info_t *context_info;
    int index;

    if (restore_state)
	*restore_state = 0;

    index = drawable->screen_info->context_stack_size++;

    context_info = &drawable->screen_info->context_stack[index];
    context_info->drawable = drawable;
    context_info->surface = surface;
    context_info->constraint = constraint;

    _glitz_wgl_context_update (context_info->drawable, constraint);

    return 1;
}

glitz_surface_t *
glitz_wgl_pop_current (void *abstract_drawable)
{
    glitz_wgl_drawable_t *drawable = (glitz_wgl_drawable_t *) abstract_drawable;
    glitz_wgl_context_info_t *context_info = NULL;
    int index;

    drawable->screen_info->context_stack_size--;
    index = drawable->screen_info->context_stack_size - 1;

    context_info = &drawable->screen_info->context_stack[index];

    if (context_info->drawable)
	_glitz_wgl_context_update (context_info->drawable,
				   context_info->constraint);

    if (context_info->constraint == GLITZ_DRAWABLE_CURRENT)
	return context_info->surface;

    return NULL;
}
