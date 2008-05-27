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

typedef uint32_t FbBits;

void
pixman_add_traps (pixman_image_t *	image,
		  int16_t	x_off,
		  int16_t	y_off,
		  int		ntrap,
		  pixman_trap_t *traps)
{
    int		bpp;
    int		width;
    int		height;

    pixman_fixed_t	x_off_fixed;
    pixman_fixed_t	y_off_fixed;
    pixman_edge_t  l, r;
    pixman_fixed_t	t, b;

    width = image->bits.width;
    height = image->bits.height;
    bpp = PIXMAN_FORMAT_BPP (image->bits.format);
    
    x_off_fixed = pixman_int_to_fixed(x_off);
    y_off_fixed = pixman_int_to_fixed(y_off);

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

static void
dump_image (pixman_image_t *image,
	    const char *title)
{
    int i, j;
    
    if (!image->type == BITS)
    {
	printf ("%s is not a regular image\n", title);
    }

    if (!image->bits.format == PIXMAN_a8)
    {
	printf ("%s is not an alpha mask\n", title);
    }

    printf ("\n\n\n%s: \n", title);
    
    for (i = 0; i < image->bits.height; ++i)
    {
	uint8_t *line =
	    (uint8_t *)&(image->bits.bits[i * image->bits.rowstride]);
	    
	for (j = 0; j < image->bits.width; ++j)
	    printf ("%c", line[j]? '#' : ' ');

	printf ("\n");
    }
}

void
pixman_add_trapezoids       (pixman_image_t      *image,
			     int16_t              x_off,
			     int                      y_off,
			     int                      ntraps,
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

void
pixman_rasterize_trapezoid (pixman_image_t *    image,
			    const pixman_trapezoid_t *trap,
			    int			x_off,
			    int			y_off)
{
    int		bpp;
    int		width;
    int		height;

    pixman_fixed_t	x_off_fixed;
    pixman_fixed_t	y_off_fixed;
    pixman_edge_t	l, r;
    pixman_fixed_t	t, b;

    return_if_fail (image->type == BITS);
    
    if (!pixman_trapezoid_valid (trap))
	return;

    width = image->bits.width;
    height = image->bits.height;
    bpp = PIXMAN_FORMAT_BPP (image->bits.format);
    
    x_off_fixed = pixman_int_to_fixed(x_off);
    y_off_fixed = pixman_int_to_fixed(y_off);
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

#if 0
static int
_GreaterY (pixman_point_fixed_t *a, pixman_point_fixed_t *b)
{
    if (a->y == b->y)
	return a->x > b->x;
    return a->y > b->y;
}

/*
 * Note that the definition of this function is a bit odd because
 * of the X coordinate space (y increasing downwards).
 */
static int
_Clockwise (pixman_point_fixed_t *ref, pixman_point_fixed_t *a, pixman_point_fixed_t *b)
{
    pixman_point_fixed_t	ad, bd;

    ad.x = a->x - ref->x;
    ad.y = a->y - ref->y;
    bd.x = b->x - ref->x;
    bd.y = b->y - ref->y;

    return ((pixman_fixed_32_32_t) bd.y * ad.x - (pixman_fixed_32_32_t) ad.y * bd.x) < 0;
}

/* FIXME -- this could be made more efficient */
void
fbAddTriangles (pixman_image_t *  pPicture,
		int16_t	    x_off,
		int16_t	    y_off,
		int	    ntri,
		xTriangle *tris)
{
    pixman_point_fixed_t	  *top, *left, *right, *tmp;
    xTrapezoid	    trap;

    for (; ntri; ntri--, tris++)
    {
	top = &tris->p1;
	left = &tris->p2;
	right = &tris->p3;
	if (_GreaterY (top, left)) {
	    tmp = left; left = top; top = tmp;
	}
	if (_GreaterY (top, right)) {
	    tmp = right; right = top; top = tmp;
	}
	if (_Clockwise (top, right, left)) {
	    tmp = right; right = left; left = tmp;
	}
	
	/*
	 * Two cases:
	 *
	 *		+		+
	 *	       / \             / \
	 *	      /   \           /   \
	 *	     /     +         +     \
	 *      /    --           --    \
	 *     /   --               --   \
	 *    / ---                   --- \
	 *	 +--                         --+
	 */
	
	trap.top = top->y;
	trap.left.p1 = *top;
	trap.left.p2 = *left;
	trap.right.p1 = *top;
	trap.right.p2 = *right;
	if (right->y < left->y)
	    trap.bottom = right->y;
	else
	    trap.bottom = left->y;
	fbRasterizeTrapezoid (pPicture, &trap, x_off, y_off);
	if (right->y < left->y)
	{
	    trap.top = right->y;
	    trap.bottom = left->y;
	    trap.right.p1 = *right;
	    trap.right.p2 = *left;
	}
	else
	{
	    trap.top = left->y;
	    trap.bottom = right->y;
	    trap.left.p1 = *left;
	    trap.left.p2 = *right;
	}
	fbRasterizeTrapezoid (pPicture, &trap, x_off, y_off);
    }
}
#endif
