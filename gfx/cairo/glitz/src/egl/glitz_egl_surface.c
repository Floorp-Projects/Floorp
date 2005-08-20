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

#include "glitz_eglint.h"

static glitz_egl_surface_t *
_glitz_egl_create_surface (glitz_egl_screen_info_t *screen_info,
                           glitz_egl_context_t     *context,
                           glitz_drawable_format_t *format,
                           EGLSurface              egl_surface,
                           int                     width,
                           int                     height)
{
  glitz_egl_surface_t *surface;
  
  if (width <= 0 || height <= 0)
    return NULL;

  surface = (glitz_egl_surface_t *) malloc (sizeof (glitz_egl_surface_t));
  if (surface == NULL)
    return NULL;

  surface->base.ref_count = 1;
  surface->screen_info = screen_info;
  surface->context = context;
  surface->egl_surface = egl_surface;
  surface->base.format = format;
  surface->base.backend = &context->backend;

  glitz_drawable_update_size (&surface->base, width, height);

  if (!context->initialized) {
    glitz_egl_push_current (surface, NULL, GLITZ_CONTEXT_CURRENT);
    glitz_egl_pop_current (surface);
  }
  
  if (width > context->max_viewport_dims[0] ||
      height > context->max_viewport_dims[1]) {
    free (surface);
    return NULL;
  }
  
  screen_info->drawables++;
  
  return surface;
}

static glitz_drawable_t *
_glitz_egl_create_pbuffer_surface (glitz_egl_screen_info_t *screen_info,
                                    glitz_drawable_format_t *format,
                                    unsigned int            width,
                                    unsigned int            height)
{
  glitz_egl_surface_t *surface;
  glitz_egl_context_t *context;
  EGLSurface egl_pbuffer;

  if (!format->types.pbuffer)
    return NULL;
  
  context = glitz_egl_context_get (screen_info, format);
  if (!context)
    return NULL;

  egl_pbuffer = glitz_egl_pbuffer_create (screen_info, context->egl_config,
                                      (int) width, (int) height);
  if (!egl_pbuffer)
    return NULL;
  
  surface = _glitz_egl_create_surface (screen_info, context, format,
                                         egl_pbuffer,
                                         width, height);
  if (!surface) {
    glitz_egl_pbuffer_destroy (screen_info, egl_pbuffer);
    return NULL;
  }
  
  return &surface->base;
}

glitz_drawable_t *
glitz_egl_create_pbuffer (void                    *abstract_templ,
                          glitz_drawable_format_t *format,
                          unsigned int            width,
                          unsigned int            height)
{
  glitz_egl_surface_t *templ = (glitz_egl_surface_t *) abstract_templ;

  return _glitz_egl_create_pbuffer_surface (templ->screen_info, format,
                                             width, height);
}

glitz_drawable_t *
glitz_egl_create_surface (EGLDisplay              egl_display,
                          EGLScreenMESA           egl_screen,
                          glitz_drawable_format_t *format,
                          EGLSurface              egl_surface,
                          unsigned int            width,
                          unsigned int            height)
{
  glitz_egl_surface_t *surface;
  glitz_egl_screen_info_t *screen_info;
  glitz_egl_context_t *context;

  screen_info = glitz_egl_screen_info_get (egl_display, egl_screen);
  if (!screen_info)
    return NULL;
  
  context = glitz_egl_context_get (screen_info, format);
  if (!context)
    return NULL;
  
  surface = _glitz_egl_create_surface (screen_info, context, format,
                                         egl_surface,
                                         width, height);
  if (!surface)
    return NULL;

  return &surface->base;
}
slim_hidden_def(glitz_egl_create_surface);

glitz_drawable_t *
glitz_egl_create_pbuffer_surface (EGLDisplay              display,
                                   EGLScreenMESA           screen,
                                   glitz_drawable_format_t *format,
                                   unsigned int            width,
                                   unsigned int            height)
{
  glitz_egl_screen_info_t *screen_info;

  screen_info = glitz_egl_screen_info_get (display, screen);
  if (!screen_info)
    return NULL;

  return _glitz_egl_create_pbuffer_surface (screen_info, format,
                                             width, height);
}
slim_hidden_def(glitz_egl_create_pbuffer_surface);

void
glitz_egl_destroy (void *abstract_drawable)
{
  EGLint value;
  glitz_egl_surface_t *surface = (glitz_egl_surface_t *) abstract_drawable;

  surface->screen_info->drawables--;
  if (surface->screen_info->drawables == 0) {
    /*
     * Last drawable? We have to destroy all fragment programs as this may
     * be our last chance to have a context current.
     */
    glitz_egl_push_current (abstract_drawable, NULL, GLITZ_CONTEXT_CURRENT);
    glitz_program_map_fini (&surface->base.backend->gl,
                            &surface->screen_info->program_map);
    glitz_egl_pop_current (abstract_drawable);
  }

  if (eglGetCurrentSurface ( 0 ) == surface->egl_surface)
    eglMakeCurrent (surface->screen_info->display_info->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  
  eglQuerySurface (surface->screen_info->display_info->egl_display, surface->egl_surface, 
                    EGL_SURFACE_TYPE, &value);
  if (value == EGL_PBUFFER_BIT)
    glitz_egl_pbuffer_destroy (surface->screen_info, surface->egl_surface);
  
  free (surface);
}

void
glitz_egl_swap_buffers (void *abstract_drawable)
{
  glitz_egl_surface_t *surface = (glitz_egl_surface_t *) abstract_drawable;

  eglSwapBuffers (surface->screen_info->display_info->egl_display,
                  surface->egl_surface);
}
