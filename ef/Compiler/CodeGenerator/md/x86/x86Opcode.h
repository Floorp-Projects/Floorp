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
/*
	x86Opcode.h
	
	desantis
	simon

	NOTE:
	This file is being phased out -- please do not add to it without speaking to simon
*/

#ifndef _X86OPCODE
#define _X86OPCODE

#include "CatchAssert.h"
#include <stdio.h>

#include "Fundamentals.h"
#include "LogModule.h"

class x86ArgumentList;
class x86ArgListInstruction;

//================================================================================
// enums
enum x86NoArgsCode
{
	opCdq,
	opBreak,
    opSahf
};

enum x86DoubleOpCode
{
	opMul,
	opMovSxB,
	opMovSxH,
	opMovZxB,
	opMovZxH
};

enum x86DoubleOpDirCode
{
	raAdd,
	raAdc,
	raSub,
	raSbb,
	raAnd,
	raOr,
	raXor,
	raCmp,

	raLoadI,
	raCopyI,
	raStoreI,
	raSaveReg,

	raStoreB,
	raStoreH
};

// Floating-point instructions in which one operand is a memory location.
// If there is a second operand, then it is implicitly the top of the FPU stack.
enum x86FloatMemoryType
{
	fstp32,        // TOS => 32-bit float memory.  Pop FPU.
    fstp64,        // TOS => 64-bit double memory.  Pop FPU.
    fst32,         // TOS => 32-bit float memory. (Don't pop FPU stack.)
    fst64,         // TOS => 64-bit double memory.  (Don't pop FPU stack.)
    fistp32,       // Round(TOS) => 32-bit int memory.  Pop FPU.
    fistp64,       // Round(TOS) => 64-bit long memory. Pop FPU.
    fld32,         // 32-bit float memory => Push on FPU stack
    fld64,         // 64-bit float memory => Push on FPU stack
    fild32,        // 32-bit int memory => convert to FP and push on FPU stack
    fild64,        // 64-bit long memory => convert to FP and push on FPU stack
    fadd32,        // Add TOS and 32-bit float memory => replace TOS
    fadd64,        // Add TOS and 64-bit double memory => replace TOS
    fmul32,        // Multiply TOS and 32-bit float memory => replace TOS
    fmul64,        // Multiply TOS and 64-bit double memory => replace TOS
    fsub32,        // Subtract TOS from 32-bit float memory => replace TOS
    fsub64,        // Subtract TOS from 64-bit double memory => replace TOS
    fsubr32,       // Subtract 32-bit float memory from TOS => replace TOS
    fsubr64,       // Subtract 64-bit double memory from TOS => replace TOS
    fdiv32,        // Divide TOS by 32-bit float memory => replace TOS
    fdiv64,        // Divide TOS by 64-bit double memory => replace TOS
    fcomp32,       // Compare TOS to 32-bit float memory, setting FPU flags, pop TOS
    fcomp64        // Compare TOS to 64-bit double memory, setting FPU flags, pop TOS
};

// enums for the x96 conidtion code
enum x86ConditionCode
{
	ccJO	= 0x00,
	ccJNO	= 0x01,
	ccJB	= 0x02,
	ccJNB	= 0x03,
	ccJE	= 0x04,
	ccJNE	= 0x05,
	ccJBE	= 0x06,
	ccJNBE	= 0x07,
	ccJS	= 0x08,
	ccJNS	= 0x09,
	ccJP	= 0x0a,
	ccJNP	= 0x0b,
	ccJL	= 0x0c,
	ccJNL	= 0x0d,
	ccJLE	= 0x0e,
	ccJNLE	= 0x0f
};

/*================================================================================
x86 Opcodes
	- Can be one byte or two bytes in length. If two bytes, the first byte is 0x0f.
	- Can have bits to control:
		- data flow direction(D), 
		- sign extension(S)				// FIX pete, isn't this 'size' not 'sign'?
		- conditions (cond), and 
		- register field 
		- opcode extensions (Reg).  
	- Can have an alternate (condensed) encoding which allows a single register operand to be encoded in the opcode.
*/

enum x86GPR
{
  EAX = 0,
  ECX = 1,
  EDX = 2,
  EBX = 3,
  ESP = 4,
  EBP = 5,
  ESI = 6,
  EDI = 7,
  NOT_A_REG = 255
};

// bit masks
const int kRegfield_Mask =			0x38;	// 0b00111000
const int kIs_Memory_Form_Mask =	0x40;	// 0b01000000
const int kCondition_Code_Mask =	0x0f;	// 0b00001111
const int kCondensed_Op_Mask =		0xf8;	// 0b11111000
const int kIs_2_Byte_Mask =			0x80;	// 0b10000000
const int kIs_16_Bit_Mask =			0x40;	// 0b01000000
const int kExtension_Mask =			0x40;	// 0b01000000

const int kPrefix_For_16_Bits =		0x66;
const int kPrefix_For_2_Byte =		0x0f;	// 0b00001111

/*--------------------------------------------------------------------------------
Opcode Information
*/
struct x86OpcodeInfo
{
	uint8 oiBaseOpcode;				// generally the opcode, but can have additional info
	uint8 oiOpcodeInformation;		// additional info about opcode
	char* oiText;					// string for fake disassembly
};

extern	x86OpcodeInfo iaInfo[];
enum x86ImmediateArithType
{
	iaAddImm,
	iaCmpImm,
	iaJmp,
	iaPushImm,
	iaAndImm,
	iaOrImm,
	iaXorImm
};

extern x86OpcodeInfo eInfo[];
enum x86ExtendedType
{
	eIDiv,
	eDiv,
	eMul,
	eMoveImm,
	ePush,
	eSarImm,
	eShrImm,
	eShlImm,
	eSar1,
	eShr1,
	eShl1,
	eSarCl,
	eShrCl,
	eShlCl,
	eNeg
};

extern x86OpcodeInfo ceInfo[];
enum x86CondensableExtendedType
{
	ceInc,
	ceDec,
	ceMoveImm,
	cePop
};

extern x86OpcodeInfo srmInfo[];
enum x86SpecialRegMemType
{
	srmCmpImm0,
	srm1NOTUSED,
	srmMoveImm0,
	srm3NOTUSED
};

//================================================================================
// Class Declarations

/*--------------------------------------------------------------------------------
x86Opcode -- encodes x86StandardType

Basic opcode
	- 8 or 16 bits
	- no toggable bits and 
	- does NOT have a register field opcode extension or alternate encoding.
*/
class x86Opcode
{
public:					   
  virtual void			opPrintPretty(LogModuleObject &f) { UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%-9s", opInfo->oiText)); }
  virtual uint8			opSize() { return( (opInfo->oiOpcodeInformation >> 7) + 1); } 
  virtual void			opFormatToMemory(void* inStart, x86ArgumentList& /*inAL*/, x86ArgListInstruction& /*inInsn*/); 
  
  virtual bool			opHasRegFieldExtension( x86ArgumentList& /*inAL*/, x86ArgListInstruction& /*inInsn*/ ) { return (false); }
  virtual uint8			opGetRegFieldExtension() { assert(false); return 0; }

  virtual bool			opRegOperandCanBeCondensed() { return false; }

  virtual void			opReverseOperands() { assert(false); } 

  virtual void			opSwitchToRegisterIndirect() { }
  
  virtual bool			opCanAccept1ByteImmediate() { assert(false); return false; }
  virtual bool			opCanAccept4ByteImmediate() { assert(false); return false; }

protected:							
						x86Opcode() {}
						x86Opcode( x86ImmediateArithType inType )		{ opInfo = &(iaInfo[inType]); }
						x86Opcode( x86ExtendedType inType )				{ opInfo = &(eInfo[inType]); }
						x86Opcode( x86CondensableExtendedType inType )	{ opInfo = &(ceInfo[inType]); }
						x86Opcode( x86SpecialRegMemType inType )		{ opInfo = &(srmInfo[inType]); }
  x86OpcodeInfo* opInfo;
};

inline void x86Opcode::
opFormatToMemory(void* inStart, x86ArgumentList& /*inAL*/, x86ArgListInstruction& /*inInsn*/)
{
	uint8* start = (uint8*) inStart;
	if(opInfo->oiOpcodeInformation & kIs_2_Byte_Mask)
		*start++ = kPrefix_For_2_Byte;
	*start = opInfo->oiBaseOpcode;
}

/*--------------------------------------------------------------------------------
x86Opcode_Reg -- encodes x86EntendedType

	- no switchable bits
	- three bit opcode extension (stored in the REG field of the MODR/M byte)
*/
class x86Opcode_Reg :
  public virtual x86Opcode
{
public: 
						x86Opcode_Reg ( x86ExtendedType inInfo ) : x86Opcode(inInfo)  { }

  virtual bool			opHasRegFieldExtension( x86ArgumentList& /*inAL*/, x86ArgListInstruction& /*inInsn*/ ) { return true; }
  virtual uint8			opGetRegFieldExtension() { return (kRegfield_Mask & opInfo->oiOpcodeInformation); }

  virtual bool			opCanAccept1ByteImmediate() { return ((0x02 & opInfo->oiOpcodeInformation) == 0x02); }
  virtual bool			opCanAccept4ByteImmediate() { return ((0x01 & opInfo->oiOpcodeInformation) == 0x01); }

protected:							
						x86Opcode_Reg() { }
};

/*--------------------------------------------------------------------------------
x86Opcode_Condensable_Reg -- encodes x86CondensableExtendedType

	- no switchable bits
	- three bit opcode extension (stored in the REG field of the MODR/M byte)
	- alternate encoding which has a 5-bit opcode + 3-bit register operand
*/
class x86Opcode_Condensable_Reg :
  public x86Opcode
{
public: 
						x86Opcode_Condensable_Reg ( x86CondensableExtendedType inInfo ) : x86Opcode(inInfo)	 { }

  virtual bool			opHasRegFieldExtension( x86ArgumentList& inAL, x86ArgListInstruction& inInsn );
  virtual uint8			opGetRegFieldExtension() { return ((0x03 & opInfo->oiOpcodeInformation) << 3); }
  
  virtual bool			opCanAccept1ByteImmediate()	{ return false; }
  virtual bool			opCanAccept4ByteImmediate()	{ return true; }

  virtual bool			opRegOperandCanBeCondensed(){ return true; }

  virtual uint8			opSize() { return 1; } 
  virtual void			opFormatToMemory(void* inStart, x86ArgumentList& inAL, x86ArgListInstruction& inInsn); 
};

/*--------------------------------------------------------------------------------
x86Opcode_ImmArith -- encodes x86ImmediateArithType

	- must have sign/size bit which specifies 8 or 32 bits operand
	- _possible_ three bit opcode extension (stored in the REG field of the MODR/M byte)
*/
class x86Opcode_ImmArith :
  public x86Opcode
{
public:
						x86Opcode_ImmArith( x86ImmediateArithType inInfo ) : x86Opcode( inInfo ) { }
  virtual void			opFormatToMemory( void* inStart, x86ArgumentList& /*inAL*/, x86ArgListInstruction& /*inInsn*/);

  virtual bool			opHasRegFieldExtension( x86ArgumentList& /*inAL*/, x86ArgListInstruction& /*inInsn*/ )
							{ return  (bool)((opInfo->oiOpcodeInformation >> 6) && 1); }
  virtual uint8			opGetRegFieldExtension() 
							{ assert(kExtension_Mask & opInfo->oiOpcodeInformation); return (kRegfield_Mask & opInfo->oiOpcodeInformation); }
  virtual bool			opCanAccept1ByteImmediate() { return true; }
  virtual bool			opCanAccept4ByteImmediate() { return true; }

  virtual void			opSwitchToRegisterIndirect() { }	// FIX can't do this for imul ???

protected:							
						x86Opcode_ImmArith() { }
};

/*--------------------------------------------------------------------------------
x86Opcode_SpecialRegMem -- x86SpecialRegMemType

	- switches between 
		x86Opcode_Reg when operand is a memory argument, and
		x86Opcode when operand is a register argument
*/
class x86Opcode_SpecialRegMem :
  public x86Opcode_Reg		// note not x86Opcode!
{
public:
						x86Opcode_SpecialRegMem( x86SpecialRegMemType inType) : 
							x86Opcode(inType) { }

  virtual bool			opHasRegFieldExtension( x86ArgumentList& /*inAL*/, x86ArgListInstruction& /*inInsn*/ ) 
  						{ 
  							return((kIs_Memory_Form_Mask & opInfo->oiOpcodeInformation) == kIs_Memory_Form_Mask);
  						}
  							
  virtual uint8			opGetRegFieldExtension() 
  						{ 
  							assert(kIs_Memory_Form_Mask & opInfo->oiOpcodeInformation); 
  							return (kRegfield_Mask & opInfo->oiOpcodeInformation); 
  						}

  virtual void			opSwitchToRegisterIndirect()
  						{ 
  							assert(!(kIs_Memory_Form_Mask & opInfo->oiOpcodeInformation)); 
  							opInfo++; 
  						}
};

#endif // _X86OPCODE
