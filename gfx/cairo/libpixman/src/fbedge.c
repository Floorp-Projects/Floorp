/*
 * $Id: fbedge.c,v 1.3 2005/06/04 07:03:28 vladimir%pobox.com Exp $
 *
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

#include "pixman-xserver-compat.h"

#ifdef RENDER

/*
 * 8 bit alpha
 */

#define N_BITS	8
#define rasterizeEdges   fbRasterizeEdges8

#define DefineAlpha(line,x) \
    CARD8	*__ap = (CARD8 *) line + (x)

#define StepAlpha	__ap++

#define AddAlpha(a) {				    \
    CARD16 __a = a + *__ap;			    \
    *__ap = ((CARD8) ((__a) | (0 - ((__a) >> 8)))); \
}

#include "fbedgeimp.h"

#undef AddAlpha
#undef StepAlpha
#undef DefineAlpha
#undef rasterizeEdges
#undef N_BITS

/*
 * 4 bit alpha
 */

#define N_BITS	4
#define rasterizeEdges	fbRasterizeEdges4

#if BITMAP_BIT_ORDER == LSBFirst
#define Shift4(o)	((o) << 2)
#else
#define Shift4(o)	((1-(o)) << 2)
#endif

#define Get4(x,o)	(((x) >> Shift4(o)) & 0xf)
#define Put4(x,o,v)	(((x) & ~(0xf << Shift4(o))) | (((v) & 0xf) << Shift4(o)))

#define DefineAlpha(line,x) \
    CARD8   *__ap = (CARD8 *) line + ((x) >> 1); \
    int	    __ao = (x) & 1

#define StepAlpha	((__ap += __ao), (__ao ^= 1))

#define AddAlpha(a) {						\
    CARD8   __o = *__ap;					\
    CARD8   __a = (a) + Get4(__o, __ao);			\
    *__ap = Put4 (__o, __ao, __a | (0 - ((__a) >> 4)));		\
}

#include "fbedgeimp.h"

#undef AddAlpha
#undef StepAlpha
#undef DefineAlpha
#undef rasterizeEdges
#undef N_BITS


/*
 * 1 bit alpha
 */

#define N_BITS 1
#define rasterizeEdges	fbRasterizeEdges1

#include "fbedgeimp.h"

#undef rasterizeEdges
#undef N_BITS

void
fbRasterizeEdges (FbBits	*buf,
		  int		bpp,
		  int		width,
		  int		stride,
		  RenderEdge	*l,
		  RenderEdge	*r,
		  xFixed	t,
		  xFixed	b)
{
    switch (bpp) {
    case 1:
	fbRasterizeEdges1 (buf, width, stride, l, r, t, b);
	break;
    case 4:
	fbRasterizeEdges4 (buf, width, stride, l, r, t, b);
	break;
    case 8:
	fbRasterizeEdges8 (buf, width, stride, l, r, t, b);
	break;
    }
}

#endif /* RENDER */
