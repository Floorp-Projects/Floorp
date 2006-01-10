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

#include "glitz_aglint.h"

#include <stdlib.h>
#include <string.h>

static GLint _attribs[] = {
    AGL_RGBA,
    AGL_ALPHA_SIZE, 16,
    AGL_RED_SIZE, 16,
    AGL_NONE
};

static GLint _db_attribs[] = {
    AGL_RGBA,
    AGL_DOUBLEBUFFER,
    AGL_ALPHA_SIZE, 16,
    AGL_RED_SIZE, 16,
    AGL_NONE
};

static GLint _stencil_attribs[] = {
    AGL_RGBA,
    AGL_ALPHA_SIZE, 16,
    AGL_RED_SIZE, 16,
    AGL_DEPTH_SIZE, 8,
    AGL_STENCIL_SIZE, 8,
    AGL_NONE
};

static GLint _db_stencil_attribs[] = {
    AGL_RGBA,
    AGL_DOUBLEBUFFER,
    AGL_ALPHA_SIZE, 16,
    AGL_RED_SIZE, 16,
    AGL_STENCIL_SIZE, 8,
    AGL_NONE
};

static GLint _ms2_attribs[] = {
    AGL_RGBA,
    AGL_ALPHA_SIZE, 16,
    AGL_RED_SIZE, 16,
    AGL_STENCIL_SIZE, 8,
    AGL_SAMPLE_BUFFERS_ARB, 1,
    AGL_SAMPLES_ARB, 2,
    AGL_NONE
};

static GLint _db_ms2_attribs[] = {
    AGL_RGBA,
    AGL_DOUBLEBUFFER,
    AGL_ALPHA_SIZE, 16,
    AGL_RED_SIZE, 16,
    AGL_STENCIL_SIZE, 8,
    AGL_SAMPLE_BUFFERS_ARB, 1,
    AGL_SAMPLES_ARB, 2,
    AGL_NONE
};

static GLint _ms4_attribs[] = {
    AGL_RGBA,
    AGL_ALPHA_SIZE, 16,
    AGL_RED_SIZE, 16,
    AGL_STENCIL_SIZE, 8,
    AGL_SAMPLE_BUFFERS_ARB, 1,
    AGL_SAMPLES_ARB, 4,
    AGL_NONE
};

static GLint _db_ms4_attribs[] = {
    AGL_RGBA,
    AGL_DOUBLEBUFFER,
    AGL_ALPHA_SIZE, 16,
    AGL_RED_SIZE, 16,
    AGL_STENCIL_SIZE, 8,
    AGL_SAMPLE_BUFFERS_ARB, 1,
    AGL_SAMPLES_ARB, 4,
    AGL_NONE
};

static GLint _ms6_attribs[] = {
    AGL_RGBA,
    AGL_ALPHA_SIZE, 16,
    AGL_RED_SIZE, 16,
    AGL_STENCIL_SIZE, 8,
    AGL_SAMPLE_BUFFERS_ARB, 1,
    AGL_SAMPLES_ARB, 6,
    AGL_NONE
};

static GLint _db_ms6_attribs[] = {
    AGL_RGBA,
    AGL_DOUBLEBUFFER,
    AGL_ALPHA_SIZE, 16,
    AGL_RED_SIZE, 16,
    AGL_STENCIL_SIZE, 8,
    AGL_SAMPLE_BUFFERS_ARB, 1,
    AGL_SAMPLES_ARB, 6,
    AGL_NONE
};

static GLint *_attribs_list[] = {
    _attribs,
    _db_attribs,
    _stencil_attribs,
    _db_stencil_attribs,
    _ms2_attribs,
    _db_ms2_attribs,
    _ms4_attribs,
    _db_ms4_attribs,
    _ms6_attribs,
    _db_ms6_attribs
};

static int
_glitz_agl_format_compare (const void *elem1,
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
_glitz_agl_add_format (glitz_agl_thread_info_t     *thread_info,
		       glitz_int_drawable_format_t *format,
		       AGLPixelFormat               pixel_format)
{
    if (!glitz_drawable_format_find (thread_info->formats,
				     thread_info->n_formats,
				     GLITZ_DRAWABLE_FORMAT_ALL_EXCEPT_ID_MASK,
				     format, 0)) {
	int n = thread_info->n_formats;

	thread_info->formats =
	    realloc (thread_info->formats,
		     sizeof (glitz_int_drawable_format_t) * (n + 1));
	thread_info->pixel_formats =
	    realloc (thread_info->pixel_formats,
		     sizeof (AGLPixelFormat) * (n + 1));

	if (thread_info->formats && thread_info->pixel_formats) {
	    thread_info->formats[n] = *(glitz_int_drawable_format_t*)format;
	    thread_info->formats[n].d.id = n;
	    thread_info->pixel_formats[n] = pixel_format;
	    thread_info->n_formats++;
	}
    }
}

void
glitz_agl_query_formats (glitz_agl_thread_info_t *thread_info)
{
    glitz_int_drawable_format_t format;
    AGLPixelFormat pixel_format, *new_pfs;
    int n_attribs_list, i;

    format.types	= GLITZ_DRAWABLE_TYPE_WINDOW_MASK;
    format.d.id		= 0;
    format.color.fourcc = GLITZ_FOURCC_RGB;

    n_attribs_list = sizeof (_attribs_list) / sizeof (GLint *);

    for (i = 0; i < n_attribs_list; i++) {
	GLint value;

	pixel_format = aglChoosePixelFormat (NULL, 0, _attribs_list[i]);

	/* Stereo is not supported yet */
	if (!(aglDescribePixelFormat (pixel_format, AGL_STEREO, &value)) ||
	    value) {
	    aglDestroyPixelFormat (pixel_format);
	    continue;
	}

	aglDescribePixelFormat (pixel_format, AGL_DOUBLEBUFFER, &value);
	format.d.doublebuffer = (value)? 1: 0;

	aglDescribePixelFormat (pixel_format, AGL_RED_SIZE, &value);
	format.d.color.red_size = (unsigned short) value;
	aglDescribePixelFormat (pixel_format, AGL_GREEN_SIZE, &value);
	format.d.color.green_size = (unsigned short) value;
	aglDescribePixelFormat (pixel_format, AGL_BLUE_SIZE, &value);
	format.d.color.blue_size = (unsigned short) value;
	aglDescribePixelFormat (pixel_format, AGL_ALPHA_SIZE, &value);
	format.d.color.alpha_size = (unsigned short) value;
	aglDescribePixelFormat (pixel_format, AGL_DEPTH_SIZE, &value);
	format.d.depth_size = (unsigned short) value;
	aglDescribePixelFormat (pixel_format, AGL_STENCIL_SIZE, &value);
	format.d.stencil_size = (unsigned short) value;

	if (thread_info->agl_feature_mask & GLITZ_AGL_FEATURE_MULTISAMPLE_MASK)
	{
	    aglDescribePixelFormat (pixel_format, AGL_SAMPLE_BUFFERS_ARB,
				    &value);
	    if (value) {
		aglDescribePixelFormat (pixel_format, AGL_SAMPLES_ARB, &value);
		format.d.samples = (unsigned short) (value > 1)? value: 1;
	    } else
		format.d.samples = 1;
	} else
	    format.d.samples = 1;

	if (thread_info->agl_feature_mask & GLITZ_AGL_FEATURE_PBUFFER_MASK) {
	    if (format.d.color.red_size && format.d.color.green_size &&
		format.d.color.blue_size && format.d.color.alpha_size &&
		format.d.doublebuffer == 0 && format.d.stencil_size == 0 &&
		format.d.depth_size == 0) {

		if (thread_info->agl_feature_mask &
		    GLITZ_AGL_FEATURE_PBUFFER_MULTISAMPLE_MASK)
		    format.types |= GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;
		else if (format.d.samples == 1)
		    format.types |= GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;
		else
		    format.types &= ~GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;
	    } else
		format.types &= ~GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;
	} else
	    format.types &= ~GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;

	if (format.d.color.red_size ||
	    format.d.color.green_size ||
	    format.d.color.blue_size ||
	    format.d.color.alpha_size)
	    _glitz_agl_add_format (thread_info, &format, pixel_format);
    }

    if (!thread_info->n_formats)
	return;

    qsort (thread_info->formats, thread_info->n_formats,
	   sizeof (glitz_int_drawable_format_t), _glitz_agl_format_compare);

    /*
     * Update AGLPixelFormat list so that it matches the sorted format list.
     */
    new_pfs = malloc (sizeof (AGLPixelFormat) * thread_info->n_formats);
    if (!new_pfs) {
	thread_info->n_formats = 0;
	return;
    }

    for (i = 0; i < thread_info->n_formats; i++) {
	new_pfs[i] = thread_info->pixel_formats[thread_info->formats[i].d.id];
	thread_info->formats[i].d.id = i;
    }

    free (thread_info->pixel_formats);
    thread_info->pixel_formats = new_pfs;
}

glitz_drawable_format_t *
glitz_agl_find_window_format (unsigned long                 mask,
			      const glitz_drawable_format_t *templ,
			      int                           count)
{
    glitz_int_drawable_format_t itempl;
    glitz_agl_thread_info_t *thread_info =
	glitz_agl_thread_info_get ();

    glitz_drawable_format_copy (templ, &itempl.d, mask);

    itempl.types = GLITZ_DRAWABLE_TYPE_WINDOW_MASK;
    mask |= GLITZ_INT_FORMAT_WINDOW_MASK;

    return glitz_drawable_format_find (thread_info->formats,
				       thread_info->n_formats,
				       mask, &itempl, count);
}
slim_hidden_def(glitz_agl_find_window_format);


glitz_drawable_format_t *
glitz_agl_find_pbuffer_format (unsigned long                 mask,
			      const glitz_drawable_format_t *templ,
			      int                           count)
{
    glitz_int_drawable_format_t itempl;
    glitz_agl_thread_info_t *thread_info =
	glitz_agl_thread_info_get ();

    glitz_drawable_format_copy (templ, &itempl.d, mask);

    itempl.types = GLITZ_DRAWABLE_TYPE_PBUFFER_MASK;
    mask |= GLITZ_INT_FORMAT_PBUFFER_MASK;

    return glitz_drawable_format_find (thread_info->formats,
				       thread_info->n_formats,
				       mask, &itempl, count);
}
slim_hidden_def(glitz_agl_find_pbuffer_format);

