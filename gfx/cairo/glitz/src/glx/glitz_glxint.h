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

#ifndef GLITZ_GLXINT_H_INCLUDED
#define GLITZ_GLXINT_H_INCLUDED

#include "glitz.h"
#include "glitzint.h"

#include "glitz-glx.h"

#include <GL/gl.h>
#include <GL/glx.h>

#include "glitz_glxext.h"

#define GLITZ_GLX_FEATURE_VISUAL_RATING_MASK       (1L << 0)
#define GLITZ_GLX_FEATURE_FBCONFIG_MASK            (1L << 1)
#define GLITZ_GLX_FEATURE_PBUFFER_MASK             (1L << 2)
#define GLITZ_GLX_FEATURE_MAKE_CURRENT_READ_MASK   (1L << 3)
#define GLITZ_GLX_FEATURE_GET_PROC_ADDRESS_MASK    (1L << 4)
#define GLITZ_GLX_FEATURE_MULTISAMPLE_MASK         (1L << 5)
#define GLITZ_GLX_FEATURE_PBUFFER_MULTISAMPLE_MASK (1L << 6)

typedef struct _glitz_glx_drawable glitz_glx_drawable_t;
typedef struct _glitz_glx_screen_info_t glitz_glx_screen_info_t;
typedef struct _glitz_glx_display_info_t glitz_glx_display_info_t;

typedef struct _glitz_glx_static_proc_address_list_t {
    glitz_glx_get_proc_address_t         get_proc_address;
    glitz_glx_get_fbconfigs_t            get_fbconfigs;
    glitz_glx_get_fbconfig_attrib_t      get_fbconfig_attrib;
    glitz_glx_get_visual_from_fbconfig_t get_visual_from_fbconfig;
    glitz_glx_create_pbuffer_t           create_pbuffer;
    glitz_glx_destroy_pbuffer_t          destroy_pbuffer;
    glitz_glx_query_drawable_t           query_drawable;
    glitz_glx_make_context_current_t     make_context_current;
    glitz_glx_create_new_context_t       create_new_context;
} glitz_glx_static_proc_address_list_t;

typedef struct _glitz_glx_thread_info_t {
    glitz_glx_display_info_t **displays;
    int                      n_displays;
    char                     *gl_library;
    void                     *dlhand;
    glitz_context_t          *cctx;
} glitz_glx_thread_info_t;

struct _glitz_glx_display_info_t {
    glitz_glx_thread_info_t *thread_info;
    Display                 *display;
    glitz_glx_screen_info_t **screens;
    int n_screens;
};

typedef struct _glitz_glx_context_info_t {
    glitz_glx_drawable_t *drawable;
    glitz_surface_t      *surface;
    glitz_constraint_t   constraint;
} glitz_glx_context_info_t;

typedef struct _glitz_glx_context_t {
    glitz_context_t   base;
    GLXContext        context;
    glitz_format_id_t id;
    GLXFBConfig       fbconfig;
    glitz_backend_t   backend;
    glitz_bool_t      initialized;
} glitz_glx_context_t;

struct _glitz_glx_screen_info_t {
    glitz_glx_display_info_t             *display_info;
    int                                  screen;
    int                                  drawables;
    glitz_int_drawable_format_t          *formats;
    int                                  n_formats;
    glitz_glx_context_t                  **contexts;
    int                                  n_contexts;
    glitz_glx_context_info_t           context_stack[GLITZ_CONTEXT_STACK_SIZE];
    int                                  context_stack_size;
    GLXContext                           root_context;
    unsigned long                        glx_feature_mask;
    glitz_gl_float_t                     glx_version;
    glitz_glx_static_proc_address_list_t glx;
    glitz_program_map_t                  program_map;
};

struct _glitz_glx_drawable {
    glitz_drawable_t        base;

    glitz_glx_screen_info_t *screen_info;
    glitz_glx_context_t     *context;
    GLXDrawable             drawable;
    GLXDrawable             pbuffer;
    int                     width;
    int                     height;
};

extern void __internal_linkage
glitz_glx_query_extensions (glitz_glx_screen_info_t *screen_info,
			    glitz_gl_float_t        glx_version);

extern glitz_glx_screen_info_t __internal_linkage *
glitz_glx_screen_info_get (Display *display,
			   int     screen);

extern glitz_function_pointer_t __internal_linkage
glitz_glx_get_proc_address (const char *name,
			    void       *closure);

extern glitz_glx_context_t __internal_linkage *
glitz_glx_context_get (glitz_glx_screen_info_t *screen_info,
		       glitz_drawable_format_t *format);

extern void __internal_linkage
glitz_glx_context_destroy (glitz_glx_screen_info_t *screen_info,
			   glitz_glx_context_t     *context);

extern void __internal_linkage
glitz_glx_query_formats (glitz_glx_screen_info_t *screen_info);

extern glitz_bool_t __internal_linkage
_glitz_glx_drawable_update_size (glitz_glx_drawable_t *drawable,
				 int                  width,
				 int                  height);

extern GLXPbuffer __internal_linkage
glitz_glx_pbuffer_create (glitz_glx_screen_info_t    *screen_info,
			  GLXFBConfig                fbconfig,
			  int                        width,
			  int                        height);

extern void __internal_linkage
glitz_glx_pbuffer_destroy (glitz_glx_screen_info_t *screen_info,
			   GLXPbuffer              pbuffer);

extern glitz_drawable_t __internal_linkage *
glitz_glx_create_pbuffer (void                    *abstract_templ,
			  glitz_drawable_format_t *format,
			  unsigned int            width,
			  unsigned int            height);

extern glitz_bool_t __internal_linkage
glitz_glx_push_current (void               *abstract_drawable,
			glitz_surface_t    *surface,
			glitz_constraint_t constraint);

extern glitz_surface_t __internal_linkage *
glitz_glx_pop_current (void *abstract_drawable);

void
glitz_glx_make_current (void               *abstract_drawable,
			glitz_constraint_t constraint);

extern glitz_status_t __internal_linkage
glitz_glx_make_current_read (void *abstract_surface);

extern void __internal_linkage
glitz_glx_destroy (void *abstract_drawable);

extern glitz_bool_t __internal_linkage
glitz_glx_swap_buffers (void *abstract_drawable);

/* Avoid unnecessary PLT entries. */

slim_hidden_proto(glitz_glx_init)
slim_hidden_proto(glitz_glx_fini)
slim_hidden_proto(glitz_glx_find_window_format)
slim_hidden_proto(glitz_glx_find_pbuffer_format)
slim_hidden_proto(glitz_glx_find_drawable_format_for_visual)
slim_hidden_proto(glitz_glx_get_visual_info_from_format)
slim_hidden_proto(glitz_glx_create_drawable_for_window)
slim_hidden_proto(glitz_glx_create_pbuffer_drawable)

#endif /* GLITZ_GLXINT_H_INCLUDED */
