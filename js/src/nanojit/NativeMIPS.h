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
 * MIPS Technologies Inc
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Dearman <chris@mips.com>
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

#ifndef __nanojit_NativeMIPS__
#define __nanojit_NativeMIPS__

#include "../vprof/vprof.h"
#ifdef PERFM
#define DOPROF
#endif
#define count_instr()   _nvprof("mips", 1)
#define count_mov()     do { _nvprof("mips-mov", 1); count_instr(); } while (0)
#define count_jmp()     do { _nvprof("mips-jmp", 1); count_instr(); } while (0)
#define count_prolog()  do { _nvprof("mips-prolog", 1); count_instr(); } while (0)
#define count_alu()     do { _nvprof("mips-alu", 1); count_instr(); } while (0)
#define count_misc()    do { _nvprof("mips-misc", 1); count_instr(); } while (0)
#define count_fpu()     do { _nvprof("mips-fpu", 1); count_instr(); } while (0)
#define count_br()      do { _nvprof("mips-br", 1); count_instr(); } while (0)

namespace nanojit
{
    // Req: NJ_MAX_STACK_ENTRY is number of instructions to hold in LIR stack
#if 0
    // FIXME: Inconsistent use in signed/unsigned expressions makes this generate errors
    static const uint32_t NJ_MAX_STACK_ENTRY = 4096;
#else
#define NJ_MAX_STACK_ENTRY 4096
#endif
    static const int NJ_ALIGN_STACK = 8;

    typedef uint32_t NIns;                // REQ: Instruction count
    typedef uint64_t RegisterMask;        // REQ: Large enough to hold LastRegNum-FirstRegNum bits
#define _rmask_(r)        (1LL<<(r))

    typedef uint32_t Register;            // REQ: Register identifiers
    // Register numbers for Native code generator
    static const Register
        ZERO = { 0 },
        AT = { 1 },
        V0 = { 2 },
        V1 = { 3 },
        A0 = { 4 },
        A1 = { 5 },
        A2 = { 6 },
        A3 = { 7 },

        T0 = { 8 },
        T1 = { 9 },
        T2 = { 10 },
        T3 = { 11 },
        T4 = { 12 },
        T5 = { 13 },
        T6 = { 14 },
        T7 = { 15 },

        S0 = { 16 },
        S1 = { 17 },
        S2 = { 18 },
        S3 = { 19 },
        S4 = { 20 },
        S5 = { 21 },
        S6 = { 22 },
        S7 = { 23 },

        T8 = { 24 },
        T9 = { 25 },
        K0 = { 26 },
        K1 = { 27 },
        GP = { 28 },
        SP = { 29 },
        FP = { 30 },
        RA = { 31 },

        F0 = { 32 },
        F1 = { 33 },
        F2 = { 34 },
        F3 = { 35 },
        F4 = { 36 },
        F5 = { 37 },
        F6 = { 38 },
        F7 = { 39 },

        F8 = { 40 },
        F9 = { 41 },
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

        // FP register aliases
        FV0 = F0,
        FV1 = F2,
        FA0 = F12,
        FA1 = F14,
        FT0 = F4,
        FT1 = F6,
        FT2 = F8,
        FT3 = F10,
        FT4 = F16,
        FT5 = F18,
        FS0 = F20,
        FS1 = F22,
        FS2 = F24,
        FS3 = F26,
        FS4 = F28,
        FS5 = F30,

        deprecated_UnknownReg = { 127 };    // XXX: remove eventually, see bug 538924

    static const uint32_t FirstRegNum = ZERO;
    static const uint32_t LastRegNum = F31;
}

#define NJ_USE_UINT32_REGISTER 1
#include "NativeCommon.h"

namespace nanojit {
    // REQ: register names
    verbose_only(extern const char* regNames[];)

    // REQ: Bytes of icache to flush after Assembler::patch
    const size_t LARGEST_BRANCH_PATCH = 2 * sizeof(NIns);

    // REQ: largest value passed to underrunProtect
    static const int LARGEST_UNDERRUN_PROT = 32;

    // REQ: Number of callee saved registers
#ifdef FPCALLEESAVED
    static const int NumSavedRegs = 14;
#else
    static const int NumSavedRegs = 8;
#endif

    // REQ: Callee saved registers
    const RegisterMask SavedRegs =
#ifdef FPCALLEESAVED
                    _rmask_(FS0) | _rmask_(FS1) | _rmask_(FS2) |
                    _rmask_(FS3) | _rmask_(FS4) | _rmask_(FS5) |
#endif
                    _rmask_(S0) | _rmask_(S1) | _rmask_(S2) | _rmask_(S3) |
                    _rmask_(S4) | _rmask_(S5) | _rmask_(S6) | _rmask_(S7);

    // REQ: General purpose registers
    static const RegisterMask GpRegs =
                    _rmask_(V0) | _rmask_(V1) |
                    _rmask_(A0) | _rmask_(A1) | _rmask_(A2) | _rmask_(A3) |
                    _rmask_(S0) | _rmask_(S1) | _rmask_(S2) | _rmask_(S3) |
                    _rmask_(S4) | _rmask_(S5) | _rmask_(S6) | _rmask_(S7) |
                    _rmask_(T0) | _rmask_(T1) | _rmask_(T2) | _rmask_(T3) |
                    _rmask_(T4) | _rmask_(T5) | _rmask_(T6) | _rmask_(T7) |
                    _rmask_(T8) | _rmask_(T9);

    // REQ: Floating point registers
    static const RegisterMask FpRegs =
#ifdef FPCALLEESAVED
                    _rmask_(FS0) | _rmask_(FS1) | _rmask_(FS2) |
                    _rmask_(FS3) | _rmask_(FS4) | _rmask_(FS5) |
#endif
                    _rmask_(FV0) | _rmask_(FV1) |
                    _rmask_(FA0) | _rmask_(FA1) |
                    _rmask_(FT0) | _rmask_(FT1) | _rmask_(FT2) |
                    _rmask_(FT3) | _rmask_(FT4) | _rmask_(FT5);

    static const RegisterMask AllowableFlagRegs = GpRegs;        // REQ: Registers that can hold flag results FIXME

    static inline bool IsFpReg(Register r)
    {
        return (_rmask_(r) & FpRegs) != 0;
    }

    static inline bool IsGpReg(Register r)
    {
        return (_rmask_(r) & GpRegs) != 0;
    }

#define GPR(r) ((r)&31)
#define FPR(r) ((r)&31)

// REQ: Platform specific declarations to include in Stats structure
#define DECLARE_PLATFORM_STATS()

// REQ: Platform specific declarations to include in Assembler class
#define DECLARE_PLATFORM_ASSEMBLER()                                    \
    const static Register argRegs[4];                                   \
    const static Register retRegs[2];                                   \
    void nativePageSetup(void);                                         \
    void nativePageReset(void);                                         \
    void underrunProtect(int bytes);                                    \
    bool hardenNopInsertion(const Config& c) { return false; }          \
    NIns *_nSlot;                                                       \
    NIns *_nExitSlot;                                                   \
    int max_out_args;                                                   \
    Register ovreg;                                                     \
                                                                        \
    void asm_ldst(int op, Register r, int offset, Register b);          \
    void asm_ldst64(bool store, Register fr, int offset, Register b);   \
    void asm_store_imm64(LIns *value, int dr, Register rbase);          \
    void asm_li32(Register r, int32_t imm);                             \
    void asm_li_d(Register fr, int32_t msw, int32_t lsw);               \
    void asm_li(Register r, int32_t imm);                               \
    void asm_j(NIns*, bool bdelay);                                     \
    void asm_cmp(LOpcode condop, LIns *a, LIns *b, Register cr);        \
    void asm_move(Register d, Register s);                              \
    void asm_regarg(ArgType ty, LIns* p, Register r);                   \
    void asm_stkarg(LIns* arg, int stkd);                               \
    void asm_arg(ArgType ty, LIns* arg, Register& r, Register& fr, int& stkd);     \
    void asm_arg_64(LIns* arg, Register& r, Register& fr, int& stkd);   \
    NIns *asm_branchtarget(NIns*);                                      \
    NIns *asm_bxx(bool, LOpcode, Register, Register, NIns*);

// REQ: Platform specific declarations to include in RegAlloc class
#define DECLARE_PLATFORM_REGALLOC()

// REQ:
#define swapptrs()  do {                                                \
        NIns* _tins = _nIns; _nIns = _nExitIns; _nExitIns = _tins;      \
        NIns* _nslot = _nSlot; _nSlot = _nExitSlot; _nExitSlot = _nslot; \
    } while (0)

#define TODO(x) do { verbose_only(avmplus::AvmLog(#x);) NanoAssertMsgf(false, "%s", #x); } while (0)
#ifdef MIPSDEBUG
#define TAG(fmt, ...) do { debug_only(verbose_outputf("             # MIPS: " fmt, ##__VA_ARGS__);) } while (0)
#else
#define TAG(fmt, ...) do { } while (0)
#endif

#define EMIT(ins, fmt, ...) do {                   \
        underrunProtect(4);                        \
        *(--_nIns) = (NIns) (ins);                 \
        debug_only(codegenBreak(_nIns);)           \
        asm_output(fmt, ##__VA_ARGS__);            \
    } while (0)

// Emit code in trampoline/literal area
// Assume that underrunProtect has already been called
// This is a bit hacky...
#define TRAMP(ins, fmt, ...) do {                                       \
        verbose_only(                                                   \
                     NIns *save_nIns = _nIns; _nIns = _nSlot;           \
                     )                                                  \
        *_nSlot = (NIns)ins;                                            \
        debug_only(codegenBreak(_nSlot);)                               \
        _nSlot++;                                                       \
        verbose_only(setOutputForEOL("<= trampoline");)                 \
        asm_output(fmt, ##__VA_ARGS__);                                 \
        verbose_only(                                                   \
                     _nIns = save_nIns;                                 \
                     )                                                  \
    } while (0)

#define MR(d, s)        asm_move(d, s)

// underrun guarantees that there is always room to insert a jump and branch delay slot
#define JMP(t)          asm_j(t, true)

// Opcodes: bits 31..26
#define OP_SPECIAL      0x00
#define OP_REGIMM       0x01
#define OP_J            0x02
#define OP_JAL          0x03
#define OP_BEQ          0x04
#define OP_BNE          0x05
#define OP_ADDIU        0x09
#define OP_SLTIU        0x0b
#define OP_ANDI         0x0c
#define OP_ORI          0x0d
#define OP_XORI         0x0e
#define OP_LUI          0x0f
#define OP_COP1         0x11
#define OP_COP1X        0x13
#define OP_SPECIAL2     0x1c
#define OP_LB           0x20
#define OP_LH           0x21
#define OP_LW           0x23
#define OP_LBU          0x24
#define OP_LHU          0x25
#define OP_SB           0x28
#define OP_SH           0x29
#define OP_SW           0x2b
#define OP_LWC1         0x31
#define OP_LDC1         0x35
#define OP_SWC1         0x39
#define OP_SDC1         0x3d

// REGIMM: bits 20..16
#define REGIMM_BLTZ     0x00
#define REGIMM_BGEZ     0x01

// COP1: bits 25..21
#define COP1_ADD        0x00
#define COP1_SUB        0x01
#define COP1_MUL        0x02
#define COP1_DIV        0x03
#define COP1_MOV        0x06
#define COP1_NEG        0x07
#define COP1_BC         0x08
#define COP1_TRUNCW     0x0d
#define COP1_CVTD       0x21

// COP1X: bits 5..0
#define COP1X_LDXC1     0x01
#define COP1X_SDXC1     0x09

// SPECIAL: bits 5..0
#define SPECIAL_SLL     0x00
#define SPECIAL_MOVCI   0x01
#define SPECIAL_SRL     0x02
#define SPECIAL_SRA     0x03
#define SPECIAL_SLLV    0x04
#define SPECIAL_SRLV    0x06
#define SPECIAL_SRAV    0x07
#define SPECIAL_JR      0x08
#define SPECIAL_JALR    0x09
#define SPECIAL_MOVN    0x0b
#define SPECIAL_MFHI    0x10
#define SPECIAL_MFLO    0x12
#define SPECIAL_MULT    0x18
#define SPECIAL_ADDU    0x21
#define SPECIAL_SUBU    0x23
#define SPECIAL_AND     0x24
#define SPECIAL_OR      0x25
#define SPECIAL_XOR     0x26
#define SPECIAL_NOR     0x27
#define SPECIAL_SLT     0x2a
#define SPECIAL_SLTU    0x2b

// SPECIAL2: bits 5..0
#define SPECIAL2_MUL    0x02

// FORMAT: bits 25..21
#define FMT_S           0x10
#define FMT_D           0x11
#define FMT_W           0x14
#define FMT_L           0x15
#define FMT_PS          0x16

// CONDITION: bits 4..0
#define COND_F          0x0
#define COND_UN         0x1
#define COND_EQ         0x2
#define COND_UEQ        0x3
#define COND_OLT        0x4
#define COND_ULT        0x5
#define COND_OLE        0x6
#define COND_ULE        0x7
#define COND_SF         0x8
#define COND_NGLE       0x9
#define COND_SEQ        0xa
#define COND_NGL        0xb
#define COND_LT         0xc
#define COND_NGE        0xd
#define COND_LE         0xe
#define COND_NGT        0xf

// Helper definitions to encode different classes of MIPS instructions
// Parameters are in instruction order

#define R_FORMAT(op, rs, rt, rd, re, func)                              \
    (((op)<<26)|(GPR(rs)<<21)|(GPR(rt)<<16)|(GPR(rd)<<11)|((re)<<6)|(func))

#define I_FORMAT(op, rs, rt, simm)                              \
    (((op)<<26)|(GPR(rs)<<21)|(GPR(rt)<<16)|((simm)&0xffff))

#define J_FORMAT(op, index)                     \
    (((op)<<26)|(index))

#define U_FORMAT(op, rs, rt, uimm)                              \
    (((op)<<26)|(GPR(rs)<<21)|(GPR(rt)<<16)|((uimm)&0xffff))

#define F_FORMAT(op, ffmt, ft, fs, fd, func)                            \
    (((op)<<26)|((ffmt)<<21)|(FPR(ft)<<16)|(FPR(fs)<<11)|(FPR(fd)<<6)|(func))

#define oname(op) Assembler::oname[op]
#define cname(cond) Assembler::cname[cond]
#define fname(ffmt) Assembler::fname[ffmt]
#define fpn(fr) gpn(fr)

#define BOFFSET(targ)    (uint32_t(targ - (_nIns+1)))

#define LDST(op, rt, offset, base)                                      \
    do { count_misc(); EMIT(I_FORMAT(op, base, rt, offset),             \
                            "%s %s, %d(%s)", oname[op], gpn(rt), offset, gpn(base)); } while (0)

#define BX(op, rs, rt, targ)                                            \
    do { count_br(); EMIT(I_FORMAT(op, rs, rt, BOFFSET(targ)),          \
                          "%s %s, %s, %p", oname[op], gpn(rt), gpn(rs), targ); } while (0)

// MIPS instructions
// Parameters are in "assembler" order
#define ADDIU(rt, rs, simm)                                             \
    do { count_alu(); EMIT(I_FORMAT(OP_ADDIU, rs, rt, simm),            \
                           "addiu %s, %s, %d", gpn(rt), gpn(rs), simm); } while (0)

#define trampADDIU(rt, rs, simm)                                        \
    do { count_alu(); TRAMP(I_FORMAT(OP_ADDIU, rs, rt, simm),           \
                            "addiu %s, %s, %d", gpn(rt), gpn(rs), simm); } while (0)

#define ADDU(rd, rs, rt)                                                \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_ADDU), \
                           "addu %s, %s, %s", gpn(rd), gpn(rs), gpn(rt)); } while (0)

#define AND(rd, rs, rt)                                                 \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_AND), \
                           "and %s, %s, %s", gpn(rd), gpn(rs), gpn(rt)); } while (0)

#define ANDI(rt, rs, uimm)                                              \
    do { count_alu(); EMIT(U_FORMAT(OP_ANDI, rs, rt, uimm),             \
                           "andi %s, %s, 0x%x", gpn(rt), gpn(rs), ((uimm)&0xffff)); } while (0)

#define BC1F(targ)                                                      \
    do { count_br(); EMIT(I_FORMAT(OP_COP1, COP1_BC, 0, BOFFSET(targ)), \
                          "bc1f %p", targ); } while (0)

#define BC1T(targ)                                                      \
    do { count_br(); EMIT(I_FORMAT(OP_COP1, COP1_BC, 1, BOFFSET(targ)), \
                          "bc1t %p", targ); } while (0)

#define B(targ)                 BX(OP_BEQ, ZERO, ZERO, targ)
#define BEQ(rs, rt, targ)       BX(OP_BEQ, rs, rt, targ)
#define BNE(rs, rt, targ)       BX(OP_BNE, rs, rt, targ)
#define BLEZ(rs, targ)          BX(OP_BLEZ, rs, ZERO, targ)
#define BGTZ(rs, targ)          BX(OP_BGTZ, rs, ZERO, targ)
#define BGEZ(rs, targ)          BX(OP_REGIMM, rs, REGIMM_BGEZ, targ)
#define BLTZ(rs, targ)          BX(OP_REGIMM, rs, REGIMM_BLTZ, targ)

#define JINDEX(dest) ((uint32_t(dest)>>2)&0x03ffffff)

#define J(dest)                                             \
    do { count_jmp(); EMIT(J_FORMAT(OP_J, JINDEX(dest)),    \
                           "j %p", dest); } while (0)

#define trampJ(dest)                                        \
    do { count_jmp(); TRAMP(J_FORMAT(OP_J, JINDEX(dest)),   \
                            "j %p", dest); } while (0)

#define JAL(dest)                                           \
    do { count_jmp(); EMIT(J_FORMAT(OP_JAL, JINDEX(dest)),  \
                           "jal %p", dest); } while (0)

#define JALR(rs)                                                        \
    do { count_jmp(); EMIT(R_FORMAT(OP_SPECIAL, rs, 0, RA, 0, SPECIAL_JALR), \
                           "jalr %s", gpn(rs)); } while (0)

#define JR(rs)                                                            \
    do { count_jmp(); EMIT(R_FORMAT(OP_SPECIAL, rs, 0, 0, 0, SPECIAL_JR), \
                           "jr %s", gpn(rs)); } while (0)
#define trampJR(rs)                                                     \
    do { count_jmp(); TRAMP(R_FORMAT(OP_SPECIAL, rs, 0, 0, 0, SPECIAL_JR), \
                            "jr %s", gpn(rs)); } while (0)

#define LB(rt, offset, base)                    \
    LDST(OP_LB, rt, offset, base)

#define LH(rt, offset, base)                    \
    LDST(OP_LH, rt, offset, base)

#define LUI(rt, uimm)                                                   \
    do { count_alu(); EMIT(U_FORMAT(OP_LUI, 0, rt, uimm),               \
                           "lui %s, 0x%x", gpn(rt), ((uimm)&0xffff)); } while (0)

#define LW(rt, offset, base)                    \
    LDST(OP_LW, rt, offset, base)

#define MFHI(rd)                                                        \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, 0, 0, rd, 0, SPECIAL_MFHI), \
                           "mfhi %s", gpn(rd)); } while (0)

#define MFLO(rd)                                                        \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, 0, 0, rd, 0, SPECIAL_MFLO), \
                           "mflo %s", gpn(rd)); } while (0)

#define MUL(rd, rs, rt)                                                 \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL2, rs, rt, rd, 0, SPECIAL2_MUL), \
                           "mul %s, %s, %s", gpn(rd), gpn(rs), gpn(rt)); } while (0)

#define MULT(rs, rt)                                                    \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, 0, 0, SPECIAL_MULT), \
                           "mult %s, %s", gpn(rs), gpn(rt)); } while (0)

#define MOVE(rd, rs)                                                    \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, ZERO, rd, 0, SPECIAL_ADDU), \
                           "move %s, %s", gpn(rd), gpn(rs)); } while (0)

#define MOVN(rd, rs, rt)                                                \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_MOVN), \
                           "movn %s, %s, %s", gpn(rd), gpn(rs), gpn(rt)); } while (0)

#define NEGU(rd, rt)                                                    \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, ZERO, rt, rd, 0, SPECIAL_SUBU), \
                           "negu %s, %s", gpn(rd), gpn(rt)); } while (0)

#define NOP()                                                           \
    do { count_misc(); EMIT(R_FORMAT(OP_SPECIAL, 0, 0, 0, 0, SPECIAL_SLL), \
                            "nop"); } while (0)

#define trampNOP()                                                      \
    do { count_misc(); TRAMP(R_FORMAT(OP_SPECIAL, 0, 0, 0, 0, SPECIAL_SLL), \
                             "nop"); } while (0)

#define NOR(rd, rs, rt)                                                 \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_NOR), \
                           "nor %s, %s, %s", gpn(rd), gpn(rs), gpn(rt)); } while (0)

#define NOT(rd, rs)                                                     \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, ZERO, rd, 0, SPECIAL_NOR), \
                           "not %s, %s", gpn(rd), gpn(rs)); } while (0)

#define OR(rd, rs, rt)                                                  \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_OR), \
                           "or %s, %s, %s", gpn(rd), gpn(rs), gpn(rt)); } while (0)

#define ORI(rt, rs, uimm)                                               \
    do { count_alu(); EMIT(U_FORMAT(OP_ORI, rs, rt, uimm),              \
                           "ori %s, %s, 0x%x", gpn(rt), gpn(rs), ((uimm)&0xffff)); } while (0)

#define SLTIU(rt, rs, simm)                                             \
    do { count_alu(); EMIT(I_FORMAT(OP_SLTIU, rs, rt, simm),            \
                           "sltiu %s, %s, %d", gpn(rt), gpn(rs), simm); } while (0)

#define SLT(rd, rs, rt)                                                 \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_SLT), \
                           "slt %s, %s, %s", gpn(rd), gpn(rs), gpn(rt)); } while (0)

#define SLTU(rd, rs, rt)                                                \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_SLTU), \
                           "sltu %s, %s, %s", gpn(rd), gpn(rs), gpn(rt)); } while (0)

#define SLL(rd, rt, sa)                                                 \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, 0, rt, rd, sa, SPECIAL_SLL), \
                           "sll %s, %s, %d", gpn(rd), gpn(rt), sa); } while (0)

#define SLLV(rd, rt, rs)                                                \
    do { count_misc(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_SLLV), \
                            "sllv %s, %s, %s", gpn(rd), gpn(rt), gpn(rs)); } while (0)

#define SRA(rd, rt, sa)                                                 \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, 0, rt, rd, sa, SPECIAL_SRA), \
                           "sra %s, %s, %d", gpn(rd), gpn(rt), sa); } while (0)

#define SRAV(rd, rt, rs)                                                \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_SRAV), \
                           "srav %s, %s, %s", gpn(rd), gpn(rt), gpn(rs)); } while (0)

#define SRL(rd, rt, sa)                                                 \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, 0, rt, rd, sa, SPECIAL_SRL), \
                           "srl %s, %s, %d", gpn(rd), gpn(rt), sa); } while (0)

#define SRLV(rd, rt, rs)                                                \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_SRLV), \
                           "srlv %s, %s, %s", gpn(rd), gpn(rt), gpn(rs)); } while (0)

#define SUBU(rd, rs, rt)                                                \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_SUBU), \
                           "subu %s, %s, %s", gpn(rd), gpn(rs), gpn(rt)); } while (0)

#define SW(rt, offset, base)                    \
    LDST(OP_SW, rt, offset, base)

#define XOR(rd, rs, rt)                                                 \
    do { count_alu(); EMIT(R_FORMAT(OP_SPECIAL, rs, rt, rd, 0, SPECIAL_XOR), \
                           "xor %s, %s, %s", gpn(rd), gpn(rs), gpn(rt)); } while (0)

#define XORI(rt, rs, uimm)                                              \
    do { count_alu(); EMIT(U_FORMAT(OP_XORI, rs, rt, uimm),             \
                           "xori %s, %s, 0x%x", gpn(rt), gpn(rs), ((uimm)&0xffff)); } while (0)


/* FPU instructions */
#ifdef NJ_SOFTFLOAT_SUPPORTED

#if !defined(__mips_soft_float) || __mips_soft_float != 1
#error NJ_SOFTFLOAT_SUPPORTED defined but not compiled with -msoft-float
#endif

#define LWC1(ft, offset, base)  NanoAssertMsg(0, "softfloat LWC1")
#define SWC1(ft, offset, base)  NanoAssertMsg(0, "softfloat SWC1")
#define LDC1(ft, offset, base)  NanoAssertMsg(0, "softfloat LDC1")
#define SDC1(ft, offset, base)  NanoAssertMsg(0, "softfloat SDC1")
#define LDXC1(fd, index, base)  NanoAssertMsg(0, "softfloat LDXC1")
#define SDXC1(fs, index, base)  NanoAssertMsg(0, "softfloat SDXC1")

#define MFC1(rt, fs)            NanoAssertMsg(0, "softfloat MFC1")
#define MTC1(rt, fs)            NanoAssertMsg(0, "softfloat MTC1")
#define MOVF(rt, fs, cc)        NanoAssertMsg(0, "softfloat MOVF")
#define CVT_D_W(fd, fs)         NanoAssertMsg(0, "softfloat CVT_D_W")
#define C_EQ_D(fs, ft)          NanoAssertMsg(0, "softfloat C_EQ_D")
#define C_LE_D(fs, ft)          NanoAssertMsg(0, "softfloat C_LE_D")
#define C_LT_D(fs, ft)          NanoAssertMsg(0, "softfloat C_LT_D")
#define ADD_D(fd, fs, ft)       NanoAssertMsg(0, "softfloat ADD_D")
#define DIV_D(fd, fs, ft)       NanoAssertMsg(0, "softfloat DIV_D")
#define MOV_D(fd, fs)           NanoAssertMsg(0, "softfloat MOV_D")
#define MUL_D(fd, fs, ft)       NanoAssertMsg(0, "softfloat MUL_D")
#define NEG_D(fd, fs)           NanoAssertMsg(0, "softfloat NEG_D")
#define SUB_D(fd, fs, ft)       NanoAssertMsg(0, "softfloat SUB_D")
#define TRUNC_W_D(fd,fs)        NanoAssertMsg(0, "softfloat TRUNC_W_D")

#else

#if defined(__mips_soft_float) && __mips_soft_float != 0
#error compiled with -msoft-float but NJ_SOFTFLOAT_SUPPORTED not defined
#endif

#define FOP_FMT2(ffmt, fd, fs, func, name)                              \
    do { count_fpu(); EMIT(F_FORMAT(OP_COP1, ffmt, 0, fs, fd, func),    \
                           "%s.%s %s, %s", name, fname[ffmt], fpn(fd), fpn(fs)); } while (0)

#define FOP_FMT3(ffmt, fd, fs, ft, func, name)                          \
    do { count_fpu(); EMIT(F_FORMAT(OP_COP1, ffmt, ft, fs, fd, func),   \
                           "%s.%s %s, %s, %s", name, fname[ffmt], fpn(fd), fpn(fs), fpn(ft)); } while (0)

#define C_COND_FMT(cond, ffmt, fs, ft)                                  \
    do { count_fpu(); EMIT(F_FORMAT(OP_COP1, ffmt, ft, fs, 0, 0x30|(cond)), \
                           "c.%s.%s %s, %s", cname[cond], fname[ffmt], fpn(fs), fpn(ft)); } while (0)

#define MFC1(rt, fs)                                                    \
    do { count_fpu(); EMIT(F_FORMAT(OP_COP1, 0, rt, fs, 0, 0),          \
                           "mfc1 %s, %s", gpn(rt), fpn(fs)); } while (0)

#define MTC1(rt, fs)                                                    \
    do { count_fpu(); EMIT(F_FORMAT(OP_COP1, 4, rt, fs, 0, 0),          \
                           "mtc1 %s, %s", gpn(rt), fpn(fs)); } while (0)

#define MOVF(rd, rs, cc)                                                \
    do { count_fpu(); EMIT(R_FORMAT(OP_SPECIAL, rs, (cc)<<2, rd, 0, SPECIAL_MOVCI), \
                           "movf %s, %s, $fcc%d", gpn(rd), gpn(rs), cc); } while (0)

#define CVT_D_W(fd, fs)                                                 \
    do { count_fpu(); EMIT(F_FORMAT(OP_COP1, FMT_W, 0, fs, fd, COP1_CVTD), \
                           "cvt.d.w %s, %s", fpn(fd), fpn(fs)); } while (0)

#define TRUNC_W_D(fd, fs)                                               \
    do { count_fpu(); EMIT(F_FORMAT(OP_COP1, FMT_D, 0, fs, fd, COP1_TRUNCW), \
                           "trunc.w.d %s, %s", fpn(fd), fpn(fs)); } while (0)


#define LWC1(ft, offset, base)  LDST(OP_LWC1, ft, offset, base)
#define SWC1(ft, offset, base)  LDST(OP_SWC1, ft, offset, base)
#define LDC1(ft, offset, base)  LDST(OP_LDC1, ft, offset, base)
#define SDC1(ft, offset, base)  LDST(OP_SDC1, ft, offset, base)
#define LDXC1(fd, index, base)                                          \
    do { count_fpu(); EMIT(R_FORMAT(OP_COP1X, base, index, 0, fd, COP1X_LDXC1), \
                           "ldxc1 %s, %s(%s)", fpn(fd), gpn(index), gpn(base)); } while (0)
#define SDXC1(fs, index, base)                                          \
    do { count_fpu(); EMIT(R_FORMAT(OP_COP1X, base, index, fs, 0, COP1X_SDXC1), \
                           "sdxc1 %s, %s(%s)", fpn(fs), gpn(index), gpn(base)); } while (0)

#define C_EQ_D(fs, ft)          C_COND_FMT(COND_EQ, FMT_D, fs, ft)
#define C_LE_D(fs, ft)          C_COND_FMT(COND_LE, FMT_D, fs, ft)
#define C_LT_D(fs, ft)          C_COND_FMT(COND_LT, FMT_D, fs, ft)
#define ADD_D(fd, fs, ft)       FOP_FMT3(FMT_D, fd, fs, ft, COP1_ADD, "add")
#define DIV_D(fd, fs, ft)       FOP_FMT3(FMT_D, fd, fs, ft, COP1_DIV, "div")
#define MOV_D(fd, fs)           FOP_FMT2(FMT_D, fd, fs, COP1_MOV, "mov")
#define MUL_D(fd, fs, ft)       FOP_FMT3(FMT_D, fd, fs, ft, COP1_MUL, "mul")
#define NEG_D(fd, fs)           FOP_FMT2(FMT_D, fd, fs, COP1_NEG, "neg")
#define SUB_D(fd, fs, ft)       FOP_FMT3(FMT_D, fd, fs, ft, COP1_SUB, "sub")
#endif

}
#endif // __nanojit_NativeMIPS__
