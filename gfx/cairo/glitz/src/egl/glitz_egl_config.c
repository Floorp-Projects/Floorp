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
#include <string.h>

static int
_glitz_egl_format_compare (const void *elem1,
                           const void *elem2)
{
  int i, score[2];
  glitz_drawable_format_t *format[2];
  
  format[0] = (glitz_drawable_format_t *) elem1;
  format[1] = (glitz_drawable_format_t *) elem2;
  i = score[0] = score[1] = 0;

  for (; i < 2; i++) {
    if (format[i]->color.red_size) {
      if (format[i]->color.red_size == 8)
        score[i] += 5;
      score[i] += 10;
    }

    if (format[i]->color.green_size) {
      if (format[i]->color.green_size == 8)
        score[i] += 5;
      score[i] += 10;
    }
    
    if (format[i]->color.alpha_size) {
      if (format[i]->color.alpha_size == 8)
        score[i] += 5;
      score[i] += 10;
    }

    if (format[i]->stencil_size)
      score[i] += 5;

    if (format[i]->depth_size)
      score[i] += 5;

    if (format[i]->doublebuffer)
      score[i] += 10;
    
    if (format[i]->types.window)
      score[i] += 10;
    
    if (format[i]->types.pbuffer)
      score[i] += 10;
    
    if (format[i]->samples > 1) 
      score[i] -= (20 - format[i]->samples);
  }
  
  return score[1] - score[0];
}

static void
_glitz_add_format (glitz_egl_screen_info_t *screen_info,
                   glitz_drawable_format_t *format,
                   EGLConfig               egl_id)
{
  if (!glitz_drawable_format_find (screen_info->formats,
                                   screen_info->n_formats,
                                   GLITZ_DRAWABLE_FORMAT_ALL_EXCEPT_ID_MASK,
                                   format, 0)) {
    int n = screen_info->n_formats;
    
    screen_info->formats =
      realloc (screen_info->formats,
               sizeof (glitz_drawable_format_t) * (n + 1));
    screen_info->egl_config_ids =
      realloc (screen_info->egl_config_ids, sizeof (EGLConfig) * (n + 1));

    if (screen_info->formats && screen_info->egl_config_ids) {
      screen_info->formats[n] = *format;
      screen_info->formats[n].id = n;
      screen_info->egl_config_ids[n] = egl_id;
      screen_info->n_formats++;
    }
  }
}

static glitz_status_t
_glitz_egl_query_configs (glitz_egl_screen_info_t *screen_info)
{
  EGLDisplay egl_display;
  glitz_drawable_format_t format;
  EGLConfig *egl_configs;
  int i, num_configs;
  EGLConfig egl_id;
  
  egl_display = screen_info->display_info->egl_display;

  eglGetConfigs(egl_display, NULL, 0, &num_configs);
  egl_configs = malloc(sizeof(*egl_configs) * num_configs);
  eglGetConfigs(egl_display, egl_configs, num_configs, &num_configs);

  for (i = 0; i < num_configs; i++) {
    int value;
    
    eglGetConfigAttrib(egl_display, egl_configs[i],
                              EGL_SURFACE_TYPE, &value);
    if (!((value & EGL_WINDOW_BIT) || (value & EGL_PBUFFER_BIT)))
      continue;
    
    format.types.window = (value & EGL_WINDOW_BIT)? 1: 0;
    format.types.pbuffer = (value & EGL_PBUFFER_BIT)? 1: 0;
    format.id = 0;
    
    eglGetConfigAttrib(egl_display, egl_configs[i], EGL_CONFIG_ID, &value);
    egl_id = (EGLConfig) value;

    eglGetConfigAttrib(egl_display, egl_configs[i], EGL_RED_SIZE, &value);
    format.color.red_size = (unsigned short) value;
    eglGetConfigAttrib(egl_display, egl_configs[i], EGL_GREEN_SIZE, &value);
    format.color.green_size = (unsigned short) value;
    eglGetConfigAttrib(egl_display, egl_configs[i], EGL_BLUE_SIZE, &value);
    format.color.blue_size = (unsigned short) value;
    eglGetConfigAttrib(egl_display, egl_configs[i], EGL_ALPHA_SIZE, &value);
    format.color.alpha_size = (unsigned short) value;
    eglGetConfigAttrib(egl_display, egl_configs[i], EGL_DEPTH_SIZE, &value);
    format.depth_size = (unsigned short) value;
    eglGetConfigAttrib(egl_display, egl_configs[i], EGL_STENCIL_SIZE, &value);
    format.stencil_size = (unsigned short) value;

    format.doublebuffer = 1;

    eglGetConfigAttrib(egl_display, egl_configs[i], EGL_SAMPLE_BUFFERS, &value);
    if (value) {
      eglGetConfigAttrib(egl_display, egl_configs[i], EGL_SAMPLES, &value);
      format.samples = (unsigned short) (value > 1)? value: 1;
      if (format.samples > 1)
          format.types.pbuffer = 0;
    } else
      format.samples = 1;
    
    _glitz_add_format (screen_info, &format, egl_id);
  }
  
  free(egl_configs);

  return GLITZ_STATUS_SUCCESS;
}

void
glitz_egl_query_configs (glitz_egl_screen_info_t *screen_info)
{
  EGLConfig *egl_new_ids;
  int i;

  _glitz_egl_query_configs (screen_info);

  if (!screen_info->n_formats)
    return;

  qsort (screen_info->formats, screen_info->n_formats, 
         sizeof (glitz_drawable_format_t), _glitz_egl_format_compare);

  /*
   * Update XID list so that it matches the sorted format list.
   */
  egl_new_ids = malloc (sizeof (EGLConfig) * screen_info->n_formats);
  if (!egl_new_ids) {
    screen_info->n_formats = 0;
    return;
  }
  
  for (i = 0; i < screen_info->n_formats; i++) {
    egl_new_ids[i] = screen_info->egl_config_ids[screen_info->formats[i].id];
    screen_info->formats[i].id = i;
  }
  
  free (screen_info->egl_config_ids);
  screen_info->egl_config_ids = egl_new_ids;
}

glitz_drawable_format_t *
glitz_egl_find_config (EGLDisplay                    egl_display,
                       EGLScreenMESA                 egl_screen,
                       unsigned long                 mask,
                       const glitz_drawable_format_t *templ,
                       int                           count)
{
  glitz_egl_screen_info_t *screen_info =
    glitz_egl_screen_info_get (egl_display, egl_screen);

  return glitz_drawable_format_find (screen_info->formats,
                                     screen_info->n_formats,
                                     mask, templ, count);
}
slim_hidden_def(glitz_egl_find_config);
