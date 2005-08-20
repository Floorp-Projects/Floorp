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

/*
  Define the following before including this file:
  
  TRAPS    name of function for adding trapezoids
  UNIT     type of underlying vertex unit
  TRAP     type of underlying trapezoid structure
  TRAPINIT initialization code for underlying trapezoid structure
*/

#define BYTES_PER_VERTEX (2 * sizeof (UNIT) + sizeof (glitz_float_t))
#define BYTES_PER_QUAD   (4 * BYTES_PER_VERTEX)

#define VSKIP (BYTES_PER_VERTEX / sizeof (UNIT))
#define TSKIP (BYTES_PER_VERTEX / sizeof (glitz_float_t))

#define ADD_VERTEX(vptr, tptr, texcoord, _x, _y) \
    (vptr)[0]  = (UNIT) (_x);                    \
    (vptr)[1]  = (UNIT) (_y);                    \
    (tptr)[0]  = (texcoord);                     \
    (vptr)    += VSKIP;                          \
    (tptr)    += TSKIP

#define ADD_QUAD(vptr, tptr, offset, size,           \
                 t00, t01, t10, t11, x1, y1, x2, y2) \
    if ((offset) == (size))                          \
        return (toff);                               \
    (offset) += BYTES_PER_QUAD;                      \
    ADD_VERTEX (vptr, tptr, t00, x1, y1);            \
    ADD_VERTEX (vptr, tptr, t01, x2, y1);            \
    ADD_VERTEX (vptr, tptr, t10, x2, y2);            \
    ADD_VERTEX (vptr, tptr, t11, x1, y2)

#define ADD_PIXEL(vptr, tptr, offset, size, tmp0, tbase, tsize, \
                  x1, y1, x2, y2, alpha)                        \
    (tmp0) = (tbase) + (alpha - 0.5f) * (tsize);                \
    ADD_QUAD (vptr, tptr, offset, size, tmp0, tmp0, tmp0, tmp0, \
              x1, y1, x2, y2)

#define CALC_SPAN(tmp0, tmp1, tbase, tsize, edge, end) \
    (tmp0) = (edge) - (end);                           \
    (tmp0) = (tbase) - (tmp0) * (tsize);               \
    (tmp1) = (tbase) + (end) * (tsize)

#define ADD_LEFT_SPAN(vptr, tptr, offset, size, tmp0, tmp1, tbase, tsize, \
                      x1, y1, x2, y2, end)                                \
    CALC_SPAN (tmp0, tmp1, tbase, tsize, (x2) - (x1), end);               \
    ADD_QUAD (vptr, tptr, offset, size, tmp0, tmp1, tmp1, tmp0,           \
              x1, y1, x2, y2)

#define ADD_RIGHT_SPAN(vptr, tptr, offset, size, tmp0, tmp1, tbase, tsize, \
                       x1, y1, x2, y2, end)                                \
    CALC_SPAN (tmp0, tmp1, tbase, tsize, (x2) - (x1), end);                \
    ADD_QUAD (vptr, tptr, offset, size, tmp1, tmp0, tmp0, tmp1,            \
              x1, y1, x2, y2)

#define ADD_TOP_SPAN(vptr, tptr, offset, size, tmp0, tmp1, tbase, tsize, \
                     x1, y1, x2, y2, end)                                \
    CALC_SPAN (tmp0, tmp1, tbase, tsize, (y2) - (y1), end);              \
    ADD_QUAD (vptr, tptr, offset, size, tmp0, tmp0, tmp1, tmp1,          \
              x1, y1, x2, y2)

#define ADD_BOTTOM_SPAN(vptr, tptr, offset, size, tmp0, tmp1, tbase, tsize, \
                        x1, y1, x2, y2, end)                                \
    CALC_SPAN (tmp0, tmp1, tbase, tsize, (y2) - (y1), end);                 \
    ADD_QUAD (vptr, tptr, offset, size, tmp1, tmp1, tmp0, tmp0,             \
              x1, y1, x2, y2)


#define CALC_EDGE(tl, tr, bl, br, x1, x2, tsize, edge, tx, bx)             \
    if ((edge)->hyp)                                                       \
    {                                                                      \
        (edge)->hypx = (edge)->dy /                                        \
                sqrtf ((edge)->dx * (edge)->dx + (edge)->dy * (edge)->dy); \
        (edge)->hyp = 0;                                                   \
    }                                                                      \
    (tl) = (((tx) - (x1)) * (edge)->hypx) * tsize;                         \
    (tr) = (((tx) - (x2)) * (edge)->hypx) * tsize;                         \
    (bl) = (((bx) - (x1)) * (edge)->hypx) * tsize;                         \
    (br) = (((bx) - (x2)) * (edge)->hypx) * tsize

#define CALC_LEFT_EDGE(tl, tr, bl, br, x1, x2, tbase, tsize, edge, tx, bx) \
    CALC_EDGE (tl, tr, bl, br, x1, x2, tsize, edge, tx, bx);               \
    (tl) = (tbase) - (tl);                                                 \
    (tr) = (tbase) - (tr);                                                 \
    (bl) = (tbase) - (bl);                                                 \
    (br) = (tbase) - (br)

#define CALC_RIGHT_EDGE(tl, tr, bl, br, x1, x2, tbase, tsize, edge, tx, bx) \
    CALC_EDGE (tl, tr, bl, br, x1, x2, tsize, edge, tx, bx);                \
    (tl) = (tbase) + (tl);                                                  \
    (tr) = (tbase) + (tr);                                                  \
    (bl) = (tbase) + (bl);                                                  \
    (br) = (tbase) + (br)

#define ADD_LEFT_EDGE(vptr, tptr, offset, size,                 \
                      tmp0, tmp1, tmp2, tmp3, tbase, tsize,     \
                      edge, x1, y1, x2, y2, tx, bx)             \
    CALC_LEFT_EDGE (tmp0, tmp1, tmp2, tmp3, x1, x2,             \
                    tbase, tsize, edge, tx, bx);                \
    ADD_QUAD (vptr, tptr, offset, size, tmp0, tmp1, tmp3, tmp2, \
              x1, y1, x2, y2)

#define ADD_RIGHT_EDGE(vptr, tptr, offset, size,                \
                       tmp0, tmp1, tmp2, tmp3, tbase, tsize,    \
                       edge, x1, y1, x2, y2, tx, bx)            \
    CALC_RIGHT_EDGE (tmp0, tmp1, tmp2, tmp3, x1, x2,            \
                     tbase, tsize, edge, tx, bx);               \
    ADD_QUAD (vptr, tptr, offset, size, tmp0, tmp1, tmp3, tmp2, \
              x1, y1, x2, y2)

/*
  An implicit mask surface for a single anti-aliased edge that
  intersect a rectangle in any direction, can be represented
  accurately and consistently with a set of one-dimensional texture
  coordinates in a fixed size texture. By using linear texture
  filtering this allows us to render perfectly anti-aliased
  geometry on a wide range of hardware.

  Each trapezoid needs to be slit up into a set of rectangles along
  with appropriate horizontal texture coordinates for the specified
  mask surface. In general the mask is a 2x1 surface with pixel
  0,0 "clear black" and pixel 1,0 "solid white". It could also be a
  Nx1 surface containing a pre-computed gamma curve with resolution
  N for gamma corrected anti-aliasing.

  This function generates geometry data in the following format:

  primitive   : QUADS
  type        : SHORT|INT|FLOAT|DOUBLE
  stride      : 2 * sizeof (type) + sizeof (FLOAT)
  attributes  : MASK_COORD
  mask.type   : FLOAT
  mask.size   : COORDINATE_SIZE_X
  mask.offset : 2 * sizeof (type)
  
  Trapezoid:
  top    { l, r, y }
  bottom { l, r, y }
  
  Bounds:
  x1 = floor (MIN (top.l, bottom.l))
  x2 = ceil  (MAX (top.r, bottom.r))
  y1 = floor (top.y)
  y2 = ceil  (bottom.y)
  
  W = x2 - x1;
  H = y2 - y1;

  TH = 1 if TOP    rectangle exist, else 0
  BH = 1 if BOTTOM rectangle exist, else 0
  
  TOP    rectangle exists if: floor (top.y)    != top.y
  BOTTOM rectangle exists if: ceil  (bottom.y) != bottom.y &&
  bottom.y > ceil (top.y)
  MIDDLE rectangle exists if: (H - TH - BH) > 0
  
                              W
  +----------------------------------------------------+
  |                                                    | 
  |   TOP           +---------------------------+      | TH
  |                /                            |      | 
  +---------------/-----------------------------+------+
  |              /                              |      |
  |   MIDDLE    /                               |      |
  |            /                                |      |
  |           /                                 |      |
  |          /                                  |      | H - TH - BH
  |         /                                   |      |
  |        /                                    |      |
  |       /                                     |      |
  |      /                                      |      |
  +-----/---------------------------------------+------+
  |    /                                        |      |
  |   +-----------------------------------------+      | BH
  |   BOTTOM                                           |
  +----------------------------------------------------+
*/
static unsigned int
TRAPS (void            *ptr,
       unsigned int    size,
       glitz_surface_t *mask,
       TRAP            *traps,
       int             *n_traps)
{
    unsigned int  toff = 0, offset = 0;
    UNIT          *vptr = (UNIT *) ptr;
    glitz_float_t *tptr = (glitz_float_t *) (vptr + 2);
    
    glitz_edge_t  left, right;
    glitz_float_t top, bottom;

    glitz_float_t tbase, tsize;
    glitz_float_t l, r, lspan, rspan;
    glitz_float_t tmp0, tmp1, tmp2, tmp3, tmpx, tmpy;
    glitz_float_t x1, x2, lx, rx;
    glitz_float_t y, y0, y1, y2, y3;
    glitz_float_t y1lx, y2lx, y1rx, y2rx;
    glitz_float_t area;

    size -= size % BYTES_PER_QUAD;

    tsize = (glitz_float_t) (mask->texture.box.x2 - mask->texture.box.x1);
    tbase = (glitz_float_t) mask->texture.box.x1 + (tsize / 2.0f);

    tsize = (tsize - 1.0f) * mask->texture.texcoord_width_unit;
    tbase *= mask->texture.texcoord_width_unit;

    for (; *n_traps; (*n_traps)--, traps++)
    {
        TRAPINIT (traps, top, bottom, &left, &right);
        
        /*
          Top left X greater than top right X or bottom left X greater
          than bottom right X. To me, this seems like an invalid trapezoid.
          Cairo's current caps generation code, creates a lot of these. The
          following adjustments makes them render almost correct.
        */
        if (left.tx > right.tx)
        {
            if (floorf (left.tx) > floorf (right.tx))
                left.tx = right.tx = floorf (left.tx);   
        }
        
        if (left.bx > right.bx)
        {
            if (floorf (left.bx) > floorf (right.bx))
                left.bx = right.bx = floorf (left.bx);
        }

        x1 = floorf (MIN (left.tx, left.bx));
        x2 = ceilf (MAX (right.tx, right.bx));

        /*
          TOP rectangle
                                       W
          +---------+---------+------------------+---------+--------+
          |         |         |                  |         |        |
          |   lE    |  +------+------------------|------+  |   rE   |
          |         |/        |                  |        \|        | 1
          |        /|   lEtE  |     tE           | tErE    |\       |
          |       +-+---------+------------------+---------+-+      |
          +-----+---+---------+------------------+---------+--------+

          The following set sub-rectangles might need to be generated
          to render the top span of the trapezoid correctly.
          
          lE   = Left Edge
          tE   = Top Edge
          rE   = Right Edge
          lEtE = Left/Top Edge intersection
          tErE = Top/Right Edge intersection
        */
        y1 = ceilf (top);
        if (y1 != top)
        {
            y0 = floorf (top);
            lx = x1;
            rx = x2;
            
            y1lx = EDGE_X (&left, y1);
            y1rx = EDGE_X (&right, y1);

            if (bottom > y1)
            {
                l = MAX (left.tx, y1lx);
                r = MIN (right.tx, y1rx);
            }
            else
            {
                l = MAX (left.tx, left.bx);
                r = MIN (right.tx, right.bx);
            }
                    
            lspan = ceilf (l);
            rspan = floorf (r);
                
            l = floorf (l);
            if (l > rspan)
                l = rspan;

            r = ceilf (r);
            if (r < lspan)
                r = lspan;

            /* Left Edge */
            if (l > x1)
            {
                lx = EDGE_X (&left, y0);
                if (left.dx > 0.0f)
                {
                    tmpx = y1lx + (left.tx - lx);
                    lx = left.tx;
                }
                else
                {   
                    if (bottom < y1)
                    {
                        lx += (left.bx - y1lx);
                        tmpx = left.bx;
                    }
                    else
                        tmpx = y1lx;
                }
                
                ADD_LEFT_EDGE (vptr, tptr, offset, size,
                               tmp0, tmp1, tmp2, tmp3,
                               tbase, tsize, &left,
                               x1, y0, l, y1,
                               lx, tmpx);
                
                lx = l;
            }

            /* Right Edge */
            if (r < x2)
            {
                rx = EDGE_X (&right, y0);
                if (right.dx < 0.0f)
                {
                    tmpx = y1rx - (rx - right.tx);
                    rx = right.tx;   
                }
                else
                {
                    if (bottom < y1)
                    {
                        rx -= (y1rx - right.bx);
                        tmpx = right.bx;
                    }
                    else
                        tmpx = y1rx;
                }
                
                ADD_RIGHT_EDGE (vptr, tptr, offset, size,
                                tmp0, tmp1, tmp2, tmp3,
                                tbase, tsize, &right,
                                r, y0, x2, y1,
                                rx, tmpx);
                rx = r;
            }

            /* Left/Top Edge intersection */
            if (lx < l)
            {
                area = _glitz_pixel_area (l++, y0,
                                          top, bottom,
                                          &left, &right);
                
                ADD_LEFT_SPAN (vptr, tptr, offset, size,
                               tmp0, tmp1,
                               tbase, tsize,
                               lx, y0, l, y1,
                               area);
            }
            
            /* Top Edge */
            while (l < r)
            {
                if (l < lspan || l >= rspan)
                {
                    area = _glitz_pixel_area (l, y0,
                                              top, bottom,
                                              &left, &right);
                    
                    tmpx = l++;
                
                    ADD_PIXEL (vptr, tptr, offset, size,
                               tmp0,
                               tbase, tsize,
                               tmpx, y0, l, y1,
                               area);
                }
                else
                {
                    area = MIN (bottom, y1) - top;
                    ADD_TOP_SPAN (vptr, tptr, offset, size,
                                  tmp0, tmp1,
                                  tbase, tsize,
                                  lspan, y0, rspan, y1,
                                  area);
                    l = rspan;
                }
            }

            /* Top/Right Edge intersection */
            if (rx > r)
            {
                area = _glitz_pixel_area (r, y0,
                                          top, bottom,
                                          &left, &right);

                ADD_RIGHT_SPAN (vptr, tptr, offset, size,
                                tmp0, tmp1,
                                tbase, tsize,
                                r, y0, rx, y1,
                                area);
            }
        }
        else
        {
            y1lx = EDGE_X (&left, y1);
            y1rx = EDGE_X (&right, y1);
        }

        /*
          BOTTOM rectangle
                                       W
          +--+------+---------+------------------+---------+----+----+
          |    \    |         |                  |         |  /      |
          |      \  |         |                  |         |/        |
          |        \|         |                  |        /|         | 1
          |         |\  lEbE  |     bE           | bErE /  |         |
          |   lE    |  +------+------------------|----+    |   rE    |
          |         |         |                  |         |         |
          +---------+---------+------------------+---------+---------+

          The following set sub-rectangles might need to be generated
          to render the bottom span of the trapezoid correctly.
          
          lE   = Left Edge
          bE   = Top Edge
          rE   = Right Edge
          lEbE = Left/Bottom Edge intersection
          bErE = Bottom/Right Edge intersection
        */

        y2 = floorf (bottom);
        if (y2 != bottom && y2 >= y1)
        {
            y3 = ceilf (bottom);
            lx = x1;
            rx = x2;

            if (y2 > y1)
            {
                y2lx = EDGE_X (&left, y2);
                y2rx = EDGE_X (&right, y2);
            }
            else
            {
                y2lx = y1lx;
                y2rx = y1rx;
            }

            l = MAX (left.bx, y2lx);
            r = MIN (right.bx, y2rx);
                    
            lspan = ceilf (l);
            rspan = floorf (r);
            
            l = floorf (l);
            if (l > rspan)
                l = rspan;
            
            r = ceilf (r);
            if (r < lspan)
                r = lspan;

            /* Left Edge */
            if (l > x1)
            {
                lx = EDGE_X (&left, y3);
                if (y2lx > left.bx)
                {
                    tmpx = y2lx + (left.bx - lx);
                    lx = left.bx;
                } else
                    tmpx = y2lx;
                
                ADD_LEFT_EDGE (vptr, tptr, offset, size,
                               tmp0, tmp1, tmp2, tmp3,
                               tbase, tsize, &left,
                               x1, y2, l, y3,
                               tmpx, lx);
                lx = l;
            }

            /* Right Edge */
            if (r < x2)
            {
                rx = EDGE_X (&right, y3);
                if (y2rx < right.bx)
                {
                    tmpx = y2rx - (rx - right.bx);
                    rx = right.bx;
                } else
                    tmpx = y2rx;
                    
                ADD_RIGHT_EDGE (vptr, tptr, offset, size,
                                tmp0, tmp1, tmp2, tmp3,
                                tbase, tsize, &right,
                                r, y2, x2, y3,
                                tmpx, rx);
                rx = r;
            }

            /* Left/Bottom Edge intersection */
            if (lx < l)
            {
                area = _glitz_pixel_area (l++, y2,
                                          top, bottom,
                                          &left, &right);

                ADD_LEFT_SPAN (vptr, tptr, offset, size,
                               tmp0, tmp1,
                               tbase, tsize,
                               lx, y2, l, y3,
                               area);
            }
            
            /* Bottom Edge */
            while (l < r)
            {
                if (l < lspan || l >= rspan)
                {
                    area = _glitz_pixel_area (l, y2,
                                              top, bottom,
                                              &left, &right);
                    
                    tmpx = l++;
                
                    ADD_PIXEL (vptr, tptr, offset, size,
                               tmp0,
                               tbase, tsize,
                               tmpx, y2, l, y3,
                               area);
                }
                else
                {
                    area = bottom - y2;
                    ADD_BOTTOM_SPAN (vptr, tptr, offset, size,
                                     tmp0, tmp1,
                                     tbase, tsize,
                                     lspan, y2, rspan, y3,
                                     area);
                    l = rspan;
                }
            }

            /* Bottom/Right Edge intersection */
            if (rx > r)
            {
                area = _glitz_pixel_area (r, y2,
                                          top, bottom,
                                          &left, &right);

                ADD_RIGHT_SPAN (vptr, tptr, offset, size,
                                tmp0, tmp1,
                                tbase, tsize,
                                r, y2, rx, y3,
                                area);
            }
        }
        else
        {
            y2lx = EDGE_X (&left, y2);
            y2rx = EDGE_X (&right, y2);
        }

        /*
          MIDDLE rectangle
                                      W
          +---+---------------------------------+--------+------+
          |    \                                |       /       |
          |     \                               |      /        |
          |      \              lE              |     /   rE    | H - TH - BH
          |       \                             |    /          |
          |        \                            |   /           |
          +---------+---------------------------+--+------------+

          The following set sub-rectangles might need to be generated
          to render the middle span of the trapezoid correctly.
          
          lE = Left Edge
          rE = Right Edge

          If floor (MIN (rE)) < ceil (MAX (lE)) a number of left and right
          edges needs to be generated as pixels intersected by both edges
          needs to be calculated separately in a middle span.
        */
        if (y1 < y2)
        {
            left.tx = y1lx;
            right.tx = y1rx;

            do {
                if (left.tx > y2rx)
                {
                    rx = lx = ceilf (left.tx);
                    if (right.dx)
                        y = floorf (EDGE_Y (&right, rx));
                    else
                        y = y2;
                }
                else
                {
                    rx = lx = floorf (MIN (right.tx, y2rx));
                    if (left.dx)
                        y = floorf (EDGE_Y (&left, rx));
                    else
                        y = y2;
                }

                if (y == y1)
                    y = y1 + 1.0f;

                if (y > y1 && y < y2)
                {
                    left.bx = EDGE_X (&left, y);
                    right.bx = EDGE_X (&right, y);
                }
                else
                {
                    y = y2;
                    left.bx = y2lx;
                    right.bx = y2rx;
                }

                if (lx > right.tx)
                    lx = floorf (right.tx);

                if (lx > right.bx)
                    lx = floorf (right.bx);

                if (rx < left.tx)
                    rx = ceilf (left.tx);

                if (rx < left.bx)
                    rx = ceilf (left.bx);

                /* Left Edge */
                if (lx > x1)
                {
                    if (left.dx)
                    {
                        ADD_LEFT_EDGE (vptr, tptr, offset, size,
                                       tmp0, tmp1, tmp2, tmp3,
                                       tbase, tsize, &left,
                                       x1, y1, lx, y,
                                       left.tx, left.bx);
                    }
                    else
                    {
                        area = lx - left.x0;
                        ADD_LEFT_SPAN (vptr, tptr, offset, size,
                                       tmp0, tmp1,
                                       tbase, tsize,
                                       x1, y1, lx, y,
                                       area);
                    }
                }

                /* Middle Span */
                while (lx < rx)
                {
                    tmpy = y1;
                    tmpx = lx++;
                    
                    while (tmpy < y)
                    {
                        y0 = tmpy++;
                        area = _glitz_pixel_area (tmpx, y0,
                                                  y0, y2,
                                                  &left, &right);
                    
                        ADD_PIXEL (vptr, tptr, offset, size,
                                   tmp0,
                                   tbase, tsize,
                                   tmpx, y0, lx, tmpy,
                                   area);
                    }
                }
                    
                /* Right Edge */
                if (rx < x2)
                {
                    if (right.dx)
                    {
                        ADD_RIGHT_EDGE (vptr, tptr, offset, size,
                                        tmp0, tmp1, tmp2, tmp3,
                                        tbase, tsize, &right,
                                        rx, y1, x2, y,
                                        right.tx, right.bx);
                    }
                    else
                    {
                        area = right.x0 - rx;
                        ADD_RIGHT_SPAN (vptr, tptr, offset, size,
                                        tmp0, tmp1,
                                        tbase, tsize,
                                        rx, y1, x2, y,
                                        area);
                    }
                }

                left.tx = left.bx;
                right.tx = right.bx;
                y1 = y;
            } while (y < y2);
        }

        toff = offset;
    }

    return toff;
}

#undef ADD_RIGHT_EDGE
#undef ADD_LEFT_EDGE
#undef CALC_RIGHT_EDGE
#undef CALC_LEFT_EDGE
#undef CALC_EDGE
#undef ADD_BOTTOM_SPAN
#undef ADD_TOP_SPAN
#undef ADD_RIGHT_SPAN
#undef ADD_LEFT_SPAN
#undef CALC_SPAN
#undef ADD_PIXEL
#undef ADD_QUAD
#undef ADD_VERTEX
#undef TSKIP
#undef VSKIP
#undef BYTES_PER_QUAD
#undef BYTES_PER_VERTEX
