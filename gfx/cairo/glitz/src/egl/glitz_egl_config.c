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
    glitz_int_drawable_format_t *format[2];
    int				i, score[2];

    format[0] = (glitz_int_drawable_format_t *) elem1;
    format[1] = (glitz_int_drawable_format_t *) elem2;
    i = score[0] = score[1] = 0;

    for (; i < 2; i++)
    {
	if (format[i]->d.color.red_size)
	{
	    if (format[i]->d.color.red_size >= 8)
		score[i] += 5;

	    score[i] += 10;
	}

	if (format[i]->d.color.alpha_size)
	{
	    if (format[i]->d.color.alpha_size >= 8)
		score[i] += 5;

	    score[i] += 10;
	}

	if (format[i]->d.stencil_size)
	    score[i] += 5;

	if (format[i]->d.depth_size)
	    score[i] += 5;

	if (format[i]->d.doublebuffer)
	    score[i] += 10;

	if (format[i]->d.samples > 1)
	    score[i] -= (20 - format[i]->d.samples);

	if (format[i]->types & GLITZ_DRAWABLE_TYPE_WINDOW_MASK)
	    score[i] += 10;

	if (format[i]->types & GLITZ_DRAWABLE_TYPE_PBUFFER_MASK)
	    score[i] += 10;

	if (format[i]->caveat)
	    score[i] -= 1000;
    }

    return score[1] - score[0];
}

static void
_glitz_add_format (glitz_egl_screen_info_t     *screen_info,
		   glitz_int_drawable_format_t *format)
{
    int n = screen_info->n_formats;

    screen_info->formats =
	realloc (screen_info->formats,
		 sizeof (glitz_int_drawable_format_t) * (n + 1));
    if (screen_info->formats)
    {
	screen_info->formats[n] = *format;
	screen_info->formats[n].d.id = n;
	screen_info->n_formats++;
    }
}

static void
_glitz_egl_query_configs (glitz_egl_screen_info_t *screen_info)
{
    glitz_int_drawable_format_t format;
    EGLDisplay			egl_display;
    EGLConfig			*egl_configs;
    int				i, num_configs;

    egl_display = screen_info->display_info->egl_display;

    eglGetConfigs (egl_display, NULL, 0, &num_configs);
    egl_configs = malloc (sizeof (EGLConfig) * num_configs);
    if (!egl_configs)
	return;

    format.d.id = 0;
    format.d.doublebuffer = 1;

    eglGetConfigs (egl_display, egl_configs, num_configs, &num_configs);

    for (i = 0; i < num_configs; i++)
    {
	int value;

	eglGetConfigAttrib (egl_display, egl_configs[i], EGL_SURFACE_TYPE,
			    &value);
	if (!((value & EGL_WINDOW_BIT) || (value & EGL_PBUFFER_BIT)))
	    continue;

	format.types = 0;
	if (value & EGL_WINDOW_BIT)
	    format.types |= GLITZ_DRAWABLE_TYPE_WINDOW_MASK;

	if (value & EGL_PBUFFER_BIT)
	    format.types |= GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;

	eglGetConfigAttrib (egl_display, egl_configs[i], EGL_CONFIG_ID,
                            &value);
	format.u.uval = value;

	eglGetConfigAttrib (egl_display, egl_configs[i], EGL_RED_SIZE, &value);
	format.d.color.red_size = (unsigned short) value;
	eglGetConfigAttrib (egl_display, egl_configs[i], EGL_GREEN_SIZE,
                            &value);
	format.d.color.green_size = (unsigned short) value;
	eglGetConfigAttrib (egl_display, egl_configs[i], EGL_BLUE_SIZE,
                            &value);
	format.d.color.blue_size = (unsigned short) value;
	eglGetConfigAttrib (egl_display, egl_configs[i], EGL_ALPHA_SIZE,
                            &value);
	format.d.color.alpha_size = (unsigned short) value;
	eglGetConfigAttrib (egl_display, egl_configs[i], EGL_DEPTH_SIZE,
                            &value);
	format.d.depth_size = (unsigned short) value;
	eglGetConfigAttrib (egl_display, egl_configs[i], EGL_STENCIL_SIZE,
			    &value);
	format.d.stencil_size = (unsigned short) value;
	eglGetConfigAttrib (egl_display, egl_configs[i], EGL_CONFIG_CAVEAT,
			    &value);
	format.caveat = (unsigned short) value;

	eglGetConfigAttrib (egl_display, egl_configs[i], EGL_SAMPLE_BUFFERS,
			    &value);
	if (value)
	{
	    eglGetConfigAttrib (egl_display, egl_configs[i], EGL_SAMPLES,
                                &value);
	    format.d.samples = (unsigned short) (value > 1)? value: 1;
	}
	else
	    format.d.samples = 1;

	_glitz_add_format (screen_info, &format);
    }

    free (egl_configs);
}

void
glitz_egl_query_configs (glitz_egl_screen_info_t *screen_info)
{
    int i;

    _glitz_egl_query_configs (screen_info);

    if (!screen_info->n_formats)
	return;

    qsort (screen_info->formats, screen_info->n_formats,
	   sizeof (glitz_int_drawable_format_t), _glitz_egl_format_compare);

    for (i = 0; i < screen_info->n_formats; i++)
	screen_info->formats[i].d.id = i;
}

glitz_drawable_format_t *
glitz_egl_find_window_config (EGLDisplay                    egl_display,
			      EGLScreenMESA                 egl_screen,
			      unsigned long                 mask,
			      const glitz_drawable_format_t *templ,
			      int                           count)
{
    glitz_int_drawable_format_t itempl;
    glitz_egl_screen_info_t *screen_info =
	glitz_egl_screen_info_get (egl_display, egl_screen);

    glitz_drawable_format_copy (templ, &itempl.d, mask);

    itempl.types = GLITZ_DRAWABLE_TYPE_WINDOW_MASK;
    mask |= GLITZ_INT_FORMAT_WINDOW_MASK;

    return glitz_drawable_format_find (screen_info->formats,
				       screen_info->n_formats,
				       mask, &itempl, count);
}
slim_hidden_def(glitz_egl_find_window_config);

glitz_drawable_format_t *
glitz_egl_find_pbuffer_config (EGLDisplay                    egl_display,
			       EGLScreenMESA                 egl_screen,
			       unsigned long                 mask,
			       const glitz_drawable_format_t *templ,
			       int                           count)
{
    glitz_int_drawable_format_t itempl;
    glitz_egl_screen_info_t *screen_info =
	glitz_egl_screen_info_get (egl_display, egl_screen);

    glitz_drawable_format_copy (templ, &itempl.d, mask);

    itempl.types = GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;
    mask |= GLITZ_INT_FORMAT_PBUFFER_MASK;

    return glitz_drawable_format_find (screen_info->formats,
				       screen_info->n_formats,
				       mask, &itempl, count);
}
slim_hidden_def(glitz_egl_find_pbuffer_config);
