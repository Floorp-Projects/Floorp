/*
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *             2005 Lars Knoll & Zack Rusin, Trolltech
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "pixman-private.h"

void
_pixman_gradient_walker_init (pixman_gradient_walker_t *walker,
                              gradient_t *              gradient,
                              pixman_repeat_t		repeat)
{
    walker->num_stops = gradient->n_stops;
    walker->stops     = gradient->stops;
    walker->left_x    = 0;
    walker->right_x   = 0x10000;
    walker->stepper   = 0;
    walker->left_ag   = 0;
    walker->left_rb   = 0;
    walker->right_ag  = 0;
    walker->right_rb  = 0;
    walker->repeat    = repeat;

    walker->need_reset = TRUE;
}

static void
gradient_walker_reset (pixman_gradient_walker_t *walker,
		       pixman_fixed_48_16_t      pos)
{
    int32_t x, left_x, right_x;
    pixman_color_t *left_c, *right_c;
    int n, count = walker->num_stops;
    pixman_gradient_stop_t *stops = walker->stops;

    if (walker->repeat == PIXMAN_REPEAT_NORMAL)
    {
	x = (int32_t)pos & 0xffff;
    }
    else if (walker->repeat == PIXMAN_REPEAT_REFLECT)
    {
	x = (int32_t)pos & 0xffff;
	if ((int32_t)pos & 0x10000)
	    x = 0x10000 - x;
    }
    else
    {
	x = pos;
    }
    
    for (n = 0; n < count; n++)
    {
	if (x < stops[n].x)
	    break;
    }
    
    left_x =  stops[n - 1].x;
    left_c = &stops[n - 1].color;
    
    right_x =  stops[n].x;
    right_c = &stops[n].color;

    if (walker->repeat == PIXMAN_REPEAT_NORMAL)
    {
	left_x  += (pos - x);
	right_x += (pos - x);
    }
    else if (walker->repeat == PIXMAN_REPEAT_REFLECT)
    {
	if ((int32_t)pos & 0x10000)
	{
	    pixman_color_t  *tmp_c;
	    int32_t tmp_x;

	    tmp_x   = 0x10000 - right_x;
	    right_x = 0x10000 - left_x;
	    left_x  = tmp_x;

	    tmp_c   = right_c;
	    right_c = left_c;
	    left_c  = tmp_c;

	    x = 0x10000 - x;
	}
	left_x  += (pos - x);
	right_x += (pos - x);
    }
    else if (walker->repeat == PIXMAN_REPEAT_NONE)
    {
	if (n == 0)
	    right_c = left_c;
	else if (n == count)
	    left_c = right_c;
    }

    walker->left_x   = left_x;
    walker->right_x  = right_x;
    walker->left_ag  = ((left_c->alpha >> 8) << 16)   | (left_c->green >> 8);
    walker->left_rb  = ((left_c->red & 0xff00) << 8)  | (left_c->blue >> 8);
    walker->right_ag = ((right_c->alpha >> 8) << 16)  | (right_c->green >> 8);
    walker->right_rb = ((right_c->red & 0xff00) << 8) | (right_c->blue >> 8);

    if (walker->left_x == walker->right_x                ||
        (walker->left_ag == walker->right_ag &&
	 walker->left_rb == walker->right_rb))
    {
	walker->stepper = 0;
    }
    else
    {
	int32_t width = right_x - left_x;
	walker->stepper = ((1 << 24) + width / 2) / width;
    }

    walker->need_reset = FALSE;
}

uint32_t
_pixman_gradient_walker_pixel (pixman_gradient_walker_t *walker,
                               pixman_fixed_48_16_t      x)
{
    int dist, idist;
    uint32_t t1, t2, a, color;

    if (walker->need_reset || x < walker->left_x || x >= walker->right_x)
	gradient_walker_reset (walker, x);

    dist  = ((int)(x - walker->left_x) * walker->stepper) >> 16;
    idist = 256 - dist;

    /* combined INTERPOLATE and premultiply */
    t1 = walker->left_rb * idist + walker->right_rb * dist;
    t1 = (t1 >> 8) & 0xff00ff;

    t2  = walker->left_ag * idist + walker->right_ag * dist;
    t2 &= 0xff00ff00;

    color = t2 & 0xff000000;
    a     = t2 >> 24;

    t1  = t1 * a + 0x800080;
    t1  = (t1 + ((t1 >> 8) & 0xff00ff)) >> 8;

    t2  = (t2 >> 8) * a + 0x800080;
    t2  = (t2 + ((t2 >> 8) & 0xff00ff));

    return (color | (t1 & 0xff00ff) | (t2 & 0xff00));
}

