// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// contributor Siarhei Siamashka <siarhei.siamashka@gmail.com>

#include "yuv_convert.h"
#include "ycbcr_to_rgb565.h"



#ifdef HAVE_YCBCR_TO_RGB565

namespace mozilla {

namespace gfx {

#  if defined(MOZILLA_MAY_SUPPORT_NEON)
#  if defined(__clang__)
void __attribute((noinline))
#  else
void __attribute((noinline,optimize("-fomit-frame-pointer")))
#  endif
    yuv42x_to_rgb565_row_neon(uint16 *dst,
                              const uint8 *y,
                              const uint8 *u,
                              const uint8 *v,
                              int n,
                              int oddflag)
{
    static __attribute__((aligned(16))) uint16 acc_r[8] = {
        22840, 22840, 22840, 22840, 22840, 22840, 22840, 22840,
    };
    static __attribute__((aligned(16))) uint16 acc_g[8] = {
        17312, 17312, 17312, 17312, 17312, 17312, 17312, 17312,
    };
    static __attribute__((aligned(16))) uint16 acc_b[8] = {
        28832, 28832, 28832, 28832, 28832, 28832, 28832, 28832,
    };
    /*
     * Registers:
     * q0, q1 : d0, d1, d2, d3  - are used for initial loading of YUV data
     * q2     : d4, d5          - are used for storing converted RGB data
     * q3     : d6, d7          - are used for temporary storage
     *
     * q4-q7 - reserved
     *
     * q8, q9 : d16, d17, d18, d19  - are used for expanded Y data
     * q10    : d20, d21
     * q11    : d22, d23
     * q12    : d24, d25
     * q13    : d26, d27
     * q13, q14, q15            - various constants (#16, #149, #204, #50, #104, #154)
     */
    asm volatile (
".fpu neon\n"
/* Allow to build on targets not supporting neon, and force the object file
 * target to avoid bumping the final binary target */
".arch armv7-a\n"
".object_arch armv4t\n"
".macro convert_macroblock size\n"
/* load up to 16 source pixels */
	".if \\size == 16\n"
	    "pld [%[y], #64]\n"
	    "pld [%[u], #64]\n"
	    "pld [%[v], #64]\n"
	    "vld1.8 {d1}, [%[y]]!\n"
	    "vld1.8 {d3}, [%[y]]!\n"
	    "vld1.8 {d0}, [%[u]]!\n"
	    "vld1.8 {d2}, [%[v]]!\n"
	".elseif \\size == 8\n"
	    "vld1.8 {d1}, [%[y]]!\n"
	    "vld1.8 {d0[0]}, [%[u]]!\n"
	    "vld1.8 {d0[1]}, [%[u]]!\n"
	    "vld1.8 {d0[2]}, [%[u]]!\n"
	    "vld1.8 {d0[3]}, [%[u]]!\n"
	    "vld1.8 {d2[0]}, [%[v]]!\n"
	    "vld1.8 {d2[1]}, [%[v]]!\n"
	    "vld1.8 {d2[2]}, [%[v]]!\n"
	    "vld1.8 {d2[3]}, [%[v]]!\n"
	".elseif \\size == 4\n"
	    "vld1.8 {d1[0]}, [%[y]]!\n"
	    "vld1.8 {d1[1]}, [%[y]]!\n"
	    "vld1.8 {d1[2]}, [%[y]]!\n"
	    "vld1.8 {d1[3]}, [%[y]]!\n"
	    "vld1.8 {d0[0]}, [%[u]]!\n"
	    "vld1.8 {d0[1]}, [%[u]]!\n"
	    "vld1.8 {d2[0]}, [%[v]]!\n"
	    "vld1.8 {d2[1]}, [%[v]]!\n"
	".elseif \\size == 2\n"
	    "vld1.8 {d1[0]}, [%[y]]!\n"
	    "vld1.8 {d1[1]}, [%[y]]!\n"
	    "vld1.8 {d0[0]}, [%[u]]!\n"
	    "vld1.8 {d2[0]}, [%[v]]!\n"
	".elseif \\size == 1\n"
	    "vld1.8 {d1[0]}, [%[y]]!\n"
	    "vld1.8 {d0[0]}, [%[u]]!\n"
	    "vld1.8 {d2[0]}, [%[v]]!\n"
	".else\n"
	    ".error \"unsupported macroblock size\"\n"
	".endif\n"

        /* d1 - Y data (first 8 bytes) */
        /* d3 - Y data (next 8 bytes) */
        /* d0 - U data, d2 - V data */

	/* split even and odd Y color components */
	"vuzp.8      d1, d3\n"                       /* d1 - evenY, d3 - oddY */
	/* clip upper and lower boundaries */
	"vqadd.u8    q0, q0, q4\n"
	"vqadd.u8    q1, q1, q4\n"
	"vqsub.u8    q0, q0, q5\n"
	"vqsub.u8    q1, q1, q5\n"

	"vshr.u8     d4, d2, #1\n"                   /* d4 = V >> 1 */

	"vmull.u8    q8, d1, d27\n"                  /* q8 = evenY * 149 */
	"vmull.u8    q9, d3, d27\n"                  /* q9 = oddY * 149 */

	"vld1.16     {d20, d21}, [%[acc_r], :128]\n" /* q10 - initialize accumulator for red */
	"vsubw.u8    q10, q10, d4\n"                 /* red acc -= (V >> 1) */
	"vmlsl.u8    q10, d2, d28\n"                 /* red acc -= V * 204 */
	"vld1.16     {d22, d23}, [%[acc_g], :128]\n" /* q11 - initialize accumulator for green */
	"vmlsl.u8    q11, d2, d30\n"                 /* green acc -= V * 104 */
	"vmlsl.u8    q11, d0, d29\n"                 /* green acc -= U * 50 */
	"vld1.16     {d24, d25}, [%[acc_b], :128]\n" /* q12 - initialize accumulator for blue */
	"vmlsl.u8    q12, d0, d30\n"                 /* blue acc -= U * 104 */
	"vmlsl.u8    q12, d0, d31\n"                 /* blue acc -= U * 154 */

	"vhsub.s16   q3, q8, q10\n"                  /* calculate even red components */
	"vhsub.s16   q10, q9, q10\n"                 /* calculate odd red components */
	"vqshrun.s16 d0, q3, #6\n"                   /* right shift, narrow and saturate even red components */
	"vqshrun.s16 d3, q10, #6\n"                  /* right shift, narrow and saturate odd red components */

	"vhadd.s16   q3, q8, q11\n"                  /* calculate even green components */
	"vhadd.s16   q11, q9, q11\n"                 /* calculate odd green components */
	"vqshrun.s16 d1, q3, #6\n"                   /* right shift, narrow and saturate even green components */
	"vqshrun.s16 d4, q11, #6\n"                  /* right shift, narrow and saturate odd green components */

	"vhsub.s16   q3, q8, q12\n"                  /* calculate even blue components */
	"vhsub.s16   q12, q9, q12\n"                 /* calculate odd blue components */
	"vqshrun.s16 d2, q3, #6\n"                   /* right shift, narrow and saturate even blue components */
	"vqshrun.s16 d5, q12, #6\n"                  /* right shift, narrow and saturate odd blue components */

	"vzip.8      d0, d3\n"                       /* join even and odd red components */
	"vzip.8      d1, d4\n"                       /* join even and odd green components */
	"vzip.8      d2, d5\n"                       /* join even and odd blue components */

	"vshll.u8    q3, d0, #8\n\t"
	"vshll.u8    q8, d1, #8\n\t"
	"vshll.u8    q9, d2, #8\n\t"
	"vsri.u16    q3, q8, #5\t\n"
	"vsri.u16    q3, q9, #11\t\n"
	/* store pixel data to memory */
	".if \\size == 16\n"
	"    vst1.16 {d6, d7}, [%[dst]]!\n"
	"    vshll.u8    q3, d3, #8\n\t"
	"    vshll.u8    q8, d4, #8\n\t"
	"    vshll.u8    q9, d5, #8\n\t"
	"    vsri.u16    q3, q8, #5\t\n"
	"    vsri.u16    q3, q9, #11\t\n"
	"    vst1.16 {d6, d7}, [%[dst]]!\n"
	".elseif \\size == 8\n"
	"    vst1.16 {d6, d7}, [%[dst]]!\n"
	".elseif \\size == 4\n"
	"    vst1.16 {d6}, [%[dst]]!\n"
	".elseif \\size == 2\n"
	"    vst1.16 {d6[0]}, [%[dst]]!\n"
	"    vst1.16 {d6[1]}, [%[dst]]!\n"
	".elseif \\size == 1\n"
	"    vst1.16 {d6[0]}, [%[dst]]!\n"
	".endif\n"
	".endm\n"

	"vmov.u8     d8, #15\n" /* add this to U/V to saturate upper boundary */
	"vmov.u8     d9, #20\n" /* add this to Y to saturate upper boundary */
	"vmov.u8     d10, #31\n" /* sub this from U/V to saturate lower boundary */
	"vmov.u8     d11, #36\n" /* sub this from Y to saturate lower boundary */

	"vmov.u8     d26, #16\n"
	"vmov.u8     d27, #149\n"
	"vmov.u8     d28, #204\n"
	"vmov.u8     d29, #50\n"
	"vmov.u8     d30, #104\n"
	"vmov.u8     d31, #154\n"

	"cmp         %[oddflag], #0\n"
	"beq         1f\n"
	"convert_macroblock 1\n"
	"sub         %[n], %[n], #1\n"
    "1:\n"
	"subs        %[n], %[n], #16\n"
	"blt         2f\n"
    "1:\n"
	"convert_macroblock 16\n"
	"subs        %[n], %[n], #16\n"
	"bge         1b\n"
    "2:\n"
	"tst         %[n], #8\n"
	"beq         3f\n"
	"convert_macroblock 8\n"
    "3:\n"
	"tst         %[n], #4\n"
	"beq         4f\n"
	"convert_macroblock 4\n"
    "4:\n"
	"tst         %[n], #2\n"
	"beq         5f\n"
	"convert_macroblock 2\n"
    "5:\n"
	"tst         %[n], #1\n"
	"beq         6f\n"
	"convert_macroblock 1\n"
    "6:\n"
	".purgem convert_macroblock\n"
	: [y] "+&r" (y), [u] "+&r" (u), [v] "+&r" (v), [dst] "+&r" (dst), [n] "+&r" (n)
	: [acc_r] "r" (&acc_r[0]), [acc_g] "r" (&acc_g[0]), [acc_b] "r" (&acc_b[0]),
	  [oddflag] "r" (oddflag)
	: "cc", "memory",
	  "d0",  "d1",  "d2",  "d3",  "d4",  "d5",  "d6",  "d7",
	  "d8",  "d9",  "d10", "d11", /* "d12", "d13", "d14", "d15", */
	  "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
	  "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31"
    );
}
#  endif // MOZILLA_MAY_SUPPORT_NEON

} // namespace gfx

} // namespace mozilla

#endif // HAVE_YCBCR_TO_RGB565
