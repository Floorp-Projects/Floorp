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

#include "glitz_glxint.h"

#include <stdlib.h>
#include <string.h>

static int
_glitz_glx_format_compare (const void *elem1,
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
_glitz_add_format (glitz_glx_screen_info_t *screen_info,
                   glitz_drawable_format_t *format,
                   XID                     id)
{
    int n = screen_info->n_formats;
    
    screen_info->formats =
      realloc (screen_info->formats,
               sizeof (glitz_drawable_format_t) * (n + 1));
    screen_info->format_ids =
      realloc (screen_info->format_ids, sizeof (XID) * (n + 1));

    if (screen_info->formats && screen_info->format_ids) {
      screen_info->formats[n] = *format;
      screen_info->formats[n].id = n;
      screen_info->format_ids[n] = id;
      screen_info->n_formats++;
    }
}

static void
_glitz_glx_query_formats (glitz_glx_screen_info_t *screen_info)
{
  Display *display;
  glitz_drawable_format_t format;
  XVisualInfo visual_templ;
  XVisualInfo *visuals;
  int i, num_visuals;
  
  display = screen_info->display_info->display;

  visual_templ.screen = screen_info->screen;
  visuals = XGetVisualInfo (display, VisualScreenMask,
                            &visual_templ, &num_visuals);

  /* No pbuffers without fbconfigs */
  format.types.window = 1;
  format.types.pbuffer = 0;
  format.id = 0;

  for (i = 0; i < num_visuals; i++) {
    int value;
    
    if ((glXGetConfig (display, &visuals[i], GLX_USE_GL, &value) != 0) ||
        (value == 0))
      continue;
    
    glXGetConfig (display, &visuals[i], GLX_RGBA, &value);
    if (value == 0)
      continue;

    /* Stereo is not supported yet */
    glXGetConfig (display, &visuals[i], GLX_STEREO, &value);
    if (value != 0)
      continue;
    
    glXGetConfig (display, &visuals[i], GLX_RED_SIZE, &value);
    format.color.red_size = (unsigned short) value;
    glXGetConfig (display, &visuals[i], GLX_GREEN_SIZE, &value);
    format.color.green_size = (unsigned short) value;
    glXGetConfig (display, &visuals[i], GLX_BLUE_SIZE, &value);
    format.color.blue_size = (unsigned short) value;
    glXGetConfig (display, &visuals[i], GLX_ALPHA_SIZE, &value);
    format.color.alpha_size = (unsigned short) value;
    glXGetConfig (display, &visuals[i], GLX_DEPTH_SIZE, &value);
    format.depth_size = (unsigned short) value;
    glXGetConfig (display, &visuals[i], GLX_STENCIL_SIZE, &value);
    format.stencil_size = (unsigned short) value;
    glXGetConfig (display, &visuals[i], GLX_DOUBLEBUFFER, &value);
    format.doublebuffer = (value) ? 1: 0;

    if (screen_info->glx_feature_mask & GLITZ_GLX_FEATURE_MULTISAMPLE_MASK) {
      glXGetConfig (display, &visuals[i], GLX_SAMPLE_BUFFERS_ARB, &value);
      if (value) {
        glXGetConfig (display, &visuals[i], GLX_SAMPLES_ARB, &value);
        format.samples = (unsigned short) (value > 1)? value: 1;
      } else
        format.samples = 1;
    } else
      format.samples = 1;
    
    _glitz_add_format (screen_info, &format, visuals[i].visualid);
  }
  
  if (visuals)
    XFree (visuals);
}

static glitz_status_t
_glitz_glx_query_formats_using_fbconfigs (glitz_glx_screen_info_t *screen_info)
{
  glitz_glx_static_proc_address_list_t *glx = &screen_info->glx;
  Display *display;
  glitz_drawable_format_t format;
  GLXFBConfig *fbconfigs;
  int i, num_configs;
  XID id;
  
  display = screen_info->display_info->display;

  fbconfigs = glx->get_fbconfigs (display, screen_info->screen, &num_configs);
  if (!fbconfigs) {
    /* fbconfigs are not supported, falling back to visuals */
    screen_info->glx_feature_mask &= ~GLITZ_GLX_FEATURE_FBCONFIG_MASK;
    screen_info->glx_feature_mask &= ~GLITZ_GLX_FEATURE_PBUFFER_MASK;
    
    return GLITZ_STATUS_NOT_SUPPORTED;
  }

  for (i = 0; i < num_configs; i++) {
    int value;
    
    if ((glx->get_fbconfig_attrib (display, fbconfigs[i],
                                   GLX_RENDER_TYPE, &value) != 0) ||
        (!(value & GLX_RGBA_BIT)))
      continue;

    /* Stereo is not supported yet */
    glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_STEREO, &value);
    if (value != 0)
      continue;

    glx->get_fbconfig_attrib (display, fbconfigs[i],
                              GLX_DRAWABLE_TYPE, &value);
    if (!((value & GLX_WINDOW_BIT) || (value & GLX_PBUFFER_BIT)))
      continue;
    
    format.types.window = (value & GLX_WINDOW_BIT)? 1: 0;
    format.types.pbuffer = (value & GLX_PBUFFER_BIT)? 1: 0;
    format.id = 0;
    
    glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_FBCONFIG_ID, &value);
    id = (XID) value;

    glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_RED_SIZE, &value);
    format.color.red_size = (unsigned short) value;
    glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_GREEN_SIZE, &value);
    format.color.green_size = (unsigned short) value;
    glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_BLUE_SIZE, &value);
    format.color.blue_size = (unsigned short) value;
    glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_ALPHA_SIZE, &value);
    format.color.alpha_size = (unsigned short) value;
    glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_DEPTH_SIZE, &value);
    format.depth_size = (unsigned short) value;
    glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_STENCIL_SIZE, &value);
    format.stencil_size = (unsigned short) value;
    glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_DOUBLEBUFFER, &value);
    format.doublebuffer = (value)? 1: 0;

    if (screen_info->glx_feature_mask & GLITZ_GLX_FEATURE_MULTISAMPLE_MASK) {
      glx->get_fbconfig_attrib (display, fbconfigs[i],
                                GLX_SAMPLE_BUFFERS_ARB, &value);
      if (value) {
        glx->get_fbconfig_attrib (display, fbconfigs[i],
                                  GLX_SAMPLES_ARB, &value);
        format.samples = (unsigned short) (value > 1)? value: 1;
        if (format.samples > 1) {
          if (!(screen_info->glx_feature_mask &
                GLITZ_GLX_FEATURE_PBUFFER_MULTISAMPLE_MASK))
            format.types.pbuffer = 0;
        }
      } else
        format.samples = 1;
    } else
      format.samples = 1;
    
    _glitz_add_format (screen_info, &format, id);
  }
  
  if (fbconfigs)
    XFree (fbconfigs);

  return GLITZ_STATUS_SUCCESS;
}

void
glitz_glx_query_formats (glitz_glx_screen_info_t *screen_info)
{
  glitz_status_t status = GLITZ_STATUS_NOT_SUPPORTED;
  XID *new_ids;
  int i;

  if (screen_info->glx_feature_mask & GLITZ_GLX_FEATURE_FBCONFIG_MASK)
    status = _glitz_glx_query_formats_using_fbconfigs (screen_info);

  if (status)
    _glitz_glx_query_formats (screen_info);

  if (!screen_info->n_formats)
    return;

  qsort (screen_info->formats, screen_info->n_formats, 
         sizeof (glitz_drawable_format_t), _glitz_glx_format_compare);

  /*
   * Update XID list so that it matches the sorted format list.
   */
  new_ids = malloc (sizeof (XID) * screen_info->n_formats);
  if (!new_ids) {
    screen_info->n_formats = 0;
    return;
  }
  
  for (i = 0; i < screen_info->n_formats; i++) {
    new_ids[i] = screen_info->format_ids[screen_info->formats[i].id];
    screen_info->formats[i].id = i;
  }
  
  free (screen_info->format_ids);
  screen_info->format_ids = new_ids;
}

glitz_drawable_format_t *
glitz_glx_find_drawable_format (Display                       *display,
                                int                           screen,
                                unsigned long                 mask,
                                const glitz_drawable_format_t *templ,
                                int                           count)
{
  glitz_glx_screen_info_t *screen_info =
    glitz_glx_screen_info_get (display, screen);

  return glitz_drawable_format_find (screen_info->formats,
                                     screen_info->n_formats,
                                     mask, templ, count);
}
slim_hidden_def(glitz_glx_find_drawable_format);

glitz_drawable_format_t *
glitz_glx_find_drawable_format_for_visual (Display  *display,
                                           int       screen,
                                           VisualID  visual_id)
{
    glitz_drawable_format_t *format = NULL;
    glitz_glx_screen_info_t *screen_info;
    int                     i; 

    screen_info = glitz_glx_screen_info_get (display, screen);
    if (!screen_info)
        return NULL;

    if (screen_info->glx_feature_mask & GLITZ_GLX_FEATURE_FBCONFIG_MASK)
    {
        glitz_glx_static_proc_address_list_t *glx = &screen_info->glx;
        GLXFBConfig                          *fbconfigs;
        int                                  fid, n_fbconfigs;
        
        fid = -1;
        fbconfigs = glx->get_fbconfigs (display, screen, &n_fbconfigs);
        for (i = 0; i < n_fbconfigs; i++)
        {
            XVisualInfo *visinfo;
            
            visinfo = glx->get_visual_from_fbconfig (display, fbconfigs[i]);
            if (visinfo && visinfo->visualid == visual_id)
            {
                int value;

                glx->get_fbconfig_attrib (display, fbconfigs[i],
                                          GLX_FBCONFIG_ID, &value);
                for (fid = 0; fid < screen_info->n_formats; fid++)
                {
                    if (screen_info->format_ids[fid] == value)
                    {
                        format = screen_info->formats + fid;
                        break;
                    }
                }
                
                if (format)
                    break;
            }
        }
        
        if (fbconfigs)
            XFree (fbconfigs);
    }
    else
    {
        for (i = 0; i < screen_info->n_formats; i++)
        {
            if (visual_id == screen_info->format_ids[i])
            {
                format = screen_info->formats + i;
                break;
            }
        }
    }
    
    return format;
}
slim_hidden_def(glitz_glx_find_drawable_format_for_visual);

XVisualInfo *
glitz_glx_get_visual_info_from_format (Display                 *display,
                                       int                     screen,
                                       glitz_drawable_format_t *format)
{
  XVisualInfo *vinfo = NULL;
  glitz_glx_screen_info_t *screen_info =
    glitz_glx_screen_info_get (display, screen);
  glitz_glx_static_proc_address_list_t *glx = &screen_info->glx;

  if (screen_info->glx_feature_mask & GLITZ_GLX_FEATURE_FBCONFIG_MASK) {
    GLXFBConfig *fbconfigs;
    int i, n_fbconfigs;
    int fbconfigid = screen_info->format_ids[format->id];

    fbconfigs = glx->get_fbconfigs (display, screen, &n_fbconfigs);
    for (i = 0; i < n_fbconfigs; i++) {
      int value;
      
      glx->get_fbconfig_attrib (display, fbconfigs[i],
                                GLX_FBCONFIG_ID, &value);
      if (value == fbconfigid)
        break;
    }
    
    if (i < n_fbconfigs)
      vinfo = glx->get_visual_from_fbconfig (display, fbconfigs[i]);
    
    if (fbconfigs)
      XFree (fbconfigs);
    
  } else {
    XVisualInfo templ;
    int n_items;
    
    templ.visualid = screen_info->format_ids[format->id];
    
    vinfo = XGetVisualInfo (display, VisualIDMask, &templ, &n_items);
  }
  
  return vinfo;
}
slim_hidden_def(glitz_glx_get_visual_info_from_format);
