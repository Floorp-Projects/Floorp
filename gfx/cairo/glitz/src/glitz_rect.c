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

#define STORE_16(dst, size, src) \
  dst = ((size) ? \
         ((((((1L << (size)) - 1L) * (src)) / 0xffff) * 0xffff) / \
          ((1L << (size)) - 1L)) : \
         dst)

static glitz_buffer_t *
_glitz_minimum_buffer (glitz_surface_t         *surface,
                       const glitz_rectangle_t *rects,
                       int                     n_rects,
                       unsigned int            *pixel)
{
    glitz_buffer_t *buffer;
    int            i, size = 0;
    unsigned int   *data;
  
    while (n_rects--)
    {
        i = rects->width * rects->height;
        if (i > size)
            size = i;
        
        rects++;
    }

    if (size <= 1)
        return glitz_buffer_create_for_data (pixel);
        
    buffer = glitz_pixel_buffer_create (surface->drawable, NULL,
                                        size * sizeof (unsigned int),
                                        GLITZ_BUFFER_HINT_STATIC_DRAW);
    if (!buffer)
        return NULL;
    
    data = glitz_buffer_map (buffer, GLITZ_BUFFER_ACCESS_WRITE_ONLY);
    
    while (size--)
        *data++ = *pixel;

    glitz_buffer_unmap (buffer);
    
    return buffer;
}

void
glitz_set_rectangles (glitz_surface_t         *dst,
                      const glitz_color_t     *color,
                      const glitz_rectangle_t *rects,
                      int                     n_rects)
{
    GLITZ_GL_SURFACE (dst);

    if (n_rects < 1)
        return;

    if (SURFACE_SOLID (dst))
    {
        glitz_color_t old = dst->solid;
        glitz_box_t   *clip  = dst->clip;
        int           n_clip = dst->n_clip;

        for (; n_clip; clip++, n_clip--)
        {
            if (clip->x1 > 0 ||
                clip->y1 > 0 ||
                clip->x2 < 1 ||
                clip->y2 < 1)
                continue;
            
            for (; n_rects; rects++, n_rects--)
            {
                if (rects->x > 0 ||
                    rects->y > 0 ||
                    (rects->x + rects->width) < 1 ||
                    (rects->y + rects->height) < 1)
                    continue;
            
                STORE_16 (dst->solid.red,
                          dst->format->color.red_size,
                          color->red);
                STORE_16 (dst->solid.green,
                          dst->format->color.green_size,
                          color->green);
                STORE_16 (dst->solid.blue,
                          dst->format->color.blue_size,
                          color->blue);
                STORE_16 (dst->solid.alpha,
                          dst->format->color.alpha_size,
                          color->alpha);
    
                if (dst->flags & GLITZ_SURFACE_FLAG_SOLID_DAMAGE_MASK)
                {
                    dst->flags &= ~GLITZ_SURFACE_FLAG_SOLID_DAMAGE_MASK;
                    glitz_surface_damage (dst, &dst->box,
                                          GLITZ_DAMAGE_TEXTURE_MASK |
                                          GLITZ_DAMAGE_DRAWABLE_MASK);
                }
                else
                {
                    if (dst->solid.red   != old.red   ||
                        dst->solid.green != old.green ||
                        dst->solid.blue  != old.blue  ||
                        dst->solid.alpha != old.alpha)
                        glitz_surface_damage (dst, &dst->box,
                                              GLITZ_DAMAGE_TEXTURE_MASK |
                                              GLITZ_DAMAGE_DRAWABLE_MASK);
                }
                break;
            }
            break;
        }
    }
    else
    {
        static glitz_pixel_format_t pf = {
            {
                32,
                0xff000000,
                0x00ff0000,
                0x0000ff00,
                0x000000ff
            },
            0, 0, 0,
            GLITZ_PIXEL_SCANLINE_ORDER_BOTTOM_UP
        };
        glitz_buffer_t *buffer = NULL;
        glitz_box_t    box;
        glitz_bool_t   drawable = 0;

        if (n_rects == 1 && rects->width <= 1 && rects->height <= 1)
        {
            glitz_surface_push_current (dst, GLITZ_ANY_CONTEXT_CURRENT);
        }
        else
        {
            drawable = glitz_surface_push_current (dst,
                                                   GLITZ_DRAWABLE_CURRENT);
        }

        if (drawable)
        {
            glitz_box_t *clip;
            int         n_clip;
            int         target_height = SURFACE_DRAWABLE_HEIGHT (dst);
                
            gl->clear_color (color->red   / (glitz_gl_clampf_t) 0xffff,
                             color->green / (glitz_gl_clampf_t) 0xffff,
                             color->blue  / (glitz_gl_clampf_t) 0xffff,
                             color->alpha / (glitz_gl_clampf_t) 0xffff);

            while (n_rects--)
            {
                clip = dst->clip;
                n_clip = dst->n_clip;
                while (n_clip--)
                {
                    box.x1 = clip->x1 + dst->x_clip;
                    box.y1 = clip->y1 + dst->y_clip;
                    box.x2 = clip->x2 + dst->x_clip;
                    box.y2 = clip->y2 + dst->y_clip;
                    
                    if (dst->box.x1 > box.x1)
                        box.x1 = dst->box.x1;
                    if (dst->box.y1 > box.y1)
                        box.y1 = dst->box.y1;
                    if (dst->box.x2 < box.x2)
                        box.x2 = dst->box.x2;
                    if (dst->box.y2 < box.y2)
                        box.y2 = dst->box.y2;
                    
                    if (rects->x > box.x1)
                        box.x1 = rects->x;
                    if (rects->y > box.y1)
                        box.y1 = rects->y;
                    if (rects->x + rects->width < box.x2)
                        box.x2 = rects->x + rects->width;
                    if (rects->y + rects->height < box.y2)
                        box.y2 = rects->y + rects->height;
                    
                    if (box.x1 < box.x2 && box.y1 < box.y2)
                    {
                        gl->scissor (box.x1,
                                     target_height - dst->y - box.y2,
                                     box.x2 - box.x1,
                                     box.y2 - box.y1);
                        
                        gl->clear (GLITZ_GL_COLOR_BUFFER_BIT);
                            
                        glitz_surface_damage (dst, &box,
                                              GLITZ_DAMAGE_TEXTURE_MASK |
                                              GLITZ_DAMAGE_SOLID_MASK);
                    }
                    
                    clip++;
                }
                rects++;
            }
        }
        else
        {
            unsigned int pixel =
                ((((unsigned int) color->alpha * 0xff) / 0xffff) << 24) |
                ((((unsigned int) color->red   * 0xff) / 0xffff) << 16) |
                ((((unsigned int) color->green * 0xff) / 0xffff) << 8) |
                ((((unsigned int) color->blue  * 0xff) / 0xffff));
            int x1, y1, x2, y2;

            buffer = _glitz_minimum_buffer (dst, rects, n_rects, &pixel);
            if (!buffer)
            {
                glitz_surface_status_add (dst, GLITZ_STATUS_NO_MEMORY_MASK);
                return;
            }

            while (n_rects--)
            {
                x1 = rects->x;
                y1 = rects->y;
                x2 = x1 + rects->width;
                y2 = y1 + rects->height;
            
                if (x1 < 0)
                    x1 = 0;
                if (y1 < 0)
                    y1 = 0;
                if (x2 > dst->box.x2)
                    x2 = dst->box.x2;
                if (y2 > dst->box.y2)
                    y2 = dst->box.y2;

                if (x1 < x2 && y1 < y2)
                    glitz_set_pixels (dst,
                                      x1, y1,
                                      x2 - x1, y2 - y1,
                                      &pf, buffer);

                rects++;
            }
        
            if (buffer)
                glitz_buffer_destroy (buffer);
        } 
        glitz_surface_pop_current (dst);
    }
}
slim_hidden_def(glitz_set_rectangles);

void
glitz_set_rectangle (glitz_surface_t     *dst,
                     const glitz_color_t *color,
                     int                 x,
                     int                 y,
                     unsigned int        width,
                     unsigned int        height)
{
    glitz_rectangle_t rect;

    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;

    glitz_set_rectangles (dst, color, &rect, 1);
}
slim_hidden_def(glitz_set_rectangle);
