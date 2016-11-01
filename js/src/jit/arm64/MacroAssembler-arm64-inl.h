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

void
MacroAssembler::move64(Register64 src, Register64 dest)
{
    movePtr(src.reg, dest.reg);
}

void
MacroAssembler::move64(Imm64 imm, Register64 dest)
{
    movePtr(ImmWord(imm.value), dest.reg);
}

void
MacroAssembler::moveFloat32ToGPR(FloatRegister src, Register dest)
{
    MOZ_CRASH("NYI: moveFloat32ToGPR");
}

void
MacroAssembler::moveGPRToFloat32(Register src, FloatRegister dest)
{
    MOZ_CRASH("NYI: moveGPRToFloat32");
}

void
MacroAssembler::move8SignExtend(Register src, Register dest)
{
    MOZ_CRASH("NYI: move8SignExtend");
}

void
MacroAssembler::move16SignExtend(Register src, Register dest)
{
    MOZ_CRASH("NYI: move16SignExtend");
}

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
MacroAssembler::and64(Register64 src, Register64 dest)
{
    MOZ_CRASH("NYI: and64");
}

void
MacroAssembler::or64(Imm64 imm, Register64 dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    mov(ImmWord(imm.value), scratch);
    orPtr(scratch, dest.reg);
}

void
MacroAssembler::xor64(Imm64 imm, Register64 dest)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    mov(ImmWord(imm.value), scratch);
    xorPtr(scratch, dest.reg);
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
MacroAssembler::xor32(Register src, Register dest)
{
    Eor(ARMRegister(dest, 32), ARMRegister(dest, 32), Operand(ARMRegister(src, 32)));
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
MacroAssembler::addFloat32(FloatRegister src, FloatRegister dest)
{
    fadd(ARMFPRegister(dest, 32), ARMFPRegister(dest, 32), ARMFPRegister(src, 32));
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
MacroAssembler::sub64(Register64 src, Register64 dest)
{
    MOZ_CRASH("NYI: sub64");
}

void
MacroAssembler::subDouble(FloatRegister src, FloatRegister dest)
{
    fsub(ARMFPRegister(dest, 64), ARMFPRegister(dest, 64), ARMFPRegister(src, 64));
}

void
MacroAssembler::subFloat32(FloatRegister src, FloatRegister dest)
{
    fsub(ARMFPRegister(dest, 32), ARMFPRegister(dest, 32), ARMFPRegister(src, 32));
}

void
MacroAssembler::mul32(Register rhs, Register srcDest)
{
    MOZ_CRASH("NYI - mul32");
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
MacroAssembler::mul64(const Register64& src, const Register64& dest, const Register temp)
{
    MOZ_CRASH("NYI: mul64");
}

void
MacroAssembler::mulBy3(Register src, Register dest)
{
    ARMRegister xdest(dest, 64);
    ARMRegister xsrc(src, 64);
    Add(xdest, xsrc, Operand(xsrc, vixl::LSL, 1));
}

void
MacroAssembler::mulFloat32(FloatRegister src, FloatRegister dest)
{
    fmul(ARMFPRegister(dest, 32), ARMFPRegister(dest, 32), ARMFPRegister(src, 32));
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
MacroAssembler::quotient32(Register rhs, Register srcDest, bool isUnsigned)
{
    MOZ_CRASH("NYI - quotient32");
}

void
MacroAssembler::remainder32(Register rhs, Register srcDest, bool isUnsigned)
{
    MOZ_CRASH("NYI - remainder32");
}

void
MacroAssembler::divFloat32(FloatRegister src, FloatRegister dest)
{
    fdiv(ARMFPRegister(dest, 32), ARMFPRegister(dest, 32), ARMFPRegister(src, 32));
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

void
MacroAssembler::absFloat32(FloatRegister src, FloatRegister dest)
{
    MOZ_CRASH("NYI - absFloat32");
}

void
MacroAssembler::absDouble(FloatRegister src, FloatRegister dest)
{
    MOZ_CRASH("NYI - absDouble");
}

void
MacroAssembler::sqrtFloat32(FloatRegister src, FloatRegister dest)
{
    MOZ_CRASH("NYI - sqrtFloat32");
}

void
MacroAssembler::sqrtDouble(FloatRegister src, FloatRegister dest)
{
    MOZ_CRASH("NYI - sqrtDouble");
}

void
MacroAssembler::minFloat32(FloatRegister other, FloatRegister srcDest, bool handleNaN)
{
    MOZ_CRASH("NYI - minFloat32");
}

void
MacroAssembler::minDouble(FloatRegister other, FloatRegister srcDest, bool handleNaN)
{
    MOZ_CRASH("NYI - minDouble");
}

void
MacroAssembler::maxFloat32(FloatRegister other, FloatRegister srcDest, bool handleNaN)
{
    MOZ_CRASH("NYI - maxFloat32");
}

void
MacroAssembler::maxDouble(FloatRegister other, FloatRegister srcDest, bool handleNaN)
{
    MOZ_CRASH("NYI - maxDouble");
}

// ===============================================================
// Shift functions

void
MacroAssembler::lshiftPtr(Imm32 imm, Register dest)
{
    MOZ_ASSERT(0 <= imm.value && imm.value < 64);
    Lsl(ARMRegister(dest, 64), ARMRegister(dest, 64), imm.value);
}

void
MacroAssembler::lshift64(Imm32 imm, Register64 dest)
{
    MOZ_ASSERT(0 <= imm.value && imm.value < 64);
    lshiftPtr(imm, dest.reg);
}

void
MacroAssembler::lshift64(Register shift, Register64 srcDest)
{
    MOZ_CRASH("NYI: lshift64");
}

void
MacroAssembler::lshift32(Register shift, Register dest)
{
    Lsl(ARMRegister(dest, 32), ARMRegister(dest, 32), ARMRegister(shift, 32));
}

void
MacroAssembler::lshift32(Imm32 imm, Register dest)
{
    MOZ_ASSERT(0 <= imm.value && imm.value < 32);
    Lsl(ARMRegister(dest, 32), ARMRegister(dest, 32), imm.value);
}

void
MacroAssembler::rshiftPtr(Imm32 imm, Register dest)
{
    MOZ_ASSERT(0 <= imm.value && imm.value < 64);
    Lsr(ARMRegister(dest, 64), ARMRegister(dest, 64), imm.value);
}

void
MacroAssembler::rshiftPtr(Imm32 imm, Register src, Register dest)
{
    MOZ_ASSERT(0 <= imm.value && imm.value < 64);
    Lsr(ARMRegister(dest, 64), ARMRegister(src, 64), imm.value);
}

void
MacroAssembler::rshift32(Register shift, Register dest)
{
    Lsr(ARMRegister(dest, 32), ARMRegister(dest, 32), ARMRegister(shift, 32));
}

void
MacroAssembler::rshift32(Imm32 imm, Register dest)
{
    MOZ_ASSERT(0 <= imm.value && imm.value < 32);
    Lsr(ARMRegister(dest, 32), ARMRegister(dest, 32), imm.value);
}

void
MacroAssembler::rshiftPtrArithmetic(Imm32 imm, Register dest)
{
    MOZ_ASSERT(0 <= imm.value && imm.value < 64);
    Asr(ARMRegister(dest, 64), ARMRegister(dest, 64), imm.value);
}

void
MacroAssembler::rshift32Arithmetic(Register shift, Register dest)
{
    Asr(ARMRegister(dest, 32), ARMRegister(dest, 32), ARMRegister(shift, 32));
}

void
MacroAssembler::rshift32Arithmetic(Imm32 imm, Register dest)
{
    MOZ_ASSERT(0 <= imm.value && imm.value < 32);
    Asr(ARMRegister(dest, 32), ARMRegister(dest, 32), imm.value);
}

void
MacroAssembler::rshift64(Imm32 imm, Register64 dest)
{
    MOZ_ASSERT(0 <= imm.value && imm.value < 64);
    rshiftPtr(imm, dest.reg);
}

void
MacroAssembler::rshift64(Register shift, Register64 srcDest)
{
    MOZ_CRASH("NYI: rshift64");
}

void
MacroAssembler::rshift64Arithmetic(Imm32 imm, Register64 dest)
{
    MOZ_CRASH("NYI: rshift64Arithmetic");
}

void
MacroAssembler::rshift64Arithmetic(Register shift, Register64 srcDest)
{
    MOZ_CRASH("NYI: rshift64Arithmetic");
}

// ===============================================================
// Rotation functions

void
MacroAssembler::rotateLeft(Imm32 count, Register input, Register dest)
{
    MOZ_CRASH("NYI: rotateLeft by immediate");
}

void
MacroAssembler::rotateLeft(Register count, Register input, Register dest)
{
    MOZ_CRASH("NYI: rotateLeft by register");
}

void
MacroAssembler::rotateRight(Imm32 count, Register input, Register dest)
{
    MOZ_CRASH("NYI: rotateRight by immediate");
}

void
MacroAssembler::rotateRight(Register count, Register input, Register dest)
{
    MOZ_CRASH("NYI: rotateRight by register");
}

void
MacroAssembler::rotateLeft64(Register count, Register64 input, Register64 dest, Register temp)
{
    MOZ_CRASH("NYI: rotateLeft64");
}

void
MacroAssembler::rotateRight64(Register count, Register64 input, Register64 dest, Register temp)
{
    MOZ_CRASH("NYI: rotateRight64");
}

// ===============================================================
// Bit counting functions

void
MacroAssembler::clz32(Register src, Register dest, bool knownNotZero)
{
    MOZ_CRASH("NYI: clz32");
}

void
MacroAssembler::ctz32(Register src, Register dest, bool knownNotZero)
{
    MOZ_CRASH("NYI: ctz32");
}

void
MacroAssembler::clz64(Register64 src, Register dest)
{
    MOZ_CRASH("NYI: clz64");
}

void
MacroAssembler::ctz64(Register64 src, Register dest)
{
    MOZ_CRASH("NYI: ctz64");
}

void
MacroAssembler::popcnt32(Register src, Register dest, Register temp)
{
    MOZ_CRASH("NYI: popcnt32");
}

void
MacroAssembler::popcnt64(Register64 src, Register64 dest, Register temp)
{
    MOZ_CRASH("NYI: popcnt64");
}

// ===============================================================
// Branch functions

template <class L>
void
MacroAssembler::branch32(Condition cond, Register lhs, Register rhs, L label)
{
    cmp32(lhs, rhs);
    B(label, cond);
}

template <class L>
void
MacroAssembler::branch32(Condition cond, Register lhs, Imm32 imm, L label)
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
MacroAssembler::branch32(Condition cond, wasm::SymbolicAddress lhs, Imm32 rhs, Label* label)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    movePtr(lhs, scratch);
    branch32(cond, Address(scratch, 0), rhs, label);
}

void
MacroAssembler::branch64(Condition cond, const Address& lhs, Imm64 val, Label* label)
{
    MOZ_ASSERT(cond == Assembler::NotEqual,
               "other condition codes not supported");

    branchPtr(cond, lhs, ImmWord(val.value), label);
}

void
MacroAssembler::branch64(Condition cond, const Address& lhs, const Address& rhs, Register scratch,
                         Label* label)
{
    MOZ_ASSERT(cond == Assembler::NotEqual,
               "other condition codes not supported");
    MOZ_ASSERT(lhs.base != scratch);
    MOZ_ASSERT(rhs.base != scratch);

    loadPtr(rhs, scratch);
    branchPtr(cond, lhs, scratch, label);
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

template <class L>
void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, Register rhs, L label)
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

template <typename T>
CodeOffsetJump
MacroAssembler::branchPtrWithPatch(Condition cond, Register lhs, T rhs, RepatchLabel* label)
{
    cmpPtr(lhs, rhs);
    return jumpWithPatch(label, cond);
}

template <typename T>
CodeOffsetJump
MacroAssembler::branchPtrWithPatch(Condition cond, Address lhs, T rhs, RepatchLabel* label)
{
    // The scratch register is unused after the condition codes are set.
    {
        vixl::UseScratchRegisterScope temps(this);
        const Register scratch = temps.AcquireX().asUnsized();
        MOZ_ASSERT(scratch != lhs.base);
        loadPtr(lhs, scratch);
        cmpPtr(scratch, rhs);
    }
    return jumpWithPatch(label, cond);
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
MacroAssembler::branchTruncateFloat32MaybeModUint32(FloatRegister src, Register dest, Label* fail)
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

void
MacroAssembler::branchTruncateFloat32ToInt32(FloatRegister src, Register dest, Label* fail)
{
    convertFloat32ToInt32(src, dest, fail);
}

void
MacroAssembler::branchDouble(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                             Label* label)
{
    compareDouble(cond, lhs, rhs);
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
MacroAssembler::branchTruncateDoubleMaybeModUint32(FloatRegister src, Register dest, Label* fail)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch64 = temps.AcquireX();

    // An out of range integer will be saturated to the destination size.
    ARMFPRegister src64(src, 64);
    ARMRegister dest64(dest, 64);

    MOZ_ASSERT(!scratch64.Is(dest64));

    Fcvtzs(dest64, src64);
    Add(scratch64, dest64, Operand(0x7fffffffffffffff));
    Cmn(scratch64, 3);
    B(fail, Assembler::Above);
    And(dest64, dest64, Operand(0xffffffff));
}

void
MacroAssembler::branchTruncateDoubleToInt32(FloatRegister src, Register dest, Label* fail)
{
    convertDoubleToInt32(src, dest, fail);
}

template <typename T, typename L>
void
MacroAssembler::branchAdd32(Condition cond, T src, Register dest, L label)
{
    adds32(src, dest);
    B(label, cond);
}

template <typename T>
void
MacroAssembler::branchSub32(Condition cond, T src, Register dest, Label* label)
{
    subs32(src, dest);
    branch(cond, label);
}

void
MacroAssembler::decBranchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    Subs(ARMRegister(lhs, 64), ARMRegister(lhs, 64), Operand(rhs.value));
    B(cond, label);
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

template <class L>
void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Register rhs, L label)
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

template <class L>
void
MacroAssembler::branchTest64(Condition cond, Register64 lhs, Register64 rhs, Register temp,
                             L label)
{
    branchTestPtr(cond, lhs.reg, rhs.reg, label);
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
    B(label, c);
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
    B(label, c);
}

void
MacroAssembler::branchTestInt32Truthy(bool truthy, const ValueOperand& value, Label* label)
{
    Condition c = testInt32Truthy(truthy, value);
    B(label, c);
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
    B(label, c);
}

void
MacroAssembler::branchTestDoubleTruthy(bool truthy, FloatRegister reg, Label* label)
{
    Fcmp(ARMFPRegister(reg, 64), 0.0);
    if (!truthy) {
        // falsy values are zero, and NaN.
        branch(Zero, label);
        branch(Overflow, label);
    } else {
        // truthy values are non-zero and not nan.
        // If it is overflow
        Label onFalse;
        branch(Zero, &onFalse);
        branch(Overflow, &onFalse);
        B(label);
        bind(&onFalse);
    }
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
    Condition c = testNumber(cond, t);
    B(label, c);
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
MacroAssembler::branchTestBooleanImpl(Condition cond, const T& tag, Label* label)
{
    Condition c = testBoolean(cond, tag);
    B(label, c);
}

void
MacroAssembler::branchTestBooleanTruthy(bool truthy, const ValueOperand& value, Label* label)
{
    Condition c = testBooleanTruthy(truthy, value);
    B(label, c);
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
    B(label, c);
}

void
MacroAssembler::branchTestStringTruthy(bool truthy, const ValueOperand& value, Label* label)
{
    Condition c = testStringTruthy(truthy, value);
    B(label, c);
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
    B(label, c);
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
    B(label, c);
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
    B(label, c);
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
MacroAssembler::branchTestGCThingImpl(Condition cond, const T& src, Label* label)
{
    Condition c = testGCThing(cond, src);
    B(label, c);
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
    B(label, c);
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
    Condition c = testMagic(cond, t);
    B(label, c);
}

void
MacroAssembler::branchTestMagic(Condition cond, const Address& valaddr, JSWhyMagic why, Label* label)
{
    uint64_t magic = MagicValue(why).asRawBits();
    cmpPtr(valaddr, ImmWord(magic));
    B(label, cond);
}

// ========================================================================
// Memory access primitives.
void
MacroAssembler::storeUncanonicalizedDouble(FloatRegister src, const Address& dest)
{
    Str(ARMFPRegister(src, 64), MemOperand(ARMRegister(dest.base, 64), dest.offset));
}
void
MacroAssembler::storeUncanonicalizedDouble(FloatRegister src, const BaseIndex& dest)
{
    doBaseIndex(ARMFPRegister(src, 64), dest, vixl::STR_d);
}

void
MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src, const Address& addr)
{
    Str(ARMFPRegister(src, 32), MemOperand(ARMRegister(addr.base, 64), addr.offset));
}
void
MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src, const BaseIndex& addr)
{
    doBaseIndex(ARMFPRegister(src, 32), addr, vixl::STR_s);
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

void
MacroAssembler::memoryBarrier(MemoryBarrierBits barrier)
{
    MOZ_CRASH("NYI");
}

// ===============================================================
// Clamping functions.

void
MacroAssembler::clampIntToUint8(Register reg)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch32 = temps.AcquireW();
    const ARMRegister reg32(reg, 32);
    MOZ_ASSERT(!scratch32.Is(reg32));

    Cmp(reg32, Operand(reg32, vixl::UXTB));
    Csel(reg32, reg32, vixl::wzr, Assembler::GreaterThanOrEqual);
    Mov(scratch32, Operand(0xff));
    Csel(reg32, reg32, scratch32, Assembler::LessThanOrEqual);
}

// ========================================================================
// wasm support

template <class L>
void
MacroAssembler::wasmBoundsCheck(Condition cond, Register index, L label)
{
    MOZ_CRASH("NYI");
}

void
MacroAssembler::wasmPatchBoundsCheck(uint8_t* patchAt, uint32_t limit)
{
    MOZ_CRASH("NYI");
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

// If source is a double, load into dest.
// If source is int32, convert to double and store in dest.
// Else, branch to failure.
void
MacroAssemblerCompat::ensureDouble(const ValueOperand& source, FloatRegister dest, Label* failure)
{
    Label isDouble, done;

    // TODO: splitTagForTest really should not leak a scratch register.
    Register tag = splitTagForTest(source);
    {
        vixl::UseScratchRegisterScope temps(this);
        temps.Exclude(ARMRegister(tag, 64));

        asMasm().branchTestDouble(Assembler::Equal, tag, &isDouble);
        asMasm().branchTestInt32(Assembler::NotEqual, tag, failure);
    }

    convertInt32ToDouble(source.valueReg(), dest);
    jump(&done);

    bind(&isDouble);
    unboxDouble(source, dest);

    bind(&done);
}

void
MacroAssemblerCompat::unboxValue(const ValueOperand& src, AnyRegister dest)
{
    if (dest.isFloat()) {
        Label notInt32, end;
        asMasm().branchTestInt32(Assembler::NotEqual, src, &notInt32);
        convertInt32ToDouble(src.valueReg(), dest.fpu());
        jump(&end);
        bind(&notInt32);
        unboxDouble(src, dest.fpu());
        bind(&end);
    } else {
        unboxNonDouble(src, dest.gpr());
    }
}

} // namespace jit
} // namespace js

#endif /* jit_arm64_MacroAssembler_arm64_inl_h */
