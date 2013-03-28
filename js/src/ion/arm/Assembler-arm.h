/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_cpu_arm_assembler_h__
#define jsion_cpu_arm_assembler_h__

#include "mozilla/MathAlgorithms.h"
#include "mozilla/Util.h"

#include "ion/shared/Assembler-shared.h"
#include "assembler/assembler/AssemblerBufferWithConstantPool.h"
#include "ion/CompactBuffer.h"
#include "ion/IonCode.h"
#include "ion/arm/Architecture-arm.h"
#include "ion/shared/IonAssemblerBufferWithConstantPools.h"

namespace js {
namespace ion {

//NOTE: there are duplicates in this list!
// sometimes we want to specifically refer to the
// link register as a link register (bl lr is much
// clearer than bl r14).  HOWEVER, this register can
// easily be a gpr when it is not busy holding the return
// address.
static const Register r0  = { Registers::r0 };
static const Register r1  = { Registers::r1 };
static const Register r2  = { Registers::r2 };
static const Register r3  = { Registers::r3 };
static const Register r4  = { Registers::r4 };
static const Register r5  = { Registers::r5 };
static const Register r6  = { Registers::r6 };
static const Register r7  = { Registers::r7 };
static const Register r8  = { Registers::r8 };
static const Register r9  = { Registers::r9 };
static const Register r10 = { Registers::r10 };
static const Register r11 = { Registers::r11 };
static const Register r12 = { Registers::ip };
static const Register ip  = { Registers::ip };
static const Register sp  = { Registers::sp };
static const Register r14 = { Registers::lr };
static const Register lr  = { Registers::lr };
static const Register pc  = { Registers::pc };

static const Register ScratchRegister = {Registers::ip};

static const Register OsrFrameReg = r3;
static const Register ArgumentsRectifierReg = r8;
static const Register CallTempReg0 = r5;
static const Register CallTempReg1 = r6;
static const Register CallTempReg2 = r7;
static const Register CallTempReg3 = r8;
static const Register CallTempReg4 = r0;
static const Register CallTempReg5 = r1;

static const Register CallTempNonArgRegs[] = { r5, r6, r7, r8 };
static const uint32_t NumCallTempNonArgRegs =
    mozilla::ArrayLength(CallTempNonArgRegs);

static const Register PreBarrierReg = r1;

static const Register InvalidReg = { Registers::invalid_reg };
static const FloatRegister InvalidFloatReg = { FloatRegisters::invalid_freg };

static const Register JSReturnReg_Type = r3;
static const Register JSReturnReg_Data = r2;
static const Register StackPointer = sp;
static const Register FramePointer = InvalidReg;
static const Register ReturnReg = r0;
static const FloatRegister ReturnFloatReg = { FloatRegisters::d0 };
static const FloatRegister ScratchFloatReg = { FloatRegisters::d1 };

static const FloatRegister d0  = {FloatRegisters::d0};
static const FloatRegister d1  = {FloatRegisters::d1};
static const FloatRegister d2  = {FloatRegisters::d2};
static const FloatRegister d3  = {FloatRegisters::d3};
static const FloatRegister d4  = {FloatRegisters::d4};
static const FloatRegister d5  = {FloatRegisters::d5};
static const FloatRegister d6  = {FloatRegisters::d6};
static const FloatRegister d7  = {FloatRegisters::d7};
static const FloatRegister d8  = {FloatRegisters::d8};
static const FloatRegister d9  = {FloatRegisters::d9};
static const FloatRegister d10 = {FloatRegisters::d10};
static const FloatRegister d11 = {FloatRegisters::d11};
static const FloatRegister d12 = {FloatRegisters::d12};
static const FloatRegister d13 = {FloatRegisters::d13};
static const FloatRegister d14 = {FloatRegisters::d14};
static const FloatRegister d15 = {FloatRegisters::d15};

// For maximal awesomeness, 8 should be sufficent.
// ldrd/strd (dual-register load/store) operate in a single cycle
// when the address they are dealing with is 8 byte aligned.
// Also, the ARM abi wants the stack to be 8 byte aligned at
// function boundaries.  I'm trying to make sure this is always true.
static const uint32_t StackAlignment = 8;
static const bool StackKeptAligned = true;
static const uint32_t NativeFrameSize = sizeof(void*);
static const uint32_t AlignmentAtPrologue = sizeof(void*);

static const Scale ScalePointer = TimesFour;

class Instruction;
class InstBranchImm;
uint32_t RM(Register r);
uint32_t RS(Register r);
uint32_t RD(Register r);
uint32_t RT(Register r);
uint32_t RN(Register r);

uint32_t maybeRD(Register r);
uint32_t maybeRT(Register r);
uint32_t maybeRN(Register r);

Register toRN (Instruction &i);
Register toRM (Instruction &i);
Register toRD (Instruction &i);
Register toR (Instruction &i);

class VFPRegister;
uint32_t VD(VFPRegister vr);
uint32_t VN(VFPRegister vr);
uint32_t VM(VFPRegister vr);

class VFPRegister
{
  public:
    // What type of data is being stored in this register?
    // UInt / Int are specifically for vcvt, where we need
    // to know how the data is supposed to be converted.
    enum RegType {
        Double = 0x0,
        Single = 0x1,
        UInt   = 0x2,
        Int    = 0x3
    };

  protected:
    RegType kind : 2;
    // ARM doesn't have more than 32 registers...
    // don't take more bits than we'll need.
    // Presently, I don't have plans to address the upper
    // and lower halves of the double registers seprately, so
    // 5 bits should suffice.  If I do decide to address them seprately
    // (vmov, I'm looking at you), I will likely specify it as a separate
    // field.
    uint32_t _code : 5;
    bool _isInvalid : 1;
    bool _isMissing : 1;

    VFPRegister(int  r, RegType k)
      : kind(k), _code (r), _isInvalid(false), _isMissing(false)
    { }

  public:
    VFPRegister()
      : _isInvalid(true), _isMissing(false)
    { }

    VFPRegister(bool b)
      : _isInvalid(false), _isMissing(b)
    { }

    VFPRegister(FloatRegister fr)
      : kind(Double), _code(fr.code()), _isInvalid(false), _isMissing(false)
    {
        JS_ASSERT(_code == (unsigned)fr.code());
    }

    VFPRegister(FloatRegister fr, RegType k)
      : kind(k), _code (fr.code()), _isInvalid(false), _isMissing(false)
    {
        JS_ASSERT(_code == (unsigned)fr.code());
    }
    bool isDouble() { return kind == Double; }
    bool isSingle() { return kind == Single; }
    bool isFloat() { return (kind == Double) || (kind == Single); }
    bool isInt() { return (kind == UInt) || (kind == Int); }
    bool isSInt()   { return kind == Int; }
    bool isUInt()   { return kind == UInt; }
    bool equiv(VFPRegister other) { return other.kind == kind; }
    size_t size() { return (kind == Double) ? 8 : 4; }
    bool isInvalid();
    bool isMissing();

    VFPRegister doubleOverlay();
    VFPRegister singleOverlay();
    VFPRegister sintOverlay();
    VFPRegister uintOverlay();

    struct VFPRegIndexSplit;
    VFPRegIndexSplit encode();

    // for serializing values
    struct VFPRegIndexSplit {
        const uint32_t block : 4;
        const uint32_t bit : 1;

      private:
        friend VFPRegIndexSplit js::ion::VFPRegister::encode();

        VFPRegIndexSplit (uint32_t block_, uint32_t bit_)
          : block(block_), bit(bit_)
        {
            JS_ASSERT (block == block_);
            JS_ASSERT(bit == bit_);
        }
    };

    uint32_t code() const {
        return _code;
    }
};

// For being passed into the generic vfp instruction generator when
// there is an instruction that only takes two registers
extern VFPRegister NoVFPRegister;

struct ImmTag : public Imm32
{
    ImmTag(JSValueTag mask)
      : Imm32(int32_t(mask))
    { }
};

struct ImmType : public ImmTag
{
    ImmType(JSValueType type)
      : ImmTag(JSVAL_TYPE_TO_TAG(type))
    { }
};

enum Index {
    Offset = 0 << 21 | 1<<24,
    PreIndex = 1<<21 | 1 << 24,
    PostIndex = 0 << 21 | 0 << 24
    // The docs were rather unclear on this. it sounds like
    // 1<<21 | 0 << 24 encodes dtrt
};

// Seriously, wtf arm
enum IsImmOp2_ {
    IsImmOp2    = 1 << 25,
    IsNotImmOp2 = 0 << 25
};
enum IsImmDTR_ {
    IsImmDTR    = 0 << 25,
    IsNotImmDTR = 1 << 25
};
// For the extra memory operations, ldrd, ldrsb, ldrh
enum IsImmEDTR_ {
    IsImmEDTR    = 1 << 22,
    IsNotImmEDTR = 0 << 22
};


enum ShiftType {
    LSL = 0, // << 5
    LSR = 1, // << 5
    ASR = 2, // << 5
    ROR = 3, // << 5
    RRX = ROR // RRX is encoded as ROR with a 0 offset.
};

// The actual codes that get set by instructions
// and the codes that are checked by the conditions below.
struct ConditionCodes
{
    bool Zero : 1;
    bool Overflow : 1;
    bool Carry : 1;
    bool Minus : 1;
};

// Modes for STM/LDM.
// Names are the suffixes applied to
// the instruction.
enum DTMMode {
    A = 0 << 24, // empty / after
    B = 1 << 24, // full / before
    D = 0 << 23, // decrement
    I = 1 << 23, // increment
    DA = D | A,
    DB = D | B,
    IA = I | A,
    IB = I | B
};

enum DTMWriteBack {
    WriteBack   = 1 << 21,
    NoWriteBack = 0 << 21
};

enum SetCond_ {
    SetCond   = 1 << 20,
    NoSetCond = 0 << 20
};
enum LoadStore {
    IsLoad  = 1 << 20,
    IsStore = 0 << 20
};
// You almost never want to use this directly.
// Instead, you wantto pass in a signed constant,
// and let this bit be implicitly set for you.
// this is however, necessary if we want a negative index
enum IsUp_ {
    IsUp   = 1 << 23,
    IsDown = 0 << 23
};
enum ALUOp {
    op_mov = 0xd << 21,
    op_mvn = 0xf << 21,
    op_and = 0x0 << 21,
    op_bic = 0xe << 21,
    op_eor = 0x1 << 21,
    op_orr = 0xc << 21,
    op_adc = 0x5 << 21,
    op_add = 0x4 << 21,
    op_sbc = 0x6 << 21,
    op_sub = 0x2 << 21,
    op_rsb = 0x3 << 21,
    op_rsc = 0x7 << 21,
    op_cmn = 0xb << 21,
    op_cmp = 0xa << 21,
    op_teq = 0x9 << 21,
    op_tst = 0x8 << 21,
    op_invalid = -1
};


enum MULOp {
    opm_mul   = 0 << 21,
    opm_mla   = 1 << 21,
    opm_umaal = 2 << 21,
    opm_mls   = 3 << 21,
    opm_umull = 4 << 21,
    opm_umlal = 5 << 21,
    opm_smull = 6 << 21,
    opm_smlal = 7 << 21
};
enum BranchTag {
    op_b = 0x0a000000,
    op_b_mask = 0x0f000000,
    op_b_dest_mask = 0x00ffffff,
    op_bl = 0x0b000000,
    op_blx = 0x012fff30,
    op_bx  = 0x012fff10
};

// Just like ALUOp, but for the vfp instruction set.
enum VFPOp {
    opv_mul  = 0x2 << 20,
    opv_add  = 0x3 << 20,
    opv_sub  = 0x3 << 20 | 0x1 << 6,
    opv_div  = 0x8 << 20,
    opv_mov  = 0xB << 20 | 0x1 << 6,
    opv_abs  = 0xB << 20 | 0x3 << 6,
    opv_neg  = 0xB << 20 | 0x1 << 6 | 0x1 << 16,
    opv_sqrt = 0xB << 20 | 0x3 << 6 | 0x1 << 16,
    opv_cmp  = 0xB << 20 | 0x1 << 6 | 0x4 << 16,
    opv_cmpz  = 0xB << 20 | 0x1 << 6 | 0x5 << 16
};
// Negate the operation, AND negate the immediate that we were passed in.
ALUOp ALUNeg(ALUOp op, Register dest, Imm32 *imm, Register *negDest);
bool can_dbl(ALUOp op);
bool condsAreSafe(ALUOp op);
// If there is a variant of op that has a dest (think cmp/sub)
// return that variant of it.
ALUOp getDestVariant(ALUOp op);

static const ValueOperand JSReturnOperand = ValueOperand(JSReturnReg_Type, JSReturnReg_Data);

// All of these classes exist solely to shuffle data into the various operands.
// For example Operand2 can be an imm8, a register-shifted-by-a-constant or
// a register-shifted-by-a-register.  I represent this in C++ by having a
// base class Operand2, which just stores the 32 bits of data as they will be
// encoded in the instruction.  You cannot directly create an Operand2
// since it is tricky, and not entirely sane to do so.  Instead, you create
// one of its child classes, e.g. Imm8.  Imm8's constructor takes a single
// integer argument.  Imm8 will verify that its argument can be encoded
// as an ARM 12 bit imm8, encode it using an Imm8data, and finally call
// its parent's (Operand2) constructor with the Imm8data.  The Operand2
// constructor will then call the Imm8data's encode() function to extract
// the raw bits from it.  In the future, we should be able to extract
// data from the Operand2 by asking it for its component Imm8data
// structures.  The reason this is so horribly round-about is I wanted
// to have Imm8 and RegisterShiftedRegister inherit directly from Operand2
// but have all of them take up only a single word of storage.
// I also wanted to avoid passing around raw integers at all
// since they are error prone.
namespace datastore {
struct Reg
{
    // the "second register"
    uint32_t RM : 4;
    // do we get another register for shifting
    uint32_t RRS : 1;
    ShiftType Type : 2;
    // I'd like this to be a more sensible encoding, but that would
    // need to be a struct and that would not pack :(
    uint32_t ShiftAmount : 5;
    uint32_t pad : 20;

    Reg(uint32_t rm, ShiftType type, uint32_t rsr, uint32_t shiftamount)
      : RM(rm), RRS(rsr), Type(type), ShiftAmount(shiftamount), pad(0)
    { }

    uint32_t encode() {
        return RM | RRS << 4 | Type << 5 | ShiftAmount << 7;
    }
};

// Op2 has a mode labelled "<imm8m>", which is arm's magical
// immediate encoding.  Some instructions actually get 8 bits of
// data, which is called Imm8Data below.  These should have edit
// distance > 1, but this is how it is for now.
struct Imm8mData
{
  private:
    uint32_t data : 8;
    uint32_t rot : 4;
    // Throw in an extra bit that will be 1 if we can't encode this
    // properly.  if we can encode it properly, a simple "|" will still
    // suffice to meld it into the instruction.
    uint32_t buff : 19;
  public:
    uint32_t invalid : 1;

    uint32_t encode() {
        JS_ASSERT(!invalid);
        return data | rot << 8;
    };

    // Default constructor makes an invalid immediate.
    Imm8mData()
      : data(0xff), rot(0xf), invalid(1)
    { }

    Imm8mData(uint32_t data_, uint32_t rot_)
      : data(data_), rot(rot_), invalid(0)
    {
        JS_ASSERT(data == data_);
        JS_ASSERT(rot == rot_);
    }
};

struct Imm8Data
{
  private:
    uint32_t imm4L : 4;
    uint32_t pad : 4;
    uint32_t imm4H : 4;

  public:
    uint32_t encode() {
        return imm4L | (imm4H << 8);
    };
    Imm8Data(uint32_t imm) : imm4L(imm&0xf), imm4H(imm>>4) {
        JS_ASSERT(imm <= 0xff);
    }
};

// VLDR/VSTR take an 8 bit offset, which is implicitly left shifted
// by 2.
struct Imm8VFPOffData
{
  private:
    uint32_t data;

  public:
    uint32_t encode() {
        return data;
    };
    Imm8VFPOffData(uint32_t imm) : data (imm) {
        JS_ASSERT((imm & ~(0xff)) == 0);
    }
};

// ARM can magically encode 256 very special immediates to be moved
// into a register.
struct Imm8VFPImmData
{
  private:
    uint32_t imm4L : 4;
    uint32_t pad : 12;
    uint32_t imm4H : 4;
    int32_t isInvalid : 12;

  public:
    Imm8VFPImmData()
      : imm4L(-1U & 0xf), imm4H(-1U & 0xf), isInvalid(-1)
    { }

    Imm8VFPImmData(uint32_t imm)
      : imm4L(imm&0xf), imm4H(imm>>4), isInvalid(0)
    {
        JS_ASSERT(imm <= 0xff);
    }

    uint32_t encode() {
        if (isInvalid != 0)
            return -1;
        return imm4L | (imm4H << 16);
    };
};

struct Imm12Data
{
    uint32_t data : 12;
    uint32_t encode() {
        return data;
    }

    Imm12Data(uint32_t imm)
      : data(imm)
    {
        JS_ASSERT(data == imm);
    }

};

struct RIS
{
    uint32_t ShiftAmount : 5;
    uint32_t encode () {
        return ShiftAmount;
    }

    RIS(uint32_t imm)
      : ShiftAmount(imm)
    {
        JS_ASSERT(ShiftAmount == imm);
    }
};

struct RRS
{
    uint32_t MustZero : 1;
    // the register that holds the shift amount
    uint32_t RS : 4;

    RRS(uint32_t rs)
      : RS(rs)
    {
        JS_ASSERT(rs == RS);
    }

    uint32_t encode () {
        return RS << 1;
    }
};

} // namespace datastore

class MacroAssemblerARM;
class Operand;

class Operand2
{
    friend class Operand;
    friend class MacroAssemblerARM;

  public:
    uint32_t oper : 31;
    uint32_t invalid : 1;

  protected:
    Operand2(datastore::Imm8mData base)
      : oper(base.invalid ? -1 : (base.encode() | (uint32_t)IsImmOp2)),
        invalid(base.invalid)
    { }

    Operand2(datastore::Reg base)
      : oper(base.encode() | (uint32_t)IsNotImmOp2)
    { }

  private:
    Operand2(int blob)
      : oper(blob)
    { }

  public:
    uint32_t encode() {
        return oper;
    }
};

class Imm8 : public Operand2
{
  public:
    static datastore::Imm8mData encodeImm(uint32_t imm) {
        // In gcc, clz is undefined if you call it with 0.
        if (imm == 0)
            return datastore::Imm8mData(0, 0);
        int left = js_bitscan_clz32(imm) & 30;
        // See if imm is a simple value that can be encoded with a rotate of 0.
        // This is effectively imm <= 0xff, but I assume this can be optimized
        // more
        if (left >= 24)
            return datastore::Imm8mData(imm, 0);

        // Mask out the 8 bits following the first bit that we found, see if we
        // have 0 yet.
        int no_imm = imm & ~(0xff << (24 - left));
        if (no_imm == 0) {
            return  datastore::Imm8mData(imm >> (24 - left), ((8+left) >> 1));
        }
        // Look for the most signifigant bit set, once again.
        int right = 32 - (js_bitscan_clz32(no_imm)  & 30);
        // If it is in the bottom 8 bits, there is a chance that this is a
        // wraparound case.
        if (right >= 8)
            return datastore::Imm8mData();
        // Rather than masking out bits and checking for 0, just rotate the
        // immediate that we were passed in, and see if it fits into 8 bits.
        unsigned int mask = imm << (8 - right) | imm >> (24 + right);
        if (mask <= 0xff)
            return datastore::Imm8mData(mask, (8-right) >> 1);
        return datastore::Imm8mData();
    }
    // pair template?
    struct TwoImm8mData
    {
        datastore::Imm8mData fst, snd;

        TwoImm8mData()
          : fst(), snd()
        { }

        TwoImm8mData(datastore::Imm8mData _fst, datastore::Imm8mData _snd)
          : fst(_fst), snd(_snd)
        { }
    };

    static TwoImm8mData encodeTwoImms(uint32_t);
    Imm8(uint32_t imm)
      : Operand2(encodeImm(imm))
    { }
};

class Op2Reg : public Operand2
{
  public:
    Op2Reg(Register rm, ShiftType type, datastore::RIS shiftImm)
      : Operand2(datastore::Reg(rm.code(), type, 0, shiftImm.encode()))
    { }

    Op2Reg(Register rm, ShiftType type, datastore::RRS shiftReg)
      : Operand2(datastore::Reg(rm.code(), type, 1, shiftReg.encode()))
    { }
};

class O2RegImmShift : public Op2Reg
{
  public:
    O2RegImmShift(Register rn, ShiftType type, uint32_t shift)
      : Op2Reg(rn, type, datastore::RIS(shift))
    { }
};

class O2RegRegShift : public Op2Reg
{
  public:
    O2RegRegShift(Register rn, ShiftType type, Register rs)
      : Op2Reg(rn, type, datastore::RRS(rs.code()))
    { }
};

O2RegImmShift O2Reg(Register r);
O2RegImmShift lsl (Register r, int amt);
O2RegImmShift lsr (Register r, int amt);
O2RegImmShift asr (Register r, int amt);
O2RegImmShift rol (Register r, int amt);
O2RegImmShift ror (Register r, int amt);

O2RegRegShift lsl (Register r, Register amt);
O2RegRegShift lsr (Register r, Register amt);
O2RegRegShift asr (Register r, Register amt);
O2RegRegShift ror (Register r, Register amt);

// An offset from a register to be used for ldr/str.  This should include
// the sign bit, since ARM has "signed-magnitude" offsets.  That is it encodes
// an unsigned offset, then the instruction specifies if the offset is positive
// or negative.  The +/- bit is necessary if the instruction set wants to be
// able to have a negative register offset e.g. ldr pc, [r1,-r2];
class DtrOff
{
    uint32_t data;

  protected:
    DtrOff(datastore::Imm12Data immdata, IsUp_ iu)
      : data(immdata.encode() | (uint32_t)IsImmDTR | ((uint32_t)iu))
    { }

    DtrOff(datastore::Reg reg, IsUp_ iu = IsUp)
      : data(reg.encode() | (uint32_t) IsNotImmDTR | iu)
    { }

  public:
    uint32_t encode() { return data; }
};

class DtrOffImm : public DtrOff
{
  public:
    DtrOffImm(int32_t imm)
      : DtrOff(datastore::Imm12Data(mozilla::Abs(imm)), imm >= 0 ? IsUp : IsDown)
    {
        JS_ASSERT(mozilla::Abs(imm) < 4096);
    }
};

class DtrOffReg : public DtrOff
{
    // These are designed to be called by a constructor of a subclass.
    // Constructing the necessary RIS/RRS structures are annoying
  protected:
    DtrOffReg(Register rn, ShiftType type, datastore::RIS shiftImm, IsUp_ iu = IsUp)
      : DtrOff(datastore::Reg(rn.code(), type, 0, shiftImm.encode()), iu)
    { }

    DtrOffReg(Register rn, ShiftType type, datastore::RRS shiftReg, IsUp_ iu = IsUp)
      : DtrOff(datastore::Reg(rn.code(), type, 1, shiftReg.encode()), iu)
    { }
};

class DtrRegImmShift : public DtrOffReg
{
  public:
    DtrRegImmShift(Register rn, ShiftType type, uint32_t shift, IsUp_ iu = IsUp)
      : DtrOffReg(rn, type, datastore::RIS(shift), iu)
    { }
};

class DtrRegRegShift : public DtrOffReg
{
  public:
    DtrRegRegShift(Register rn, ShiftType type, Register rs, IsUp_ iu = IsUp)
      : DtrOffReg(rn, type, datastore::RRS(rs.code()), iu)
    { }
};

// we will frequently want to bundle a register with its offset so that we have
// an "operand" to a load instruction.
class DTRAddr
{
    uint32_t data;

  public:
    DTRAddr(Register reg, DtrOff dtr)
      : data(dtr.encode() | (reg.code() << 16))
    { }

    uint32_t encode() {
        return data;
    }
    Register getBase() {
        return Register::FromCode((data >> 16) &0xf);
    }
  private:
    friend class Operand;
    DTRAddr(uint32_t blob)
      : data(blob)
    { }
};

// Offsets for the extended data transfer instructions:
// ldrsh, ldrd, ldrsb, etc.
class EDtrOff
{
    uint32_t data;

  protected:
    EDtrOff(datastore::Imm8Data imm8, IsUp_ iu = IsUp)
      : data(imm8.encode() | IsImmEDTR | (uint32_t)iu)
    { }

    EDtrOff(Register rm, IsUp_ iu = IsUp)
      : data(rm.code() | IsNotImmEDTR | iu)
    { }

  public:
    uint32_t encode() {
        return data;
    }
};

class EDtrOffImm : public EDtrOff
{
  public:
    EDtrOffImm(int32_t imm)
      : EDtrOff(datastore::Imm8Data(mozilla::Abs(imm)), (imm >= 0) ? IsUp : IsDown)
    {
        JS_ASSERT(mozilla::Abs(imm) < 256);
    }
};

// this is the most-derived class, since the extended data
// transfer instructions don't support any sort of modifying the
// "index" operand
class EDtrOffReg : public EDtrOff
{
  public:
    EDtrOffReg(Register rm)
      : EDtrOff(rm)
    { }
};

class EDtrAddr
{
    uint32_t data;

  public:
    EDtrAddr(Register r, EDtrOff off)
      : data(RN(r) | off.encode())
    { }

    uint32_t encode() {
        return data;
    }
};

class VFPOff
{
    uint32_t data;

  protected:
    VFPOff(datastore::Imm8VFPOffData imm, IsUp_ isup)
      : data(imm.encode() | (uint32_t)isup)
    { }

  public:
    uint32_t encode() {
        return data;
    }
};

class VFPOffImm : public VFPOff
{
  public:
    VFPOffImm(int32_t imm)
      : VFPOff(datastore::Imm8VFPOffData(mozilla::Abs(imm) / 4), imm < 0 ? IsDown : IsUp)
    {
        JS_ASSERT(mozilla::Abs(imm) <= 255 * 4);
    }
};
class VFPAddr
{
    friend class Operand;

    uint32_t data;

  protected:
    VFPAddr(uint32_t blob)
      : data(blob)
    { }

  public:
    VFPAddr(Register base, VFPOff off)
      : data(RN(base) | off.encode())
    { }

    uint32_t encode() {
        return data;
    }
};

class VFPImm {
    uint32_t data;

  public:
    VFPImm(uint32_t top);

    uint32_t encode() {
        return data;
    }
    bool isValid() {
        return data != -1U;
    }
};

// A BOffImm is an immediate that is used for branches. Namely, it is the offset that will
// be encoded in the branch instruction. This is the only sane way of constructing a branch.
class BOffImm
{
    uint32_t data;

  public:
    uint32_t encode() {
        return data;
    }
    int32_t decode() {
        return ((((int32_t)data) << 8) >> 6) + 8;
    }

    explicit BOffImm(int offset)
      : data ((offset - 8) >> 2 & 0x00ffffff)
    {
        JS_ASSERT((offset & 0x3) == 0);
        JS_ASSERT(isInRange(offset));
    }
    static bool isInRange(int offset)
    {
        if ((offset - 8) < -33554432)
            return false;
        if ((offset - 8) > 33554428)
            return false;
        return true;
    }
    static const int INVALID = 0x00800000;
    BOffImm()
      : data(INVALID)
    { }

    bool isInvalid() {
        return data == uint32_t(INVALID);
    }
    Instruction *getDest(Instruction *src);

  private:
    friend class InstBranchImm;
    BOffImm(Instruction &inst);
};

class Imm16
{
    uint32_t lower : 12;
    uint32_t pad : 4;
    uint32_t upper : 4;
    uint32_t invalid : 12;

  public:
    Imm16();
    Imm16(uint32_t imm);
    Imm16(Instruction &inst);

    uint32_t encode() {
        return lower | upper << 16;
    }
    uint32_t decode() {
        return lower | upper << 12;
    }

    bool isInvalid () {
        return invalid;
    }
};

// FP Instructions use a different set of registers,
// with a different encoding, so this calls for a different class.
// which will be implemented later
// IIRC, this has been subsumed by vfpreg.
class FloatOp
{
    uint32_t data;
};

/* I would preffer that these do not exist, since there are essentially
* no instructions that would ever take more than one of these, however,
* the MIR wants to only have one type of arguments to functions, so bugger.
*/
class Operand
{
    // the encoding of registers is the same for OP2, DTR and EDTR
    // yet the type system doesn't let us express this, so choices
    // must be made.
  public:
    enum Tag_ {
        OP2,
        MEM,
        FOP
    };

  private:
    Tag_ Tag : 3;
    uint32_t reg : 5;
    int32_t offset;
    uint32_t data;

  public:
    Operand (Register reg_)
      : Tag(OP2), reg(reg_.code())
    { }

    Operand (FloatRegister freg)
      : Tag(FOP), reg(freg.code())
    { }

    Operand (Register base, Imm32 off)
      : Tag(MEM), reg(base.code()), offset(off.value)
    { }

    Operand (Register base, int32_t off)
      : Tag(MEM), reg(base.code()), offset(off)
    { }

    Operand (const Address &addr)
      : Tag(MEM), reg(addr.base.code()), offset(addr.offset)
    { }

    Tag_ getTag() const {
        return Tag;
    }

    Operand2 toOp2() const {
        JS_ASSERT(Tag == OP2);
        return O2Reg(Register::FromCode(reg));
    }

    Register toReg() const {
        JS_ASSERT(Tag == OP2);
        return Register::FromCode(reg);
    }

    void toAddr(Register *r, Imm32 *dest) const {
        JS_ASSERT(Tag == MEM);
        *r = Register::FromCode(reg);
        *dest = Imm32(offset);
    }
    Address toAddress() const {
        return Address(Register::FromCode(reg), offset);
    }
    int32_t disp() const {
        JS_ASSERT(Tag == MEM);
        return offset;
    }

    int32_t base() const {
        JS_ASSERT(Tag == MEM);
        return reg;
    }
    Register baseReg() const {
        return Register::FromCode(reg);
    }
    DTRAddr toDTRAddr() const {
        return DTRAddr(baseReg(), DtrOffImm(offset));
    }
    VFPAddr toVFPAddr() const {
        return VFPAddr(baseReg(), VFPOffImm(offset));
    }
};

void
PatchJump(CodeLocationJump &jump_, CodeLocationLabel label);
class InstructionIterator;
class Assembler;
typedef js::ion::AssemblerBufferWithConstantPool<1024, 4, Instruction, Assembler, 1> ARMBuffer;

class Assembler
{
  public:
    // ARM conditional constants
    typedef enum {
        EQ = 0x00000000, // Zero
        NE = 0x10000000, // Non-zero
        CS = 0x20000000,
        CC = 0x30000000,
        MI = 0x40000000,
        PL = 0x50000000,
        VS = 0x60000000,
        VC = 0x70000000,
        HI = 0x80000000,
        LS = 0x90000000,
        GE = 0xa0000000,
        LT = 0xb0000000,
        GT = 0xc0000000,
        LE = 0xd0000000,
        AL = 0xe0000000
    } ARMCondition;

    enum Condition {
        Equal = EQ,
        NotEqual = NE,
        Above = HI,
        AboveOrEqual = CS,
        Below = CC,
        BelowOrEqual = LS,
        GreaterThan = GT,
        GreaterThanOrEqual = GE,
        LessThan = LT,
        LessThanOrEqual = LE,
        Overflow = VS,
        Signed = MI,
        Unsigned = PL,
        Zero = EQ,
        NonZero = NE,
        Always  = AL,

        VFP_NotEqualOrUnordered = NE,
        VFP_Equal = EQ,
        VFP_Unordered = VS,
        VFP_NotUnordered = VC,
        VFP_GreaterThanOrEqualOrUnordered = CS,
        VFP_GreaterThanOrEqual = GE,
        VFP_GreaterThanOrUnordered = HI,
        VFP_GreaterThan = GT,
        VFP_LessThanOrEqualOrUnordered = LE,
        VFP_LessThanOrEqual = LS,
        VFP_LessThanOrUnordered = LT,
        VFP_LessThan = CC // MI is valid too.
    };

    // Bit set when a DoubleCondition does not map to a single ARM condition.
    // The macro assembler has to special-case these conditions, or else
    // ConditionFromDoubleCondition will complain.
    static const int DoubleConditionBitSpecial = 0x1;

    enum DoubleCondition {
        // These conditions will only evaluate to true if the comparison is ordered - i.e. neither operand is NaN.
        DoubleOrdered = VFP_NotUnordered,
        DoubleEqual = VFP_Equal,
        DoubleNotEqual = VFP_NotEqualOrUnordered | DoubleConditionBitSpecial,
        DoubleGreaterThan = VFP_GreaterThan,
        DoubleGreaterThanOrEqual = VFP_GreaterThanOrEqual,
        DoubleLessThan = VFP_LessThan,
        DoubleLessThanOrEqual = VFP_LessThanOrEqual,
        // If either operand is NaN, these conditions always evaluate to true.
        DoubleUnordered = VFP_Unordered,
        DoubleEqualOrUnordered = VFP_Equal | DoubleConditionBitSpecial,
        DoubleNotEqualOrUnordered = VFP_NotEqualOrUnordered,
        DoubleGreaterThanOrUnordered = VFP_GreaterThanOrUnordered,
        DoubleGreaterThanOrEqualOrUnordered = VFP_GreaterThanOrEqualOrUnordered,
        DoubleLessThanOrUnordered = VFP_LessThanOrUnordered,
        DoubleLessThanOrEqualOrUnordered = VFP_LessThanOrEqualOrUnordered
    };

    Condition getCondition(uint32_t inst) {
        return (Condition) (0xf0000000 & inst);
    }
    static inline Condition ConditionFromDoubleCondition(DoubleCondition cond) {
        JS_ASSERT(!(cond & DoubleConditionBitSpecial));
        return static_cast<Condition>(cond);
    }

    // :( this should be protected, but since CodeGenerator
    // wants to use it, It needs to go out here :(

    BufferOffset nextOffset() {
        return m_buffer.nextOffset();
    }

  protected:
    BufferOffset labelOffset (Label *l) {
        return BufferOffset(l->bound());
    }

    Instruction * editSrc (BufferOffset bo) {
        return m_buffer.getInst(bo);
    }
  public:
    void resetCounter();
    uint32_t actualOffset(uint32_t) const;
    uint32_t actualIndex(uint32_t) const;
    static uint8_t *PatchableJumpAddress(IonCode *code, uint32_t index);
    BufferOffset actualOffset(BufferOffset) const;
  protected:

    // structure for fixing up pc-relative loads/jumps when a the machine code
    // gets moved (executable copy, gc, etc.)
    struct RelativePatch
    {
        // the offset within the code buffer where the value is loaded that
        // we want to fix-up
        BufferOffset offset;
        void *target;
        Relocation::Kind kind;
        void fixOffset(ARMBuffer &m_buffer) {
            offset = BufferOffset(offset.getOffset() + m_buffer.poolSizeBefore(offset.getOffset()));
        }
        RelativePatch(BufferOffset offset, void *target, Relocation::Kind kind)
          : offset(offset),
            target(target),
            kind(kind)
        { }
    };

    // TODO: this should actually be a pool-like object
    //       It is currently a big hack, and probably shouldn't exist
    class JumpPool;
    js::Vector<CodeLabel, 0, SystemAllocPolicy> codeLabels_;
    js::Vector<RelativePatch, 8, SystemAllocPolicy> jumps_;
    js::Vector<JumpPool *, 0, SystemAllocPolicy> jumpPools_;
    js::Vector<BufferOffset, 0, SystemAllocPolicy> tmpJumpRelocations_;
    js::Vector<BufferOffset, 0, SystemAllocPolicy> tmpDataRelocations_;
    js::Vector<BufferOffset, 0, SystemAllocPolicy> tmpPreBarriers_;
    class JumpPool : TempObject
    {
        BufferOffset start;
        uint32_t size;
        bool fixup(IonCode *code, uint8_t *data);
    };

    CompactBufferWriter jumpRelocations_;
    CompactBufferWriter dataRelocations_;
    CompactBufferWriter relocations_;
    CompactBufferWriter preBarriers_;

    bool enoughMemory_;

    //typedef JSC::AssemblerBufferWithConstantPool<1024, 4, 4, js::ion::Assembler> ARMBuffer;
    ARMBuffer m_buffer;

    // There is now a semi-unified interface for instruction generation.
    // During assembly, there is an active buffer that instructions are
    // being written into, but later, we may wish to modify instructions
    // that have already been created.  In order to do this, we call the
    // same assembly function, but pass it a destination address, which
    // will be overwritten with a new instruction. In order to do this very
    // after assembly buffers no longer exist, when calling with a third
    // dest parameter, a this object is still needed.  dummy always happens
    // to be null, but we shouldn't be looking at it in any case.
    static Assembler *dummy;
    Pool pools_[4];
    Pool *int32Pool;
    Pool *doublePool;

  public:
    Assembler()
      : enoughMemory_(true),
        m_buffer(4, 4, 0, &pools_[0], 8),
        int32Pool(m_buffer.getPool(1)),
        doublePool(m_buffer.getPool(0)),
        isFinished(false),
        dtmActive(false),
        dtmCond(Always)
    {
    }

    // We need to wait until an AutoIonContextAlloc is created by the
    // IonMacroAssembler, before allocating any space.
    void initWithAllocator() {
        m_buffer.initWithAllocator();

        // Set up the backwards double region
        new (&pools_[2]) Pool (1024, 8, 4, 8, 8, true);
        // Set up the backwards 32 bit region
        new (&pools_[3]) Pool (4096, 4, 4, 8, 4, true, true);
        // Set up the forwards double region
        new (doublePool) Pool (1024, 8, 4, 8, 8, false, false, &pools_[2]);
        // Set up the forwards 32 bit region
        new (int32Pool) Pool (4096, 4, 4, 8, 4, false, true, &pools_[3]);
        for (int i = 0; i < 4; i++) {
            if (pools_[i].poolData == NULL) {
                m_buffer.fail_oom();
                return;
            }
        }
    }

    static Condition InvertCondition(Condition cond);

    // MacroAssemblers hold onto gcthings, so they are traced by the GC.
    void trace(JSTracer *trc);
    void writeRelocation(BufferOffset src) {
        tmpJumpRelocations_.append(src);
    }

    // As opposed to x86/x64 version, the data relocation has to be executed
    // before to recover the pointer, and not after.
    void writeDataRelocation(const ImmGCPtr &ptr) {
        if (ptr.value)
            tmpDataRelocations_.append(nextOffset());
    }
    void writePrebarrierOffset(CodeOffsetLabel label) {
        tmpPreBarriers_.append(BufferOffset(label.offset()));
    }

    enum RelocBranchStyle {
        B_MOVWT,
        B_LDR_BX,
        B_LDR,
        B_MOVW_ADD
    };

    enum RelocStyle {
        L_MOVWT,
        L_LDR
    };

  public:
    // Given the start of a Control Flow sequence, grab the value that is finally branched to
    // given the start of a function that loads an address into a register get the address that
    // ends up in the register.
    template <class Iter>
    static const uint32_t * getCF32Target(Iter *iter);

    static uintptr_t getPointer(uint8_t *);
    template <class Iter>
    static const uint32_t * getPtr32Target(Iter *iter, Register *dest = NULL, RelocStyle *rs = NULL);

    bool oom() const;

    void setPrinter(Sprinter *sp) {
    }

  private:
    bool isFinished;
  public:
    void finish();
    void executableCopy(void *buffer);
    void processCodeLabels(uint8_t *rawCode);
    void copyJumpRelocationTable(uint8_t *dest);
    void copyDataRelocationTable(uint8_t *dest);
    void copyPreBarrierTable(uint8_t *dest);

    bool addCodeLabel(CodeLabel label);

    // Size of the instruction stream, in bytes.
    size_t size() const;
    // Size of the jump relocation table, in bytes.
    size_t jumpRelocationTableBytes() const;
    size_t dataRelocationTableBytes() const;
    size_t preBarrierTableBytes() const;

    // Size of the data table, in bytes.
    size_t bytesNeeded() const;

    // Write a blob of binary into the instruction stream *OR*
    // into a destination address. If dest is NULL (the default), then the
    // instruction gets written into the instruction stream. If dest is not null
    // it is interpreted as a pointer to the location that we want the
    // instruction to be written.
    BufferOffset writeInst(uint32_t x, uint32_t *dest = NULL);
    // A static variant for the cases where we don't want to have an assembler
    // object at all. Normally, you would use the dummy (NULL) object.
    static void writeInstStatic(uint32_t x, uint32_t *dest);

  public:
    void writeCodePointer(AbsoluteLabel *label);

    BufferOffset align(int alignment);
    BufferOffset as_nop();
    BufferOffset as_alu(Register dest, Register src1, Operand2 op2,
                ALUOp op, SetCond_ sc = NoSetCond, Condition c = Always);

    BufferOffset as_mov(Register dest,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_mvn(Register dest, Operand2 op2,
                SetCond_ sc = NoSetCond, Condition c = Always);
    // logical operations
    BufferOffset as_and(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_bic(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_eor(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_orr(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    // mathematical operations
    BufferOffset as_adc(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_add(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_sbc(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_sub(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_rsb(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_rsc(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always);
    // test operations
    BufferOffset as_cmn(Register src1, Operand2 op2,
                Condition c = Always);
    BufferOffset as_cmp(Register src1, Operand2 op2,
                Condition c = Always);
    BufferOffset as_teq(Register src1, Operand2 op2,
                Condition c = Always);
    BufferOffset as_tst(Register src1, Operand2 op2,
                Condition c = Always);

    // Not quite ALU worthy, but useful none the less:
    // These also have the isue of these being formatted
    // completly differently from the standard ALU operations.
    BufferOffset as_movw(Register dest, Imm16 imm, Condition c = Always, Instruction *pos = NULL);
    BufferOffset as_movt(Register dest, Imm16 imm, Condition c = Always, Instruction *pos = NULL);

    BufferOffset as_genmul(Register d1, Register d2, Register rm, Register rn,
                   MULOp op, SetCond_ sc, Condition c = Always);
    BufferOffset as_mul(Register dest, Register src1, Register src2,
                SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_mla(Register dest, Register acc, Register src1, Register src2,
                SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_umaal(Register dest1, Register dest2, Register src1, Register src2,
                  Condition c = Always);
    BufferOffset as_mls(Register dest, Register acc, Register src1, Register src2,
                Condition c = Always);
    BufferOffset as_umull(Register dest1, Register dest2, Register src1, Register src2,
                SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_umlal(Register dest1, Register dest2, Register src1, Register src2,
                SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_smull(Register dest1, Register dest2, Register src1, Register src2,
                SetCond_ sc = NoSetCond, Condition c = Always);
    BufferOffset as_smlal(Register dest1, Register dest2, Register src1, Register src2,
                SetCond_ sc = NoSetCond, Condition c = Always);
    // Data transfer instructions: ldr, str, ldrb, strb.
    // Using an int to differentiate between 8 bits and 32 bits is
    // overkill, but meh
    BufferOffset as_dtr(LoadStore ls, int size, Index mode,
                Register rt, DTRAddr addr, Condition c = Always, uint32_t *dest = NULL);
    // Handles all of the other integral data transferring functions:
    // ldrsb, ldrsh, ldrd, etc.
    // size is given in bits.
    BufferOffset as_extdtr(LoadStore ls, int size, bool IsSigned, Index mode,
                   Register rt, EDtrAddr addr, Condition c = Always, uint32_t *dest = NULL);

    BufferOffset as_dtm(LoadStore ls, Register rn, uint32_t mask,
                DTMMode mode, DTMWriteBack wb, Condition c = Always);
    // load a 32 bit immediate from a pool into a register
    BufferOffset as_Imm32Pool(Register dest, uint32_t value, ARMBuffer::PoolEntry *pe = NULL, Condition c = Always);
    // make a patchable jump that can target the entire 32 bit address space.
    BufferOffset as_BranchPool(uint32_t value, RepatchLabel *label, ARMBuffer::PoolEntry *pe = NULL, Condition c = Always);

    // load a 64 bit floating point immediate from a pool into a register
    BufferOffset as_FImm64Pool(VFPRegister dest, double value, ARMBuffer::PoolEntry *pe = NULL, Condition c = Always);
    // Control flow stuff:

    // bx can *only* branch to a register
    // never to an immediate.
    BufferOffset as_bx(Register r, Condition c = Always, bool isPatchable = false);

    // Branch can branch to an immediate *or* to a register.
    // Branches to immediates are pc relative, branches to registers
    // are absolute
    BufferOffset as_b(BOffImm off, Condition c, bool isPatchable = false);

    BufferOffset as_b(Label *l, Condition c = Always, bool isPatchable = false);
    BufferOffset as_b(BOffImm off, Condition c, BufferOffset inst);

    // blx can go to either an immediate or a register.
    // When blx'ing to a register, we change processor mode
    // depending on the low bit of the register
    // when blx'ing to an immediate, we *always* change processor state.
    BufferOffset as_blx(Label *l);

    BufferOffset as_blx(Register r, Condition c = Always);
    BufferOffset as_bl(BOffImm off, Condition c);
    // bl can only branch+link to an immediate, never to a register
    // it never changes processor state
    BufferOffset as_bl();
    // bl #imm can have a condition code, blx #imm cannot.
    // blx reg can be conditional.
    BufferOffset as_bl(Label *l, Condition c);
    BufferOffset as_bl(BOffImm off, Condition c, BufferOffset inst);

    // VFP instructions!
  private:

    enum vfp_size {
        isDouble = 1 << 8,
        isSingle = 0 << 8
    };

    BufferOffset writeVFPInst(vfp_size sz, uint32_t blob, uint32_t *dest=NULL);
    // Unityped variants: all registers hold the same (ieee754 single/double)
    // notably not included are vcvt; vmov vd, #imm; vmov rt, vn.
    BufferOffset as_vfp_float(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                      VFPOp op, Condition c = Always);

  public:
    BufferOffset as_vadd(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c = Always);

    BufferOffset as_vdiv(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c = Always);

    BufferOffset as_vmul(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c = Always);

    BufferOffset as_vnmul(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c = Always);

    BufferOffset as_vnmla(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c = Always);

    BufferOffset as_vnmls(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c = Always);

    BufferOffset as_vneg(VFPRegister vd, VFPRegister vm, Condition c = Always);

    BufferOffset as_vsqrt(VFPRegister vd, VFPRegister vm, Condition c = Always);

    BufferOffset as_vabs(VFPRegister vd, VFPRegister vm, Condition c = Always);

    BufferOffset as_vsub(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c = Always);

    BufferOffset as_vcmp(VFPRegister vd, VFPRegister vm,
                 Condition c = Always);
    BufferOffset as_vcmpz(VFPRegister vd,  Condition c = Always);

    // specifically, a move between two same sized-registers
    BufferOffset as_vmov(VFPRegister vd, VFPRegister vsrc, Condition c = Always);
    /*xfer between Core and VFP*/
    enum FloatToCore_ {
        FloatToCore = 1 << 20,
        CoreToFloat = 0 << 20
    };

  private:
    enum VFPXferSize {
        WordTransfer   = 0x02000010,
        DoubleTransfer = 0x00400010
    };

  public:
    // Unlike the next function, moving between the core registers and vfp
    // registers can't be *that* properly typed.  Namely, since I don't want to
    // munge the type VFPRegister to also include core registers.  Thus, the core
    // and vfp registers are passed in based on their type, and src/dest is
    // determined by the float2core.

    BufferOffset as_vxfer(Register vt1, Register vt2, VFPRegister vm, FloatToCore_ f2c,
                  Condition c = Always, int idx = 0);

    // our encoding actually allows just the src and the dest (and theiyr types)
    // to uniquely specify the encoding that we are going to use.
    BufferOffset as_vcvt(VFPRegister vd, VFPRegister vm, bool useFPSCR = false,
                         Condition c = Always);
    // hard coded to a 32 bit fixed width result for now
    BufferOffset as_vcvtFixed(VFPRegister vd, bool isSigned, uint32_t fixedPoint, bool toFixed, Condition c = Always);

    /* xfer between VFP and memory*/
    BufferOffset as_vdtr(LoadStore ls, VFPRegister vd, VFPAddr addr,
                 Condition c = Always /* vfp doesn't have a wb option*/,
                 uint32_t *dest = NULL);

    // VFP's ldm/stm work differently from the standard arm ones.
    // You can only transfer a range

    BufferOffset as_vdtm(LoadStore st, Register rn, VFPRegister vd, int length,
                 /*also has update conditions*/Condition c = Always);

    BufferOffset as_vimm(VFPRegister vd, VFPImm imm, Condition c = Always);

    BufferOffset as_vmrs(Register r, Condition c = Always);
    // label operations
    bool nextLink(BufferOffset b, BufferOffset *next);
    void bind(Label *label, BufferOffset boff = BufferOffset());
    void bind(RepatchLabel *label);
    uint32_t currentOffset() {
        return nextOffset().getOffset();
    }
    void retarget(Label *label, Label *target);
    // I'm going to pretend this doesn't exist for now.
    void retarget(Label *label, void *target, Relocation::Kind reloc);
    void Bind(uint8_t *rawCode, AbsoluteLabel *label, const void *address);

    void call(Label *label);
    void call(void *target);

    void as_bkpt();

  public:
    static void TraceJumpRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);
    static void TraceDataRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);

  protected:
    void addPendingJump(BufferOffset src, void *target, Relocation::Kind kind) {
        enoughMemory_ &= jumps_.append(RelativePatch(src, target, kind));
        if (kind == Relocation::IONCODE)
            writeRelocation(src);
    }

  public:
    // The buffer is about to be linked, make sure any constant pools or excess
    // bookkeeping has been flushed to the instruction stream.
    void flush() {
        JS_ASSERT(!isFinished);
        m_buffer.flushPool();
        return;
    }

    // Copy the assembly code to the given buffer, and perform any pending
    // relocations relying on the target address.
    void executableCopy(uint8_t *buffer);

    // Actual assembly emitting functions.

    // Since I can't think of a reasonable default for the mode, I'm going to
    // leave it as a required argument.
    void startDataTransferM(LoadStore ls, Register rm,
                            DTMMode mode, DTMWriteBack update = NoWriteBack,
                            Condition c = Always)
    {
        JS_ASSERT(!dtmActive);
        dtmUpdate = update;
        dtmBase = rm;
        dtmLoadStore = ls;
        dtmLastReg = -1;
        dtmRegBitField = 0;
        dtmActive = 1;
        dtmCond = c;
        dtmMode = mode;
    }

    void transferReg(Register rn) {
        JS_ASSERT(dtmActive);
        JS_ASSERT(rn.code() > dtmLastReg);
        dtmRegBitField |= 1 << rn.code();
        if (dtmLoadStore == IsLoad && rn.code() == 13 && dtmBase.code() == 13) {
            JS_ASSERT("ARM Spec says this is invalid");
        }
    }
    void finishDataTransfer() {
        dtmActive = false;
        as_dtm(dtmLoadStore, dtmBase, dtmRegBitField, dtmMode, dtmUpdate, dtmCond);
    }

    void startFloatTransferM(LoadStore ls, Register rm,
                             DTMMode mode, DTMWriteBack update = NoWriteBack,
                             Condition c = Always)
    {
        JS_ASSERT(!dtmActive);
        dtmActive = true;
        dtmUpdate = update;
        dtmLoadStore = ls;
        dtmBase = rm;
        dtmCond = c;
        dtmLastReg = -1;
        dtmMode = mode;
        dtmDelta = 0;
    }
    void transferFloatReg(VFPRegister rn)
    {
        if (dtmLastReg == -1) {
            vdtmFirstReg = rn.code();
        } else {
            if (dtmDelta == 0) {
                dtmDelta = rn.code() - dtmLastReg;
                JS_ASSERT(dtmDelta == 1 || dtmDelta == -1);
            }
            JS_ASSERT(dtmLastReg >= 0);
            JS_ASSERT(rn.code() == unsigned(dtmLastReg) + dtmDelta);
        }
        dtmLastReg = rn.code();
    }
    void finishFloatTransfer() {
        JS_ASSERT(dtmActive);
        dtmActive = false;
        JS_ASSERT(dtmLastReg != -1);
        dtmDelta = dtmDelta ? dtmDelta : 1;
        // fencepost problem.
        int len = dtmDelta * (dtmLastReg - vdtmFirstReg) + 1;
        as_vdtm(dtmLoadStore, dtmBase,
                VFPRegister(FloatRegister::FromCode(Min(vdtmFirstReg, dtmLastReg))),
                len, dtmCond);
    }

  private:
    int dtmRegBitField;
    int vdtmFirstReg;
    int dtmLastReg;
    int dtmDelta;
    Register dtmBase;
    DTMWriteBack dtmUpdate;
    DTMMode dtmMode;
    LoadStore dtmLoadStore;
    bool dtmActive;
    Condition dtmCond;

  public:
    enum {
        padForAlign8  = (int)0x00,
        padForAlign16 = (int)0x0000,
        padForAlign32 = (int)0xe12fff7f  // 'bkpt 0xffff'
    };

    // API for speaking with the IonAssemblerBufferWithConstantPools
    // generate an initial placeholder instruction that we want to later fix up
    static void insertTokenIntoTag(uint32_t size, uint8_t *load, int32_t token);
    // take the stub value that was written in before, and write in an actual load
    // using the index we'd computed previously as well as the address of the pool start.
    static bool patchConstantPoolLoad(void* loadAddr, void* constPoolAddr);
    // this is a callback for when we have filled a pool, and MUST flush it now.
    // The pool requires the assembler to place a branch past the pool, and it
    // calls this function.
    static uint32_t placeConstantPoolBarrier(int offset);
    // END API

    // move our entire pool into the instruction stream
    // This is to force an opportunistic dump of the pool, prefferably when it
    // is more convenient to do a dump.
    void dumpPool();
    void flushBuffer();
    void enterNoPool();
    void leaveNoPool();
    // this should return a BOffImm, but I didn't want to require everyplace that used the
    // AssemblerBuffer to make that class.
    static ptrdiff_t getBranchOffset(const Instruction *i);
    static void retargetNearBranch(Instruction *i, int offset, Condition cond, bool final = true);
    static void retargetNearBranch(Instruction *i, int offset, bool final = true);
    static void retargetFarBranch(Instruction *i, uint8_t **slot, uint8_t *dest, Condition cond);

    static void writePoolHeader(uint8_t *start, Pool *p, bool isNatural);
    static void writePoolFooter(uint8_t *start, Pool *p, bool isNatural);
    static void writePoolGuard(BufferOffset branch, Instruction *inst, BufferOffset dest);


    static uint32_t patchWrite_NearCallSize();
    static uint32_t nopSize() { return 4; }
    static void patchWrite_NearCall(CodeLocationLabel start, CodeLocationLabel toCall);
    static void patchDataWithValueCheck(CodeLocationLabel label, ImmWord newValue,
                                        ImmWord expectedValue);
    static void patchWrite_Imm32(CodeLocationLabel label, Imm32 imm);
    static uint32_t alignDoubleArg(uint32_t offset) {
        return (offset+1)&~1;
    }
    static uint8_t *nextInstruction(uint8_t *instruction, uint32_t *count = NULL);
    // Toggle a jmp or cmp emitted by toggledJump().

    static void ToggleToJmp(CodeLocationLabel inst_);
    static void ToggleToCmp(CodeLocationLabel inst_);

    static void ToggleCall(CodeLocationLabel inst_, bool enabled);
}; // Assembler

// An Instruction is a structure for both encoding and decoding any and all ARM instructions.
// many classes have not been implemented thusfar.
class Instruction
{
    uint32_t data;

  protected:
    // This is not for defaulting to always, this is for instructions that
    // cannot be made conditional, and have the usually invalid 4b1111 cond field
    Instruction (uint32_t data_, bool fake = false) : data(data_ | 0xf0000000) {
        JS_ASSERT (fake || ((data_ & 0xf0000000) == 0));
    }
    // Standard constructor
    Instruction (uint32_t data_, Assembler::Condition c) : data(data_ | (uint32_t) c) {
        JS_ASSERT ((data_ & 0xf0000000) == 0);
    }
    // You should never create an instruction directly.  You should create a
    // more specific instruction which will eventually call one of these
    // constructors for you.
  public:
    uint32_t encode() const {
        return data;
    }
    // Check if this instruction is really a particular case
    template <class C>
    bool is() const { return C::isTHIS(*this); }

    // safely get a more specific variant of this pointer
    template <class C>
    C *as() const { return C::asTHIS(*this); }

    const Instruction & operator=(const Instruction &src) {
        data = src.data;
        return *this;
    }
    // Since almost all instructions have condition codes, the condition
    // code extractor resides in the base class.
    void extractCond(Assembler::Condition *c) {
        if (data >> 28 != 0xf )
            *c = (Assembler::Condition)(data & 0xf0000000);
    }
    // Get the next instruction in the instruction stream.
    // This does neat things like ignoreconstant pools and their guards.
    Instruction *next();

    // Sometimes, an api wants a uint32_t (or a pointer to it) rather than
    // an instruction.  raw() just coerces this into a pointer to a uint32_t
    const uint32_t *raw() const { return &data; }
    uint32_t size() const { return 4; }
}; // Instruction

// make sure that it is the right size
JS_STATIC_ASSERT(sizeof(Instruction) == 4);

// Data Transfer Instructions
class InstDTR : public Instruction
{
  public:
    enum IsByte_ {
        IsByte = 0x00400000,
        IsWord = 0x00000000
    };
    static const int IsDTR     = 0x04000000;
    static const int IsDTRMask = 0x0c000000;

    // TODO: Replace the initialization with something that is safer.
    InstDTR(LoadStore ls, IsByte_ ib, Index mode, Register rt, DTRAddr addr, Assembler::Condition c)
      : Instruction(ls | ib | mode | RT(rt) | addr.encode() | IsDTR, c)
    { }

    static bool isTHIS(const Instruction &i);
    static InstDTR *asTHIS(Instruction &i);

};
JS_STATIC_ASSERT(sizeof(InstDTR) == sizeof(Instruction));

class InstLDR : public InstDTR
{
  public:
    InstLDR(Index mode, Register rt, DTRAddr addr, Assembler::Condition c)
        : InstDTR(IsLoad, IsWord, mode, rt, addr, c)
    { }
    static bool isTHIS(const Instruction &i);
    static InstLDR *asTHIS(Instruction &i);

};
JS_STATIC_ASSERT(sizeof(InstDTR) == sizeof(InstLDR));

class InstNOP : public Instruction
{
    static const uint32_t NopInst = 0x0320f000;

  public:
    InstNOP()
      : Instruction(NopInst, Assembler::Always)
    { }

    static bool isTHIS(const Instruction &i);
    static InstNOP *asTHIS(Instruction &i);
};

// Branching to a register, or calling a register
class InstBranchReg : public Instruction
{
  protected:
    // Don't use BranchTag yourself, use a derived instruction.
    enum BranchTag {
        IsBX  = 0x012fff10,
        IsBLX = 0x012fff30
    };
    static const uint32_t IsBRegMask = 0x0ffffff0;
    InstBranchReg(BranchTag tag, Register rm, Assembler::Condition c)
      : Instruction(tag | rm.code(), c)
    { }
  public:
    static bool isTHIS (const Instruction &i);
    static InstBranchReg *asTHIS (const Instruction &i);
    // Get the register that is being branched to
    void extractDest(Register *dest);
    // Make sure we are branching to a pre-known register
    bool checkDest(Register dest);
};
JS_STATIC_ASSERT(sizeof(InstBranchReg) == sizeof(Instruction));

// Branching to an immediate offset, or calling an immediate offset
class InstBranchImm : public Instruction
{
  protected:
    enum BranchTag {
        IsB   = 0x0a000000,
        IsBL  = 0x0b000000
    };
    static const uint32_t IsBImmMask = 0x0f000000;

    InstBranchImm(BranchTag tag, BOffImm off, Assembler::Condition c)
      : Instruction(tag | off.encode(), c)
    { }

  public:
    static bool isTHIS (const Instruction &i);
    static InstBranchImm *asTHIS (const Instruction &i);
    void extractImm(BOffImm *dest);
};
JS_STATIC_ASSERT(sizeof(InstBranchImm) == sizeof(Instruction));

// Very specific branching instructions.
class InstBXReg : public InstBranchReg
{
  public:
    static bool isTHIS (const Instruction &i);
    static InstBXReg *asTHIS (const Instruction &i);
};
class InstBLXReg : public InstBranchReg
{
  public:
    InstBLXReg(Register reg, Assembler::Condition c)
      : InstBranchReg(IsBLX, reg, c)
    { }

    static bool isTHIS (const Instruction &i);
    static InstBLXReg *asTHIS (const Instruction &i);
};
class InstBImm : public InstBranchImm
{
  public:
    InstBImm(BOffImm off, Assembler::Condition c)
      : InstBranchImm(IsB, off, c)
    { }

    static bool isTHIS (const Instruction &i);
    static InstBImm *asTHIS (const Instruction &i);
};
class InstBLImm : public InstBranchImm
{
  public:
    InstBLImm(BOffImm off, Assembler::Condition c)
      : InstBranchImm(IsBL, off, c)
    { }

    static bool isTHIS (const Instruction &i);
    static InstBLImm *asTHIS (Instruction &i);
};

// Both movw and movt. The layout of both the immediate and the destination
// register is the same so the code is being shared.
class InstMovWT : public Instruction
{
  protected:
    enum WT {
        IsW = 0x03000000,
        IsT = 0x03400000
    };
    static const uint32_t IsWTMask = 0x0ff00000;

    InstMovWT(Register rd, Imm16 imm, WT wt, Assembler::Condition c)
      : Instruction (RD(rd) | imm.encode() | wt, c)
    { }

  public:
    void extractImm(Imm16 *dest);
    void extractDest(Register *dest);
    bool checkImm(Imm16 dest);
    bool checkDest(Register dest);

    static bool isTHIS (Instruction &i);
    static InstMovWT *asTHIS (Instruction &i);

};
JS_STATIC_ASSERT(sizeof(InstMovWT) == sizeof(Instruction));

class InstMovW : public InstMovWT
{
  public:
    InstMovW (Register rd, Imm16 imm, Assembler::Condition c)
      : InstMovWT(rd, imm, IsW, c)
    { }

    static bool isTHIS (const Instruction &i);
    static InstMovW *asTHIS (const Instruction &i);
};

class InstMovT : public InstMovWT
{
  public:
    InstMovT (Register rd, Imm16 imm, Assembler::Condition c)
      : InstMovWT(rd, imm, IsT, c)
    { }
    static bool isTHIS (const Instruction &i);
    static InstMovT *asTHIS (const Instruction &i);
};

class InstALU : public Instruction
{
    static const int32_t ALUMask = 0xc << 24;
  public:
    InstALU (Register rd, Register rn, Operand2 op2, ALUOp op, SetCond_ sc, Assembler::Condition c)
        : Instruction(RD(rd) | RN(rn) | op2.encode() | op | sc | c)
    { }
    static bool isTHIS (const Instruction &i);
    static InstALU *asTHIS (const Instruction &i);
    void extractOp(ALUOp *ret);
    bool checkOp(ALUOp op);
    void extractDest(Register *ret);
    bool checkDest(Register rd);
    void extractOp1(Register *ret);
    bool checkOp1(Register rn);
    void extractOp2(Operand2 *ret);
};
class InstCMP : public InstALU
{
  public:
    static bool isTHIS (const Instruction &i);
    static InstCMP *asTHIS (const Instruction &i);
};


class InstructionIterator {
  private:
    Instruction *i;
  public:
    InstructionIterator(Instruction *i_) : i(i_) {}
    Instruction *next() {
        i = i->next();
        return cur();
    }
    Instruction *cur() const {
        return i;
    }
};

static const uint32_t NumIntArgRegs = 4;
static const uint32_t NumFloatArgRegs = 8;

#ifdef JS_CPU_ARM_HARDFP
static inline bool
GetIntArgReg(uint32_t usedIntArgs, uint32_t usedFloatArgs, Register *out)
{
   if (usedIntArgs >= NumIntArgRegs)
        return false;
    *out = Register::FromCode(usedIntArgs);
    return true;
}

// Get a register in which we plan to put a quantity that will be used as an
// integer argument.  This differs from GetIntArgReg in that if we have no more
// actual argument registers to use we will fall back on using whatever
// CallTempReg* don't overlap the argument registers, and only fail once those
// run out too.
static inline bool
GetTempRegForIntArg(uint32_t usedIntArgs, uint32_t usedFloatArgs, Register *out)
{
    if (GetIntArgReg(usedIntArgs, usedFloatArgs, out))
        return true;
    // Unfortunately, we have to assume things about the point at which
    // GetIntArgReg returns false, because we need to know how many registers it
    // can allocate.
    usedIntArgs -= NumIntArgRegs;
    if (usedIntArgs >= NumCallTempNonArgRegs)
        return false;
    *out = CallTempNonArgRegs[usedIntArgs];
    return true;
}

static inline bool
GetFloatArgReg(uint32_t usedIntArgs, uint32_t usedFloatArgs, FloatRegister *out)
{
    if (usedFloatArgs >= NumFloatArgRegs)
        return false;
    *out = FloatRegister::FromCode(usedFloatArgs);
    return true;
}

static inline uint32_t
GetIntArgStackDisp(uint32_t usedIntArgs, uint32_t usedFloatArgs, uint32_t *padding)
{
    JS_ASSERT(usedIntArgs >= NumIntArgRegs);
    uint32_t doubleSlots = Max(0, (int32_t)usedFloatArgs - (int32_t)NumFloatArgRegs);
    doubleSlots *= 2;
    int intSlots = usedIntArgs - NumIntArgRegs;
    return (intSlots + doubleSlots + *padding) * STACK_SLOT_SIZE;
}

static inline uint32_t
GetFloatArgStackDisp(uint32_t usedIntArgs, uint32_t usedFloatArgs, uint32_t *padding)
{

    JS_ASSERT(usedFloatArgs >= NumFloatArgRegs);
    uint32_t intSlots = 0;
    if (usedIntArgs > NumIntArgRegs) {
        intSlots = usedIntArgs - NumIntArgRegs;
        // update the amount of padding required.
        *padding += (*padding + usedIntArgs) % 2;
    }
    uint32_t doubleSlots = usedFloatArgs - NumFloatArgRegs;
    doubleSlots *= 2;
    return (intSlots + doubleSlots + *padding) * STACK_SLOT_SIZE;
}
#else
static inline bool
GetIntArgReg(uint32_t arg, uint32_t floatArg, Register *out)
{
    if (arg < NumIntArgRegs) {
        *out = Register::FromCode(arg);
        return true;
    }
    return false;
}

// Get a register in which we plan to put a quantity that will be used as an
// integer argument.  This differs from GetIntArgReg in that if we have no more
// actual argument registers to use we will fall back on using whatever
// CallTempReg* don't overlap the argument registers, and only fail once those
// run out too.
static inline bool
GetTempRegForIntArg(uint32_t usedIntArgs, uint32_t usedFloatArgs, Register *out)
{
    if (GetIntArgReg(usedIntArgs, usedFloatArgs, out))
        return true;
    // Unfortunately, we have to assume things about the point at which
    // GetIntArgReg returns false, because we need to know how many registers it
    // can allocate.
    usedIntArgs -= NumIntArgRegs;
    if (usedIntArgs >= NumCallTempNonArgRegs)
        return false;
    *out = CallTempNonArgRegs[usedIntArgs];
    return true;
}

static inline uint32_t
GetArgStackDisp(uint32_t arg)
{
    JS_ASSERT(arg >= NumIntArgRegs);
    return (arg - NumIntArgRegs) * STACK_SLOT_SIZE;
}

#endif
class DoubleEncoder {
    uint32_t rep(bool b, uint32_t count) {
        uint32_t ret = 0;
        for (uint32_t i = 0; i < count; i++)
            ret = (ret << 1) | b;
        return ret;
    }
    uint32_t encode(uint8_t value) {
        //ARM ARM "VFP modified immediate constants"
        // aBbbbbbb bbcdefgh 000...
        // we want to return the top 32 bits of the double
        // the rest are 0.
        bool a = value >> 7;
        bool b = value >> 6 & 1;
        bool B = !b;
        uint32_t cdefgh = value & 0x3f;
        return a << 31 |
            B << 30 |
            rep(b, 8) << 22 |
            cdefgh << 16;
    }

    struct DoubleEntry
    {
        uint32_t dblTop;
        datastore::Imm8VFPImmData data;

        DoubleEntry()
          : dblTop(-1)
        { }
        DoubleEntry(uint32_t dblTop_, datastore::Imm8VFPImmData data_)
          : dblTop(dblTop_), data(data_)
        { }
    };
    DoubleEntry table [256];

    // grumble singleton, grumble
    static DoubleEncoder _this;
    DoubleEncoder()
    {
        for (int i = 0; i < 256; i++) {
            table[i] = DoubleEntry(encode(i), datastore::Imm8VFPImmData(i));
        }
    }

  public:
    static bool lookup(uint32_t top, datastore::Imm8VFPImmData *ret) {
        for (int i = 0; i < 256; i++) {
            if (_this.table[i].dblTop == top) {
                *ret = _this.table[i].data;
                return true;
            }
        }
        return false;
    }
};

class AutoForbidPools {
    Assembler *masm_;
  public:
    AutoForbidPools(Assembler *masm) : masm_(masm) {
        masm_->enterNoPool();
    }
    ~AutoForbidPools() {
        masm_->leaveNoPool();
    }
};

} // namespace ion
} // namespace js

#endif // jsion_cpu_arm_assembler_h__
