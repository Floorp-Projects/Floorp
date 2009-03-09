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


#ifndef __nanojit_NativeThumb__
#define __nanojit_NativeThumb__


namespace nanojit
{
	const int NJ_LOG2_PAGE_SIZE	= 12;		// 4K
	const int NJ_MAX_REGISTERS = 6; // R0-R5
	const int NJ_MAX_STACK_ENTRY = 256;
	const int NJ_MAX_PARAMETERS = 1;
	const int NJ_ALIGN_STACK = 8;
	const int NJ_STACK_OFFSET = 8;

	#define NJ_CONSTANT_POOLS
    const int NJ_MAX_CPOOL_OFFSET = 1024;
    const int NJ_CPOOL_SIZE = 16;

	#define NJ_SOFTFLOAT
	#define NJ_STACK_GROWTH_UP
	#define NJ_THUMB_JIT

	const int LARGEST_UNDERRUN_PROT = 32;  // largest value passed to underrunProtect

	typedef unsigned short NIns;

	/* ARM registers */
	typedef enum 
	{
		R0  = 0,  // 32bit return value, aka A1
		R1  = 1,  // msw of 64bit return value, A2
		R2  = 2,  // A3
		R3  = 3,  // A4
		R4  = 4,  // V1
		R5  = 5,  // V2
		R6  = 6,  // V3
		R7  = 7,  // V4
		R8  = 8,  // V5
		R9  = 9,   // V6, SB (stack base)
		R10 = 10,  // V7, SL
		FP  = 11,  // V8, frame pointer 
		IP  = 12,  // intra-procedure call scratch register
		SP  = 13,  // stack pointer
		LR  = 14,  // link register (BL sets LR = return address)
		PC  = 15,  // program counter
		
		FirstReg = 0,
		LastReg = 5,
		Scratch	= 6,
		UnknownReg = 6
	}
	Register;

	typedef int RegisterMask;
	typedef struct _FragInfo
	{
		RegisterMask	needRestoring;
		NIns*			epilogue;
	} 
	FragInfo;

	static const int NumSavedRegs = 4;
	static const RegisterMask SavedRegs = 1<<R4 | 1<<R5 | 1<<R6 | 1<<R7;
	static const RegisterMask FpRegs = 0x0000; // FST0-FST7
	static const RegisterMask GpRegs = 0x003F;
	static const RegisterMask AllowableFlagRegs = 1<<R0 | 1<<R1 | 1<<R2 | 1<<R3 | 1<<R4 | 1<<R5;

	#define firstreg()		R0
	#define nextreg(r)		(Register)((int)r+1)
	#define imm2register(c) (Register)(c-1)
 
	verbose_only( extern const char* regNames[]; )

	// abstract to platform specific calls
	#define nExtractPlatformFlags(x)	0

	#define DECLARE_PLATFORM_STATS()

	#define DECLARE_PLATFORM_REGALLOC()

	#define DECLARE_PLATFORM_ASSEMBLER()\
        const static Register argRegs[4], retRegs[2];\
		void STi(Register b, int32_t d, int32_t v);\
		void LDi(Register r, int32_t v);\
		void BL(NIns* target);\
		void PUSH_mask(RegisterMask);\
		void POP_mask(RegisterMask);\
		void POPr(Register);\
		void underrunProtect(int bytes);\
		void B_cond(int c, NIns *target);\
		void B(NIns *target);\
		void MOVi(Register r, int32_t imm);\
		void ST(Register base, int32_t offset, Register reg);\
		void STR_m(Register base, int32_t offset, Register reg);\
		void STR_index(Register base, Register off, Register reg);\
		void STR_sp(int32_t offset, Register reg);\
		void STMIA(Register base, RegisterMask regs);\
		void LDMIA(Register base, RegisterMask regs);\
		void ADDi(Register r, int32_t imm);\
		void ADDi8(Register r, int32_t imm);\
		void SUBi(Register r, int32_t imm);\
		void SUBi8(Register r, int32_t imm);\
		void JMP(NIns *target);\
        void LD32_nochk(Register r, int32_t imm);\
		void CALL(const CallInfo*);\
		void nativePageReset();\
		void nativePageSetup();\
		int* _nPool;\
		int* _nSlot;\
		int* _nExitPool;\
		int* _nExitSlot;

    #define asm_farg(i) NanoAssert(false)

	#define swapptrs()  { NIns* _tins = _nIns; _nIns=_nExitIns; _nExitIns=_tins; \
								int* _npool = _nPool;\
								int* _nslot = _nSlot;\
								_nPool = _nExitPool; _nExitPool = _npool;\
								_nSlot = _nExitSlot; _nExitSlot = _nslot;}

#define BX(r)		do {\
	underrunProtect(2); \
	*(--_nIns) = (NIns)(0x4700 | ((r)<<3));\
	asm_output("bx %s",gpn(r)); } while(0)

#define OR(l,r)		do {underrunProtect(2); *(--_nIns) = (NIns)(0x4300 | (r<<3) | l); asm_output("or %s,%s",gpn(l),gpn(r)); } while(0)
#define ORi(r,i)	do {										\
	if (isS8(i)) {												\
		underrunProtect(4); 									\
		*(--_nIns) = (NIns)(0x4300 | (Scratch<<3) | (r));			\
		*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | (i&0xFF));} \
	else if (isS8(i) && int32_t(i)<0) {									\
		underrunProtect(6);												\
		*(--_nIns) = (NIns)(0x4030 | (Scratch<<3) | (r) ); 				\
		*(--_nIns) = (NIns)(0x4240 | ((Scratch)<<3) | (Scratch));		\
		*(--_nIns) = (NIns)(0x2000 | ((Scratch)<<8) | ((-(i))&0xFF) );}	\
	else NanoAssert(0);													\
	asm_output("or %s,%d",gpn(r), i); } while(0)

#define AND(l,r)	do {underrunProtect(2); *(--_nIns) = (NIns)(0x4000 | ((r)<<3) | (l)); asm_output("and %s,%s",gpn(l),gpn(r)); } while(0)

#define ANDi(_r,_i) do {													\
	if (isU8(_i)) {														\
		underrunProtect(4);												\
		*(--_nIns) = (NIns)(0x4000 | (Scratch<<3) | (_r) ); 				\
		*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | ((_i)&0xFF) );} 		\
	else if (isS8(_i) && int32_t(_i)<0) {									\
		underrunProtect(6);												\
		*(--_nIns) = (NIns)(0x4000 | (Scratch<<3) | (_r) ); 				\
		*(--_nIns) = (NIns)(0x4240 | ((Scratch)<<3) | (Scratch));		\
		*(--_nIns) = (NIns)(0x2000 | ((Scratch)<<8) | ((-(_i))&0xFF) );}	\
	else {											\
		underrunProtect(2);										\
		*(--_nIns) = (NIns)(0x4000 |  ((Scratch)<<3) | (_r));	\
		LDi(Scratch, (_i));}											\
	asm_output("and %s,%d",gpn(_r),(_i)); } while (0)


#define XOR(l,r)	do {underrunProtect(2); *(--_nIns) = (NIns)(0x4040 | ((r)<<3) | (l)); asm_output("eor %s,%s",gpn(l),gpn(r)); } while(0)
#define XORi(r,i)	do {	\
	if (isS8(i)){	\
		underrunProtect(4);		\
		*(--_nIns) = (NIns)(0x4040 | (Scratch<<3) | (r)); \
		*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | ((i)&0xFF));} \
	else if (isS8(i) && int32_t(i)<0) {									\
		underrunProtect(6);												\
		*(--_nIns) = (NIns)(0x4040 | (Scratch<<3) | (r) ); 				\
		*(--_nIns) = (NIns)(0x4240 | ((Scratch)<<3) | (Scratch));		\
		*(--_nIns) = (NIns)(0x2000 | ((Scratch)<<8) | ((-(i))&0xFF) );}	\
	else NanoAssert(0);													\
	asm_output("eor %s,%d",gpn(r),(i)); } while(0)

#define ADD3(d,l,r) do {underrunProtect(2); *(--_nIns) = (NIns)(0x1800 | ((r)<<6) | ((l)<<3) | (d)); asm_output("add %s,%s,%s",gpn(d),gpn(l),gpn(r)); } while(0)
#define ADD(l,r)    ADD3(l,l,r)

#define SUB(l,r)	do {underrunProtect(2); *(--_nIns) = (NIns)(0x1A00 | ((r)<<6) | ((l)<<3) | (l)); asm_output("sub %s,%s",gpn(l),gpn(r)); } while(0)
#define MUL(l,r)	do {underrunProtect(2); *(--_nIns) = (NIns)(0x4340 | ((r)<<3) | (l)); asm_output("mul %s,%s",gpn(l),gpn(r)); } while(0)

#define NEG(r)		do {\
	underrunProtect(2);\
	*(--_nIns) = (NIns)(0x4240 | ((r)<<3) | (r) );\
	asm_output("neg %s",gpn(r));\
 } while(0)

#define NOT(r)		do {underrunProtect(2);	*(--_nIns) = (NIns)(0x43C0 | ((r)<<3) | (r) ); asm_output("mvn %s",gpn(r)); } while(0)

#define SHR(r,s)	do {underrunProtect(2); *(--_nIns) = (NIns)(0x40C0 | ((s)<<3) | (r)); asm_output("shr %s,%s",gpn(r),gpn(s)); } while(0)
#define SHRi(r,i)	do {underrunProtect(2); *(--_nIns) = (NIns)(0x0800 | ((i)<<6) | ((r)<<3) | (r)); asm_output("shr %s,%d",gpn(r),i); } while(0)

#define SAR(r,s)	do {underrunProtect(2); *(--_nIns) = (NIns)(0x4100 | ((s)<<3) | (r)); asm_output("asr %s,%s",gpn(r),gpn(s)); } while(0)
#define SARi(r,i)	do {underrunProtect(2); *(--_nIns) = (NIns)(0x1000 | ((i)<<6) | ((r)<<3) | (r)); asm_output("asr %s,%d",gpn(r),i); } while(0)

#define SHL(r,s)	do {\
	underrunProtect(2); \
	*(--_nIns) = (NIns)(0x4080 | ((s)<<3) | (r)); \
	asm_output("lsl %s,%s",gpn(r),gpn(s));\
 } while(0)

#define SHLi(r,i)	do {\
	underrunProtect(2);\
	NanoAssert((i)>=0 && (i)<32);\
	*(--_nIns) = (NIns)(0x0000 | ((i)<<6) | ((r)<<3) | (r)); \
	asm_output("lsl %s,%d",gpn(r),(i));\
 } while(0)
					

					
#define TEST(d,s)	do{underrunProtect(2); *(--_nIns) = (NIns)(0x4200 | ((d)<<3) | (s)); asm_output("test %s,%s",gpn(d),gpn(s));} while(0)
#define CMP(l,r)	do{underrunProtect(2); *(--_nIns) = (NIns)(0x4280 | ((r)<<3) | (l)); asm_output("cmp %s,%s",gpn(l),gpn(r));} while(0)

#define CMPi(_r,_i)	do{													\
	if (_i<0) {															\
		NanoAssert(isS16((_i)));											\
		if ((_i)>-256)	{													\
			underrunProtect(4);													\
			*(--_nIns) = (NIns)(0x42C0 | ((Scratch)<<3) | (_r));					\
			asm_output("cmn %s,%s",gpn(_r),gpn(Scratch));						\
			*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | ((-(_i))&0xFF) );		\
			asm_output("mov %s,%d",gpn(Scratch),(_i));}					\
		else {																\
			NanoAssert(!((_i)&3));											\
			underrunProtect(10);											\
			*(--_nIns) = (NIns)(0x42C0 | ((Scratch)<<3) | (_r));			\
			asm_output("cmn %s,%s",gpn(_r),gpn(Scratch));					\
			*(--_nIns) = (NIns)(0x0000 | (2<<6) | (Scratch<<3) | (Scratch) );\
			asm_output("lsl %s,%d",gpn(Scratch),2);						\
			*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | ((-(_i)/4)&0xFF) );	\
			asm_output("mov %s,%d",gpn(Scratch),(_i));}				\
	} else {																\
		if ((_i)>255) {														\
			int pad=0;														\
			underrunProtect(2*(7));										\
			*(--_nIns) = (NIns)(0x4280 | ((Scratch)<<3) | (_r));			\
			asm_output("cmp %s,%X",gpn(_r),(_i));							\
			if ( (((int)(_nIns-2))%4) != 0)	pad=1;							\
			if (pad) {														\
				*(--_nIns) = 0xBAAD;										\
				asm_output("PAD 0xBAAD"); }									\
			*(--_nIns) = (short)((_i) >> 16);								\
			*(--_nIns) = (short)((_i) & 0xFFFF);							\
			asm_output("imm %d", (_i));									\
			*(--_nIns) = 0xBAAD;											\
			asm_output("PAD 0xBAAD");										\
			if (pad) *(--_nIns) = (NIns)(0xE000 | (6>>1));					\
			else *(--_nIns) = (NIns)(0xE000 | (4>>1));						\
			asm_output("b %X", (int)_nIns+(pad?6:4)+4);					\
			*(--_nIns) = (NIns)(0x4800 | ((Scratch)<<8) | (1));}			\
		else {																\
			NanoAssert((_i)<256);											\
			underrunProtect(2);												\
			*(--_nIns) = (NIns)(0x2800 | ((_r)<<8) | ((_i)&0xFF));			\
			asm_output("cmp %s,%X",gpn(_r),(_i));}							\
	} } while(0)

#define MR(d,s)	do {											\
	underrunProtect(2);											\
	if (((d)<8) && ((s)<8))											\
		*(--_nIns) = (NIns)(0x1C00 | ((s)<<3) | (d));				\
	else {														\
		if (d<8) *(--_nIns) = (NIns)(0x4600 | ((s)<<3) | ((d)&7));	\
		else *(--_nIns) = (NIns)(0x4600 | (1<<7) | ((s)<<3) | ((d)&7));\
	} asm_output("mov %s,%s",gpn(d),gpn(s)); } while (0)

// Thumb doesn't support conditional-move
#define MREQ(d,s)	do { NanoAssert(0); } while (0)
#define MRNE(d,s)	do { NanoAssert(0); } while (0)
#define MRL(d,s)	do { NanoAssert(0); } while (0)
#define MRLE(d,s)	do { NanoAssert(0); } while (0)
#define MRG(d,s)	do { NanoAssert(0); } while (0)
#define MRGE(d,s)	do { NanoAssert(0); } while (0)
#define MRB(d,s)	do { NanoAssert(0); } while (0)
#define MRBE(d,s)	do { NanoAssert(0); } while (0)
#define MRA(d,s)	do { NanoAssert(0); } while (0)
#define MRAE(d,s)	do { NanoAssert(0); } while (0)
#define MRNC(d,s)	do { NanoAssert(0); } while (0)
#define MRNO(d,s)	do { NanoAssert(0); } while (0)

#define LD(reg,offset,base) do{												\
	int off = (offset) >> 2;													\
	if (base==PC){															\
		underrunProtect(2);													\
		NanoAssert(off>=0 && off<256);												\
		*(--_nIns) = (NIns)(0x4800 | ((reg)<<8) | (off&0xFF));				\
		asm_output("ld %s,%d(%s)",gpn(reg),(offset),gpn(base));			\
	} else if (base==SP) {													\
		NanoAssert(off>=0);													\
		if (off<256){														\
			underrunProtect(2);												\
			*(--_nIns) = (NIns)(0x9800 | ((reg)<<8) | (off&0xFF));}			\
		else {																\
			underrunProtect(4);												\
			int rem = (offset) - 1020; NanoAssert(rem<125);					\
			*(--_nIns) = (NIns)(0x6800 | (rem&0x1F)<<6 | (reg)<<3 | (reg));	\
			*(--_nIns) = (NIns)(0xA800 | ((reg)<<8) | (0xFF));}				\
		asm_output("ld %s,%d(%s)",gpn(reg),(offset),gpn(base));			\
	} else if ((offset)<0) {												\
		underrunProtect(8);													\
		*(--_nIns) = (NIns)(0x5800 | (Scratch<<6) | (base<<3) | (reg));		\
		asm_output("ld %s,%d(%s)",gpn(reg),(offset),gpn(Scratch));			\
		*(--_nIns) = (NIns)(0x4240 | (Scratch<<3) | Scratch);				\
		asm_output("neg %s,%s",gpn(Scratch),gpn(Scratch));					\
		if ((offset)<-255){												\
			NanoAssert( (offset)>=-1020);											\
			*(--_nIns) = (NIns)(0x0000 | (2<<6) | (Scratch<<3) | (Scratch) );	\
			asm_output("lsl %s,%d",gpn(Scratch),2);					\
			*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | ((-(off))&0xFF) );	\
			asm_output("mov %s,%d",gpn(Scratch),(offset));}					\
		else {																\
			*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | ((-(offset))&0xFF) );	\
			asm_output("mov %s,%d",gpn(Scratch),(offset));}					\
	} else {																		\
		if ((off)<32) {																\
			underrunProtect(2);														\
			*(--_nIns) = (NIns)(0x6800 | ((off&0x1F)<<6) | ((base)<<3) | (reg));	\
			asm_output("ld %s,%d(%s)",gpn(reg),(offset),gpn(base));}				\
		else {																		\
			underrunProtect(2);														\
			*(--_nIns) = (NIns)(0x5800 | (Scratch<<6) | (base<<3) | (reg));			\
			asm_output("ld %s,%d(%s)",gpn(reg),(offset),gpn(Scratch));				\
			LDi(Scratch, (offset));}}											\
	} while(0)

// load 8-bit, zero extend  (aka LDRB)
// note, only 5-bit offsets (!) are supported for this, but that's all we need at the moment,
// and we get a nice advantage in Thumb mode...
#define LD8Z(_r,_d,_b) do{    \
    NanoAssert((_d)>=0 && (_d)<=31);\
    underrunProtect(2);\
    *(--_nIns) = (NIns)(0x7800 | (((_d)&31)<<6) | ((_b)<<3) | (_r) ); \
    asm_output("ldrb %s,%d(%s)", gpn(_r),(_d),gpn(_b));\
    } while(0)


#define LEA(_r,_d,_b) do{										\
	NanoAssert((_d)>=0);										\
	if (_b!=SP) NanoAssert(0);									\
	if ((int)(_d)<=1020 && (_d)>=0) {							\
		underrunProtect(2);										\
		*(--_nIns) = (NIns)(0xA800 | ((_r)<<8) | ((_d)>>2));}	\
	else {														\
		underrunProtect(4);										\
		int rem = (_d) - 1020; NanoAssert(rem<256);				\
		*(--_nIns) = (NIns)(0x3000 | ((_r)<<8) | ((rem)&0xFF));	\
		*(--_nIns) = (NIns)(0xA800 | ((_r)<<8) | (0xFF));}		\
	asm_output("lea %s, %d(SP)", gpn(_r), _d);					\
	} while(0)


//NanoAssert((t)<2048);
#define JMP_long_nochk_offset(t) do {								\
	*(--_nIns) = (NIns)(0xF800 | (((t)&0xFFF)>>1) );	\
	*(--_nIns) = (NIns)(0xF000 | (((t)>>12)&0x7FF) );	\
	asm_output("BL offset=%d",int(t));} while (0)

#define JMP_long_placeholder()	BL(_nIns)

// conditional branch
enum {
		EQ=0,
		NE,
		CSHS,
		CCLO,
		MI,		
		PL,
		VS,
		VC,
		HI,
		LS,
		GE,
		LT,
		GT,
		LE,
		AL,
		NV
};


#define JA(t)	B_cond(HI,t)
#define JNA(t)	B_cond(LS,t)
#define JB(t)	B_cond(CCLO,t)
#define JNB(t)	B_cond(CSHS,t)
#define JE(t)	B_cond(EQ,t)
#define JNE(t)	B_cond(NE,t)
#define JBE(t)	B_cond(LS,t)
#define JNBE(t) B_cond(HI,t)
#define JAE(t)	B_cond(CSHS,t)
#define JNAE(t) B_cond(CCLO,t)
#define JL(t)	B_cond(LT,t)
#define JNL(t)	B_cond(GE,t)
#define JG(t)	B_cond(GT,t)
#define JNG(t)	B_cond(LE,t)
#define JLE(t)	B_cond(LE,t)
#define JNLE(t)	B_cond(GT,t)
#define JGE(t)	B_cond(GE,t)
#define JNGE(t)	B_cond(LT,t)
#define JC(t)	B_cond(CSHS,t)
#define JNC(t)	B_cond(CCLO,t)
#define JO(t)	B_cond(VS,t)
#define JNO(t)	B_cond(VC,t)

// B(cond) +4	- if condition, skip to MOV
// EOR R, R		- set register to 0	
// B(AL) +2		- skip over next
// MOV R, 1 	- set register to 1
#define SET(r,cond)									\
	underrunProtect(10);								\
	*(--_nIns) = (NIns)(0x0000);					\
	*(--_nIns) = (NIns)(0x2000 | (r<<8) | (1));		\
	*(--_nIns) = (NIns)(0xE000 | 1 );				\
	*(--_nIns) = (NIns)(0x4040 | (r<<3) | r);		\
	*(--_nIns) = (NIns)(0xD000 | ((cond)<<8) | (1) );

#define SETE(r)		do {SET(r,EQ); asm_output("sete %s",gpn(r)); } while(0)
#define SETL(r)		do {SET(r,LT); asm_output("setl %s",gpn(r)); } while(0)
#define SETLE(r)	do {SET(r,LE); asm_output("setle %s",gpn(r)); } while(0)
#define SETG(r)		do {SET(r,GT); asm_output("setg %s",gpn(r)); } while(0)
#define SETGE(r)	do {SET(r,GE); asm_output("setge %s",gpn(r)); } while(0)
#define SETB(r)		do {SET(r,CCLO); asm_output("setb %s",gpn(r)); } while(0)
#define SETBE(r)	do {SET(r,LS); asm_output("setbe %s",gpn(r)); } while(0)
#define SETAE(r)	do {SET(r,CSHS); asm_output("setae %s",gpn(r)); } while(0) /* warning, untested */
#define SETA(r)		do {SET(r,HI); asm_output("seta %s",gpn(r)); } while(0) /* warning, untested */
#define SETC(r)		do {SET(r,CSHS); asm_output("setc %s",gpn(r)); } while(0) /* warning, untested */
#define SETO(r)		do {SET(r,VS); asm_output("seto %s",gpn(r)); } while(0) /* warning, untested */

// This zero-extends a reg that has been set using one of the SET macros,
// but is a NOOP on ARM/Thumb
#define MOVZX8(r,r2)

// If the offset is 0-255, no problem, just load 8-bit imm
// If the offset is greater than that, we load the SP
// 
// If offset is 8-bit, we
// 		MOV 	Scratch, SP
// 		MOV 	_r, offset
// 		LDRSH	_r, offset, Scratch
// 
// else
// 		ADD_sp	Scratch, offset/4
// 		MOV		_r, offset%4
// 		LDRSH	_r, Scratch, _r
#define LD16S(_r,_d,_b) do{														\
	if (_b==SP) {																\
		NanoAssert((int)(_d)>=0);												\
		if (isU8(_d)) {															\
			underrunProtect(6);													\
			*(--_nIns) = (NIns)(0x5E00 | ((_r)<<6) | (Scratch<<3) | (_r) );		\
			*(--_nIns) = (NIns)(0x2000 | ((_r)<<8) | (_d)&0xFF );				\
			*(--_nIns) = (NIns)(0x4600 | (SP<<3) | Scratch );}					\
		else {																\
			underrunProtect(6);													\
			*(--_nIns) = (NIns)(0x5E00 | ((_r)<<6) | (Scratch<<3) | (_r) );		\
			*(--_nIns) = (NIns)(0x2000 | ((_r)<<8) | ((_d)%4) );				\
			*(--_nIns) = (NIns)(0xA800 | (Scratch<<8) | (alignTo((_d), 4))/4);}	\
	} else {																	\
		if ((_d)<0) {														\
			if ((_d)<-255) {														\
				NanoAssert((_d)>=-510);										\
				underrunProtect(8);													\
				int rem = -(_d) - 255;												\
				*(--_nIns) = (NIns)(0x5E00 | (Scratch<<6) | ((_b)<<3) | (_r));			\
				*(--_nIns) = (NIns)(0x4240 | (Scratch<<3) | Scratch);						\
				*(--_nIns) = (NIns)(0x3000 | (Scratch<<8) | (rem&0xFF));				\
				*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | 0xFF );}					\
			else {																	\
				underrunProtect(6);													\
				*(--_nIns) = (NIns)(0x5E00 | ((Scratch)<<6) | ((_b)<<3) | (_r) );	\
				*(--_nIns) = (NIns)(0x4240 | (Scratch<<3) | Scratch);				\
				*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | ((-(_d))&0xFF) );}}		\
		else if ((int)(_d)<256) {												\
			underrunProtect(4);													\
			*(--_nIns) = (NIns)(0x5E00 | (Scratch<<6) | ((_b)<<3) | (_r));		\
			*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | ((_d)&0xFF) );}			\
		else {																	\
			NanoAssert((int)(_d)<=510);											\
			underrunProtect(6);													\
			int rem = (_d) - 255;												\
			*(--_nIns) = (NIns)(0x5E00 | (Scratch<<6) | ((_b)<<3) | (_r));			\
			*(--_nIns) = (NIns)(0x3000 | (Scratch<<8) | (rem&0xFF));				\
			*(--_nIns) = (NIns)(0x2000 | (Scratch<<8) | 0xFF );}					\
	} asm_output("movsx %s, %d(%s)", gpn(_r), (_d), gpn(_b)); } while(0)

	//*(--_nIns) = (NIns)(0x8800 | (((_d)>>1)<<6) | (Scratch<<3) | (_r));\
	//*(--_nIns) = (NIns)(0x4600 | (SP<<3) | Scratch );}				\


}
#endif // __nanojit_NativeThumb__
