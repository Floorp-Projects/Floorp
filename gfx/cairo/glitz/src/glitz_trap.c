/*
 * Copyright © 2005 Novell, Inc.
 * 
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
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

/* whether 't' is a well defined not obviously empty trapezoid */
#define TRAPEZOID_VALID(t) ((t)->left.p1.y != (t)->left.p2.y &&   \
                            (t)->right.p1.y != (t)->right.p2.y && \
                            (int) ((t)->bottom - (t)->top) > 0)

/* whether 't' is a well defined not obviously empty trap */
#define TRAP_VALID(t) ((int) ((t)->bottom.y - (t)->top.y) > 0)

typedef struct _glitz_edge {
    glitz_float_t dx, dy;
    glitz_float_t x0, y0;
    glitz_float_t kx, ky;
    glitz_float_t tx, bx;
    glitz_float_t hypx;
    int           hyp;
} glitz_edge_t;

#define EDGE_INIT(e, p1x, p1y, p2x, p2y) \
    (e)->hyp = 1;                        \
    (e)->dx = (p2x) - (p1x);             \
    (e)->dy = (p2y) - (p1y);             \
    if ((e)->dy)                         \
        (e)->kx = (e)->dx / (e)->dy;     \
    else                                 \
         (e)->kx = 0;                    \
    (e)->x0 = (p1x) - (e)->kx * (p1y);   \
    if ((e)->dx)                         \
        (e)->ky = (e)->dy / (e)->dx;     \
    else                                 \
         (e)->ky = 0;                    \
    (e)->y0 = (p1y) - (e)->ky * (p1x)

#define EDGE_X(e, _y) (((e)->kx * (_y)) + (e)->x0)
#define EDGE_Y(e, _x) (((e)->ky * (_x)) + (e)->y0)

#define EDGE_INTERSECT_BOX(upper_x, lower_y, left_y, right_y, \
                           box_x1, box_x2, box_y1, box_y2,    \
                           max_x, max_y,                      \
                           _x1, _y1, _x2, _y2)                \
    if (upper_x < (box_x1))                                   \
    {                                                         \
        (_x1) = 0.0f;                                         \
        (_y1) = (left_y) - (box_y1);                          \
    }                                                         \
    else if ((upper_x) > (box_x2))                            \
    {                                                         \
        (_x1) = (max_x);                                      \
        (_y1) = (right_y) - (box_y1);                         \
    }                                                         \
    else                                                      \
    {                                                         \
        (_x1) = (upper_x) - (box_x1);                         \
        (_y1) = 0.0f;                                         \
    }                                                         \
    if ((lower_x) < (box_x1))                                 \
    {                                                         \
        (_x2) = 0.0f;                                         \
        (_y2) = (left_y) - (box_y1);                          \
    }                                                         \
    else if ((lower_x) > (box_x2))                            \
    {                                                         \
        (_x2) = (max_x);                                      \
        (_y2) = (right_y) - (box_y1);                         \
    }                                                         \
    else                                                      \
    {                                                         \
        (_x2) = (lower_x) - (box_x1);                         \
        (_y2) = (max_y);                                      \
    }

#define AREA_ABOVE_LEFT(x1, y1, x2, y2, bottom) \
    ((x1) * (y1) +                              \
     (((x1) + (x2)) / 2.0f) * ((y2) - (y1)) +   \
     (x2) * ((bottom) - (y2)))

/*
  Given a single pixel position this function calculates
  trapezoid pixel coverage.
*/
static glitz_float_t
_glitz_pixel_area (glitz_float_t pixel_x,
                   glitz_float_t pixel_y,
                   glitz_float_t top,
                   glitz_float_t bottom,
                   glitz_edge_t  *left,
                   glitz_edge_t  *right)
{
    glitz_float_t area;
    glitz_float_t upper_x, lower_x;
    glitz_float_t left_y, right_y;
    glitz_float_t pixel_x_1 = pixel_x + 1.0f;
    glitz_float_t pixel_y_1 = pixel_y + 1.0f;
    glitz_float_t x1, x2, y1, y2;

    if (bottom >= pixel_y_1)
        bottom = 1.0f;
    else
        bottom = bottom - pixel_y;

    if (top <= pixel_y)
        top = 0.0f;
    else
        top = top - pixel_y;
    
    if (right->ky)
    {
        upper_x = EDGE_X (right, pixel_y);
        lower_x = EDGE_X (right, pixel_y_1);

        left_y  = EDGE_Y (right, pixel_x);
        right_y = EDGE_Y (right, pixel_x_1);

        EDGE_INTERSECT_BOX (upper_x, lower_y, left_y, right_y,
                            pixel_x, pixel_x_1, pixel_y, pixel_y_1,
                            1.0f, 1.0f, x1, y1, x2, y2);
        
        if (bottom <= y1)
        {
            if (left_y > right_y)
                area = bottom;
            else
                area = 0.0f;
        }
        else
        {
            if (bottom < y2)
            {
                x2 -= right->kx * (y2 - bottom);
                y2 = bottom;
            }   
            area = AREA_ABOVE_LEFT (x1, y1, x2, y2, bottom);
        }

        if (top <= y1)
        {
            if (left_y > right_y)
                area -= top;
        }
        else
        {
            if (top < y2)
            {
                x2 -= right->kx * (y2 - top);
                y2 = top;
            }   

            area -= AREA_ABOVE_LEFT (x1, y1, x2, y2, top);
        }
    }
    else
    {
        /* Vertical Edge */
        if (right->x0 < pixel_x_1)
            area = (right->x0 - pixel_x) * (bottom - top);
        else
            area = bottom - top;
    }

    if (left->kx)
    {
        upper_x = EDGE_X (left, pixel_y);
        lower_x = EDGE_X (left, pixel_y_1);

        left_y  = EDGE_Y (left, pixel_x);
        right_y = EDGE_Y (left, pixel_x_1);

        EDGE_INTERSECT_BOX (upper_x, lower_y, left_y, right_y,
                            pixel_x, pixel_x_1, pixel_y, pixel_y_1,
                            1.0f, 1.0f, x1, y1, x2, y2);
        
        if (bottom <= y1)
        {
            if (left_y > right_y)
                area -= bottom;
        }
        else
        {
            if (bottom < y2)
            {
                x2 -= left->kx * (y2 - bottom);
                y2 = bottom;
            }   
            area -= AREA_ABOVE_LEFT (x1, y1, x2, y2, bottom);
        }

        if (top <= y1)
        {
            if (left_y > right_y)
                area += top;
        }
        else
        {
            if (top < y2)
            {
                x2 -= left->kx * (y2 - top);
                y2 = top;
            }   
            
            area += AREA_ABOVE_LEFT (x1, y1, x2, y2, top);
        }
    }
    else
    {
        /* Vertical Edge */
        if (left->x0 > pixel_x)
            area -= (left->x0 - pixel_x) * (bottom - top);
    }
    
    return area;
}

#define TRAPINIT(trap, _top, _bottom, _left, _right)    \
    if (!TRAPEZOID_VALID (trap))                        \
        continue;                                       \
                                                        \
    (_top)    = FIXED_TO_FLOAT ((trap)->top);           \
    (_bottom) = FIXED_TO_FLOAT ((trap)->bottom);        \
                                                        \
    (_left)->tx = FIXED_TO_FLOAT ((trap)->left.p1.x);   \
    (_left)->bx = FIXED_TO_FLOAT ((trap)->left.p1.y);   \
                                                        \
    (_right)->tx = FIXED_TO_FLOAT ((trap)->right.p1.x); \
    (_right)->bx = FIXED_TO_FLOAT ((trap)->right.p1.y); \
                                                        \
    EDGE_INIT (_left,                                   \
               (_left)->tx, (_left)->bx,                \
               FIXED_TO_FLOAT ((trap)->left.p2.x),      \
               FIXED_TO_FLOAT ((trap)->left.p2.y));     \
                                                        \
    EDGE_INIT (_right,                                  \
               (_right)->tx, (_right)->bx,              \
               FIXED_TO_FLOAT ((trap)->right.p2.x),     \
               FIXED_TO_FLOAT ((trap)->right.p2.y));    \
                                                        \
    if ((_left)->dx)                                    \
    {                                                   \
        (_left)->tx = EDGE_X (_left, _top);             \
        (_left)->bx = EDGE_X (_left, _bottom);          \
    } else                                              \
        (_left)->tx = (_left)->bx = (_left)->x0;        \
                                                        \
    if ((_right)->dx)                                   \
    {                                                   \
        (_right)->tx = EDGE_X (_right, _top);           \
        (_right)->bx = EDGE_X (_right, _bottom);        \
    } else                                              \
        (_right)->tx = (_right)->bx = (_right)->x0

#define TRAP  glitz_trapezoid_t

#define UNIT  glitz_short_t
#define TRAPS _glitz_add_trapezoids_short
#include "glitz_trapimp.h"
#undef  TRAPS
#undef  UNIT

#define UNIT  glitz_int_t
#define TRAPS _glitz_add_trapezoids_int
#include "glitz_trapimp.h"
#undef  TRAPS
#undef  UNIT

#define UNIT  glitz_float_t
#define TRAPS _glitz_add_trapezoids_float
#include "glitz_trapimp.h"
#undef  TRAPS
#undef  UNIT

#define UNIT  glitz_double_t
#define TRAPS _glitz_add_trapezoids_double
#include "glitz_trapimp.h"
#undef  TRAPS
#undef  UNIT

#undef  TRAP
#undef  TRAPINIT

#define TRAPINIT(trap, _top, _bottom, _left, _right)      \
    if (!TRAP_VALID (trap))                               \
        continue;                                         \
                                                          \
    (_top)    = FIXED_TO_FLOAT ((trap)->top.y);           \
    (_bottom) = FIXED_TO_FLOAT ((trap)->bottom.y);        \
                                                          \
    (_left)->tx = FIXED_TO_FLOAT ((trap)->top.left);      \
    (_left)->bx = FIXED_TO_FLOAT ((trap)->bottom.left);   \
                                                          \
    (_right)->tx = FIXED_TO_FLOAT ((trap)->top.right);    \
    (_right)->bx = FIXED_TO_FLOAT ((trap)->bottom.right); \
                                                          \
    EDGE_INIT (_left,                                     \
               (_left)->tx, _top,                         \
               (_left)->bx, _bottom);                     \
                                                          \
    EDGE_INIT (_right,                                    \
               (_right)->tx, _top,                        \
               (_right)->bx, _bottom)

#define TRAP  glitz_trap_t

#define UNIT  glitz_short_t
#define TRAPS _glitz_add_traps_short
#include "glitz_trapimp.h"
#undef  TRAPS
#undef  UNIT

#define UNIT  glitz_int_t
#define TRAPS _glitz_add_traps_int
#include "glitz_trapimp.h"
#undef  TRAPS
#undef  UNIT

#define UNIT  glitz_float_t
#define TRAPS _glitz_add_traps_float
#include "glitz_trapimp.h"
#undef  TRAPS
#undef  UNIT

#define UNIT  glitz_double_t
#define TRAPS _glitz_add_traps_double
#include "glitz_trapimp.h"
#undef  TRAPS
#undef  UNIT

#undef  TRAP
#undef  TRAPINIT

int
glitz_add_trapezoids (glitz_buffer_t    *buffer,
                      int               offset,
                      unsigned int      size,
                      glitz_data_type_t type,
                      glitz_surface_t   *mask,
                      glitz_trapezoid_t *traps,
                      int               n_traps,
                      int               *n_added)
{
    int     count, n = n_traps;
    uint8_t *ptr;
    
    *n_added = 0;
    
    ptr = glitz_buffer_map (buffer, GLITZ_BUFFER_ACCESS_WRITE_ONLY);
    if (!ptr)
        return 0;

    ptr += offset;
    
    switch (type) {
    case GLITZ_DATA_TYPE_SHORT:
        count = _glitz_add_trapezoids_short (ptr, size, mask, traps, &n_traps);
        break;
    case GLITZ_DATA_TYPE_INT:
        count = _glitz_add_trapezoids_int (ptr, size, mask, traps, &n_traps);
        break;
    case GLITZ_DATA_TYPE_DOUBLE:
        count = _glitz_add_trapezoids_double (ptr, size, mask,
                                              traps, &n_traps);
        break;
    default:
        count = _glitz_add_trapezoids_float (ptr, size, mask, traps, &n_traps);
        break;
    }

    if (glitz_buffer_unmap (buffer))
        return 0;
    
    *n_added = n - n_traps;

    return count;
}

int
glitz_add_traps (glitz_buffer_t    *buffer,
                 int               offset,
                 unsigned int      size,
                 glitz_data_type_t type,
                 glitz_surface_t   *mask,
                 glitz_trap_t      *traps,
                 int               n_traps,
                 int               *n_added)
{
    int     count, n = n_traps;
    uint8_t *ptr;
    
    *n_added = 0;
    
    ptr = glitz_buffer_map (buffer, GLITZ_BUFFER_ACCESS_WRITE_ONLY);
    if (!ptr)
	return 0;

    ptr += offset;
    
    switch (type) {
    case GLITZ_DATA_TYPE_SHORT:
        count = _glitz_add_traps_short (ptr, size, mask, traps, &n_traps);
        break;
    case GLITZ_DATA_TYPE_INT:
        count = _glitz_add_traps_int (ptr, size, mask, traps, &n_traps);
        break;
    case GLITZ_DATA_TYPE_DOUBLE:
        count = _glitz_add_traps_double (ptr, size, mask, traps, &n_traps);
        break;
    default:
        count = _glitz_add_traps_float (ptr, size, mask, traps, &n_traps);
        break;
    }

    if (glitz_buffer_unmap (buffer))
        return 0;

    *n_added = n - n_traps;

    return count;
}
