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
 *   leon.sha@oracle.com
 *   ginn.chen@oracle.com
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


#ifndef __nanojit_NativeSparc__
#define __nanojit_NativeSparc__

#include "NativeCommon.h"

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

namespace nanojit
{
    const int NJ_MAX_REGISTERS = 30; // L0 - L7, I0 - I5, F2 - F14

    const int LARGEST_UNDERRUN_PROT = 32;  // largest value passed to underrunProtect

#define NJ_MAX_STACK_ENTRY              8192
#define NJ_MAX_PARAMETERS               1

#define NJ_JTBL_SUPPORTED               0
#define NJ_EXPANDED_LOADSTORE_SUPPORTED 0
#define NJ_F2I_SUPPORTED                1
#define NJ_SOFTFLOAT_SUPPORTED          0
#define NJ_DIVI_SUPPORTED               0    

    const int NJ_ALIGN_STACK = 16;

    typedef uint32_t NIns;

    // Bytes of icache to flush after Assembler::patch
    const size_t LARGEST_BRANCH_PATCH = 2 * sizeof(NIns);

    // These are used as register numbers in various parts of the code
    static const Register
        G0  = { 0 },
        G1  = { 1 },
        G2  = { 2 },
        G3  = { 3 },
        G4  = { 4 },
        G5  = { 5 }, // Reserved for system
        G6  = { 6 }, // Reserved for system
        G7  = { 7 }, // Reserved for system

        O0  = { 8 },
        O1  = { 9 },
        O2  = { 10 },
        O3  = { 11 },
        O4  = { 12 },
        O5  = { 13 },
        O6  = { 14 }, // SP
        O7  = { 15 }, // Not used.

        L0  = { 16 },
        L1  = { 17 },
        L2  = { 18 },
        L3  = { 19 },
        L4  = { 20 },
        L5  = { 21 },
        L6  = { 22 },
        L7  = { 23 },

        I0  = { 24 },
        I1  = { 25 },
        I2  = { 26 },
        I3  = { 27 },
        I4  = { 28 },
        I5  = { 29 },
        I6  = { 30 }, // FP
        I7  = { 31 }, // Not used

        SP  = O6,
        FP  = I6,

        F0  = { 32 },
        F1  = { 33 },
        F2  = { 34 },
        F3  = { 35 },
        F4  = { 36 },
        F5  = { 37 },
        F6  = { 38 },
        F7  = { 39 },
        F8  = { 40 },
        F9  = { 41 },
        F10 = { 42 },
        F11 = { 43 },
        F12 = { 44 },
        F13 = { 45 },
        F14 = { 46 },
        F15 = { 47 },
        F16 = { 48 },
        F17 = { 49 },
        F18 = { 50 },
        F19 = { 51 },
        F20 = { 52 },
        F21 = { 53 },
        F22 = { 54 },
        F23 = { 55 },
        F24 = { 56 },
        F25 = { 57 },
        F26 = { 58 },
        F27 = { 59 },
        F28 = { 60 },
        F29 = { 61 },
        F30 = { 62 },
        F31 = { 63 },

        // helpers
        FRAME_PTR = G4,

        deprecated_UnknownReg = { 64 };     // XXX: remove eventually, see bug 538924

    static const uint32_t FirstRegNum = 0;
    static const uint32_t LastRegNum = 63;
}

namespace nanojit
{
    // We only use 32 registers to be managed.
    // So we choose some of them.
    // And other unmanaged registers we can use them directly.
    // The registers that can be used directly are G0-G4, L0, L2, L4, L6
    // SP, FP, F8-F12, F24-F28
    typedef uint64_t RegisterMask;
#define _rmask_(r)        ((RegisterMask)1<<(REGNUM(r)))
#define _reg_(r)          (REGNUM(r) & 0x1F)
    // Assembler::savedRegs[] is not needed for sparc because the
    // registers are already saved automatically by "save" instruction.
    static const int NumSavedRegs = 0;

    static const RegisterMask SavedRegs = _rmask_(L1) | _rmask_(L3) | _rmask_(L5) | _rmask_(L7) |
                                          _rmask_(I0) | _rmask_(I1) | _rmask_(I2) | _rmask_(I3) |
                                          _rmask_(I4) | _rmask_(I5);
    static const RegisterMask GpRegs = SavedRegs | _rmask_(O0) | _rmask_(O1) | _rmask_(O2) |
                                                   _rmask_(O3) | _rmask_(O4) | _rmask_(O5);
    static const RegisterMask FpRegs =  _rmask_(F0)  | _rmask_(F2)  | _rmask_(F4)  |
                                        _rmask_(F6)  | _rmask_(F14) | _rmask_(F16) |
                                        _rmask_(F18) | _rmask_(F20) | _rmask_(F22);
    static const RegisterMask AllowableFlagRegs = GpRegs;

    verbose_only( extern const char* regNames[]; )

#define DECLARE_PLATFORM_STATS()

#define DECLARE_PLATFORM_REGALLOC()

#define DECLARE_PLATFORM_ASSEMBLER()    \
     const static Register argRegs[6], retRegs[1]; \
     bool has_cmov; \
     void nativePageReset(); \
     void nativePageSetup(); \
     void underrunProtect(int bytes); \
     void asm_align_code(); \
     void asm_cmp(LIns *cond); \
     void asm_cmpd(LIns *cond); \
     NIns* asm_branchd(bool, LIns*, NIns*); \
     void IMM32(int32_t i) { \
         --_nIns; \
         *((int32_t*)_nIns) = i; \
     } \
     void CALL(const CallInfo* ci); \
     void Format_2(int32_t x, int32_t op2, int32_t imm22) { \
         int32_t i = x << 25 | op2 << 22 | imm22 & 0x3FFFFF; \
         IMM32(i); \
     } \
     void Format_2A(Register rd, int32_t op2, int32_t imm22) { \
         Format_2(_reg_(rd), op2, imm22); \
     } \
     void Format_2_2(int32_t a, int32_t cond, int32_t op2, int32_t disp22) { \
         Format_2((a & 0x1) << 4 | cond & 0xF, op2, disp22); \
     } \
     void Format_2_3(int32_t a, int32_t cond, int32_t op2, int32_t cc1, int32_t cc0, int32_t p, int32_t disp19) { \
         Format_2_2(a, cond, op2, (cc1 & 0x1) << 21 | (cc0 & 0x1) << 20 | (p & 0x1) << 19 | disp19 & 0x7FFFF); \
     } \
     void Format_2_4(int32_t a, int32_t rcond, int32_t op2, int32_t d16hi, int32_t p, int32_t rs1, int32_t d16lo) { \
         Format_2_2(a, rcond & 0x7, op2, (d16hi & 0x3) << 20 | (p & 0x1) << 19 | rs1 << 14 | d16lo & 0x3FFF); \
     } \
     void Format_3(int32_t op1, int32_t x, int32_t op3, int32_t bits19) { \
         int32_t i = op1 << 30 | x << 25 | op3 << 19 |  bits19 & 0x7FFFF; \
         IMM32(i); \
     } \
     void Format_3A(int32_t op1, Register rd, int32_t op3, int32_t bits19) { \
         Format_3(op1, _reg_(rd), op3, bits19); \
     } \
     void Format_3_1(int32_t op1, Register rd, int32_t op3, Register rs1, int32_t bit8, Register rs2) { \
         Format_3A(op1, rd, op3, _reg_(rs1) << 14 | (bit8 & 0xFF) << 5 | _reg_(rs2)); \
     } \
     void Format_3_1I(int32_t op1, Register rd, int32_t op3, Register rs1, int32_t simm13) { \
         Format_3A(op1, rd, op3, _reg_(rs1) << 14 | 1 << 13 | simm13 & 0x1FFF); \
     } \
    void Format_3_2(int32_t op1, Register rd, int32_t op3, Register rs1, int32_t rcond, Register rs2) { \
        Format_3A(op1, rd, op3,  _reg_(rs1) << 14 | (rcond & 0x3) << 10 | _reg_(rs2)); \
    } \
    void Format_3_2I(int32_t op1, Register rd, int32_t op3, Register rs1, int32_t rcond, int32_t simm10) { \
        Format_3A(op1, rd, op3, _reg_(rs1) << 14 | 1 << 13 | (rcond & 0x3) << 10 | simm10 & 0x1FFF); \
    } \
    void Format_3_3(int32_t op1, Register rd, int32_t op3, Register rs1, int32_t cmask, int32_t mmask) { \
        Format_3A(op1, rd, op3, _reg_(rs1) << 14 | 1 << 13 | (cmask & 0x7) << 5 | mmask & 0xF); \
    } \
    void Format_3_4(int32_t op1, Register rd, int32_t op3, int32_t bits19) { \
        Format_3A(op1, rd, op3, bits19); \
    } \
    void Format_3_5(int32_t op1, Register rd, int32_t op3, Register rs1, int32_t x, Register rs2) { \
        Format_3A(op1, rd, op3, _reg_(rs1) << 14 | (x & 0x1) << 12 | _reg_(rs2)); \
    } \
    void Format_3_6(int32_t op1, Register rd, int32_t op3, Register rs1, int32_t shcnt32) { \
        Format_3A(op1, rd, op3, _reg_(rs1) << 14 |  1 << 13 | (shcnt32 & 0x3F)); \
    } \
    void Format_3_7(int32_t op1, Register rd, int32_t op3, Register rs1, int32_t shcnt64) { \
        Format_3A(op1, rd, op3, _reg_(rs1) << 14 | 1 << 13 | 1 << 12 | (shcnt64 & 0x7F)); \
    } \
    void Format_3_8(int32_t op1, Register rd, int32_t op3, Register rs1, int32_t bits9, Register rs2) { \
        Format_3A(op1, rd, op3, _reg_(rs1) << 14 | (bits9 & 0x1FF) << 5 | _reg_(rs2)); \
    } \
    void Format_3_9(int32_t op1, int32_t cc1, int32_t cc0, int32_t op3, Register rs1, int32_t bits9, Register rs2) { \
        Format_3(op1, (cc1 & 0x1) << 1 | (cc0 & 0x1), op3,  _reg_(rs1) << 14  | (bits9 & 0x1FF) << 5 |  _reg_(rs2)); \
    } \
    void Format_4_1(Register rd, int32_t op3, Register rs1, int32_t cc1, int32_t cc0, Register rs2) { \
        Format_3A(2, rd, op3, _reg_(rs1) << 14 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | _reg_(rs2)); \
    } \
    void Format_4_1I(Register rd, int32_t op3, Register rs1, int32_t cc1, int32_t cc0, int32_t simm11) { \
        Format_3A(2, rd, op3, _reg_(rs1) << 14 | (cc1 & 0x1) << 12 | 1 << 13 |(cc0 & 0x1) << 11 | simm11 & 0x7FF); \
    } \
    void Format_4_2(Register rd, int32_t op3, int32_t cc2, int32_t cond, int32_t cc1, int32_t cc0, Register rs2) { \
        Format_3A(2, rd, op3, (cc2 & 0x1) << 18 | (cond & 0xF) << 14 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | _reg_(rs2)); \
    } \
    void Format_4_2I(Register rd, int32_t op3, int32_t cc2, int32_t cond, int32_t cc1, int32_t cc0, int32_t simm11) { \
        Format_3A(2, rd, op3, (cc2 & 0x1) << 18 | (cond & 0xF) << 14 | 1 << 13 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | (simm11 & 0x7FF)); \
    } \
    void Format_4_3(Register rd, int32_t op3, Register rs1, int32_t cc1, int32_t cc0, int32_t swap_trap) { \
        Format_3A(2, rd, op3, _reg_(rs1) << 14 | 1 << 13 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | swap_trap & 0x7F); \
    } \
    void Format_4_4(Register rd, int32_t op3, Register rs1, int32_t rcond, int32_t opf_low, Register rs2) { \
        Format_3A(2, rd, op3, _reg_(rs1) << 14 | (rcond & 0x7) << 10 | (opf_low & 0x1F) << 5 | _reg_(rs2)); \
    } \
    void Format_4_5(Register rd, int32_t op3, int32_t cond, int32_t opf_cc, int32_t opf_low, Register rs2) { \
        Format_3A(2, rd, op3, (cond & 0xF) << 14 | (opf_cc & 0x7) << 11 | (opf_low & 0x3F) << 5 | _reg_(rs2)); \
    } \
    void IntegerOperation(Register rs1, Register rs2, Register rd, int32_t op3, const char *opcode); \
    void Assembler::IntegerOperationI(Register rs1, int32_t simm13, Register rd, int32_t op3, const char *opcode); \
    void FloatOperation(Register rs1, Register rs2, Register rd, int32_t op3, const char *opcode); \
    void Bicc(int32_t a, int32_t dsp22, int32_t cond, const char *opcode); \
    void FBfcc(int32_t a, int32_t dsp22, int32_t cond, const char *opcode); \
    void LoadOperation(Register rs1, Register rs2, Register rd, int32_t op3, const char* opcode); \
    void LoadOperationI(Register rs1, int32_t simm13, Register rd, int32_t op3, const char* opcode); \
    void MOVcc(Register rs, int32_t cc2, int32_t cc1, int32_t cc0, Register rd, int32_t cond, const char *opcode); \
    void MOVccI(int32_t simm11, int32_t cc2, int32_t cc1, int32_t cc0, Register rd, int32_t cond, const char *opcode); \
    void ShiftOperation(Register rs1, Register rs2, Register rd, int32_t op3, const char* opcode); \
    void ShiftOperationI(Register rs1, int32_t shcnt32, Register rd, int32_t op3, const char* opcode); \
    void Store(Register rd, Register rs1, Register rs2, int32_t op3, const char* opcode); \
    void Assembler::StoreI(Register rd, int32_t simm13, Register rs1, int32_t op3, const char* opcode); \
    void ADD(Register rs1, Register rs2, Register rd); \
    void ADDCC(Register rs1, Register rs2, Register rd); \
    void AND(Register rs1, Register rs2, Register rd); \
    void ANDCC(Register rs1, Register rs2, Register rd); \
    void OR(Register rs1, Register rs2, Register rd); \
    void ORI(Register rs1, int32_t simm13, Register rd); \
    void ORN(Register rs1, Register rs2, Register rd); \
    void SMULCC(Register rs1, Register rs2, Register rd); \
    void SUB(Register rs1, Register rs2, Register rd); \
    void SUBCC(Register rs1, Register rs2, Register rd); \
    void SUBI(Register rs1, int32_t simm13, Register rd); \
    void XOR(Register rs1, Register rs2, Register rd); \
    void BA(int32_t a, int32_t dsp22); \
    void BE(int32_t a, int32_t dsp22); \
    void BNE(int32_t a, int32_t dsp22); \
    void BG(int32_t a, int32_t dsp22); \
    void BGU(int32_t a, int32_t dsp22); \
    void BGE(int32_t a, int32_t dsp22); \
    void BL(int32_t a, int32_t dsp22); \
    void BLE(int32_t a, int32_t dsp22); \
    void BLEU(int32_t a, int32_t dsp22); \
    void BCC(int32_t a, int32_t dsp22); \
    void BCS(int32_t a, int32_t dsp22); \
    void BVC(int32_t a, int32_t dsp22); \
    void BVS(int32_t a, int32_t dsp22); \
    void FABSS(Register rs2, Register rd); \
    void FADDD(Register rs1, Register rs2, Register rd); \
    void FBE(int32_t a, int32_t dsp22); \
    void FBNE(int32_t a, int32_t dsp22); \
    void FBUE(int32_t a, int32_t dsp22); \
    void FBG(int32_t a, int32_t dsp22); \
    void FBUG(int32_t a, int32_t dsp22); \
    void FBGE(int32_t a, int32_t dsp22); \
    void FBUGE(int32_t a, int32_t dsp22); \
    void FBL(int32_t a, int32_t dsp22); \
    void FBUL(int32_t a, int32_t dsp22); \
    void FBLE(int32_t a, int32_t dsp22); \
    void FBULE(int32_t a, int32_t dsp22); \
    void FCMPD(Register rs1, Register rs2); \
    void FSUBD(Register rs1, Register rs2, Register rd); \
    void FMULD(Register rs1, Register rs2, Register rd); \
    void FDTOI(Register rs2, Register rd); \
    void FDIVD(Register rs1, Register rs2, Register rd); \
    void FMOVD(Register rs2, Register rd); \
    void FNEGD(Register rs2, Register rd); \
    void FITOD(Register rs2, Register rd); \
    void JMPL(Register rs1, Register rs2, Register rd); \
    void JMPLI(Register rs1, int32_t simm13, Register rd); \
    void LDF(Register rs1, Register rs2, Register rd); \
    void LDFI(Register rs1, int32_t simm13, Register rd); \
    void LDDF32(Register rs1, int32_t immI, Register rd); \
    void LDUB(Register rs1, Register rs2, Register rd); \
    void LDUBI(Register rs1, int32_t simm13, Register rd); \
    void LDUB32(Register rs1, int32_t immI, Register rd); \
    void LDUH(Register rs1, Register rs2, Register rd); \
    void LDUHI(Register rs1, int32_t simm13, Register rd); \
    void LDUH32(Register rs1, int32_t immI, Register rd); \
    void LDSW(Register rs1, Register rs2, Register rd); \
    void LDSWI(Register rs1, int32_t simm13, Register rd); \
    void LDSW32(Register rs1, int32_t immI, Register rd); \
    void MOVE(Register rs, Register rd); \
    void MOVNE(Register rs, Register rd); \
    void MOVL(Register rs, Register rd); \
    void MOVLE(Register rs, Register rd); \
    void MOVG(Register rs, Register rd); \
    void MOVGE(Register rs, Register rd); \
    void MOVLEU(Register rs, Register rd); \
    void MOVGU(Register rs, Register rd); \
    void MOVCC(Register rs, Register rd); \
    void MOVCS(Register rs, Register rd); \
    void MOVVC(Register rs, Register rd); \
    void MOVEI(int32_t simm11, Register rd); \
    void MOVFEI(int32_t simm11, Register rd); \
    void MOVNEI(int32_t simm11, Register rd); \
    void MOVLI(int32_t simm11, Register rd); \
    void MOVFLI(int32_t simm11, Register rd); \
    void MOVLEI(int32_t simm11, Register rd); \
    void MOVFLEI(int32_t simm11, Register rd); \
    void MOVGI(int32_t simm11, Register rd); \
    void MOVFGI(int32_t simm11, Register rd); \
    void MOVGEI(int32_t simm11, Register rd); \
    void MOVFGEI(int32_t simm11, Register rd); \
    void MOVLEUI(int32_t simm11, Register rd); \
    void MOVGUI(int32_t simm11, Register rd); \
    void MOVCCI(int32_t simm11, Register rd); \
    void MOVCSI(int32_t simm11, Register rd); \
    void MOVVSI(int32_t simm11, Register rd); \
    void NOP(); \
    void RDY(Register rd); \
    void RESTORE(Register rs1, Register rs2, Register rd); \
    void SAVE(Register rs1, Register rs2, Register rd); \
    void SAVEI(Register rs1, int32_t simm13, Register rd); \
    void SETHI(int32_t immI, Register rd); \
    void SET32(int32_t immI, Register rd); \
    void SLL(Register rs1, Register rs2, Register rd); \
    void SRA(Register rs1, Register rs2, Register rd); \
    void SRAI(Register rs1, int32_t shcnt32, Register rd); \
    void SRL(Register rs1, Register rs2, Register rd); \
    void STF(Register rd, Register rs1, Register rs2); \
    void STFI(Register rd, int32_t simm13, Register rs1); \
    void STF32(Register rd, int32_t immI, Register rs1); \
    void STDF32(Register rd, int32_t immI, Register rs1); \
    void STW(Register rd, Register rs1, Register rs2); \
    void STWI(Register rd, int32_t simm13, Register rs1); \
    void STW32(Register rd, int32_t immI, Register rs1); \
    void STB(Register rd, Register rs1, Register rs2); \
    void STBI(Register rd, int32_t simm13, Register rs1); \
    void STB32(Register rd, int32_t immI, Register rs1); \
    bool isIMM13(int32_t imm) { return (imm) <= 0xfff && (imm) >= -0x1000; } \
    bool isIMM19(int32_t imm) { return (imm) <= 0x3ffff && (imm) >= -0x40000; } \
    bool isIMM22(int32_t imm) { return (imm) <= 0x1fffff && (imm) >= -0x200000; } \
    void JMP_long_nocheck(int32_t t); \
    void JMP_long(int32_t t); \
    void JMP_long_placeholder(); \
    int32_t JCC(void *t); \
    void JMP(void *t); \
    void MR(Register rd, Register rs);
}
#endif // __nanojit_NativeSparc__
