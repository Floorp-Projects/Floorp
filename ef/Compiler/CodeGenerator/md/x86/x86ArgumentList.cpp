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
//
//	x86ArgumentList.cpp
//
//	Peter DeSantis
//	Simon Holmes a Court

#include "x86ArgumentList.h"
#include "x86Opcode.h"
#include "x86Win32Instruction.h"
#include "MemoryAccess.h"
#include "x86Formatter.h"

UT_DEFINE_LOG_MODULE(x86ArgList);

// Maps register alloc color to x86 machine register
// Order matters.
x86GPR colorTox86GPR[] =
{
	EAX,
	EBX,
	ECX,
	EDX, 
	ESI,
	EDI,
	/*ESP,
	EBP*/
};

// Maps x86 machine register to register alloc color
// Order matters.
Uint8 x86GPRToColor[] =
{
	0,	// EAX
	2,	// ECX
	3,	// EDX
	1,	// EBX
	6,	// ESP
	7,	// EBP
	4,	// ESI
	5,	// EDI
};

// Text labels listed by processor register numbers
// Order matters.
char* x86GPRText[] =
{
	"eax",
	"ecx",
	"edx",
	"ebx",
	"esp",
	"ebp",
	"esi",
	"edi"
};

/*--------------------------------------------------------------------------------
x86ArgumentType 
	struct x86ArgumentTypeInfo
	{
		Uint8		atiSize;			// number of bytes needed to format
		Uint8		atiNumUses;			// number of uses consumed
		SIBScale	atiScale;			// scale factor
		TypeClass	atTypeClass;		// can the type be passed to the constructor?
		char*		atiPrintInfo;		// debug formatting string		// FIX shouldn't be in release build
	}
atRegisterIndirect, atBaseIndexed, atScaledIndexedBy2, atScaledIndexedBy4, atScaledIndexedBy8, atStackOffset can
only be specified without displacements.  The constructor promotes these types based on the size of the
displacement.  The constructors depends on the ordering of these. 
*/

enum SIBScale
{
	kScaleBy1		= 0x00,
	kScaleBy2		= 0x40,
	kScaleBy4		= 0x80,
	kScaleBy8		= 0xc0,
	kScaleIllegal	= 0xff	// not a legal SIB value -- used only for debugging
};

enum TypeClass
{
	kCannotHaveDisp,// can be passed in as a parameter
	kMustHaveDisp,	// as per kPrimaryArg, but _must_ have an extra arg (displacement or immediate)
	kMayHaveDisp,	// as per kPrimaryArg, but _may_ have an extra arg (displacement or immediate)
	kDerived		// cannot be passed in with a parameter
};

// Each x86ArgumentType has a x86ArgumentTypeInfo defined for it in atInfo.
struct x86ArgumentTypeInfo
{
	Uint8		atiSize;			// number of bytes needed to format
	Uint8		atiNumUses;			// number of uses consumed
	SIBScale	atiScale;			// scale factor
	TypeClass	atTypeClass;		// see TypeClass
	char*		atiPrintInfo;		// debug formatting string		// FIX shouldn't be in release build
}; 

x86ArgumentTypeInfo atInfo[] =
{
	{1, 1, kScaleIllegal,	kCannotHaveDisp,"%s"},				// atRegDirect
	{5, 1, kScaleIllegal,	kCannotHaveDisp,"%d[%s]"},			// atRegAllocStackSlot
    {5, 1, kScaleIllegal,	kCannotHaveDisp,"%d[%s]"},			// atRegAllocStackSlotHi32
	
	{1,	1, kScaleIllegal,	kMayHaveDisp,	"[%s]"},			// atRegisterIndirect
	{2, 1, kScaleIllegal,	kDerived,		"%d[%s]"},			// atRegisterRelative1ByteDisplacement
	{5, 1, kScaleIllegal,	kDerived,		"%d[%s]"},			// atRegisterRelative4ByteDisplacement
	
	{2, 2, kScaleBy1,		kMayHaveDisp,	"[%s+%s]"},			// atBaseIndexed
	{3, 2, kScaleBy1,		kDerived,		"%d[%s+%s]"},		// atBaseRelativeIndexed1ByteDisplacement
	{6, 2, kScaleBy1,		kDerived,		"%d[%s+%s]"},		// atBaseRelativeIndexed4ByteDisplacement
	
	{2, 2, kScaleBy2,		kMayHaveDisp,	"[%s+2*%s]"},		// atScaledIndexedBy2
	{3, 2, kScaleBy2,		kDerived,		"%d[%s+2*%s]"},		// atScaledIndexed1ByteDisplacementBy2
	{6, 2, kScaleBy2,		kDerived,		"%d[%s+2*%s]"},		// atBaseRelativeIndexed4ByteDisplacementBy2
	
	{2, 2, kScaleBy4,		kMayHaveDisp,	"[%s+4*%s]"},		// atScaledIndexedBy4
	{3, 2, kScaleBy4,		kDerived,		"%d[%s+4*%s]"},		// atScaledIndexed1ByteDisplacementBy4
	{6, 2, kScaleBy4,		kDerived,		"%d[%s+4*%s]"},		// atScaledIndexed4ByteDisplacementBy4
	
	{2, 2, kScaleBy8,		kMayHaveDisp,	"[%s+8*%s]"},		// atScaledIndexedBy8
	{3, 2, kScaleBy8,		kDerived,		"%d[%s+8*%s]"},		// atScaledIndexed1ByteDisplacementBy8
	{6, 2, kScaleBy8,		kDerived,		"%d[%s+8*%s]"},		// atScaledIndexed4ByteDisplacementBy8

	{0, 0, kScaleIllegal,	kMayHaveDisp,	"NOT USED"},		// atStackOffset
	{2, 0, kScaleIllegal,	kDerived,		"%d[EBP]"},			// atStackOffset1ByteDisplacement
	{5, 0, kScaleIllegal,	kDerived,		"%d[EBP]"},			// atStackOffset4ByteDisplacement

    {6, 0, kScaleIllegal,	kMustHaveDisp,	"[0x%08x]"},		// atAbsoluteAddress
	{0, 0, kScaleIllegal,	kCannotHaveDisp,"%d"}				// atImmediateOnly
};

//--------------------------------------------------------------------------------
// SingleArgumentList

// Method:	alGetRegisterOperand
// Purpose:	extract the register or registers that are the components of the current operand
// Used by:	X86Opcode_Reg_Condensable when it is trying to append register operands into the opcode
// Input:	instruction
// Output:	returns the register number (if SingleArgumentList and ImmediateArgumentList)
//			or NOT_A_REG (otherwise)
Uint8 x86SingleArgumentList::				
alGetRegisterOperand(x86ArgListInstruction& inInsn)
{ 
	if(argType1 == atRegDirect)
	{
		InstructionUse* useBegin = &(inInsn.getInstructionUseBegin()[0]);
		InstructionUse*	useEnd = inInsn.getInstructionUseEnd();
		
		if((useBegin < useEnd) && useBegin->isVirtualRegister())
			return useToRegisterNumber(*useBegin);
		
		InstructionDefine* defineBegin = inInsn.getInstructionDefineBegin();
		InstructionDefine*	defineEnd = inInsn.getInstructionDefineEnd();
		assert((defineBegin < defineEnd) && (defineBegin)->isVirtualRegister()) ; 
		return defineToRegisterNumber(*defineBegin);
	}
	// Must be memory
	return NOT_A_REG; 
}

// Method:	extractOperand
// Purpose:	extract the register or registers that are the components of the current operand
// Input:	instruction
//			location of the register encodings
// Output:	one or two registers (expressed as x86 register numbers)
//			reg  = either InstructionUses[inInstructionUseStartIndex] or the first InstructionDefine
//			reg2 = is filled iff the use or define after reg is a valid register
inline void extractOperand(x86ArgListInstruction& inInsn, Uint8 inInstructionUseStartIndex, Uint8& reg, Uint8& reg2)
{

	InstructionUse* useBegin = &(inInsn.getInstructionUseBegin()[inInstructionUseStartIndex]);
	InstructionUse*	useEnd = inInsn.getInstructionUseEnd();
	
	if((useBegin < useEnd) && useBegin->isVirtualRegister()) 
	{
		reg = useToRegisterNumber(*useBegin);
		if((useBegin + 1 < useEnd) && (useBegin +1)->isVirtualRegister())
			reg2 = useToRegisterNumber(*(useBegin+1));
		else
			reg2 = NOT_A_REG;
	} 
	else 
	{  // This means that the virtual registers must be the first of the defines
		InstructionDefine* defineBegin = inInsn.getInstructionDefineBegin();
		InstructionDefine*	defineEnd = inInsn.getInstructionDefineEnd();
		assert((defineBegin < defineEnd) && (defineBegin)->isVirtualRegister()) ; 
		reg = defineToRegisterNumber(*defineBegin);
		if ((defineBegin + 1 < defineEnd) && (defineBegin + 1)->isVirtualRegister())
			reg2 = defineToRegisterNumber(*(defineBegin+1));
		else
			reg2 = NOT_A_REG;
	}
}

#ifdef DEBUG_LOG

// Method:	alPrintPretty
// Purpose:	basic print pretty utility function for all argument lists which have one or more reg/mem operands
// Input:	LogModuleObject
//			x86ArgListInstruction to be printed
//			type of argument
//			location of the register encodings
// Output:	prints to LogModuleObject
void x86SingleArgumentList::
alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn, x86ArgumentType inType, Uint8 inInstructionUseStartIndex )
{	
	switch (inType) 
	{
	case atImmediateOnly:
        return;

	case atAbsoluteAddress:
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (atInfo[inType].atiPrintInfo, arg4ByteDisplacement));
		return;
	
	case atRegAllocStackSlot:
		{
		InstructionUse* useBegin = &(inInsn.getInstructionUseBegin()[inInstructionUseStartIndex]);
		InstructionUse*	useEnd = inInsn.getInstructionUseEnd();	

		if((useBegin < useEnd) && useBegin->isVirtualRegister())		// Stack slot is the use
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("slot%d", useBegin->getVirtualRegister().getColor()));
		else															// Stack slot is the define
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("slot%d", inInsn.getInstructionDefineBegin()->getVirtualRegister().getColor() ));	
		}
	  return;

    case atRegAllocStackSlotHi32:
        {
            InstructionUse* useBegin = &(inInsn.getInstructionUseBegin()[inInstructionUseStartIndex]);
            InstructionUse*	useEnd = inInsn.getInstructionUseEnd();	
            
            if((useBegin < useEnd) && useBegin->isVirtualRegister())		// Stack slot is the use
                UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Hi32(slot%d)", useBegin->getVirtualRegister().getColor()));
            else															// Stack slot is the define
                UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Hi32(slot%d)", inInsn.getInstructionDefineBegin()->getVirtualRegister().getColor() ));	
        }
        return;

	case atStackOffset1ByteDisplacement:
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (atInfo[inType].atiPrintInfo, arg1ByteDisplacement));
		return;

	case atStackOffset4ByteDisplacement:
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (atInfo[inType].atiPrintInfo, arg4ByteDisplacement));
		return;
	default:
		0;	// do nothing -- fall through (is there a better way to make gcc happy?)
	}

	// registers are needed for remaining types
	Uint8 reg, reg2;
	extractOperand(inInsn, inInstructionUseStartIndex, reg, reg2);
		
	switch (inType) 
	{
		case (atRegDirect):
		case (atRegisterIndirect): 
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (atInfo[inType].atiPrintInfo,  x86GPRText[reg] ));
			break;

		case (atRegisterRelative1ByteDisplacement):
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (atInfo[inType].atiPrintInfo, arg1ByteDisplacement, x86GPRText[reg] ));
			break;

		case (atRegisterRelative4ByteDisplacement):
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (atInfo[inType].atiPrintInfo, arg4ByteDisplacement, x86GPRText[reg] ));
			break;

		case(atBaseIndexed):
		case(atScaledIndexedBy2):
		case(atScaledIndexedBy4): 
		case(atScaledIndexedBy8):
			assert(reg2 != NOT_A_REG);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (atInfo[inType].atiPrintInfo, x86GPRText[reg], x86GPRText[reg2] ));
			break;
			
		case(atBaseRelativeIndexed1ByteDisplacement):
		case(atScaledIndexed1ByteDisplacementBy2):
		case(atScaledIndexed1ByteDisplacementBy4):
		case(atScaledIndexed1ByteDisplacementBy8):
			assert(reg2 != NOT_A_REG);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (atInfo[inType].atiPrintInfo, arg1ByteDisplacement, x86GPRText[reg], x86GPRText[reg2] ));
			break;
 
		default:
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (atInfo[inType].atiPrintInfo, arg4ByteDisplacement, x86GPRText[reg], x86GPRText[reg2] ));
			break;
	}
}

#endif // DEBUG_LOG																					

// Method:	alSize
// Purpose:	calculate the size of the argument list
// Input:	type of argument
//			instruction
//			location of the register encodings
// Output:	number of bytes of space needed to encode argument
Uint8 x86SingleArgumentList::
alSize(x86ArgumentType inType, x86ArgListInstruction& inInsn, Uint8 inInstructionUseStartIndex, MdFormatter& inFormatter)
{
	switch (inType) 
    {
	case atRegisterIndirect:
        {      
            Uint8 reg, reg2;
            extractOperand(inInsn, inInstructionUseStartIndex, reg, reg2);
            if ((reg == EBP)) 
                return 2;
            return atInfo[inType].atiSize;
        }
		break;

    case atRegAllocStackSlot:
    case atRegAllocStackSlotHi32:
		{
		InstructionUseOrDefine* uord = NULL;
	    InstructionUse* useBegin = &inInsn.getInstructionUseBegin()[inInstructionUseStartIndex];
	    InstructionUse*	useEnd = inInsn.getInstructionUseEnd();
        if ((useBegin < useEnd) && useBegin->isVirtualRegister())
			uord = useBegin;
        else
			uord = inInsn.getInstructionDefineBegin();

		Uint32 unsizedDisplacement = inFormatter.getStackSlotOffset(uord->getVirtualRegister());
		if(valueIsOneByteSigned(unsizedDisplacement))
			return 2;
		else 
			return 5;
		}
		break;

/* 
	If we want to use ESP
	case (atRegisterIndirect): 
	  if ((reg == EBP)) 
          return 2;
    case (atRegisterRelative1ByteDisplacement):
      if (getRegUse() == ESP)
        return 3;

    case (atRegisterRelative4ByteDisplacement):
      if (getRegUse() == ESP)
        return 6;
*/

	default:
		return atInfo[inType].atiSize;
    }
}

void x86SingleArgumentList::
alFormatStackSlotToMemory(void* inStart, x86ArgListInstruction& inInsn, Uint8 inInstructionUseStartIndex,
                          MdFormatter& inFormatter, int offset)
{
	Uint8 modRM = 0;
	Uint8* start = (Uint8*) inStart;

	InstructionUse* useBegin = &inInsn.getInstructionUseBegin()[inInstructionUseStartIndex];
	InstructionUse*	useEnd = inInsn.getInstructionUseEnd();

	// find the stack slot
	VirtualRegister* vrStackSlot = NULL;
	if ((useBegin < useEnd)	&& useBegin->isVirtualRegister())  // if there are uses
	{
		vrStackSlot = &(useBegin->getVirtualRegister());
	}
	
	// at this point vrStackSlot points to
	//	a. a stack slot (the correct thing to do)
	//	b. an non-stack slot register (bad)
	//	c. NULL
	// In the last two conditions we need to get the stack slot from the define
	if(vrStackSlot == NULL || vrStackSlot->getClass() != StackSlotRegister)
	{
		InstructionDefine* defineBegin = inInsn.getInstructionDefineBegin();
		InstructionDefine* defineEnd = inInsn.getInstructionDefineEnd();
		assert(defineBegin < defineEnd);	// we've got to have one!

		vrStackSlot = &(defineBegin->getVirtualRegister());
		assert(vrStackSlot->getClass() == StackSlotRegister);
	}

	Int32 unsizedDisplacement = inFormatter.getStackSlotOffset(*vrStackSlot) + offset;
	assert(vrStackSlot->getClass() == StackSlotRegister);

	if(valueIsOneByteSigned(unsizedDisplacement))
	{
		modRM = 0x40 | EBP;
		*start++ = modRM;
		*(Int8*)start = (Uint8)unsizedDisplacement;
	} 
	else 
	{
		modRM = 0x80 | EBP;
		*start++ = modRM;
		*(Int32*)start = unsizedDisplacement; 
	}
}

void x86SingleArgumentList::
alFormatNonRegToMemory(void* inStart, x86ArgListInstruction& /*inInsn*/, x86ArgumentType inType, MdFormatter& /*inFormatter*/)
{
	Uint8* start = (Uint8*) inStart;
	Uint8 modRM, SIB;

	switch(inType)
	{
	case atAbsoluteAddress:
		modRM = 0x04;
        SIB = 0x25;
		*start++ = modRM;
        *start++ = SIB;
		// Absolute memory address must use a 4 byte "displacement".
		writeLittleWordUnaligned((void*)start, arg4ByteDisplacement);
		break;

	case atStackOffset1ByteDisplacement:
		modRM = 0x45;
		*start++ = modRM;
		*start = arg1ByteDisplacement;
		break;

	case atStackOffset4ByteDisplacement:
		modRM = 0x85;
		*start++ = modRM;
		writeLittleWordUnaligned((void*)start, arg4ByteDisplacement);
		break;

	default:
		trespass("illegal inType");
	}
}

void x86SingleArgumentList::
alFormatRegToMemory(void* inStart, x86ArgListInstruction& inInsn, x86ArgumentType inType, Uint8 inInstructionUseStartIndex, MdFormatter& /*inFormatter*/)
{
	Uint8 modRM;
	Uint8 SIB;
	Uint8 reg, reg2;

	Uint8* start = (Uint8*) inStart;
	SIBScale scale = atInfo[argType1].atiScale;
	extractOperand(inInsn, inInstructionUseStartIndex, reg, reg2);
	assert( reg != ESP && reg2 != ESP );

	switch (inType) 
	{
	case atRegDirect:
		modRM = 0xc0 | reg;
		*start = modRM;
		break;

	case atRegisterIndirect: 
   		if (reg == EBP)
		{
			modRM = 0x45;
			*start++ = modRM;
			*start = 0;
			break;
		}
	/*	if (reg == ESP) 
		{
		  modRM = 0x04;
		  SIB = 0x24;
		  *start = modRM;
		  start++;
		  *start = SIB;
		  return;
		} 
	*/
	  modRM = 0x00 | reg;
	  *start = modRM;
	  break;

	case atBaseIndexed:
	case atScaledIndexedBy2:
	case atScaledIndexedBy4: 
	case atScaledIndexedBy8:
		assert(reg2 != NOT_A_REG);
		assert(scale != kScaleIllegal);
		modRM = 0x04;
		*start++ = modRM;
		assert (reg != EBP);		// EBP is an invalid index  
		//assert (reg2 != ESP);	// Index 
		SIB = scale | (reg2 << 3) | reg;
		*start = SIB;
		break;
  
	case atBaseRelativeIndexed1ByteDisplacement:
	case atScaledIndexed1ByteDisplacementBy2:
	case atScaledIndexed1ByteDisplacementBy4:
	case atScaledIndexed1ByteDisplacementBy8:
		assert(reg2 != NOT_A_REG);
		assert(scale != kScaleIllegal);
		modRM = 0x44;
		*start++ = modRM;
		SIB = scale | ( reg2 << 3) | reg;
		*start++ = SIB;
		*start = arg1ByteDisplacement;
		break;

	case atBaseRelativeIndexed4ByteDisplacement:  
	case atScaledIndexed4ByteDisplacementBy2:
	case atScaledIndexed4ByteDisplacementBy4:
	case atScaledIndexed4ByteDisplacementBy8:
		assert(reg2 != NOT_A_REG);		// must have two regs
		assert(scale != kScaleIllegal);
		modRM = 0x84;
		assert (reg2 != ESP);			// ESP is an invalid index  
		SIB = scale | ( reg2 << 3) | reg;
		*start++ = modRM;
		*start++ = SIB;
		writeLittleWordUnaligned((void*)start, arg4ByteDisplacement);
		break;

	case atRegisterRelative1ByteDisplacement:
		if (reg == ESP) 
		{
			modRM = 0x44;
			SIB = 0x24;
			*start++ = modRM;
			*start++ = SIB;
			*start = arg1ByteDisplacement;
		} 
		else 
		{
			modRM = 0x40 | reg;
			*start++ = modRM;
			*start = arg1ByteDisplacement;
		}
		break;
	
	case atRegisterRelative4ByteDisplacement:
		if (reg == ESP) 
		{
			modRM = 0x84;
			SIB = 0x24;
			*start++ = modRM;
			*start = SIB;
		} else {
			modRM = 0x80 | reg;
			*start++ = modRM;
			writeLittleWordUnaligned((void*)start, arg4ByteDisplacement);
		}
		break;

	default:
		trespass("illegal inType");
	}
}

// Format an instruction to IA32 machine code
void x86SingleArgumentList::
alFormatToMemory(void* inStart, x86ArgListInstruction& inInsn, x86ArgumentType inType, Uint8 inInstructionUseStartIndex, MdFormatter& inFormatter)
{
	// FIX replace by table lookup if more efficient
	switch(inType)
	{
	case atImmediateOnly:
		return;

	case atAbsoluteAddress:
	case atStackOffset1ByteDisplacement:
	case atStackOffset4ByteDisplacement:
		alFormatNonRegToMemory(inStart, inInsn, inType, inFormatter);
		return;

	case atRegAllocStackSlot:
		alFormatStackSlotToMemory(inStart, inInsn, inInstructionUseStartIndex, inFormatter, 0);
        return;

    case atRegAllocStackSlotHi32:
		alFormatStackSlotToMemory(inStart, inInsn, inInstructionUseStartIndex, inFormatter, 4);
		return;

	case atRegDirect:
	case atRegisterIndirect: 
	case atBaseIndexed:
	case atScaledIndexedBy2:
	case atScaledIndexedBy4: 
	case atScaledIndexedBy8:
	case atBaseRelativeIndexed1ByteDisplacement:
	case atScaledIndexed1ByteDisplacementBy2:
	case atScaledIndexed1ByteDisplacementBy4:
	case atScaledIndexed1ByteDisplacementBy8:
	case atBaseRelativeIndexed4ByteDisplacement:  
	case atScaledIndexed4ByteDisplacementBy2:
	case atScaledIndexed4ByteDisplacementBy4:
	case atScaledIndexed4ByteDisplacementBy8:
	case atRegisterRelative1ByteDisplacement:
	case atRegisterRelative4ByteDisplacement:
		alFormatRegToMemory(inStart, inInsn, inType, inInstructionUseStartIndex, inFormatter);
		return;

	default:
		trespass("illegal inType");
	}
}

//--------------------------------------------------------------------------------
// x86SingleArgumentList
x86SingleArgumentList::
x86SingleArgumentList(x86ArgumentType inType) :
	argType1(inType)
{
	// check that inType is valid.
	assert(atInfo[inType].atTypeClass == kMayHaveDisp || atInfo[inType].atTypeClass == kCannotHaveDisp);
}

x86SingleArgumentList::
x86SingleArgumentList(x86ArgumentType inType, Uint32 inDisplacement) 
{
	assert(atInfo[inType].atTypeClass == kMayHaveDisp || atInfo[inType].atTypeClass == kMustHaveDisp);
    if(inType == atAbsoluteAddress)
	{
		// Must take a 4 byte address
		arg4ByteDisplacement = inDisplacement;
		argType1 = inType;
		return;
    }

	assert(atInfo[inType].atTypeClass == kMayHaveDisp);
	if(valueIsOneByteSigned(inDisplacement))
	{	
		arg1ByteDisplacement = (Uint8)inDisplacement;
		argType1 = (x86ArgumentType)(inType + 1);
	} else {
		arg4ByteDisplacement = inDisplacement;
		argType1 = (x86ArgumentType)(inType + 2);
	}
}

bool x86SingleArgumentList::
alSwitchArgumentTypeToSpill(Uint8 /*inWhichUse*/, x86ArgListInstruction& /* inInsn */ )
{
	// assert(argType1 != atRegAllocStackSlot);	// wouldn't we be lying in our result if this were true?
	bool couldSpill;

	if(argType1 == atRegDirect)
	{
		argType1 = atRegAllocStackSlot;
		couldSpill = true;
	}
	else
		couldSpill = false;

	return couldSpill;
}

void x86SingleArgumentList::
alFormatToMemory(void* inStart, Uint32 /*inOffset*/, x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{ 
	// If the argument is a register direct and the opcode can be appended with the reg
	// then the argument list is empty, so do nothing.
	if( (argType1 != atRegDirect) || (!inInsn.regOperandCanBeCondensed()) )
		alFormatToMemory(inStart, inInsn, argType1, 0, inFormatter); 
}

Uint8 x86SingleArgumentList::
alSize(x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{ 
  // If the argument is a register direct and the opcode can be appended with the reg
  // then the argument list is empty, so do nothing.
  if( (argType1 != atRegDirect) || (!inInsn.regOperandCanBeCondensed()) )
	return alSize(argType1, inInsn, 0, inFormatter);
  else
	return 0;
}

//--------------------------------------------------------------------------------
// x86DoubleArgumentList
x86DoubleArgumentList::
x86DoubleArgumentList(x86ArgumentType inType1, x86ArgumentType inType2) :
	x86SingleArgumentList(inType1),
	argType2(inType2)
{
	// check that at most one of the operands is 'memory'
	// ie check that at least on of the operands is register
	assert(inType1 == atRegDirect || inType2 == atRegDirect);

	// check that inType is valid.
	assert(atInfo[inType1].atTypeClass == kMayHaveDisp || atInfo[inType1].atTypeClass == kCannotHaveDisp);
	assert(atInfo[inType2].atTypeClass == kMayHaveDisp || atInfo[inType2].atTypeClass == kCannotHaveDisp);
}

x86DoubleArgumentList::
x86DoubleArgumentList(x86ArgumentType inType1, x86ArgumentType inType2, Uint32 inDisplacement) 
{
	// Make sure only one of the operands is memory type. And set the type based on the displacement.
	switch(inType1)
    {
    case (atRegDirect): 
		argType1 = inType1;
		switch(inType2)
		{
/*			case (atMemoryDirect):
				arg4ByteDisplacement = inDisplacement;
				argType2 = inType2;
				break;
*/
			case (atRegisterIndirect):
			case (atBaseIndexed):
			case (atScaledIndexedBy2):
			case (atScaledIndexedBy4):
			case (atScaledIndexedBy8): 
				if(valueIsOneByteSigned(inDisplacement))
				{	
					arg1ByteDisplacement = (Uint8)inDisplacement;
					argType2 = x86ArgumentType(inType2 + 1);
				} else {
					arg4ByteDisplacement = inDisplacement;
					argType2 = x86ArgumentType(inType2 + 2);
				}
				break;
			default:
				assert(false);
		}
		break; 

/*	case (atMemoryDirect):
		arg4ByteDisplacement = inDisplacement;
		argType1 = inType1;
		assert(inType2 == atRegDirect);
		argType2 = atRegDirect;
		break;
*/
//	case kMayHaveDisp:
	case (atRegisterIndirect):
	case (atBaseIndexed):
	case (atScaledIndexedBy2):
	case (atScaledIndexedBy4):
	case (atScaledIndexedBy8):
	case (atStackOffset):
		if(valueIsOneByteSigned(inDisplacement))
		{	
			arg1ByteDisplacement = (Uint8)inDisplacement;
			argType1 = x86ArgumentType(inType1 + 1);
		} else {
			arg4ByteDisplacement = inDisplacement;
			argType1 = x86ArgumentType(inType1 + 2);
		}
		assert(inType2 == atRegDirect);
		argType2 = atRegDirect;
		break;
	default:
		assert(false); 
    }
}

#ifdef DEBUG
void x86DoubleArgumentList::
alCheckIntegrity(x86ArgListInstruction& inInsn)
{
	VRClass reg1;
	VRClass reg2;

	if(argType1 == atRegDirect || argType1 == atRegAllocStackSlot || argType1 == atRegAllocStackSlotHi32)
		reg1 = alGetArg1VirtualRegister(inInsn)->getClass();
	if(argType2 == atRegDirect || argType2 == atRegAllocStackSlot || argType2 == atRegAllocStackSlotHi32)
		reg2 = alGetArg2VirtualRegister(inInsn)->getClass();

	// ie if use1 is an IntegerRegister, arg1 must be atRegDirect
	// etc.
	if(argType1 == atRegDirect)
		if(reg1 != IntegerRegister)
			goto failedTest;
	if(argType1 == atRegAllocStackSlot || argType1 == atRegAllocStackSlotHi32)
		if(reg1 != StackSlotRegister)
			goto failedTest;
	if(argType2 == atRegDirect)
		if(reg2 != IntegerRegister)
			goto failedTest;
	if(argType2 == atRegAllocStackSlot || argType2 == atRegAllocStackSlotHi32)
		if(reg2 != StackSlotRegister)
			goto failedTest;

	return;

failedTest:
	fprintf(stderr, "\n\n** Failed Test\n");
	alPrintArgs(inInsn);
	trespass("argument list failed integrity check");
}

void x86DoubleArgumentList::
alPrintArgs(x86ArgListInstruction& inInsn)
{
	UT_LOG(x86ArgList, PR_LOG_DEBUG, ("\n Instruction %p\n", &inInsn));
	UT_LOG(x86ArgList, PR_LOG_DEBUG, ("   argType1:       "));
	switch(argType1)
	{
		case atRegDirect:		UT_LOG(x86ArgList, PR_LOG_DEBUG, ("atRegDirect    "));	break;
		case atRegAllocStackSlot:	UT_LOG(x86ArgList, PR_LOG_DEBUG, ("atRegAllocStackSlot  "));	break;
        case atRegAllocStackSlotHi32:	UT_LOG(x86ArgList, PR_LOG_DEBUG, ("atRegAllocStackSlotHi32  "));	break;
		default:					UT_LOG(x86ArgList, PR_LOG_DEBUG, ("other               "));	break;
	}
	UT_LOG(x86ArgList, PR_LOG_DEBUG, ("use1(vr %p): ", alGetArg1VirtualRegister(inInsn)));
	switch(alGetArg1VirtualRegister(inInsn)->getClass())
	{
		case IntegerRegister:		UT_LOG(x86ArgList, PR_LOG_DEBUG, ("IntegerRegister\n"));	break;
		case StackSlotRegister:		UT_LOG(x86ArgList, PR_LOG_DEBUG, ("StackSlotRegister\n"));	break;
		default:					UT_LOG(x86ArgList, PR_LOG_DEBUG, ("other\n"));				break;
	}

	UT_LOG(x86ArgList, PR_LOG_DEBUG, ("   argType2:       "));
	switch(argType2)
	{
		case atRegDirect:		UT_LOG(x86ArgList, PR_LOG_DEBUG, ("atRegDirect    "));	break;
		case atRegAllocStackSlot:	UT_LOG(x86ArgList, PR_LOG_DEBUG, ("atRegAllocStackSlot  "));	break;
        case atRegAllocStackSlotHi32:	UT_LOG(x86ArgList, PR_LOG_DEBUG, ("atRegAllocStackSlotHi32  "));	break;
        default:					UT_LOG(x86ArgList, PR_LOG_DEBUG, ("other               "));	break;
	}
	UT_LOG(x86ArgList, PR_LOG_DEBUG, ("use2(vr %p): ", alGetArg2VirtualRegister(inInsn)));
	switch(alGetArg2VirtualRegister(inInsn)->getClass())
	{
		case IntegerRegister:		UT_LOG(x86ArgList, PR_LOG_DEBUG, ("IntegerRegister\n\n"));		break;
		case StackSlotRegister:		UT_LOG(x86ArgList, PR_LOG_DEBUG, ("StackSlotRegister\n\n"));	break;
		default:					UT_LOG(x86ArgList, PR_LOG_DEBUG, ("other\n\n"));				break;
	}
}

#endif

bool x86DoubleArgumentList::
alSwitchArgumentTypeToSpill(Uint8 inWhichUse, x86ArgListInstruction& inInsn)
{

#ifdef DEBUG
	alCheckIntegrity(inInsn);					// ensure that everthing makes sense here
#endif

	// ensure that we are not trying to spill a stack slot register
	if(inWhichUse == 0)
		assert(argType1 != atRegAllocStackSlot && argType1 != atRegAllocStackSlotHi32);
	else
		assert(argType2 != atRegAllocStackSlot && argType2 != atRegAllocStackSlotHi32);

	// can only spill if both arguments are currently atRegDirect
	if((argType1 != atRegDirect) || (argType2 != atRegDirect))
		return false;

	// try to spill
	assert(inWhichUse <= 1);
	if(inWhichUse == 0)		// first use
	{
		// to spill we'll need to reverse the operands
		// return failure if operands couldn't be reversed
		if(!inInsn.canReverseOperands())
			return false;

		// ok, we can reverse, so do it
		argType1 = atRegAllocStackSlot;
	}
	else					// second use
	{
		// FIX FIX FIX Quick Hack
		if(!inInsn.canReverseOperands())
			return false;

		argType2 = atRegAllocStackSlot;
	}
	return true;
}

// temporary helper method until x86DoubleArgumentList can be cleaned up
bool x86DoubleArgumentList::
alHasTwoArgs(x86ArgListInstruction& inInsn)
{
	InstructionUse* useBegin = inInsn.getInstructionUseBegin();
	InstructionUse*	supplementaryUseBegin = useBegin + atInfo[argType1].atiNumUses;	// skip over first arg's registers
	InstructionUse*	useEnd = inInsn.getInstructionUseEnd();
	return (supplementaryUseBegin < useEnd) && supplementaryUseBegin->isVirtualRegister();
}

void x86DoubleArgumentList::
alFormatToMemory(void* inStart, Uint32 /*inOffset*/, x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{
	VirtualRegister* directReg;

	// Four cases:
	// 
	// Use1 == Def							    Use2 is memory
	//	  a     b								  a     [b]				
	//	+----------+		     <-				+----------+			 <-			
	//	|    add   +  --> add   a, b			|    add   +  --> add   a, [b]	
	//	+----------+							+----------+						
	//	     a									      a								
	//
	//
	// Use1 != Def								 Use1 is memory							
	//		  a     							  [a]     b	  
	//	+----------+			 				+----------+			  ->			
	//	|   movsx  +  --> movsx b, a			|    add   +  --> add    b, [a]	
	//	+----------+							+----------+					
	//	      b										  [a]
	//				
	//
	//
	// 1. if there is a memory, format it first
	// 2. if both register and only one use, format the define first

	// Format Mem/Reg Operand																						// mem/reg == first use if...
	if(	(argType1 != atRegDirect) ||													// ...arg1 is memory, or
		(argType1 == atRegDirect && argType2 == atRegDirect && !alHasTwoArgs(inInsn)))	// ...both reg, and there is only one use
	{
		x86SingleArgumentList::alFormatToMemory(inStart, inInsn, argType1, 0, inFormatter);
		directReg = alGetArg2VirtualRegister(inInsn);
		assert(directReg->getClass() == IntegerRegister);								// reg must be Integer register
	} 
	else
	{
		//otherwise mem/reg == second use
		x86SingleArgumentList::alFormatToMemory(inStart, inInsn, argType2, 1, inFormatter);
		directReg = alGetArg1VirtualRegister(inInsn);
		assert(directReg->getClass() == IntegerRegister);								// reg must be Integer register
	}

	// Format Reg Operand
	*(Uint8*)inStart = *(Uint8*)inStart | (getRegisterNumber(directReg) << 3);
}

#ifdef DEBUG_LOG

void x86DoubleArgumentList::
alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn)
{
	InstructionUse*		useBegin	= inInsn.getInstructionUseBegin();
	InstructionUse*		useEnd		= inInsn.getInstructionUseEnd();
	Uint8				totalUses	= atInfo[argType1].atiNumUses + atInfo[argType2].atiNumUses;

	// Check to see if there are enough uses to encode all of the registers.  If this is the
	// case then the operands should be printed in there normal order.   Otherwise the second operand is defined
	// only in the defines (ie a mov instruction) and should be printed first.
	if(useEnd >= useBegin + totalUses)
	{
		x86SingleArgumentList::alPrintPretty(f, inInsn, argType1, 0);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", "));
		x86SingleArgumentList::alPrintPretty(f, inInsn, argType2, atInfo[argType1].atiNumUses);
	} else {
		x86SingleArgumentList::alPrintPretty(f, inInsn, argType2, atInfo[argType1].atiNumUses);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", "));
		x86SingleArgumentList::alPrintPretty(f, inInsn, argType1, 0);
	}
}

#endif // DEBUG_LOG																					

VirtualRegister* x86DoubleArgumentList::
alGetArg1VirtualRegister(x86ArgListInstruction& inInsn)
{
	InstructionUse* use = inInsn.getInstructionUseBegin();
	return &use->getVirtualRegister();
}

// Find the virtual register corresponding to the second argument
VirtualRegister* x86DoubleArgumentList::
alGetArg2VirtualRegister(x86ArgListInstruction& inInsn)
{
	VirtualRegister* vreg;

	InstructionUse* useBegin = inInsn.getInstructionUseBegin();
	InstructionUse*	useEnd = inInsn.getInstructionUseEnd();
	InstructionUse*	supplementaryUseBegin = useBegin + atInfo[argType1].atiNumUses;	// skip over first arg's registers

	if( (supplementaryUseBegin < useEnd) && supplementaryUseBegin->isVirtualRegister() )
	{	
		vreg = &(supplementaryUseBegin->getVirtualRegister());						// supplementary uses
	}
	else
	{
		InstructionDefine* defineBegin = inInsn.getInstructionDefineBegin();
		vreg = &(defineBegin->getVirtualRegister());								// first define
	}

	return vreg;
}

//--------------------------------------------------------------------------------
// x86ControlNodeOffsetArgumentList
#ifdef DEBUG_LOG

void x86ControlNodeOffsetArgumentList::
alPrintPretty(LogModuleObject &f, x86ArgListInstruction& /*inInsn*/) 
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("0x%x [<A HREF=\"#N%d\">N%d</A>]", cnoTarget.getNativeOffset(), cnoTarget.dfsNum, cnoTarget.dfsNum)); 
}
 
#endif // DEBUG_LOG																					

void x86ControlNodeOffsetArgumentList::
alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/) 
{ 
	// To compute the relative branch we subtract our current address from out target address.  Then we subtract the size
	// of our instruction because our current IP is actually there.  This is the sum of the size of the opcode and the size of the
	// argumentlist.  NOTE: If we use 1 byte jumps in the future then this needs to be fixed from a constant 4.
	writeLittleWordUnaligned((void*)inStart, cnoTarget.getNativeOffset() - inOffset - inInsn.opcodeSize() - 4);
}

//--------------------------------------------------------------------------------
// x86CondensableImmediateArgumentList
// must take 4 byte immediate
void x86CondensableImmediateArgumentList::
alFormatToMemory(void* inStart, Uint32 /*inOffset*/, x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{
  Uint8* immLocation;
  // find location of immediate. 
  if(argType1 != atRegDirect)
  {	
	// argument list takes up some space
	immLocation = (Uint8*)inStart + x86SingleArgumentList::alSize(argType1, inInsn, 0, inFormatter); 
    x86SingleArgumentList::alFormatToMemory(inStart, inInsn, argType1, 0, inFormatter);
  } else
	// argument list has been captured in the opcode
	immLocation = (Uint8*)inStart;

	// write out the immediate
	writeLittleWordUnaligned((void*)immLocation, imm4Bytes);
}

Uint8 x86CondensableImmediateArgumentList::
alSize(x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{
  return  x86SingleArgumentList::alSize(inInsn, inFormatter) + 4;
}

//--------------------------------------------------------------------------------
// x86ImmediateArgumentList
void x86ImmediateArgumentList::
alFormatToMemory(void* inStart, Uint32 /*inOffset*/, x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{
	// Find the location of the immediate.  
    Uint8* immLocation = (Uint8*)inStart;
    if (argType1 != atImmediateOnly)
	{
        immLocation += x86SingleArgumentList::alSize(argType1, inInsn, 0, inFormatter); 
	    x86SingleArgumentList::alFormatToMemory(inStart, inInsn, argType1, 0, inFormatter);
    }

	// write out the immediate
	if(iSize == is1ByteImmediate && inInsn.opcodeCanAccept1ByteImmediate())
		*immLocation = imm1Byte;
	else
	{
		assert(inInsn.opcodeCanAccept4ByteImmediate());
		writeLittleWordUnaligned((void*)immLocation, imm4Bytes);
	}
}

Uint8 x86ImmediateArgumentList::
alSize(x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{
	Uint8 size = 0;

    // Handle special case of immediate push, which has only one argument
    if (argType1 != atImmediateOnly)
        size += x86SingleArgumentList::alSize(inInsn, inFormatter);

	if (iSize == is1ByteImmediate && inInsn.opcodeCanAccept1ByteImmediate())
		size += 1;
	else
	{
		assert(inInsn.opcodeCanAccept4ByteImmediate());
		size += 4;
	}
	return size;
}

//--------------------------------------------------------------------------------
// x86SpecialRegMemArgumentList
void x86SpecialRegMemArgumentList::
alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{
	if(argType1 == atRegDirect)
	{
		// We should output ourselves as though we were a double argument list 
		// with both arguments being the same register.
		InstructionUse*	useBegin = inInsn.getInstructionUseBegin();
		InstructionUse*	useEnd	 = inInsn.getInstructionUseEnd();
		Uint8 reg;

		if( (useBegin < useEnd) && (useBegin->isVirtualRegister()) )
			reg = useToRegisterNumber(*useBegin);
		else {
			InstructionDefine* defineBegin = inInsn.getInstructionDefineBegin();
			InstructionDefine* defineEnd = inInsn.getInstructionDefineEnd();
			assert( (defineBegin < defineEnd) && (defineBegin->isVirtualRegister()) );
			reg = defineToRegisterNumber(*defineBegin);
		}
		*(Uint8*)inStart = ( 0xc0 | (reg) | (reg << 3) );
	}
	else
	{
		// Otherwise we format ourselves as though we were an immediate argument list
		x86ImmediateArgumentList::alFormatToMemory(inStart, inOffset, inInsn, inFormatter);
	}
}

#ifdef DEBUG_LOG

void x86SpecialRegMemArgumentList::
alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn)
{
	if(argType1 == atRegDirect)
	{
		// We should print ourselves as though we were a double argument list 
		// with both arguments being the same register.
		InstructionUse*	useBegin = inInsn.getInstructionUseBegin();
		InstructionUse*	useEnd	 = inInsn.getInstructionUseEnd();
		Uint8 reg;
		if( (useBegin < useEnd) && (useBegin->isVirtualRegister()) )
			reg = useToRegisterNumber(*useBegin);
		else {
			InstructionDefine* defineBegin = inInsn.getInstructionDefineBegin();
			InstructionDefine* defineEnd = inInsn.getInstructionDefineEnd();
			assert( (defineBegin < defineEnd) && (defineBegin->isVirtualRegister()) );
			reg = defineToRegisterNumber(*defineBegin);
		}
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s, %s", x86GPRText[reg], x86GPRText[reg] ));
	} else
		// Otherwise we print ourselves as though we were an immediate argument list
		x86ImmediateArgumentList::alPrintPretty(f, inInsn);
}

#endif // DEBUG_LOG
