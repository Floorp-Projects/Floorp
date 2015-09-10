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
MacroAssembler::xor32(Imm32 imm, Register dest)
{
    ma_eor(imm, dest, SetCC);
}

//}}} check_macroassembler_style
// ===============================================================

void
MacroAssemblerARMCompat::branchTest64(Condition cond, Register64 lhs, Register64 rhs, Register temp, Label* label)
{
    if (cond == Assembler::Zero) {
        MOZ_ASSERT(lhs.low == rhs.low);
        MOZ_ASSERT(lhs.high == rhs.high);
        mov(lhs.low, ScratchRegister);
        asMasm().or32(lhs.high, ScratchRegister);
        branchTestPtr(cond, ScratchRegister, ScratchRegister, label);
    } else {
        MOZ_CRASH("Unsupported condition");
    }
}

void
MacroAssemblerARMCompat::and64(Imm64 imm, Register64 dest)
{
    asMasm().and32(Imm32(imm.value & 0xFFFFFFFFL), dest.low);
    asMasm().and32(Imm32((imm.value >> 32) & 0xFFFFFFFFL), dest.high);
}

void
MacroAssemblerARMCompat::or64(Register64 src, Register64 dest)
{
    asMasm().or32(src.low, dest.low);
    asMasm().or32(src.high, dest.high);
}

} // namespace jit
} // namespace js

#endif /* jit_arm_MacroAssembler_arm_inl_h */
