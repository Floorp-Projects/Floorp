/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_mips64_MacroAssembler_mips64_inl_h
#define jit_mips64_MacroAssembler_mips64_inl_h

#include "jit/mips64/MacroAssembler-mips64.h"

#include "jit/mips-shared/MacroAssembler-mips-shared-inl.h"

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

// ===============================================================
// Logical instructions

void
MacroAssembler::andPtr(Register src, Register dest)
{
    ma_and(dest, src);
}

void
MacroAssembler::andPtr(Imm32 imm, Register dest)
{
    ma_and(dest, imm);
}

void
MacroAssembler::and64(Imm64 imm, Register64 dest)
{
    ma_li(ScratchRegister, ImmWord(imm.value));
    ma_and(dest.reg, ScratchRegister);
}

void
MacroAssembler::or64(Imm64 imm, Register64 dest)
{
    ma_li(ScratchRegister, ImmWord(imm.value));
    ma_or(dest.reg, ScratchRegister);
}

void
MacroAssembler::xor64(Imm64 imm, Register64 dest)
{
    ma_li(ScratchRegister, ImmWord(imm.value));
    ma_xor(dest.reg, ScratchRegister);
}

void
MacroAssembler::orPtr(Register src, Register dest)
{
    ma_or(dest, src);
}

void
MacroAssembler::orPtr(Imm32 imm, Register dest)
{
    ma_or(dest, imm);
}

void
MacroAssembler::or64(Register64 src, Register64 dest)
{
    ma_or(dest.reg, src.reg);
}

void
MacroAssembler::xor64(Register64 src, Register64 dest)
{
    ma_xor(dest.reg, src.reg);
}

void
MacroAssembler::xorPtr(Register src, Register dest)
{
    ma_xor(dest, src);
}

void
MacroAssembler::xorPtr(Imm32 imm, Register dest)
{
    ma_xor(dest, imm);
}

// ===============================================================
// Arithmetic functions

void
MacroAssembler::addPtr(Register src, Register dest)
{
    ma_daddu(dest, src);
}

void
MacroAssembler::addPtr(Imm32 imm, Register dest)
{
    ma_daddu(dest, imm);
}

void
MacroAssembler::addPtr(ImmWord imm, Register dest)
{
    movePtr(imm, ScratchRegister);
    addPtr(ScratchRegister, dest);
}

void
MacroAssembler::add64(Register64 src, Register64 dest)
{
    addPtr(src.reg, dest.reg);
}

void
MacroAssembler::add64(Imm32 imm, Register64 dest)
{
    ma_daddu(dest.reg, imm);
}

void
MacroAssembler::subPtr(Register src, Register dest)
{
    as_dsubu(dest, dest, src);
}

void
MacroAssembler::subPtr(Imm32 imm, Register dest)
{
    ma_dsubu(dest, dest, imm);
}

void
MacroAssembler::mul64(Imm64 imm, const Register64& dest)
{
    MOZ_ASSERT(dest.reg != ScratchRegister);
    mov(ImmWord(imm.value), ScratchRegister);
    as_dmultu(dest.reg, ScratchRegister);
    as_mflo(dest.reg);
}

void
MacroAssembler::mulBy3(Register src, Register dest)
{
    as_daddu(dest, src, src);
    as_daddu(dest, dest, src);
}

void
MacroAssembler::inc64(AbsoluteAddress dest)
{
    ma_li(ScratchRegister, ImmWord(uintptr_t(dest.addr)));
    as_ld(SecondScratchReg, ScratchRegister, 0);
    as_daddiu(SecondScratchReg, SecondScratchReg, 1);
    as_sd(SecondScratchReg, ScratchRegister, 0);
}

// ===============================================================
// Shift functions

void
MacroAssembler::lshiftPtr(Imm32 imm, Register dest)
{
    ma_dsll(dest, dest, imm);
}

void
MacroAssembler::lshift64(Imm32 imm, Register64 dest)
{
    ma_dsll(dest.reg, dest.reg, imm);
}

void
MacroAssembler::rshiftPtr(Imm32 imm, Register dest)
{
    ma_dsrl(dest, dest, imm);
}

void
MacroAssembler::rshiftPtrArithmetic(Imm32 imm, Register dest)
{
    ma_dsra(dest, dest, imm);
}

void
MacroAssembler::rshift64(Imm32 imm, Register64 dest)
{
    ma_dsrl(dest.reg, dest.reg, imm);
}

// ===============================================================
// Branch functions

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
MacroAssembler::branchPrivatePtr(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    if (rhs != ScratchRegister)
        movePtr(rhs, ScratchRegister);
    // Instead of unboxing lhs, box rhs and do direct comparison with lhs.
    rshiftPtr(Imm32(1), ScratchRegister);
    branchPtr(cond, lhs, ScratchRegister, label);
}

void
MacroAssembler::branchTest64(Condition cond, Register64 lhs, Register64 rhs, Register temp,
                             Label* label)
{
    branchTestPtr(cond, lhs.reg, rhs.reg, label);
}

void
MacroAssembler::branchTestUndefined(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestUndefined(cond, scratch2, label);
}

void
MacroAssembler::branchTestInt32(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestInt32(cond, scratch2, label);
}

void
MacroAssembler::branchTestInt32Truthy(bool b, const ValueOperand& value, Label* label)
{
    ScratchRegisterScope scratch(*this);
    ma_dext(scratch, value.valueReg(), Imm32(0), Imm32(32));
    ma_b(scratch, scratch, label, b ? NonZero : Zero);
}

void
MacroAssembler::branchTestDouble(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    Condition actual = (cond == Equal) ? BelowOrEqual : Above;
    ma_b(tag, ImmTag(JSVAL_TAG_MAX_DOUBLE), label, actual);
}

void
MacroAssembler::branchTestDouble(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestDouble(cond, scratch2, label);
}

void
MacroAssembler::branchTestNumber(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestNumber(cond, scratch2, label);
}

void
MacroAssembler::branchTestBoolean(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestBoolean(cond, scratch2, label);
}

void
MacroAssembler::branchTestBooleanTruthy(bool b, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    unboxBoolean(value, scratch2);
    ma_b(scratch2, scratch2, label, b ? NonZero : Zero);
}

void
MacroAssembler::branchTestString(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestString(cond, scratch2, label);
}

void
MacroAssembler::branchTestStringTruthy(bool b, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    unboxString(value, scratch2);
    load32(Address(scratch2, JSString::offsetOfLength()), scratch2);
    ma_b(scratch2, Imm32(0), label, b ? NotEqual : Equal);
}

void
MacroAssembler::branchTestSymbol(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestSymbol(cond, scratch2, label);
}

void
MacroAssembler::branchTestNull(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestNull(cond, scratch2, label);
}

void
MacroAssembler::branchTestObject(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestObject(cond, scratch2, label);
}

void
MacroAssembler::branchTestPrimitive(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestPrimitive(cond, scratch2, label);
}

template <class L>
void
MacroAssembler::branchTestMagic(Condition cond, const ValueOperand& value, L label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    ma_b(scratch2, ImmTag(JSVAL_TAG_MAGIC), label, cond);
}

void
MacroAssembler::branchTestMagic(Condition cond, const Address& valaddr, JSWhyMagic why, Label* label)
{
    uint64_t magic = MagicValue(why).asRawBits();
    ScratchRegisterScope scratch(*this);
    loadPtr(valaddr, scratch);
    ma_b(scratch, ImmWord(magic), label, cond);
}

//}}} check_macroassembler_style
// ===============================================================

void
MacroAssemblerMIPS64Compat::incrementInt32Value(const Address& addr)
{
    asMasm().add32(Imm32(1), addr);
}

void
MacroAssemblerMIPS64Compat::computeEffectiveAddress(const BaseIndex& address, Register dest)
{
    computeScaledAddress(address, dest);
    if (address.offset)
        asMasm().addPtr(Imm32(address.offset), dest);
}

void
MacroAssemblerMIPS64Compat::retn(Imm32 n)
{
    // pc <- [sp]; sp += n
    loadPtr(Address(StackPointer, 0), ra);
    asMasm().addPtr(n, StackPointer);
    as_jr(ra);
    as_nop();
}

} // namespace jit
} // namespace js

#endif /* jit_mips64_MacroAssembler_mips64_inl_h */
