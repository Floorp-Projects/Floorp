
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
        

    FUNC_HEADER fbCompositeSrcAdd_8000x8000arm
    stmfd	sp!, {r4, r5, r6, r7, r8, fp, lr}
    add	fp, sp, #24	; 0x18
    ldr	r2, [r1, #120]
    ldr	r4, [r3, #120]
    ldrsh	r0, [fp, #8]
    ldrsh	ip, [fp, #24]
    mov	r8, r2, lsl #2
    ldr	lr, [r1, #112]
    ldr	r2, [r3, #112]
    mov	r7, r4, lsl #2
    ldrsh	r3, [fp, #4]
    ldrh	r4, [fp, #32]
    mla	r0, r8, r0, lr
    mla	ip, r7, ip, r2
    ldrsh	r2, [fp, #20]
    add	r5, r0, r3
    sub	r3, r4, #1	; 0x1
    add	ip, ip, r2
    ldr	r2, fbCompositeSrcAdd_8000x8000arm0 ; 0xffff
    uxth	r4, r3
    cmp	r4, r2
    ldrh	r6, [fp, #28]
    beq	fbCompositeSrcAdd_8000x8000arm1
fbCompositeSrcAdd_8000x8000arm7
    cmp	r6, #0	; 0x0
    movne	lr, ip
    movne	r0, r5
    movne	r1, r6
    beq	fbCompositeSrcAdd_8000x8000arm2
fbCompositeSrcAdd_8000x8000arm5
    tst	lr, #3	; 0x3
    sub	r3, r1, #1	; 0x1
    bne	fbCompositeSrcAdd_8000x8000arm3
    tst	r0, #3	; 0x3
    beq	fbCompositeSrcAdd_8000x8000arm4
fbCompositeSrcAdd_8000x8000arm3
    uxth	r1, r3
    cmp	r1, #0	; 0x0
    ldrb	r3, [lr]
    ldrb	r2, [r0], #1
    uqadd8	r3, r2, r3
    strb	r3, [lr], #1
    bne	fbCompositeSrcAdd_8000x8000arm5
fbCompositeSrcAdd_8000x8000arm2
    add	ip, ip, r7
    add	r5, r5, r8
fbCompositeSrcAdd_8000x8000arm12
    sub	r3, r4, #1	; 0x1
    ldr	r2, fbCompositeSrcAdd_8000x8000arm0 ; 0xffff
    uxth	r4, r3
    cmp	r4, r2
    bne	fbCompositeSrcAdd_8000x8000arm7
fbCompositeSrcAdd_8000x8000arm1
    ldmfd	sp!, {r4, r5, r6, r7, r8, fp, pc}
fbCompositeSrcAdd_8000x8000arm4
    cmp	r1, #3	; 0x3
    bls	fbCompositeSrcAdd_8000x8000arm8
fbCompositeSrcAdd_8000x8000arm9
    sub	r3, r1, #4	; 0x4
    ldr	r2, [r0], #4
    uxth	r1, r3
    cmp	r1, #3	; 0x3
    ldr	r3, [lr]
    uqadd8	r2, r2, r3
    str	r2, [lr], #4
    bhi	fbCompositeSrcAdd_8000x8000arm9
fbCompositeSrcAdd_8000x8000arm8
    cmp	r1, #0	; 0x0
    beq	fbCompositeSrcAdd_8000x8000arm2
fbCompositeSrcAdd_8000x8000arm11
    sub	r3, r1, #1	; 0x1
    ldrb	r2, [lr]
    uxth	r1, r3
    cmp	r1, #0	; 0x0
    ldrb	r3, [r0], #1
    uqadd8	r2, r3, r2
    strb	r2, [lr], #1
    bne	fbCompositeSrcAdd_8000x8000arm11
    add	ip, ip, r7
    add	r5, r5, r8
    b	fbCompositeSrcAdd_8000x8000arm12
fbCompositeSrcAdd_8000x8000arm0
    dcd 0x0000ffff
    ENTRY_END
    ENDP

    FUNC_HEADER fbCompositeSrc_8888x8888arm
    stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    add	fp, sp, #32	; 0x20
    sub	sp, sp, #28	; 0x1c
    ldrh	r0, [fp, #32]
    ldr	r7, [r1, #120]
    ldr	r8, [r3, #120]
    cmp	r0, #0	; 0x0
    ldrsh	r6, [fp, #4]
    ldrsh	r5, [fp, #8]
    ldrsh	r4, [fp, #20]
    ldrsh	r2, [fp, #24]
    ldrh	lr, [fp, #28]
    ldr	ip, [r3, #112]
    ldr	r1, [r1, #112]
    beq	fbCompositeSrc_8888x8888arm0
    mla	r3, r8, r2, r4
    mla	r2, r7, r5, r6
    mov	r3, r3, lsl #2
    add	r9, r3, ip
    mov	r2, r2, lsl #2
    add	sl, r2, r1
    mov	r8, r8, lsl #2
    mov	r7, r7, lsl #2
    str	r8, [fp, #-48]
    str	r7, [fp, #-44]
fbCompositeSrc_8888x8888arm5
    ldr	r3, [fp, #-48]
    ldr	ip, [fp, #-44]
    add	r3, r9, r3
    add	ip, sl, ip
    str	r3, [fp, #-56]
    str	ip, [fp, #-40]
    ldr	r1, fbCompositeSrc_8888x8888arm1 ; 0x800080
    ldr	r2, fbCompositeSrc_8888x8888arm2 ; 0xff00ff00
    mov	r3, #255	; 0xff
    mov	ip, lr
    cmp	ip, #0	; 0x0
    beq	fbCompositeSrc_8888x8888arm3
fbCompositeSrc_8888x8888arm4
    ldr	r5, [sl], #4
    ldr	r4, [r9]
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
    str	r5, [r9], #4
    subs	ip, ip, #1	; 0x1
    bne	fbCompositeSrc_8888x8888arm4
fbCompositeSrc_8888x8888arm3
    sub	r0, r0, #1	; 0x1
    ldr	r9, [fp, #-56]
    uxth	r0, r0
    cmp	r0, #0	; 0x0
    ldr	sl, [fp, #-40]
    bne	fbCompositeSrc_8888x8888arm5
fbCompositeSrc_8888x8888arm0
    sub	sp, fp, #32	; 0x20
    ldmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
fbCompositeSrc_8888x8888arm1
    dcd 0x00800080
fbCompositeSrc_8888x8888arm2
    dcd 0xff00ff00
    ENTRY_END
    ENDP

    FUNC_HEADER fbCompositeSrc_8888x8x8888arm
    stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    add	fp, sp, #32	; 0x20
    sub	sp, sp, #36	; 0x24
    mov	r0, r2
    ldr	r2, [r2]
    ldr	r5, [r1, #120]
    cmp	r2, #4	; 0x4
    ldrsh	r2, [fp, #4]
    ldr	r6, [r3, #96]
    ldrsh	r9, [fp, #20]
    str	r2, [fp, #-52]
    ldrsh	r2, [fp, #8]
    str	r2, [fp, #-56]
    ldrsh	r2, [fp, #24]
    str	r2, [fp, #-60]
    ldrh	r2, [fp, #28]
    str	r2, [fp, #-64]
    ldrh	r2, [fp, #32]
    str	r2, [fp, #-68]
    moveq	r2, #2	; 0x2
    ldr	r7, [r1, #112]
    ldr	r8, [r3, #112]
    ldr	r4, [r3, #120]
    ldreq	r1, [r0, #100]
    beq	fbCompositeSrc_8888x8x8888arm0
    ldr	lr, [r0, #96]
    ldr	r0, [r0, #112]
    mov	r3, lr, lsr #24
    sub	r3, r3, #1	; 0x1
    cmp	r3, #31	; 0x1f
    addls	pc, pc, r3, lsl #2
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm2
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm9
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm17
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm25
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm1
    b	fbCompositeSrc_8888x8x8888arm33
fbCompositeSrc_8888x8x8888arm33
    ldr	r1, [r0]
fbCompositeSrc_8888x8x8888arm40
    tst	lr, #61440	; 0xf000
    movne	r3, lr, lsr #16
    moveq	r3, lr, lsr #16
    andne	r2, r3, #255	; 0xff
    orreq	r1, r1, #-16777216	; 0xff000000
    andeq	r2, r3, #255	; 0xff
fbCompositeSrc_8888x8x8888arm0
    mov	r3, r6, lsr #16
    and	r3, r3, #255	; 0xff
    cmp	r2, r3
    beq	fbCompositeSrc_8888x8x8888arm34
    and	r3, r1, #65280	; 0xff00
    and	r2, r1, #-16777216	; 0xff000000
    and	r0, r1, #16711680	; 0xff0000
    orr	r3, r3, r2
    and	r1, r1, #255	; 0xff
    orr	r3, r3, r0, lsr #16
    orr	r1, r3, r1, lsl #16
fbCompositeSrc_8888x8x8888arm34
    ldr	r3, [fp, #-68]
    mov	ip, r1, lsr #24
    cmp	r3, #0	; 0x0
    beq	fbCompositeSrc_8888x8x8888arm1
    ldr	r0, [fp, #-60]
    ldr	r1, [fp, #-56]
    mla	r3, r4, r0, r9
    ldr	r0, [fp, #-52]
    mov	r4, r4, lsl #2
    mla	r2, r5, r1, r0
    mov	r3, r3, lsl #2
    add	sl, r3, r8
    mov	r2, r2, lsl #2
    add	lr, r2, r7
    mov	r5, r5, lsl #2
    str	r4, [fp, #-48]
    str	r5, [fp, #-44]
fbCompositeSrc_8888x8x8888arm39
    ldr	r2, [fp, #-44]
    ldr	r1, [fp, #-48]
    add	r2, lr, r2
    add	r0, sl, r1
    str	r2, [fp, #-40]
    mov	r3, #255	; 0xff
    ldr	r2, fbCompositeSrc_8888x8x8888arm36 ; 0x800080
    ldr	r1, [fp, #-64]
    cmp	r1, #0	; 0x0
    beq	fbCompositeSrc_8888x8x8888arm37
fbCompositeSrc_8888x8x8888arm38
    ldr	r5, [lr], #4
    ldr	r4, [sl]
    uxtb16	r6, r5
    uxtb16	r7, r5, ror #8
    mla	r6, r6, ip, r2
    mla	r7, r7, ip, r2
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
    str	r5, [sl], #4
    subs	r1, r1, #1	; 0x1
    bne	fbCompositeSrc_8888x8x8888arm38
fbCompositeSrc_8888x8x8888arm37
    ldr	r2, [fp, #-68]
    mov	sl, r0
    sub	r1, r2, #1	; 0x1
    ldr	lr, [fp, #-40]
    uxth	r1, r1
    cmp	r1, #0	; 0x0
    str	r1, [fp, #-68]
    bne	fbCompositeSrc_8888x8x8888arm39
fbCompositeSrc_8888x8x8888arm1
    sub	sp, fp, #32	; 0x20
    ldmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
fbCompositeSrc_8888x8x8888arm2
    ldr	r3, [r0]
    ands	r1, r3, #1	; 0x1
    movne	r1, #-16777216	; 0xff000000
    b	fbCompositeSrc_8888x8x8888arm40
fbCompositeSrc_8888x8x8888arm9
    ldrb	r3, [r0]
    mov	r1, r3, lsl #24
    b	fbCompositeSrc_8888x8x8888arm40
fbCompositeSrc_8888x8x8888arm17
    ldrh	r2, [r0]
    mov	sl, r2, lsl #3
    mov	r0, r2, lsl #27
    mov	r1, r2, lsl #5
    and	r3, sl, #248	; 0xf8
    orr	r3, r3, r0, lsr #29
    and	r1, r1, #64512	; 0xfc00
    mov	r0, r2, lsr #1
    orr	r3, r3, r1
    and	r0, r0, #768	; 0x300
    mov	r2, r2, lsl #8
    orr	r3, r3, r0
    and	r2, r2, #16252928	; 0xf80000
    orr	r3, r3, r2
    and	r0, sl, #458752	; 0x70000
    orr	r1, r3, r0
    b	fbCompositeSrc_8888x8x8888arm40
fbCompositeSrc_8888x8x8888arm25
    tst	r0, #1	; 0x1
    ldrneh	r2, [r0, #1]
    ldreqb	r2, [r0, #2]
    ldrneb	r3, [r0]
    ldreqh	r3, [r0]
    orrne	r1, r3, r2, lsl #8
    orreq	r1, r3, r2, lsl #16
    b	fbCompositeSrc_8888x8x8888arm40
fbCompositeSrc_8888x8x8888arm36
    dcd 0x00800080
    ENTRY_END
    ENDP

    FUNC_HEADER fbCompositeSolidMask_nx8x8888arm
    stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    add	fp, sp, #32	; 0x20
    sub	sp, sp, #36	; 0x24
    mov	r6, r2
    ldr	r2, [r1]
    mov	r5, r3
    cmp	r2, #4	; 0x4
    ldrsh	r2, [fp, #12]
    ldrsh	r3, [fp, #16]
    ldrh	ip, [fp, #28]
    str	r2, [fp, #-52]
    ldrh	r2, [fp, #32]
    str	r3, [fp, #-56]
    str	ip, [fp, #-60]
    str	r2, [fp, #-64]
    ldrsh	r7, [fp, #20]
    ldrsh	r8, [fp, #24]
    ldreq	r0, [r1, #100]
    moveq	r2, #2	; 0x2
    beq	fbCompositeSolidMask_nx8x8888arm0
    ldr	lr, [r1, #96]
    ldr	r0, [r1, #112]
    mov	r3, lr, lsr #24
    sub	r3, r3, #1	; 0x1
    cmp	r3, #31	; 0x1f
    addls	pc, pc, r3, lsl #2
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm2
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm9
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm17
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm25
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm1
    b	fbCompositeSolidMask_nx8x8888arm33
fbCompositeSolidMask_nx8x8888arm33
    ldr	r0, [r0]
fbCompositeSolidMask_nx8x8888arm41
    tst	lr, #61440	; 0xf000
    movne	r3, lr, lsr #16
    moveq	r3, lr, lsr #16
    andne	r2, r3, #255	; 0xff
    orreq	r0, r0, #-16777216	; 0xff000000
    andeq	r2, r3, #255	; 0xff
fbCompositeSolidMask_nx8x8888arm0
    ldrb	r3, [r5, #98]
    cmp	r2, r3
    beq	fbCompositeSolidMask_nx8x8888arm34
    and	r3, r0, #65280	; 0xff00
    and	r2, r0, #-16777216	; 0xff000000
    and	ip, r0, #16711680	; 0xff0000
    orr	r3, r3, r2
    and	r1, r0, #255	; 0xff
    orr	r3, r3, ip, lsr #16
    orr	r0, r3, r1, lsl #16
fbCompositeSolidMask_nx8x8888arm34
    cmp	r0, #0	; 0x0
    beq	fbCompositeSolidMask_nx8x8888arm1
    mov	r3, r0, lsr #8
    bic	sl, r3, #-16777216	; 0xff000000
    ldr	r3, [fp, #-64]
    bic	r9, r0, #-16777216	; 0xff000000
    cmp	r3, #0	; 0x0
    bic	sl, sl, #65280	; 0xff00
    bic	r9, r9, #65280	; 0xff00
    ldr	r1, [r5, #120]
    ldr	ip, [r6, #120]
    ldr	lr, [r5, #112]
    ldr	r2, [r6, #112]
    beq	fbCompositeSolidMask_nx8x8888arm1
    mla	r3, r1, r8, r7
    mov	r1, r1, lsl #2
    str	r1, [fp, #-44]
    ldr	r1, [fp, #-56]
    mov	ip, ip, lsl #2
    mov	r3, r3, lsl #2
    mla	r2, ip, r1, r2
    add	lr, r3, lr
    ldr	r3, [fp, #-52]
    str	ip, [fp, #-48]
    add	ip, r2, r3
fbCompositeSolidMask_nx8x8888arm40
    ldr	r3, [fp, #-48]
    ldr	r2, [fp, #-44]
    add	r3, ip, r3
    add	r1, r2, lr
    str	r3, [fp, #-40]
    ldr	r2, [fp, #-60]
    ldr	r3, fbCompositeSolidMask_nx8x8888arm37 ; 0x800080
    cmp	r2, #0	; 0x0
    beq	fbCompositeSolidMask_nx8x8888arm38
fbCompositeSolidMask_nx8x8888arm39
    ldrb	r5, [ip], #1
    ldr	r4, [lr]
    mla	r6, r9, r5, r3
    mla	r7, sl, r5, r3
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
    str	r5, [lr], #4
    subs	r2, r2, #1	; 0x1
    bne	fbCompositeSolidMask_nx8x8888arm39
fbCompositeSolidMask_nx8x8888arm38
    ldr	r3, [fp, #-64]
    mov	lr, r1
    sub	r2, r3, #1	; 0x1
    ldr	ip, [fp, #-40]
    uxth	r2, r2
    cmp	r2, #0	; 0x0
    str	r2, [fp, #-64]
    bne	fbCompositeSolidMask_nx8x8888arm40
fbCompositeSolidMask_nx8x8888arm1
    sub	sp, fp, #32	; 0x20
    ldmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
fbCompositeSolidMask_nx8x8888arm2
    ldr	r3, [r0]
    ands	r0, r3, #1	; 0x1
    movne	r0, #-16777216	; 0xff000000
    b	fbCompositeSolidMask_nx8x8888arm41
fbCompositeSolidMask_nx8x8888arm9
    ldrb	r3, [r0]
    mov	r0, r3, lsl #24
    b	fbCompositeSolidMask_nx8x8888arm41
fbCompositeSolidMask_nx8x8888arm17
    ldrh	r2, [r0]
    mov	ip, r2, lsl #3
    mov	r4, r2, lsl #27
    and	r3, ip, #248	; 0xf8
    mov	r1, r2, lsl #5
    orr	r3, r3, r4, lsr #29
    and	r1, r1, #64512	; 0xfc00
    mov	r4, r2, lsr #1
    orr	r3, r3, r1
    and	r4, r4, #768	; 0x300
    mov	r2, r2, lsl #8
    orr	r3, r3, r4
    and	r2, r2, #16252928	; 0xf80000
    orr	r3, r3, r2
    and	ip, ip, #458752	; 0x70000
    orr	r0, r3, ip
    b	fbCompositeSolidMask_nx8x8888arm41
fbCompositeSolidMask_nx8x8888arm25
    tst	r0, #1	; 0x1
    ldrneh	r2, [r0, #1]
    ldreqb	r2, [r0, #2]
    ldrneb	r3, [r0]
    ldreqh	r3, [r0]
    orrne	r0, r3, r2, lsl #8
    orreq	r0, r3, r2, lsl #16
    b	fbCompositeSolidMask_nx8x8888arm41
fbCompositeSolidMask_nx8x8888arm37
    dcd 0x00800080
    ENTRY_END
    ENDP
    end
