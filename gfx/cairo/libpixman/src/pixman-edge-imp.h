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

#ifndef rasterizeSpan
#endif

static void
rasterizeEdges (pixman_image_t  *image,
		pixman_edge_t	*l,
		pixman_edge_t	*r,
		pixman_fixed_t		t,
		pixman_fixed_t		b)
{
    pixman_fixed_t  y = t;
    uint32_t  *line;
    uint32_t *buf = (image)->bits.bits;
    int stride = (image)->bits.rowstride;
    int width = (image)->bits.width;

    line = buf + pixman_fixed_to_int (y) * stride;

    for (;;)
    {
	pixman_fixed_t	lx;
	pixman_fixed_t      rx;
	int	lxi;
	int rxi;

	lx = l->x;
	rx = r->x;
#if N_BITS == 1
	/* For the non-antialiased case, round the coordinates up, in effect
	 * sampling the center of the pixel. (The AA case does a similar 
	 * adjustment in RenderSamplesX) */
	lx += X_FRAC_FIRST(1);
	rx += X_FRAC_FIRST(1);
#endif
	/* clip X */
	if (lx < 0)
	    lx = 0;
	if (pixman_fixed_to_int (rx) >= width)
#if N_BITS == 1
	    rx = pixman_int_to_fixed (width);
#else
	    /* Use the last pixel of the scanline, covered 100%.
	     * We can't use the first pixel following the scanline,
	     * because accessing it could result in a buffer overrun.
	     */
	    rx = pixman_int_to_fixed (width) - 1;
#endif

	/* Skip empty (or backwards) sections */
	if (rx > lx)
	{

	    /* Find pixel bounds for span */
	    lxi = pixman_fixed_to_int (lx);
	    rxi = pixman_fixed_to_int (rx);

#if N_BITS == 1
	    {
		uint32_t  *a = line;
		uint32_t  startmask;
		uint32_t  endmask;
		int	    nmiddle;
		int	    width = rxi - lxi;
		int	    x = lxi;

		a += x >> FB_SHIFT;
		x &= FB_MASK;

		FbMaskBits (x, width, startmask, nmiddle, endmask);
		    if (startmask) {
			WRITE(image, a, READ(image, a) | startmask);
			a++;
		    }
		    while (nmiddle--)
			WRITE(image, a++, FB_ALLONES);
		    if (endmask)
			WRITE(image, a, READ(image, a) | endmask);
	    }
#else
	    {
		DefineAlpha(line,lxi);
		int	    lxs;
		int     rxs;

		/* Sample coverage for edge pixels */
		lxs = RenderSamplesX (lx, N_BITS);
		rxs = RenderSamplesX (rx, N_BITS);

		/* Add coverage across row */
		if (lxi == rxi)
		{
		    AddAlpha (rxs - lxs);
		}
		else
		{
		    int	xi;

		    AddAlpha (N_X_FRAC(N_BITS) - lxs);
		    StepAlpha;
		    for (xi = lxi + 1; xi < rxi; xi++)
		    {
			AddAlpha (N_X_FRAC(N_BITS));
			StepAlpha;
		    }
		    AddAlpha (rxs);
		}
	    }
#endif
	}

	if (y == b)
	    break;

#if N_BITS > 1
	if (pixman_fixed_frac (y) != Y_FRAC_LAST(N_BITS))
	{
	    RenderEdgeStepSmall (l);
	    RenderEdgeStepSmall (r);
	    y += STEP_Y_SMALL(N_BITS);
	}
	else
#endif
	{
	    RenderEdgeStepBig (l);
	    RenderEdgeStepBig (r);
	    y += STEP_Y_BIG(N_BITS);
	    line += stride;
	}
    }
}

#undef rasterizeSpan
