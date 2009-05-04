/*
 * Copyright © 2009 Mozilla Corporation
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
 * Author:  Ian Rickards (ian.rickards@arm.com) 
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "pixman-arm-neon.h"

#include <arm_neon.h>


#if !defined(__ARMCC_VERSION) && !defined(FORCE_NO_NEON_INLINE_ASM)
// [both armcc & gcc set __GNUC__]
// Use GNU style inline asm on gcc, for best performance
// Use intrinsics on armcc
// This switch determines if any GNU style inline asm is allowed
#define USE_NEON_INLINE_ASM
#endif


static force_inline uint8x8x4_t unpack0565(uint16x8_t rgb)
{
    uint16x8_t gb, b;
    uint8x8x4_t res;

    res.val[3] = vdup_n_u8(0);
    gb = vshrq_n_u16(rgb, 5);
    b = vshrq_n_u16(rgb, 5+6);
    res.val[0] = vmovn_u16(rgb);  // get low 5 bits
    res.val[1] = vmovn_u16(gb);   // get mid 6 bits
    res.val[2] = vmovn_u16(b);    // get top 5 bits

    res.val[0] = vshl_n_u8(res.val[0], 3); // shift to top
    res.val[1] = vshl_n_u8(res.val[1], 2); // shift to top
    res.val[2] = vshl_n_u8(res.val[2], 3); // shift to top

    res.val[0] = vsri_n_u8(res.val[0], res.val[0], 5); 
    res.val[1] = vsri_n_u8(res.val[1], res.val[1], 6);
    res.val[2] = vsri_n_u8(res.val[2], res.val[2], 5);

    return res;
}

static force_inline uint16x8_t pack0565(uint8x8x4_t s)
{
    uint16x8_t rgb, val_g, val_r;

    rgb = vshll_n_u8(s.val[2],8);
    val_g = vshll_n_u8(s.val[1],8);
    val_r = vshll_n_u8(s.val[0],8);
    rgb = vsriq_n_u16(rgb, val_g, 5);
    rgb = vsriq_n_u16(rgb, val_r, 5+6);

    return rgb;
}

static force_inline uint8x8_t neon2mul(uint8x8_t x, uint8x8_t alpha)
{
    uint16x8_t tmp,tmp2;
    uint8x8_t res;

    tmp = vmull_u8(x,alpha);
    tmp2 = vrshrq_n_u16(tmp,8);
    res = vraddhn_u16(tmp,tmp2);

    return res;
}

static force_inline uint8x8x4_t neon8mul(uint8x8x4_t x, uint8x8_t alpha)
{
    uint16x8x4_t tmp;
    uint8x8x4_t res;
    uint16x8_t qtmp1,qtmp2;

    tmp.val[0] = vmull_u8(x.val[0],alpha);
    tmp.val[1] = vmull_u8(x.val[1],alpha);
    tmp.val[2] = vmull_u8(x.val[2],alpha);
    tmp.val[3] = vmull_u8(x.val[3],alpha);

    qtmp1 = vrshrq_n_u16(tmp.val[0],8);
    qtmp2 = vrshrq_n_u16(tmp.val[1],8);
    res.val[0] = vraddhn_u16(tmp.val[0],qtmp1);
    qtmp1 = vrshrq_n_u16(tmp.val[2],8);
    res.val[1] = vraddhn_u16(tmp.val[1],qtmp2);
    qtmp2 = vrshrq_n_u16(tmp.val[3],8);
    res.val[2] = vraddhn_u16(tmp.val[2],qtmp1);
    res.val[3] = vraddhn_u16(tmp.val[3],qtmp2);

    return res;
}

static force_inline uint8x8x4_t neon8qadd(uint8x8x4_t x, uint8x8x4_t y)
{
    uint8x8x4_t res;

    res.val[0] = vqadd_u8(x.val[0],y.val[0]);
    res.val[1] = vqadd_u8(x.val[1],y.val[1]);
    res.val[2] = vqadd_u8(x.val[2],y.val[2]);
    res.val[3] = vqadd_u8(x.val[3],y.val[3]);

    return res;
}


void
fbCompositeSrcAdd_8000x8000neon (pixman_op_t op,
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
    uint8_t     *dstLine, *dst;
    uint8_t     *srcLine, *src;
    int dstStride, srcStride;
    uint16_t    w;

    fbComposeGetStart (pSrc, xSrc, ySrc, uint8_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);

    if (width>=8)
    {
        // Use overlapping 8-pixel method
        while (height--)
        {
            dst = dstLine;
            dstLine += dstStride;
            src = srcLine;
            srcLine += srcStride;
            w = width;

            uint8_t *keep_dst;

#ifndef USE_NEON_INLINE_ASM
            uint8x8_t sval,dval,temp;

            sval = vld1_u8((void*)src);
            dval = vld1_u8((void*)dst);
            keep_dst = dst;

            temp = vqadd_u8(dval,sval);

            src += (w & 7);
            dst += (w & 7);
            w -= (w & 7);

            while (w)
            {
                sval = vld1_u8((void*)src);
                dval = vld1_u8((void*)dst);

                vst1_u8((void*)keep_dst,temp);
                keep_dst = dst;

                temp = vqadd_u8(dval,sval);

                src+=8;
                dst+=8;
                w-=8;
            }
            vst1_u8((void*)keep_dst,temp);
#else
            asm volatile (
// avoid using d8-d15 (q4-q7) aapcs callee-save registers
                        "vld1.8  {d0}, [%[src]]\n\t"
                        "vld1.8  {d4}, [%[dst]]\n\t"
                        "mov     %[keep_dst], %[dst]\n\t"

                        "and ip, %[w], #7\n\t"
                        "add %[src], %[src], ip\n\t"
                        "add %[dst], %[dst], ip\n\t"
                        "subs %[w], %[w], ip\n\t"
                        "b 9f\n\t"
// LOOP
                        "2:\n\t"
                        "vld1.8  {d0}, [%[src]]!\n\t"
                        "vld1.8  {d4}, [%[dst]]!\n\t"
                        "vst1.8  {d20}, [%[keep_dst]]\n\t"
                        "sub     %[keep_dst], %[dst], #8\n\t"
                        "subs %[w], %[w], #8\n\t"
                        "9:\n\t"
                        "vqadd.u8 d20, d0, d4\n\t"

                        "bne 2b\n\t"

                        "1:\n\t"
                        "vst1.8  {d20}, [%[keep_dst]]\n\t"

                        : [w] "+r" (w), [src] "+r" (src), [dst] "+r" (dst), [keep_dst] "+r" (keep_dst)
                        :
                        : "ip", "cc", "memory", "d0","d4",
                          "d20"
                        );
#endif
        }
    }
    else
    {
        while (height--)
        {
            dst = dstLine;
            dstLine += dstStride;
            src = srcLine;
            srcLine += srcStride;
            w = width;
            uint8x8_t sval, dval;
            uint8_t *dst4, *dst2;

            if (w&4)
            {
                sval = vreinterpret_u8_u32(vld1_lane_u32((void*)src,vreinterpret_u32_u8(sval),1));
                dval = vreinterpret_u8_u32(vld1_lane_u32((void*)dst,vreinterpret_u32_u8(dval),1));
                dst4=dst;
                src+=4;
                dst+=4;
            }
            if (w&2)
            {
                sval = vreinterpret_u8_u16(vld1_lane_u16((void*)src,vreinterpret_u16_u8(sval),1));
                dval = vreinterpret_u8_u16(vld1_lane_u16((void*)dst,vreinterpret_u16_u8(dval),1));
                dst2=dst;
                src+=2;
                dst+=2;
            }
            if (w&1)
            {
                sval = vld1_lane_u8((void*)src,sval,1);
                dval = vld1_lane_u8((void*)dst,dval,1);
            }

            dval = vqadd_u8(dval,sval);

            if (w&1)
                vst1_lane_u8((void*)dst,dval,1);
            if (w&2)
                vst1_lane_u16((void*)dst2,vreinterpret_u16_u8(dval),1);
            if (w&4)
                vst1_lane_u32((void*)dst4,vreinterpret_u32_u8(dval),1);
        }
    }
}


void
fbCompositeSrc_8888x8888neon (pixman_op_t op,
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
    uint32_t	w;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    if (width>=8)
    {
        // Use overlapping 8-pixel method  
        while (height--)
        {
	    dst = dstLine;
	    dstLine += dstStride;
	    src = srcLine;
	    srcLine += srcStride;
	    w = width;

            uint32_t *keep_dst;

#ifndef USE_NEON_INLINE_ASM
            uint8x8x4_t sval,dval,temp;

            sval = vld4_u8((void*)src);
            dval = vld4_u8((void*)dst);
            keep_dst = dst;

            temp = neon8mul(dval,vmvn_u8(sval.val[3]));
            temp = neon8qadd(sval,temp);

            src += (w & 7);
            dst += (w & 7);
            w -= (w & 7);

            while (w)
            {
                sval = vld4_u8((void*)src);
                dval = vld4_u8((void*)dst);

                vst4_u8((void*)keep_dst,temp);
                keep_dst = dst;

                temp = neon8mul(dval,vmvn_u8(sval.val[3]));
                temp = neon8qadd(sval,temp);

                src+=8;
                dst+=8;
                w-=8;
            }
            vst4_u8((void*)keep_dst,temp);
#else
            asm volatile (
// avoid using d8-d15 (q4-q7) aapcs callee-save registers
                        "vld4.8  {d0-d3}, [%[src]]\n\t"
                        "vld4.8  {d4-d7}, [%[dst]]\n\t"
                        "mov     %[keep_dst], %[dst]\n\t"

                        "and ip, %[w], #7\n\t"
                        "add %[src], %[src], ip, LSL#2\n\t"
                        "add %[dst], %[dst], ip, LSL#2\n\t"
                        "subs %[w], %[w], ip\n\t"
                        "b 9f\n\t"
// LOOP
                        "2:\n\t"
                        "vld4.8  {d0-d3}, [%[src]]!\n\t"
                        "vld4.8  {d4-d7}, [%[dst]]!\n\t"
                        "vst4.8  {d20-d23}, [%[keep_dst]]\n\t"
                        "sub     %[keep_dst], %[dst], #8*4\n\t"
                        "subs %[w], %[w], #8\n\t"
                        "9:\n\t"
                        "vmvn.8  d31, d3\n\t"
                        "vmull.u8 q10, d31, d4\n\t"
                        "vmull.u8 q11, d31, d5\n\t"
                        "vmull.u8 q12, d31, d6\n\t"
                        "vmull.u8 q13, d31, d7\n\t"
                        "vrshr.u16 q8, q10, #8\n\t"
                        "vrshr.u16 q9, q11, #8\n\t"
                        "vraddhn.u16 d20, q10, q8\n\t"
                        "vraddhn.u16 d21, q11, q9\n\t"
                        "vrshr.u16 q8, q12, #8\n\t"
                        "vrshr.u16 q9, q13, #8\n\t"
                        "vraddhn.u16 d22, q12, q8\n\t"
                        "vraddhn.u16 d23, q13, q9\n\t"
// result in d20-d23
                        "vqadd.u8 d20, d0, d20\n\t"
                        "vqadd.u8 d21, d1, d21\n\t"
                        "vqadd.u8 d22, d2, d22\n\t"
                        "vqadd.u8 d23, d3, d23\n\t"

                        "bne 2b\n\t"

                        "1:\n\t"
                        "vst4.8  {d20-d23}, [%[keep_dst]]\n\t"

                        : [w] "+r" (w), [src] "+r" (src), [dst] "+r" (dst), [keep_dst] "+r" (keep_dst)
                        : 
                        : "ip", "cc", "memory", "d0","d1","d2","d3","d4","d5","d6","d7",
                          "d16","d17","d18","d19","d20","d21","d22","d23"
                        );
#endif
        }
    }
    else
    {
        uint8x8_t    alpha_selector=vreinterpret_u8_u64(vcreate_u64(0x0707070703030303ULL));

        // Handle width<8
        while (height--)
        {
            dst = dstLine;
            dstLine += dstStride;
            src = srcLine;
            srcLine += srcStride;
            w = width;

            while (w>=2)
            {
                uint8x8_t sval,dval;

                /* two 32-bit pixels packed into D-reg; ad-hoc vectorization */
                sval = vreinterpret_u8_u32(vld1_u32((void*)src));
                dval = vreinterpret_u8_u32(vld1_u32((void*)dst));
                dval = neon2mul(dval,vtbl1_u8(vmvn_u8(sval),alpha_selector));
                vst1_u8((void*)dst,vqadd_u8(sval,dval));

                src+=2;
                dst+=2;
                w-=2;
            }

            if (w)
            {
                uint8x8_t sval,dval;

                /* single 32-bit pixel in lane 0 */
                sval = vreinterpret_u8_u32(vld1_dup_u32((void*)src));  // only interested in lane 0
                dval = vreinterpret_u8_u32(vld1_dup_u32((void*)dst));  // only interested in lane 0
                dval = neon2mul(dval,vtbl1_u8(vmvn_u8(sval),alpha_selector));
                vst1_lane_u32((void*)dst,vreinterpret_u32_u8(vqadd_u8(sval,dval)),0);
            }
        }
    }
}



void
fbCompositeSrc_x888x0565neon (pixman_op_t op,
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
    uint16_t    *dstLine, *dst;
    uint32_t    *srcLine, *src;
    int dstStride, srcStride;
    uint32_t    w;

    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);

    if (width>=8)
    {
        while (height--)
        {
            dst = dstLine;
            dstLine += dstStride;
            src = srcLine;
            srcLine += srcStride;
            w = width;

	    do {
	        while (w>=8)
	        {
#ifndef USE_NEON_INLINE_ASM
	            vst1q_u16(dst, pack0565(vld4_u8((void*)src)));
#else
                    asm volatile (
                        "vld4.8       {d4-d7}, [%[src]]\n\t"
                        "vshll.u8     q0, d6, #8\n\t"
                        "vshll.u8     q1, d5, #8\n\t"
                        "vsriq.u16    q0, q1, #5\t\n"
                        "vshll.u8     q1, d4, #8\n\t"
                        "vsriq.u16    q0, q1, #11\t\n"
                        "vst1.16      {q0}, [%[dst]]\n\t"
                        :
                        : [dst] "r" (dst), [src] "r" (src)
                        : "memory", "d0","d1","d2","d3","d4","d5","d6","d7"
                        );
#endif
	            src+=8;
	            dst+=8;
	            w-=8;
          	}
                if (w != 0)
                {
                    src -= (8-w);
                    dst -= (8-w);
                    w = 8;  // do another vector
                }
            } while (w!=0);
        }
    }
    else
    {
        // Handle width<8
        while (height--)
        {
            dst = dstLine;
            dstLine += dstStride;
            src = srcLine;
            srcLine += srcStride;
            w = width;

	    while (w>=2)
	    {
	        uint32x2_t sval, rgb, g, b;
	        sval = vld1_u32(src);
	        rgb = vshr_n_u32(sval,8-5); // r (5 bits) 
	        g = vshr_n_u32(sval,8+8-6);  // g to bottom byte
	        rgb = vsli_n_u32(rgb, g, 5);
	        b = vshr_n_u32(sval,8+8+8-5);  // b to bottom byte
                rgb = vsli_n_u32(rgb, b, 11);
	        vst1_lane_u16(dst++,vreinterpret_u16_u32(rgb),0);
	        vst1_lane_u16(dst++,vreinterpret_u16_u32(rgb),2);
	        src+=2;
	        w-=2;
	    }
            if (w)
            {
                uint32x2_t sval, rgb, g, b;
                sval = vld1_dup_u32(src);
                rgb = vshr_n_u32(sval,8-5); // r (5 bits)
                g = vshr_n_u32(sval,8+8-6);  // g to bottom byte
                rgb = vsli_n_u32(rgb, g, 5);
                b = vshr_n_u32(sval,8+8+8-5);  // b to bottom byte
                rgb = vsli_n_u32(rgb, b, 11);
                vst1_lane_u16(dst++,vreinterpret_u16_u32(rgb),0);
            }
	}
    }
}


void
fbCompositeSrc_8888x8x8888neon (pixman_op_t op,
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
    uint32_t	w;
    uint8x8_t mask_alpha;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    fbComposeGetSolid (pMask, mask, pDst->bits.format);
    mask_alpha = vdup_n_u8((mask) >> 24);

    if (width>=8)
    {
        // Use overlapping 8-pixel method
        while (height--)
        {
            dst = dstLine;
            dstLine += dstStride;
            src = srcLine;
            srcLine += srcStride;
            w = width;

            uint32_t *keep_dst;

#ifndef USE_NEON_INLINE_ASM
            uint8x8x4_t sval,dval,temp;

            sval = vld4_u8((void*)src);
            dval = vld4_u8((void*)dst);
            keep_dst = dst;

            sval = neon8mul(sval,mask_alpha);
            temp = neon8mul(dval,vmvn_u8(sval.val[3]));
            temp = neon8qadd(sval,temp);

            src += (w & 7);
            dst += (w & 7);
            w -= (w & 7);

            while (w)
            {
                sval = vld4_u8((void*)src);
                dval = vld4_u8((void*)dst);

                vst4_u8((void*)keep_dst,temp);
                keep_dst = dst;

                sval = neon8mul(sval,mask_alpha);
                temp = neon8mul(dval,vmvn_u8(sval.val[3]));
                temp = neon8qadd(sval,temp);

                src+=8;
                dst+=8;
                w-=8;
            }
            vst4_u8((void*)keep_dst,temp);
#else
            asm volatile (
// avoid using d8-d15 (q4-q7) aapcs callee-save registers
                        "vdup.32      d30, %[mask]\n\t"
                        "vdup.8       d30, d30[3]\n\t"

                        "vld4.8       {d0-d3}, [%[src]]\n\t"
                        "vld4.8       {d4-d7}, [%[dst]]\n\t"
                        "mov  %[keep_dst], %[dst]\n\t"

                        "and  ip, %[w], #7\n\t"
                        "add  %[src], %[src], ip, LSL#2\n\t"
                        "add  %[dst], %[dst], ip, LSL#2\n\t"
                        "subs  %[w], %[w], ip\n\t"
                        "b 9f\n\t"
// LOOP
                        "2:\n\t"
                        "vld4.8       {d0-d3}, [%[src]]!\n\t"
                        "vld4.8       {d4-d7}, [%[dst]]!\n\t"
                        "vst4.8       {d20-d23}, [%[keep_dst]]\n\t"
                        "sub  %[keep_dst], %[dst], #8*4\n\t"
                        "subs  %[w], %[w], #8\n\t"

                        "9:\n\t"
                        "vmull.u8     q10, d30, d0\n\t"
                        "vmull.u8     q11, d30, d1\n\t"
                        "vmull.u8     q12, d30, d2\n\t"
                        "vmull.u8     q13, d30, d3\n\t"
                        "vrshr.u16    q8, q10, #8\n\t"
                        "vrshr.u16    q9, q11, #8\n\t"
                        "vraddhn.u16  d0, q10, q8\n\t"
                        "vraddhn.u16  d1, q11, q9\n\t"
                        "vrshr.u16    q9, q13, #8\n\t"
                        "vrshr.u16    q8, q12, #8\n\t"
                        "vraddhn.u16  d3, q13, q9\n\t"
                        "vraddhn.u16  d2, q12, q8\n\t"

                        "vmvn.8       d31, d3\n\t"
                        "vmull.u8     q10, d31, d4\n\t"
                        "vmull.u8     q11, d31, d5\n\t"
                        "vmull.u8     q12, d31, d6\n\t"
                        "vmull.u8     q13, d31, d7\n\t"
                        "vrshr.u16    q8, q10, #8\n\t"
                        "vrshr.u16    q9, q11, #8\n\t"
                        "vraddhn.u16  d20, q10, q8\n\t"
                        "vrshr.u16    q8, q12, #8\n\t"
                        "vraddhn.u16  d21, q11, q9\n\t"
                        "vrshr.u16    q9, q13, #8\n\t"
                        "vraddhn.u16  d22, q12, q8\n\t"
                        "vraddhn.u16  d23, q13, q9\n\t"
// result in d20-d23
                        "vqadd.u8     d20, d0, d20\n\t"
                        "vqadd.u8     d21, d1, d21\n\t"
                        "vqadd.u8     d22, d2, d22\n\t"
                        "vqadd.u8     d23, d3, d23\n\t"

                        "bne  2b\n\t"

                        "1:\n\t"
                        "vst4.8       {d20-d23}, [%[keep_dst]]\n\t"

                        : [w] "+r" (w), [src] "+r" (src), [dst] "+r" (dst), [keep_dst] "+r" (keep_dst)
                        : [mask] "r" (mask)
                        : "ip", "cc", "memory", "d0","d1","d2","d3","d4","d5","d6","d7",
                          "d16","d17","d18","d19","d20","d21","d22","d23","d24","d25","d26","d27",
                          "d30","d31"
                        );
#endif
        }
    }
    else
    {
        uint8x8_t    alpha_selector=vreinterpret_u8_u64(vcreate_u64(0x0707070703030303ULL));

        // Handle width<8
        while (height--)
        {
            dst = dstLine;
            dstLine += dstStride;
            src = srcLine;
            srcLine += srcStride;
            w = width;

            while (w>=2)
            {
                uint8x8_t sval,dval;

                sval = vreinterpret_u8_u32(vld1_u32((void*)src));
                dval = vreinterpret_u8_u32(vld1_u32((void*)dst));

                /* sval * const alpha_mul */
                sval = neon2mul(sval,mask_alpha);

                /* dval * 255-(src alpha) */
                dval = neon2mul(dval,vtbl1_u8(vmvn_u8(sval), alpha_selector));

                vst1_u8((void*)dst,vqadd_u8(sval,dval));

                src+=2;
                dst+=2;
                w-=2;
            }

            if (w)
            {
                uint8x8_t sval,dval;

                sval = vreinterpret_u8_u32(vld1_dup_u32((void*)src));
                dval = vreinterpret_u8_u32(vld1_dup_u32((void*)dst));

                /* sval * const alpha_mul */
                sval = neon2mul(sval,mask_alpha);

                /* dval * 255-(src alpha) */
                dval = neon2mul(dval,vtbl1_u8(vmvn_u8(sval), alpha_selector));

                vst1_lane_u32((void*)dst,vreinterpret_u32_u8(vqadd_u8(sval,dval)),0);
            }
        }
    }
}



void
fbCompositeSolidMask_nx8x0565neon (pixman_op_t op,
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
    uint32_t     src, srca;
    uint16_t    *dstLine, *dst;
    uint8_t     *maskLine, *mask;
    int          dstStride, maskStride;
    uint32_t     w;
    uint8x8_t    sval2;
    uint8x8x4_t  sval8;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (src == 0)
        return;

    sval2=vreinterpret_u8_u32(vdup_n_u32(src));
    sval8.val[0]=vdup_lane_u8(sval2,0);
    sval8.val[1]=vdup_lane_u8(sval2,1);
    sval8.val[2]=vdup_lane_u8(sval2,2);
    sval8.val[3]=vdup_lane_u8(sval2,3);

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    if (width>=8)
    {
        // Use overlapping 8-pixel method, modified to avoid rewritten dest being reused
        while (height--)
        {
            uint16_t *keep_dst;

            dst = dstLine;
            dstLine += dstStride;
            mask = maskLine;
            maskLine += maskStride;
            w = width;

#ifndef USE_NEON_INLINE_ASM
            uint8x8_t alpha;
            uint16x8_t dval, temp; 
            uint8x8x4_t sval8temp;

            alpha = vld1_u8((void*)mask);
            dval = vld1q_u16((void*)dst);
            keep_dst = dst;

            sval8temp = neon8mul(sval8,alpha);
            temp = pack0565(neon8qadd(sval8temp,neon8mul(unpack0565(dval),vmvn_u8(sval8temp.val[3]))));

            mask += (w & 7);
            dst += (w & 7);
            w -= (w & 7);

            while (w)
            {
                dval = vld1q_u16((void*)dst);
	        alpha = vld1_u8((void*)mask);

                vst1q_u16((void*)keep_dst,temp);
                keep_dst = dst;

                sval8temp = neon8mul(sval8,alpha);
                temp = pack0565(neon8qadd(sval8temp,neon8mul(unpack0565(dval),vmvn_u8(sval8temp.val[3]))));

                mask+=8;
                dst+=8;
                w-=8;
            }
            vst1q_u16((void*)keep_dst,temp);
#else
        asm volatile (
                        "vdup.32      d0, %[src]\n\t"
                        "vdup.8       d1, d0[1]\n\t"
                        "vdup.8       d2, d0[2]\n\t"
                        "vdup.8       d3, d0[3]\n\t"
                        "vdup.8       d0, d0[0]\n\t"

                        "vld1.8       {q12}, [%[dst]]\n\t"
                        "vld1.8       {d31}, [%[mask]]\n\t"
                        "mov  %[keep_dst], %[dst]\n\t"

                        "and  ip, %[w], #7\n\t"
                        "add  %[mask], %[mask], ip\n\t"
                        "add  %[dst], %[dst], ip, LSL#1\n\t"
                        "subs  %[w], %[w], ip\n\t"
                        "b  9f\n\t"
// LOOP
                        "2:\n\t"

                        "vld1.16      {q12}, [%[dst]]!\n\t"
                        "vld1.8       {d31}, [%[mask]]!\n\t"
                        "vst1.16      {q10}, [%[keep_dst]]\n\t"
                        "sub  %[keep_dst], %[dst], #8*2\n\t"
                        "subs  %[w], %[w], #8\n\t"
                        "9:\n\t"
// expand 0565 q12 to 8888 {d4-d7}
                        "vmovn.u16    d4, q12\t\n"
                        "vshr.u16     q11, q12, #5\t\n"
                        "vshr.u16     q10, q12, #6+5\t\n"
                        "vmovn.u16    d5, q11\t\n"
                        "vmovn.u16    d6, q10\t\n"
                        "vshl.u8      d4, d4, #3\t\n"
                        "vshl.u8      d5, d5, #2\t\n"
                        "vshl.u8      d6, d6, #3\t\n"
                        "vsri.u8      d4, d4, #5\t\n"
                        "vsri.u8      d5, d5, #6\t\n"
                        "vsri.u8      d6, d6, #5\t\n"

                        "vmull.u8     q10, d31, d0\n\t"
                        "vmull.u8     q11, d31, d1\n\t"
                        "vmull.u8     q12, d31, d2\n\t"
                        "vmull.u8     q13, d31, d3\n\t"
                        "vrshr.u16    q8, q10, #8\n\t"
                        "vrshr.u16    q9, q11, #8\n\t"
                        "vraddhn.u16  d20, q10, q8\n\t"
                        "vraddhn.u16  d21, q11, q9\n\t"
                        "vrshr.u16    q9, q13, #8\n\t"
                        "vrshr.u16    q8, q12, #8\n\t"
                        "vraddhn.u16  d23, q13, q9\n\t"
                        "vraddhn.u16  d22, q12, q8\n\t"

// duplicate in 4/2/1 & 8pix vsns
                        "vmvn.8       d30, d23\n\t"
                        "vmull.u8     q14, d30, d6\n\t"
                        "vmull.u8     q13, d30, d5\n\t"
                        "vmull.u8     q12, d30, d4\n\t"
                        "vrshr.u16    q8, q14, #8\n\t"
                        "vrshr.u16    q9, q13, #8\n\t"
                        "vraddhn.u16  d6, q14, q8\n\t"
                        "vrshr.u16    q8, q12, #8\n\t"
                        "vraddhn.u16  d5, q13, q9\n\t"
                        "vqadd.u8     d6, d6, d22\n\t"  // moved up
                        "vraddhn.u16  d4, q12, q8\n\t"
// intentionally don't calculate alpha
// result in d4-d6

//                      "vqadd.u8     d6, d6, d22\n\t"  ** moved up
                        "vqadd.u8     d5, d5, d21\n\t"
                        "vqadd.u8     d4, d4, d20\n\t"

// pack 8888 {d20-d23} to 0565 q10
                        "vshll.u8     q10, d6, #8\n\t"
                        "vshll.u8     q3, d5, #8\n\t"
                        "vshll.u8     q2, d4, #8\n\t"
                        "vsri.u16     q10, q3, #5\t\n"
                        "vsri.u16     q10, q2, #11\t\n"

                        "bne 2b\n\t"

                        "1:\n\t"
                        "vst1.16      {q10}, [%[keep_dst]]\n\t"

                        : [w] "+r" (w), [dst] "+r" (dst), [mask] "+r" (mask), [keep_dst] "+r" (keep_dst)
                        : [src] "r" (src)
                        : "ip", "cc", "memory", "d0","d1","d2","d3","d4","d5","d6","d7",
                          "d16","d17","d18","d19","d20","d21","d22","d23","d24","d25","d26","d27","d28","d29",
                          "d30","d31"
                        );
#endif
        }
    }
    else
    {
        while (height--)
        {
            void *dst4, *dst2;

            dst = dstLine;
            dstLine += dstStride;
            mask = maskLine;
            maskLine += maskStride;
            w = width;


#ifndef USE_NEON_INLINE_ASM
            uint8x8_t alpha;
            uint16x8_t dval, temp;
            uint8x8x4_t sval8temp;

            if (w&4)
            {
                alpha = vreinterpret_u8_u32(vld1_lane_u32((void*)mask,vreinterpret_u32_u8(alpha),1));
                dval = vreinterpretq_u16_u64(vld1q_lane_u64((void*)dst,vreinterpretq_u64_u16(dval),1));
                dst4=dst;
                mask+=4;
                dst+=4;
            }
            if (w&2)
            {
                alpha = vreinterpret_u8_u16(vld1_lane_u16((void*)mask,vreinterpret_u16_u8(alpha),1));
                dval = vreinterpretq_u16_u32(vld1q_lane_u32((void*)dst,vreinterpretq_u32_u16(dval),1));
                dst2=dst;
                mask+=2;
                dst+=2;
            }
            if (w&1)
            {
                alpha = vld1_lane_u8((void*)mask,alpha,1);
                dval = vld1q_lane_u16((void*)dst,dval,1);
            }

            sval8temp = neon8mul(sval8,alpha);
            temp = pack0565(neon8qadd(sval8temp,neon8mul(unpack0565(dval),vmvn_u8(sval8temp.val[3]))));

            if (w&1)
                vst1q_lane_u16((void*)dst,temp,1);
            if (w&2)
                vst1q_lane_u32((void*)dst2,vreinterpretq_u32_u16(temp),1);
            if (w&4)
                vst1q_lane_u64((void*)dst4,vreinterpretq_u64_u16(temp),1);
#else
            asm volatile (
                        "vdup.32      d0, %[src]\n\t"
                        "vdup.8       d1, d0[1]\n\t"
                        "vdup.8       d2, d0[2]\n\t"
                        "vdup.8       d3, d0[3]\n\t"
                        "vdup.8       d0, d0[0]\n\t"

                        "tst  %[w], #4\t\n"
                        "beq  skip_load4\t\n"

                        "vld1.64      {d25}, [%[dst]]\n\t"
                        "vld1.32      {d31[1]}, [%[mask]]\n\t"
                        "mov  %[dst4], %[dst]\t\n"
                        "add  %[mask], %[mask], #4\t\n"
                        "add  %[dst], %[dst], #4*2\t\n"

                        "skip_load4:\t\n"
                        "tst  %[w], #2\t\n"
                        "beq  skip_load2\t\n"
                        "vld1.32      {d24[1]}, [%[dst]]\n\t"
                        "vld1.16      {d31[1]}, [%[mask]]\n\t"
                        "mov  %[dst2], %[dst]\t\n"
                        "add  %[mask], %[mask], #2\t\n"
                        "add  %[dst], %[dst], #2*2\t\n"

                        "skip_load2:\t\n"
                        "tst  %[w], #1\t\n"
                        "beq  skip_load1\t\n"
                        "vld1.16      {d24[1]}, [%[dst]]\n\t"
                        "vld1.8       {d31[1]}, [%[mask]]\n\t"

                        "skip_load1:\t\n"
// expand 0565 q12 to 8888 {d4-d7}
                        "vmovn.u16    d4, q12\t\n"
                        "vshr.u16     q11, q12, #5\t\n"
                        "vshr.u16     q10, q12, #6+5\t\n"
                        "vmovn.u16    d5, q11\t\n"
                        "vmovn.u16    d6, q10\t\n"
                        "vshl.u8      d4, d4, #3\t\n"
                        "vshl.u8      d5, d5, #2\t\n"
                        "vshl.u8      d6, d6, #3\t\n"
                        "vsri.u8      d4, d4, #5\t\n"
                        "vsri.u8      d5, d5, #6\t\n"
                        "vsri.u8      d6, d6, #5\t\n"

                        "vmull.u8     q10, d31, d0\n\t"
                        "vmull.u8     q11, d31, d1\n\t"
                        "vmull.u8     q12, d31, d2\n\t"
                        "vmull.u8     q13, d31, d3\n\t"
                        "vrshr.u16    q8, q10, #8\n\t"
                        "vrshr.u16    q9, q11, #8\n\t"
                        "vraddhn.u16  d20, q10, q8\n\t"
                        "vraddhn.u16  d21, q11, q9\n\t"
                        "vrshr.u16    q9, q13, #8\n\t"
                        "vrshr.u16    q8, q12, #8\n\t"
                        "vraddhn.u16  d23, q13, q9\n\t"
                        "vraddhn.u16  d22, q12, q8\n\t"

// duplicate in 4/2/1 & 8pix vsns
                        "vmvn.8       d30, d23\n\t"
                        "vmull.u8     q14, d30, d6\n\t"
                        "vmull.u8     q13, d30, d5\n\t"
                        "vmull.u8     q12, d30, d4\n\t"
                        "vrshr.u16    q8, q14, #8\n\t"
                        "vrshr.u16    q9, q13, #8\n\t"
                        "vraddhn.u16  d6, q14, q8\n\t"
                        "vrshr.u16    q8, q12, #8\n\t"
                        "vraddhn.u16  d5, q13, q9\n\t"
                        "vqadd.u8     d6, d6, d22\n\t"  // moved up
                        "vraddhn.u16  d4, q12, q8\n\t"
// intentionally don't calculate alpha
// result in d4-d6

//                      "vqadd.u8     d6, d6, d22\n\t"  ** moved up
                        "vqadd.u8     d5, d5, d21\n\t"
                        "vqadd.u8     d4, d4, d20\n\t"

// pack 8888 {d20-d23} to 0565 q10
                        "vshll.u8     q10, d6, #8\n\t"
                        "vshll.u8     q3, d5, #8\n\t"
                        "vshll.u8     q2, d4, #8\n\t"
                        "vsri.u16     q10, q3, #5\t\n"
                        "vsri.u16     q10, q2, #11\t\n"

                        "tst  %[w], #1\n\t"
                        "beq skip_store1\t\n"
                        "vst1.16      {d20[1]}, [%[dst]]\t\n"
                        "skip_store1:\t\n"
                        "tst  %[w], #2\n\t"
                        "beq  skip_store2\t\n"
                        "vst1.32      {d20[1]}, [%[dst2]]\t\n"
                        "skip_store2:\t\n"
                        "tst  %[w], #4\n\t"
                        "beq skip_store4\t\n"
                        "vst1.16      {d21}, [%[dst4]]\t\n"
                        "skip_store4:\t\n"

                        : [w] "+r" (w), [dst] "+r" (dst), [mask] "+r" (mask), [dst4] "+r" (dst4), [dst2] "+r" (dst2)
                        : [src] "r" (src)
                        : "ip", "cc", "memory", "d0","d1","d2","d3","d4","d5","d6","d7",
                          "d16","d17","d18","d19","d20","d21","d22","d23","d24","d25","d26","d27","d28","d29",
                          "d30","d31"
                        );
#endif
        }
    }
}


void
fbCompositeSolidMask_nx8x8888neon (pixman_op_t      op,
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
    uint32_t	 w;
    uint8x8_t    sval2;
    uint8x8x4_t  sval8;
    uint8x8_t    mask_selector=vreinterpret_u8_u64(vcreate_u64(0x0101010100000000ULL));
    uint8x8_t    alpha_selector=vreinterpret_u8_u64(vcreate_u64(0x0707070703030303ULL));

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    sval2=vreinterpret_u8_u32(vdup_n_u32(src));
    sval8.val[0]=vdup_lane_u8(sval2,0);
    sval8.val[1]=vdup_lane_u8(sval2,1);
    sval8.val[2]=vdup_lane_u8(sval2,2);
    sval8.val[3]=vdup_lane_u8(sval2,3);

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    if (width>=8)
    {
        // Use overlapping 8-pixel method, modified to avoid rewritten dest being reused
        while (height--)
        {
            uint32_t *keep_dst;

            dst = dstLine;
            dstLine += dstStride;
            mask = maskLine;
            maskLine += maskStride;
            w = width;

#ifndef USE_NEON_INLINE_ASM
            uint8x8_t alpha;
            uint8x8x4_t dval, temp;

            alpha = vld1_u8((void*)mask);
            dval = vld4_u8((void*)dst);
            keep_dst = dst;

            temp = neon8mul(sval8,alpha);
            dval = neon8mul(dval,vmvn_u8(temp.val[3]));
            temp = neon8qadd(temp,dval);

            mask += (w & 7);
            dst += (w & 7);
            w -= (w & 7);

            while (w)
            {
                alpha = vld1_u8((void*)mask);
                dval = vld4_u8((void*)dst);

                vst4_u8((void*)keep_dst,temp);
                keep_dst = dst;

                temp = neon8mul(sval8,alpha);
                dval = neon8mul(dval,vmvn_u8(temp.val[3]));
                temp = neon8qadd(temp,dval);

                mask+=8;
                dst+=8;
                w-=8;
            }
            vst4_u8((void*)keep_dst,temp);
#else
        asm volatile (
                        "vdup.32      d0, %[src]\n\t"
                        "vdup.8       d1, d0[1]\n\t"
                        "vdup.8       d2, d0[2]\n\t"
                        "vdup.8       d3, d0[3]\n\t"
                        "vdup.8       d0, d0[0]\n\t"

                        "vld4.8       {d4-d7}, [%[dst]]\n\t"
                        "vld1.8       {d31}, [%[mask]]\n\t"
                        "mov  %[keep_dst], %[dst]\n\t"

                        "and  ip, %[w], #7\n\t"
                        "add  %[mask], %[mask], ip\n\t"
                        "add  %[dst], %[dst], ip, LSL#2\n\t"
                        "subs  %[w], %[w], ip\n\t"
                        "b 9f\n\t"
// LOOP
                        "2:\n\t" 
                        "vld4.8       {d4-d7}, [%[dst]]!\n\t"
                        "vld1.8       {d31}, [%[mask]]!\n\t"
                        "vst4.8       {d20-d23}, [%[keep_dst]]\n\t"
                        "sub  %[keep_dst], %[dst], #8*4\n\t"
                        "subs  %[w], %[w], #8\n\t"
                        "9:\n\t"

                        "vmull.u8     q10, d31, d0\n\t"
                        "vmull.u8     q11, d31, d1\n\t"
                        "vmull.u8     q12, d31, d2\n\t"
                        "vmull.u8     q13, d31, d3\n\t"
                        "vrshr.u16    q8, q10, #8\n\t"
                        "vrshr.u16    q9, q11, #8\n\t"
                        "vraddhn.u16  d20, q10, q8\n\t"
                        "vraddhn.u16  d21, q11, q9\n\t"
                        "vrshr.u16    q9, q13, #8\n\t"
                        "vrshr.u16    q8, q12, #8\n\t"
                        "vraddhn.u16  d23, q13, q9\n\t"
                        "vraddhn.u16  d22, q12, q8\n\t"

                        "vmvn.8       d30, d23\n\t"
                        "vmull.u8     q12, d30, d4\n\t"
                        "vmull.u8     q13, d30, d5\n\t"
                        "vmull.u8     q14, d30, d6\n\t"
                        "vmull.u8     q15, d30, d7\n\t"

                        "vrshr.u16    q8, q12, #8\n\t"
                        "vrshr.u16    q9, q13, #8\n\t"
                        "vraddhn.u16  d4, q12, q8\n\t"
                        "vrshr.u16    q8, q14, #8\n\t"
                        "vraddhn.u16  d5, q13, q9\n\t"
                        "vrshr.u16    q9, q15, #8\n\t"
                        "vraddhn.u16  d6, q14, q8\n\t"
                        "vraddhn.u16  d7, q15, q9\n\t"
// result in d4-d7

                        "vqadd.u8     d20, d4, d20\n\t"
                        "vqadd.u8     d21, d5, d21\n\t"
                        "vqadd.u8     d22, d6, d22\n\t"
                        "vqadd.u8     d23, d7, d23\n\t"

                        "bne 2b\n\t"

                        "1:\n\t"
                        "vst4.8       {d20-d23}, [%[keep_dst]]\n\t"

                        : [w] "+r" (w), [dst] "+r" (dst), [mask] "+r" (mask), [keep_dst] "+r" (keep_dst)
                        : [src] "r" (src) 
                        : "ip", "cc", "memory", "d0","d1","d2","d3","d4","d5","d6","d7",
                          "d16","d17","d18","d19","d20","d21","d22","d23","d24","d25","d26","d27","d28","d29",
                          "d30","d31"
                        );
#endif
        }
    }
    else
    {
        while (height--)
        {
            uint8x8_t alpha;

            dst = dstLine;
            dstLine += dstStride;
            mask = maskLine;
            maskLine += maskStride;
            w = width;

            while (w>=2)
            {
                uint8x8_t dval, temp, res;

                alpha = vtbl1_u8(vreinterpret_u8_u16(vld1_dup_u16((void*)mask)), mask_selector);
                dval = vld1_u8((void*)dst);

                temp = neon2mul(sval2,alpha);
                res = vqadd_u8(temp,neon2mul(dval,vtbl1_u8(vmvn_u8(temp), alpha_selector)));

                vst1_u8((void*)dst,res);

                mask+=2;
                dst+=2;
                w-=2;
            }
            if (w)
            {
                uint8x8_t dval, temp, res;

                alpha = vtbl1_u8(vld1_dup_u8((void*)mask), mask_selector);
                dval = vreinterpret_u8_u32(vld1_dup_u32((void*)dst));

                temp = neon2mul(sval2,alpha);
                res = vqadd_u8(temp,neon2mul(dval,vtbl1_u8(vmvn_u8(temp), alpha_selector)));

                vst1_lane_u32((void*)dst,vreinterpret_u32_u8(res),0);
            }
        }
    }
}


void
fbCompositeSrcAdd_8888x8x8neon (pixman_op_t op,
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
    uint8_t     *dstLine, *dst;
    uint8_t     *maskLine, *mask;
    int dstStride, maskStride;
    uint32_t    w;
    uint32_t    src;
    uint8x8_t   sa;

    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);
    fbComposeGetSolid (pSrc, src, pDst->bits.format);
    sa = vdup_n_u8((src) >> 24);

    if (width>=8)
    {
        // Use overlapping 8-pixel method, modified to avoid rewritten dest being reused
        while (height--)
        {
            dst = dstLine;
            dstLine += dstStride;
            mask = maskLine;
            maskLine += maskStride;
            w = width;

            uint8x8_t mval, dval, res;
            uint8_t     *keep_dst;

            mval = vld1_u8((void *)mask);
            dval = vld1_u8((void *)dst);
            keep_dst = dst;

            res = vqadd_u8(neon2mul(mval,sa),dval);

            mask += (w & 7);
            dst += (w & 7);
            w -= w & 7;

            while (w)
            {
                mval = vld1_u8((void *)mask);
                dval = vld1_u8((void *)dst);
                vst1_u8((void *)keep_dst, res);
                keep_dst = dst;

                res = vqadd_u8(neon2mul(mval,sa),dval);

                mask += 8;
                dst += 8;
                w -= 8;
            }
            vst1_u8((void *)keep_dst, res);
        }
    }
    else
    {
        // Use 4/2/1 load/store method to handle 1-7 pixels
        while (height--)
        {
            dst = dstLine;
            dstLine += dstStride;
            mask = maskLine;
            maskLine += maskStride;
            w = width;

            uint8x8_t mval, dval, res;
            uint8_t *dst4, *dst2;

            if (w&4)
            {
                mval = vreinterpret_u8_u32(vld1_lane_u32((void *)mask, vreinterpret_u32_u8(mval), 1));
                dval = vreinterpret_u8_u32(vld1_lane_u32((void *)dst, vreinterpret_u32_u8(dval), 1));

                dst4 = dst;
                mask += 4;
                dst += 4;
            }
            if (w&2)
            {
                mval = vreinterpret_u8_u16(vld1_lane_u16((void *)mask, vreinterpret_u16_u8(mval), 1));
                dval = vreinterpret_u8_u16(vld1_lane_u16((void *)dst, vreinterpret_u16_u8(dval), 1));
                dst2 = dst;
                mask += 2;
                dst += 2;
            }
            if (w&1)
            {
                mval = vld1_lane_u8((void *)mask, mval, 1);
                dval = vld1_lane_u8((void *)dst, dval, 1);
            }

            res = vqadd_u8(neon2mul(mval,sa),dval);

            if (w&1)
                vst1_lane_u8((void *)dst, res, 1);
            if (w&2)
                vst1_lane_u16((void *)dst2, vreinterpret_u16_u8(res), 1);
            if (w&4)
                vst1_lane_u32((void *)dst4, vreinterpret_u32_u8(res), 1);
        }
    }
}

