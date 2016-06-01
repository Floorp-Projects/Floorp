/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_mips_shared_MacroAssembler_mips_shared_inl_h
#define jit_mips_shared_MacroAssembler_mips_shared_inl_h

#include "jit/mips-shared/MacroAssembler-mips-shared.h"

namespace js {
namespace jit {

//{{{ check_macroassembler_style
// ===============================================================
// Logical instructions

void
MacroAssembler::not32(Register reg)
{
    ma_not(reg, reg);
}

void
MacroAssembler::and32(Register src, Register dest)
{
    as_and(dest, dest, src);
}

void
MacroAssembler::and32(Imm32 imm, Register dest)
{
    ma_and(dest, imm);
}

void
MacroAssembler::and32(Imm32 imm, const Address& dest)
{
    load32(dest, SecondScratchReg);
    ma_and(SecondScratchReg, imm);
    store32(SecondScratchReg, dest);
}

void
MacroAssembler::and32(const Address& src, Register dest)
{
    load32(src, SecondScratchReg);
    ma_and(dest, SecondScratchReg);
}

void
MacroAssembler::or32(Register src, Register dest)
{
    ma_or(dest, src);
}

void
MacroAssembler::or32(Imm32 imm, Register dest)
{
    ma_or(dest, imm);
}

void
MacroAssembler::or32(Imm32 imm, const Address& dest)
{
    load32(dest, SecondScratchReg);
    ma_or(SecondScratchReg, imm);
    store32(SecondScratchReg, dest);
}

void
MacroAssembler::xor32(Imm32 imm, Register dest)
{
    ma_xor(dest, imm);
}

// ===============================================================
// Arithmetic instructions

void
MacroAssembler::add32(Register src, Register dest)
{
    as_addu(dest, dest, src);
}

void
MacroAssembler::add32(Imm32 imm, Register dest)
{
    ma_addu(dest, dest, imm);
}

void
MacroAssembler::add32(Imm32 imm, const Address& dest)
{
    load32(dest, SecondScratchReg);
    ma_addu(SecondScratchReg, imm);
    store32(SecondScratchReg, dest);
}

void
MacroAssembler::addPtr(Imm32 imm, const Address& dest)
{
    loadPtr(dest, ScratchRegister);
    addPtr(imm, ScratchRegister);
    storePtr(ScratchRegister, dest);
}

void
MacroAssembler::addPtr(const Address& src, Register dest)
{
    loadPtr(src, ScratchRegister);
    addPtr(ScratchRegister, dest);
}

void
MacroAssembler::addDouble(FloatRegister src, FloatRegister dest)
{
    as_addd(dest, dest, src);
}

void
MacroAssembler::sub32(Register src, Register dest)
{
    as_subu(dest, dest, src);
}

void
MacroAssembler::sub32(Imm32 imm, Register dest)
{
    ma_subu(dest, dest, imm);
}

void
MacroAssembler::sub32(const Address& src, Register dest)
{
    load32(src, SecondScratchReg);
    as_subu(dest, dest, SecondScratchReg);
}

void
MacroAssembler::subPtr(Register src, const Address& dest)
{
    loadPtr(dest, SecondScratchReg);
    subPtr(src, SecondScratchReg);
    storePtr(SecondScratchReg, dest);
}

void
MacroAssembler::subPtr(const Address& addr, Register dest)
{
    loadPtr(addr, SecondScratchReg);
    subPtr(SecondScratchReg, dest);
}

void
MacroAssembler::subDouble(FloatRegister src, FloatRegister dest)
{
    as_subd(dest, dest, src);
}

void
MacroAssembler::mulDouble(FloatRegister src, FloatRegister dest)
{
    as_muld(dest, dest, src);
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
    as_divd(dest, dest, src);
}

void
MacroAssembler::neg32(Register reg)
{
    ma_negu(reg, reg);
}

void
MacroAssembler::negateDouble(FloatRegister reg)
{
    as_negd(reg, reg);
}

// ===============================================================
// Rotation functions
void
MacroAssembler::rotateLeft(Imm32 count, Register input, Register dest)
{
    if (count.value)
        ma_rol(dest, input, count);
    else
        ma_move(dest, input);
}
void
MacroAssembler::rotateLeft(Register count, Register input, Register dest)
{
    ma_rol(dest, input, count);
}
void
MacroAssembler::rotateRight(Imm32 count, Register input, Register dest)
{
    if (count.value)
        ma_ror(dest, input, count);
    else
        ma_move(dest, input);
}
void
MacroAssembler::rotateRight(Register count, Register input, Register dest)
{
    ma_ror(dest, input, count);
}

// ===============================================================
// Branch functions

void
MacroAssembler::branch32(Condition cond, Register lhs, Register rhs, Label* label)
{
    ma_b(lhs, rhs, label, cond);
}

template <class L>
void
MacroAssembler::branch32(Condition cond, Register lhs, Imm32 imm, L label)
{
    ma_b(lhs, imm, label, cond);
}

void
MacroAssembler::branch32(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    ma_b(SecondScratchReg, rhs, label, cond);
}

void
MacroAssembler::branch32(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    ma_b(SecondScratchReg, rhs, label, cond);
}

void
MacroAssembler::branch32(Condition cond, const AbsoluteAddress& lhs, Register rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    ma_b(SecondScratchReg, rhs, label, cond);
}

void
MacroAssembler::branch32(Condition cond, const AbsoluteAddress& lhs, Imm32 rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    ma_b(SecondScratchReg, rhs, label, cond);
}

void
MacroAssembler::branch32(Condition cond, const BaseIndex& lhs, Imm32 rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    ma_b(SecondScratchReg, rhs, label, cond);
}

void
MacroAssembler::branch32(Condition cond, wasm::SymbolicAddress addr, Imm32 imm, Label* label)
{
    load32(addr, SecondScratchReg);
    ma_b(SecondScratchReg, imm, label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, Register rhs, Label* label)
{
    ma_b(lhs, rhs, label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    ma_b(lhs, rhs, label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmPtr rhs, Label* label)
{
    ma_b(lhs, rhs, label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmGCPtr rhs, Label* label)
{
    ma_b(lhs, rhs, label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmWord rhs, Label* label)
{
    ma_b(lhs, rhs, label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmPtr rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmGCPtr rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmWord rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const AbsoluteAddress& lhs, Register rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const AbsoluteAddress& lhs, ImmWord rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, wasm::SymbolicAddress lhs, Register rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

template <typename T>
CodeOffsetJump
MacroAssembler::branchPtrWithPatch(Condition cond, Register lhs, T rhs, RepatchLabel* label)
{
    movePtr(rhs, ScratchRegister);
    Label skipJump;
    ma_b(lhs, ScratchRegister, &skipJump, InvertCondition(cond), ShortJump);
    CodeOffsetJump off = jumpWithPatch(label);
    bind(&skipJump);
    return off;
}

template <typename T>
CodeOffsetJump
MacroAssembler::branchPtrWithPatch(Condition cond, Address lhs, T rhs, RepatchLabel* label)
{
    loadPtr(lhs, SecondScratchReg);
    movePtr(rhs, ScratchRegister);
    Label skipJump;
    ma_b(SecondScratchReg, ScratchRegister, &skipJump, InvertCondition(cond), ShortJump);
    CodeOffsetJump off = jumpWithPatch(label);
    bind(&skipJump);
    return off;
}

void
MacroAssembler::branchFloat(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                            Label* label)
{
    ma_bc1s(lhs, rhs, label, cond);
}

void
MacroAssembler::branchTruncateFloat32(FloatRegister src, Register dest, Label* fail)
{
    Label test, success;
    as_truncws(ScratchFloat32Reg, src);
    as_mfc1(dest, ScratchFloat32Reg);

    ma_b(dest, Imm32(INT32_MAX), fail, Assembler::Equal);
}

void
MacroAssembler::branchDouble(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                             Label* label)
{
    ma_bc1d(lhs, rhs, label, cond);
}

// Convert the floating point value to an integer, if it did not fit, then it
// was clamped to INT32_MIN/INT32_MAX, and we can test it.
// NOTE: if the value really was supposed to be INT32_MAX / INT32_MIN then it
// will be wrong.
void
MacroAssembler::branchTruncateDouble(FloatRegister src, Register dest, Label* fail)
{
    Label test, success;
    as_truncwd(ScratchDoubleReg, src);
    as_mfc1(dest, ScratchDoubleReg);

    ma_b(dest, Imm32(INT32_MAX), fail, Assembler::Equal);
    ma_b(dest, Imm32(INT32_MIN), fail, Assembler::Equal);
}

template <typename T>
void
MacroAssembler::branchAdd32(Condition cond, T src, Register dest, Label* overflow)
{
    switch (cond) {
      case Overflow:
        ma_addTestOverflow(dest, dest, src, overflow);
        break;
      default:
        MOZ_CRASH("NYI");
    }
}

template <typename T>
void
MacroAssembler::branchSub32(Condition cond, T src, Register dest, Label* overflow)
{
    switch (cond) {
      case Overflow:
        ma_subTestOverflow(dest, dest, src, overflow);
        break;
      case NonZero:
      case Zero:
        ma_subu(dest, src);
        ma_b(dest, dest, overflow, cond);
        break;
      default:
        MOZ_CRASH("NYI");
    }
}

void
MacroAssembler::decBranchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    subPtr(rhs, lhs);
    branchPtr(cond, lhs, Imm32(0), label);
}

template <class L>
void
MacroAssembler::branchTest32(Condition cond, Register lhs, Register rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    if (lhs == rhs) {
        ma_b(lhs, rhs, label, cond);
    } else {
        as_and(ScratchRegister, lhs, rhs);
        ma_b(ScratchRegister, ScratchRegister, label, cond);
    }
}

template <class L>
void
MacroAssembler::branchTest32(Condition cond, Register lhs, Imm32 rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    ma_and(ScratchRegister, lhs, rhs);
    ma_b(ScratchRegister, ScratchRegister, label, cond);
}

void
MacroAssembler::branchTest32(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    branchTest32(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchTest32(Condition cond, const AbsoluteAddress& lhs, Imm32 rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    branchTest32(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Register rhs, Label* label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    if (lhs == rhs) {
        ma_b(lhs, rhs, label, cond);
    } else {
        as_and(ScratchRegister, lhs, rhs);
        ma_b(ScratchRegister, ScratchRegister, label, cond);
    }
}

void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    ma_and(ScratchRegister, lhs, rhs);
    ma_b(ScratchRegister, ScratchRegister, label, cond);
}

void
MacroAssembler::branchTestPtr(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchTestPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchTestUndefined(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_b(tag, ImmTag(JSVAL_TAG_UNDEFINED), label, cond);
}

void
MacroAssembler::branchTestUndefined(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestUndefined(cond, scratch2, label);
}

void
MacroAssembler::branchTestUndefined(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestUndefined(cond, scratch2, label);
}

void
MacroAssembler::branchTestInt32(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_b(tag, ImmTag(JSVAL_TAG_INT32), label, cond);
}

void
MacroAssembler::branchTestInt32(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestInt32(cond, scratch2, label);
}

void
MacroAssembler::branchTestInt32(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestInt32(cond, scratch2, label);
}

void
MacroAssembler::branchTestDouble(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestDouble(cond, scratch2, label);
}

void
MacroAssembler::branchTestDouble(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestDouble(cond, scratch2, label);
}

void
MacroAssembler::branchTestDoubleTruthy(bool b, FloatRegister value, Label* label)
{
    ma_lid(ScratchDoubleReg, 0.0);
    DoubleCondition cond = b ? DoubleNotEqual : DoubleEqualOrUnordered;
    ma_bc1d(value, ScratchDoubleReg, label, cond);
}

void
MacroAssembler::branchTestNumber(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    Condition actual = cond == Equal ? BelowOrEqual : Above;
    ma_b(tag, ImmTag(JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET), label, actual);
}

void
MacroAssembler::branchTestBoolean(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_b(tag, ImmTag(JSVAL_TAG_BOOLEAN), label, cond);
}

void
MacroAssembler::branchTestBoolean(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestBoolean(cond, scratch2, label);
}

void
MacroAssembler::branchTestBoolean(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestBoolean(cond, scratch2, label);
}

void
MacroAssembler::branchTestString(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_b(tag, ImmTag(JSVAL_TAG_STRING), label, cond);
}

void
MacroAssembler::branchTestString(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestString(cond, scratch2, label);
}

void
MacroAssembler::branchTestSymbol(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_b(tag, ImmTag(JSVAL_TAG_SYMBOL), label, cond);
}

void
MacroAssembler::branchTestSymbol(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestSymbol(cond, scratch2, label);
}

void
MacroAssembler::branchTestNull(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_b(tag, ImmTag(JSVAL_TAG_NULL), label, cond);
}

void
MacroAssembler::branchTestNull(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestNull(cond, scratch2, label);
}

void
MacroAssembler::branchTestNull(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestNull(cond, scratch2, label);
}

void
MacroAssembler::branchTestObject(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_b(tag, ImmTag(JSVAL_TAG_OBJECT), label, cond);
}

void
MacroAssembler::branchTestObject(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestObject(cond, scratch2, label);
}

void
MacroAssembler::branchTestObject(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestObject(cond, scratch2, label);
}

void
MacroAssembler::branchTestGCThing(Condition cond, const Address& address, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    ma_b(scratch2, ImmTag(JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET), label,
         (cond == Equal) ? AboveOrEqual : Below);
}
void
MacroAssembler::branchTestGCThing(Condition cond, const BaseIndex& address, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    ma_b(scratch2, ImmTag(JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET), label,
         (cond == Equal) ? AboveOrEqual : Below);
}

void
MacroAssembler::branchTestPrimitive(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_b(tag, ImmTag(JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET), label,
         (cond == Equal) ? Below : AboveOrEqual);
}

void
MacroAssembler::branchTestMagic(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_b(tag, ImmTag(JSVAL_TAG_MAGIC), label, cond);
}

void
MacroAssembler::branchTestMagic(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestMagic(cond, scratch2, label);
}

void
MacroAssembler::branchTestMagic(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestMagic(cond, scratch2, label);
}

// ========================================================================
// Memory access primitives.
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

} // namespace jit
} // namespace js

#endif /* jit_mips_shared_MacroAssembler_mips_shared_inl_h */
