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
// File:	x86ArgumentList.h
//
// Authors:	Peter DeSantis
//			Simon Holmes a Court
//

#ifndef _X86ARGUMENTLIST
#define _X86ARGUMENTLIST

/*
***********************************************************************************************************************************
x86 ArgumentLists
Anywhere from 0 to 10 bytes of information following the x86 Opcode.  
Includes the ModR/M byte, the SIB byte, the Displacement field and the immediate field.
***********************************************************************************************************************************
*/

#include "CatchAssert.h"
#include <stdio.h>


#include "prtypes.h"
#include "ControlNodes.h"
#include "x86Opcode.h"
#include "MemoryAccess.h"

class x86ArgListInstruction;

enum x86ImmediateSize
{
  isNoImmediate,
  is1ByteImmediate,
  is4ByteImmediate
};


enum x86ArgumentType
{
  atRegDirect,
  atRegAllocStackSlot,	        // Stack slot designated by the register allocator
  atRegAllocStackSlotHi32,      // Upper 32-bits of 64-bit stack slot designated by the register allocator
  
  atRegisterIndirect,	// 								
  atRegisterRelative1ByteDisplacement,				
  atRegisterRelative4ByteDisplacement,
  
  atBaseIndexed,		// 
  atBaseRelativeIndexed1ByteDisplacement,
  atBaseRelativeIndexed4ByteDisplacement,
  
  atScaledIndexedBy2,	// 
  atScaledIndexed1ByteDisplacementBy2,
  atScaledIndexed4ByteDisplacementBy2,
 
  atScaledIndexedBy4,	// 
  atScaledIndexed1ByteDisplacementBy4,
  atScaledIndexed4ByteDisplacementBy4,

  atScaledIndexedBy8,	//
  atScaledIndexed1ByteDisplacementBy8,
  atScaledIndexed4ByteDisplacementBy8,

  atStackOffset,
  atStackOffset1ByteDisplacement,
  atStackOffset4ByteDisplacement,

  atAbsoluteAddress,
  atImmediateOnly
};

// Maps register alloc color to x86 machine register
extern x86GPR colorTox86GPR[];

// Maps x86 machine register to register alloc color
extern Uint8 x86GPRToColor[];

//--------------------------------------------------------------------------------
// x86ArgumentList
// Abstract base class
class x86ArgumentList
{
protected:
							x86ArgumentList() { }

public:
  
  virtual void				alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/) = 0;
  virtual Uint8				alSize(x86ArgListInstruction& /*inInsn*/, MdFormatter& /*inFormatter*/) = 0;
  
  virtual Uint8				alGetRegisterOperand(  x86ArgListInstruction& /*inInsn*/ ) { return NOT_A_REG; }

  virtual bool				alSwitchArgumentTypeToSpill(Uint8 /*inWhichUse*/, x86ArgListInstruction& /* inInsn */ ) { return false;}
  virtual x86ImmediateSize	alSizeOfImmediate() { return isNoImmediate; }
  virtual bool				alIsRegisterDirect() { return true; }			

#ifdef DEBUG
  virtual void				alCheckIntegrity(x86ArgListInstruction& /*inInsn*/) { /* do nothing for now, except for double argument list */ }
  virtual void				alPrintArgs(x86ArgListInstruction& /*inInsn*/)		{ /* do nothing for now, except for double argument list */ }
#endif // DEBUG

#ifdef DEBUG_LOG
  virtual void				alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn) = 0;
#endif // DEBUG_LOG
};

//--------------------------------------------------------------------------------
// x86EmptyArgumentList
// Contains no information
class x86EmptyArgumentList :
  public x86ArgumentList
{
public:
							x86EmptyArgumentList() {}
  virtual void				alFormatToMemory(void* /*inStart*/, Uint32 /*inOffset*/, x86ArgListInstruction& /*inInsn*/, MdFormatter& /*inFormatter*/) {}
  virtual Uint8				alSize(x86ArgListInstruction& /*inInsn*/, MdFormatter& /*inFormatter*/) { return 0; }

#ifdef DEBUG_LOG
public:
  virtual void				alPrintPretty(LogModuleObject /*f*/, x86ArgListInstruction& /*inInsn*/) {}  
#endif // DEBUG_LOG
};

//--------------------------------------------------------------------------------
// x86ControlNodeOffsetArgumentList
class x86ControlNodeOffsetArgumentList :
  public x86ArgumentList
{
public:
								x86ControlNodeOffsetArgumentList( ControlNode& inTarget )  : cnoTarget(inTarget) { }
	virtual void				alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/);
	// FIX In the future we would like short jumps to be encode with 1 byte immediates.
	virtual Uint8				alSize(x86ArgListInstruction& /*inInsn*/, MdFormatter& /*inFormatter*/) {  return 4; }
	virtual x86ImmediateSize	alSizeOfImmediate() { return is4ByteImmediate; }
  
protected:
	ControlNode&				cnoTarget;

#ifdef DEBUG_LOG
public:
	virtual void				alPrintPretty(LogModuleObject &f, x86ArgListInstruction& /*inInsn*/);
#endif
};

/*================================================================================
	The following argument lists all depend on the InstructionUses and InstructionDefines to contain the Virtual Registers
	which have been assigned machine registers prior to printing or formating.  Additionally, all make assumptions about the 
	properties of the uses and defines which hold the VRs.  

	Let the array of uses be enumerated u0, u1, u2,...un, and the array of defines d0, d1, d2,...dm.
	  
	These assumptions require the following:
	
	For some ux, all ui i <= x are uses which contain VRs (represent machine registers) and all uj j > x are uses which do 
	not contain VRs (represent machine registers).
	
	Similarly, For some dx, all ui i <= x are defines which contain VRs (represent machine registers) and all dj j > x are defines which do 
	not contain VRs (represent machine registers).

	If some register/memory operand is overwritten then it encoded in the first use(s) and the first define(s).

	Many of the functions use a wrap around approach.  They take some offset into the uses array and expect to find the virtual registers
	necessary to format themselves at that spot.  If instead they find that they are beyond the end of the array of uses or that the current 
	use is not a VR, then they know that they can find they VRs at the beginning of the defines.
*/

//--------------------------------------------------------------------------------
// x86SingleArgumentList
// One register or memory operand
class x86SingleArgumentList :
  public x86ArgumentList
{
public:
					x86SingleArgumentList(x86ArgumentType inType);
					x86SingleArgumentList(x86ArgumentType inType, Uint32 inDisplacement);

	virtual void	alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/);											
	virtual Uint8	alSize(x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/);
	virtual bool	alSwitchArgumentTypeToSpill(Uint8 inWhichUse, x86ArgListInstruction& /* inInsn */);

	virtual Uint8	alGetRegisterOperand( x86ArgListInstruction& inInsn );
	virtual bool	alIsRegisterDirect() { return argType1 == atRegDirect; }			

protected:
					x86SingleArgumentList() { }
  
	//  Utility functions used by all super classes of x86SingleArgumentList */
	void			alFormatToMemory(void* inStart, x86ArgListInstruction& inInsn, x86ArgumentType inType, Uint8 inInstructionUseStartIndex, MdFormatter& /*inFormatter*/);
	Uint8			alSize(x86ArgumentType inType, x86ArgListInstruction& inInsn, Uint8 inInstructionUseStartIndex, MdFormatter& /*inFormatter*/);
  
  	x86ArgumentType	argType1;
  
	union
	{
		Uint8		arg1ByteDisplacement; 
		Uint32		arg4ByteDisplacement;
	};

private:
	void			alFormatStackSlotToMemory(void* inStart, x86ArgListInstruction& inInsn, Uint8 inInstructionUseStartIndex, MdFormatter& /*inFormatter*/, int offset);
	void			alFormatNonRegToMemory(void* inStart, x86ArgListInstruction& inInsn, x86ArgumentType inType, MdFormatter& /*inFormatter*/);
	void			alFormatRegToMemory(void* inStart, x86ArgListInstruction& inInsn, x86ArgumentType inType, Uint8 inInstructionUseStartIndex, MdFormatter& /*inFormatter*/);

#ifdef DEBUG_LOG
public:
	virtual void	alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn) { alPrintPretty(f, inInsn, argType1, 0); }
	void			alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn, x86ArgumentType inType, Uint8 inInstructionUseStartIndex);
#endif // DEBUG_LOG
};
 
//--------------------------------------------------------------------------------
// x86DoubleArgumentList
// A memory and a register operand or two register operands
class x86DoubleArgumentList :
  public x86SingleArgumentList
{
protected:
  x86ArgumentType			argType2;

public:
							x86DoubleArgumentList(x86ArgumentType inType1, x86ArgumentType inType2);
							x86DoubleArgumentList(x86ArgumentType inType1, x86ArgumentType inType2, Uint32 inDisplacement);

  virtual void				alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/); 
  virtual Uint8				alSize(x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/); 
  
  virtual Uint8				alGetRegisterOperand(  x86ArgListInstruction& /*inInsn*/ ) { return NOT_A_REG; }

  virtual bool				alSwitchArgumentTypeToSpill(Uint8 inWhichUse, x86ArgListInstruction& /* inInsn */ );

  VirtualRegister*			alGetArg1VirtualRegister(x86ArgListInstruction& inInsn);
  VirtualRegister*			alGetArg2VirtualRegister(x86ArgListInstruction& inInsn);

  virtual bool				alIsRegisterDirect() { return argType1 == atRegDirect && argType2 == atRegDirect; }			
  bool						akIsArg1Direct() { return argType1 == atRegDirect; }
  bool						akIsArg2Direct() { return argType2 == atRegDirect; }
  bool						alHasTwoArgs(x86ArgListInstruction& inInsn);

#ifdef DEBUG
  virtual void				alCheckIntegrity(x86ArgListInstruction& inInsn);
  virtual void				alPrintArgs(x86ArgListInstruction& inInsn);
#endif // DEBUG

#ifdef DEBUG_LOG
public:
  virtual void				alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn);
#endif // DEBUG_LOG
};

inline Uint8 x86DoubleArgumentList::
alSize(x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{
	// The formatting of the mem argument determines the size of the argumentlist because one register direct can
	// just be ORed.
	int size;
	if(argType1 !=  atRegDirect)
		size = x86SingleArgumentList::alSize(argType1, inInsn, 0, inFormatter);
	else
		size = x86SingleArgumentList::alSize(argType2, inInsn, 1, inFormatter);
	return size;
}

//--------------------------------------------------------------------------------
// x86ImmediateArgumentList
// One reg or mem operand and an immediate operand.
class x86ImmediateArgumentList :
  public x86SingleArgumentList
{
public:
							x86ImmediateArgumentList(x86ArgumentType inType1, Uint32 inImm); 
							x86ImmediateArgumentList(x86ArgumentType inType1, Uint32 inDisplacement, Uint32 inImm);
                                                                                                    
  virtual void				alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/); 
  virtual Uint8				alSize(x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/); 
  
  virtual x86ImmediateSize	alSizeOfImmediate() { return iSize; }

protected:

  union 
  {
    Uint8					imm1Byte;
    Uint32					imm4Bytes;
  };
 
  x86ImmediateSize			iSize;

#ifdef DEBUG_LOG
public:
	virtual void			alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn);
#endif // DEBUG_LOG
};

inline x86ImmediateArgumentList::
x86ImmediateArgumentList(x86ArgumentType inType1, Uint32 inImm)
: x86SingleArgumentList(inType1) 
{	 
	imm4Bytes = inImm;
	if(valueIsOneByteSigned(inImm)) 
		iSize = is1ByteImmediate; 
	else 
		iSize = is4ByteImmediate;
}																											
	
inline x86ImmediateArgumentList::
x86ImmediateArgumentList(x86ArgumentType inType1, Uint32 inDisplacement, Uint32 inImm)
: x86SingleArgumentList(inType1, inDisplacement) 
{	 
	imm4Bytes = inImm;
	if(valueIsOneByteSigned(inImm)) 
		iSize = is1ByteImmediate; 
	else 
		iSize = is4ByteImmediate;
}

#ifdef DEBUG_LOG

inline void x86ImmediateArgumentList::
alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn)
{
	x86SingleArgumentList::alPrintPretty(f, inInsn, argType1, 0 );
    if (argType1 != atImmediateOnly)
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", "));
	if(iSize == is1ByteImmediate)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%0x", imm1Byte));
	else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%0x", imm4Bytes));
}

#endif // DEBUG_LOG																					

//--------------------------------------------------------------------------------
// x86CondensableImmediateArgumentList
class x86CondensableImmediateArgumentList :
  public x86SingleArgumentList
{
public:
							x86CondensableImmediateArgumentList(x86ArgumentType inType1, Uint32 inImm); 
							x86CondensableImmediateArgumentList(x86ArgumentType inType1, Uint32 inDisplacement, Uint32 inImm);
                                                                                                    
  virtual void				alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/); 
  virtual Uint8				alSize(x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/); 
  
  virtual x86ImmediateSize	alSizeOfImmediate() { return iSize; }

protected:
    Uint32					imm4Bytes;
	x86ImmediateSize		iSize;

#ifdef DEBUG_LOG
public:
	virtual void			alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn);
#endif // DEBUG_LOG
};

inline x86CondensableImmediateArgumentList::
x86CondensableImmediateArgumentList(x86ArgumentType inType1, Uint32 inImm)
: x86SingleArgumentList(inType1), imm4Bytes(inImm)
{	 
}																											
	
inline x86CondensableImmediateArgumentList::
x86CondensableImmediateArgumentList(x86ArgumentType inType1, Uint32 inDisplacement, Uint32 inImm)
: x86SingleArgumentList(inType1, inDisplacement), imm4Bytes(inImm)
{	 
}
																					
#ifdef DEBUG_LOG

inline void x86CondensableImmediateArgumentList::
alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn)
{
	x86SingleArgumentList::alPrintPretty(f, inInsn, argType1, 0 );
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", %0x", imm4Bytes));
}

#endif // DEBUG_LOG																					

//--------------------------------------------------------------------------------
// x86DoubleImmediateArgumentList
// [ immediate argument, mem, and reg] or [immediate argument, two reg operands]
class x86DoubleImmediateArgumentList :
  public x86DoubleArgumentList
{
public:
							x86DoubleImmediateArgumentList(x86ArgumentType inType1, x86ArgumentType inType2, Uint32 inImm); 
							x86DoubleImmediateArgumentList(x86ArgumentType inType1, x86ArgumentType inType2, Uint32 inDisplacement, Uint32 inImm);
   virtual void				alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/); 
  virtual Uint8				alSize(x86ArgListInstruction& /*inInsn*/, MdFormatter& /*inFormatter*/); 
 
  virtual x86ImmediateSize	alSizeOfImmediate() { return iSize; }

protected:
 
 union 
 {
    Uint8					imm1Byte;
    Uint32					imm4Bytes;
 };

 x86ImmediateSize			iSize;

#ifdef DEBUG_LOG
public:
 	virtual void			alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn);
#endif // DEBUG_LOG
};

inline x86DoubleImmediateArgumentList::
x86DoubleImmediateArgumentList(x86ArgumentType inType1, x86ArgumentType inType2, Uint32 inImm)
: x86DoubleArgumentList(inType1, inType2)
{ 
	imm4Bytes = inImm;
//	if(valueIsOneByteSigned(inImm)) 
//		iSize = is1ByteImmediate; 
//	else 
		iSize = is4ByteImmediate;
}

inline x86DoubleImmediateArgumentList::
x86DoubleImmediateArgumentList(x86ArgumentType inType1, x86ArgumentType inType2, Uint32 inDisplacement, Uint32 inImm)
 : x86DoubleArgumentList(inType1, inType2, inDisplacement)
{ 
	imm4Bytes = inImm;
//	if(valueIsOneByteSigned(inImm))		// FIX
//		iSize = is1ByteImmediate; 
//	else 
		iSize = is4ByteImmediate;
}

inline Uint8 x86DoubleImmediateArgumentList::
alSize(x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{
	Uint32 size = x86DoubleArgumentList::alSize(inInsn, inFormatter);

 	if(iSize == is1ByteImmediate)
 		size += 1;
 	else
		size += 4;

	return size;
 }

inline void x86DoubleImmediateArgumentList::
alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{
	x86DoubleArgumentList::alFormatToMemory(inStart, inOffset, inInsn, inFormatter);

	Uint8* immLocation = (Uint8*)inStart + x86DoubleArgumentList::alSize(inInsn, inFormatter); 
 	if(iSize == is1ByteImmediate)
		*immLocation = imm1Byte;
	else
		writeLittleWordUnaligned((void*)immLocation, imm4Bytes);
}

#ifdef DEBUG_LOG

inline void x86DoubleImmediateArgumentList::
alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn)
{
	x86DoubleArgumentList::alPrintPretty(f, inInsn);
	if(iSize == is1ByteImmediate)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", %0x", imm1Byte));
	else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", %0x", imm4Bytes));
}

#endif // DEBUG_LOG																					

//--------------------------------------------------------------------------------
// x86SpecialRegMemArgumentList
// Only used with x86SpecialRegMemOpcode
// This argument list holds an immediate value and a single argument.  In its initial form it emits a double argument list
// of the one register operand.  If it is switched to spill type it prints the memory operand and the immediate value.

class x86SpecialRegMemArgumentList :
	public x86ImmediateArgumentList
{
public:
							x86SpecialRegMemArgumentList(Uint32 inImm) : x86ImmediateArgumentList(atRegDirect, inImm) { }

  virtual void				alFormatToMemory(void* inStart, Uint32 inOffset, x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/); 
  virtual Uint8				alSize(x86ArgListInstruction& inInsn, MdFormatter& /*inFormatter*/); 

#ifdef DEBUG_LOG
public:
	virtual void			alPrintPretty(LogModuleObject &f, x86ArgListInstruction& inInsn);
#endif // DEBUG_LOG
};

inline Uint8 x86SpecialRegMemArgumentList::
alSize(x86ArgListInstruction& inInsn, MdFormatter& inFormatter)
{
	Uint8 size;
	if(argType1 == atRegDirect)
		size = 1;  // Only need the ModR/M byte
	else
		size = x86ImmediateArgumentList::alSize(inInsn, inFormatter);
	return size;
}

#endif  //X86_ARGUMENTLIST
