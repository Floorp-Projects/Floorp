/*
 * Id: $
 *
 * Copyright © 1998 Keith Packard
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

#include "icint.h"

#ifndef ICNOPIXADDR
/*
 * This is a slight abuse of the preprocessor to generate repetitive
 * code, the idea is to generate code for each case of a copy-mode
 * transparent stipple
 */
#define LaneCases1(c,a)	    case c: \
				while (n--) { (void)IcLaneCase(c,a); a++; } \
				break
#define LaneCases2(c,a)	    LaneCases1(c,a); LaneCases1(c+1,a)
#define LaneCases4(c,a)	    LaneCases2(c,a); LaneCases2(c+2,a)
#define LaneCases8(c,a)	    LaneCases4(c,a); LaneCases4(c+4,a)
#define LaneCases16(c,a)    LaneCases8(c,a); LaneCases8(c+8,a)
#define LaneCases32(c,a)    LaneCases16(c,a); LaneCases16(c+16,a)
#define LaneCases64(c,a)    LaneCases32(c,a); LaneCases32(c+32,a)
#define LaneCases128(c,a)   LaneCases64(c,a); LaneCases64(c+64,a)
#define LaneCases256(c,a)   LaneCases128(c,a); LaneCases128(c+128,a)
    
#if IC_SHIFT == 6
#define LaneCases(a)	    LaneCases256(0,a)
#endif
    
#if IC_SHIFT == 5
#define LaneCases(a)	    LaneCases16(0,a)
#endif
							   
/*
 * Repeat a transparent stipple across a scanline n times
 */

void
IcTransparentSpan (pixman_bits_t   *dst,
		   pixman_bits_t   stip,
		   pixman_bits_t   fgxor,
		   int	    n)
{
    IcStip  s;

    s  = ((IcStip) (stip      ) & 0x01);
    s |= ((IcStip) (stip >>  8) & 0x02);
    s |= ((IcStip) (stip >> 16) & 0x04);
    s |= ((IcStip) (stip >> 24) & 0x08);
#if IC_SHIFT > 5
    s |= ((IcStip) (stip >> 32) & 0x10);
    s |= ((IcStip) (stip >> 40) & 0x20);
    s |= ((IcStip) (stip >> 48) & 0x40);
    s |= ((IcStip) (stip >> 56) & 0x80);
#endif
    switch (s) {
	LaneCases(dst);
    }
}
#endif
