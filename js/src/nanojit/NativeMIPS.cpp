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

#include "nanojit.h"

#if defined FEATURE_NANOJIT && defined NANOJIT_MIPS

namespace nanojit
{
#ifdef NJ_VERBOSE
    const char *regNames[] = {
        "$zr", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
        "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
        "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
        "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",

        "$f0",  "$f1",  "$f2",  "$f3",  "$f4",  "$f5",  "$f6",  "$f7",
        "$f8",  "$f9",  "$f10", "$f11", "$f12", "$f13", "$f14", "$f15",
        "$f16", "$f17", "$f18", "$f19", "$f20", "$f21", "$f22", "$f23",
        "$f24", "$f25", "$f26", "$f27", "$f28", "$f29", "$f30", "$f31"
    };

    const char *cname[16] = {
        "f",    "un",   "eq",   "ueq",
        "olt",  "ult",  "ole",  "ule",
        "sf",   "ngle", "seq",  "ngl",
        "lt",   "nge",  "le",   "ngt"
    };

    const char *fname[32] = {
        "resv", "resv", "resv", "resv",
        "resv", "resv", "resv", "resv",
        "resv", "resv", "resv", "resv",
        "resv", "resv", "resv", "resv",
        "s",    "d",    "resv", "resv",
        "w",    "l",    "ps",   "resv",
        "resv", "resv", "resv", "resv",
        "resv", "resv", "resv", "resv",
    };

    const char *oname[64] = {
        "special", "regimm", "j",    "jal",   "beq",      "bne",  "blez",  "bgtz",
        "addi",    "addiu",  "slti", "sltiu", "andi",     "ori",  "xori",  "lui",
        "cop0",    "cop1",   "cop2", "cop1x", "beql",     "bnel", "blezl", "bgtzl",
        "resv",    "resv",   "resv", "resv",  "special2", "jalx", "resv",  "special3",
        "lb",      "lh",     "lwl",  "lw",    "lbu",      "lhu",  "lwr",   "resv",
        "sb",      "sh",     "swl",  "sw",    "resv",     "resv", "swr",   "cache",
        "ll",      "lwc1",   "lwc2", "pref",  "resv",     "ldc1", "ldc2",  "resv",
        "sc",      "swc1",   "swc2", "resv",  "resv",     "sdc1", "sdc2",  "resv",
    };
#endif

    const Register Assembler::argRegs[] = { A0, A1, A2, A3 };
    const Register Assembler::retRegs[] = { V0, V1 };
    const Register Assembler::savedRegs[NumSavedRegs] = {
        S0, S1, S2, S3, S4, S5, S6, S7,
#ifdef FPCALLEESAVED
        FS0, FS1, FS2, FS3, FS4, FS5
#endif
    };

#define USE(x) (void)x
#define BADOPCODE(op) NanoAssertMsgf(false, "unexpected opcode %s", lirNames[op])

    // This function will get will get optimised by the compiler into a known value
    static inline bool isLittleEndian(void)
    {
        const union {
            uint32_t      ival;
            unsigned char cval[4];
        } u = { 1 };
        return u.cval[0] == 1;
    }

    // offsets to most/least significant parts of 64bit data in memory
    // These functions will get optimised by the compiler into a known value
    static inline int mswoff(void) {
        return isLittleEndian() ? 4 : 0;
    }

    static inline int lswoff(void) {
        return isLittleEndian() ? 0 : 4;
    }

    static inline Register mswregpair(Register r) {
        return Register(r + (isLittleEndian() ? 1 : 0));
    }

    static inline Register lswregpair(Register r) {
        return Register(r + (isLittleEndian() ? 0 : 1));
    }

// These variables affect the code generator
// They can be defined as constants and the compiler will remove
// the unused paths through dead code elimination
// Alternatively they can be defined as variables which will allow
// the exact code generated to be determined at runtime
//
//  cpu_has_fpu        CPU has fpu
//  cpu_has_movn    CPU has movn
//  cpu_has_cmov    CPU has movf/movn instructions
//  cpu_has_lsdc1   CPU has ldc1/sdc1 instructions
//  cpu_has_lsxdc1    CPU has ldxc1/sdxc1 instructions
//  cpu_has_fpuhazard hazard between c.xx.xx & bc1[tf]
//
// Currently the values are initialised bases on preprocessor definitions

#ifdef DEBUG
    // Don't allow the compiler to eliminate dead code for debug builds
    #define _CONST
#else
    #define _CONST const
#endif

#ifdef NJ_SOFTFLOAT
    _CONST bool cpu_has_fpu = false;
#else
    _CONST bool cpu_has_fpu = true;
#endif

#if (__mips==4 || __mips==32 || __mips==64)
    _CONST bool cpu_has_cmov = true;
#else
    _CONST bool cpu_has_cmov = false;
#endif

#if __mips != 1
    _CONST bool cpu_has_lsdc1 = true;
#else
    _CONST bool cpu_has_lsdc1 = false;
#endif

#if (__mips==32 || __mips==64) && __mips_isa_rev>=2
    _CONST bool cpu_has_lsdxc1 = true;
#else
    _CONST bool cpu_has_lsdxc1 = false;
#endif

#if (__mips==1 || __mips==2 || __mips==3)
    _CONST bool cpu_has_fpuhazard = true;
#else
    _CONST bool cpu_has_fpuhazard = false;
#endif
#undef _CONST

    /* Support routines */

    debug_only (
                // break to debugger when generating code to this address
                static NIns *breakAddr;
                static void codegenBreak(NIns *genAddr)
                {
                    NanoAssert (breakAddr != genAddr);
                }
    )

    // Equivalent to assembler %hi(), %lo()
    uint16_t hi(uint32_t v)
    {
        uint16_t r = v >> 16;
        if ((int16_t)(v) < 0)
            r += 1;
        return r;
    }

    int16_t lo(uint32_t v)
    {
        int16_t r = v;
        return r;
    }

    void Assembler::asm_li32(Register r, int32_t imm)
    {
        // general case generating a full 32-bit load
        ADDIU(r, r, lo(imm));
        LUI(r, hi(imm));
    }

    void Assembler::asm_li(Register r, int32_t imm)
    {
#if !PEDANTIC
        if (isU16(imm)) {
            ORI(r, ZERO, imm);
            return;
        }
        if (isS16(imm)) {
            ADDIU(r, ZERO, imm);
            return;
        }
        if ((imm & 0xffff) == 0) {
            LUI(r, uint32_t(imm) >> 16);
            return;
        }
#endif
        asm_li32(r, imm);
    }

    // 64 bit immediate load to a register pair
    void Assembler::asm_li_d(Register r, int32_t msw, int32_t lsw)
    {
        if (IsFpReg(r)) {
            NanoAssert(cpu_has_fpu);
            // li   $at,lsw         # iff lsw != 0
            // mtc1 $at,$r          # may use $0 instead of $at
            // li   $at,msw         # iff (msw != 0) && (msw != lsw)
            // mtc1 $at,$(r+1)      # may use $0 instead of $at
            if (msw == 0)
                MTC1(ZERO, r+1);
            else {
                MTC1(AT, r+1);
                // If the MSW & LSW values are different, reload AT
                if (msw != lsw)
                    asm_li(AT, msw);
            }
            if (lsw == 0)
                MTC1(ZERO, r);
            else {
                MTC1(AT, r);
                asm_li(AT, lsw);
            }
        }
        else {
            /*
             * li $r.lo, lsw
             * li $r.hi, msw   # will be converted to move $f.hi,$f.lo if (msw==lsw)
             */
            if (msw == lsw)
                MOVE(mswregpair(r), lswregpair(r));
            else
                asm_li(mswregpair(r), msw);
            asm_li(lswregpair(r), lsw);
        }
    }

    void Assembler::asm_move(Register d, Register s)
    {
        MOVE(d, s);
    }

    // General load/store operation
    void Assembler::asm_ldst(int op, Register rt, int dr, Register rbase)
    {
#if !PEDANTIC
        if (isS16(dr)) {
            LDST(op, rt, dr, rbase);
            return;
        }
#endif

        // lui AT,hi(d)
        // addu AT,rbase
        // ldst rt,lo(d)(AT)
        LDST(op, rt, lo(dr), AT);
        ADDU(AT, AT, rbase);
        LUI(AT, hi(dr));
    }

    void Assembler::asm_ldst64(bool store, Register r, int dr, Register rbase)
    {
#if !PEDANTIC
        if (isS16(dr) && isS16(dr+4)) {
            if (IsGpReg(r)) {
                LDST(store ? OP_SW : OP_LW, r+1, dr+4, rbase);
                LDST(store ? OP_SW : OP_LW, r,   dr, rbase);
            }
            else {
                NanoAssert(cpu_has_fpu);
                // NanoAssert((dr & 7) == 0);
                if (cpu_has_lsdc1 && ((dr & 7) == 0)) {
                    // lsdc1 $fr,dr($rbase)
                    LDST(store ? OP_SDC1 : OP_LDC1, r, dr, rbase);
                }
                else {
                    // lswc1 $fr,  dr+LSWOFF($rbase)
                    // lswc1 $fr+1,dr+MSWOFF($rbase)
                    LDST(store ? OP_SWC1 : OP_LWC1, r+1, dr+mswoff(), rbase);
                    LDST(store ? OP_SWC1 : OP_LWC1, r,   dr+lswoff(), rbase);
                }
                return;
            }
        }
#endif

        if (IsGpReg(r)) {
            // lui   $at,%hi(d)
            // addu  $at,$rbase
            // ldsw  $r,  %lo(d)($at)
            // ldst  $r+1,%lo(d+4)($at)
            LDST(store ? OP_SW : OP_LW, r+1, lo(dr+4), AT);
            LDST(store ? OP_SW : OP_LW, r,   lo(dr), AT);
            ADDU(AT, AT, rbase);
            LUI(AT, hi(dr));
        }
        else {
            NanoAssert(cpu_has_fpu);
            if (cpu_has_lsdxc1) {
                // li     $at,dr
                // lsdcx1 $r,$at($rbase)
                if (store)
                    SDXC1(r, AT, rbase);
                else
                    LDXC1(r, AT, rbase);
                asm_li(AT, dr);
            }
            else if (cpu_has_lsdc1) {
                // lui    $at,%hi(dr)
                // addu   $at,$rbase
                // lsdc1  $r,%lo(dr)($at)
                LDST(store ? OP_SDC1 : OP_LDC1, r, lo(dr), AT);
                ADDU(AT, AT, rbase);
                LUI(AT, hi(dr));
            }
            else {
                // lui   $at,%hi(d)
                // addu  $at,$rbase
                // lswc1 $r,  %lo(d+LSWOFF)($at)
                // lswc1 $r+1,%lo(d+MSWOFF)($at)
                LDST(store ? OP_SWC1 : OP_LWC1, r+1, lo(dr+mswoff()), AT);
                LDST(store ? OP_SWC1 : OP_LWC1, r,   lo(dr+lswoff()), AT);
                ADDU(AT, AT, rbase);
                LUI(AT, hi(dr));
            }
        }
    }

    void Assembler::asm_store_imm64(LIns *value, int dr, Register rbase)
    {
        NanoAssert(value->isconstf());
        int32_t msw = value->imm64_1();
        int32_t lsw = value->imm64_0();

        // li $at,lsw                   # iff lsw != 0
        // sw $at,off+LSWOFF($rbase)    # may use $0 instead of $at
        // li $at,msw                   # iff (msw != 0) && (msw != lsw)
        // sw $at,off+MSWOFF($rbase)    # may use $0 instead of $at

        NanoAssert(isS16(dr) && isS16(dr+4));

        if (lsw == 0)
            SW(ZERO, dr+lswoff(), rbase);
        else {
            SW(AT, dr+lswoff(), rbase);
            if (msw != lsw)
                asm_li(AT, lsw);
        }
        if (msw == 0)
            SW(ZERO, dr+mswoff(), rbase);
        else {
            SW(AT, dr+mswoff(), rbase);
            // If the MSW & LSW values are different, reload AT
            if (msw != lsw)
                asm_li(AT, msw);
        }
    }

    void Assembler::asm_regarg(ArgType ty, LInsp p, Register r)
    {
        NanoAssert(deprecated_isKnownReg(r));
        if (ty == ARGTYPE_I || ty == ARGTYPE_U) {
            // arg goes in specific register
            if (p->isconst())
                asm_li(r, p->imm32());
            else {
                if (p->isUsed()) {
                    if (!p->deprecated_hasKnownReg()) {
                        // load it into the arg reg
                        int d = findMemFor(p);
                        if (p->isop(LIR_alloc))
                            ADDIU(r, FP, d);
                        else
                            asm_ldst(OP_LW, r, d, FP);
                    }
                    else
                        // it must be in a saved reg
                        MOVE(r, p->deprecated_getReg());
                }
                else {
                    // this is the last use, so fine to assign it
                    // to the scratch reg, it's dead after this point.
                    findSpecificRegFor(p, r);
                }
            }
        }
        else {
            // Other argument types unsupported
            NanoAssert(false);
        }
    }

    void Assembler::asm_stkarg(LInsp arg, int stkd)
    {
        bool isF64 = arg->isF64();
        Register rr;
        if (arg->isUsed() && (rr = arg->deprecated_getReg(), deprecated_isKnownReg(rr))) {
            // The argument resides somewhere in registers, so we simply need to
            // push it onto the stack.
            if (!cpu_has_fpu || !isF64) {
                NanoAssert(IsGpReg(rr));
                SW(rr, stkd, SP);
            }
            else {
                NanoAssert(cpu_has_fpu);
                NanoAssert(IsFpReg(rr));
                NanoAssert((stkd & 7) == 0);
                asm_ldst64(true, rr, stkd, SP);
            }
        }
        else {
            // The argument does not reside in registers, so we need to get some
            // memory for it and then copy it onto the stack.
            int d = findMemFor(arg);
            if (!isF64) {
                SW(AT, stkd, SP);
                if (arg->isop(LIR_alloc))
                    ADDIU(AT, FP, d);
                else
                    LW(AT, d, FP);
            }
            else {
                NanoAssert((stkd & 7) == 0);
                SW(AT, stkd+4, SP);
                LW(AT, d+4,    FP);
                SW(AT, stkd,   SP);
                LW(AT, d,      FP);
            }
        }
    }

    // Encode a 64-bit floating-point argument using the appropriate ABI.
    // This function operates in the same way as asm_arg, except that it will only
    // handle arguments where (ArgType)ty == ARGTYPE_F.
    void
    Assembler::asm_arg_64(LInsp arg, Register& r, Register& fr, int& stkd)
    {
        // The stack offset always be at least aligned to 4 bytes.
        NanoAssert((stkd & 3) == 0);
        // The only use for this function when we are using soft floating-point
        // is for LIR_qjoin.
        NanoAssert(cpu_has_fpu || arg->isop(LIR_qjoin));

        // O32 ABI requires that 64-bit arguments are aligned on even-numbered
        // registers, as A0:A1/FA0 or A2:A3/FA1. Use the stack offset to keep track
        // where we are
        if (stkd & 4) {
            if (stkd < 16) {
                r = nextreg(r);
                fr = nextreg(fr);
            }
            stkd += 4;
        }

        if (stkd < 16) {
            NanoAssert(fr == FA0 || fr == FA1 || fr == A2);
            if (fr == FA0 || fr == FA1)
                findSpecificRegFor(arg, fr);
            else {
                findSpecificRegFor(arg, FA1);
                // Move it to the integer pair
                Register fpupair = arg->getReg();
                Register intpair = fr;
                MFC1(mswregpair(intpair), nextreg(fpupair));       // Odd fpu register contains sign,expt,manthi
                MFC1(lswregpair(intpair), fpupair);                // Even fpu register contains mantlo
            }
            r = nextreg(nextreg(r));
            fr = nextreg(nextreg(fr));
        }
        else
            asm_stkarg(arg, stkd);

        stkd += 8;
    }

    /* Required functions */

#define FRAMESIZE        8
#define RA_OFFSET        4
#define FP_OFFSET        0

    void Assembler::asm_store32(LOpcode op, LIns *value, int dr, LIns *base)
    {
        Register rt, rbase;
        getBaseReg2(GpRegs, value, rt, GpRegs, base, rbase, dr);

        switch (op) {
        case LIR_sti:
            asm_ldst(OP_SW, rt, dr, rbase);
            break;
        case LIR_sts:
            asm_ldst(OP_SH, rt, dr, rbase);
            break;
        case LIR_stb:
            asm_ldst(OP_SB, rt, dr, rbase);
            break;
        default:
            BADOPCODE(op);
        }

        TAG("asm_store32(value=%p{%s}, dr=%d, base=%p{%s})",
            value, lirNames[value->opcode()], dr, base, lirNames[base->opcode()]);
    }

    void Assembler::asm_u2f(LIns *ins)
    {
        Register fr = deprecated_prepResultReg(ins, FpRegs);
        Register v = findRegFor(ins->oprnd1(), GpRegs);
        Register ft = registerAllocTmp(FpRegs & ~(rmask(fr)));    // allocate temporary register for constant

        // todo: support int value in memory, as per x86
        NanoAssert(deprecated_isKnownReg(v));

        // mtc1       $v,$ft
        // bgez       $v,1f
        //  cvt.d.w $fr,$ft
        // lui       $at,0x41f0    # (double)0x10000000LL = 0x41f0000000000000
        // mtc1    $0,$ft
        // mtc1    $at,$ft+1
        // add.d   $fr,$fr,$ft
        // 1:

        underrunProtect(6*4);   // keep branch and destination together
        NIns *here = _nIns;
        ADD_D(fr,fr,ft);
        MTC1(AT,ft+1);
        MTC1(ZERO,ft);
        LUI(AT,0x41f0);
        CVT_D_W(fr,ft);            // branch delay slot
        BGEZ(v,here);
        MTC1(v,ft);

        TAG("asm_u2f(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_f2i(LInsp ins)
    {
        NanoAssert(cpu_has_fpu);

        Register rr = deprecated_prepResultReg(ins, GpRegs);
        Register sr = findRegFor(ins->oprnd1(), FpRegs);
        // trunc.w.d $sr,$sr
        // mfc1 $rr,$sr
        MFC1(rr,sr);
        TRUNC_W_D(sr,sr);
        TAG("asm_u2f(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_qjoin(LIns *ins)
    {
        int d = findMemFor(ins);
        NanoAssert(d && isS16(d));
        LIns* lo = ins->oprnd1();
        LIns* hi = ins->oprnd2();

        Register r = findRegFor(hi, GpRegs);
        SW(r, d+mswoff(), FP);
        r = findRegFor(lo, GpRegs);             // okay if r gets recycled.
        SW(r, d+lswoff(), FP);
        deprecated_freeRsrcOf(ins);             // if we had a reg in use, flush it to mem

        TAG("asm_qjoin(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_fop(LIns *ins)
    {
        NanoAssert(cpu_has_fpu);
        if (cpu_has_fpu) {
            LInsp lhs = ins->oprnd1();
            LInsp rhs = ins->oprnd2();
            LOpcode op = ins->opcode();

            // rr = ra OP rb

            Register rr = deprecated_prepResultReg(ins, FpRegs);
            Register ra = findRegFor(lhs, FpRegs);
            Register rb = (rhs == lhs) ? ra : findRegFor(rhs, FpRegs & ~rmask(ra));

            switch (op) {
            case LIR_fadd: ADD_D(rr, ra, rb); break;
            case LIR_fsub: SUB_D(rr, ra, rb); break;
            case LIR_fmul: MUL_D(rr, ra, rb); break;
            case LIR_fdiv: DIV_D(rr, ra, rb); break;
            default:
                BADOPCODE(op);
            }
        }
        TAG("asm_fop(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_fneg(LIns *ins)
    {
        NanoAssert(cpu_has_fpu);
        if (cpu_has_fpu) {
            LInsp lhs = ins->oprnd1();
            Register rr = deprecated_prepResultReg(ins, FpRegs);
            Register sr = ( !lhs->isInReg()
                            ? findRegFor(lhs, FpRegs)
                            : lhs->deprecated_getReg() );
            NEG_D(rr, sr);
        }
        TAG("asm_fneg(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_immf(LIns *ins)
    {
        int d = deprecated_disp(ins);
        Register rr = ins->deprecated_getReg();

        deprecated_freeRsrcOf(ins);

        if (cpu_has_fpu && deprecated_isKnownReg(rr)) {
            if (d)
                asm_spill(rr, d, false, true);
            asm_li_d(rr, ins->imm64_1(), ins->imm64_0());
        }
        else {
            NanoAssert(d);
            asm_store_imm64(ins, d, FP);
        }
        TAG("asm_immf(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void
    Assembler::asm_q2i(LIns *)
    {
        NanoAssert(0);  // q2i shouldn't occur on 32-bit platforms
    }

    void Assembler::asm_promote(LIns *ins)
    {
        USE(ins);
        TODO(asm_promote);
        TAG("asm_promote(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_load64(LIns *ins)
    {
        NanoAssert(ins->isF64());

        LIns* base = ins->oprnd1();
        int dr = ins->disp();

        Register rd = ins->deprecated_getReg();
        int ds = deprecated_disp(ins);

        Register rbase = findRegFor(base, GpRegs);
        NanoAssert(IsGpReg(rbase));
        deprecated_freeRsrcOf(ins);

        if (cpu_has_fpu && deprecated_isKnownReg(rd)) {
            NanoAssert(IsFpReg(rd));
            asm_ldst64 (false, rd, dr, rbase);
        }
        else {
            // Either FPU is not available or the result needs to go into memory;
            // in either case, FPU instructions are not required. Note that the
            // result will never be loaded into registers if FPU is not available.
            NanoAssert(!deprecated_isKnownReg(rd));
            NanoAssert(ds != 0);

            NanoAssert(isS16(dr) && isS16(dr+4));
            NanoAssert(isS16(ds) && isS16(ds+4));

            // Check that the offset is 8-byte (64-bit) aligned.
            NanoAssert((ds & 0x7) == 0);

            // FIXME: allocate a temporary to use for the copy
            // to avoid load to use delay
            // lw $at,dr($rbase)
            // sw $at,ds($fp)
            // lw $at,dr+4($rbase)
            // sw $at,ds+4($fp)

            SW(AT, ds+4, FP);
            LW(AT, dr+4, rbase);
            SW(AT, ds,   FP);
            LW(AT, dr,   rbase);
        }

        TAG("asm_load64(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_cond(LIns *ins)
    {
        Register r = deprecated_prepResultReg(ins, GpRegs);
        LOpcode op = ins->opcode();

        // LIR_ov uses the flags set by arithmetic ops
        if (op == LIR_ov) {
            ovreg = r;
        }
        else {
            LIns *a = ins->oprnd1();
            LIns *b = ins->oprnd2();
            asm_cmp(op, a, b, r);
        }
        TAG("asm_cond(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_qhi(LIns *ins)
    {
        Register rr = deprecated_prepResultReg(ins, GpRegs);
        LIns *q = ins->oprnd1();
        int d = findMemFor(q);
        LW(rr, d+mswoff(), FP);
        TAG("asm_qhi(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_qlo(LIns *ins)
    {
        Register rr = deprecated_prepResultReg(ins, GpRegs);
        LIns *q = ins->oprnd1();
        int d = findMemFor(q);
        LW(rr, d+lswoff(), FP);
        TAG("asm_qlo(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_neg_not(LIns *ins)
    {
        LOpcode op = ins->opcode();
        Register rr = deprecated_prepResultReg(ins, GpRegs);

        LIns* lhs = ins->oprnd1();
        // If this is the last use of lhs in reg, we can re-use result reg.
        // Else, lhs already has a register assigned.
        Register ra = !lhs->isInReg() ? findSpecificRegFor(lhs, rr) : lhs->deprecated_getReg();
        if (op == LIR_not)
            NOT(rr, ra);
        else
            NEGU(rr, ra);
        TAG("asm_neg_not(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_immi(LIns *ins)
    {
        Register rr = deprecated_prepResultReg(ins, GpRegs);
        asm_li(rr, ins->imm32());
        TAG("asm_immi(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_cmov(LIns *ins)
    {
        LIns* condval = ins->oprnd1();
        LIns* iftrue  = ins->oprnd2();
        LIns* iffalse = ins->oprnd3();

        NanoAssert(condval->isCmp());
        NanoAssert(ins->opcode() == LIR_cmov && iftrue->isI32() && iffalse->isI32());

        const Register rr = deprecated_prepResultReg(ins, GpRegs);

        const Register iftruereg = findRegFor(iftrue, GpRegs & ~rmask(rr));
        MOVN(rr, iftruereg, AT);
        /*const Register iffalsereg =*/ findSpecificRegFor(iffalse, rr);
        asm_cmp(condval->opcode(), condval->oprnd1(), condval->oprnd2(), AT);
        TAG("asm_cmov(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_fcond(LIns *ins)
    {
        NanoAssert(cpu_has_fpu);
        if (cpu_has_fpu) {
            Register r = deprecated_prepResultReg(ins, GpRegs);
            LOpcode op = ins->opcode();
            LIns *a = ins->oprnd1();
            LIns *b = ins->oprnd2();

            if (cpu_has_cmov) {
                // c.xx.d  $a,$b
                // li      $r,1
                // movf    $r,$0,$fcc0
                MOVF(r, ZERO, 0);
                ORI(r, ZERO, 1);
            }
            else {
                // c.xx.d  $a,$b
                // [nop]
                // bc1t    1f
                //  li      $r,1
                // move    $r,$0
                // 1:
                NIns *here = _nIns;
                verbose_only(verbose_outputf("%p:", here);)
                underrunProtect(3*4);
                MOVE(r, ZERO);
                ORI(r, ZERO, 1);        // branch delay slot
                BC1T(here);
                if (cpu_has_fpuhazard)
                    NOP();
            }
            asm_cmp(op, a, b, r);
        }
        TAG("asm_fcond(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_i2f(LIns *ins)
    {
        NanoAssert(cpu_has_fpu);
        if (cpu_has_fpu) {
            Register fr = deprecated_prepResultReg(ins, FpRegs);
            Register v = findRegFor(ins->oprnd1(), GpRegs);

            // mtc1    $v,$fr
            // cvt.d.w $fr,$fr
            CVT_D_W(fr,fr);
            MTC1(v,fr);
        }
        TAG("asm_i2f(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_ret(LIns *ins)
    {
        genEpilogue();

        releaseRegisters();
        assignSavedRegs();
        ovreg = deprecated_UnknownReg;

        LIns *value = ins->oprnd1();
        if (ins->isop(LIR_ret)) {
            findSpecificRegFor(value, V0);
        }
        else {
            NanoAssert(ins->isop(LIR_fret));
            if (cpu_has_fpu)
                findSpecificRegFor(value, FV0);
            else {
                NanoAssert(value->isop(LIR_qjoin));
                // FIXME: endianess?
                findSpecificRegFor(value->oprnd1(), V0); // lo
                findSpecificRegFor(value->oprnd2(), V1); // hi
            }
        }
        TAG("asm_ret(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_load32(LIns *ins)
    {
        LOpcode op = ins->opcode();
        LIns* base = ins->oprnd1();
        int d = ins->disp();

        Register rres = deprecated_prepResultReg(ins, GpRegs);
        Register rbase = getBaseReg(base, d, GpRegs);

        switch (op) {
        case LIR_ldzb:          // 8-bit integer load, zero-extend to 32-bit
            asm_ldst(OP_LBU, rres, d, rbase);
            break;
        case LIR_ldzs:          // 16-bit integer load, zero-extend to 32-bit
            asm_ldst(OP_LHU, rres, d, rbase);
            break;
        case LIR_ldsb:          // 8-bit integer load, sign-extend to 32-bit
            asm_ldst(OP_LB, rres, d, rbase);
            break;
        case LIR_ldss:          // 16-bit integer load, sign-extend to 32-bit
            asm_ldst(OP_LH, rres, d, rbase);
            break;
        case LIR_ld:            // 32-bit integer load
            asm_ldst(OP_LW, rres, d, rbase);
            break;
        default:
            BADOPCODE(op);
        }

        TAG("asm_load32(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_param(LIns *ins)
    {
        uint32_t a = ins->paramArg();
        uint32_t kind = ins->paramKind();

        if (kind == 0) {
            // ordinary param
            // first 4 args A0..A3
            if (a < 4) {
                // incoming arg in register
                deprecated_prepResultReg(ins, rmask(argRegs[a]));
            } else {
                // incoming arg is on stack
                Register r = deprecated_prepResultReg(ins, GpRegs);
                TODO(Check stack offset);
                int d = FRAMESIZE + a * sizeof(intptr_t);
                LW(r, d, FP);
            }
        }
        else {
            // saved param
            deprecated_prepResultReg(ins, rmask(savedRegs[a]));
        }
        TAG("asm_param(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_arith(LIns *ins)
    {
        LOpcode op = ins->opcode();
        LInsp lhs = ins->oprnd1();
        LInsp rhs = ins->oprnd2();

        RegisterMask allow = GpRegs;

        // We always need the result register and the first operand register.
        Register rr = deprecated_prepResultReg(ins, allow);

        // If this is the last use of lhs in reg, we can re-use the result reg.
        // Else, lhs already has a register assigned.
        Register ra = !lhs->isInReg() ? findSpecificRegFor(lhs, rr) : lhs->deprecated_getReg();
        Register rb;

        // Don't re-use the registers we've already allocated.
        NanoAssert(deprecated_isKnownReg(rr));
        NanoAssert(deprecated_isKnownReg(ra));
        allow &= ~rmask(rr);
        allow &= ~rmask(ra);

        if (rhs->isconst()) {
            int32_t rhsc = rhs->imm32();
            if (isS16(rhsc)) {
                // MIPS arith immediate ops sign-extend the imm16 value
                switch (op) {
                case LIR_add:
                    if (ovreg != deprecated_UnknownReg)
                        SLT(ovreg, rr, ra);
                    ADDIU(rr, ra, rhsc);
                    goto done;
                case LIR_sub:
                    if (isS16(-rhsc)) {
                        if (ovreg != deprecated_UnknownReg)
                            SLT(ovreg, ra, rr);
                        ADDIU(rr, ra, -rhsc);
                        goto done;
                    }
                    break;
                case LIR_mul:
                    // FIXME: optimise constant multiply by 2^n
                    // if ((rhsc & (rhsc-1)) == 0)
                    //    SLL(rr, ra, ffs(rhsc)-1);
                    //goto done;
                    break;
                default:
                    break;
                }
            }
            if (isU16(rhsc)) {
                // MIPS logical immediate zero-extend the imm16 value
                switch (op) {
                case LIR_or:
                    ORI(rr, ra, rhsc);
                    goto done;
                case LIR_and:
                    ANDI(rr, ra, rhsc);
                    goto done;
                case LIR_xor:
                    XORI(rr, ra, rhsc);
                    goto done;
                default:
                    break;
                }
            }

            // LIR shift ops only use last 5bits of shift const
            switch (op) {
            case LIR_lsh:
                SLL(rr, ra, rhsc&31);
                goto done;
            case LIR_ush:
                SRL(rr, ra, rhsc&31);
                goto done;
            case LIR_rsh:
                SRA(rr, ra, rhsc&31);
                goto done;
            default:
                break;
            }
        }

        // general case, put rhs in register
        rb = (rhs == lhs) ? ra : findRegFor(rhs, allow);
        NanoAssert(deprecated_isKnownReg(rb));

        switch (op) {
            case LIR_add:
                if (ovreg != deprecated_UnknownReg)
                    SLT(ovreg,rr,ra);
                ADDU(rr, ra, rb);
                break;
            case LIR_and:
                AND(rr, ra, rb);
                break;
            case LIR_or:
                OR(rr, ra, rb);
                break;
            case LIR_xor:
                XOR(rr, ra, rb);
                break;
            case LIR_sub:
                if (ovreg != deprecated_UnknownReg)
                    SLT(ovreg,ra,rr);
                SUBU(rr, ra, rb);
                break;
            case LIR_lsh:
                SLLV(rr, ra, rb);
                ANDI(rb, rb, 31);
                break;
            case LIR_rsh:
                SRAV(rr, ra, rb);
                ANDI(rb, rb, 31);
                break;
            case LIR_ush:
                SRLV(rr, ra, rb);
                ANDI(rb, rb, 31);
                break;
            case LIR_mul:
                if (ovreg != deprecated_UnknownReg) {
                    // Overflow indication required
                    // Do a 32x32 signed multiply generating a 64 bit result
                    // Compare bit31 of the result with the high order bits
                    // mult $ra,$rb
                    // mflo $rr         # result to $rr
                    // sra  $at,$rr,31  # $at = 0x00000000 or 0xffffffff
                    // mfhi $ovreg
                    // xor  $ovreg,$ovreg,$at   # sets $ovreg to nonzero if overflow
                    // sltu $ovreg,$0,$ovreg    # sets $ovreg to 1 if overflow
                    SLTU(ovreg,ZERO,ovreg);
                    XOR(ovreg,ovreg,AT);
                    MFHI(ovreg);
                    SRA(AT,rr,31);
                    MFLO(rr);
                    MULT(ra, rb);
                }
                else
                    MUL(rr, ra, rb);
                break;
            case LIR_div:
            case LIR_mod:
            default:
                BADOPCODE(op);
        }
    done:
        ovreg = deprecated_UnknownReg;     // Potential overflow has been handled
        TAG("asm_arith(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    void Assembler::asm_store64(LOpcode op, LIns *value, int dr, LIns *base)
    {
        Register rbase;
        // NanoAssert((dr & 7) == 0);
        NanoAssert (op == LIR_stqi || op == LIR_st32f || op == LIR_stfi);

        switch (op) {
            case LIR_stfi:
                // handled by mainline code below for now
                break;
            case LIR_st32f:
                NanoAssertMsg(0, "NJ_EXPANDED_LOADSTORE_SUPPORTED not yet supported for this architecture");
                return;
            default:
                BADOPCODE(op);
                return;
        }

        if (op == LIR_stfi) {
            if (base->isop(LIR_alloc)) {
                rbase = FP;
                dr += findMemFor(base);
            }
            else
                rbase = findRegFor(base, GpRegs);

            if (value->isconstf())
                asm_store_imm64(value, dr, rbase);
            else if (!cpu_has_fpu || value->isop(LIR_ldq)) {

                int ds = findMemFor(value);

                // lw $at,ds(FP)
                // sw $at,dr($rbase)
                // lw $at,ds+4(FP)
                // sw $at,dr+4($rbase)
                SW(AT, dr+4, rbase);
                LW(AT, ds+4, FP);
                SW(AT, dr,   rbase);
                LW(AT, ds,   FP);
            }
            else {
                NanoAssert (cpu_has_fpu);
                // if value is already in a reg, use that, otherwise
                // allocate an FPU register
                Register fr = !value->isInReg() ?
                    findRegFor(value, FpRegs) : value->getReg();
                asm_ldst64(true, fr, dr, rbase);
            }
        }
        else
            BADOPCODE(op);

        TAG("asm_store64(value=%p{%s}, dr=%d, base=%p{%s})",
            value, lirNames[value->opcode()], dr, base, lirNames[base->opcode()]);
    }

    void Assembler::asm_restore(LIns *i, Register r)
    {
        int d;
        if (i->isop(LIR_alloc)) {
            d = deprecated_disp(i);
            if (isS16(d))
                ADDIU(r, FP, d);
            else {
                ADDU(r, FP, AT);
                asm_li(AT, d);
            }
        }
        else if (i->isconst()) {
            if (!i->deprecated_getArIndex())
                i->deprecated_markAsClear();
            asm_li(r, i->imm32());
        }
        else {
            d = findMemFor(i);
            if (IsFpReg(r)) {
                asm_ldst64(false, r, d, FP);
            }
            else {
                asm_ldst(OP_LW, r, d, FP);
            }
        }
        TAG("asm_restore(i=%p{%s}, r=%d)", i, lirNames[i->opcode()], r);
    }

    void Assembler::asm_cmp(LOpcode condop, LIns *a, LIns *b, Register cr)
    {
        RegisterMask allow = isFCmpOpcode(condop) ? FpRegs : GpRegs;
        Register ra = findRegFor(a, allow);
        Register rb = (b==a) ? ra : findRegFor(b, allow & ~rmask(ra));

        // FIXME: Use slti if b is small constant

        /* Generate the condition code */
        switch (condop) {
        case LIR_eq:
            SLTIU(cr,cr,1);
            XOR(cr,ra,rb);
            break;
        case LIR_lt:
            SLT(cr,ra,rb);
            break;
        case LIR_gt:
            SLT(cr,rb,ra);
            break;
        case LIR_le:
            XORI(cr,cr,1);
            SLT(cr,rb,ra);
            break;
        case LIR_ge:
            XORI(cr,cr,1);
            SLT(cr,ra,rb);
            break;
        case LIR_ult:
            SLTU(cr,ra,rb);
            break;
        case LIR_ugt:
            SLTU(cr,rb,ra);
            break;
        case LIR_ule:
            XORI(cr,cr,1);
            SLTU(cr,rb,ra);
            break;
        case LIR_uge:
            XORI(cr,cr,1);
            SLTU(cr,ra,rb);
            break;
        case LIR_feq:
            C_EQ_D(ra,rb);
            break;
        case LIR_flt:
            C_LT_D(ra,rb);
            break;
        case LIR_fgt:
            C_LT_D(rb,ra);
            break;
        case LIR_fle:
            C_LE_D(ra,rb);
            break;
        case LIR_fge:
            C_LE_D(rb,ra);
            break;
        default:
            debug_only(outputf("%s",lirNames[condop]);)
            TODO(asm_cond);
        }
    }

#define SEG(addr) (uint32_t(addr) & 0xf0000000)
#define SEGOFFS(addr) (uint32_t(addr) & 0x0fffffff)

    NIns* Assembler::asm_branch(bool branchOnFalse, LIns *cond, NIns * const targ)
    {
        LOpcode condop = cond->opcode();
        NanoAssert(cond->isCond());
        bool inrange;
        RegisterMask allow = isFCmpOpcode(condop) ? FpRegs : GpRegs;
        LIns *a = cond->oprnd1();
        LIns *b = cond->oprnd2();
        Register ra = findRegFor(a, allow);
        Register rb = (b==a) ? ra : findRegFor(b, allow & ~rmask(ra));
        NIns *btarg = targ;

        // do initial underrun check here to ensure that inrange test is correct
        if (targ)
            underrunProtect(2 * 4);    // branch + delay slot

        // MIPS offsets are based on the address of the branch delay slot
        // which is the next instruction that will be generated
        ptrdiff_t bd = BOFFSET(targ-1);

#if PEDANTIC
        inrange = false;
#else
        inrange = (targ && isS16(bd));
#endif

        // If the branch target is known and in range we can just generate a branch
        // Otherwise generate a branch to a trampoline that will be stored in the
        // literal area
        if (inrange) {
            NOP();
        }
        else {
            underrunProtect(5 * 4);                        // worst case: branch;lui and ori,jr,nop
            NIns *tramp = _nSlot;
            if (targ) {
                if (SEG(targ) == SEG(tramp)) {
                    // bxxx trampoline
                    //  nop
                    // ...
                    // trampoline:
                    // j targ
                    //  nop
                    // NB trampoline code is emitted in the correct order!
                    tramp = _nSlot;
                    trampJ(targ);
                    trampNOP();                                    // delay slot

                    NOP();                                        // delay slot
                }
                else {
                    // bxxx trampoline
                    //  lui $at,((targ)>>16) & 0xffff
                    // ...
                    // trampoline:
                    // ori $at,targ & 0xffff
                    // jr $at
                    //  nop
                    tramp = _nSlot;
                    trampADDIU(AT,AT,lo(uint32_t(targ)));
                    trampJR(AT);
                    trampNOP();                                    // delay slot

                    LUI(AT,hi(uint32_t(targ)));                        // delay slot
                }
            }
            else {
                // Worst case is lui;ori;jr;nop as above
                // Best case is branch to trampoline can be replaced with branch to target
                // Fixup handled in nPatchBranch
                trampNOP();
                trampNOP();
                trampNOP();

                NOP();
            }
            btarg = tramp;
        }

        NIns *patch = NULL;
        if (cpu_has_fpu && isFCmpOpcode(condop)) {
            // c.xx.d $ra,$rb
            // bc1x   btarg
            switch (condop) {
            case LIR_feq:
                if (branchOnFalse)
                    BC1F(btarg);
                else
                    BC1T(btarg);
                patch = _nIns;
                if (cpu_has_fpuhazard)
                    NOP();
                C_EQ_D(ra,rb);
                break;
            case LIR_flt:
                if (branchOnFalse)
                    BC1F(btarg);
                else
                    BC1T(btarg);
                patch = _nIns;
                if (cpu_has_fpuhazard)
                    NOP();
                C_LT_D(ra,rb);
                break;
            case LIR_fgt:
                if (branchOnFalse)
                    BC1F(btarg);
                else
                    BC1T(btarg);
                patch = _nIns;
                if (cpu_has_fpuhazard)
                    NOP();
                C_LT_D(rb,ra);
                break;
            case LIR_fle:
                if (branchOnFalse)
                    BC1F(btarg);
                else
                    BC1T(btarg);
                patch = _nIns;
                if (cpu_has_fpuhazard)
                    NOP();
                C_LE_D(ra,rb);
                break;
            case LIR_fge:
                if (branchOnFalse)
                    BC1F(btarg);
                else
                    BC1T(btarg);
                patch = _nIns;
                if (cpu_has_fpuhazard)
                    NOP();
                C_LE_D(rb,ra);
                break;
            default:
                BADOPCODE(condop);
                break;
            }
        }
        else {
            // general case
            // s[lg]tu?   $at,($ra,$rb|$rb,$ra)
            // b(ne|eq)z  $at,btarg
            switch (condop) {
            case LIR_eq:
                // special case
                // b(ne|eq)  $ra,$rb,btarg
                if (branchOnFalse)
                    BNE(ra,rb,btarg);
                else
                    BEQ(ra,rb,btarg);
                patch = _nIns;
                break;
            case LIR_lt:
                if (branchOnFalse)
                    BEQ(AT,ZERO,btarg);
                else
                    BNE(AT,ZERO,btarg);
                patch = _nIns;
                SLT(AT,ra,rb);
                break;
            case LIR_gt:
                if (branchOnFalse)
                    BEQ(AT,ZERO,btarg);
                else
                    BNE(AT,ZERO,btarg);
                patch = _nIns;
                SLT(AT,rb,ra);
                break;
            case LIR_le:
                if (branchOnFalse)
                    BNE(AT,ZERO,btarg);
                else
                    BEQ(AT,ZERO,btarg);
                patch = _nIns;
                SLT(AT,rb,ra);
                break;
            case LIR_ge:
                if (branchOnFalse)
                    BNE(AT,ZERO,btarg);
                else
                    BEQ(AT,ZERO,btarg);
                patch = _nIns;
                SLT(AT,ra,rb);
                break;
            case LIR_ult:
                if (branchOnFalse)
                    BEQ(AT,ZERO,btarg);
                else
                    BNE(AT,ZERO,btarg);
                patch = _nIns;
                SLTU(AT,ra,rb);
                break;
            case LIR_ugt:
                if (branchOnFalse)
                    BEQ(AT,ZERO,btarg);
                else
                    BNE(AT,ZERO,btarg);
                patch = _nIns;
                SLTU(AT,rb,ra);
                break;
            case LIR_ule:
                if (branchOnFalse)
                    BNE(AT,ZERO,btarg);
                else
                    BEQ(AT,ZERO,btarg);
                patch = _nIns;
                SLT(AT,rb,ra);
                break;
            case LIR_uge:
                if (branchOnFalse)
                    BNE(AT,ZERO,btarg);
                else
                    BEQ(AT,ZERO,btarg);
                patch = _nIns;
                SLTU(AT,ra,rb);
                break;
            default:
                BADOPCODE(condop);
            }
        }
        TAG("asm_branch(branchOnFalse=%d, cond=%p{%s}, targ=%p)",
            branchOnFalse, cond, lirNames[cond->opcode()], targ);
        return patch;
    }

    void Assembler::asm_j(NIns * const targ, bool bdelay)
    {
        underrunProtect(2*4);    // branch + delay
        if (targ)
            NanoAssert(SEG(targ) == SEG(_nIns));
        if (bdelay)
            NOP();
        J(targ);
        TAG("asm_j(targ=%p) bdelay=%d", targ);
    }

    void
    Assembler::asm_spill(Register rr, int d, bool pop, bool quad)
    {
        USE(pop);
        USE(quad);
        NanoAssert(d);
        if (IsFpReg(rr)) {
            NanoAssert(quad);
            asm_ldst64(true, rr, d, FP);
        }
        else {
            NanoAssert(!quad);
            asm_ldst(OP_SW, rr, d, FP);
        }
        TAG("asm_spill(rr=%d, d=%d, pop=%d, quad=%d)", rr, d, pop, quad);
    }

    void
    Assembler::asm_nongp_copy(Register dst, Register src)
    {
        NanoAssert ((rmask(dst) & FpRegs) && (rmask(src) & FpRegs));
        MOV_D(dst, src);
        TAG("asm_nongp_copy(dst=%d src=%d)", dst, src);
    }

    /*
     * asm_arg will encode the specified argument according to the current ABI, and
     * will update r and stkd as appropriate so that the next argument can be
     * encoded.
     *
     * - doubles are 64-bit aligned.  both in registers and on the stack.
     *   If the next available argument register is A1, it is skipped
     *   and the double is placed in A2:A3.  If A0:A1 or A2:A3 are not
     *   available, the double is placed on the stack, 64-bit aligned.
     * - 32-bit arguments are placed in registers and 32-bit aligned
     *   on the stack.
     */
    void
    Assembler::asm_arg(ArgType ty, LInsp arg, Register& r, Register& fr, int& stkd)
    {
        // The stack offset must always be at least aligned to 4 bytes.
        NanoAssert((stkd & 3) == 0);

        if (ty == ARGTYPE_F) {
            // This task is fairly complex and so is delegated to asm_arg_64.
            asm_arg_64(arg, r, fr, stkd);
        } else {
            NanoAssert(ty == ARGTYPE_I || ty == ARGTYPE_U);
            if (stkd < 16) {
                asm_regarg(ty, arg, r);
                fr = nextreg(fr);
                r = nextreg(r);
            }
            else
                asm_stkarg(arg, stkd);
            // The o32 ABI calling convention is that if the first arguments
            // is not a double, subsequent double values are passed in integer registers
            fr = r;
            stkd += 4;
        }
    }

    void
    Assembler::asm_call(LInsp ins)
    {
        Register rr;
        LOpcode op = ins->opcode();

        switch (op) {
        case LIR_fcall:
            NanoAssert(cpu_has_fpu);
            rr = FV0;
            break;
        case LIR_icall:
            rr = retRegs[0];
            break;
        default:
            BADOPCODE(op);
            return;
        }

        deprecated_prepResultReg(ins, rmask(rr));

        // Do this after we've handled the call result, so we don't
        // force the call result to be spilled unnecessarily.

        evictScratchRegsExcept(0);

        const CallInfo* ci = ins->callInfo();
        ArgType argTypes[MAXARGS];
        uint32_t argc = ci->getArgTypes(argTypes);
        bool indirect = ci->isIndirect();

        // FIXME: Put one of the argument moves into the BDS slot

        underrunProtect(2*4);    // branch+delay
        NOP();
        JALR(T9);

        if (!indirect)
            // FIXME: If we can tell that we are calling non-PIC
            // (ie JIT) code, we could call direct instead of using t9
            asm_li(T9, ci->_address);
        else
            // Indirect call: we assign the address arg to t9
            // which matches the o32 ABI for calling functions
            asm_regarg(ARGTYPE_P, ins->arg(--argc), T9);

        // Encode the arguments, starting at A0 and with an empty argument stack.
        Register    r = A0, fr = FA0;
        int         stkd = 0;

        // Iterate through the argument list and encode each argument according to
        // the ABI.
        // Note that we loop through the arguments backwards as LIR specifies them
        // in reverse order.
        while(argc--)
            asm_arg(argTypes[argc], ins->arg(argc), r, fr, stkd);

        if (stkd > max_out_args)
            max_out_args = stkd;
        TAG("asm_call(ins=%p{%s})", ins, lirNames[ins->opcode()]);
    }

    Register
    Assembler::nRegisterAllocFromSet(RegisterMask set)
    {
        Register i;
        int n;

        // note, deliberate truncation of 64->32 bits
        if (set & 0xffffffff) {
            // gp reg
            n = ffs(int(set));
            NanoAssert(n != 0);
            i = Register(n - 1);
        }
        else {
            // fp reg
            NanoAssert(cpu_has_fpu);
            n = ffs(int(set >> 32));
            NanoAssert(n != 0);
            i = Register(32 + n - 1);
        }
        _allocator.free &= ~rmask(i);
        TAG("nRegisterAllocFromSet(set=%016llx) => %s", set, gpn(i));
        return i;
    }

    void
    Assembler::nRegisterResetAll(RegAlloc& regs)
    {
        regs.clear();
        regs.free = GpRegs;
        if (cpu_has_fpu)
            regs.free |= FpRegs;
        debug_only(regs.managed = regs.free;)
    }

#define signextend16(s) ((int32_t(s)<<16)>>16)

    void
    Assembler::nPatchBranch(NIns* branch, NIns* target)
    {
        uint32_t op = (branch[0] >> 26) & 0x3f;
        uint32_t bdoffset = target-(branch+1);

        if (op == OP_BEQ || op == OP_BNE ||
            ((branch[0] & 0xfffe0000) == ((OP_COP1 << 26) | (COP1_BC << 21)))) {
            if (isS16(bdoffset)) {
                // The branch is in range, so just replace the offset in the instruction
                // The trampoline that was allocated is redundant and will remain unused
                branch[0] = (branch[0]  & 0xffff0000) | (bdoffset & 0xffff);
            }
            else {
                // The branch is pointing to a trampoline. Find out where that is
                NIns *tramp = branch + 1 + (signextend16(branch[0] & 0xffff));
                if (SEG(branch) == SEG(target)) {
                    *tramp = J_FORMAT(OP_J,JINDEX(target));
                }
                else {
                    // Full 32-bit jump
                    // bxx tramp
                    //  lui $at,(target>>16)>0xffff
                    // ..
                    // tramp:
                    // ori $at,target & 0xffff
                    // jr $at
                    //  nop
                    branch[1] = U_FORMAT(OP_LUI,0,AT,hi(uint32_t(target)));
                    tramp[0] = U_FORMAT(OP_ADDIU,AT,AT,lo(uint32_t(target)));
                    tramp[1] = R_FORMAT(OP_SPECIAL,AT,0,0,0,SPECIAL_JR);
                }
            }
        }
        else if (op == OP_J) {
            NanoAssert (SEG(branch) == SEG(target));
            branch[0] = J_FORMAT(OP_J,JINDEX(target));
        }
        else
            TODO(unknown_patch);
        // TAG("nPatchBranch(branch=%p target=%p)", branch, target);
    }

    void
    Assembler::nFragExit(LIns *guard)
    {
        SideExit *exit = guard->record()->exit;
        Fragment *frag = exit->target;
        bool destKnown = (frag && frag->fragEntry);

        // Generate jump to epilogue and initialize lr.

        // If the guard already exists, use a simple jump.
        if (destKnown) {
            // j     _fragEntry
            //  move $v0,$zero
            MOVE(V0, ZERO);
            asm_j(frag->fragEntry, false);
        }
        else {
            // Target doesn't exist. Jump to an epilogue for now.
            // This can be patched later.
            if (!_epilogue)
                _epilogue = genEpilogue();
            GuardRecord *lr = guard->record();
            // FIXME: _epilogue may be in another segment
            // lui    $v0,%hi(lr)
            // j      _epilogue
            //  addiu $v0,%lo(lr)
            ADDIU(V0, V0, lo(int32_t(lr)));
            asm_j(_epilogue, false);
            LUI(V0, hi(int32_t(lr)));
            lr->jmp = _nIns;
        }

        // profiling for the exit
        verbose_only(
            if (_logc->lcbits & LC_FragProfile) {
                underrunProtect(4*4);
                // lui   $fp,%hi(profCount)
                // lw    $at,%lo(profCount)(fp)
                // addiu $at,1
                // sw    $at,%lo(profCount)(fp)
                uint32_t profCount = uint32_t(&guard->record()->profCount);
                SW(AT, lo(profCount), FP);
                ADDIU(AT, AT, 1);
                LW(AT, lo(profCount), FP);
                LUI(FP, hi(profCount));
            }
        )

        // Pop the stack frame.
        MOVE(SP, FP);

        // return value is GuardRecord*
        TAG("nFragExit(guard=%p{%s})", guard, lirNames[guard->opcode()]);
    }

    void
    Assembler::nInit(AvmCore*)
    {
        // Cannot use outputf
    }

    void Assembler::nBeginAssembly()
    {
        max_out_args = 16;        // Always reserve space for a0-a3
    }

    // Increment the 32-bit profiling counter at pCtr, without
    // changing any registers.
    verbose_only(
    void Assembler::asm_inc_m32(uint32_t* /*pCtr*/)
    {
        // TODO: implement this
    }
    )

    void
    Assembler::nativePageReset(void)
    {
        _nSlot = 0;
        _nExitSlot = 0;
        TAG("nativePageReset()");
    }

    void
    Assembler::nativePageSetup(void)
    {
        NanoAssert(!_inExit);
        if (!_nIns)
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
        if (!_nExitIns)
            codeAlloc(exitStart, exitEnd, _nExitIns verbose_only(, exitBytes));

        // constpool starts at bottom of page and moves up
        // code starts at top of page and goes down,

        if (!_nSlot)
            _nSlot = codeStart;
        if (!_nExitSlot)
            _nExitSlot = exitStart;

        TAG("nativePageSetup()");
    }


    NIns*
    Assembler::genPrologue(void)
    {
        /*
         * Use a non standard fp because we don't know the final framesize until now
         * addiu  $sp,-FRAMESIZE
         * sw      $ra,RA_OFFSET($sp)
         * sw      $fp,FP_OFFSET($sp)
         * move   $fp,$sp
         * addu      $sp,-stackNeeded
         */

        uint32_t stackNeeded = max_out_args + STACK_GRANULARITY * _activation.stackSlotsNeeded();
        uint32_t amt = alignUp(stackNeeded, NJ_ALIGN_STACK);

        if (amt) {
            if (isS16(-amt))
                ADDIU(SP, SP, -amt);
            else {
                ADDU(SP, SP, AT);
                asm_li(AT, -amt);
            }
        }

        NIns *patchEntry = _nIns; // FIXME: who uses this value and where should it point?

        MOVE(FP, SP);
        SW(FP, FP_OFFSET, SP);
        SW(RA, RA_OFFSET, SP);        // No need to save for leaf functions
        ADDIU(SP, SP, -FRAMESIZE);

        TAG("genPrologue()");

        return patchEntry;
    }

    NIns*
    Assembler::genEpilogue(void)
    {
        /*
         * move    $sp,$fp
         * lw      $ra,RA_OFFSET($sp)
         * lw      $fp,FP_OFFSET($sp)
         * j       $ra
         * addiu   $sp,FRAMESIZE
         */
        ADDIU(SP, SP, FRAMESIZE);
        JR(RA);
        LW(FP, FP_OFFSET, SP);
        LW(RA, RA_OFFSET, SP);
        MOVE(SP, FP);

        TAG("genEpilogue()");

        return _nIns;
    }

    RegisterMask
    Assembler::hint(LIns* i)
    {
        uint32_t op = i->opcode();
        RegisterMask prefer = 0LL;

        if (op == LIR_icall)
            prefer = rmask(V0);
        else if (op == LIR_callh)
            prefer = rmask(V1);
        else if (op == LIR_fcall)
            prefer = rmask(FV0);
        else if (op == LIR_param) {
            // FIXME: FLOAT parameters?
            if (i->paramArg() < 4)
                prefer = rmask(argRegs[i->paramArg()]);
        }

        return prefer;
    }

    void
    Assembler::underrunProtect(int bytes)
    {
        NanoAssertMsg(bytes<=LARGEST_UNDERRUN_PROT, "constant LARGEST_UNDERRUN_PROT is too small");
        NanoAssert(_nSlot != 0);
        uintptr_t top = uintptr_t(_nSlot);
        uintptr_t pc = uintptr_t(_nIns);
        if (pc - bytes < top) {
            verbose_only(verbose_outputf("        %p:", _nIns);)
            NIns* target = _nIns;
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));

            _nSlot = codeStart;

            // _nSlot points to the first empty position in the new code block
            // _nIns points just past the last empty position.
            asm_j(target, true);
        }
    }

    void
    Assembler::swapCodeChunks() {
        if (!_nExitIns)
            codeAlloc(exitStart, exitEnd, _nExitIns verbose_only(, exitBytes));
        if (!_nExitSlot)
            _nExitSlot = exitStart;
        SWAP(NIns*, _nIns, _nExitIns);
        SWAP(NIns*, _nSlot, _nExitSlot);
        SWAP(NIns*, codeStart, exitStart);
        SWAP(NIns*, codeEnd, exitEnd);
        verbose_only( SWAP(size_t, codeBytes, exitBytes); )
    }
}

#endif // FEATURE_NANOJIT && NANOJIT_MIPS
