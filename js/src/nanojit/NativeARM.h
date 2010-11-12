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
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Jacob Bramley <Jacob.Bramley@arm.com>
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


#ifndef __nanojit_NativeARM__
#define __nanojit_NativeARM__


#ifdef PERFM
#include "../vprof/vprof.h"
#define count_instr() _nvprof("arm",1)
#define count_prolog() _nvprof("arm-prolog",1); count_instr();
#define count_imt() _nvprof("arm-imt",1) count_instr()
#else
#define count_instr()
#define count_prolog()
#define count_imt()
#endif

#ifdef DEBUG
#define ARM_ARCH_AT_LEAST(wanted) (_config.arm_arch >= (wanted))
#define ARM_VFP (_config.arm_vfp)
#else
/* Note: Non DEBUG builds will ignore arm_arch if it is lower than
   NJ_COMPILER_ARM_ARCH, and will ignore arm_vfp if NJ_COMPILER_ARM_ARCH
   is greater or equal to 7. */
#define ARM_ARCH_AT_LEAST(wanted) \
    ((NJ_COMPILER_ARM_ARCH >= (wanted)) || (_config.arm_arch >= (wanted)))
#define ARM_VFP ((NJ_COMPILER_ARM_ARCH >= 7) || (_config.arm_vfp))
#endif

namespace nanojit
{
#if defined VMCFG_DOUBLE_MSW_FIRST || defined _MSC_VER
#  undef  NJ_ARM_EABI
#else
#  define NJ_ARM_EABI  1
#endif

// GCC defines __ARM_PCS_VFP if it uses hardware floating point ABI
// See http://gcc.gnu.org/viewcvs?view=revision&revision=162637
#ifdef __ARM_PCS_VFP
#  define NJ_ARM_EABI_HARD_FLOAT 1
#endif

#ifdef NJ_ARM_EABI_HARD_FLOAT
#  define ARM_EABI_HARD true
#else
#  define ARM_EABI_HARD false
#endif

// only d0-d7 are used; in addition, we'll use d0 as s0-s1 for i2d/u2f/etc.
#define NJ_VFP_MAX_REGISTERS            8
#define NJ_MAX_REGISTERS                (11 + NJ_VFP_MAX_REGISTERS)
#define NJ_MAX_STACK_ENTRY              4096
#define NJ_MAX_PARAMETERS               16
#define NJ_ALIGN_STACK                  8

#define NJ_JTBL_SUPPORTED               1
#define NJ_EXPANDED_LOADSTORE_SUPPORTED 1
#define NJ_F2I_SUPPORTED                1
#define NJ_SOFTFLOAT_SUPPORTED          1
#define NJ_DIVI_SUPPORTED               0

#define NJ_CONSTANT_POOLS
const int NJ_MAX_CPOOL_OFFSET = 4096;
const int NJ_CPOOL_SIZE = 16;

const int LARGEST_UNDERRUN_PROT = 32;  // largest value passed to underrunProtect

typedef int NIns;

// Bytes of icache to flush after Assembler::patch
const size_t LARGEST_BRANCH_PATCH = 2 * sizeof(NIns);

/* ARM registers */
typedef uint32_t Register;
static const Register
    R0  = { 0 },
    R1  = { 1 },
    R2  = { 2 },
    R3  = { 3 },
    R4  = { 4 },
    R5  = { 5 },
    R6  = { 6 },
    R7  = { 7 },
    R8  = { 8 },
    R9  = { 9 },
    R10 = { 10 },
    FP  = { 11 },
    IP  = { 12 },
    SP  = { 13 },
    LR  = { 14 },
    PC  = { 15 },

    // VFP regs (we currently only use D0-D7 and S0)
    D0 = { 16 },
    D1 = { 17 },
    D2 = { 18 },
    D3 = { 19 },
    D4 = { 20 },
    D5 = { 21 },
    D6 = { 22 },
    D7 = { 23 },
    // D8-D15 are caller-saved registers that we don't currently handle.

    FirstFloatReg = D0,
    LastFloatReg = D7,

    deprecated_UnknownReg = { 32 },     // XXX: remove eventually, see bug 538924

    // S0 overlaps with D0 and is hard-coded into i2d and u2f operations
    S0 = { 24 },

    SBZ = { 0 } ;   // Used for 'should-be-zero' fields in instructions with
                    // unused register fields.

/* winmo builds error with C2057 and C2229 on usage of First/LastRegNum as R0/D7 */
static const uint32_t FirstRegNum = 0; /* R0 */
static const uint32_t LastRegNum = 23; /* D7 */
}

#define NJ_USE_UINT32_REGISTER 1
#include "NativeCommon.h"

namespace nanojit
{

/* ARM condition codes */
typedef enum {
    EQ = 0x0, // Equal
    NE = 0x1, // Not Equal
    CS = 0x2, // Carry Set (or HS)
    HS = 0x2,
    CC = 0x3, // Carry Clear (or LO)
    LO = 0x3,
    MI = 0x4, // MInus
    PL = 0x5, // PLus
    VS = 0x6, // oVerflow Set
    VC = 0x7, // oVerflow Clear
    HI = 0x8, // HIgher
    LS = 0x9, // Lower or Same
    GE = 0xA, // Greater or Equal
    LT = 0xB, // Less Than
    GT = 0xC, // Greater Than
    LE = 0xD, // Less or Equal
    AL = 0xE, // ALways

    // Note that condition code NV is unpredictable on ARMv3 and ARMv4, and has
    // special meaning for ARMv5 onwards. As such, it should never be used in
    // an instruction encoding unless the special (ARMv5+) meaning is required.
    NV = 0xF  // NeVer
} ConditionCode;
#define IsCond(cc)        (((cc) >= EQ) && ((cc) <= AL))

// Bit 0 of the condition code can be flipped to obtain the opposite condition.
// However, this won't work for AL because its opposite — NV — has special
// meaning.
#define OppositeCond(cc)  ((ConditionCode)((unsigned int)(cc)^0x1))

typedef int RegisterMask;
typedef struct _FragInfo {
    RegisterMask    needRestoring;
    NIns*           epilogue;
} FragInfo;

typedef struct _ParameterRegisters {
    int stkd;
    Register r;
#ifdef NJ_ARM_EABI_HARD_FLOAT
    Register float_r;
#endif
} ParameterRegisters;

#ifdef NJ_ARM_EABI_HARD_FLOAT
#define init_params(a,b,c) { (a), (b), (c) }
#else
#define init_params(a,b,c) { (a), (b) }
#endif

// D0-D7 are not saved; D8-D15 are, but we don't use those,
// so we don't have to worry about saving/restoring them
static const RegisterMask SavedFpRegs = 0;
static const RegisterMask SavedRegs = 1<<R4 | 1<<R5 | 1<<R6 | 1<<R7 | 1<<R8 | 1<<R9 | 1<<R10;
static const int NumSavedRegs = 7;

static const RegisterMask FpRegs = 1<<D0 | 1<<D1 | 1<<D2 | 1<<D3 | 1<<D4 | 1<<D5 | 1<<D6 | 1<<D7;
static const RegisterMask GpRegs = 0xFFFF;
static const RegisterMask AllowableFlagRegs = 1<<R0 | 1<<R1 | 1<<R2 | 1<<R3 | 1<<R4 | 1<<R5 | 1<<R6 | 1<<R7 | 1<<R8 | 1<<R9 | 1<<R10;

#define isU12(offs) (((offs) & 0xfff) == (offs))

#define IsFpReg(_r)     ((rmask((Register)_r) & (FpRegs)) != 0)
#define IsGpReg(_r)     ((rmask((Register)_r) & (GpRegs)) != 0)
#define FpRegNum(_fpr)  ((_fpr) - FirstFloatReg)

#define firstreg()      R0
// only good for normal regs
#define imm2register(c) (Register)(c-1)

verbose_only( extern const char* regNames[]; )
verbose_only( extern const char* condNames[]; )
verbose_only( extern const char* shiftNames[]; )

// abstract to platform specific calls
#define nExtractPlatformFlags(x)    0

#define DECLARE_PLATFORM_STATS()

#define DECLARE_PLATFORM_REGALLOC()

#ifdef DEBUG
# define DECLARE_PLATFORM_ASSEMBLER_DEBUG()                             \
    inline bool         isOp2Imm(uint32_t literal);                     \
    inline uint32_t     decOp2Imm(uint32_t enc);
#else
// define stubs, for code that defines NJ_VERBOSE without DEBUG
# define DECLARE_PLATFORM_ASSEMBLER_DEBUG()                             \
    inline bool         isOp2Imm(uint32_t ) { return true; }            \
    inline uint32_t     decOp2Imm(uint32_t ) { return 0; }
#endif

#define DECLARE_PLATFORM_ASSEMBLER()                                            \
                                                                                \
    DECLARE_PLATFORM_ASSEMBLER_DEBUG()                                          \
                                                                                \
    const static Register argRegs[4], retRegs[2];                               \
                                                                                \
    void        BranchWithLink(NIns* addr);                                     \
    inline void BLX(Register addr, bool chk = true);                            \
    void        JMP_far(NIns*);                                                 \
    void        B_cond_chk(ConditionCode, NIns*, bool);                         \
    void        underrunProtect(int bytes);                                     \
    void        nativePageReset();                                              \
    void        nativePageSetup();                                              \
    bool        hardenNopInsertion(const Config& /*c*/) { return false; }       \
    void        asm_immd_nochk(Register, int32_t, int32_t);                     \
    void        asm_regarg(ArgType, LIns*, Register);                           \
    void        asm_stkarg(LIns* p, int stkd);                                  \
    void        asm_cmpi(Register, int32_t imm);                                \
    void        asm_ldr_chk(Register d, Register b, int32_t off, bool chk);     \
    int32_t     asm_str(Register rt, Register rr, int32_t off);                 \
    void        asm_cmp(LIns *cond);                                            \
    void        asm_cmpd(LIns *cond);                                           \
    void        asm_ld_imm(Register d, int32_t imm, bool chk = true);           \
    void        asm_arg(ArgType ty, LIns* arg, ParameterRegisters& params);     \
    void        asm_arg_64(LIns* arg, ParameterRegisters& params);              \
    void        asm_add_imm(Register rd, Register rn, int32_t imm, int stat = 0);   \
    void        asm_sub_imm(Register rd, Register rn, int32_t imm, int stat = 0);   \
    void        asm_and_imm(Register rd, Register rn, int32_t imm, int stat = 0);   \
    void        asm_orr_imm(Register rd, Register rn, int32_t imm, int stat = 0);   \
    void        asm_eor_imm(Register rd, Register rn, int32_t imm, int stat = 0);   \
    inline bool     encOp2Imm(uint32_t literal, uint32_t * enc);                \
    inline uint32_t CountLeadingZeroes(uint32_t data);                          \
    int *       _nSlot;                                                         \
    int *       _nExitSlot;                                                     \
    int         max_out_args; /* bytes */

#define IMM32(imm)  *(--_nIns) = (NIns)((imm));

#define OP_IMM  (1<<25)
#define OP_STAT (1<<20)

#define COND_AL ((uint32_t)AL<<28)

typedef enum {
    LSL_imm = 0, // LSL #c - Logical Shift Left
    LSL_reg = 1, // LSL Rc - Logical Shift Left
    LSR_imm = 2, // LSR #c - Logical Shift Right
    LSR_reg = 3, // LSR Rc - Logical Shift Right
    ASR_imm = 4, // ASR #c - Arithmetic Shift Right
    ASR_reg = 5, // ASR Rc - Arithmetic Shift Right
    ROR_imm = 6, // Rotate Right (c != 0)
    RRX     = 6, // Rotate Right one bit with extend (c == 0)
    ROR_reg = 7  // Rotate Right
} ShiftOperator;
#define IsShift(sh)    (((sh) >= LSL_imm) && ((sh) <= ROR_reg))

#define LD32_size 8

#define BEGIN_NATIVE_CODE(x)                    \
    { DWORD* _nIns = (uint8_t*)x

#define END_NATIVE_CODE(x)                      \
    (x) = (dictwordp*)_nIns; }

// BX
#define BX(_r)  do {                                                    \
        underrunProtect(4);                                             \
        NanoAssert(IsGpReg(_r));                                        \
        *(--_nIns) = (NIns)( COND_AL | (0x12<<20) | (0xFFF<<8) | (1<<4) | (_r)); \
        asm_output("bx %s", gpn(_r)); } while(0)

/*
 * ALU operations
 */

enum {
    ARM_and = 0,
    ARM_eor = 1,
    ARM_sub = 2,
    ARM_rsb = 3,
    ARM_add = 4,
    ARM_adc = 5,
    ARM_sbc = 6,
    ARM_rsc = 7,
    ARM_tst = 8,
    ARM_teq = 9,
    ARM_cmp = 10,
    ARM_cmn = 11,
    ARM_orr = 12,
    ARM_mov = 13,
    ARM_bic = 14,
    ARM_mvn = 15
};
#define IsOp(op)      (((ARM_##op) >= ARM_and) && ((ARM_##op) <= ARM_mvn))

// ALU operation with register and 8-bit immediate arguments
//  S   - bit, 0 or 1, whether the CPSR register is updated
//  rd  - destination register
//  rl  - first (left) operand register
//  op2imm  - operand 2 immediate. Use encOp2Imm (from NativeARM.cpp) to calculate this.
#define ALUi(cond, op, S, rd, rl, op2imm)   ALUi_chk(cond, op, S, rd, rl, op2imm, 1)
#define ALUi_chk(cond, op, S, rd, rl, op2imm, chk) do {\
        if (chk) underrunProtect(4);\
        NanoAssert(IsCond(cond));\
        NanoAssert(IsOp(op));\
        NanoAssert(((S)==0) || ((S)==1));\
        NanoAssert(IsGpReg(rd) && IsGpReg(rl));\
        NanoAssert(isOp2Imm(op2imm));\
        *(--_nIns) = (NIns) ((cond)<<28 | OP_IMM | (ARM_##op)<<21 | (S)<<20 | (rl)<<16 | (rd)<<12 | (op2imm));\
        if (ARM_##op == ARM_mov || ARM_##op == ARM_mvn) {               \
            asm_output("%s%s%s %s, #0x%X", #op, condNames[cond], (S)?"s":"", gpn(rd), decOp2Imm(op2imm));\
        } else if (ARM_##op >= ARM_tst && ARM_##op <= ARM_cmn) {         \
            NanoAssert(S==1);\
            asm_output("%s%s %s, #0x%X", #op, condNames[cond], gpn(rl), decOp2Imm(op2imm));\
        } else {                                                        \
            asm_output("%s%s%s %s, %s, #0x%X", #op, condNames[cond], (S)?"s":"", gpn(rd), gpn(rl), decOp2Imm(op2imm));\
        }\
    } while (0)

// ALU operation with two register arguments
//  S   - bit, 0 or 1, whether the CPSR register is updated
//  rd  - destination register
//  rl  - first (left) operand register
//  rr  - first (left) operand register
#define ALUr(cond, op, S, rd, rl, rr)   ALUr_chk(cond, op, S, rd, rl, rr, 1)
#define ALUr_chk(cond, op, S, rd, rl, rr, chk) do {\
        if (chk) underrunProtect(4);\
        NanoAssert(IsCond(cond));\
        NanoAssert(IsOp(op));\
        NanoAssert(((S)==0) || ((S)==1));\
        NanoAssert(IsGpReg(rd) && IsGpReg(rl) && IsGpReg(rr));\
        *(--_nIns) = (NIns) ((cond)<<28 |(ARM_##op)<<21 | (S)<<20 | (rl)<<16 | (rd)<<12 | (rr));\
        if (ARM_##op == ARM_mov || ARM_##op == ARM_mvn) {               \
            asm_output("%s%s%s %s, %s", #op, condNames[cond], (S)?"s":"", gpn(rd), gpn(rr));\
        } else if (ARM_##op >= ARM_tst && ARM_##op <= ARM_cmn) {         \
            NanoAssert(S==1);\
            asm_output("%s%s  %s, %s", #op, condNames[cond], gpn(rl), gpn(rr));\
        } else {                                                        \
            asm_output("%s%s%s %s, %s, %s", #op, condNames[cond], (S)?"s":"", gpn(rd), gpn(rl), gpn(rr));\
        }\
    } while (0)

// ALU operation with two register arguments, with rr operated on by a shift and shift immediate
//  S   - bit, 0 or 1, whether the CPSR register is updated
//  rd  - destination register
//  rl  - first (left) operand register
//  rr  - second (right) operand register
//  sh  - a ShiftOperator
//  imm - immediate argument to shift operator, 5 bits (0..31)
#define ALUr_shi(cond, op, S, rd, rl, rr, sh, imm) do {\
        underrunProtect(4);\
        NanoAssert(IsCond(cond));\
        NanoAssert(IsOp(op));\
        NanoAssert(((S)==0) || ((S)==1));\
        NanoAssert(IsGpReg(rd) && IsGpReg(rl) && IsGpReg(rr));\
        NanoAssert(IsShift(sh));\
        NanoAssert((imm)>=0 && (imm)<32);\
        *(--_nIns) = (NIns) ((cond)<<28 |(ARM_##op)<<21 | (S)<<20 | (rl)<<16 | (rd)<<12 | (imm)<<7 | (sh)<<4 | (rr));\
        if (ARM_##op == ARM_mov || ARM_##op == ARM_mvn) {               \
            NanoAssert(rl==0);                                          \
            asm_output("%s%s%s %s, %s, %s #%d", #op, condNames[cond], (S)?"s":"", gpn(rd), gpn(rr), shiftNames[sh], (imm));\
        } else if (ARM_##op >= ARM_tst && ARM_##op <= ARM_cmn) {         \
            NanoAssert(S==1);\
            NanoAssert(rd==0);\
            asm_output("%s%s  %s, %s, %s #%d", #op, condNames[cond], gpn(rl), gpn(rr), shiftNames[sh], (imm));\
        } else {                                                        \
            asm_output("%s%s%s %s, %s, %s, %s #%d", #op, condNames[cond], (S)?"s":"", gpn(rd), gpn(rl), gpn(rr), shiftNames[sh], (imm));\
        }\
    } while (0)

// ALU operation with two register arguments, with rr operated on by a shift and shift register
//  S   - bit, 0 or 1, whether the CPSR register is updated
//  rd  - destination register
//  rl  - first (left) operand register
//  rr  - first (left) operand register
//  sh  - a ShiftOperator
//  rs  - shift operand register
#define ALUr_shr(cond, op, S, rd, rl, rr, sh, rs) do {\
        underrunProtect(4);\
        NanoAssert(IsCond(cond));\
        NanoAssert(IsOp(op));\
        NanoAssert(((S)==0) || ((S)==1));\
        NanoAssert(IsGpReg(rd) && IsGpReg(rl) && IsGpReg(rr) && IsGpReg(rs));\
        NanoAssert(IsShift(sh));\
        *(--_nIns) = (NIns) ((cond)<<28 |(ARM_##op)<<21 | (S)<<20 | (rl)<<16 | (rd)<<12 | (rs)<<8 | (sh)<<4 | (rr));\
        if (ARM_##op == ARM_mov || ARM_##op == ARM_mvn) {               \
            asm_output("%s%s%s %s, %s, %s %s", #op, condNames[cond], (S)?"s":"", gpn(rd), gpn(rr), shiftNames[sh], gpn(rs));\
        } else if (ARM_##op >= ARM_tst && ARM_##op <= ARM_cmn) {         \
            NanoAssert(S==1);\
            asm_output("%s%s  %s, %s, %s %s", #op, condNames[cond], gpn(rl), gpn(rr), shiftNames[sh], gpn(rs));\
        } else {                                                        \
            asm_output("%s%s%s %s, %s, %s, %s %s", #op, condNames[cond], (S)?"s":"", gpn(rd), gpn(rl), gpn(rr), shiftNames[sh], gpn(rs));\
        }\
    } while (0)

// --------
// Basic arithmetic operations.
// --------
// Argument naming conventions for these macros:
//  _d      Destination register.
//  _l      First (left) operand.
//  _r      Second (right) operand.
//  _op2imm An operand 2 immediate value. Use encOp2Imm to calculate this.
//  _s      Set to 1 to update the status flags (for subsequent conditional
//          tests). Otherwise, set to 0.

// _d = _l + decOp2Imm(_op2imm)
#define ADDis(_d,_l,_op2imm,_s) ALUi(AL, add, _s, _d, _l, _op2imm)
#define ADDi(_d,_l,_op2imm)     ALUi(AL, add,  0, _d, _l, _op2imm)

// _d = _l & ~decOp2Imm(_op2imm)
#define BICis(_d,_l,_op2imm,_s) ALUi(AL, bic, _s, _d, _l, _op2imm)
#define BICi(_d,_l,_op2imm)     ALUi(AL, bic,  0, _d, _l, _op2imm)

// _d = _l - decOp2Imm(_op2imm)
#define SUBis(_d,_l,_op2imm,_s) ALUi(AL, sub, _s, _d, _l, _op2imm)
#define SUBi(_d,_l,_op2imm)     ALUi(AL, sub,  0, _d, _l, _op2imm)

// _d = _l & decOp2Imm(_op2imm)
#define ANDis(_d,_l,_op2imm,_s) ALUi(AL, and, _s, _d, _l, _op2imm)
#define ANDi(_d,_l,_op2imm)     ALUi(AL, and,  0, _d, _l, _op2imm)

// _d = _l | decOp2Imm(_op2imm)
#define ORRis(_d,_l,_op2imm,_s) ALUi(AL, orr, _s, _d, _l, _op2imm)
#define ORRi(_d,_l,_op2imm)     ALUi(AL, orr,  0, _d, _l, _op2imm)

// _d = _l ^ decOp2Imm(_op2imm)
#define EORis(_d,_l,_op2imm,_s) ALUi(AL, eor, _s, _d, _l, _op2imm)
#define EORi(_d,_l,_op2imm)     ALUi(AL, eor,  0, _d, _l, _op2imm)

// _d = _l | _r
#define ORRs(_d,_l,_r,_s)   ALUr(AL, orr, _s, _d, _l, _r)
#define ORR(_d,_l,_r)       ALUr(AL, orr,  0, _d, _l, _r)

// _d = _l & _r
#define ANDs(_d,_l,_r,_s)   ALUr(AL, and, _s, _d, _l, _r)
#define AND(_d,_l,_r)       ALUr(AL, and,  0, _d, _l, _r)

// _d = _l ^ _r
#define EORs(_d,_l,_r,_s)   ALUr(AL, eor, _s, _d, _l, _r)
#define EOR(_d,_l,_r)       ALUr(AL, eor,  0, _d, _l, _r)

// _d = _l + _r
#define ADDs(_d,_l,_r,_s)   ALUr(AL, add, _s, _d, _l, _r)
#define ADD(_d,_l,_r)       ALUr(AL, add,  0, _d, _l, _r)

// _d = _l - _r
#define SUBs(_d,_l,_r,_s)   ALUr(AL, sub, _s, _d, _l, _r)
#define SUB(_d,_l,_r)       ALUr(AL, sub,  0, _d, _l, _r)

// --------
// Other operations.
// --------

// [_d_hi,_d] = _l * _r
#define SMULL(_d, _d_hi, _l, _r)  do {                                                          \
        underrunProtect(4);                                                                     \
        NanoAssert(ARM_ARCH_AT_LEAST(6) || ((_d   ) != (_l)));                               \
        NanoAssert(ARM_ARCH_AT_LEAST(6) || ((_d_hi) != (_l)));                               \
        NanoAssert(IsGpReg(_d) && IsGpReg(_d_hi) && IsGpReg(_l) && IsGpReg(_r));                \
        NanoAssert(((_d) != PC) && ((_d_hi) != PC) && ((_l) != PC) && ((_r) != PC));            \
        *(--_nIns) = (NIns)( COND_AL | 0xc00090 | (_d_hi)<<16 | (_d)<<12 | (_r)<<8 | (_l) );    \
        asm_output("smull %s, %s, %s, %s",gpn(_d),gpn(_d_hi),gpn(_l),gpn(_r));                  \
} while(0)

// _d = _l * _r
#define MUL(_d, _l, _r)  do {                                               \
        underrunProtect(4);                                                 \
        NanoAssert(ARM_ARCH_AT_LEAST(6) || ((_d) != (_l)));              \
        NanoAssert(IsGpReg(_d) && IsGpReg(_l) && IsGpReg(_r));              \
        NanoAssert(((_d) != PC) && ((_l) != PC) && ((_r) != PC));           \
        *(--_nIns) = (NIns)( COND_AL | (_d)<<16 | (_r)<<8 | 0x90 | (_l) );  \
        asm_output("mul %s, %s, %s",gpn(_d),gpn(_l),gpn(_r));               \
} while(0)

// RSBS _d, _r
// _d = 0 - _r
#define RSBS(_d,_r) ALUi(AL, rsb, 1, _d, _r, 0)

// MVN
// _d = ~_r (one's compliment)
#define MVN(_d,_r)                          ALUr(AL, mvn, 0, _d, 0, _r)
#define MVNis_chk(_d,_op2imm,_stat,_chk)    ALUi_chk(AL, mvn, _stat, _d, 0, op2imm, _chk)
#define MVNis(_d,_op2imm,_stat)             MVNis_chk(_d,_op2imm,_stat,1);

// Logical Shift Right (LSR) rotates the bits without maintaining sign extensions.
// MOVS _d, _r, LSR <_s>
// _d = _r >> _s
#define LSR(_d,_r,_s) ALUr_shr(AL, mov, 1, _d, 0, _r, LSR_reg, _s)

// Logical Shift Right (LSR) rotates the bits without maintaining sign extensions.
// MOVS _d, _r, LSR #(_imm & 0x1f)
// _d = _r >> (_imm & 0x1f)
#define LSRi(_d,_r,_imm)  ALUr_shi(AL, mov, 1, _d, 0, _r, LSR_imm, (_imm & 0x1f))

// Arithmetic Shift Right (ASR) maintains the sign extension.
// MOVS _d, _r, ASR <_s>
// _d = _r >> _s
#define ASR(_d,_r,_s) ALUr_shr(AL, mov, 1, _d, 0, _r, ASR_reg, _s)

// Arithmetic Shift Right (ASR) maintains the sign extension.
// MOVS _r, _r, ASR #(_imm & 0x1f)
// _d = _r >> (_imm & 0x1f)
#define ASRi(_d,_r,_imm) ALUr_shi(AL, mov, 1, _d, 0, _r, ASR_imm, (_imm & 0x1f))

// Logical Shift Left (LSL).
// MOVS _d, _r, LSL <_s>
// _d = _r << _s
#define LSL(_d, _r, _s) ALUr_shr(AL, mov, 1, _d, 0, _r, LSL_reg, _s)

// Logical Shift Left (LSL).
// MOVS _d, _r, LSL #(_imm & 0x1f)
// _d = _r << (_imm & 0x1f)
#define LSLi(_d, _r, _imm) ALUr_shi(AL, mov, 1, _d, 0, _r, LSL_imm, (_imm & 0x1f))

// TST
#define TST(_l,_r)      ALUr(AL, tst, 1, 0, _l, _r)
#define TSTi(_d,_imm)   ALUi(AL, tst, 1, 0, _d, _imm)

// CMP
#define CMP(_l,_r)  ALUr(AL, cmp, 1, 0, _l, _r)
#define CMN(_l,_r)  ALUr(AL, cmn, 1, 0, _l, _r)

// MOV
#define MOVis_chk(_d,_op2imm,_stat,_chk)    ALUi_chk(AL, mov, _stat, _d, 0, op2imm, _chk)
#define MOVis(_d,_op2imm,_stat)             MOVis_chk(_d,_op2imm,_stat,1)
#define MOVi(_d,_op2imm)                    MOVis(_d,_op2imm,0);

#define MOV_cond(_cond,_d,_s)               ALUr(_cond, mov, 0, _d, 0, _s)

#define MOV(dr,sr)   MOV_cond(AL, dr, sr)

// _d = [_b+off]
#define LDR(_d,_b,_off)        asm_ldr_chk(_d,_b,_off,1)
#define LDR_nochk(_d,_b,_off)  asm_ldr_chk(_d,_b,_off,0)

// _d = [_b + _x<<_s]
#define LDR_scaled(_d, _b, _x, _s) do { \
        NanoAssert(((_s)&31) == _s);\
        NanoAssert(IsGpReg(_d) && IsGpReg(_b) && IsGpReg(_x));\
        underrunProtect(4);\
        *(--_nIns) = (NIns)(COND_AL | (0x79<<20) | ((_b)<<16) | ((_d)<<12) | ((_s)<<7) | (_x));\
        asm_output("ldr %s, [%s, +%s, LSL #%d]", gpn(_d), gpn(_b), gpn(_x), (_s));\
    } while (0)

// _d = #_imm
#define LDi(_d,_imm) asm_ld_imm(_d,_imm)

// MOVW and MOVT are ARMv6T2 or newer only

// MOVW -- writes _imm into _d, zero-extends.
#define MOVWi_cond_chk(_cond,_d,_imm,_chk) do {                         \
        NanoAssert(isU16(_imm));                                        \
        NanoAssert(IsGpReg(_d));                                        \
        NanoAssert(IsCond(_cond));                                      \
        if (_chk) underrunProtect(4);                                   \
        *(--_nIns) = (NIns)( (_cond)<<28 | 3<<24 | 0<<20 | (((_imm)>>12)&0xf)<<16 | (_d)<<12 | ((_imm)&0xfff) ); \
        asm_output("movw%s %s, #0x%x", condNames[_cond], gpn(_d), (_imm)); \
    } while (0)

#define MOVWi(_d,_imm)              MOVWi_cond_chk(AL, _d, _imm, 1)
#define MOVWi_chk(_d,_imm,_chk)     MOVWi_cond_chk(AL, _d, _imm, _chk)
#define MOVWi_cond(_cond,_d,_imm)   MOVWi_cond_chk(_cond, _d, _imm, 1)

// MOVT -- writes _imm into top halfword of _d, does not affect bottom halfword
#define MOVTi_cond_chk(_cond,_d,_imm,_chk) do {                         \
        NanoAssert(isU16(_imm));                                        \
        NanoAssert(IsGpReg(_d));                                        \
        NanoAssert(IsCond(_cond));                                      \
        if (_chk) underrunProtect(4);                                   \
        *(--_nIns) = (NIns)( (_cond)<<28 | 3<<24 | 4<<20 | (((_imm)>>12)&0xf)<<16 | (_d)<<12 | ((_imm)&0xfff) ); \
        asm_output("movt%s %s, #0x%x", condNames[_cond], gpn(_d), (_imm)); \
    } while (0)

#define MOVTi(_d,_imm)              MOVTi_cond_chk(AL, _d, _imm, 1)
#define MOVTi_chk(_d,_imm,_chk)     MOVTi_cond_chk(AL, _d, _imm, _chk)
#define MOVTi_cond(_cond,_d,_imm)   MOVTi_cond_chk(_cond, _d, _imm, 1)

// i386 compat, for Assembler.cpp
#define MR(d,s)                     MOV(d,s)
#define ST(base,offset,reg)         STR(reg,base,offset)

// Load a byte (8 bits). The offset range is ±4095.
#define LDRB(_d,_n,_off) do {                                           \
        NanoAssert(IsGpReg(_d) && IsGpReg(_n));                         \
        underrunProtect(4);                                             \
        if (_off < 0) {                                                 \
            NanoAssert(isU12(-(_off)));                                 \
            *(--_nIns) = (NIns)( COND_AL | (0x55<<20) | ((_n)<<16) | ((_d)<<12) | ((-(_off))&0xfff)  ); \
        } else {                                                        \
            NanoAssert(isU12(_off));                                    \
            *(--_nIns) = (NIns)( COND_AL | (0x5D<<20) | ((_n)<<16) | ((_d)<<12) | ((_off)&0xfff)  ); \
        }                                                               \
        asm_output("ldrb %s, [%s,#%d]", gpn(_d),gpn(_n),(_off));        \
    } while(0)

// Load a byte (8 bits), sign-extend to 32 bits. The offset range is
// ±255 (different from LDRB, same as LDRH/LDRSH)
#define LDRSB(_d,_n,_off) do {                                          \
        NanoAssert(IsGpReg(_d) && IsGpReg(_n));                         \
        underrunProtect(4);                                             \
        if (_off < 0) {                                                 \
            NanoAssert(isU8(-(_off)));                                  \
            *(--_nIns) = (NIns)( COND_AL | (0x15<<20) | ((_n)<<16) | ((_d)<<12) | ((0xD)<<4) | (((-(_off))&0xf0)<<4) | ((-(_off))&0xf) ); \
        } else {                                                        \
            NanoAssert(isU8(_off));                                     \
            *(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_n)<<16) | ((_d)<<12) | ((0xD)<<4) | (((_off)&0xf0)<<4) | ((_off)&0xf) ); \
        }                                                               \
        asm_output("ldrsb %s, [%s,#%d]", gpn(_d),gpn(_n),(_off));       \
    } while(0)

// Load and sign-extend a half word (16 bits). The offset range is ±255, and
// must be aligned to two bytes on some architectures (the caller is responsible
// for ensuring appropriate alignment)
#define LDRH(_d,_n,_off) do {                                           \
        NanoAssert(IsGpReg(_d) && IsGpReg(_n));                         \
        underrunProtect(4);                                             \
        if (_off < 0) {                                                 \
            NanoAssert(isU8(-(_off)));                                  \
            *(--_nIns) = (NIns)( COND_AL | (0x15<<20) | ((_n)<<16) | ((_d)<<12) | ((0xB)<<4) | (((-(_off))&0xf0)<<4) | ((-(_off))&0xf) ); \
        } else {                                                        \
            NanoAssert(isU8(_off));                                     \
            *(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_n)<<16) | ((_d)<<12) | ((0xB)<<4) | (((_off)&0xf0)<<4) | ((_off)&0xf) ); \
        }                                                               \
        asm_output("ldrh %s, [%s,#%d]", gpn(_d),gpn(_n),(_off));       \
    } while(0)

// Load and sign-extend a half word (16 bits). The offset range is ±255, and
// must be aligned to two bytes on some architectures (the caller is responsible
// for ensuring appropriate alignment)
#define LDRSH(_d,_n,_off) do {                                          \
        NanoAssert(IsGpReg(_d) && IsGpReg(_n));                         \
        underrunProtect(4);                                             \
        if (_off < 0) {                                                 \
            NanoAssert(isU8(-(_off)));                                  \
            *(--_nIns) = (NIns)( COND_AL | (0x15<<20) | ((_n)<<16) | ((_d)<<12) | ((0xF)<<4) | (((-(_off))&0xf0)<<4) | ((-(_off))&0xf) ); \
        } else {                                                        \
            NanoAssert(isU8(_off));                                     \
            *(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_n)<<16) | ((_d)<<12) | ((0xF)<<4) | (((_off)&0xf0)<<4) | ((_off)&0xf) ); \
        }                                                               \
        asm_output("ldrsh %s, [%s,#%d]", gpn(_d),gpn(_n),(_off));       \
    } while(0)

// Valid offset for STR and STRB is +/- 4095, STRH only has +/- 255
#define STR(_d,_n,_off) do {                                            \
        NanoAssert(IsGpReg(_d) && IsGpReg(_n));                         \
        NanoAssert(isU12(_off) || isU12(-(_off)));                      \
        underrunProtect(4);                                             \
        if ((_off)<0)   *(--_nIns) = (NIns)( COND_AL | (0x50<<20) | ((_n)<<16) | ((_d)<<12) | ((-(_off))&0xFFF) ); \
        else            *(--_nIns) = (NIns)( COND_AL | (0x58<<20) | ((_n)<<16) | ((_d)<<12) | ((_off)&0xFFF) ); \
        asm_output("str %s, [%s, #%d]", gpn(_d), gpn(_n), (_off)); \
    } while(0)

#define STRB(_d,_n,_off) do {                                           \
        NanoAssert(IsGpReg(_d) && IsGpReg(_n));                         \
        NanoAssert(isU12(_off) || isU12(-(_off)));                      \
        underrunProtect(4);                                             \
        if ((_off)<0)   *(--_nIns) = (NIns)( COND_AL | (0x54<<20) | ((_n)<<16) | ((_d)<<12) | ((-(_off))&0xFFF) ); \
        else            *(--_nIns) = (NIns)( COND_AL | (0x5C<<20) | ((_n)<<16) | ((_d)<<12) | ((_off)&0xFFF) ); \
        asm_output("strb %s, [%s, #%d]", gpn(_d), gpn(_n), (_off)); \
    } while(0)

// Only +/- 255 range, unlike STRB/STR
#define STRH(_d,_n,_off) do {                                           \
        NanoAssert(IsGpReg(_d) && IsGpReg(_n));                         \
        underrunProtect(4);                                             \
        if ((_off)<0) {                                                 \
            NanoAssert(isU8(-(_off)));                                  \
            *(--_nIns) = (NIns)( COND_AL | (0x14<<20) | ((_n)<<16) | ((_d)<<12) | (((-(_off))&0xF0)<<4) | (0xB<<4) | ((-(_off))&0xF) ); \
        } else {                                                        \
            NanoAssert(isU8(_off));                                     \
            *(--_nIns) = (NIns)( COND_AL | (0x1C<<20) | ((_n)<<16) | ((_d)<<12) | (((_off)&0xF0)<<4) | (0xB<<4) | ((_off)&0xF) ); \
        }                                                               \
        asm_output("strh %s, [%s, #%d]", gpn(_d), gpn(_n), (_off));     \
    } while(0)

// Encode a breakpoint. The ID is not important and is ignored by the
// processor, but it can be useful as a marker when debugging emitted code.
#define BKPT_insn       ((NIns)( COND_AL | (0x12<<20) | (0x7<<4) ))
#define BKPTi_insn(id)  ((NIns)(BKPT_insn | ((id << 4) & 0xfff00) | (id & 0xf)));

#define BKPT_nochk()    BKPTi_nochk(0)
#define BKPTi_nochk(id) do {                                \
        NanoAssert((id & 0xffff) == id);                    \
        *(--_nIns) = BKPTi_insn(id);                        \
        } while (0)

// STMFD SP!, {reg}
#define PUSHr(_r)  do {                                                 \
        underrunProtect(4);                                             \
        NanoAssert(IsGpReg(_r));                                        \
        *(--_nIns) = (NIns)( COND_AL | (0x92<<20) | (SP<<16) | rmask(_r) ); \
        asm_output("push %s",gpn(_r)); } while (0)

// STMFD SP!,{reglist}
#define PUSH_mask(_mask)  do {                                          \
        underrunProtect(4);                                             \
        NanoAssert(isU16(_mask));                                       \
        *(--_nIns) = (NIns)( COND_AL | (0x92<<20) | (SP<<16) | (_mask) ); \
        asm_output("push %x", (_mask));} while (0)

// LDMFD SP!,{reg}
#define POPr(_r) do {                                                   \
        underrunProtect(4);                                             \
        NanoAssert(IsGpReg(_r));                                        \
        *(--_nIns) = (NIns)( COND_AL | (0x8B<<20) | (SP<<16) | rmask(_r) ); \
        asm_output("pop %s",gpn(_r));} while (0)

// LDMFD SP!,{reglist}
#define POP_mask(_mask) do {                                            \
        underrunProtect(4);                                             \
        NanoAssert(isU16(_mask));                                       \
        *(--_nIns) = (NIns)( COND_AL | (0x8B<<20) | (SP<<16) | (_mask) ); \
        asm_output("pop %x", (_mask));} while (0)

// PC always points to current instruction + 8, so when calculating pc-relative
// offsets, use PC+8.
#define PC_OFFSET_FROM(target,frompc) ((intptr_t)(target) - ((intptr_t)(frompc) + 8))

#define B_cond(_c,_t)                           \
    B_cond_chk(_c,_t,1)

#define B_nochk(_t)                             \
    B_cond_chk(AL,_t,0)

#define B(t)    B_cond(AL,t)
#define BHI(t)  B_cond(HI,t)
#define BLS(t)  B_cond(LS,t)
#define BHS(t)  B_cond(HS,t)
#define BLO(t)  B_cond(LO,t)
#define BEQ(t)  B_cond(EQ,t)
#define BNE(t)  B_cond(NE,t)
#define BLT(t)  B_cond(LT,t)
#define BGE(t)  B_cond(GE,t)
#define BLE(t)  B_cond(LE,t)
#define BGT(t)  B_cond(GT,t)
#define BVS(t)  B_cond(VS,t)
#define BVC(t)  B_cond(VC,t)
#define BCC(t)  B_cond(CC,t)
#define BCS(t)  B_cond(CS,t)

#define JMP(t) B(t)
#define JMP_nochk(t) B_nochk(t)

// MOV(cond) _r, #1
// MOV(!cond) _r, #0
#define SET(_r,_cond) do {                                              \
    ConditionCode _opp = OppositeCond(_cond);                           \
    underrunProtect(8);                                                 \
    *(--_nIns) = (NIns)( ( _opp<<28) | (0x3A<<20) | ((_r)<<12) | (0) ); \
    *(--_nIns) = (NIns)( (_cond<<28) | (0x3A<<20) | ((_r)<<12) | (1) ); \
    asm_output("mov%s %s, #1", condNames[_cond], gpn(_r));              \
    asm_output("mov%s %s, #0", condNames[_opp], gpn(_r));               \
    } while (0)

#define SETEQ(r)    SET(r,EQ)
#define SETNE(r)    SET(r,NE)
#define SETLT(r)    SET(r,LT)
#define SETLE(r)    SET(r,LE)
#define SETGT(r)    SET(r,GT)
#define SETGE(r)    SET(r,GE)
#define SETLO(r)    SET(r,LO)
#define SETLS(r)    SET(r,LS)
#define SETHI(r)    SET(r,HI)
#define SETHS(r)    SET(r,HS)
#define SETVS(r)    SET(r,VS)
#define SETCS(r)    SET(r,CS)

// Load and sign extend a 16-bit value into a reg
#define MOVSX(_d,_off,_b) do {                                          \
        if ((_off)>=0) {                                                \
            if ((_off)<256) {                                           \
                underrunProtect(4);                                     \
                *(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_b)<<16) | ((_d)<<12) |  ((((_off)>>4)&0xF)<<8) | (0xF<<4) | ((_off)&0xF)  ); \
            } else if ((_off)<=510) {                                   \
                underrunProtect(8);                                     \
                int rem = (_off) - 255;                                 \
                NanoAssert(rem<256);                                    \
                *(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_d)<<16) | ((_d)<<12) |  ((((rem)>>4)&0xF)<<8) | (0xF<<4) | ((rem)&0xF)  ); \
                *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_b)<<16) | ((_d)<<12) | (0xFF) ); \
            } else {                                                    \
                underrunProtect(16);                                    \
                int rem = (_off) & 3;                                   \
                *(--_nIns) = (NIns)( COND_AL | (0x19<<20) | ((_b)<<16) | ((_d)<<12) | (0xF<<4) | (_d) ); \
                asm_output("ldrsh %s,[%s, #%d]",gpn(_d), gpn(_b), (_off)); \
                *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_d)<<16) | ((_d)<<12) | rem ); \
                *(--_nIns) = (NIns)( COND_AL | (0x1A<<20) | ((_d)<<12) | (2<<7)| (_d) ); \
                *(--_nIns) = (NIns)( COND_AL | (0x3B<<20) | ((_d)<<12) | (((_off)>>2)&0xFF) ); \
                asm_output("mov %s,%d",gpn(_d),(_off));                \
            }                                                           \
        } else {                                                        \
            if ((_off)>-256) {                                          \
                underrunProtect(4);                                     \
                *(--_nIns) = (NIns)( COND_AL | (0x15<<20) | ((_b)<<16) | ((_d)<<12) |  ((((-(_off))>>4)&0xF)<<8) | (0xF<<4) | ((-(_off))&0xF)  ); \
                asm_output("ldrsh %s,[%s, #%d]",gpn(_d), gpn(_b), (_off)); \
            } else if ((_off)>=-510){                                   \
                underrunProtect(8);                                     \
                int rem = -(_off) - 255;                                \
                NanoAssert(rem<256);                                    \
                *(--_nIns) = (NIns)( COND_AL | (0x15<<20) | ((_d)<<16) | ((_d)<<12) |  ((((rem)>>4)&0xF)<<8) | (0xF<<4) | ((rem)&0xF)  ); \
                *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_b)<<16) | ((_d)<<12) | (0xFF) ); \
            } else NanoAssert(0);                                        \
        }                                                               \
    } while(0)

/*
 * VFP
 */

#define FMDRR(_Dm,_Rd,_Rn) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsFpReg(_Dm) && IsGpReg(_Rd) && IsGpReg(_Rn));       \
        *(--_nIns) = (NIns)( COND_AL | (0xC4<<20) | ((_Rn)<<16) | ((_Rd)<<12) | (0xB1<<4) | (FpRegNum(_Dm)) ); \
        asm_output("fmdrr %s,%s,%s", gpn(_Dm), gpn(_Rd), gpn(_Rn));    \
    } while (0)

#define FMRRD(_Rd,_Rn,_Dm) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsGpReg(_Rd) && IsGpReg(_Rn) && IsFpReg(_Dm));       \
        *(--_nIns) = (NIns)( COND_AL | (0xC5<<20) | ((_Rn)<<16) | ((_Rd)<<12) | (0xB1<<4) | (FpRegNum(_Dm)) ); \
        asm_output("fmrrd %s,%s,%s", gpn(_Rd), gpn(_Rn), gpn(_Dm));    \
    } while (0)

#define FMRDH(_Rd,_Dn) do {                                             \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsGpReg(_Rd) && IsFpReg(_Dn));                       \
        *(--_nIns) = (NIns)( COND_AL | (0xE3<<20) | (FpRegNum(_Dn)<<16) | ((_Rd)<<12) | (0xB<<8) | (1<<4) ); \
        asm_output("fmrdh %s,%s", gpn(_Rd), gpn(_Dn));                  \
    } while (0)

#define FMRDL(_Rd,_Dn) do {                                             \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsGpReg(_Rd) && IsFpReg(_Dn));                       \
        *(--_nIns) = (NIns)( COND_AL | (0xE1<<20) | (FpRegNum(_Dn)<<16) | ((_Rd)<<12) | (0xB<<8) | (1<<4) ); \
        asm_output("fmrdh %s,%s", gpn(_Rd), gpn(_Dn));                  \
    } while (0)

#define FSTD_allowD7(_Dd,_Rn,_offs,_allowD7) do {                       \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                            \
        NanoAssert(((_offs)%4) == 0);                                   \
        NanoAssert(isU8((_offs)/4) || isU8(-(_offs)/4));                \
        NanoAssert(IsFpReg(_Dd) && IsGpReg(_Rn));                       \
        int negflag = 1<<23;                                            \
        intptr_t offs = (_offs);                                        \
        if (_offs < 0) {                                                \
            negflag = 0<<23;                                            \
            offs = -(offs);                                             \
        }                                                               \
        *(--_nIns) = (NIns)( COND_AL | (0xD0<<20) | ((_Rn)<<16) | (FpRegNum(_Dd)<<12) | (0xB<<8) | negflag | ((offs>>2)&0xff) ); \
        asm_output("fstd %s,%s(%d)", gpn(_Dd), gpn(_Rn), _offs);    \
    } while (0)

#define FSTD(_Dd,_Rn,_offs) \
        FSTD_allowD7(_Dd,_Rn,_offs,0)

#define FLDD_chk(_Dd,_Rn,_offs,_chk) do {                               \
        if(_chk) underrunProtect(4);                                    \
        NanoAssert(ARM_VFP);                                            \
        NanoAssert(((_offs)%4) == 0);                                   \
        NanoAssert(isU8((_offs)/4) || isU8(-(_offs)/4));                \
        NanoAssert(IsFpReg(_Dd) && !IsFpReg(_Rn));                      \
        int negflag = 1<<23;                                            \
        intptr_t offs = (_offs);                                        \
        if (_offs < 0) {                                                \
            negflag = 0<<23;                                            \
            offs = -(offs);                                             \
        }                                                               \
        *(--_nIns) = (NIns)( COND_AL | (0xD1<<20) | ((_Rn)<<16) | (FpRegNum(_Dd)<<12) | (0xB<<8) | negflag | ((offs>>2)&0xff) ); \
        asm_output("fldd %s,%s(%d)", gpn(_Dd), gpn(_Rn), _offs);       \
    } while (0)
#define FLDD(_Dd,_Rn,_offs) FLDD_chk(_Dd,_Rn,_offs,1)

#define FUITOD(_Dd,_Sm) do {                                            \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsFpReg(_Dd) && ((_Sm) == S0));                     \
        *(--_nIns) = (NIns)( COND_AL | (0xEB8<<16) | (FpRegNum(_Dd)<<12) | (0x2D<<6) | (0<<5) | (0x0) ); \
        asm_output("fuitod %s,%s", gpn(_Dd), gpn(_Sm));                \
    } while (0)

#define FNEGD(_Dd,_Dm) do {                                             \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dm));                       \
        *(--_nIns) = (NIns)( COND_AL | (0xEB1<<16) | (FpRegNum(_Dd)<<12) | (0xB4<<4) | (FpRegNum(_Dm)) ); \
        asm_output("fnegd %s,%s", gpn(_Dd), gpn(_Dm));                 \
    } while (0)

#define FADDD(_Dd,_Dn,_Dm) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dn) && IsFpReg(_Dm));       \
        *(--_nIns) = (NIns)( COND_AL | (0xE3<<20) | (FpRegNum(_Dn)<<16) | (FpRegNum(_Dd)<<12) | (0xB0<<4) | (FpRegNum(_Dm)) ); \
        asm_output("faddd %s,%s,%s", gpn(_Dd), gpn(_Dn), gpn(_Dm));    \
    } while (0)

#define FSUBD(_Dd,_Dn,_Dm) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dn) && IsFpReg(_Dm));       \
        *(--_nIns) = (NIns)( COND_AL | (0xE3<<20) | (FpRegNum(_Dn)<<16) | (FpRegNum(_Dd)<<12) | (0xB4<<4) | (FpRegNum(_Dm)) ); \
        asm_output("fsubd %s,%s,%s", gpn(_Dd), gpn(_Dn), gpn(_Dm));    \
    } while (0)

#define FMULD(_Dd,_Dn,_Dm) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dn) && IsFpReg(_Dm));       \
        *(--_nIns) = (NIns)( COND_AL | (0xE2<<20) | (FpRegNum(_Dn)<<16) | (FpRegNum(_Dd)<<12) | (0xB0<<4) | (FpRegNum(_Dm)) ); \
        asm_output("fmuld %s,%s,%s", gpn(_Dd), gpn(_Dn), gpn(_Dm));    \
    } while (0)

#define FDIVD(_Dd,_Dn,_Dm) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dn) && IsFpReg(_Dm));       \
        *(--_nIns) = (NIns)( COND_AL | (0xE8<<20) | (FpRegNum(_Dn)<<16) | (FpRegNum(_Dd)<<12) | (0xB0<<4) | (FpRegNum(_Dm)) ); \
        asm_output("fdivd %s,%s,%s", gpn(_Dd), gpn(_Dn), gpn(_Dm));    \
    } while (0)

#define FMSTAT() do {                               \
        underrunProtect(4);                         \
        NanoAssert(ARM_VFP);                \
        *(--_nIns) = (NIns)( COND_AL | 0x0EF1FA10); \
        asm_output("fmstat");                       \
    } while (0)

#define FCMPD(_Dd,_Dm,_E) do {                                          \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dm));                       \
        NanoAssert(((_E)==0) || ((_E)==1));                             \
        *(--_nIns) = (NIns)( COND_AL | (0xEB4<<16) | (FpRegNum(_Dd)<<12) | (0xB<<8) | ((_E)<<7) | (0x4<<4) | (FpRegNum(_Dm)) ); \
        asm_output("fcmp%sd %s,%s", (((_E)==1)?"e":""), gpn(_Dd), gpn(_Dm)); \
    } while (0)

#define FCPYD_cond(_cond,_Dd,_Dm) do {                                      \
        underrunProtect(4);                                                 \
        NanoAssert(ARM_VFP);                                                \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dm));                           \
        NanoAssert(IsCond(_cond));                                          \
        *(--_nIns) = (NIns)( ((_cond)<<28) | (0xEB0<<16) | (FpRegNum(_Dd)<<12) | (0xB4<<4) | (FpRegNum(_Dm)) ); \
        asm_output("fcpyd%s %s,%s", condNames[_cond], gpn(_Dd), gpn(_Dm));  \
    } while (0)
#define FCPYD(_Dd,_Dm)      FCPYD_cond(AL,_Dd,_Dm)

#define FMRS(_Rd,_Sn) do {                                              \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(((_Sn) == S0) && IsGpReg(_Rd));                     \
        *(--_nIns) = (NIns)( COND_AL | (0xE1<<20) | (0x0<<16) | ((_Rd)<<12) | (0xA<<8) | (0<<7) | (0x1<<4) ); \
        asm_output("fmrs %s,%s", gpn(_Rd), gpn(_Sn));                  \
    } while (0)

/*
 * The following instructions can only be used with S0 as the
 * single-precision register; that limitation can be removed if
 * needed, but we'd have to teach NJ about all the single precision
 * regs, and their encoding is strange (top 4 bits usually in a block,
 * low bit elsewhere).
 */

#define FSITOD(_Dd,_Sm) do {                                            \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(IsFpReg(_Dd) && ((_Sm) == S0));                     \
        *(--_nIns) = (NIns)( COND_AL | (0xEB8<<16) | (FpRegNum(_Dd)<<12) | (0x2F<<6) | (0<<5) | (0x0) ); \
        asm_output("fsitod %s,%s", gpn(_Dd), gpn(_Sm));                \
    } while (0)

#define FMSR(_Sn,_Rd) do {                                              \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(((_Sn) == S0) && IsGpReg(_Rd));                     \
        *(--_nIns) = (NIns)( COND_AL | (0xE0<<20) | (0x0<<16) | ((_Rd)<<12) | (0xA<<8) | (0<<7) | (0x1<<4) ); \
        asm_output("fmsr %s,%s", gpn(_Sn), gpn(_Rd));                  \
    } while (0)

#define FMRS(_Rd,_Sn) do {                                              \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(((_Sn) == S0) && IsGpReg(_Rd));                     \
        *(--_nIns) = (NIns)( COND_AL | (0xE1<<20) | (0x0<<16) | ((_Rd)<<12) | (0xA<<8) | (0<<7) | (0x1<<4) ); \
        asm_output("fmrs %s,%s", gpn(_Rd), gpn(_Sn));                  \
    } while (0)

#define FMSR(_Sn,_Rd) do {                                              \
        underrunProtect(4);                                             \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(((_Sn) == S0) && IsGpReg(_Rd));                     \
        *(--_nIns) = (NIns)( COND_AL | (0xE0<<20) | (0x0<<16) | ((_Rd)<<12) | (0xA<<8) | (0<<7) | (0x1<<4) ); \
        asm_output("fmsr %s,%s", gpn(_Sn), gpn(_Rd));                  \
    } while (0)

#define FCVTSD(_Sd,_Dm) do {                        \
        underrunProtect(4);                         \
        NanoAssert(ARM_VFP);                \
        NanoAssert(((_Sd) == S0) && IsFpReg(_Dm)); \
        *(--_nIns) = (NIns)( COND_AL | (0xEB7<<16) | (0x0<<12) | (0xBC<<4) | (FpRegNum(_Dm)) ); \
        asm_output("[0x%08x] fcvtsd s0,%s", *_nIns, gpn(_Dm));                          \
    } while (0)

#define FCVTDS(_Dd,_Sm) do {                                    \
        underrunProtect(4);                                     \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(((_Sm) == S0) && IsFpReg(_Dd));             \
        *(--_nIns) = (NIns)( COND_AL | (0xEB7<<16) | (FpRegNum(_Dd)<<12) | (0xAC<<4) | (0x0) ); \
        asm_output("fcvtds %s,s0", gpn(_Dd));                  \
    } while(0)

#define FLDS(_Sd,_Rn,_offs) do {                                \
        underrunProtect(4);                                     \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(((_Sd) == S0) && !IsFpReg(_Rn));            \
        NanoAssert(((_offs)%4) == 0);                           \
        NanoAssert((isU8((_offs)/4)) || isU8(-(_offs)/4));      \
        int addflag = 1<<23;                                    \
        intptr_t offs = (_offs);                                \
        if (offs < 0) {                                         \
            addflag = 0;                                        \
            offs = -offs;                                       \
        }                                                       \
        *(--_nIns) = (NIns)( COND_AL | (0xD1<<20) | ((_Rn)<<16) | (0x0<<12) | (0xA << 8) | addflag | ((offs>>2)&0xff) ); \
        asm_output("flds s0, [%s, #%d]", gpn(_Rn), (_offs));   \
    } while (0)

#define FSTS(_Sd,_Rn,_offs) do {                                \
        underrunProtect(4);                                     \
        NanoAssert(ARM_VFP);                                    \
        NanoAssert(((_Sd) == S0) && !IsFpReg(_Rn));            \
        NanoAssert(((_offs)%4) == 0);                           \
        NanoAssert((isU8((_offs)/4)) || isU8(-(_offs)/4));      \
        int addflag = 1<<23;                                    \
        intptr_t offs = (_offs);                                \
        if (offs < 0) {                                         \
            addflag = 0;                                        \
            offs = -offs;                                       \
        }                                                       \
        *(--_nIns) = (NIns)( COND_AL | (0xD0<<20) | ((_Rn)<<16) | (0x0<<12) | (0xA << 8) | addflag | ((offs>>2)&0xff) ); \
        asm_output("fsts s0, [%s, #%d]", gpn(_Rn), (_offs));   \
    } while (0)

#define FTOSID(_Sd,_Dm) do {                                   \
        underrunProtect(4);                                    \
        NanoAssert(ARM_VFP);                           \
        NanoAssert(((_Sd) == S0) && IsFpReg(_Dm));            \
        *(--_nIns) = (NIns)( COND_AL | (0xEBD<<16) | (0x0<<12) | (0xB4<<4) | FpRegNum(_Dm) ); \
        asm_output("ftosid s0, %s", gpn(_Dm));                \
    } while (0)

} // namespace nanojit
#endif // __nanojit_NativeARM__
