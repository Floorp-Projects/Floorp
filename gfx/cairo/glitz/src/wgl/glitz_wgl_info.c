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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "glitz_wglint.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <malloc.h>
#include <assert.h>

void
glitz_wgl_print_win32_error_string (int error_code)
{
  char *msg = NULL;

  FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
		 FORMAT_MESSAGE_IGNORE_INSERTS |
		 FORMAT_MESSAGE_FROM_SYSTEM,
		 NULL, error_code, 0,
		 (LPTSTR) &msg, 0, NULL);

  if (msg != NULL)
    fprintf (stderr, "%s", msg);
  else
    fprintf (stderr, "Unknown error code %d", error_code);

  LocalFree (msg);
}

void
glitz_wgl_win32_api_failed (const char *api)
{
  fprintf (stderr, "%s failed: ", api);
  glitz_wgl_print_win32_error_string (GetLastError ());
  fprintf (stderr, "\n");
}

glitz_gl_proc_address_list_t _glitz_wgl_gl_proc_address = {

  /* core */
  (glitz_gl_enable_t) glEnable,
  (glitz_gl_disable_t) glDisable,
  (glitz_gl_get_error_t) glGetError,
  (glitz_gl_get_string_t) glGetString,
  (glitz_gl_enable_client_state_t) glEnableClientState,
  (glitz_gl_disable_client_state_t) glDisableClientState,
  (glitz_gl_vertex_pointer_t) glVertexPointer,
  (glitz_gl_tex_coord_pointer_t) glTexCoordPointer,
  (glitz_gl_draw_arrays_t) glDrawArrays,
  (glitz_gl_tex_env_f_t) glTexEnvf,
  (glitz_gl_tex_env_fv_t) glTexEnvfv,
  (glitz_gl_tex_gen_i_t) glTexGeni,
  (glitz_gl_tex_gen_fv_t) glTexGenfv,
  (glitz_gl_color_4us_t) glColor4us,
  (glitz_gl_color_4f_t) glColor4f,
  (glitz_gl_scissor_t) glScissor,
  (glitz_gl_blend_func_t) glBlendFunc,
  (glitz_gl_clear_t) glClear,
  (glitz_gl_clear_color_t) glClearColor,
  (glitz_gl_clear_stencil_t) glClearStencil,
  (glitz_gl_stencil_func_t) glStencilFunc,
  (glitz_gl_stencil_op_t) glStencilOp,
  (glitz_gl_push_attrib_t) glPushAttrib,
  (glitz_gl_pop_attrib_t) glPopAttrib,
  (glitz_gl_matrix_mode_t) glMatrixMode,
  (glitz_gl_push_matrix_t) glPushMatrix,
  (glitz_gl_pop_matrix_t) glPopMatrix,
  (glitz_gl_load_identity_t) glLoadIdentity,
  (glitz_gl_load_matrix_f_t) glLoadMatrixf,
  (glitz_gl_depth_range_t) glDepthRange,
  (glitz_gl_viewport_t) glViewport,
  (glitz_gl_raster_pos_2f_t) glRasterPos2f,
  (glitz_gl_bitmap_t) glBitmap,
  (glitz_gl_read_buffer_t) glReadBuffer,
  (glitz_gl_draw_buffer_t) glDrawBuffer,
  (glitz_gl_copy_pixels_t) glCopyPixels,
  (glitz_gl_flush_t) glFlush,
  (glitz_gl_finish_t) glFinish,
  (glitz_gl_pixel_store_i_t) glPixelStorei,
  (glitz_gl_ortho_t) glOrtho,
  (glitz_gl_scale_f_t) glScalef,
  (glitz_gl_translate_f_t) glTranslatef,
  (glitz_gl_hint_t) glHint,
  (glitz_gl_depth_mask_t) glDepthMask,
  (glitz_gl_polygon_mode_t) glPolygonMode,
  (glitz_gl_shade_model_t) glShadeModel,
  (glitz_gl_color_mask_t) glColorMask,
  (glitz_gl_read_pixels_t) glReadPixels,
  (glitz_gl_get_tex_image_t) glGetTexImage,
  (glitz_gl_tex_sub_image_2d_t) glTexSubImage2D,
  (glitz_gl_gen_textures_t) glGenTextures,
  (glitz_gl_delete_textures_t) glDeleteTextures,
  (glitz_gl_bind_texture_t) glBindTexture,
  (glitz_gl_tex_image_2d_t) glTexImage2D,
  (glitz_gl_tex_parameter_i_t) glTexParameteri,
  (glitz_gl_get_tex_level_parameter_iv_t) glGetTexLevelParameteriv,
  (glitz_gl_copy_tex_sub_image_2d_t) glCopyTexSubImage2D,
  (glitz_gl_get_integer_v_t) glGetIntegerv,

  /* extensions */
  (glitz_gl_blend_color_t) 0,
  (glitz_gl_active_texture_t) 0,
  (glitz_gl_client_active_texture_t) 0,
  (glitz_gl_multi_draw_arrays_t) 0,
  (glitz_gl_gen_programs_t) 0,
  (glitz_gl_delete_programs_t) 0,
  (glitz_gl_program_string_t) 0,
  (glitz_gl_bind_program_t) 0,
  (glitz_gl_program_local_param_4fv_t) 0,
  (glitz_gl_get_program_iv_t) 0,
  (glitz_gl_gen_buffers_t) 0,
  (glitz_gl_delete_buffers_t) 0,
  (glitz_gl_bind_buffer_t) 0,
  (glitz_gl_buffer_data_t) 0,
  (glitz_gl_buffer_sub_data_t) 0,
  (glitz_gl_get_buffer_sub_data_t) 0,
  (glitz_gl_map_buffer_t) 0,
  (glitz_gl_unmap_buffer_t) 0
};

glitz_function_pointer_t
glitz_wgl_get_proc_address (const char *name,
			    void       *closure)
{
  glitz_wgl_screen_info_t *screen_info = (glitz_wgl_screen_info_t *) closure;
  glitz_wgl_thread_info_t *info = screen_info->thread_info;
  glitz_function_pointer_t address = (glitz_function_pointer_t) wglGetProcAddress (name);

  if (!address && info->gl_library) {
    /* Not sure whether this has any use at all... */

    if (!info->dlhand)
      info->dlhand = LoadLibrary (info->gl_library);

    if (info->dlhand) {
      address = (glitz_function_pointer_t) GetProcAddress (info->dlhand, name);
    }
  }

  return address;
}

static void
_glitz_wgl_proc_address_lookup (glitz_wgl_screen_info_t *screen_info)
{
  screen_info->wgl.get_pixel_format_attrib_iv =
    (glitz_wgl_get_pixel_format_attrib_iv_t)
    glitz_wgl_get_proc_address ("wglGetPixelFormatAttribivARB", screen_info);
  screen_info->wgl.choose_pixel_format = (glitz_wgl_choose_pixel_format_t)
    glitz_wgl_get_proc_address ("wglChoosePixelFormatARB", screen_info);

  screen_info->wgl.create_pbuffer = (glitz_wgl_create_pbuffer_t)
    glitz_wgl_get_proc_address ("wglCreatePbufferARB", screen_info);
  screen_info->wgl.get_pbuffer_dc = (glitz_wgl_get_pbuffer_dc_t)
    glitz_wgl_get_proc_address ("wglGetPbufferDCARB", screen_info);
  screen_info->wgl.release_pbuffer_dc = (glitz_wgl_release_pbuffer_dc_t)
    glitz_wgl_get_proc_address ("wglReleasePbufferDCARB", screen_info);
  screen_info->wgl.destroy_pbuffer = (glitz_wgl_destroy_pbuffer_t)
    glitz_wgl_get_proc_address ("wglDestroyPbufferARB", screen_info);
  screen_info->wgl.query_pbuffer = (glitz_wgl_query_pbuffer_t)
    glitz_wgl_get_proc_address ("wglQueryPbufferARB", screen_info);

  screen_info->wgl.make_context_current = (glitz_wgl_make_context_current_t)
    glitz_wgl_get_proc_address ("wglMakeContextCurrentARB", screen_info);

  if ((!screen_info->wgl.create_pbuffer) ||
      (!screen_info->wgl.destroy_pbuffer) ||
      (!screen_info->wgl.query_pbuffer))
    screen_info->wgl_feature_mask &= ~GLITZ_WGL_FEATURE_PBUFFER_MASK;

  
}

static void
_glitz_wgl_screen_destroy (glitz_wgl_screen_info_t *screen_info);

static void
_glitz_wgl_thread_info_init (glitz_wgl_thread_info_t *thread_info)
{
  thread_info->screen = NULL;
  thread_info->gl_library = NULL;
  thread_info->dlhand = NULL;
}

static void
_glitz_wgl_thread_info_fini (glitz_wgl_thread_info_t *thread_info)
{
  _glitz_wgl_screen_destroy (thread_info->screen);

  thread_info->screen = NULL;

  if (thread_info->gl_library) {
    free (thread_info->gl_library);
    thread_info->gl_library = NULL;
  }

  if (thread_info->dlhand) {
    FreeLibrary (thread_info->dlhand);
    thread_info->dlhand = NULL;
  }
}

static int tsd_initialized = 0;
static DWORD info_tsd;

static void
_glitz_wgl_thread_info_destroy (glitz_wgl_thread_info_t *thread_info)
{
  TlsSetValue (info_tsd, NULL);
  
  if (thread_info) {
    _glitz_wgl_thread_info_fini (thread_info);
    free (thread_info);
  }
}

static glitz_wgl_thread_info_t *
_glitz_wgl_thread_info_get_inner (void)
{
  glitz_wgl_thread_info_t *thread_info;
  void *p;
    
  if (!tsd_initialized) {
    info_tsd = TlsAlloc ();
    tsd_initialized = 1;
  }

  p = TlsGetValue (info_tsd);
  
  if (p == NULL) {
    thread_info = malloc (sizeof (glitz_wgl_thread_info_t));
    _glitz_wgl_thread_info_init (thread_info);
    
    TlsSetValue (info_tsd, thread_info);
  } else
    thread_info = (glitz_wgl_thread_info_t *) p;
  
  return thread_info;
}

static glitz_wgl_thread_info_t *
_glitz_wgl_thread_info_get (const char *gl_library)
{
  glitz_wgl_thread_info_t *thread_info = _glitz_wgl_thread_info_get_inner ();

  if (gl_library) {
    int len = strlen (gl_library);
      
    if (thread_info->gl_library) {
      free (thread_info->gl_library);
      thread_info->gl_library = NULL;
    }
      
    thread_info->gl_library = malloc (len + 1);
    if (thread_info->gl_library) {
      memcpy (thread_info->gl_library, gl_library, len);
      thread_info->gl_library[len] = '\0';
    }
  }

  return thread_info;
}

int
glitz_wgl_thread_starter (const glitz_wgl_thread_starter_arg_t *arg)
{
  glitz_wgl_thread_info_t *thread_info;
  int retval;

  retval = (*arg->user_thread_function) (arg->user_thread_function_arg);

  if (tsd_initialized) {
    thread_info = (glitz_wgl_thread_info_t *) TlsGetValue (info_tsd);
    _glitz_wgl_thread_info_fini (thread_info);
    free (thread_info);
  }

  return retval;
}

static void
_glitz_wgl_create_root_context (glitz_wgl_screen_info_t *screen_info)
{
  WNDCLASSEX wcl;
  ATOM klass;

  static PIXELFORMATDESCRIPTOR pfd = {
    sizeof (PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL,
    PFD_TYPE_RGBA,
    32,
    0, 0, 0,
    0, 0, 0,
    0, 0,
    0, 0, 0, 0, 0,
    0,
    0,
    0,
    PFD_MAIN_PLANE,
    0,
    0,
    0,
    0
  };

  int pixel_format;

  wcl.cbSize = sizeof (wcl);
  wcl.style = 0;
  wcl.lpfnWndProc = DefWindowProc;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = GetModuleHandle (NULL);
  wcl.hIcon = NULL;
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW);
  wcl.hbrBackground = NULL;
  wcl.lpszMenuName = NULL;
  wcl.lpszClassName = "glitz-wgl-root-window-class";
  wcl.hIconSm = NULL;

  klass = RegisterClassEx (&wcl);

  if (!klass) {
    exit (1);
  }

  screen_info->root_window =
    CreateWindowEx (0, (LPCTSTR) (DWORD) klass, "glitz-wgl-root-window",
		    WS_OVERLAPPEDWINDOW,
		    CW_USEDEFAULT, CW_USEDEFAULT,
		    100, 100,
		    GetDesktopWindow (),
		    NULL, GetModuleHandle (NULL), NULL);

  screen_info->root_dc = GetDC (screen_info->root_window);

  pixel_format = ChoosePixelFormat (screen_info->root_dc, &pfd);

  if (pixel_format != 0) {
    SetPixelFormat (screen_info->root_dc, pixel_format, &pfd);

    screen_info->root_context = wglCreateContext (screen_info->root_dc);

    wglMakeCurrent (screen_info->root_dc, screen_info->root_context);
  } else {
    screen_info->root_context = NULL;
  }
}

glitz_wgl_screen_info_t *
glitz_wgl_screen_info_get (void)
{
  static glitz_wgl_screen_info_t *screen_info = NULL;
  const char *gl_version;

  if (screen_info != NULL)
    return screen_info;

  screen_info = malloc (sizeof (glitz_wgl_screen_info_t));

  screen_info->drawables = 0;
  screen_info->formats = NULL;
  screen_info->format_ids = NULL;
  screen_info->n_formats = 0;

  screen_info->contexts = NULL;
  screen_info->n_contexts = 0;

  memset (&screen_info->wgl, 0, sizeof (glitz_wgl_static_proc_address_list_t));

  glitz_program_map_init (&screen_info->program_map);

  _glitz_wgl_create_root_context (screen_info);

  gl_version = glGetString (GL_VERSION);

  screen_info->wgl_version = 1;

  /* Can't use atof(), as GL_VERSION might always use a decimal period,
   * but atof() looks for the locale-specific decimal separator.
   */
  if (gl_version != NULL && gl_version[0]) {
    int whole_part;
    int n;
    if (sscanf (gl_version, "%d%n", &whole_part, &n) == 1 &&
	(gl_version[n] == '.' ||
	 gl_version[n] == ',')) {
      double fraction = 0.1;

      screen_info->wgl_version = whole_part;
      while (isdigit (gl_version[n+1])) {
	screen_info->wgl_version += (gl_version[n+1] - '0') * fraction;
	fraction /= 10;
	n++;
      }
    }
  }

  screen_info->wgl_feature_mask = 0;

  screen_info->thread_info = _glitz_wgl_thread_info_get_inner ();
  screen_info->thread_info->screen = screen_info;
  _glitz_wgl_thread_info_get (NULL);

  glitz_wgl_query_extensions (screen_info, screen_info->wgl_version);
  _glitz_wgl_proc_address_lookup (screen_info);
  glitz_wgl_query_formats (screen_info);
  
  screen_info->context_stack_size = 1;
  screen_info->context_stack->drawable = NULL;
  screen_info->context_stack->surface = NULL;
  screen_info->context_stack->constraint = GLITZ_NONE;
  
  return screen_info;
}

static void
_glitz_wgl_screen_destroy (glitz_wgl_screen_info_t *screen_info)
{
  int i;

  if (screen_info->root_context) {
    wglMakeCurrent (NULL, NULL);
  }

  for (i = 0; i < screen_info->n_contexts; i++)
    glitz_wgl_context_destroy (screen_info, screen_info->contexts[i]);

  if (screen_info->contexts)
    free (screen_info->contexts);
  
  if (screen_info->formats)
    free (screen_info->formats);

  if (screen_info->format_ids)
    free (screen_info->format_ids);
  
  if (screen_info->root_context) {
    wglDeleteContext (screen_info->root_context);
  }
  
  if (screen_info->root_dc) {
    DeleteDC (screen_info->root_dc);
  }

  if (screen_info->root_window) {
    DestroyWindow (screen_info->root_window);
  }

  free (screen_info);
}

void
glitz_wgl_init (const char *gl_library)
{
  _glitz_wgl_thread_info_get (gl_library);
}

void
glitz_wgl_fini (void)
{
  glitz_wgl_thread_info_t *info = _glitz_wgl_thread_info_get (NULL);

  _glitz_wgl_thread_info_destroy (info);
}
