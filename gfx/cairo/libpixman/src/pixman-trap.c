/*
 * Copyright © 2004 Keith Packard
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
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "pixman-private.h"

/*
 * Compute the smallest value greater than or equal to y which is on a
 * grid row.
 */

PIXMAN_EXPORT pixman_fixed_t
pixman_sample_ceil_y (pixman_fixed_t y, int n)
{
    pixman_fixed_t f = pixman_fixed_frac (y);
    pixman_fixed_t i = pixman_fixed_floor (y);

    f = DIV (f - Y_FRAC_FIRST (n) + (STEP_Y_SMALL (n) - pixman_fixed_e), STEP_Y_SMALL (n)) * STEP_Y_SMALL (n) +
	Y_FRAC_FIRST (n);
    
    if (f > Y_FRAC_LAST (n))
    {
	if (pixman_fixed_to_int (i) == 0x7fff)
	{
	    f = 0xffff; /* saturate */
	}
	else
	{
	    f = Y_FRAC_FIRST (n);
	    i += pixman_fixed_1;
	}
    }
    return (i | f);
}

/*
 * Compute the largest value strictly less than y which is on a
 * grid row.
 */
PIXMAN_EXPORT pixman_fixed_t
pixman_sample_floor_y (pixman_fixed_t y,
                       int            n)
{
    pixman_fixed_t f = pixman_fixed_frac (y);
    pixman_fixed_t i = pixman_fixed_floor (y);

    f = DIV (f - pixman_fixed_e - Y_FRAC_FIRST (n), STEP_Y_SMALL (n)) * STEP_Y_SMALL (n) +
	Y_FRAC_FIRST (n);

    if (f < Y_FRAC_FIRST (n))
    {
	if (pixman_fixed_to_int (i) == 0x8000)
	{
	    f = 0; /* saturate */
	}
	else
	{
	    f = Y_FRAC_LAST (n);
	    i -= pixman_fixed_1;
	}
    }
    return (i | f);
}

/*
 * Step an edge by any amount (including negative values)
 */
PIXMAN_EXPORT void
pixman_edge_step (pixman_edge_t *e,
                  int            n)
{
    pixman_fixed_48_16_t ne;

    e->x += n * e->stepx;

    ne = e->e + n * (pixman_fixed_48_16_t) e->dx;

    if (n >= 0)
    {
	if (ne > 0)
	{
	    int nx = (ne + e->dy - 1) / e->dy;
	    e->e = ne - nx * (pixman_fixed_48_16_t) e->dy;
	    e->x += nx * e->signdx;
	}
    }
    else
    {
	if (ne <= -e->dy)
	{
	    int nx = (-ne) / e->dy;
	    e->e = ne + nx * (pixman_fixed_48_16_t) e->dy;
	    e->x -= nx * e->signdx;
	}
    }
}

/*
 * A private routine to initialize the multi-step
 * elements of an edge structure
 */
static void
_pixman_edge_multi_init (pixman_edge_t * e,
                         int             n,
                         pixman_fixed_t *stepx_p,
                         pixman_fixed_t *dx_p)
{
    pixman_fixed_t stepx;
    pixman_fixed_48_16_t ne;

    ne = n * (pixman_fixed_48_16_t) e->dx;
    stepx = n * e->stepx;

    if (ne > 0)
    {
	int nx = ne / e->dy;
	ne -= nx * e->dy;
	stepx += nx * e->signdx;
    }

    *dx_p = ne;
    *stepx_p = stepx;
}

/*
 * Initialize one edge structure given the line endpoints and a
 * starting y value
 */
PIXMAN_EXPORT void
pixman_edge_init (pixman_edge_t *e,
                  int            n,
                  pixman_fixed_t y_start,
                  pixman_fixed_t x_top,
                  pixman_fixed_t y_top,
                  pixman_fixed_t x_bot,
                  pixman_fixed_t y_bot)
{
    pixman_fixed_t dx, dy;

    e->x = x_top;
    e->e = 0;
    dx = x_bot - x_top;
    dy = y_bot - y_top;
    e->dy = dy;
    e->dx = 0;

    if (dy)
    {
	if (dx >= 0)
	{
	    e->signdx = 1;
	    e->stepx = dx / dy;
	    e->dx = dx % dy;
	    e->e = -dy;
	}
	else
	{
	    e->signdx = -1;
	    e->stepx = -(-dx / dy);
	    e->dx = -dx % dy;
	    e->e = 0;
	}

	_pixman_edge_multi_init (e, STEP_Y_SMALL (n),
				 &e->stepx_small, &e->dx_small);

	_pixman_edge_multi_init (e, STEP_Y_BIG (n),
				 &e->stepx_big, &e->dx_big);
    }
    pixman_edge_step (e, y_start - y_top);
}

/*
 * Initialize one edge structure given a line, starting y value
 * and a pixel offset for the line
 */
PIXMAN_EXPORT void
pixman_line_fixed_edge_init (pixman_edge_t *            e,
                             int                        n,
                             pixman_fixed_t             y,
                             const pixman_line_fixed_t *line,
                             int                        x_off,
                             int                        y_off)
{
    pixman_fixed_t x_off_fixed = pixman_int_to_fixed (x_off);
    pixman_fixed_t y_off_fixed = pixman_int_to_fixed (y_off);
    const pixman_point_fixed_t *top, *bot;

    if (line->p1.y <= line->p2.y)
    {
	top = &line->p1;
	bot = &line->p2;
    }
    else
    {
	top = &line->p2;
	bot = &line->p1;
    }
    
    pixman_edge_init (e, n, y,
                      top->x + x_off_fixed,
                      top->y + y_off_fixed,
                      bot->x + x_off_fixed,
                      bot->y + y_off_fixed);
}

PIXMAN_EXPORT void
pixman_add_traps (pixman_image_t * image,
                  int16_t          x_off,
                  int16_t          y_off,
                  int              ntrap,
                  pixman_trap_t *  traps)
{
    int bpp;
    int width;
    int height;

    pixman_fixed_t x_off_fixed;
    pixman_fixed_t y_off_fixed;
    pixman_edge_t l, r;
    pixman_fixed_t t, b;

    _pixman_image_validate (image);
    
    width = image->bits.width;
    height = image->bits.height;
    bpp = PIXMAN_FORMAT_BPP (image->bits.format);

    x_off_fixed = pixman_int_to_fixed (x_off);
    y_off_fixed = pixman_int_to_fixed (y_off);

    while (ntrap--)
    {
	t = traps->top.y + y_off_fixed;
	if (t < 0)
	    t = 0;
	t = pixman_sample_ceil_y (t, bpp);

	b = traps->bot.y + y_off_fixed;
	if (pixman_fixed_to_int (b) >= height)
	    b = pixman_int_to_fixed (height) - 1;
	b = pixman_sample_floor_y (b, bpp);

	if (b >= t)
	{
	    /* initialize edge walkers */
	    pixman_edge_init (&l, bpp, t,
	                      traps->top.l + x_off_fixed,
	                      traps->top.y + y_off_fixed,
	                      traps->bot.l + x_off_fixed,
	                      traps->bot.y + y_off_fixed);

	    pixman_edge_init (&r, bpp, t,
	                      traps->top.r + x_off_fixed,
	                      traps->top.y + y_off_fixed,
	                      traps->bot.r + x_off_fixed,
	                      traps->bot.y + y_off_fixed);

	    pixman_rasterize_edges (image, &l, &r, t, b);
	}

	traps++;
    }
}

#if 0
static void
dump_image (pixman_image_t *image,
            const char *    title)
{
    int i, j;

    if (!image->type == BITS)
	printf ("%s is not a regular image\n", title);

    if (!image->bits.format == PIXMAN_a8)
	printf ("%s is not an alpha mask\n", title);

    printf ("\n\n\n%s: \n", title);

    for (i = 0; i < image->bits.height; ++i)
    {
	uint8_t *line =
	    (uint8_t *)&(image->bits.bits[i * image->bits.rowstride]);

	for (j = 0; j < image->bits.width; ++j)
	    printf ("%c", line[j] ? '#' : ' ');

	printf ("\n");
    }
}
#endif

PIXMAN_EXPORT void
pixman_add_trapezoids (pixman_image_t *          image,
                       int16_t                   x_off,
                       int                       y_off,
                       int                       ntraps,
                       const pixman_trapezoid_t *traps)
{
    int i;

#if 0
    dump_image (image, "before");
#endif

    for (i = 0; i < ntraps; ++i)
    {
	const pixman_trapezoid_t *trap = &(traps[i]);

	if (!pixman_trapezoid_valid (trap))
	    continue;

	pixman_rasterize_trapezoid (image, trap, x_off, y_off);
    }

#if 0
    dump_image (image, "after");
#endif
}

PIXMAN_EXPORT void
pixman_rasterize_trapezoid (pixman_image_t *          image,
                            const pixman_trapezoid_t *trap,
                            int                       x_off,
                            int                       y_off)
{
    int bpp;
    int width;
    int height;

    pixman_fixed_t x_off_fixed;
    pixman_fixed_t y_off_fixed;
    pixman_edge_t l, r;
    pixman_fixed_t t, b;

    return_if_fail (image->type == BITS);

    _pixman_image_validate (image);
    
    if (!pixman_trapezoid_valid (trap))
	return;

    width = image->bits.width;
    height = image->bits.height;
    bpp = PIXMAN_FORMAT_BPP (image->bits.format);

    x_off_fixed = pixman_int_to_fixed (x_off);
    y_off_fixed = pixman_int_to_fixed (y_off);

    t = trap->top + y_off_fixed;
    if (t < 0)
	t = 0;
    t = pixman_sample_ceil_y (t, bpp);

    b = trap->bottom + y_off_fixed;
    if (pixman_fixed_to_int (b) >= height)
	b = pixman_int_to_fixed (height) - 1;
    b = pixman_sample_floor_y (b, bpp);
    
    if (b >= t)
    {
	/* initialize edge walkers */
	pixman_line_fixed_edge_init (&l, bpp, t, &trap->left, x_off, y_off);
	pixman_line_fixed_edge_init (&r, bpp, t, &trap->right, x_off, y_off);

	pixman_rasterize_edges (image, &l, &r, t, b);
    }
}
