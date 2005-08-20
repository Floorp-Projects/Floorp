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

#include "glitzint.h"

#include <stdlib.h>
#include <string.h>

static glitz_extension_map gl_extensions[] = {
  { 0.0, "GL_ARB_texture_rectangle", GLITZ_FEATURE_TEXTURE_RECTANGLE_MASK },
  { 0.0, "GL_EXT_texture_rectangle", GLITZ_FEATURE_TEXTURE_RECTANGLE_MASK },
  { 0.0, "GL_NV_texture_rectangle", GLITZ_FEATURE_TEXTURE_RECTANGLE_MASK },
  { 0.0, "GL_ARB_texture_non_power_of_two",
    GLITZ_FEATURE_TEXTURE_NON_POWER_OF_TWO_MASK },
  { 0.0, "GL_ARB_texture_mirrored_repeat",
    GLITZ_FEATURE_TEXTURE_MIRRORED_REPEAT_MASK },
  { 0.0, "GL_ARB_texture_border_clamp",
    GLITZ_FEATURE_TEXTURE_BORDER_CLAMP_MASK },
  { 0.0, "GL_ARB_texture_env_combine",
    GLITZ_FEATURE_TEXTURE_ENV_COMBINE_MASK },
  { 0.0, "GL_EXT_texture_env_combine",
    GLITZ_FEATURE_TEXTURE_ENV_COMBINE_MASK },
  { 0.0, "GL_ARB_texture_env_dot3", GLITZ_FEATURE_TEXTURE_ENV_DOT3_MASK },
  { 0.0, "GL_ARB_multisample", GLITZ_FEATURE_MULTISAMPLE_MASK },
  { 0.0, "GL_NV_multisample_filter_hint",
    GLITZ_FEATURE_MULTISAMPLE_FILTER_HINT_MASK },
  { 0.0, "GL_ARB_multitexture", GLITZ_FEATURE_MULTITEXTURE_MASK },
  { 0.0, "GL_EXT_multi_draw_arrays", GLITZ_FEATURE_MULTI_DRAW_ARRAYS_MASK },
  { 0.0, "GL_ARB_fragment_program", GLITZ_FEATURE_FRAGMENT_PROGRAM_MASK },
  { 0.0, "GL_ARB_vertex_buffer_object",
    GLITZ_FEATURE_VERTEX_BUFFER_OBJECT_MASK },
  { 0.0, "GL_ARB_pixel_buffer_object",
    GLITZ_FEATURE_PIXEL_BUFFER_OBJECT_MASK },
  { 0.0, "GL_EXT_pixel_buffer_object",
    GLITZ_FEATURE_PIXEL_BUFFER_OBJECT_MASK },
  { 0.0, "GL_EXT_blend_color", GLITZ_FEATURE_BLEND_COLOR_MASK },
  { 0.0, "GL_ARB_imaging", GLITZ_FEATURE_BLEND_COLOR_MASK },
  { 0.0, "GL_APPLE_packed_pixels", GLITZ_FEATURE_PACKED_PIXELS_MASK },
  { 0.0, "GL_EXT_framebuffer_object", GLITZ_FEATURE_FRAMEBUFFER_OBJECT_MASK },
  { 0.0, NULL, 0 }
};

static glitz_bool_t
_glitz_extension_check (const char *extensions,
                        const char *ext_name)
{
  char *end;
  char *p = (char *) extensions;
  int ext_name_len = strlen (ext_name);

  if (! p)
    return 0;

  end = p + strlen (p);

  while (p < end) {
    int n = strcspn (p, " ");

    if ((ext_name_len == n) && (strncmp (ext_name, p, n) == 0)) {
      return 1;
    }
    p += (n + 1);
  }
  return 0;
}

unsigned long
glitz_extensions_query (glitz_gl_float_t    version,
                        const char          *extensions_string,
                        glitz_extension_map *extensions_map)
{
  unsigned long mask = 0;
  int i;

  for (i = 0; extensions_map[i].name; i++)
    if (((extensions_map[i].version > 1.0) &&
         (version >= extensions_map[i].version)) ||
        _glitz_extension_check (extensions_string, extensions_map[i].name))
      mask |= extensions_map[i].mask;

  return mask;
}

static glitz_status_t
_glitz_query_gl_extensions (glitz_gl_proc_address_list_t *gl,
                            glitz_gl_float_t             *gl_version,
                            unsigned long                *feature_mask)
{
  const char *gl_extensions_string;

  *gl_version = atof ((const char *) gl->get_string (GLITZ_GL_VERSION));
  if (*gl_version < 1.2f)
    return GLITZ_STATUS_NOT_SUPPORTED;
  
  gl_extensions_string = (const char *) gl->get_string (GLITZ_GL_EXTENSIONS);
  
  *feature_mask = glitz_extensions_query (*gl_version,
                                          gl_extensions_string,
                                          gl_extensions);
  
  if ((*feature_mask & GLITZ_FEATURE_TEXTURE_ENV_COMBINE_MASK) &&
      (*feature_mask & GLITZ_FEATURE_TEXTURE_ENV_DOT3_MASK)) {
    glitz_gl_int_t max_texture_units;
    
    gl->get_integer_v (GLITZ_GL_MAX_TEXTURE_UNITS, &max_texture_units);
    if (max_texture_units >= 3)
      *feature_mask |= GLITZ_FEATURE_PER_COMPONENT_RENDERING_MASK;
  }

  return GLITZ_STATUS_SUCCESS;
}

static void
_glitz_gl_proc_address_lookup (glitz_backend_t               *backend,
                               glitz_get_proc_address_proc_t get_proc_address,
                               void                          *closure)
{
  if (backend->feature_mask & GLITZ_FEATURE_BLEND_COLOR_MASK) {
    if (backend->gl_version >= 1.4f) {
      backend->gl.blend_color = (glitz_gl_blend_color_t)
        get_proc_address ("glBlendColor", closure);
    } else {
      backend->gl.blend_color = (glitz_gl_blend_color_t)
        get_proc_address ("glBlendColorEXT", closure);
    }

    if (!backend->gl.blend_color)
      backend->feature_mask &= ~GLITZ_FEATURE_BLEND_COLOR_MASK;
  }

  if (backend->feature_mask & GLITZ_FEATURE_MULTITEXTURE_MASK) {
    if (backend->gl_version >= 1.3f) {
      backend->gl.active_texture = (glitz_gl_active_texture_t)
        get_proc_address ("glActiveTexture", closure);
      backend->gl.client_active_texture = (glitz_gl_client_active_texture_t)
        get_proc_address ("glClientActiveTexture", closure);
    } else {
      backend->gl.active_texture = (glitz_gl_active_texture_t)
        get_proc_address ("glActiveTextureARB", closure);
      backend->gl.client_active_texture = (glitz_gl_client_active_texture_t)
        get_proc_address ("glClientActiveTextureARB", closure);
    }

    if ((!backend->gl.active_texture) ||
        (!backend->gl.client_active_texture)) {
      backend->feature_mask &= ~GLITZ_FEATURE_MULTITEXTURE_MASK;
      backend->feature_mask &= ~GLITZ_FEATURE_PER_COMPONENT_RENDERING_MASK;
    }
  }

  if (backend->feature_mask & GLITZ_FEATURE_MULTI_DRAW_ARRAYS_MASK) {
    backend->gl.multi_draw_arrays = (glitz_gl_multi_draw_arrays_t)
      get_proc_address ("glMultiDrawArraysEXT", closure);

    if (!backend->gl.multi_draw_arrays)
      backend->feature_mask &= ~GLITZ_FEATURE_MULTI_DRAW_ARRAYS_MASK;
  }

  if (backend->feature_mask & GLITZ_FEATURE_FRAGMENT_PROGRAM_MASK) {
    backend->gl.gen_programs = (glitz_gl_gen_programs_t)
      get_proc_address ("glGenProgramsARB", closure);
    backend->gl.delete_programs = (glitz_gl_delete_programs_t)
      get_proc_address ("glDeleteProgramsARB", closure);
    backend->gl.program_string = (glitz_gl_program_string_t)
      get_proc_address ("glProgramStringARB", closure);
    backend->gl.bind_program = (glitz_gl_bind_program_t)
      get_proc_address ("glBindProgramARB", closure);
    backend->gl.program_local_param_4fv = (glitz_gl_program_local_param_4fv_t)
      get_proc_address ("glProgramLocalParameter4fvARB", closure);
    backend->gl.get_program_iv = (glitz_gl_get_program_iv_t)
      get_proc_address ("glGetProgramivARB", closure);

    if ((!backend->gl.gen_programs) ||
        (!backend->gl.delete_programs) ||
        (!backend->gl.program_string) ||
        (!backend->gl.bind_program) ||
        (!backend->gl.program_local_param_4fv))
      backend->feature_mask &= ~GLITZ_FEATURE_FRAGMENT_PROGRAM_MASK;
  }

  if ((backend->feature_mask & GLITZ_FEATURE_VERTEX_BUFFER_OBJECT_MASK) ||
      (backend->feature_mask & GLITZ_FEATURE_PIXEL_BUFFER_OBJECT_MASK)) {
    if (backend->gl_version >= 1.5f) {
      backend->gl.gen_buffers = (glitz_gl_gen_buffers_t)
        get_proc_address ("glGenBuffers", closure);
      backend->gl.delete_buffers = (glitz_gl_delete_buffers_t)
        get_proc_address ("glDeleteBuffers", closure);
      backend->gl.bind_buffer = (glitz_gl_bind_buffer_t)
        get_proc_address ("glBindBuffer", closure);
      backend->gl.buffer_data = (glitz_gl_buffer_data_t)
        get_proc_address ("glBufferData", closure);
      backend->gl.buffer_sub_data = (glitz_gl_buffer_sub_data_t)
        get_proc_address ("glBufferSubData", closure);
      backend->gl.get_buffer_sub_data = (glitz_gl_get_buffer_sub_data_t)
        get_proc_address ("glGetBufferSubData", closure);
      backend->gl.map_buffer = (glitz_gl_map_buffer_t)
        get_proc_address ("glMapBuffer", closure);
      backend->gl.unmap_buffer = (glitz_gl_unmap_buffer_t)
        get_proc_address ("glUnmapBuffer", closure);
    } else {
      backend->gl.gen_buffers = (glitz_gl_gen_buffers_t)
        get_proc_address ("glGenBuffersARB", closure);
      backend->gl.delete_buffers = (glitz_gl_delete_buffers_t)
        get_proc_address ("glDeleteBuffersARB", closure);
      backend->gl.bind_buffer = (glitz_gl_bind_buffer_t)
        get_proc_address ("glBindBufferARB", closure);
      backend->gl.buffer_data = (glitz_gl_buffer_data_t)
        get_proc_address ("glBufferDataARB", closure);
      backend->gl.buffer_sub_data = (glitz_gl_buffer_sub_data_t)
        get_proc_address ("glBufferSubDataARB", closure);
      backend->gl.get_buffer_sub_data = (glitz_gl_get_buffer_sub_data_t)
        get_proc_address ("glGetBufferSubDataARB", closure);
      backend->gl.map_buffer = (glitz_gl_map_buffer_t)
        get_proc_address ("glMapBufferARB", closure);
      backend->gl.unmap_buffer = (glitz_gl_unmap_buffer_t)
        get_proc_address ("glUnmapBufferARB", closure);
    }

    if ((!backend->gl.gen_buffers) ||
        (!backend->gl.delete_buffers) ||
        (!backend->gl.bind_buffer) ||
        (!backend->gl.buffer_data) ||
        (!backend->gl.buffer_sub_data) ||
        (!backend->gl.get_buffer_sub_data) ||
        (!backend->gl.map_buffer) ||
        (!backend->gl.unmap_buffer)) {
      backend->feature_mask &= ~GLITZ_FEATURE_VERTEX_BUFFER_OBJECT_MASK;
      backend->feature_mask &= ~GLITZ_FEATURE_PIXEL_BUFFER_OBJECT_MASK;
    }
  }

  if (backend->feature_mask & GLITZ_FEATURE_FRAMEBUFFER_OBJECT_MASK) {
    backend->gl.gen_framebuffers = (glitz_gl_gen_framebuffers_t)
      get_proc_address ("glGenFramebuffersEXT", closure);
    backend->gl.delete_framebuffers = (glitz_gl_delete_framebuffers_t)
      get_proc_address ("glDeleteFramebuffersEXT", closure);
    backend->gl.bind_framebuffer = (glitz_gl_bind_framebuffer_t)
      get_proc_address ("glBindFramebufferEXT", closure);
    backend->gl.check_framebuffer_status =
      (glitz_gl_check_framebuffer_status_t)
      get_proc_address ("glCheckFramebufferStatusEXT", closure);
    backend->gl.framebuffer_texture_2d = (glitz_gl_framebuffer_texture_2d_t)
      get_proc_address ("glFramebufferTexture2DEXT", closure);
    
    if ((!backend->gl.gen_framebuffers) ||
        (!backend->gl.delete_framebuffers) ||
        (!backend->gl.bind_framebuffer) ||
        (!backend->gl.check_framebuffer_status) ||
        (!backend->gl.framebuffer_texture_2d))
      backend->feature_mask &= ~GLITZ_FEATURE_FRAMEBUFFER_OBJECT_MASK;
  }
}

void
glitz_backend_init (glitz_backend_t               *backend,
                    glitz_get_proc_address_proc_t get_proc_address,
                    void                          *closure)
{
  if (!_glitz_query_gl_extensions (&backend->gl,
                                   &backend->gl_version,
                                   &backend->feature_mask)) {
    _glitz_gl_proc_address_lookup (backend, get_proc_address, closure);
    glitz_create_surface_formats (&backend->gl,
                                  &backend->formats,
                                  &backend->texture_formats,
                                  &backend->n_formats);
  }
  
  backend->gl.get_integer_v (GLITZ_GL_MAX_TEXTURE_SIZE,
                             &backend->max_texture_2d_size);
  
  if (backend->feature_mask & GLITZ_FEATURE_TEXTURE_RECTANGLE_MASK)
    backend->gl.get_integer_v (GLITZ_GL_MAX_RECTANGLE_TEXTURE_SIZE,
                               &backend->max_texture_rect_size);
  else
    backend->max_texture_rect_size = 0;
}

unsigned int
glitz_uint_to_power_of_two (unsigned int x)
{
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  
  return (x + 1);
}

void
glitz_set_raster_pos (glitz_gl_proc_address_list_t *gl,
                      glitz_float_t                x,
                      glitz_float_t                y)
{
  gl->push_attrib (GLITZ_GL_TRANSFORM_BIT | GLITZ_GL_VIEWPORT_BIT);
  gl->matrix_mode (GLITZ_GL_PROJECTION);
  gl->push_matrix ();
  gl->load_identity ();
  gl->matrix_mode (GLITZ_GL_MODELVIEW);
  gl->push_matrix ();
  gl->load_identity ();
  gl->depth_range (0, 1);
  gl->viewport (-1, -1, 2, 2);
  
  gl->raster_pos_2f (0, 0);
  gl->bitmap (0, 0, 1, 1, x, y, NULL);
  
  gl->pop_matrix ();
  gl->matrix_mode (GLITZ_GL_PROJECTION);
  gl->pop_matrix ();
  gl->pop_attrib ();
}

void
glitz_clamp_value (glitz_float_t *value,
                   glitz_float_t min, glitz_float_t max)
{
  if (*value < min)
    *value = min;
  else if (*value > max)
    *value = max;
}

void
glitz_initiate_state (glitz_gl_proc_address_list_t *gl)
{
  gl->hint (GLITZ_GL_PERSPECTIVE_CORRECTION_HINT, GLITZ_GL_FASTEST);
  gl->disable (GLITZ_GL_CULL_FACE);
  gl->depth_mask (GLITZ_GL_FALSE);  
  gl->polygon_mode (GLITZ_GL_FRONT_AND_BACK, GLITZ_GL_FILL);
  gl->disable (GLITZ_GL_POLYGON_SMOOTH);
  gl->disable (GLITZ_GL_LINE_SMOOTH);
  gl->disable (GLITZ_GL_POINT_SMOOTH);
  gl->shade_model (GLITZ_GL_FLAT);
  gl->color_mask (GLITZ_GL_TRUE, GLITZ_GL_TRUE, GLITZ_GL_TRUE, GLITZ_GL_TRUE);
  gl->enable (GLITZ_GL_SCISSOR_TEST);
  gl->disable (GLITZ_GL_STENCIL_TEST);
  gl->enable_client_state (GLITZ_GL_VERTEX_ARRAY);
  gl->disable (GLITZ_GL_DEPTH_TEST); 
}
