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
glitz_context_make_current (glitz_context_t *context)
{
    context->drawable->backend->make_current (context, context->drawable);
}
slim_hidden_def(glitz_context_make_current);

void
glitz_context_bind_texture (glitz_context_t *context,
                            glitz_surface_t *surface)
{
    glitz_gl_proc_address_list_t *gl = &context->drawable->backend->gl;
    
    if (!surface->texture.name)
        gl->gen_textures (1, &surface->texture.name);
  
    gl->bind_texture (surface->texture.target, surface->texture.name);
}
slim_hidden_def(glitz_context_bind_texture);
