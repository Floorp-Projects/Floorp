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


static glitz_gl_enum_t
_glitz_data_type (glitz_data_type_t type)
{
    switch (type) {
    case GLITZ_DATA_TYPE_SHORT:
	return GLITZ_GL_SHORT;
    case GLITZ_DATA_TYPE_INT:
	return GLITZ_GL_INT;
    case GLITZ_DATA_TYPE_DOUBLE:
	return GLITZ_GL_DOUBLE;
    default:
	return GLITZ_GL_FLOAT;
    }
}

void
glitz_set_geometry (glitz_surface_t         *dst,
		    glitz_geometry_type_t   type,
		    glitz_geometry_format_t *format,
		    glitz_buffer_t          *buffer)
{
    switch (type) {
    case GLITZ_GEOMETRY_TYPE_VERTEX:
    {
	glitz_buffer_reference (buffer);
	if (dst->geometry.buffer)
	    glitz_buffer_destroy (dst->geometry.buffer);
	dst->geometry.buffer = buffer;

	dst->geometry.type = GLITZ_GEOMETRY_TYPE_VERTEX;

	switch (format->vertex.primitive) {
	case GLITZ_PRIMITIVE_POINTS:
	    dst->geometry.u.v.prim = GLITZ_GL_POINTS;
	    break;
	case GLITZ_PRIMITIVE_LINES:
	    dst->geometry.u.v.prim = GLITZ_GL_LINES;
	    break;
	case GLITZ_PRIMITIVE_LINE_STRIP:
	    dst->geometry.u.v.prim = GLITZ_GL_LINE_STRIP;
	    break;
	case GLITZ_PRIMITIVE_LINE_LOOP:
	    dst->geometry.u.v.prim = GLITZ_GL_LINE_LOOP;
	    break;
	case GLITZ_PRIMITIVE_TRIANGLES:
	    dst->geometry.u.v.prim = GLITZ_GL_TRIANGLES;
	    break;
	case GLITZ_PRIMITIVE_TRIANGLE_STRIP:
	    dst->geometry.u.v.prim = GLITZ_GL_TRIANGLE_STRIP;
	    break;
	case GLITZ_PRIMITIVE_TRIANGLE_FAN:
	    dst->geometry.u.v.prim = GLITZ_GL_TRIANGLE_FAN;
	    break;
	case GLITZ_PRIMITIVE_QUADS:
	    dst->geometry.u.v.prim = GLITZ_GL_QUADS;
	    break;
	case GLITZ_PRIMITIVE_QUAD_STRIP:
	    dst->geometry.u.v.prim = GLITZ_GL_QUAD_STRIP;
	    break;
	default:
	    dst->geometry.u.v.prim = GLITZ_GL_POLYGON;
	    break;
	}

	dst->geometry.u.v.type = _glitz_data_type (format->vertex.type);
	dst->geometry.stride = format->vertex.bytes_per_vertex;
	dst->geometry.attributes = format->vertex.attributes;

	if (format->vertex.attributes & GLITZ_VERTEX_ATTRIBUTE_SRC_COORD_MASK)
	{
	    dst->geometry.u.v.src.type =
		_glitz_data_type (format->vertex.src.type);
	    dst->geometry.u.v.src.offset = format->vertex.src.offset;

	    if (format->vertex.src.size == GLITZ_COORDINATE_SIZE_XY)
		dst->geometry.u.v.src.size = 2;
	    else
		dst->geometry.u.v.src.size = 1;
	}

	if (format->vertex.attributes & GLITZ_VERTEX_ATTRIBUTE_MASK_COORD_MASK)
	{
	    dst->geometry.u.v.mask.type =
		_glitz_data_type (format->vertex.mask.type);
	    dst->geometry.u.v.mask.offset = format->vertex.mask.offset;

	    if (format->vertex.mask.size == GLITZ_COORDINATE_SIZE_XY)
		dst->geometry.u.v.mask.size = 2;
	    else
		dst->geometry.u.v.mask.size = 1;
	}
    } break;
    case GLITZ_GEOMETRY_TYPE_BITMAP:
	glitz_buffer_reference (buffer);
	if (dst->geometry.buffer)
	    glitz_buffer_destroy (dst->geometry.buffer);
	dst->geometry.buffer = buffer;

	dst->geometry.type = GLITZ_GEOMETRY_TYPE_BITMAP;

	if (format->bitmap.scanline_order ==
	    GLITZ_PIXEL_SCANLINE_ORDER_TOP_DOWN)
	    dst->geometry.u.b.top_down = 1;
	else
	    dst->geometry.u.b.top_down = 0;

	switch (format->bitmap.pad) {
	case 2:
	    dst->geometry.u.b.pad = 2;
	    break;
	case 4:
	    dst->geometry.u.b.pad = 4;
	    break;
	case 8:
	    dst->geometry.u.b.pad = 8;
	    break;
	default:
	    dst->geometry.u.b.pad = 1;
	    break;
	}

	dst->geometry.stride = format->bitmap.bytes_per_line;
	dst->geometry.attributes = 0;
	break;
    default:
	dst->geometry.type = GLITZ_GEOMETRY_TYPE_NONE;
	if (dst->geometry.buffer)
	    glitz_buffer_destroy (dst->geometry.buffer);

	dst->geometry.buffer = NULL;
	dst->geometry.attributes = 0;
	break;
    }
}
slim_hidden_def(glitz_set_geometry);

void
glitz_set_array (glitz_surface_t    *dst,
		 int                first,
		 int                size,
		 unsigned int       count,
		 glitz_fixed16_16_t x_off,
		 glitz_fixed16_16_t y_off)
{
    if (dst->geometry.array)
    {
	glitz_multi_array_destroy (dst->geometry.array);
	dst->geometry.array = NULL;
    }

    dst->geometry.first    = first;
    dst->geometry.size     = size;
    dst->geometry.count    = count;
    dst->geometry.off.v[0] = FIXED_TO_FLOAT (x_off);
    dst->geometry.off.v[1] = FIXED_TO_FLOAT (y_off);
}
slim_hidden_def(glitz_set_array);

glitz_multi_array_t *
glitz_multi_array_create (unsigned int size)
{
    glitz_multi_array_t *array;
    int                 alloc_size;

    if (!size)
	return NULL;

    alloc_size = sizeof (glitz_multi_array_t) + (sizeof (int) +
						 sizeof (int) +
						 sizeof (unsigned int) +
						 sizeof (glitz_vec2_t) +
						 sizeof (int)) * (size);

    array = (glitz_multi_array_t *) malloc (alloc_size);
    if (array == NULL)
	return NULL;

    array->ref_count = 1;
    array->size      = size;
    array->first     = (int *)          (array + 1);
    array->sizes     = (int *)          (array->first + size);
    array->count     = (int *)          (array->sizes + size);
    array->off       = (glitz_vec2_t *) (array->count + size);
    array->span      = (int *)          (array->off   + size);

    array->n_arrays = 0;

    return array;
}
slim_hidden_def(glitz_multi_array_create);

void
glitz_multi_array_destroy (glitz_multi_array_t *array)
{
    if (!array)
	return;

    array->ref_count--;
    if (array->ref_count)
	return;

    free (array);
}

void
glitz_multi_array_reference (glitz_multi_array_t *array)
{
    if (array == NULL)
	return;

    array->ref_count++;
}

void
glitz_multi_array_add (glitz_multi_array_t *array,
		       int                 first,
		       int                 size,
		       unsigned int        count,
		       glitz_fixed16_16_t  x_off,
		       glitz_fixed16_16_t  y_off)
{
    int i;

    if (array->size == array->n_arrays)
	return;

    i = array->n_arrays++;

    array->first[i] = first;
    array->sizes[i] = size;
    array->count[i] = count;
    array->span[i]  = 0;

    if (i == 0 || x_off || y_off)
    {
	array->off[i].v[0]  = FIXED_TO_FLOAT (x_off);
	array->off[i].v[1]  = FIXED_TO_FLOAT (y_off);
	array->current_span = &array->span[i];
    }

    (*array->current_span)++;
}
slim_hidden_def(glitz_multi_array_add);

void
glitz_multi_array_reset (glitz_multi_array_t *array)
{
    array->n_arrays = 0;
}
slim_hidden_def(glitz_multi_array_reset);

void
glitz_set_multi_array (glitz_surface_t     *dst,
		       glitz_multi_array_t *array,
		       glitz_fixed16_16_t  x_off,
		       glitz_fixed16_16_t  y_off)
{
    glitz_multi_array_reference (array);

    if (dst->geometry.array)
	glitz_multi_array_destroy (dst->geometry.array);

    dst->geometry.array = array;
    dst->geometry.count = array->n_arrays;
    dst->geometry.off.v[0] = FIXED_TO_FLOAT (x_off);
    dst->geometry.off.v[1] = FIXED_TO_FLOAT (y_off);
}
slim_hidden_def(glitz_set_multi_array);

void
glitz_geometry_enable_none (glitz_gl_proc_address_list_t *gl,
			    glitz_surface_t              *dst,
			    glitz_box_t                  *box)
{
    dst->geometry.data[0] = (glitz_float_t) box->x1;
    dst->geometry.data[1] = (glitz_float_t) box->y1;
    dst->geometry.data[2] = (glitz_float_t) box->x2;
    dst->geometry.data[3] = (glitz_float_t) box->y1;
    dst->geometry.data[4] = (glitz_float_t) box->x2;
    dst->geometry.data[5] = (glitz_float_t) box->y2;
    dst->geometry.data[6] = (glitz_float_t) box->x1;
    dst->geometry.data[7] = (glitz_float_t) box->y2;

    gl->vertex_pointer (2, GLITZ_GL_FLOAT, 0, dst->geometry.data);
}

void
glitz_geometry_enable (glitz_gl_proc_address_list_t *gl,
		       glitz_surface_t              *dst,
		       glitz_box_t                  *box)
{
    switch (dst->geometry.type) {
    case GLITZ_GEOMETRY_TYPE_VERTEX:
	gl->vertex_pointer (2, dst->geometry.u.v.type, dst->geometry.stride,
			    glitz_buffer_bind (dst->geometry.buffer,
					       GLITZ_GL_ARRAY_BUFFER));
	break;
    case GLITZ_GEOMETRY_TYPE_BITMAP:
	dst->geometry.u.b.base =
	    glitz_buffer_bind (dst->geometry.buffer,
			       GLITZ_GL_PIXEL_UNPACK_BUFFER);

	gl->pixel_store_i (GLITZ_GL_UNPACK_SKIP_PIXELS, 0);
	gl->pixel_store_i (GLITZ_GL_UNPACK_SKIP_ROWS, 0);

#if BITMAP_BIT_ORDER == MSBFirst
	gl->pixel_store_i (GLITZ_GL_UNPACK_LSB_FIRST, GLITZ_GL_FALSE);
#else
	gl->pixel_store_i (GLITZ_GL_UNPACK_LSB_FIRST, GLITZ_GL_TRUE);
#endif

	break;
    case GLITZ_GEOMETRY_TYPE_NONE:
	glitz_geometry_enable_none (gl, dst, box);
    }
}

void
glitz_geometry_disable (glitz_surface_t *dst)
{
    if (dst->geometry.buffer)
	glitz_buffer_unbind (dst->geometry.buffer);
}

static void
_glitz_draw_rectangle (glitz_gl_proc_address_list_t *gl,
		       glitz_surface_t              *dst,
		       glitz_box_t                  *bounds,
		       int                          damage)
{
    glitz_box_t *clip = dst->clip;
    int         n_clip = dst->n_clip;
    glitz_box_t box;

    while (n_clip--)
    {
	box.x1 = clip->x1 + dst->x_clip;
	box.y1 = clip->y1 + dst->y_clip;
	box.x2 = clip->x2 + dst->x_clip;
	box.y2 = clip->y2 + dst->y_clip;
	if (bounds->x1 > box.x1)
	    box.x1 = bounds->x1;
	if (bounds->y1 > box.y1)
	    box.y1 = bounds->y1;
	if (bounds->x2 < box.x2)
	    box.x2 = bounds->x2;
	if (bounds->y2 < box.y2)
	    box.y2 = bounds->y2;

	if (box.x1 < box.x2 && box.y1 < box.y2)
	{
	    gl->scissor (box.x1 + dst->x,
			 dst->attached->height - dst->y - box.y2,
			 box.x2 - box.x1, box.y2 - box.y1);

	    gl->draw_arrays (GLITZ_GL_QUADS, 0, 4);

	    if (damage)
		glitz_surface_damage (dst, &box, damage);
	}

	clip++;
    }
}

#define MULTI_DRAW_ARRAYS(surface)                      \
    ((surface)->drawable->backend->feature_mask &       \
     GLITZ_FEATURE_MULTI_DRAW_ARRAYS_MASK)

static void
_glitz_draw_vertex_arrays (glitz_gl_proc_address_list_t *gl,
			   glitz_surface_t              *dst,
			   glitz_box_t                  *bounds,
			   int                          damage)
{
    glitz_multi_array_t *array = dst->geometry.array;
    glitz_box_t         *clip = dst->clip;
    int                 i, n_clip = dst->n_clip;
    glitz_box_t         box;

    while (n_clip--)
    {
	box.x1 = clip->x1 + dst->x_clip;
	box.y1 = clip->y1 + dst->y_clip;
	box.x2 = clip->x2 + dst->x_clip;
	box.y2 = clip->y2 + dst->y_clip;
	if (bounds->x1 > box.x1)
	    box.x1 = bounds->x1;
	if (bounds->y1 > box.y1)
	    box.y1 = bounds->y1;
	if (bounds->x2 < box.x2)
	    box.x2 = bounds->x2;
	if (bounds->y2 < box.y2)
	    box.y2 = bounds->y2;

	if (box.x1 < box.x2 && box.y1 < box.y2)
	{
	    gl->scissor (box.x1 + dst->x,
			 dst->attached->height - dst->y - box.y2,
			 box.x2 - box.x1, box.y2 - box.y1);

	    gl->push_matrix ();

	    if (dst->geometry.off.v[0] || dst->geometry.off.v[1])
		gl->translate_f (dst->geometry.off.v[0],
				 dst->geometry.off.v[1], 0.0f);

	    if (array)
	    {
		for (i = 0; i < array->n_arrays;)
		{
		    gl->translate_f (array->off[i].v[0],
				     array->off[i].v[1], 0.0f);

		    if (MULTI_DRAW_ARRAYS (dst))
		    {
			gl->multi_draw_arrays (dst->geometry.u.v.prim,
					       &array->first[i],
					       &array->count[i],
					       array->span[i]);
			i += array->span[i];
		    }
		    else
		    {
			do {
			    if (array->count[i])
				gl->draw_arrays (dst->geometry.u.v.prim,
						 array->first[i],
						 array->count[i]);

			} while (array->span[++i] == 0);
		    }
		}
	    } else
		gl->draw_arrays (dst->geometry.u.v.prim,
				 dst->geometry.first,
				 dst->geometry.count);

	    gl->pop_matrix ();

	    if (damage)
		glitz_surface_damage (dst, &box, damage);
	}

	clip++;
    }
}

#define N_STACK_BITMAP 128

#define BITMAP_PAD 4
#define BITMAP_STRIDE(x, w, a)                                  \
    (((((x) & 3) + (w) + (((a) << 3) - 1)) / ((a) << 3)) * (a))

#define BITMAP_SETUP(dst, first, size, count, _w, _h, _b_off, _p_off)   \
    (_w)     = (size);                                                  \
    (_h)     = (count);                                                 \
    (_b_off) = (first) >> 3;                                            \
    if ((_p_off) != ((first) & 3))                                      \
    {                                                                   \
	(_p_off) = (first) & 3;                                         \
	gl->pixel_store_i (GLITZ_GL_UNPACK_SKIP_PIXELS, _p_off);        \
    }                                                                   \
    if ((dst)->geometry.u.b.top_down)                                   \
    {                                                                   \
	dst_stride = BITMAP_STRIDE ((first), _w, BITMAP_PAD);           \
	if ((dst)->geometry.stride)                                     \
	    src_stride = (dst)->geometry.stride;                        \
	else                                                            \
	    src_stride = BITMAP_STRIDE ((first), _w,                    \
					(dst)->geometry.u.b.pad);       \
	min_stride = MIN (src_stride, dst_stride);                      \
	base = (dst)->geometry.u.b.base + (_b_off);                     \
	y = (_h);                                                       \
	while (y--)                                                     \
	    memcpy (bitmap + ((_h) - 1 - y) * dst_stride,               \
		    base + y * src_stride,                              \
		    min_stride);                                        \
	(_b_off) = 0;                                                   \
    }

/* TODO: clipping could be done without glScissor, that might be
   faster. Other then solid colors can be used if bitmap fits into a
   stipple pattern. Maybe we should add a repeat parameter to
   glitz_bitmap_format_t as 2, 4, 8, 16 and 32 sized bitmaps can be tiled.
*/
static void
_glitz_draw_bitmap_arrays (glitz_gl_proc_address_list_t *gl,
			   glitz_surface_t              *dst,
			   glitz_box_t                  *bounds,
			   int                          damage)
{
    glitz_multi_array_t *array = dst->geometry.array;
    glitz_box_t         *clip = dst->clip;
    int                 n, i, n_clip = dst->n_clip;
    int                 x, y, w, h, min_stride, dst_stride, src_stride;
    glitz_gl_ubyte_t    *heap_bitmap = NULL;
    glitz_gl_ubyte_t    stack_bitmap[N_STACK_BITMAP];
    glitz_gl_ubyte_t    *base, *bitmap = dst->geometry.u.b.base;
    int                 byte_offset, pixel_offset = 0;
    glitz_float_t       x_off, y_off;
    glitz_box_t         box;

    if (dst->geometry.u.b.top_down)
    {
	int max_size = 0;

	if (array)
	{
	    int size;

	    for (i = 0, n = array->n_arrays; n--; i++)
	    {
		x = array->first[i];
		w = array->sizes[i];
		size = BITMAP_STRIDE (x, w, BITMAP_PAD) * array->count[i];
		if (size > max_size)
		    max_size = size;
	    }
	}
	else
	{
	    x = dst->geometry.first;
	    w = dst->geometry.size;
	    max_size = BITMAP_STRIDE (x, w, BITMAP_PAD) * dst->geometry.count;
	}

	if (max_size > N_STACK_BITMAP)
	{
	    heap_bitmap = malloc (max_size);
	    if (!heap_bitmap)
	    {
		glitz_surface_status_add (dst, GLITZ_STATUS_NO_MEMORY_MASK);
		return;
	    }
	    bitmap = heap_bitmap;
	} else
	    bitmap = stack_bitmap;

	gl->pixel_store_i (GLITZ_GL_UNPACK_ALIGNMENT, BITMAP_PAD);
	gl->pixel_store_i (GLITZ_GL_UNPACK_ROW_LENGTH, 0);
    }
    else
    {
	gl->pixel_store_i (GLITZ_GL_UNPACK_ALIGNMENT, dst->geometry.u.b.pad);
	gl->pixel_store_i (GLITZ_GL_UNPACK_ROW_LENGTH,
			   dst->geometry.stride * 8);
    }

    while (n_clip--)
    {
	box.x1 = clip->x1 + dst->x_clip;
	box.y1 = clip->y1 + dst->y_clip;
	box.x2 = clip->x2 + dst->x_clip;
	box.y2 = clip->y2 + dst->y_clip;
	if (bounds->x1 > box.x1)
	    box.x1 = bounds->x1;
	if (bounds->y1 > box.y1)
	    box.y1 = bounds->y1;
	if (bounds->x2 < box.x2)
	    box.x2 = bounds->x2;
	if (bounds->y2 < box.y2)
	    box.y2 = bounds->y2;

	if (box.x1 < box.x2 && box.y1 < box.y2)
	{
	    gl->scissor (box.x1 + dst->x,
			 dst->attached->height - dst->y - box.y2,
			 box.x2 - box.x1, box.y2 - box.y1);

	    x_off = dst->x + dst->geometry.off.v[0];
	    y_off = dst->y + dst->geometry.off.v[1];

	    if (array)
	    {
		x_off += array->off->v[0];
		y_off += array->off->v[1];

		glitz_set_raster_pos (gl, x_off,
				      dst->attached->height - y_off);

		for (i = 0, n = array->n_arrays; n--; i++)
		{
		    if (n)
		    {
			x_off = array->off[i + 1].v[0];
			y_off = array->off[i + 1].v[1];
		    }

		    BITMAP_SETUP (dst,
				  array->first[i],
				  array->sizes[i],
				  array->count[i],
				  w, h, byte_offset, pixel_offset);

		    gl->bitmap (w, h,
				0.0f, (glitz_gl_float_t) array->count[i],
				x_off, -y_off,
				bitmap + byte_offset);
		}
	    }
	    else
	    {
		glitz_set_raster_pos (gl, x_off,
				      dst->attached->height - y_off);

		BITMAP_SETUP (dst,
			      dst->geometry.first,
			      dst->geometry.size,
			      dst->geometry.count,
			      w, h, byte_offset, pixel_offset);

		gl->bitmap (w, h,
			    0.0f, (glitz_gl_float_t) dst->geometry.count,
			    0.0f, 0.0f,
			    bitmap + byte_offset);
	    }

	    if (damage)
		glitz_surface_damage (dst, &box, damage);
	}

	clip++;
    }

    if (heap_bitmap)
	free (heap_bitmap);
}

void
glitz_geometry_draw_arrays (glitz_gl_proc_address_list_t *gl,
			    glitz_surface_t              *dst,
			    glitz_geometry_type_t        type,
			    glitz_box_t                  *bounds,
			    int                          damage)
{
    switch (type) {
    case GLITZ_GEOMETRY_TYPE_VERTEX:
	_glitz_draw_vertex_arrays (gl, dst, bounds, damage);
	break;
    case GLITZ_GEOMETRY_TYPE_BITMAP:
	_glitz_draw_bitmap_arrays (gl, dst, bounds, damage);
	break;
    case GLITZ_GEOMETRY_TYPE_NONE:
	_glitz_draw_rectangle (gl, dst, bounds, damage);
	break;
    }
}
