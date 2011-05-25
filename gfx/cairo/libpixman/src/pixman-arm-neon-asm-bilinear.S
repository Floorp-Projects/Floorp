/*
 * Copyright © 2011 SCore Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author:  Siarhei Siamashka (siarhei.siamashka@nokia.com)
 * Author:  Taekyun Kim (tkq.kim@samsung.com)
 */

/*
 * This file contains scaled bilinear scanline functions implemented
 * using older siarhei's bilinear macro template.
 *
 * << General scanline function procedures >>
 *  1. bilinear interpolate source pixels
 *  2. load mask pixels
 *  3. load destination pixels
 *  4. duplicate mask to fill whole register
 *  5. interleave source & destination pixels
 *  6. apply mask to source pixels
 *  7. combine source & destination pixels
 *  8, Deinterleave final result
 *  9. store destination pixels
 *
 * All registers with single number (i.e. src0, tmp0) are 64-bits registers.
 * Registers with double numbers(src01, dst01) are 128-bits registers.
 * All temp registers can be used freely outside the code block.
 * Assume that symbol(register .req) OUT and MASK are defined at caller of these macro blocks.
 *
 * TODOs
 *  Support 0565 pixel format
 *  Optimization for two and last pixel cases
 *
 * Remarks
 *  There can be lots of pipeline stalls inside code block and between code blocks.
 *  Further optimizations will be done by new macro templates using head/tail_head/tail scheme.
 */

/* Prevent the stack from becoming executable for no reason... */
#if defined(__linux__) && defined (__ELF__)
.section .note.GNU-stack,"",%progbits
#endif

.text
.fpu neon
.arch armv7a
.object_arch armv4
.eabi_attribute 10, 0
.eabi_attribute 12, 0
.arm
.altmacro

#include "pixman-arm-neon-asm.h"

/*
 * Bilinear macros from pixman-arm-neon-asm.S
 */

/* Supplementary macro for setting function attributes */
.macro pixman_asm_function fname
    .func fname
    .global fname
#ifdef __ELF__
    .hidden fname
    .type fname, %function
#endif
fname:
.endm

/*
 * Bilinear scaling support code which tries to provide pixel fetching, color
 * format conversion, and interpolation as separate macros which can be used
 * as the basic building blocks for constructing bilinear scanline functions.
 */

.macro bilinear_load_8888 reg1, reg2, tmp
    mov       TMP2, X, asr #16
    add       X, X, UX
    add       TMP1, TOP, TMP2, asl #2
    add       TMP2, BOTTOM, TMP2, asl #2
    vld1.32   {reg1}, [TMP1]
    vld1.32   {reg2}, [TMP2]
.endm

.macro bilinear_load_0565 reg1, reg2, tmp
    mov       TMP2, X, asr #16
    add       X, X, UX
    add       TMP1, TOP, TMP2, asl #1
    add       TMP2, BOTTOM, TMP2, asl #1
    vld1.32   {reg2[0]}, [TMP1]
    vld1.32   {reg2[1]}, [TMP2]
    convert_four_0565_to_x888_packed reg2, reg1, reg2, tmp
.endm

.macro bilinear_load_and_vertical_interpolate_two_8888 \
                    acc1, acc2, reg1, reg2, reg3, reg4, tmp1, tmp2

    bilinear_load_8888 reg1, reg2, tmp1
    vmull.u8  acc1, reg1, d28
    vmlal.u8  acc1, reg2, d29
    bilinear_load_8888 reg3, reg4, tmp2
    vmull.u8  acc2, reg3, d28
    vmlal.u8  acc2, reg4, d29
.endm

.macro bilinear_load_and_vertical_interpolate_four_8888 \
                xacc1, xacc2, xreg1, xreg2, xreg3, xreg4, xacc2lo, xacc2hi \
                yacc1, yacc2, yreg1, yreg2, yreg3, yreg4, yacc2lo, yacc2hi

    bilinear_load_and_vertical_interpolate_two_8888 \
                xacc1, xacc2, xreg1, xreg2, xreg3, xreg4, xacc2lo, xacc2hi
    bilinear_load_and_vertical_interpolate_two_8888 \
                yacc1, yacc2, yreg1, yreg2, yreg3, yreg4, yacc2lo, yacc2hi
.endm

.macro bilinear_load_and_vertical_interpolate_two_0565 \
                acc1, acc2, reg1, reg2, reg3, reg4, acc2lo, acc2hi

    mov       TMP2, X, asr #16
    add       X, X, UX
    mov       TMP4, X, asr #16
    add       X, X, UX
    add       TMP1, TOP, TMP2, asl #1
    add       TMP2, BOTTOM, TMP2, asl #1
    add       TMP3, TOP, TMP4, asl #1
    add       TMP4, BOTTOM, TMP4, asl #1
    vld1.32   {acc2lo[0]}, [TMP1]
    vld1.32   {acc2hi[0]}, [TMP3]
    vld1.32   {acc2lo[1]}, [TMP2]
    vld1.32   {acc2hi[1]}, [TMP4]
    convert_0565_to_x888 acc2, reg3, reg2, reg1
    vzip.u8   reg1, reg3
    vzip.u8   reg2, reg4
    vzip.u8   reg3, reg4
    vzip.u8   reg1, reg2
    vmull.u8  acc1, reg1, d28
    vmlal.u8  acc1, reg2, d29
    vmull.u8  acc2, reg3, d28
    vmlal.u8  acc2, reg4, d29
.endm

.macro bilinear_load_and_vertical_interpolate_four_0565 \
                xacc1, xacc2, xreg1, xreg2, xreg3, xreg4, xacc2lo, xacc2hi \
                yacc1, yacc2, yreg1, yreg2, yreg3, yreg4, yacc2lo, yacc2hi

    mov       TMP2, X, asr #16
    add       X, X, UX
    mov       TMP4, X, asr #16
    add       X, X, UX
    add       TMP1, TOP, TMP2, asl #1
    add       TMP2, BOTTOM, TMP2, asl #1
    add       TMP3, TOP, TMP4, asl #1
    add       TMP4, BOTTOM, TMP4, asl #1
    vld1.32   {xacc2lo[0]}, [TMP1]
    vld1.32   {xacc2hi[0]}, [TMP3]
    vld1.32   {xacc2lo[1]}, [TMP2]
    vld1.32   {xacc2hi[1]}, [TMP4]
    convert_0565_to_x888 xacc2, xreg3, xreg2, xreg1
    mov       TMP2, X, asr #16
    add       X, X, UX
    mov       TMP4, X, asr #16
    add       X, X, UX
    add       TMP1, TOP, TMP2, asl #1
    add       TMP2, BOTTOM, TMP2, asl #1
    add       TMP3, TOP, TMP4, asl #1
    add       TMP4, BOTTOM, TMP4, asl #1
    vld1.32   {yacc2lo[0]}, [TMP1]
    vzip.u8   xreg1, xreg3
    vld1.32   {yacc2hi[0]}, [TMP3]
    vzip.u8   xreg2, xreg4
    vld1.32   {yacc2lo[1]}, [TMP2]
    vzip.u8   xreg3, xreg4
    vld1.32   {yacc2hi[1]}, [TMP4]
    vzip.u8   xreg1, xreg2
    convert_0565_to_x888 yacc2, yreg3, yreg2, yreg1
    vmull.u8  xacc1, xreg1, d28
    vzip.u8   yreg1, yreg3
    vmlal.u8  xacc1, xreg2, d29
    vzip.u8   yreg2, yreg4
    vmull.u8  xacc2, xreg3, d28
    vzip.u8   yreg3, yreg4
    vmlal.u8  xacc2, xreg4, d29
    vzip.u8   yreg1, yreg2
    vmull.u8  yacc1, yreg1, d28
    vmlal.u8  yacc1, yreg2, d29
    vmull.u8  yacc2, yreg3, d28
    vmlal.u8  yacc2, yreg4, d29
.endm

.macro bilinear_store_8888 numpix, tmp1, tmp2
.if numpix == 4
    vst1.32   {d0, d1}, [OUT]!
.elseif numpix == 2
    vst1.32   {d0}, [OUT]!
.elseif numpix == 1
    vst1.32   {d0[0]}, [OUT, :32]!
.else
    .error bilinear_store_8888 numpix is unsupported
.endif
.endm

.macro bilinear_store_0565 numpix, tmp1, tmp2
    vuzp.u8 d0, d1
    vuzp.u8 d2, d3
    vuzp.u8 d1, d3
    vuzp.u8 d0, d2
    convert_8888_to_0565 d2, d1, d0, q1, tmp1, tmp2
.if numpix == 4
    vst1.16   {d2}, [OUT]!
.elseif numpix == 2
    vst1.32   {d2[0]}, [OUT]!
.elseif numpix == 1
    vst1.16   {d2[0]}, [OUT]!
.else
    .error bilinear_store_0565 numpix is unsupported
.endif
.endm


/*
 * Macros for loading mask pixels into register 'mask'.
 * vdup must be done in somewhere else.
 */
.macro bilinear_load_mask_x numpix, mask
.endm

.macro bilinear_load_mask_8 numpix, mask
.if numpix == 4
    vld1.32     {mask[0]}, [MASK]!
.elseif numpix == 2
    vld1.16     {mask[0]}, [MASK]!
.elseif numpix == 1
    vld1.8      {mask[0]}, [MASK]!
.else
    .error bilinear_load_mask_8 numpix is unsupported
.endif
.endm

.macro bilinear_load_mask mask_fmt, numpix, mask
    bilinear_load_mask_&mask_fmt numpix, mask
.endm


/*
 * Macros for loading destination pixels into register 'dst0' and 'dst1'.
 * Interleave should be done somewhere else.
 */
.macro bilinear_load_dst_0565_src numpix, dst0, dst1, dst01
.endm

.macro bilinear_load_dst_8888_src numpix, dst0, dst1, dst01
.endm

.macro bilinear_load_dst_8888 numpix, dst0, dst1, dst01
.if numpix == 4
    vld1.32     {dst0, dst1}, [OUT]
.elseif numpix == 2
    vld1.32     {dst0}, [OUT]
.elseif numpix == 1
    vld1.32     {dst0[0]}, [OUT]
.else
    .error bilinear_load_dst_8888 numpix is unsupported
.endif
.endm

.macro bilinear_load_dst_8888_over numpix, dst0, dst1, dst01
    bilinear_load_dst_8888 numpix, dst0, dst1, dst01
.endm

.macro bilinear_load_dst_8888_add numpix, dst0, dst1, dst01
    bilinear_load_dst_8888 numpix, dst0, dst1, dst01
.endm

.macro bilinear_load_dst dst_fmt, op, numpix, dst0, dst1, dst01
    bilinear_load_dst_&dst_fmt&_&op numpix, dst0, dst1, dst01
.endm

/*
 * Macros for duplicating partially loaded mask to fill entire register.
 * We will apply mask to interleaved source pixels, that is
 *  (r0, r1, r2, r3, g0, g1, g2, g3) x (m0, m1, m2, m3, m0, m1, m2, m3)
 *  (b0, b1, b2, b3, a0, a1, a2, a3) x (m0, m1, m2, m3, m0, m1, m2, m3)
 * So, we need to duplicate loaded mask into whole register.
 *
 * For two pixel case
 *  (r0, r1, x, x, g0, g1, x, x) x (m0, m1, m0, m1, m0, m1, m0, m1)
 *  (b0, b1, x, x, a0, a1, x, x) x (m0, m1, m0, m1, m0, m1, m0, m1)
 * We can do some optimizations for this including one pixel cases.
 */
.macro bilinear_duplicate_mask_x numpix, mask
.endm

.macro bilinear_duplicate_mask_8 numpix, mask
.if numpix == 4
    vdup.32     mask, mask[0]
.elseif numpix == 2
    vdup.16     mask, mask[0]
.elseif numpix == 1
    vdup.8      mask, mask[0]
.else
    .error bilinear_duplicate_mask_8 is unsupported
.endif
.endm

.macro bilinear_duplicate_mask mask_fmt, numpix, mask
    bilinear_duplicate_mask_&mask_fmt numpix, mask
.endm

/*
 * Macros for interleaving src and dst pixels to rrrr gggg bbbb aaaa form.
 * Interleave should be done when maks is enabled or operator is 'over'.
 */
.macro bilinear_interleave src0, src1, dst0, dst1
    vuzp.8      src0, src1
    vuzp.8      dst0, dst1
    vuzp.8      src0, src1
    vuzp.8      dst0, dst1
.endm

.macro bilinear_interleave_src_dst_x_src \
                numpix, src0, src1, src01, dst0, dst1, dst01
.endm

.macro bilinear_interleave_src_dst_x_over \
                numpix, src0, src1, src01, dst0, dst1, dst01

    bilinear_interleave src0, src1, dst0, dst1
.endm

.macro bilinear_interleave_src_dst_x_add \
                numpix, src0, src1, src01, dst0, dst1, dst01
.endm

.macro bilinear_interleave_src_dst_8_src \
                numpix, src0, src1, src01, dst0, dst1, dst01

    bilinear_interleave src0, src1, dst0, dst1
.endm

.macro bilinear_interleave_src_dst_8_over \
                numpix, src0, src1, src01, dst0, dst1, dst01

    bilinear_interleave src0, src1, dst0, dst1
.endm

.macro bilinear_interleave_src_dst_8_add \
                numpix, src0, src1, src01, dst0, dst1, dst01

    bilinear_interleave src0, src1, dst0, dst1
.endm

.macro bilinear_interleave_src_dst \
                mask_fmt, op, numpix, src0, src1, src01, dst0, dst1, dst01

    bilinear_interleave_src_dst_&mask_fmt&_&op \
                numpix, src0, src1, src01, dst0, dst1, dst01
.endm


/*
 * Macros for applying masks to src pixels. (see combine_mask_u() function)
 * src, dst should be in interleaved form.
 * mask register should be in form (m0, m1, m2, m3).
 */
.macro bilinear_apply_mask_to_src_x \
                numpix, src0, src1, src01, mask, \
                tmp01, tmp23, tmp45, tmp67
.endm

.macro bilinear_apply_mask_to_src_8 \
                numpix, src0, src1, src01, mask, \
                tmp01, tmp23, tmp45, tmp67

    vmull.u8        tmp01, src0, mask
    vmull.u8        tmp23, src1, mask
    /* bubbles */
    vrshr.u16       tmp45, tmp01, #8
    vrshr.u16       tmp67, tmp23, #8
    /* bubbles */
    vraddhn.u16     src0, tmp45, tmp01
    vraddhn.u16     src1, tmp67, tmp23
.endm

.macro bilinear_apply_mask_to_src \
                mask_fmt, numpix, src0, src1, src01, mask, \
                tmp01, tmp23, tmp45, tmp67

    bilinear_apply_mask_to_src_&mask_fmt \
                numpix, src0, src1, src01, mask, \
                tmp01, tmp23, tmp45, tmp67
.endm


/*
 * Macros for combining src and destination pixels.
 * Interleave or not is depending on operator 'op'.
 */
.macro bilinear_combine_src \
                numpix, src0, src1, src01, dst0, dst1, dst01, \
                tmp01, tmp23, tmp45, tmp67, tmp8
.endm

.macro bilinear_combine_over \
                numpix, src0, src1, src01, dst0, dst1, dst01, \
                tmp01, tmp23, tmp45, tmp67, tmp8

    vdup.32     tmp8, src1[1]
    /* bubbles */
    vmvn.8      tmp8, tmp8
    /* bubbles */
    vmull.u8    tmp01, dst0, tmp8
    /* bubbles */
    vmull.u8    tmp23, dst1, tmp8
    /* bubbles */
    vrshr.u16   tmp45, tmp01, #8
    vrshr.u16   tmp67, tmp23, #8
    /* bubbles */
    vraddhn.u16 dst0, tmp45, tmp01
    vraddhn.u16 dst1, tmp67, tmp23
    /* bubbles */
    vqadd.u8    src01, dst01, src01
.endm

.macro bilinear_combine_add \
                numpix, src0, src1, src01, dst0, dst1, dst01, \
                tmp01, tmp23, tmp45, tmp67, tmp8

    vqadd.u8    src01, dst01, src01
.endm

.macro bilinear_combine \
                op, numpix, src0, src1, src01, dst0, dst1, dst01, \
                tmp01, tmp23, tmp45, tmp67, tmp8

    bilinear_combine_&op \
                numpix, src0, src1, src01, dst0, dst1, dst01, \
                tmp01, tmp23, tmp45, tmp67, tmp8
.endm

/*
 * Macros for final deinterleaving of destination pixels if needed.
 */
.macro bilinear_deinterleave numpix, dst0, dst1, dst01
    vuzp.8      dst0, dst1
    /* bubbles */
    vuzp.8      dst0, dst1
.endm

.macro bilinear_deinterleave_dst_x_src numpix, dst0, dst1, dst01
.endm

.macro bilinear_deinterleave_dst_x_over numpix, dst0, dst1, dst01
    bilinear_deinterleave numpix, dst0, dst1, dst01
.endm

.macro bilinear_deinterleave_dst_x_add numpix, dst0, dst1, dst01
.endm

.macro bilinear_deinterleave_dst_8_src numpix, dst0, dst1, dst01
    bilinear_deinterleave numpix, dst0, dst1, dst01
.endm

.macro bilinear_deinterleave_dst_8_over numpix, dst0, dst1, dst01
    bilinear_deinterleave numpix, dst0, dst1, dst01
.endm

.macro bilinear_deinterleave_dst_8_add numpix, dst0, dst1, dst01
    bilinear_deinterleave numpix, dst0, dst1, dst01
.endm

.macro bilinear_deinterleave_dst mask_fmt, op, numpix, dst0, dst1, dst01
    bilinear_deinterleave_dst_&mask_fmt&_&op numpix, dst0, dst1, dst01
.endm


.macro bilinear_interpolate_last_pixel src_fmt, mask_fmt, dst_fmt, op
    bilinear_load_&src_fmt d0, d1, d2
    bilinear_load_mask mask_fmt, 1, d4
    bilinear_load_dst dst_fmt, op, 1, d18, d19, q9
    vmull.u8  q1, d0, d28
    vmlal.u8  q1, d1, d29
    vshr.u16  d30, d24, #8
    /* 4 cycles bubble */
    vshll.u16 q0, d2, #8
    vmlsl.u16 q0, d2, d30
    vmlal.u16 q0, d3, d30
    /* 5 cycles bubble */
    bilinear_duplicate_mask mask_fmt, 1, d4
    vshrn.u32 d0, q0, #16
    /* 3 cycles bubble */
    vmovn.u16 d0, q0
    /* 1 cycle bubble */
    bilinear_interleave_src_dst \
                mask_fmt, op, 1, d0, d1, q0, d18, d19, q9
    bilinear_apply_mask_to_src \
                mask_fmt, 1, d0, d1, q0, d4, \
                q3, q8, q10, q11
    bilinear_combine \
                op, 1, d0, d1, q0, d18, d19, q9, \
                q3, q8, q10, q11, d5
    bilinear_deinterleave_dst mask_fmt, op, 1, d0, d1, q0
    bilinear_store_&dst_fmt 1, q2, q3
.endm

.macro bilinear_interpolate_two_pixels src_fmt, mask_fmt, dst_fmt, op
    bilinear_load_and_vertical_interpolate_two_&src_fmt \
                q1, q11, d0, d1, d20, d21, d22, d23
    bilinear_load_mask mask_fmt, 2, d4
    bilinear_load_dst dst_fmt, op, 2, d18, d19, q9
    vshr.u16  q15, q12, #8
    vadd.u16  q12, q12, q13
    vshll.u16 q0, d2, #8
    vmlsl.u16 q0, d2, d30
    vmlal.u16 q0, d3, d30
    vshll.u16 q10, d22, #8
    vmlsl.u16 q10, d22, d31
    vmlal.u16 q10, d23, d31
    vshrn.u32 d30, q0, #16
    vshrn.u32 d31, q10, #16
    bilinear_duplicate_mask mask_fmt, 2, d4
    vmovn.u16 d0, q15
    bilinear_interleave_src_dst \
                mask_fmt, op, 2, d0, d1, q0, d18, d19, q9
    bilinear_apply_mask_to_src \
                mask_fmt, 2, d0, d1, q0, d4, \
                q3, q8, q10, q11
    bilinear_combine \
                op, 2, d0, d1, q0, d18, d19, q9, \
                q3, q8, q10, q11, d5
    bilinear_deinterleave_dst mask_fmt, op, 2, d0, d1, q0
    bilinear_store_&dst_fmt 2, q2, q3
.endm

.macro bilinear_interpolate_four_pixels src_fmt, mask_fmt, dst_fmt, op
    bilinear_load_and_vertical_interpolate_four_&src_fmt \
                q1, q11, d0, d1, d20, d21, d22, d23 \
                q3, q9,  d4, d5, d16, d17, d18, d19
    pld       [TMP1, PF_OFFS]
    vshr.u16  q15, q12, #8
    vadd.u16  q12, q12, q13
    vshll.u16 q0, d2, #8
    vmlsl.u16 q0, d2, d30
    vmlal.u16 q0, d3, d30
    vshll.u16 q10, d22, #8
    vmlsl.u16 q10, d22, d31
    vmlal.u16 q10, d23, d31
    vshr.u16  q15, q12, #8
    vshll.u16 q2, d6, #8
    vmlsl.u16 q2, d6, d30
    vmlal.u16 q2, d7, d30
    vshll.u16 q8, d18, #8
    bilinear_load_mask mask_fmt, 4, d30
    bilinear_load_dst dst_fmt, op, 4, d2, d3, q1
    pld       [TMP2, PF_OFFS]
    vmlsl.u16 q8, d18, d31
    vmlal.u16 q8, d19, d31
    vadd.u16  q12, q12, q13
    vshrn.u32 d0, q0, #16
    vshrn.u32 d1, q10, #16
    vshrn.u32 d4, q2, #16
    vshrn.u32 d5, q8, #16
    bilinear_duplicate_mask mask_fmt, 4, d30
    vmovn.u16 d0, q0
    vmovn.u16 d1, q2
    bilinear_interleave_src_dst \
                mask_fmt, op, 4, d0, d1, q0, d2, d3, q1
    bilinear_apply_mask_to_src \
                mask_fmt, 4, d0, d1, q0, d30, \
                q3, q8, q9, q10
    bilinear_combine \
                op, 4, d0, d1, q0, d2, d3, q1, \
                q3, q8, q9, q10, d22
    bilinear_deinterleave_dst mask_fmt, op, 4, d0, d1, q0
    bilinear_store_&dst_fmt 4, q2, q3
.endm

.macro generate_bilinear_scanline_func_src_dst \
                fname, src_fmt, dst_fmt, op, \
                bpp_shift, prefetch_distance

pixman_asm_function fname
    OUT       .req      r0
    TOP       .req      r1
    BOTTOM    .req      r2
    WT        .req      r3
    WB        .req      r4
    X         .req      r5
    UX        .req      r6
    WIDTH     .req      ip
    TMP1      .req      r3
    TMP2      .req      r4
    PF_OFFS   .req      r7
    TMP3      .req      r8
    TMP4      .req      r9

    mov       ip, sp
    push      {r4, r5, r6, r7, r8, r9}
    mov       PF_OFFS, #prefetch_distance
    ldmia     ip, {WB, X, UX, WIDTH}
    mul       PF_OFFS, PF_OFFS, UX

    cmp       WIDTH, #0
    ble       3f

    vdup.u16  q12, X
    vdup.u16  q13, UX
    vdup.u8   d28, WT
    vdup.u8   d29, WB
    vadd.u16  d25, d25, d26
    vadd.u16  q13, q13, q13

    subs      WIDTH, WIDTH, #4
    blt       1f
    mov       PF_OFFS, PF_OFFS, asr #(16 - bpp_shift)
0:
    bilinear_interpolate_four_pixels src_fmt, x, dst_fmt, op
    subs      WIDTH, WIDTH, #4
    bge       0b
1:
    tst       WIDTH, #2
    beq       2f
    bilinear_interpolate_two_pixels src_fmt, x, dst_fmt, op
2:
    tst       WIDTH, #1
    beq       3f
    bilinear_interpolate_last_pixel src_fmt, x, dst_fmt, op
3:
    pop       {r4, r5, r6, r7, r8, r9}
    bx        lr

    .unreq    OUT
    .unreq    TOP
    .unreq    BOTTOM
    .unreq    WT
    .unreq    WB
    .unreq    X
    .unreq    UX
    .unreq    WIDTH
    .unreq    TMP1
    .unreq    TMP2
    .unreq    PF_OFFS
    .unreq    TMP3
    .unreq    TMP4
.endfunc

.endm

.macro generate_bilinear_scanline_func_src_a8_dst \
                fname, src_fmt, dst_fmt, op, \
                bpp_shift, prefetch_distance

pixman_asm_function fname
    OUT       .req      r0
    MASK      .req      r1
    TOP       .req      r2
    BOTTOM    .req      r3
    WT        .req      r4
    WB        .req      r5
    X         .req      r6
    UX        .req      r7
    WIDTH     .req      ip
    TMP1      .req      r4
    TMP2      .req      r5
    PF_OFFS   .req      r8
    TMP3      .req      r9
    TMP4      .req      r10

    mov       ip, sp
    push      {r4, r5, r6, r7, r8, r9, r10, ip}
    mov       PF_OFFS, #prefetch_distance
    ldmia     ip, {WT, WB, X, UX, WIDTH}
    mul       PF_OFFS, PF_OFFS, UX

    cmp       WIDTH, #0
    ble       3f

    vdup.u16  q12, X
    vdup.u16  q13, UX
    vdup.u8   d28, WT
    vdup.u8   d29, WB
    vadd.u16  d25, d25, d26
    vadd.u16  q13, q13, q13

    subs      WIDTH, WIDTH, #4
    blt       1f
    mov       PF_OFFS, PF_OFFS, asr #(16 - bpp_shift)
0:
    bilinear_interpolate_four_pixels src_fmt, 8, dst_fmt, op
    subs      WIDTH, WIDTH, #4
    bge       0b
1:
    tst       WIDTH, #2
    beq       2f
    bilinear_interpolate_two_pixels src_fmt, 8, dst_fmt, op
2:
    tst       WIDTH, #1
    beq       3f
    bilinear_interpolate_last_pixel src_fmt, 8, dst_fmt, op
3:
    pop       {r4, r5, r6, r7, r8, r9, r10, ip}
    bx        lr

    .unreq    OUT
    .unreq    TOP
    .unreq    BOTTOM
    .unreq    WT
    .unreq    WB
    .unreq    X
    .unreq    UX
    .unreq    WIDTH
    .unreq    MASK
    .unreq    TMP1
    .unreq    TMP2
    .unreq    PF_OFFS
    .unreq    TMP3
    .unreq    TMP4
.endfunc

.endm

generate_bilinear_scanline_func_src_dst \
    pixman_scaled_bilinear_scanline_8888_8888_OVER_asm_neon, \
    8888, 8888, over, 2, 28

generate_bilinear_scanline_func_src_dst \
    pixman_scaled_bilinear_scanline_8888_8888_ADD_asm_neon, \
    8888, 8888, add, 2, 28

generate_bilinear_scanline_func_src_a8_dst \
    pixman_scaled_bilinear_scanline_8888_8_8888_SRC_asm_neon, \
    8888, 8888, src, 2, 28

generate_bilinear_scanline_func_src_a8_dst \
    pixman_scaled_bilinear_scanline_8888_8_0565_SRC_asm_neon, \
    8888, 0565, src, 2, 28

generate_bilinear_scanline_func_src_a8_dst \
    pixman_scaled_bilinear_scanline_0565_8_x888_SRC_asm_neon, \
    0565, 8888, src, 1, 28

generate_bilinear_scanline_func_src_a8_dst \
    pixman_scaled_bilinear_scanline_0565_8_0565_SRC_asm_neon, \
    0565, 0565, src, 1, 28

generate_bilinear_scanline_func_src_a8_dst \
    pixman_scaled_bilinear_scanline_8888_8_8888_OVER_asm_neon, \
    8888, 8888, over, 2, 28

generate_bilinear_scanline_func_src_a8_dst \
    pixman_scaled_bilinear_scanline_8888_8_8888_ADD_asm_neon, \
    8888, 8888, add, 2, 28
