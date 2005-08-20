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
                    unsigned long   feature_mask,
                    glitz_bool_t    unnormalized)
{
    texture->filter = 0;
    texture->wrap = 0;
    texture->format = texture_format;
    texture->name = 0;

    if (feature_mask & GLITZ_FEATURE_TEXTURE_BORDER_CLAMP_MASK)
    {
        texture->box.x1 = texture->box.y1 = 0;
        texture->box.x2 = texture->width = width;
        texture->box.y2 = texture->height = height;
        texture->flags = GLITZ_TEXTURE_FLAG_REPEATABLE_MASK |
            GLITZ_TEXTURE_FLAG_PADABLE_MASK;
    }
    else
    {
        texture->box.x1 = texture->box.y1 = 1;
        texture->box.x2 = width + 1;
        texture->box.y2 = height + 1;
        texture->width = width + 2;
        texture->height = height + 2;
        texture->flags = 0;
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
                          glitz_gl_int_t               max_rect) {
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

  if (texture->box.x2 != texture->width ||
      texture->box.y2 != texture->height) {
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
                       GLITZ_GL_NEAREST);
  gl->tex_parameter_i (texture->target,
                       GLITZ_GL_TEXTURE_MIN_FILTER,
                       GLITZ_GL_NEAREST);
  
  texture->filter = GLITZ_GL_NEAREST;

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
glitz_texture_ensure_filter (glitz_gl_proc_address_list_t *gl,
                             glitz_texture_t              *texture,
                             glitz_gl_enum_t              filter)
{
  if (!texture->name)
    return;
    
  if (texture->filter != filter) {
    gl->tex_parameter_i (texture->target, GLITZ_GL_TEXTURE_MAG_FILTER, filter);
    gl->tex_parameter_i (texture->target, GLITZ_GL_TEXTURE_MIN_FILTER, filter);
    texture->filter = filter;
  }
}

void
glitz_texture_ensure_wrap (glitz_gl_proc_address_list_t *gl,
                           glitz_texture_t              *texture,
                           glitz_gl_enum_t              wrap)
{
  if (!texture->name)
    return;
  
  if (texture->wrap != wrap) {
    gl->tex_parameter_i (texture->target, GLITZ_GL_TEXTURE_WRAP_S, wrap);
    gl->tex_parameter_i (texture->target, GLITZ_GL_TEXTURE_WRAP_T, wrap);
    texture->wrap = wrap;
  }
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
