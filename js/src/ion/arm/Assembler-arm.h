/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <dvander@alliedmods.net>
 *   Marty Rosenberg <mrosenberg@mozilla.com>
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jsion_cpu_arm_assembler_h__
#define jsion_cpu_arm_assembler_h__

#include "ion/shared/Assembler-shared.h"
#include "assembler/assembler/ARMAssembler.h"
#include "ion/CompactBuffer.h"
#include "ion/IonCode.h"

namespace js {
namespace ion {

//NOTE: there are duplicates in this list!
// sometimes we want to specifically refer to the
// link register as a link register (bl lr is much
// clearer than bl r14).  HOWEVER, this register can
// easily be a gpr when it is not busy holding the return
// address.
static const Register r0  = { JSC::ARMRegisters::r0 };
static const Register r1  = { JSC::ARMRegisters::r1 };
static const Register r2  = { JSC::ARMRegisters::r2 };
static const Register r3  = { JSC::ARMRegisters::r3 };
static const Register r4  = { JSC::ARMRegisters::r4 };
static const Register r5  = { JSC::ARMRegisters::r5 };
static const Register r6  = { JSC::ARMRegisters::r6 };
static const Register r7  = { JSC::ARMRegisters::r7 };
static const Register r8  = { JSC::ARMRegisters::r8 };
static const Register r9  = { JSC::ARMRegisters::r9 };
static const Register r10 = { JSC::ARMRegisters::r10 };
static const Register r11 = { JSC::ARMRegisters::r11 };
static const Register r12 = { JSC::ARMRegisters::ip };
static const Register ip  = { JSC::ARMRegisters::ip };
static const Register sp  = { JSC::ARMRegisters::sp };
static const Register r14 = { JSC::ARMRegisters::lr };
static const Register lr  = { JSC::ARMRegisters::lr };
static const Register pc  = { JSC::ARMRegisters::pc };

static const Register ScratchRegister = {JSC::ARMRegisters::ip};

static const Register InvalidReg = { JSC::ARMRegisters::invalid_reg };
static const FloatRegister InvalidFloatReg = { JSC::ARMRegisters::invalid_freg };

static const Register JSReturnReg_Type = r3;
static const Register JSReturnReg_Data = r2;
static const Register StackPointer = sp;
static const Register ReturnReg = r0;
static const FloatRegister ScratchFloatReg = { JSC::ARMRegisters::d0 };

static const FloatRegister d0 = {JSC::ARMRegisters::d0};
static const FloatRegister d1 = {JSC::ARMRegisters::d1};
static const FloatRegister d2 = {JSC::ARMRegisters::d2};
static const FloatRegister d3 = {JSC::ARMRegisters::d3};
static const FloatRegister d4 = {JSC::ARMRegisters::d4};
static const FloatRegister d5 = {JSC::ARMRegisters::d5};
static const FloatRegister d6 = {JSC::ARMRegisters::d6};
static const FloatRegister d7 = {JSC::ARMRegisters::d7};
static const FloatRegister d8 = {JSC::ARMRegisters::d8};
static const FloatRegister d9 = {JSC::ARMRegisters::d9};
static const FloatRegister d10 = {JSC::ARMRegisters::d10};
static const FloatRegister d11 = {JSC::ARMRegisters::d11};
static const FloatRegister d12 = {JSC::ARMRegisters::d12};
static const FloatRegister d13 = {JSC::ARMRegisters::d13};
static const FloatRegister d14 = {JSC::ARMRegisters::d14};
static const FloatRegister d15 = {JSC::ARMRegisters::d15};

uint32 RM(Register r);
uint32 RS(Register r);
uint32 RD(Register r);
uint32 RT(Register r);
uint32 RN(Register r);

class VFPRegister;
uint32 VD(VFPRegister vr);
uint32 VN(VFPRegister vr);
uint32 VM(VFPRegister vr);

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
  private:
    RegType kind:2;
    // ARM doesn't have more than 32 registers...
    // don't take more bits than we'll need.
    // Presently, I don't have plans to address the upper
    // and lower halves of the double registers seprately, so
    // 5 bits should suffice.  If I do decide to address them seprately
    // (vmov, I'm looking at you), I will likely specify it as a separate
    // field.
    int _code:5;
    bool _isInvalid:1;
    bool _isMissing:1;
    VFPRegister(int  r, RegType k)
        : kind(k), _code (r), _isInvalid(false), _isMissing(false) {}
  public:
    VFPRegister()
        : _isInvalid(true), _isMissing(false) {}
    VFPRegister(bool b)
        : _isMissing(b), _isInvalid(false) {}
    VFPRegister(FloatRegister fr)
        : kind(Double), _code(fr.code()), _isInvalid(false), _isMissing(false)
    {
        JS_ASSERT(_code == fr.code());
    }
    VFPRegister(FloatRegister fr, RegType k)
        : kind(k), _code (fr.code())
    {
        JS_ASSERT(_code == fr.code());
    }
    bool isDouble() { return kind == Double; }
    bool isSingle() { return kind == Single; }
    bool isFloat() { return (kind == Double) || (kind == Single); }
    bool isInt() { return (kind == UInt) || (kind == Int); }
    bool isSInt()   { return kind == Int; }
    bool isUInt()   { return kind == UInt; }
    bool equiv(VFPRegister other) { return other.kind == kind; }
    VFPRegister doubleOverlay() {
        JS_ASSERT(!_isInvalid);
        if (kind != Double) {
            return VFPRegister(_code >> 1, Double);
        } else {
            return *this;
        }
    }
    VFPRegister singleOverlay() {
        JS_ASSERT(!_isInvalid);
        if (kind == Double) {
            // There are no corresponding float registers for d16-d31
            ASSERT(_code < 16);
            return VFPRegister(_code << 1, Double);
        } else {
            return VFPRegister(_code, Single);
        }
    }
    VFPRegister intOverlay() {
        JS_ASSERT(!_isInvalid);
        if (kind == Double) {
            // There are no corresponding float registers for d16-d31
            ASSERT(_code < 16);
            return VFPRegister(_code << 1, Double);
        } else {
            return VFPRegister(_code, Int);
        }
    }
    bool isInvalid() {
        return _isInvalid;
    }
    bool isMissing() {
        JS_ASSERT(!_isInvalid);
        return _isMissing;
    }
    struct VFPRegIndexSplit;
    VFPRegIndexSplit encode();
    // for serializing values
    struct VFPRegIndexSplit {
        const uint32 block : 4;
        const uint32 bit : 1;
      private:
        friend VFPRegIndexSplit js::ion::VFPRegister::encode();
        VFPRegIndexSplit (uint32 block_, uint32 bit_)
            : block(block_), bit(bit_)
        {
            JS_ASSERT (block == block_);
            JS_ASSERT(bit == bit_);
        }
    };
    uint32 code() {
        return _code;
    }
};
// For being passed into the generic vfp instruction generator when
// there is an instruction that only takes two registers
extern VFPRegister NoVFPRegister;
struct ImmTag : public Imm32
{
    ImmTag(JSValueTag mask)
        : Imm32(int32(mask))
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
    opv_cmp  = 0xB << 20 | 0x1 << 6 | 0x4 << 16
};
// Negate the operation, AND negate the immediate that we were passed in.
ALUOp ALUNeg(ALUOp op, Imm32 *imm);
bool can_dbl(ALUOp op);
bool condsAreSafe(ALUOp op);
// If there is a variant of op that has a dest (think cmp/sub)
// return that variant of it.
ALUOp getDestVariant(ALUOp op);
// Operands that are JSVal's.  Why on earth are these part of the assembler?
class ValueOperand
{
    Register type_;
    Register payload_;

  public:
    ValueOperand(Register type, Register payload)
        : type_(type), payload_(payload)
    { }

    Register typeReg() const {
        return type_;
    }
    Register payloadReg() const {
        return payload_;
    }
};


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
// constructor will then call the Imm8data's toInt() function to extract
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
    uint32 RM : 4;
    // do we get another register for shifting
    uint32 RRS : 1;
    ShiftType Type : 2;
    // I'd like this to be a more sensible encoding, but that would
    // need to be a struct and that would not pack :(
    uint32 ShiftAmount : 5;
    uint32 pad : 20;
    Reg(uint32 rm, ShiftType type, uint32 rsr, uint32 shiftamount)
        : RM(rm), RRS(rsr), Type(type), ShiftAmount(shiftamount), pad(0) {}
    uint32 toInt() {
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
    uint32 data:8;
    uint32 rot:4;
    // Throw in an extra bit that will be 1 if we can't encode this
    // properly.  if we can encode it properly, a simple "|" will still
    // suffice to meld it into the instruction.
    uint32 buff : 19;
  public:
    uint32 invalid : 1;
    uint32 toInt() {
        JS_ASSERT(!invalid);
        return data | rot << 8;
    };
    // Default constructor makes an invalid immediate.
    Imm8mData() : data(0xff), rot(0xf), invalid(1) {}
    Imm8mData(uint32 data_, uint32 rot_)
        : data(data_), rot(rot_), invalid(0)  {}
};

struct Imm8Data
{
  private:
    uint32 imm4L : 4;
    uint32 pad : 4;
    uint32 imm4H : 4;
  public:
    uint32 toInt() {
        return imm4L | (imm4H << 8);
    };
    Imm8Data(uint32 imm) : imm4L(imm&0xf), imm4H(imm>>4) {
        JS_ASSERT(imm <= 0xff);
    }
};
// VLDR/VSTR take an 8 bit offset, which is implicitly left shifted
// by 2.
struct Imm8VFPOffData
{
  private:
    uint32 data;
  public:
    uint32 toInt() {
        return data;
    };
    Imm8VFPOffData(uint32 imm) : data (imm) {
        JS_ASSERT((imm & ~(0xff)) == 0);
    }
};
// ARM can magically encode 256 very special immediates to be moved
// into a register.
struct Imm8VFPImmData
{
  private:
    uint32 imm4L : 4;
    uint32 pad : 12;
    uint32 imm4H : 4;
    int32 isInvalid : 12;
  public:
    uint32 encode() {
        if (isInvalid != 0)
            return -1;
        return imm4L | (imm4H << 16);
    };
    Imm8VFPImmData() : imm4L(-1), imm4H(-1), isInvalid(-1) {}
    Imm8VFPImmData(uint32 imm) : imm4L(imm&0xf), imm4H(imm>>4), isInvalid(0) {
        JS_ASSERT(imm <= 0xff);
    }
};

struct Imm12Data
{
    uint32 data : 12;
    Imm12Data(uint32 imm) : data(imm) { JS_ASSERT(data == imm); }
    uint32 toInt() { return data; }
};

struct RIS
{
    uint32 ShiftAmount : 5;
    RIS(uint32 imm) : ShiftAmount(imm) { ASSERT(ShiftAmount == imm); }
    uint32 toInt () {
        return ShiftAmount;
    }
};
struct RRS
{
    uint32 MustZero : 1;
    // the register that holds the shift amount
    uint32 RS : 4;
    RRS(uint32 rs) : RS(rs) { ASSERT(rs == RS); }
    uint32 toInt () {return RS << 1;}
};

} // datastore

//
class MacroAssemblerARM;
class Operand;
class Operand2
{
  public:
    uint32 oper : 31;
    uint32 invalid : 1;
    // Constructors
  protected:
    friend class MacroAssemblerARM;
    Operand2(datastore::Imm8mData base)
        : oper(base.invalid ? -1 : (base.toInt() | (uint32)IsImmOp2)),
          invalid(base.invalid)
    {
    }
    Operand2(datastore::Reg base) : oper(base.toInt() | (uint32)IsNotImmOp2) {}
  private:
    friend class Operand;
    Operand2(int blob) : oper(blob) {}
  public:
    uint32 toInt() { return oper; }
};

class Imm8 : public Operand2
{
  public:
    static datastore::Imm8mData encodeImm(uint32 imm) {
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
        TwoImm8mData() : fst(), snd() {}
        TwoImm8mData(datastore::Imm8mData _fst, datastore::Imm8mData _snd)
            : fst(_fst), snd(_snd) {}
    };
    static TwoImm8mData encodeTwoImms(uint32);
    Imm8(uint32 imm) : Operand2(encodeImm(imm)) {}
};

class Op2Reg : public Operand2
{
  protected:
    Op2Reg(Register rm, ShiftType type, datastore::RIS shiftImm)
        : Operand2(datastore::Reg(rm.code(), type, 0, shiftImm.toInt())) {}
    Op2Reg(Register rm, ShiftType type, datastore::RRS shiftReg)
        : Operand2(datastore::Reg(rm.code(), type, 1, shiftReg.toInt())) {}
};
class O2RegImmShift : public Op2Reg
{
  public:
    O2RegImmShift(Register rn, ShiftType type, uint32 shift)
        : Op2Reg(rn, type, datastore::RIS(shift)) {}
};

class O2RegRegShift : public Op2Reg
{
  public:
    O2RegRegShift(Register rn, ShiftType type, Register rs)
        : Op2Reg(rn, type, datastore::RRS(rs.code())) {}
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
    uint32 data;
  protected:
    DtrOff(datastore::Imm12Data immdata, IsUp_ iu)
    : data(immdata.toInt() | (uint32)IsImmDTR | ((uint32)iu)) {}
    DtrOff(datastore::Reg reg, IsUp_ iu = IsUp)
        : data(reg.toInt() | (uint32) IsNotImmDTR | iu) {}
  public:
    uint32 toInt() { return data; }
};

class DtrOffImm : public DtrOff
{
  public:
    DtrOffImm(int32 imm)
        : DtrOff(datastore::Imm12Data(abs(imm)), imm >= 0 ? IsUp : IsDown)
    { JS_ASSERT((imm < 4096) && (imm > -4096)); }
};

class DtrOffReg : public DtrOff
{
    // These are designed to be called by a constructor of a subclass.
    // Constructing the necessary RIS/RRS structures are annoying
  protected:
    DtrOffReg(Register rn, ShiftType type, datastore::RIS shiftImm)
        : DtrOff(datastore::Reg(rn.code(), type, 0, shiftImm.toInt())) {}
    DtrOffReg(Register rn, ShiftType type, datastore::RRS shiftReg)
        : DtrOff(datastore::Reg(rn.code(), type, 1, shiftReg.toInt())) {}
};

class DtrRegImmShift : public DtrOffReg
{
  public:
    DtrRegImmShift(Register rn, ShiftType type, uint32 shift)
        : DtrOffReg(rn, type, datastore::RIS(shift)) {}
};

class DtrRegRegShift : public DtrOffReg
{
  public:
    DtrRegRegShift(Register rn, ShiftType type, Register rs)
        : DtrOffReg(rn, type, datastore::RRS(rs.code())) {}
};

// we will frequently want to bundle a register with its offset so that we have
// an "operand" to a load instruction.
class DTRAddr
{
    uint32 data;
  public:
    DTRAddr(Register reg, DtrOff dtr)
        : data(dtr.toInt() | (reg.code() << 16)) {}
    uint32 toInt() { return data; }
  private:
    friend class Operand;
    DTRAddr(uint32 blob) : data(blob) {}
};

// Offsets for the extended data transfer instructions:
// ldrsh, ldrd, ldrsb, etc.
class EDtrOff
{
  protected:
    uint32 data;
    EDtrOff(datastore::Imm8Data imm8, IsUp_ iu = IsUp)
        : data(imm8.toInt() | IsImmEDTR | (uint32)iu) {}
    EDtrOff(Register rm, IsUp_ iu = IsUp)
        : data(rm.code() | IsNotImmEDTR | iu) {}
  public:
    uint32 toInt() { return data; }
};

class EDtrOffImm : public EDtrOff
{
  public:
    EDtrOffImm(uint32 imm)
        : EDtrOff(datastore::Imm8Data(abs(imm)), (imm >= 0) ? IsUp : IsDown) {}
};

// this is the most-derived class, since the extended data
// transfer instructions don't support any sort of modifying the
// "index" operand
class EDtrOffReg : EDtrOff
{
  public:
    EDtrOffReg(Register rm) : EDtrOff(rm) {}
};

class EDtrAddr
{
    uint32 data;
  public:
    EDtrAddr(Register r, EDtrOff off) : data(RN(r) | off.toInt()) {}
    uint32 toInt() { return data; }
};

class VFPOff
{
    uint32 data;
  protected:
    VFPOff(datastore::Imm8VFPOffData imm, IsUp_ isup)
        : data(imm.toInt() | (uint32)isup) {}
  public:
    uint32 encode() { return data; }
};

class VFPOffImm : public VFPOff
{
  public:
    VFPOffImm(uint32 imm)
        : VFPOff(datastore::Imm8VFPOffData(imm >> 2), imm < 0 ? IsDown : IsUp) {}
};
class VFPAddr
{
    uint32 data;
    friend class Operand;
    VFPAddr(uint32 blob) : data(blob) {}
  public:
    VFPAddr(Register base, VFPOff off)
        : data(RN(base) | off.encode())
    {
    }
    uint32 toInt() { return data; }
};

class VFPImm {
    uint32 data;
  public:
    VFPImm(uint32 top);
    uint32 encode() { return data; }
    bool isValid() { return data != -1; }
};

class BOffImm
{
    uint32 data;
  public:
    uint32 toInt() {
        return data;
    }
    BOffImm(int offset) : data (offset >> 2 & 0x00ffffff) {
        JS_ASSERT ((offset & 0x3) == 0);
        JS_ASSERT (offset >= -33554432);
        JS_ASSERT (offset <= 33554428);
    }
};
class Imm16
{
    uint32 lower : 12;
    uint32 pad : 4;
    uint32 upper : 4;
  public:
    Imm16(uint32 imm)
        : lower(imm & 0xfff), pad(0), upper((imm>>12) & 0xf)
    {
        JS_ASSERT(uint32(lower | (upper << 12)) == imm);
    }
    uint32 toInt() { return lower | upper << 16; }
};
// FP Instructions use a different set of registers,
// with a different encoding, so this calls for a different class.
// which will be implemented later
// IIRC, this has been subsumed by vfpreg.
class FloatOp
{
    uint32 data;
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
        DTR,
        EDTR,
        VDTR,
        FOP
    };
  private:
    Tag_ Tag;
    uint32 data;
  public:
    Operand (Operand2 init) : Tag(OP2), data(init.toInt()) {}
    Operand (Register reg)  : Tag(OP2), data(O2Reg(reg).toInt()) {}
    Operand (FloatRegister reg)  : Tag(FOP), data(reg.code()) {}
    Operand (DTRAddr addr) : Tag(DTR), data(addr.toInt()) {}
    Operand (VFPAddr addr) : Tag(VDTR), data(addr.toInt()) {}
    Tag_ getTag() { return Tag; }
    Operand2 toOp2() { return Operand2(data); }
    DTRAddr toDTRAddr() {JS_ASSERT(Tag == DTR); return DTRAddr(data); }
    VFPAddr toVFPAddr() {JS_ASSERT(Tag == VDTR); return VFPAddr(data); }
};

class Assembler
{
  public:
    enum Condition {
        Equal = JSC::ARMAssembler::EQ,
        NotEqual = JSC::ARMAssembler::NE,
        Above = JSC::ARMAssembler::HI,
        AboveOrEqual = JSC::ARMAssembler::CS,
        Below = JSC::ARMAssembler::CC,
        BelowOrEqual = JSC::ARMAssembler::LS,
        GreaterThan = JSC::ARMAssembler::GT,
        GreaterThanOrEqual = JSC::ARMAssembler::GE,
        LessThan = JSC::ARMAssembler::LT,
        LessThanOrEqual = JSC::ARMAssembler::LE,
        Overflow = JSC::ARMAssembler::VS,
        Signed = JSC::ARMAssembler::MI,
        Zero = JSC::ARMAssembler::EQ,
        NonZero = JSC::ARMAssembler::NE,
        Always  = JSC::ARMAssembler::AL,

        NotEqual_Unordered = NotEqual,
        GreaterEqual_Unordered = AboveOrEqual,
        Unordered = Overflow,
        NotUnordered = JSC::ARMAssembler::VC,
        Greater_Unordered = Above
        // there are more.
    };
    Condition getCondition(uint32 inst) {
        return (Condition) (0xf0000000 & inst);
    }
  protected:

    class BufferOffset;
    BufferOffset nextOffset () {
        return BufferOffset(m_buffer.uncheckedSize());
    }
    BufferOffset labelOffset (Label *l) {
        return BufferOffset(l->bound());
    }

    uint32 * editSrc (BufferOffset bo) {
        return (uint32*)(((char*)m_buffer.data()) + bo.getOffset());
    }
    // encodes offsets within a buffer, This should be the ONLY interface
    // for reading data out of a code buffer.
    class BufferOffset
    {
        int offset;
      public:
        friend BufferOffset nextOffset();
        explicit BufferOffset(int offset_) : offset(offset_) {}
        int getOffset() { return offset; }
        BOffImm diffB(BufferOffset other) {
            return BOffImm(offset - other.offset-8);
        }
        BOffImm diffB(Label *other) {
            JS_ASSERT(other->bound());
            return BOffImm(offset - other->offset()-8);
        }
        explicit BufferOffset(Label *l) : offset(l->offset()) {
        }
        BufferOffset() : offset(INT_MIN) {}
        bool assigned() { return offset != INT_MIN; };
    };

    // structure for fixing up pc-relative loads/jumps when a the machine code
    // gets moved (executable copy, gc, etc.)
    struct RelativePatch {
        // the offset within the code buffer where the value is loaded that
        // we want to fix-up
        BufferOffset offset;
        void *target;
        Relocation::Kind kind;

        RelativePatch(BufferOffset offset, void *target, Relocation::Kind kind)
          : offset(offset),
            target(target),
            kind(kind)
        { }
    };

    js::Vector<DeferredData *, 0, SystemAllocPolicy> data_;
    js::Vector<CodeLabel *, 0, SystemAllocPolicy> codeLabels_;
    js::Vector<RelativePatch, 8, SystemAllocPolicy> jumps_;
    CompactBufferWriter jumpRelocations_;
    CompactBufferWriter dataRelocations_;
    CompactBufferWriter relocations_;
    size_t dataBytesNeeded_;

    bool enoughMemory_;

    typedef JSC::AssemblerBufferWithConstantPool<2048, 4, 4, JSC::ARMAssembler> ARMBuffer;
    ARMBuffer m_buffer;



public:
    Assembler()
      : dataBytesNeeded_(0),
        enoughMemory_(true),
        dtmActive(false),
        dtmCond(Always)

    {
    }
    static Condition InvertCondition(Condition cond);

    // MacroAssemblers hold onto gcthings, so they are traced by the GC.
    void trace(JSTracer *trc);

    bool oom() const {
        return m_buffer.oom() ||
            !enoughMemory_ ||
            jumpRelocations_.oom();
    }

    void executableCopy(void *buffer);
    void processDeferredData(IonCode *code, uint8 *data);
    void processCodeLabels(IonCode *code);
    void copyJumpRelocationTable(uint8 *buffer);
    void copyDataRelocationTable(uint8 *buffer);

    bool addDeferredData(DeferredData *data, size_t bytes) {
        data->setOffset(dataBytesNeeded_);
        dataBytesNeeded_ += bytes;
        if (dataBytesNeeded_ >= MAX_BUFFER_SIZE)
            return false;
        return data_.append(data);
    }

    bool addCodeLabel(CodeLabel *label) {
        return codeLabels_.append(label);
    }

    // Size of the instruction stream, in bytes.
    size_t size() const {
        return m_buffer.uncheckedSize();
    }
    // Size of the jump relocation table, in bytes.
    size_t jumpRelocationTableBytes() const {
        return jumpRelocations_.length();
    }
    size_t dataRelocationTableBytes() const {
        return dataRelocations_.length();
    }
    // Size of the data table, in bytes.
    size_t dataSize() const {
        return dataBytesNeeded_;
    }
    size_t bytesNeeded() const {
        return size() +
               dataSize() +
               jumpRelocationTableBytes() +
               dataRelocationTableBytes();
    }
    // write a blob of binary into the instruction stream
    void writeBlob(uint32 x)
    {
        m_buffer.putInt(x);
    }

  public:
    void align(int alignment) {
        while (!m_buffer.isAligned(alignment))
            as_mov(r0, O2Reg(r0));

    }
    void as_alu(Register dest, Register src1, Operand2 op2,
                ALUOp op, SetCond_ sc = NoSetCond, Condition c = Always) {
        writeBlob((int)op | (int)sc | (int) c | op2.toInt() |
                  ((dest == InvalidReg) ? 0 : RD(dest)) |
                  ((src1 == InvalidReg) ? 0 : RN(src1)));
    }
    void as_mov(Register dest,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, InvalidReg, op2, op_mov, sc, c);
    }
    void as_mvn(Register dest, Operand2 op2,
                SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, InvalidReg, op2, op_mvn, sc, c);
    }
    // logical operations
    void as_and(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, src1, op2, op_and, sc, c);
    }
    void as_bic(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, src1, op2, op_bic, sc, c);
    }
    void as_eor(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, src1, op2, op_eor, sc, c);
    }
    void as_orr(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, src1, op2, op_orr, sc, c);
    }
    // mathematical operations
    void as_adc(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, src1, op2, op_adc, sc, c);
    }
    void as_add(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, src1, op2, op_add, sc, c);
    }
    void as_sbc(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, src1, op2, op_sbc, sc, c);
    }
    void as_sub(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, src1, op2, op_sub, sc, c);
    }
    void as_rsb(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, src1, op2, op_rsb, sc, c);
    }
    void as_rsc(Register dest, Register src1,
                Operand2 op2, SetCond_ sc = NoSetCond, Condition c = Always) {
        as_alu(dest, src1, op2, op_rsc, sc, c);
    }
    // test operations
    void as_cmn(Register src1, Operand2 op2,
                Condition c = Always) {
        as_alu(InvalidReg, src1, op2, op_cmn, SetCond, c);
    }
    void as_cmp(Register src1, Operand2 op2,
                Condition c = Always) {
        as_alu(InvalidReg, src1, op2, op_cmp, SetCond, c);
    }
    void as_teq(Register src1, Operand2 op2,
                Condition c = Always) {
        as_alu(InvalidReg, src1, op2, op_teq, SetCond, c);
    }
    void as_tst(Register src1, Operand2 op2,
                Condition c = Always) {
        as_alu(InvalidReg, src1, op2, op_tst, SetCond, c);
    }

    // Not quite ALU worthy, but useful none the less:
    // These also have the isue of these being formatted
    // completly differently from the standard ALU operations.
    void as_movw(Register dest, Imm16 imm, Condition c = Always) {
        JS_ASSERT(hasMOVWT());
        writeBlob(0x03000000 | c | imm.toInt() | RD(dest));
    }
    void as_movt(Register dest, Imm16 imm, Condition c = Always) {
        JS_ASSERT(hasMOVWT());
        writeBlob(0x03400000 | c | imm.toInt() | RD(dest));
    }
    // Data transfer instructions: ldr, str, ldrb, strb.
    // Using an int to differentiate between 8 bits and 32 bits is
    // overkill, but meh
    void as_dtr(LoadStore ls, int size, Index mode,
                Register rt, DTRAddr addr, Condition c = Always)
    {
        JS_ASSERT(size == 32 || size == 8);
        writeBlob( 0x04000000 | ls | (size == 8 ? 0x00400000 : 0) | mode | c |
                   RT(rt) | addr.toInt());
        return;
    }
    // Handles all of the other integral data transferring functions:
    // ldrsb, ldrsh, ldrd, etc.
    // size is given in bits.
    void as_extdtr(LoadStore ls, int size, bool IsSigned, Index mode,
                   Register rt, EDtrAddr addr, Condition c = Always)
    {
        int extra_bits2 = 0;
        int extra_bits1 = 0;
        switch(size) {
          case 8:
            JS_ASSERT(IsSigned);
            JS_ASSERT(ls!=IsStore);
            break;
          case 16:
            //case 32:
            // doesn't need to be handled-- it is handled by the default ldr/str
            extra_bits2 = 0x01;
            extra_bits1 = (ls == IsStore) ? 0 : 1;
            if (IsSigned) {
                JS_ASSERT(ls != IsStore);
                extra_bits2 |= 0x2;
            }
            break;
          case 64:
            if (ls == IsStore) {
                extra_bits2 = 0x3;
            } else {
                extra_bits2 = 0x2;
            }
            extra_bits1 = 0;
            break;
          default:
            JS_NOT_REACHED("SAY WHAT?");
        }
        writeBlob(extra_bits2 << 5 | extra_bits1 << 20 | 0x90 |
                  addr.toInt() | RT(rt) | c);
        return;
    }

    void as_dtm(LoadStore ls, Register rn, uint32 mask,
                DTMMode mode, DTMWriteBack wb, Condition c = Always)
    {
        writeBlob(0x08000000 | RN(rn) | ls |
                  mode | mask | c | wb);

        return;
    }

    // Control flow stuff:

    // bx can *only* branch to a register
    // never to an immediate.
    void as_bx(Register r, Condition c = Always)
    {
        writeBlob(((int) c) | op_bx | r.code());
    }

    // Branch can branch to an immediate *or* to a register.
    // Branches to immediates are pc relative, branches to registers
    // are absolute
    void as_b(BOffImm off, Condition c)
    {
        writeBlob(((int)c) | op_b | off.toInt());
    }

    void as_b(Label *l, Condition c = Always)
    {
        BufferOffset next = nextOffset();
        if (l->bound()) {
            as_b(BufferOffset(l).diffB(next), c);
        } else {
            // Ugh.  int32 :(
            int32 old = l->use(next.getOffset());
            if (old == LabelBase::INVALID_OFFSET) {
                old = -4;
            }
            // This will currently throw an assertion if we couldn't actually
            // encode the offset of the branch.
            as_b(BOffImm(old), c);
        }
    }
    void as_b(BOffImm off, Condition c, BufferOffset inst)
    {
        *editSrc(inst) = ((int)c) | op_b | off.toInt();
    }

    // blx can go to either an immediate or a register.
    // When blx'ing to a register, we change processor mode
    // depending on the low bit of the register
    // when blx'ing to an immediate, we *always* change processor state.
    void as_blx(Label *l)
    {
        JS_NOT_REACHED("Feature NYI");
    }

    void as_blx(Register r, Condition c = Always)
    {
        writeBlob(((int) c) | op_blx | r.code());
    }
    void as_bl(BOffImm off, Condition c)
    {
        writeBlob(((int)c) | op_bl | off.toInt());
    }
    // bl can only branch+link to an immediate, never to a register
    // it never changes processor state
    void as_bl()
    {
        JS_NOT_REACHED("Feature NYI");
    }
    // bl #imm can have a condition code, blx #imm cannot.
    // blx reg can be conditional.
    void as_bl(Label *l, Condition c)
    {
        BufferOffset next = nextOffset();
        if (l->bound()) {
            as_bl(BufferOffset(l).diffB(next), c);
        } else {
            int32 old = l->use(next.getOffset());
            // See if the list was empty :(
            if (old == -1) {
                old = -4;
            }
            // This will fail if we couldn't actually
            // encode the offset of the branch.
            as_bl(BOffImm(old), c);
        }
    }
    void as_bl(BOffImm off, Condition c, BufferOffset inst)
    {
        *editSrc(inst) = ((int)c) | op_bl | off.toInt();
    }

    // VFP instructions!
    enum vfp_size {
        isDouble = 1 << 8,
        isSingle = 0 << 8
    };
    // Unityped variants: all registers hold the same (ieee754 single/double)
    // notably not included are vcvt; vmov vd, #imm; vmov rt, vn.
    void as_vfp_float(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                      VFPOp op, Condition c = Always)
    {
        // Make sure we believe that all of our operands are the same kind
        JS_ASSERT(vd.equiv(vn) && vd.equiv(vm));
        vfp_size sz = isDouble;
        if (!vd.isDouble()) {
            sz = isSingle;
        }
        writeBlob(VD(vd) | VN(vn) | VM(vm) | op | c | sz | 0x0e000a00);
    }

    void as_vadd(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c = Always)
    {
        as_vfp_float(vd, vn, vm, opv_add, c);
    }

    void as_vdiv(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c = Always)
    {
        as_vfp_float(vd, vn, vm, opv_mul, c);
    }

    void as_vmul(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c = Always)
    {
        as_vfp_float(vd, vn, vm, opv_mul, c);
    }

    void as_vnmul(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c = Always)
    {
        as_vfp_float(vd, vn, vm, opv_mul, c);
        JS_NOT_REACHED("Feature NYI");
    }

    void as_vnmla(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c = Always)
    {
        JS_NOT_REACHED("Feature NYI");
    }

    void as_vnmls(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c = Always)
    {
        JS_NOT_REACHED("Feature NYI");
    }

    void as_vneg(VFPRegister vd, VFPRegister vm, Condition c = Always)
    {
        as_vfp_float(vd, NoVFPRegister, vm, opv_neg, c);
    }

    void as_vsqrt(VFPRegister vd, VFPRegister vm, Condition c = Always)
    {
        as_vfp_float(vd, NoVFPRegister, vm, opv_sqrt, c);
    }

    void as_vabs(VFPRegister vd, VFPRegister vm, Condition c = Always)
    {
        as_vfp_float(vd, NoVFPRegister, vm, opv_abs, c);
    }

    void as_vsub(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c = Always)
    {
        as_vfp_float(vd, vn, vm, opv_sub, c);
    }

    void as_vcmp(VFPRegister vd, VFPRegister vm,
                 Condition c = Always)
    {
        as_vfp_float(vd, NoVFPRegister, vm, opv_sub, c);
    }

    // specifically, a move between two same sized-registers
    void as_vmov(VFPRegister vd, VFPRegister vsrc, Condition c = Always)
    {
        as_vfp_float(vd, NoVFPRegister, vsrc, opv_mov, c);
    }
    /*xfer between Core and VFP*/
    enum FloatToCore_ {
        FloatToCore = 1 << 20,
        CoreToFloat = 0 << 20
    };

    enum VFPXferSize {
        WordTransfer   = 0x0E000A10,
        DoubleTransfer = 0x0C400A10
    };

    // Unlike the next function, moving between the core registers and vfp
    // registers can't be *that* properly typed.  Namely, since I don't want to
    // munge the type VFPRegister to also include core registers.  Thus, the core
    // and vfp registers are passed in based on their type, and src/dest is
    // determined by the float2core.

    void as_vxfer(Register vt1, Register vt2, VFPRegister vm, FloatToCore_ f2c,
                  Condition c = Always)
    {
        vfp_size sz = isSingle;
        if (vm.isDouble()) {
            // Technically, this can be done with a vmov à la ARM ARM under vmov
            // however, that requires at least an extra bit saying if the
            // operation should be performed on the lower or upper half of the
            // double.  Moving a single to/from 2N/2N+1 isn't equivalent,
            // since there are 32 single registers, and 32 double registers
            // so there is no way to encode the last 16 double registers.
            JS_ASSERT(vt2 != InvalidReg);
            sz = isDouble;
        }
        VFPXferSize xfersz = WordTransfer;
        if (vt2 != InvalidReg) {
            // We are doing a 64 bit transfer.
            xfersz = DoubleTransfer;
        }
        writeBlob(xfersz | f2c | c | sz |
                 RT(vt1) | ((vt2 != InvalidReg) ? RN(vt2) : 0) | VM(vm));
    }

    // our encoding actually allows just the src and the dest (and theiyr types)
    // to uniquely specify the encoding that we are going to use.
    void as_vcvt(VFPRegister vd, VFPRegister vm,
                 Condition c = Always)
    {
        JS_NOT_REACHED("Feature NYI");
    }
    /* xfer between VFP and memory*/
    void as_vdtr(LoadStore ls, VFPRegister vd, VFPAddr addr,
                 Condition c = Always /* vfp doesn't have a wb option*/) {
        vfp_size sz = isDouble;
        if (!vd.isDouble()) {
            sz = isSingle;
        }

        writeBlob(0x0D000A00 | addr.toInt() | VD(vd) | sz | c);
    }

    // VFP's ldm/stm work differently from the standard arm ones.
    // You can only transfer a range

    void as_vdtm(LoadStore st, Register rn, VFPRegister vd, int length,
                 /*also has update conditions*/Condition c = Always)
    {
        JS_ASSERT(length <= 16 && length >= 0);
        vfp_size sz = isDouble;
        if (!vd.isDouble()) {
            sz = isSingle;
        } else {
            length *= 2;
        }
        writeBlob(dtmLoadStore | RN(rn) | VD(vd) |
                  length |
                  dtmMode | dtmUpdate | dtmCond |
                  0x0C000B00 | sz);
    }

    void as_vimm(VFPRegister vd, VFPImm imm, Condition c = Always)
    {
        vfp_size sz = isDouble;
        if (!vd.isDouble()) {
            // totally do not know how to handle this right now
            sz = isSingle;
            JS_NOT_REACHED("non-double immediate");
        }
        writeBlob(c | sz | imm.encode() | VD(vd) | 0x0EB00A00);

    }

    bool nextLink(BufferOffset b, BufferOffset *next)
    {
        uint32 branch = *editSrc(b);
        JS_ASSERT(((branch & op_b_mask) == op_b) ||
                  ((branch & op_b_mask) == op_bl));
        uint32 dest = (branch & op_b_dest_mask);
        // turns out the end marker is the same as the mask.
        if (dest == op_b_dest_mask)
            return false;
        // add in the extra 2 bits of padding that we chopped off when we made the b
        dest = dest << 2;
        // and let everyone know about it.
        new (next) BufferOffset(dest);
        return true;
    }

    void bind(Label *label) {
        //        JSC::MacroAssembler::Label jsclabel;
        if (label->used()) {
            bool more;
            BufferOffset dest = nextOffset();
            BufferOffset b(label);
            do {
                BufferOffset next;
                more = nextLink(b, &next);
                uint32 branch = *editSrc(b);
                Condition c = getCondition(branch);
                switch (branch & op_b_mask) {
                  case op_b:
                    as_b(dest.diffB(b), c, b);
                    break;
                  case op_bl:
                    as_bl(dest.diffB(b), c, b);
                    break;
                  default:
                    JS_NOT_REACHED("crazy fixup!");
                }
                b = next;
            } while (more);
        }
        label->bind(nextOffset().getOffset());
    }

    static void Bind(IonCode *code, AbsoluteLabel *label, const void *address) {
#if 0
        uint8 *raw = code->raw();
        if (label->used()) {
            intptr_t src = label->offset();
            do {
                intptr_t next = reinterpret_cast<intptr_t>(JSC::ARMAssembler::getPointer(raw + src));
                JSC::ARMAssembler::setPointer(raw + src, address);
                src = next;
            } while (src != AbsoluteLabel::INVALID_OFFSET);
        }
        JS_ASSERT(((uint8 *)address - raw) >= 0 && ((uint8 *)address - raw) < INT_MAX);
        label->bind();
#endif
        JS_NOT_REACHED("Feature NYI");
    }

    void call(Label *label) {
#if 0
        if (label->bound()) {
            masm.linkJump(masm.call(), JmpDst(label->offset()));
        } else {
            JmpSrc j = masm.call();
            JmpSrc prev = JmpSrc(label->use(j.offset()));
            masm.setNextJump(j, prev);
        }
#endif
        JS_NOT_REACHED("Feature NYI");
    }

    void as_bkpt() {
        writeBlob(0xe1200070);
    }

  public:
    static void TraceJumpRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);
    static void TraceDataRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);

    // The buffer is about to be linked, make sure any constant pools or excess
    // bookkeeping has been flushed to the instruction stream.
    void flush() { }

    // Copy the assembly code to the given buffer, and perform any pending
    // relocations relying on the target address.
    void executableCopy(uint8 *buffer);

    // Actual assembly emitting functions.
    // convert what to what now?
    void cvttsd2s(const FloatRegister &src, const Register &dest) {
    }

    void as_b(void *target, Relocation::Kind reloc) {
        JS_NOT_REACHED("feature NYI");
#if 0
        JmpSrc src = masm.branch();
        addPendingJump(src, target, reloc);
#endif
    }
    void as_b(Condition cond, void *target, Relocation::Kind reloc) {
        JS_NOT_REACHED("feature NYI");
#if 0
        JmpSrc src = masm.branch(cond);
        addPendingJump(src, target, reloc);
#endif
    }
#if 0
    void movsd(const double *dp, const FloatRegister &dest) {
    }
    void movsd(AbsoluteLabel *label, const FloatRegister &dest) {
        JS_ASSERT(!label->bound());
        // Thread the patch list through the unpatched address word in the
        // instruction stream.
    }
#endif
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
    }
    void transferFloatReg(VFPRegister rn)
    {
        if (dtmLastReg == -1) {
            vdtmFirstReg = rn;
        } else {
            JS_ASSERT(rn.code() == dtmLastReg + 1);
        }
        dtmLastReg = rn.code();
    }
    void finishFloatTransfer() {
        JS_ASSERT(dtmActive);
        dtmActive = false;
        JS_ASSERT(dtmLastReg != -1);
        // fencepost problem.
        int len = dtmLastReg - vdtmFirstReg.code() + 1;
        as_vdtm(dtmLoadStore, dtmBase, vdtmFirstReg, len, dtmCond);
    }
private:
    int dtmRegBitField;
    int dtmLastReg;
    Register dtmBase;
    VFPRegister vdtmFirstReg;
    DTMWriteBack dtmUpdate;
    DTMMode dtmMode;
    LoadStore dtmLoadStore;
    bool dtmActive;
    Condition dtmCond;
}; // Assembler.

static const uint32 NumArgRegs = 4;

static inline bool
GetArgReg(uint32 arg, Register *out)
{
    switch (arg) {
      case 0:
        *out = r0;
        return true;
      case 1:
        *out = r1;
        return true;
      case 2:
        *out = r2;
        return true;
      case 3:
        *out = r3;
        return true;
      default:
        return false;
    }
}

class DoubleEncoder {
    uint32 rep(bool b, uint32 count) {
        uint ret = 0;
        for (int i = 0; i < count; i++)
            ret = (ret << 1) | b;
        return ret;
    }
    uint32 encode(uint8 value) {
        //ARM ARM "VFP modified immediate constants"
        // aBbbbbbb bbcdefgh 000...
        // we want to return the top 32 bits of the double
        // the rest are 0.
        bool a = value >> 7;
        bool b = value >> 6 & 1;
        bool B = !b;
        uint32 cdefgh = value & 0x3f;
        return a << 31 |
            B << 30 |
            rep(b, 8) << 22 |
            cdefgh << 16;
    }
    struct DoubleEntry {
        uint32 dblTop;
        datastore::Imm8VFPImmData data;
        DoubleEntry(uint32 dblTop_, datastore::Imm8VFPImmData data_)
            : dblTop(dblTop_), data(data_) {}
        DoubleEntry() :dblTop(-1) {}
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
    static bool lookup(uint32 top, datastore::Imm8VFPImmData *ret) {
        for (int i = 0; i < 256; i++) {
            if (_this.table[i].dblTop == top) {
                *ret = _this.table[i].data;
                return true;
            }
        }
        return false;
    }
};

} // namespace ion
} // namespace js

#endif // jsion_cpu_arm_assembler_h__
