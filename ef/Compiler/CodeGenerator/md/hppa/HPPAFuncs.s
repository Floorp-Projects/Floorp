/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
	.SPACE $PRIVATE$
	.SUBSPA $DATA$

	.align 4
	.EXPORT HPPAremI,DATA
HPPAremI:
	.long _remI

	.EXPORT HPPAremU,DATA
HPPAremU:
	.long _remU

	.EXPORT HPPAdivI,DATA
HPPAdivI:
	.long _divI

	.EXPORT HPPAdivU,DATA
HPPAdivU:
	.long _divU

	.EXPORT HPPASpecialCodeBegin,DATA
HPPASpecialCodeBegin:
	.long _begin

	.EXPORT HPPASpecialCodeEnd,DATA
HPPASpecialCodeEnd:
	.long _end

	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
_begin:

_remI:
	addit,= 0,%arg1,%r0
	add,>= %r0,%arg0,%ret1
	sub %r0,%ret1,%ret1
	sub %r0,%arg1,%r1
	ds %r0,%r1,%r0
	copy %r0,%r1
	add %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	movb,>=,n %r1,%ret1,L5
	add,< %arg1,%r0,%r0
	add,tr %r1,%arg1,%ret1
	sub %r1,%arg1,%ret1
L5:	add,>= %arg0,%r0,%r0
	sub %r0,%ret1,%ret1
	bv %r0(%r31)
	nop

	.align 4
_remU:
	comibf,<,n 0x0,%arg1,_remU_special_case
	sub %r0,%arg1,%ret1
	ds %r0,%ret1,%r0
	add %arg0,%arg0,%r1
	ds %r0,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	addc %r1,%r1,%r1
	ds %ret1,%arg1,%ret1
	comiclr,<= 0,%ret1,%r0
	add %ret1,%arg1,%ret1
	bv,n %r0(%r31)
	nop
_remU_special_case:
	addit,= 0,%arg1,%r0
	sub,>>= %arg0,%arg1,%ret1
	copy %arg0,%ret1
	bv,n %r0(%r31)
	nop

	.align 4
_divI:
	comibf,<<,n 0xf,%arg1,_divI_small_divisor
	add,>= %r0,%arg0,%ret1
_divI_normal:
	sub %r0,%ret1,%ret1
	sub %r0,%arg1,%r1
	ds %r0,%r1,%r0
	add %ret1,%ret1,%ret1
	ds %r0,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	xor,>= %arg0,%arg1,%r0
	sub %r0,%ret1,%ret1
	bv,n %r0(%r31)
	nop

_divI_small_divisor:
	blr,n %arg1,%r0
	nop
	addit,= 0,%arg1,%r0
	nop
	bv %r0(%r31)
	copy %arg0,%ret1
	b,n _divI_2
	nop
	b,n _divI_3
	nop
	b,n _divI_4
	nop
	b,n _divI_5
	nop
	b,n _divI_6
	nop
	b,n _divI_7
	nop
	b,n _divI_8
	nop
	b,n _divI_9
	nop
	b,n _divI_10
	nop
	b _divI_normal
	add,>= %r0,%arg0,%ret1
	b,n _divI_12
	nop
	b _divI_normal
	add,>= %r0,%arg0,%ret1
	b,n _divI_14
	nop
	b,n _divI_15
	nop

	.align 4
_divU:
	comibf,< 0xf,%arg1,_divU_special_divisor
	sub %r0,%arg1,%r1
	ds %r0,%r1,%r0
_divU_normal:
	add %arg0,%arg0,%ret1
	ds %r0,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	addc %ret1,%ret1,%ret1
	ds %r1,%arg1,%r1
	bv %r0(%r31)
	addc %ret1,%ret1,%ret1
_divU_special_divisor:
	comibf,<= 0,%arg1,_divU_big_divisor
	nop
	blr %arg1,%r0
	nop
_divU_small_divisor:
	addit,= 0,%arg1,%r0
	nop
	bv %r0(%r31)
	copy %arg0,%ret1
	bv %r0(%r31)
	extru %arg0,30,31,%ret1
	b,n _divU_3
	nop
	bv %r0(%r31)
	extru %arg0,29,30,%ret1
	b,n _divU_5
	nop
	b,n _divU_6
	nop
	b,n _divU_7
	nop
	bv %r0(%r31)
	extru %arg0,28,29,%ret1
	b,n _divU_9
	nop
	b,n _divU_10
	nop
	b _divU_normal
	ds %r0,%r1,%r0
	b,n _divU_12
	nop
	b _divU_normal
	ds %r0,%r1,%r0
	b,n _divU_14
	nop
	b,n _divU_15
	nop
_divU_big_divisor:
	sub %arg0,%arg1,%r0
	bv %r0(%r31)
	addc %r0,%r0,%ret1

_divI_2:
	comclr,>= %arg0,%r0,%r0
	addi 1,%arg0,%arg0
	bv %r0(%r31)
	extrs %arg0,30,31,%ret1

_divI_4:
	comclr,>= %arg0,%r0,%r0
	addi 3,%arg0,%arg0
	bv %r0(%r31)
	extrs %arg0,29,30,%ret1

_divI_8:
	comclr,>= %arg0,%r0,%r0
	addi 7,%arg0,%arg0
	bv %r0(%r31)
	extrs %arg0,28,29,%ret1

_divI_3:
	comb,<,n %arg0,%r0,_divI_neg3
	addi 1,%arg0,%arg0
	extru %arg0,1,2,%ret1
	sh2add %arg0,%arg0,%arg0
	b _divI_pos
	addc %ret1,%r0,%ret1
_divI_neg3:
	subi 1,%arg0,%arg0
	extru %arg0,1,2,%ret1
	sh2add %arg0,%arg0,%arg0
	b _divI_neg
	addc %ret1,%r0,%ret1
_divU_3:
	addi 1,%arg0,%arg0
	addc %r0,%r0,%ret1
	shd %ret1,%arg0,30,%arg1
	sh2add %arg0,%arg0,%arg0
	b _divI_pos
	addc %ret1,%arg1,%ret1

_divI_5:
	comb,<,n %arg0,%r0,_divI_neg5
	addi 3,%arg0,%arg1
	sh1add %arg0,%arg1,%arg0
	b _divI_pos
	addc %r0,%r0,%ret1
_divI_neg5:
	sub %r0,%arg0,%arg0
	addi 1,%arg0,%arg0
	shd %r0,%arg0,31,%ret1
	sh1add %arg0,%arg0,%arg0
	b _divI_neg
	addc %ret1,%r0,%ret1
_divU_5:
	addi 1,%arg0,%arg0
	addc %r0,%r0,%ret1
	shd %ret1,%arg0,31,%arg1
	sh1add %arg0,%arg0,%arg0
	b _divI_pos
	addc %arg1,%ret1,%ret1

_divI_6:
	comb,<,n %arg0,%r0,_divI_neg6
	extru %arg0,30,31,%arg0
	addi 5,%arg0,%arg1
	sh2add %arg0,%arg1,%arg0
	b _divI_pos
	addc %r0,%r0,%ret1
_divI_neg6:
	subi 2,%arg0,%arg0
	extru %arg0,30,31,%arg0
	shd %r0,%arg0,30,%ret1
	sh2add %arg0,%arg0,%arg0
	b _divI_neg
	addc %ret1,%r0,%ret1
_divU_6:
	extru %arg0,30,31,%arg0
	addi 1,%arg0,%arg0
	shd %r0,%arg0,30,%ret1
	sh2add %arg0,%arg0,%arg0
	b _divI_pos
	addc %ret1,%r0,%ret1

_divI_10:
	comb,< %arg0,%r0,_divI_neg10
	copy %r0,%ret1
	extru %arg0,30,31,%arg0
	addibf 1,%arg0,_divI_pos
	sh1add %arg0,%arg0,%arg0
_divI_neg10:
	subi 2,%arg0,%arg0
	extru %arg0,30,31,%arg0
	sh1add %arg0,%arg0,%arg0

_divI_neg:
	shd %ret1,%arg0,28,%arg1
	shd %arg0,%r0,28,%r1
	add %arg0,%r1,%arg0
	addc %ret1,%arg1,%ret1
	shd %ret1,%arg0,24,%arg1
	shd %arg0,%r0,24,%r1
	add %arg0,%r1,%arg0
	addc %ret1,%arg1,%ret1
	shd %ret1,%arg0,16,%arg1
	shd %arg0,%r0,16,%r1
	add %arg0,%r1,%arg0
	addc %ret1,%arg1,%ret1
	bv %r0(%r31)
	sub %r0,%ret1,%ret1

_divU_10:
	extru %arg0,30,31,%arg0
	addi 3,%arg0,%arg1
	sh1add %arg0,%arg1,%arg0
	addc %r0,%r0,%ret1

_divI_pos:
	shd %ret1,%arg0,28,%arg1
_divI_pos_for_15:
	shd %arg0,%r0,28,%r1
	add %arg0,%r1,%arg0
	addc %ret1,%arg1,%ret1
	shd %ret1,%arg0,24,%arg1
	shd %arg0,%r0,24,%r1
	add %arg0,%r1,%arg0
	addc %ret1,%arg1,%ret1
	shd %ret1,%arg0,16,%arg1
	shd %arg0,%r0,16,%r1
	add %arg0,%r1,%arg0
	bv %r0(%r31)
	addc %ret1,%arg1,%ret1

_divI_12:
	comb,< %arg0,%r0,_divI_neg12
	copy %r0,%ret1
	extru %arg0,29,30,%arg0
	addibf 1,%arg0,_divI_pos
	sh2add %arg0,%arg0,%arg0
_divI_neg12:
	subi 4,%arg0,%arg0
	extru %arg0,29,30,%arg0
	b _divI_neg
	sh2add %arg0,%arg0,%arg0
_divU_12:
	extru %arg0,29,30,%arg0
	addi 5,%arg0,%arg1
	sh2add %arg0,%arg1,%arg0
	b _divI_pos
	addc %r0,%r0,%ret1

_divI_15:
	comb,< %arg0,%r0,_divI_neg15
	copy %r0,%ret1
	addibf 1,%arg0,_divI_pos_for_15
	shd %ret1,%arg0,28,%arg1
_divI_neg15:
	b _divI_neg
	subi 1,%arg0,%arg0
_divU_15:
	addi 1,%arg0,%arg0
	b _divI_pos
	addc %r0,%r0,%ret1

_divI_7:
	comb,<,n %arg0,%r0,_divI_neg7
_divI_7a:
	addi 1,%arg0,%arg0
	shd %r0,%arg0,29,%ret1
	sh3add %arg0,%arg0,%arg0
	addc %ret1,%r0,%ret1
_divI_pos7:
	shd %ret1,%arg0,26,%arg1
	shd %arg0,%r0,26,%r1
	add %arg0,%r1,%arg0
	addc %ret1,%arg1,%ret1
	shd %ret1,%arg0,20,%arg1
	shd %arg0,%r0,20,%r1
	add %arg0,%r1,%arg0
	addc %ret1,%arg1,%arg1
	copy %r0,%ret1
	shd,= %arg1,%arg0,24,%arg1
L1: addbf %arg1,%ret1,L2
	extru %arg0,31,24,%arg0
	bv,n %r0(%r31)
L2:	addbf %arg1,%arg0,L1
	extru,= %arg0,7,8,%arg1

_divI_neg7:
	subi 1,%arg0,%arg0
_divI_neg7a:
	shd %r0,%arg0,29,%ret1
	sh3add %arg0,%arg0,%arg0
	addc %ret1,%r0,%ret1
_divI_neg7_shift:
	shd %ret1,%arg0,26,%arg1
	shd %arg0,%r0,26,%r1
	add %arg0,%r1,%arg0
	addc %ret1,%arg1,%ret1
	shd %ret1,%arg0,20,%arg1
	shd %arg0,%r0,20,%r1
	add %arg0,%r1,%arg0
	addc %ret1,%arg1,%arg1
	copy %r0,%ret1
	shd,= %arg1,%arg0,24,%arg1
L3:	addbf %arg1,%ret1,L4
	extru %arg0,31,24,%arg0
	bv %r0(%r31)
	sub %r0,%ret1,%ret1
L4:	addbf %arg1,%arg0,L3
	extru,= %arg0,7,8,%arg1

_divU_7:
	addi 1,%arg0,%arg0
	addc %r0,%r0,%ret1
	shd %ret1,%arg0,29,%arg1
	sh3add %arg0,%arg0,%arg0
	b _divI_pos7
	addc %arg1,%ret1,%ret1

_divI_9:
	comb,<,n %arg0,%r0,_divI_neg9
	addi 1,%arg0,%arg0
	shd %r0,%arg0,29,%arg1
	shd %arg0,%r0,29,%r1
	sub %r1,%arg0,%arg0
	b _divI_pos7
	subb %arg1,%r0,%ret1
_divI_neg9:
	subi 1,%arg0,%arg0
	shd %r0,%arg0,29,%arg1
	shd %arg0,%r0,29,%r1
	sub %r1,%arg0,%arg0
	b _divI_neg7_shift
	subb %arg1,%r0,%ret1
_divU_9:
	addi 1,%arg0,%arg0
	addc %r0,%r0,%ret1
	shd %ret1,%arg0,29,%arg1
	shd %arg0,%r0,29,%r1
	sub %r1,%arg0,%arg0
	b _divI_pos7
	subb %arg1,%ret1,%ret1

_divI_14:
	comb,<,n %arg0,%r0,_divI_neg14
_divU_14:
	b _divI_7a
	extru %arg0,30,31,%arg0
_divI_neg14:
	subi 2,%arg0,%arg0
	b _divI_neg7a
	extru %arg0,30,31,%arg0

_end:
