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
// PPCInstructions.h
//
// Scott M. Silver
// Peter Desantis

#ifndef _H_PPCINSTRUCTIONS
#define _H_PPCINSTRUCTIONS

#include "prtypes.h"
#include "Instruction.h"
#include "ControlNodes.h"

typedef Uint8 PPCInsnFlags;
enum 
{
	pfNil 		= 0x00,		// nothing special
	pfRc 		= 0x01,		// sets a condition code
	pfOE 		= 0x02		// set the overflow flag 
};

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊPPCInstructionXY ¥
#endif

// PPCInstructionXY
//
// Base class for all PPC instructions
class  PPCInstructionXY :
	public InsnUseXDefineYFromPool
{
public:
	inline			PPCInstructionXY(DataNode* inPrimitive, Pool& inPool, Uint8 inX, Uint8 inY) : 
						InsnUseXDefineYFromPool(inPrimitive, inPool, inX, inY) { }
	virtual size_t	getFormattedSize(MdFormatter& /*inFormatter*/) { return (4); }		
};


// Fixed point color to register map.
const Uint8 sFixedPointRegisterMap[] =
{
	3,	4,	5,	6,	7,	8,	9,	10,	11,	12,	31, // -> first non-volatile
	30,	29,	28,	27,	26,	25,	24,	23,	22,	21,	20,
	19,	18,	17,	16,	15,	14	//	13	// -> last non-volatile, but used as globals
};

const Uint8 sFloatingPointRegisterMap[] =
{
	0,	1,	2,	3,	4,	5,	6,	7,	8,	9,	10, 
	11,	12,	13,	// -> first non-volatile
	14,	15,	16,	17,	18,	19,	20,	21,	22, 23, 24,
	25,	26,	27,	28,	29,	30, 31					// -> last non-volatile
};

// udToRegisterNumber
//
// Map colors of registers to real register numbers
inline Uint8 
udToRegisterNumber(InstructionUseOrDefine& inUse)
{
	Uint32 color = inUse.getVirtualRegister().colorInfo.color;	
	if (inUse.getVirtualRegister().getClass() == vrcInteger)
	{
		if (color < sizeof(sFixedPointRegisterMap) / sizeof(Uint8))
			return (sFixedPointRegisterMap[color]);
#ifdef DEBUG
		else
			return 255;
#endif		
	}
	else if (inUse.getVirtualRegister().getClass() == vrcFloatingPoint)
	{
		if (color < sizeof(sFloatingPointRegisterMap) / sizeof(Uint8))
			return (sFloatingPointRegisterMap[color]);
#ifdef DEBUG
		else
			return 255;
#endif	
	}
	else
		assert(false);
	return 255;		// never reached
}

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊD-Form ¥
#endif

#define MAKE_DFORM_ENUM(inName) 	df##inName,
	
#define DFORM_ARITH_DEFINE(inName, inOpcode)	MAKE_DFORM_ENUM(inName)
#define DFORM_ARITHCC_DEFINE(inName, inOpcode)	MAKE_DFORM_ENUM(inName)
#define DFORM_LOAD_DEFINE(inName, inOpcode)		MAKE_DFORM_ENUM(inName)
#define DFORM_LOADU_DEFINE(inName, inOpcode)	MAKE_DFORM_ENUM(inName)
#define DFORM_STORE_DEFINE(inName, inOpcode)	MAKE_DFORM_ENUM(inName)
#define DFORM_STOREU_DEFINE(inName, inOpcode)	MAKE_DFORM_ENUM(inName)
#define DFORM_TRAP_DEFINE(inName, inOpcode)		MAKE_DFORM_ENUM(inName)
#define DO_DFORM
enum DFormKind
{
	#include "PPCInstructionTemplates.h"
	dfLast
};

#undef DFORM_ARITH_DEFINE
#undef DFORM_ARITHCC_DEFINE
#undef DFORM_LOAD_DEFINE
#undef DFORM_LOADU_DEFINE
#undef DFORM_STORE_DEFINE
#undef DFORM_STOREU_DEFINE
#undef DFORM_TRAP_DEFINE

struct DFormInstructionInfo
{
	Uint16	opcode;
	char*	formatStr;
};

#define MAKE_DFORM_INFO(inOpcode, inString) \
	{ inOpcode, inString },
	
#define DFORM_ARITH_DEFINE(inName, inOpcode)	MAKE_DFORM_INFO(inOpcode, #inName" r%d, r%d, %d")
#define DFORM_ARITHCC_DEFINE(inName, inOpcode)	MAKE_DFORM_INFO(inOpcode, #inName" r%d, r%d, %d")
#define DFORM_LOAD_DEFINE(inName, inOpcode)		MAKE_DFORM_INFO(inOpcode, #inName" r%d, %d(r%d)")
#define DFORM_LOADU_DEFINE(inName, inOpcode)	MAKE_DFORM_INFO(inOpcode, #inName" r%d, %d(r%d)")
#define DFORM_STORE_DEFINE(inName, inOpcode)	MAKE_DFORM_INFO(inOpcode, #inName" r%d, %d(r%d)")
#define DFORM_STOREU_DEFINE(inName, inOpcode)	MAKE_DFORM_INFO(inOpcode, #inName" r%d, %d(r%d)")
#define DFORM_TRAP_DEFINE(inName, inOpcode)		MAKE_DFORM_INFO(inOpcode, #inName"%s r%d, %d")

const DFormInstructionInfo sDFormInfos[] =
{
	#include "PPCInstructionTemplates.h"
};
#undef DO_DFORM





class DFormXY :
	public PPCInstructionXY
{
public:
	inline			DFormXY(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Uint8 inX, Uint8 inY) : 
						PPCInstructionXY(inPrimitive, inPool, inX, inY),
						mKind(inKind) { }
	virtual void	formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/);

protected:
			Uint8 	getOPCD() { return (sDFormInfos[mKind].opcode); }
	virtual Uint8 	getD() { return (udToRegisterNumber(getInstructionDefineBegin()[0])); }
	virtual Uint8 	getA() { return (udToRegisterNumber(getInstructionUseBegin()[0])); }
	virtual Uint16	getIMM() = 0;

	const Uint16	mKind;
	
#ifdef DEBUG
public:
  virtual void printPretty(FILE* f) {fprintf(f, sDFormInfos[mKind].formatStr, getD(), getA(), getIMM());}		
#endif
};

class DFormXYImmediate :
	public DFormXY
{
public:
	inline		DFormXYImmediate(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Uint8 inX, Uint8 inY, Uint16 inImmediate) :
					mImmediate(inImmediate),
					DFormXY(inPrimitive, inPool, inKind, inX, inY)  { }
	
	virtual Uint16	getIMM() { return mImmediate; }
	
	Uint16 mImmediate;		// immediate value	
};



template<Uint8 tUses, Uint8 tDefines, Uint8 tAOffset, Uint8 tDOffset>
class LdD : 
	public DFormXYImmediate
{
public:
	inline			LdD(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Uint16 inImmediate) :
						DFormXYImmediate(inPrimitive, inPool, inKind, tUses, tDefines, inImmediate) { }
						
	virtual Uint8 	getA() { return (udToRegisterNumber(getInstructionUseBegin()[tAOffset])); }
 	virtual Uint8 	getD() { return (udToRegisterNumber(getInstructionDefineBegin()[tDOffset])); }
};


template<Uint8 tFixedSourceRegister>
class LdD_FixedSource :
	public LdD<0, 1, 0, 0>
{
public:
	inline 			LdD_FixedSource(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Uint16 inImmediate) :
						LdD<0, 1, 0, 0>(inPrimitive, inPool, inKind, inImmediate) { assert (tFixedSourceRegister < 32); }
	
	virtual Uint8 	getA() { return (tFixedSourceRegister); }
};

// the formatToMemory routine does a fix-up
// based on the real toc offset
class LdD_RTOC :
	public LdD_FixedSource<2>
{
public:
	inline 			LdD_RTOC(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Uint16 inImmediate) :
						LdD_FixedSource<2>(inPrimitive, inPool, inKind, inImmediate) { }
	
	virtual void	formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& inFormatter);
};

typedef LdD<2, 1, 1, 0> LdD_;		// Rvalue | mem Raddress
typedef LdD<2, 2, 1, 1> LdD_V;		// mem Rvalue | mem Raddress

// Rvalue | Sstackslot
class LdD_SpillRegister :
	public LdD<1, 1, 0, 0>
{
public:
	inline 			LdD_SpillRegister(DataNode* inPrimitive, Pool& inPool, VirtualRegister& inStackSlot, VirtualRegister& inDefine) :
						LdD<1, 1, 0, 0>(inPrimitive, inPool, dfLwz, 0) { addUse(0, inStackSlot); addDefine(0, inDefine); }

	// FIX-ME what about 64 bit fregs.
	virtual Uint16 	getIMM() { return ((Uint16)(getInstructionUseBegin()[0].getVirtualRegister().getColor() * -4) - 4); }
	virtual Uint8 	getA() { return (1); }
};

// --

// mem | mem Raddress Rvalue
class StD : 
	public DFormXYImmediate
{
public:
	inline			StD(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Int16 inImmediate, Uint8 inUses = 3, Uint8 inDefines = 1) :
						DFormXYImmediate(inPrimitive, inPool, inKind, inUses, inDefines, inImmediate) { }
						
	virtual Uint8 	getA() { return (udToRegisterNumber(getInstructionUseBegin()[1])); }
 	virtual Uint8 	getD() { return (udToRegisterNumber(getInstructionUseBegin()[2])); }

#ifdef DEBUG
public:
  virtual void printPretty(FILE* f) {fprintf(f, sDFormInfos[mKind].formatStr, getD(), getA(), getIMM());}		
#endif
};

// Sstackslot | Rvalue
class StD_SpillRegister :
	public StD
{
public:
	inline 			StD_SpillRegister(DataNode* inPrimitive, Pool& inPool, VirtualRegister& inStackSlot, VirtualRegister& inUse) :
						StD(inPrimitive, inPool, dfStw, 0, 1, 1) { addDefine(0, inStackSlot); addUse(0, inUse); }

	virtual Uint16 	getIMM() { return ((Uint16)(getInstructionUseBegin()[0].getVirtualRegister().getColor() * -4) - 4); }
	virtual Uint8 	getA() { return (1); }
 	virtual Uint8 	getD() { return (udToRegisterNumber(getInstructionUseBegin()[0])); }
};

// mem | mem value
template<Uint8 tFixedDestRegister>
class StD_FixedDestRegister :
	public StD
{
public:
	inline 			StD_FixedDestRegister(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Uint16 inImmediate) :
						StD(inPrimitive, inPool, inKind, inImmediate, 2, 1) { }

	virtual Uint8 	getA() { return (tFixedDestRegister); }
 	virtual Uint8 	getD() { return (udToRegisterNumber(getInstructionUseBegin()[1])); }
};

class ArithID :
	public DFormXYImmediate
{
public:
	inline						ArithID(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Int16 inImmediate) : 
									DFormXYImmediate(inPrimitive, inPool, inKind, 1, 1, inImmediate) { }

	virtual InstructionFlags	getFlags() const { return ((mImmediate == 0 && mKind == dfAddi) ? ifCopy : ifNone); }
};

class LogicalID :
	public DFormXYImmediate
{
public:
	inline						LogicalID(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Int16 inImmediate) : 
									DFormXYImmediate(inPrimitive, inPool, inKind, 1, 1, inImmediate) { }

	Uint8						getA() { return (udToRegisterNumber(getInstructionDefineBegin()[0])); }
	Uint8 						getD() {  return (udToRegisterNumber(getInstructionUseBegin()[0])); }
	virtual InstructionFlags	getFlags() const { return ((mImmediate == 0 && mKind == dfAddi) ? ifCopy : ifNone); }
};


class ArithIZeroInputD :
	public DFormXYImmediate
{
public:
	inline			ArithIZeroInputD(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Uint16 inImmediate, Uint8 inFakeUses = 0) : 
						DFormXYImmediate(inPrimitive, inPool, inKind, inFakeUses, 1, inImmediate) { }
	
	Uint8 			getA() { return (0); }
};

class LogicalIZeroInputD :
	public DFormXYImmediate
{
public:
	inline			LogicalIZeroInputD(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Uint16 inImmediate, Uint8 inFakeUses = 0) : 
						DFormXYImmediate(inPrimitive, inPool, inKind, inFakeUses, 1, inImmediate) { }
	
	Uint8			getA() { return (udToRegisterNumber(getInstructionDefineBegin()[0])); }
	Uint8 			getD() { return (0); }
};


class Copy_I :
	public ArithID
{
public:
	inline			Copy_I(DataNode* inPrimitive, Pool& inPool) :
						ArithID(inPrimitive, inPool, dfAddi, 0) { }
};


class ArithIDSetCC :
	public DFormXYImmediate
{
public:
	inline			ArithIDSetCC(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Uint16 inImmediate) : 
						DFormXYImmediate(inPrimitive, inPool, inKind, 2, 2, inImmediate) {  }
};


class CmpID :
	public DFormXYImmediate
{
public:
	inline			CmpID(DataNode* inPrimitive, Pool& inPool, DFormKind inKind, Uint16 inImmediate) : 
						DFormXYImmediate(inPrimitive, inPool, inKind, 1, 1, inImmediate) { }

protected:
	virtual Uint8	getD() { return (0); }	// always use cr0, and L=0 

#ifdef DEBUG
public:
	virtual void 	printPretty(FILE* f) { fprintf(f, sDFormInfos[mKind].formatStr, getD() >> 2, getA(), getIMM()); }		
#endif
};

// the bits in order (or offsets into a condition register)
// neg (lt)
// pos (gt)
// zero (eq)
// summary overflow (so)
// 0010y branch if cond is false
// 0110y branch if cond is true
struct TrapCondition
{
	char*	name;
	Uint8	to;
};

// indexed by Condition2
static TrapCondition sSignedTrapConditions[] =
{
	{"",	31},
	{"lt",	16},
	{"eq",	4},
	{"le",	20},
	{"gt",	8},
	{"ne",	24},	
	{"ge",	12}
};

// indexed by Condition2
static TrapCondition sUnsignedTrapConditions[]  =
{
	{"",	31},
	{"llt",	2},
	{"eq",	4},
	{"lle",	6},
	{"lgt",	1},
	{"ne", 24},	
	{"lge",	5}
};

class TrapD :
	public DFormXYImmediate
{
public:
	inline			TrapD(DataNode* inPrimitive, Pool& inPool, Condition2 inCondition, bool inSigned, Uint16 inImmediate) : 
						DFormXYImmediate(inPrimitive, inPool, dfTwi, 1, 0, inImmediate), 
						mSigned(inSigned), 
						mCond(inCondition) { }

protected:
	virtual Uint8 	getD() { return (mSigned ? sSignedTrapConditions[mCond].to :  sUnsignedTrapConditions[mCond].to); }
	virtual Uint8 	getA() { return (udToRegisterNumber(getInstructionUseBegin()[0])); }
	
	const bool		mSigned;
	const Condition2 	mCond;

#ifdef DEBUG
public:
	virtual void 	printPretty(FILE* f) { fprintf(f, sDFormInfos[mKind].formatStr, mSigned ? sSignedTrapConditions[mCond].name :  sUnsignedTrapConditions[mCond].name, getA(), getIMM()); }		
#endif
};

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊX & XO-Form ¥
#endif


#define MAKE_XFORM_ENUM(inName) 	xf##inName,
	
#define XFORM_ARITH_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_ENUM(inName)
#define XFORM_INAONLY_DEFINE(inName, inOpcode, inPrimary)	MAKE_XFORM_ENUM(inName)
#define XFORM_CMP_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_ENUM(inName)
#define XFORM_LOAD_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_ENUM(inName)
#define XFORM_LOADU_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_ENUM(inName)
#define XFORM_STORE_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_ENUM(inName)
#define XFORM_STOREU_DEFINE(inName, inOpcode, inPrimary)	MAKE_XFORM_ENUM(inName)
#define XFORM_TRAP_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_ENUM(inName)
#define XFORM_INBONLY_DEFINE(inName, inOpcode, inPrimary)	MAKE_XFORM_ENUM(inName)
#define DO_XFORM
enum XFormKind
{
	#include "PPCInstructionTemplates.h"
	xfLast
};

#undef XFORM_ARITH_DEFINE
#undef XFORM_INAONLY_DEFINE
#undef XFORM_CMP_DEFINE
#undef XFORM_LOAD_DEFINE
#undef XFORM_LOADU_DEFINE
#undef XFORM_STORE_DEFINE
#undef XFORM_STOREU_DEFINE
#undef XFORM_TRAP_DEFINE
#undef XFORM_INBONLY_DEFINE

struct XFormInstructionInfo
{
	Uint16	opcode;
	Uint16	primary;
	char*	formatStr;
};

#define MAKE_XFORM_INFO(inOpcode, inPrimary, inString) \
	{ inOpcode, inPrimary, inString },
	
#define XFORM_ARITH_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_INFO(inOpcode, inPrimary, #inName" r%d, r%d, r%d")
#define XFORM_INAONLY_DEFINE(inName, inOpcode, inPrimary)	MAKE_XFORM_INFO(inOpcode, inPrimary, #inName" r%d, r%d, r%d")
#define XFORM_CMP_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_INFO(inOpcode, inPrimary, #inName" r%d, r%d, r%d")
#define XFORM_LOAD_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_INFO(inOpcode, inPrimary, #inName" r%d, r%d, r%d")
#define XFORM_LOADU_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_INFO(inOpcode, inPrimary, #inName" r%d, r%d, r%d")
#define XFORM_STORE_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_INFO(inOpcode, inPrimary, #inName" r%d, r%d, r%d")
#define XFORM_STOREU_DEFINE(inName, inOpcode, inPrimary)	MAKE_XFORM_INFO(inOpcode, inPrimary, #inName" r%d, r%d, r%d")
#define XFORM_TRAP_DEFINE(inName, inOpcode, inPrimary)		MAKE_XFORM_INFO(inOpcode, inPrimary, #inName" r%d, r%d, r%d")
#define XFORM_INBONLY_DEFINE(inName, inOpcode, inPrimary)	MAKE_XFORM_INFO(inOpcode, inPrimary, #inName" r%d, r%d, r%d")

const XFormInstructionInfo sXFormInfos[] =
{
	#include "PPCInstructionTemplates.h"
};
#undef DO_XFORM

static char*
sFlagsExtensionTbl[] =
{
	"",		// none
	".",	// pfRc 
	"o",	// pfOE
	"o."	// pfRC | afOE
};


class XFormXY :
	public PPCInstructionXY
{
public:
	inline			XFormXY(DataNode* inPrimitive, Pool& inPool, Uint8 inX, Uint8 inY, XFormKind inKind, PPCInsnFlags inFlags) : 
						PPCInstructionXY(inPrimitive, inPool, inX, inY),
						mFlags(inFlags),
						mKind(inKind) { }
	virtual void	formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/);

protected:
	virtual Uint8	getOPCD() { return (31); }
	virtual Uint8 	getD() { return (udToRegisterNumber(getInstructionDefineBegin()[0])); }
	virtual Uint8 	getA() { return (udToRegisterNumber(getInstructionUseBegin()[0]));  }
	virtual Uint8 	getB() { return (udToRegisterNumber(getInstructionUseBegin()[1])); }
	virtual Uint16 	getXO() { return (sXFormInfos[mKind].opcode); }

	PPCInsnFlags	mFlags;
	XFormKind		mKind;
	
#ifdef DEBUG
public:
	virtual void	printPretty(FILE* f) { fprintf(f, sXFormInfos[mKind].formatStr, flagsToExtension(), getD(), getA(), getB()); }
	const char*		flagsToExtension() { return sFlagsExtensionTbl[mFlags & 3]; }	// only look at OE and Rc bits
#endif
};


class TrapX :
	public XFormXY
{
public:
	inline			TrapX(DataNode* inPrimitive, Pool& inPool, Condition2 inCondition, bool inSigned, PPCInsnFlags inFlags) : 
						XFormXY(inPrimitive, inPool, 2, 0, xfTw, inFlags), 
						mSigned(inSigned), 
						mCond(inCondition) { }

protected:
	virtual Uint8 getD() { return (mSigned ? sSignedTrapConditions[mCond].to :  sUnsignedTrapConditions[mCond].to); }
	
#ifdef DEBUG
public:
	virtual void 	printPretty(FILE* f) { fprintf(f, sXFormInfos[mKind].formatStr, mSigned ? sSignedTrapConditions[mCond].name :  sUnsignedTrapConditions[mCond].name, getA(), getB()); }		
#endif

protected:
	bool		mSigned;
	Condition2 	mCond;
};

template<Uint8 inX, Uint8 inY, Uint8 inD, Uint8 inA, Uint8 inB>
class LdX :
	public XFormXY
{
public:
	inline			LdX(DataNode* inPrimitive, Pool& inPool, XFormKind inKind) :
						XFormXY(inPrimitive, inPool, inX, inY, inKind, pfNil) { assert(inKind >= xfLbzx && inKind <= xfLwzx); }

	virtual Uint8 	getD() { return (udToRegisterNumber(getInstructionDefineBegin()[inD])); }
	virtual Uint8 	getA() { return (udToRegisterNumber(getInstructionUseBegin()[inA]));  }
	virtual Uint8 	getB() { return (udToRegisterNumber(getInstructionUseBegin()[inB])); }
};

typedef LdX<2, 1, 0, 0, 1> LdX_C;		// Rvalue <- Raddress Radddress
typedef LdX<3, 2, 1, 1, 2> LdX_V;		// mem Rvalue Raddress <- mem Raddress Raddress
typedef LdX<3, 1, 0, 1, 2> LdX_;		// Rvalue <- mem Raddress Raddress


// mem <- mem Raddress Raddress Value
class StX :
	public XFormXY
{
public:
	inline			StX(DataNode* inPrimitive, Pool& inPool, XFormKind inKind) :
						XFormXY(inPrimitive, inPool, 4, 1, inKind, pfNil) { assert(inKind >= xfStbx && inKind <= xfStwx); }

	virtual Uint8 	getD() { return (udToRegisterNumber(getInstructionUseBegin()[3])); }
	virtual Uint8 	getA() { return (udToRegisterNumber(getInstructionUseBegin()[1]));  }
	virtual Uint8 	getB() { return (udToRegisterNumber(getInstructionUseBegin()[2])); }
};	


class ArithX :
	public XFormXY
{
public:
	inline			ArithX(DataNode* inPrimitive, Pool& inPool, XFormKind inKind, PPCInsnFlags inFlags) :
						 XFormXY(inPrimitive, inPool, 2, ((inFlags & pfRc) == 0) ? 1 : 2, inKind, inFlags) { }
};

class XOInAOnly :
	public XFormXY
{
public:
	// if we want to use the lswi, need to deal with memory edge
	inline			XOInAOnly(DataNode* inPrimitive, Pool& inPool, XFormKind inKind, PPCInsnFlags inFlags) :
						XFormXY(inPrimitive, inPool, 1, ((inFlags & pfRc) == 0) ? 1 : 2, inKind, inFlags) {}

protected:
	virtual Uint8	getA() { return 0; }
};

class XInAOnly :
	public XFormXY
{
public:
	// if we want to use the lswi, need to deal with memory edge
	inline			XInAOnly(DataNode* inPrimitive, Pool& inPool, XFormKind inKind, PPCInsnFlags inFlags, Uint8 inB) :
						XFormXY(inPrimitive, inPool, 1, ((inFlags & pfRc) == 0) ? 1 : 2, inKind, (inFlags | pfOE)),
						mB(inB) { }

protected:
	virtual Uint8	getD() { return (XFormXY::getA()); }
	virtual Uint8	getA() { return (XFormXY::getD()); }
	virtual Uint8	getB() { assert (mB < (1 << 6)); return mB; }
	Uint8	mB;
};

class XInBOnly :
	public XFormXY
{
public:
	inline			XInBOnly(DataNode* inPrimitive, Pool& inPool, XFormKind inKind, PPCInsnFlags inFlags) :
							 XFormXY(inPrimitive, inPool, 1, ((inFlags & pfRc) == 0) ? 1 : 2, inKind, inFlags) { }

protected:
	virtual Uint8	getA() { return (0); }
	virtual Uint8	getB() { return (udToRegisterNumber(getInstructionUseBegin()[0])); }
	virtual InstructionFlags getFlags() const { return ((mKind == xfFmr) ? ifCopy : ifNone); }
	Uint8	mA;
};

class CmpIX :
	public XFormXY
{
public:
	inline			CmpIX(DataNode* inPrimitive, Pool& inPool, XFormKind inKind, PPCInsnFlags inFlags) :
							XFormXY(inPrimitive, inPool, 2, 1, inKind, inFlags) { }

protected:
	virtual Uint8	getD() { return (0); }	// always use cr0
};

class CmpFX :
	public XFormXY
{
public:
	inline			CmpFX(DataNode* inPrimitive, Pool& inPool, XFormKind inKind, PPCInsnFlags inFlags) :
							XFormXY(inPrimitive, inPool, 2, 1, inKind, inFlags) { }

protected:
	virtual Uint8	getD() { return (0); } // always use cr0
};

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊIForm ¥
#endif

class BranchI :
	public PPCInstructionXY
{
public:
	inline			BranchI(DataNode* inPrimitive, Pool& inPool, ControlNode& inTarget, bool inAbsolute = false, bool inLink = false) : 
						PPCInstructionXY(inPrimitive, inPool, 0, 0), mTarget(inTarget) 
						{ mIForm.OPCD = 18; mIForm.LI = 0; mIForm.AA = inAbsolute; mIForm.LK = inLink;}
						
	virtual void	formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/);
  #ifdef DEBUG
	virtual void	printPretty(FILE* f) {  fprintf(f, "bxx %d", mTarget.getNativeOffset()); }
  #endif

protected:	
	ControlNode&	mTarget;
	
	struct
	{
		int OPCD:6;
		int LI:24;
		int AA:1;
		int LK:1;	
	} mIForm;
};

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊIForm ¥
#endif

// the bits in order (or offsets into a condition register)
// neg (lt)
// pos (gt)
// zero (eq)
// summary overflow (so)
// 0010y branch if cond is false
// 0110y branch if cond is true
struct BranchCondition
{
	char*	name;
	Uint8	bo;
	Uint8	bi;
};

// indexed by Condition2
const BranchCondition sBranchConditions[] =
{
	{"always", 0, 0},
	{"lt",	12, 0},
	{"eq",	12,	2},
	{"le",	4,	1},
	{"gt",	12,	1},
	{"ne",	4,	2},
	{"ge",	4,	0}
};

#if 0
																				BO			BI
	cond0,		// 0000  Always false																
	condLt,		// 0001  arg1 < arg2											0110y		0 lt
	condEq,		// 0010  arg1 = arg2											0110y		2 eq	
	condLe,		// 0011  arg1 <= arg2											0010y		1 not gt
	condGt,		// 0100  arg1 > arg2											0110y		1 gt
	condLgt,	// 0101  arg1 <> arg2											0010y		2 not eq
	condGe,		// 0110  arg1 >= arg2											0010y		0 not lt
	
-- only up to these are supported
	condOrd,	// 0111  arg1 <=> arg2 (i.e. arg1 and arg2 are ordered)			0110y		3 so ?? nan
	condUnord,	// 1000  arg1 ? arg2 (i.e. arg1 and arg2 are unordered)			0010y		3 not so
	condULt,	// 1001  arg1 ?< arg2											do we have to deal with these below here??
	condUEq,	// 1010  arg1 ?= arg2
	condULe,	// 1011  arg1 ?<= arg2
	condUGt,	// 1100  arg1 ?> arg2
	condNe,		// 1101  arg1 != arg2
	condUGe,	// 1110  arg1 ?>= arg2
	cond1		// 1111  Always true
#endif

class BranchCB :
	public PPCInstructionXY
{
public:
					BranchCB(DataNode* inPrimitive, Pool& inPool, ControlNode& inTarget, Condition2 inCondition, bool inAbsolute = false, bool inLink = false);
	virtual void	formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/);

	
protected:	
	ControlNode&	mTarget;
	
	struct
	{
		unsigned int OPCD:6;
		unsigned int BO:5;
//		unsigned int Y:1;
		unsigned int BI:5;
		unsigned int BD:14;
		unsigned int AA:1;
		unsigned int LK:1;	
	} mCForm;
	
#if DEBUG
public:
	virtual void	printPretty(FILE* f) {  fprintf(f, "b%s %d", sBranchConditions[mCond].name, mTarget.getNativeOffset()); }

protected:
	Condition2 mCond;
#endif
};


#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊM-Form ¥
#endif

#define DO_MFORM
struct MFormInstructionInfo
{
	Uint16	opcode;
	char*	formatStr;
};


#define MAKE_MFORM_ENUM(inName) 			mf##inName,
#define MFORM_DEFINE(inName, inOpcode)		MAKE_MFORM_ENUM(inName)

enum MFormKind
{
	#include "PPCInstructionTemplates.h"
	mfLast
};

#undef MFORM_DEFINE

#define MAKE_MFORM_INFO(inName, inOpcode) \
	{ inOpcode, #inName"%s r%d, r%d, r%d, %d, %d"},
	
#define MFORM_DEFINE(inName, inOpcode)		MAKE_MFORM_INFO(inName, inOpcode)

const MFormInstructionInfo sMFormInfos[] =
{
	#include "PPCInstructionTemplates.h"
};

#undef MFORM_DEFINE
#undef DO_MFORM

class MForm :
	public PPCInstructionXY
{
public:
	inline			MForm(DataNode* inPrimitive, Pool& inPool, Uint8 inUses, MFormKind inKind, Uint8 inMaskBegin, Uint8 inMaskEnd, PPCInsnFlags inFlags) :
						PPCInstructionXY(inPrimitive, inPool, inUses, 1 + (bool) (inFlags & pfRc)),
						mFlags(inFlags), 
						mKind(inKind), 
						mMaskBegin(inMaskBegin), 
						mMaskEnd(inMaskEnd) { }
	virtual void	formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/);

	

protected:
	Uint16			getOPCD() { return (sXFormInfos[mKind].opcode); }
	Uint8 			getS() { return (udToRegisterNumber(getInstructionUseBegin()[0])); }
	Uint8 			getA() { return (udToRegisterNumber(getInstructionDefineBegin()[0])); }
	virtual Uint8 	getB() { return (udToRegisterNumber(getInstructionUseBegin()[1])); }
	Uint8 			getMB() { return (mMaskBegin); }
	Uint8   		getME() { return (mMaskEnd); }

#ifdef DEBUG
public:
	virtual void	printPretty(FILE* f) { fprintf(f, sMFormInfos[mKind].formatStr, flagsToExtension(), getA(), getS(), getB(), getMB(), getME()); }
protected:
	const char*		flagsToExtension() { return sFlagsExtensionTbl[mFlags & 1]; /* only look at Rc bit */ }
#endif

	PPCInsnFlags	mFlags;
	MFormKind		mKind;
	Uint8			mMaskBegin;
	Uint8			mMaskEnd;
};


class MFormInAOnly :
	public MForm
{
public:
	inline		MFormInAOnly(DataNode* inPrimitive, Pool& inPool, MFormKind inKind, Uint8 inB, Uint8 inMaskBegin, Uint8 inMaskEnd, PPCInsnFlags inFlags) : 
					MForm(inPrimitive, inPool, 1, inKind, inMaskBegin, inMaskEnd, inFlags),
					mB(inB) { } 
protected:
	virtual Uint8	getB() { return (mB); } 

	Uint8 mB;
};

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊA-Form ¥
#endif


#define DO_AFORM


#define MAKE_AFORM_ENUM(inName) 			af##inName,
#define AFORM_DEFINE(inName, inOpcode, inXo, inHasA, inHasB, inHasC, inRc)		MAKE_AFORM_ENUM(inName)

enum AFormKind
{
	#include "PPCInstructionTemplates.h"
	afLast
};

#undef AFORM_DEFINE

template <bool tHasA, bool tHasB, bool tHasC, bool tRc>
class AForm :
	public PPCInstructionXY
{
public:
	inline			AForm(DataNode* inPrimitive, Pool& inPool, AFormKind inKind) :
						PPCInstructionXY(inPrimitive, inPool, tHasA + tHasB + tHasC, tRc + 1),
						mKind(inKind) {}
	
protected:
	Uint16	getOPCD() { return (sAFormInfos[mKind].opcode); }
	Uint8 	getD() { return (udToRegisterNumber(getInstructionDefineBegin()[0])); }
	Uint8 	getA() { return (tHasA ? udToRegisterNumber(getInstructionUseBegin()[0]) : 0); }
	Uint8 	getB() { return (tHasB ? udToRegisterNumber(getInstructionUseBegin()[tHasA]) : 0); }
	Uint8 	getC() { return (tHasC ? udToRegisterNumber(getInstructionUseBegin()[tHasA + tHasB]) : 0); }

public:
	virtual void 
	formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/)
	{
		struct
		{
			uint OPCD:6;
			uint D:5;
			uint A:5;
			uint B:5;
			uint C:5;
			uint XO:5;
			uint RC:1;
		} aForm;
		
		aForm.OPCD = sAFormInfos[mKind].opcode;
		aForm.D = getD();
		aForm.A = getA();
		aForm.B = getB();
		aForm.C = getC();
		aForm.XO = sAFormInfos[mKind].xo;
		aForm.RC = tRc;
		
		*(Uint32*) inStart = *(Uint32*) &aForm;
	}

	static Instruction& 
	createInstruction(DataNode& inPrimitive, Pool& mPool, AFormKind inKind)
	{
		return *new(mPool) AForm<tHasA, tHasB, tHasC, tRc>(&inPrimitive, mPool, inKind);
	}

protected:
	const AFormKind	mKind;
#ifdef DEBUG
public:
	virtual void	printPretty(FILE* /*f*/) {}
protected:
	const char*		flagsToExtension() { return sFlagsExtensionTbl[tRc];  } // only look at Rc bit 
#endif
};

struct AFormInstructionInfo
{
	Uint16			opcode;
	Uint16			xo;
	Instruction& 	(*creatorFunction)(DataNode&, Pool&, AFormKind);
	char*			formatStr;
};

#define MAKE_AFORM_INFO(inName, inOpcode, inXo, inHasA, inHasB, inHasC, inRc) \
	{ inOpcode, inXo, AForm<inHasA, inHasB, inHasC, inRc>::createInstruction, #inName"%s r%d, r%d, r%d, %d, %d"},
	
#define AFORM_DEFINE(inName, inOpcode, inXo, inHasA, inHasB, inHasC, inRc)		MAKE_AFORM_INFO(inName, inOpcode, inXo, inHasA, inHasB, inHasC, inRc)

const AFormInstructionInfo sAFormInfos[] =
{
	#include "PPCInstructionTemplates.h"
};

#undef AFORM_DEFINE
#undef DO_AFORM

#endif // _H_PPCINSTRUCTIONS
