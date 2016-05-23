/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm_MacroAssembler_arm_inl_h
#define jit_arm_MacroAssembler_arm_inl_h

#include "jit/arm/MacroAssembler-arm.h"

namespace js {
namespace jit {

//{{{ check_macroassembler_style

void
MacroAssembler::move64(Register64 src, Register64 dest)
{
    move32(src.low, dest.low);
    move32(src.high, dest.high);
}

void
MacroAssembler::move64(Imm64 imm, Register64 dest)
{
    move32(Imm32(imm.value & 0xFFFFFFFFL), dest.low);
    move32(Imm32((imm.value >> 32) & 0xFFFFFFFFL), dest.high);
}

// ===============================================================
// Logical instructions

void
MacroAssembler::not32(Register reg)
{
    ma_mvn(reg, reg);
}

void
MacroAssembler::and32(Register src, Register dest)
{
    ma_and(src, dest, SetCC);
}

void
MacroAssembler::and32(Imm32 imm, Register dest)
{
    ma_and(imm, dest, SetCC);
}

void
MacroAssembler::and32(Imm32 imm, const Address& dest)
{
    ScratchRegisterScope scratch(*this);
    load32(dest, scratch);
    ma_and(imm, scratch);
    store32(scratch, dest);
}

void
MacroAssembler::and32(const Address& src, Register dest)
{
    ScratchRegisterScope scratch(*this);
    load32(src, scratch);
    ma_and(scratch, dest, SetCC);
}

void
MacroAssembler::andPtr(Register src, Register dest)
{
    ma_and(src, dest);
}

void
MacroAssembler::andPtr(Imm32 imm, Register dest)
{
    ma_and(imm, dest);
}

void
MacroAssembler::and64(Imm64 imm, Register64 dest)
{
    and32(Imm32(imm.value & 0xFFFFFFFFL), dest.low);
    and32(Imm32((imm.value >> 32) & 0xFFFFFFFFL), dest.high);
}

void
MacroAssembler::or64(Imm64 imm, Register64 dest)
{
    or32(Imm32(imm.value & 0xFFFFFFFFL), dest.low);
    or32(Imm32((imm.value >> 32) & 0xFFFFFFFFL), dest.high);
}

void
MacroAssembler::xor64(Imm64 imm, Register64 dest)
{
    xor32(Imm32(imm.value & 0xFFFFFFFFL), dest.low);
    xor32(Imm32((imm.value >> 32) & 0xFFFFFFFFL), dest.high);
}

void
MacroAssembler::or32(Register src, Register dest)
{
    ma_orr(src, dest);
}

void
MacroAssembler::or32(Imm32 imm, Register dest)
{
    ma_orr(imm, dest);
}

void
MacroAssembler::or32(Imm32 imm, const Address& dest)
{
    ScratchRegisterScope scratch(*this);
    load32(dest, scratch);
    ma_orr(imm, scratch);
    store32(scratch, dest);
}

void
MacroAssembler::orPtr(Register src, Register dest)
{
    ma_orr(src, dest);
}

void
MacroAssembler::orPtr(Imm32 imm, Register dest)
{
    ma_orr(imm, dest);
}

void
MacroAssembler::or64(Register64 src, Register64 dest)
{
    or32(src.low, dest.low);
    or32(src.high, dest.high);
}

void
MacroAssembler::xor64(Register64 src, Register64 dest)
{
    ma_eor(src.low, dest.low);
    ma_eor(src.high, dest.high);
}

void
MacroAssembler::xor32(Imm32 imm, Register dest)
{
    ma_eor(imm, dest, SetCC);
}

void
MacroAssembler::xorPtr(Register src, Register dest)
{
    ma_eor(src, dest);
}

void
MacroAssembler::xorPtr(Imm32 imm, Register dest)
{
    ma_eor(imm, dest);
}

// ===============================================================
// Arithmetic functions

void
MacroAssembler::add32(Register src, Register dest)
{
    ma_add(src, dest, SetCC);
}

void
MacroAssembler::add32(Imm32 imm, Register dest)
{
    ma_add(imm, dest, SetCC);
}

void
MacroAssembler::add32(Imm32 imm, const Address& dest)
{
    ScratchRegisterScope scratch(*this);
    load32(dest, scratch);
    ma_add(imm, scratch, SetCC);
    store32(scratch, dest);
}

void
MacroAssembler::addPtr(Register src, Register dest)
{
    ma_add(src, dest);
}

void
MacroAssembler::addPtr(Imm32 imm, Register dest)
{
    ma_add(imm, dest);
}

void
MacroAssembler::addPtr(ImmWord imm, Register dest)
{
    addPtr(Imm32(imm.value), dest);
}

void
MacroAssembler::addPtr(Imm32 imm, const Address& dest)
{
    ScratchRegisterScope scratch(*this);
    loadPtr(dest, scratch);
    addPtr(imm, scratch);
    storePtr(scratch, dest);
}

void
MacroAssembler::addPtr(const Address& src, Register dest)
{
    ScratchRegisterScope scratch(*this);
    load32(src, scratch);
    ma_add(scratch, dest, SetCC);
}

void
MacroAssembler::add64(Register64 src, Register64 dest)
{
    ma_add(src.low, dest.low, SetCC);
    ma_adc(src.high, dest.high);
}

void
MacroAssembler::add64(Imm32 imm, Register64 dest)
{
    ma_add(imm, dest.low, SetCC);
    ma_adc(Imm32(0), dest.high, LeaveCC);
}

void
MacroAssembler::addDouble(FloatRegister src, FloatRegister dest)
{
    ma_vadd(dest, src, dest);
}

void
MacroAssembler::sub32(Register src, Register dest)
{
    ma_sub(src, dest, SetCC);
}

void
MacroAssembler::sub32(Imm32 imm, Register dest)
{
    ma_sub(imm, dest, SetCC);
}

void
MacroAssembler::sub32(const Address& src, Register dest)
{
    ScratchRegisterScope scratch(*this);
    load32(src, scratch);
    ma_sub(scratch, dest, SetCC);
}

void
MacroAssembler::subPtr(Register src, Register dest)
{
    ma_sub(src, dest);
}

void
MacroAssembler::subPtr(Register src, const Address& dest)
{
    ScratchRegisterScope scratch(*this);
    loadPtr(dest, scratch);
    ma_sub(src, scratch);
    storePtr(scratch, dest);
}

void
MacroAssembler::subPtr(Imm32 imm, Register dest)
{
    ma_sub(imm, dest);
}

void
MacroAssembler::subPtr(const Address& addr, Register dest)
{
    ScratchRegisterScope scratch(*this);
    loadPtr(addr, scratch);
    ma_sub(scratch, dest);
}

void
MacroAssembler::subDouble(FloatRegister src, FloatRegister dest)
{
    ma_vsub(dest, src, dest);
}

void
MacroAssembler::mul64(Imm64 imm, const Register64& dest)
{
    // LOW32  = LOW(LOW(dest) * LOW(imm));
    // HIGH32 = LOW(HIGH(dest) * LOW(imm)) [multiply imm into upper bits]
    //        + LOW(LOW(dest) * HIGH(imm)) [multiply dest into upper bits]
    //        + HIGH(LOW(dest) * LOW(imm)) [carry]

    // HIGH(dest) = LOW(HIGH(dest) * LOW(imm));
    ma_mov(Imm32(imm.value & 0xFFFFFFFFL), ScratchRegister);
    as_mul(dest.high, dest.high, ScratchRegister);

    // high:low = LOW(dest) * LOW(imm);
    as_umull(secondScratchReg_, ScratchRegister, dest.low, ScratchRegister);

    // HIGH(dest) += high;
    as_add(dest.high, dest.high, O2Reg(secondScratchReg_));

    // HIGH(dest) += LOW(LOW(dest) * HIGH(imm));
    if (((imm.value >> 32) & 0xFFFFFFFFL) == 5)
        as_add(secondScratchReg_, dest.low, lsl(dest.low, 2));
    else
        MOZ_CRASH("Not supported imm");
    as_add(dest.high, dest.high, O2Reg(secondScratchReg_));

    // LOW(dest) = low;
    ma_mov(ScratchRegister, dest.low);
}

void
MacroAssembler::mulBy3(Register src, Register dest)
{
    as_add(dest, src, lsl(src, 1));
}

void
MacroAssembler::mulDouble(FloatRegister src, FloatRegister dest)
{
    ma_vmul(dest, src, dest);
}

void
MacroAssembler::mulDoublePtr(ImmPtr imm, Register temp, FloatRegister dest)
{
    movePtr(imm, ScratchRegister);
    loadDouble(Address(ScratchRegister, 0), ScratchDoubleReg);
    mulDouble(ScratchDoubleReg, dest);
}

void
MacroAssembler::divDouble(FloatRegister src, FloatRegister dest)
{
    ma_vdiv(dest, src, dest);
}

void
MacroAssembler::inc64(AbsoluteAddress dest)
{
    ScratchRegisterScope scratch(*this);

    ma_strd(r0, r1, EDtrAddr(sp, EDtrOffImm(-8)), PreIndex);

    ma_mov(Imm32((int32_t)dest.addr), scratch);
    ma_ldrd(EDtrAddr(scratch, EDtrOffImm(0)), r0, r1);

    ma_add(Imm32(1), r0, SetCC);
    ma_adc(Imm32(0), r1, LeaveCC);

    ma_strd(r0, r1, EDtrAddr(scratch, EDtrOffImm(0)));
    ma_ldrd(EDtrAddr(sp, EDtrOffImm(8)), r0, r1, PostIndex);
}

void
MacroAssembler::neg32(Register reg)
{
    ma_neg(reg, reg, SetCC);
}

void
MacroAssembler::negateDouble(FloatRegister reg)
{
    ma_vneg(reg, reg);
}

// ===============================================================
// Shift functions

void
MacroAssembler::lshiftPtr(Imm32 imm, Register dest)
{
    ma_lsl(imm, dest, dest);
}

void
MacroAssembler::lshift64(Imm32 imm, Register64 dest)
{
    as_mov(dest.high, lsl(dest.high, imm.value));
    as_orr(dest.high, dest.high, lsr(dest.low, 32 - imm.value));
    as_mov(dest.low, lsl(dest.low, imm.value));
}

void
MacroAssembler::rshiftPtr(Imm32 imm, Register dest)
{
    ma_lsr(imm, dest, dest);
}

void
MacroAssembler::rshiftPtrArithmetic(Imm32 imm, Register dest)
{
    ma_asr(imm, dest, dest);
}

void
MacroAssembler::rshift64(Imm32 imm, Register64 dest)
{
    as_mov(dest.low, lsr(dest.low, imm.value));
    as_orr(dest.low, dest.low, lsl(dest.high, 32 - imm.value));
    as_mov(dest.high, lsr(dest.high, imm.value));
}

// ===============================================================
// Rotate functions
void
MacroAssembler::rotateLeft(Imm32 count, Register input, Register dest)
{
    if (count.value)
        ma_rol(count, input, dest);
    else
        ma_mov(input, dest);
}

void
MacroAssembler::rotateLeft(Register count, Register input, Register dest)
{
    ma_rol(count, input, dest);
}

void
MacroAssembler::rotateRight(Imm32 count, Register input, Register dest)
{
    if (count.value)
        ma_ror(count, input, dest);
    else
        ma_mov(input, dest);
}

void
MacroAssembler::rotateRight(Register count, Register input, Register dest)
{
    ma_ror(count, input, dest);
}

// ===============================================================
// Branch functions

void
MacroAssembler::branch32(Condition cond, Register lhs, Register rhs, Label* label)
{
    ma_cmp(lhs, rhs);
    ma_b(label, cond);
}

template <class L>
void
MacroAssembler::branch32(Condition cond, Register lhs, Imm32 rhs, L label)
{
    ma_cmp(lhs, rhs);
    ma_b(label, cond);
}

void
MacroAssembler::branch32(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    ScratchRegisterScope scratch(*this);
    load32(lhs, scratch);
    branch32(cond, scratch, rhs, label);
}

void
MacroAssembler::branch32(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    // branch32 will use ScratchRegister.
    AutoRegisterScope scratch(*this, secondScratchReg_);
    load32(lhs, scratch);
    branch32(cond, scratch, rhs, label);
}

void
MacroAssembler::branch32(Condition cond, const AbsoluteAddress& lhs, Register rhs, Label* label)
{
    AutoRegisterScope scratch2(*this, secondScratchReg_);
    loadPtr(lhs, scratch2); // ma_cmp will use the scratch register.
    ma_cmp(scratch2, rhs);
    ma_b(label, cond);
}

void
MacroAssembler::branch32(Condition cond, const AbsoluteAddress& lhs, Imm32 rhs, Label* label)
{
    AutoRegisterScope scratch2(*this, secondScratchReg_);
    loadPtr(lhs, scratch2); // ma_cmp will use the scratch register.
    ma_cmp(scratch2, rhs);
    ma_b(label, cond);
}

void
MacroAssembler::branch32(Condition cond, const BaseIndex& lhs, Imm32 rhs, Label* label)
{
    // branch32 will use ScratchRegister.
    AutoRegisterScope scratch2(*this, secondScratchReg_);
    load32(lhs, scratch2);
    branch32(cond, scratch2, rhs, label);
}

void
MacroAssembler::branch32(Condition cond, wasm::SymbolicAddress lhs, Imm32 rhs, Label* label)
{
    ScratchRegisterScope scratch(*this);
    loadPtr(lhs, scratch);
    branch32(cond, scratch, rhs, label);
}

void
MacroAssembler::branch64(Condition cond, const Address& lhs, Imm64 val, Label* label)
{
    MOZ_ASSERT(cond == Assembler::NotEqual,
               "other condition codes not supported");

    branch32(cond, lhs, val.firstHalf(), label);
    branch32(cond, Address(lhs.base, lhs.offset + sizeof(uint32_t)), val.secondHalf(), label);
}

void
MacroAssembler::branch64(Condition cond, const Address& lhs, const Address& rhs, Register scratch,
                         Label* label)
{
    MOZ_ASSERT(cond == Assembler::NotEqual,
               "other condition codes not supported");
    MOZ_ASSERT(lhs.base != scratch);
    MOZ_ASSERT(rhs.base != scratch);

    load32(rhs, scratch);
    branch32(cond, lhs, scratch, label);

    load32(Address(rhs.base, rhs.offset + sizeof(uint32_t)), scratch);
    branch32(cond, Address(lhs.base, lhs.offset + sizeof(uint32_t)), scratch, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, Register rhs, Label* label)
{
    branch32(cond, lhs, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    branch32(cond, lhs, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmPtr rhs, Label* label)
{
    branchPtr(cond, lhs, ImmWord(uintptr_t(rhs.value)), label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmGCPtr rhs, Label* label)
{
    ScratchRegisterScope scratch(*this);
    movePtr(rhs, scratch);
    branchPtr(cond, lhs, scratch, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmWord rhs, Label* label)
{
    branch32(cond, lhs, Imm32(rhs.value), label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    branch32(cond, lhs, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmPtr rhs, Label* label)
{
    branchPtr(cond, lhs, ImmWord(uintptr_t(rhs.value)), label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmGCPtr rhs, Label* label)
{
    AutoRegisterScope scratch2(*this, secondScratchReg_);
    loadPtr(lhs, scratch2);
    branchPtr(cond, scratch2, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmWord rhs, Label* label)
{
    AutoRegisterScope scratch2(*this, secondScratchReg_);
    loadPtr(lhs, scratch2);
    branchPtr(cond, scratch2, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const AbsoluteAddress& lhs, Register rhs, Label* label)
{
    ScratchRegisterScope scratch(*this);
    loadPtr(lhs, scratch);
    branchPtr(cond, scratch, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const AbsoluteAddress& lhs, ImmWord rhs, Label* label)
{
    ScratchRegisterScope scratch(*this);
    loadPtr(lhs, scratch);
    branchPtr(cond, scratch, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, wasm::SymbolicAddress lhs, Register rhs, Label* label)
{
    ScratchRegisterScope scratch(*this);
    loadPtr(lhs, scratch);
    branchPtr(cond, scratch, rhs, label);
}

template <typename T>
CodeOffsetJump
MacroAssembler::branchPtrWithPatch(Condition cond, Register lhs, T rhs, RepatchLabel* label)
{
    ma_cmp(lhs, rhs);
    return jumpWithPatch(label, cond);
}

template <typename T>
CodeOffsetJump
MacroAssembler::branchPtrWithPatch(Condition cond, Address lhs, T rhs, RepatchLabel* label)
{
    AutoRegisterScope scratch2(*this, secondScratchReg_);
    ma_ldr(lhs, scratch2);
    ma_cmp(scratch2, rhs);
    return jumpWithPatch(label, cond);
}

void
MacroAssembler::branchPrivatePtr(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    branchPtr(cond, lhs, rhs, label);
}

void
MacroAssembler::branchFloat(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                            Label* label)
{
    compareFloat(lhs, rhs);

    if (cond == DoubleNotEqual) {
        // Force the unordered cases not to jump.
        Label unordered;
        ma_b(&unordered, VFP_Unordered);
        ma_b(label, VFP_NotEqualOrUnordered);
        bind(&unordered);
        return;
    }

    if (cond == DoubleEqualOrUnordered) {
        ma_b(label, VFP_Unordered);
        ma_b(label, VFP_Equal);
        return;
    }

    ma_b(label, ConditionFromDoubleCondition(cond));
}

void
MacroAssembler::branchTruncateFloat32(FloatRegister src, Register dest, Label* fail)
{
    ScratchFloat32Scope scratch(*this);
    ma_vcvt_F32_I32(src, scratch.sintOverlay());
    ma_vxfer(scratch, dest);
    ma_cmp(dest, Imm32(0x7fffffff));
    ma_cmp(dest, Imm32(0x80000000), Assembler::NotEqual);
    ma_b(fail, Assembler::Equal);
}

void
MacroAssembler::branchDouble(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                             Label* label)
{
    compareDouble(lhs, rhs);

    if (cond == DoubleNotEqual) {
        // Force the unordered cases not to jump.
        Label unordered;
        ma_b(&unordered, VFP_Unordered);
        ma_b(label, VFP_NotEqualOrUnordered);
        bind(&unordered);
        return;
    }

    if (cond == DoubleEqualOrUnordered) {
        ma_b(label, VFP_Unordered);
        ma_b(label, VFP_Equal);
        return;
    }

    ma_b(label, ConditionFromDoubleCondition(cond));
}

// There are two options for implementing branchTruncateDouble:
//
// 1. Convert the floating point value to an integer, if it did not fit, then it
// was clamped to INT_MIN/INT_MAX, and we can test it. NOTE: if the value
// really was supposed to be INT_MAX / INT_MIN then it will be wrong.
//
// 2. Convert the floating point value to an integer, if it did not fit, then it
// set one or two bits in the fpcsr. Check those.
void
MacroAssembler::branchTruncateDouble(FloatRegister src, Register dest, Label* fail)
{
    ScratchDoubleScope scratch(*this);
    FloatRegister scratchSIntReg = scratch.sintOverlay();

    ma_vcvt_F64_I32(src, scratchSIntReg);
    ma_vxfer(scratchSIntReg, dest);
    ma_cmp(dest, Imm32(0x7fffffff));
    ma_cmp(dest, Imm32(0x80000000), Assembler::NotEqual);
    ma_b(fail, Assembler::Equal);
}

template <typename T>
void
MacroAssembler::branchAdd32(Condition cond, T src, Register dest, Label* label)
{
    add32(src, dest);
    j(cond, label);
}

template <typename T>
void
MacroAssembler::branchSub32(Condition cond, T src, Register dest, Label* label)
{
    ma_sub(src, dest, SetCC);
    j(cond, label);
}

void
MacroAssembler::decBranchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    ma_sub(rhs, lhs, SetCC);
    as_b(label, cond);
}

template <class L>
void
MacroAssembler::branchTest32(Condition cond, Register lhs, Register rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    // x86 likes test foo, foo rather than cmp foo, #0.
    // Convert the former into the latter.
    if (lhs == rhs && (cond == Zero || cond == NonZero))
        ma_cmp(lhs, Imm32(0));
    else
        ma_tst(lhs, rhs);
    ma_b(label, cond);
}

template <class L>
void
MacroAssembler::branchTest32(Condition cond, Register lhs, Imm32 rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    ma_tst(lhs, rhs);
    ma_b(label, cond);
}

void
MacroAssembler::branchTest32(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    // branchTest32 will use ScratchRegister.
    AutoRegisterScope scratch2(*this, secondScratchReg_);
    load32(lhs, scratch2);
    branchTest32(cond, scratch2, rhs, label);
}

void
MacroAssembler::branchTest32(Condition cond, const AbsoluteAddress& lhs, Imm32 rhs, Label* label)
{
    // branchTest32 will use ScratchRegister.
    AutoRegisterScope scratch2(*this, secondScratchReg_);
    load32(lhs, scratch2);
    branchTest32(cond, scratch2, rhs, label);
}

void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Register rhs, Label* label)
{
    branchTest32(cond, lhs, rhs, label);
}

void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    branchTest32(cond, lhs, rhs, label);
}

void
MacroAssembler::branchTestPtr(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    branchTest32(cond, lhs, rhs, label);
}

void
MacroAssembler::branchTest64(Condition cond, Register64 lhs, Register64 rhs, Register temp,
                             Label* label)
{
    if (cond == Assembler::Zero) {
        MOZ_ASSERT(lhs.low == rhs.low);
        MOZ_ASSERT(lhs.high == rhs.high);
        ma_orr(lhs.low, lhs.high, ScratchRegister);
        branchTestPtr(cond, ScratchRegister, ScratchRegister, label);
    } else {
        MOZ_CRASH("Unsupported condition");
    }
}

void
MacroAssembler::branchTestUndefined(Condition cond, Register tag, Label* label)
{
    branchTestUndefinedImpl(cond, tag, label);
}

void
MacroAssembler::branchTestUndefined(Condition cond, const Address& address, Label* label)
{
    branchTestUndefinedImpl(cond, address, label);
}

void
MacroAssembler::branchTestUndefined(Condition cond, const BaseIndex& address, Label* label)
{
    branchTestUndefinedImpl(cond, address, label);
}

void
MacroAssembler::branchTestUndefined(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestUndefinedImpl(cond, value, label);
}

template <typename T>
void
MacroAssembler::branchTestUndefinedImpl(Condition cond, const T& t, Label* label)
{
    Condition c = testUndefined(cond, t);
    ma_b(label, c);
}

void
MacroAssembler::branchTestInt32(Condition cond, Register tag, Label* label)
{
    branchTestInt32Impl(cond, tag, label);
}

void
MacroAssembler::branchTestInt32(Condition cond, const Address& address, Label* label)
{
    branchTestInt32Impl(cond, address, label);
}

void
MacroAssembler::branchTestInt32(Condition cond, const BaseIndex& address, Label* label)
{
    branchTestInt32Impl(cond, address, label);
}

void
MacroAssembler::branchTestInt32(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestInt32Impl(cond, value, label);
}

template <typename T>
void
MacroAssembler::branchTestInt32Impl(Condition cond, const T& t, Label* label)
{
    Condition c = testInt32(cond, t);
    ma_b(label, c);
}

void
MacroAssembler::branchTestInt32Truthy(bool truthy, const ValueOperand& value, Label* label)
{
    Condition c = testInt32Truthy(truthy, value);
    ma_b(label, c);
}

void
MacroAssembler::branchTestDouble(Condition cond, Register tag, Label* label)
{
    branchTestDoubleImpl(cond, tag, label);
}

void
MacroAssembler::branchTestDouble(Condition cond, const Address& address, Label* label)
{
    branchTestDoubleImpl(cond, address, label);
}

void
MacroAssembler::branchTestDouble(Condition cond, const BaseIndex& address, Label* label)
{
    branchTestDoubleImpl(cond, address, label);
}

void
MacroAssembler::branchTestDouble(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestDoubleImpl(cond, value, label);
}

template <typename T>
void
MacroAssembler::branchTestDoubleImpl(Condition cond, const T& t, Label* label)
{
    Condition c = testDouble(cond, t);
    ma_b(label, c);
}

void
MacroAssembler::branchTestDoubleTruthy(bool truthy, FloatRegister reg, Label* label)
{
    Condition c = testDoubleTruthy(truthy, reg);
    ma_b(label, c);
}

void
MacroAssembler::branchTestNumber(Condition cond, Register tag, Label* label)
{
    branchTestNumberImpl(cond, tag, label);
}

void
MacroAssembler::branchTestNumber(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestNumberImpl(cond, value, label);
}

template <typename T>
void
MacroAssembler::branchTestNumberImpl(Condition cond, const T& t, Label* label)
{
    cond = testNumber(cond, t);
    ma_b(label, cond);
}

void
MacroAssembler::branchTestBoolean(Condition cond, Register tag, Label* label)
{
    branchTestBooleanImpl(cond, tag, label);
}

void
MacroAssembler::branchTestBoolean(Condition cond, const Address& address, Label* label)
{
    branchTestBooleanImpl(cond, address, label);
}

void
MacroAssembler::branchTestBoolean(Condition cond, const BaseIndex& address, Label* label)
{
    branchTestBooleanImpl(cond, address, label);
}

void
MacroAssembler::branchTestBoolean(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestBooleanImpl(cond, value, label);
}

template <typename T>
void
MacroAssembler::branchTestBooleanImpl(Condition cond, const T& t, Label* label)
{
    Condition c = testBoolean(cond, t);
    ma_b(label, c);
}

void
MacroAssembler::branchTestBooleanTruthy(bool truthy, const ValueOperand& value, Label* label)
{
    Condition c = testBooleanTruthy(truthy, value);
    ma_b(label, c);
}

void
MacroAssembler::branchTestString(Condition cond, Register tag, Label* label)
{
    branchTestStringImpl(cond, tag, label);
}

void
MacroAssembler::branchTestString(Condition cond, const BaseIndex& address, Label* label)
{
    branchTestStringImpl(cond, address, label);
}

void
MacroAssembler::branchTestString(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestStringImpl(cond, value, label);
}

template <typename T>
void
MacroAssembler::branchTestStringImpl(Condition cond, const T& t, Label* label)
{
    Condition c = testString(cond, t);
    ma_b(label, c);
}

void
MacroAssembler::branchTestStringTruthy(bool truthy, const ValueOperand& value, Label* label)
{
    Condition c = testStringTruthy(truthy, value);
    ma_b(label, c);
}

void
MacroAssembler::branchTestSymbol(Condition cond, Register tag, Label* label)
{
    branchTestSymbolImpl(cond, tag, label);
}

void
MacroAssembler::branchTestSymbol(Condition cond, const BaseIndex& address, Label* label)
{
    branchTestSymbolImpl(cond, address, label);
}

void
MacroAssembler::branchTestSymbol(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestSymbolImpl(cond, value, label);
}

template <typename T>
void
MacroAssembler::branchTestSymbolImpl(Condition cond, const T& t, Label* label)
{
    Condition c = testSymbol(cond, t);
    ma_b(label, c);
}

void
MacroAssembler::branchTestNull(Condition cond, Register tag, Label* label)
{
    branchTestNullImpl(cond, tag, label);
}

void
MacroAssembler::branchTestNull(Condition cond, const Address& address, Label* label)
{
    branchTestNullImpl(cond, address, label);
}

void
MacroAssembler::branchTestNull(Condition cond, const BaseIndex& address, Label* label)
{
    branchTestNullImpl(cond, address, label);
}

void
MacroAssembler::branchTestNull(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestNullImpl(cond, value, label);
}

template <typename T>
void
MacroAssembler::branchTestNullImpl(Condition cond, const T& t, Label* label)
{
    Condition c = testNull(cond, t);
    ma_b(label, c);
}

void
MacroAssembler::branchTestObject(Condition cond, Register tag, Label* label)
{
    branchTestObjectImpl(cond, tag, label);
}

void
MacroAssembler::branchTestObject(Condition cond, const Address& address, Label* label)
{
    branchTestObjectImpl(cond, address, label);
}

void
MacroAssembler::branchTestObject(Condition cond, const BaseIndex& address, Label* label)
{
    branchTestObjectImpl(cond, address, label);
}

void
MacroAssembler::branchTestObject(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestObjectImpl(cond, value, label);
}

template <typename T>
void
MacroAssembler::branchTestObjectImpl(Condition cond, const T& t, Label* label)
{
    Condition c = testObject(cond, t);
    ma_b(label, c);
}

void
MacroAssembler::branchTestGCThing(Condition cond, const Address& address, Label* label)
{
    branchTestGCThingImpl(cond, address, label);
}

void
MacroAssembler::branchTestGCThing(Condition cond, const BaseIndex& address, Label* label)
{
    branchTestGCThingImpl(cond, address, label);
}

template <typename T>
void
MacroAssembler::branchTestGCThingImpl(Condition cond, const T& t, Label* label)
{
    Condition c = testGCThing(cond, t);
    ma_b(label, c);
}

void
MacroAssembler::branchTestPrimitive(Condition cond, Register tag, Label* label)
{
    branchTestPrimitiveImpl(cond, tag, label);
}

void
MacroAssembler::branchTestPrimitive(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestPrimitiveImpl(cond, value, label);
}

template <typename T>
void
MacroAssembler::branchTestPrimitiveImpl(Condition cond, const T& t, Label* label)
{
    Condition c = testPrimitive(cond, t);
    ma_b(label, c);
}

void
MacroAssembler::branchTestMagic(Condition cond, Register tag, Label* label)
{
    branchTestMagicImpl(cond, tag, label);
}

void
MacroAssembler::branchTestMagic(Condition cond, const Address& address, Label* label)
{
    branchTestMagicImpl(cond, address, label);
}

void
MacroAssembler::branchTestMagic(Condition cond, const BaseIndex& address, Label* label)
{
    branchTestMagicImpl(cond, address, label);
}

template <class L>
void
MacroAssembler::branchTestMagic(Condition cond, const ValueOperand& value, L label)
{
    branchTestMagicImpl(cond, value, label);
}

template <typename T, class L>
void
MacroAssembler::branchTestMagicImpl(Condition cond, const T& t, L label)
{
    cond = testMagic(cond, t);
    ma_b(label, cond);
}

void
MacroAssembler::branchTestMagic(Condition cond, const Address& valaddr, JSWhyMagic why, Label* label)
{
    branchTestMagic(cond, valaddr, label);
    branch32(cond, ToPayload(valaddr), Imm32(why), label);
}

// ========================================================================
// Memory access primitives.
void
MacroAssembler::storeUncanonicalizedDouble(FloatRegister src, const Address& addr)
{
    ma_vstr(src, addr);
}
void
MacroAssembler::storeUncanonicalizedDouble(FloatRegister src, const BaseIndex& addr)
{
    uint32_t scale = Imm32::ShiftOf(addr.scale).value;
    ma_vstr(src, addr.base, addr.index, scale, addr.offset);
}

void
MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src, const Address& addr)
{
    ma_vstr(src.asSingle(), addr);
}
void
MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src, const BaseIndex& addr)
{
    uint32_t scale = Imm32::ShiftOf(addr.scale).value;
    ma_vstr(src.asSingle(), addr.base, addr.index, scale, addr.offset);
}

void
MacroAssembler::storeFloat32x3(FloatRegister src, const Address& dest)
{
    MOZ_CRASH("NYI");
}
void
MacroAssembler::storeFloat32x3(FloatRegister src, const BaseIndex& dest)
{
    MOZ_CRASH("NYI");
}

//}}} check_macroassembler_style
// ===============================================================

void
MacroAssemblerARMCompat::incrementInt32Value(const Address& addr)
{
    asMasm().add32(Imm32(1), ToPayload(addr));
}

} // namespace jit
} // namespace js

#endif /* jit_arm_MacroAssembler_arm_inl_h */
