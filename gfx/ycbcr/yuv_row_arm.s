/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

    .arch   armv7-a
    .fpu    neon
/* Allow to build on targets not supporting neon, and force the object file
 * target to avoid bumping the final binary target */
    .object_arch armv4t
    .text
    .align

    .balign 64
YCbCr42xToRGB565_DITHER03_CONSTS_NEON:
    .short -14240
    .short -14240+384
    .short   8672
    .short   8672+192
    .short -17696
    .short -17696+384
    .byte 102
    .byte  25
    .byte  52
    .byte 129
YCbCr42xToRGB565_DITHER12_CONSTS_NEON:
    .short -14240+128
    .short -14240+256
    .short   8672+64
    .short   8672+128
    .short -17696+128
    .short -17696+256
    .byte 102
    .byte  25
    .byte  52
    .byte 129
YCbCr42xToRGB565_DITHER21_CONSTS_NEON:
    .short -14240+256
    .short -14240+128
    .short   8672+128
    .short   8672+64
    .short -17696+256
    .short -17696+128
    .byte 102
    .byte  25
    .byte  52
    .byte 129
YCbCr42xToRGB565_DITHER30_CONSTS_NEON:
    .short -14240+384
    .short -14240
    .short   8672+192
    .short   8672
    .short -17696+384
    .short -17696
    .byte 102
    .byte  25
    .byte  52
    .byte 129

@ void ScaleYCbCr42xToRGB565_BilinearY_Row_NEON(
@  yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither);
@
@ ctx = {
@   PRUint16 *rgb_row;       /*r0*/
@   const PRUint8 *y_row;    /*r1*/
@   const PRUint8 *u_row;    /*r2*/
@   const PRUint8 *v_row;    /*r3*/
@   int y_yweight;           /*r4*/
@   int y_pitch;             /*r5*/
@   int width;               /*r6*/
@   int source_x0_q16;       /*r7*/
@   int source_dx_q16;       /*r8*/
@   int source_uv_xoffs_q16; /*r9*/
@ };
    .global ScaleYCbCr42xToRGB565_BilinearY_Row_NEON
    .type   ScaleYCbCr42xToRGB565_BilinearY_Row_NEON, %function
    .balign 64
    .fnstart
ScaleYCbCr42xToRGB565_BilinearY_Row_NEON:
    STMFD       r13!,{r4-r9,r14}       @ 8 words.
    ADR         r14,YCbCr42xToRGB565_DITHER03_CONSTS_NEON
    VPUSH       {Q4-Q7}                @ 16 words.
    ADD         r14,r14,r1, LSL #4     @ Select the dither table to use
    LDMIA       r0, {r0-r9}
    @ Set up image index registers.
    ADD         r12,r8, r8
    VMOV.I32    D16,#0         @ Q8 = < 2| 2| 0| 0>*source_dx_q16
    VDUP.32     D17,r12
    ADD         r12,r12,r12
    VTRN.32     D16,D17        @ Q2 = < 2| 0| 2| 0>*source_dx_q16
    VDUP.32     D19,r12        @ Q9 = < 4| 4| ?| ?>*source_dx_q16
    ADD         r12,r12,r12
    VDUP.32     Q0, r7         @ Q0 = < 1| 1| 1| 1>*source_x0_q16
    VADD.I32    D17,D17,D19    @ Q8 = < 6| 4| 2| 0>*source_dx_q16
    CMP         r8, #0                 @ If source_dx_q16 is negative...
    VDUP.32     Q9, r12        @ Q9 = < 8| 8| 8| 8>*source_dx_q16
    ADDLT       r7, r7, r8, LSL #4     @ Make r7 point to the end of the block
    VADD.I32    Q0, Q0, Q8     @ Q0 = < 6| 4| 2| 0>*source_dx_q16+source_x0_q16
    SUBLT       r7, r7, r8             @ (i.e., the lowest address we'll use)
    VADD.I32    Q1, Q0, Q9     @ Q1 = <14|12|10| 8>*source_dx_q16+source_x0_q16
    VDUP.I32    Q9, r8         @ Q8 = < 1| 1| 1| 1>*source_dx_q16
    VADD.I32    Q2, Q0, Q9     @ Q2 = < 7| 5| 3| 1>*source_dx_q16+source_x0_q16
    VADD.I32    Q3, Q1, Q9     @ Q3 = <15|13|11| 9>*source_dx_q16+source_x0_q16
    VLD1.64     {D30,D31},[r14,:128]   @ Load some constants
    VMOV.I8     D28,#52
    VMOV.I8     D29,#129
    @ The basic idea here is to do aligned loads of a block of data and then
    @  index into it using VTBL to extract the data from the source X
    @  coordinate corresponding to each destination pixel.
    @ This is significantly less code and significantly fewer cycles than doing
    @  a series of single-lane loads, but it means that the X step between
    @  pixels must be limited to 2.0 or less, otherwise we couldn't guarantee
    @  that we could read 8 pixels from a single aligned 32-byte block of data.
    @ Q0...Q3 contain the 16.16 fixed-point X coordinates of each pixel,
    @  separated into even pixels and odd pixels to make extracting offsets and
    @  weights easier.
    @ We then pull out two bytes from the middle of each coordinate: the top
    @  byte corresponds to the integer part of the X coordinate, and the bottom
    @  byte corresponds to the weight to use for bilinear blending.
    @ These are separated out into different registers with VTRN.
    @ Then by subtracting the integer X coordinate of the first pixel in the
    @  data block we loaded, we produce an index register suitable for use by
    @  VTBL.
s42xbily_neon_loop:
    @ Load the Y' data.
    MOV         r12,r7, ASR #16
    VRSHRN.S32  D16,Q0, #8
    AND         r12,r12,#~15   @ Read 16-byte aligned blocks
    VDUP.I8     D20,r12
    ADD         r12,r1, r12    @ r12 = y_row+(source_x&~7)
    VRSHRN.S32  D17,Q1, #8
    PLD         [r12,#64]
    VLD1.64     {D8, D9, D10,D11},[r12,:128],r5        @ Load Y' top row
    ADD         r14,r7, r8, LSL #3
    VRSHRN.S32  D18,Q2, #8
    MOV         r14,r14,ASR #16
    VRSHRN.S32  D19,Q3, #8
    AND         r14,r14,#~15   @ Read 16-byte aligned blocks
    VLD1.64     {D12,D13,D14,D15},[r12,:128]           @ Load Y' bottom row
    PLD         [r12,#64]
    VDUP.I8     D21,r14
    ADD         r14,r1, r14    @ r14 = y_row+(source_x&~7)
    VMOV.I8     Q13,#1
    PLD         [r14,#64]
    VTRN.8      Q8, Q9         @ Q8  = <wFwEwDwCwBwAw9w8w7w6w5w4w3w2w1w0>
                               @ Q9  = <xFxExDxCxBxAx9x8x7x6x5x4x3x2x1x0>
    VSUB.S8     Q9, Q9, Q10    @ Make offsets relative to the data we loaded.
    @ First 8 Y' pixels
    VTBL.8      D20,{D8, D9, D10,D11},D18      @ Index top row at source_x
    VTBL.8      D24,{D12,D13,D14,D15},D18      @ Index bottom row at source_x
    VADD.S8     Q13,Q9, Q13                    @ Add 1 to source_x
    VTBL.8      D22,{D8, D9, D10,D11},D26      @ Index top row at source_x+1
    VTBL.8      D26,{D12,D13,D14,D15},D26      @ Index bottom row at source_x+1
    @ Next 8 Y' pixels
    VLD1.64     {D8, D9, D10,D11},[r14,:128],r5        @ Load Y' top row
    VLD1.64     {D12,D13,D14,D15},[r14,:128]           @ Load Y' bottom row
    PLD         [r14,#64]
    VTBL.8      D21,{D8, D9, D10,D11},D19      @ Index top row at source_x
    VTBL.8      D25,{D12,D13,D14,D15},D19      @ Index bottom row at source_x
    VTBL.8      D23,{D8, D9, D10,D11},D27      @ Index top row at source_x+1
    VTBL.8      D27,{D12,D13,D14,D15},D27      @ Index bottom row at source_x+1
    @ Blend Y'.
    VDUP.I16    Q9, r4         @ Load the y weights.
    VSUBL.U8    Q4, D24,D20    @ Q5:Q4 = c-a
    VSUBL.U8    Q5, D25,D21
    VSUBL.U8    Q6, D26,D22    @ Q7:Q6 = d-b
    VSUBL.U8    Q7, D27,D23
    VMUL.S16    Q4, Q4, Q9     @ Q5:Q4 = (c-a)*yweight
    VMUL.S16    Q5, Q5, Q9
    VMUL.S16    Q6, Q6, Q9     @ Q7:Q6 = (d-b)*yweight
    VMUL.S16    Q7, Q7, Q9
    VMOVL.U8    Q12,D16        @ Promote the x weights to 16 bits.
    VMOVL.U8    Q13,D17        @ Sadly, there's no VMULW.
    VRSHRN.S16  D8, Q4, #8     @ Q4 = (c-a)*yweight+128>>8
    VRSHRN.S16  D9, Q5, #8
    VRSHRN.S16  D12,Q6, #8     @ Q6 = (d-b)*yweight+128>>8
    VRSHRN.S16  D13,Q7, #8
    VADD.I8     Q10,Q10,Q4     @ Q10 = a+((c-a)*yweight+128>>8)
    VADD.I8     Q11,Q11,Q6     @ Q11 = b+((d-b)*yweight+128>>8)
    VSUBL.U8    Q4, D22,D20    @ Q5:Q4 = b-a
    VSUBL.U8    Q5, D23,D21
    VMUL.S16    Q4, Q4, Q12    @ Q5:Q4 = (b-a)*xweight
    VMUL.S16    Q5, Q5, Q13
    VRSHRN.S16  D8, Q4, #8     @ Q4 = (b-a)*xweight+128>>8
    ADD         r12,r7, r9
    VRSHRN.S16  D9, Q5, #8
    MOV         r12,r12,ASR #17
    VADD.I8     Q8, Q10,Q4     @ Q8 = a+((b-a)*xweight+128>>8)
    @ Start extracting the chroma x coordinates, and load Cb and Cr.
    AND         r12,r12,#~15   @ Read 16-byte aligned blocks
    VDUP.I32    Q9, r9         @ Q9 = source_uv_xoffs_q16 x 4
    ADD         r14,r2, r12
    VADD.I32    Q10,Q0, Q9
    VLD1.64     {D8, D9, D10,D11},[r14,:128]   @ Load Cb
    PLD         [r14,#64]
    VADD.I32    Q11,Q1, Q9
    ADD         r14,r3, r12
    VADD.I32    Q12,Q2, Q9
    VLD1.64     {D12,D13,D14,D15},[r14,:128]   @ Load Cr
    PLD         [r14,#64]
    VADD.I32    Q13,Q3, Q9
    VRSHRN.S32  D20,Q10,#9     @ Q10 = <xEwExCwCxAwAx8w8x6w6x4w4x2w2x0w0>
    VRSHRN.S32  D21,Q11,#9
    VDUP.I8     Q9, r12
    VRSHRN.S32  D22,Q12,#9     @ Q11 = <xFwFxDwDxBwBx9w9x7w7x5w5x3w3x1w1>
    VRSHRN.S32  D23,Q13,#9
    @ We don't actually need the x weights, but we get them for free.
    @ Free ALU slot
    VTRN.8      Q10,Q11        @ Q10 = <wFwEwDwCwBwAw9w8w7w6w5w4w3w2w1w0>
    @ Free ALU slot            @ Q11 = <xFxExDxCxBxAx9x8x7x6x5x4x3x2x1x0>
    VSUB.S8     Q11,Q11,Q9     @ Make offsets relative to the data we loaded.
    VTBL.8      D18,{D8, D9, D10,D11},D22      @ Index Cb at source_x
    VMOV.I8     D24,#74
    VTBL.8      D19,{D8, D9, D10,D11},D23
    VMOV.I8     D26,#102
    VTBL.8      D20,{D12,D13,D14,D15},D22      @ Index Cr at source_x
    VMOV.I8     D27,#25
    VTBL.8      D21,{D12,D13,D14,D15},D23
    @ We now have Y' in Q8, Cb in Q9, and Cr in Q10
    @ We use VDUP to expand constants, because it's a permute instruction, so
    @  it can dual issue on the A8.
    SUBS        r6, r6, #16    @ width -= 16
    VMULL.U8    Q4, D16,D24    @  Q5:Q4  = Y'*74
    VDUP.32     Q6, D30[1]     @  Q7:Q6  = bias_G
    VMULL.U8    Q5, D17,D24
    VDUP.32     Q7, D30[1]
    VMLSL.U8    Q6, D18,D27    @  Q7:Q6  = -25*Cb+bias_G
    VDUP.32     Q11,D30[0]     @ Q12:Q11 = bias_R
    VMLSL.U8    Q7, D19,D27
    VDUP.32     Q12,D30[0]
    VMLAL.U8    Q11,D20,D26    @ Q12:Q11 = 102*Cr+bias_R
    VDUP.32     Q8, D31[0]     @ Q13:Q8  = bias_B
    VMLAL.U8    Q12,D21,D26
    VDUP.32     Q13,D31[0]
    VMLAL.U8    Q8, D18,D29    @ Q13:Q8  = 129*Cb+bias_B
    VMLAL.U8    Q13,D19,D29
    VMLSL.U8    Q6, D20,D28    @  Q7:Q6  = -25*Cb-52*Cr+bias_G
    VMLSL.U8    Q7, D21,D28
    VADD.S16    Q11,Q4, Q11    @ Q12:Q11 = 74*Y'+102*Cr+bias_R
    VADD.S16    Q12,Q5, Q12
    VQADD.S16   Q8, Q4, Q8     @ Q13:Q8  = 74*Y'+129*Cr+bias_B
    VQADD.S16   Q13,Q5, Q13
    VADD.S16    Q6, Q4, Q6     @  Q7:Q6  = 74*Y'-25*Cb-52*Cr+bias_G
    VADD.S16    Q7, Q5, Q7
    @ Push each value to the top of its word and saturate it.
    VQSHLU.S16 Q11,Q11,#2
    VQSHLU.S16 Q12,Q12,#2
    VQSHLU.S16 Q6, Q6, #2
    VQSHLU.S16 Q7, Q7, #2
    VQSHLU.S16 Q8, Q8, #2
    VQSHLU.S16 Q13,Q13,#2
    @ Merge G and B into R.
    VSRI.U16   Q11,Q6, #5
    VSRI.U16   Q12,Q7, #5
    VSRI.U16   Q11,Q8, #11
    MOV         r14,r8, LSL #4
    VSRI.U16   Q12,Q13,#11
    BLT s42xbily_neon_tail
    VDUP.I32    Q13,r14
    @ Store the result.
    VST1.16     {D22,D23,D24,D25},[r0]!
    BEQ s42xbily_neon_done
    @ Advance the x coordinates.
    VADD.I32    Q0, Q0, Q13
    VADD.I32    Q1, Q1, Q13
    ADD         r7, r14
    VADD.I32    Q2, Q2, Q13
    VADD.I32    Q3, Q3, Q13
    B s42xbily_neon_loop
s42xbily_neon_tail:
    @ We have between 1 and 15 pixels left to write.
    @ -r6 == the number of pixels we need to skip writing.
    @ Adjust r0 to point to the last one we need to write, because we're going
    @  to write them in reverse order.
    ADD         r0, r0, r6, LSL #1
    MOV         r14,#-2
    ADD         r0, r0, #30
    @ Skip past the ones we don't need to write.
    SUB         PC, PC, r6, LSL #2
    ORR         r0, r0, r0
    VST1.16     {D25[3]},[r0,:16],r14
    VST1.16     {D25[2]},[r0,:16],r14
    VST1.16     {D25[1]},[r0,:16],r14
    VST1.16     {D25[0]},[r0,:16],r14
    VST1.16     {D24[3]},[r0,:16],r14
    VST1.16     {D24[2]},[r0,:16],r14
    VST1.16     {D24[1]},[r0,:16],r14
    VST1.16     {D24[0]},[r0,:16],r14
    VST1.16     {D23[3]},[r0,:16],r14
    VST1.16     {D23[2]},[r0,:16],r14
    VST1.16     {D23[1]},[r0,:16],r14
    VST1.16     {D23[0]},[r0,:16],r14
    VST1.16     {D22[3]},[r0,:16],r14
    VST1.16     {D22[2]},[r0,:16],r14
    VST1.16     {D22[1]},[r0,:16],r14
    VST1.16     {D22[0]},[r0,:16]
s42xbily_neon_done:
    VPOP        {Q4-Q7}                @ 16 words.
    LDMFD       r13!,{r4-r9,PC}        @ 8 words.
    .fnend
    .size ScaleYCbCr42xToRGB565_BilinearY_Row_NEON, .-ScaleYCbCr42xToRGB565_BilinearY_Row_NEON

#if defined(__ELF__)&&defined(__linux__)
    .section .note.GNU-stack,"",%progbits
#endif
