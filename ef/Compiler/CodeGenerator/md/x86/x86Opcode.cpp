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
	desantis
	holmes a court (commenting)
	
	x86Opcode.cpp

	NOTE:
	This file is being phased out -- please do not add to it without speaking to simon
*/

#include "CatchAssert.h"

#include "x86Opcode.h"
#include "x86ArgumentList.h"

// Not used anywhere
//	{ 0x69, 0x00, "imul " }				//iaMulImm		69		0000 0000

//================================================================================
/* Opcode Information

Each OpcodeInfo consists of 
	x86OpcodeInfo { 
		oiBaseOpcode			// generally the opcode, but can have additional info
		oiOpcodeInformation		// additional info about opcode
		oiText 					// string for fake disassembly
	}
	
Field bits are referred to as [7 6 5 4  3 2 1 0]

--------------------------------------------------------------------------------
ImmediateArithType
eg:
	1000 00s1: 11 111 reg:immdata			immediate with register
	1000 00s1:mod 111 r/m:immdata			immediate with memory
	
oiBaseOpcode
	contains the opcode for the instruction
	1 (Size)	always 0 (assigned at format time)

oiOpcodeInformation
	7 			set if two byte opcode
	6 			set if has an opcode extension
	[5 4 3] 	opcode extension of the instructrion 
	1			set if must take a 1-byte immediate
 	0 			set if must take a 4-byte immediate
	others		set to 0
*/
x86OpcodeInfo iaInfo[] =
{
										//						  ** *
	{ 0x81, 0x40, "add " },				//iaAddImm		81/0	0100 0000
	{ 0x81, 0x78, "cmp " },				//iaCmpImm		81/7	0111 1000
	{ 0xe9, 0x40, "jmp " },				//iaJmp			e9/0	0100 0000
	{ 0x68, 0x40, "push "},				//iaPushImm		68/0	0100 0000
	{ 0x81, 0x60, "and "},				//iaAndImm		81/4	0110 0000
	{ 0x81, 0x48, "or  "},				//iaOrImm		81/1	0100 1000
	{ 0x81, 0x70, "xor "},				//iaXorImm		81/6	0111 0000
};

/*--------------------------------------------------------------------------------
ExtendedType
eg
	1100 0001: 11 111 reg:imm8data			reg by immediate count
	1100 0001:mod 111 r/m:imm8data			memory by immediate count

oiBaseOpcode
	contains the opcode for the instruction
	
oiOpcodeInformation
	7			set if two byte opcode
	[5 4 3]		contain Regfield opcode extension
	1			set if can take a 1-byte immediate
 	0 			set if can take a 4-byte immediate
	others		set to 0
*/
x86OpcodeInfo eInfo[] =
{										//					      ** *
	{ 0xf7, 0x38, "idiv eax, "  },		//eIDiv			f7/7	0011 1000
	{ 0xf7, 0x30, "div eax, "  },		//eDiv			f7/6	0011 0000
	{ 0xf7, 0x20, "div eax, "  },		//eMul			f7/4	0010 0000
    { 0xc7, 0x01, "movImm " },			//eMoveImm		c7/0    0000 0001	// used when won't be condensable
	{ 0xff, 0x30, "push "},				//ePush			ff/6	0011 0000 	// FIX Peter? (what's wrong?)
	{ 0xc1, 0x3a, "sar "},				//eSarImm		c1/7	0011 1010
	{ 0xc1, 0x2a, "shr "},				//eShrImm		c1/5	0010 1010
	{ 0xc1, 0x22, "shl "},				//eShlImm		c1/4	0010 0010
	{ 0xd1, 7<<3, "sar(by1) "},			//eSar1			c1/7	0011 1000	
	{ 0xd1, 5<<3, "shr(by1) "},			//eShr1			c1/5	0010 1000
	{ 0xd1, 4<<3, "shl(by1) "},			//eShl1			c1/4	0010 0000
	{ 0xd3, 7<<3, "sar(bycl) "},		//eSarCl		c1/7	0011 1000	
	{ 0xd3, 5<<3, "shr(bycl) "},		//eShrCl		c1/5	0010 1000
	{ 0xd3, 4<<3, "shl(bycl) "},		//eShlCl		c1/4	0010 0000
	{ 0xf7, 3<<3, "neg "},				//eNeg			f7/3	0001 1000
};

/*--------------------------------------------------------------------------------
CondensableExtendedType
Like ExtendedOpcodes but have alternate (condensed) encoding: 5-bit opcode + 3-bit reg operand
eg
	0100 0 rg								reg (preferred encoding)
	1111 1111:mod 000 r/m					memory

oiBaseOpcode
	contains the opcode for the instruction

oiOpcodeInformation
	[7 6 5 4 3]	register condensed form opcode
	[2 1 0]		regfield extension in extended form  

NOTE: These opcodes cannot be two bytes and must take 4 BYTE IMMEDIATES
*/

x86OpcodeInfo ceInfo[] =
{										//			norm condns <cond><x>
	{ 0xff, 0x40, "inc "  },			//ceInc		ff/0 40+reg	0100 0000
	{ 0xff, 0x49, "dec " },				//ceDec		ff/1 48+reg	0100 1001
    { 0xc7, 0xb8, "movImm " },			//ceMoveImm	c7/0 b8+reg 1011 1000
	{ 0xff, 0x46, "pop "  }				//cePop		ff/0 40+reg	0101 0110			// 50+rd, ff/6
};


/*--------------------------------------------------------------------------------
SpecialRegMemType

eg [cmp vrx, 0] is the same as [test vrx, vrx] but the latter saves a byte
eg [mov vrx, 0] = [xor  vrx, vrx] but the latter saves 4 bytes

Optimisations like the above can only be applied to reg/reg

oiBaseOpcode
	contains the opcode for the instruction

oiOpcodeInformation
	7			set if two byte opcode
	6			0 means register (StandardType) form
				1 means memory (ExtendedType) form
	[5 4 3]		contain Regfield opcode extension for ExtendedType form

	1			set if can take a 1-byte immediate
 	0 			set if can take a 4-byte immediate
	
Pairing
	register form followed by memory form  
*/

x86OpcodeInfo srmInfo[] =
{
	// cmp vrx, 0 = test vrx, vrx  (save 1 byte)
	{0x85, 0x00, "test "},				//srmCmpImm0	85/r 0000 0000	(2 bytes)
	{0x83, 0x7a, "cmp "},				//srm1NOTUSED	83/7 0111 1010	(2+1 bytes)

	// mov vrx, 0 = xor  vrx, vrx  (save 4 bytes)
	{0x31, 0x00, "xor "},				//srmMoveImm0	31/r 0000 0000	(2 bytes)
	{0xc7, 0x41, "mov "}				//srm3NOTUSED	c7/0 0100 0001	(2+4 bytes)
};

//================================================================================
// Member functions 

/*--------------------------------------------------------------------------------
x86Opcode_ImmArith -- encodes x86ImmediateArithType

	- Sign/size bit which specifies 8 or 32 bits operand
	- _possible_ three bit opcode extension (stored in the REG field of the MODR/M byte)
*/
void x86Opcode_ImmArith::
opFormatToMemory(void* inStart, x86ArgumentList& inAL, x86ArgListInstruction& /*inInsn*/ )
{
  Uint8* start = (Uint8*) inStart;

  if(opInfo->oiOpcodeInformation & kIs_2_Byte_Mask)
    *start++ = kPrefix_For_2_Byte;

  switch(inAL.alSizeOfImmediate())
  {
  case(is4ByteImmediate):
	*start = opInfo->oiBaseOpcode;
	break;
	
  case(is1ByteImmediate):
	*start = opInfo->oiBaseOpcode | (0x02);
	break;
	
  default:
	assert(false);  // FIX what other possibilities are there?
	break;
  }
}

//--------------------------------------------------------------------------------
//x86Opcode_Condensable_Reg -- encodes x86CondensableExtendedType
void x86Opcode_Condensable_Reg::
opFormatToMemory(void* inStart, x86ArgumentList& inAL, x86ArgListInstruction& inInsn)
{
  Uint8* start = (Uint8*) inStart;
  Uint8 reg = inAL.alGetRegisterOperand(inInsn);

  if(reg == NOT_A_REG)	// Operand is not a register
	*start = opInfo->oiBaseOpcode;
  else
	*start = (opInfo->oiOpcodeInformation & kCondensed_Op_Mask) | reg;	// 5-bit opcode + 3-bit reg
}

bool x86Opcode_Condensable_Reg::
opHasRegFieldExtension(x86ArgumentList& inAL, x86ArgListInstruction& inInsn)
{ 
	Uint8 reg = inAL.alGetRegisterOperand( inInsn );
	if(reg == NOT_A_REG)	// Operand is not a register
		return true;
	return false;
}

//--------------------------------------------------------------------------------
