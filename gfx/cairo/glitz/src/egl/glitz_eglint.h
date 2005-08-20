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

#ifndef GLITZ_EGLINT_H_INCLUDED
#define GLITZ_EGLINT_H_INCLUDED

#include "glitz.h"
#include "glitzint.h"

#include "glitz-egl.h"

#include "glitz_eglext.h"

#define GLITZ_EGL_FEATURE_MAKE_CURRENT_READ_MASK   (1L << 2)
#define GLITZ_EGL_FEATURE_GET_PROC_ADDRESS_MASK    (1L << 3)
#define GLITZ_EGL_FEATURE_MULTISAMPLE_MASK         (1L << 4)
#define GLITZ_EGL_FEATURE_PBUFFER_MULTISAMPLE_MASK (1L << 5)

typedef struct _glitz_egl_surface glitz_egl_surface_t;
typedef struct _glitz_egl_screen_info_t glitz_egl_screen_info_t;
typedef struct _glitz_egl_display_info_t glitz_egl_display_info_t;

typedef struct _glitz_egl_thread_info_t {
  glitz_egl_display_info_t **displays;
  int                      n_displays;
  char                     *gl_library;
  void                     *dlhand;
  glitz_context_t          *cctx;
} glitz_egl_thread_info_t;

struct _glitz_egl_display_info_t {
  glitz_egl_thread_info_t *thread_info;
  EGLDisplay              egl_display;
  glitz_egl_screen_info_t **screens;
  int n_screens;
};

typedef struct _glitz_egl_context_info_t {
  glitz_egl_surface_t  *drawable;
  glitz_surface_t      *surface;
  glitz_constraint_t   constraint;
} glitz_egl_context_info_t;

typedef struct _glitz_egl_context_t {
  glitz_context_t   base;
  EGLContext        egl_context;
  glitz_format_id_t id;
  EGLConfig         egl_config;
  glitz_backend_t   backend;
  glitz_gl_int_t    max_viewport_dims[2];
  glitz_gl_int_t    max_texture_2d_size;
  glitz_gl_int_t    max_texture_rect_size;
  glitz_bool_t      initialized;
} glitz_egl_context_t;

struct _glitz_egl_screen_info_t {
  glitz_egl_display_info_t             *display_info;
  int                                  screen;
  int                                  drawables;
  glitz_drawable_format_t              *formats;
  EGLConfig                            *egl_config_ids;
  int                                  n_formats;  
  glitz_egl_context_t                  **contexts;
  int                                  n_contexts;
  glitz_egl_context_info_t             context_stack[GLITZ_CONTEXT_STACK_SIZE];
  int                                  context_stack_size;
  EGLContext                           egl_root_context;
  unsigned long                        egl_feature_mask;
  glitz_gl_float_t                     egl_version;
  glitz_program_map_t                  program_map;
};

struct _glitz_egl_surface {
  glitz_drawable_t        base;
  
  glitz_egl_screen_info_t *screen_info;
  glitz_egl_context_t     *context;
  EGLSurface              egl_surface;
};

extern void __internal_linkage
glitz_egl_query_extensions (glitz_egl_screen_info_t *screen_info,
                            glitz_gl_float_t        egl_version);

extern glitz_egl_screen_info_t __internal_linkage *
glitz_egl_screen_info_get (EGLDisplay egl_display,
                           EGLScreenMESA  egl_screen);

extern glitz_function_pointer_t __internal_linkage
glitz_egl_get_proc_address (const char *name,
                            void       *closure);

extern glitz_egl_context_t __internal_linkage *
glitz_egl_context_get (glitz_egl_screen_info_t *screen_info,
                       glitz_drawable_format_t *format);

extern void __internal_linkage
glitz_egl_context_destroy (glitz_egl_screen_info_t *screen_info,
                           glitz_egl_context_t     *context);

extern void __internal_linkage
glitz_egl_query_configs (glitz_egl_screen_info_t *screen_info);

extern EGLSurface __internal_linkage
glitz_egl_pbuffer_create (glitz_egl_screen_info_t    *screen_info,
                          EGLConfig                  egl_config,
                          int                        width,
                          int                        height);

extern void __internal_linkage
glitz_egl_pbuffer_destroy (glitz_egl_screen_info_t *screen_info,
                           EGLSurface              egl_pbuffer);

extern glitz_drawable_t __internal_linkage *
glitz_egl_create_pbuffer (void                    *abstract_templ,
                          glitz_drawable_format_t *format,
                          unsigned int            width,
                          unsigned int            height);

extern void __internal_linkage
glitz_egl_push_current (void               *abstract_drawable,
                        glitz_surface_t    *surface,
                        glitz_constraint_t constraint);

extern glitz_surface_t __internal_linkage *
glitz_egl_pop_current (void *abstract_drawable);

void
glitz_egl_make_current (void               *abstract_drawable,
                        glitz_constraint_t constraint);

extern glitz_status_t __internal_linkage
glitz_egl_make_current_read (void *abstract_surface);

extern void __internal_linkage
glitz_egl_destroy (void *abstract_drawable);

extern void __internal_linkage
glitz_egl_swap_buffers (void *abstract_drawable);

/* Avoid unnecessary PLT entries.  */

slim_hidden_proto(glitz_egl_init)
slim_hidden_proto(glitz_egl_fini)
slim_hidden_proto(glitz_egl_find_config)
slim_hidden_proto(glitz_egl_create_surface)
slim_hidden_proto(glitz_egl_create_pbuffer_surface)

#endif /* GLITZ_EGLINT_H_INCLUDED */
