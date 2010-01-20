
   ;;; WARNING - AUTO GENERATED
   INCLUDE kxarm.h

   area pixman_msvc, code, readonly

   MACRO
   FUNC_HEADER $Name
FuncName    SETS    VBar:CC:"$Name":CC:VBar
PrologName  SETS    VBar:CC:"$Name":CC:"_Prolog":CC:VBar
FuncEndName SETS    VBar:CC:"$Name":CC:"_end":CC:VBar

   AREA |.pdata|,ALIGN=2,PDATA
   DCD $FuncName
   DCD (($PrologName-$FuncName)/4) :OR: ((($FuncEndName-$FuncName)/4):SHL:8) :OR: 0x40000000
   AREA $AreaName,CODE,READONLY
   ALIGN   2
   GLOBAL  $FuncName
   EXPORT  $FuncName
$FuncName
   ROUT
$PrologName
   MEND
        
   IMPORT  _pixman_image_get_solid

    FUNC_HEADER arm_composite_add_8000_8000
    stmfd	sp!, {r4, r5, r6, r7, r8, fp, lr}
    add	fp, sp, #24	; 0x18
    ldr	r3, [r2, #132]
    ldr	r0, [fp, #4]
    ldr	ip, [r2, #124]
    mov	r6, r3, lsl #2
    ldr	r1, [r0, #132]
    ldr	r3, [r0, #124]
    ldr	r0, [fp, #28]
    mov	r7, r1, lsl #2
    ldr	r1, [fp, #12]
    mla	r0, r7, r0, r3
    mla	r1, r6, r1, ip
    ldr	r4, [fp, #36]
    ldr	r3, [fp, #8]
    ldr	r2, [fp, #24]
    subs	r4, r4, #1	; 0x1
    ldr	r8, [fp, #32]
    add	r5, r1, r3
    add	r0, r0, r2
    bcc	arm_composite_add_8000_80000
arm_composite_add_8000_80005
    uxth	lr, r8
    cmp	lr, #0	; 0x0
    movne	ip, r0
    movne	r1, r5
    beq	arm_composite_add_8000_80001
arm_composite_add_8000_80004
    tst	ip, #3	; 0x3
    sub	r3, lr, #1	; 0x1
    bne	arm_composite_add_8000_80002
    tst	r1, #3	; 0x3
    beq	arm_composite_add_8000_80003
arm_composite_add_8000_80002
    uxth	lr, r3
    cmp	lr, #0	; 0x0
    ldrb	r3, [ip]
    ldrb	r2, [r1], #1
    uqadd8	r3, r2, r3
    strb	r3, [ip], #1
    bne	arm_composite_add_8000_80004
arm_composite_add_8000_80001
    add	r0, r0, r7
    add	r5, r5, r6
arm_composite_add_8000_800010
    subs	r4, r4, #1	; 0x1
    bcs	arm_composite_add_8000_80005
arm_composite_add_8000_80000
    ldmfd	sp!, {r4, r5, r6, r7, r8, fp, pc}
arm_composite_add_8000_80003
    cmp	lr, #3	; 0x3
    bls	arm_composite_add_8000_80006
arm_composite_add_8000_80007
    sub	r3, lr, #4	; 0x4
    ldr	r2, [r1], #4
    uxth	lr, r3
    cmp	lr, #3	; 0x3
    ldr	r3, [ip]
    uqadd8	r2, r2, r3
    str	r2, [ip], #4
    bhi	arm_composite_add_8000_80007
arm_composite_add_8000_80006
    cmp	lr, #0	; 0x0
    beq	arm_composite_add_8000_80001
arm_composite_add_8000_80009
    sub	r3, lr, #1	; 0x1
    ldrb	r2, [ip]
    uxth	lr, r3
    cmp	lr, #0	; 0x0
    ldrb	r3, [r1], #1
    uqadd8	r2, r3, r2
    strb	r2, [ip], #1
    bne	arm_composite_add_8000_80009
    add	r0, r0, r7
    add	r5, r5, r6
    b	arm_composite_add_8000_800010
    ENTRY_END
    ENDP

    FUNC_HEADER arm_composite_over_8888_8888
    stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    add	fp, sp, #32	; 0x20
    sub	sp, sp, #28	; 0x1c
    ldr	r3, [fp, #4]
    ldr	r0, [fp, #36]
    ldr	r4, [r2, #132]
    cmp	r0, #0	; 0x0
    ldr	r5, [r3, #132]
    ldr	r1, [r3, #124]
    ldr	lr, [r2, #124]
    beq	arm_composite_over_8888_88880
    ldr	r2, [fp, #28]
    ldr	ip, [fp, #24]
    ldr	r0, [fp, #12]
    mla	r3, r5, r2, ip
    ldr	ip, [fp, #8]
    ldrh	r9, [fp, #32]
    mla	r2, r4, r0, ip
    mov	r0, r3, lsl #2
    add	sl, r0, r1
    mov	ip, r2, lsl #2
    add	lr, ip, lr
    mov	r5, r5, lsl #2
    mov	r4, r4, lsl #2
    mov	r0, #0	; 0x0
    str	r5, [fp, #-48]
    str	r4, [fp, #-44]
    str	r0, [fp, #-40]
arm_composite_over_8888_88885
    ldr	r1, [fp, #-48]
    ldr	r2, [fp, #-44]
    add	r1, r1, sl
    str	r1, [fp, #-56]
    add	ip, lr, r2
    ldr	r1, arm_composite_over_8888_88881 ; 0x800080
    ldr	r2, arm_composite_over_8888_88882 ; 0xff00ff00
    mov	r3, #255	; 0xff
    mov	r0, r9
    cmp	r0, #0	; 0x0
    beq	arm_composite_over_8888_88883
arm_composite_over_8888_88884
    ldr	r5, [lr], #4
    ldr	r4, [sl]
    sub	r8, r3, r5, lsr #24
    uxtb16	r6, r4
    uxtb16	r7, r4, ror #8
    mla	r6, r6, r8, r1
    mla	r7, r7, r8, r1
    uxtab16	r6, r6, r6, ror #8
    uxtab16	r7, r7, r7, ror #8
    and	r7, r7, r2
    uxtab16	r6, r7, r6, ror #8
    uqadd8	r5, r6, r5
    str	r5, [sl], #4
    subs	r0, r0, #1	; 0x1
    bne	arm_composite_over_8888_88884
arm_composite_over_8888_88883
    ldr	r1, [fp, #-40]
    ldr	r2, [fp, #36]
    add	r1, r1, #1	; 0x1
    cmp	r1, r2
    ldr	sl, [fp, #-56]
    mov	lr, ip
    str	r1, [fp, #-40]
    bne	arm_composite_over_8888_88885
arm_composite_over_8888_88880
    sub	sp, fp, #32	; 0x20
    ldmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
arm_composite_over_8888_88881
    dcd 0x00800080
arm_composite_over_8888_88882
    dcd 0xff00ff00
    ENTRY_END
    ENDP

    FUNC_HEADER fbComposite_x8r8g8b8_src_r5g6b5_internal_armv6
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv611
    stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr}
    ldr	r4, [sp, #40]
    ldr	ip, fbComposite_x8r8g8b8_src_r5g6b5_internal_armv60 ; 0x1f001f
    sub	r3, r3, r2
    str	r3, [sp, #40]
    ldr	r3, [sp, #44]
    sub	r4, r4, r2
    str	r4, [sp, #44]
    str	r2, [sp, #32]
    subs	r3, r3, #1	; 0x1
    blt	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv61
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv610
    tst	r0, #2	; 0x2
    beq	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv62
    ldr	r4, [r1], #4
    sub	r2, r2, #1	; 0x1
    and	lr, ip, r4, lsr #3
    and	r4, r4, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r4, lr, r4, lsr #5
    strh	r4, [r0], #2
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv62
    tst	r0, #4	; 0x4
    beq	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv63
    ldmia	r1!, {r4, r5}
    sub	r2, r2, #2	; 0x2
    and	lr, ip, r4, lsr #3
    and	r4, r4, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r4, lr, r4, lsr #5
    and	lr, ip, r5, lsr #3
    and	r5, r5, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r5, lr, r5, lsr #5
    pkhbt	r4, r4, r5, lsl #16
    str	r4, [r0], #4
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv63
    tst	r0, #8	; 0x8
    beq	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv64
    ldmia	r1!, {r4, r5, r6, r7}
    sub	r2, r2, #4	; 0x4
    and	lr, ip, r4, lsr #3
    and	r4, r4, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r4, lr, r4, lsr #5
    and	lr, ip, r5, lsr #3
    and	r5, r5, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r5, lr, r5, lsr #5
    and	lr, ip, r6, lsr #3
    and	r6, r6, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r6, lr, r6, lsr #5
    and	lr, ip, r7, lsr #3
    and	r7, r7, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r7, lr, r7, lsr #5
    pkhbt	r4, r4, r5, lsl #16
    pkhbt	r6, r6, r7, lsl #16
    stmia	r0!, {r4, r6}
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv64
    subs	r2, r2, #8	; 0x8
    blt	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv65
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv66
    ldmia	r1!, {r4, r5, r6, r7, r8, r9, sl, fp}
    subs	r2, r2, #8	; 0x8
    and	lr, ip, r4, lsr #3
    and	r4, r4, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r4, lr, r4, lsr #5
    and	lr, ip, r5, lsr #3
    and	r5, r5, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r5, lr, r5, lsr #5
    and	lr, ip, r6, lsr #3
    and	r6, r6, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r6, lr, r6, lsr #5
    and	lr, ip, r7, lsr #3
    and	r7, r7, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r7, lr, r7, lsr #5
    and	lr, ip, r8, lsr #3
    and	r8, r8, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r8, lr, r8, lsr #5
    and	lr, ip, r9, lsr #3
    and	r9, r9, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r9, lr, r9, lsr #5
    and	lr, ip, sl, lsr #3
    and	sl, sl, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	sl, lr, sl, lsr #5
    and	lr, ip, fp, lsr #3
    and	fp, fp, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	fp, lr, fp, lsr #5
    pkhbt	r4, r4, r5, lsl #16
    pkhbt	r6, r6, r7, lsl #16
    pkhbt	r8, r8, r9, lsl #16
    pkhbt	sl, sl, fp, lsl #16
    stmia	r0!, {r4, r6, r8, sl}
    bge	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv66
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv65
    tst	r2, #4	; 0x4
    beq	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv67
    ldmia	r1!, {r4, r5, r6, r7}
    and	lr, ip, r4, lsr #3
    and	r4, r4, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r4, lr, r4, lsr #5
    and	lr, ip, r5, lsr #3
    and	r5, r5, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r5, lr, r5, lsr #5
    and	lr, ip, r6, lsr #3
    and	r6, r6, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r6, lr, r6, lsr #5
    and	lr, ip, r7, lsr #3
    and	r7, r7, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r7, lr, r7, lsr #5
    pkhbt	r4, r4, r5, lsl #16
    pkhbt	r6, r6, r7, lsl #16
    stmia	r0!, {r4, r6}
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv67
    tst	r2, #2	; 0x2
    beq	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv68
    ldmia	r1!, {r4, r5}
    and	lr, ip, r4, lsr #3
    and	r4, r4, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r4, lr, r4, lsr #5
    and	lr, ip, r5, lsr #3
    and	r5, r5, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r5, lr, r5, lsr #5
    pkhbt	r4, r4, r5, lsl #16
    str	r4, [r0], #4
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv68
    tst	r2, #1	; 0x1
    beq	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv69
    ldr	r4, [r1], #4
    and	lr, ip, r4, lsr #3
    and	r4, r4, #64512	; 0xfc00
    orr	lr, lr, lr, lsr #5
    orr	r4, lr, r4, lsr #5
    strh	r4, [r0], #2
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv69
    ldr	r4, [sp, #40]
    ldr	r5, [sp, #44]
    ldr	r2, [sp, #32]
    subs	r3, r3, #1	; 0x1
    add	r0, r0, r4, lsl #1
    add	r1, r1, r5, lsl #2
    bge	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv610
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv61
    ldmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, pc}
fbComposite_x8r8g8b8_src_r5g6b5_internal_armv60
    dcd 0x001f001f
    ENTRY_END
    ENDP

    FUNC_HEADER arm_composite_src_8888_0565
    stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    add	fp, sp, #32	; 0x20
    sub	sp, sp, #12	; 0xc
    ldr	lr, [fp, #4]
    ldr	r5, [r2, #132]
    ldr	r0, [fp, #12]
    ldr	r3, [lr, #132]
    ldr	r1, [fp, #28]
    ldrh	r6, [fp, #32]
    mov	r4, r3, lsl #1
    ldr	r3, [fp, #8]
    ldr	ip, [r2, #124]
    mla	r0, r5, r0, r3
    ldr	r3, [fp, #24]
    cmp	r6, #6	; 0x6
    mla	r1, r4, r1, r3
    ldr	r3, [lr, #124]
    mov	r0, r0, lsl #2
    mov	r1, r1, lsl #1
    add	sl, r0, ip
    add	r1, r1, r3
    ldrh	ip, [fp, #36]
    bhi	arm_composite_src_8888_05650
    cmp	ip, #0	; 0x0
    beq	arm_composite_src_8888_05651
    rsb	r3, r6, r5
    rsb	r2, r6, r4
    ldr	lr, arm_composite_src_8888_05652 ; 0x1f001f
    mov	r0, r1
    sub	r5, ip, #1	; 0x1
    mov	r8, r3, lsl #2
    mov	r7, r2, lsl #1
    mov	r1, sl
    b	arm_composite_src_8888_05653
arm_composite_src_8888_05659
    tst	r0, #2	; 0x2
    sub	ip, r6, #1	; 0x1
    beq	arm_composite_src_8888_05654
    ldr	r3, [r1], #4
    and	r2, r3, #64512	; 0xfc00
    and	r3, lr, r3, lsr #3
    orr	r2, r3, r2, lsr #5
    orr	r2, r2, r3, lsr #5
    strh	r2, [r0], #2
arm_composite_src_8888_056510
    mov	r9, r1
    mov	r4, r0
    subs	ip, ip, #2	; 0x2
    blt	arm_composite_src_8888_05655
arm_composite_src_8888_05656
    ldr	r2, [r9], #4
    ldr	r3, [r9], #4
    subs	ip, ip, #2	; 0x2
    and	sl, lr, r2, lsr #3
    and	r2, r2, #64512	; 0xfc00
    orr	sl, sl, sl, lsr #5
    orr	r2, sl, r2, lsr #5
    and	sl, lr, r3, lsr #3
    and	r3, r3, #64512	; 0xfc00
    orr	sl, sl, sl, lsr #5
    orr	r3, sl, r3, lsr #5
    pkhbt	r2, r2, r3, lsl #16
    str	r2, [r4], #4
    bge	arm_composite_src_8888_05656
arm_composite_src_8888_05655
    tst	ip, #1	; 0x1
    mov	r0, r9
    beq	arm_composite_src_8888_05657
    ldr	r3, [r0], #4
    and	r2, r3, #64512	; 0xfc00
    and	r3, lr, r3, lsr #3
    orr	r2, r3, r2, lsr #5
    orr	r2, r2, r3, lsr #5
    strh	r2, [r4], #2
arm_composite_src_8888_05657
    cmp	r5, #0	; 0x0
    add	r1, r0, r8
    sub	r5, r5, #1	; 0x1
    add	r0, r4, r7
    beq	arm_composite_src_8888_05651
arm_composite_src_8888_05653
    cmp	r6, #0	; 0x0
    bne	arm_composite_src_8888_05659
arm_composite_src_8888_05654
    mov	ip, r6
    b	arm_composite_src_8888_056510
arm_composite_src_8888_05650
    mov	r0, r1
    mov	r2, r6
    mov	r1, sl
    mov	r3, r4
    stmea	sp, {r5, ip}
    bl	fbComposite_x8r8g8b8_src_r5g6b5_internal_armv611
arm_composite_src_8888_05651
    sub	sp, fp, #32	; 0x20
    ldmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
arm_composite_src_8888_05652
    dcd 0x001f001f
    ENTRY_END
    ENDP

    FUNC_HEADER arm_composite_over_n_8_8888
    stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    add	fp, sp, #32	; 0x20
    sub	sp, sp, #36	; 0x24
    ldr	r4, [fp, #4]
    mov	r0, r2
    mov	r5, r3
    ldr	r1, [r4, #108]
arm_composite_over_n_8_88880
    bl	_pixman_image_get_solid
    cmp	r0, #0	; 0x0
    str	r0, [fp, #-56]
    beq	arm_composite_over_n_8_88881
    ldr	r1, [fp, #36]
    mov	r3, r0, lsr #8
    bic	r3, r3, #-16777216	; 0xff000000
    bic	r3, r3, #65280	; 0xff00
    cmp	r1, #0	; 0x0
    bic	r9, r0, #-16777216	; 0xff000000
    str	r3, [fp, #-48]
    bic	r9, r9, #65280	; 0xff00
    ldr	r1, [r4, #132]
    ldr	ip, [r5, #132]
    ldr	lr, [r4, #124]
    ldr	r3, [r5, #124]
    beq	arm_composite_over_n_8_88881
    ldr	r2, [fp, #28]
    ldr	r4, [fp, #24]
    mov	ip, ip, lsl #2
    mla	r0, r1, r2, r4
    ldr	r2, [fp, #20]
    mov	r1, r1, lsl #2
    str	r1, [fp, #-44]
    mla	r1, ip, r2, r3
    ldrh	sl, [fp, #32]
    ldr	r3, [fp, #16]
    mov	r2, r0, lsl #2
    str	ip, [fp, #-52]
    add	r0, r1, r3
    add	ip, r2, lr
    mov	r4, #0	; 0x0
    str	r4, [fp, #-40]
arm_composite_over_n_8_88886
    ldr	r1, [fp, #-44]
    ldr	r2, [fp, #-52]
    add	r1, ip, r1
    add	r2, r0, r2
    str	r1, [fp, #-64]
    str	r2, [fp, #-68]
    ldr	r3, arm_composite_over_n_8_88883 ; 0x800080
    mov	lr, sl
    ldr	r1, [fp, #-56]
    ldr	r2, [fp, #-48]
    cmp	lr, #0	; 0x0
    beq	arm_composite_over_n_8_88884
arm_composite_over_n_8_88885
    ldrb	r5, [r0], #1
    ldr	r4, [ip]
    mla	r6, r9, r5, r3
    mla	r7, r2, r5, r3
    uxtab16	r6, r6, r6, ror #8
    uxtab16	r7, r7, r7, ror #8
    uxtb16	r6, r6, ror #8
    uxtb16	r7, r7, ror #8
    orr	r5, r6, r7, lsl #8
    uxtb16	r6, r4
    uxtb16	r7, r4, ror #8
    mvn	r8, r5
    mov	r8, r8, lsr #24
    mla	r6, r6, r8, r3
    mla	r7, r7, r8, r3
    uxtab16	r6, r6, r6, ror #8
    uxtab16	r7, r7, r7, ror #8
    uxtb16	r6, r6, ror #8
    uxtb16	r7, r7, ror #8
    orr	r6, r6, r7, lsl #8
    uqadd8	r5, r6, r5
    str	r5, [ip], #4
    subs	lr, lr, #1	; 0x1
    bne	arm_composite_over_n_8_88885
arm_composite_over_n_8_88884
    ldr	r3, [fp, #-40]
    ldr	r4, [fp, #36]
    add	r3, r3, #1	; 0x1
    cmp	r3, r4
    str	r1, [fp, #-56]
    ldr	ip, [fp, #-64]
    ldr	r0, [fp, #-68]
    str	r3, [fp, #-40]
    bne	arm_composite_over_n_8_88886
arm_composite_over_n_8_88881
    sub	sp, fp, #32	; 0x20
    ldmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
arm_composite_over_n_8_88883
    dcd 0x00800080
    ENTRY_END
    ENDP

    FUNC_HEADER arm_composite_over_8888_n_8888
    stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    add	fp, sp, #32	; 0x20
    sub	sp, sp, #28	; 0x1c
    ldr	ip, [fp, #4]
    ldr	r1, arm_composite_over_8888_n_88880 ; 0x20028888
    mov	r0, r3
    ldr	r7, [ip, #132]
    ldr	r6, [r2, #132]
    ldr	r4, [ip, #124]
    ldr	r5, [r2, #124]
arm_composite_over_8888_n_88881
    bl	_pixman_image_get_solid
    ldr	r1, [fp, #36]
    cmp	r1, #0	; 0x0
    mov	r0, r0, lsr #24
    str	r0, [fp, #-40]
    beq	arm_composite_over_8888_n_88882
    ldr	r2, [fp, #28]
    ldr	r0, [fp, #24]
    ldr	r1, [fp, #12]
    mla	r3, r7, r2, r0
    ldr	r0, [fp, #8]
    ldrh	sl, [fp, #32]
    mla	r2, r6, r1, r0
    mov	r1, r3, lsl #2
    add	lr, r1, r4
    mov	r0, r2, lsl #2
    add	ip, r0, r5
    mov	r7, r7, lsl #2
    mov	r6, r6, lsl #2
    mov	r1, #0	; 0x0
    str	r7, [fp, #-52]
    str	r6, [fp, #-48]
    str	r1, [fp, #-44]
arm_composite_over_8888_n_88886
    ldr	r2, [fp, #-52]
    ldr	r3, [fp, #-48]
    add	r2, lr, r2
    add	r3, ip, r3
    str	r2, [fp, #-56]
    str	r3, [fp, #-60]
    ldr	r2, arm_composite_over_8888_n_88883 ; 0x800080
    mov	r3, #255	; 0xff
    mov	r0, sl
    ldr	r1, [fp, #-40]
    cmp	r0, #0	; 0x0
    beq	arm_composite_over_8888_n_88884
arm_composite_over_8888_n_88885
    ldr	r5, [ip], #4
    ldr	r4, [lr]
    uxtb16	r6, r5
    uxtb16	r7, r5, ror #8
    mla	r6, r6, r1, r2
    mla	r7, r7, r1, r2
    uxtab16	r6, r6, r6, ror #8
    uxtab16	r7, r7, r7, ror #8
    uxtb16	r6, r6, ror #8
    uxtb16	r7, r7, ror #8
    orr	r5, r6, r7, lsl #8
    uxtb16	r6, r4
    uxtb16	r7, r4, ror #8
    sub	r8, r3, r5, lsr #24
    mla	r6, r6, r8, r2
    mla	r7, r7, r8, r2
    uxtab16	r6, r6, r6, ror #8
    uxtab16	r7, r7, r7, ror #8
    uxtb16	r6, r6, ror #8
    uxtb16	r7, r7, ror #8
    orr	r6, r6, r7, lsl #8
    uqadd8	r5, r6, r5
    str	r5, [lr], #4
    subs	r0, r0, #1	; 0x1
    bne	arm_composite_over_8888_n_88885
arm_composite_over_8888_n_88884
    ldr	r2, [fp, #-44]
    ldr	r3, [fp, #36]
    add	r2, r2, #1	; 0x1
    cmp	r2, r3
    ldr	lr, [fp, #-56]
    ldr	ip, [fp, #-60]
    str	r2, [fp, #-44]
    bne	arm_composite_over_8888_n_88886
arm_composite_over_8888_n_88882
    sub	sp, fp, #32	; 0x20
    ldmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
arm_composite_over_8888_n_88880
    dcd 0x20028888
arm_composite_over_8888_n_88883
    dcd 0x00800080
    ENTRY_END
    ENDP
    end
