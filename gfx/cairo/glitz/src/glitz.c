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

static glitz_gl_uint_t _texture_units[] = {
    GLITZ_GL_TEXTURE0,
    GLITZ_GL_TEXTURE1,
    GLITZ_GL_TEXTURE2
};

typedef struct _glitz_texture_unit_t {
  glitz_texture_t *texture;
  glitz_gl_uint_t unit;
  glitz_bool_t    transform;
} glitz_texture_unit_t;

void
glitz_composite (glitz_operator_t op,
                 glitz_surface_t *src,
                 glitz_surface_t *mask,
                 glitz_surface_t *dst,
                 int             x_src,
                 int             y_src,
                 int             x_mask,
                 int             y_mask,
                 int             x_dst,
                 int             y_dst,
                 int             width,
                 int             height)
{
    glitz_composite_op_t comp_op;
    int                  i, texture_nr = -1;
    glitz_texture_t      *stexture, *mtexture;
    glitz_texture_unit_t textures[3];
    glitz_box_t          bounds;
    glitz_bool_t         no_border_clamp;
    unsigned long        flags;

    GLITZ_GL_SURFACE (dst);

    bounds.x1 = MAX (x_dst, 0);
    bounds.y1 = MAX (y_dst, 0);
    bounds.x2 = x_dst + width;
    bounds.y2 = y_dst + height;
    
    if (bounds.x2 > dst->box.x2)
        bounds.x2 = dst->box.x2;
    if (bounds.y2 > dst->box.y2)
        bounds.y2 = dst->box.y2;

    if (bounds.x1 >= bounds.x2 || bounds.y1 >= bounds.y2)
        return;

    if (dst->geometry.buffer && (!dst->geometry.count))
        return;
  
    glitz_composite_op_init (&comp_op, op, src, mask, dst);
    if (comp_op.type == GLITZ_COMBINE_TYPE_NA)
    {
        glitz_surface_status_add (dst, GLITZ_STATUS_NOT_SUPPORTED_MASK);
        return;
    }

    src = comp_op.src;
    mask = comp_op.mask;

    if (src)
    {
        stexture = glitz_surface_get_texture (src, 0);
        if (!stexture)
            return;
    } else
        stexture = NULL;

    if (mask)
    {
        mtexture = glitz_surface_get_texture (mask, 0);
        if (!mtexture)
            return;
    } else
        mtexture = NULL;

    if (!glitz_surface_push_current (dst, GLITZ_DRAWABLE_CURRENT))
    {
        glitz_surface_status_add (dst, GLITZ_STATUS_NOT_SUPPORTED_MASK);
        glitz_surface_pop_current (dst);
        return;
    }
    
    no_border_clamp = !(dst->drawable->backend->feature_mask &
                        GLITZ_FEATURE_TEXTURE_BORDER_CLAMP_MASK);

    if (mtexture)
    {
        textures[0].texture = mtexture;
        textures[0].unit = _texture_units[0];
        textures[0].transform = 0;
        texture_nr = 0;

        glitz_texture_bind (gl, mtexture);

        flags = mask->flags | GLITZ_SURFACE_FLAGS_GEN_COORDS_MASK;
        if (dst->geometry.attributes & GLITZ_VERTEX_ATTRIBUTE_MASK_COORD_MASK)
        {
            flags &= ~GLITZ_SURFACE_FLAG_GEN_S_COORDS_MASK;
        
            if (dst->geometry.u.v.mask.size == 2)
                flags &= ~GLITZ_SURFACE_FLAG_GEN_T_COORDS_MASK;
        }

        glitz_texture_set_tex_gen (gl,
                                   mtexture,
                                   &dst->geometry,
                                   x_dst - x_mask,
                                   y_dst - y_mask,
                                   flags,
                                   &dst->geometry.u.v.mask);
    
        if (mask->transform)
        {
            textures[0].transform = 1;
            gl->matrix_mode (GLITZ_GL_TEXTURE);
            gl->load_matrix_f (SURFACE_EYE_COORDS (mask)?
                               mask->transform->m: mask->transform->t);
            gl->matrix_mode (GLITZ_GL_MODELVIEW);
      
            if (SURFACE_LINEAR_TRANSFORM_FILTER (mask))
                glitz_texture_ensure_filter (gl, mtexture, GLITZ_GL_LINEAR);
            else
                glitz_texture_ensure_filter (gl, mtexture, GLITZ_GL_NEAREST);
        }
        else
        {
            if ((dst->geometry.attributes &
                 GLITZ_VERTEX_ATTRIBUTE_MASK_COORD_MASK) &&
                SURFACE_LINEAR_TRANSFORM_FILTER (mask))
                glitz_texture_ensure_filter (gl, mtexture, GLITZ_GL_LINEAR);
            else
                glitz_texture_ensure_filter (gl, mtexture, GLITZ_GL_NEAREST);
        }
    
        if (SURFACE_REPEAT (mask))
        {
            if (SURFACE_MIRRORED (mask))
                glitz_texture_ensure_wrap (gl, mtexture,
                                           GLITZ_GL_MIRRORED_REPEAT);
            else
                glitz_texture_ensure_wrap (gl, mtexture, GLITZ_GL_REPEAT);
        }
        else
        {
            if (no_border_clamp || SURFACE_PAD (mask))
                glitz_texture_ensure_wrap (gl, mtexture,
                                           GLITZ_GL_CLAMP_TO_EDGE);
            else
                glitz_texture_ensure_wrap (gl, mtexture,
                                           GLITZ_GL_CLAMP_TO_BORDER);
        }
    }
    
    if (stexture)
    {
        int last_texture_nr = comp_op.combine->texture_units - 1;
        
        while (texture_nr < last_texture_nr)
        {
            textures[++texture_nr].texture = stexture;
            textures[texture_nr].unit = _texture_units[texture_nr];
            textures[texture_nr].transform = 0;
            if (texture_nr > 0)
            {
                gl->active_texture (textures[texture_nr].unit);
                gl->client_active_texture (textures[texture_nr].unit);
            }
            glitz_texture_bind (gl, stexture);
        }

        flags = src->flags | GLITZ_SURFACE_FLAGS_GEN_COORDS_MASK;
        if (dst->geometry.attributes & GLITZ_VERTEX_ATTRIBUTE_SRC_COORD_MASK)
        {
            flags &= ~GLITZ_SURFACE_FLAG_GEN_S_COORDS_MASK;
            
            if (dst->geometry.u.v.src.size == 2)
                flags &= ~GLITZ_SURFACE_FLAG_GEN_T_COORDS_MASK;
        }
    
        glitz_texture_set_tex_gen (gl,
                                   stexture,
                                   &dst->geometry,
                                   x_dst - x_src,
                                   y_dst - y_src,
                                   flags,
                                   &dst->geometry.u.v.src);

        if (src->transform)
        {
            textures[texture_nr].transform = 1;
            gl->matrix_mode (GLITZ_GL_TEXTURE);
            gl->load_matrix_f (SURFACE_EYE_COORDS (src)?
                               src->transform->m: src->transform->t);
            gl->matrix_mode (GLITZ_GL_MODELVIEW);

            if (SURFACE_LINEAR_TRANSFORM_FILTER (src))
                glitz_texture_ensure_filter (gl, stexture, GLITZ_GL_LINEAR);
            else
                glitz_texture_ensure_filter (gl, stexture, GLITZ_GL_NEAREST);
        }
        else
        {
            if ((dst->geometry.attributes &
                 GLITZ_VERTEX_ATTRIBUTE_SRC_COORD_MASK) &&
                SURFACE_LINEAR_TRANSFORM_FILTER (src))
                glitz_texture_ensure_filter (gl, stexture, GLITZ_GL_LINEAR);
            else
                glitz_texture_ensure_filter (gl, stexture, GLITZ_GL_NEAREST);
        }
      
        if (SURFACE_REPEAT (src))
        {
            if (SURFACE_MIRRORED (src))
                glitz_texture_ensure_wrap (gl, stexture,
                                           GLITZ_GL_MIRRORED_REPEAT);
            else
                glitz_texture_ensure_wrap (gl, stexture, GLITZ_GL_REPEAT);
        }
        else
        {
            if (no_border_clamp || SURFACE_PAD (src))
                glitz_texture_ensure_wrap (gl, stexture,
                                           GLITZ_GL_CLAMP_TO_EDGE);
            else
                glitz_texture_ensure_wrap (gl, stexture,
                                           GLITZ_GL_CLAMP_TO_BORDER);
        }
    }

    glitz_geometry_enable (gl, dst, &bounds);

    if (comp_op.per_component)
    {
        static unsigned short alpha_map[4][4] = {
            { 0, 0, 0, 1 },
            { 0, 0, 1, 0 },
            { 0, 1, 0, 0 },
            { 1, 0, 0, 0 }
        };
        static int damage[4] = {
            GLITZ_DAMAGE_TEXTURE_MASK | GLITZ_DAMAGE_SOLID_MASK,
            0,
            0,
            0
        };
        glitz_color_t alpha = comp_op.alpha_mask;
        int           component = 4;
        int           cmask = 1;
        
        while (component--)
        {
            comp_op.alpha_mask.red   = alpha_map[component][0] * alpha.red;
            comp_op.alpha_mask.green = alpha_map[component][1] * alpha.green;
            comp_op.alpha_mask.blue  = alpha_map[component][2] * alpha.blue;
            comp_op.alpha_mask.alpha = alpha_map[component][3] * alpha.alpha;
            
            gl->color_mask ((cmask & 1)     ,
                            (cmask & 2) >> 1,
                            (cmask & 4) >> 2,
                            (cmask & 8) >> 3);
                
            glitz_composite_enable (&comp_op);
            glitz_geometry_draw_arrays (gl, dst,
                                        dst->geometry.type, &bounds,
                                        damage[component]);
            cmask <<= 1;
        }
        
        gl->color_mask (1, 1, 1, 1);
    }
    else
    {
        glitz_composite_enable (&comp_op);
        glitz_geometry_draw_arrays (gl, dst, dst->geometry.type, &bounds,
                                    GLITZ_DAMAGE_TEXTURE_MASK |
                                    GLITZ_DAMAGE_SOLID_MASK);
    }
    
    glitz_composite_disable (&comp_op);
    glitz_geometry_disable (dst);

    for (i = texture_nr; i >= 0; i--)
    {
        glitz_texture_unbind (gl, textures[i].texture);
        if (textures[i].transform)
        {
            gl->matrix_mode (GLITZ_GL_TEXTURE);
            gl->load_identity ();
            gl->matrix_mode (GLITZ_GL_MODELVIEW);
        }

        if (i > 0)
        {
            gl->client_active_texture (textures[i - 1].unit);
            gl->active_texture (textures[i - 1].unit);
        }
    }
  
    glitz_surface_pop_current (dst);
}

void
glitz_copy_area (glitz_surface_t *src,
                 glitz_surface_t *dst,
                 int             x_src,
                 int             y_src,
                 int             width,
                 int             height,
                 int             x_dst,
                 int             y_dst)
{
    glitz_status_t status;
    glitz_box_t    bounds;
    int            src_width  = src->box.x2;
    int            src_height = src->box.y2;

    GLITZ_GL_SURFACE (dst);

    if (x_src < 0)
    {
        bounds.x1 = x_dst - x_src;
        width += x_src;
    }
    else
    {
        bounds.x1 = x_dst;
        src_width -= x_src;
    }
        
    if (y_src < 0)
    {
        bounds.y1 = y_dst - y_src;
        height += y_src;
    }
    else
    {
        bounds.y1 = y_dst;
        src_height -= y_src;
    }
    
    if (width > src_width)
        bounds.x2 = bounds.x1 + src_width;
    else
        bounds.x2 = bounds.x1 + width;
    
    if (height > src_height)
        bounds.y2 = bounds.y1 + src_height;
    else
        bounds.y2 = bounds.y1 + height;

    if (bounds.x1 < 0)
        bounds.x1 = 0;
    if (bounds.y1 < 0)
        bounds.y1 = 0;
    if (bounds.x2 > dst->box.x2)
        bounds.x2 = dst->box.x2;
    if (bounds.y2 > dst->box.y2)
        bounds.y2 = dst->box.y2;

    if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
        return;
    
    status = GLITZ_STATUS_NOT_SUPPORTED;
    if (glitz_surface_push_current (dst, GLITZ_DRAWABLE_CURRENT))
    {
        int target_height = SURFACE_DRAWABLE_HEIGHT (dst);
        int source_height = SURFACE_DRAWABLE_HEIGHT (src);
        
        if (src == dst || (dst->attached && src->attached == dst->attached))
        {
            glitz_box_t box, *clip = dst->clip;
            int         n_clip = dst->n_clip;

            if (REGION_NOTEMPTY (&src->drawable_damage))
            {
                glitz_surface_push_current (src, GLITZ_DRAWABLE_CURRENT);
                glitz_surface_pop_current (src);
            }
            
            gl->read_buffer (src->buffer);
            gl->draw_buffer (dst->buffer);
      
            glitz_set_operator (gl, GLITZ_OPERATOR_SRC);
      
            x_src += src->x;
            y_src += src->y;

            while (n_clip--)
            {
                box.x1 = clip->x1 + dst->x_clip;
                box.y1 = clip->y1 + dst->y_clip;
                box.x2 = clip->x2 + dst->x_clip;
                box.y2 = clip->y2 + dst->y_clip;
                if (bounds.x1 > box.x1)
                    box.x1 = bounds.x1;
                if (bounds.y1 > box.y1)
                    box.y1 = bounds.y1;
                if (bounds.x2 < box.x2)
                    box.x2 = bounds.x2;
                if (bounds.y2 < box.y2)
                    box.y2 = bounds.y2;
                
                if (box.x1 < box.x2 && box.y1 < box.y2)
                {
                    glitz_set_raster_pos (gl,
                                          dst->x + box.x1,
                                          target_height - (dst->y + box.y2));

                    gl->scissor (dst->x + box.x1,
                                 target_height - (dst->y + box.y2),
                                 box.x2 - box.x1,
                                 box.y2 - box.y1);

                    gl->copy_pixels (x_src + (box.x1 - x_dst),
                                     source_height -
                                     (y_src + (box.y2 - y_dst)),
                                     box.x2 - box.x1, box.y2 - box.y1,
                                     GLITZ_GL_COLOR);

                    glitz_surface_damage (dst, &box,
                                          GLITZ_DAMAGE_TEXTURE_MASK |
                                          GLITZ_DAMAGE_SOLID_MASK);
                }
                
                clip++;
            }
        }
        else
        {
            glitz_texture_t *texture;

            texture = glitz_surface_get_texture (src, 0);
            if (texture)
            {
                glitz_texture_bind (gl, texture);

                glitz_texture_set_tex_gen (gl, texture, NULL,
                                           x_dst - x_src,
                                           y_dst - y_src,
                                           GLITZ_SURFACE_FLAGS_GEN_COORDS_MASK,
                                           NULL);

                gl->tex_env_f (GLITZ_GL_TEXTURE_ENV, GLITZ_GL_TEXTURE_ENV_MODE,
                               GLITZ_GL_REPLACE);
                gl->color_4us (0x0, 0x0, 0x0, 0xffff);
        
                glitz_texture_ensure_wrap (gl, texture,
                                           GLITZ_GL_CLAMP_TO_EDGE);
                glitz_texture_ensure_filter (gl, texture, GLITZ_GL_NEAREST);

                glitz_set_operator (gl, GLITZ_OPERATOR_SRC);

                if (dst->n_clip > 1)
                {
                    glitz_float_t *data;
                    void          *ptr;
                    int           vertices = 0;
                    glitz_box_t   box, *clip = dst->clip;
                    int           n_clip = dst->n_clip;

                    ptr = malloc (n_clip * 8 * sizeof (glitz_float_t));
                    if (!ptr) {
                        glitz_surface_pop_current (dst);
                        glitz_surface_status_add (dst,
                                                  GLITZ_STATUS_NO_MEMORY_MASK);
                        return;
                    }
                    
                    data = (glitz_float_t *) ptr;
                    
                    while (n_clip--)
                    {
                        box.x1 = clip->x1 + dst->x_clip;
                        box.y1 = clip->y1 + dst->y_clip;
                        box.x2 = clip->x2 + dst->x_clip;
                        box.y2 = clip->y2 + dst->y_clip;
                        if (bounds.x1 > box.x1)
                            box.x1 = bounds.x1;
                        if (bounds.y1 > box.y1)
                            box.y1 = bounds.y1;
                        if (bounds.x2 < box.x2)
                            box.x2 = bounds.x2;
                        if (bounds.y2 < box.y2)
                            box.y2 = bounds.y2;
                        
                        if (box.x1 < box.x2 && box.y1 < box.y2)
                        {
                            *data++ = (glitz_float_t) box.x1;
                            *data++ = (glitz_float_t) box.y1;
                            *data++ = (glitz_float_t) box.x2;
                            *data++ = (glitz_float_t) box.y1;
                            *data++ = (glitz_float_t) box.x2;
                            *data++ = (glitz_float_t) box.y2;
                            *data++ = (glitz_float_t) box.x1;
                            *data++ = (glitz_float_t) box.y2;

                            vertices += 4;
                            glitz_surface_damage (dst, &box,
                                                  GLITZ_DAMAGE_TEXTURE_MASK |
                                                  GLITZ_DAMAGE_SOLID_MASK);
                        }
                
                        clip++;
                    }

                    if (vertices)
                    {
                        gl->scissor (bounds.x1 + dst->x,
                                     (target_height - dst->y) - bounds.y2,
                                     bounds.x2 - bounds.x1,
                                     bounds.y2 - bounds.y1);

                        gl->vertex_pointer (2, GLITZ_GL_FLOAT, 0, ptr);
                        gl->draw_arrays (GLITZ_GL_QUADS, 0, vertices);
                    }
                    
                    free (ptr);
                }
                else
                {
                    glitz_geometry_enable_none (gl, dst, &bounds);
                    glitz_geometry_draw_arrays (gl, dst,
                                                GLITZ_GEOMETRY_TYPE_NONE,
                                                &bounds,
                                                GLITZ_DAMAGE_TEXTURE_MASK |
                                                GLITZ_DAMAGE_SOLID_MASK);
                }
                
                glitz_texture_unbind (gl, texture);
            }
        }

        status = GLITZ_STATUS_SUCCESS;
    }

    glitz_surface_pop_current (dst);

    if (status && src->attached)
    {
        if (glitz_surface_push_current (src, GLITZ_DRAWABLE_CURRENT))
        {
            glitz_texture_t *texture;

            gl->read_buffer (src->buffer);

            texture = glitz_surface_get_texture (dst, 1);
            if (texture)
            {
                glitz_box_t box, *clip  = dst->clip;
                int         n_clip = dst->n_clip;

                gl->disable (GLITZ_GL_SCISSOR_TEST);

                glitz_texture_bind (gl, texture);

                x_src += src->x;
                y_src += src->y;

                while (n_clip--)
                {
                    box.x1 = clip->x1 + dst->x_clip;
                    box.y1 = clip->y1 + dst->y_clip;
                    box.x2 = clip->x2 + dst->x_clip;
                    box.y2 = clip->y2 + dst->y_clip;
                    if (bounds.x1 > box.x1)
                        box.x1 = bounds.x1;
                    if (bounds.y1 > box.y1)
                        box.y1 = bounds.y1;
                    if (bounds.x2 < box.x2)
                        box.x2 = bounds.x2;
                    if (bounds.y2 < box.y2)
                        box.y2 = bounds.y2;
                    
                    if (box.x1 < box.x2 && box.y1 < box.y2)
                    {
                        glitz_texture_copy_drawable (gl,
                                                     texture,
                                                     src->attached,
                                                     x_src + (box.x1 - x_dst),
                                                     y_src + (box.y1 - y_dst),
                                                     box.x2 - box.x1,
                                                     box.y2 - box.y1,
                                                     box.x1,
                                                     box.y1);

                        glitz_surface_damage (dst, &box,
                                              GLITZ_DAMAGE_DRAWABLE_MASK |
                                              GLITZ_DAMAGE_SOLID_MASK);
                    }
                    
                    clip++;
                }
                
                glitz_texture_unbind (gl, texture);
                
                gl->enable (GLITZ_GL_SCISSOR_TEST);
        
                status = GLITZ_STATUS_SUCCESS;
            }
        }
        glitz_surface_pop_current (src);
    }

    if (status)
        glitz_surface_status_add (dst, glitz_status_to_status_mask (status));
}
