/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_mips32_MacroAssembler_mips32_inl_h
#define jit_mips32_MacroAssembler_mips32_inl_h

#include "jit/mips32/MacroAssembler-mips32.h"

#include "jit/mips-shared/MacroAssembler-mips-shared-inl.h"

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
    and32(Imm32(imm.value & LOW_32_MASK), dest.low);
    and32(Imm32((imm.value >> 32) & LOW_32_MASK), dest.high);
}

void
MacroAssembler::or64(Imm64 imm, Register64 dest)
{
    or32(Imm32(imm.value & LOW_32_MASK), dest.low);
    or32(Imm32((imm.value >> 32) & LOW_32_MASK), dest.high);
}

void
MacroAssembler::xor64(Imm64 imm, Register64 dest)
{
    xor32(Imm32(imm.value & LOW_32_MASK), dest.low);
    xor32(Imm32((imm.value >> 32) & LOW_32_MASK), dest.high);
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
    or32(src.low, dest.low);
    or32(src.high, dest.high);
}

void
MacroAssembler::xor64(Register64 src, Register64 dest)
{
    ma_xor(dest.low, src.low);
    ma_xor(dest.high, src.high);
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
    ma_addu(dest, src);
}

void
MacroAssembler::addPtr(Imm32 imm, Register dest)
{
    ma_addu(dest, imm);
}

void
MacroAssembler::addPtr(ImmWord imm, Register dest)
{
    addPtr(Imm32(imm.value), dest);
}

void
MacroAssembler::add64(Register64 src, Register64 dest)
{
    as_addu(dest.low, dest.low, src.low);
    as_sltu(ScratchRegister, dest.low, src.low);
    as_addu(dest.high, dest.high, src.high);
    as_addu(dest.high, dest.high, ScratchRegister);
}

void
MacroAssembler::add64(Imm32 imm, Register64 dest)
{
    as_addiu(dest.low, dest.low, imm.value);
    as_sltiu(ScratchRegister, dest.low, imm.value);
    as_addu(dest.high, dest.high, ScratchRegister);
}

void
MacroAssembler::subPtr(Register src, Register dest)
{
    as_subu(dest, dest, src);
}

void
MacroAssembler::subPtr(Imm32 imm, Register dest)
{
    ma_subu(dest, dest, imm);
}

void
MacroAssembler::mul64(Imm64 imm, const Register64& dest)
{
    // LOW32  = LOW(LOW(dest) * LOW(imm));
    // HIGH32 = LOW(HIGH(dest) * LOW(imm)) [multiply imm into upper bits]
    //        + LOW(LOW(dest) * HIGH(imm)) [multiply dest into upper bits]
    //        + HIGH(LOW(dest) * LOW(imm)) [carry]

    // HIGH(dest) = LOW(HIGH(dest) * LOW(imm));
    ma_li(ScratchRegister, Imm32(imm.value & LOW_32_MASK));
    as_multu(dest.high, ScratchRegister);
    as_mflo(dest.high);

    // mfhi:mflo = LOW(dest) * LOW(imm);
    as_multu(dest.low, ScratchRegister);

    // HIGH(dest) += mfhi;
    as_mfhi(ScratchRegister);
    as_addu(dest.high, dest.high, ScratchRegister);

    if (((imm.value >> 32) & LOW_32_MASK) == 5) {
        // Optimized case for Math.random().

        // HIGH(dest) += LOW(LOW(dest) * HIGH(imm));
        as_sll(ScratchRegister, dest.low, 2);
        as_addu(ScratchRegister, ScratchRegister, dest.low);
        as_addu(dest.high, dest.high, ScratchRegister);

        // LOW(dest) = mflo;
        as_mflo(dest.low);
    } else {
        // tmp = mflo
        as_mflo(SecondScratchReg);

        // HIGH(dest) += LOW(LOW(dest) * HIGH(imm));
        ma_li(ScratchRegister, Imm32((imm.value >> 32) & LOW_32_MASK));
        as_multu(dest.low, ScratchRegister);
        as_mflo(ScratchRegister);
        as_addu(dest.high, dest.high, ScratchRegister);

        // LOW(dest) = tmp;
        ma_move(dest.low, SecondScratchReg);
    }
}

void
MacroAssembler::mulBy3(Register src, Register dest)
{
    as_addu(dest, src, src);
    as_addu(dest, dest, src);
}

void
MacroAssembler::inc64(AbsoluteAddress dest)
{
    ma_li(ScratchRegister, Imm32((int32_t)dest.addr));
    as_lw(SecondScratchReg, ScratchRegister, 0);

    as_addiu(SecondScratchReg, SecondScratchReg, 1);
    as_sw(SecondScratchReg, ScratchRegister, 0);

    as_sltiu(SecondScratchReg, SecondScratchReg, 1);
    as_lw(ScratchRegister, ScratchRegister, 4);

    as_addu(SecondScratchReg, ScratchRegister, SecondScratchReg);

    ma_li(ScratchRegister, Imm32((int32_t)dest.addr));
    as_sw(SecondScratchReg, ScratchRegister, 4);
}

// ===============================================================
// Shift functions

void
MacroAssembler::lshiftPtr(Imm32 imm, Register dest)
{
    ma_sll(dest, dest, imm);
}

void
MacroAssembler::lshift64(Imm32 imm, Register64 dest)
{
    ScratchRegisterScope scratch(*this);
    as_sll(dest.high, dest.high, imm.value);
    as_srl(scratch, dest.low, 32 - imm.value);
    as_or(dest.high, dest.high, scratch);
    as_sll(dest.low, dest.low, imm.value);
}

void
MacroAssembler::rshiftPtr(Imm32 imm, Register dest)
{
    ma_srl(dest, dest, imm);
}

void
MacroAssembler::rshiftPtrArithmetic(Imm32 imm, Register dest)
{
    ma_sra(dest, dest, imm);
}

void
MacroAssembler::rshift64(Imm32 imm, Register64 dest)
{
    ScratchRegisterScope scratch(*this);
    as_srl(dest.low, dest.low, imm.value);
    as_sll(scratch, dest.high, 32 - imm.value);
    as_or(dest.low, dest.low, scratch);
    as_srl(dest.high, dest.high, imm.value);
}

// ===============================================================
// Branch functions

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
MacroAssembler::branchPrivatePtr(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    branchPtr(cond, lhs, rhs, label);
}

void
MacroAssembler::branchTest64(Condition cond, Register64 lhs, Register64 rhs, Register temp,
                             Label* label)
{
    if (cond == Assembler::Zero) {
        MOZ_ASSERT(lhs.low == rhs.low);
        MOZ_ASSERT(lhs.high == rhs.high);
        as_or(ScratchRegister, lhs.low, lhs.high);
        branchTestPtr(cond, ScratchRegister, ScratchRegister, label);
    } else {
        MOZ_CRASH("Unsupported condition");
    }
}

void
MacroAssembler::branchTestUndefined(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestUndefined(cond, value.typeReg(), label);
}

void
MacroAssembler::branchTestInt32(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestInt32(cond, value.typeReg(), label);
}

void
MacroAssembler::branchTestInt32Truthy(bool b, const ValueOperand& value, Label* label)
{
    ScratchRegisterScope scratch(*this);
    as_and(scratch, value.payloadReg(), value.payloadReg());
    ma_b(scratch, scratch, label, b ? NonZero : Zero);
}

void
MacroAssembler::branchTestDouble(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    Condition actual = (cond == Equal) ? Below : AboveOrEqual;
    ma_b(tag, ImmTag(JSVAL_TAG_CLEAR), label, actual);
}

void
MacroAssembler::branchTestDouble(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestDouble(cond, value.typeReg(), label);
}

void
MacroAssembler::branchTestNumber(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestNumber(cond, value.typeReg(), label);
}

void
MacroAssembler::branchTestBoolean(Condition cond, const ValueOperand& value, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_b(value.typeReg(), ImmType(JSVAL_TYPE_BOOLEAN), label, cond);
}

void
MacroAssembler::branchTestBooleanTruthy(bool b, const ValueOperand& value, Label* label)
{
    ma_b(value.payloadReg(), value.payloadReg(), label, b ? NonZero : Zero);
}

void
MacroAssembler::branchTestString(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestString(cond, value.typeReg(), label);
}

void
MacroAssembler::branchTestStringTruthy(bool b, const ValueOperand& value, Label* label)
{
    Register string = value.payloadReg();
    SecondScratchRegisterScope scratch2(*this);
    ma_lw(scratch2, Address(string, JSString::offsetOfLength()));
    ma_b(scratch2, Imm32(0), label, b ? NotEqual : Equal);
}

void
MacroAssembler::branchTestSymbol(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestSymbol(cond, value.typeReg(), label);
}

void
MacroAssembler::branchTestNull(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestNull(cond, value.typeReg(), label);
}

void
MacroAssembler::branchTestObject(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestObject(cond, value.typeReg(), label);
}

void
MacroAssembler::branchTestPrimitive(Condition cond, const ValueOperand& value, Label* label)
{
    branchTestPrimitive(cond, value.typeReg(), label);
}

template <class L>
void
MacroAssembler::branchTestMagic(Condition cond, const ValueOperand& value, L label)
{
    ma_b(value.typeReg(), ImmTag(JSVAL_TAG_MAGIC), label, cond);
}

void
MacroAssembler::branchTestMagic(Condition cond, const Address& valaddr, JSWhyMagic why, Label* label)
{
    branchTestMagic(cond, valaddr, label);
    branch32(cond, ToPayload(valaddr), Imm32(why), label);
}

//}}} check_macroassembler_style
// ===============================================================

void
MacroAssemblerMIPSCompat::incrementInt32Value(const Address& addr)
{
    asMasm().add32(Imm32(1), ToPayload(addr));
}

void
MacroAssemblerMIPSCompat::computeEffectiveAddress(const BaseIndex& address, Register dest)
{
    computeScaledAddress(address, dest);
    if (address.offset)
        asMasm().addPtr(Imm32(address.offset), dest);
}

void
MacroAssemblerMIPSCompat::retn(Imm32 n) {
    // pc <- [sp]; sp += n
    loadPtr(Address(StackPointer, 0), ra);
    asMasm().addPtr(n, StackPointer);
    as_jr(ra);
    as_nop();
}

} // namespace jit
} // namespace js

#endif /* jit_mips32_MacroAssembler_mips32_inl_h */
