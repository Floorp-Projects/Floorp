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

void
_glitz_drawable_init (glitz_drawable_t	          *drawable,
		      glitz_int_drawable_format_t *format,
		      glitz_backend_t	          *backend,
		      int		          width,
		      int		          height)
{
    drawable->ref_count = 1;

    drawable->format  = format;
    drawable->backend = backend;

    drawable->width  = width;
    drawable->height = height;

    drawable->front = NULL;
    drawable->back  = NULL;

    drawable->viewport.x = -32767;
    drawable->viewport.y = -32767;
    drawable->viewport.width = 65535;
    drawable->viewport.height = 65535;

    drawable->update_all = 1;
}

static glitz_bool_t
_glitz_drawable_size_check (glitz_drawable_t *other,
			    unsigned int     width,
			    unsigned int     height)
{
    if (width == 0 || height == 0)
	return 0;

    if (width  > other->backend->max_viewport_dims[0] ||
	height > other->backend->max_viewport_dims[1])
	return 0;

    return 1;
}

glitz_drawable_t *
glitz_create_drawable (glitz_drawable_t        *other,
		       glitz_drawable_format_t *format,
		       unsigned int            width,
		       unsigned int            height)
{
    glitz_int_drawable_format_t *iformat;

    if (!_glitz_drawable_size_check (other, width, height))
	return NULL;

    if (format->id >= other->backend->n_drawable_formats)
	return NULL;

    iformat = &other->backend->drawable_formats[format->id];
    if (!(iformat->types & GLITZ_DRAWABLE_TYPE_FBO_MASK))
	return NULL;

    return _glitz_fbo_drawable_create (other, iformat, width, height);
}
slim_hidden_def(glitz_create_drawable);

glitz_drawable_t *
glitz_create_pbuffer_drawable (glitz_drawable_t        *other,
			       glitz_drawable_format_t *format,
			       unsigned int            width,
			       unsigned int            height)
{
    glitz_int_drawable_format_t *iformat;

    if (!_glitz_drawable_size_check (other, width, height))
	return NULL;

    if (format->id >= other->backend->n_drawable_formats)
	return NULL;

    iformat = &other->backend->drawable_formats[format->id];
    if (!(iformat->types & GLITZ_DRAWABLE_TYPE_PBUFFER_MASK))
	return NULL;

    return other->backend->create_pbuffer (other, format, width, height);
}
slim_hidden_def(glitz_create_pbuffer_drawable);

void
glitz_drawable_destroy (glitz_drawable_t *drawable)
{
    if (!drawable)
	return;

    drawable->ref_count--;
    if (drawable->ref_count)
	return;

    drawable->backend->destroy (drawable);
}

void
glitz_drawable_reference (glitz_drawable_t *drawable)
{
    if (!drawable)
	return;

    drawable->ref_count++;
}

void
glitz_drawable_update_size (glitz_drawable_t *drawable,
			    unsigned int     width,
			    unsigned int     height)
{
    drawable->width = (int) width;
    drawable->height = (int) height;

    drawable->viewport.x = -32767;
    drawable->viewport.y = -32767;
    drawable->viewport.width = 65535;
    drawable->viewport.height = 65535;

    drawable->update_all = 1;
}

unsigned int
glitz_drawable_get_width (glitz_drawable_t *drawable)
{
    return (unsigned int) drawable->width;
}
slim_hidden_def(glitz_drawable_get_width);

unsigned int
glitz_drawable_get_height (glitz_drawable_t *drawable)
{
    return (unsigned int) drawable->height;
}
slim_hidden_def(glitz_drawable_get_height);

void
glitz_drawable_swap_buffer_region (glitz_drawable_t *drawable,
				   int              x_origin,
				   int              y_origin,
				   glitz_box_t      *box,
				   int              n_box)
{
    if (drawable->format->d.doublebuffer && n_box)
    {
	glitz_box_t	rect;
	glitz_surface_t *surface = NULL;
	int		x_pos, y_pos;
	int		x, y, w, h;

	GLITZ_GL_DRAWABLE (drawable);

	if (n_box == 1)
	{
	    rect.x1 = x_origin + box->x1;
	    rect.y1 = y_origin + box->y1;
	    rect.x2 = x_origin + box->x2;
	    rect.y2 = y_origin + box->y2;

	    if (rect.x1 <= 0		   &&
		rect.y1 <= 0		   &&
		rect.x2 >= drawable->width &&
		rect.x2 >= drawable->height)
	    {
		if (drawable->backend->swap_buffers (drawable))
		{
		    if (drawable->front)
		    {
			REGION_EMPTY (&drawable->front->drawable_damage);
			glitz_surface_damage (drawable->front, NULL,
					      GLITZ_DAMAGE_TEXTURE_MASK |
					      GLITZ_DAMAGE_SOLID_MASK);
		    }
		    return;
		}
	    }
	}

	if (drawable->front)
	{
	    if (glitz_surface_push_current (drawable->front,
					    GLITZ_DRAWABLE_CURRENT))
		surface = drawable->front;
	}

	if (!surface)
	{
	    if (drawable->backend->push_current (drawable, NULL,
						 GLITZ_DRAWABLE_CURRENT))
	    {
		drawable->update_all = 1;

		gl->viewport (0, 0, drawable->width, drawable->height);
		gl->matrix_mode (GLITZ_GL_PROJECTION);
		gl->load_identity ();
		gl->ortho (0.0, drawable->width, 0.0,
			   drawable->height, -1.0, 1.0);
		gl->matrix_mode (GLITZ_GL_MODELVIEW);
		gl->load_identity ();
		gl->scale_f (1.0f, -1.0f, 1.0f);
		gl->translate_f (0.0f, -drawable->height, 0.0f);
	    }
	    else
	    {
		drawable->backend->pop_current (drawable);
		return;
	    }
	}

	gl->disable (GLITZ_GL_DITHER);

	gl->read_buffer (GLITZ_GL_BACK);
	gl->draw_buffer (GLITZ_GL_FRONT);

	glitz_set_operator (gl, GLITZ_OPERATOR_SRC);

	x_pos = 0;
	y_pos = 0;

	glitz_set_raster_pos (gl, x_pos, y_pos);

	while (n_box--)
	{
	    rect.x1 = x_origin + box->x1;
	    rect.y1 = y_origin + box->y1;
	    rect.x2 = x_origin + box->x2;
	    rect.y2 = y_origin + box->y2;

	    if (rect.x1 < rect.x2 && rect.y1 < rect.y2)
	    {
		x = rect.x1;
		y = drawable->height - rect.y2;
		w = rect.x2 - rect.x1;
		h = rect.y2 - rect.y1;

		if (x != x_pos || y != y_pos)
		{
		    gl->bitmap (0, 0, 0, 0, x - x_pos, y - y_pos, NULL);

		    x_pos = x;
		    y_pos = y;
		}

		gl->scissor (x, y, w, h);
		gl->copy_pixels (x, y, w, h, GLITZ_GL_COLOR);

		if (surface)
		    glitz_surface_damage (surface, &rect,
					  GLITZ_DAMAGE_TEXTURE_MASK |
					  GLITZ_DAMAGE_SOLID_MASK);

		box++;
	    }
	}
	drawable->backend->gl->finish ();

	if (surface)
	    glitz_surface_pop_current (surface);
	else
	    drawable->backend->pop_current (drawable);
    }
}

void
glitz_drawable_swap_buffers (glitz_drawable_t *drawable)
{
    glitz_box_t box;

    box.x1 = 0;
    box.y1 = 0;
    box.x2 = drawable->width;
    box.y2 = drawable->height;

    glitz_drawable_swap_buffer_region (drawable, 0, 0, &box, 1);
}
slim_hidden_def(glitz_drawable_swap_buffers);

void
glitz_drawable_flush (glitz_drawable_t *drawable)
{
    drawable->backend->push_current (drawable, NULL, GLITZ_DRAWABLE_CURRENT);
    drawable->backend->gl->flush ();
    drawable->backend->pop_current (drawable);
}
slim_hidden_def(glitz_drawable_flush);

void
glitz_drawable_finish (glitz_drawable_t *drawable)
{
    drawable->backend->push_current (drawable, NULL, GLITZ_DRAWABLE_CURRENT);
    drawable->backend->gl->finish ();
    drawable->backend->pop_current (drawable);
}
slim_hidden_def(glitz_drawable_finish);

unsigned long
glitz_drawable_get_features (glitz_drawable_t *drawable)
{
    return drawable->backend->feature_mask;
}
slim_hidden_def(glitz_drawable_get_features);

glitz_drawable_format_t *
glitz_drawable_get_format (glitz_drawable_t *drawable)
{
    return &drawable->format->d;
}
slim_hidden_def(glitz_drawable_get_format);
