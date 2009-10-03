/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
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

#ifdef PERFM
#define DOPROF
#include "../vprof/vprof.h"
#define count_instr() _nvprof("x86",1)
#define count_ret() _nvprof("x86-ret",1); count_instr();
#define count_push() _nvprof("x86-push",1); count_instr();
#define count_pop() _nvprof("x86-pop",1); count_instr();
#define count_st() _nvprof("x86-st",1); count_instr();
#define count_stq() _nvprof("x86-stq",1); count_instr();
#define count_ld() _nvprof("x86-ld",1); count_instr();
#define count_ldq() _nvprof("x86-ldq",1); count_instr();
#define count_call() _nvprof("x86-call",1); count_instr();
#define count_calli() _nvprof("x86-calli",1); count_instr();
#define count_prolog() _nvprof("x86-prolog",1); count_instr();
#define count_alu() _nvprof("x86-alu",1); count_instr();
#define count_mov() _nvprof("x86-mov",1); count_instr();
#define count_fpu() _nvprof("x86-fpu",1); count_instr();
#define count_jmp() _nvprof("x86-jmp",1); count_instr();
#define count_jcc() _nvprof("x86-jcc",1); count_instr();
#define count_fpuld() _nvprof("x86-ldq",1); _nvprof("x86-fpu",1); count_instr()
#define count_aluld() _nvprof("x86-ld",1); _nvprof("x86-alu",1); count_instr()
#define count_alust() _nvprof("x86-ld",1); _nvprof("x86-alu",1); _nvprof("x86-st",1); count_instr()
#define count_pushld() _nvprof("x86-ld",1); _nvprof("x86-push",1); count_instr()
#define count_imt() _nvprof("x86-imt",1) count_instr()
#else
#define count_instr()
#define count_ret()
#define count_push()
#define count_pop()
#define count_st()
#define count_stq()
#define count_ld()
#define count_ldq()
#define count_call()
#define count_calli()
#define count_prolog()
#define count_alu()
#define count_mov()
#define count_fpu()
#define count_jmp()
#define count_jcc()
#define count_fpuld()
#define count_aluld()
#define count_alust()
#define count_pushld()
#define count_imt()
#endif

namespace nanojit
{
    const int NJ_MAX_REGISTERS = 24; // gpregs, x87 regs, xmm regs

    #define NJ_MAX_STACK_ENTRY 256
    #define NJ_MAX_PARAMETERS 1

        // Preserve a 16-byte stack alignment, to support the use of
        // SSE instructions like MOVDQA (if not by Tamarin itself,
        // then by the C functions it calls).
    const int NJ_ALIGN_STACK = 16;

    const int32_t LARGEST_UNDERRUN_PROT = 3200;  // largest value passed to underrunProtect

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

        FirstReg = 0,
        LastReg = 16,
        UnknownReg = 17
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

    static inline bool isValidDisplacement(int32_t d) {
        return true;
    }

    #define _rmask_(r)      (1<<(r))
    #define _is_xmm_reg_(r) ((_rmask_(r)&XmmRegs)!=0)
    #define _is_x87_reg_(r) ((_rmask_(r)&x87Regs)!=0)
    #define _is_fp_reg_(r)  ((_rmask_(r)&FpRegs)!=0)
    #define _is_gp_reg_(r)  ((_rmask_(r)&GpRegs)!=0)

    #define nextreg(r)      Register(r+1)

    verbose_only( extern const char* regNames[]; )

    #define DECLARE_PLATFORM_STATS()

    #define DECLARE_PLATFORM_REGALLOC()

    #define DECLARE_PLATFORM_ASSEMBLER()    \
        const static Register argRegs[2], retRegs[2]; \
        bool x87Dirty;                      \
        bool pad[3];\
        void nativePageReset();\
        void nativePageSetup();\
        void underrunProtect(int);\
        void asm_farg(LInsp);\
        void asm_arg(ArgSize, LIns*, Register);\
        void asm_pusharg(LInsp);\
        void asm_fcmp(LIns *cond); \
        void asm_cmp(LIns *cond); \
        void asm_div_mod(LIns *cond);

    #define swapptrs()  { NIns* _tins = _nIns; _nIns=_nExitIns; _nExitIns=_tins; }

#define IMM32(i)    \
    _nIns -= 4;     \
    *((int32_t*)_nIns) = (int32_t)(i)

#define MODRMs(r,d,b,l,i) \
        NanoAssert(unsigned(i)<8 && unsigned(b)<8 && unsigned(r)<8); \
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

#define MODRMSIB(reg,base,index,scale,disp)                    \
        if (disp != 0 || base == EBP) {                        \
            if (isS8(disp)) {                                \
                *(--_nIns) = int8_t(disp);                    \
            } else {                                        \
                IMM32(disp);                                \
            }                                                \
        }                                                    \
        *(--_nIns) = uint8_t((scale)<<6|(index)<<3|(base));    \
        if (disp == 0 && base != EBP) {                        \
            *(--_nIns) = uint8_t(((reg)<<3)|4);                \
        } else {                                            \
            if (isS8(disp))                                    \
                *(--_nIns) = uint8_t((1<<6)|(reg<<3)|4);    \
            else                                            \
                *(--_nIns) = uint8_t((2<<6)|(reg<<3)|4);    \
        }

#define MODRMdm(r,addr)                    \
        NanoAssert(unsigned(r)<8);        \
        IMM32(addr);                    \
        *(--_nIns) = (uint8_t)( (r)<<3 | 5 );

#define MODRM(d,s) \
        NanoAssert(((unsigned)(d))<8 && ((unsigned)(s))<8); \
        *(--_nIns) = (uint8_t) ( 3<<6|(d)<<3|(s) )

#define ALU0(o)             \
        underrunProtect(1);\
        *(--_nIns) = (uint8_t) (o)

#define ALUm(c,r,d,b)       \
        underrunProtect(8); \
        MODRMm(r,d,b);      \
        *(--_nIns) = uint8_t(c)

#define ALUdm(c,r,addr)        \
        underrunProtect(6);    \
        MODRMdm(r,addr);    \
        *(--_nIns) = uint8_t(c)

#define ALUsib(c,r,base,index,scale,disp)    \
        underrunProtect(7);                    \
        MODRMSIB(r,base,index,scale,disp);    \
        *(--_nIns) = uint8_t(c)

#define ALUm16(c,r,d,b)     \
        underrunProtect(9); \
        MODRMm(r,d,b);      \
        *(--_nIns) = uint8_t(c);\
        *(--_nIns) = 0x66

#define ALU2dm(c,r,addr)    \
        underrunProtect(7);    \
        MODRMdm(r,addr);    \
        *(--_nIns) = (uint8_t) (c);\
        *(--_nIns) = (uint8_t) ((c)>>8)

#define ALU2m(c,r,d,b)      \
        underrunProtect(9); \
        MODRMm(r,d,b);      \
        *(--_nIns) = (uint8_t) (c);\
        *(--_nIns) = (uint8_t) ((c)>>8)

#define ALU2sib(c,r,base,index,scale,disp)    \
        underrunProtect(8);                    \
        MODRMSIB(r,base,index,scale,disp);    \
        *(--_nIns) = (uint8_t) (c);            \
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
        MODRM((d),(s)); \
        _nIns -= 2; \
        _nIns[0] = (uint8_t) ( ((c)>>8) ); \
        _nIns[1] = (uint8_t) ( (c) )

#define LAHF()      do { count_alu(); ALU0(0x9F);                   asm_output("lahf"); } while(0)
#define SAHF()      do { count_alu(); ALU0(0x9E);                   asm_output("sahf"); } while(0)
#define OR(l,r)     do { count_alu(); ALU(0x0b, (l),(r));           asm_output("or %s,%s",gpn(l),gpn(r)); } while(0)
#define AND(l,r)    do { count_alu(); ALU(0x23, (l),(r));           asm_output("and %s,%s",gpn(l),gpn(r)); } while(0)
#define XOR(l,r)    do { count_alu(); ALU(0x33, (l),(r));           asm_output("xor %s,%s",gpn(l),gpn(r)); } while(0)
#define ADD(l,r)    do { count_alu(); ALU(0x03, (l),(r));           asm_output("add %s,%s",gpn(l),gpn(r)); } while(0)
#define SUB(l,r)    do { count_alu(); ALU(0x2b, (l),(r));           asm_output("sub %s,%s",gpn(l),gpn(r)); } while(0)
#define MUL(l,r)    do { count_alu(); ALU2(0x0faf,(l),(r));         asm_output("mul %s,%s",gpn(l),gpn(r)); } while(0)
#define DIV(r)      do { count_alu(); ALU(0xf7, (Register)7,(r));   asm_output("idiv  edx:eax, %s",gpn(r)); } while(0)
#define NOT(r)      do { count_alu(); ALU(0xf7, (Register)2,(r));   asm_output("not %s",gpn(r)); } while(0)
#define NEG(r)      do { count_alu(); ALU(0xf7, (Register)3,(r));   asm_output("neg %s",gpn(r)); } while(0)
#define SHR(r,s)    do { count_alu(); ALU(0xd3, (Register)5,(r));   asm_output("shr %s,%s",gpn(r),gpn(s)); } while(0)
#define SAR(r,s)    do { count_alu(); ALU(0xd3, (Register)7,(r));   asm_output("sar %s,%s",gpn(r),gpn(s)); } while(0)
#define SHL(r,s)    do { count_alu(); ALU(0xd3, (Register)4,(r));   asm_output("shl %s,%s",gpn(r),gpn(s)); } while(0)

#define SHIFT(c,r,i) \
        underrunProtect(3);\
        *--_nIns = (uint8_t)(i);\
        MODRM((Register)c,r);\
        *--_nIns = 0xc1;

#define SHLi(r,i)   do { count_alu(); SHIFT(4,r,i); asm_output("shl %s,%d", gpn(r),i); } while(0)
#define SHRi(r,i)   do { count_alu(); SHIFT(5,r,i); asm_output("shr %s,%d", gpn(r),i); } while(0)
#define SARi(r,i)   do { count_alu(); SHIFT(7,r,i); asm_output("sar %s,%d", gpn(r),i); } while(0)

#define MOVZX8(d,s) do { count_alu(); ALU2(0x0fb6,d,s); asm_output("movzx %s,%s", gpn(d),gpn(s)); } while(0)

#define SUBi(r,i)   do { count_alu(); ALUi(0x2d,r,i);               asm_output("sub %s,%d",gpn(r),i); } while(0)
#define ADDi(r,i)   do { count_alu(); ALUi(0x05,r,i);               asm_output("add %s,%d",gpn(r),i); } while(0)
#define ANDi(r,i)   do { count_alu(); ALUi(0x25,r,i);               asm_output("and %s,%d",gpn(r),i); } while(0)
#define ORi(r,i)    do { count_alu(); ALUi(0x0d,r,i);               asm_output("or %s,%d",gpn(r),i); } while(0)
#define XORi(r,i)   do { count_alu(); ALUi(0x35,r,i);               asm_output("xor %s,%d",gpn(r),i); } while(0)

#define ADDmi(d,b,i) do { count_alust(); ALUmi(0x05, d, b, i); asm_output("add %d(%s), %d", d, gpn(b), i); } while(0)

#define TEST(d,s)   do { count_alu(); ALU(0x85,d,s);                asm_output("test %s,%s",gpn(d),gpn(s)); } while(0)
#define CMP(l,r)    do { count_alu(); ALU(0x3b, (l),(r));           asm_output("cmp %s,%s",gpn(l),gpn(r)); } while(0)
#define CMPi(r,i)   do { count_alu(); ALUi(0x3d,r,i);               asm_output("cmp %s,%d",gpn(r),i); } while(0)

#define MR(d,s)     do { count_mov(); ALU(0x8b,d,s);                asm_output("mov %s,%s",gpn(d),gpn(s)); } while(0)
#define LEA(r,d,b)  do { count_alu(); ALUm(0x8d, r,d,b);            asm_output("lea %s,%d(%s)",gpn(r),d,gpn(b)); } while(0)
// lea %r, d(%i*4)
// This addressing mode is not supported by the MODRMSIB macro.
#define LEAmi4(r,d,i) do { count_alu(); IMM32(d); *(--_nIns) = (2<<6)|((uint8_t)i<<3)|5; *(--_nIns) = (0<<6)|((uint8_t)r<<3)|4; *(--_nIns) = 0x8d;                    asm_output("lea %s, %p(%s*4)", gpn(r), (void*)d, gpn(i)); } while(0)

#define CDQ()       do { SARi(EDX, 31); MR(EDX, EAX); } while(0)

#define INCLi(p)    do { count_alu(); \
                         underrunProtect(6); \
                         IMM32((uint32_t)(ptrdiff_t)p); *(--_nIns) = 0x05; *(--_nIns) = 0xFF; \
                         asm_output("incl  (%p)", (void*)p); } while (0)

#define SETE(r)     do { count_alu(); ALU2(0x0f94,(r),(r));         asm_output("sete %s",gpn(r)); } while(0)
#define SETNP(r)    do { count_alu(); ALU2(0x0f9B,(r),(r));         asm_output("setnp %s",gpn(r)); } while(0)
#define SETL(r)     do { count_alu(); ALU2(0x0f9C,(r),(r));         asm_output("setl %s",gpn(r)); } while(0)
#define SETLE(r)    do { count_alu(); ALU2(0x0f9E,(r),(r));         asm_output("setle %s",gpn(r)); } while(0)
#define SETG(r)     do { count_alu(); ALU2(0x0f9F,(r),(r));         asm_output("setg %s",gpn(r)); } while(0)
#define SETGE(r)    do { count_alu(); ALU2(0x0f9D,(r),(r));         asm_output("setge %s",gpn(r)); } while(0)
#define SETB(r)     do { count_alu(); ALU2(0x0f92,(r),(r));         asm_output("setb %s",gpn(r)); } while(0)
#define SETBE(r)    do { count_alu(); ALU2(0x0f96,(r),(r));         asm_output("setbe %s",gpn(r)); } while(0)
#define SETA(r)     do { count_alu(); ALU2(0x0f97,(r),(r));         asm_output("seta %s",gpn(r)); } while(0)
#define SETAE(r)    do { count_alu(); ALU2(0x0f93,(r),(r));         asm_output("setae %s",gpn(r)); } while(0)
#define SETO(r)     do { count_alu(); ALU2(0x0f92,(r),(r));         asm_output("seto %s",gpn(r)); } while(0)

#define MREQ(dr,sr) do { count_alu(); ALU2(0x0f44,dr,sr); asm_output("cmove %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRNE(dr,sr) do { count_alu(); ALU2(0x0f45,dr,sr); asm_output("cmovne %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRL(dr,sr)  do { count_alu(); ALU2(0x0f4C,dr,sr); asm_output("cmovl %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRLE(dr,sr) do { count_alu(); ALU2(0x0f4E,dr,sr); asm_output("cmovle %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRG(dr,sr)  do { count_alu(); ALU2(0x0f4F,dr,sr); asm_output("cmovg %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRGE(dr,sr) do { count_alu(); ALU2(0x0f4D,dr,sr); asm_output("cmovge %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRB(dr,sr)  do { count_alu(); ALU2(0x0f42,dr,sr); asm_output("cmovb %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRBE(dr,sr) do { count_alu(); ALU2(0x0f46,dr,sr); asm_output("cmovbe %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRA(dr,sr)  do { count_alu(); ALU2(0x0f47,dr,sr); asm_output("cmova %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRAE(dr,sr) do { count_alu(); ALU2(0x0f43,dr,sr); asm_output("cmovae %s,%s", gpn(dr),gpn(sr)); } while(0)
#define MRNO(dr,sr) do { count_alu(); ALU2(0x0f41,dr,sr); asm_output("cmovno %s,%s", gpn(dr),gpn(sr)); } while(0)

// these aren't currently used but left in for reference
//#define LDEQ(r,d,b) do { ALU2m(0x0f44,r,d,b); asm_output("cmove %s,%d(%s)", gpn(r),d,gpn(b)); } while(0)
//#define LDNEQ(r,d,b) do { ALU2m(0x0f45,r,d,b); asm_output("cmovne %s,%d(%s)", gpn(r),d,gpn(b)); } while(0)

#define LD(reg,disp,base)   do {    \
    count_ld();\
    ALUm(0x8b,reg,disp,base);   \
    asm_output("mov %s,%d(%s)",gpn(reg),disp,gpn(base)); } while(0)

#define LDdm(reg,addr) do {        \
    count_ld();                 \
    ALUdm(0x8b,reg,addr);        \
    asm_output("mov   %s,0(%lx)",gpn(reg),(unsigned long)addr); \
    } while (0)


#define SIBIDX(n)    "1248"[n]

#define LDsib(reg,disp,base,index,scale) do {    \
    count_ld();                                 \
    ALUsib(0x8b,reg,base,index,scale,disp);        \
    asm_output("mov   %s,%d(%s+%s*%c)",gpn(reg),disp,gpn(base),gpn(index),SIBIDX(scale)); \
    } while (0)

// load 16-bit, sign extend
#define LD16S(r,d,b) do { count_ld(); ALU2m(0x0fbf,r,d,b); asm_output("movsx %s,%d(%s)", gpn(r),d,gpn(b)); } while(0)

// load 16-bit, zero extend
#define LD16Z(r,d,b) do { count_ld(); ALU2m(0x0fb7,r,d,b); asm_output("movsz %s,%d(%s)", gpn(r),d,gpn(b)); } while(0)

#define LD16Zdm(r,addr) do { count_ld(); ALU2dm(0x0fb7,r,addr); asm_output("movsz %s,0(%lx)", gpn(r),(unsigned long)addr); } while (0)

#define LD16Zsib(r,disp,base,index,scale) do {    \
    count_ld();                                 \
    ALU2sib(0x0fb7,r,base,index,scale,disp);    \
    asm_output("movsz %s,%d(%s+%s*%c)",gpn(r),disp,gpn(base),gpn(index),SIBIDX(scale)); \
    } while (0)

// load 8-bit, zero extend
// note, only 5-bit offsets (!) are supported for this, but that's all we need at the moment
// (movzx actually allows larger offsets mode but 5-bit gives us advantage in Thumb mode)
#define LD8Z(r,d,b)    do { count_ld(); NanoAssert((d)>=0&&(d)<=31); ALU2m(0x0fb6,r,d,b); asm_output("movzx %s,%d(%s)", gpn(r),d,gpn(b)); } while(0)

#define LD8Zdm(r,addr) do { \
    count_ld(); \
    NanoAssert((d)>=0&&(d)<=31); \
    ALU2dm(0x0fb6,r,addr); \
    asm_output("movzx %s,0(%lx)", gpn(r),(long unsigned)addr); \
    } while(0)

#define LD8Zsib(r,disp,base,index,scale) do {    \
    count_ld();                                 \
    NanoAssert((d)>=0&&(d)<=31);                \
    ALU2sib(0x0fb6,r,base,index,scale,disp);    \
    asm_output("movzx %s,%d(%s+%s*%c)",gpn(r),disp,gpn(base),gpn(index),SIBIDX(scale)); \
    } while(0)


#define LDi(r,i) do { \
    count_ld();\
    underrunProtect(5);         \
    IMM32(i);                   \
    NanoAssert(((unsigned)r)<8); \
    *(--_nIns) = (uint8_t) (0xb8 | (r) );       \
    asm_output("mov %s,%d",gpn(r),i); } while(0)

#define ST(base,disp,reg) do {  \
    count_st();\
    ALUm(0x89,reg,disp,base);   \
    asm_output("mov %d(%s),%s",disp,base==UnknownReg?"0":gpn(base),gpn(reg)); } while(0)

#define STi(base,disp,imm)  do { \
    count_st();\
    underrunProtect(12);    \
    IMM32(imm);             \
    MODRMm(0, disp, base);  \
    *(--_nIns) = 0xc7;      \
    asm_output("mov %d(%s),%d",disp,gpn(base),imm); } while(0)

#define RET()   do { count_ret(); ALU0(0xc3); asm_output("ret"); } while(0)
#define NOP()   do { count_alu(); ALU0(0x90); asm_output("nop"); } while(0)
#define INT3()  do { ALU0(0xcc); asm_output("int3"); } while(0)

#define PUSHi(i) do { \
    count_push();\
    if (isS8(i)) { \
        underrunProtect(2);         \
        _nIns-=2; _nIns[0] = 0x6a; _nIns[1] = (uint8_t)(i); \
        asm_output("push %d",i); \
    } else \
        { PUSHi32(i); } } while(0)

#define PUSHi32(i)  do {    \
    count_push();\
    underrunProtect(5); \
    IMM32(i);           \
    *(--_nIns) = 0x68;  \
    asm_output("push %d",i); } while(0)

#define PUSHr(r) do {  \
    count_push();\
    underrunProtect(1);         \
    NanoAssert(((unsigned)r)<8); \
    *(--_nIns) = (uint8_t) ( 0x50 | (r) );  \
    asm_output("push %s",gpn(r)); } while(0)

#define PUSHm(d,b) do { \
    count_pushld();\
    ALUm(0xff, 6, d, b);        \
    asm_output("push %d(%s)",d,gpn(b)); } while(0)

#define POPr(r) do { \
    count_pop();\
    underrunProtect(1);         \
    NanoAssert(((unsigned)r)<8); \
    *(--_nIns) = (uint8_t) ( 0x58 | (r) ); \
    asm_output("pop %s",gpn(r)); } while(0)

#define JCC32 0x0f
#define JMP8  0xeb
#define JMP32 0xe9

#define JCC(o,t,n) do { \
    count_jcc();\
    underrunProtect(6); \
    intptr_t tt = (intptr_t)t - (intptr_t)_nIns;    \
    if (isS8(tt)) { \
        verbose_only( NIns* next = _nIns; (void)next; ) \
        _nIns -= 2; \
        _nIns[0] = (uint8_t) ( 0x70 | (o) ); \
        _nIns[1] = (uint8_t) (tt); \
        asm_output("%-5s %p",(n),(next+tt)); \
    } else { \
        verbose_only( NIns* next = _nIns; ) \
        IMM32(tt); \
        _nIns -= 2; \
        _nIns[0] = JCC32; \
        _nIns[1] = (uint8_t) ( 0x80 | (o) ); \
        asm_output("%-5s %p",(n),(next+tt)); \
    } } while(0)

#define JMP_long(t) do { \
    count_jmp();\
    underrunProtect(5); \
    intptr_t tt = (intptr_t)t - (intptr_t)_nIns;    \
    JMP_long_nochk_offset(tt);  \
    verbose_only( verbose_outputf("%010lx:", (unsigned long)_nIns); )    \
    } while(0)

#define JMP(t)      do {    \
    count_jmp();\
    underrunProtect(5); \
    intptr_t tt = (intptr_t)t - (intptr_t)_nIns;    \
    if (isS8(tt)) { \
        verbose_only( NIns* next = _nIns; (void)next; ) \
        _nIns -= 2; \
        _nIns[0] = JMP8; \
        _nIns[1] = (uint8_t) ( (tt)&0xff ); \
        asm_output("jmp %p",(next+tt)); \
    } else { \
        JMP_long_nochk_offset(tt);  \
    } } while(0)

// this should only be used when you can guarantee there is enough room on the page
#define JMP_long_nochk_offset(o) do {\
        verbose_only( NIns* next = _nIns; (void)next; ) \
        IMM32((o)); \
        *(--_nIns) = JMP32; \
        asm_output("jmp %p",(next+(o))); } while(0)

#define JMP_indirect(r) do { \
        underrunProtect(2);  \
        MODRMm(4, 0, r);     \
        *(--_nIns) = 0xff;   \
        asm_output("jmp   *(%s)", gpn(r)); } while (0)

#define JE(t)   JCC(0x04, t, "je")
#define JNE(t)  JCC(0x05, t, "jne")
#define JP(t)   JCC(0x0A, t, "jp")
#define JNP(t)  JCC(0x0B, t, "jnp")

#define JB(t)   JCC(0x02, t, "jb")
#define JNB(t)  JCC(0x03, t, "jnb")
#define JBE(t)  JCC(0x06, t, "jbe")
#define JNBE(t) JCC(0x07, t, "jnbe")

#define JA(t)   JCC(0x07, t, "ja")
#define JNA(t)  JCC(0x06, t, "jna")
#define JAE(t)  JCC(0x03, t, "jae")
#define JNAE(t) JCC(0x02, t, "jnae")

#define JL(t)   JCC(0x0C, t, "jl")
#define JNL(t)  JCC(0x0D, t, "jnl")
#define JLE(t)  JCC(0x0E, t, "jle")
#define JNLE(t) JCC(0x0F, t, "jnle")

#define JG(t)   JCC(0x0F, t, "jg")
#define JNG(t)  JCC(0x0E, t, "jng")
#define JGE(t)  JCC(0x0D, t, "jge")
#define JNGE(t) JCC(0x0C, t, "jnge")

#define JO(t)   JCC(0x00, t, "jo")
#define JNO(t)  JCC(0x01, t, "jno")

// sse instructions
#define SSE(c,d,s)  \
        underrunProtect(9); \
        MODRM((d),(s)); \
        _nIns -= 3; \
        _nIns[0] = (uint8_t)(((c)>>16)&0xff); \
        _nIns[1] = (uint8_t)(((c)>>8)&0xff); \
        _nIns[2] = (uint8_t)((c)&0xff)

#define SSEm(c,r,d,b)   \
        underrunProtect(9); \
        MODRMm((r),(d),(b));    \
        _nIns -= 3;     \
        _nIns[0] = (uint8_t)(((c)>>16)&0xff); \
        _nIns[1] = (uint8_t)(((c)>>8)&0xff); \
        _nIns[2] = (uint8_t)((c)&0xff)

#define LDSD(r,d,b)do {     \
    count_ldq();\
    SSEm(0xf20f10, (r)&7, (d), (b)); \
    asm_output("movsd %s,%d(%s)",gpn(r),(d),gpn(b)); \
    } while(0)

#define LDSDm(r,addr)do {     \
    count_ldq();\
    underrunProtect(8); \
    const double* daddr = addr; \
    IMM32(int32_t(daddr));\
    *(--_nIns) = uint8_t(((r)&7)<<3|5); \
    *(--_nIns) = 0x10;\
    *(--_nIns) = 0x0f;\
    *(--_nIns) = 0xf2;\
    asm_output("movsd %s,(#%p) // =%f",gpn(r),(void*)daddr,*daddr); \
    } while(0)

#define STSD(d,b,r)do {     \
    count_stq();\
    SSEm(0xf20f11, (r)&7, (d), (b)); \
    asm_output("movsd %d(%s),%s",(d),gpn(b),gpn(r)); \
    } while(0)

#define SSE_LDQ(r,d,b)do {  \
    count_ldq();\
    SSEm(0xf30f7e, (r)&7, (d), (b)); \
    asm_output("movq %s,%d(%s)",gpn(r),d,gpn(b)); \
    } while(0)

#define SSE_STQ(d,b,r)do {  \
    count_stq();\
    SSEm(0x660fd6, (r)&7, (d), (b)); \
    asm_output("movq %d(%s),%s",(d),gpn(b),gpn(r)); \
    } while(0)

#define SSE_CVTSI2SD(xr,gr) do{ \
    count_fpu();\
    SSE(0xf20f2a, (xr)&7, (gr)&7); \
    asm_output("cvtsi2sd %s,%s",gpn(xr),gpn(gr)); \
    } while(0)

#define CVTDQ2PD(dstr,srcr) do{ \
    count_fpu();\
    SSE(0xf30fe6, (dstr)&7, (srcr)&7); \
    asm_output("cvtdq2pd %s,%s",gpn(dstr),gpn(srcr)); \
    } while(0)

// move and zero-extend gpreg to xmm reg
#define SSE_MOVD(d,s) do{ \
    count_mov();\
    if (_is_xmm_reg_(s)) { \
        NanoAssert(_is_gp_reg_(d)); \
        SSE(0x660f7e, (s)&7, (d)&7); \
    } else { \
        NanoAssert(_is_gp_reg_(s)); \
        NanoAssert(_is_xmm_reg_(d)); \
        SSE(0x660f6e, (d)&7, (s)&7); \
    } \
    asm_output("movd %s,%s",gpn(d),gpn(s)); \
    } while(0)

#define SSE_MOVSD(rd,rs) do{ \
    count_mov();\
    NanoAssert(_is_xmm_reg_(rd) && _is_xmm_reg_(rs));\
    SSE(0xf20f10, (rd)&7, (rs)&7); \
    asm_output("movsd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)

#define SSE_MOVDm(d,b,xrs) do {\
    count_st();\
    NanoAssert(_is_xmm_reg_(xrs) && _is_gp_reg_(b));\
    SSEm(0x660f7e, (xrs)&7, d, b);\
    asm_output("movd %d(%s),%s", d, gpn(b), gpn(xrs));\
    } while(0)

#define SSE_ADDSD(rd,rs) do{ \
    count_fpu();\
    NanoAssert(_is_xmm_reg_(rd) && _is_xmm_reg_(rs));\
    SSE(0xf20f58, (rd)&7, (rs)&7); \
    asm_output("addsd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)

#define SSE_ADDSDm(r,addr)do {     \
    count_fpuld();\
    underrunProtect(8); \
    NanoAssert(_is_xmm_reg_(r));\
    const double* daddr = addr; \
    IMM32(int32_t(daddr));\
    *(--_nIns) = uint8_t(((r)&7)<<3|5); \
    *(--_nIns) = 0x58;\
    *(--_nIns) = 0x0f;\
    *(--_nIns) = 0xf2;\
    asm_output("addsd %s,%p // =%f",gpn(r),(void*)daddr,*daddr); \
    } while(0)

#define SSE_SUBSD(rd,rs) do{ \
    count_fpu();\
    NanoAssert(_is_xmm_reg_(rd) && _is_xmm_reg_(rs));\
    SSE(0xf20f5c, (rd)&7, (rs)&7); \
    asm_output("subsd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)
#define SSE_MULSD(rd,rs) do{ \
    count_fpu();\
    NanoAssert(_is_xmm_reg_(rd) && _is_xmm_reg_(rs));\
    SSE(0xf20f59, (rd)&7, (rs)&7); \
    asm_output("mulsd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)
#define SSE_DIVSD(rd,rs) do{ \
    count_fpu();\
    NanoAssert(_is_xmm_reg_(rd) && _is_xmm_reg_(rs));\
    SSE(0xf20f5e, (rd)&7, (rs)&7); \
    asm_output("divsd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)
#define SSE_UCOMISD(rl,rr) do{ \
    count_fpu();\
    NanoAssert(_is_xmm_reg_(rl) && _is_xmm_reg_(rr));\
    SSE(0x660f2e, (rl)&7, (rr)&7); \
    asm_output("ucomisd %s,%s",gpn(rl),gpn(rr)); \
    } while(0)

#define CVTSI2SDm(xr,d,b) do{ \
    count_fpu();\
    NanoAssert(_is_xmm_reg_(xr) && _is_gp_reg_(b));\
    SSEm(0xf20f2a, (xr)&7, (d), (b)); \
    asm_output("cvtsi2sd %s,%d(%s)",gpn(xr),(d),gpn(b)); \
    } while(0)

#define SSE_XORPD(r, maskaddr) do {\
    count_fpuld();\
    underrunProtect(8); \
    IMM32(maskaddr);\
    *(--_nIns) = uint8_t(((r)&7)<<3|5); \
    *(--_nIns) = 0x57;\
    *(--_nIns) = 0x0f;\
    *(--_nIns) = 0x66;\
    asm_output("xorpd %s,[0x%p]",gpn(r),(void*)(maskaddr));\
    } while(0)

#define SSE_XORPDr(rd,rs) do{ \
    count_fpu();\
    SSE(0x660f57, (rd)&7, (rs)&7); \
    asm_output("xorpd %s,%s",gpn(rd),gpn(rs)); \
    } while(0)

// floating point unit
#define FPUc(o)                             \
        underrunProtect(2);                 \
        *(--_nIns) = ((uint8_t)(o)&0xff);       \
        *(--_nIns) = (uint8_t)(((o)>>8)&0xff)

#define FPU(o,r)                            \
        underrunProtect(2);                 \
        *(--_nIns) = uint8_t(((uint8_t)(o)&0xff) | (r&7));\
        *(--_nIns) = (uint8_t)(((o)>>8)&0xff)

#define FPUm(o,d,b)                         \
        underrunProtect(7);                 \
        MODRMm((uint8_t)(o), d, b);         \
        *(--_nIns) = (uint8_t)((o)>>8)

#define TEST_AH(i) do {                             \
        count_alu();\
        underrunProtect(3);                 \
        *(--_nIns) = ((uint8_t)(i));            \
        *(--_nIns) = 0xc4;                  \
        *(--_nIns) = 0xf6;                  \
        asm_output("test ah, %d",i); } while(0)

#define TEST_AX(i) do {                             \
        count_fpu();\
        underrunProtect(5);                 \
        *(--_nIns) = (0);       \
        *(--_nIns) = ((uint8_t)(i));            \
        *(--_nIns) = ((uint8_t)((i)>>8));       \
        *(--_nIns) = (0);       \
        *(--_nIns) = 0xa9;                  \
        asm_output("test ax, %d",i); } while(0)

#define FNSTSW_AX() do { count_fpu(); FPUc(0xdfe0);             asm_output("fnstsw_ax"); } while(0)
#define FCHS()      do { count_fpu(); FPUc(0xd9e0);             asm_output("fchs"); } while(0)
#define FLD1()      do { count_fpu(); FPUc(0xd9e8);             asm_output("fld1"); fpu_push(); } while(0)
#define FLDZ()      do { count_fpu(); FPUc(0xd9ee);             asm_output("fldz"); fpu_push(); } while(0)
#define FFREE(r)    do { count_fpu(); FPU(0xddc0, r);           asm_output("ffree %s",fpn(r)); } while(0)
#define FSTQ(p,d,b) do { count_stq(); FPUm(0xdd02|(p), d, b);   asm_output("fst%sq %d(%s)",((p)?"p":""),d,gpn(b)); if (p) fpu_pop(); } while(0)
#define FSTPQ(d,b)  FSTQ(1,d,b)
#define FCOM(p,d,b) do { count_fpuld(); FPUm(0xdc02|(p), d, b); asm_output("fcom%s %d(%s)",((p)?"p":""),d,gpn(b)); if (p) fpu_pop(); } while(0)
#define FLDQ(d,b)   do { count_ldq(); FPUm(0xdd00, d, b);       asm_output("fldq %d(%s)",d,gpn(b)); fpu_push();} while(0)
#define FILDQ(d,b)  do { count_fpuld(); FPUm(0xdf05, d, b);     asm_output("fildq %d(%s)",d,gpn(b)); fpu_push(); } while(0)
#define FILD(d,b)   do { count_fpuld(); FPUm(0xdb00, d, b);     asm_output("fild %d(%s)",d,gpn(b)); fpu_push(); } while(0)
#define FADD(d,b)   do { count_fpu(); FPUm(0xdc00, d, b);       asm_output("fadd %d(%s)",d,gpn(b)); } while(0)
#define FSUB(d,b)   do { count_fpu(); FPUm(0xdc04, d, b);       asm_output("fsub %d(%s)",d,gpn(b)); } while(0)
#define FSUBR(d,b)  do { count_fpu(); FPUm(0xdc05, d, b);       asm_output("fsubr %d(%s)",d,gpn(b)); } while(0)
#define FMUL(d,b)   do { count_fpu(); FPUm(0xdc01, d, b);       asm_output("fmul %d(%s)",d,gpn(b)); } while(0)
#define FDIV(d,b)   do { count_fpu(); FPUm(0xdc06, d, b);       asm_output("fdiv %d(%s)",d,gpn(b)); } while(0)
#define FDIVR(d,b)  do { count_fpu(); FPUm(0xdc07, d, b);       asm_output("fdivr %d(%s)",d,gpn(b)); } while(0)
#define FINCSTP()   do { count_fpu(); FPUc(0xd9f7);             asm_output("fincstp"); } while(0)
#define FSTP(r)     do { count_fpu(); FPU(0xddd8, r&7);         asm_output("fstp %s",fpn(r)); fpu_pop();} while(0)
#define FCOMP()     do { count_fpu(); FPUc(0xD8D9);             asm_output("fcomp"); fpu_pop();} while(0)
#define FCOMPP()    do { count_fpu(); FPUc(0xDED9);             asm_output("fcompp"); fpu_pop();fpu_pop();} while(0)
#define FLDr(r)     do { count_ldq(); FPU(0xd9c0,r);            asm_output("fld %s",fpn(r)); fpu_push(); } while(0)
#define EMMS()      do { count_fpu(); FPUc(0x0f77);             asm_output("emms"); } while (0)

// standard direct call
#define CALL(c) do { \
  count_call();\
  underrunProtect(5);                   \
  int offset = (c->_address) - ((int)_nIns); \
  IMM32( (uint32_t)offset );    \
  *(--_nIns) = 0xE8;        \
  verbose_only(asm_output("call %s",(c->_name));) \
  debug_only(if ((c->_argtypes & ARGSIZE_MASK_ANY)==ARGSIZE_F) fpu_push();)\
} while (0)

// indirect call thru register
#define CALLr(c,r)  do { \
  count_calli();\
  underrunProtect(2);\
  ALU(0xff, 2, (r));\
  verbose_only(asm_output("call %s",gpn(r));) \
  debug_only(if ((c->_argtypes & ARGSIZE_MASK_ANY)==ARGSIZE_F) fpu_push();)\
} while (0)


}
#endif // __nanojit_Nativei386__
