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
// Branch functions

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

void
MacroAssembler::branchPrivatePtr(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    branchPtr(cond, lhs, rhs, label);
}

//}}} check_macroassembler_style
// ===============================================================

template <typename T>
void
MacroAssemblerARMCompat::branchAdd32(Condition cond, T src, Register dest, Label* label)
{
    asMasm().add32(src, dest);
    j(cond, label);
}

void
MacroAssemblerARMCompat::incrementInt32Value(const Address& addr)
{
    asMasm().add32(Imm32(1), ToPayload(addr));
}

} // namespace jit
} // namespace js

#endif /* jit_arm_MacroAssembler_arm_inl_h */
