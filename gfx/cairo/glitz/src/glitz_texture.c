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
glitz_texture_init (glitz_texture_t *texture,
		    int             width,
		    int             height,
		    glitz_gl_int_t  texture_format,
		    glitz_fourcc_t  fourcc,
		    unsigned long   feature_mask,
		    glitz_bool_t    unnormalized)
{
    texture->param.filter[0] = texture->param.filter[1] = GLITZ_GL_NEAREST;
    texture->param.wrap[0] = texture->param.wrap[1] = GLITZ_GL_CLAMP;
    texture->param.border_color.red = texture->param.border_color.green =
	texture->param.border_color.blue =
	texture->param.border_color.alpha = 0;

    texture->format = texture_format;
    texture->fourcc = fourcc;
    texture->name   = 0;

    switch (fourcc) {
    case GLITZ_FOURCC_YV12:
	/* U and V plane added below */
	texture->box.x1 = texture->box.y1 = 0;
	texture->box.x2 = width;
	texture->box.y2 = height;
	texture->width  = (width + 1) & ~1;
	texture->height = (height + 1) & ~1;
	texture->height += texture->height >> 1;
	texture->flags  = GLITZ_TEXTURE_FLAG_PADABLE_MASK;
	break;
    case GLITZ_FOURCC_YUY2:
	/* 1 RGBA texel for 2 YUY2 pixels */
	texture->box.x1 = texture->box.y1 = 0;
	texture->box.x2 = texture->width  = width >> 1;
	texture->box.y2 = texture->height = height;
	texture->flags  = GLITZ_TEXTURE_FLAG_CLAMPABLE_MASK |
	    GLITZ_TEXTURE_FLAG_REPEATABLE_MASK |
	    GLITZ_TEXTURE_FLAG_PADABLE_MASK;
	break;
    default:
	texture->box.x1 = texture->box.y1 = 0;
	texture->box.x2 = texture->width = width;
	texture->box.y2 = texture->height = height;

	texture->flags = GLITZ_TEXTURE_FLAG_PADABLE_MASK |
	    GLITZ_TEXTURE_FLAG_REPEATABLE_MASK;

	if (feature_mask & GLITZ_FEATURE_TEXTURE_BORDER_CLAMP_MASK)
	    texture->flags |= GLITZ_TEXTURE_FLAG_CLAMPABLE_MASK;
    }

    if (!unnormalized &&
	((feature_mask & GLITZ_FEATURE_TEXTURE_NON_POWER_OF_TWO_MASK) ||
	 (POWER_OF_TWO (texture->width) && POWER_OF_TWO (texture->height))))
    {
	texture->target = GLITZ_GL_TEXTURE_2D;
    }
    else
    {
	texture->flags &= ~GLITZ_TEXTURE_FLAG_REPEATABLE_MASK;

	if (feature_mask & GLITZ_FEATURE_TEXTURE_RECTANGLE_MASK)
	{
	    texture->target = GLITZ_GL_TEXTURE_RECTANGLE;
	}
	else
	{
	    texture->target = GLITZ_GL_TEXTURE_2D;
	    texture->flags &= ~GLITZ_TEXTURE_FLAG_PADABLE_MASK;

	    if (!POWER_OF_TWO (texture->width))
		texture->width = glitz_uint_to_power_of_two (texture->width);

	    if (!POWER_OF_TWO (texture->height))
		texture->height = glitz_uint_to_power_of_two (texture->height);
	}
    }

    if (texture->target == GLITZ_GL_TEXTURE_2D)
    {
	texture->texcoord_width_unit = 1.0f / texture->width;
	texture->texcoord_height_unit = 1.0f / texture->height;
    }
    else
    {
	texture->texcoord_width_unit = 1.0f;
	texture->texcoord_height_unit = 1.0f;
    }
}

void
glitz_texture_size_check (glitz_gl_proc_address_list_t *gl,
			  glitz_texture_t              *texture,
			  glitz_gl_int_t               max_2d,
			  glitz_gl_int_t               max_rect)
{
    glitz_gl_enum_t proxy_target;
    glitz_gl_int_t value, max;

    if (texture->target == GLITZ_GL_TEXTURE_2D) {
	max = max_2d;
	proxy_target = GLITZ_GL_PROXY_TEXTURE_2D;
    } else {
	max = max_rect;
	proxy_target = GLITZ_GL_PROXY_TEXTURE_RECTANGLE;
    }

    if (texture->width > max || texture->height > max) {
	texture->flags |= GLITZ_TEXTURE_FLAG_INVALID_SIZE_MASK;
	return;
    }

    gl->tex_image_2d (proxy_target, 0,
		      texture->format, texture->width, texture->height,
		      0, GLITZ_GL_RGBA, GLITZ_GL_UNSIGNED_BYTE, NULL);
    gl->get_tex_level_parameter_iv (proxy_target, 0,
				    GLITZ_GL_TEXTURE_WIDTH, &value);
    if (value != texture->width) {
	texture->flags |= GLITZ_TEXTURE_FLAG_INVALID_SIZE_MASK;
	return;
    }

    gl->get_tex_level_parameter_iv (proxy_target, 0,
				    GLITZ_GL_TEXTURE_HEIGHT, &value);
    if (value != texture->height)
	texture->flags |= GLITZ_TEXTURE_FLAG_INVALID_SIZE_MASK;
}

void
glitz_texture_allocate (glitz_gl_proc_address_list_t *gl,
			glitz_texture_t              *texture)
{
    char *data = NULL;

    if (!texture->name)
	gl->gen_textures (1, &texture->name);

    texture->flags |= GLITZ_TEXTURE_FLAG_ALLOCATED_MASK;

    glitz_texture_bind (gl, texture);

    if (TEXTURE_CLAMPABLE (texture) &&
	(texture->box.x2 > texture->width ||
	 texture->box.y2 > texture->height))
    {
	data = malloc (texture->width * texture->height);
	if (data)
	    memset (data, 0, texture->width * texture->height);

	gl->pixel_store_i (GLITZ_GL_UNPACK_ROW_LENGTH, 0);
	gl->pixel_store_i (GLITZ_GL_UNPACK_ROW_LENGTH, 0);
	gl->pixel_store_i (GLITZ_GL_UNPACK_SKIP_ROWS, 0);
	gl->pixel_store_i (GLITZ_GL_UNPACK_SKIP_PIXELS, 0);
	gl->pixel_store_i (GLITZ_GL_UNPACK_ALIGNMENT, 1);
    }

    gl->tex_image_2d (texture->target, 0, texture->format,
		      texture->width, texture->height, 0,
		      GLITZ_GL_ALPHA, GLITZ_GL_UNSIGNED_BYTE, data);

    gl->tex_parameter_i (texture->target,
			 GLITZ_GL_TEXTURE_MAG_FILTER,
			 texture->param.filter[0]);
    gl->tex_parameter_i (texture->target,
			 GLITZ_GL_TEXTURE_MIN_FILTER,
			 texture->param.filter[1]);

    glitz_texture_unbind (gl, texture);

    if (data)
	free (data);
}

void
glitz_texture_fini (glitz_gl_proc_address_list_t *gl,
		    glitz_texture_t              *texture)
{
    if (texture->name)
	gl->delete_textures (1, &texture->name);
}

void
glitz_texture_bind (glitz_gl_proc_address_list_t *gl,
		    glitz_texture_t              *texture)
{
    gl->disable (GLITZ_GL_TEXTURE_RECTANGLE);
    gl->disable (GLITZ_GL_TEXTURE_2D);

    if (!texture->name)
	return;

    gl->enable (texture->target);
    gl->bind_texture (texture->target, texture->name);
}

void
glitz_texture_unbind (glitz_gl_proc_address_list_t *gl,
		      glitz_texture_t              *texture)
{
    gl->bind_texture (texture->target, 0);
    gl->disable (texture->target);
}

void
glitz_texture_copy_drawable (glitz_gl_proc_address_list_t *gl,
			     glitz_texture_t              *texture,
			     glitz_drawable_t             *drawable,
			     int                          x_drawable,
			     int                          y_drawable,
			     int                          width,
			     int                          height,
			     int                          x_texture,
			     int                          y_texture)
{
    gl->copy_tex_sub_image_2d (texture->target, 0,
			       texture->box.x1 + x_texture,
			       texture->box.y2 - y_texture - height,
			       x_drawable,
			       drawable->height - y_drawable - height,
			       width, height);
}

void
glitz_texture_set_tex_gen (glitz_gl_proc_address_list_t *gl,
			   glitz_texture_t              *texture,
			   glitz_geometry_t             *geometry,
			   int                          x_src,
			   int                          y_src,
			   unsigned long                flags,
			   glitz_int_coordinate_t       *coord)
{
    glitz_vec4_t plane;

    if (flags & GLITZ_SURFACE_FLAG_GEN_S_COORDS_MASK)
    {
	plane.v[1] = plane.v[2] = 0.0f;

	if (flags & GLITZ_SURFACE_FLAG_EYE_COORDS_MASK)
	{
	    plane.v[0] = 1.0f;
	    plane.v[3] = -x_src;
	}
	else
	{
	    plane.v[0] = texture->texcoord_width_unit;

	    if (flags & GLITZ_SURFACE_FLAG_TRANSFORM_MASK)
		plane.v[3] = -(x_src) * texture->texcoord_width_unit;
	    else
		plane.v[3] = -(x_src - texture->box.x1) *
		    texture->texcoord_width_unit;
	}

	gl->tex_gen_i (GLITZ_GL_S, GLITZ_GL_TEXTURE_GEN_MODE,
		       GLITZ_GL_EYE_LINEAR);
	gl->tex_gen_fv (GLITZ_GL_S, GLITZ_GL_EYE_PLANE, plane.v);

	gl->enable (GLITZ_GL_TEXTURE_GEN_S);
    }
    else
	gl->disable (GLITZ_GL_TEXTURE_GEN_S);

    if (flags & GLITZ_SURFACE_FLAG_GEN_T_COORDS_MASK)
    {
	plane.v[0] = plane.v[2] = 0.0f;

	if (flags & GLITZ_SURFACE_FLAG_EYE_COORDS_MASK)
	{
	    plane.v[1] = 1.0f;
	    plane.v[3] = -y_src;
	}
	else
	{
	    plane.v[1] = -texture->texcoord_height_unit;
	    if (flags & GLITZ_SURFACE_FLAG_TRANSFORM_MASK)
		plane.v[3] = (y_src + texture->box.y2 - texture->box.y1) *
		    texture->texcoord_height_unit;
	    else
		plane.v[3] = (y_src + texture->box.y2) *
		    texture->texcoord_height_unit;
	}

	gl->tex_gen_i (GLITZ_GL_T, GLITZ_GL_TEXTURE_GEN_MODE,
		       GLITZ_GL_EYE_LINEAR);
	gl->tex_gen_fv (GLITZ_GL_T, GLITZ_GL_EYE_PLANE, plane.v);

	gl->enable (GLITZ_GL_TEXTURE_GEN_T);
    }
    else
	gl->disable (GLITZ_GL_TEXTURE_GEN_T);

    if (!(flags & GLITZ_SURFACE_FLAG_GEN_S_COORDS_MASK))
    {
	unsigned char *ptr;

	gl->enable_client_state (GLITZ_GL_TEXTURE_COORD_ARRAY);

	ptr = glitz_buffer_bind (geometry->buffer, GLITZ_GL_ARRAY_BUFFER);
	ptr += coord->offset;

	gl->tex_coord_pointer (coord->size,
			       coord->type,
			       geometry->stride,
			       (void *) ptr);
    } else
	gl->disable_client_state (GLITZ_GL_TEXTURE_COORD_ARRAY);
}

glitz_texture_object_t *
glitz_texture_object_create (glitz_surface_t *surface)
{
    glitz_texture_object_t *texture;

    GLITZ_GL_SURFACE (surface);

    /* texture dimensions must match surface dimensions */
    if (surface->texture.width  != surface->box.x2 &&
	surface->texture.height != surface->box.y2)
	return 0;

    texture = malloc (sizeof (glitz_texture_object_t));
    if (!texture)
	return 0;

    texture->ref_count = 1;

    glitz_surface_reference (surface);
    texture->surface = surface;

    if (!(TEXTURE_ALLOCATED (&surface->texture)))
	glitz_texture_allocate (gl, &surface->texture);

    texture->param = surface->texture.param;

    return texture;
}

void
glitz_texture_object_destroy (glitz_texture_object_t *texture)
{
    texture->ref_count--;
    if (texture->ref_count)
	return;

    glitz_surface_destroy (texture->surface);
    free (texture);
}

void
glitz_texture_object_reference (glitz_texture_object_t *texture)
{
    texture->ref_count++;
}

void
glitz_texture_object_set_filter (glitz_texture_object_t      *texture,
				 glitz_texture_filter_type_t type,
				 glitz_texture_filter_t      filter)
{
    static glitz_gl_enum_t filters[2][6] = {
	{
	    GLITZ_GL_NEAREST,
	    GLITZ_GL_LINEAR,
	    GLITZ_GL_NEAREST,
	    GLITZ_GL_LINEAR,
	    GLITZ_GL_NEAREST,
	    GLITZ_GL_LINEAR
	}, {
	    GLITZ_GL_NEAREST,
	    GLITZ_GL_LINEAR,
	    GLITZ_GL_NEAREST_MIPMAP_NEAREST,
	    GLITZ_GL_LINEAR_MIPMAP_NEAREST,
	    GLITZ_GL_NEAREST_MIPMAP_LINEAR,
	    GLITZ_GL_LINEAR_MIPMAP_LINEAR
	}
    };

    texture->param.filter[type] = filters[type][filter];
}

void
glitz_texture_object_set_wrap (glitz_texture_object_t    *texture,
			       glitz_texture_wrap_type_t type,
			       glitz_texture_wrap_t      wrap)
{
    static glitz_gl_enum_t wraps[] = {
	GLITZ_GL_CLAMP,
	GLITZ_GL_CLAMP_TO_EDGE,
	GLITZ_GL_CLAMP_TO_BORDER,
	GLITZ_GL_REPEAT,
	GLITZ_GL_MIRRORED_REPEAT
    };

    texture->param.wrap[type] = wraps[wrap];
}

void
glitz_texture_object_set_border_color (glitz_texture_object_t *texture,
				       glitz_color_t          *color)
{
    texture->param.border_color = *color;
}

void
glitz_texture_ensure_parameters (glitz_gl_proc_address_list_t *gl,
				 glitz_texture_t	      *texture,
				 glitz_texture_parameters_t   *param)
{
    static const glitz_gl_enum_t filters[] = {
	GLITZ_GL_TEXTURE_MAG_FILTER,
	GLITZ_GL_TEXTURE_MIN_FILTER
    };
    static const glitz_gl_enum_t wraps[] = {
	GLITZ_GL_TEXTURE_WRAP_S,
	GLITZ_GL_TEXTURE_WRAP_T
    };
    int	i;

    if (!texture->name)
	return;

    for (i = 0; i < 2; i++)
    {
	if (texture->param.filter[i] != param->filter[i])
	{
	    texture->param.filter[i] = param->filter[i];
	    gl->tex_parameter_i (texture->target, filters[i],
				 param->filter[i]);
	}

	if (texture->param.wrap[i] != param->wrap[i])
	{
	    texture->param.wrap[i] = param->wrap[i];
	    gl->tex_parameter_i (texture->target, wraps[i], param->wrap[i]);
	}
    }

    if (texture->param.wrap[0] == GLITZ_GL_CLAMP_TO_BORDER ||
	texture->param.wrap[1] == GLITZ_GL_CLAMP_TO_BORDER ||
	texture->param.wrap[0] == GLITZ_GL_CLAMP ||
	texture->param.wrap[1] == GLITZ_GL_CLAMP)
    {
	if (memcmp (&texture->param.border_color, &param->border_color,
		    sizeof (glitz_color_t)))
	{
	    glitz_vec4_t vec;

	    vec.v[0] = (glitz_float_t) param->border_color.red / 0xffff;
	    vec.v[1] = (glitz_float_t) param->border_color.green / 0xffff;
	    vec.v[2] = (glitz_float_t) param->border_color.blue / 0xffff;
	    vec.v[3] = (glitz_float_t) param->border_color.alpha / 0xffff;

	    gl->tex_parameter_fv (texture->target,
				  GLITZ_GL_TEXTURE_BORDER_COLOR,
				  vec.v);

	    texture->param.border_color = param->border_color;
	}
    }
}

glitz_texture_target_t
glitz_texture_object_get_target (glitz_texture_object_t *texture)
{
    if (texture->surface->texture.target == GLITZ_GL_TEXTURE_2D)
	return GLITZ_TEXTURE_TARGET_2D;

    return GLITZ_TEXTURE_TARGET_RECT;
}
