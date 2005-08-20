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

extern glitz_gl_proc_address_list_t _glitz_agl_gl_proc_address;

static CFBundleRef
_glitz_agl_get_bundle (const char *name)
{
  CFBundleRef bundle = 0;
  FSRefParam ref_param;
  unsigned char framework_name[256];

  framework_name[0] = strlen (name);
  strcpy (&framework_name[1], name);
  
  memset (&ref_param, 0, sizeof (ref_param));

  if (FindFolder (kSystemDomain,
                  kFrameworksFolderType,
                  kDontCreateFolder,
                  &ref_param.ioVRefNum,
                  &ref_param.ioDirID) == noErr) {
    FSRef ref;

    memset (&ref, 0, sizeof (ref));

    ref_param.ioNamePtr = framework_name;
    ref_param.newRef = &ref;

    if (PBMakeFSRefSync (&ref_param) == noErr) {
      CFURLRef url;

      url = CFURLCreateFromFSRef (kCFAllocatorDefault, &ref);
      if (url) {
        bundle = CFBundleCreate (kCFAllocatorDefault, url);
        CFRelease (url);

        if (!CFBundleLoadExecutable (bundle)) {
          CFRelease (bundle);
          return (CFBundleRef) 0;
        }
      }
    }
  }
    
  return bundle;
}

static void
_glitz_agl_release_bundle (CFBundleRef bundle)
{
  if (bundle) {
    CFBundleUnloadExecutable (bundle);
    CFRelease (bundle);
  }
}

static glitz_function_pointer_t
_glitz_agl_get_proc_address (const char *name, void *closure)
{
  glitz_function_pointer_t address = NULL;
  CFBundleRef bundle = (CFBundleRef) closure;
  CFStringRef str;
  
  if (bundle) {
    str = CFStringCreateWithCString (kCFAllocatorDefault, name,
                                     kCFStringEncodingMacRoman);

    address = CFBundleGetFunctionPointerForName (bundle, str);

    CFRelease (str);
  }
  
  return address;
}

static glitz_context_t *
_glitz_agl_create_context (void                    *abstract_drawable,
                           glitz_drawable_format_t *format)
{
  glitz_agl_drawable_t *drawable = (glitz_agl_drawable_t *) abstract_drawable;
  glitz_agl_thread_info_t *thread_info = drawable->thread_info;
  glitz_agl_context_t *context;
  
  context = malloc (sizeof (glitz_agl_context_t));
  if (!context)
    return NULL;

  context->context =
    aglCreateContext (thread_info->pixel_formats[format->id],
                      thread_info->root_context);
  
  _glitz_context_init (&context->base, &drawable->base);

  context->pbuffer = 0;

  return (glitz_context_t *) context;
}

static void
_glitz_agl_context_destroy (void *abstract_context)
{
  glitz_agl_context_t *context = (glitz_agl_context_t *) abstract_context;
  glitz_agl_drawable_t *drawable = (glitz_agl_drawable_t *)
      context->base.drawable;

  if (drawable->thread_info->cctx == &context->base)
  {
      aglSetCurrentContext (NULL);

      drawable->thread_info->cctx = NULL;
  }

  aglDestroyContext (context->context);

  _glitz_context_fini (&context->base);
  
  free (context);
}

static void
_glitz_agl_copy_context (void          *abstract_src,
                         void          *abstract_dst,
                         unsigned long mask)
{
  glitz_agl_context_t  *src = (glitz_agl_context_t *) abstract_src;
  glitz_agl_context_t  *dst = (glitz_agl_context_t *) abstract_dst;

  aglCopyContext (src->context, dst->context, mask);
}

static void
_glitz_agl_make_current (void *abstract_context,
                         void *abstract_drawable)
{
  glitz_agl_context_t  *context = (glitz_agl_context_t *) abstract_context;
  glitz_agl_drawable_t *drawable = (glitz_agl_drawable_t *) abstract_drawable;
  int update = 0;
  
  if (aglGetCurrentContext () != context->context)
  {
      update = 1;
  }
  else
  {
      if (drawable->pbuffer)
      {
          AGLPbuffer pbuffer;
          GLuint unused;

          aglGetPBuffer (context->context, &pbuffer,
                         &unused, &unused, &unused);
        
        if (pbuffer != drawable->pbuffer)
            update = 1;
        
      }
      else if (drawable->drawable)
      {
          if (aglGetDrawable (context->context) != drawable->drawable)
              update = 1;
      }
  }
  
  if (update)
  {
      if (drawable->thread_info->cctx)
      {
          glitz_context_t *ctx = drawable->thread_info->cctx;

          if (ctx->lose_current)
              ctx->lose_current (ctx->closure);
      }
   
      if (drawable->pbuffer) {
          aglSetPBuffer (context->context, drawable->pbuffer, 0, 0,
                         aglGetVirtualScreen (context->context));
          context->pbuffer = 1;
      }
      else
      {
          if (context->pbuffer) {
              aglSetDrawable (context->context, NULL);
              context->pbuffer = 0;
          }
          aglSetDrawable (context->context, drawable->drawable);
      }
      
      aglSetCurrentContext (context->context);
  }
  
  drawable->thread_info->cctx = &context->base;
}

static glitz_function_pointer_t
_glitz_agl_context_get_proc_address (void       *abstract_context,
                                     const char *name)
{
  glitz_agl_context_t  *context = (glitz_agl_context_t *) abstract_context;
  glitz_agl_drawable_t *drawable = (glitz_agl_drawable_t *)
      context->base.drawable;
  glitz_function_pointer_t func;
  CFBundleRef bundle;

  _glitz_agl_make_current (context, drawable);

  bundle = _glitz_agl_get_bundle ("OpenGL.framework");

  func = _glitz_agl_get_proc_address (name, (void *) bundle);

  _glitz_agl_release_bundle (bundle);

  return func;
}

glitz_agl_context_t *
glitz_agl_context_get (glitz_agl_thread_info_t *thread_info,
                       glitz_drawable_format_t *format)
{
  glitz_agl_context_t *context;
  glitz_agl_context_t **contexts = thread_info->contexts;
  int index, n_contexts = thread_info->n_contexts;

  for (; n_contexts; n_contexts--, contexts++)
    if ((*contexts)->id == format->id)
      return *contexts;
  
  index = thread_info->n_contexts++;

  thread_info->contexts =
    realloc (thread_info->contexts,
             sizeof (glitz_agl_context_t *) * thread_info->n_contexts);
  if (!thread_info->contexts)
    return NULL;

  context = malloc (sizeof (glitz_agl_context_t));
  if (!context)
    return NULL;
  
  thread_info->contexts[index] = context;

  context->context =
    aglCreateContext (thread_info->pixel_formats[format->id],
                      thread_info->root_context);
  if (!context->context) {
    free (context);
    return NULL;
  } 
  
  context->id = format->id;
  context->pbuffer = 0;

  if (!thread_info->root_context)
    thread_info->root_context = context->context;

  memcpy (&context->backend.gl,
          &_glitz_agl_gl_proc_address,
          sizeof (glitz_gl_proc_address_list_t));

  context->backend.create_pbuffer = glitz_agl_create_pbuffer;
  context->backend.destroy = glitz_agl_destroy;
  context->backend.push_current = glitz_agl_push_current;
  context->backend.pop_current = glitz_agl_pop_current;
  context->backend.swap_buffers = glitz_agl_swap_buffers;

  context->backend.create_context = _glitz_agl_create_context;
  context->backend.destroy_context = _glitz_agl_context_destroy;
  context->backend.copy_context = _glitz_agl_copy_context;
  context->backend.make_current = _glitz_agl_make_current;
  context->backend.get_proc_address = _glitz_agl_context_get_proc_address;
  
  context->backend.drawable_formats = thread_info->formats;
  context->backend.n_drawable_formats = thread_info->n_formats;

  context->backend.texture_formats = NULL;
  context->backend.formats = NULL;
  context->backend.n_formats = 0;
  
  context->backend.program_map = &thread_info->program_map;
  context->backend.feature_mask = 0;

  context->initialized = 0;
  
  return context;
}

void
glitz_agl_context_destroy (glitz_agl_thread_info_t *thread_info,
                           glitz_agl_context_t *context)
{
  if (context->backend.formats)
    free (context->backend.formats);

  if (context->backend.texture_formats)
    free (context->backend.texture_formats);
  
  aglDestroyContext (context->context);
  
  free (context);
}

static void
_glitz_agl_context_initialize (glitz_agl_thread_info_t *thread_info,
                               glitz_agl_context_t     *context)
{
  CFBundleRef bundle;
  
  bundle = _glitz_agl_get_bundle ("OpenGL.framework");

  glitz_backend_init (&context->backend,
                      _glitz_agl_get_proc_address,
                      (void *) bundle);

  _glitz_agl_release_bundle (bundle);

  context->backend.gl.get_integer_v (GLITZ_GL_MAX_VIEWPORT_DIMS,
                                     context->max_viewport_dims);

  glitz_initiate_state (&_glitz_agl_gl_proc_address);
    
  context->initialized = 1;
}

static void
_glitz_agl_context_make_current (glitz_agl_drawable_t *drawable,
                                 glitz_bool_t         finish)
{
  if (finish)
    glFinish ();

  if (drawable->thread_info->cctx)
  {
      glitz_context_t *ctx = drawable->thread_info->cctx;

      if (ctx->lose_current)
          ctx->lose_current (ctx->closure);

      drawable->thread_info->cctx = NULL;
  }
  
  if (drawable->pbuffer) {
    aglSetPBuffer (drawable->context->context, drawable->pbuffer, 0, 0,
                   aglGetVirtualScreen (drawable->context->context));
    drawable->context->pbuffer = 1;
  } else {
    if (drawable->context->pbuffer) {
      aglSetDrawable (drawable->context->context, NULL);
      drawable->context->pbuffer = 0;
    }
    
    aglSetDrawable (drawable->context->context, drawable->drawable);
  }

  aglSetCurrentContext (drawable->context->context);

  drawable->base.update_all = 1;
  
  if (!drawable->context->initialized)
    _glitz_agl_context_initialize (drawable->thread_info, drawable->context);
}

static void
_glitz_agl_context_update (glitz_agl_drawable_t *drawable,
                           glitz_constraint_t   constraint)
{
  AGLContext context;

  switch (constraint) {
  case GLITZ_NONE:
    break;
  case GLITZ_ANY_CONTEXT_CURRENT:
    context = aglGetCurrentContext ();
    if (context == (AGLContext) 0)
      _glitz_agl_context_make_current (drawable, 0);
    break;
  case GLITZ_CONTEXT_CURRENT:
    context = aglGetCurrentContext ();
    if (context != drawable->context->context)
      _glitz_agl_context_make_current (drawable, (context)? 1: 0);
    break;
  case GLITZ_DRAWABLE_CURRENT:
    context = aglGetCurrentContext ();
    if (context != drawable->context->context) {
      _glitz_agl_context_make_current (drawable, (context)? 1: 0);
    } else {
      if (drawable->pbuffer) {
        AGLPbuffer pbuffer;
        GLuint unused;

        aglGetPBuffer (drawable->context->context, &pbuffer,
                       &unused, &unused, &unused);
        
        if (pbuffer != drawable->pbuffer)
          _glitz_agl_context_make_current (drawable, (context)? 1: 0);
        
      } else if (drawable->drawable) {
        if (aglGetDrawable (drawable->context->context) != drawable->drawable)
          _glitz_agl_context_make_current (drawable, (context)? 1: 0);
      }
    }
    break;
  }
}

void
glitz_agl_push_current (void               *abstract_drawable,
                        glitz_surface_t    *surface,
                        glitz_constraint_t constraint)
{
  glitz_agl_drawable_t *drawable = (glitz_agl_drawable_t *) abstract_drawable;
  glitz_agl_context_info_t *context_info;
  int index;

  index = drawable->thread_info->context_stack_size++;

  context_info = &drawable->thread_info->context_stack[index];
  context_info->drawable = drawable;
  context_info->surface = surface;
  context_info->constraint = constraint;
  
  _glitz_agl_context_update (context_info->drawable, constraint);
}

glitz_surface_t *
glitz_agl_pop_current (void *abstract_drawable)
{
  glitz_agl_drawable_t *drawable = (glitz_agl_drawable_t *) abstract_drawable;
  glitz_agl_context_info_t *context_info = NULL;
  int index;

  drawable->thread_info->context_stack_size--;
  index = drawable->thread_info->context_stack_size - 1;

  context_info = &drawable->thread_info->context_stack[index];

  if (context_info->drawable)
    _glitz_agl_context_update (context_info->drawable,
                               context_info->constraint);
  
  if (context_info->constraint == GLITZ_DRAWABLE_CURRENT)
      return context_info->surface;
  
  return NULL;
}
