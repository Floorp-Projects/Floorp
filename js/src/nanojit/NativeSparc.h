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
 *   leon.sha@sun.com
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

    const int NJ_ALIGN_STACK = 16;

    typedef uint32_t NIns;

    // Bytes of icache to flush after Assembler::patch
    const size_t LARGEST_BRANCH_PATCH = 2 * sizeof(NIns);

    // These are used as register numbers in various parts of the code
    typedef uint32_t Register;
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

        F0  = { 0 },
        F1  = { 1 },
        F2  = { 2 },
        F3  = { 3 },
        F4  = { 4 },
        F5  = { 5 },
        F6  = { 6 },
        F7  = { 7 },
        F8  = { 8 },
        F9  = { 9 },
        F10 = { 10 },
        F11 = { 11 },
        F12 = { 12 },
        F13 = { 13 },
        F14 = { 14 },
        F15 = { 15 },
        F16 = { 16 },
        F17 = { 17 },
        F18 = { 18 },
        F19 = { 19 },
        F20 = { 20 },
        F21 = { 21 },
        F22 = { 22 },
        F23 = { 23 },
        F24 = { 24 },
        F25 = { 25 },
        F26 = { 26 },
        F27 = { 27 },
        F28 = { 28 },
        F29 = { 29 },
        F30 = { 30 },
        F31 = { 31 },

        // helpers
        FRAME_PTR = G4,

        deprecated_UnknownReg = { 30 };     // XXX: remove eventually, see bug 538924

    static const uint32_t FirstRegNum = 0;
    static const uint32_t LastRegNum = 29;
}

#define NJ_USE_UINT32_REGISTER 1
#include "NativeCommon.h"

namespace nanojit
{
    // We only use 32 registers to be managed.
    // So we choose some of them.
    // And other unmanaged registers we can use them directly.
    // The registers that can be used directly are G0-G4, L0, L2, L4, L6
    // SP, FP, F8-F12, F24-F28
    typedef int RegisterMask;
#define _rmask_(r)        (1<<(r))

    // Assembler::savedRegs[] is not needed for sparc because the
    // registers are already saved automatically by "save" instruction.
    static const int NumSavedRegs = 0;

    static const RegisterMask SavedRegs = 1<<L1 | 1<<L3 | 1<<L5 | 1<<L7 |
    1<<I0 | 1<<I1 | 1<<I2 | 1<<I3 |
    1<<I4 | 1<<I5;
    static const RegisterMask GpRegs = SavedRegs | 1<<O0 | 1<<O1 | 1<<O2 |
    1<<O3 | 1<<O4 | 1<<O5;
    static const RegisterMask FpRegs = 1<<F0  | 1<<F2  | 1<<F4  | 1<<F6  |
    1<<F14 | 1<<F16 | 1<<F18 | 1<<F20 |
    1<<F22;
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
    NIns* asm_branchd(bool, LIns*, NIns*);

#define IMM32(i)    \
    --_nIns;        \
    *((int32_t*)_nIns) = (int32_t)(i)

#define CALL(c)    do { \
    int offset = (c->_address) - ((int)_nIns) + 4; \
    int i = 0x40000000 | ((offset >> 2) & 0x3FFFFFFF); \
    IMM32(i);    \
    asm_output("call %s",(c->_name)); \
    } while (0)

#define Format_2_1(rd, op2, imm22) do { \
    int i = rd << 25 | op2 << 22 | (imm22 & 0x3FFFFF); \
    IMM32(i);    \
    } while (0)

#define Format_2_2(a, cond, op2, disp22) \
    Format_2_1((a & 0x1) << 4 | (cond & 0xF), op2, disp22)

#define Format_2_3(a, cond, op2, cc1, cc0, p, disp19) \
    Format_2_2(a, cond, op2, (cc1 & 0x1) << 21 | (cc0 & 0x1) << 20 | (p & 0x1) << 19 | (disp19 & 0x7FFFF))

#define Format_2_4(a, rcond, op2, d16hi, p, rs1, d16lo) \
    Format_2_2(a, (rcond & 0x7), op2, (d16hi & 0x3) << 20 | (p & 0x1) << 19 | rs1 << 14 | (d16lo & 0x3FFF))

#define Format_3(op1, rd, op3, bits19) do { \
    int i = op1 << 30 | rd << 25 | op3 << 19 | (bits19 & 0x7FFFF); \
    IMM32(i);    \
    } while (0)

#define Format_3_1(op1, rd, op3, rs1, bit8, rs2) \
    Format_3(op1, rd, op3, rs1 << 14 | (bit8 & 0xFF) << 5 | rs2)

#define Format_3_1I(op1, rd, op3, rs1, simm13) \
    Format_3(op1, rd, op3, rs1 << 14 | 1 << 13 | (simm13 & 0x1FFF))

#define Format_3_2(op1, rd, op3, rs1, rcond, rs2) \
    Format_3(op1, rd, op3, rs1 << 14 | (rcond & 0x3) << 10 | rs2)

#define Format_3_2I(op1, rd, op3, rs1, rcond, simm10) \
    Format_3(op1, rd, op3, rs1 << 14 | 1 << 13 | (rcond & 0x3) << 10 | (simm10 & 0x1FFF))

#define Format_3_3(op1, rd, op3, rs1, cmask, mmask) \
    Format_3(op1, rd, op3, rs1 << 14 | 1 << 13 | (cmask & 0x7) << 5 | (mmask & 0xF))

#define Format_3_4(op1, rd, op3, bits19) \
    Format_3(op1, rd, op3, bits19)

#define Format_3_5(op1, rd, op3, rs1, x, rs2) \
    Format_3(op1, rd, op3, rs1 << 14 | (x & 0x1) << 12 | rs2)

#define Format_3_6(op1, rd, op3, rs1, shcnt32) \
    Format_3(op1, rd, op3, rs1 << 14 | 1 << 13 | (shcnt32 & 0x3F))

#define Format_3_7(op1, rd, op3, rs1, shcnt64) \
    Format_3(op1, rd, op3, rs1 << 14 | 1 << 13 | 1 << 12 | (shcnt64 & 0x7F))

#define Format_3_8(op1, rd, op3, rs1, bits9, rs2) \
    Format_3(op1, rd, op3, rs1 << 14 | (bits9 & 0x1FF) << 5 | rs2)

#define Format_3_9(op1, cc1, cc0, op3, rs1, bits9, rs2) \
    Format_3(op1, (cc1 & 0x1) << 1 | (cc0 & 0x1), op3, rs1 << 14 | (bits9 & 0x1FF) << 5 | rs2)

#define Format_4_1(rd, op3, rs1, cc1, cc0, rs2) \
    Format_3(2, rd, op3, rs1 << 14 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | rs2)

#define Format_4_1I(rd, op3, rs1, cc1, cc0, simm11) \
    Format_3(2, rd, op3, rs1 << 14 | (cc1 & 0x1) << 12 | 1 << 13 |(cc0 & 0x1) << 11 | (simm11 & 0x7FF))

#define Format_4_2(rd, op3, cc2, cond, cc1, cc0, rs2) \
    Format_3(2, rd, op3, (cc2 & 0x1) << 18 | (cond & 0xF) << 14 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | rs2)

#define Format_4_2I(rd, op3, cc2, cond, cc1, cc0, simm11) \
    Format_3(2, rd, op3, (cc2 & 0x1) << 18 | (cond & 0xF) << 14 | 1 << 13 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | (simm11 & 0x7FF))

#define Format_4_3(rd, op3, rs1, cc1, cc0, swap_trap) \
    Format_3(2, rd, op3, rs1 << 14 | 1 << 13 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | (swap_trap & 0x7F))

#define Format_4_4(rd, op3, rs1, rcond, opf_low, rs2) \
    Format_3(2, rd, op3, rs1 << 14 | (rcond & 0x7) << 10 | (opf_low & 0x1F) << 5 | rs2)

#define Format_4_5(rd, op3, cond, opf_cc, opf_low, rs2) \
    Format_3(2, rd, op3, (cond & 0xF) << 14 | (opf_cc & 0x7) << 11 | (opf_low & 0x3F) << 5 | rs2)

#define ADDCC(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x10, rs1, 0, rs2); \
    asm_output("addcc %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define ADD(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0, rs1, 0, rs2); \
    asm_output("add %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define AND(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x1, rs1, 0, rs2); \
    asm_output("and %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define BA(a, dsp22) \
    do { \
    Format_2_2(a, 0x8, 0x2, dsp22); \
    asm_output("ba %p", _nIns + dsp22 - 1); \
    } while (0)

#define BE(a, dsp22) \
    do { \
    Format_2_2(a, 0x1, 0x2, dsp22); \
    asm_output("be %p", _nIns + dsp22 - 1); \
    } while (0)

#define BG(a, dsp22) \
    do { \
    Format_2_2(a, 0xA, 0x2, dsp22); \
    asm_output("bg %p", _nIns + dsp22 - 1); \
    } while (0)

#define BGU(a, dsp22) \
    do { \
    Format_2_2(a, 0xC, 0x2, dsp22); \
    asm_output("bgu %p", _nIns + dsp22 - 1); \
    } while (0)

#define BGE(a, dsp22) \
    do { \
    Format_2_2(a, 0xB, 0x2, dsp22); \
    asm_output("bge %p", _nIns + dsp22 - 1); \
    } while (0)

#define BL(a, dsp22) \
    do { \
    Format_2_2(a, 0x3, 0x2, dsp22); \
    asm_output("bl %p", _nIns + dsp22 - 1); \
    } while (0)

#define BLE(a, dsp22) \
    do { \
    Format_2_2(a, 0x2, 0x2, dsp22); \
    asm_output("ble %p", _nIns + dsp22 - 1); \
    } while (0)

#define BLEU(a, dsp22) \
    do { \
    Format_2_2(a, 0x4, 0x2, dsp22); \
    asm_output("bleu %p", _nIns + dsp22 - 1); \
    } while (0)

#define BCC(a, dsp22) \
    do { \
    Format_2_2(a, 0xd, 0x2, dsp22); \
    asm_output("bcc %p", _nIns + dsp22 - 1); \
    } while (0)

#define BCS(a, dsp22) \
    do { \
    Format_2_2(a, 0x5, 0x2, dsp22); \
    asm_output("bcs %p", _nIns + dsp22 - 1); \
    } while (0)

#define BVC(a, dsp22) \
    do { \
    Format_2_2(a, 0xf, 0x2, dsp22); \
    asm_output("bvc %p", _nIns + dsp22 - 1); \
    } while (0)

#define BVS(a, dsp22) \
    do { \
    Format_2_2(a, 0x7, 0x2, dsp22); \
    asm_output("bvc %p", _nIns + dsp22 - 1); \
    } while (0)

#define BNE(a, dsp22) \
    do { \
    Format_2_2(a, 0x9, 0x2, dsp22); \
    asm_output("bne %p", _nIns + dsp22 - 1); \
    } while (0)

#define FABSS(rs2, rd) \
    do { \
    Format_3_8(2, rd, 0x34, 0, 0x9, rs2); \
    asm_output("fabs %s, %s", gpn(rs2+32), gpn(rd+32)); \
    } while (0)

#define FADDD(rs1, rs2, rd) \
    do { \
    Format_3_8(2, rd, 0x34, rs1, 0x42, rs2); \
    asm_output("faddd %s, %s, %s", gpn(rs1+32), gpn(rs2+32), gpn(rd+32)); \
    } while (0)

#define FBE(a, dsp22) \
    do { \
    Format_2_2(a, 0x9, 0x6, dsp22); \
    asm_output("fbe %p", _nIns + dsp22 - 1); \
    } while(0)

#define FBNE(a, dsp22) \
    do { \
    Format_2_2(a, 0x1, 0x6, dsp22); \
    asm_output("fbne %p", _nIns + dsp22 - 1); \
    } while(0)

#define FBUE(a, dsp22) \
    do { \
    Format_2_2(a, 0xA, 0x6, dsp22); \
    asm_output("fbue %p", _nIns + dsp22 - 1); \
    } while(0)

#define FBG(a, dsp22) \
    do { \
    Format_2_2(a, 0x6, 0x6, dsp22); \
    asm_output("fng %p", _nIns + dsp22 - 1); \
    } while(0)

#define FBUG(a, dsp22) \
    do { \
    Format_2_2(a, 0x5, 0x6, dsp22); \
    asm_output("fbug %p", _nIns + dsp22 - 1); \
    } while(0)

#define FBGE(a, dsp22) \
    do { \
    Format_2_2(a, 0xB, 0x6, dsp22); \
    asm_output("fbge %p", _nIns + dsp22 - 1); \
    } while(0)

#define FBUGE(a, dsp22) \
    do { \
    Format_2_2(a, 0xC, 0x6, dsp22); \
    asm_output("fbuge %p", _nIns + dsp22 - 1); \
    } while(0)

#define FBL(a, dsp22) \
    do { \
    Format_2_2(a, 0x4, 0x6, dsp22); \
    asm_output("fbl %p", _nIns + dsp22 - 1); \
    } while(0)

#define FBUL(a, dsp22) \
    do { \
    Format_2_2(a, 0x3, 0x6, dsp22); \
    asm_output("fbl %p", _nIns + dsp22 - 1); \
    } while(0)

#define FBLE(a, dsp22) \
    do { \
    Format_2_2(a, 0xD, 0x6, dsp22); \
    asm_output("fble %p", _nIns + dsp22 - 1); \
    } while(0)

#define FBULE(a, dsp22) \
    do { \
    Format_2_2(a, 0xE, 0x6, dsp22); \
    asm_output("fbule %p", _nIns + dsp22 - 1); \
    } while(0)

#define FCMPD(rs1, rs2) \
    do { \
    Format_3_9(2, 0, 0, 0x35, rs1, 0x52, rs2); \
    asm_output("fcmpd %s, %s", gpn(rs1+32), gpn(rs2+32)); \
    } while (0)

#define FSUBD(rs1, rs2, rd) \
    do { \
    Format_3_8(2, rd, 0x34, rs1, 0x46, rs2); \
    asm_output("fsubd %s, %s, %s", gpn(rs1+32), gpn(rs2+32), gpn(rd+32)); \
    } while (0)

#define FMULD(rs1, rs2, rd) \
    do { \
    Format_3_8(2, rd, 0x34, rs1, 0x4a, rs2); \
    asm_output("fmuld %s, %s, %s", gpn(rs1+32), gpn(rs2+32), gpn(rd+32)); \
    } while (0)

#define FDTOI(rs2, rd) \
    do { \
    Format_3_8(2, rd, 0x34, 0, 0xd2, rs2); \
    asm_output("fdtoi %s, %s", gpn(rs2+32), gpn(rd+32)); \
    } while (0)

#define FDIVD(rs1, rs2, rd) \
    do { \
    Format_3_8(2, rd, 0x34, rs1, 0x4e, rs2); \
    asm_output("fdivd %s, %s, %s", gpn(rs1+32), gpn(rs2+32), gpn(rd+32)); \
    } while (0)

#define FMOVD(rs2, rd) \
    do { \
    Format_3_8(2, rd, 0x34, 0, 0x2, rs2); \
    asm_output("fmovd %s, %s", gpn(rs2+32), gpn(rd+32)); \
    } while (0)

#define FNEGD(rs2, rd) \
    do { \
    Format_3_8(2, rd, 0x34, 0, 0x6, rs2); \
    asm_output("fnegd %s, %s", gpn(rs2+32), gpn(rd+32)); \
    } while (0)

#define FITOD(rs2, rd) \
    do { \
    Format_3_8(2, rd, 0x34, 0, 0xc8, rs2); \
    asm_output("fitod %s, %s", gpn(rs2+32), gpn(rd+32)); \
    } while (0)

#define JMPL(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x38, rs1, 0, rs2); \
    asm_output("jmpl [%s + %s]", gpn(rs1), gpn(rs2)); \
    } while (0)

#define JMPLI(rs1, simm13, rd) \
    do { \
    Format_3_1I(2, rd, 0x38, rs1, simm13); \
    asm_output("jmpl [%s + %d]", gpn(rs1), simm13); \
    } while (0)

#define LDF(rs1, rs2, rd) \
    do { \
    Format_3_1(3, rd, 0x20, rs1, 0, rs2); \
    asm_output("ld [%s + %s], %s", gpn(rs1), gpn(rs2), gpn(rd+32)); \
    } while (0)

#define LDFI(rs1, simm13, rd) \
    do { \
    Format_3_1I(3, rd, 0x20, rs1, simm13); \
    asm_output("ld [%s + %d], %s", gpn(rs1), simm13, gpn(rd+32)); \
    } while (0)

#define LDUB(rs1, rs2, rd) \
    do { \
    Format_3_1(3, rd, 0x1, rs1, 0, rs2); \
    asm_output("ld [%s + %s], %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define LDUBI(rs1, simm13, rd) \
    do { \
    Format_3_1I(3, rd, 0x1, rs1, simm13); \
    asm_output("ld [%s + %d], %s", gpn(rs1), simm13, gpn(rd)); \
    } while (0)

#define LDUH(rs1, rs2, rd) \
    do { \
    Format_3_1(3, rd, 0x2, rs1, 0, rs2); \
    asm_output("ld [%s + %s], %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define LDUHI(rs1, simm13, rd) \
    do { \
    Format_3_1I(3, rd, 0x2, rs1, simm13); \
    asm_output("ld [%s + %d], %s", gpn(rs1), simm13, gpn(rd)); \
    } while (0)

#define LDSW(rs1, rs2, rd) \
    do { \
    Format_3_1(3, rd, 0x8, rs1, 0, rs2); \
    asm_output("ld [%s + %s], %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define LDSWI(rs1, simm13, rd) \
    do { \
    Format_3_1I(3, rd, 0x8, rs1, simm13); \
    asm_output("ld [%s + %d], %s", gpn(rs1), simm13, gpn(rd)); \
    } while (0)

#define MOVE(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 1, cc1, cc0, rs); \
    asm_output("move %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVNE(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 9, cc1, cc0, rs); \
    asm_output("movne %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVL(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 3, cc1, cc0, rs); \
    asm_output("movl %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVLE(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 2, cc1, cc0, rs); \
    asm_output("movle %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVG(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 0xa, cc1, cc0, rs); \
    asm_output("movg %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVGE(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 0xb, cc1, cc0, rs); \
    asm_output("movge %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVCS(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 5, cc1, cc0, rs); \
    asm_output("movcs %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVLEU(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 4, cc1, cc0, rs); \
    asm_output("movleu %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVGU(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 0xc, cc1, cc0, rs); \
    asm_output("movgu %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVCC(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 0xd, cc1, cc0, rs); \
    asm_output("movcc %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVVC(rs, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2(rd, 0x2c, cc2, 0xf, cc1, cc0, rs); \
    asm_output("movvc %s, %s", gpn(rs), gpn(rd)); \
    } while (0)

#define MOVEI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 1, cc1, cc0, simm11); \
    asm_output("move %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVFEI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 9, cc1, cc0, simm11); \
    asm_output("move %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVNEI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 9, cc1, cc0, simm11); \
    asm_output("move %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVLI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 3, cc1, cc0, simm11); \
    asm_output("move %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVFLI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 4, cc1, cc0, simm11); \
    asm_output("move %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVLEI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 2, cc1, cc0, simm11); \
    asm_output("movle %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVFLEI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 0xd, cc1, cc0, simm11); \
    asm_output("movle %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVGI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 0xa, cc1, cc0, simm11); \
    asm_output("movg %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVFGI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 6, cc1, cc0, simm11); \
    asm_output("movg %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVGEI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 0xb, cc1, cc0, simm11); \
    asm_output("movge %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVFGEI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 0xb, cc1, cc0, simm11); \
    asm_output("movge %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVLEUI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 4, cc1, cc0, simm11); \
    asm_output("movleu %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVGUI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 0xc, cc1, cc0, simm11); \
    asm_output("movgu %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVCCI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 0xd, cc1, cc0, simm11); \
    asm_output("movcc %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVCSI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 0x5, cc1, cc0, simm11); \
    asm_output("movcs %d, %s", simm11, gpn(rd)); \
    } while (0)

#define MOVVSI(simm11, cc2, cc1, cc0, rd) \
    do { \
    Format_4_2I(rd, 0x2c, cc2, 7, cc1, cc0, simm11); \
    asm_output("movvs %d, %s", simm11, gpn(rd)); \
    } while (0)

#define SMULCC(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x1b, rs1, 0, rs2); \
    asm_output("smulcc %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define NOP() \
    do { \
    Format_2_1(0, 0x4, 0); \
    asm_output("nop"); \
    } while (0)

#define ORI(rs1, simm13, rd) \
    do { \
    Format_3_1I(2, rd, 0x2, rs1, simm13); \
    asm_output("or %s, %d, %s", gpn(rs1), simm13, gpn(rd)); \
    } while (0)

#define OR(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x2, rs1, 0, rs2); \
    asm_output("or %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define ORN(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x6, rs1, 0, rs2); \
    asm_output("orn %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define ANDCC(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x11, rs1, 0, rs2); \
    asm_output("andcc %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define RDY(rd) \
    do { \
    Format_3_1(2, rd, 0x28, 0, 0, 0); \
    asm_output("rdy %s", gpn(rd)); \
    } while (0)

#define RESTORE(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x3D, rs1, 0, rs2); \
    asm_output("restore"); \
    } while (0)

#define SAVEI(rs1, simm13, rd) \
    do { \
    Format_3_1I(2, rd, 0x3C, rs1, simm13); \
    asm_output("save %s, %d, %s", gpn(rs1), simm13, gpn(rd)); \
    } while (0)

#define SAVE(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x3C, rs1, 0, rs2); \
    asm_output("save %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define SETHI(imm22, rd) \
    do { \
    Format_2_1(rd, 0x4, (imm22 >> 10)); \
    asm_output("sethi %p, %s", imm22,  gpn(rd)); \
    } while (0)

#define SLL(rs1, rs2, rd) \
    do { \
    Format_3_5(2, rd, 0x25, rs1, 0, rs2); \
    asm_output("sll %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define SRA(rs1, rs2, rd) \
    do { \
    Format_3_5(2, rd, 0x27, rs1, 0, rs2); \
    asm_output("sra %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define SRAI(rs1, shcnt32, rd) \
    do { \
    Format_3_6(2, rd, 0x27, rs1, shcnt32); \
    asm_output("sra %s, %d, %s", gpn(rs1), shcnt32, gpn(rd)); \
    } while (0)

#define SRL(rs1, rs2, rd) \
    do { \
    Format_3_5(2, rd, 0x26, rs1, 0, rs2); \
    asm_output("srl %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define STF(rd, rs1, rs2) \
    do { \
    Format_3_1(3, rd, 0x24, rs1, 0, rs2); \
    asm_output("st %s, [%s + %s]", gpn(rd+32), gpn(rs1), gpn(rs2)); \
    } while (0)

#define STFI(rd, simm13, rs1) \
    do { \
    Format_3_1I(3, rd, 0x24, rs1, simm13); \
    asm_output("st %s, [%s + %d]", gpn(rd+32), gpn(rs1), simm13); \
    } while (0)

#define STW(rd, rs2, rs1) \
    do { \
    Format_3_1(3, rd, 0x4, rs1, 0, rs2); \
    asm_output("st %s, [%s + %s]", gpn(rd), gpn(rs1), gpn(rs2)); \
    } while (0)

#define STWI(rd, simm13, rs1) \
    do { \
    Format_3_1I(3, rd, 0x4, rs1, simm13); \
    asm_output("st %s, [%s + %d]", gpn(rd), gpn(rs1), simm13); \
    } while (0)

#define STB(rd, rs2, rs1) \
    do { \
    Format_3_1(3, rd, 0x5, rs1, 0, rs2); \
    asm_output("stb %s, [%s + %s]", gpn(rd), gpn(rs1), gpn(rs2)); \
    } while (0)

#define STBI(rd, simm13, rs1) \
    do { \
    Format_3_1I(3, rd, 0x5, rs1, simm13); \
    asm_output("stb %s, [%s + %d]", gpn(rd), gpn(rs1), simm13); \
    } while (0)


#define SUBCC(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x14, rs1, 0, rs2); \
    asm_output("subcc %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define SUB(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x4, rs1, 0, rs2); \
    asm_output("sub %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

#define SUBI(rs1, simm13, rd) \
    do { \
    Format_3_1I(2, rd, 0x4, rs1, simm13); \
    asm_output("sub %s, %d, %s", gpn(rs1), simm13, gpn(rd)); \
    } while (0)

#define XOR(rs1, rs2, rd) \
    do { \
    Format_3_1(2, rd, 0x3, rs1, 0, rs2); \
    asm_output("xor %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd)); \
    } while (0)

        // Returns true if imm below 13-bit unsigned immediate)
#define isIMM13(imm) \
    (imm) <= 4095 && (imm) >= -4096

#define isIMM19(imm) \
    (imm) <= 0x3ffff && (imm) >= -0x40000

#define isIMM22(imm) \
    (imm) <= 0x1fffff && (imm) >= -0x200000

#define SET32(immI, rd) \
    if(isIMM13(immI)) { \
       ORI(G0, immI, rd); \
    } else { \
      ORI(rd, immI & 0x3FF, rd); \
      SETHI(immI, rd); \
    }

#define STDF32(rd, immI, rs1) \
    if(isIMM13(immI+4)) { \
      STFI(rd+1, immI+4, rs1); \
      STFI(rd, immI, rs1); \
    } else { \
      STF(rd+1, L0, rs1); \
      SET32(immI+4, L0); \
      STF(rd, L0, rs1); \
      SET32(immI, L0); \
    }

#define STF32(rd, immI, rs1) \
    if(isIMM13(immI+4)) { \
      STFI(rd, immI, rs1); \
    } else { \
      STF(rd, L0, rs1); \
      SET32(immI, L0); \
    }

#define LDDF32(rs1, immI, rd) \
    if(isIMM13(immI+4)) { \
      LDFI(rs1, immI+4, rd+1); \
      LDFI(rs1, immI, rd); \
    } else { \
      LDF(rs1, L0, rd+1); \
      SET32(immI+4, L0); \
      LDF(rs1, L0, rd); \
      SET32(immI, L0); \
    }

#define STW32(rd, immI, rs1) \
    if(isIMM13(immI)) { \
      STWI(rd, immI, rs1); \
    } else { \
      STW(rd, L0, rs1); \
      SET32(immI, L0); \
    }

#define STB32(rd, immI, rs1) \
    if(isIMM13(immI)) { \
      STBI(rd, immI, rs1); \
    } else { \
      STB(rd, L0, rs1); \
      SET32(immI, L0); \
    }

#define LDUB32(rs1, immI, rd) \
    if(isIMM13(immI)) { \
      LDUBI(rs1, immI, rd); \
    } else { \
      LDUB(rs1, L0, rd); \
      SET32(immI, L0); \
    }

#define LDUH32(rs1, immI, rd) \
    if(isIMM13(immI)) { \
      LDUHI(rs1, immI, rd); \
    } else { \
      LDUH(rs1, L0, rd); \
      SET32(immI, L0); \
    }

#define LDSW32(rs1, immI, rd) \
    if(isIMM13(immI)) { \
      LDSWI(rs1, immI, rd); \
    } else { \
      LDSW(rs1, L0, rd); \
      SET32(immI, L0); \
    }


        // general Assemble
#define JMP_long_nocheck(t) do { NOP(); JMPL(G0, G2, G0); ORI(G2, t & 0x3FF, G2); SETHI(t, G2); } while(0)
#define JMP_long(t) do { underrunProtect(16); JMP_long_nocheck(t); } while(0)
#define JMP_long_placeholder() JMP_long(0)

#define JCC(t, tt) \
    underrunProtect(32); \
    tt = ((intptr_t)t - (intptr_t)_nIns + 8) >> 2; \
    if( !(isIMM22((int32_t)tt)) ) { \
        NOP(); \
        JMPL(G0, G2, G0); \
        SET32((intptr_t)t, G2); \
        NOP(); \
        BA(0, 5); \
        tt = 4; \
    } \
    NOP()

#define JMP(t)   do { \
        if (!t) { \
            JMP_long_placeholder(); \
        } else { \
            intptr_t tt; \
            JCC(t, tt); \
            BA(0, tt); \
        } \
    } while(0)

#define MR(d, s)   do { underrunProtect(4); ORI(s, 0, d); } while(0)

        }
#endif // __nanojit_NativeSparc__
