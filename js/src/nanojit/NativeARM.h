/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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


#ifndef __nanojit_NativeArm__
#define __nanojit_NativeArm__


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

namespace nanojit
{

const int NJ_LOG2_PAGE_SIZE = 12;       // 4K

// If NJ_ARM_VFP is defined, then VFP is assumed to
// be present.  If it's not defined, then softfloat
// is used, and NJ_SOFTFLOAT is defined.
#define NJ_ARM_VFP

#ifdef NJ_ARM_VFP

// only d0-d7; we'll use d7 as s14-s15 for i2f/u2f/etc.
#define NJ_VFP_MAX_REGISTERS            8

#else

#define NJ_VFP_MAX_REGISTERS            0
#define NJ_SOFTFLOAT

#endif

#define NJ_MAX_REGISTERS                (11 + NJ_VFP_MAX_REGISTERS)
#define NJ_MAX_STACK_ENTRY              256
#define NJ_MAX_PARAMETERS               16
#define NJ_ALIGN_STACK                  8
#define NJ_STACK_OFFSET                 0

#define NJ_CONSTANT_POOLS
const int NJ_MAX_CPOOL_OFFSET = 4096;
const int NJ_CPOOL_SIZE = 16;

typedef int NIns;

/* ARM registers */
typedef enum {
    R0  = 0,
    R1  = 1,
    R2  = 2,
    R3  = 3,
    R4  = 4,
    R5  = 5,
    R6  = 6,
    R7  = 7,
    R8  = 8,
    R9  = 9,
    R10 = 10,
    FP  = 11,
    IP  = 12,
    SP  = 13,
    LR  = 14,
    PC  = 15,

    // FP regs
    D0 = 16,
    D1 = 17,
    D2 = 18,
    D3 = 19,
    D4 = 20,
    D5 = 21,
    D6 = 22,
    D7 = 23,

    FirstFloatReg = 16,
    LastFloatReg = 22,
        
    FirstReg = 0,
#ifdef NJ_ARM_VFP
    LastReg = 23,
#else
    LastReg = 10,
#endif
    Scratch = IP,
    UnknownReg = 31,

    // special value referring to S14
    FpSingleScratch = 24
} Register;

/* ARM condition codes */
typedef enum {
    EQ = 0x0, // Equal
    NE = 0x1, // Not Equal
    CS = 0x2, // Carry Set (or HS)
    CC = 0x3, // Carry Clear (or LO)
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
    NV = 0xF  // NeVer
} ConditionCode;


typedef int RegisterMask;
typedef struct _FragInfo {
    RegisterMask    needRestoring;
    NIns*           epilogue;
} FragInfo;

// D0-D6 are not saved; D7-D15 are, but we don't use those,
// so we don't have to worry about saving/restoring them
static const RegisterMask SavedFpRegs = 0;
static const RegisterMask SavedRegs = 1<<R4 | 1<<R5 | 1<<R6 | 1<<R7 | 1<<R8 | 1<<R9 | 1<<R10;
static const int NumSavedRegs = 7;

static const RegisterMask FpRegs = 1<<D0 | 1<<D1 | 1<<D2 | 1<<D3 | 1<<D4 | 1<<D5 | 1<<D6; // no D7; S14-S15 are used for i2f/u2f.
static const RegisterMask GpRegs = 0x07FF;
static const RegisterMask AllowableFlagRegs = 1<<R0 | 1<<R1 | 1<<R2 | 1<<R3 | 1<<R4 | 1<<R5 | 1<<R6 | 1<<R7 | 1<<R8 | 1<<R9 | 1<<R10;

#define IsFpReg(_r)     ((rmask(_r) & (FpRegs | (1<<D7))) != 0)
#define IsGpReg(_r)     ((rmask(_r) & (GpRegs | (1<<Scratch))) != 0)
#define FpRegNum(_fpr)  ((_fpr) - FirstFloatReg)

#define firstreg()      R0
#define nextreg(r)      ((Register)((int)(r)+1))
#if 0
static Register nextreg(Register r) {
    if (r == R10)
        return D0;
    return (Register)(r+1);
}
#endif
// only good for normal regs
#define imm2register(c) (Register)(c-1)

verbose_only( extern const char* regNames[]; )

// abstract to platform specific calls
#define nExtractPlatformFlags(x)    0

#define DECLARE_PLATFORM_STATS()

#define DECLARE_PLATFORM_REGALLOC()

#define DECLARE_PLATFORM_ASSEMBLER()                                    \
    const static Register argRegs[4], retRegs[2];                       \
    void LD32_nochk(Register r, int32_t imm);                           \
    void BL(NIns*);                                                     \
    void JMP_far(NIns*);                                                \
    void B_cond_chk(ConditionCode, NIns*, bool);                        \
    void underrunProtect(int bytes);                                    \
    void nativePageReset();                                             \
    void nativePageSetup();                                             \
    void asm_quad_nochk(Register, const int32_t*);                      \
    void asm_add_imm(Register, Register, int32_t);                      \
    int* _nSlot;                                                        \
    int* _nExitSlot;


#define asm_farg(i) NanoAssert(false)

//printf("jmp_l_n count=%d, nins=%X, %X = %X\n", (_c), nins, _nIns, ((intptr_t)(nins+(_c))-(intptr_t)_nIns - 4) );

#define swapptrs()  {                                                   \
        NIns* _tins = _nIns; _nIns=_nExitIns; _nExitIns=_tins;          \
        int* _nslot = _nSlot;                                           \
        _nSlot = _nExitSlot;                                            \
        _nExitSlot = _nslot;                                            \
    }


#define IMM32(imm)  *(--_nIns) = (NIns)((imm));

#define OP_IMM  (1<<25)
#define OP_STAT (1<<20)

#define COND_AL (0xE<<28)

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

#define LD32_size 8

#define BEGIN_NATIVE_CODE(x)                    \
    { DWORD* _nIns = (uint8_t*)x

#define END_NATIVE_CODE(x)                      \
    (x) = (dictwordp*)_nIns; }

// BX 
#define BX(_r)  do {                                                    \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x12<<20) | (0xFFF<<8) | (1<<4) | (_r)); \
        asm_output("bx LR"); } while(0)

// _l = _r OR _l
#define OR(_l,_r)       do {                                            \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0xC<<21) | (_r<<16) | (_l<<12) | (_l) ); \
        asm_output2("or %s,%s",gpn(_l),gpn(_r)); } while(0)

// _r = _r OR _imm
#define ORi(_r,_imm)    do {                                            \
        NanoAssert(isU8((_imm)));                                       \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | OP_IMM | (0xC<<21) | (_r<<16) | (_r<<12) | ((_imm)&0xFF) ); \
        asm_output2("or %s,%d",gpn(_r), (_imm)); } while(0)

// _l = _r AND _l
#define AND(_l,_r) do {                                                 \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | ((_r)<<16) | ((_l)<<12) | (_l)); \
        asm_output2("and %s,%s",gpn(_l),gpn(_r)); } while(0)

// _r = _r AND _imm
#define ANDi(_r,_imm) do {                                              \
        if (isU8((_imm))) {                                             \
            underrunProtect(4);                                         \
            *(--_nIns) = (NIns)( COND_AL | OP_IMM | ((_r)<<16) | ((_r)<<12) | ((_imm)&0xFF) ); \
            asm_output2("and %s,%d",gpn(_r),(_imm));}                   \
        else if ((_imm)<0 && (_imm)>-256) {                             \
            underrunProtect(8);                                         \
            *(--_nIns) = (NIns)( COND_AL | ((_r)<<16) | ((_r)<<12) | (Scratch) ); \
            asm_output2("and %s,%s",gpn(_r),gpn(Scratch));              \
            *(--_nIns) = (NIns)( COND_AL | (0x3E<<20) | ((Scratch)<<12) | (((_imm)^0xFFFFFFFF)&0xFF) ); \
            asm_output2("mvn %s,%d",gpn(Scratch),(_imm));}              \
        else NanoAssert(0);                                             \
    } while (0)


// _l = _l XOR _r
#define XOR(_l,_r)  do {                                                \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (1<<21) | ((_r)<<16) | ((_l)<<12) | (_l)); \
        asm_output2("eor %s,%s",gpn(_l),gpn(_r)); } while(0)

// _r = _r XOR _imm
#define XORi(_r,_imm)   do {                                            \
        NanoAssert(isU8((_imm)));                                       \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<21) | ((_r)<<16) | ((_r)<<12) | ((_imm)&0xFF) ); \
        asm_output2("eor %s,%d",gpn(_r),(_imm)); } while(0)

// _d = _n + _m
#define arm_ADD(_d,_n,_m) do {                                          \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | OP_STAT | (1<<23) | ((_n)<<16) | ((_d)<<12) | (_m)); \
        asm_output3("add %s,%s+%s",gpn(_d),gpn(_n),gpn(_m)); } while(0)

// _l = _l + _r
#define ADD(_l,_r)   arm_ADD(_l,_l,_r)

// Note that this sometimes converts negative immediate values to a to a sub.
// _d = _r + _imm
#define arm_ADDi(_d,_n,_imm)   asm_add_imm(_d,_n,_imm)
#define ADDi(_r,_imm)  arm_ADDi(_r,_r,_imm)

// _l = _l - _r
#define SUB(_l,_r)  do {                                                \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (1<<22) | ((_l)<<16) | ((_l)<<12) | (_r)); \
        asm_output2("sub %s,%s",gpn(_l),gpn(_r)); } while(0)

// _r = _r - _imm
#define SUBi(_r,_imm)  do {                                             \
        if ((_imm)>-256 && (_imm)<256) {                                \
            underrunProtect(4);                                         \
            if ((_imm)>=0)  *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_r)<<16) | ((_r)<<12) | ((_imm)&0xFF) ); \
            else            *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_r)<<16) | ((_r)<<12) | ((-(_imm))&0xFF) ); \
        } else {                                                        \
            if ((_imm)>=0) {                                            \
                if ((_imm)<=510) {                                      \
                    underrunProtect(8);                                 \
                    int rem = (_imm) - 255;                             \
                    NanoAssert(rem<256);                                \
                    *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_r)<<16) | ((_r)<<12) | (rem&0xFF) ); \
                    *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_r)<<16) | ((_r)<<12) | (0xFF) ); \
                } else {                                                \
                    underrunProtect(4+LD32_size);                       \
                    *(--_nIns) = (NIns)( COND_AL | (1<<22) | ((_r)<<16) | ((_r)<<12) | (Scratch)); \
                    LD32_nochk(Scratch, _imm);                          \
                }                                                       \
            } else {                                                    \
                if ((_imm)>=-510) {                                     \
                    underrunProtect(8);                                 \
                    int rem = -(_imm) - 255;                            \
                    *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_r)<<16) | ((_r)<<12) | ((rem)&0xFF) ); \
                    *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_r)<<16) | ((_r)<<12) | (0xFF) ); \
                } else {                                                \
                    underrunProtect(4+LD32_size);                       \
                    *(--_nIns) = (NIns)( COND_AL | (1<<23) | ((_r)<<16) | ((_r)<<12) | (Scratch)); \
                    LD32_nochk(Scratch, -(_imm)); \
                }                                                       \
            }                                                           \
        }                                                               \
        asm_output2("sub %s,%d",gpn(_r),(_imm));                        \
    } while (0)

// _l = _l * _r
#define MUL(_l,_r)  do {                                                \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (_l)<<16 | (_l)<<8 | 0x90 | (_r) ); \
        asm_output2("mul %s,%s",gpn(_l),gpn(_r)); } while(0)


// RSBS
// _r = -_r
#define NEG(_r) do {                                                    \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL |  (0x27<<20) | ((_r)<<16) | ((_r)<<12) ); \
        asm_output1("neg %s",gpn(_r)); } while(0)

// MVNS
// _r = !_r
#define NOT(_r) do {                                                    \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL |  (0x1F<<20) | ((_r)<<12) |  (_r) ); \
        asm_output1("mvn %s",gpn(_r)); } while(0)

// MOVS _r, _r, LSR <_s>
// _r = _r >> _s
#define SHR(_r,_s) do {                                                 \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_s)<<8) | (LSR_reg<<4) | (_r) ); \
        asm_output2("shr %s,%s",gpn(_r),gpn(_s)); } while(0)

// MOVS _r, _r, LSR #_imm
// _r = _r >> _imm
#define SHRi(_r,_imm) do {                                              \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_imm)<<7) | (LSR_imm<<4) | (_r) ); \
        asm_output2("shr %s,%d",gpn(_r),_imm); } while(0)

// MOVS _r, _r, ASR <_s>
// _r = _r >> _s
#define SAR(_r,_s) do {                                                 \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_s)<<8) | (ASR_reg<<4) | (_r) ); \
        asm_output2("asr %s,%s",gpn(_r),gpn(_s)); } while(0)


// MOVS _r, _r, ASR #_imm
// _r = _r >> _imm
#define SARi(_r,_imm) do {                                              \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_imm)<<7) | (ASR_imm<<4) | (_r) ); \
        asm_output2("asr %s,%d",gpn(_r),_imm); } while(0)

// MOVS _r, _r, LSL <_s>
// _r = _r << _s
#define SHL(_r,_s) do {                                                 \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_s)<<8) | (LSL_reg<<4) | (_r) ); \
        asm_output2("lsl %s,%s",gpn(_r),gpn(_s)); } while(0)

// MOVS _r, _r, LSL #_imm
// _r = _r << _imm
#define SHLi(_r,_imm) do {                                              \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x1B<<20) | ((_r)<<12) | ((_imm)<<7) | (LSL_imm<<4) | (_r) ); \
        asm_output2("lsl %s,%d",gpn(_r),(_imm)); } while(0)
                    
// TST
#define TEST(_d,_s) do {                                                \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x11<<20) | ((_d)<<16) | (_s) ); \
        asm_output2("test %s,%s",gpn(_d),gpn(_s)); } while(0)

#define TSTi(_d,_imm) do {                                              \
        underrunProtect(4);                                             \
        NanoAssert(((_imm) & 0xff) == (_imm));                          \
        *(--_nIns) = (NIns)( COND_AL | OP_IMM | (0x11<<20) | ((_d) << 16) | (0xF<<12) | ((_imm) & 0xff) ); \
        asm_output2("tst %s,#0x%x", gpn(_d), _imm);                     \
    } while (0);

// CMP
#define CMP(_l,_r)  do {                                                \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x015<<20) | ((_l)<<16) | (_r) ); \
        asm_output2("cmp %s,%s",gpn(_l),gpn(_r)); } while(0)

// CMP (or CMN)
#define CMPi(_r,_imm)  do {                                             \
        if (_imm<0) {                                                   \
            if ((_imm)>-256) {                                          \
                underrunProtect(4);                                     \
                *(--_nIns) = (NIns)( COND_AL | (0x37<<20) | ((_r)<<16) | (-(_imm)) ); \
            } else {                                                      \
                underrunProtect(4+LD32_size);                           \
                *(--_nIns) = (NIns)( COND_AL | (0x17<<20) | ((_r)<<16) | (Scratch) ); \
                LD32_nochk(Scratch, (_imm));                            \
            }                                                           \
        } else {                                                        \
            if ((_imm)<256) {                                           \
                underrunProtect(4);                                     \
                *(--_nIns) = (NIns)( COND_AL | (0x035<<20) | ((_r)<<16) | ((_imm)&0xFF) ); \
            } else {                                                    \
                underrunProtect(4+LD32_size);                           \
                *(--_nIns) = (NIns)( COND_AL | (0x015<<20) | ((_r)<<16) | (Scratch) ); \
                LD32_nochk(Scratch, (_imm));                            \
            }                                                           \
        }                                                               \
        asm_output2("cmp %s,0x%x",gpn(_r),(_imm));                      \
    } while(0)

// MOV
#define MR(_d,_s)  do {                                                 \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0xD<<21) | ((_d)<<12) | (_s) ); \
        asm_output2("mov %s,%s",gpn(_d),gpn(_s)); } while (0)


#define MR_cond(_d,_s,_cond,_nm)  do {                                  \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( ((_cond)<<28) | (0xD<<21) | ((_d)<<12) | (_s) ); \
        asm_output2(_nm " %s,%s",gpn(_d),gpn(_s)); } while (0)

#define MREQ(dr,sr) MR_cond(dr, sr, EQ, "moveq")
#define MRNE(dr,sr) MR_cond(dr, sr, NE, "movne")
#define MRL(dr,sr)  MR_cond(dr, sr, LT, "movlt")
#define MRLE(dr,sr) MR_cond(dr, sr, LE, "movle")
#define MRG(dr,sr)  MR_cond(dr, sr, GT, "movgt")
#define MRGE(dr,sr) MR_cond(dr, sr, GE, "movge")
#define MRB(dr,sr)  MR_cond(dr, sr, CC, "movcc")
#define MRBE(dr,sr) MR_cond(dr, sr, LS, "movls")
#define MRA(dr,sr)  MR_cond(dr, sr, HI, "movcs")
#define MRAE(dr,sr) MR_cond(dr, sr, CS, "movhi")
#define MRNO(dr,sr) MR_cond(dr, sr, VC, "movvc") // overflow clear
#define MRNC(dr,sr) MR_cond(dr, sr, CC, "movcc") // carry clear

#define LDR_chk(_d,_b,_off,_chk) do {                                   \
        if (IsFpReg(_d)) {                                              \
            FLDD_chk(_d,_b,_off,_chk);                                  \
        } else if ((_off)<0) {                                          \
            if (_chk) underrunProtect(4);                               \
            NanoAssert((_off)>-4096);                                   \
            *(--_nIns) = (NIns)( COND_AL | (0x51<<20) | ((_b)<<16) | ((_d)<<12) | ((-(_off))&0xFFF) ); \
        } else {                                                        \
            if (isS16(_off) || isU16(_off)) {                           \
                if (_chk) underrunProtect(4);                           \
                NanoAssert((_off)<4096);                                \
                *(--_nIns) = (NIns)( COND_AL | (0x59<<20) | ((_b)<<16) | ((_d)<<12) | ((_off)&0xFFF) ); \
            } else {                                                    \
                if (_chk) underrunProtect(4+LD32_size);                 \
                *(--_nIns) = (NIns)( COND_AL | (0x79<<20) | ((_b)<<16) | ((_d)<<12) | Scratch ); \
                LD32_nochk(Scratch, _off);                              \
            }                                                           \
        }                                                               \
        asm_output3("ldr %s, [%s, #%d]",gpn(_d),gpn(_b),(_off));        \
    } while(0)

#define LDR(_d,_b,_off)        LDR_chk(_d,_b,_off,1)
#define LDR_nochk(_d,_b,_off)  LDR_chk(_d,_b,_off,0)

// i386 compat, for Assembler.cpp
#define LD(reg,offset,base)    LDR_chk(reg,base,offset,1)
#define ST(base,offset,reg)    STR(reg,base,offset)

#define LDi(_d,_imm) do {                                               \
        if ((_imm) == 0) {                                              \
            XOR(_d,_d);                                                 \
        } else if (isS8((_imm)) || isU8((_imm))) {                      \
            underrunProtect(4);                                         \
            if ((_imm)<0)   *(--_nIns) = (NIns)( COND_AL | (0x3E<<20) | ((_d)<<12) | (((_imm)^0xFFFFFFFF)&0xFF) ); \
            else            *(--_nIns) = (NIns)( COND_AL | (0x3B<<20) | ((_d)<<12) | ((_imm)&0xFF) ); \
            asm_output2("ld  %s,0x%x",gpn((_d)),(_imm));                \
        } else {                                                        \
            underrunProtect(LD32_size);                                 \
            LD32_nochk(_d, (_imm));                                     \
            asm_output2("ld  %s,0x%x",gpn((_d)),(_imm));                \
        }                                                               \
    } while(0)


// load 8-bit, zero extend (aka LDRB) note, only 5-bit offsets (!) are
// supported for this, but that's all we need at the moment.
// (LDRB/LDRH actually allow a 12-bit offset in ARM mode but
// constraining to 5-bit gives us advantage for Thumb)
#define LDRB(_d,_off,_b) do {                                           \
        NanoAssert((_off)>=0&&(_off)<=31);                              \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x5D<<20) | ((_b)<<16) | ((_d)<<12) | ((_off)&0xfff)  ); \
        asm_output3("ldrb %s,%d(%s)", gpn(_d),(_off),gpn(_b));          \
    } while(0)

// P and U
#define LDRH(_d,_off,_b) do {                  \
        NanoAssert((_off)>=0&&(_off)<=31);      \
        underrunProtect(4);                     \
        *(--_nIns) = (NIns)( COND_AL | (0x1D<<20) | ((_b)<<16) | ((_d)<<12) | ((0xB)<<4) | (((_off)&0xf0)<<4) | ((_off)&0xf) ); \
        asm_output3("ldrsh %s,%d(%s)", gpn(_d),(_off),gpn(_b));         \
    } while(0)

#define STR(_d,_n,_off) do {                                            \
        NanoAssert(!IsFpReg(_d) && isS12(_off));                        \
        underrunProtect(4);                                             \
        if ((_off)<0)   *(--_nIns) = (NIns)( COND_AL | (0x50<<20) | ((_n)<<16) | ((_d)<<12) | ((-(_off))&0xFFF) ); \
        else            *(--_nIns) = (NIns)( COND_AL | (0x58<<20) | ((_n)<<16) | ((_d)<<12) | ((_off)&0xFFF) ); \
        asm_output3("str %s, [%s, #%d]", gpn(_d), gpn(_n), (_off)); \
    } while(0)

// Rd += _off; [Rd] = Rn
#define STR_preindex(_d,_n,_off) do {                                   \
        NanoAssert(!IsFpReg(_d) && isS12(_off));                        \
        underrunProtect(4);                                             \
        if ((_off)<0)   *(--_nIns) = (NIns)( COND_AL | (0x52<<20) | ((_n)<<16) | ((_d)<<12) | ((-(_off))&0xFFF) ); \
        else            *(--_nIns) = (NIns)( COND_AL | (0x5A<<20) | ((_n)<<16) | ((_d)<<12) | ((_off)&0xFFF) ); \
        asm_output3("str %s, [%s, #%d]", gpn(_d), gpn(_n), (_off));      \
    } while(0)

// [Rd] = Rn ; Rd += _off
#define STR_postindex(_d,_n,_off) do {                                  \
        NanoAssert(!IsFpReg(_d) && isS12(_off));                        \
        underrunProtect(4);                                             \
        if ((_off)<0)   *(--_nIns) = (NIns)( COND_AL | (0x40<<20) | ((_n)<<16) | ((_d)<<12) | ((-(_off))&0xFFF) ); \
        else            *(--_nIns) = (NIns)( COND_AL | (0x48<<20) | ((_n)<<16) | ((_d)<<12) | ((_off)&0xFFF) ); \
        asm_output3("str %s, [%s], %d", gpn(_d), gpn(_n), (_off));      \
    } while(0)


#define LEA(_r,_d,_b) do {                                              \
        NanoAssert((_d)<=1020);                                         \
        NanoAssert(((_d)&3)==0);                                        \
        if (_b!=SP) NanoAssert(0);                                      \
        if ((_d)<256) {                                                 \
            underrunProtect(4);                                         \
            *(--_nIns) = (NIns)( COND_AL | (0x28<<20) | ((_b)<<16) | ((_r)<<12) | ((_d)&0xFF) ); \
        } else {                                                        \
            underrunProtect(8);                                         \
            *(--_nIns) = (NIns)( COND_AL | (0x4<<21) | ((_b)<<16) | ((_r)<<12) | (2<<7)| (_r) ); \
            *(--_nIns) = (NIns)( COND_AL | (0x3B<<20) | ((_r)<<12) | (((_d)>>2)&0xFF) ); \
        }                                                               \
        asm_output2("lea %s, %d(SP)", gpn(_r), _d);                     \
    } while(0)


//#define RET()   underrunProtect(1); *(--_nIns) = 0xc3;    asm_output("ret")
//#define NOP()     underrunProtect(1); *(--_nIns) = 0x90;  asm_output("nop")
//#define INT3()  underrunProtect(1); *(--_nIns) = 0xcc;  asm_output("int3")
//#define RET() INT3()

#define BKPT_insn ((NIns)( (0xE<<24) | (0x12<<20) | (0x7<<4) ))
#define BKPT_nochk() do { \
        *(--_nIns) = BKPT_insn; } while (0)

// this isn't a armv6t2 NOP -- it's a mov r0,r0
#define NOP_nochk() do { \
        *(--_nIns) = (NIns)( COND_AL | (0xD<<21) | ((R0)<<12) | (R0) ); \
        asm_output("nop"); } while(0)

// this is pushing a reg
#define PUSHr(_r)  do {                                                 \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x92<<20) | (SP<<16) | (1<<(_r)) ); \
        asm_output1("push %s",gpn(_r)); } while (0)

// STMDB
#define PUSH_mask(_mask)  do {                                          \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x92<<20) | (SP<<16) | (_mask) ); \
        asm_output1("push %x", (_mask));} while (0)

// this form of PUSH takes a base + offset
// we need to load into scratch reg, then push onto stack
#define PUSHm(_off,_b)  do {                                            \
        NanoAssert( (int)(_off)>0 );                                    \
        underrunProtect(8);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x92<<20) | (SP<<16) | (1<<(Scratch)) ); \
        *(--_nIns) = (NIns)( COND_AL | (0x59<<20) | ((_b)<<16) | ((Scratch)<<12) | ((_off)&0xFFF) ); \
        asm_output2("push %d(%s)",(_off),gpn(_b)); } while (0)

#define POPr(_r) do {                                                   \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x8B<<20) | (SP<<16) | (1<<(_r)) ); \
        asm_output1("pop %s",gpn(_r));} while (0)

#define POP_mask(_mask) do {                                            \
        underrunProtect(4);                                             \
        *(--_nIns) = (NIns)( COND_AL | (0x8B<<20) | (SP<<16) | (_mask) ); \
        asm_output1("pop %x", (_mask));} while (0)

// PC always points to current instruction + 8, so when calculating pc-relative
// offsets, use PC+8.
#define PC_OFFSET_FROM(target,frompc) ((intptr_t)(target) - ((intptr_t)(frompc) + 8))
#define isS12(offs) ((-(1<<12)) <= (offs) && (offs) < (1<<12))

#define B_cond(_c,_t)                           \
    B_cond_chk(_c,_t,1)

// NB: don't use COND_AL here, we shift the condition into place!
#define JMP(_t)                                 \
    B_cond_chk(AL,_t,1)

#define JMP_nochk(_t)                           \
    B_cond_chk(AL,_t,0)

#define JA(t)   do {B_cond(HI,t); asm_output1("ja 0x%08x",(unsigned int)t); } while(0)
#define JNA(t)  do {B_cond(LS,t); asm_output1("jna 0x%08x",(unsigned int)t); } while(0)
#define JB(t)   do {B_cond(CC,t); asm_output1("jb 0x%08x",(unsigned int)t); } while(0)
#define JNB(t)  do {B_cond(CS,t); asm_output1("jnb 0x%08x",(unsigned int)t); } while(0)
#define JE(t)   do {B_cond(EQ,t); asm_output1("je 0x%08x",(unsigned int)t); } while(0)
#define JNE(t)  do {B_cond(NE,t); asm_output1("jne 0x%08x",(unsigned int)t); } while(0)                     
#define JBE(t)  do {B_cond(LS,t); asm_output1("jbe 0x%08x",(unsigned int)t); } while(0)
#define JNBE(t) do {B_cond(HI,t); asm_output1("jnbe 0x%08x",(unsigned int)t); } while(0)
#define JAE(t)  do {B_cond(CS,t); asm_output1("jae 0x%08x",(unsigned int)t); } while(0)
#define JNAE(t) do {B_cond(CC,t); asm_output1("jnae 0x%08x",(unsigned int)t); } while(0)
#define JL(t)   do {B_cond(LT,t); asm_output1("jl 0x%08x",(unsigned int)t); } while(0)  
#define JNL(t)  do {B_cond(GE,t); asm_output1("jnl 0x%08x",(unsigned int)t); } while(0)
#define JLE(t)  do {B_cond(LE,t); asm_output1("jle 0x%08x",(unsigned int)t); } while(0)
#define JNLE(t) do {B_cond(GT,t); asm_output1("jnle 0x%08x",(unsigned int)t); } while(0)
#define JGE(t)  do {B_cond(GE,t); asm_output1("jge 0x%08x",(unsigned int)t); } while(0)
#define JNGE(t) do {B_cond(LT,t); asm_output1("jnge 0x%08x",(unsigned int)t); } while(0)
#define JG(t)   do {B_cond(GT,t); asm_output1("jg 0x%08x",(unsigned int)t); } while(0)  
#define JNG(t)  do {B_cond(LE,t); asm_output1("jng 0x%08x",(unsigned int)t); } while(0)
#define JC(t)   do {B_cond(CS,t); asm_output1("bcs 0x%08x",(unsigned int)t); } while(0)
#define JNC(t)  do {B_cond(CC,t); asm_output1("bcc 0x%08x",(unsigned int)t); } while(0)
#define JO(t)   do {B_cond(VS,t); asm_output1("bvs 0x%08x",(unsigned int)t); } while(0)
#define JNO(t)  do {B_cond(VC,t); asm_output1("bvc 0x%08x",(unsigned int)t); } while(0)

// used for testing result of an FP compare on x86; not used on arm.
// JP = comparison  false
#define JP(t)   do {NanoAssert(0); B_cond(NE,t); asm_output1("jp 0x%08x",t); } while(0) 

// JNP = comparison true
#define JNP(t)  do {NanoAssert(0); B_cond(EQ,t); asm_output1("jnp 0x%08x",t); } while(0)


// MOV(EQ) _r, #1 
// EOR(NE) _r, _r
#define SET(_r,_cond,_opp)                                              \
    underrunProtect(8);                                                 \
    *(--_nIns) = (NIns)( (_opp<<28) | (1<<21) | ((_r)<<16) | ((_r)<<12) | (_r) ); \
    *(--_nIns) = (NIns)( (_cond<<28) | (0x3A<<20) | ((_r)<<12) | (1) );


#define SETE(r)     do {SET(r,EQ,NE); asm_output1("sete %s",gpn(r)); } while(0)
#define SETL(r)     do {SET(r,LT,GE); asm_output1("setl %s",gpn(r)); } while(0)
#define SETLE(r)    do {SET(r,LE,GT); asm_output1("setle %s",gpn(r)); } while(0)
#define SETG(r)     do {SET(r,GT,LE); asm_output1("setg %s",gpn(r)); } while(0)
#define SETGE(r)    do {SET(r,GE,LT); asm_output1("setge %s",gpn(r)); } while(0)
#define SETB(r)     do {SET(r,CC,CS); asm_output1("setb %s",gpn(r)); } while(0)
#define SETBE(r)    do {SET(r,LS,HI); asm_output1("setb %s",gpn(r)); } while(0)
#define SETAE(r)    do {SET(r,CS,CC); asm_output1("setae %s",gpn(r)); } while(0)
#define SETA(r)     do {SET(r,HI,LS); asm_output1("seta %s",gpn(r)); } while(0)
#define SETO(r)     do {SET(r,VS,LS); asm_output1("seto %s",gpn(r)); } while(0)
#define SETC(r)     do {SET(r,CS,LS); asm_output1("setc %s",gpn(r)); } while(0)

// This zero-extends a reg that has been set using one of the SET macros,
// but is a NOOP on ARM/Thumb
#define MOVZX8(r,r2)

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
                asm_output3("ldrsh %s,[%s, #%d]",gpn(_d), gpn(_b), (_off)); \
                *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | ((_d)<<16) | ((_d)<<12) | rem ); \
                *(--_nIns) = (NIns)( COND_AL | (0x1A<<20) | ((_d)<<12) | (2<<7)| (_d) ); \
                *(--_nIns) = (NIns)( COND_AL | (0x3B<<20) | ((_d)<<12) | (((_off)>>2)&0xFF) ); \
                asm_output2("mov %s,%d",gpn(_d),(_off));                \
            }                                                           \
        } else {                                                        \
            if ((_off)>-256) {                                          \
                underrunProtect(4);                                     \
                *(--_nIns) = (NIns)( COND_AL | (0x15<<20) | ((_b)<<16) | ((_d)<<12) |  ((((-(_off))>>4)&0xF)<<8) | (0xF<<4) | ((-(_off))&0xF)  ); \
                asm_output3("ldrsh %s,[%s, #%d]",gpn(_d), gpn(_b), (_off)); \
            } else if ((_off)>=-510){                                   \
                underrunProtect(8);                                     \
                int rem = -(_off) - 255;                                \
                NanoAssert(rem<256);                                    \
                *(--_nIns) = (NIns)( COND_AL | (0x15<<20) | ((_d)<<16) | ((_d)<<12) |  ((((rem)>>4)&0xF)<<8) | (0xF<<4) | ((rem)&0xF)  ); \
                *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<22) | ((_b)<<16) | ((_d)<<12) | (0xFF) ); \
            } else NanoAssert(0);                                        \
        }                                                               \
    } while(0)

#define STMIA(_b, _mask) do {                                           \
        underrunProtect(4);                                             \
        NanoAssert(((_mask)&rmask(_b))==0 && isU8(_mask));              \
        *(--_nIns) = (NIns)(COND_AL | (0x8A<<20) | ((_b)<<16) | (_mask)&0xFF); \
        asm_output2("stmia %s!,{0x%x}", gpn(_b), _mask); \
    } while (0)

#define LDMIA(_b, _mask) do {                                           \
        underrunProtect(4);                                             \
        NanoAssert(((_mask)&rmask(_b))==0 && isU8(_mask));              \
        *(--_nIns) = (NIns)(COND_AL | (0x8B<<20) | ((_b)<<16) | (_mask)&0xFF); \
        asm_output2("ldmia %s!,{0x%x}", gpn(_b), (_mask)); \
    } while (0)

#define MRS(_d) do {                            \
        underrunProtect(4);                     \
        *(--_nIns) = (NIns)(COND_AL | (0x10<<20) | (0xF<<16) | ((_d)<<12)); \
        asm_output1("msr %s", gpn(_d));                                 \
    } while (0)

/*
 * VFP
 */

#define FMDRR(_Dm,_Rd,_Rn) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(IsFpReg(_Dm) && IsGpReg(_Rd) && IsGpReg(_Rn));       \
        *(--_nIns) = (NIns)( COND_AL | (0xC4<<20) | ((_Rn)<<16) | ((_Rd)<<12) | (0xB1<<4) | (FpRegNum(_Dm)) ); \
        asm_output3("fmdrr %s,%s,%s", gpn(_Dm), gpn(_Rd), gpn(_Rn));    \
    } while (0)

#define FMRRD(_Rd,_Rn,_Dm) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(IsGpReg(_Rd) && IsGpReg(_Rn) && IsFpReg(_Dm));       \
        *(--_nIns) = (NIns)( COND_AL | (0xC5<<20) | ((_Rn)<<16) | ((_Rd)<<12) | (0xB1<<4) | (FpRegNum(_Dm)) ); \
        asm_output3("fmrrd %s,%s,%s", gpn(_Rd), gpn(_Rn), gpn(_Dm));    \
    } while (0)

#define FSTD(_Dd,_Rn,_offs) do {                                        \
        underrunProtect(4);                                             \
        NanoAssert((((_offs) & 3) == 0) && isS8((_offs) >> 2));         \
        NanoAssert(IsFpReg(_Dd) && !IsFpReg(_Rn));                      \
        int negflag = 1<<23;                                            \
        intptr_t offs = (_offs);                                        \
        if (_offs < 0) {                                                \
            negflag = 0<<23;                                            \
            offs = -(offs);                                             \
        }                                                               \
        *(--_nIns) = (NIns)( COND_AL | (0xD0<<20) | ((_Rn)<<16) | (FpRegNum(_Dd)<<12) | (0xB<<8) | negflag | ((offs>>2)&0xff) ); \
        asm_output3("fstd %s,%s(%d)", gpn(_Dd), gpn(_Rn), _offs);    \
    } while (0)

#define FLDD_chk(_Dd,_Rn,_offs,_chk) do {                               \
        if(_chk) underrunProtect(4);                                    \
        NanoAssert((((_offs) & 3) == 0) && isS8((_offs) >> 2));         \
        NanoAssert(IsFpReg(_Dd) && !IsFpReg(_Rn));                      \
        int negflag = 1<<23;                                            \
        intptr_t offs = (_offs);                                        \
        if (_offs < 0) {                                                \
            negflag = 0<<23;                                            \
            offs = -(offs);                                             \
        }                                                               \
        *(--_nIns) = (NIns)( COND_AL | (0xD1<<20) | ((_Rn)<<16) | (FpRegNum(_Dd)<<12) | (0xB<<8) | negflag | ((offs>>2)&0xff) ); \
        asm_output3("fldd %s,%s(%d)", gpn(_Dd), gpn(_Rn), _offs);       \
    } while (0)
#define FLDD(_Dd,_Rn,_offs) FLDD_chk(_Dd,_Rn,_offs,1)

#define FSITOD(_Dd,_Sm) do {                                            \
        underrunProtect(4);                                             \
        NanoAssert(IsFpReg(_Dd) && ((_Sm) == FpSingleScratch));         \
        *(--_nIns) = (NIns)( COND_AL | (0xEB8<<16) | (FpRegNum(_Dd)<<12) | (0x2F<<6) | (0<<5) | (0x7) ); \
        asm_output2("fsitod %s,%s", gpn(_Dd), gpn(_Sm));                \
    } while (0)


#define FUITOD(_Dd,_Sm) do {                                            \
        underrunProtect(4);                                             \
        NanoAssert(IsFpReg(_Dd) && ((_Sm) == FpSingleScratch));         \
        *(--_nIns) = (NIns)( COND_AL | (0xEB8<<16) | (FpRegNum(_Dd)<<12) | (0x2D<<6) | (0<<5) | (0x7) ); \
        asm_output2("fuitod %s,%s", gpn(_Dd), gpn(_Sm));                \
    } while (0)

#define FMSR(_Sn,_Rd) do {                                              \
        underrunProtect(4);                                             \
        NanoAssert(((_Sn) == FpSingleScratch) && IsGpReg(_Rd));         \
        *(--_nIns) = (NIns)( COND_AL | (0xE0<<20) | (0x7<<16) | ((_Rd)<<12) | (0xA<<8) | (0<<7) | (0x1<<4) ); \
        asm_output2("fmsr %s,%s", gpn(_Sn), gpn(_Rd));                  \
    } while (0)

#define FNEGD(_Dd,_Dm) do {                                             \
        underrunProtect(4);                                             \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dm));                       \
        *(--_nIns) = (NIns)( COND_AL | (0xEB1<<16) | (FpRegNum(_Dd)<<12) | (0xB4<<4) | (FpRegNum(_Dm)) ); \
        asm_output2("fnegd %s,%s", gpn(_Dd), gpn(_Dm));                 \
    } while (0)

#define FADDD(_Dd,_Dn,_Dm) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dn) && IsFpReg(_Dm));       \
        *(--_nIns) = (NIns)( COND_AL | (0xE3<<20) | (FpRegNum(_Dn)<<16) | (FpRegNum(_Dd)<<12) | (0xB0<<4) | (FpRegNum(_Dm)) ); \
        asm_output3("faddd %s,%s,%s", gpn(_Dd), gpn(_Dn), gpn(_Dm));    \
    } while (0)

#define FSUBD(_Dd,_Dn,_Dm) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dn) && IsFpReg(_Dm));       \
        *(--_nIns) = (NIns)( COND_AL | (0xE3<<20) | (FpRegNum(_Dn)<<16) | (FpRegNum(_Dd)<<12) | (0xB4<<4) | (FpRegNum(_Dm)) ); \
        asm_output3("fsubd %s,%s,%s", gpn(_Dd), gpn(_Dn), gpn(_Dm));    \
    } while (0)

#define FMULD(_Dd,_Dn,_Dm) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dn) && IsFpReg(_Dm));       \
        *(--_nIns) = (NIns)( COND_AL | (0xE2<<20) | (FpRegNum(_Dn)<<16) | (FpRegNum(_Dd)<<12) | (0xB0<<4) | (FpRegNum(_Dm)) ); \
        asm_output3("fmuld %s,%s,%s", gpn(_Dd), gpn(_Dn), gpn(_Dm));    \
    } while (0)

#define FDIVD(_Dd,_Dn,_Dm) do {                                         \
        underrunProtect(4);                                             \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dn) && IsFpReg(_Dm));       \
        *(--_nIns) = (NIns)( COND_AL | (0xE8<<20) | (FpRegNum(_Dn)<<16) | (FpRegNum(_Dd)<<12) | (0xB0<<4) | (FpRegNum(_Dm)) ); \
        asm_output3("fmuld %s,%s,%s", gpn(_Dd), gpn(_Dn), gpn(_Dm));    \
    } while (0)

#define FMSTAT() do {                               \
        underrunProtect(4);                         \
        *(--_nIns) = (NIns)( COND_AL | 0x0EF1FA10); \
        asm_output("fmstat");                       \
    } while (0)

#define FCMPD(_Dd,_Dm) do {                                             \
        underrunProtect(4);                                             \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dm));                       \
        *(--_nIns) = (NIns)( COND_AL | (0xEB4<<16) | (FpRegNum(_Dd)<<12) | (0xB4<<4) | (FpRegNum(_Dm)) ); \
        asm_output2("fcmpd %s,%s", gpn(_Dd), gpn(_Dm));                 \
    } while (0)

#define FCPYD(_Dd,_Dm) do {                                             \
        underrunProtect(4);                                             \
        NanoAssert(IsFpReg(_Dd) && IsFpReg(_Dm));                       \
        *(--_nIns) = (NIns)( COND_AL | (0xEB0<<16) | (FpRegNum(_Dd)<<12) | (0xB4<<4) | (FpRegNum(_Dm)) ); \
        asm_output2("fcpyd %s,%s", gpn(_Dd), gpn(_Dm));                 \
    } while (0)
}
#endif // __nanojit_NativeThumb__
