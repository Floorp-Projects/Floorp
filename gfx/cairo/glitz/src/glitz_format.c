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

struct _texture_format {
  glitz_gl_int_t texture_format;
  glitz_format_t format;
} _texture_formats[] = {
  { GLITZ_GL_ALPHA4,   { 0, GLITZ_FORMAT_TYPE_COLOR, {  0,  0,  0,  4 } } },
  { GLITZ_GL_ALPHA8,   { 0, GLITZ_FORMAT_TYPE_COLOR, {  0,  0,  0,  8 } } },
  { GLITZ_GL_ALPHA12,  { 0, GLITZ_FORMAT_TYPE_COLOR, {  0,  0,  0, 12 } } },
  { GLITZ_GL_ALPHA16,  { 0, GLITZ_FORMAT_TYPE_COLOR, {  0,  0,  0, 16 } } },
  { GLITZ_GL_R3_G3_B2, { 0, GLITZ_FORMAT_TYPE_COLOR, {  3,  3,  2,  0 } } },
  { GLITZ_GL_RGB4,     { 0, GLITZ_FORMAT_TYPE_COLOR, {  4,  4,  4,  0 } } },
  { GLITZ_GL_RGB5,     { 0, GLITZ_FORMAT_TYPE_COLOR, {  5,  6,  5,  0 } } },
  { GLITZ_GL_RGB8,     { 0, GLITZ_FORMAT_TYPE_COLOR, {  8,  8,  8,  0 } } },
  { GLITZ_GL_RGB10,    { 0, GLITZ_FORMAT_TYPE_COLOR, { 10, 10, 10,  0 } } },
  { GLITZ_GL_RGB12,    { 0, GLITZ_FORMAT_TYPE_COLOR, { 12, 12, 12,  0 } } },
  { GLITZ_GL_RGB16,    { 0, GLITZ_FORMAT_TYPE_COLOR, { 16, 16, 16,  0 } } },
  { GLITZ_GL_RGBA2,    { 0, GLITZ_FORMAT_TYPE_COLOR, {  2,  2,  2,  2 } } },
  { GLITZ_GL_RGB5_A1,  { 0, GLITZ_FORMAT_TYPE_COLOR, {  5,  5,  5,  1 } } },
  { GLITZ_GL_RGBA4,    { 0, GLITZ_FORMAT_TYPE_COLOR, {  4,  4,  4,  4 } } },
  { GLITZ_GL_RGBA8,    { 0, GLITZ_FORMAT_TYPE_COLOR, {  8,  8,  8,  8 } } },
  { GLITZ_GL_RGB10_A2, { 0, GLITZ_FORMAT_TYPE_COLOR, { 10, 10, 10,  2 } } },
  { GLITZ_GL_RGBA12,   { 0, GLITZ_FORMAT_TYPE_COLOR, { 12, 12, 12, 12 } } },
  { GLITZ_GL_RGBA16,   { 0, GLITZ_FORMAT_TYPE_COLOR, { 16, 16, 16, 16 } } }
};

static void
_glitz_add_texture_format (glitz_format_t **formats,
                           glitz_gl_int_t **texture_formats,
                           int            *n_formats,
                           glitz_gl_int_t texture_format,
                           glitz_format_t *format)
{
  *formats = realloc (*formats, sizeof (glitz_format_t) * (*n_formats + 1));
  *texture_formats = realloc (*texture_formats,
                              sizeof (glitz_gl_enum_t) * (*n_formats + 1));

  if (*formats && *texture_formats) {
    (*texture_formats)[*n_formats] = texture_format;
    (*formats)[*n_formats] = *format;
    (*formats)[*n_formats].id = *n_formats;
    (*n_formats)++;
  } else
    *n_formats = 0;
}

void
glitz_create_surface_formats (glitz_gl_proc_address_list_t *gl,
                              glitz_format_t               **formats,
                              glitz_gl_int_t               **texture_formats,
                              int                          *n_formats)
{
  glitz_gl_int_t value;
  int i, n_texture_formats;
  
  n_texture_formats =
    sizeof (_texture_formats) / sizeof (struct _texture_format);
  
  for (i = 0; i < n_texture_formats; i++) {
    gl->tex_image_2d (GLITZ_GL_PROXY_TEXTURE_2D, 0,
                      _texture_formats[i].texture_format, 1, 1, 0,
                      GLITZ_GL_RGBA, GLITZ_GL_UNSIGNED_BYTE, NULL);

    switch (_texture_formats[i].format.type) {
    case GLITZ_FORMAT_TYPE_COLOR:
      if (_texture_formats[i].format.color.red_size) {
        gl->get_tex_level_parameter_iv (GLITZ_GL_PROXY_TEXTURE_2D, 0,
                                        GLITZ_GL_TEXTURE_RED_SIZE, &value);
        if (value != _texture_formats[i].format.color.red_size)
          continue;
      }
      
      if (_texture_formats[i].format.color.green_size) {
        gl->get_tex_level_parameter_iv (GLITZ_GL_PROXY_TEXTURE_2D, 0,
                                        GLITZ_GL_TEXTURE_GREEN_SIZE, &value);
        if (value != _texture_formats[i].format.color.green_size)
          continue;
      }

      if (_texture_formats[i].format.color.blue_size) {
        gl->get_tex_level_parameter_iv (GLITZ_GL_PROXY_TEXTURE_2D, 0,
                                        GLITZ_GL_TEXTURE_BLUE_SIZE, &value);
        if (value != _texture_formats[i].format.color.blue_size)
          continue;
      }

      if (_texture_formats[i].format.color.alpha_size) {
        gl->get_tex_level_parameter_iv (GLITZ_GL_PROXY_TEXTURE_2D, 0,
                                        GLITZ_GL_TEXTURE_ALPHA_SIZE, &value);
        if (value != _texture_formats[i].format.color.alpha_size)
          continue;
      }
      break;
    default:
      continue;
    }
    
    _glitz_add_texture_format (formats,
                               texture_formats,
                               n_formats,
                               _texture_formats[i].texture_format,
                               &_texture_formats[i].format);
  }
}

glitz_drawable_format_t *
glitz_drawable_format_find (glitz_drawable_format_t       *formats,
                            int                           n_formats,
                            unsigned long                 mask,
                            const glitz_drawable_format_t *templ,
                            int                           count)
{
  for (; n_formats; n_formats--, formats++) {
    if (mask & GLITZ_FORMAT_ID_MASK)
      if (templ->id != formats->id)
        continue;

    if (mask & GLITZ_FORMAT_RED_SIZE_MASK)
      if (templ->color.red_size != formats->color.red_size)
        continue;

    if (mask & GLITZ_FORMAT_GREEN_SIZE_MASK)
      if (templ->color.green_size != formats->color.green_size)
        continue;

    if (mask & GLITZ_FORMAT_BLUE_SIZE_MASK)
      if (templ->color.blue_size != formats->color.blue_size)
        continue;

    if (mask & GLITZ_FORMAT_ALPHA_SIZE_MASK)
      if (templ->color.alpha_size != formats->color.alpha_size)
        continue;

    if (mask & GLITZ_FORMAT_DEPTH_SIZE_MASK)
      if (templ->depth_size != formats->depth_size)
        continue;

    if (mask & GLITZ_FORMAT_STENCIL_SIZE_MASK)
      if (templ->stencil_size != formats->stencil_size)
        continue;

    if (mask & GLITZ_FORMAT_DOUBLEBUFFER_MASK)
      if (templ->doublebuffer != formats->doublebuffer)
        continue;

    if (mask & GLITZ_FORMAT_SAMPLES_MASK)
      if (templ->samples != formats->samples)
        continue;

    if (mask & GLITZ_FORMAT_WINDOW_MASK)
      if (templ->types.window != formats->types.window)
        continue;

    if (mask & GLITZ_FORMAT_PBUFFER_MASK)
      if (templ->types.pbuffer != formats->types.pbuffer)
        continue;
    
    if (count-- == 0)
      return formats;    
  }

  return NULL;
}

static glitz_format_t *
_glitz_format_find (glitz_format_t       *formats,
                    int                  n_formats,
                    unsigned long        mask,
                    const glitz_format_t *templ,
                    int                  count)
{
  for (; n_formats; n_formats--, formats++) {
    if (mask & GLITZ_FORMAT_ID_MASK)
      if (templ->id != formats->id)
        continue;

    if (mask & GLITZ_FORMAT_TYPE_MASK)
      if (templ->type != formats->type)
        continue;

    if (mask & GLITZ_FORMAT_RED_SIZE_MASK)
      if (templ->color.red_size != formats->color.red_size)
        continue;

    if (mask & GLITZ_FORMAT_GREEN_SIZE_MASK)
      if (templ->color.green_size != formats->color.green_size)
        continue;

    if (mask & GLITZ_FORMAT_BLUE_SIZE_MASK)
      if (templ->color.blue_size != formats->color.blue_size)
        continue;

    if (mask & GLITZ_FORMAT_ALPHA_SIZE_MASK)
      if (templ->color.alpha_size != formats->color.alpha_size)
        continue;

    if (count-- == 0)
      return formats;    
  }

  return NULL;
}

glitz_format_t *
glitz_find_format (glitz_drawable_t     *drawable,
                   unsigned long        mask,
                   const glitz_format_t *templ,
                   int                  count)
{
  return _glitz_format_find (drawable->backend->formats,
                             drawable->backend->n_formats,
                             mask, templ, count);
}

glitz_format_t *
glitz_find_standard_format (glitz_drawable_t    *drawable,
                            glitz_format_name_t format_name)
{
  glitz_format_t templ;
  unsigned long mask = GLITZ_FORMAT_RED_SIZE_MASK |
    GLITZ_FORMAT_GREEN_SIZE_MASK | GLITZ_FORMAT_BLUE_SIZE_MASK |
    GLITZ_FORMAT_ALPHA_SIZE_MASK | GLITZ_FORMAT_TYPE_MASK;

  templ.type = GLITZ_FORMAT_TYPE_COLOR;

  switch (format_name) {
  case GLITZ_STANDARD_ARGB32:
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 8;
    break;
  case GLITZ_STANDARD_RGB24:
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 0;
    break;
  case GLITZ_STANDARD_A8:
    templ.color.red_size = 0;
    templ.color.green_size = 0;
    templ.color.blue_size = 0;
    templ.color.alpha_size = 8;
    break;
  case GLITZ_STANDARD_A1:
    templ.color.red_size = 0;
    templ.color.green_size = 0;
    templ.color.blue_size = 0;
    templ.color.alpha_size = 1;
    break;
  }

  return glitz_find_format (drawable, mask, &templ, 0);
}
