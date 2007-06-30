/*
 * Copyright © 2002 Keith Packard
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

#include "pixmanint.h"

#include <assert.h>

pixman_image_t *
FbCreateAlphaPicture (pixman_image_t	*dst,
		      pixman_format_t	*format,
		      uint16_t	width,
		      uint16_t	height)
{
    pixman_format_t	 local_format;

    if (width > 32767 || height > 32767)
	return NULL;

    if (!format)
    {
	int ret;
	format = &local_format;
	if (dst->polyEdge == PolyEdgeSharp)
	     ret = pixman_format_init (format, PIXMAN_FORMAT_NAME_A1);
	else
	     ret = pixman_format_init (format, PIXMAN_FORMAT_NAME_A8);
	assert (ret);
    }

    /* pixman_image_create zeroes out the pixels, so we don't have to */
    return pixman_image_create (format, width, height);
}

static pixman_fixed16_16_t
pixman_line_fixed_x (const pixman_line_fixed_t *l, pixman_fixed16_16_t y, int ceil)
{
    pixman_fixed16_16_t    dx = l->p2.x - l->p1.x;
    xFixed_32_32    ex = (xFixed_32_32) (y - l->p1.y) * dx;
    pixman_fixed16_16_t    dy = l->p2.y - l->p1.y;
    if (ceil)
	ex += (dy - 1);
    return l->p1.x + (pixman_fixed16_16_t) (ex / dy);
}

static void
pixman_trapezoid_bounds (int ntrap, const pixman_trapezoid_t *traps, pixman_box16_t *box)
{
    box->y1 = MAXSHORT;
    box->y2 = MINSHORT;
    box->x1 = MAXSHORT;
    box->x2 = MINSHORT;
    for (; ntrap; ntrap--, traps++)
    {
	int16_t x1, y1, x2, y2;

	if (!xTrapezoidValid(traps))
	    continue;
	y1 = xFixedToInt (traps->top);
	if (y1 < box->y1)
	    box->y1 = y1;

	y2 = xFixedToInt (xFixedCeil (traps->bottom));
	if (y2 > box->y2)
	    box->y2 = y2;

	x1 = xFixedToInt (MIN (pixman_line_fixed_x (&traps->left, traps->top, 0),
			       pixman_line_fixed_x (&traps->left, traps->bottom, 0)));
	if (x1 < box->x1)
	    box->x1 = x1;

	x2 = xFixedToInt (xFixedCeil (MAX (pixman_line_fixed_x (&traps->right, traps->top, 1),
					   pixman_line_fixed_x (&traps->right, traps->bottom, 1))));
	if (x2 > box->x2)
	    box->x2 = x2;
    }
}

int
pixman_composite_trapezoids (pixman_operator_t	      op,
			     pixman_image_t	      *src,
			     pixman_image_t	      *dst,
			     int		      xSrc,
			     int		      ySrc,
			     const pixman_trapezoid_t *traps,
			     int		      ntraps)
{
    pixman_image_t		*image = NULL;
    pixman_box16_t		 traps_bounds, dst_bounds, bounds;
    pixman_region16_t		 traps_region, dst_region;
    int16_t			 xDst, yDst;
    int16_t			 xRel, yRel;
    pixman_format_t		 format;
    pixman_region_status_t	 status;
    int                          ret;

    if (ntraps == 0)
	return 0;

    /*
     * Check for solid alpha add
     */
    if (op == PIXMAN_OPERATOR_ADD && miIsSolidAlpha (src))
    {
	for (; ntraps; ntraps--, traps++)
	    fbRasterizeTrapezoid (dst, traps, 0, 0);
	return 0;
    }

    xDst = traps[0].left.p1.x >> 16;
    yDst = traps[0].left.p1.y >> 16;

    pixman_trapezoid_bounds (ntraps, traps, &traps_bounds);
    pixman_region_init_with_extents (&traps_region, &traps_bounds);

    /* XXX: If the image has a clip region set, we should really be
     * fetching it here instead, but it looks like we don't yet expose
     * a pixman_image_get_clip_region function. */
    dst_bounds.x1 = 0;
    dst_bounds.y1 = 0;
    dst_bounds.x2 = pixman_image_get_width (dst);
    dst_bounds.y2 = pixman_image_get_height (dst);

    pixman_region_init_with_extents (&dst_region, &dst_bounds);
    status = pixman_region_intersect (&traps_region, &traps_region, &dst_region);
    if (status != PIXMAN_REGION_STATUS_SUCCESS) {
	pixman_region_fini (&traps_region);
	pixman_region_fini (&dst_region);
	return 1;
    }

    bounds = *(pixman_region_extents (&traps_region));

    pixman_region_fini (&traps_region);
    pixman_region_fini (&dst_region);

    if (bounds.y1 >= bounds.y2 || bounds.x1 >= bounds.x2)
	return 0;

    ret = pixman_format_init (&format, PIXMAN_FORMAT_NAME_A8);
    assert (ret);

    image = FbCreateAlphaPicture (dst, &format,
				  bounds.x2 - bounds.x1,
				  bounds.y2 - bounds.y1);
    if (!image)
	return 1;

    for (; ntraps; ntraps--, traps++)
    {
	if (!xTrapezoidValid(traps))
	    continue;
	fbRasterizeTrapezoid (image, traps,
			      -bounds.x1, -bounds.y1);
    }

    xRel = bounds.x1 + xSrc - xDst;
    yRel = bounds.y1 + ySrc - yDst;
    pixman_composite (op, src, image, dst,
		      xRel, yRel, 0, 0, bounds.x1, bounds.y1,
		      bounds.x2 - bounds.x1,
		      bounds.y2 - bounds.y1);
    pixman_image_destroy (image);

    return 0;
}

void
pixman_add_trapezoids (pixman_image_t		*dst,
		       int			x_off,
		       int			y_off,
		       const pixman_trapezoid_t	*traps,
		       int			ntraps)
{
    for (; ntraps; ntraps--, traps++)
    {
	if (!xTrapezoidValid (traps))
	    continue;

	fbRasterizeTrapezoid (dst, traps, x_off, y_off);
    }
}
