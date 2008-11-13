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


#ifndef __nanojit_NativeAMD64__
#define __nanojit_NativeAMD64__

#include <limits.h>

namespace nanojit
{
	const int NJ_LOG2_PAGE_SIZE	= 12;		// 4K
	const int NJ_MAX_REGISTERS = 32; // gpregs, x87 regs, xmm regs
	
	// WARNING: setting this allows the NJ to growth memory as needed without bounds
	const bool NJ_UNLIMITED_GROWTH	= true;

	#define NJ_MAX_STACK_ENTRY 256
	#define NJ_MAX_PARAMETERS 1

	/* Stack is always aligned to 16-bit on x64 */
	const int NJ_ALIGN_STACK = 16;
	
	typedef uint8_t NIns;

	// These are used as register numbers in various parts of the code
	typedef enum
	{
		// general purpose 32bit regs
		RAX = 0, // return value, scratch
		RCX = 1, // this/arg0, scratch
		RDX = 2, // arg1, return-msw, scratch
		RBX = 3,
		RSP = 4, // stack pointer
		RBP = 5, // frame pointer
		RSI = 6,
		RDI = 7,

		SP = RSP, // alias SP to RSP for convenience
		FP = RBP, // alias FP to RBP for convenience

		R8 = 8,
		R9 = 9,
		R10 = 10,
		R11 = 11,
		R12 = 12,
		R13 = 13,
		R14 = 14,
		R15 = 15,

		XMM0 = 16,
		XMM1 = 17,
		XMM2 = 18,
		XMM3 = 19,
		XMM4 = 20,
		XMM5 = 21,
		XMM6 = 22,
		XMM7 = 23,
		XMM8 = 24,
		XMM9 = 25,
		XMM10 = 26,
		XMM11 = 27,
		XMM12 = 28,
		XMM13 = 29,
		XMM14 = 30,
		XMM15 = 31,

		FirstReg = 0,
		LastReg = 31,
		UnknownReg = 32
	} 
	Register;

	typedef int RegisterMask;

	/* RBX, R13-R15 */
	static const int NumSavedRegs = 3;
	static const RegisterMask SavedRegs = /*(1<<RBX) |*/ /*(1<<R12) |*/ (1<<R13) | (1<<R14) | (1<<R15);
	/* RAX, RCX, RDX, RDI, RSI, R8-R11 */
	static const RegisterMask TempRegs = (1<<RAX) | (1<<RCX) | (1<<RDX) | (1<<R8) | (1<<R9) | (1<<R10) | (1<<R11) | (1<<RDI) | (1<<RSI);
	static const RegisterMask GpRegs = SavedRegs | TempRegs;
	/* XMM0-XMM7 */
	static const RegisterMask XmmRegs = (1<<XMM0) | (1<<XMM1) | (1<<XMM2) | (1<<XMM3) | (1<<XMM4) | (1<<XMM5) | (1<<XMM6) | (1<<XMM7) | (1<<XMM8) | (1<<XMM9) | (1<<XMM10) | (1<<XMM11) | (1<<XMM13) | (1<<XMM14) | (1<<XMM15);
	static const RegisterMask FpRegs = XmmRegs;
	static const RegisterMask ScratchRegs = TempRegs | XmmRegs;

	static const RegisterMask AllowableFlagRegs = 1<<RAX |1<<RCX | 1<<RDX | 1<<RBX;

    #if defined WIN64
    typedef __int64 nj_printf_ld;
    #else
    typedef long int nj_printf_ld;
    #endif

	#define _rmask_(r)		(1<<(r))
	#define _is_xmm_reg_(r)	((_rmask_(r)&XmmRegs)!=0)
	#define _is_fp_reg_(r)	((_rmask_(r)&FpRegs)!=0)
	#define _is_gp_reg_(r)	((_rmask_(r)&GpRegs)!=0)

	#define nextreg(r)		Register(r+1)
	#define prevreg(r)		Register(r-1)
	
	verbose_only( extern const char* regNames[]; )

	#define DECLARE_PLATFORM_STATS()

	#define DECLARE_PLATFORM_REGALLOC()

    #if !defined WIN64
    #define DECLARE_PLATFORM_ASSEMBLER_START() \
        const static Register argRegs[6], retRegs[2];
    #else
    #define DECLARE_PLATFORM_ASSEMBLER_START() \
        const static Register argRegs[4], retRegs[2];
    #endif

	#if !defined WIN64
	#define DECLARE_PLATFORM_ASSEMBLER()	\
        DECLARE_PLATFORM_ASSEMBLER_START()  \
		void nativePageReset();				\
		void nativePageSetup();				\
        void asm_farg(LInsp);				\
        void asm_qbinop(LInsp);             \
		int32_t _pageData;					\
		NIns *_dblNegPtr;					\
		NIns *_negOnePtr;					\
        NIns *overrideProtect;              
	#endif
		
	#define swapptrs()  { NIns* _tins = _nIns; _nIns=_nExitIns; _nExitIns=_tins; }
		
	// enough room for n bytes
	#define underrunProtect(n)									\
		{														\
			intptr_t u = n + (sizeof(PageHeader)/sizeof(NIns) + _pageData + 5); \
			if ( !samepage(_nIns-u,_nIns-1) )					\
			{													\
				NIns *tt = _nIns; 								\
				_nIns = pageAlloc(_inExit);	    				\
                if (!_inExit) {                                 \
				    _pageData = 0;								\
				    _dblNegPtr = NULL;							\
				    _negOnePtr = NULL;							\
                }                                               \
				intptr_t d = tt-_nIns; 							\
                if (d <= INT_MAX && d >= INT_MIN) {             \
				    JMP_long_nochk_offset(d);					\
                } else {                                        \
                    /* Insert a 64-bit jump... */               \
                    _nIns -= 8;                                 \
                    *(intptr_t *)_nIns = intptr_t(tt);          \
                    JMPm_nochk(0);                              \
                }                                               \
			}													\
            overrideProtect = _nIns;                            \
		}

#define AMD64_NEEDS_REX(x)				(((x) & 8) == 8)
#define AMD64_ENCODE_REG(x)				((x) & 7)

#define AMD64_MODRM(mode, reg, rm)	(uint8_t)(((mode & 3) << 6) | ((reg & 7) << 3) | (rm & 7))
#define AMD64_ALUOP(x)				((x)>>3)

/**
 * Returns a REX prefix.
 *
 * q				1 for 64-bit, 0 for default.
 * mdReg			ModRM register field.
 * mdRm				ModRM R/M field, or opcode register field.
 */
#define AMD64_REX(q,mdReg,mdRm)		(uint8_t)(0x40 | (q << 3) | (((mdReg >> 3) & 1) << 2) | ((mdRm >> 3) & 1))

#define AMD64_ADD_RAX				0x05		/* imm */
#define AMD64_ADD_REG_RM			0x03		/* rm/r */
#define AMD64_ADDSD					0xF20F58	/* rm/r */
#define AMD64_ALU8					0x83		/* rm/x imm8 */
#define AMD64_ALU32					0x81		/* rm/x imm32 */
#define AMD64_AND_RAX				0x25		/* imm */
#define AMD64_AND_REG_RM			0x23		/* rm/r */
#define AMD64_CMP_RAX				0x3D		/* imm */
#define AMD64_CMP_REG_RM			0x3B		/* rm/r */
#define AMD64_CVTSI2SD				0xF20F2A	/* rm/r */
#define AMD64_DIVSD					0xF20F5E	/* rm/r */
#define AMD64_IMUL_REG_RM_1			0x0F		/* see next */
#define AMD64_IMUL_REG_RM_2			0xAF		/* rm/r */
#define AMD64_INT3					0xCC		/* */
#define AMD64_LEA					0x8D		/* rm/r */
#define AMD64_MOV_REG_IMM(x)		(0xB8 | AMD64_ENCODE_REG(x))
#define AMD64_MOV_REG_RM			0x8B		/* rm/r */
#define AMD64_MOV_RM_REG			0x89		/* rm/r */
#define AMD64_MOV_RM_IMM			0xC7		/* rm/0 imm */
#define AMD64_MOVD_REG_RM			0x660F6E	/* rm/r */
#define AMD64_MOVD_RM_REG			0x660F7E	/* rm/r */
#define AMD64_MOVSD_REG_RM			0xF20F10	/* rm/r */
#define AMD64_MOVZX					0x0FB6		/* rm/r */
#define AMD64_MOVSZ                 0x0FB7      /* rm/r */
#define AMD64_MULSD					0xF20F59	/* rm/r */
#define AMD64_NEG_RM				0xF7		/* rm/3 */
#define AMD64_NOT_RM				0xF7		/* rm/2 */
#define AMD64_OR_RAX				0x0D		/* imm */
#define AMD64_OR_REG_RM				0x0B		/* rm/r */
#define AMD64_POP_REG(x)			(0x58 | AMD64_ENCODE_REG(x))
#define AMD64_PUSH_REG(x)			(0x50 | AMD64_ENCODE_REG(x))
#define AMD64_PUSH_RM				0xFF		/* rm/6 */
#define AMD64_PUSHF					0x9C		/* */
#define AMD64_RET					0xC3		/* */
#define AMD64_SAR_RM_1				0xD1		/* rm/7 */
#define AMD64_SAR_RM				0xD3		/* rm/7 */
#define AMD64_SAR_RM_IMM8			0xC1		/* rm/7 */
#define AMD64_SHL_RM_1				0xD1		/* rm/4 */
#define AMD64_SHL_RM				0xD3		/* rm/4 */
#define AMD64_SHL_RM_IMM8			0xC1		/* rm/4 imm */
#define AMD64_SHR_RM_1				0xD1		/* rm/5 */
#define AMD64_SHR_RM				0xD3		/* rm/5 */
#define AMD64_SHR_RM_IMM8			0xC1		/* rm/5 */
#define AMD64_SUB_RAX				0x2D		/* imm */
#define AMD64_SUB_REG_RM			0x2B		/* rm/r */
#define AMD64_SUBSD					0xF20F5C	/* rm/r */
#define AMD64_TEST_RM_REG			0x85		/* rm/r */
#define AMD64_UCOMISD				0x660F2E	/* rm/r */
#define AMD64_XOR_RAX				0x35		/* imm */
#define AMD64_XOR_REG_RM			0x33		/* rm/r */
#define AMD64_XORPD					0x660F57	/* rm/r */


#define IMM32(i)	\
	_nIns -= 4;		\
	*((int32_t*)_nIns) = (int32_t)(i)


#define IMM64(i)	\
	_nIns -= 8;		\
	*((int64_t*)_nIns) = (int64_t)(i)


#define AMD64_MODRM_REG(reg, rm)	AMD64_MODRM(3, reg, rm)


#define AMD64_MODRM_DISP(reg, rm, disp)							\
	if ((disp) == 0 && (((rm) & 0x7) != 5)) {					\
		*(--_nIns) = AMD64_MODRM(0, reg, rm);					\
	} else if (isS8(disp)) {									\
		*(--_nIns) = int8_t(disp);								\
		*(--_nIns) = AMD64_MODRM(1, reg, rm);					\
	} else {													\
		IMM32(disp);											\
		*(--_nIns) = AMD64_MODRM(2, reg, rm);					\
	}
		


#define AMD64_ALU(op, reg, imm, q) 								\
	underrunProtect(7);											\
	if (isS8(imm)) {											\
		*(--_nIns) = uint8_t(imm);								\
		*(--_nIns) = AMD64_MODRM_REG(AMD64_ALUOP(op), reg);		\
		*(--_nIns) = AMD64_ALU8;								\
	} else {													\
		IMM32(imm);												\
		if (reg == RAX) {										\
			*(--_nIns) = op;									\
		} else {												\
			*(--_nIns) = AMD64_MODRM_REG(AMD64_ALUOP(op), reg);	\
			*(--_nIns) = AMD64_ALU32;							\
		}														\
	}															\
	if (q || AMD64_NEEDS_REX(reg)) {							\
		*(--_nIns) = AMD64_REX(q,0,reg);						\
	}


#define AMD64_ALU_MEM(op, reg, disp, imm, q)					\
	underrunProtect(11);										\
	if (isS8(imm)) {											\
		*(--_nIns) = uint8_t(imm);								\
	} else {													\
		IMM32(imm);												\
	}															\
	AMD64_MODRM_DISP(AMD64_ALUOP(op), reg, disp);				\
	if (isS8(imm)) {											\
		*(--_nIns) = AMD64_ALU8;								\
	} else {													\
		*(--_nIns) = AMD64_ALU32;								\
	}															\
	if (q || AMD64_NEEDS_REX(reg)) {							\
		*(--_nIns) = AMD64_REX(q,0,reg);						\
	}


#define NOT(r) do { 											\
	underrunProtect(3);											\
	*(--_nIns) = AMD64_MODRM_REG(2, r);							\
	*(--_nIns) = AMD64_NOT_RM;									\
	*(--_nIns) = AMD64_REX(1,0,r);								\
	asm_output("not %s",gpn(r)); 								\
	} while(0)


#define MOVZX8(d,s) do { 										\
	underrunProtect(4);											\
	*(--_nIns) = AMD64_MODRM_REG(d, s);							\
	*(--_nIns) = AMD64_MOVZX & 0xFF;							\
	*(--_nIns) = AMD64_MOVZX >> 8;								\
	if (AMD64_NEEDS_REX(s) || AMD64_NEEDS_REX(d)) {				\
		*(--_nIns) = AMD64_REX(0,d,s);							\
	}															\
	asm_output("movzx %s,%s", gpn(d),gpn(s)); 					\
	} while(0)


#define AMD64_ADDmi(d,b,i,q) do {								\
	AMD64_ALU_MEM(AMD64_ADD_RAX, b, d, i, q);					\
	asm_output("add %d(%s), %d", d, gpn(b), i); 				\
	} while(0)


#define ADDQmi(d,b,i) do {										\
	AMD64_ADDmi(d,b,i,1);										\
	asm_output("add %d(%s), %d", d, gpn(b), i);				\
	} while (0)


#define CMPi(r,i) do { 											\
	AMD64_ALU(AMD64_CMP_RAX, r, i, 0);							\
	asm_output("cmp %s,%d",gpn(r),i); 							\
	} while(0)


#define TEST(d,s) do { 											\
	underrunProtect(3);											\
	*(--_nIns) = AMD64_MODRM_REG(d, s);							\
	*(--_nIns) = AMD64_TEST_RM_REG;								\
	if (AMD64_NEEDS_REX(d) || AMD64_NEEDS_REX(s)) {				\
		*(--_nIns) = AMD64_REX(0,d,s);							\
	}															\
	asm_output("test %s,%s",gpn(d),gpn(s)); 					\
	} while(0)


#define TESTQ(d,s) do { 										\
	underrunProtect(3);											\
	*(--_nIns) = AMD64_MODRM_REG(d, s);							\
	*(--_nIns) = AMD64_TEST_RM_REG;								\
	*(--_nIns) = AMD64_REX(1,d,s);							\
	asm_output("test %s,%s",gpn(d),gpn(s)); 					\
	} while(0)


#define CMP(l,r) do {											\
	underrunProtect(3);											\
	*(--_nIns) = AMD64_MODRM_REG(l, r);							\
	*(--_nIns) = AMD64_CMP_REG_RM;								\
	if (AMD64_NEEDS_REX(l) || AMD64_NEEDS_REX(r)) {				\
		*(--_nIns) = AMD64_REX(0,l,r);							\
	}															\
	asm_output("cmp %s,%s",gpn(l),gpn(r));						\
	} while(0)

#define CMPQ(l,r) do {											\
	underrunProtect(3);											\
	*(--_nIns) = AMD64_MODRM_REG(l, r);							\
	*(--_nIns) = AMD64_CMP_REG_RM;								\
	*(--_nIns) = AMD64_REX(1,l,r);							\
	asm_output("cmp %s,%s",gpn(l),gpn(r));						\
	} while(0)

#define ADDi(r,i) do { 											\
	AMD64_ALU(AMD64_ADD_RAX, r, i, 0);							\
	asm_output("add %s,%d",gpn(r),i); 							\
	} while(0)


#define ADDQi(r,i) do { 										\
	AMD64_ALU(AMD64_ADD_RAX, r, i, 1);							\
	asm_output("add %s,%d",gpn(r),i); 							\
	} while(0)


#define AMD64_PRIM(op,l,r) 										\
	underrunProtect(3);											\
	*(--_nIns) = AMD64_MODRM_REG(l, r);							\
	*(--_nIns) = op;											\
	if (AMD64_NEEDS_REX(l) || AMD64_NEEDS_REX(r))				\
		*(--_nIns) = AMD64_REX(0,l,r);


#define AMD64_PRIMQ(op,l,r) 									\
	underrunProtect(3);											\
	*(--_nIns) = AMD64_MODRM_REG(l, r);							\
	*(--_nIns) = op;											\
	*(--_nIns) = AMD64_REX(1,l,r);


#define SHR(r,s) do {											\
	AMD64_PRIM(AMD64_SHR_RM, 5, r);								\
	asm_output("shr %s,%s",gpn(r),gpn(s)); 					\
	} while(0)


#define AND(l,r) do { 											\
	AMD64_PRIM(AMD64_AND_REG_RM, l, r);							\
	asm_output("and %s,%s",gpn(l),gpn(r)); 					\
	} while(0)


#define ANDQ(l,r) do { 											\
	AMD64_PRIMQ(AMD64_AND_REG_RM, l, r);						\
	asm_output("and %s,%s",gpn(l),gpn(r)); 					\
	} while(0)


#define XOR(l,r) do { 											\
	AMD64_PRIM(AMD64_XOR_REG_RM, l, r);							\
	asm_output("xor %s,%s",gpn(l),gpn(r));						\
	} while(0)


#define OR(l,r) do { 											\
	AMD64_PRIM(AMD64_OR_REG_RM, l, r);							\
	asm_output("or %s,%s",gpn(l),gpn(r)); 						\
	} while(0)


#define ORQ(l,r) do { 											\
	AMD64_PRIMQ(AMD64_OR_REG_RM, l, r);							\
	asm_output("or %s,%s",gpn(l),gpn(r)); 						\
	} while(0)


#define SHRi(r,i) do { 											\
	if (i == 1) {												\
		underrunProtect(3);										\
		*(--_nIns) = AMD64_MODRM_REG(5, r);						\
		*(--_nIns) = AMD64_SHR_RM_1;							\
	} else {													\
		underrunProtect(4);										\
		*(--_nIns) = uint8_t(i);								\
		*(--_nIns) = AMD64_MODRM_REG(5, r);						\
		*(--_nIns) = AMD64_SHR_RM_IMM8;							\
	}															\
	if (AMD64_NEEDS_REX(r))										\
		*(--_nIns) = AMD64_REX(0,0,r);							\
	asm_output("shr %s,%d", gpn(r), i);						\
	} while (0)


#define ANDi(r,i) do { 											\
	AMD64_ALU(AMD64_AND_RAX, r, i, 0);							\
	asm_output("and %s,%d",gpn(r),i);							\
	} while(0)


#define ANDQi(r,i) do { 										\
	AMD64_ALU(AMD64_AND_RAX, r, i, 1);							\
	asm_output("and %s,%d",gpn(r),i);							\
	} while(0)


#define ORQi(r,i) do { 										    \
	AMD64_ALU(AMD64_OR_RAX, r, i, 1);							\
	asm_output("or %s,%d",gpn(r),i);							\
	} while(0)

#define XORi(r,i) do { 											\
	AMD64_ALU(AMD64_XOR_RAX, r, i, 0);							\
	asm_output("xor %s,%d",gpn(r),i); 							\
	} while(0)


#define ORi(r,i) do { 											\
	AMD64_ALU(AMD64_OR_RAX, r, i, 0);							\
	asm_output("or %s,%d",gpn(r),i);							\
	} while(0)


#define MUL(l,r) do {											\
	underrunProtect(4);											\
	*(--_nIns) = AMD64_MODRM_REG(l, r);							\
	*(--_nIns) = AMD64_IMUL_REG_RM_2;							\
	*(--_nIns) = AMD64_IMUL_REG_RM_1;							\
	if (AMD64_NEEDS_REX(l) || AMD64_NEEDS_REX(r))				\
		*(--_nIns) = AMD64_REX(0,l,r);							\
	asm_output("mul %s,%s", gpn(l), gpn(r));					\
	} while (0)


#define NEG(r) do { 											\
	AMD64_PRIM(AMD64_NEG_RM, 3, r);								\
	asm_output("neg %s",gpn(r)); 								\
	} while(0)


#define ADD(l,r) do {											\
	AMD64_PRIM(AMD64_ADD_REG_RM, l, r);							\
	asm_output("add %s,%s", gpn(l), gpn(r));					\
	} while (0)


#define ADDQ(l,r) do {											\
	AMD64_PRIMQ(AMD64_ADD_REG_RM, l, r);						\
	asm_output("add %s,%s", gpn(l), gpn(r));					\
	} while (0)


#define SUB(l,r) do {											\
	AMD64_PRIM(AMD64_SUB_REG_RM, l, r);							\
	asm_output("sub %s,%s", gpn(l), gpn(r));					\
	} while (0)



#define SAR(r,s) do {											\
	AMD64_PRIM(AMD64_SAR_RM, 7, r);								\
	asm_output("sar %s,%s",gpn(r),gpn(s));						\
	} while (0)


#define SARi(r,i) do {											\
	if (i == 1) {												\
		underrunProtect(3);										\
		*(--_nIns) = AMD64_MODRM_REG(7, r);						\
		*(--_nIns) = AMD64_SAR_RM_1;							\
	} else {													\
		underrunProtect(4);										\
		*(--_nIns) = uint8_t(i);								\
		*(--_nIns) = AMD64_MODRM_REG(7, r);						\
		*(--_nIns) = AMD64_SAR_RM_IMM8;							\
	}															\
	if (AMD64_NEEDS_REX(r))										\
		*(--_nIns) = AMD64_REX(0,0,r);							\
	asm_output("sar %s,%d", gpn(r), i);						\
	} while (0)


#define SHLi(r,i) do { 											\
	if (i == 1) {												\
		underrunProtect(3);										\
		*(--_nIns) = AMD64_MODRM_REG(4, r);						\
		*(--_nIns) = AMD64_SHL_RM_1;							\
	} else {													\
		underrunProtect(4);										\
		*(--_nIns) = uint8_t(i);								\
		*(--_nIns) = AMD64_MODRM_REG(4, r);						\
		*(--_nIns) = AMD64_SHL_RM_IMM8;							\
	}															\
	if (AMD64_NEEDS_REX(r))										\
		*(--_nIns) = AMD64_REX(0,0,r);							\
	asm_output("shl %s,%d", gpn(r), i);						\
	} while (0)


#define SHLQi(r,i) do { 										\
	if (i == 1) {												\
		underrunProtect(3);										\
		*(--_nIns) = AMD64_MODRM_REG(4, r);						\
		*(--_nIns) = AMD64_SHL_RM_1;							\
	} else {													\
		underrunProtect(4);										\
		*(--_nIns) = uint8_t(i);								\
		*(--_nIns) = AMD64_MODRM_REG(4, r);						\
		*(--_nIns) = AMD64_SHL_RM_IMM8;							\
	}															\
	*(--_nIns) = AMD64_REX(1,0,r);							    \
	asm_output("shl %s,%d", gpn(r), i);						\
	} while (0)


#define SHL(r,s) do { 											\
	AMD64_PRIM(AMD64_SHL_RM, 4, r);								\
	asm_output("shl %s,%s",gpn(r),gpn(s));						\
	} while (0)


#define AMD64_SUBi(r,i,q) do {									\
	AMD64_ALU(AMD64_SUB_RAX, r, i, q);							\
	asm_output("sub %s,%d",gpn(r),i);							\
	} while (0)


#define SUBi(r,i) AMD64_SUBi(r, i, 0)


#define SUBQi(r,i) AMD64_SUBi(r, i, 1)
		


#define MR(d,s) do { 											\
	underrunProtect(3);											\
	*(--_nIns) = AMD64_MODRM_REG(d, s);							\
	*(--_nIns) = AMD64_MOV_REG_RM;								\
	*(--_nIns) = AMD64_REX(1,d,s);								\
	asm_output("mov %s,%s",gpn(d),gpn(s));						\
	} while (0)



#define LEA(r,d,b) do { 										\
	underrunProtect(8);											\
	AMD64_MODRM_DISP(r, b, d);									\
	*(--_nIns) = AMD64_LEA;										\
	if (AMD64_NEEDS_REX(r) || AMD64_NEEDS_REX(b)) {				\
		*(--_nIns) = AMD64_REX(0,r,b);							\
	}															\
	asm_output("lea %s,%d(%s)",gpn(r),d,gpn(b));				\
	} while(0)


#define LEAQ(r,d,b) do { 										\
	underrunProtect(8);											\
	AMD64_MODRM_DISP(r, b, d);									\
	*(--_nIns) = AMD64_LEA;										\
	*(--_nIns) = AMD64_REX(1,r,b);							    \
	asm_output("lea %s,%d(%s)",gpn(r),d,gpn(b));				\
	} while(0)


#define AMD64_SETCC(op, r) 				\
	underrunProtect(4);					\
	*(--_nIns) = AMD64_MODRM_REG(0,r);	\
	*(--_nIns) = op & 0xFF;				\
	*(--_nIns) = op >> 8;				\
	if (AMD64_NEEDS_REX(r)) {			\
		*(--_nIns) = AMD64_REX(0,0,r);	\
	}



#define SETE(r)		do { AMD64_SETCC(0x0f94,(r));			asm_output("sete %s",gpn(r)); } while(0)
#define SETNP(r)	do { AMD64_SETCC(0x0f9B,(r));			asm_output("setnp %s",gpn(r)); } while(0)
#define SETL(r)		do { AMD64_SETCC(0x0f9C,(r));			asm_output("setl %s",gpn(r)); } while(0)
#define SETLE(r)	do { AMD64_SETCC(0x0f9E,(r));			asm_output("setle %s",gpn(r)); } while(0)
#define SETG(r)		do { AMD64_SETCC(0x0f9F,(r));			asm_output("setg %s",gpn(r)); } while(0)
#define SETGE(r)	do { AMD64_SETCC(0x0f9D,(r));			asm_output("setge %s",gpn(r)); } while(0)
#define SETB(r)     do { AMD64_SETCC(0x0f92,(r));          asm_output("setb %s",gpn(r)); } while(0)
#define SETBE(r)    do { AMD64_SETCC(0x0f96,(r));          asm_output("setbe %s",gpn(r)); } while(0)
#define SETA(r)     do { AMD64_SETCC(0x0f97,(r));          asm_output("seta %s",gpn(r)); } while(0)
#define SETAE(r)    do { AMD64_SETCC(0x0f93,(r));          asm_output("setae %s",gpn(r)); } while(0)
#define SETC(r)     do { AMD64_SETCC(0x0f90,(r));          asm_output("setc %s",gpn(r)); } while(0)
#define SETO(r)     do { AMD64_SETCC(0x0f92,(r));          asm_output("seto %s",gpn(r)); } while(0)


#define AMD64_CMOV(op, dr, sr)				\
	underrunProtect(4);						\
	*(--_nIns) = AMD64_MODRM_REG(dr, sr);	\
	*(--_nIns) = op & 0xFF;					\
	*(--_nIns) = op >> 8;					\
	if (AMD64_NEEDS_REX(dr) || AMD64_NEEDS_REX(sr)) { \
		*(--_nIns) = AMD64_REX(0,dr,sr);	\
	}

#define AMD64_CMOVQ(op, dr, sr)				\
	underrunProtect(4);						\
	*(--_nIns) = AMD64_MODRM_REG(dr, sr);	\
	*(--_nIns) = op & 0xFF;					\
	*(--_nIns) = op >> 8;					\
	*(--_nIns) = AMD64_REX(1,dr,sr);	    


#define MREQ(dr,sr)	do { AMD64_CMOV(0x0f44,dr,sr); asm_output("cmove %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRNE(dr,sr)	do { AMD64_CMOV(0x0f45,dr,sr); asm_output("cmovne %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRL(dr,sr)	do { AMD64_CMOV(0x0f4C,dr,sr); asm_output("cmovl %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRLE(dr,sr)	do { AMD64_CMOV(0x0f4E,dr,sr); asm_output("cmovle %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRG(dr,sr)	do { AMD64_CMOV(0x0f4F,dr,sr); asm_output("cmovg %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRGE(dr,sr)	do { AMD64_CMOV(0x0f4D,dr,sr); asm_output("cmovge %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRB(dr,sr)	do { AMD64_CMOV(0x0f42,dr,sr); asm_output("cmovb %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRBE(dr,sr)	do { AMD64_CMOV(0x0f46,dr,sr); asm_output("cmovbe %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRA(dr,sr)	do { AMD64_CMOV(0x0f47,dr,sr); asm_output("cmova %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRAE(dr,sr)	do { AMD64_CMOV(0x0f43,dr,sr); asm_output("cmovae %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRNC(dr,sr)	do { AMD64_CMOV(0x0f43,dr,sr); asm_output("cmovnc %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRNO(dr,sr)	do { AMD64_CMOV(0x0f41,dr,sr); asm_output("cmovno %s,%s", gpn(dr),gpn(sr)); } while(0)

#define MRQEQ(dr,sr) do { AMD64_CMOVQ(0x0f44,dr,sr); asm_output("cmove %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQNE(dr,sr) do { AMD64_CMOVQ(0x0f45,dr,sr); asm_output("cmovne %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQL(dr,sr)  do { AMD64_CMOVQ(0x0f4C,dr,sr); asm_output("cmovl %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQLE(dr,sr) do { AMD64_CMOVQ(0x0f4E,dr,sr); asm_output("cmovle %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQG(dr,sr)  do { AMD64_CMOVQ(0x0f4F,dr,sr); asm_output("cmovg %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQGE(dr,sr) do { AMD64_CMOVQ(0x0f4D,dr,sr); asm_output("cmovge %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQB(dr,sr)  do { AMD64_CMOVQ(0x0f42,dr,sr); asm_output("cmovb %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQBE(dr,sr) do { AMD64_CMOVQ(0x0f46,dr,sr); asm_output("cmovbe %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQA(dr,sr)  do { AMD64_CMOVQ(0x0f47,dr,sr); asm_output("cmova %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQAE(dr,sr) do { AMD64_CMOVQ(0x0f43,dr,sr); asm_output("cmovae %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQNC(dr,sr) do { AMD64_CMOVQ(0x0f43,dr,sr); asm_output("cmovnc %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRQNO(dr,sr) do { AMD64_CMOVQ(0x0f41,dr,sr); asm_output("cmovno %s,%s", gpn(dr),gpn(sr)); } while(0)

#define AMD64_LD(reg,disp,base,q) 								\
	underrunProtect(7);											\
	AMD64_MODRM_DISP(reg,base,disp);							\
	*(--_nIns) = AMD64_MOV_REG_RM;								\
	if (q || AMD64_NEEDS_REX(reg) || AMD64_NEEDS_REX(base))		\
		*(--_nIns) = AMD64_REX(q,reg,base);	


#define LD(reg,disp,base) do {									\
	AMD64_LD(reg,disp,base,0);									\
	asm_output("mov dword %s,%d(%s)",gpn(reg),disp,gpn(base));	\
	} while (0)


#define LDQ(reg,disp,base) do {									\
	AMD64_LD(reg,disp,base,1);									\
	asm_output("mov %s,%d(%s)",gpn(reg),disp,gpn(base));		\
	} while (0)

// load 8-bit, zero extend
// note, only 5-bit offsets (!) are supported for this, but that's all we need at the moment
// (movzx actually allows larger offsets mode but 5-bit gives us advantage in Thumb mode)

#define LD8Z(r,d,b)	do { 										\
	underrunProtect(5);											\
	AMD64_MODRM_DISP(r, b, d);									\
	*(--_nIns) = AMD64_MOVZX & 0xFF;							\
	*(--_nIns) = AMD64_MOVZX >> 8;								\
	if (AMD64_NEEDS_REX(r) || AMD64_NEEDS_REX(b)) {				\
		*(--_nIns) = AMD64_REX(0,r,b);							\
	}															\
	asm_output("movzx %s,%d(%s)", gpn(r),d,gpn(b)); 			\
	} while(0)

	
#define LD16Z(r,d,b) do {                                        \
    underrunProtect(5);                                         \
    AMD64_MODRM_DISP(r, b, d);                                  \
    *(--_nIns) = AMD64_MOVSX & 0xFF;                            \
    *(--_nIns) = AMD64_MOVSX >> 8;                              \
    if (AMD64_NEEDS_REX(r) || AMD64_NEEDS_REX(b)) {             \
        *(--_nIns) = AMD64_REX(0,r,b);                          \
    }                                                           \
    asm_output("movsx %s,%d(%s)", gpn(r),d,gpn(b));            \
    } while(0)

    
#define LDi(r,i) do { 											\
	underrunProtect(6);											\
	IMM32(i);													\
	*(--_nIns) = AMD64_MODRM_REG(0, r);							\
	*(--_nIns) = AMD64_MOV_RM_IMM;								\
	*(--_nIns) = AMD64_REX(1,0,r);							    \
	asm_output("mov %s,%d",gpn(r),i);							\
	} while (0)


#define LDQi(r,i) do {											\
	underrunProtect(10);										\
	IMM64(i);													\
	*(--_nIns) = AMD64_MOV_REG_IMM(r);							\
	*(--_nIns) = AMD64_REX(1,0,r);								\
	asm_output("mov %s,%ld",gpn(r),(nj_printf_ld)i);			\
	} while(0)


#define AMD64_ST(base,disp,reg,q)								\
	underrunProtect(7);											\
	AMD64_MODRM_DISP(reg, base, disp);							\
	*(--_nIns) = AMD64_MOV_RM_REG;								\
	if (q || AMD64_NEEDS_REX(reg) || AMD64_NEEDS_REX(base))		\
		*(--_nIns) = AMD64_REX(q,reg,base);						\


#define ST(base,disp,reg) do { 									\
	AMD64_ST(base,disp,reg,0);									\
	asm_output("mov dword %d(%s),%s",disp,gpn(base),gpn(reg));	\
	} while(0);


#define STQ(base,disp,reg) do {									\
	AMD64_ST(base,disp,reg,1);									\
	asm_output("mov %d(%s),%s",disp,gpn(base),gpn(reg));		\
	} while(0);


#define STi(base,disp,imm)	do { 								\
	underrunProtect(11);										\
	IMM32(imm);													\
	AMD64_MODRM_DISP(0, base, disp);							\
	*(--_nIns) = AMD64_MOV_RM_IMM;								\
	if (AMD64_NEEDS_REX(base))									\
		*(--_nIns) = AMD64_REX(0,0,base);						\
	asm_output("mov %d(%s),%d",disp,gpn(base),imm);			\
	} while(0);


#define RET() do { 												\
	underrunProtect(1);											\
	*(--_nIns) = AMD64_RET;										\
	asm_output("ret");											\
	} while (0)


#define INT3()  do {											\
	underrunProtect(1);											\
	*(--_nIns) = AMD64_INT3;									\
	asm_output("int3");											\
	} while (0)

#define PUSHi(i) do { \
	if (isS8(i)) { \
		underrunProtect(2);			\
		_nIns-=2; _nIns[0] = 0x6a; _nIns[1] = (uint8_t)(i); \
		asm_output("push %d",i); \
	} else \
		{ PUSHi32(i); } } while(0)

#define PUSHi32(i)	do {	\
	underrunProtect(5);	\
	IMM32(i);			\
	*(--_nIns) = 0x68;	\
	asm_output("push %d",i); } while(0)

/**
 * Note: PUSH/POP do not use REX's prefix 64-bit field.
 */

#define PUSHr(r) do {										\
	underrunProtect(2);										\
	*(--_nIns) = (uint8_t)AMD64_PUSH_REG(r);				\
	if (AMD64_NEEDS_REX(r))									\
		*(--_nIns) = AMD64_REX(0,0,r); 						\
	asm_output("push %s",gpn(r)); 							\
	} while(0)

#define PUSHm(d,b) do { 									\
	underrunProtect(7);										\
	AMD64_MODRM_DISP(6, b, d);								\
	*(--_nIns) = AMD64_PUSH_RM;								\
	if (AMD64_NEEDS_REX(b)) {								\
		*(--_nIns) = AMD64_REX(0,6,b);						\
	}														\
	asm_output("push %d(%s)",d,gpn(b)); 					\
	} while(0)


#define POPr(r) do { 										\
	underrunProtect(2);										\
	*(--_nIns) = (uint8_t)(AMD64_POP_REG(r));				\
	if (AMD64_NEEDS_REX(r))									\
		*(--_nIns) = AMD64_REX(0,0,r);						\
	asm_output("pop %s",gpn(r)); 							\
	} while(0)

#define JCC(o,t,n) do {                                     \
	underrunProtect(6);	                                    \
	intptr_t tt = (intptr_t)t - (intptr_t)_nIns;	        \
	if (isS8(tt)) {                                         \
		verbose_only( NIns* next = _nIns; (void)next; )     \
		_nIns -= 2;                                         \
		_nIns[0] = (uint8_t) ( 0x70 | (o) );                \
		_nIns[1] = (uint8_t) (tt);                          \
		asm_output("%s %lX",(n),(ptrdiff_t)(next+tt));     \
	} else if (tt <= INT_MAX && tt >= INT_MIN) {            \
		verbose_only( NIns* next = _nIns; )                 \
		IMM32(tt);                                          \
		_nIns -= 2;                                         \
		_nIns[0] = 0x0f;                                    \
		_nIns[1] = (uint8_t) ( 0x80 | (o) );                \
		asm_output("%s %lX",(n),(ptrdiff_t)(next+tt));     \
	} else {                                                \
        underrunProtect(20);                                \
        NanoAssert(!_inExit);                               \
        /* We could now be in range, but assume we're not. */ \
        /* Note we generate the thunk forwards, and the */  \
        /* jcc to the thunk backwards. */                   \
        uint8_t* base;                                      \
        intptr_t offs;                                      \
		base = (uint8_t *)((uintptr_t)_nIns & ~((uintptr_t)NJ_PAGE_SIZE-1)); \
		base += sizeof(PageHeader) + _pageData;			    \
        _pageData += 14;                                    \
        *(base++) = 0xFF;                                   \
        *(base++) = 0x25;                                   \
        *(int *)base = 0;                                   \
        base += 4;                                          \
        *(intptr_t *)base = intptr_t(t);                    \
        offs = intptr_t(base-6) - intptr_t(_nIns);          \
        NanoAssert(offs >= INT_MIN && offs <= INT_MAX);     \
        if (isS8(offs)) {                                   \
            _nIns -= 2;                                     \
            _nIns[0] = uint8_t( 0x70 | (o) );               \
            _nIns[1] = uint8_t( (offs) );                   \
        } else {                                            \
            IMM32(offs);                                    \
            _nIns -= 2;                                     \
            _nIns[0] = 0x0f;                                \
            _nIns[1] = uint8_t( 0x80 | (o) );               \
        }                                                   \
        asm_output("%s %d(rip) #%lX",n,offs,intptr_t(t));  \
    }                                                       \
    } while(0)

#define JMPm_nochk(rip) do {                \
    IMM32(rip);                             \
    *(--_nIns) = 0x25;                      \
    *(--_nIns) = 0xFF;                      \
    } while (0)

#define JMP_long(t) do { \
	underrunProtect(5);	\
	intptr_t tt = (intptr_t)t - (intptr_t)_nIns;	\
	JMP_long_nochk_offset(tt);	\
	} while(0)

#define JMP(t)		do { 	                            \
   	underrunProtect(5);	                                \
	intptr_t tt = (intptr_t)t - (intptr_t)_nIns;	    \
	if (isS8(tt)) {                                     \
		verbose_only( NIns* next = _nIns; (void)next; ) \
		_nIns -= 2;                                     \
		_nIns[0] = 0xeb;                                \
		_nIns[1] = (uint8_t) ( (tt)&0xff );             \
		asm_output("jmp %lX",(ptrdiff_t)(next+tt));    \
	} else {                                            \
        if (tt >= INT_MIN && tt <= INT_MAX) {           \
    		JMP_long_nochk_offset(tt);	                \
        } else {                                        \
            underrunProtect(14);                        \
            _nIns -= 8;                                 \
            *(intptr_t *)_nIns = intptr_t(t);           \
            JMPm_nochk(0);                              \
        }                                               \
	} } while(0)

#define JMP_long_nochk(t)		do { 	\
	intptr_t tt = (intptr_t)t - (intptr_t)_nIns;	\
	JMP_long_nochk_offset(tt);	\
	} while(0)

#define JMPc 0xe9
		
#define JMP_long_placeholder()	do {                    \
	underrunProtect(14);				                \
    IMM64(-1);                                          \
    JMPm_nochk(0);                                      \
    } while (0)
	
// this should only be used when you can guarantee there is enough room on the page
#define JMP_long_nochk_offset(o) do {\
		verbose_only( NIns* next = _nIns; (void)next; ) \
        NanoAssert(o <= INT_MAX && o >= INT_MIN);       \
 		IMM32((o)); \
 		*(--_nIns) = JMPc; \
		asm_output("jmp %lX",(ptrdiff_t)(next+(o))); } while(0)

#define JMPr(r) do {                    \
    underrunProtect(2);                 \
    *(--_nIns) = AMD64_MODRM_REG(4, r); \
    *(--_nIns) = 0xFF;                  \
    } while (0)

#define JE(t)	JCC(0x04, t, "je")
#define JNE(t)	JCC(0x05, t, "jne")
#define JP(t)	JCC(0x0A, t, "jp")
#define JNP(t)	JCC(0x0B, t, "jnp")

#define JB(t)	JCC(0x02, t, "jb")
#define JNB(t)	JCC(0x03, t, "jnb")
#define JBE(t)	JCC(0x06, t, "jbe")
#define JNBE(t) JCC(0x07, t, "jnbe")

#define JA(t)	JCC(0x07, t, "ja")
#define JNA(t)	JCC(0x06, t, "jna")
#define JAE(t)	JCC(0x03, t, "jae")
#define JNAE(t) JCC(0x02, t, "jnae")

#define JL(t)	JCC(0x0C, t, "jl")
#define JNL(t)	JCC(0x0D, t, "jnl")
#define JLE(t)	JCC(0x0E, t, "jle")
#define JNLE(t)	JCC(0x0F, t, "jnle")

#define JG(t)	JCC(0x0F, t, "jg")
#define JNG(t)	JCC(0x0E, t, "jng")
#define JGE(t)	JCC(0x0D, t, "jge")
#define JNGE(t)	JCC(0x0C, t, "jnge")

#define JC(t)   JCC(0x02, t, "jc")
#define JNC(t)  JCC(0x03, t, "jnc")
#define JO(t)   JCC(0x00, t, "jo")
#define JNO(t)  JCC(0x01, t, "jno")


#define AMD64_OP3(c,q,r,b)						\
		*(--_nIns) = (uint8_t)((c)&0xff);		\
		*(--_nIns) = (uint8_t)(((c)>>8)&0xff);	\
		if (q 									\
			|| AMD64_NEEDS_REX(r)				\
			|| AMD64_NEEDS_REX(b)) {			\
			*(--_nIns) = AMD64_REX(q,r,b);		\
		}										\
 		*(--_nIns) = (uint8_t)(((c)>>16)&0xff);


#define SSE_LDQ(r,d,b) do {										\
	underrunProtect(7);											\
	AMD64_MODRM_DISP(r,b,d);									\
	AMD64_OP3(AMD64_MOVD_REG_RM,1,r,b);							\
	asm_output("movd %s,%d(%s)",gpn(r),(d),gpn(b));			\
	} while (0)


#define SSE_STQ(d,r,b) do {										\
	underrunProtect(7);											\
	AMD64_MODRM_DISP(b,r,d);									\
	AMD64_OP3(AMD64_MOVD_RM_REG,1,b,r);							\
	asm_output("movd %d(%s),%s",(d),gpn(r),gpn(b));			\
	} while (0)


#define SSE_CVTSI2SD(xr,gr) do{ 								\
	underrunProtect(5);											\
	*(--_nIns) = AMD64_MODRM_REG(xr, gr);						\
	AMD64_OP3(AMD64_CVTSI2SD,0,xr,gr);							\
    asm_output("cvtsi2sd %s,%s",gpn(xr),gpn(gr)); 				\
    } while(0)

// move and zero-extend gpreg to xmm reg

#define SSE_MOVD(d,s) do{ 										\
	underrunProtect(7);											\
	if (_is_xmm_reg_(s)) { 										\
		NanoAssert(_is_gp_reg_(d)); 							\
		*(--_nIns) = AMD64_MODRM_REG(s, d);						\
		AMD64_OP3(AMD64_MOVD_RM_REG, 1, s, d);					\
	} else { 													\
		NanoAssert(_is_gp_reg_(s)); 							\
		NanoAssert(_is_xmm_reg_(d)); 							\
		*(--_nIns) = AMD64_MODRM_REG(d, s);						\
		AMD64_OP3(AMD64_MOVD_REG_RM, 1, d, s);					\
	} 															\
    asm_output("movd %s,%s",gpn(d),gpn(s)); 					\
    } while(0)


#define SSE_MOVSD(rd,rs) do{									\
	underrunProtect(7);											\
	*(--_nIns) = AMD64_MODRM_REG(rd, rs);						\
	AMD64_OP3(AMD64_MOVSD_REG_RM,0,rd,rs);						\
    asm_output("movsd %s,%s",gpn(rd),gpn(rs)); 				\
    } while(0)


#define SSE_MOVDm(d,b,xrs) do {									\
	AMD64_MODRM_DISP(xrs, b, d);								\
	AMD64_OP3(AMD64_MOVD_RM_REG, 1, xrs, b);					\
    asm_output("movd %d(%s),%s", d, gpn(b), gpn(xrs));			\
    } while(0)


#define SSE_ADDSD(rd,rs) do{ 									\
	underrunProtect(5);											\
	*(--_nIns) = AMD64_MODRM_REG(rd, rs);						\
	AMD64_OP3(AMD64_ADDSD, 0, rd, rs);							\
    asm_output("addsd %s,%s",gpn(rd),gpn(rs)); 				\
    } while(0)


#define SSE_ADDSDm(r,addr)do {     							\
    underrunProtect(10); 									\
	ptrdiff_t d = (NIns *)addr - _nIns;						\
	NanoAssert(d >= INT_MIN && d <= INT_MAX);				\
	IMM32((int32_t)d);										\
	*(--_nIns) = AMD64_MODRM(0, r, 5);						\
	AMD64_OP3(AMD64_ADDSD, 0, r, 0);						\
    asm_output("addsd %s,%p // =%f",gpn(r),addr,*(double*)addr); 	\
    } while(0)


#define SSE_SUBSD(rd,rs) do{ 							\
	underrunProtect(5);									\
	*(--_nIns) = AMD64_MODRM_REG(rd, rs);				\
	AMD64_OP3(AMD64_SUBSD, 0, rd, rs);					\
    asm_output("subsd %s,%s",gpn(rd),gpn(rs)); 		\
    } while(0)


#define SSE_MULSD(rd,rs) do{ 							\
	underrunProtect(5);									\
	*(--_nIns) = AMD64_MODRM_REG(rd, rs);				\
	AMD64_OP3(AMD64_MULSD, 0, rd, rs);					\
    asm_output("mulsd %s,%s",gpn(rd),gpn(rs)); 		\
    } while(0)


#define SSE_DIVSD(rd,rs) do{ 							\
	underrunProtect(5);									\
	*(--_nIns) = AMD64_MODRM_REG(rd, rs);				\
	AMD64_OP3(AMD64_DIVSD, 0, rd, rs);					\
    asm_output("divsd %s,%s",gpn(rd),gpn(rs)); 		\
    } while(0)


#define SSE_UCOMISD(rl,rr) do{ 							\
	*(--_nIns) = AMD64_MODRM_REG(rl, rr);				\
	AMD64_OP3(AMD64_UCOMISD, 0, rl, rr);				\
    asm_output("ucomisd %s,%s",gpn(rl),gpn(rr)); 		\
    } while(0)

#define EMIT_XORPD_MASK(mask)							\
	do {												\
		uint8_t *base, *begin;							\
		uint32_t *addr;									\
		base = (uint8_t *)((uintptr_t)_nIns & ~((uintptr_t)NJ_PAGE_SIZE-1)); \
		base += sizeof(PageHeader) + _pageData;			\
		begin = base;									\
		/* Make sure we align */						\
		if ((uintptr_t)base & 0xF) {					\
			base = (NIns *)((uintptr_t)base & ~(0xF));	\
			base += 16;									\
		}												\
		_pageData += (int32_t)(base - begin) + 16;		\
		_dblNegPtr = (NIns *)base;						\
		addr = (uint32_t *)base;						\
		addr[0] = mask[0];								\
		addr[1] = mask[1];								\
		addr[2] = mask[2];								\
		addr[3] = mask[3];								\
	} while (0)

/**
 * Note: high underrun protect is for:
 * 12 bytes of alignment max 
 * 16 bytes for the data
 * 10 bytes for the instruction
 */
#define SSE_XORPD(r, maskaddr) do {						\
	if (_dblNegPtr != NULL) {							\
		underrunProtect(10);							\
	}													\
	if (_dblNegPtr == NULL) {							\
		underrunProtect(38);							\
		EMIT_XORPD_MASK(maskaddr);						\
	}													\
	ptrdiff_t d = _dblNegPtr - _nIns;					\
	IMM32((int32_t)d);									\
	*(--_nIns) = AMD64_MODRM(0, rr, 5);					\
	AMD64_OP3(AMD64_XORPD, 0, rr, 0);					\
	asm_output("xorpd %s,[0x%X]",gpn(rr),(int32_t)d);	\
    } while(0)


#define SSE_XORPDr(rd,rs) do{ 							\
	underrunProtect(5);									\
	*(--_nIns) = AMD64_MODRM_REG(rd, rs);				\
	AMD64_OP3(AMD64_XORPD, 0, rd, rs);					\
    asm_output("xorpd %s,%s",gpn(rd),gpn(rs)); 		\
    } while(0)


#define TEST_AL(i) do {									\
		underrunProtect(2);								\
		*(--_nIns) = uint8_t(i);						\
		*(--_nIns) = 0xA8;								\
		asm_output("test al, %d",i);					\
		} while(0)


#define PUSHFQ() do {									\
	underrunProtect(1);									\
	*(--_nIns) = AMD64_PUSHF;							\
	asm_output("pushf");								\
	} while (0)

#define CALL(c)	do { 									\
  underrunProtect(5);									\
  intptr_t offset = (c->_address) - ((intptr_t)_nIns);	\
  if (offset <= INT_MAX && offset >= INT_MIN) {			\
    IMM32( (uint32_t)offset );							\
    *(--_nIns) = 0xE8;									\
  } else {												\
   	*(--_nIns) = 0xD0;									\
    *(--_nIns) = 0xFF;									\
    LDQi(RAX, c->_address);								\
  }														\
  verbose_only(asm_output("call %s",(c->_name));)		\
} while (0)

}
#endif // __nanojit_NativeAMD64__
