/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x86_shared_MacroAssembler_x86_shared_inl_h
#define jit_x86_shared_MacroAssembler_x86_shared_inl_h

#include "jit/x86-shared/MacroAssembler-x86-shared.h"

namespace js {
namespace jit {

//{{{ check_macroassembler_style
// ===============================================================
// Logical instructions

void
MacroAssembler::not32(Register reg)
{
    notl(reg);
}

void
MacroAssembler::and32(Register src, Register dest)
{
    andl(src, dest);
}

void
MacroAssembler::and32(Imm32 imm, Register dest)
{
    andl(imm, dest);
}

void
MacroAssembler::and32(Imm32 imm, const Address& dest)
{
    andl(imm, Operand(dest));
}

void
MacroAssembler::and32(const Address& src, Register dest)
{
    andl(Operand(src), dest);
}

void
MacroAssembler::or32(Register src, Register dest)
{
    orl(src, dest);
}

void
MacroAssembler::or32(Imm32 imm, Register dest)
{
    orl(imm, dest);
}

void
MacroAssembler::or32(Imm32 imm, const Address& dest)
{
    orl(imm, Operand(dest));
}

void
MacroAssembler::xor32(Register src, Register dest)
{
    xorl(src, dest);
}

void
MacroAssembler::xor32(Imm32 imm, Register dest)
{
    xorl(imm, dest);
}

void
MacroAssembler::clz32(Register src, Register dest, bool knownNotZero)
{
    // On very recent chips (Haswell and newer?) there is actually an
    // LZCNT instruction that does all of this.

    bsrl(src, dest);
    if (!knownNotZero) {
        // If the source is zero then bsrl leaves garbage in the destination.
        Label nonzero;
        j(Assembler::NonZero, &nonzero);
        movl(Imm32(0x3F), dest);
        bind(&nonzero);
    }
    xorl(Imm32(0x1F), dest);
}

void
MacroAssembler::ctz32(Register src, Register dest, bool knownNotZero)
{
    bsfl(src, dest);
    if (!knownNotZero) {
        Label nonzero;
        j(Assembler::NonZero, &nonzero);
        movl(Imm32(32), dest);
        bind(&nonzero);
    }
}

void
MacroAssembler::popcnt32(Register input, Register output, Register tmp)
{
    if (AssemblerX86Shared::HasPOPCNT()) {
        popcntl(input, output);
        return;
    }

    MOZ_ASSERT(tmp != InvalidReg);

    // Equivalent to mozilla::CountPopulation32()

    movl(input, output);
    shrl(Imm32(1), output);
    andl(Imm32(0x55555555), output);
    subl(output, tmp);
    movl(tmp, output);
    andl(Imm32(0x33333333), output);
    shrl(Imm32(2), tmp);
    andl(Imm32(0x33333333), tmp);
    addl(output, tmp);
    movl(tmp, output);
    shrl(Imm32(4), output);
    addl(tmp, output);
    andl(Imm32(0xF0F0F0F), output);
    imull(Imm32(0x1010101), output, output);
    shrl(Imm32(24), output);
}

// ===============================================================
// Arithmetic instructions

void
MacroAssembler::add32(Register src, Register dest)
{
    addl(src, dest);
}

void
MacroAssembler::add32(Imm32 imm, Register dest)
{
    addl(imm, dest);
}

void
MacroAssembler::add32(Imm32 imm, const Address& dest)
{
    addl(imm, Operand(dest));
}

void
MacroAssembler::add32(Imm32 imm, const AbsoluteAddress& dest)
{
    addl(imm, Operand(dest));
}

void
MacroAssembler::addFloat32(FloatRegister src, FloatRegister dest)
{
    vaddss(src, dest, dest);
}

void
MacroAssembler::addDouble(FloatRegister src, FloatRegister dest)
{
    vaddsd(src, dest, dest);
}

void
MacroAssembler::sub32(Register src, Register dest)
{
    subl(src, dest);
}

void
MacroAssembler::sub32(Imm32 imm, Register dest)
{
    subl(imm, dest);
}

void
MacroAssembler::sub32(const Address& src, Register dest)
{
    subl(Operand(src), dest);
}

void
MacroAssembler::subDouble(FloatRegister src, FloatRegister dest)
{
    vsubsd(src, dest, dest);
}

void
MacroAssembler::mulDouble(FloatRegister src, FloatRegister dest)
{
    vmulsd(src, dest, dest);
}

void
MacroAssembler::divDouble(FloatRegister src, FloatRegister dest)
{
    vdivsd(src, dest, dest);
}

void
MacroAssembler::neg32(Register reg)
{
    negl(reg);
}

void
MacroAssembler::negateFloat(FloatRegister reg)
{
    ScratchFloat32Scope scratch(*this);
    vpcmpeqw(Operand(scratch), scratch, scratch);
    vpsllq(Imm32(31), scratch, scratch);

    // XOR the float in a float register with -0.0.
    vxorps(scratch, reg, reg); // s ^ 0x80000000
}

void
MacroAssembler::negateDouble(FloatRegister reg)
{
    // From MacroAssemblerX86Shared::maybeInlineDouble
    ScratchDoubleScope scratch(*this);
    vpcmpeqw(Operand(scratch), scratch, scratch);
    vpsllq(Imm32(63), scratch, scratch);

    // XOR the float in a float register with -0.0.
    vxorpd(scratch, reg, reg); // s ^ 0x80000000000000
}

// ===============================================================
// Rotation instructions
void
MacroAssembler::rotateLeft(Imm32 count, Register input, Register dest)
{
    MOZ_ASSERT(input == dest, "defineReuseInput");
    if (count.value)
        roll(count, input);
}

void
MacroAssembler::rotateLeft(Register count, Register input, Register dest)
{
    MOZ_ASSERT(input == dest, "defineReuseInput");
    MOZ_ASSERT(count == ecx, "defineFixed(ecx)");
    roll_cl(input);
}

void
MacroAssembler::rotateRight(Imm32 count, Register input, Register dest)
{
    MOZ_ASSERT(input == dest, "defineReuseInput");
    if (count.value)
        rorl(count, input);
}

void
MacroAssembler::rotateRight(Register count, Register input, Register dest)
{
    MOZ_ASSERT(input == dest, "defineReuseInput");
    MOZ_ASSERT(count == ecx, "defineFixed(ecx)");
    rorl_cl(input);
}

// ===============================================================
// Shift instructions

void
MacroAssembler::lshift32(Register shift, Register srcDest)
{
    MOZ_ASSERT(shift == ecx);
    shll_cl(srcDest);
}

void
MacroAssembler::rshift32(Register shift, Register srcDest)
{
    MOZ_ASSERT(shift == ecx);
    shrl_cl(srcDest);
}

void
MacroAssembler::rshift32Arithmetic(Register shift, Register srcDest)
{
    MOZ_ASSERT(shift == ecx);
    sarl_cl(srcDest);
}

// ===============================================================
// Branch instructions

void
MacroAssembler::branch32(Condition cond, Register lhs, Register rhs, Label* label)
{
    cmp32(lhs, rhs);
    j(cond, label);
}

template <class L>
void
MacroAssembler::branch32(Condition cond, Register lhs, Imm32 rhs, L label)
{
    cmp32(lhs, rhs);
    j(cond, label);
}

void
MacroAssembler::branch32(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    cmp32(Operand(lhs), rhs);
    j(cond, label);
}

void
MacroAssembler::branch32(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    cmp32(Operand(lhs), rhs);
    j(cond, label);
}

void
MacroAssembler::branch32(Condition cond, const BaseIndex& lhs, Register rhs, Label* label)
{
    cmp32(Operand(lhs), rhs);
    j(cond, label);
}

void
MacroAssembler::branch32(Condition cond, const BaseIndex& lhs, Imm32 rhs, Label* label)
{
    cmp32(Operand(lhs), rhs);
    j(cond, label);
}

void
MacroAssembler::branch32(Condition cond, const Operand& lhs, Register rhs, Label* label)
{
    cmp32(lhs, rhs);
    j(cond, label);
}

void
MacroAssembler::branch32(Condition cond, const Operand& lhs, Imm32 rhs, Label* label)
{
    cmp32(lhs, rhs);
    j(cond, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, Register rhs, Label* label)
{
    cmpPtr(lhs, rhs);
    j(cond, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    branchPtrImpl(cond, lhs, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmPtr rhs, Label* label)
{
    branchPtrImpl(cond, lhs, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmGCPtr rhs, Label* label)
{
    branchPtrImpl(cond, lhs, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmWord rhs, Label* label)
{
    branchPtrImpl(cond, lhs, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    branchPtrImpl(cond, lhs, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmPtr rhs, Label* label)
{
    branchPtrImpl(cond, lhs, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmGCPtr rhs, Label* label)
{
    branchPtrImpl(cond, lhs, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmWord rhs, Label* label)
{
    branchPtrImpl(cond, lhs, rhs, label);
}

template <typename T, typename S>
void
MacroAssembler::branchPtrImpl(Condition cond, const T& lhs, const S& rhs, Label* label)
{
    cmpPtr(Operand(lhs), rhs);
    j(cond, label);
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
    cmpPtr(lhs, rhs);
    return jumpWithPatch(label, cond);
}

void
MacroAssembler::branchFloat(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                            Label* label)
{
    compareFloat(cond, lhs, rhs);

    if (cond == DoubleEqual) {
        Label unordered;
        j(Parity, &unordered);
        j(Equal, label);
        bind(&unordered);
        return;
    }

    if (cond == DoubleNotEqualOrUnordered) {
        j(NotEqual, label);
        j(Parity, label);
        return;
    }

    MOZ_ASSERT(!(cond & DoubleConditionBitSpecial));
    j(ConditionFromDoubleCondition(cond), label);
}

void
MacroAssembler::branchDouble(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                             Label* label)
{
    compareDouble(cond, lhs, rhs);

    if (cond == DoubleEqual) {
        Label unordered;
        j(Parity, &unordered);
        j(Equal, label);
        bind(&unordered);
        return;
    }
    if (cond == DoubleNotEqualOrUnordered) {
        j(NotEqual, label);
        j(Parity, label);
        return;
    }

    MOZ_ASSERT(!(cond & DoubleConditionBitSpecial));
    j(ConditionFromDoubleCondition(cond), label);
}

template <typename T>
void
MacroAssembler::branchAdd32(Condition cond, T src, Register dest, Label* label)
{
    addl(src, dest);
    j(cond, label);
}

template <typename T>
void
MacroAssembler::branchSub32(Condition cond, T src, Register dest, Label* label)
{
    subl(src, dest);
    j(cond, label);
}

void
MacroAssembler::decBranchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    subPtr(rhs, lhs);
    j(cond, label);
}

template <class L>
void
MacroAssembler::branchTest32(Condition cond, Register lhs, Register rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    test32(lhs, rhs);
    j(cond, label);
}

template <class L>
void
MacroAssembler::branchTest32(Condition cond, Register lhs, Imm32 rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    test32(lhs, rhs);
    j(cond, label);
}

void
MacroAssembler::branchTest32(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    test32(Operand(lhs), rhs);
    j(cond, label);
}

void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Register rhs, Label* label)
{
    testPtr(lhs, rhs);
    j(cond, label);
}

void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    testPtr(lhs, rhs);
    j(cond, label);
}

void
MacroAssembler::branchTestPtr(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    testPtr(Operand(lhs), rhs);
    j(cond, label);
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
    cond = testUndefined(cond, t);
    j(cond, label);
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
    cond = testInt32(cond, t);
    j(cond, label);
}

void
MacroAssembler::branchTestInt32Truthy(bool truthy, const ValueOperand& value, Label* label)
{
    Condition cond = testInt32Truthy(truthy, value);
    j(cond, label);
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
    cond = testDouble(cond, t);
    j(cond, label);
}

void
MacroAssembler::branchTestDoubleTruthy(bool truthy, FloatRegister reg, Label* label)
{
    Condition cond = testDoubleTruthy(truthy, reg);
    j(cond, label);
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
    j(cond, label);
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
    cond = testBoolean(cond, t);
    j(cond, label);
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
    cond = testString(cond, t);
    j(cond, label);
}

void
MacroAssembler::branchTestStringTruthy(bool truthy, const ValueOperand& value, Label* label)
{
    Condition cond = testStringTruthy(truthy, value);
    j(cond, label);
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
    cond = testSymbol(cond, t);
    j(cond, label);
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
    cond = testNull(cond, t);
    j(cond, label);
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
    cond = testObject(cond, t);
    j(cond, label);
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
    cond = testGCThing(cond, t);
    j(cond, label);
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
    cond = testPrimitive(cond, t);
    j(cond, label);
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
    j(cond, label);
}

// ========================================================================
// Canonicalization primitives.
void
MacroAssembler::canonicalizeFloat32x4(FloatRegister reg, FloatRegister scratch)
{
    ScratchSimd128Scope scratch2(*this);

    MOZ_ASSERT(scratch.asSimd128() != scratch2.asSimd128());
    MOZ_ASSERT(reg.asSimd128() != scratch2.asSimd128());
    MOZ_ASSERT(reg.asSimd128() != scratch.asSimd128());

    FloatRegister mask = scratch;
    vcmpordps(Operand(reg), reg, mask);

    FloatRegister ifFalse = scratch2;
    float nanf = float(JS::GenericNaN());
    loadConstantSimd128Float(SimdConstant::SplatX4(nanf), ifFalse);

    bitwiseAndSimd128(Operand(mask), reg);
    bitwiseAndNotSimd128(Operand(ifFalse), mask);
    bitwiseOrSimd128(Operand(mask), reg);
}

// ========================================================================
// Memory access primitives.
void
MacroAssembler::storeUncanonicalizedDouble(FloatRegister src, const Address& dest)
{
    vmovsd(src, dest);
}
void
MacroAssembler::storeUncanonicalizedDouble(FloatRegister src, const BaseIndex& dest)
{
    vmovsd(src, dest);
}
void
MacroAssembler::storeUncanonicalizedDouble(FloatRegister src, const Operand& dest)
{
    switch (dest.kind()) {
      case Operand::MEM_REG_DISP:
        storeUncanonicalizedDouble(src, dest.toAddress());
        break;
      case Operand::MEM_SCALE:
        storeUncanonicalizedDouble(src, dest.toBaseIndex());
        break;
      default:
        MOZ_CRASH("unexpected operand kind");
    }
}

template void MacroAssembler::storeDouble(FloatRegister src, const Operand& dest);

void
MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src, const Address& dest)
{
    vmovss(src, dest);
}
void
MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src, const BaseIndex& dest)
{
    vmovss(src, dest);
}
void
MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src, const Operand& dest)
{
    switch (dest.kind()) {
      case Operand::MEM_REG_DISP:
        storeUncanonicalizedFloat32(src, dest.toAddress());
        break;
      case Operand::MEM_SCALE:
        storeUncanonicalizedFloat32(src, dest.toBaseIndex());
        break;
      default:
        MOZ_CRASH("unexpected operand kind");
    }
}

template void MacroAssembler::storeFloat32(FloatRegister src, const Operand& dest);

void
MacroAssembler::storeFloat32x3(FloatRegister src, const Address& dest)
{
    Address destZ(dest);
    destZ.offset += 2 * sizeof(int32_t);
    storeDouble(src, dest);
    ScratchSimd128Scope scratch(*this);
    vmovhlps(src, scratch, scratch);
    storeFloat32(scratch, destZ);
}
void
MacroAssembler::storeFloat32x3(FloatRegister src, const BaseIndex& dest)
{
    BaseIndex destZ(dest);
    destZ.offset += 2 * sizeof(int32_t);
    storeDouble(src, dest);
    ScratchSimd128Scope scratch(*this);
    vmovhlps(src, scratch, scratch);
    storeFloat32(scratch, destZ);
}

//}}} check_macroassembler_style
// ===============================================================

void
MacroAssemblerX86Shared::clampIntToUint8(Register reg)
{
    Label inRange;
    asMasm().branchTest32(Assembler::Zero, reg, Imm32(0xffffff00), &inRange);
    {
        sarl(Imm32(31), reg);
        notl(reg);
        andl(Imm32(255), reg);
    }
    bind(&inRange);
}

} // namespace jit
} // namespace js

#endif /* jit_x86_shared_MacroAssembler_x86_shared_inl_h */
