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
_glitz_add_format (glitz_glx_screen_info_t     *screen_info,
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
_glitz_glx_query_formats (glitz_glx_screen_info_t *screen_info)
{
    Display			*display;
    glitz_int_drawable_format_t format;
    XVisualInfo			visual_templ;
    XVisualInfo			*visuals;
    int				i, num_visuals;

    display = screen_info->display_info->display;

    visual_templ.screen = screen_info->screen;
    visuals = XGetVisualInfo (display, VisualScreenMask,
			      &visual_templ, &num_visuals);

    /* No pbuffers without fbconfigs */
    format.types = GLITZ_DRAWABLE_TYPE_WINDOW_MASK;
    format.d.id  = 0;

    for (i = 0; i < num_visuals; i++)
    {
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
	format.d.color.red_size = (unsigned short) value;
	glXGetConfig (display, &visuals[i], GLX_GREEN_SIZE, &value);
	format.d.color.green_size = (unsigned short) value;
	glXGetConfig (display, &visuals[i], GLX_BLUE_SIZE, &value);
	format.d.color.blue_size = (unsigned short) value;
	glXGetConfig (display, &visuals[i], GLX_ALPHA_SIZE, &value);
	format.d.color.alpha_size = (unsigned short) value;
	glXGetConfig (display, &visuals[i], GLX_DEPTH_SIZE, &value);
	format.d.depth_size = (unsigned short) value;
	glXGetConfig (display, &visuals[i], GLX_STENCIL_SIZE, &value);
	format.d.stencil_size = (unsigned short) value;
	glXGetConfig (display, &visuals[i], GLX_DOUBLEBUFFER, &value);
	format.d.doublebuffer = (value) ? 1: 0;

	if (screen_info->glx_feature_mask &
	    GLITZ_GLX_FEATURE_VISUAL_RATING_MASK)
	{
	    glXGetConfig (display, &visuals[i], GLX_VISUAL_CAVEAT_EXT, &value);
	    switch (value) {
	    case GLX_SLOW_VISUAL_EXT:
	    case GLX_NON_CONFORMANT_VISUAL_EXT:
		format.caveat = 1;
		break;
	    default:
		format.caveat = 0;
		break;
	    }
	}
	else
	    format.caveat = 0;

	if (screen_info->glx_feature_mask & GLITZ_GLX_FEATURE_MULTISAMPLE_MASK)
	{
	    glXGetConfig (display, &visuals[i], GLX_SAMPLE_BUFFERS_ARB,
			  &value);
	    if (value)
	    {
		glXGetConfig (display, &visuals[i], GLX_SAMPLES_ARB, &value);
		format.d.samples = (unsigned short) (value > 1)? value: 1;
	    }
	    else
		format.d.samples = 1;
	}
	else
	    format.d.samples = 1;

	format.u.uval = visuals[i].visualid;

	_glitz_add_format (screen_info, &format);
    }

    if (visuals)
	XFree (visuals);
}

static glitz_status_t
_glitz_glx_query_formats_using_fbconfigs (glitz_glx_screen_info_t *screen_info)
{
    glitz_glx_static_proc_address_list_t *glx = &screen_info->glx;
    Display				 *display;
    glitz_int_drawable_format_t		 format;
    GLXFBConfig				 *fbconfigs;
    int					 i, num_configs;

    display = screen_info->display_info->display;

    fbconfigs = glx->get_fbconfigs (display, screen_info->screen,
				    &num_configs);
    if (!fbconfigs)
    {
	/* fbconfigs are not supported, falling back to visuals */
	screen_info->glx_feature_mask &= ~GLITZ_GLX_FEATURE_FBCONFIG_MASK;
	screen_info->glx_feature_mask &= ~GLITZ_GLX_FEATURE_PBUFFER_MASK;

	return GLITZ_STATUS_NOT_SUPPORTED;
    }

    for (i = 0; i < num_configs; i++)
    {
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

	format.types = 0;
	if (value & GLX_WINDOW_BIT)
	    format.types |= GLITZ_DRAWABLE_TYPE_WINDOW_MASK;

	if (value & GLX_PBUFFER_BIT)
	    format.types |= GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;

	format.d.id = 0;

	glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_FBCONFIG_ID,
				  &value);
	format.u.uval = value;

	glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_RED_SIZE, &value);
	format.d.color.red_size = (unsigned short) value;
	glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_GREEN_SIZE,
				  &value);
	format.d.color.green_size = (unsigned short) value;
	glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_BLUE_SIZE,
				  &value);
	format.d.color.blue_size = (unsigned short) value;
	glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_ALPHA_SIZE,
				  &value);
	format.d.color.alpha_size = (unsigned short) value;
	glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_DEPTH_SIZE,
				  &value);
	format.d.depth_size = (unsigned short) value;
	glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_STENCIL_SIZE,
				  &value);
	format.d.stencil_size = (unsigned short) value;
	glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_DOUBLEBUFFER,
				  &value);
	format.d.doublebuffer = (value)? 1: 0;
	glx->get_fbconfig_attrib (display, fbconfigs[i], GLX_CONFIG_CAVEAT,
				  &value);
	switch (value) {
	case GLX_SLOW_VISUAL_EXT:
	case GLX_NON_CONFORMANT_VISUAL_EXT:
	    format.caveat = 1;
	    break;
	default:
	    format.caveat = 0;
	    break;
	}

	if (screen_info->glx_feature_mask & GLITZ_GLX_FEATURE_MULTISAMPLE_MASK)
	{
	    glx->get_fbconfig_attrib (display, fbconfigs[i],
				      GLX_SAMPLE_BUFFERS_ARB, &value);
	    if (value)
	    {
		glx->get_fbconfig_attrib (display, fbconfigs[i],
					  GLX_SAMPLES_ARB, &value);
		format.d.samples = (unsigned short) (value > 1)? value: 1;
		if (format.d.samples > 1)
		{
		    if (!(screen_info->glx_feature_mask &
			  GLITZ_GLX_FEATURE_PBUFFER_MULTISAMPLE_MASK))
			format.types &= ~GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;
		}
	    }
	    else
		format.d.samples = 1;
	}
	else
	    format.d.samples = 1;

	_glitz_add_format (screen_info, &format);
    }

    if (fbconfigs)
	XFree (fbconfigs);

    return GLITZ_STATUS_SUCCESS;
}

void
glitz_glx_query_formats (glitz_glx_screen_info_t *screen_info)
{
    glitz_status_t status = GLITZ_STATUS_NOT_SUPPORTED;
    int		   i;

    if (screen_info->glx_feature_mask & GLITZ_GLX_FEATURE_FBCONFIG_MASK)
	status = _glitz_glx_query_formats_using_fbconfigs (screen_info);

    if (status)
	_glitz_glx_query_formats (screen_info);

    if (!screen_info->n_formats)
	return;

    qsort (screen_info->formats, screen_info->n_formats,
	   sizeof (glitz_int_drawable_format_t), _glitz_glx_format_compare);

    for (i = 0; i < screen_info->n_formats; i++)
	screen_info->formats[i].d.id = i;
}

glitz_drawable_format_t *
glitz_glx_find_window_format (Display                       *display,
			      int                           screen,
			      unsigned long                 mask,
			      const glitz_drawable_format_t *templ,
			      int                           count)
{
    glitz_int_drawable_format_t itempl;
    glitz_glx_screen_info_t *screen_info =
	glitz_glx_screen_info_get (display, screen);

    glitz_drawable_format_copy (templ, &itempl.d, mask);

    itempl.types = GLITZ_DRAWABLE_TYPE_WINDOW_MASK;
    mask |= GLITZ_INT_FORMAT_WINDOW_MASK;

    return glitz_drawable_format_find (screen_info->formats,
				       screen_info->n_formats,
				       mask, &itempl, count);
}
slim_hidden_def(glitz_glx_find_window_format);

glitz_drawable_format_t *
glitz_glx_find_pbuffer_format (Display                       *display,
			       int                           screen,
			       unsigned long                 mask,
			       const glitz_drawable_format_t *templ,
			       int                           count)
{
    glitz_int_drawable_format_t itempl;
    glitz_glx_screen_info_t *screen_info =
	glitz_glx_screen_info_get (display, screen);

    glitz_drawable_format_copy (templ, &itempl.d, mask);

    itempl.types = GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;
    mask |= GLITZ_INT_FORMAT_PBUFFER_MASK;

    return glitz_drawable_format_find (screen_info->formats,
				       screen_info->n_formats,
				       mask, &itempl, count);
}
slim_hidden_def(glitz_glx_find_pbuffer_format);

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
		    if (screen_info->formats[fid].u.uval == value)
		    {
			format = &screen_info->formats[fid].d;
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
	    if (visual_id == screen_info->formats[i].u.uval)
	    {
		format = &screen_info->formats[i].d;
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

    if (screen_info->glx_feature_mask & GLITZ_GLX_FEATURE_FBCONFIG_MASK)
    {
	GLXFBConfig *fbconfigs;
	int	    i, n_fbconfigs;
	int	    fbconfigid = screen_info->formats[format->id].u.uval;

	fbconfigs = glx->get_fbconfigs (display, screen, &n_fbconfigs);
	for (i = 0; i < n_fbconfigs; i++)
	{
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

    }
    else
    {
	XVisualInfo templ;
	int	    n_items;

	templ.visualid = screen_info->formats[format->id].u.uval;

	vinfo = XGetVisualInfo (display, VisualIDMask, &templ, &n_items);
    }

    return vinfo;
}
slim_hidden_def(glitz_glx_get_visual_info_from_format);
