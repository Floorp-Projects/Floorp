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

#ifndef GLITZ_WGLINT_H_INCLUDED
#define GLITZ_WGLINT_H_INCLUDED

#include <windows.h>		/* To let it define MAX/MINSHORT first */
#undef MAXSHORT
#undef MINSHORT

#include "glitzint.h"

#include "glitz-wgl.h"

#include <GL/gl.h>

#ifdef USE_MESA
#define ChoosePixelFormat wglChoosePixelFormat
#define DescribePixelFormat wglDescribePixelFormat
#define GetPixelFormat wglGetPixelFormat
#define SetPixelFormat wglSetPixelFormat
#define SwapBuffers wglSwapBuffers
#endif

#include "glitz_wglext.h"

#define GLITZ_WGL_FEATURE_PIXEL_FORMAT_MASK        (1L << 0)
#define GLITZ_WGL_FEATURE_PBUFFER_MASK             (1L << 1)
#define GLITZ_WGL_FEATURE_MAKE_CURRENT_READ_MASK   (1L << 2)
/* wglGetProcAddress is always present */
#define GLITZ_WGL_FEATURE_MULTISAMPLE_MASK         (1L << 4)
#define GLITZ_WGL_FEATURE_PBUFFER_MULTISAMPLE_MASK (1L << 5)

typedef struct _glitz_wgl_drawable glitz_wgl_drawable_t;
typedef struct _glitz_wgl_screen_info_t glitz_wgl_screen_info_t;

typedef struct _glitz_wgl_static_proc_address_list_t {
  glitz_wgl_get_pixel_format_attrib_iv_t get_pixel_format_attrib_iv;
  glitz_wgl_choose_pixel_format_t        choose_pixel_format;
  glitz_wgl_create_pbuffer_t	         create_pbuffer;
  glitz_wgl_get_pbuffer_dc_t             get_pbuffer_dc;
  glitz_wgl_release_pbuffer_dc_t         release_pbuffer_dc;
  glitz_wgl_destroy_pbuffer_t            destroy_pbuffer;
  glitz_wgl_query_pbuffer_t              query_pbuffer;
  glitz_wgl_make_context_current_t       make_context_current;
} glitz_wgl_static_proc_address_list_t;

typedef struct _glitz_wgl_thread_info_t {
  glitz_wgl_screen_info_t *screen;
  char                    *gl_library;
  HMODULE                 dlhand;
  glitz_context_t         *cctx;
} glitz_wgl_thread_info_t;

typedef struct _glitz_wgl_context_info_t {
  glitz_wgl_drawable_t *drawable;
  glitz_surface_t      *surface;
  glitz_constraint_t   constraint;
} glitz_wgl_context_info_t;

typedef struct _glitz_wgl_context_t {
  glitz_context_t   base;

  HGLRC             context;
  glitz_format_id_t id;
  int               pixel_format;
  glitz_backend_t   backend;
  glitz_gl_int_t    max_viewport_dims[2];
  glitz_bool_t      initialized;
} glitz_wgl_context_t;

struct _glitz_wgl_screen_info_t {
  glitz_wgl_thread_info_t *thread_info;
  int                                  drawables;
  glitz_int_drawable_format_t          *formats;
  int                                  *format_ids;
  int                                  n_formats;
  glitz_wgl_context_t                  **contexts;
  int                                  n_contexts;
  glitz_wgl_context_info_t             context_stack[GLITZ_CONTEXT_STACK_SIZE];
  int                                  context_stack_size;
  /* root_context is used only for sharing a single display-list space
   * amont the Glitz rendering contexts. Nothing is ever actually
   * rendered into it. The related root_dc and root_window are kept
   * around only for proper destruction.
   */
  HGLRC                                root_context;
  HDC                                  root_dc;
  HWND				       root_window;
  unsigned long                        wgl_feature_mask;
  glitz_gl_float_t                     wgl_version;
  glitz_wgl_static_proc_address_list_t wgl;
  glitz_program_map_t                  program_map;
};

struct _glitz_wgl_drawable {
  glitz_drawable_t        base;

  glitz_wgl_screen_info_t *screen_info;
  glitz_wgl_context_t     *context;
  HWND                    window;
  HDC                     dc;
  HPBUFFERARB             pbuffer;
};

extern void __internal_linkage
glitz_wgl_query_extensions (glitz_wgl_screen_info_t *screen_info,
			    glitz_gl_float_t        wgl_version);

extern glitz_wgl_screen_info_t *__internal_linkage
glitz_wgl_screen_info_get (void);

extern glitz_function_pointer_t __internal_linkage
glitz_wgl_get_proc_address (const char *name,
			    void       *closure);

extern glitz_wgl_context_t *__internal_linkage
glitz_wgl_context_get (glitz_wgl_screen_info_t *screen_info,
		       HDC                     dc,
		       glitz_drawable_format_t *format);

extern void __internal_linkage
glitz_wgl_context_destroy (glitz_wgl_screen_info_t *screen_info,
			   glitz_wgl_context_t     *context);

extern void __internal_linkage
glitz_wgl_query_formats (glitz_wgl_screen_info_t *screen_info);

extern HPBUFFERARB __internal_linkage
glitz_wgl_pbuffer_create (glitz_wgl_screen_info_t    *screen_info,
			  int                        pixel_format,
			  unsigned int               width,
			  unsigned int               height,
			  HDC                        *dcp);

extern void __internal_linkage
glitz_wgl_pbuffer_destroy (glitz_wgl_screen_info_t *screen_info,
			   HPBUFFERARB             pbuffer,
			   HDC                     dc);

extern glitz_drawable_t *__internal_linkage
glitz_wgl_create_pbuffer (void                       *abstract_templ,
			  glitz_drawable_format_t    *format,
			  unsigned int               width,
			  unsigned int               height);

extern glitz_bool_t __internal_linkage
glitz_wgl_push_current (void                *abstract_drawable,
			glitz_surface_t     *surface,
			glitz_constraint_t  constraint,
			glitz_bool_t       *restore_state);

extern glitz_surface_t *__internal_linkage
glitz_wgl_pop_current (void *abstract_drawable);

extern glitz_status_t __internal_linkage
glitz_wgl_make_current_read (void *abstract_surface);

extern void __internal_linkage
glitz_wgl_destroy (void *abstract_drawable);

extern glitz_bool_t __internal_linkage
glitz_wgl_swap_buffers (void *abstract_drawable);

extern glitz_bool_t __internal_linkage
glitz_wgl_copy_sub_buffer (void *abstract_drawable,
			   int  x,
			   int  y,
			   int  width,
			   int  height);

extern void __internal_linkage
glitz_wgl_print_win32_error_string (int error_code);

extern void __internal_linkage
glitz_wgl_win32_api_failed (const char *api);

#endif /* GLITZ_WGLINT_H_INCLUDED */
