/*
 * Copyright © 2004 David Reveman, Peter Nilsson
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the names of
 * David Reveman and Peter Nilsson not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. David Reveman and Peter Nilsson
 * makes no representations about the suitability of this software for
 * any purpose. It is provided "as is" without express or implied warranty.
 *
 * DAVID REVEMAN AND PETER NILSSON DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL DAVID REVEMAN AND
 * PETER NILSSON BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: David Reveman <davidr@novell.com>
 *          Peter Nilsson <c99pnn@cs.umu.se>
 */

#ifndef GLITZ_GLXEXT_H_INCLUDED
#define GLITZ_GLXEXT_H_INCLUDED

#ifndef GLX_VERSION_1_3
#define GLX_WINDOW_BIT                     0x00000001
#define GLX_PIXMAP_BIT                     0x00000002
#define GLX_PBUFFER_BIT                    0x00000004
#define GLX_RGBA_BIT                       0x00000001
#define GLX_COLOR_INDEX_BIT                0x00000002
#define GLX_PBUFFER_CLOBBER_MASK           0x08000000
#define GLX_FRONT_LEFT_BUFFER_BIT          0x00000001
#define GLX_FRONT_RIGHT_BUFFER_BIT         0x00000002
#define GLX_BACK_LEFT_BUFFER_BIT           0x00000004
#define GLX_BACK_RIGHT_BUFFER_BIT          0x00000008
#define GLX_AUX_BUFFERS_BIT                0x00000010
#define GLX_DEPTH_BUFFER_BIT               0x00000020
#define GLX_STENCIL_BUFFER_BIT             0x00000040
#define GLX_ACCUM_BUFFER_BIT               0x00000080
#define GLX_CONFIG_CAVEAT                  0x20
#define GLX_X_VISUAL_TYPE                  0x22
#define GLX_TRANSPARENT_TYPE               0x23
#define GLX_TRANSPARENT_INDEX_VALUE        0x24
#define GLX_TRANSPARENT_RED_VALUE          0x25
#define GLX_TRANSPARENT_GREEN_VALUE        0x26
#define GLX_TRANSPARENT_BLUE_VALUE         0x27
#define GLX_TRANSPARENT_ALPHA_VALUE        0x28
#define GLX_DONT_CARE                      0xFFFFFFFF
#define GLX_NONE                           0x8000
#define GLX_SLOW_CONFIG                    0x8001
#define GLX_TRUE_COLOR                     0x8002
#define GLX_DIRECT_COLOR                   0x8003
#define GLX_PSEUDO_COLOR                   0x8004
#define GLX_STATIC_COLOR                   0x8005
#define GLX_GRAY_SCALE                     0x8006
#define GLX_STATIC_GRAY                    0x8007
#define GLX_TRANSPARENT_RGB                0x8008
#define GLX_TRANSPARENT_INDEX              0x8009
#define GLX_VISUAL_ID                      0x800B
#define GLX_SCREEN                         0x800C
#define GLX_NON_CONFORMANT_CONFIG          0x800D
#define GLX_DRAWABLE_TYPE                  0x8010
#define GLX_RENDER_TYPE                    0x8011
#define GLX_X_RENDERABLE                   0x8012
#define GLX_FBCONFIG_ID                    0x8013
#define GLX_RGBA_TYPE                      0x8014
#define GLX_COLOR_INDEX_TYPE               0x8015
#define GLX_MAX_PBUFFER_WIDTH              0x8016
#define GLX_MAX_PBUFFER_HEIGHT             0x8017
#define GLX_MAX_PBUFFER_PIXELS             0x8018
#define GLX_PRESERVED_CONTENTS             0x801B
#define GLX_LARGEST_PBUFFER                0x801C
#define GLX_WIDTH                          0x801D
#define GLX_HEIGHT                         0x801E
#define GLX_EVENT_MASK                     0x801F
#define GLX_DAMAGED                        0x8020
#define GLX_SAVED                          0x8021
#define GLX_WINDOW                         0x8022
#define GLX_PBUFFER                        0x8023
#define GLX_PBUFFER_HEIGHT                 0x8040
#define GLX_PBUFFER_WIDTH                  0x8041

typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef XID GLXFBConfigID;
typedef XID GLXContextID;
typedef XID GLXWindow;
typedef XID GLXPbuffer;

#endif

typedef glitz_function_pointer_t (* glitz_glx_get_proc_address_t)
     (const glitz_gl_ubyte_t *);
typedef GLXFBConfig *(* glitz_glx_get_fbconfigs_t)
     (Display *display, int screen, int *n_elements);
typedef int (* glitz_glx_get_fbconfig_attrib_t)
     (Display *display, GLXFBConfig config, int attribute, int *value);
typedef XVisualInfo *(* glitz_glx_get_visual_from_fbconfig_t)
     (Display *display, GLXFBConfig config);
typedef GLXPbuffer (* glitz_glx_create_pbuffer_t)
     (Display *display, GLXFBConfig config, const int *attrib_list);
typedef void (* glitz_glx_destroy_pbuffer_t)
     (Display *display, GLXPbuffer pbuffer);
typedef void (* glitz_glx_query_drawable_t)
     (Display *display, GLXDrawable drawable,
      int attribute, unsigned int *value);
typedef Bool (* glitz_glx_make_context_current_t)
     (Display *display, GLXDrawable draw, GLXDrawable read, GLXContext ctx);
typedef GLXContext (* glitz_glx_create_new_context_t)
     (Display *display, GLXFBConfig config, int render_type,
      GLXContext share_list, Bool direct);

#ifndef GLX_ARB_multisample
#define GLX_SAMPLE_BUFFERS_ARB              0x186a0
#define GLX_SAMPLES_ARB                     0x186a1
#endif

typedef Bool *(* glitz_glx_bind_tex_image_t)
     (Display *display, GLXPbuffer pbuffer, int buffer);
typedef Bool (* glitz_glx_release_tex_image_t)
     (Display *display, GLXPbuffer pbuffer, int buffer);

#endif /* GLITZ_GLXEXT_H_INCLUDED */
