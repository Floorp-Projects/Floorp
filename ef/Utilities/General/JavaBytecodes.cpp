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

#include "JavaBytecodes.h"
#include "MemoryAccess.h"
#include "ConstantPool.h"
#include "Attributes.h"
#include "DebugUtils.h"

// Kinds and sizes of normal bytecodes
const BytecodeControlInfo normalBytecodeControlInfos[256] =
{
	{BytecodeControlInfo::bckNormal, 1},		// 00				nop
	{BytecodeControlInfo::bckNormal, 1},		// 01				aconst_null
	{BytecodeControlInfo::bckNormal, 1},		// 02				iconst_m1
	{BytecodeControlInfo::bckNormal, 1},		// 03				iconst_0
	{BytecodeControlInfo::bckNormal, 1},		// 04				iconst_1
	{BytecodeControlInfo::bckNormal, 1},		// 05				iconst_2
	{BytecodeControlInfo::bckNormal, 1},		// 06				iconst_3
	{BytecodeControlInfo::bckNormal, 1},		// 07				iconst_4
	{BytecodeControlInfo::bckNormal, 1},		// 08				iconst_5
	{BytecodeControlInfo::bckNormal, 1},		// 09				lconst_0
	{BytecodeControlInfo::bckNormal, 1},		// 0A				lconst_1
	{BytecodeControlInfo::bckNormal, 1},		// 0B				fconst_0
	{BytecodeControlInfo::bckNormal, 1},		// 0C				fconst_1
	{BytecodeControlInfo::bckNormal, 1},		// 0D				fconst_2
	{BytecodeControlInfo::bckNormal, 1},		// 0E				dconst_0
	{BytecodeControlInfo::bckNormal, 1},		// 0F				dconst_1
	{BytecodeControlInfo::bckNormal, 2},		// 10 cc			bipush c
	{BytecodeControlInfo::bckNormal, 3},		// 11 cccc			sipush c
	{BytecodeControlInfo::bckNormal, 2},		// 12 ii			ldc i
	{BytecodeControlInfo::bckNormal, 3},		// 13 iiii			ldc_w i
	{BytecodeControlInfo::bckNormal, 3},		// 14 iiii			ldc2_w i
	{BytecodeControlInfo::bckNormal, 2},		// 15 vv			iload v
	{BytecodeControlInfo::bckNormal, 2},		// 16 vv			lload v
	{BytecodeControlInfo::bckNormal, 2},		// 17 vv			fload v
	{BytecodeControlInfo::bckNormal, 2},		// 18 vv			dload v
	{BytecodeControlInfo::bckNormal, 2},		// 19 vv			aload v
	{BytecodeControlInfo::bckNormal, 1},		// 1A				iload_0
	{BytecodeControlInfo::bckNormal, 1},		// 1B				iload_1
	{BytecodeControlInfo::bckNormal, 1},		// 1C				iload_2
	{BytecodeControlInfo::bckNormal, 1},		// 1D				iload_3
	{BytecodeControlInfo::bckNormal, 1},		// 1E				lload_0
	{BytecodeControlInfo::bckNormal, 1},		// 1F				lload_1
	{BytecodeControlInfo::bckNormal, 1},		// 20				lload_2
	{BytecodeControlInfo::bckNormal, 1},		// 21				lload_3
	{BytecodeControlInfo::bckNormal, 1},		// 22				fload_0
	{BytecodeControlInfo::bckNormal, 1},		// 23				fload_1
	{BytecodeControlInfo::bckNormal, 1},		// 24				fload_2
	{BytecodeControlInfo::bckNormal, 1},		// 25				fload_3
	{BytecodeControlInfo::bckNormal, 1},		// 26				dload_0
	{BytecodeControlInfo::bckNormal, 1},		// 27				dload_1
	{BytecodeControlInfo::bckNormal, 1},		// 28				dload_2
	{BytecodeControlInfo::bckNormal, 1},		// 29				dload_3
	{BytecodeControlInfo::bckNormal, 1},		// 2A				aload_0
	{BytecodeControlInfo::bckNormal, 1},		// 2B				aload_1
	{BytecodeControlInfo::bckNormal, 1},		// 2C				aload_2
	{BytecodeControlInfo::bckNormal, 1},		// 2D				aload_3
	{BytecodeControlInfo::bckExc, 1},			// 2E				iaload
	{BytecodeControlInfo::bckExc, 1},			// 2F				laload
	{BytecodeControlInfo::bckExc, 1},			// 30				faload
	{BytecodeControlInfo::bckExc, 1},			// 31				daload
	{BytecodeControlInfo::bckExc, 1},			// 32				aaload
	{BytecodeControlInfo::bckExc, 1},			// 33				baload
	{BytecodeControlInfo::bckExc, 1},			// 34				caload
	{BytecodeControlInfo::bckExc, 1},			// 35				saload
	{BytecodeControlInfo::bckNormal, 2},		// 36 vv			istore v
	{BytecodeControlInfo::bckNormal, 2},		// 37 vv			lstore v
	{BytecodeControlInfo::bckNormal, 2},		// 38 vv			fstore v
	{BytecodeControlInfo::bckNormal, 2},		// 39 vv			dstore v
	{BytecodeControlInfo::bckNormal, 2},		// 3A vv			astore v
	{BytecodeControlInfo::bckNormal, 1},		// 3B				istore_0
	{BytecodeControlInfo::bckNormal, 1},		// 3C				istore_1
	{BytecodeControlInfo::bckNormal, 1},		// 3D				istore_2
	{BytecodeControlInfo::bckNormal, 1},		// 3E				istore_3
	{BytecodeControlInfo::bckNormal, 1},		// 3F				lstore_0
	{BytecodeControlInfo::bckNormal, 1},		// 40				lstore_1
	{BytecodeControlInfo::bckNormal, 1},		// 41				lstore_2
	{BytecodeControlInfo::bckNormal, 1},		// 42				lstore_3
	{BytecodeControlInfo::bckNormal, 1},		// 43				fstore_0
	{BytecodeControlInfo::bckNormal, 1},		// 44				fstore_1
	{BytecodeControlInfo::bckNormal, 1},		// 45				fstore_2
	{BytecodeControlInfo::bckNormal, 1},		// 46				fstore_3
	{BytecodeControlInfo::bckNormal, 1},		// 47				dstore_0
	{BytecodeControlInfo::bckNormal, 1},		// 48				dstore_1
	{BytecodeControlInfo::bckNormal, 1},		// 49				dstore_2
	{BytecodeControlInfo::bckNormal, 1},		// 4A				dstore_3
	{BytecodeControlInfo::bckNormal, 1},		// 4B				astore_0
	{BytecodeControlInfo::bckNormal, 1},		// 4C				astore_1
	{BytecodeControlInfo::bckNormal, 1},		// 4D				astore_2
	{BytecodeControlInfo::bckNormal, 1},		// 4E				astore_3
	{BytecodeControlInfo::bckExc, 1},			// 4F				iastore
	{BytecodeControlInfo::bckExc, 1},			// 50				lastore
	{BytecodeControlInfo::bckExc, 1},			// 51				fastore
	{BytecodeControlInfo::bckExc, 1},			// 52				dastore
	{BytecodeControlInfo::bckExc, 1},			// 53				aastore
	{BytecodeControlInfo::bckExc, 1},			// 54				bastore
	{BytecodeControlInfo::bckExc, 1},			// 55				castore
	{BytecodeControlInfo::bckExc, 1},			// 56				sastore
	{BytecodeControlInfo::bckNormal, 1},		// 57				pop
	{BytecodeControlInfo::bckNormal, 1},		// 58				pop2
	{BytecodeControlInfo::bckNormal, 1},		// 59				dup
	{BytecodeControlInfo::bckNormal, 1},		// 5A				dup_x1
	{BytecodeControlInfo::bckNormal, 1},		// 5B				dup_x2
	{BytecodeControlInfo::bckNormal, 1},		// 5C				dup2
	{BytecodeControlInfo::bckNormal, 1},		// 5D				dup2_x1
	{BytecodeControlInfo::bckNormal, 1},		// 5E				dup2_x2
	{BytecodeControlInfo::bckNormal, 1},		// 5F				swap
	{BytecodeControlInfo::bckNormal, 1},		// 60				iadd
	{BytecodeControlInfo::bckNormal, 1},		// 61				ladd
	{BytecodeControlInfo::bckNormal, 1},		// 62				fadd
	{BytecodeControlInfo::bckNormal, 1},		// 63				dadd
	{BytecodeControlInfo::bckNormal, 1},		// 64				isub
	{BytecodeControlInfo::bckNormal, 1},		// 65				lsub
	{BytecodeControlInfo::bckNormal, 1},		// 66				fsub
	{BytecodeControlInfo::bckNormal, 1},		// 67				dsub
	{BytecodeControlInfo::bckNormal, 1},		// 68				imul
	{BytecodeControlInfo::bckNormal, 1},		// 69				lmul
	{BytecodeControlInfo::bckNormal, 1},		// 6A				fmul
	{BytecodeControlInfo::bckNormal, 1},		// 6B				dmul
	{BytecodeControlInfo::bckExc, 1},			// 6C				idiv
	{BytecodeControlInfo::bckExc, 1},			// 6D				ldiv
	{BytecodeControlInfo::bckNormal, 1},		// 6E				fdiv
	{BytecodeControlInfo::bckNormal, 1},		// 6F				ddiv
	{BytecodeControlInfo::bckExc, 1},			// 70				irem
	{BytecodeControlInfo::bckExc, 1},			// 71				lrem
	{BytecodeControlInfo::bckNormal, 1},		// 72				frem
	{BytecodeControlInfo::bckNormal, 1},		// 73				drem
	{BytecodeControlInfo::bckNormal, 1},		// 74				ineg
	{BytecodeControlInfo::bckNormal, 1},		// 75				lneg
	{BytecodeControlInfo::bckNormal, 1},		// 76				fneg
	{BytecodeControlInfo::bckNormal, 1},		// 77				dneg
	{BytecodeControlInfo::bckNormal, 1},		// 78				ishl
	{BytecodeControlInfo::bckNormal, 1},		// 79				lshl
	{BytecodeControlInfo::bckNormal, 1},		// 7A				ishr
	{BytecodeControlInfo::bckNormal, 1},		// 7B				lshr
	{BytecodeControlInfo::bckNormal, 1},		// 7C				iushr
	{BytecodeControlInfo::bckNormal, 1},		// 7D				lushr
	{BytecodeControlInfo::bckNormal, 1},		// 7E				iand
	{BytecodeControlInfo::bckNormal, 1},		// 7F				land
	{BytecodeControlInfo::bckNormal, 1},		// 80				ior
	{BytecodeControlInfo::bckNormal, 1},		// 81				lor
	{BytecodeControlInfo::bckNormal, 1},		// 82				ixor
	{BytecodeControlInfo::bckNormal, 1},		// 83				lxor
	{BytecodeControlInfo::bckNormal, 3},		// 84 vv cc			iinc v,c
	{BytecodeControlInfo::bckNormal, 1},		// 85				i2l
	{BytecodeControlInfo::bckNormal, 1},		// 86				i2f
	{BytecodeControlInfo::bckNormal, 1},		// 87				i2d
	{BytecodeControlInfo::bckNormal, 1},		// 88				l2i
	{BytecodeControlInfo::bckNormal, 1},		// 89				l2f
	{BytecodeControlInfo::bckNormal, 1},		// 8A				l2d
	{BytecodeControlInfo::bckNormal, 1},		// 8B				f2i
	{BytecodeControlInfo::bckNormal, 1},		// 8C				f2l
	{BytecodeControlInfo::bckNormal, 1},		// 8D				f2d
	{BytecodeControlInfo::bckNormal, 1},		// 8E				d2i
	{BytecodeControlInfo::bckNormal, 1},		// 8F				d2l
	{BytecodeControlInfo::bckNormal, 1},		// 90				d2f
	{BytecodeControlInfo::bckNormal, 1},		// 91				i2b
	{BytecodeControlInfo::bckNormal, 1},		// 92				i2c
	{BytecodeControlInfo::bckNormal, 1},		// 93				i2s
	{BytecodeControlInfo::bckNormal, 1},		// 94				lcmp
	{BytecodeControlInfo::bckNormal, 1},		// 95				fcmpl
	{BytecodeControlInfo::bckNormal, 1},		// 96				fcmpg
	{BytecodeControlInfo::bckNormal, 1},		// 97				dcmpl
	{BytecodeControlInfo::bckNormal, 1},		// 98				dcmpg
	{BytecodeControlInfo::bckIf, 3},			// 99 dddd			ifeq
	{BytecodeControlInfo::bckIf, 3},			// 9A dddd			ifne
	{BytecodeControlInfo::bckIf, 3},			// 9B dddd			iflt
	{BytecodeControlInfo::bckIf, 3},			// 9C dddd			ifge
	{BytecodeControlInfo::bckIf, 3},			// 9D dddd			ifgt
	{BytecodeControlInfo::bckIf, 3},			// 9E dddd			ifle
	{BytecodeControlInfo::bckIf, 3},			// 9F dddd			if_icmpeq
	{BytecodeControlInfo::bckIf, 3},			// A0 dddd			if_icmpne
	{BytecodeControlInfo::bckIf, 3},			// A1 dddd			if_icmplt
	{BytecodeControlInfo::bckIf, 3},			// A2 dddd			if_icmpge
	{BytecodeControlInfo::bckIf, 3},			// A3 dddd			if_icmpgt
	{BytecodeControlInfo::bckIf, 3},			// A4 dddd			if_icmple
	{BytecodeControlInfo::bckIf, 3},			// A5 dddd			if_acmpeq
	{BytecodeControlInfo::bckIf, 3},			// A6 dddd			if_acmpne
	{BytecodeControlInfo::bckGoto, 3},			// A7 dddd			goto
	{BytecodeControlInfo::bckJsr, 3},			// A8 dddd			jsr
	{BytecodeControlInfo::bckRet, 2},			// A9 vv			ret v
	{BytecodeControlInfo::bckTableSwitch, 1},	// AA ...			tableswitch
	{BytecodeControlInfo::bckLookupSwitch, 1},	// AB ...			lookupswitch
	{BytecodeControlInfo::bckReturn, 1},		// AC				ireturn
	{BytecodeControlInfo::bckReturn, 1},		// AD				lreturn
	{BytecodeControlInfo::bckReturn, 1},		// AE				freturn
	{BytecodeControlInfo::bckReturn, 1},		// AF				dreturn
	{BytecodeControlInfo::bckReturn, 1},		// B0				areturn
	{BytecodeControlInfo::bckReturn, 1},		// B1				return
	{BytecodeControlInfo::bckNormal, 3},		// B2 iiii			getstatic
	{BytecodeControlInfo::bckNormal, 3},		// B3 iiii			putstatic
	{BytecodeControlInfo::bckExc, 3},			// B4 iiii			getfield
	{BytecodeControlInfo::bckExc, 3},			// B5 iiii			putfield
	{BytecodeControlInfo::bckExc, 3},			// B6 iiii			invokevirtual
	{BytecodeControlInfo::bckExc, 3},			// B7 iiii			invokespecial
	{BytecodeControlInfo::bckExc, 3},			// B8 iiii			invokestatic
	{BytecodeControlInfo::bckExc, 5},			// B9 iiii nn00		invokeinterface
	{BytecodeControlInfo::bckIllegal, 1},		// BA				unused
	{BytecodeControlInfo::bckExc, 3},			// BB iiii			new
	{BytecodeControlInfo::bckExc, 2},			// BC tt			newarray
	{BytecodeControlInfo::bckExc, 3},			// BD iiii			anewarray
	{BytecodeControlInfo::bckExc, 1},			// BE				arraylength
	{BytecodeControlInfo::bckThrow, 1},			// BF				athrow
	{BytecodeControlInfo::bckExc, 3},			// C0 iiii			checkcast
	{BytecodeControlInfo::bckNormal, 3},		// C1 iiii			instanceof
	{BytecodeControlInfo::bckExc, 1},			// C2				monitorenter
	{BytecodeControlInfo::bckExc, 1},			// C3				monitorexit
	{BytecodeControlInfo::bckNormal, 0},		// C4 ...			wide
	{BytecodeControlInfo::bckExc, 4},			// C5 iiii nn		multianewarray
	{BytecodeControlInfo::bckIf, 3},			// C6 dddd			ifnull
	{BytecodeControlInfo::bckIf, 3},			// C7 dddd			ifnonnull
	{BytecodeControlInfo::bckGoto_W, 5},		// C8 dddddddd		goto_w
	{BytecodeControlInfo::bckJsr_W, 5},			// C9 dddddddd		jsr_w
	{BytecodeControlInfo::bckExc, 1},			// CA				breakpoint
	{BytecodeControlInfo::bckIllegal, 1},		// CB				unused
	{BytecodeControlInfo::bckIllegal, 1},		// CC				unused
	{BytecodeControlInfo::bckIllegal, 1},		// CD				unused
	{BytecodeControlInfo::bckIllegal, 1},		// CE				unused
	{BytecodeControlInfo::bckIllegal, 1},		// CF				unused
	{BytecodeControlInfo::bckIllegal, 1},		// D0				unused
	{BytecodeControlInfo::bckIllegal, 1},		// D1				unused
	{BytecodeControlInfo::bckIllegal, 1},		// D2				unused
	{BytecodeControlInfo::bckIllegal, 1},		// D3				unused
	{BytecodeControlInfo::bckIllegal, 1},		// D4				unused
	{BytecodeControlInfo::bckIllegal, 1},		// D5				unused
	{BytecodeControlInfo::bckIllegal, 1},		// D6				unused
	{BytecodeControlInfo::bckIllegal, 1},		// D7				unused
	{BytecodeControlInfo::bckIllegal, 1},		// D8				unused
	{BytecodeControlInfo::bckIllegal, 1},		// D9				unused
	{BytecodeControlInfo::bckIllegal, 1},		// DA				unused
	{BytecodeControlInfo::bckIllegal, 1},		// DB				unused
	{BytecodeControlInfo::bckIllegal, 1},		// DC				unused
	{BytecodeControlInfo::bckIllegal, 1},		// DD				unused
	{BytecodeControlInfo::bckIllegal, 1},		// DE				unused
	{BytecodeControlInfo::bckIllegal, 1},		// DF				unused
	{BytecodeControlInfo::bckIllegal, 1},		// E0				unused
	{BytecodeControlInfo::bckIllegal, 1},		// E1				unused
	{BytecodeControlInfo::bckIllegal, 1},		// E2				unused
	{BytecodeControlInfo::bckIllegal, 1},		// E3				unused
	{BytecodeControlInfo::bckIllegal, 1},		// E4				unused
	{BytecodeControlInfo::bckIllegal, 1},		// E5				unused
	{BytecodeControlInfo::bckIllegal, 1},		// E6				unused
	{BytecodeControlInfo::bckIllegal, 1},		// E7				unused
	{BytecodeControlInfo::bckIllegal, 1},		// E8				unused
	{BytecodeControlInfo::bckIllegal, 1},		// E9				unused
	{BytecodeControlInfo::bckIllegal, 1},		// EA				unused
	{BytecodeControlInfo::bckIllegal, 1},		// EB				unused
	{BytecodeControlInfo::bckIllegal, 1},		// EC				unused
	{BytecodeControlInfo::bckIllegal, 1},		// ED				unused
	{BytecodeControlInfo::bckIllegal, 1},		// EE				unused
	{BytecodeControlInfo::bckIllegal, 1},		// EF				unused
	{BytecodeControlInfo::bckIllegal, 1},		// F0				unused
	{BytecodeControlInfo::bckIllegal, 1},		// F1				unused
	{BytecodeControlInfo::bckIllegal, 1},		// F2				unused
	{BytecodeControlInfo::bckIllegal, 1},		// F3				unused
	{BytecodeControlInfo::bckIllegal, 1},		// F4				unused
	{BytecodeControlInfo::bckIllegal, 1},		// F5				unused
	{BytecodeControlInfo::bckIllegal, 1},		// F6				unused
	{BytecodeControlInfo::bckIllegal, 1},		// F7				unused
	{BytecodeControlInfo::bckIllegal, 1},		// F8				unused
	{BytecodeControlInfo::bckIllegal, 1},		// F9				unused
	{BytecodeControlInfo::bckIllegal, 1},		// FA				unused
	{BytecodeControlInfo::bckIllegal, 1},		// FB				unused
	{BytecodeControlInfo::bckIllegal, 1},		// FC				unused
	{BytecodeControlInfo::bckIllegal, 1},		// FD				unused
	{BytecodeControlInfo::bckIllegal, 1},		// FE				unused
	{BytecodeControlInfo::bckIllegal, 1}		// FF				unused
};

// Kinds and sizes of wide bytecodes
const BytecodeControlInfo wideBytecodeControlInfos[256] =
{
	{BytecodeControlInfo::bckIllegal, 2},		// C4 00			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 01			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 02			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 03			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 04			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 05			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 06			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 07			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 08			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 09			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 0A			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 0B			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 0C			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 0D			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 0E			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 0F			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 10			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 11			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 12			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 13			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 14			wide unused
	{BytecodeControlInfo::bckNormal, 4},		// C4 15 vvvv		wide iload v
	{BytecodeControlInfo::bckNormal, 4},		// C4 16 vvvv		wide lload v
	{BytecodeControlInfo::bckNormal, 4},		// C4 17 vvvv		wide fload v
	{BytecodeControlInfo::bckNormal, 4},		// C4 18 vvvv		wide dload v
	{BytecodeControlInfo::bckNormal, 4},		// C4 19 vvvv		wide aload v
	{BytecodeControlInfo::bckIllegal, 2},		// C4 1A			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 1B			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 1C			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 1D			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 1E			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 1F			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 20			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 21			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 22			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 23			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 24			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 25			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 26			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 27			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 28			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 29			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 2A			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 2B			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 2C			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 2D			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 2E			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 2F			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 30			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 31			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 32			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 33			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 34			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 35			wide unused
	{BytecodeControlInfo::bckNormal, 4},		// C4 36 vvvv		wide istore v
	{BytecodeControlInfo::bckNormal, 4},		// C4 37 vvvv		wide lstore v
	{BytecodeControlInfo::bckNormal, 4},		// C4 38 vvvv		wide fstore v
	{BytecodeControlInfo::bckNormal, 4},		// C4 39 vvvv		wide dstore v
	{BytecodeControlInfo::bckNormal, 4},		// C4 3A vvvv		wide astore v
	{BytecodeControlInfo::bckIllegal, 2},		// C4 3B			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 3C			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 3D			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 3E			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 3F			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 40			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 41			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 42			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 43			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 44			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 45			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 46			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 47			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 48			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 49			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 4A			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 4B			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 4C			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 4D			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 4E			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 4F			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 50			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 51			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 52			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 53			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 54			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 55			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 56			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 57			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 58			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 59			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 5A			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 5B			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 5C			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 5D			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 5E			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 5F			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 60			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 61			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 62			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 63			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 64			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 65			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 66			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 67			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 68			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 69			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 6A			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 6B			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 6C			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 6D			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 6E			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 6F			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 70			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 71			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 72			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 73			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 74			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 75			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 76			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 77			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 78			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 79			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 7A			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 7B			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 7C			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 7D			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 7E			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 7F			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 80			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 81			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 82			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 83			wide unused
	{BytecodeControlInfo::bckNormal, 6},		// C4 84 vvvv cccc	wide iinc v,c
	{BytecodeControlInfo::bckIllegal, 2},		// C4 85			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 86			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 87			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 88			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 89			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 8A			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 8B			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 8C			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 8D			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 8E			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 8F			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 90			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 91			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 92			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 93			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 94			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 95			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 96			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 97			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 98			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 99			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 9A			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 9B			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 9C			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 9D			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 9E			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 9F			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 A0			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 A1			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 A2			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 A3			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 A4			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 A5			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 A6			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 A7			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 A8			wide unused
	{BytecodeControlInfo::bckRet_W, 4},			// C4 A9 vvvv		wide ret v
	{BytecodeControlInfo::bckIllegal, 2},		// C4 AA			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 AB			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 AC			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 AD			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 AE			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 AF			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 B0			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 B1			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 B2			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 B3			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 B4			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 B5			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 B6			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 B7			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 B8			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 B9			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 BA			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 BB			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 BC			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 BD			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 BE			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 BF			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 C0			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 C1			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 C2			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 C3			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 C4			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 C5			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 C6			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 C7			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 C8			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 C9			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 CA			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 CB			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 CC			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 CD			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 CE			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 CF			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 D0			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 D1			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 D2			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 D3			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 D4			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 D5			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 D6			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 D7			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 D8			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 D9			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 DA			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 DB			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 DC			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 DD			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 DE			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 DF			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 E0			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 E1			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 E2			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 E3			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 E4			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 E5			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 E6			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 E7			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 E8			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 E9			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 EA			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 EB			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 EC			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 ED			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 EE			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 EF			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 F0			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 F1			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 F2			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 F3			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 F4			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 F5			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 F6			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 F7			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 F8			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 F9			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 FA			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 FB			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 FC			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 FD			wide unused
	{BytecodeControlInfo::bckIllegal, 2},		// C4 FE			wide unused
	{BytecodeControlInfo::bckIllegal, 2}		// C4 FF			wide unused
};


// ----------------------------------------------------------------------------


#ifdef DEBUG_LOG
struct BytecodeDisasmInfo
{
	enum ArgFormat			// Formats of arguments
	{
		afNone,				// No arguments
		afSByte,			// One signed byte
		afSHalf,			// One signed halfword
		afByteConstIndex,	// One unsigned byte constant table index
		afHalfConstIndex,	// One unsigned halfword constant table index
		afVar,				// One unsigned byte (or halfword if wide) local variable index
		afIInc,				// Arguments for iinc or wide iinc
		afDisp,				// One signed halfword branch displacement
		afWideDisp,			// One signed word branch displacement
		afTableSwitch,		// Arguments for tableswitch
		afLookupSwitch,		// Arguments for lookupswitch
		afInvokeInterface,	// Arguments for invokeinterface
		afNewArray,			// Argument for newarray
		afMultiANewArray,	// Argument for multianewarray
		afWide,				// Wide instruction prefix
		afIllegal			// Any bytecode that's not defined by the Java spec
	};

	ArgFormat format;							// Kind of bytecode
	char name[16];								// Name of bytecode
};


// Disassembly table for normal bytecodes
static BytecodeDisasmInfo bytecodeDisasmInfos[256] =
{
	{BytecodeDisasmInfo::afNone, "nop"},						// 00				nop
	{BytecodeDisasmInfo::afNone, "aconst_null"},				// 01				aconst_null
	{BytecodeDisasmInfo::afNone, "iconst_m1"},					// 02				iconst_m1
	{BytecodeDisasmInfo::afNone, "iconst_0"},					// 03				iconst_0
	{BytecodeDisasmInfo::afNone, "iconst_1"},					// 04				iconst_1
	{BytecodeDisasmInfo::afNone, "iconst_2"},					// 05				iconst_2
	{BytecodeDisasmInfo::afNone, "iconst_3"},					// 06				iconst_3
	{BytecodeDisasmInfo::afNone, "iconst_4"},					// 07				iconst_4
	{BytecodeDisasmInfo::afNone, "iconst_5"},					// 08				iconst_5
	{BytecodeDisasmInfo::afNone, "lconst_0"},					// 09				lconst_0
	{BytecodeDisasmInfo::afNone, "lconst_1"},					// 0A				lconst_1
	{BytecodeDisasmInfo::afNone, "fconst_0"},					// 0B				fconst_0
	{BytecodeDisasmInfo::afNone, "fconst_1"},					// 0C				fconst_1
	{BytecodeDisasmInfo::afNone, "fconst_2"},					// 0D				fconst_2
	{BytecodeDisasmInfo::afNone, "dconst_0"},					// 0E				dconst_0
	{BytecodeDisasmInfo::afNone, "dconst_1"},					// 0F				dconst_1
	{BytecodeDisasmInfo::afSByte, "bipush"},					// 10 cc			bipush c
	{BytecodeDisasmInfo::afSHalf, "sipush"},					// 11 cccc			sipush c
	{BytecodeDisasmInfo::afByteConstIndex, "ldc"},				// 12 ii			ldc i
	{BytecodeDisasmInfo::afHalfConstIndex, "ldc_w"},			// 13 iiii			ldc_w i
	{BytecodeDisasmInfo::afHalfConstIndex, "ldc2_w"},			// 14 iiii			ldc2_w i
	{BytecodeDisasmInfo::afVar, "iload"},						// 15 vv			iload v
	{BytecodeDisasmInfo::afVar, "lload"},						// 16 vv			lload v
	{BytecodeDisasmInfo::afVar, "fload"},						// 17 vv			fload v
	{BytecodeDisasmInfo::afVar, "dload"},						// 18 vv			dload v
	{BytecodeDisasmInfo::afVar, "aload"},						// 19 vv			aload v
	{BytecodeDisasmInfo::afNone, "iload_0"},					// 1A				iload_0
	{BytecodeDisasmInfo::afNone, "iload_1"},					// 1B				iload_1
	{BytecodeDisasmInfo::afNone, "iload_2"},					// 1C				iload_2
	{BytecodeDisasmInfo::afNone, "iload_3"},					// 1D				iload_3
	{BytecodeDisasmInfo::afNone, "lload_0"},					// 1E				lload_0
	{BytecodeDisasmInfo::afNone, "lload_1"},					// 1F				lload_1
	{BytecodeDisasmInfo::afNone, "lload_2"},					// 20				lload_2
	{BytecodeDisasmInfo::afNone, "lload_3"},					// 21				lload_3
	{BytecodeDisasmInfo::afNone, "fload_0"},					// 22				fload_0
	{BytecodeDisasmInfo::afNone, "fload_1"},					// 23				fload_1
	{BytecodeDisasmInfo::afNone, "fload_2"},					// 24				fload_2
	{BytecodeDisasmInfo::afNone, "fload_3"},					// 25				fload_3
	{BytecodeDisasmInfo::afNone, "dload_0"},					// 26				dload_0
	{BytecodeDisasmInfo::afNone, "dload_1"},					// 27				dload_1
	{BytecodeDisasmInfo::afNone, "dload_2"},					// 28				dload_2
	{BytecodeDisasmInfo::afNone, "dload_3"},					// 29				dload_3
	{BytecodeDisasmInfo::afNone, "aload_0"},					// 2A				aload_0
	{BytecodeDisasmInfo::afNone, "aload_1"},					// 2B				aload_1
	{BytecodeDisasmInfo::afNone, "aload_2"},					// 2C				aload_2
	{BytecodeDisasmInfo::afNone, "aload_3"},					// 2D				aload_3
	{BytecodeDisasmInfo::afNone, "iaload"},						// 2E				iaload
	{BytecodeDisasmInfo::afNone, "laload"},						// 2F				laload
	{BytecodeDisasmInfo::afNone, "faload"},						// 30				faload
	{BytecodeDisasmInfo::afNone, "daload"},						// 31				daload
	{BytecodeDisasmInfo::afNone, "aaload"},						// 32				aaload
	{BytecodeDisasmInfo::afNone, "baload"},						// 33				baload
	{BytecodeDisasmInfo::afNone, "caload"},						// 34				caload
	{BytecodeDisasmInfo::afNone, "saload"},						// 35				saload
	{BytecodeDisasmInfo::afVar, "istore"},						// 36 vv			istore v
	{BytecodeDisasmInfo::afVar, "lstore"},						// 37 vv			lstore v
	{BytecodeDisasmInfo::afVar, "fstore"},						// 38 vv			fstore v
	{BytecodeDisasmInfo::afVar, "dstore"},						// 39 vv			dstore v
	{BytecodeDisasmInfo::afVar, "astore"},						// 3A vv			astore v
	{BytecodeDisasmInfo::afNone, "istore_0"},					// 3B				istore_0
	{BytecodeDisasmInfo::afNone, "istore_1"},					// 3C				istore_1
	{BytecodeDisasmInfo::afNone, "istore_2"},					// 3D				istore_2
	{BytecodeDisasmInfo::afNone, "istore_3"},					// 3E				istore_3
	{BytecodeDisasmInfo::afNone, "lstore_0"},					// 3F				lstore_0
	{BytecodeDisasmInfo::afNone, "lstore_1"},					// 40				lstore_1
	{BytecodeDisasmInfo::afNone, "lstore_2"},					// 41				lstore_2
	{BytecodeDisasmInfo::afNone, "lstore_3"},					// 42				lstore_3
	{BytecodeDisasmInfo::afNone, "fstore_0"},					// 43				fstore_0
	{BytecodeDisasmInfo::afNone, "fstore_1"},					// 44				fstore_1
	{BytecodeDisasmInfo::afNone, "fstore_2"},					// 45				fstore_2
	{BytecodeDisasmInfo::afNone, "fstore_3"},					// 46				fstore_3
	{BytecodeDisasmInfo::afNone, "dstore_0"},					// 47				dstore_0
	{BytecodeDisasmInfo::afNone, "dstore_1"},					// 48				dstore_1
	{BytecodeDisasmInfo::afNone, "dstore_2"},					// 49				dstore_2
	{BytecodeDisasmInfo::afNone, "dstore_3"},					// 4A				dstore_3
	{BytecodeDisasmInfo::afNone, "astore_0"},					// 4B				astore_0
	{BytecodeDisasmInfo::afNone, "astore_1"},					// 4C				astore_1
	{BytecodeDisasmInfo::afNone, "astore_2"},					// 4D				astore_2
	{BytecodeDisasmInfo::afNone, "astore_3"},					// 4E				astore_3
	{BytecodeDisasmInfo::afNone, "iastore"},					// 4F				iastore
	{BytecodeDisasmInfo::afNone, "lastore"},					// 50				lastore
	{BytecodeDisasmInfo::afNone, "fastore"},					// 51				fastore
	{BytecodeDisasmInfo::afNone, "dastore"},					// 52				dastore
	{BytecodeDisasmInfo::afNone, "aastore"},					// 53				aastore
	{BytecodeDisasmInfo::afNone, "bastore"},					// 54				bastore
	{BytecodeDisasmInfo::afNone, "castore"},					// 55				castore
	{BytecodeDisasmInfo::afNone, "sastore"},					// 56				sastore
	{BytecodeDisasmInfo::afNone, "pop"},						// 57				pop
	{BytecodeDisasmInfo::afNone, "pop2"},						// 58				pop2
	{BytecodeDisasmInfo::afNone, "dup"},						// 59				dup
	{BytecodeDisasmInfo::afNone, "dup_x1"},						// 5A				dup_x1
	{BytecodeDisasmInfo::afNone, "dup_x2"},						// 5B				dup_x2
	{BytecodeDisasmInfo::afNone, "dup2"},						// 5C				dup2
	{BytecodeDisasmInfo::afNone, "dup2_x1"},					// 5D				dup2_x1
	{BytecodeDisasmInfo::afNone, "dup2_x2"},					// 5E				dup2_x2
	{BytecodeDisasmInfo::afNone, "swap"},						// 5F				swap
	{BytecodeDisasmInfo::afNone, "iadd"},						// 60				iadd
	{BytecodeDisasmInfo::afNone, "ladd"},						// 61				ladd
	{BytecodeDisasmInfo::afNone, "fadd"},						// 62				fadd
	{BytecodeDisasmInfo::afNone, "dadd"},						// 63				dadd
	{BytecodeDisasmInfo::afNone, "isub"},						// 64				isub
	{BytecodeDisasmInfo::afNone, "lsub"},						// 65				lsub
	{BytecodeDisasmInfo::afNone, "fsub"},						// 66				fsub
	{BytecodeDisasmInfo::afNone, "dsub"},						// 67				dsub
	{BytecodeDisasmInfo::afNone, "imul"},						// 68				imul
	{BytecodeDisasmInfo::afNone, "lmul"},						// 69				lmul
	{BytecodeDisasmInfo::afNone, "fmul"},						// 6A				fmul
	{BytecodeDisasmInfo::afNone, "dmul"},						// 6B				dmul
	{BytecodeDisasmInfo::afNone, "idiv"},						// 6C				idiv
	{BytecodeDisasmInfo::afNone, "ldiv"},						// 6D				ldiv
	{BytecodeDisasmInfo::afNone, "fdiv"},						// 6E				fdiv
	{BytecodeDisasmInfo::afNone, "ddiv"},						// 6F				ddiv
	{BytecodeDisasmInfo::afNone, "irem"},						// 70				irem
	{BytecodeDisasmInfo::afNone, "lrem"},						// 71				lrem
	{BytecodeDisasmInfo::afNone, "frem"},						// 72				frem
	{BytecodeDisasmInfo::afNone, "drem"},						// 73				drem
	{BytecodeDisasmInfo::afNone, "ineg"},						// 74				ineg
	{BytecodeDisasmInfo::afNone, "lneg"},						// 75				lneg
	{BytecodeDisasmInfo::afNone, "fneg"},						// 76				fneg
	{BytecodeDisasmInfo::afNone, "dneg"},						// 77				dneg
	{BytecodeDisasmInfo::afNone, "ishl"},						// 78				ishl
	{BytecodeDisasmInfo::afNone, "lshl"},						// 79				lshl
	{BytecodeDisasmInfo::afNone, "ishr"},						// 7A				ishr
	{BytecodeDisasmInfo::afNone, "lshr"},						// 7B				lshr
	{BytecodeDisasmInfo::afNone, "iushr"},						// 7C				iushr
	{BytecodeDisasmInfo::afNone, "lushr"},						// 7D				lushr
	{BytecodeDisasmInfo::afNone, "iand"},						// 7E				iand
	{BytecodeDisasmInfo::afNone, "land"},						// 7F				land
	{BytecodeDisasmInfo::afNone, "ior"},						// 80				ior
	{BytecodeDisasmInfo::afNone, "lor"},						// 81				lor
	{BytecodeDisasmInfo::afNone, "ixor"},						// 82				ixor
	{BytecodeDisasmInfo::afNone, "lxor"},						// 83				lxor
	{BytecodeDisasmInfo::afIInc, "iinc"},						// 84 vv cc			iinc v,c
	{BytecodeDisasmInfo::afNone, "i2l"},						// 85				i2l
	{BytecodeDisasmInfo::afNone, "i2f"},						// 86				i2f
	{BytecodeDisasmInfo::afNone, "i2d"},						// 87				i2d
	{BytecodeDisasmInfo::afNone, "l2i"},						// 88				l2i
	{BytecodeDisasmInfo::afNone, "l2f"},						// 89				l2f
	{BytecodeDisasmInfo::afNone, "l2d"},						// 8A				l2d
	{BytecodeDisasmInfo::afNone, "f2i"},						// 8B				f2i
	{BytecodeDisasmInfo::afNone, "f2l"},						// 8C				f2l
	{BytecodeDisasmInfo::afNone, "f2d"},						// 8D				f2d
	{BytecodeDisasmInfo::afNone, "d2i"},						// 8E				d2i
	{BytecodeDisasmInfo::afNone, "d2l"},						// 8F				d2l
	{BytecodeDisasmInfo::afNone, "d2f"},						// 90				d2f
	{BytecodeDisasmInfo::afNone, "i2b"},						// 91				i2b
	{BytecodeDisasmInfo::afNone, "i2c"},						// 92				i2c
	{BytecodeDisasmInfo::afNone, "i2s"},						// 93				i2s
	{BytecodeDisasmInfo::afNone, "lcmp"},						// 94				lcmp
	{BytecodeDisasmInfo::afNone, "fcmpl"},						// 95				fcmpl
	{BytecodeDisasmInfo::afNone, "fcmpg"},						// 96				fcmpg
	{BytecodeDisasmInfo::afNone, "dcmpl"},						// 97				dcmpl
	{BytecodeDisasmInfo::afNone, "dcmpg"},						// 98				dcmpg
	{BytecodeDisasmInfo::afDisp, "ifeq"},						// 99 dddd			ifeq
	{BytecodeDisasmInfo::afDisp, "ifne"},						// 9A dddd			ifne
	{BytecodeDisasmInfo::afDisp, "iflt"},						// 9B dddd			iflt
	{BytecodeDisasmInfo::afDisp, "ifge"},						// 9C dddd			ifge
	{BytecodeDisasmInfo::afDisp, "ifgt"},						// 9D dddd			ifgt
	{BytecodeDisasmInfo::afDisp, "ifle"},						// 9E dddd			ifle
	{BytecodeDisasmInfo::afDisp, "if_icmpeq"},					// 9F dddd			if_icmpeq
	{BytecodeDisasmInfo::afDisp, "if_icmpne"},					// A0 dddd			if_icmpne
	{BytecodeDisasmInfo::afDisp, "if_icmplt"},					// A1 dddd			if_icmplt
	{BytecodeDisasmInfo::afDisp, "if_icmpge"},					// A2 dddd			if_icmpge
	{BytecodeDisasmInfo::afDisp, "if_icmpgt"},					// A3 dddd			if_icmpgt
	{BytecodeDisasmInfo::afDisp, "if_icmple"},					// A4 dddd			if_icmple
	{BytecodeDisasmInfo::afDisp, "if_acmpeq"},					// A5 dddd			if_acmpeq
	{BytecodeDisasmInfo::afDisp, "if_acmpne"},					// A6 dddd			if_acmpne
	{BytecodeDisasmInfo::afDisp, "goto"},						// A7 dddd			goto
	{BytecodeDisasmInfo::afDisp, "jsr"},						// A8 dddd			jsr
	{BytecodeDisasmInfo::afVar, "ret"},							// A9 vv			ret v
	{BytecodeDisasmInfo::afTableSwitch, "tableswitch"},			// AA ...			tableswitch
	{BytecodeDisasmInfo::afLookupSwitch, "lookupswitch"},		// AB ...			lookupswitch
	{BytecodeDisasmInfo::afNone, "ireturn"},					// AC				ireturn
	{BytecodeDisasmInfo::afNone, "lreturn"},					// AD				lreturn
	{BytecodeDisasmInfo::afNone, "freturn"},					// AE				freturn
	{BytecodeDisasmInfo::afNone, "dreturn"},					// AF				dreturn
	{BytecodeDisasmInfo::afNone, "areturn"},					// B0				areturn
	{BytecodeDisasmInfo::afNone, "return"},						// B1				return
	{BytecodeDisasmInfo::afHalfConstIndex, "getstatic"},		// B2 iiii			getstatic
	{BytecodeDisasmInfo::afHalfConstIndex, "putstatic"},		// B3 iiii			putstatic
	{BytecodeDisasmInfo::afHalfConstIndex, "getfield"},			// B4 iiii			getfield
	{BytecodeDisasmInfo::afHalfConstIndex, "putfield"},			// B5 iiii			putfield
	{BytecodeDisasmInfo::afHalfConstIndex, "invokevirtual"},	// B6 iiii			invokevirtual
	{BytecodeDisasmInfo::afHalfConstIndex, "invokespecial"},	// B7 iiii			invokespecial
	{BytecodeDisasmInfo::afHalfConstIndex, "invokestatic"},		// B8 iiii			invokestatic
	{BytecodeDisasmInfo::afInvokeInterface, "invokeinterface"},	// B9 iiii nn00		invokeinterface
	{BytecodeDisasmInfo::afIllegal, "????"},					// BA				unused
	{BytecodeDisasmInfo::afHalfConstIndex, "new"},				// BB iiii			new
	{BytecodeDisasmInfo::afNewArray, "newarray"},				// BC tt			newarray
	{BytecodeDisasmInfo::afHalfConstIndex, "anewarray"},		// BD iiii			anewarray
	{BytecodeDisasmInfo::afNone, "arraylength"},				// BE				arraylength
	{BytecodeDisasmInfo::afNone, "athrow"},						// BF				athrow
	{BytecodeDisasmInfo::afHalfConstIndex, "checkcast"},		// C0 iiii			checkcast
	{BytecodeDisasmInfo::afHalfConstIndex, "instanceof"},		// C1 iiii			instanceof
	{BytecodeDisasmInfo::afNone, "monitorenter"},				// C2				monitorenter
	{BytecodeDisasmInfo::afNone, "monitorexit"},				// C3				monitorexit
	{BytecodeDisasmInfo::afWide, "wide"},						// C4 ...			wide
	{BytecodeDisasmInfo::afMultiANewArray, "multianewarray"},	// C5 iiii nn		multianewarray
	{BytecodeDisasmInfo::afDisp, "ifnull"},						// C6 dddd			ifnull
	{BytecodeDisasmInfo::afDisp, "ifnonnull"},					// C7 dddd			ifnonnull
	{BytecodeDisasmInfo::afWideDisp, "goto_w"},					// C8 dddddddd		goto_w
	{BytecodeDisasmInfo::afWideDisp, "jsr_w"},					// C9 dddddddd		jsr_w
	{BytecodeDisasmInfo::afNone, "breakpoint"},					// CA				breakpoint
	{BytecodeDisasmInfo::afIllegal, "????"},					// CB				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// CC				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// CD				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// CE				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// CF				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// D0				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// D1				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// D2				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// D3				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// D4				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// D5				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// D6				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// D7				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// D8				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// D9				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// DA				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// DB				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// DC				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// DD				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// DE				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// DF				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// E0				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// E1				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// E2				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// E3				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// E4				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// E5				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// E6				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// E7				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// E8				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// E9				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// EA				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// EB				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// EC				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// ED				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// EE				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// EF				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// F0				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// F1				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// F2				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// F3				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// F4				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// F5				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// F6				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// F7				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// F8				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// F9				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// FA				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// FB				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// FC				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// FD				unused
	{BytecodeDisasmInfo::afIllegal, "????"},					// FE				unused
	{BytecodeDisasmInfo::afIllegal, "????"}						// FF				unused
};


//
// If c is non-nil, disassemble the constant pool index symbolically using the information
// in c.  If c is nil, print the index preceded by an '@' sign.
//
static void printConstantPoolIndex(LogModuleObject &f, Uint32 index, const ConstantPool *c)
{
	if (c)
		c->printItem(f, index);
	else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("@%u", index));
}


//
// Disassemble the alignment bytes and default target for a tableswitch
// or lookupswitch instruction and print them on the output stream f.
// origin is the address of the first bytecode in the function (and must
// be word-aligned).  bc is base+1.  The base and the instruction's first
// byte has already been printed on f.
// Return the address of the npairs or lowbyte word.
//
static const bytecode *disasmAlignAndDefault(LogModuleObject &f, const bytecode *bc, const bytecode *base,
		const bytecode *origin, const char *name, int margin)
{
	int nPadBytes = -(int)bc & 3;
	for (int i = 0; i != 3; i++)
		if (i < nPadBytes) {
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.2X ", *(Uint8 *)bc));
			bc++;
		} else
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("   "));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("         %s\n", name));

	printMargin(f, margin);
	Int32 d = readBigSWord(bc);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X:     %.8X              default -> 0x%.4X\n", bc - origin, d, d + (base - origin)));
	bc += 4;
	printMargin(f, margin);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X:     ", bc - origin));
	return bc;
}


//
// Disassemble a single Java tableswitch bytecode starting at base and print it
// on the output stream f.  origin is the address of the first bytecode in
// the function (and must be word-aligned).  bc is base+1.  The base and the
// instruction's first byte has already been printed on f.
// Return the address of the next bytecode.
//
static const bytecode *disasmTableSwitch(LogModuleObject &f, const bytecode *bc, const bytecode *base, const bytecode *origin, int margin)
{
	bc = disasmAlignAndDefault(f, bc, base, origin, "tableswitch", margin);
	Int32 low = readBigSWord(bc);
	bc += 4;
	Int32 high = readBigSWord(bc);
	bc += 4;
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.8X %.8X", low, high));
	if (low <= high)
		while (true) {
			Int32 d = readBigSWord(bc);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
			printMargin(f, margin);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X:     %.8X              %d -> 0x%.4X", bc - origin, d, low, d + (base - origin)));
			bc += 4;
			if (low == high)
				break;
			low++;
		}
	return bc;
}


//
// Disassemble a single Java lookupswitch bytecode starting at base and print it
// on the output stream f.  origin is the address of the first bytecode in
// the function (and must be word-aligned).  bc is base+1.  The base and the
// instruction's first byte has already been printed on f.
// Return the address of the next bytecode.
//
static const bytecode *disasmLookupSwitch(LogModuleObject &f, const bytecode *bc, const bytecode *base, const bytecode *origin, int margin)
{
	bc = disasmAlignAndDefault(f, bc, base, origin, "lookupswitch", margin);
	Int32 nPairs = readBigSWord(bc);
	bc += 4;
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.8X", nPairs));
	while (nPairs > 0) {
		Int32 match = readBigSWord(bc);
		Int32 d = readBigSWord(bc + 4);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
		printMargin(f, margin);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X:     %.8X %.8X     %d -> 0x%.4X", bc - origin, match, d, match, d + (base - origin)));
		bc += 8;
		nPairs--;
	}
	return bc;
}


//
// Disassemble a single Java bytecode starting at bc and print it as a complete
// line on the output stream f.  origin is the address of the first bytecode in
// the function (and must be word-aligned).
// If c is non-nil, disassemble constant pool indices symbolically using the information
// in c.
// Return the address of the next bytecode.
//
const bytecode *disassembleBytecode(LogModuleObject &f, const bytecode *bc, const bytecode *origin,
		const ConstantPool *c, int margin)
{
	assert(((size_t)origin & 3) == 0);	// Make sure that origin is word-aligned.

	const bytecode *base = bc;
	bytecode b = *bc++;
	printMargin(f, margin);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X:  %.2X ", base - origin, b));
	const BytecodeDisasmInfo &bdi = bytecodeDisasmInfos[b];
	Int32 i;
	Uint32 u;
	Uint32 n;
	const char *s;

	switch (bdi.format) {

		case BytecodeDisasmInfo::afSByte:
			i = *(Int8 *)bc;
			bc++;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.2X                %s %d", i & 0xFF, bdi.name, i));
			break;

		case BytecodeDisasmInfo::afSHalf:
			i = readBigSHalfwordUnaligned(bc);
			bc += 2;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X              %s %d", i & 0xFFFF, bdi.name, i));
			break;

		case BytecodeDisasmInfo::afByteConstIndex:
			u = *(Uint8 *)bc;
			bc++;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.2X                %s ", u, bdi.name));
			printConstantPoolIndex(f, u, c);
			break;

		case BytecodeDisasmInfo::afHalfConstIndex:
			u = readBigUHalfwordUnaligned(bc);
			bc += 2;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X              %s ", u, bdi.name));
			printConstantPoolIndex(f, u, c);
			break;

		case BytecodeDisasmInfo::afVar:
			u = *(Uint8 *)bc;
			bc++;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.2X                %s %u", u, bdi.name, u));
			break;

		case BytecodeDisasmInfo::afIInc:
			u = *(Uint8 *)bc;
			bc++;
			i = *(Int8 *)bc;
			bc++;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.2X %.2X             %s %u,%d", u, i & 0xFF, bdi.name, u, i));
			break;

		case BytecodeDisasmInfo::afDisp:
			i = readBigSHalfwordUnaligned(bc);
			bc += 2;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X              %s 0x%.4X", i & 0xFFFF, bdi.name, i + (base - origin)));
			break;

		case BytecodeDisasmInfo::afWideDisp:
			i = readBigSWordUnaligned(bc);
			bc += 4;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.8X          %s 0x%.4X", i, bdi.name, i + (base - origin)));
			break;

		case BytecodeDisasmInfo::afTableSwitch:
			bc = disasmTableSwitch(f, bc, base, origin, margin);
			break;

		case BytecodeDisasmInfo::afLookupSwitch:
			bc = disasmLookupSwitch(f, bc, base, origin, margin);
			break;

		case BytecodeDisasmInfo::afInvokeInterface:
			u = readBigUHalfwordUnaligned(bc);
			bc += 2;
			n = *(Uint8 *)bc;
			bc++;
			i = *(Uint8 *)bc;
			bc++;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X %.2X %.2X        %s ", u, n, i, bdi.name));
			printConstantPoolIndex(f, u, c);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", %d", n));
			break;

		case BytecodeDisasmInfo::afNewArray:
			u = *(Uint8 *)bc;
			bc++;
			switch (u) {
				case  4: s = "T_BOOLEAN"; break;
				case  5: s = "T_CHAR"; break;
				case  6: s = "T_FLOAT"; break;
				case  7: s = "T_DOUBLE"; break;
				case  8: s = "T_BYTE"; break;
				case  9: s = "T_SHORT"; break;
				case 10: s = "T_INT"; break;
				case 11: s = "T_LONG"; break;
				default: s = "????";
			}
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.2X                %s %s", u, bdi.name, s));
			break;

		case BytecodeDisasmInfo::afMultiANewArray:
			u = readBigUHalfwordUnaligned(bc);
			bc += 2;
			n = *(Uint8 *)bc;
			bc++;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X %.2X           %s ", u, n, bdi.name));
			printConstantPoolIndex(f, u, c);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", %d", n));
			break;

		case BytecodeDisasmInfo::afWide:
			{
				b = *bc++;
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.2X ", b));
				const BytecodeDisasmInfo &bdi = bytecodeDisasmInfos[b];
				switch (bdi.format) {
					case BytecodeDisasmInfo::afVar:
						u = readBigUHalfwordUnaligned(bc);
						bc += 2;
						UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X           wide %s %d", u, bdi.name, u));
						break;
					case BytecodeDisasmInfo::afIInc:
						u = readBigUHalfwordUnaligned(bc);
						bc += 2;
						i = readBigSHalfwordUnaligned(bc);
						bc += 2;
						UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.2X %.2X          wide %s %u,%d", u, i & 0xFFFF, bdi.name, u, i));
						break;
					default:
						UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("             wide ????"));
				}
			}
			break;

		default:
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("                  %s", bdi.name));
	}
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
	return bc;
}


//
// Disassemble Java bytecodes starting from begin (inclusive) up to end (exclusive)
// and print them as complete lines on the output stream f.  origin is the address
// of the first bytecode in the function (and must be word-aligned).
// If c is non-nil, disassemble constant pool indices symbolically using the information
// in c.
//
void disassembleBytecodes(LogModuleObject &f, const bytecode *begin, const bytecode *end, const bytecode *origin,
		const ConstantPool *c, int margin)
{
	while (begin < end)
		begin = disassembleBytecode(f, begin, origin, c, margin);
	assert(begin == end);
}


//
// Disassemble nExceptionEntries exception entries starting at exceptionEntries
// and print them on the output stream f.
// If c is non-nil, disassemble constant pool indices symbolically using the information
// in c.
//
void disassembleExceptions(LogModuleObject &f, Uint32 nExceptionEntries, const ExceptionItem *exceptionEntries,
		const ConstantPool *c, int margin)
{
	if (nExceptionEntries) {
		printMargin(f, margin);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Exception handlers:\n"));
		while (nExceptionEntries--) {
			printMargin(f, margin + 4);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.4X-%.4X, ", exceptionEntries->startPc, exceptionEntries->endPc));
			printConstantPoolIndex(f, exceptionEntries->catchType, c);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" -> %.4X\n", exceptionEntries->handlerPc));
			exceptionEntries++;
		}
	}
}
#endif
