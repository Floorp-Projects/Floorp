/*
 * Copyright © 2004 David Reveman
 * 
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
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
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "glitz_aglint.h"

static glitz_agl_drawable_t *
_glitz_agl_create_drawable (glitz_agl_thread_info_t *thread_info,
                            glitz_agl_context_t     *context,
                            glitz_drawable_format_t *format,
                            AGLDrawable             agl_drawable,
                            AGLPbuffer              agl_pbuffer,
                            unsigned int            width,
                            unsigned int            height)
{
  glitz_agl_drawable_t *drawable;
  
  if (width <= 0 || height <= 0)
    return NULL;

  drawable = (glitz_agl_drawable_t *) malloc (sizeof (glitz_agl_drawable_t));
  if (drawable == NULL)
    return NULL;

  drawable->base.ref_count = 1;
  drawable->thread_info = thread_info;
  drawable->context = context;
  drawable->drawable = agl_drawable;
  drawable->pbuffer = agl_pbuffer;
  drawable->base.format = format;
  drawable->base.backend = &context->backend;

  glitz_drawable_update_size (&drawable->base, width, height);
  
  if (!context->initialized) {
    glitz_agl_push_current (drawable, NULL, GLITZ_CONTEXT_CURRENT);
    glitz_agl_pop_current (drawable);
  }
  
  if (width > context->max_viewport_dims[0] ||
      height > context->max_viewport_dims[1]) {
    free (drawable);
    return NULL;
  }

  thread_info->drawables++;
  
  return drawable;
}

static glitz_drawable_t *
_glitz_agl_create_pbuffer_drawable (glitz_agl_thread_info_t *thread_info,
                                    glitz_drawable_format_t *format,
                                    unsigned int            width,
                                    unsigned int            height)
{
  glitz_agl_drawable_t *drawable;
  glitz_agl_context_t *context;
  AGLPbuffer pbuffer;

  if (!format->types.pbuffer)
    return NULL;
  
  context = glitz_agl_context_get (thread_info, format);
  if (!context)
    return NULL;

  pbuffer = glitz_agl_pbuffer_create (thread_info, (int) width, (int) height);
  if (!pbuffer)
    return NULL;
  
  drawable = _glitz_agl_create_drawable (thread_info, context, format,
                                         (AGLDrawable) 0, pbuffer,
                                         width, height);
  if (!drawable) {
    glitz_agl_pbuffer_destroy (pbuffer);
    return NULL;
  }
  
  return &drawable->base;
}

glitz_drawable_t *
glitz_agl_create_pbuffer (void                    *abstract_templ,
                          glitz_drawable_format_t *format,
                          unsigned int            width,
                          unsigned int            height)
{
  glitz_agl_drawable_t *templ = (glitz_agl_drawable_t *) abstract_templ;

  return _glitz_agl_create_pbuffer_drawable (templ->thread_info, format,
                                             width, height);
}

glitz_drawable_t *
glitz_agl_create_drawable_for_window (glitz_drawable_format_t *format,
                                      WindowRef               window,
                                      unsigned int            width,
                                      unsigned int            height)
{
  glitz_agl_drawable_t *drawable;
  glitz_agl_thread_info_t *thread_info;
  glitz_agl_context_t *context;
  AGLDrawable agl_drawable;

  agl_drawable = (AGLDrawable) GetWindowPort (window);
  if (!agl_drawable)
    return NULL;

  thread_info = glitz_agl_thread_info_get ();
  if (!thread_info)
    return NULL;

  context = glitz_agl_context_get (thread_info, format);
  if (!context)
    return NULL;

  drawable = _glitz_agl_create_drawable (thread_info, context, format,
                                         agl_drawable, (AGLPbuffer) 0,
                                         width, height);
  if (!drawable)
    return NULL;

  return &drawable->base;
}
slim_hidden_def(glitz_agl_create_drawable_for_window);

glitz_drawable_t *
glitz_agl_create_pbuffer_drawable (glitz_drawable_format_t *format,
                                   unsigned int            width,
                                   unsigned int            height)
{
  glitz_agl_thread_info_t *thread_info;

  thread_info = glitz_agl_thread_info_get ();
  if (!thread_info)
    return NULL;

  return _glitz_agl_create_pbuffer_drawable (thread_info, format,
                                             width, height);
}
slim_hidden_def(glitz_agl_create_pbuffer_drawable);

void
glitz_agl_destroy (void *abstract_drawable)
{
  glitz_agl_drawable_t *drawable = (glitz_agl_drawable_t *) abstract_drawable;

  drawable->thread_info->drawables--;
  if (drawable->thread_info->drawables == 0) {
    /*
     * Last drawable? We have to destroy all fragment programs as this may
     * be our last chance to have a context current.
     */
    glitz_agl_push_current (abstract_drawable, NULL, GLITZ_CONTEXT_CURRENT);
    glitz_program_map_fini (&drawable->base.backend->gl,
                            &drawable->thread_info->program_map);
    glitz_agl_pop_current (abstract_drawable);
  }

  if (drawable->drawable || drawable->pbuffer) {
    AGLContext context = aglGetCurrentContext ();
    
    if (context == drawable->context->context) { 
      if (drawable->pbuffer) {
        AGLPbuffer pbuffer;
        GLuint unused;
        
        aglGetPBuffer (context, &pbuffer, &unused, &unused, &unused);
        
        if (pbuffer == drawable->pbuffer)
          aglSetCurrentContext (NULL);
      } else {
        if (aglGetDrawable (context) == drawable->drawable)
          aglSetCurrentContext (NULL);
      }
    }
    
    if (drawable->pbuffer)
      glitz_agl_pbuffer_destroy (drawable->pbuffer);
  }
  
  free (drawable);
}

void
glitz_agl_swap_buffers (void *abstract_drawable)
{
  glitz_agl_drawable_t *drawable = (glitz_agl_drawable_t *) abstract_drawable;

  glitz_agl_push_current (abstract_drawable, NULL, GLITZ_DRAWABLE_CURRENT);
  aglSwapBuffers (drawable->context->context);
  glitz_agl_pop_current (abstract_drawable);
}
