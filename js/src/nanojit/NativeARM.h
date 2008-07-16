/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef __nanojit_NativeArm__
#define __nanojit_NativeArm__


namespace nanojit
{
	const int NJ_LOG2_PAGE_SIZE	= 12;		// 4K
	#define NJ_MAX_REGISTERS				11
	#define NJ_MAX_STACK_ENTRY				256
	#define NJ_MAX_PARAMETERS				16
	#define NJ_ALIGN_STACK					8
	#define NJ_STACK_OFFSET					8

	#define NJ_SOFTFLOAT
	#define NJ_STACK_GROWTH_UP

	#define NJ_CONSTANT_POOLS
	const int NJ_MAX_CPOOL_OFFSET = 4096;
	const int NJ_CPOOL_SIZE = 16;

	typedef int NIns;

	/* ARM registers */
	typedef enum 
	{
		R0  = 0,
		R1  = 1,
		R2  = 2,
		R3  = 3,
		R4  = 4,
		R5  = 5,
		R6  = 6,
		R7  = 7,
		R8  = 8,
		R9  = 9,
		R10 = 10,
		//FP  =11,
		IP  = 12,
		SP  = 13,
		LR  = 14,
		PC  = 15,

		FP = 13,
		
		// Pseudo-register for floating point
		F0  = 0,

		// helpers
		FRAME_PTR = 11,
		ESP	= 13,
		
		FirstReg = 0,
		LastReg = 10,
		Scratch	= 12,
		UnknownReg = 11
	}
	Register;

	/* ARM condition codes */
	typedef enum
	{
		EQ = 0x0, // Equal
		NE = 0x1, // Not Equal
		CS = 0x2, // Carry Set (or HS)
		CC = 0x3, // Carry Clear (or LO)
		MI = 0x4, // MInus
		PL = 0x5, // PLus
		VS = 0x6, // oVerflow Set
		VC = 0x7, // oVerflow Clear
		HI = 0x8, // HIgher
		LS = 0x9, // Lower or Same
		GE = 0xA, // Greater or Equal
		LT = 0xB, // Less Than
		GT = 0xC, // Greater Than
		LE = 0xD, // Less or Equal
		AL = 0xE, // ALways
		NV = 0xF  // NeVer
	}
	ConditionCode;


	typedef int RegisterMask;
	typedef struct _FragInfo
	{
		RegisterMask	needRestoring;
		NIns*			epilogue;
	} 
	FragInfo;

	static const RegisterMask SavedRegs = 1<<R4 | 1<<R5 | 1<<R6 | 1<<R7 | 1<<R8 | 1<<R9 | 1<<R10;
	static const RegisterMask FpRegs = 0x0000; // FST0-FST7
	static const RegisterMask GpRegs = 0x07FF;
	static const RegisterMask AllowableFlagRegs = 1<<R0 | 1<<R1 | 1<<R2 | 1<<R3 | 1<<R4 | 1<<R5 | 1<<R6 | 1<<R7 | 1<<R8 | 1<<R9 | 1<<R10;

	#define firstreg()		R0
	#define nextreg(r)		(Register)((int)r+1)
	#define imm2register(c) (Register)(c-1)

	verbose_only( extern const char* regNames[]; )

	// abstract to platform specific calls
	#define nExtractPlatformFlags(x)	0

	#define DECLARE_PLATFORM_STATS() \
		counter_define(x87Top);

	#define DECLARE_PLATFORM_REGALLOC()


	#define DECLARE_PLATFORM_ASSEMBLER()\
		const static Register argRegs[4], retRegs[2];\
		void LD32_nochk(Register r, int32_t imm);\
		void CALL(const CallInfo*);\
		void underrunProtect(int bytes);\
		bool has_cmov;\
		void nativePageReset();\
		void nativePageSetup();\
		int* _nSlot;\
		int* _nExitSlot;


    #define asm_farg(i) NanoAssert(false)

	//printf("jmp_l_n count=%d, nins=%X, %X = %X\n", (_c), nins, _nIns, ((intptr_t)(nins+(_c))-(intptr_t)_nIns - 4) );

	#define swapptrs()  { NIns* _tins = _nIns; _nIns=_nExitIns; _nExitIns=_tins; \
								int* _nslot = _nSlot;\
								_nSlot = _nExitSlot;\
								_nExitSlot = _nslot;}


#define IMM32(imm)	*(--_nIns) = (NIns)((imm));

#define FUNCADDR(addr) ( ((int)addr) )	


#define OP_IMM	(1<<25)

#define COND_AL	(0xE<<28)

typedef enum
{
	LSL_imm = 0, // LSL #c - Logical Shift Left
	LSL_reg = 1, // LSL Rc - Logical Shift Left
	LSR_imm = 2, // LSR #c - Logical Shift Right
	LSR_reg = 3, // LSR Rc - Logical Shift Right
	ASR_imm = 4, // ASR #c - Arithmetic Shift Right
	ASR_reg = 5, // ASR Rc - Arithmetic Shift Right
	ROR_imm = 6, // Rotate Right (c != 0)
	RRX     = 6, // Rotate Right one bit with extend (c == 0)
	ROR_reg = 7  // Rotate Right
}
ShiftOperator;

#define LD32_size 4


#define BEGIN_NATIVE_CODE(x) \
	{ DWORD* _nIns = (uint8_t*)x

#define END_NATIVE_CODE(x) \
	(x) = (dictwordp*)_nIns; }

// BX 
#define BX(_r)	do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( COND_AL | (0x12<<20) | (0xFFF<<8) | (1<<4) | (_r));\
	asm_output("bx LR"); } while(0)

// _l = _r OR _l
#define OR(_l,_r)		do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( COND_AL | (0xC<<21) | (_r<<16) | (_l<<12) | (_l) );\
	asm_output2("or %s,%s",gpn(_l),gpn(_r)); } while(0)

// _r = _r OR _imm
#define ORi(_r,_imm)	do {\
	NanoAssert(isU8((_imm)));\
	underrunProtect(4); \
	*(--_nIns) = (NIns)( COND_AL | OP_IMM | (0xC<<21) | (_r<<16) | (_r<<12) | ((_imm)&0xFF) );\
	asm_output2("or %s,%d",gpn(_r), (_imm)); } while(0)

// _l = _r AND _l
#define AND(_l,_r) do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( COND_AL | ((_r)<<16) | ((_l)<<12) | (_l));\
	asm_output2("and %s,%s",gpn(_l),gpn(_r)); } while(0)

// _r = _r AND _imm
#define ANDi(_r,_imm) do {\
	if (isU8((_imm))) {\
		underrunProtect(4);\
		*(--_nIns) = (NIns)( COND_AL | OP_IMM | ((_r)<<16) | ((_r)<<12) | ((_imm)&0xFF) );\
		asm_output2("and %s,%d",gpn(_r),(_imm));}\
	else if ((_imm)<0 && (_imm)>-256) {\
		*(--_nIns) = (NIns)( COND_AL | ((_r)<<16) | ((_r)<<12) | (Scratch) );\
		asm_output2("and %s,%s",gpn(_r),gpn(Scratch));\
		*(--_nIns) = (NIns)( COND_AL | (0x3E<<20) | ((Scratch)<<12) | (((_imm)^0xFFFFFFFF)&0xFF) );\
		asm_output2("mvn %s,%d",gpn(Scratch),(_imm));}\
	else NanoAssert(0);\
	} while (0)


// _l = _l XOR _r
#define XOR(_l,_r)	do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( COND_AL | (1<<21) | ((_r)<<16) | ((_l)<<12) | (_l));\
	asm_output2("eor %s,%s",gpn(_l),gpn(_r)); } while(0)

// _r = _r XOR _imm
#define XORi(_r,_imm)	do {	\
	NanoAssert(isU8((_imm)));\
	underrunProtect(4);		\
	*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<21) | ((_r)<<16) | ((_r)<<12) | ((_imm)&0xFF) );\
	asm_output2("eor %s,%d",gpn(_r),(_imm)); } while(0)

// _l = _l + _r
#define ADD(_l,_r) do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( COND_AL | (1<<23) | ((_r)<<16) | ((_l)<<12) | (_l));\
	asm_output2("add %s,%s",gpn(_l),gpn(_r)); } while(0)

// _r = _r + _imm
#define ADDi(_r,_imm)	do {\
	if ((_imm)>-256 && (_imm)<256) {\
		underrunProtect(4);\
		if	((_imm)>=0) *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_r)<<16) | ((_r)<<12) | ((_imm)&0xFF) );\
		else			*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_r)<<16) | ((_r)<<12) | ((-(_imm))&0xFF) );}\
	else {\
		if ((_imm)>=0){\
			if ((_imm)<=1020 && (((_imm)&3)==0) ){\
				underrunProtect(4);\
				*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_r)<<16) | ((_r)<<12) | (15<<8)| ((_imm)>>2) );}\
			else {\
				underrunProtect(4+LD32_size);\
				*(--_nIns) = (NIns)( COND_AL | (1<<23) | ((_r)<<16) | ((_r)<<12) | (Scratch));\
				LD32_nochk(Scratch, _imm);}}\
		else{\
      if ((_imm)>=-510){\
			  underrunProtect(8);\
			  int rem = -(_imm) - 255;\
			  *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_r)<<16) | ((_r)<<12) | ((rem)&0xFF) );\
        *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_r)<<16) | ((_r)<<12) | (0xFF) );}\
      else {\
				underrunProtect(4+LD32_size);\
				*(--_nIns) = (NIns)( COND_AL | (1<<22) | ((_r)<<16) | ((_r)<<12) | (Scratch));\
        LD32_nochk(Scratch, -(_imm));}\
    }\
  }\
	asm_output2("addi %s,%d",gpn(_r),(_imm));} while(0)

// _l = _l - _r
#define SUB(_l,_r)	do {\
	underrunProtect(4); \
	*(--_nIns) = (NIns)( COND_AL | (1<<22) | ((_l)<<16) | ((_l)<<12) | (_r));\
	asm_output2("sub %s,%s",gpn(_l),gpn(_r)); } while(0)

// _r = _r - _imm
#define SUBi(_r,_imm)	do{\
	if ((_imm)>-256 && (_imm)<256){\
		underrunProtect(4);\
		if ((_imm)>=0)	*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_r)<<16) | ((_r)<<12) | ((_imm)&0xFF) );\
		else			*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_r)<<16) | ((_r)<<12) | ((-(_imm))&0xFF) );}\
	else {\
		if ((_imm)>=0){\
			if ((_imm)<=510){\
				underrunProtect(8);\
				int rem = (_imm) - 255;\
				NanoAssert(rem<256);\
				*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_r)<<16) | ((_r)<<12) | (rem&0xFF) );\
				*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_r)<<16) | ((_r)<<12) | (0xFF) );}\
			else {\
				underrunProtect(4+LD32_size);\
				*(--_nIns) = (NIns)( COND_AL | (1<<22) | ((_r)<<16) | ((_r)<<12) | (Scratch));\
				LD32_nochk(Scratch, _imm);}}\
		else{\
      if ((_imm)>=-510) {\
			  underrunProtect(8);\
			  int rem = -(_imm) - 255;\
			  *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_r)<<16) | ((_r)<<12) | ((rem)&0xFF) );\
			  *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_r)<<16) | ((_r)<<12) | (0xFF) );}\
      else {\
				underrunProtect(4+LD32_size);\
				*(--_nIns) = (NIns)( COND_AL | (1<<23) | ((_r)<<16) | ((_r)<<12) | (Scratch));\
				LD32_nochk(Scratch, -(_imm));}\
    }\
  }\
	asm_output2("sub %s,%d",gpn(_r),(_imm));} while (0)

// _l = _l * _r
#define MUL(_l,_r)	do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( COND_AL | (_l)<<16 | (_l)<<8 | 0x90 | (_r) );\
	asm_output2("mul %s,%s",gpn(_l),gpn(_r)); } while(0)


// RSBS
// _r = -_r
#define NEG(_r)	do {\
	underrunProtect(4);	\
	*(--_nIns) = (NIns)( COND_AL |  (0x27<<20) | ((_r)<<16) | ((_r)<<12) ); \
	asm_output1("neg %s",gpn(_r)); } while(0)

// MVNS
// _r = !_r
#define NOT(_r)	do {\
	underrunProtect(4);	\
	*(--_nIns) = (NIns)( COND_AL |  (0x1F<<20) | ((_r)<<12) |  (_r) ); \
	asm_output1("mvn %s",gpn(_r)); } while(0)

// MOVS _r, _r, LSR <_s>
// _r = _r >> _s
#define SHR(_r,_s) do {\
	underrunProtect(4); \
	*(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_s)<<8) | (LSR_reg<<4) | (_r) ); \
	asm_output2("shr %s,%s",gpn(_r),gpn(_s)); } while(0)

// MOVS _r, _r, LSR #_imm
// _r = _r >> _imm
#define SHRi(_r,_imm) do {\
	underrunProtect(4); \
	*(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_imm)<<7) | (LSR_imm<<4) | (_r) ); \
	asm_output2("shr %s,%d",gpn(_r),_imm); } while(0)

// MOVS _r, _r, ASR <_s>
// _r = _r >> _s
#define SAR(_r,_s) do {\
	underrunProtect(4); \
	*(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_s)<<8) | (ASR_reg<<4) | (_r) ); \
	asm_output2("asr %s,%s",gpn(_r),gpn(_s)); } while(0)


// MOVS _r, _r, ASR #_imm
// _r = _r >> _imm
#define SARi(_r,_imm) do {\
	underrunProtect(4); \
	*(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_imm)<<7) | (ASR_imm<<4) | (_r) ); \
	asm_output2("asr %s,%d",gpn(_r),_imm); } while(0)

// MOVS _r, _r, LSL <_s>
// _r = _r << _s
#define SHL(_r,_s) do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_s)<<8) | (LSL_reg<<4) | (_r) ); \
	asm_output2("lsl %s,%s",gpn(_r),gpn(_s)); } while(0)

// MOVS _r, _r, LSL #_imm
// _r = _r << _imm
#define SHLi(_r,_imm) do {\
	underrunProtect(4); \
	*(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_imm)<<7) | (LSL_imm<<4) | (_r) ); \
	asm_output2("lsl %s,%d",gpn(_r),(_imm)); } while(0)
					
// TST
#define TEST(_d,_s) do{\
	underrunProtect(4); \
	*(--_nIns) = (NIns)( COND_AL | (0x11<<20) | ((_d)<<16) | (_s) ); \
	asm_output2("test %s,%s",gpn(_d),gpn(_s));} while(0)

// CMP
#define CMP(_l,_r)	do{\
	underrunProtect(4); \
	*(--_nIns) = (NIns)( COND_AL | (0x015<<20) | ((_l)<<16) | (_r) ); \
	asm_output2("cmp %s,%s",gpn(_l),gpn(_r));} while(0)

// CMP (or CMN)
#define CMPi(_r,_imm)	do{\
	if (_imm<0) {	\
		if ((_imm)>-256) {\
			underrunProtect(4);\
			*(--_nIns) = (NIns)( COND_AL | (0x37<<20) | ((_r)<<16) | (-(_imm)) );}\
		else {\
			underrunProtect(4+LD32_size);\
			*(--_nIns) = (NIns)( COND_AL | (0x17<<20) | ((_r)<<16) | (Scratch) ); \
			LD32_nochk(Scratch, (_imm));}\
	} else {\
		if ((_imm)<256){\
			underrunProtect(4);\
			*(--_nIns) = (NIns)( COND_AL | (0x035<<20) | ((_r)<<16) | ((_imm)&0xFF) ); \
		} else {\
			underrunProtect(4+LD32_size);\
			*(--_nIns) = (NIns)( COND_AL | (0x015<<20) | ((_r)<<16) | (Scratch) ); \
			LD32_nochk(Scratch, (_imm));\
		}\
	}\
	asm_output2("cmp %s,%X",gpn(_r),(_imm)); } while(0)

// MOV
#define MR(_d,_s)	do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( COND_AL | (0xD<<21) | ((_d)<<12) | (_s) );\
	asm_output2("mov %s,%s",gpn(_d),gpn(_s)); } while (0)


#define MR_cond(_d,_s,_cond,_nm)	do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( ((_cond)<<28) | (0xD<<21) | ((_d)<<12) | (_s) );\
	asm_output2(_nm " %s,%s",gpn(_d),gpn(_s)); } while (0)

#define MREQ(dr,sr)	MR_cond(dr, sr, EQ, "moveq")
#define MRNE(dr,sr)	MR_cond(dr, sr, NE, "movne")
#define MRL(dr,sr)	MR_cond(dr, sr, LT, "movlt")
#define MRLE(dr,sr)	MR_cond(dr, sr, LE, "movle")
#define MRG(dr,sr)	MR_cond(dr, sr, GT, "movgt")
#define MRGE(dr,sr)	MR_cond(dr, sr, GE, "movge")
#define MRB(dr,sr)	MR_cond(dr, sr, CC, "movcc")
#define MRBE(dr,sr)	MR_cond(dr, sr, LS, "movls")
#define MRA(dr,sr)	MR_cond(dr, sr, HI, "movcs")
#define MRAE(dr,sr)	MR_cond(dr, sr, CS, "movhi")
#define MRNO(dr,sr)	MR_cond(dr, sr, VC, "movvc") // overflow clear
#define MRNC(dr,sr) MR_cond(dr, sr, CC, "movcc") // carry clear

#define LD(_d,_off,_b) do{\
	if ((_off)<0){\
	  underrunProtect(4);\
    NanoAssert((_off)>-4096);\
		*(--_nIns) = (NIns)( COND_AL | (0x51<<20) | ((_b)<<16) | ((_d)<<12) | ((-(_off))&0xFFF) );\
	} else {\
    if (isS16(_off) || isU16(_off)) {\
	    underrunProtect(4);\
      NanoAssert((_off)<4096);\
      *(--_nIns) = (NIns)( COND_AL | (0x59<<20) | ((_b)<<16) | ((_d)<<12) | ((_off)&0xFFF) );}\
    else {\
  	  underrunProtect(4+LD32_size);\
      *(--_nIns) = (NIns)( COND_AL | (0x79<<20) | ((_b)<<16) | ((_d)<<12) | Scratch );\
      LD32_nochk(Scratch, _off);}\
	}  asm_output3("ld %s,%d(%s)",gpn((_d)),(_off),gpn((_b))); }while(0)


#define LDi(_d,_imm) do {\
	if (isS8((_imm)) || isU8((_imm))) {	\
		underrunProtect(4);	\
		if ((_imm)<0)	*(--_nIns) = (NIns)( COND_AL | (0x3E<<20) | ((_d)<<12) | (((_imm)^0xFFFFFFFF)&0xFF) );\
		else			*(--_nIns) = (NIns)( COND_AL | (0x3B<<20) | ((_d)<<12) | ((_imm)&0xFF) );\
	} else {\
		underrunProtect(LD32_size);\
		LD32_nochk(_d, (_imm));\
	} asm_output2("ld %s,%d",gpn((_d)),(_imm)); } while(0)


// load 8-bit, zero extend (aka LDRB)
// note, only 5-bit offsets (!) are supported for this, but that's all we need at the moment
// (LDRB actually allows 12-bit offset in ARM mode but constraining to 5-bit gives us advantage for Thumb)
// @todo, untested!
#define LD8Z(_d,_off,_b) do{    \
    NanoAssert((d)>=0&&(d)<=31);\
    underrunProtect(4);\
    *(--_nIns) = (NIns)( COND_AL | (0x5D<<20) | ((_b)<<16) | ((_d)<<12) |  ((_off)&0xfff)  );\
    asm_output3("ldrb %s,%d(%s)", gpn(_d),(_off),gpn(_b));\
    } while(0)

#define ST(_b,_off,_r) do{\
	underrunProtect(4);	\
	if ((_off)<0)	*(--_nIns) = (NIns)( COND_AL | (0x50<<20) | ((_b)<<16) | ((_r)<<12) | ((-(_off))&0xFFF) );\
	else			*(--_nIns) = (NIns)( COND_AL | (0x58<<20) | ((_b)<<16) | ((_r)<<12) | ((_off)&0xFFF) );\
	asm_output3("str %s, %d(%s)",gpn(_r), (_off),gpn(_b)); } while(0)


#define STi(_b,_off,_imm) do{\
	NanoAssert((_off)>0);\
	if (isS8((_imm)) || isU8((_imm))) {	\
		underrunProtect(8);	\
	  *(--_nIns) = (NIns)( COND_AL | (0x58<<20) | ((_b)<<16) | ((Scratch)<<12) | ((_off)&0xFFF) );\
	  asm_output3("str %s, %d(%s)",gpn(Scratch), (_off),gpn(_b));			\
		if ((_imm)<0)	*(--_nIns) = (NIns)( COND_AL | (0x3E<<20) | (Scratch<<12) | (((_imm)^0xFFFFFFFF)&0xFF) );\
		else			*(--_nIns) = (NIns)( COND_AL | (0x3B<<20) | (Scratch<<12) | ((_imm)&0xFF) );\
    asm_output2("ld %s,%d",gpn((Scratch)),(_imm));	}\
  else {\
		underrunProtect(4+LD32_size);\
	  *(--_nIns) = (NIns)( COND_AL | (0x58<<20) | ((_b)<<16) | ((Scratch)<<12) | ((_off)&0xFFF) );\
	  asm_output3("str %s, %d(%s)",gpn(Scratch), (_off),gpn(_b));			\
    LD32_nochk(Scratch, (_imm));}\
 } while(0);


#define LEA(_r,_d,_b) do{						\
	NanoAssert((_d)<=1020);						\
	NanoAssert(((_d)&3)==0);						\
	if (_b!=SP) NanoAssert(0);					\
	if ((_d)<256) {								\
		underrunProtect(4);							\
		*(--_nIns) = (NIns)( COND_AL | (0x28<<20) | ((_b)<<16) | ((_r)<<12) | ((_d)&0xFF) );}\
	else{										\
		underrunProtect(8);							\
		*(--_nIns) = (NIns)( COND_AL | (0x4<<21) | ((_b)<<16) | ((_r)<<12) | (2<<7)| (_r) );\
		*(--_nIns) = (NIns)( COND_AL | (0x3B<<20) | ((_r)<<12) | (((_d)>>2)&0xFF) );}\
	asm_output2("lea %s, %d(SP)", gpn(_r), _d);	\
	} while(0)


//#define RET()   underrunProtect(1); *(--_nIns) = 0xc3;	asm_output("ret")
//#define NOP() 	underrunProtect(1); *(--_nIns) = 0x90;	asm_output("nop")
//#define INT3()  underrunProtect(1); *(--_nIns) = 0xcc;  asm_output("int3")
//#define RET() INT3()


// this is pushing a reg
#define PUSHr(_r)  do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( COND_AL | (0x92<<20) | (SP<<16) | (1<<(_r)) );	\
	asm_output1("push %s",gpn(_r)); } while (0)

// STMDB
#define PUSH_mask(_mask)  do {\
	underrunProtect(4);			\
	*(--_nIns) = (NIns)( COND_AL | (0x92<<20) | (SP<<16) | (_mask) );	\
	asm_output1("push %x", (_mask));} while (0)

// this form of PUSH takes a base + offset
// we need to load into scratch reg, then push onto stack
#define PUSHm(_off,_b)	do {\
	NanoAssert( (int)(_off)>0 );\
	underrunProtect(8);\
	*(--_nIns) = (NIns)( COND_AL | (0x92<<20) | (SP<<16) | (1<<(Scratch)) );	\
	*(--_nIns) = (NIns)( COND_AL | (0x59<<20) | ((_b)<<16) | ((Scratch)<<12) | ((_off)&0xFFF) );\
	asm_output2("push %d(%s)",(_off),gpn(_b)); } while (0)

#define POPr(_r) do {\
	underrunProtect(4);			\
	*(--_nIns) = (NIns)( COND_AL | (0x8B<<20) | (SP<<16) | (1<<(_r)) );\
	asm_output1("pop %s",gpn(_r));} while (0)

#define POP_mask(_mask) do {\
	underrunProtect(4);			\
	*(--_nIns) = (NIns)( COND_AL | (0x8B<<20) | (SP<<16) | (_mask) );\
	asm_output1("pop %x", (_mask));} while (0)

// takes an offset (right?)
#define JMP_long_nochk_offset(_off) do {\
	*(--_nIns) = (NIns)( COND_AL | (0xA<<24) | (((_off)>>2) & 0xFFFFFF) );	\
	asm_output1("jmp_l_n 0x%08x",(_off));} while (0)

// take an address, not an offset
#define JMP(t)	do {\
	underrunProtect(4);\
	intptr_t tt = (intptr_t)(t) - ((intptr_t)_nIns + 4);\
	*(--_nIns) = (NIns)( COND_AL | (0xA<<24) | (((tt)>>2) & 0xFFFFFF) );	\
	asm_output1("JMP 0x%08x\n",(unsigned int)(t)); } while (0)

#define JMP_nochk(t)	do {\
	intptr_t tt = (intptr_t)(t) - ((intptr_t)_nIns + 4);\
	*(--_nIns) = (NIns)( COND_AL | (0xA<<24) | (((tt)>>2) & 0xFFFFFF) );	\
	asm_output1("JMP 0x%08x\n",(unsigned int)(t)); } while (0)

#define JMP_long_placeholder()	do {JMP_long(0xffffffff); } while(0)

#define JMP_long(_t)	do {\
	underrunProtect(4);\
	*(--_nIns) = (NIns)( COND_AL | (0xA<<24) | (((_t)>>2) & 0xFFFFFF) );	\
	asm_output1("JMP_long 0x%08x\n", (unsigned int)(_t) ); } while (0)

#define BL(_t)	do {\
	underrunProtect(4);\
	intptr_t _tt = (intptr_t)(_t) - ((intptr_t)_nIns + 4);\
	*(--_nIns) = (NIns)( COND_AL | (0xB<<24) | (((_tt)>>2) & 0xFFFFFF) );	\
	asm_output2("BL 0x%08x offset=%d",(intptr_t)(_nIns) + (_tt),(_tt)) } while (0)


#define JMP_long_nochk(_t)	do {\
	intptr_t tt = (intptr_t)(_t) - ((intptr_t)_nIns + 4);\
	*(--_nIns) = (NIns)( COND_AL | (0xA<<24) | (((tt)>>2) & 0xFFFFFF) );	\
	asm_output1("JMP_l_n 0x%08x\n", (unsigned int)(_t)) } while (0)


#define B_cond(_c,_t)\
	underrunProtect(4);\
	intptr_t tt = (intptr_t)(_t) - ((intptr_t)_nIns + 4);\
	*(--_nIns) = (NIns)( ((_c)<<28) | (0xA<<24) | ((tt >>2)& 0xFFFFFF) );	\
	asm_output2("b(cond) 0x%08x (%tX)",(unsigned int)(_t), tt);


#define JA(t)	do {B_cond(HI,t); asm_output1("ja 0x%08x",(unsigned int)t); } while(0)
#define JNA(t)	do {B_cond(LS,t); asm_output1("jna 0x%08x",(unsigned int)t); } while(0)
#define JB(t)	do {B_cond(CC,t); asm_output1("jb 0x%08x",(unsigned int)t); } while(0)
#define JNB(t)	do {B_cond(CS,t); asm_output1("jnb 0x%08x",(unsigned int)t); } while(0)
#define JE(t)	do {B_cond(EQ,t); asm_output1("je 0x%08x",(unsigned int)t); } while(0)
#define JNE(t)	do {B_cond(NE,t); asm_output1("jne 0x%08x",(unsigned int)t); } while(0)						
#define JBE(t)	do {B_cond(LS,t); asm_output1("jbe 0x%08x",(unsigned int)t); } while(0)
#define JNBE(t) do {B_cond(HI,t); asm_output1("jnbe 0x%08x",(unsigned int)t); } while(0)
#define JAE(t)	do {B_cond(CS,t); asm_output1("jae 0x%08x",(unsigned int)t); } while(0)
#define JNAE(t) do {B_cond(CC,t); asm_output1("jnae 0x%08x",(unsigned int)t); } while(0)
#define JL(t)	do {B_cond(LT,t); asm_output1("jl 0x%08x",(unsigned int)t); } while(0)	
#define JNL(t)	do {B_cond(GE,t); asm_output1("jnl 0x%08x",(unsigned int)t); } while(0)
#define JLE(t)	do {B_cond(LE,t); asm_output1("jle 0x%08x",(unsigned int)t); } while(0)
#define JNLE(t)	do {B_cond(GT,t); asm_output1("jnle 0x%08x",(unsigned int)t); } while(0)
#define JGE(t)	do {B_cond(GE,t); asm_output1("jge 0x%08x",(unsigned int)t); } while(0)
#define JNGE(t)	do {B_cond(LT,t); asm_output1("jnge 0x%08x",(unsigned int)t); } while(0)
#define JG(t)	do {B_cond(GT,t); asm_output1("jg 0x%08x",(unsigned int)t); } while(0)	
#define JNG(t)	do {B_cond(LE,t); asm_output1("jng 0x%08x",(unsigned int)t); } while(0)
#define JC(t)	do {B_cond(CS,t); asm_output1("bcs 0x%08x",(unsigned int)t); } while(0)
#define JNC(t)	do {B_cond(CC,t); asm_output1("bcc 0x%08x",(unsigned int)t); } while(0)
#define JO(t)	do {B_cond(VS,t); asm_output1("bvs 0x%08x",(unsigned int)t); } while(0)
#define JNO(t)	do {B_cond(VC,t); asm_output1("bvc 0x%08x",(unsigned int)t); } while(0)

// used for testing result of an FP compare
// JP = comparison  false
#define JP(t)	do {B_cond(EQ,NE,t); asm_output1("jp 0x%08x",t); } while(0)	

// JNP = comparison true
#define JNP(t)	do {B_cond(NE,EQ,t); asm_output1("jnp 0x%08x",t); } while(0)


// floating point
#define FNSTSW_AX()	do {NanoAssert(0);		asm_output("fnstsw_ax"); } while(0)
#define FFREE(r)	do {NanoAssert(0);		asm_output1("ffree %s",gpn(b)); } while(0)
#define FSTQ(p,d,b)	do {NanoAssert(0);		asm_output2("fstq %d(%s)",d,gpn(b)); } while(0)
#define FSTPQ(d,b)  FSTQ(1,d,b)
//#define FSTPQ(d,b)	do {NanoAssert(0);		asm_output2("fstpq %d(%s)",d,gpn(b)); } while(0)
#define FCOM(p,d,b)	do {NanoAssert(0);		asm_output2("fcom %d(%s)",d,gpn(b)); } while(0)
#define FCOMP(d,b)	do {NanoAssert(0);		asm_output2("fcomp %d(%s)",d,gpn(b)); } while(0)
#define FLDQ(d,b)	do {NanoAssert(0);		asm_output2("fldq %d(%s)",d,gpn(b)); } while(0)
#define FILDQ(d,b)	do {NanoAssert(0);		asm_output2("fildq %d(%s)",d,gpn(b)); } while(0)
#define FILD(d,b)	do {NanoAssert(0);		asm_output2("fild %d(%s)",d,gpn(b)); } while(0)
#define FADD(d,b)	do {NanoAssert(0);		asm_output2("faddq %d(%s)",d,gpn(b)); } while(0)
#define FSUB(d,b)	do {NanoAssert(0);		asm_output2("fsubq %d(%s)",d,gpn(b)); } while(0)
#define FSUBR(d,b)	do {NanoAssert(0);		asm_output2("fsubr %d(%s)",d,gpn(b)); } while(0)
#define FMUL(d,b)	do {NanoAssert(0);		asm_output2("fmulq %d(%s)",d,gpn(b)); } while(0)
#define FDIV(d,b)	do {NanoAssert(0);		asm_output2("fdivq %d(%s)",d,gpn(b)); } while(0)
#define FDIVR(d,b)	do {NanoAssert(0);		asm_output2("fdivr %d(%s)",d,gpn(b)); } while(0)
#define FSTP(r)		do {NanoAssert(0);		asm_output1("fst st(%d)",r); } while(0)
#define FLD1()		do {NanoAssert(0);		asm_output("fld1"); } while(0)
#define FLDZ()		do {NanoAssert(0);		asm_output("fldz"); } while(0)



// MOV(EQ) _r, #1 
// EOR(NE) _r, _r
#define SET(_r,_cond,_opp)\
	underrunProtect(8);								\
	*(--_nIns) = (NIns)( (_opp<<28) | (1<<21) | ((_r)<<16) | ((_r)<<12) | (_r) );\
	*(--_nIns) = (NIns)( (_cond<<28) | (0x3A<<20) | ((_r)<<12) | (1) );


#define SETE(r)		do {SET(r,EQ,NE); asm_output1("sete %s",gpn(r)); } while(0)
#define SETL(r)		do {SET(r,LT,GE); asm_output1("setl %s",gpn(r)); } while(0)
#define SETLE(r)	do {SET(r,LE,GT); asm_output1("setle %s",gpn(r)); } while(0)
#define SETG(r)		do {SET(r,GT,LE); asm_output1("setg %s",gpn(r)); } while(0)
#define SETGE(r)	do {SET(r,GE,LT); asm_output1("setge %s",gpn(r)); } while(0)
#define SETB(r)		do {SET(r,CC,CS); asm_output1("setb %s",gpn(r)); } while(0)
#define SETBE(r)	do {SET(r,LS,HI); asm_output1("setb %s",gpn(r)); } while(0)
#define SETAE(r)	do {SET(r,CS,CC); asm_output1("setae %s",gpn(r)); } while(0)
#define SETA(r)		do {SET(r,HI,LS); asm_output1("seta %s",gpn(r)); } while(0)
#define SETO(r)		do {SET(r,VS,LS); asm_output1("seto %s",gpn(r)); } while(0)
#define SETC(r)		do {SET(r,CS,LS); asm_output1("setc %s",gpn(r)); } while(0)

// This zero-extends a reg that has been set using one of the SET macros,
// but is a NOOP on ARM/Thumb
#define MOVZX8(r,r2)

// Load and sign extend a 16-bit value into a reg
#define MOVSX(_d,_off,_b) do{\
	if ((_off)>=0){\
		if ((_off)<256){\
			underrunProtect(4);\
			*(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_b)<<16) | ((_d)<<12) |  ((((_off)>>4)&0xF)<<8) | (0xF<<4) | ((_off)&0xF)  );}\
		else if ((_off)<=510) {\
			underrunProtect(8);\
			int rem = (_off) - 255;\
			NanoAssert(rem<256);\
			*(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_d)<<16) | ((_d)<<12) |  ((((rem)>>4)&0xF)<<8) | (0xF<<4) | ((rem)&0xF)  );\
			*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_b)<<16) | ((_d)<<12) | (0xFF) );}\
		else {\
			underrunProtect(16);\
			int rem = (_off) & 3;\
			*(--_nIns) = (NIns)( COND_AL | (0x19<<20) | ((_b)<<16) | ((_d)<<12) | (0xF<<4) | (_d) );\
			asm_output3("ldrsh %s,[%s, #%d]",gpn(_d), gpn(_b), (_off));\
			*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_d)<<16) | ((_d)<<12) | rem );\
			*(--_nIns) = (NIns)( COND_AL | (0x1A<<20) | ((_d)<<12) | (2<<7)| (_d) );\
			*(--_nIns) = (NIns)( COND_AL | (0x3B<<20) | ((_d)<<12) | (((_off)>>2)&0xFF) );\
			asm_output2("mov %s,%d",gpn(_d),(_off));}}\
	else {\
		if ((_off)>-256) {\
			underrunProtect(4);\
			*(--_nIns) = (NIns)( COND_AL | (0x15<<20) | ((_b)<<16) | ((_d)<<12) |  ((((-(_off))>>4)&0xF)<<8) | (0xF<<4) | ((-(_off))&0xF)  );\
			asm_output3("ldrsh %s,[%s, #%d]",gpn(_d), gpn(_b), (_off));}\
		else if ((_off)>=-510){\
			underrunProtect(8);\
			int rem = -(_off) - 255;\
			NanoAssert(rem<256);\
			*(--_nIns) = (NIns)( COND_AL | (0x15<<20) | ((_d)<<16) | ((_d)<<12) |  ((((rem)>>4)&0xF)<<8) | (0xF<<4) | ((rem)&0xF)  );\
			*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_b)<<16) | ((_d)<<12) | (0xFF) );}\
		else NanoAssert(0);\
	}\
} while(0)

#define STMIA(_b, _mask) do {\
		  underrunProtect(2);\
		  NanoAssert(((_mask)&rmask(_b))==0 && isU8(_mask));\
      *(--_nIns) = (NIns)(COND_AL | (0x8A<<20) | ((_b)<<16) | (_mask)&0xFF);\
      asm_output2("stmia %s!,{%x}", gpn(_b), _mask);} while (0)

#define LDMIA(_b, _mask) do {\
      underrunProtect(2);\
 		  NanoAssert(((_mask)&rmask(_b))==0 && isU8(_mask));\
     *(--_nIns) = (NIns)(COND_AL | (0x8B<<20) | ((_b)<<16) | (_mask)&0xFF);\
      asm_output2("ldmia %s!,{%x}", gpn(_b), (_mask));} while (0)

/*
#define MOVSX(_d,_off,_b) do{\
	if ((_b)==SP){\
		NanoAssert( (_off)>=0 );\
		if ((_off)<256){\
			underrunProtect(4);\
			*(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_b)<<16) | ((_d)<<12) |  ((((_off)>>4)&0xF)<<8) | (0xF<<4) | ((_off)&0xF)  );}\
		else if ((_off)<=510) {\
			underrunProtect(8);\
			int rem = (_off) - 255;\
			NanoAssert(rem<256);\
			*(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_d)<<16) | ((_d)<<12) |  ((((rem)>>4)&0xF)<<8) | (0xF<<4) | ((rem)&0xF)  );\
			*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_b)<<16) | ((_d)<<12) | (0xFF) );}\
		else {\
			underrunProtect(16);\
			int rem = (_off) & 3;\
			*(--_nIns) = (NIns)( COND_AL | (0x19<<20) | ((_b)<<16) | ((_d)<<12) | (0xF<<4) | (_d) );\
			asm_output3("ldrsh %s,[%s, #%d]",gpn(_d), gpn(_b), (_off));\
			*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_d)<<16) | ((_d)<<12) | rem );\
			*(--_nIns) = (NIns)( COND_AL | (0x1A<<20) | ((_d)<<12) | (2<<7)| (_d) );\
			*(--_nIns) = (NIns)( COND_AL | (0x3B<<20) | ((_d)<<12) | (((_off)>>2)&0xFF) );\
			asm_output2("mov %s,%d",gpn(_d),(_off));}}\
	else {\
		if ((_off)>=0){\
			if ((_off)<256) {\
				underrunProtect(4);							\
				*(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_b)<<16) | ((_d)<<12) |  ((((_off)>>4)&0xF)<<8) | (0xF<<4) | ((_off)&0xF)  );\
				asm_output3("ldrsh %s,[%s, #%d]",gpn(_d), gpn(_b), (_off));}\
			else if ((_off)<=510) {\
				underrunProtect(8);\
				int rem = (_off) - 255;\
				NanoAssert(rem<256);\
				*(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_d)<<16) | ((_d)<<12) |  ((((rem)>>4)&0xF)<<8) | (0xF<<4) | ((rem)&0xF)  );\
				*(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_b)<<16) | ((_d)<<12) | (0xFF) );}\
			else NanoAssert(0);}\
		else {\
			if ((_off)>-256) {\
				*(--_nIns) = (NIns)( COND_AL | (0x15<<20) | ((_b)<<16) | ((_d)<<12) |  ((((-(_off))>>4)&0xF)<<8) | (0xF<<4) | ((-(_off))&0xF)  );\
				asm_output3("ldrsh %s,[%s, #%d]",gpn(_d), gpn(_b), (_off));}\
			else {}}\
	} while(0)
*/

}
#endif // __nanojit_NativeThumb__
