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


#ifndef __nanojit_Nativei386__
#define __nanojit_Nativei386__


namespace nanojit
{
	const int NJ_LOG2_PAGE_SIZE	= 12;		// 4K
	const int NJ_MAX_REGISTERS = 24; // gpregs, x87 regs, xmm regs
	const int NJ_STACK_OFFSET = 0;
	
	#define NJ_MAX_STACK_ENTRY 256
	#define NJ_MAX_PARAMETERS 1

        // Preserve a 16-byte stack alignment, to support the use of
        // SSE instructions like MOVDQA (if not by Tamarin itself,
        // then by the C functions it calls).
	const int NJ_ALIGN_STACK = 16;
	
	typedef uint8_t NIns;

	// These are used as register numbers in various parts of the code
	typedef enum
	{
		// general purpose 32bit regs
		EAX = 0, // return value, scratch
		ECX = 1, // this/arg0, scratch
		EDX = 2, // arg1, return-msw, scratch
		EBX = 3,
		ESP = 4, // stack pointer
		EBP = 5, // frame pointer
		ESI = 6,
		EDI = 7,

		SP = ESP, // alias SP to ESP for convenience
		FP = EBP, // alias FP to EBP for convenience

		// SSE regs come before X87 so we prefer them
		XMM0 = 8,
		XMM1 = 9,
		XMM2 = 10,
		XMM3 = 11,
		XMM4 = 12,
		XMM5 = 13,
		XMM6 = 14,
		XMM7 = 15,

        // X87 regs
		FST0 = 16,
		FST1 = 17,
		FST2 = 18,
		FST3 = 19,
		FST4 = 20,
		FST5 = 21,
		FST6 = 22,
		FST7 = 23,

		FirstReg = 0,
		LastReg = 23,
		UnknownReg = 24
	} 
	Register;

	typedef int RegisterMask;

	static const int NumSavedRegs = 3;
	static const RegisterMask SavedRegs = 1<<EBX | 1<<EDI | 1<<ESI;
	static const RegisterMask GpRegs = SavedRegs | 1<<EAX | 1<<ECX | 1<<EDX;
    static const RegisterMask XmmRegs = 1<<XMM0|1<<XMM1|1<<XMM2|1<<XMM3|1<<XMM4|1<<XMM5|1<<XMM6|1<<XMM7;
    static const RegisterMask x87Regs = 1<<FST0;
	static const RegisterMask FpRegs = x87Regs | XmmRegs;
	static const RegisterMask ScratchRegs = 1<<EAX | 1<<ECX | 1<<EDX | FpRegs;

	static const RegisterMask AllowableFlagRegs = 1<<EAX |1<<ECX | 1<<EDX | 1<<EBX;

	#define _rmask_(r)		(1<<(r))
	#define _is_xmm_reg_(r)	((_rmask_(r)&XmmRegs)!=0)
	#define _is_x87_reg_(r)	((_rmask_(r)&x87Regs)!=0)
	#define _is_fp_reg_(r)	((_rmask_(r)&FpRegs)!=0)
	#define _is_gp_reg_(r)	((_rmask_(r)&GpRegs)!=0)

	#define nextreg(r)		Register(r+1)
	#define prevreg(r)		Register(r-1)
	#define imm2register(c) (Register)(c)
	
	verbose_only( extern const char* regNames[]; )

	#define DECLARE_PLATFORM_STATS()

	#define DECLARE_PLATFORM_REGALLOC()

	#define DECLARE_PLATFORM_ASSEMBLER()	\
        const static Register argRegs[2], retRegs[2]; \
		bool x87Dirty;						\
		bool pad[3];\
		void nativePageReset();\
		void nativePageSetup();\
        void underrunProtect(int);\
        void asm_farg(LInsp);\
        void asm_align_code();
		
	#define swapptrs()  { NIns* _tins = _nIns; _nIns=_nExitIns; _nExitIns=_tins; }
		
#define IMM32(i)	\
	_nIns -= 4;		\
	*((int32_t*)_nIns) = (int32_t)(i)

#define MODRMs(r,d,b,l,i) \
		NanoAssert(unsigned(r)<8 && unsigned(b)<8 && unsigned(i)<8); \
 		if ((d) == 0 && (b) != EBP) { \
			_nIns -= 2; \
 			_nIns[0] = (uint8_t)     ( 0<<6 |   (r)<<3 | 4); \
 			_nIns[1] = (uint8_t) ((l)<<6 | (i)<<3 | (b)); \
 		} else if (isS8(d)) { \
 			_nIns -= 3; \
 			_nIns[0] = (uint8_t)     ( 1<<6 |   (r)<<3 | 4 ); \
 			_nIns[1] = (uint8_t) ( (l)<<6 | (i)<<3 | (b) ); \
			_nIns[2] = (uint8_t) (d); \
 		} else { \
 			IMM32(d); \
 			*(--_nIns) = (uint8_t) ( (l)<<6 | (i)<<3 | (b) ); \
 			*(--_nIns) = (uint8_t)    ( 2<<6 |   (r)<<3 | 4 ); \
 		}

#define MODRMm(r,d,b) \
		NanoAssert(unsigned(r)<8 && ((b)==UnknownReg || unsigned(b)<8)); \
        if ((b) == UnknownReg) {\
            IMM32(d);\
            *(--_nIns) = (uint8_t) (0<<6 | (r)<<3 | 5);\
        } else if ((b) == ESP) { \
 			MODRMs(r, d, b, 0, (Register)4); \
 		} \
		else if ( (d) == 0 && (b) != EBP) { \
 			*(--_nIns) = (uint8_t) ( 0<<6 | (r)<<3 | (b) ); \
 		} else if (isS8(d)) { \
 			*(--_nIns) = (uint8_t) (d); \
 			*(--_nIns) = (uint8_t) ( 1<<6 | (r)<<3 | (b) ); \
 		} else { \
 			IMM32(d); \
 			*(--_nIns) = (uint8_t) ( 2<<6 | (r)<<3 | (b) ); \
 		} 

#define MODRMSIB(reg,base,index,scale,disp)					\
		if (disp != 0 || base == EBP) {						\
			if (isS8(disp)) {								\
				*(--_nIns) = int8_t(disp);					\
			} else {										\
				IMM32(disp);								\
			}												\
		}													\
		*(--_nIns) = uint8_t((scale)<<6|(index)<<3|(base));	\
		if (disp == 0 && base != EBP) {						\
			*(--_nIns) = uint8_t(((reg)<<3)|4);				\
		} else {											\
			if (isS8(disp))									\
				*(--_nIns) = uint8_t((1<<6)|(reg<<3)|4);	\
			else											\
				*(--_nIns) = uint8_t((2<<6)|(reg<<3)|4);	\
		}

#define MODRMdm(r,addr)					\
		NanoAssert(unsigned(r)<8);		\
		IMM32(addr);					\
		*(--_nIns) = (uint8_t)( (r)<<3 | 5 );

#define MODRM(d,s) \
		NanoAssert(((unsigned)(d))<8 && ((unsigned)(s))<8); \
		*(--_nIns) = (uint8_t) ( 3<<6|(d)<<3|(s) )

#define ALU0(o)				\
		underrunProtect(1);\
		*(--_nIns) = (uint8_t) (o)

#define ALUm(c,r,d,b)		\
		underrunProtect(8); \
		MODRMm(r,d,b);		\
		*(--_nIns) = uint8_t(c)

#define ALUdm(c,r,addr)		\
		underrunProtect(6);	\
		MODRMdm(r,addr);	\
		*(--_nIns) = uint8_t(c)

#define ALUsib(c,r,base,index,scale,disp)	\
		underrunProtect(7);					\
		MODRMSIB(r,base,index,scale,disp);	\
		*(--_nIns) = uint8_t(c)

#define ALUm16(c,r,d,b)		\
		underrunProtect(9); \
		MODRMm(r,d,b);		\
		*(--_nIns) = uint8_t(c);\
		*(--_nIns) = 0x66

#define ALU2dm(c,r,addr)	\
		underrunProtect(7);	\
		MODRMdm(r,addr);	\
        *(--_nIns) = (uint8_t) (c);\
        *(--_nIns) = (uint8_t) ((c)>>8)

#define ALU2m(c,r,d,b)      \
        underrunProtect(9); \
        MODRMm(r,d,b);      \
        *(--_nIns) = (uint8_t) (c);\
        *(--_nIns) = (uint8_t) ((c)>>8)

#define ALU2sib(c,r,base,index,scale,disp)	\
		underrunProtect(8);					\
		MODRMSIB(r,base,index,scale,disp);	\
        *(--_nIns) = (uint8_t) (c);			\
        *(--_nIns) = (uint8_t) ((c)>>8)

#define ALU(c,d,s)  \
		underrunProtect(2);\
		MODRM(d,s); \
		*(--_nIns) = (uint8_t) (c)

#define ALUi(c,r,i) \
   		underrunProtect(6); \
		NanoAssert(unsigned(r)<8);\
 		if (isS8(i)) { \
			*(--_nIns) = uint8_t(i); \
            MODRM((c>>3),(r)); \
            *(--_nIns) = uint8_t(0x83); \
 		} else { \
 			IMM32(i); \
 			if ( (r) == EAX) { \
 				*(--_nIns) = (uint8_t) (c); \
 			} else { \
                MODRM((c>>3),(r)); \
                *(--_nIns) = uint8_t(0x81); \
 			} \
 		}

#define ALUmi(c,d,b,i) \
		underrunProtect(10); \
		NanoAssert(((unsigned)b)<8); \
 		if (isS8(i)) { \
			*(--_nIns) = uint8_t(i); \
            MODRMm((c>>3),(d),(b)); \
            *(--_nIns) = uint8_t(0x83); \
 		} else { \
 			IMM32(i); \
            MODRMm((c>>3),(d),(b)); \
            *(--_nIns) = uint8_t(0x81); \
 		}

#define ALU2(c,d,s) \
		underrunProtect(3); \
		MODRM((d),(s));	\
		_nIns -= 2; \
		_nIns[0] = (uint8_t) ( ((c)>>8) ); \
		_nIns[1] = (uint8_t) ( (c) )

#define LAHF()		do { ALU0(0x9F);					asm_output("lahf"); } while(0)
#define SAHF()		do { ALU0(0x9E);					asm_output("sahf"); } while(0)
#define OR(l,r)		do { ALU(0x0b, (l),(r));			asm_output2("or %s,%s",gpn(l),gpn(r)); } while(0)
#define AND(l,r)	do { ALU(0x23, (l),(r));			asm_output2("and %s,%s",gpn(l),gpn(r)); } while(0)
#define XOR(l,r)	do { ALU(0x33, (l),(r));			asm_output2("xor %s,%s",gpn(l),gpn(r)); } while(0)
#define ADD(l,r)	do { ALU(0x03, (l),(r));			asm_output2("add %s,%s",gpn(l),gpn(r)); } while(0)
#define SUB(l,r)	do { ALU(0x2b, (l),(r));			asm_output2("sub %s,%s",gpn(l),gpn(r)); } while(0)
#define MUL(l,r)	do { ALU2(0x0faf,(l),(r));		asm_output2("mul %s,%s",gpn(l),gpn(r)); } while(0)
#define NOT(r)		do { ALU(0xf7, (Register)2,(r));	asm_output1("not %s",gpn(r)); } while(0)
#define NEG(r)		do { ALU(0xf7, (Register)3,(r));	asm_output1("neg %s",gpn(r)); } while(0)
#define SHR(r,s)	do { ALU(0xd3, (Register)5,(r));	asm_output2("shr %s,%s",gpn(r),gpn(s)); } while(0)
#define SAR(r,s)	do { ALU(0xd3, (Register)7,(r));	asm_output2("sar %s,%s",gpn(r),gpn(s)); } while(0)
#define SHL(r,s)	do { ALU(0xd3, (Register)4,(r));	asm_output2("shl %s,%s",gpn(r),gpn(s)); } while(0)

#define SHIFT(c,r,i) \
		underrunProtect(3);\
		*--_nIns = (uint8_t)(i);\
		MODRM((Register)c,r);\
		*--_nIns = 0xc1;

#define SHLi(r,i)	do { SHIFT(4,r,i);	asm_output2("shl %s,%d", gpn(r),i); } while(0)
#define SHRi(r,i)	do { SHIFT(5,r,i);	asm_output2("shr %s,%d", gpn(r),i); } while(0)
#define SARi(r,i)	do { SHIFT(7,r,i);	asm_output2("sar %s,%d", gpn(r),i); } while(0)

#define MOVZX8(d,s) do { ALU2(0x0fb6,d,s); asm_output2("movzx %s,%s", gpn(d),gpn(s)); } while(0)

#define SUBi(r,i)	do { ALUi(0x2d,r,i);				asm_output2("sub %s,%d",gpn(r),i); } while(0)
#define ADDi(r,i)	do { ALUi(0x05,r,i);				asm_output2("add %s,%d",gpn(r),i); } while(0)
#define ANDi(r,i)	do { ALUi(0x25,r,i);				asm_output2("and %s,%d",gpn(r),i); } while(0)
#define ORi(r,i)	do { ALUi(0x0d,r,i);				asm_output2("or %s,%d",gpn(r),i); } while(0)
#define XORi(r,i)	do { ALUi(0x35,r,i);				asm_output2("xor %s,%d",gpn(r),i); } while(0)

#define ADDmi(d,b,i) do { ALUmi(0x05, d, b, i); asm_output3("add %d(%s), %d", d, gpn(b), i); } while(0)

#define TEST(d,s)	do { ALU(0x85,d,s);				asm_output2("test %s,%s",gpn(d),gpn(s)); } while(0)
#define CMP(l,r)	do { ALU(0x3b, (l),(r));			asm_output2("cmp %s,%s",gpn(l),gpn(r)); } while(0)
#define CMPi(r,i)	do { ALUi(0x3d,r,i);				asm_output2("cmp %s,%d",gpn(r),i); } while(0)

#define MR(d,s)		do { ALU(0x8b,d,s);				asm_output2("mov %s,%s",gpn(d),gpn(s)); } while(0)
#define LEA(r,d,b)	do { ALUm(0x8d, r,d,b);			asm_output3("lea %s,%d(%s)",gpn(r),d,gpn(b)); } while(0)

#define SETE(r)		do { ALU2(0x0f94,(r),(r));			asm_output1("sete %s",gpn(r)); } while(0)
#define SETNP(r)	do { ALU2(0x0f9B,(r),(r));			asm_output1("setnp %s",gpn(r)); } while(0)
#define SETL(r)		do { ALU2(0x0f9C,(r),(r));			asm_output1("setl %s",gpn(r)); } while(0)
#define SETLE(r)	do { ALU2(0x0f9E,(r),(r));			asm_output1("setle %s",gpn(r)); } while(0)
#define SETG(r)		do { ALU2(0x0f9F,(r),(r));			asm_output1("setg %s",gpn(r)); } while(0)
#define SETGE(r)	do { ALU2(0x0f9D,(r),(r));			asm_output1("setge %s",gpn(r)); } while(0)
#define SETB(r)     do { ALU2(0x0f92,(r),(r));          asm_output1("setb %s",gpn(r)); } while(0)
#define SETBE(r)    do { ALU2(0x0f96,(r),(r));          asm_output1("setbe %s",gpn(r)); } while(0)
#define SETA(r)     do { ALU2(0x0f97,(r),(r));          asm_output1("seta %s",gpn(r)); } while(0)
#define SETAE(r)    do { ALU2(0x0f93,(r),(r));          asm_output1("setae %s",gpn(r)); } while(0)
#define SETC(r)     do { ALU2(0x0f90,(r),(r));          asm_output1("setc %s",gpn(r)); } while(0)
#define SETO(r)     do { ALU2(0x0f92,(r),(r));          asm_output1("seto %s",gpn(r)); } while(0)

#define MREQ(dr,sr)	do { ALU2(0x0f44,dr,sr); asm_output2("cmove %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRNE(dr,sr)	do { ALU2(0x0f45,dr,sr); asm_output2("cmovne %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRL(dr,sr)	do { ALU2(0x0f4C,dr,sr); asm_output2("cmovl %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRLE(dr,sr)	do { ALU2(0x0f4E,dr,sr); asm_output2("cmovle %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRG(dr,sr)	do { ALU2(0x0f4F,dr,sr); asm_output2("cmovg %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRGE(dr,sr)	do { ALU2(0x0f4D,dr,sr); asm_output2("cmovge %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRB(dr,sr)	do { ALU2(0x0f42,dr,sr); asm_output2("cmovb %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRBE(dr,sr)	do { ALU2(0x0f46,dr,sr); asm_output2("cmovbe %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRA(dr,sr)	do { ALU2(0x0f47,dr,sr); asm_output2("cmova %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRNC(dr,sr)	do { ALU2(0x0f43,dr,sr); asm_output2("cmovnc %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRAE(dr,sr)	do { ALU2(0x0f43,dr,sr); asm_output2("cmovae %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRNO(dr,sr)	do { ALU2(0x0f41,dr,sr); asm_output2("cmovno %s,%s", gpn(dr),gpn(sr)); } while(0)

// these aren't currently used but left in for reference
//#define LDEQ(r,d,b) do { ALU2m(0x0f44,r,d,b); asm_output3("cmove %s,%d(%s)", gpn(r),d,gpn(b)); } while(0)
//#define LDNEQ(r,d,b) do { ALU2m(0x0f45,r,d,b); asm_output3("cmovne %s,%d(%s)", gpn(r),d,gpn(b)); } while(0)

#define LD(reg,disp,base)	do { 	\
	ALUm(0x8b,reg,disp,base);	\
	asm_output3("mov %s,%d(%s)",gpn(reg),disp,gpn(base)); } while(0)

#define LDdm(reg,addr) do {		\
	ALUdm(0x8b,reg,addr);		\
	asm_output2("mov %s,0(%lx)",gpn(reg),addr); \
	} while (0)


#define SIBIDX(n)	"1248"[n]

#define LDsib(reg,disp,base,index,scale) do {	\
	ALUsib(0x8b,reg,base,index,scale,disp);		\
	asm_output5("mov %s,%d(%s+%s*%c)",gpn(reg),disp,gpn(base),gpn(index),SIBIDX(scale)); \
	} while (0)

// load 16-bit, sign extend
#define LD16S(r,d,b) do { ALU2m(0x0fbf,r,d,b); asm_output3("movsx %s,%d(%s)", gpn(r),d,gpn(b)); } while(0)
	
// load 16-bit, zero extend
#define LD16Z(r,d,b) do { ALU2m(0x0fb7,r,d,b); asm_output3("movsz %s,%d(%s)", gpn(r),d,gpn(b)); } while(0)

#define LD16Zdm(r,addr) do { ALU2dm(0x0fb7,r,addr); asm_output2("movsz %s,0(%lx)", gpn(r),addr); } while (0)

#define LD16Zsib(r,disp,base,index,scale) do {	\
	ALU2sib(0x0fb7,r,base,index,scale,disp);	\
	asm_output5("movsz %s,%d(%s+%s*%c)",gpn(r),disp,gpn(base),gpn(index),SIBIDX(scale)); \
	} while (0)

// load 8-bit, zero extend
// note, only 5-bit offsets (!) are supported for this, but that's all we need at the moment
// (movzx actually allows larger offsets mode but 5-bit gives us advantage in Thumb mode)
#define LD8Z(r,d,b)	do { NanoAssert((d)>=0&&(d)<=31); ALU2m(0x0fb6,r,d,b); asm_output3("movzx %s,%d(%s)", gpn(r),d,gpn(b)); } while(0)

#define LD8Zdm(r,addr) do { \
	NanoAssert((d)>=0&&(d)<=31); \
	ALU2dm(0x0fb6,r,addr); \
	asm_output2("movzx %s,0(%lx)", gpn(r),addr); \
	} while(0)

#define LD8Zsib(r,disp,base,index,scale) do {	\
	NanoAssert((d)>=0&&(d)<=31);				\
	ALU2sib(0x0fb6,r,base,index,scale,disp);	\
	asm_output5("movzx %s,%d(%s+%s*%c)",gpn(r),disp,gpn(base),gpn(index),SIBIDX(scale)); \
	} while(0)
	

#define LDi(r,i) do { \
	underrunProtect(5);			\
	IMM32(i);					\
	NanoAssert(((unsigned)r)<8); \
	*(--_nIns) = (uint8_t) (0xb8 | (r) );		\
	asm_output2("mov %s,%d",gpn(r),i); } while(0)

#define ST(base,disp,reg) do {  \
	ALUm(0x89,reg,disp,base);	\
    asm_output3("mov %d(%s),%s",disp,base==UnknownReg?"0":gpn(base),gpn(reg)); } while(0)

#define STi(base,disp,imm)	do { \
	underrunProtect(12);	\
	IMM32(imm);				\
	MODRMm(0, disp, base);	\
	*(--_nIns) = 0xc7;		\
	asm_output3("mov %d(%s),%d",disp,gpn(base),imm); } while(0)

#define RET()   do { ALU0(0xc3); asm_output("ret"); } while(0)
#define NOP() 	do { ALU0(0x90); asm_output("nop"); } while(0)
#define INT3()  do { ALU0(0xcc); asm_output("int3"); } while(0)

#define PUSHi(i) do { \
	if (isS8(i)) { \
		underrunProtect(2);			\
		_nIns-=2; _nIns[0] = 0x6a; _nIns[1] = (uint8_t)(i); \
		asm_output1("push %d",i); \
	} else \
		{ PUSHi32(i); } } while(0)

#define PUSHi32(i)	do {	\
	underrunProtect(5);	\
	IMM32(i);			\
	*(--_nIns) = 0x68;	\
	asm_output1("push %d",i); } while(0)

#define PUSHr(r) do {  \
	underrunProtect(1);			\
	NanoAssert(((unsigned)r)<8); \
	*(--_nIns) = (uint8_t) ( 0x50 | (r) );	\
	asm_output1("push %s",gpn(r)); } while(0)

#define PUSHm(d,b) do { \
	ALUm(0xff, 6, d, b);		\
	asm_output2("push %d(%s)",d,gpn(b)); } while(0)

#define POPr(r) do { \
	underrunProtect(1);			\
	NanoAssert(((unsigned)r)<8); \
	*(--_nIns) = (uint8_t) ( 0x58 | (r) ); \
	asm_output1("pop %s",gpn(r)); } while(0)

#define JCC32 0x0f
#define JMP8  0xeb
#define JMP32 0xe9  
    
#define JCC(o,t,isfar,n) do { \
	underrunProtect(6);	\
	intptr_t tt = (intptr_t)t - (intptr_t)_nIns;	\
	if (isS8(tt) && !isfar) { \
		verbose_only( NIns* next = _nIns; (void)next; ) \
		_nIns -= 2; \
		_nIns[0] = (uint8_t) ( 0x70 | (o) ); \
		_nIns[1] = (uint8_t) (tt); \
		asm_output2("%s %p",(n),(next+tt)); \
	} else { \
		verbose_only( NIns* next = _nIns; ) \
		IMM32(tt); \
		_nIns -= 2; \
		_nIns[0] = JCC32; \
		_nIns[1] = (uint8_t) ( 0x80 | (o) ); \
		asm_output2("%s %p",(n),(next+tt)); \
	} } while(0)

#define JMP_long(t) do { \
	underrunProtect(5);	\
	intptr_t tt = (intptr_t)t - (intptr_t)_nIns;	\
	JMP_long_nochk_offset(tt);	\
    verbose_only( verbose_outputf("        %p:",_nIns); ) \
	} while(0)

#define JMP(t)		do { 	\
   	underrunProtect(5);	\
	intptr_t tt = (intptr_t)t - (intptr_t)_nIns;	\
	if (isS8(tt)) { \
		verbose_only( NIns* next = _nIns; (void)next; ) \
		_nIns -= 2; \
		_nIns[0] = JMP8; \
		_nIns[1] = (uint8_t) ( (tt)&0xff ); \
		asm_output1("jmp %p",(next+tt)); \
	} else { \
		JMP_long_nochk_offset(tt);	\
	} } while(0)

// this should only be used when you can guarantee there is enough room on the page
#define JMP_long_nochk_offset(o) do {\
		verbose_only( NIns* next = _nIns; (void)next; ) \
 		IMM32((o)); \
 		*(--_nIns) = JMP32; \
		asm_output1("jmp %p",(next+(o))); } while(0)

#define JE(t, isfar)	   JCC(0x04, t, isfar, "je")
#define JNE(t, isfar)	   JCC(0x05, t, isfar, "jne")
#define JP(t, isfar)	   JCC(0x0A, t, isfar, "jp")
#define JNP(t, isfar)	   JCC(0x0B, t, isfar, "jnp")

#define JB(t, isfar)	   JCC(0x02, t, isfar, "jb")
#define JNB(t, isfar)	   JCC(0x03, t, isfar, "jnb")
#define JBE(t, isfar)	   JCC(0x06, t, isfar, "jbe")
#define JNBE(t, isfar)   JCC(0x07, t, isfar, "jnbe")

#define JA(t, isfar)	   JCC(0x07, t, isfar, "ja")
#define JNA(t, isfar)	   JCC(0x06, t, isfar, "jna")
#define JAE(t, isfar)	   JCC(0x03, t, isfar, "jae")
#define JNAE(t, isfar)   JCC(0x02, t, isfar, "jnae")

#define JL(t, isfar)	   JCC(0x0C, t, isfar, "jl")
#define JNL(t, isfar)	   JCC(0x0D, t, isfar, "jnl")
#define JLE(t, isfar)	   JCC(0x0E, t, isfar, "jle")
#define JNLE(t, isfar)   JCC(0x0F, t, isfar, "jnle")

#define JG(t, isfar)	   JCC(0x0F, t, isfar, "jg")
#define JNG(t, isfar)	   JCC(0x0E, t, isfar, "jng")
#define JGE(t, isfar)	   JCC(0x0D, t, isfar, "jge")
#define JNGE(t, isfar)   JCC(0x0C, t, isfar, "jnge")

#define JC(t, isfar)     JCC(0x02, t, isfar, "jc")
#define JNC(t, isfar)    JCC(0x03, t, isfar, "jnc")
#define JO(t, isfar)     JCC(0x00, t, isfar, "jo")
#define JNO(t, isfar)    JCC(0x01, t, isfar, "jno")

// sse instructions 
#define SSE(c,d,s)  \
		underrunProtect(9);	\
		MODRM((d),(s));	\
		_nIns -= 3; \
 		_nIns[0] = (uint8_t)(((c)>>16)&0xff); \
		_nIns[1] = (uint8_t)(((c)>>8)&0xff); \
		_nIns[2] = (uint8_t)((c)&0xff)

#define SSEm(c,r,d,b)	\
		underrunProtect(9);	\
 		MODRMm((r),(d),(b));	\
		_nIns -= 3;		\
 		_nIns[0] = (uint8_t)(((c)>>16)&0xff); \
		_nIns[1] = (uint8_t)(((c)>>8)&0xff); \
		_nIns[2] = (uint8_t)((c)&0xff)

#define LDSD(r,d,b)do {     \
    SSEm(0xf20f10, (r)&7, (d), (b)); \
    asm_output3("movsd %s,%d(%s)",gpn(r),(d),gpn(b)); \
    } while(0)

#define LDSDm(r,addr)do {     \
    underrunProtect(8); \
	const double* daddr = addr; \
    IMM32(int32_t(daddr));\
    *(--_nIns) = uint8_t(((r)&7)<<3|5); \
    *(--_nIns) = 0x10;\
    *(--_nIns) = 0x0f;\
    *(--_nIns) = 0xf2;\
    asm_output3("movsd %s,(#%p) // =%f",gpn(r),(void*)daddr,*daddr); \
    } while(0)

#define STSD(d,b,r)do {     \
    SSEm(0xf20f11, (r)&7, (d), (b)); \
    asm_output3("movsd %d(%s),%s",(d),gpn(b),gpn(r)); \
    } while(0)

#define SSE_LDQ(r,d,b)do {  \
    SSEm(0xf30f7e, (r)&7, (d), (b)); \
    asm_output3("movq %s,%d(%s)",gpn(r),d,gpn(b)); \
    } while(0)

#define SSE_STQ(d,b,r)do {  \
    SSEm(0x660fd6, (r)&7, (d), (b)); \
    asm_output3("movq %d(%s),%s",(d),gpn(b),gpn(r)); \
    } while(0)

#define SSE_CVTSI2SD(xr,gr) do{ \
    SSE(0xf20f2a, (xr)&7, (gr)&7); \
    asm_output2("cvtsi2sd %s,%s",gpn(xr),gpn(gr)); \
    } while(0)

#define CVTDQ2PD(dstr,srcr) do{ \
    SSE(0xf30fe6, (dstr)&7, (srcr)&7); \
    asm_output2("cvtdq2pd %s,%s",gpn(dstr),gpn(srcr)); \
    } while(0)

// move and zero-extend gpreg to xmm reg
#define SSE_MOVD(d,s) do{ \
	if (_is_xmm_reg_(s)) { \
		NanoAssert(_is_gp_reg_(d)); \
		SSE(0x660f7e, (s)&7, (d)&7); \
	} else { \
		NanoAssert(_is_gp_reg_(s)); \
		NanoAssert(_is_xmm_reg_(d)); \
		SSE(0x660f6e, (d)&7, (s)&7); \
	} \
    asm_output2("movd %s,%s",gpn(d),gpn(s)); \
    } while(0)

#define SSE_MOVSD(rd,rs) do{ \
    NanoAssert(_is_xmm_reg_(rd) && _is_xmm_reg_(rs));\
    SSE(0xf20f10, (rd)&7, (rs)&7); \
    asm_output2("movsd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)

#define SSE_MOVDm(d,b,xrs) do {\
    NanoAssert(_is_xmm_reg_(xrs) && _is_gp_reg_(b));\
    SSEm(0x660f7e, (xrs)&7, d, b);\
    asm_output3("movd %d(%s),%s", d, gpn(b), gpn(xrs));\
    } while(0)

#define SSE_ADDSD(rd,rs) do{ \
    NanoAssert(_is_xmm_reg_(rd) && _is_xmm_reg_(rs));\
    SSE(0xf20f58, (rd)&7, (rs)&7); \
    asm_output2("addsd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)

#define SSE_ADDSDm(r,addr)do {     \
    underrunProtect(8); \
    NanoAssert(_is_xmm_reg_(r));\
	const double* daddr = addr; \
    IMM32(int32_t(daddr));\
    *(--_nIns) = uint8_t(((r)&7)<<3|5); \
    *(--_nIns) = 0x58;\
    *(--_nIns) = 0x0f;\
    *(--_nIns) = 0xf2;\
    asm_output3("addsd %s,%p // =%f",gpn(r),(void*)daddr,*daddr); \
    } while(0)

#define SSE_SUBSD(rd,rs) do{ \
    NanoAssert(_is_xmm_reg_(rd) && _is_xmm_reg_(rs));\
    SSE(0xf20f5c, (rd)&7, (rs)&7); \
    asm_output2("subsd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)
#define SSE_MULSD(rd,rs) do{ \
    NanoAssert(_is_xmm_reg_(rd) && _is_xmm_reg_(rs));\
    SSE(0xf20f59, (rd)&7, (rs)&7); \
    asm_output2("mulsd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)
#define SSE_DIVSD(rd,rs) do{ \
    NanoAssert(_is_xmm_reg_(rd) && _is_xmm_reg_(rs));\
    SSE(0xf20f5e, (rd)&7, (rs)&7); \
    asm_output2("divsd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)
#define SSE_UCOMISD(rl,rr) do{ \
    NanoAssert(_is_xmm_reg_(rl) && _is_xmm_reg_(rr));\
    SSE(0x660f2e, (rl)&7, (rr)&7); \
    asm_output2("ucomisd %s,%s",gpn(rl),gpn(rr)); \
    } while(0)

#define CVTSI2SDm(xr,d,b) do{ \
    NanoAssert(_is_xmm_reg_(xr) && _is_gp_reg_(b));\
    SSEm(0xf20f2a, (xr)&7, (d), (b)); \
    asm_output3("cvtsi2sd %s,%d(%s)",gpn(xr),(d),gpn(b)); \
    } while(0)

#define SSE_XORPD(r, maskaddr) do {\
	underrunProtect(8); \
    IMM32(maskaddr);\
    *(--_nIns) = uint8_t(((r)&7)<<3|5); \
    *(--_nIns) = 0x57;\
    *(--_nIns) = 0x0f;\
    *(--_nIns) = 0x66;\
    asm_output2("xorpd %s,[0x%p]",gpn(r),(void*)(maskaddr));\
    } while(0)

#define SSE_XORPDr(rd,rs) do{ \
    SSE(0x660f57, (rd)&7, (rs)&7); \
    asm_output2("xorpd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)

// floating point unit
#define FPUc(o)								\
		underrunProtect(2);					\
		*(--_nIns) = ((uint8_t)(o)&0xff);		\
		*(--_nIns) = (uint8_t)(((o)>>8)&0xff)

#define FPU(o,r)							\
		underrunProtect(2);					\
		*(--_nIns) = uint8_t(((uint8_t)(o)&0xff) | r&7);\
		*(--_nIns) = (uint8_t)(((o)>>8)&0xff)

#define FPUm(o,d,b)							\
		underrunProtect(7);					\
		MODRMm((uint8_t)(o), d, b);			\
		*(--_nIns) = (uint8_t)((o)>>8)

#define TEST_AH(i) do { 							\
		underrunProtect(3);					\
		*(--_nIns) = ((uint8_t)(i));			\
		*(--_nIns) = 0xc4;					\
		*(--_nIns) = 0xf6;					\
		asm_output1("test ah, %d",i); } while(0)

#define TEST_AX(i) do { 							\
		underrunProtect(5);					\
		*(--_nIns) = (0);		\
		*(--_nIns) = ((uint8_t)(i));			\
		*(--_nIns) = ((uint8_t)((i)>>8));		\
		*(--_nIns) = (0);		\
		*(--_nIns) = 0xa9;					\
		asm_output1("test ax, %d",i); } while(0)

#define FNSTSW_AX()	do { FPUc(0xdfe0);				asm_output("fnstsw_ax"); } while(0)
#define FCHS()		do { FPUc(0xd9e0);				asm_output("fchs"); } while(0)
#define FLD1()		do { FPUc(0xd9e8);				asm_output("fld1"); fpu_push(); } while(0)
#define FLDZ()		do { FPUc(0xd9ee);				asm_output("fldz"); fpu_push(); } while(0)
#define FFREE(r)	do { FPU(0xddc0, r);			asm_output1("ffree %s",fpn(r)); } while(0)
#define FSTQ(p,d,b)	do { FPUm(0xdd02|(p), d, b);	asm_output3("fst%sq %d(%s)",((p)?"p":""),d,gpn(b)); if (p) fpu_pop(); } while(0)
#define FSTPQ(d,b)  FSTQ(1,d,b)
#define FCOM(p,d,b)	do { FPUm(0xdc02|(p), d, b);	asm_output3("fcom%s %d(%s)",((p)?"p":""),d,gpn(b)); if (p) fpu_pop(); } while(0)
#define FLDQ(d,b)	do { FPUm(0xdd00, d, b);		asm_output2("fldq %d(%s)",d,gpn(b)); fpu_push();} while(0)
#define FILDQ(d,b)	do { FPUm(0xdf05, d, b);		asm_output2("fildq %d(%s)",d,gpn(b)); fpu_push(); } while(0)
#define FILD(d,b)	do { FPUm(0xdb00, d, b);		asm_output2("fild %d(%s)",d,gpn(b)); fpu_push(); } while(0)
#define FADD(d,b)	do { FPUm(0xdc00, d, b);		asm_output2("fadd %d(%s)",d,gpn(b)); } while(0)
#define FSUB(d,b)	do { FPUm(0xdc04, d, b);		asm_output2("fsub %d(%s)",d,gpn(b)); } while(0)
#define FSUBR(d,b)	do { FPUm(0xdc05, d, b);		asm_output2("fsubr %d(%s)",d,gpn(b)); } while(0)
#define FMUL(d,b)	do { FPUm(0xdc01, d, b);		asm_output2("fmul %d(%s)",d,gpn(b)); } while(0)
#define FDIV(d,b)	do { FPUm(0xdc06, d, b);		asm_output2("fdiv %d(%s)",d,gpn(b)); } while(0)
#define FDIVR(d,b)	do { FPUm(0xdc07, d, b);		asm_output2("fdivr %d(%s)",d,gpn(b)); } while(0)
#define FINCSTP()	do { FPUc(0xd9f7);				asm_output2("fincstp"); } while(0)
#define FSTP(r)		do { FPU(0xddd8, r&7);			asm_output1("fstp %s",fpn(r)); fpu_pop();} while(0)
#define FCOMP()		do { FPUc(0xD8D9);				asm_output("fcomp"); fpu_pop();} while(0)
#define FCOMPP()	do { FPUc(0xDED9);				asm_output("fcompp"); fpu_pop();fpu_pop();} while(0)
#define FLDr(r)		do { FPU(0xd9c0,r);				asm_output1("fld %s",fpn(r)); fpu_push(); } while(0)
#define EMMS()		do { FPUc(0x0f77);				asm_output("emms"); } while (0)

// standard direct call
#define CALL(c)	do { \
  underrunProtect(5);					\
  int offset = (c->_address) - ((int)_nIns); \
  IMM32( (uint32_t)offset );	\
  *(--_nIns) = 0xE8;		\
  verbose_only(asm_output1("call %s",(c->_name));) \
  debug_only(if ((c->_argtypes&3)==ARGSIZE_F) fpu_push();)\
} while (0)

// indirect call thru register
#define CALLr(c,r)	do { \
  underrunProtect(2);\
  ALU(0xff, 2, (r));\
  verbose_only(asm_output1("call %s",gpn(r));) \
  debug_only(if ((c->_argtypes&3)==ARGSIZE_F) fpu_push();)\
} while (0)


}
#endif // __nanojit_Nativei386__
