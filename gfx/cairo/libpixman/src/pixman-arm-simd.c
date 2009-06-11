/*
 * Copyright © 2008 Mozilla Corporation
 * Copyright © 2008 Nokia Corporation
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
 * Author:  Siarhei Siamashka <siarhei.siamashka@nokia.com>
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

/**
 * Conversion x8r8g8b8 -> r5g6b5
 *
 * TODO: optimize more, eliminate stalls, try to use burst writes (4 words aligned 
 * at 16 byte boundary)
 */
static inline void fbComposite_x8r8g8b8_src_r5g6b5_internal_mixed_armv6_c(
    uint16_t *dst, uint32_t *src, int w, int dst_stride,
    int src_stride, int h)
{
    uint32_t a, x, y, c1F001F = 0x1F001F;
    int backup_w = w;
    while (h--)
    {
        w = backup_w;
        if (w > 0 && (uintptr_t)dst & 2)
        {
            x = *src++;

            a = (x >> 3) & c1F001F;
            x &= 0xFC00;
            a |= a >> 5;
            a |= x >> 5;

            *dst++ = a;
            w--;
        }

        asm volatile(
            "subs  %[w], %[w], #2\n"
            "blt   2f\n"
        "1:\n"
            "ldr   %[x], [%[src]], #4\n"
            "ldr   %[y], [%[src]], #4\n"
            "subs  %[w], %[w], #2\n"
            
            "and   %[a], %[c1F001F], %[x], lsr #3\n"
            "and   %[x], %[x], #0xFC00\n\n"
            "orr   %[a], %[a], %[a], lsr #5\n"
            "orr   %[x], %[a], %[x], lsr #5\n"

            "and   %[a], %[c1F001F], %[y], lsr #3\n"
            "and   %[y], %[y], #0xFC00\n\n"
            "orr   %[a], %[a], %[a], lsr #5\n"
            "orr   %[y], %[a], %[y], lsr #5\n"

            "pkhbt %[x], %[x], %[y], lsl #16\n"
            "str   %[x], [%[dst]], #4\n"
            "bge   1b\n"
        "2:\n"
        : [c1F001F] "+&r" (c1F001F), [src] "+&r" (src), [dst] "+&r" (dst), [a] "=&r" (a), 
          [x] "=&r" (x), [y] "=&r" (y), [w] "+&r" (w)
        );

        if (w & 1)
        {
            x = *src++;

            a = (x >> 3) & c1F001F;
            x = x & 0xFC00;
            a |= a >> 5;
            a |= x >> 5;

            *dst++ = a;
        }

        src += src_stride - backup_w;
        dst += dst_stride - backup_w;
    }
}

/**
 * Conversion x8r8g8b8 -> r5g6b5
 *
 * Note: 'w' must be >= 7
 */
static void __attribute__((naked)) fbComposite_x8r8g8b8_src_r5g6b5_internal_armv6(
    uint16_t *dst, uint32_t *src, int w, int dst_stride,
    int src_stride, int h)
{
    asm volatile(
        /* define supplementary macros */
        ".macro cvt8888to565 PIX\n"
            "and   A, C1F001F, \\PIX, lsr #3\n"
            "and   \\PIX, \\PIX, #0xFC00\n\n"
            "orr   A, A, A, lsr #5\n"
            "orr   \\PIX, A, \\PIX, lsr #5\n"
        ".endm\n"

        ".macro combine_pixels_pair PIX1, PIX2\n"
            "pkhbt \\PIX1, \\PIX1, \\PIX2, lsl #16\n" /* Note: assume little endian byte order */
        ".endm\n"

        /* function entry, save all registers (10 words) to stack */
        "stmdb   sp!, {r4-r11, ip, lr}\n"
        
        /* define some aliases */
        "DST     .req  r0\n"
        "SRC     .req  r1\n"
        "W       .req  r2\n"
        "H       .req  r3\n"

        "TMP1    .req  r4\n"
        "TMP2    .req  r5\n"
        "TMP3    .req  r6\n"
        "TMP4    .req  r7\n"
        "TMP5    .req  r8\n"
        "TMP6    .req  r9\n"
        "TMP7    .req  r10\n"
        "TMP8    .req  r11\n"

        "C1F001F .req  ip\n"
        "A       .req  lr\n"
        
        "ldr     TMP1, [sp, #(10*4+0)]\n" /* load src_stride */
        "ldr     C1F001F, =0x1F001F\n"
        "sub     r3, r3, W\n"
        "str     r3, [sp, #(10*4+0)]\n" /* store (dst_stride-w) */
        "ldr     r3, [sp, #(10*4+4)]\n" /* load h */
        "sub     TMP1, TMP1, W\n"
        "str     TMP1, [sp, #(10*4+4)]\n" /* store (src_stride-w) */
        
        "str     W, [sp, #(8*4)]\n" /* saved ip = W */

    "0:\n"
        "subs    H, H, #1\n"
        "blt     6f\n"
    "1:\n"
        /* align DST at 4 byte boundary */
        "tst     DST, #2\n"
        "beq     2f\n"
        "ldr     TMP1, [SRC], #4\n"
        "sub     W, W, #1\n"
        "cvt8888to565 TMP1\n"
        "strh    TMP1, [DST], #2\n"
    "2:"
        /* align DST at 8 byte boundary */
        "tst     DST, #4\n"
        "beq     2f\n"
        "ldmia   SRC!, {TMP1, TMP2}\n"
        "sub     W, W, #2\n"
        "cvt8888to565 TMP1\n"
        "cvt8888to565 TMP2\n"
        "combine_pixels_pair TMP1, TMP2\n"
        "str     TMP1, [DST], #4\n"
    "2:"
        /* align DST at 16 byte boundary */
        "tst     DST, #8\n"
        "beq     2f\n"
        "ldmia   SRC!, {TMP1, TMP2, TMP3, TMP4}\n"
        "sub     W, W, #4\n"
        "cvt8888to565 TMP1\n"
        "cvt8888to565 TMP2\n"
        "cvt8888to565 TMP3\n"
        "cvt8888to565 TMP4\n"
        "combine_pixels_pair TMP1, TMP2\n"
        "combine_pixels_pair TMP3, TMP4\n"
        "stmia DST!, {TMP1, TMP3}\n"
    "2:"
        /* inner loop, process 8 pixels per iteration */
        "subs    W, W, #8\n"
        "blt     4f\n"
    "3:\n"
        "ldmia   SRC!, {TMP1, TMP2, TMP3, TMP4, TMP5, TMP6, TMP7, TMP8}\n"
        "subs    W, W, #8\n"
        "cvt8888to565 TMP1\n"
        "cvt8888to565 TMP2\n"
        "cvt8888to565 TMP3\n"
        "cvt8888to565 TMP4\n"
        "cvt8888to565 TMP5\n"
        "cvt8888to565 TMP6\n"
        "cvt8888to565 TMP7\n"
        "cvt8888to565 TMP8\n"
        "combine_pixels_pair TMP1, TMP2\n"
        "combine_pixels_pair TMP3, TMP4\n"
        "combine_pixels_pair TMP5, TMP6\n"
        "combine_pixels_pair TMP7, TMP8\n"
        "stmia   DST!, {TMP1, TMP3, TMP5, TMP7}\n"
        "bge     3b\n"
    "4:\n"

        /* process the remaining pixels */
        "tst     W, #4\n"
        "beq     4f\n"
        "ldmia   SRC!, {TMP1, TMP2, TMP3, TMP4}\n"
        "cvt8888to565 TMP1\n"
        "cvt8888to565 TMP2\n"
        "cvt8888to565 TMP3\n"
        "cvt8888to565 TMP4\n"
        "combine_pixels_pair TMP1, TMP2\n"
        "combine_pixels_pair TMP3, TMP4\n"
        "stmia   DST!, {TMP1, TMP3}\n"
    "4:\n"
        "tst     W, #2\n"
        "beq     4f\n"
        "ldmia   SRC!, {TMP1, TMP2}\n"
        "cvt8888to565 TMP1\n"
        "cvt8888to565 TMP2\n"
        "combine_pixels_pair TMP1, TMP2\n"
        "str     TMP1, [DST], #4\n"
    "4:\n"
        "tst     W, #1\n"
        "beq     4f\n"
        "ldr     TMP1, [SRC], #4\n"
        "cvt8888to565 TMP1\n"
        "strh    TMP1, [DST], #2\n"
    "4:\n"
        "ldr     TMP1, [sp, #(10*4+0)]\n" /* (dst_stride-w) */
        "ldr     TMP2, [sp, #(10*4+4)]\n" /* (src_stride-w) */
        "ldr     W, [sp, #(8*4)]\n"
        "subs    H, H, #1\n"
        "add     DST, DST, TMP1, lsl #1\n"
        "add     SRC, SRC, TMP2, lsl #2\n"
        "bge     1b\n"
    "6:\n"
        "ldmia   sp!, {r4-r11, ip, pc}\n" /* restore all registers and return */
        ".ltorg\n"

        ".unreq   DST\n"
        ".unreq   SRC\n"
        ".unreq   W\n"
        ".unreq   H\n"

        ".unreq   TMP1\n"
        ".unreq   TMP2\n"
        ".unreq   TMP3\n"
        ".unreq   TMP4\n"
        ".unreq   TMP5\n"
        ".unreq   TMP6\n"
        ".unreq   TMP7\n"
        ".unreq   TMP8\n"

        ".unreq   C1F001F\n"
        ".unreq   A\n"

        ".purgem  cvt8888to565\n"
        ".purgem  combine_pixels_pair\n"
    );
}


void
fbCompositeSrc_x888x0565arm (pixman_op_t op,
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
    uint16_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    int		dstStride, srcStride;
    uint16_t	w, h;

    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);

    dst = dstLine;
    src = srcLine;
    h = height;
    w = width;

    if (w < 7)
        fbComposite_x8r8g8b8_src_r5g6b5_internal_mixed_armv6_c(dst, src, w, dstStride, srcStride, h);
    else
        fbComposite_x8r8g8b8_src_r5g6b5_internal_armv6(dst, src, w, dstStride, srcStride, h);

}
