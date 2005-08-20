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

glitz_drawable_format_t *
glitz_find_similar_drawable_format (glitz_drawable_t              *other,
                                    unsigned long                 mask,
                                    const glitz_drawable_format_t *templ,
                                    int                           count)
{
  return glitz_drawable_format_find (other->backend->drawable_formats,
                                     other->backend->n_drawable_formats,
                                     mask, templ, count);
}
slim_hidden_def(glitz_find_similar_drawable_format);
    
glitz_drawable_t *
glitz_create_pbuffer_drawable (glitz_drawable_t        *other,
                               glitz_drawable_format_t *format,
                               unsigned int            width,
                               unsigned int            height)
{
  if (!format->types.pbuffer)
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
glitz_drawable_swap_buffers (glitz_drawable_t *drawable)
{
  if (drawable->format->doublebuffer)
    drawable->backend->swap_buffers (drawable);
}
slim_hidden_def(glitz_drawable_swap_buffers);

void
glitz_drawable_flush (glitz_drawable_t *drawable)
{
  drawable->backend->push_current (drawable, NULL, GLITZ_DRAWABLE_CURRENT);
  drawable->backend->gl.flush ();
  drawable->backend->pop_current (drawable);
}
slim_hidden_def(glitz_drawable_flush);

void
glitz_drawable_finish (glitz_drawable_t *drawable)
{
  drawable->backend->push_current (drawable, NULL, GLITZ_DRAWABLE_CURRENT);
  drawable->backend->gl.finish ();
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
  return drawable->format;
}
slim_hidden_def(glitz_drawable_get_format);
