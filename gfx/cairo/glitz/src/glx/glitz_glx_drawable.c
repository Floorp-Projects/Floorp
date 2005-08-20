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

#include "glitz_glxint.h"

static glitz_glx_drawable_t *
_glitz_glx_create_drawable (glitz_glx_screen_info_t *screen_info,
                            glitz_glx_context_t     *context,
                            glitz_drawable_format_t *format,
                            GLXDrawable             glx_drawable,
                            GLXPbuffer              glx_pbuffer,
                            int                     width,
                            int                     height)
{
  glitz_glx_drawable_t *drawable;
  
  if (width <= 0 || height <= 0)
    return NULL;

  drawable = (glitz_glx_drawable_t *) malloc (sizeof (glitz_glx_drawable_t));
  if (drawable == NULL)
    return NULL;

  drawable->base.ref_count = 1;
  drawable->screen_info = screen_info;
  drawable->context = context;
  drawable->drawable = glx_drawable;
  drawable->pbuffer = glx_pbuffer;
  drawable->base.format = format;
  drawable->base.backend = &context->backend;

  glitz_drawable_update_size (&drawable->base, width, height);

  if (!context->initialized) {
    glitz_glx_push_current (drawable, NULL, GLITZ_CONTEXT_CURRENT);
    glitz_glx_pop_current (drawable);
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
_glitz_glx_create_pbuffer_drawable (glitz_glx_screen_info_t *screen_info,
                                    glitz_drawable_format_t *format,
                                    unsigned int            width,
                                    unsigned int            height)
{
  glitz_glx_drawable_t *drawable;
  glitz_glx_context_t *context;
  GLXPbuffer pbuffer;

  if (!format->types.pbuffer)
    return NULL;
  
  context = glitz_glx_context_get (screen_info, format);
  if (!context)
    return NULL;

  pbuffer = glitz_glx_pbuffer_create (screen_info, context->fbconfig,
                                      (int) width, (int) height);
  if (!pbuffer)
    return NULL;
  
  drawable = _glitz_glx_create_drawable (screen_info, context, format,
                                         pbuffer, pbuffer,
                                         width, height);
  if (!drawable) {
    glitz_glx_pbuffer_destroy (screen_info, pbuffer);
    return NULL;
  }
  
  return &drawable->base;
}

glitz_drawable_t *
glitz_glx_create_pbuffer (void                    *abstract_templ,
                          glitz_drawable_format_t *format,
                          unsigned int            width,
                          unsigned int            height)
{
  glitz_glx_drawable_t *templ = (glitz_glx_drawable_t *) abstract_templ;

  return _glitz_glx_create_pbuffer_drawable (templ->screen_info, format,
                                             width, height);
}

glitz_drawable_t *
glitz_glx_create_drawable_for_window (Display                 *display,
                                      int                     screen,
                                      glitz_drawable_format_t *format,
                                      Window                  window,
                                      unsigned int            width,
                                      unsigned int            height)
{
  glitz_glx_drawable_t *drawable;
  glitz_glx_screen_info_t *screen_info;
  glitz_glx_context_t *context;

  screen_info = glitz_glx_screen_info_get (display, screen);
  if (!screen_info)
    return NULL;
  
  context = glitz_glx_context_get (screen_info, format);
  if (!context)
    return NULL;
  
  drawable = _glitz_glx_create_drawable (screen_info, context, format,
                                         window, (GLXPbuffer) 0,
                                         width, height);
  if (!drawable)
    return NULL;

  return &drawable->base;
}
slim_hidden_def(glitz_glx_create_drawable_for_window);

glitz_drawable_t *
glitz_glx_create_pbuffer_drawable (Display                 *display,
                                   int                     screen,
                                   glitz_drawable_format_t *format,
                                   unsigned int            width,
                                   unsigned int            height)
{
  glitz_glx_screen_info_t *screen_info;

  screen_info = glitz_glx_screen_info_get (display, screen);
  if (!screen_info)
    return NULL;

  return _glitz_glx_create_pbuffer_drawable (screen_info, format,
                                             width, height);
}
slim_hidden_def(glitz_glx_create_pbuffer_drawable);

void
glitz_glx_destroy (void *abstract_drawable)
{
  glitz_glx_drawable_t *drawable = (glitz_glx_drawable_t *) abstract_drawable;

  drawable->screen_info->drawables--;
  if (drawable->screen_info->drawables == 0) {
    /*
     * Last drawable? We have to destroy all fragment programs as this may
     * be our last chance to have a context current.
     */
    glitz_glx_push_current (abstract_drawable, NULL, GLITZ_CONTEXT_CURRENT);
    glitz_program_map_fini (&drawable->base.backend->gl,
                            &drawable->screen_info->program_map);
    glitz_glx_pop_current (abstract_drawable);
  }

  if (glXGetCurrentDrawable () == drawable->drawable)
    glXMakeCurrent (drawable->screen_info->display_info->display, None, NULL);
  
  if (drawable->pbuffer)
    glitz_glx_pbuffer_destroy (drawable->screen_info, drawable->pbuffer);
  
  free (drawable);
}

void
glitz_glx_swap_buffers (void *abstract_drawable)
{
  glitz_glx_drawable_t *drawable = (glitz_glx_drawable_t *) abstract_drawable;

  glXSwapBuffers (drawable->screen_info->display_info->display,
                  drawable->drawable);
}
