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

#include "NativeCommon.h"

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

    #define NJ_MAX_STACK_ENTRY           4096
    #define NJ_MAX_PARAMETERS               1

    #define NJ_USES_IMMD_POOL          1

    #define NJ_JTBL_SUPPORTED               1
    #define NJ_EXPANDED_LOADSTORE_SUPPORTED 1
    #define NJ_F2I_SUPPORTED                1
    #define NJ_SOFTFLOAT_SUPPORTED          0
    #define NJ_DIVI_SUPPORTED               1
    
        // Preserve a 16-byte stack alignment, to support the use of
        // SSE instructions like MOVDQA (if not by Tamarin itself,
        // then by the C functions it calls).
    const int NJ_ALIGN_STACK = 16;

    const int32_t LARGEST_UNDERRUN_PROT = 32;  // largest value passed to underrunProtect

    typedef uint8_t NIns;

    // Bytes of icache to flush after patch
    const size_t LARGEST_BRANCH_PATCH = 16 * sizeof(NIns);

    static const Register
        // General purpose 32 bit registers.  The names are rEAX, rEBX, etc,
        // because EAX, EBX, et al clash with <sys/regset.h> on Solaris (sigh).
        // See bug 570726 for details.
        rEAX = { 0 }, // return value, scratch
        rECX = { 1 }, // this/arg0, scratch
        rEDX = { 2 }, // arg1, return-msw, scratch
        rEBX = { 3 },
        rESP = { 4 }, // stack pointer
        rEBP = { 5 }, // frame pointer
        rESI = { 6 },
        rEDI = { 7 },

        SP = rESP,    // alias SP to ESP for convenience
        FP = rEBP,    // alias FP to EBP for convenience

        // SSE regs come before X87 so we prefer them
        XMM0 = { 8 },
        XMM1 = { 9 },
        XMM2 = { 10 },
        XMM3 = { 11 },
        XMM4 = { 12 },
        XMM5 = { 13 },
        XMM6 = { 14 },
        XMM7 = { 15 },

        // X87 regs
        FST0 = { 16 },

        deprecated_UnknownReg = { 17 }, // XXX: remove eventually, see bug 538924
        UnspecifiedReg = { 17 };

    static const uint32_t FirstRegNum = 0;
    static const uint32_t LastRegNum = 16;

    typedef int RegisterMask;

    static const int NumSavedRegs = 3;
    static const RegisterMask SavedRegs   = 1<<REGNUM(rEBX) | 1<<REGNUM(rEDI) | 1<<REGNUM(rESI);
    static const RegisterMask GpRegs      = SavedRegs | 1<<REGNUM(rEAX) | 1<<REGNUM(rECX) |
                                                        1<<REGNUM(rEDX);
    static const RegisterMask XmmRegs     = 1<<REGNUM(XMM0) | 1<<REGNUM(XMM1) | 1<<REGNUM(XMM2) |
                                            1<<REGNUM(XMM3) | 1<<REGNUM(XMM4) | 1<<REGNUM(XMM5) |
                                            1<<REGNUM(XMM6) | 1<<REGNUM(XMM7);
    static const RegisterMask x87Regs     = 1<<REGNUM(FST0);
    static const RegisterMask FpRegs      = x87Regs | XmmRegs;
    static const RegisterMask ScratchRegs = 1<<REGNUM(rEAX) | 1<<REGNUM(rECX) | 1<<REGNUM(rEDX) |
                                            FpRegs;

    static const RegisterMask AllowableByteRegs = 1<<REGNUM(rEAX) | 1<<REGNUM(rECX) |
                                                  1<<REGNUM(rEDX) | 1<<REGNUM(rEBX);

    static inline bool IsGpReg(Register r) {
        return ((1<<REGNUM(r)) & GpRegs) != 0;
    }
    static inline bool IsXmmReg(Register r) {
        return ((1<<REGNUM(r)) & XmmRegs) != 0;
    }

    verbose_only( extern const char* regNames[]; )

    #define DECLARE_PLATFORM_STATS()

    #define DECLARE_PLATFORM_REGALLOC()

    #define JCC32 0x0f
    #define JMP8  0xeb
    #define JMP32 0xe9

    #define DECLARE_PLATFORM_ASSEMBLER()    \
        const static Register argRegs[2], retRegs[2]; \
        int32_t max_stk_args;\
        void nativePageReset();\
        void nativePageSetup();\
        void underrunProtect(int);\
        bool hardenNopInsertion(const Config& c) { return c.harden_nop_insertion; } \
        void asm_immi(Register r, int32_t val, bool canClobberCCs);\
        void asm_stkarg(LIns* p, int32_t& stkd);\
        void asm_farg(LIns*, int32_t& stkd);\
        void asm_arg(ArgType ty, LIns* p, Register r, int32_t& stkd);\
        void asm_pusharg(LIns*);\
        void asm_cmpd(LIns *cond);\
        NIns* asm_branchd(bool, LIns*, NIns*);\
        void asm_cmp(LIns *cond); \
        void asm_div_mod(LIns *cond); \
        void asm_load(int d, Register r); \
        void asm_immd(Register r, uint64_t q, double d, bool canClobberCCs); \
        \
        /* These function generate fragments of instructions. */ \
        void IMM8(int32_t i) { /* Length: 1 byte. */ \
            _nIns -= 1; \
            *((int8_t*)_nIns) = int8_t(i); \
        }; \
        void IMM16(int32_t i) { /* Length: 2 bytes. */ \
            _nIns -= 2; \
            *((int16_t*)_nIns) = int16_t(i); \
        }; \
        void IMM32(int32_t i) { /* Length: 4 bytes. */ \
            _nIns -= 4; \
            *((int32_t*)_nIns) = int32_t(i); \
        }; \
        void MODRM(int32_t mod, int32_t ro, int32_t rm) { /* Length: 1 byte. */ \
            NanoAssert(unsigned(mod) < 4 && unsigned(ro) < 8 && unsigned(rm) < 8); \
            *(--_nIns) = uint8_t(mod << 6 | ro << 3 | rm); \
        } \
        void SIB(int32_t s, int32_t i, int32_t b) { /* Length: 1 byte. */ \
            NanoAssert(unsigned(s) < 4 && unsigned(i) < 8 && unsigned(b) < 8); \
            *(--_nIns) = uint8_t(s << 6 | i << 3 | b); \
        } \
        void MODRMr(int32_t d, int32_t s) { /* Length: 1 byte. */ \
            NanoAssert(unsigned(d) < 8 && unsigned(s) < 8); \
            MODRM(3, d, s); \
        }; \
        void MODRMm(int32_t r, int32_t d, Register b); \
        void MODRMsib(int32_t r, Register b, Register i, int32_t s, int32_t d); \
        void MODRMdm(int32_t r, int32_t addr); \
        \
        /* These functions generate entire instructions. */ \
        void ALU0(int32_t o); \
        void ALUm(int32_t c, int32_t r, int32_t d, Register b); \
        void ALUdm(int32_t c, Register r, int32_t addr); \
        void ALUsib(int32_t c, Register r, Register base, Register index, int32_t scale, int32_t disp); \
        void ALUsib16(int32_t c, Register r, Register base, Register index, int32_t scale, int32_t disp); \
        void ALUm16(int32_t c, int32_t r, int32_t d, Register b); \
        void ALU2dm(int32_t c, Register r, int32_t addr); \
        void ALU2m(int32_t c, Register r, int32_t d, Register b); \
        void ALU2sib(int32_t c, Register r, Register base, Register index, int32_t scale, int32_t disp); \
        void ALU(int32_t c, int32_t d, Register s) { \
            underrunProtect(2); \
            MODRMr(d, REGNUM(s)); \
            *(--_nIns) = uint8_t(c); \
        }; \
        void ALUi(int32_t c, Register r, int32_t i); \
        void ALUmi(int32_t c, int32_t d, Register b, int32_t i); \
        void ALU2(int32_t c, Register d, Register s); \
        Register AL2AHReg(Register r); \
        void OR(Register l, Register r); \
        void AND(Register l, Register r); \
        void AND8R(Register r); \
        void XOR(Register l, Register r); \
        void ADD(Register l, Register r); \
        void SUB(Register l, Register r); \
        void IMUL(Register l, Register r); \
        void DIV(Register r); \
        void NOT(Register r); \
        void NEG(Register r); \
        void SHR(Register r, Register s); \
        void SAR(Register r, Register s); \
        void SHL(Register r, Register s); \
        void SHIFT(int32_t c, Register r, int32_t i); \
        void SHLi(Register r, int32_t i); \
        void SHRi(Register r, int32_t i); \
        void SARi(Register r, int32_t i); \
        void MOVZX8(Register d, Register s); \
        void SUBi(Register r, int32_t i); \
        void ADDi(Register r, int32_t i); \
        void ANDi(Register r, int32_t i); \
        void ORi(Register r, int32_t i); \
        void XORi(Register r, int32_t i); \
        void ADDmi(int32_t d, Register b, int32_t i); \
        void TEST(Register d, Register s); \
        void CMP(Register l, Register r); \
        void CMPi(Register r, int32_t i); \
        void MR(Register d, Register s) { \
            count_mov(); \
            ALU(0x8b, REGNUM(d), s); \
            asm_output("mov %s,%s", gpn(d), gpn(s)); \
        }; \
        void LEA(Register r, int32_t d, Register b); \
        void LEAmi4(Register r, int32_t d, Register i); \
        void CDQ(); \
        void INCLi(int32_t p); \
        void SETE( Register r); \
        void SETNP(Register r); \
        void SETNPH(Register r); \
        void SETL( Register r); \
        void SETLE(Register r); \
        void SETG( Register r); \
        void SETGE(Register r); \
        void SETB( Register r); \
        void SETBE(Register r); \
        void SETA( Register r); \
        void SETAE(Register r); \
        void SETO( Register r); \
        void MREQ(Register d, Register s); \
        void MRNE(Register d, Register s); \
        void MRL( Register d, Register s); \
        void MRLE(Register d, Register s); \
        void MRG( Register d, Register s); \
        void MRGE(Register d, Register s); \
        void MRB( Register d, Register s); \
        void MRBE(Register d, Register s); \
        void MRA( Register d, Register s); \
        void MRAE(Register d, Register s); \
        void MRNO(Register d, Register s); \
        void LD(Register reg, int32_t disp, Register base); \
        void LDdm(Register reg, int32_t addr); \
        void LDsib(Register reg, int32_t disp, Register base, Register index, int32_t scale); \
        void LD16S(Register r, int32_t d, Register b); \
        void LD16Sdm(Register r, int32_t addr); \
        void LD16Ssib(Register r, int32_t disp, Register base, Register index, int32_t scale); \
        void LD16Z(Register r, int32_t d, Register b); \
        void LD16Zdm(Register r, int32_t addr); \
        void LD16Zsib(Register r, int32_t disp, Register base, Register index, int32_t scale); \
        void LD8Z(Register r, int32_t d, Register b); \
        void LD8Zdm(Register r, int32_t addr); \
        void LD8Zsib(Register r, int32_t disp, Register base, Register index, int32_t scale); \
        void LD8S(Register r, int32_t d, Register b); \
        void LD8Sdm(Register r, int32_t addr); \
        void LD8Ssib(Register r, int32_t disp, Register base, Register index, int32_t scale); \
        void LDi(Register r, int32_t i); \
        void ST8(Register base, int32_t disp, Register reg); \
        void ST8sib(int32_t disp, Register base, Register index, int32_t scale, Register reg); \
        void ST16(Register base, int32_t disp, Register reg); \
        void ST16sib(int32_t disp, Register base, Register index, int32_t scale, Register reg); \
        void ST(Register base, int32_t disp, Register reg); \
        void STsib(int32_t disp, Register base, Register index, int32_t scale, Register reg); \
        void ST8i(Register base, int32_t disp, int32_t imm); \
        void ST8isib(int32_t disp, Register base, Register index, int32_t scale, int32_t imm); \
        void ST16i(Register base, int32_t disp, int32_t imm); \
        void ST16isib(int32_t disp, Register base, Register index, int32_t scale, int32_t imm); \
        void STi(Register base, int32_t disp, int32_t imm); \
        void STisib(int32_t disp, Register base, Register index, int32_t scale, int32_t imm); \
        void RET(); \
        void NOP(); \
        void INT3(); \
        void PUSHi(int32_t i); \
        void PUSHr(Register r); \
        void PUSHm(int32_t d, Register b); \
        void POPr(Register r); \
        void JCC(int32_t o, NIns* t, const char* n); \
        void JMP_long(NIns* t); \
        void JMP(NIns* t) { \
            count_jmp(); \
            underrunProtect(5); \
            intptr_t tt = t ? (intptr_t)t - (intptr_t)_nIns : 0; \
            if (t && isS8(tt)) { \
                *(--_nIns) = uint8_t(tt & 0xff); \
                *(--_nIns) = JMP8; \
            } else { \
                IMM32(tt); \
                *(--_nIns) = JMP32; \
            } \
            asm_output("jmp %p", t); \
        }; \
        void JMP_indirect(Register r); \
        void JMP_indexed(Register x, int32_t ss, NIns** addr); \
        void JE(NIns* t); \
        void JNE(NIns* t); \
        void JP(NIns* t); \
        void JNP(NIns* t); \
        void JB(NIns* t); \
        void JNB(NIns* t); \
        void JBE(NIns* t); \
        void JNBE(NIns* t); \
        void JA(NIns* t); \
        void JNA(NIns* t); \
        void JAE(NIns* t); \
        void JNAE(NIns* t); \
        void JL(NIns* t); \
        void JNL(NIns* t); \
        void JLE(NIns* t); \
        void JNLE(NIns* t); \
        void JG(NIns* t); \
        void JNG(NIns* t); \
        void JGE(NIns* t); \
        void JNGE(NIns* t); \
        void JO(NIns* t); \
        void JNO(NIns* t); \
        void SSE(int32_t c, Register d, Register s); \
        void SSEm(int32_t c, Register r, int32_t d, Register b); \
        void SSEsib(int32_t c, Register rr, int32_t d, Register rb, Register ri, int32_t scale); \
        void LDSDm(Register r, const double* addr); \
        void SSE_LDQ( Register r, int32_t d, Register b); \
        void SSE_LDSS(Register r, int32_t d, Register b); \
        void SSE_LDQsib(Register r, int32_t d, Register rb, Register ri, int32_t scale); \
        void SSE_LDSSsib(Register r, int32_t d, Register rb, Register ri, int32_t scale); \
        void SSE_STSD(int32_t d, Register b, Register r); \
        void SSE_STQ( int32_t d, Register b, Register r); \
        void SSE_STSS(int32_t d, Register b, Register r); \
        void SSE_STQsib(int32_t d, Register rb, Register ri, int32_t scale, Register rv); \
        void SSE_CVTSI2SD(Register xr, Register gr); \
        void SSE_CVTSD2SI(Register gr, Register xr); \
        void SSE_CVTSD2SS(Register xr, Register gr); \
        void SSE_CVTSS2SD(Register xr, Register gr); \
        void SSE_CVTDQ2PD(Register d, Register r); \
        void SSE_MOVD(Register d, Register s); \
        void SSE_MOVSD(Register rd, Register rs); \
        void SSE_ADDSD(Register rd, Register rs); \
        void SSE_ADDSDm(Register r, const double* addr); \
        void SSE_SUBSD(Register rd, Register rs); \
        void SSE_MULSD(Register rd, Register rs); \
        void SSE_DIVSD(Register rd, Register rs); \
        void SSE_UCOMISD(Register rl, Register rr); \
        void SSE_XORPD(Register r, const uint32_t* maskaddr); \
        void SSE_XORPDr(Register rd, Register rs); \
        void FPUc(int32_t o); \
        void FPU(int32_t o, Register r) { \
            underrunProtect(2); \
            *(--_nIns) = uint8_t((uint8_t(o) & 0xff) | (REGNUM(r) & 7)); \
            *(--_nIns) = uint8_t((o >> 8) & 0xff); \
        }; \
        void FPUm(int32_t o, int32_t d, Register b); \
        void FPUdm(int32_t o, const double* const m); \
        void TEST_AH(int32_t i); \
        void TEST_AX(int32_t i); \
        void FNSTSW_AX(); \
        void FCHS(); \
        void FLD1(); \
        void FLDZ(); \
        void FFREE(Register r); \
        void FST32(bool p, int32_t d, Register b); \
        void FSTQ(bool p, int32_t d, Register b); \
        void FSTPQ(int32_t d, Register b); \
        void FCOM(bool p, int32_t d, Register b); \
        void FCOMdm(bool p, const double* dm); \
        void FLD32(int32_t d, Register b); \
        void FLDQ(int32_t d, Register b); \
        void FLDQdm(const double* dm); \
        void FILDQ(int32_t d, Register b); \
        void FILD(int32_t d, Register b); \
        void FIST(bool p, int32_t d, Register b); \
        void FADD( int32_t d, Register b); \
        void FSUB( int32_t d, Register b); \
        void FSUBR(int32_t d, Register b); \
        void FMUL( int32_t d, Register b); \
        void FDIV( int32_t d, Register b); \
        void FDIVR(int32_t d, Register b); \
        void FADDdm( const double *dm); \
        void FSUBRdm(const double* dm); \
        void FMULdm( const double* dm); \
        void FDIVRdm(const double* dm); \
        void FINCSTP(); \
        void FSTP(Register r) { \
            count_fpu(); \
            FPU(0xddd8, r); \
            asm_output("fstp %s", gpn(r)); \
            fpu_pop(); \
        }; \
        void FCOMP(); \
        void FCOMPP(); \
        void FLDr(Register r); \
        void EMMS(); \
        void CALL(const CallInfo* ci); \
        void CALLr(const CallInfo* ci, Register r);
}



#endif // __nanojit_Nativei386__
