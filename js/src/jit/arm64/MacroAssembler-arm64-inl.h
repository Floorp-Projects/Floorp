/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm64_MacroAssembler_arm64_inl_h
#define jit_arm64_MacroAssembler_arm64_inl_h

#include "jit/arm64/MacroAssembler-arm64.h"

namespace js {
namespace jit {

//{{{ check_macroassembler_style
// ===============================================================
// Logical instructions

void
MacroAssembler::not32(Register reg)
{
    Orn(ARMRegister(reg, 32), vixl::wzr, ARMRegister(reg, 32));
}

void
MacroAssembler::and32(Register src, Register dest)
{
    And(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(ARMRegister(src, 32)));
}

void
MacroAssembler::and32(Imm32 imm, Register dest)
{
    And(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(imm.value));
}

void
MacroAssembler::and32(Imm32 imm, Register src, Register dest)
{
    And(ARMRegister(dest, 32), ARMRegister(src, 32), Operand(imm.value));
}

void
MacroAssembler::and32(Imm32 imm, const Address& dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch32 = temps.AcquireW();
    MOZ_ASSERT(scratch32.asUnsized() != dest.base);
    load32(dest, scratch32.asUnsized());
    And(scratch32, scratch32, Operand(imm.value));
    store32(scratch32.asUnsized(), dest);
}

void
MacroAssembler::and32(const Address& src, Register dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch32 = temps.AcquireW();
    MOZ_ASSERT(scratch32.asUnsized() != src.base);
    load32(src, scratch32.asUnsized());
    And(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(scratch32));
}

void
MacroAssembler::andPtr(Register src, Register dest)
{
    And(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(ARMRegister(src, 64)));
}

void
MacroAssembler::andPtr(Imm32 imm, Register dest)
{
    And(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(imm.value));
}

void
MacroAssembler::and64(Imm64 imm, Register64 dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    mov(ImmWord(imm.value), scratch);
    andPtr(scratch, dest.reg);
}

void
MacroAssembler::or32(Imm32 imm, Register dest)
{
    Orr(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(imm.value));
}

void
MacroAssembler::or32(Register src, Register dest)
{
    Orr(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(ARMRegister(src, 32)));
}

void
MacroAssembler::or32(Imm32 imm, const Address& dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch32 = temps.AcquireW();
    MOZ_ASSERT(scratch32.asUnsized() != dest.base);
    load32(dest, scratch32.asUnsized());
    Orr(scratch32, scratch32, Operand(imm.value));
    store32(scratch32.asUnsized(), dest);
}

void
MacroAssembler::orPtr(Register src, Register dest)
{
    Orr(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(ARMRegister(src, 64)));
}

void
MacroAssembler::orPtr(Imm32 imm, Register dest)
{
    Orr(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(imm.value));
}

void
MacroAssembler::or64(Register64 src, Register64 dest)
{
    orPtr(src.reg, dest.reg);
}

void
MacroAssembler::xor64(Register64 src, Register64 dest)
{
    xorPtr(src.reg, dest.reg);
}

void
MacroAssembler::xor32(Imm32 imm, Register dest)
{
    Eor(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(imm.value));
}

void
MacroAssembler::xorPtr(Register src, Register dest)
{
    Eor(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(ARMRegister(src, 64)));
}

void
MacroAssembler::xorPtr(Imm32 imm, Register dest)
{
    Eor(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(imm.value));
}

// ===============================================================
// Arithmetic functions

void
MacroAssembler::add32(Register src, Register dest)
{
    Add(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(ARMRegister(src, 32)));
}

void
MacroAssembler::add32(Imm32 imm, Register dest)
{
    Add(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(imm.value));
}

void
MacroAssembler::add32(Imm32 imm, const Address& dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch32 = temps.AcquireW();
    MOZ_ASSERT(scratch32.asUnsized() != dest.base);

    Ldr(scratch32, MemOperand(ARMRegister(dest.base, 64), dest.offset));
    Add(scratch32, scratch32, Operand(imm.value));
    Str(scratch32, MemOperand(ARMRegister(dest.base, 64), dest.offset));
}

void
MacroAssembler::addPtr(Register src, Register dest)
{
    addPtr(src, dest, dest);
}

void
MacroAssembler::addPtr(Register src1, Register src2, Register dest)
{
    Add(ARMRegister(dest, 64), ARMRegister(src1, 64), Operand(ARMRegister(src2, 64)));
}

void
MacroAssembler::addPtr(Imm32 imm, Register dest)
{
    addPtr(imm, dest, dest);
}

void
MacroAssembler::addPtr(Imm32 imm, Register src, Register dest)
{
    Add(ARMRegister(dest, 64), ARMRegister(src, 64), Operand(imm.value));
}

void
MacroAssembler::addPtr(ImmWord imm, Register dest)
{
    Add(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(imm.value));
}

void
MacroAssembler::addPtr(Imm32 imm, const Address& dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch64 = temps.AcquireX();
    MOZ_ASSERT(scratch64.asUnsized() != dest.base);

    Ldr(scratch64, MemOperand(ARMRegister(dest.base, 64), dest.offset));
    Add(scratch64, scratch64, Operand(imm.value));
    Str(scratch64, MemOperand(ARMRegister(dest.base, 64), dest.offset));
}

void
MacroAssembler::addPtr(const Address& src, Register dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch64 = temps.AcquireX();
    MOZ_ASSERT(scratch64.asUnsized() != src.base);

    Ldr(scratch64, MemOperand(ARMRegister(src.base, 64), src.offset));
    Add(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(scratch64));
}

void
MacroAssembler::add64(Register64 src, Register64 dest)
{
    addPtr(src.reg, dest.reg);
}

void
MacroAssembler::add64(Imm32 imm, Register64 dest)
{
    Add(ARMRegister(dest.reg, 64), ARMRegister(dest.reg, 64), Operand(imm.value));
}

void
MacroAssembler::addDouble(FloatRegister src, FloatRegister dest)
{
    fadd(ARMFPRegister(dest, 64), ARMFPRegister(dest, 64), ARMFPRegister(src, 64));
}

void
MacroAssembler::sub32(Imm32 imm, Register dest)
{
    Sub(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(imm.value));
}

void
MacroAssembler::sub32(Register src, Register dest)
{
    Sub(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(ARMRegister(src, 32)));
}

void
MacroAssembler::sub32(const Address& src, Register dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch32 = temps.AcquireW();
    MOZ_ASSERT(scratch32.asUnsized() != src.base);
    load32(src, scratch32.asUnsized());
    Sub(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(scratch32));
}

void
MacroAssembler::subPtr(Register src, Register dest)
{
    Sub(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(ARMRegister(src, 64)));
}

void
MacroAssembler::subPtr(Register src, const Address& dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch64 = temps.AcquireX();
    MOZ_ASSERT(scratch64.asUnsized() != dest.base);

    Ldr(scratch64, MemOperand(ARMRegister(dest.base, 64), dest.offset));
    Sub(scratch64, scratch64, Operand(ARMRegister(src, 64)));
    Str(scratch64, MemOperand(ARMRegister(dest.base, 64), dest.offset));
}

void
MacroAssembler::subPtr(Imm32 imm, Register dest)
{
    Sub(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(imm.value));
}

void
MacroAssembler::subPtr(const Address& addr, Register dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch64 = temps.AcquireX();
    MOZ_ASSERT(scratch64.asUnsized() != addr.base);

    Ldr(scratch64, MemOperand(ARMRegister(addr.base, 64), addr.offset));
    Sub(ARMRegister(dest, 64), ARMRegister(dest, 64), Operand(scratch64));
}

void
MacroAssembler::subDouble(FloatRegister src, FloatRegister dest)
{
    fsub(ARMFPRegister(dest, 64), ARMFPRegister(dest, 64), ARMFPRegister(src, 64));
}

void
MacroAssembler::mul32(Register src1, Register src2, Register dest, Label* onOver, Label* onZero)
{
    Smull(ARMRegister(dest, 64), ARMRegister(src1, 32), ARMRegister(src2, 32));
    if (onOver) {
        Cmp(ARMRegister(dest, 64), Operand(ARMRegister(dest, 32), vixl::SXTW));
        B(onOver, NotEqual);
    }
    if (onZero)
        Cbz(ARMRegister(dest, 32), onZero);

    // Clear upper 32 bits.
    Mov(ARMRegister(dest, 32), ARMRegister(dest, 32));
}

void
MacroAssembler::mul64(Imm64 imm, const Register64& dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch64 = temps.AcquireX();
    MOZ_ASSERT(dest.reg != scratch64.asUnsized());
    mov(ImmWord(imm.value), scratch64.asUnsized());
    Mul(ARMRegister(dest.reg, 64), ARMRegister(dest.reg, 64), scratch64);
}

void
MacroAssembler::mulBy3(Register src, Register dest)
{
    ARMRegister xdest(dest, 64);
    ARMRegister xsrc(src, 64);
    Add(xdest, xsrc, Operand(xsrc, vixl::LSL, 1));
}

void
MacroAssembler::mulDouble(FloatRegister src, FloatRegister dest)
{
    fmul(ARMFPRegister(dest, 64), ARMFPRegister(dest, 64), ARMFPRegister(src, 64));
}

void
MacroAssembler::mulDoublePtr(ImmPtr imm, Register temp, FloatRegister dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(temp != scratch);
    movePtr(imm, scratch);
    const ARMFPRegister scratchDouble = temps.AcquireD();
    Ldr(scratchDouble, MemOperand(Address(scratch, 0)));
    fmul(ARMFPRegister(dest, 64), ARMFPRegister(dest, 64), scratchDouble);
}

void
MacroAssembler::divDouble(FloatRegister src, FloatRegister dest)
{
    fdiv(ARMFPRegister(dest, 64), ARMFPRegister(dest, 64), ARMFPRegister(src, 64));
}

void
MacroAssembler::inc64(AbsoluteAddress dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratchAddr64 = temps.AcquireX();
    const ARMRegister scratch64 = temps.AcquireX();

    Mov(scratchAddr64, uint64_t(dest.addr));
    Ldr(scratch64, MemOperand(scratchAddr64, 0));
    Add(scratch64, scratch64, Operand(1));
    Str(scratch64, MemOperand(scratchAddr64, 0));
}

void
MacroAssembler::neg32(Register reg)
{
    Negs(ARMRegister(reg, 32), Operand(ARMRegister(reg, 32)));
}

void
MacroAssembler::negateFloat(FloatRegister reg)
{
    fneg(ARMFPRegister(reg, 32), ARMFPRegister(reg, 32));
}

void
MacroAssembler::negateDouble(FloatRegister reg)
{
    fneg(ARMFPRegister(reg, 64), ARMFPRegister(reg, 64));
}

// ===============================================================
// Shift functions

void
MacroAssembler::lshiftPtr(Imm32 imm, Register dest)
{
    Lsl(ARMRegister(dest, 64), ARMRegister(dest, 64), imm.value);
}

void
MacroAssembler::lshift64(Imm32 imm, Register64 dest)
{
    lshiftPtr(imm, dest.reg);
}

void
MacroAssembler::rshiftPtr(Imm32 imm, Register dest)
{
    Lsr(ARMRegister(dest, 64), ARMRegister(dest, 64), imm.value);
}

void
MacroAssembler::rshiftPtr(Imm32 imm, Register src, Register dest)
{
    Lsr(ARMRegister(dest, 64), ARMRegister(src, 64), imm.value);
}

void
MacroAssembler::rshiftPtrArithmetic(Imm32 imm, Register dest)
{
    Asr(ARMRegister(dest, 64), ARMRegister(dest, 64), imm.value);
}

void
MacroAssembler::rshift64(Imm32 imm, Register64 dest)
{
    rshiftPtr(imm, dest.reg);
}

// ===============================================================
// Branch functions

void
MacroAssembler::branch32(Condition cond, Register lhs, Register rhs, Label* label)
{
    cmp32(lhs, rhs);
    B(label, cond);
}

void
MacroAssembler::branch32(Condition cond, Register lhs, Imm32 imm, Label* label)
{
    cmp32(lhs, imm);
    B(label, cond);
}

void
MacroAssembler::branch32(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(scratch != lhs.base);
    MOZ_ASSERT(scratch != rhs);
    load32(lhs, scratch);
    branch32(cond, scratch, rhs, label);
}

void
MacroAssembler::branch32(Condition cond, const Address& lhs, Imm32 imm, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(scratch != lhs.base);
    load32(lhs, scratch);
    branch32(cond, scratch, imm, label);
}

void
MacroAssembler::branch32(Condition cond, const AbsoluteAddress& lhs, Register rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    movePtr(ImmPtr(lhs.addr), scratch);
    branch32(cond, Address(scratch, 0), rhs, label);
}

void
MacroAssembler::branch32(Condition cond, const AbsoluteAddress& lhs, Imm32 rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    movePtr(ImmPtr(lhs.addr), scratch);
    branch32(cond, Address(scratch, 0), rhs, label);
}

void
MacroAssembler::branch32(Condition cond, const BaseIndex& lhs, Imm32 rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch32 = temps.AcquireW();
    MOZ_ASSERT(scratch32.asUnsized() != lhs.base);
    MOZ_ASSERT(scratch32.asUnsized() != lhs.index);
    doBaseIndex(scratch32, lhs, vixl::LDR_w);
    branch32(cond, scratch32.asUnsized(), rhs, label);
}


void
MacroAssembler::branch32(Condition cond, const Operand& lhs, Register rhs, Label* label)
{
    // since rhs is an operand, do the compare backwards
    Cmp(ARMRegister(rhs, 32), lhs);
    B(label, Assembler::InvertCmpCondition(cond));
}

void
MacroAssembler::branch32(Condition cond, const Operand& lhs, Imm32 rhs, Label* label)
{
    ARMRegister l = lhs.reg();
    Cmp(l, Operand(rhs.value));
    B(label, cond);
}

void
MacroAssembler::branch32(Condition cond, wasm::SymbolicAddress lhs, Imm32 rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    movePtr(lhs, scratch);
    branch32(cond, Address(scratch, 0), rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, Register rhs, Label* label)
{
    Cmp(ARMRegister(lhs, 64), ARMRegister(rhs, 64));
    B(label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    cmpPtr(lhs, rhs);
    B(label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmPtr rhs, Label* label)
{
    cmpPtr(lhs, rhs);
    B(label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmGCPtr rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(scratch != lhs);
    movePtr(rhs, scratch);
    branchPtr(cond, lhs, scratch, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmWord rhs, Label* label)
{
    cmpPtr(lhs, rhs);
    B(label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(scratch != lhs.base);
    MOZ_ASSERT(scratch != rhs);
    loadPtr(lhs, scratch);
    branchPtr(cond, scratch, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmPtr rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(scratch != lhs.base);
    loadPtr(lhs, scratch);
    branchPtr(cond, scratch, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmGCPtr rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch1_64 = temps.AcquireX();
    const ARMRegister scratch2_64 = temps.AcquireX();
    MOZ_ASSERT(scratch1_64.asUnsized() != lhs.base);
    MOZ_ASSERT(scratch2_64.asUnsized() != lhs.base);

    movePtr(rhs, scratch1_64.asUnsized());
    loadPtr(lhs, scratch2_64.asUnsized());
    branchPtr(cond, scratch2_64.asUnsized(), scratch1_64.asUnsized(), label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmWord rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(scratch != lhs.base);
    loadPtr(lhs, scratch);
    branchPtr(cond, scratch, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const AbsoluteAddress& lhs, Register rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(scratch != rhs);
    loadPtr(lhs, scratch);
    branchPtr(cond, scratch, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const AbsoluteAddress& lhs, ImmWord rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    loadPtr(lhs, scratch);
    branchPtr(cond, scratch, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, wasm::SymbolicAddress lhs, Register rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(scratch != rhs);
    loadPtr(lhs, scratch);
    branchPtr(cond, scratch, rhs, label);
}

void
MacroAssembler::branchPrivatePtr(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    if (rhs != scratch)
        movePtr(rhs, scratch);
    // Instead of unboxing lhs, box rhs and do direct comparison with lhs.
    rshiftPtr(Imm32(1), scratch);
    branchPtr(cond, lhs, scratch, label);
}

void
MacroAssembler::branchFloat(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                            Label* label)
{
    compareFloat(cond, lhs, rhs);
    switch (cond) {
      case DoubleNotEqual: {
        Label unordered;
        // not equal *and* ordered
        branch(Overflow, &unordered);
        branch(NotEqual, label);
        bind(&unordered);
        break;
      }
      case DoubleEqualOrUnordered:
        branch(Overflow, label);
        branch(Equal, label);
        break;
      default:
        branch(Condition(cond), label);
    }
}

void
MacroAssembler::branchTruncateFloat32(FloatRegister src, Register dest, Label* fail)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch64 = temps.AcquireX();

    ARMFPRegister src32(src, 32);
    ARMRegister dest64(dest, 64);

    MOZ_ASSERT(!scratch64.Is(dest64));

    Fcvtzs(dest64, src32);
    Add(scratch64, dest64, Operand(0x7fffffffffffffff));
    Cmn(scratch64, 3);
    B(fail, Assembler::Above);
    And(dest64, dest64, Operand(0xffffffff));
}

template <class L>
void
MacroAssembler::branchTest32(Condition cond, Register lhs, Register rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    // x86 prefers |test foo, foo| to |cmp foo, #0|.
    // Convert the former to the latter for ARM.
    if (lhs == rhs && (cond == Zero || cond == NonZero))
        cmp32(lhs, Imm32(0));
    else
        test32(lhs, rhs);
    B(label, cond);
}

template <class L>
void
MacroAssembler::branchTest32(Condition cond, Register lhs, Imm32 rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    test32(lhs, rhs);
    B(label, cond);
}

void
MacroAssembler::branchTest32(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(scratch != lhs.base);
    load32(lhs, scratch);
    branchTest32(cond, scratch, rhs, label);
}

void
MacroAssembler::branchTest32(Condition cond, const AbsoluteAddress& lhs, Imm32 rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    load32(lhs, scratch);
    branchTest32(cond, scratch, rhs, label);
}

void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Register rhs, Label* label)
{
    Tst(ARMRegister(lhs, 64), Operand(ARMRegister(rhs, 64)));
    B(label, cond);
}

void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    Tst(ARMRegister(lhs, 64), Operand(rhs.value));
    B(label, cond);
}

void
MacroAssembler::branchTestPtr(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    MOZ_ASSERT(scratch != lhs.base);
    loadPtr(lhs, scratch);
    branchTestPtr(cond, scratch, rhs, label);
}

void
MacroAssembler::branchTest64(Condition cond, Register64 lhs, Register64 rhs, Register temp,
                             Label* label)
{
    branchTestPtr(cond, lhs.reg, rhs.reg, label);
}

//}}} check_macroassembler_style
// ===============================================================

template <typename T>
void
MacroAssemblerCompat::addToStackPtr(T t)
{
    asMasm().addPtr(t, getStackPointer());
}

template <typename T>
void
MacroAssemblerCompat::addStackPtrTo(T t)
{
    asMasm().addPtr(getStackPointer(), t);
}

template <typename T>
void
MacroAssemblerCompat::subFromStackPtr(T t)
{
    asMasm().subPtr(t, getStackPointer()); syncStackPtr();
}

template <typename T>
void
MacroAssemblerCompat::subStackPtrFrom(T t)
{
    asMasm().subPtr(getStackPointer(), t);
}

template <typename T>
void
MacroAssemblerCompat::andToStackPtr(T t)
{
    asMasm().andPtr(t, getStackPointer());
    syncStackPtr();
}

template <typename T>
void
MacroAssemblerCompat::andStackPtrTo(T t)
{
    asMasm().andPtr(getStackPointer(), t);
}

template <typename T>
void
MacroAssemblerCompat::branchStackPtr(Condition cond, T rhs, Label* label)
{
    asMasm().branchPtr(cond, getStackPointer(), rhs, label);
}

template <typename T>
void
MacroAssemblerCompat::branchStackPtrRhs(Condition cond, T lhs, Label* label)
{
    asMasm().branchPtr(cond, lhs, getStackPointer(), label);
}

template <typename T>
void
MacroAssemblerCompat::branchTestStackPtr(Condition cond, T t, Label* label)
{
    asMasm().branchTestPtr(cond, getStackPointer(), t, label);
}

} // namespace jit
} // namespace js

#endif /* jit_arm64_MacroAssembler_arm64_inl_h */
