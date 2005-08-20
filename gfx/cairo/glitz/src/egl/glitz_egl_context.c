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

#include <stdlib.h>

extern glitz_gl_proc_address_list_t _glitz_egl_gl_proc_address;

static void
_glitz_egl_context_create (glitz_egl_screen_info_t *screen_info,
                                          EGLConfig               egl_config,
                                          EGLContext              egl_share_list,
                                          glitz_egl_context_t     *context)
{
  context->id = egl_config;
  context->egl_context = eglCreateContext (screen_info->display_info->egl_display,
                                         egl_config, egl_share_list, NULL);
}

static glitz_context_t *
_glitz_egl_create_context (void                    *abstract_drawable,
                           glitz_drawable_format_t *format)
{
  glitz_egl_surface_t *drawable = (glitz_egl_surface_t *) abstract_drawable;
  glitz_egl_screen_info_t *screen_info = drawable->screen_info;
  int format_id = screen_info->egl_config_ids[format->id];
  glitz_egl_context_t *context;
  
  context = malloc (sizeof (glitz_egl_context_t));
  if (!context)
    return NULL;

  _glitz_context_init (&context->base, &drawable->base);
  
  _glitz_egl_context_create (screen_info,
                             format_id,
                             screen_info->egl_root_context,
                             context);

  return (glitz_context_t *) context;
}

static void
_glitz_egl_context_destroy (void *abstract_context)
{
  glitz_egl_context_t *context = (glitz_egl_context_t *) abstract_context;
  glitz_egl_surface_t *drawable = (glitz_egl_surface_t *)
      context->base.drawable;
  
  if (drawable->screen_info->display_info->thread_info->cctx == &context->base)
  {
      eglMakeCurrent (drawable->screen_info->display_info->egl_display,
                      EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  
      drawable->screen_info->display_info->thread_info->cctx = NULL;
  }

  eglDestroyContext (drawable->screen_info->display_info->egl_display,
                     context->egl_context);

  _glitz_context_fini (&context->base);
  
  free (context);
}

static void
_glitz_egl_copy_context (void          *abstract_src,
                         void          *abstract_dst,
                         unsigned long mask)
{
  glitz_egl_context_t  *src = (glitz_egl_context_t *) abstract_src;
  glitz_egl_context_t  *dst = (glitz_egl_context_t *) abstract_dst;
  glitz_egl_surface_t *drawable = (glitz_egl_surface_t *)
      src->base.drawable;

  eglCopyContextMESA (drawable->screen_info->display_info->egl_display,
                  src->egl_context, dst->egl_context, mask);
}

static void
_glitz_egl_make_current (void *abstract_context,
                         void *abstract_drawable)
{
  glitz_egl_context_t  *context = (glitz_egl_context_t *) abstract_context;
  glitz_egl_surface_t *drawable = (glitz_egl_surface_t *) abstract_drawable;
  glitz_egl_display_info_t *display_info = drawable->screen_info->display_info;
  
  if ((eglGetCurrentContext () != context->egl_context) ||
      (eglGetCurrentSurface ( 0 ) != drawable->egl_surface))
    eglMakeCurrent (display_info->egl_display, drawable->egl_surface,
                    drawable->egl_surface, context->egl_context);

  display_info->thread_info->cctx = &context->base;
}

static glitz_function_pointer_t
_glitz_egl_context_get_proc_address (void       *abstract_context,
                                     const char *name)
{
  glitz_egl_context_t  *context = (glitz_egl_context_t *) abstract_context;
  glitz_egl_surface_t *drawable = (glitz_egl_surface_t *)
      context->base.drawable;

  _glitz_egl_make_current (context, drawable);
  
  return glitz_egl_get_proc_address (name, drawable->screen_info);
}

glitz_egl_context_t *
glitz_egl_context_get (glitz_egl_screen_info_t *screen_info,
                       glitz_drawable_format_t *format)
{
  glitz_egl_context_t *context;
  glitz_egl_context_t **contexts = screen_info->contexts;
  int index, n_contexts = screen_info->n_contexts;
  EGLConfig egl_config_id;

  for (; n_contexts; n_contexts--, contexts++)
    if ((*contexts)->id == screen_info->egl_config_ids[format->id])
      return *contexts;

  index = screen_info->n_contexts++;

  screen_info->contexts =
    realloc (screen_info->contexts,
             sizeof (glitz_egl_context_t *) * screen_info->n_contexts);
  if (!screen_info->contexts)
    return NULL;

  context = malloc (sizeof (glitz_egl_context_t));
  if (!context)
    return NULL;
  
  screen_info->contexts[index] = context;

  egl_config_id = screen_info->egl_config_ids[format->id];

  _glitz_egl_context_create (screen_info,
                             egl_config_id,
                             screen_info->egl_root_context,
                             context);
  
  if (!screen_info->egl_root_context)
    screen_info->egl_root_context = context->egl_context;

  memcpy (&context->backend.gl,
          &_glitz_egl_gl_proc_address,
          sizeof (glitz_gl_proc_address_list_t));

  context->backend.create_pbuffer = glitz_egl_create_pbuffer;
  context->backend.destroy = glitz_egl_destroy;
  context->backend.push_current = glitz_egl_push_current;
  context->backend.pop_current = glitz_egl_pop_current;
  context->backend.swap_buffers = glitz_egl_swap_buffers;
  
  context->backend.create_context = _glitz_egl_create_context;
  context->backend.destroy_context = _glitz_egl_context_destroy;
  context->backend.copy_context = _glitz_egl_copy_context;
  context->backend.make_current = _glitz_egl_make_current;
  context->backend.get_proc_address = _glitz_egl_context_get_proc_address;
  
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
glitz_egl_context_destroy (glitz_egl_screen_info_t *screen_info,
                           glitz_egl_context_t     *context)
{
  if (context->backend.formats)
    free (context->backend.formats);

  if (context->backend.texture_formats)
    free (context->backend.texture_formats);
  
  eglDestroyContext (screen_info->display_info->egl_display,
                     context->egl_context);
  free (context);
}

static void
_glitz_egl_context_initialize (glitz_egl_screen_info_t *screen_info,
                               glitz_egl_context_t     *context)
{
  const char *version;
    
  glitz_backend_init (&context->backend,
                      glitz_egl_get_proc_address,
                      (void *) screen_info);

  context->backend.gl.get_integer_v (GLITZ_GL_MAX_VIEWPORT_DIMS,
                                     context->max_viewport_dims);

  glitz_initiate_state (&_glitz_egl_gl_proc_address);

  version = (const char *) context->backend.gl.get_string (GLITZ_GL_VERSION);
  if (version)
  {
    /* Having trouble with TexSubImage2D to NPOT GL_TEXTURE_2D textures when
       using nvidia's binary driver. Seems like a driver issue, but I'm not
       sure yet. Turning of NPOT GL_TEXTURE_2D textures until this have been
       solved. */
    if (strstr (version, "NVIDIA 61.11") ||
        strstr (version, "NVIDIA 66.29"))
    {
      context->backend.feature_mask &=
        ~GLITZ_FEATURE_TEXTURE_NON_POWER_OF_TWO_MASK;
    }
  }
    
  context->initialized = 1;
}

static void
_glitz_egl_context_make_current (glitz_egl_surface_t *drawable,
                                 glitz_bool_t         finish)
{
  glitz_egl_display_info_t *display_info = drawable->screen_info->display_info;
                                 
  if (finish)
    glFinish ();

  if (display_info->thread_info->cctx)
  {
      glitz_context_t *ctx = display_info->thread_info->cctx;

      if (ctx->lose_current)
          ctx->lose_current (ctx->closure);

      display_info->thread_info->cctx = NULL;
  }

  eglMakeCurrent (display_info->egl_display,
                  drawable->egl_surface, drawable->egl_surface,
                  drawable->context->egl_context);
  
  drawable->base.update_all = 1;
  
  if (!drawable->context->initialized)
    _glitz_egl_context_initialize (drawable->screen_info, drawable->context);
}

static void
_glitz_egl_context_update (glitz_egl_surface_t *drawable,
                           glitz_constraint_t   constraint)
{
  EGLContext egl_context;
  
  switch (constraint) {
  case GLITZ_NONE:
    break;
  case GLITZ_ANY_CONTEXT_CURRENT: {
    glitz_egl_display_info_t *dinfo = drawable->screen_info->display_info;
    
    if (dinfo->thread_info->cctx)
    {
      _glitz_egl_context_make_current (drawable, 0);
    }
    else
    {
      egl_context = eglGetCurrentContext ();
      if (egl_context == (EGLContext) 0)
        _glitz_egl_context_make_current (drawable, 0);
    }
    } break;
  case GLITZ_CONTEXT_CURRENT:
    egl_context = eglGetCurrentContext ();
    if (egl_context != drawable->context->egl_context)
      _glitz_egl_context_make_current (drawable, (egl_context)? 1: 0);
    break;
  case GLITZ_DRAWABLE_CURRENT:
    egl_context = eglGetCurrentContext ();
    if ((egl_context != drawable->context->egl_context) ||
        (eglGetCurrentSurface ( 0 ) != drawable->egl_surface))
      _glitz_egl_context_make_current (drawable, (egl_context)? 1: 0);
    break;
  }
}

void
glitz_egl_push_current (void               *abstract_drawable,
                        glitz_surface_t    *surface,
                        glitz_constraint_t constraint)
{
  glitz_egl_surface_t *drawable = (glitz_egl_surface_t *) abstract_drawable;
  glitz_egl_context_info_t *context_info;
  int index;

  index = drawable->screen_info->context_stack_size++;

  context_info = &drawable->screen_info->context_stack[index];
  context_info->drawable = drawable;
  context_info->surface = surface;
  context_info->constraint = constraint;
  
  _glitz_egl_context_update (context_info->drawable, constraint);
}

glitz_surface_t *
glitz_egl_pop_current (void *abstract_drawable)
{
  glitz_egl_surface_t *drawable = (glitz_egl_surface_t *) abstract_drawable;
  glitz_egl_context_info_t *context_info = NULL;
  int index;

  drawable->screen_info->context_stack_size--;
  index = drawable->screen_info->context_stack_size - 1;

  context_info = &drawable->screen_info->context_stack[index];

  if (context_info->drawable)
    _glitz_egl_context_update (context_info->drawable,
                               context_info->constraint);
  
  if (context_info->constraint == GLITZ_DRAWABLE_CURRENT)
      return context_info->surface;
  
  return NULL;
}
