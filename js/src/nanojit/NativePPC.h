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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#ifndef __nanojit_NativePPC__
#define __nanojit_NativePPC__

#ifdef PERFM
#define DOPROF
#include "../vprof/vprof.h"
#define count_instr() _nvprof("ppc",1)
#define count_prolog() _nvprof("ppc-prolog",1); count_instr();
#define count_imt() _nvprof("ppc-imt",1) count_instr()
#else
#define count_instr()
#define count_prolog()
#define count_imt()
#endif

namespace nanojit
{
#define NJ_MAX_STACK_ENTRY              4096
#define NJ_ALIGN_STACK                  16

#define NJ_JTBL_SUPPORTED               1
#define NJ_EXPANDED_LOADSTORE_SUPPORTED 0
#define NJ_F2I_SUPPORTED                0
#define NJ_SOFTFLOAT_SUPPORTED          0
#define NJ_DIVI_SUPPORTED               0

    enum ConditionRegister {
        CR0 = 0,
        CR1 = 1,
        CR2 = 2,
        CR3 = 3,
        CR4 = 4,
        CR5 = 5,
        CR6 = 6,
        CR7 = 7,
    };

    enum ConditionBit {
        COND_lt = 0, // msb of CR
        COND_gt = 1,
        COND_eq = 2,
        COND_so = 3, // lsb of CR
        COND_un = 3,
    };

    // this is the BO field in condition instructions
    enum ConditionOption {
        BO_true = 12, // branch if true
        BO_false = 4, // branch if false
    };

    typedef uint32_t Register;
    static const Register
        // general purpose 32bit regs
        R0   = { 0 },   // scratch or the value 0, excluded from regalloc
        SP   = { 1 },   // stack pointer, excluded from regalloc
        R2   = { 2 },   // scratch on MacOSX, rtoc pointer elsewhere
        R3   = { 3 },   // this, return value, MSW of int64 return
        R4   = { 4 },   // param, LSW of int64 return
        R5   = { 5 },   // param
        R6   = { 6 },   // param
        R7   = { 7 },   // param
        R8   = { 8 },   // param
        R9   = { 9 },   // param
        R10  = { 10 },  // param
        R11  = { 11 },  // scratch in leaf funcs, outgoing arg ptr otherwise
        R12  = { 12 },  // scratch
        R13  = { 13 },  // ppc32: saved, ppc64: thread-specific storage
        R14  = { 14 },  // saved
        R15  = { 15 },
        R16  = { 16 },
        R17  = { 17 },
        R18  = { 18 },
        R19  = { 19 },
        R20  = { 20 },
        R21  = { 21 },
        R22  = { 22 },
        R23  = { 23 },
        R24  = { 24 },
        R25  = { 25 },
        R26  = { 26 },
        R27  = { 27 },
        R28  = { 28 },
        R29  = { 29 },
        R30  = { 30 },
        R31  = { 31 },   // excluded from regalloc since we use it as FP
        FP  = R31,

        // FP regs
        F0  = { 32 },   // scratch, excluded from reg alloc
        F1  = { 33 },   // param, double return value
        F2  = { 34 },   // param
        F3  = { 35 },   // param
        F4  = { 36 },   // param
        F5  = { 37 },   // param
        F6  = { 38 },   // param
        F7  = { 39 },   // param
        F8  = { 40 },   // param
        F9  = { 41 },   // param
        F10 = { 42 },   // param
        F11 = { 43 },   // param
        F12 = { 44 },   // param
        F13 = { 45 },   // param
        F14 = { 46 },   // F14-31 saved
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

        // special purpose registers (SPR)
        Rxer = { 1 },
        Rlr  = { 8 },
        Rctr = { 9 },

        deprecated_UnknownReg = { 127 };    // XXX: remove eventually, see bug 538924

    static const uint32_t FirstRegNum = R0;
    static const uint32_t LastRegNum = F31;
}

#define NJ_USE_UINT32_REGISTER 1
#include "NativeCommon.h"

namespace nanojit
{
    enum PpcOpcode {
        // opcodes
        PPC_add     = 0x7C000214, // add
        PPC_addo    = 0x7C000614, // add & OE=1 (can set OV)
        PPC_addi    = 0x38000000, // add immediate
        PPC_addis   = 0x3C000000, // add immediate shifted
        PPC_and     = 0x7C000038, // and
        PPC_andc    = 0x7C000078, // and with compliment
        PPC_andi    = 0x70000000, // and immediate
        PPC_andis   = 0x74000000, // and immediate shifted
        PPC_b       = 0x48000000, // branch
        PPC_bc      = 0x40000000, // branch conditional
        PPC_bcctr   = 0x4C000420, // branch conditional to count register
        PPC_cmp     = 0x7C000000, // compare
        PPC_cmpi    = 0x2C000000, // compare immediate
        PPC_cmpl    = 0x7C000040, // compare logical
        PPC_cmpli   = 0x28000000, // compare logical immediate
        PPC_cror    = 0x4C000382, // condition register or
        PPC_extsw   = 0x7C0007B4, // extend sign word
        PPC_fadd    = 0xFC00002A, // floating add (double precision)
        PPC_fcfid   = 0xFC00069C, // floating convert from integer doubleword
        PPC_fcmpu   = 0xFC000000, // floating compare unordered
        PPC_fdiv    = 0xFC000024, // floating divide (double precision)
        PPC_fmr     = 0xFC000090, // floating move register (double precision)
        PPC_fmul    = 0xFC000032, // floating multiply (double precision)
        PPC_fneg    = 0xFC000050, // floating negate
        PPC_fsub    = 0xFC000028, // floating subtract (double precision)
        PPC_lbz     = 0x88000000, // load byte and zero
        PPC_lbzx    = 0x7C0000AE, // load byte and zero indexed
        PPC_ld      = 0xE8000000, // load doubleword
        PPC_ldx     = 0x7C00002A, // load doubleword indexed
        PPC_lfd     = 0xC8000000, // load floating point double
        PPC_lfdx    = 0x7C0004AE, // load floating-point double indexed
        PPC_lhz     = 0xA0000000, // load halfword and zero
        PPC_lhzx    = 0x7C00022E, // load halfword and zero indexed
        PPC_lwz     = 0x80000000, // load word and zero
        PPC_lwzx    = 0x7C00002E, // load word and zero indexed
        PPC_mfcr    = 0x7C000026, // move from condition register
        PPC_mfspr   = 0x7C0002A6, // move from spr (special purpose register)
        PPC_mtspr   = 0x7C0003A6, // move to spr
        PPC_mulli   = 0x1C000000, // multiply low immediate
        PPC_mullw   = 0x7C0001D6, // multiply low word
        PPC_neg     = 0x7C0000D0, // negate
        PPC_nor     = 0x7C0000F8, // nor
        PPC_or      = 0x7C000378, // or
        PPC_ori     = 0x60000000, // or immediate
        PPC_oris    = 0x64000000, // or immediate shifted
        PPC_rlwinm  = 0x54000000, // rotate left word then and with mask
        PPC_rldicl  = 0x78000000, // rotate left doubleword immediate then clear left
        PPC_rldicr  = 0x78000004, // rotate left doubleword immediate then clear right
        PPC_rldimi  = 0x7800000C, // rotate left doubleword immediate then mask insert
        PPC_sld     = 0x7C000036, // shift left doubleword
        PPC_slw     = 0x7C000030, // shift left word
        PPC_srad    = 0x7C000634, // shift right algebraic doubleword (sign ext)
        PPC_sradi   = 0x7C000674, // shift right algebraic doubleword immediate
        PPC_sraw    = 0x7C000630, // shift right algebraic word (sign ext)
        PPC_srawi   = 0x7C000670, // shift right algebraic word immediate
        PPC_srd     = 0x7C000436, // shift right doubleword (zero ext)
        PPC_srw     = 0x7C000430, // shift right word (zero ext)
        PPC_stb     = 0x98000000, // store byte
        PPC_stbx    = 0x7C0001AE, // store byte indexed
        PPC_std     = 0xF8000000, // store doubleword
        PPC_stdu    = 0xF8000001, // store doubleword with update
        PPC_stdux   = 0x7C00016A, // store doubleword with update indexed
        PPC_stdx    = 0x7C00012A, // store doubleword indexed
        PPC_stfd    = 0xD8000000, // store floating-point double
        PPC_stfdx   = 0x7C0005AE, // store floating-point double indexed
        PPC_stw     = 0x90000000, // store word
        PPC_stwu    = 0x94000000, // store word with update
        PPC_stwux   = 0x7C00016E, // store word with update indexed
        PPC_stwx    = 0x7C00012E, // store word indexed
        PPC_subf    = 0x7C000050, // subtract from
        PPC_xor     = 0x7C000278, // xor
        PPC_xori    = 0x68000000, // xor immediate
        PPC_xoris   = 0x6C000000, // xor immediate shifted

        // simplified mnemonics
        PPC_mr = PPC_or,
        PPC_not = PPC_nor,
        PPC_nop = PPC_ori,
    };

    typedef uint64_t RegisterMask;

    static const RegisterMask GpRegs = 0xffffffff;
    static const RegisterMask FpRegs = 0xffffffff00000000LL;
    // R31 is a saved reg too, but we use it as our Frame ptr FP
#ifdef NANOJIT_64BIT
    // R13 reserved for thread-specific storage on ppc64-darwin
    static const RegisterMask SavedRegs = 0x7fffc000; // R14-R30 saved
    static const int NumSavedRegs = 17; // R14-R30
#else
    static const RegisterMask SavedRegs = 0x7fffe000; // R13-R30 saved
    static const int NumSavedRegs = 18; // R13-R30
#endif

    static inline bool IsGpReg(Register r) {
        return r <= R31;
    }
    static inline bool IsFpReg(Register r) {
        return r >= F0;
    }

    verbose_only( extern const char* regNames[]; )

    #define DECLARE_PLATFORM_STATS()
    #define DECLARE_PLATFORM_REGALLOC()

#ifdef NANOJIT_64BIT
    #define DECL_PPC64()\
        void asm_qbinop(LIns*);
#else
    #define DECL_PPC64()
#endif

    #define DECLARE_PLATFORM_ASSEMBLER()                                    \
        const static Register argRegs[8], retRegs[2];                       \
        void underrunProtect(int bytes);                                    \
        void nativePageReset();                                             \
        void nativePageSetup();                                             \
        bool hardenNopInsertion(const Config& /*c*/) { return false; }      \
        void br(NIns *addr, int link);                                      \
        void br_far(NIns *addr, int link);                                  \
        void asm_regarg(ArgType, LIns*, Register);                          \
        void asm_li(Register r, int32_t imm);                               \
        void asm_li32(Register r, int32_t imm);                             \
        void asm_li64(Register r, uint64_t imm);                            \
        void asm_cmp(LOpcode op, LIns *a, LIns *b, ConditionRegister);      \
        NIns* asm_branch_far(bool onfalse, LIns *cond, NIns * const targ);  \
        NIns* asm_branch_near(bool onfalse, LIns *cond, NIns * const targ); \
        int  max_param_size; /* bytes */                                    \
        DECL_PPC64()

    const int LARGEST_UNDERRUN_PROT = 9*4;  // largest value passed to underrunProtect

    typedef uint32_t NIns;

    // Bytes of icache to flush after Assembler::patch
    const size_t LARGEST_BRANCH_PATCH = 4 * sizeof(NIns);

    #define EMIT1(ins, fmt, ...) do {\
        underrunProtect(4);\
        *(--_nIns) = (NIns) (ins);\
        asm_output(fmt, ##__VA_ARGS__);\
        } while (0) /* no semi */

    #define GPR(r) (r)
    #define FPR(r) ((r)&31)

    #define Bx(li,aa,lk) EMIT1(PPC_b | ((li)&0xffffff)<<2 | (aa)<<1 | (lk),\
        "b%s%s %p", (lk)?"l":"", (aa)?"a":"", _nIns+(li))

    #define B(li)   Bx(li,0,0)
    #define BA(li)  Bx(li,1,0)
    #define BL(li)  Bx(li,0,1)
    #define BLA(li) Bx(li,1,1)

    #define BCx(op,bo,bit,cr,bd,aa,lk) EMIT1(PPC_bc | (bo)<<21 | (4*(cr)+COND_##bit)<<16 |\
        ((bd)&0x3fff)<<2 | (aa)<<1 | (lk),\
        "%s%s%s cr%d,%p", #op, (lk)?"l":"", (aa)?"a":"", (cr), _nIns+(bd))

    #define BLT(cr,bd) BCx(blt, BO_true,  lt, cr, bd, 0, 0)
    #define BGT(cr,bd) BCx(bgt, BO_true,  gt, cr, bd, 0, 0)
    #define BEQ(cr,bd) BCx(beq, BO_true,  eq, cr, bd, 0, 0)
    #define BGE(cr,bd) BCx(bge, BO_false, lt, cr, bd, 0, 0)
    #define BLE(cr,bd) BCx(ble, BO_false, gt, cr, bd, 0, 0)
    #define BNE(cr,bd) BCx(bne, BO_false, eq, cr, bd, 0, 0)
    #define BNG(cr,bd) BCx(bng, BO_false, gt, cr, bd, 0, 0)
    #define BNL(cr,bd) BCx(bnl, BO_false, lt, cr, bd, 0, 0)

    #define BCCTRx(op, bo, bit, cr, lk) EMIT1(PPC_bcctr | (bo)<<21 | (4*(cr)+COND_##bit)<<16 | (lk)&1,\
        "%sctr%s cr%d", #op, (lk)?"l":"", (cr))

    #define BLTCTR(cr) BCCTRx(blt, BO_true,  lt, cr, 0)
    #define BGTCTR(cr) BCCTRx(bgt, BO_true,  gt, cr, 0)
    #define BEQCTR(cr) BCCTRx(beq, BO_true,  eq, cr, 0)
    #define BGECTR(cr) BCCTRx(bge, BO_false, lt, cr, 0)
    #define BLECTR(cr) BCCTRx(ble, BO_false, gt, cr, 0)
    #define BNECTR(cr) BCCTRx(bne, BO_false, eq, cr, 0)
    #define BNGCTR(cr) BCCTRx(bng, BO_false, gt, cr, 0)
    #define BNLCTR(cr) BCCTRx(bnl, BO_false, lt, cr, 0)

    #define Simple(asm,op) EMIT1(op, "%s", #asm)

    #define BCTR(link) EMIT1(0x4E800420 | (link), "bctr%s", (link) ? "l" : "")
    #define BCTRL() BCTR(1)

    #define BLR()   EMIT1(0x4E800020, "blr")
    #define NOP()   EMIT1(PPC_nop, "nop") /* ori 0,0,0 */

    #define ALU2(op, rd, ra, rb, rc) EMIT1(PPC_##op | GPR(rd)<<21 | GPR(ra)<<16 | GPR(rb)<<11 | (rc),\
        "%s%s %s,%s,%s", #op, (rc)?".":"", gpn(rd), gpn(ra), gpn(rb))
    #define BITALU2(op, ra, rs, rb, rc) EMIT1(PPC_##op | GPR(rs)<<21 | GPR(ra)<<16 | GPR(rb)<<11 | (rc),\
        "%s%s %s,%s,%s", #op, (rc)?".":"", gpn(ra), gpn(rs), gpn(rb))
    #define FPUAB(op, d, a, b, rc) EMIT1(PPC_##op | FPR(d)<<21 | FPR(a)<<16 | FPR(b)<<11 | (rc),\
        "%s%s %s,%s,%s", #op, (rc)?".":"", gpn(d), gpn(a), gpn(b))
    #define FPUAC(op, d, a, c, rc) EMIT1(PPC_##op | FPR(d)<<21 | FPR(a)<<16 | FPR(c)<<6 | (rc),\
        "%s%s %s,%s,%s", #op, (rc)?".":"", gpn(d), gpn(a), gpn(c))

    #define ADD(rd,ra,rb)   ALU2(add,  rd, ra, rb, 0)
    #define ADD_(rd,ra,rb)  ALU2(add,  rd, ra, rb, 1)
    #define ADDO(rd,ra,rb)  ALU2(addo, rd, ra, rb, 0)
    #define ADDO_(rd,ra,rb) ALU2(addo, rd, ra, rb, 1)
    #define SUBF(rd,ra,rb)  ALU2(subf, rd, ra, rb, 0)
    #define SUBF_(rd,ra,rb) ALU2(subf, rd, ra, rb, 1)

    #define AND(rd,rs,rb)   BITALU2(and,  rd, rs, rb, 0)
    #define AND_(rd,rs,rb)  BITALU2(and,  rd, rs, rb, 1)
    #define OR(rd,rs,rb)    BITALU2(or,   rd, rs, rb, 0)
    #define OR_(rd,rs,rb)   BITALU2(or,   rd, rs, rb, 1)
    #define NOR(rd,rs,rb)   BITALU2(nor,  rd, rs, rb, 0)
    #define NOR_(rd,rs,rb)  BITALU2(nor,  rd, rs, rb, 1)
    #define SLW(rd,rs,rb)   BITALU2(slw,  rd, rs, rb, 0)
    #define SLW_(rd,rs,rb)  BITALU2(slw,  rd, rs, rb, 1)
    #define SRW(rd,rs,rb)   BITALU2(srw,  rd, rs, rb, 0)
    #define SRW_(rd,rs,rb)  BITALU2(srw,  rd, rs, rb, 1)
    #define SRAW(rd,rs,rb)  BITALU2(sraw, rd, rs, rb, 0)
    #define SRAW_(rd,rs,rb) BITALU2(sraw, rd, rs, rb, 1)
    #define XOR(rd,rs,rb)   BITALU2(xor,  rd, rs, rb, 0)
    #define XOR_(rd,rs,rb)  BITALU2(xor,  rd, rs, rb, 1)

    #define SLD(rd,rs,rb)   BITALU2(sld,  rd, rs, rb, 0)
    #define SRD(rd,rs,rb)   BITALU2(srd,  rd, rs, rb, 0)
    #define SRAD(rd,rs,rb)  BITALU2(srad, rd, rs, rb, 0)

    #define FADD(rd,ra,rb)  FPUAB(fadd, rd, ra, rb, 0)
    #define FADD_(rd,ra,rb) FPUAB(fadd, rd, ra, rb, 1)
    #define FDIV(rd,ra,rb)  FPUAB(fdiv, rd, ra, rb, 0)
    #define FDIV_(rd,ra,rb) FPUAB(fdiv, rd, ra, rb, 1)
    #define FMUL(rd,ra,rb)  FPUAC(fmul, rd, ra, rb, 0)
    #define FMUL_(rd,ra,rb) FPUAC(fmul, rd, ra, rb, 1)
    #define FSUB(rd,ra,rb)  FPUAB(fsub, rd, ra, rb, 0)
    #define FSUB_(rd,ra,rb) FPUAB(fsub, rd, ra, rb, 1)

    #define MULLI(rd,ra,simm) EMIT1(PPC_mulli | GPR(rd)<<21 | GPR(ra)<<16 | uint16_t(simm),\
        "mulli %s,%s,%d", gpn(rd), gpn(ra), int16_t(simm))
    #define MULLW(rd,ra,rb) EMIT1(PPC_mullw | GPR(rd)<<21 | GPR(ra)<<16 | GPR(rb)<<11,\
        "mullw %s,%s,%s", gpn(rd), gpn(ra), gpn(rb))

    // same as ALU2 with rs=rb, for simplified mnemonics
    #define ALU1(op, ra, rs, rc) EMIT1(PPC_##op | GPR(rs)<<21 | GPR(ra)<<16 | GPR(rs)<<11 | (rc),\
        "%s%s %s,%s", #op, (rc)?".":"", gpn(ra), gpn(rs))

    #define MR(rd, rs)    ALU1(mr,    rd, rs, 0)   // or   rd,rs,rs
    #define MR_(rd, rs)   ALU1(mr,    rd, rs, 1)   // or.  rd,rs,rs
    #define NOT(rd, rs)   ALU1(not,   rd, rs, 0)   // nor  rd,rs,rs
    #define NOT_(rd, rs)  ALU1(not,   rd, rs, 0)   // nor. rd,rs,rs

    #define EXTSW(rd, rs) EMIT1(PPC_extsw | GPR(rs)<<21 | GPR(rd)<<16,\
        "extsw %s,%s", gpn(rd), gpn(rs))

    #define NEG(rd, rs)  EMIT1(PPC_neg | GPR(rd)<<21 | GPR(rs)<<16, "neg %s,%s", gpn(rd), gpn(rs))
    #define FNEG(rd,rs)  EMIT1(PPC_fneg | FPR(rd)<<21 | FPR(rs)<<11, "fneg %s,%s", gpn(rd), gpn(rs))
    #define FMR(rd,rb)   EMIT1(PPC_fmr  | FPR(rd)<<21 | FPR(rb)<<11, "fmr %s,%s", gpn(rd), gpn(rb))
    #define FCFID(rd,rs) EMIT1(PPC_fcfid | FPR(rd)<<21 | FPR(rs)<<11, "fcfid %s,%s", gpn(rd), gpn(rs))

    #define JMP(addr) br(addr, 0)

    #define SPR(spr) ((R##spr)>>5|(R##spr&31)<<5)
    #define MTSPR(spr,rs) EMIT1(PPC_mtspr | GPR(rs)<<21 | SPR(spr)<<11,\
        "mt%s %s", #spr, gpn(rs))
    #define MFSPR(rd,spr) EMIT1(PPC_mfspr | GPR(rd)<<21 | SPR(spr)<<11,\
        "mf%s %s", #spr, gpn(rd))

    #define MTXER(r) MTSPR(xer, r)
    #define MTLR(r)  MTSPR(lr,  r)
    #define MTCTR(r) MTSPR(ctr, r)

    #define MFXER(r) MFSPR(r, xer)
    #define MFLR(r)  MFSPR(r, lr)
    #define MFCTR(r) MFSPR(r, ctr)

    #define MEMd(op, r, d, a) do {\
        NanoAssert(isS16(d));\
        EMIT1(PPC_##op | GPR(r)<<21 | GPR(a)<<16 | uint16_t(d), "%s %s,%d(%s)", #op, gpn(r), int16_t(d), gpn(a));\
        } while(0) /* no addr */

    #define FMEMd(op, r, d, b) do {\
        NanoAssert(isS16(d));\
        EMIT1(PPC_##op | FPR(r)<<21 | GPR(b)<<16 | uint16_t(d), "%s %s,%d(%s)", #op, gpn(r), int16_t(d), gpn(b));\
        } while(0) /* no addr */

    #define MEMx(op, r, a, b) EMIT1(PPC_##op | GPR(r)<<21 | GPR(a)<<16 | GPR(b)<<11,\
        "%s %s,%s,%s", #op, gpn(r), gpn(a), gpn(b))
    #define FMEMx(op, r, a, b) EMIT1(PPC_##op | FPR(r)<<21 | GPR(a)<<16 | GPR(b)<<11,\
        "%s %s,%s,%s", #op, gpn(r), gpn(a), gpn(b))

    #define MEMux(op, rs, ra, rb) EMIT1(PPC_##op | GPR(rs)<<21 | GPR(ra)<<16 | GPR(rb)<<11,\
                "%s %s,%s,%s", #op, gpn(rs), gpn(ra), gpn(rb))

    #define LBZ(r,  d, b) MEMd(lbz,  r, d, b)
    #define LHZ(r,  d, b) MEMd(lhz,  r, d, b)
    #define LWZ(r,  d, b) MEMd(lwz,  r, d, b)
    #define LD(r,   d, b) MEMd(ld,   r, d, b)
    #define LBZX(r, a, b) MEMx(lbzx, r, a, b)
    #define LHZX(r, a, b) MEMx(lhzx, r, a, b)
    #define LWZX(r, a, b) MEMx(lwzx, r, a, b)
    #define LDX(r,  a, b) MEMx(ldx,  r, a, b)

    // store word (32-bit integer)
    #define STW(r,  d, b)     MEMd(stw,    r, d, b)
    #define STWU(r, d, b)     MEMd(stwu,   r, d, b)
    #define STWX(s, a, b)     MEMx(stwx,   s, a, b)
    #define STWUX(s, a, b)    MEMux(stwux, s, a, b)

    // store byte
    #define STB(r,  d, b)     MEMd(stb,    r, d, b)
    #define STBX(s, a, b)     MEMx(stbx,   s, a, b)

    // store double (64-bit float)
    #define STD(r,  d, b)     MEMd(std,    r, d, b)
    #define STDU(r, d, b)     MEMd(stdu,   r, d, b)
    #define STDX(s, a, b)     MEMx(stdx,   s, a, b)
    #define STDUX(s, a, b)    MEMux(stdux, s, a, b)

#ifdef NANOJIT_64BIT
    #define LP(r, d, b)       LD(r, d, b)
    #define STP(r, d, b)      STD(r, d, b)
    #define STPU(r, d, b)     STDU(r, d, b)
    #define STPX(s, a, b)     STDX(s, a, b)
    #define STPUX(s, a, b)    STDUX(s, a, b)
#else
    #define LP(r, d, b)       LWZ(r, d, b)
    #define STP(r, d, b)      STW(r, d, b)
    #define STPU(r, d, b)     STWU(r, d, b)
    #define STPX(s, a, b)     STWX(s, a, b)
    #define STPUX(s, a, b)    STWUX(s, a, b)
#endif

    #define LFD(r,  d, b) FMEMd(lfd,  r, d, b)
    #define LFDX(r, a, b) FMEMx(lfdx, r, a, b)
    #define STFD(r, d, b) FMEMd(stfd, r, d, b)
    #define STFDX(s, a, b) FMEMx(stfdx, s, a, b)

    #define ALUI(op,rd,ra,d) EMIT1(PPC_##op | GPR(rd)<<21 | GPR(ra)<<16 | uint16_t(d),\
                "%s %s,%s,%d (0x%x)", #op, gpn(rd), gpn(ra), int16_t(d), int16_t(d))

    #define ADDI(rd,ra,d)  ALUI(addi,  rd, ra, d)
    #define ADDIS(rd,ra,d) ALUI(addis, rd, ra, d)

    // bitwise operators have different src/dest registers
    #define BITALUI(op,rd,ra,d) EMIT1(PPC_##op | GPR(ra)<<21 | GPR(rd)<<16 | uint16_t(d),\
                "%s %s,%s,%u (0x%x)", #op, gpn(rd), gpn(ra), uint16_t(d), uint16_t(d))

    #define ANDI(rd,ra,d)  BITALUI(andi,  rd, ra, d)
    #define ORI(rd,ra,d)   BITALUI(ori,   rd, ra, d)
    #define ORIS(rd,ra,d)  BITALUI(oris,  rd, ra, d)
    #define XORI(rd,ra,d)  BITALUI(xori,  rd, ra, d)
    #define XORIS(rd,ra,d) BITALUI(xoris, rd, ra, d)

    #define SUBI(rd,ra,d) EMIT1(PPC_addi | GPR(rd)<<21 | GPR(ra)<<16 | uint16_t(-(d)),\
        "subi %s,%s,%d", gpn(rd), gpn(ra), (d))

    #define LI(rd,v) EMIT1(PPC_addi | GPR(rd)<<21 | uint16_t(v),\
        "li %s,%d (0x%x)", gpn(rd), int16_t(v), int16_t(v)) /* addi rd,0,v */

    #define LIS(rd,v) EMIT1(PPC_addis | GPR(rd)<<21 | uint16_t(v),\
        "lis %s,%d (0x%x)", gpn(rd), int16_t(v), int16_t(v)<<16) /* addis, rd,0,v */

    #define MTCR(rs) /* mtcrf 0xff,rs */
    #define MFCR(rd) EMIT1(PPC_mfcr | GPR(rd)<<21, "mfcr %s", gpn(rd))

    #define CMPx(op, crfd, ra, rb, l) EMIT1(PPC_##op | (crfd)<<23 | (l)<<21 | GPR(ra)<<16 | GPR(rb)<<11,\
        "%s%c cr%d,%s,%s", #op, (l)?'d':'w', (crfd), gpn(ra), gpn(rb))

    #define CMPW(cr, ra, rb)   CMPx(cmp,    cr, ra, rb, 0)
    #define CMPLW(cr, ra, rb)  CMPx(cmpl,   cr, ra, rb, 0)
    #define CMPD(cr, ra, rb)   CMPx(cmp,    cr, ra, rb, 1)
    #define CMPLD(cr, ra, rb)  CMPx(cmpl,   cr, ra, rb, 1)

    #define CMPxI(cr, ra, simm, l) EMIT1(PPC_cmpi | (cr)<<23 | (l)<<21 | GPR(ra)<<16 | uint16_t(simm),\
        "cmp%ci cr%d,%s,%d (0x%x)", (l)?'d':'w', (cr), gpn(ra), int16_t(simm), int16_t(simm))

    #define CMPWI(cr, ra, simm) CMPxI(cr, ra, simm, 0)
    #define CMPDI(cr, ra, simm) CMPxI(cr, ra, simm, 1)

    #define CMPLxI(cr, ra, uimm, l) EMIT1(PPC_cmpli | (cr)<<23 | (l)<<21 | GPR(ra)<<16 | uint16_t(uimm),\
        "cmp%ci cr%d,%s,%d (0x%x)", (l)?'d':'w', (cr), gpn(ra), uint16_t(uimm), uint16_t(uimm))

    #define CMPLWI(cr, ra, uimm) CMPLxI(cr, ra, uimm, 0)
    #define CMPLDI(cr, ra, uimm) CMPLxI(cr, ra, uimm, 1)

    #define FCMPx(op, crfd, ra, rb) EMIT1(PPC_##op | (crfd)<<23 | FPR(ra)<<16 | FPR(rb)<<11,\
        "%s cr%d,%s,%s", #op, (crfd), gpn(ra), gpn(rb))

    #define FCMPU(cr, ra, rb) FCMPx(fcmpu, cr, ra, rb)

    #define CROR(cr,d,a,b) EMIT1(PPC_cror | (4*(cr)+COND_##d)<<21 | (4*(cr)+COND_##a)<<16 | (4*(cr)+COND_##b)<<11,\
        "cror %d,%d,%d", 4*(cr)+COND_##d, 4*(cr)+COND_##a, 4*(cr)+COND_##b)

    #define RLWINM(rd,rs,sh,mb,me) EMIT1(PPC_rlwinm | GPR(rs)<<21 | GPR(rd)<<16 | (sh)<<11 | (mb)<<6 | (me)<<1,\
        "rlwinm %s,%s,%d,%d,%d", gpn(rd), gpn(rs), (sh), (mb), (me))

    #define LO5(sh) ((sh) & 31)
    #define BIT6(sh) (((sh) >> 5) & 1)
    #define SPLITMB(mb) (LO5(mb)<<1 | BIT6(mb))

    #define RLDICL(rd,rs,sh,mb) \
        EMIT1(PPC_rldicl | GPR(rs)<<21 | GPR(rd)<<16 | LO5(sh)<<11 | SPLITMB(mb)<<5 | BIT6(sh)<<1,\
        "rldicl %s,%s,%d,%d", gpn(rd), gpn(rs), (sh), (mb))

    // clrldi d,s,n => rldicl d,s,0,n
    #define CLRLDI(rd,rs,n) \
        EMIT1(PPC_rldicl | GPR(rs)<<21 | GPR(rd)<<16 | SPLITMB(n)<<5,\
        "clrldi %s,%s,%d", gpn(rd), gpn(rs), (n))

    #define RLDIMI(rd,rs,sh,mb) \
        EMIT1(PPC_rldimi | GPR(rs)<<21 | GPR(rd)<<16 | LO5(sh)<<11 | SPLITMB(mb)<<5 | BIT6(sh)<<1,\
        "rldimi %s,%s,%d,%d", gpn(rd), gpn(rs), (sh), (mb))

    // insrdi rD,rS,n,b => rldimi rD,rS,64-(b+n),b: insert n bit value into rD starting at b
    #define INSRDI(rd,rs,n,b) \
        EMIT1(PPC_rldimi | GPR(rs)<<21 | GPR(rd)<<16 | LO5(64-((b)+(n)))<<11 | SPLITMB(b)<<5 | BIT6(64-((b)+(n)))<<1,\
        "insrdi %s,%s,%d,%d", gpn(rd), gpn(rs), (n), (b))

    #define EXTRWI(rd,rs,n,b) EMIT1(PPC_rlwinm | GPR(rs)<<21 | GPR(rd)<<16 | ((n)+(b))<<11 | (32-(n))<<6 | 31<<1,\
        "extrwi %s,%s,%d,%d", gpn(rd), gpn(rs), (n), (b))

    // sldi rd,rs,n (n<64) => rldicr rd,rs,n,63-n
    #define SLDI(rd,rs,n) EMIT1(PPC_rldicr | GPR(rs)<<21 | GPR(rd)<<16 | LO5(n)<<11 | SPLITMB(63-(n))<<5 | BIT6(n)<<1,\
        "sldi %s,%s,%d", gpn(rd), gpn(rs), (n))

    #define SLWI(rd,rs,n) EMIT1(PPC_rlwinm | GPR(rs)<<21 | GPR(rd)<<16 | (n)<<11 | 0<<6 | (31-(n))<<1,\
        "slwi %s,%s,%d", gpn(rd), gpn(rs), (n))
    #define SRWI(rd,rs,n) EMIT1(PPC_rlwinm | GPR(rs)<<21 | GPR(rd)<<16 | (32-(n))<<11 | (n)<<6 | 31<<1,\
        "slwi %s,%s,%d", gpn(rd), gpn(rs), (n))
    #define SRAWI(rd,rs,n) EMIT1(PPC_srawi | GPR(rs)<<21 | GPR(rd)<<16 | (n)<<11,\
        "srawi %s,%s,%d", gpn(rd), gpn(rs), (n))

} // namespace nanojit

#endif // __nanojit_NativePPC__
