/*
 * Copyright © 2012 Raspberry Pi Foundation
 * Copyright © 2012 RISC OS Open Ltd
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holders make no
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
 * Author:  Ben Avison (bavison@riscosopen.org)
 *
 */

/* Prevent the stack from becoming executable */
#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif

	.text
	.arch armv6
	.object_arch armv4
	.arm
	.altmacro
	.p2align 2

#include "pixman-arm-simd-asm.h"

/* A head macro should do all processing which results in an output of up to
 * 16 bytes, as far as the final load instruction. The corresponding tail macro
 * should complete the processing of the up-to-16 bytes. The calling macro will
 * sometimes choose to insert a preload or a decrement of X between them.
 *   cond           ARM condition code for code block
 *   numbytes       Number of output bytes that should be generated this time
 *   firstreg       First WK register in which to place output
 *   unaligned_src  Whether to use non-wordaligned loads of source image
 *   unaligned_mask Whether to use non-wordaligned loads of mask image
 *   preload        If outputting 16 bytes causes 64 bytes to be read, whether an extra preload should be output
 */

.macro blit_init
        line_saved_regs STRIDE_D, STRIDE_S
.endm

.macro blit_process_head   cond, numbytes, firstreg, unaligned_src, unaligned_mask, preload
        pixld   cond, numbytes, firstreg, SRC, unaligned_src
.endm

.macro blit_inner_loop  process_head, process_tail, unaligned_src, unaligned_mask, dst_alignment
    WK4     .req    STRIDE_D
    WK5     .req    STRIDE_S
    WK6     .req    MASK
    WK7     .req    STRIDE_M
110:    pixld   , 16, 0, SRC, unaligned_src
        pixld   , 16, 4, SRC, unaligned_src
        pld     [SRC, SCRATCH]
        pixst   , 16, 0, DST
        pixst   , 16, 4, DST
        subs    X, X, #32*8/src_bpp
        bhs     110b
    .unreq  WK4
    .unreq  WK5
    .unreq  WK6
    .unreq  WK7
.endm

generate_composite_function \
    pixman_composite_src_8888_8888_asm_armv6, 32, 0, 32, \
    FLAG_DST_WRITEONLY | FLAG_COND_EXEC | FLAG_SPILL_LINE_VARS_WIDE | FLAG_PROCESS_PRESERVES_SCRATCH, \
    4, /* prefetch distance */ \
    blit_init, \
    nop_macro, /* newline */ \
    nop_macro, /* cleanup */ \
    blit_process_head, \
    nop_macro, /* process tail */ \
    blit_inner_loop

generate_composite_function \
    pixman_composite_src_0565_0565_asm_armv6, 16, 0, 16, \
    FLAG_DST_WRITEONLY | FLAG_COND_EXEC | FLAG_SPILL_LINE_VARS_WIDE | FLAG_PROCESS_PRESERVES_SCRATCH, \
    4, /* prefetch distance */ \
    blit_init, \
    nop_macro, /* newline */ \
    nop_macro, /* cleanup */ \
    blit_process_head, \
    nop_macro, /* process tail */ \
    blit_inner_loop

generate_composite_function \
    pixman_composite_src_8_8_asm_armv6, 8, 0, 8, \
    FLAG_DST_WRITEONLY | FLAG_COND_EXEC | FLAG_SPILL_LINE_VARS_WIDE | FLAG_PROCESS_PRESERVES_SCRATCH, \
    3, /* prefetch distance */ \
    blit_init, \
    nop_macro, /* newline */ \
    nop_macro, /* cleanup */ \
    blit_process_head, \
    nop_macro, /* process tail */ \
    blit_inner_loop

/******************************************************************************/

.macro src_n_8888_init
        ldr     SRC, [sp, #ARGS_STACK_OFFSET]
        mov     STRIDE_S, SRC
        mov     MASK, SRC
        mov     STRIDE_M, SRC
.endm

.macro src_n_0565_init
        ldrh    SRC, [sp, #ARGS_STACK_OFFSET]
        orr     SRC, SRC, lsl #16
        mov     STRIDE_S, SRC
        mov     MASK, SRC
        mov     STRIDE_M, SRC
.endm

.macro src_n_8_init
        ldrb    SRC, [sp, #ARGS_STACK_OFFSET]
        orr     SRC, SRC, lsl #8
        orr     SRC, SRC, lsl #16
        mov     STRIDE_S, SRC
        mov     MASK, SRC
        mov     STRIDE_M, SRC
.endm

.macro fill_process_tail  cond, numbytes, firstreg
    WK4     .req    SRC
    WK5     .req    STRIDE_S
    WK6     .req    MASK
    WK7     .req    STRIDE_M
        pixst   cond, numbytes, 4, DST
    .unreq  WK4
    .unreq  WK5
    .unreq  WK6
    .unreq  WK7
.endm

generate_composite_function \
    pixman_composite_src_n_8888_asm_armv6, 0, 0, 32, \
    FLAG_DST_WRITEONLY | FLAG_COND_EXEC | FLAG_PROCESS_PRESERVES_PSR | FLAG_PROCESS_DOES_STORE | FLAG_PROCESS_PRESERVES_SCRATCH \
    0, /* prefetch distance doesn't apply */ \
    src_n_8888_init \
    nop_macro, /* newline */ \
    nop_macro /* cleanup */ \
    nop_macro /* process head */ \
    fill_process_tail

generate_composite_function \
    pixman_composite_src_n_0565_asm_armv6, 0, 0, 16, \
    FLAG_DST_WRITEONLY | FLAG_COND_EXEC | FLAG_PROCESS_PRESERVES_PSR | FLAG_PROCESS_DOES_STORE | FLAG_PROCESS_PRESERVES_SCRATCH \
    0, /* prefetch distance doesn't apply */ \
    src_n_0565_init \
    nop_macro, /* newline */ \
    nop_macro /* cleanup */ \
    nop_macro /* process head */ \
    fill_process_tail

generate_composite_function \
    pixman_composite_src_n_8_asm_armv6, 0, 0, 8, \
    FLAG_DST_WRITEONLY | FLAG_COND_EXEC | FLAG_PROCESS_PRESERVES_PSR | FLAG_PROCESS_DOES_STORE | FLAG_PROCESS_PRESERVES_SCRATCH \
    0, /* prefetch distance doesn't apply */ \
    src_n_8_init \
    nop_macro, /* newline */ \
    nop_macro /* cleanup */ \
    nop_macro /* process head */ \
    fill_process_tail

/******************************************************************************/

.macro src_x888_8888_pixel, cond, reg
        orr&cond WK&reg, WK&reg, #0xFF000000
.endm

.macro pixman_composite_src_x888_8888_process_head   cond, numbytes, firstreg, unaligned_src, unaligned_mask, preload
        pixld   cond, numbytes, firstreg, SRC, unaligned_src
.endm

.macro pixman_composite_src_x888_8888_process_tail   cond, numbytes, firstreg
        src_x888_8888_pixel cond, %(firstreg+0)
 .if numbytes >= 8
        src_x888_8888_pixel cond, %(firstreg+1)
  .if numbytes == 16
        src_x888_8888_pixel cond, %(firstreg+2)
        src_x888_8888_pixel cond, %(firstreg+3)
  .endif
 .endif
.endm

generate_composite_function \
    pixman_composite_src_x888_8888_asm_armv6, 32, 0, 32, \
    FLAG_DST_WRITEONLY | FLAG_COND_EXEC | FLAG_PROCESS_PRESERVES_SCRATCH, \
    3, /* prefetch distance */ \
    nop_macro, /* init */ \
    nop_macro, /* newline */ \
    nop_macro, /* cleanup */ \
    pixman_composite_src_x888_8888_process_head, \
    pixman_composite_src_x888_8888_process_tail

/******************************************************************************/

.macro src_0565_8888_init
        /* Hold loop invariants in MASK and STRIDE_M */
        ldr     MASK, =0x07E007E0
        mov     STRIDE_M, #0xFF000000
        /* Set GE[3:0] to 1010 so SEL instructions do what we want */
        ldr     SCRATCH, =0x80008000
        uadd8   SCRATCH, SCRATCH, SCRATCH
.endm

.macro src_0565_8888_2pixels, reg1, reg2
        and     SCRATCH, WK&reg1, MASK             @ 00000GGGGGG0000000000gggggg00000
        bic     WK&reg2, WK&reg1, MASK             @ RRRRR000000BBBBBrrrrr000000bbbbb
        orr     SCRATCH, SCRATCH, SCRATCH, lsr #6  @ 00000GGGGGGGGGGGG0000ggggggggggg
        mov     WK&reg1, WK&reg2, lsl #16          @ rrrrr000000bbbbb0000000000000000
        mov     SCRATCH, SCRATCH, ror #19          @ GGGG0000ggggggggggg00000GGGGGGGG
        bic     WK&reg2, WK&reg2, WK&reg1, lsr #16 @ RRRRR000000BBBBB0000000000000000
        orr     WK&reg1, WK&reg1, WK&reg1, lsr #5  @ rrrrrrrrrr0bbbbbbbbbb00000000000
        orr     WK&reg2, WK&reg2, WK&reg2, lsr #5  @ RRRRRRRRRR0BBBBBBBBBB00000000000
        pkhtb   WK&reg1, WK&reg1, WK&reg1, asr #5  @ rrrrrrrr--------bbbbbbbb--------
        sel     WK&reg1, WK&reg1, SCRATCH          @ rrrrrrrrggggggggbbbbbbbb--------
        mov     SCRATCH, SCRATCH, ror #16          @ ggg00000GGGGGGGGGGGG0000gggggggg
        pkhtb   WK&reg2, WK&reg2, WK&reg2, asr #5  @ RRRRRRRR--------BBBBBBBB--------
        sel     WK&reg2, WK&reg2, SCRATCH          @ RRRRRRRRGGGGGGGGBBBBBBBB--------
        orr     WK&reg1, STRIDE_M, WK&reg1, lsr #8 @ 11111111rrrrrrrrggggggggbbbbbbbb
        orr     WK&reg2, STRIDE_M, WK&reg2, lsr #8 @ 11111111RRRRRRRRGGGGGGGGBBBBBBBB
.endm

/* This version doesn't need STRIDE_M, but is one instruction longer.
   It would however be preferable for an XRGB target, since we could knock off the last 2 instructions, but is that a common case?
        and     SCRATCH, WK&reg1, MASK             @ 00000GGGGGG0000000000gggggg00000
        bic     WK&reg1, WK&reg1, MASK             @ RRRRR000000BBBBBrrrrr000000bbbbb
        orr     SCRATCH, SCRATCH, SCRATCH, lsr #6  @ 00000GGGGGGGGGGGG0000ggggggggggg
        mov     WK&reg2, WK&reg1, lsr #16          @ 0000000000000000RRRRR000000BBBBB
        mov     SCRATCH, SCRATCH, ror #27          @ GGGGGGGGGGGG0000ggggggggggg00000
        bic     WK&reg1, WK&reg1, WK&reg2, lsl #16 @ 0000000000000000rrrrr000000bbbbb
        mov     WK&reg2, WK&reg2, lsl #3           @ 0000000000000RRRRR000000BBBBB000
        mov     WK&reg1, WK&reg1, lsl #3           @ 0000000000000rrrrr000000bbbbb000
        orr     WK&reg2, WK&reg2, WK&reg2, lsr #5  @ 0000000000000RRRRRRRRRR0BBBBBBBB
        orr     WK&reg1, WK&reg1, WK&reg1, lsr #5  @ 0000000000000rrrrrrrrrr0bbbbbbbb
        pkhbt   WK&reg2, WK&reg2, WK&reg2, lsl #5  @ --------RRRRRRRR--------BBBBBBBB
        pkhbt   WK&reg1, WK&reg1, WK&reg1, lsl #5  @ --------rrrrrrrr--------bbbbbbbb
        sel     WK&reg2, SCRATCH, WK&reg2          @ --------RRRRRRRRGGGGGGGGBBBBBBBB
        sel     WK&reg1, SCRATCH, WK&reg1          @ --------rrrrrrrrggggggggbbbbbbbb
        orr     WK&reg2, WK&reg2, #0xFF000000      @ 11111111RRRRRRRRGGGGGGGGBBBBBBBB
        orr     WK&reg1, WK&reg1, #0xFF000000      @ 11111111rrrrrrrrggggggggbbbbbbbb
*/

.macro src_0565_8888_1pixel, reg
        bic     SCRATCH, WK&reg, MASK              @ 0000000000000000rrrrr000000bbbbb
        and     WK&reg, WK&reg, MASK               @ 000000000000000000000gggggg00000
        mov     SCRATCH, SCRATCH, lsl #3           @ 0000000000000rrrrr000000bbbbb000
        mov     WK&reg, WK&reg, lsl #5             @ 0000000000000000gggggg0000000000
        orr     SCRATCH, SCRATCH, SCRATCH, lsr #5  @ 0000000000000rrrrrrrrrr0bbbbbbbb
        orr     WK&reg, WK&reg, WK&reg, lsr #6     @ 000000000000000gggggggggggg00000
        pkhbt   SCRATCH, SCRATCH, SCRATCH, lsl #5  @ --------rrrrrrrr--------bbbbbbbb
        sel     WK&reg, WK&reg, SCRATCH            @ --------rrrrrrrrggggggggbbbbbbbb
        orr     WK&reg, WK&reg, #0xFF000000        @ 11111111rrrrrrrrggggggggbbbbbbbb
.endm

.macro src_0565_8888_process_head   cond, numbytes, firstreg, unaligned_src, unaligned_mask, preload
 .if numbytes == 16
        pixldst ld,, 8, firstreg, %(firstreg+2),,, SRC, unaligned_src
 .elseif numbytes == 8
        pixld   , 4, firstreg, SRC, unaligned_src
 .elseif numbytes == 4
        pixld   , 2, firstreg, SRC, unaligned_src
 .endif
.endm

.macro src_0565_8888_process_tail   cond, numbytes, firstreg
 .if numbytes == 16
        src_0565_8888_2pixels firstreg, %(firstreg+1)
        src_0565_8888_2pixels %(firstreg+2), %(firstreg+3)
 .elseif numbytes == 8
        src_0565_8888_2pixels firstreg, %(firstreg+1)
 .else
        src_0565_8888_1pixel firstreg
 .endif
.endm

generate_composite_function \
    pixman_composite_src_0565_8888_asm_armv6, 16, 0, 32, \
    FLAG_DST_WRITEONLY | FLAG_BRANCH_OVER, \
    3, /* prefetch distance */ \
    src_0565_8888_init, \
    nop_macro, /* newline */ \
    nop_macro, /* cleanup */ \
    src_0565_8888_process_head, \
    src_0565_8888_process_tail

/******************************************************************************/

.macro add_8_8_8pixels  cond, dst1, dst2
        uqadd8&cond  WK&dst1, WK&dst1, MASK
        uqadd8&cond  WK&dst2, WK&dst2, STRIDE_M
.endm

.macro add_8_8_4pixels  cond, dst
        uqadd8&cond  WK&dst, WK&dst, MASK
.endm

.macro add_8_8_process_head  cond, numbytes, firstreg, unaligned_src, unaligned_mask, preload
    WK4     .req    MASK
    WK5     .req    STRIDE_M
 .if numbytes == 16
        pixld   cond, 8, 4, SRC, unaligned_src
        pixld   cond, 16, firstreg, DST, 0
        add_8_8_8pixels cond, firstreg, %(firstreg+1)
        pixld   cond, 8, 4, SRC, unaligned_src
 .else
        pixld   cond, numbytes, 4, SRC, unaligned_src
        pixld   cond, numbytes, firstreg, DST, 0
 .endif
    .unreq  WK4
    .unreq  WK5
.endm

.macro add_8_8_process_tail  cond, numbytes, firstreg
 .if numbytes == 16
        add_8_8_8pixels cond, %(firstreg+2), %(firstreg+3)
 .elseif numbytes == 8
        add_8_8_8pixels cond, firstreg, %(firstreg+1)
 .else
        add_8_8_4pixels cond, firstreg
 .endif
.endm

generate_composite_function \
    pixman_composite_add_8_8_asm_armv6, 8, 0, 8, \
    FLAG_DST_READWRITE | FLAG_BRANCH_OVER | FLAG_PROCESS_PRESERVES_SCRATCH, \
    2, /* prefetch distance */ \
    nop_macro, /* init */ \
    nop_macro, /* newline */ \
    nop_macro, /* cleanup */ \
    add_8_8_process_head, \
    add_8_8_process_tail

/******************************************************************************/

.macro over_8888_8888_init
        /* Hold loop invariant in MASK */
        ldr     MASK, =0x00800080
        /* Set GE[3:0] to 0101 so SEL instructions do what we want */
        uadd8   SCRATCH, MASK, MASK
        line_saved_regs STRIDE_D, STRIDE_S, ORIG_W
.endm

.macro over_8888_8888_process_head  cond, numbytes, firstreg, unaligned_src, unaligned_mask, preload
    WK4     .req    STRIDE_D
    WK5     .req    STRIDE_S
    WK6     .req    STRIDE_M
    WK7     .req    ORIG_W
        pixld   , numbytes, %(4+firstreg), SRC, unaligned_src
        pixld   , numbytes, firstreg, DST, 0
    .unreq  WK4
    .unreq  WK5
    .unreq  WK6
    .unreq  WK7
.endm

.macro over_8888_8888_check_transparent  numbytes, reg0, reg1, reg2, reg3
        /* Since these colours a premultiplied by alpha, only 0 indicates transparent (any other colour with 0 in the alpha byte is luminous) */
        teq     WK&reg0, #0
 .if numbytes > 4
        teqeq   WK&reg1, #0
  .if numbytes > 8
        teqeq   WK&reg2, #0
        teqeq   WK&reg3, #0
  .endif
 .endif
.endm

.macro over_8888_8888_prepare  next
        mov     WK&next, WK&next, lsr #24
.endm

.macro over_8888_8888_1pixel src, dst, offset, next
        /* src = destination component multiplier */
        rsb     WK&src, WK&src, #255
        /* Split even/odd bytes of dst into SCRATCH/dst */
        uxtb16  SCRATCH, WK&dst
        uxtb16  WK&dst, WK&dst, ror #8
        /* Multiply through, adding 0.5 to the upper byte of result for rounding */
        mla     SCRATCH, SCRATCH, WK&src, MASK
        mla     WK&dst, WK&dst, WK&src, MASK
        /* Where we would have had a stall between the result of the first MLA and the shifter input,
         * reload the complete source pixel */
        ldr     WK&src, [SRC, #offset]
        /* Multiply by 257/256 to approximate 256/255 */
        uxtab16 SCRATCH, SCRATCH, SCRATCH, ror #8
        /* In this stall, start processing the next pixel */
 .if offset < -4
        mov     WK&next, WK&next, lsr #24
 .endif
        uxtab16 WK&dst, WK&dst, WK&dst, ror #8
        /* Recombine even/odd bytes of multiplied destination */
        mov     SCRATCH, SCRATCH, ror #8
        sel     WK&dst, SCRATCH, WK&dst
        /* Saturated add of source to multiplied destination */
        uqadd8  WK&dst, WK&dst, WK&src
.endm

.macro over_8888_8888_process_tail  cond, numbytes, firstreg
    WK4     .req    STRIDE_D
    WK5     .req    STRIDE_S
    WK6     .req    STRIDE_M
    WK7     .req    ORIG_W
        over_8888_8888_check_transparent numbytes, %(4+firstreg), %(5+firstreg), %(6+firstreg), %(7+firstreg)
        beq     10f
        over_8888_8888_prepare  %(4+firstreg)
 .set PROCESS_REG, firstreg
 .set PROCESS_OFF, -numbytes
 .rept numbytes / 4
        over_8888_8888_1pixel %(4+PROCESS_REG), %(0+PROCESS_REG), PROCESS_OFF, %(5+PROCESS_REG)
  .set PROCESS_REG, PROCESS_REG+1
  .set PROCESS_OFF, PROCESS_OFF+4
 .endr
        pixst   , numbytes, firstreg, DST
10:
    .unreq  WK4
    .unreq  WK5
    .unreq  WK6
    .unreq  WK7
.endm

generate_composite_function \
    pixman_composite_over_8888_8888_asm_armv6, 32, 0, 32 \
    FLAG_DST_READWRITE | FLAG_BRANCH_OVER | FLAG_PROCESS_CORRUPTS_PSR | FLAG_PROCESS_DOES_STORE | FLAG_SPILL_LINE_VARS \
    2, /* prefetch distance */ \
    over_8888_8888_init, \
    nop_macro, /* newline */ \
    nop_macro, /* cleanup */ \
    over_8888_8888_process_head, \
    over_8888_8888_process_tail

/******************************************************************************/

/* Multiply each byte of a word by a byte.
 * Useful when there aren't any obvious ways to fill the stalls with other instructions.
 * word  Register containing 4 bytes
 * byte  Register containing byte multiplier (bits 8-31 must be 0)
 * tmp   Scratch register
 * half  Register containing the constant 0x00800080
 * GE[3:0] bits must contain 0101
 */
.macro mul_8888_8  word, byte, tmp, half
        /* Split even/odd bytes of word apart */
        uxtb16  tmp, word
        uxtb16  word, word, ror #8
        /* Multiply bytes together with rounding, then by 257/256 */
        mla     tmp, tmp, byte, half
        mla     word, word, byte, half /* 1 stall follows */
        uxtab16 tmp, tmp, tmp, ror #8  /* 1 stall follows */
        uxtab16 word, word, word, ror #8
        /* Recombine bytes */
        mov     tmp, tmp, ror #8
        sel     word, tmp, word
.endm

/******************************************************************************/

.macro over_8888_n_8888_init
        /* Mask is constant */
        ldr     MASK, [sp, #ARGS_STACK_OFFSET+8]
        /* Hold loop invariant in STRIDE_M */
        ldr     STRIDE_M, =0x00800080
        /* We only want the alpha bits of the constant mask */
        mov     MASK, MASK, lsr #24
        /* Set GE[3:0] to 0101 so SEL instructions do what we want */
        uadd8   SCRATCH, STRIDE_M, STRIDE_M
        line_saved_regs Y, STRIDE_D, STRIDE_S, ORIG_W
.endm

.macro over_8888_n_8888_process_head  cond, numbytes, firstreg, unaligned_src, unaligned_mask, preload
    WK4     .req    Y
    WK5     .req    STRIDE_D
    WK6     .req    STRIDE_S
    WK7     .req    ORIG_W
        pixld   , numbytes, %(4+(firstreg%2)), SRC, unaligned_src
        pixld   , numbytes, firstreg, DST, 0
    .unreq  WK4
    .unreq  WK5
    .unreq  WK6
    .unreq  WK7
.endm

.macro over_8888_n_8888_1pixel src, dst
        mul_8888_8  WK&src, MASK, SCRATCH, STRIDE_M
        sub     WK7, WK6, WK&src, lsr #24
        mul_8888_8  WK&dst, WK7, SCRATCH, STRIDE_M
        uqadd8  WK&dst, WK&dst, WK&src
.endm

.macro over_8888_n_8888_process_tail  cond, numbytes, firstreg
    WK4     .req    Y
    WK5     .req    STRIDE_D
    WK6     .req    STRIDE_S
    WK7     .req    ORIG_W
        over_8888_8888_check_transparent numbytes, %(4+(firstreg%2)), %(5+(firstreg%2)), %(6+firstreg), %(7+firstreg)
        beq     10f
        mov     WK6, #255
 .set PROCESS_REG, firstreg
 .rept numbytes / 4
  .if numbytes == 16 && PROCESS_REG == 2
        /* We're using WK6 and WK7 as temporaries, so half way through
         * 4 pixels, reload the second two source pixels but this time
         * into WK4 and WK5 */
        ldmdb   SRC, {WK4, WK5}
  .endif
        over_8888_n_8888_1pixel  %(4+(PROCESS_REG%2)), %(PROCESS_REG)
  .set PROCESS_REG, PROCESS_REG+1
 .endr
        pixst   , numbytes, firstreg, DST
10:
    .unreq  WK4
    .unreq  WK5
    .unreq  WK6
    .unreq  WK7
.endm

generate_composite_function \
    pixman_composite_over_8888_n_8888_asm_armv6, 32, 0, 32 \
    FLAG_DST_READWRITE | FLAG_BRANCH_OVER | FLAG_PROCESS_CORRUPTS_PSR | FLAG_PROCESS_DOES_STORE | FLAG_SPILL_LINE_VARS \
    2, /* prefetch distance */ \
    over_8888_n_8888_init, \
    nop_macro, /* newline */ \
    nop_macro, /* cleanup */ \
    over_8888_n_8888_process_head, \
    over_8888_n_8888_process_tail

/******************************************************************************/

.macro over_n_8_8888_init
        /* Source is constant, but splitting it into even/odd bytes is a loop invariant */
        ldr     SRC, [sp, #ARGS_STACK_OFFSET]
        /* Not enough registers to hold this constant, but we still use it here to set GE[3:0] */
        ldr     SCRATCH, =0x00800080
        uxtb16  STRIDE_S, SRC
        uxtb16  SRC, SRC, ror #8
        /* Set GE[3:0] to 0101 so SEL instructions do what we want */
        uadd8   SCRATCH, SCRATCH, SCRATCH
        line_saved_regs Y, STRIDE_D, STRIDE_M, ORIG_W
.endm

.macro over_n_8_8888_newline
        ldr     STRIDE_D, =0x00800080
        b       1f
 .ltorg
1:
.endm

.macro over_n_8_8888_process_head  cond, numbytes, firstreg, unaligned_src, unaligned_mask, preload
    WK4     .req    STRIDE_M
        pixld   , numbytes/4, 4, MASK, unaligned_mask
        pixld   , numbytes, firstreg, DST, 0
    .unreq  WK4
.endm

.macro over_n_8_8888_1pixel src, dst
        uxtb    Y, WK4, ror #src*8
        /* Trailing part of multiplication of source */
        mla     SCRATCH, STRIDE_S, Y, STRIDE_D
        mla     Y, SRC, Y, STRIDE_D
        mov     ORIG_W, #255
        uxtab16 SCRATCH, SCRATCH, SCRATCH, ror #8
        uxtab16 Y, Y, Y, ror #8
        mov     SCRATCH, SCRATCH, ror #8
        sub     ORIG_W, ORIG_W, Y, lsr #24
        sel     Y, SCRATCH, Y
        /* Then multiply the destination */
        mul_8888_8  WK&dst, ORIG_W, SCRATCH, STRIDE_D
        uqadd8  WK&dst, WK&dst, Y
.endm

.macro over_n_8_8888_process_tail  cond, numbytes, firstreg
    WK4     .req    STRIDE_M
        teq     WK4, #0
        beq     10f
 .set PROCESS_REG, firstreg
 .rept numbytes / 4
        over_n_8_8888_1pixel  %(PROCESS_REG-firstreg), %(PROCESS_REG)
  .set PROCESS_REG, PROCESS_REG+1
 .endr
        pixst   , numbytes, firstreg, DST
10:
    .unreq  WK4
.endm

generate_composite_function \
    pixman_composite_over_n_8_8888_asm_armv6, 0, 8, 32 \
    FLAG_DST_READWRITE | FLAG_BRANCH_OVER | FLAG_PROCESS_CORRUPTS_PSR | FLAG_PROCESS_DOES_STORE | FLAG_SPILL_LINE_VARS \
    2, /* prefetch distance */ \
    over_n_8_8888_init, \
    over_n_8_8888_newline, \
    nop_macro, /* cleanup */ \
    over_n_8_8888_process_head, \
    over_n_8_8888_process_tail

/******************************************************************************/

