/*
 * Copyright © 2008 Mozilla Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Mozilla Corporation not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Mozilla Corporation makes no
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
 *
 * Author:  Jeff Muizelaar (jeff@infidigm.net)
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "pixman-arm-simd.h"

void
fbCompositeSrcAdd_8000x8000arm (pixman_op_t op,
				pixman_image_t * pSrc,
				pixman_image_t * pMask,
				pixman_image_t * pDst,
				int16_t      xSrc,
				int16_t      ySrc,
				int16_t      xMask,
				int16_t      yMask,
				int16_t      xDst,
				int16_t      yDst,
				uint16_t     width,
				uint16_t     height)
{
    uint8_t	*dstLine, *dst;
    uint8_t	*srcLine, *src;
    int	dstStride, srcStride;
    uint16_t	w;
    uint8_t	s, d;

    fbComposeGetStart (pSrc, xSrc, ySrc, uint8_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

        /* ensure both src and dst are properly aligned before doing 32 bit reads
         * we'll stay in this loop if src and dst have differing alignments */
	while (w && (((unsigned long)dst & 3) || ((unsigned long)src & 3)))
	{
	    s = *src;
	    d = *dst;
	    asm("uqadd8 %0, %1, %2" : "+r"(d) : "r"(s));
	    *dst = d;

	    dst++;
	    src++;
	    w--;
	}

	while (w >= 4)
	{
	    asm("uqadd8 %0, %1, %2" : "=r"(*(uint32_t*)dst) : "r"(*(uint32_t*)src), "r"(*(uint32_t*)dst));
	    dst += 4;
	    src += 4;
	    w -= 4;
	}

	while (w)
	{
	    s = *src;
	    d = *dst;
	    asm("uqadd8 %0, %1, %2" : "+r"(d) : "r"(s));
	    *dst = d;

	    dst++;
	    src++;
	    w--;
	}
    }

}

void
fbCompositeSrc_8888x8888arm (pixman_op_t op,
			 pixman_image_t * pSrc,
			 pixman_image_t * pMask,
			 pixman_image_t * pDst,
			 int16_t      xSrc,
			 int16_t      ySrc,
			 int16_t      xMask,
			 int16_t      yMask,
			 int16_t      xDst,
			 int16_t      yDst,
			 uint16_t     width,
			 uint16_t     height)
{
    uint32_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    int	dstStride, srcStride;
    uint16_t	w;
    uint32_t component_half = 0x800080;
    uint32_t upper_component_mask = 0xff00ff00;
    uint32_t alpha_mask = 0xff;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

//#define inner_branch
	asm volatile (
			"cmp %[w], #0\n\t"
			"beq 2f\n\t"
			"1:\n\t"
			/* load src */
			"ldr r5, [%[src]], #4\n\t"
#ifdef inner_branch
			/* We can avoid doing the multiplication in two cases: 0x0 or 0xff.
			 * The 0x0 case also allows us to avoid doing an unecessary data
			 * write which is more valuable so we only check for that */
			"cmp r5, #0\n\t"
			"beq 3f\n\t"

			/* = 255 - alpha */
			"sub r8, %[alpha_mask], r5, lsr #24\n\t"

			"ldr r4, [%[dest]] \n\t"

#else
			"ldr r4, [%[dest]] \n\t"

			/* = 255 - alpha */
			"sub r8, %[alpha_mask], r5, lsr #24\n\t"
#endif
			"uxtb16 r6, r4\n\t"
			"uxtb16 r7, r4, ror #8\n\t"

			/* multiply by 257 and divide by 65536 */
			"mla r6, r6, r8, %[component_half]\n\t"
			"mla r7, r7, r8, %[component_half]\n\t"

			"uxtab16 r6, r6, r6, ror #8\n\t"
			"uxtab16 r7, r7, r7, ror #8\n\t"

			/* recombine the 0xff00ff00 bytes of r6 and r7 */
			"and r7, r7, %[upper_component_mask]\n\t"
			"uxtab16 r6, r7, r6, ror #8\n\t"

			"uqadd8 r5, r6, r5\n\t"

#ifdef inner_branch
			"3:\n\t"

#endif
			"str r5, [%[dest]], #4\n\t"
			/* increment counter and jmp to top */
			"subs	%[w], %[w], #1\n\t"
			"bne	1b\n\t"
			"2:\n\t"
			: [w] "+r" (w), [dest] "+r" (dst), [src] "+r" (src)
			: [component_half] "r" (component_half), [upper_component_mask] "r" (upper_component_mask),
			  [alpha_mask] "r" (alpha_mask)
			: "r4", "r5", "r6", "r7", "r8", "cc", "memory"
			);
    }
}

void
fbCompositeSrc_8888x8x8888arm (pixman_op_t op,
			       pixman_image_t * pSrc,
			       pixman_image_t * pMask,
			       pixman_image_t * pDst,
			       int16_t	xSrc,
			       int16_t	ySrc,
			       int16_t      xMask,
			       int16_t      yMask,
			       int16_t      xDst,
			       int16_t      yDst,
			       uint16_t     width,
			       uint16_t     height)
{
    uint32_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    uint32_t	mask;
    int	dstStride, srcStride;
    uint16_t	w;
    uint32_t component_half = 0x800080;
    uint32_t alpha_mask = 0xff;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    fbComposeGetSolid (pMask, mask, pDst->bits.format);
    mask = (mask) >> 24;

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

//#define inner_branch
	asm volatile (
			"cmp %[w], #0\n\t"
			"beq 2f\n\t"
			"1:\n\t"
			/* load src */
			"ldr r5, [%[src]], #4\n\t"
#ifdef inner_branch
			/* We can avoid doing the multiplication in two cases: 0x0 or 0xff.
			 * The 0x0 case also allows us to avoid doing an unecessary data
			 * write which is more valuable so we only check for that */
			"cmp r5, #0\n\t"
			"beq 3f\n\t"

#endif
			"ldr r4, [%[dest]] \n\t"

			"uxtb16 r6, r5\n\t"
			"uxtb16 r7, r5, ror #8\n\t"

			/* multiply by alpha (r8) then by 257 and divide by 65536 */
			"mla r6, r6, %[mask_alpha], %[component_half]\n\t"
			"mla r7, r7, %[mask_alpha], %[component_half]\n\t"

			"uxtab16 r6, r6, r6, ror #8\n\t"
			"uxtab16 r7, r7, r7, ror #8\n\t"

			"uxtb16 r6, r6, ror #8\n\t"
			"uxtb16 r7, r7, ror #8\n\t"

			/* recombine */
			"orr r5, r6, r7, lsl #8\n\t"

			"uxtb16 r6, r4\n\t"
			"uxtb16 r7, r4, ror #8\n\t"

			/* 255 - alpha */
			"sub r8, %[alpha_mask], r5, lsr #24\n\t"

			/* multiply by alpha (r8) then by 257 and divide by 65536 */
			"mla r6, r6, r8, %[component_half]\n\t"
			"mla r7, r7, r8, %[component_half]\n\t"

			"uxtab16 r6, r6, r6, ror #8\n\t"
			"uxtab16 r7, r7, r7, ror #8\n\t"

			"uxtb16 r6, r6, ror #8\n\t"
			"uxtb16 r7, r7, ror #8\n\t"

			/* recombine */
			"orr r6, r6, r7, lsl #8\n\t"

			"uqadd8 r5, r6, r5\n\t"

#ifdef inner_branch
			"3:\n\t"

#endif
			"str r5, [%[dest]], #4\n\t"
			/* increment counter and jmp to top */
			"subs	%[w], %[w], #1\n\t"
			"bne	1b\n\t"
			"2:\n\t"
			: [w] "+r" (w), [dest] "+r" (dst), [src] "+r" (src)
			: [component_half] "r" (component_half), [mask_alpha] "r" (mask),
			  [alpha_mask] "r" (alpha_mask)
			: "r4", "r5", "r6", "r7", "r8", "r9", "cc", "memory"
			);
    }
}

void
fbCompositeSolidMask_nx8x8888arm (pixman_op_t      op,
			       pixman_image_t * pSrc,
			       pixman_image_t * pMask,
			       pixman_image_t * pDst,
			       int16_t      xSrc,
			       int16_t      ySrc,
			       int16_t      xMask,
			       int16_t      yMask,
			       int16_t      xDst,
			       int16_t      yDst,
			       uint16_t     width,
			       uint16_t     height)
{
    uint32_t	 src, srca;
    uint32_t	*dstLine, *dst;
    uint8_t	*maskLine, *mask;
    int		 dstStride, maskStride;
    uint16_t	 w;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    uint32_t component_mask = 0xff00ff;
    uint32_t component_half = 0x800080;

    uint32_t src_hi = (src >> 8) & component_mask;
    uint32_t src_lo = src & component_mask;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

//#define inner_branch
	asm volatile (
			"cmp %[w], #0\n\t"
			"beq 2f\n\t"
			"1:\n\t"
			/* load mask */
			"ldrb r5, [%[mask]], #1\n\t"
#ifdef inner_branch
			/* We can avoid doing the multiplication in two cases: 0x0 or 0xff.
			 * The 0x0 case also allows us to avoid doing an unecessary data
			 * write which is more valuable so we only check for that */
			"cmp r5, #0\n\t"
			"beq 3f\n\t"

#endif
			"ldr r4, [%[dest]] \n\t"

			/* multiply by alpha (r8) then by 257 and divide by 65536 */
			"mla r6, %[src_lo], r5, %[component_half]\n\t"
			"mla r7, %[src_hi], r5, %[component_half]\n\t"

			"uxtab16 r6, r6, r6, ror #8\n\t"
			"uxtab16 r7, r7, r7, ror #8\n\t"

			"uxtb16 r6, r6, ror #8\n\t"
			"uxtb16 r7, r7, ror #8\n\t"

			/* recombine */
			"orr r5, r6, r7, lsl #8\n\t"

			"uxtb16 r6, r4\n\t"
			"uxtb16 r7, r4, ror #8\n\t"

			/* we could simplify this to use 'sub' if we were
			 * willing to give up a register for alpha_mask */
			"mvn r8, r5\n\t"
			"mov r8, r8, lsr #24\n\t"

			/* multiply by alpha (r8) then by 257 and divide by 65536 */
			"mla r6, r6, r8, %[component_half]\n\t"
			"mla r7, r7, r8, %[component_half]\n\t"

			"uxtab16 r6, r6, r6, ror #8\n\t"
			"uxtab16 r7, r7, r7, ror #8\n\t"

			"uxtb16 r6, r6, ror #8\n\t"
			"uxtb16 r7, r7, ror #8\n\t"

			/* recombine */
			"orr r6, r6, r7, lsl #8\n\t"

			"uqadd8 r5, r6, r5\n\t"

#ifdef inner_branch
			"3:\n\t"

#endif
			"str r5, [%[dest]], #4\n\t"
			/* increment counter and jmp to top */
			"subs	%[w], %[w], #1\n\t"
			"bne	1b\n\t"
			"2:\n\t"
			: [w] "+r" (w), [dest] "+r" (dst), [src] "+r" (src), [mask] "+r" (mask)
			: [component_half] "r" (component_half),
			  [src_hi] "r" (src_hi), [src_lo] "r" (src_lo)
			: "r4", "r5", "r6", "r7", "r8", "cc", "memory"
			);
    }
}
