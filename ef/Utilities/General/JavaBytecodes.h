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

#ifndef JAVABYTECODES_H
#define JAVABYTECODES_H

#include "Fundamentals.h"
#include "LogModule.h"

typedef unsigned char bytecode;

enum BytecodeOpcode
{
	bcNop,
	bcAConst_Null,
	bcIConst_m1,
	bcIConst_0,
	bcIConst_1,
	bcIConst_2,
	bcIConst_3,
	bcIConst_4,
	bcIConst_5,
	bcLConst_0,
	bcLConst_1,
	bcFConst_0,
	bcFConst_1,
	bcFConst_2,
	bcDConst_0,
	bcDConst_1,
	bcBIPush,
	bcSIPush,
	bcLdc,
	bcLdc_W,
	bcLdc2_W,
	bcILoad,
	bcLLoad,
	bcFLoad,
	bcDLoad,
	bcALoad,
	bcILoad_0,
	bcILoad_1,
	bcILoad_2,
	bcILoad_3,
	bcLLoad_0,
	bcLLoad_1,
	bcLLoad_2,
	bcLLoad_3,
	bcFLoad_0,
	bcFLoad_1,
	bcFLoad_2,
	bcFLoad_3,
	bcDLoad_0,
	bcDLoad_1,
	bcDLoad_2,
	bcDLoad_3,
	bcALoad_0,
	bcALoad_1,
	bcALoad_2,
	bcALoad_3,
	bcIALoad,
	bcLALoad,
	bcFALoad,
	bcDALoad,
	bcAALoad,
	bcBALoad,
	bcCALoad,
	bcSALoad,
	bcIStore,
	bcLStore,
	bcFStore,
	bcDStore,
	bcAStore,
	bcIStore_0,
	bcIStore_1,
	bcIStore_2,
	bcIStore_3,
	bcLStore_0,
	bcLStore_1,
	bcLStore_2,
	bcLStore_3,
	bcFStore_0,
	bcFStore_1,
	bcFStore_2,
	bcFStore_3,
	bcDStore_0,
	bcDStore_1,
	bcDStore_2,
	bcDStore_3,
	bcAStore_0,
	bcAStore_1,
	bcAStore_2,
	bcAStore_3,
	bcIAStore,
	bcLAStore,
	bcFAStore,
	bcDAStore,
	bcAAStore,
	bcBAStore,
	bcCAStore,
	bcSAStore,
	bcPop,
	bcPop2,
	bcDup,
	bcDup_x1,
	bcDup_x2,
	bcDup2,
	bcDup2_x1,
	bcDup2_x2,
	bcSwap,
	bcIAdd,
	bcLAdd,
	bcFAdd,
	bcDAdd,
	bcISub,
	bcLSub,
	bcFSub,
	bcDSub,
	bcIMul,
	bcLMul,
	bcFMul,
	bcDMul,
	bcIDiv,
	bcLDiv,
	bcFDiv,
	bcDDiv,
	bcIRem,
	bcLRem,
	bcFRem,
	bcDRem,
	bcINeg,
	bcLNeg,
	bcFNeg,
	bcDNeg,
	bcIShl,
	bcLShl,
	bcIShr,
	bcLShr,
	bcIUShr,
	bcLUShr,
	bcIAnd,
	bcLAnd,
	bcIOr,
	bcLOr,
	bcIXor,
	bcLXor,
	bcIInc,
	bcI2L,
	bcI2F,
	bcI2D,
	bcL2I,
	bcL2F,
	bcL2D,
	bcF2I,
	bcF2L,
	bcF2D,
	bcD2I,
	bcD2L,
	bcD2F,
	bcInt2Byte,
	bcInt2Char,
	bcInt2Short,
	bcLCmp,
	bcFCmpL,
	bcFCmpG,
	bcDCmpL,
	bcDCmpG,
	bcIfEq,
	bcIfNe,
	bcIfLt,
	bcIfGe,
	bcIfGt,
	bcIfLe,
	bcIf_ICmpEq,
	bcIf_ICmpNe,
	bcIf_ICmpLt,
	bcIf_ICmpGe,
	bcIf_ICmpGt,
	bcIf_ICmpLe,
	bcIf_ACmpEq,
	bcIf_ACmpNe,
	bcGoto,
	bcJsr,
	bcRet,
	bcTableSwitch,
	bcLookupSwitch,
	bcIReturn,
	bcLReturn,
	bcFReturn,
	bcDReturn,
	bcAReturn,
	bcReturn,
	bcGetStatic,
	bcPutStatic,
	bcGetField,
	bcPutField,
	bcInvokeVirtual,
	bcInvokeSpecial,
	bcInvokeStatic,
	bcInvokeInterface,
	bc_unused_,
	bcNew,
	bcNewArray,
	bcANewArray,
	bcArrayLength,
	bcAThrow,
	bcCheckCast,
	bcInstanceOf,
	bcMonitorEnter,
	bcMonitorExit,
	bcWide,
	bcMultiANewArray,
	bcIfNull,
	bcIfNonnull,
	bcGoto_W,
	bcJsr_W,
	bcBreakpoint
};


// Types for newarray bytecode
enum NewArrayType
{
	natBoolean = 4,
	natChar,
	natFloat,
	natDouble,
	natByte,
	natShort,
	natInt,
	natLong
};
const uint natMin = natBoolean;
const uint natLimit = natLong + 1;


struct BytecodeControlInfo
{
	enum Kind				// Categories of bytecodes
	{
		bckNormal,			// Any bytecode that can't branch anywhere
		bckThrow,			// A bytecode that always causes an exception
		bckExc,				// A bytecode that can cause an exception
		bckGoto,			// bcGoto
		bckGoto_W,			// bcGoto_W
		bckIf,				// Any of the if bytecodes
		bckTableSwitch,		// bcTableSwitch
		bckLookupSwitch,	// bcLookupSwitch
		bckReturn,			// Any of the return bytecodes
		bckJsr,				// bcJsr
		bckJsr_W,			// bcJsr_W
		bckRet,				// bcRet
		bckRet_W,			// wide bcRet
		bckIllegal			// Any bytecode that's not defined by the Java spec
	};

	Kind kind ENUM_8;							// Kind of bytecode
	Uint8 size;									// Size of bytecode in bytes
												//   (1 for bcTableSwitch, bcLookupSwitch, and illegal; 2 for wide illegal)
};

extern const BytecodeControlInfo normalBytecodeControlInfos[256];
extern const BytecodeControlInfo wideBytecodeControlInfos[256];
const BytecodeControlInfo &getBytecodeControlInfo(const bytecode *bc);


#ifdef DEBUG_LOG
class ConstantPool;
struct ExceptionItem;

const bytecode *disassembleBytecode(LogModuleObject &f, const bytecode *bc, const bytecode *origin, const ConstantPool *c, int margin);
void disassembleBytecodes(LogModuleObject &f, const bytecode *begin, const bytecode *end, const bytecode *origin, const ConstantPool *c,
						  int margin);
void disassembleExceptions(LogModuleObject &f, Uint32 nExceptionEntries, const ExceptionItem *exceptionEntries, const ConstantPool *c,
						   int margin);
#endif


// --- INLINES ----------------------------------------------------------------


//
// Return the BytecodeControlInfo corresponding to the bytecode instruction that
// starts at the given address.  The kind and size fields of the BytecodeControlInfo
// will be correct except for the tableswitch and lookupswitch instructions, for
// which the size will be 1.
//
inline const BytecodeControlInfo &getBytecodeControlInfo(const bytecode *bc)
{
	bytecode b = bc[0];
	if (b != bcWide)
		return normalBytecodeControlInfos[b];
	else
		return wideBytecodeControlInfos[bc[1]];
}

#endif
