/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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
// Move instructions

void MacroAssembler::moveFloat32ToGPR(FloatRegister src, Register dest) {
  vmovd(src, dest);
}

void MacroAssembler::moveGPRToFloat32(Register src, FloatRegister dest) {
  vmovd(src, dest);
}

void MacroAssembler::move8SignExtend(Register src, Register dest) {
  movsbl(src, dest);
}

void MacroAssembler::move16SignExtend(Register src, Register dest) {
  movswl(src, dest);
}

void MacroAssembler::loadAbiReturnAddress(Register dest) {
  loadPtr(Address(getStackPointer(), 0), dest);
}

// ===============================================================
// Logical instructions

void MacroAssembler::not32(Register reg) { notl(reg); }

void MacroAssembler::and32(Register src, Register dest) { andl(src, dest); }

void MacroAssembler::and32(Imm32 imm, Register dest) { andl(imm, dest); }

void MacroAssembler::and32(Imm32 imm, const Address& dest) {
  andl(imm, Operand(dest));
}

void MacroAssembler::and32(const Address& src, Register dest) {
  andl(Operand(src), dest);
}

void MacroAssembler::or32(Register src, Register dest) { orl(src, dest); }

void MacroAssembler::or32(Imm32 imm, Register dest) { orl(imm, dest); }

void MacroAssembler::or32(Imm32 imm, const Address& dest) {
  orl(imm, Operand(dest));
}

void MacroAssembler::xor32(Register src, Register dest) { xorl(src, dest); }

void MacroAssembler::xor32(Imm32 imm, Register dest) { xorl(imm, dest); }

void MacroAssembler::clz32(Register src, Register dest, bool knownNotZero) {
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

void MacroAssembler::ctz32(Register src, Register dest, bool knownNotZero) {
  bsfl(src, dest);
  if (!knownNotZero) {
    Label nonzero;
    j(Assembler::NonZero, &nonzero);
    movl(Imm32(32), dest);
    bind(&nonzero);
  }
}

void MacroAssembler::popcnt32(Register input, Register output, Register tmp) {
  if (AssemblerX86Shared::HasPOPCNT()) {
    popcntl(input, output);
    return;
  }

  MOZ_ASSERT(tmp != InvalidReg);

  // Equivalent to mozilla::CountPopulation32()

  movl(input, tmp);
  if (input != output) {
    movl(input, output);
  }
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

void MacroAssembler::add32(Register src, Register dest) { addl(src, dest); }

void MacroAssembler::add32(Imm32 imm, Register dest) { addl(imm, dest); }

void MacroAssembler::add32(Imm32 imm, const Address& dest) {
  addl(imm, Operand(dest));
}

void MacroAssembler::add32(Imm32 imm, const AbsoluteAddress& dest) {
  addl(imm, Operand(dest));
}

void MacroAssembler::addFloat32(FloatRegister src, FloatRegister dest) {
  vaddss(src, dest, dest);
}

void MacroAssembler::addDouble(FloatRegister src, FloatRegister dest) {
  vaddsd(src, dest, dest);
}

void MacroAssembler::sub32(Register src, Register dest) { subl(src, dest); }

void MacroAssembler::sub32(Imm32 imm, Register dest) { subl(imm, dest); }

void MacroAssembler::sub32(const Address& src, Register dest) {
  subl(Operand(src), dest);
}

void MacroAssembler::subDouble(FloatRegister src, FloatRegister dest) {
  vsubsd(src, dest, dest);
}

void MacroAssembler::subFloat32(FloatRegister src, FloatRegister dest) {
  vsubss(src, dest, dest);
}

void MacroAssembler::mul32(Register rhs, Register srcDest) {
  imull(rhs, srcDest);
}

void MacroAssembler::mulFloat32(FloatRegister src, FloatRegister dest) {
  vmulss(src, dest, dest);
}

void MacroAssembler::mulDouble(FloatRegister src, FloatRegister dest) {
  vmulsd(src, dest, dest);
}

void MacroAssembler::quotient32(Register rhs, Register srcDest,
                                bool isUnsigned) {
  MOZ_ASSERT(srcDest == eax);

  // Sign extend eax into edx to make (edx:eax): idiv/udiv are 64-bit.
  if (isUnsigned) {
    mov(ImmWord(0), edx);
    udiv(rhs);
  } else {
    cdq();
    idiv(rhs);
  }
}

void MacroAssembler::remainder32(Register rhs, Register srcDest,
                                 bool isUnsigned) {
  MOZ_ASSERT(srcDest == eax);

  // Sign extend eax into edx to make (edx:eax): idiv/udiv are 64-bit.
  if (isUnsigned) {
    mov(ImmWord(0), edx);
    udiv(rhs);
  } else {
    cdq();
    idiv(rhs);
  }
  mov(edx, eax);
}

void MacroAssembler::divFloat32(FloatRegister src, FloatRegister dest) {
  vdivss(src, dest, dest);
}

void MacroAssembler::divDouble(FloatRegister src, FloatRegister dest) {
  vdivsd(src, dest, dest);
}

void MacroAssembler::neg32(Register reg) { negl(reg); }

void MacroAssembler::negateFloat(FloatRegister reg) {
  ScratchFloat32Scope scratch(*this);
  vpcmpeqw(Operand(scratch), scratch, scratch);
  vpsllq(Imm32(31), scratch, scratch);

  // XOR the float in a float register with -0.0.
  vxorps(scratch, reg, reg);  // s ^ 0x80000000
}

void MacroAssembler::negateDouble(FloatRegister reg) {
  // From MacroAssemblerX86Shared::maybeInlineDouble
  ScratchDoubleScope scratch(*this);
  vpcmpeqw(Operand(scratch), scratch, scratch);
  vpsllq(Imm32(63), scratch, scratch);

  // XOR the float in a float register with -0.0.
  vxorpd(scratch, reg, reg);  // s ^ 0x80000000000000
}

void MacroAssembler::absFloat32(FloatRegister src, FloatRegister dest) {
  ScratchFloat32Scope scratch(*this);
  loadConstantFloat32(mozilla::SpecificNaN<float>(
                          0, mozilla::FloatingPoint<float>::kSignificandBits),
                      scratch);
  vandps(scratch, src, dest);
}

void MacroAssembler::absDouble(FloatRegister src, FloatRegister dest) {
  ScratchDoubleScope scratch(*this);
  loadConstantDouble(mozilla::SpecificNaN<double>(
                         0, mozilla::FloatingPoint<double>::kSignificandBits),
                     scratch);
  vandpd(scratch, src, dest);
}

void MacroAssembler::sqrtFloat32(FloatRegister src, FloatRegister dest) {
  vsqrtss(src, src, dest);
}

void MacroAssembler::sqrtDouble(FloatRegister src, FloatRegister dest) {
  vsqrtsd(src, src, dest);
}

void MacroAssembler::minFloat32(FloatRegister other, FloatRegister srcDest,
                                bool handleNaN) {
  minMaxFloat32(srcDest, other, handleNaN, false);
}

void MacroAssembler::minDouble(FloatRegister other, FloatRegister srcDest,
                               bool handleNaN) {
  minMaxDouble(srcDest, other, handleNaN, false);
}

void MacroAssembler::maxFloat32(FloatRegister other, FloatRegister srcDest,
                                bool handleNaN) {
  minMaxFloat32(srcDest, other, handleNaN, true);
}

void MacroAssembler::maxDouble(FloatRegister other, FloatRegister srcDest,
                               bool handleNaN) {
  minMaxDouble(srcDest, other, handleNaN, true);
}

// ===============================================================
// Rotation instructions
void MacroAssembler::rotateLeft(Imm32 count, Register input, Register dest) {
  MOZ_ASSERT(input == dest, "defineReuseInput");
  count.value &= 0x1f;
  if (count.value) {
    roll(count, input);
  }
}

void MacroAssembler::rotateLeft(Register count, Register input, Register dest) {
  MOZ_ASSERT(input == dest, "defineReuseInput");
  MOZ_ASSERT(count == ecx, "defineFixed(ecx)");
  roll_cl(input);
}

void MacroAssembler::rotateRight(Imm32 count, Register input, Register dest) {
  MOZ_ASSERT(input == dest, "defineReuseInput");
  count.value &= 0x1f;
  if (count.value) {
    rorl(count, input);
  }
}

void MacroAssembler::rotateRight(Register count, Register input,
                                 Register dest) {
  MOZ_ASSERT(input == dest, "defineReuseInput");
  MOZ_ASSERT(count == ecx, "defineFixed(ecx)");
  rorl_cl(input);
}

// ===============================================================
// Shift instructions

void MacroAssembler::lshift32(Register shift, Register srcDest) {
  MOZ_ASSERT(shift == ecx);
  shll_cl(srcDest);
}

void MacroAssembler::flexibleLshift32(Register shift, Register srcDest) {
  if (shift == ecx) {
    shll_cl(srcDest);
  } else {
    // Shift amount must be in ecx.
    xchg(shift, ecx);
    shll_cl(shift == srcDest ? ecx : srcDest == ecx ? shift : srcDest);
    xchg(shift, ecx);
  }
}

void MacroAssembler::rshift32(Register shift, Register srcDest) {
  MOZ_ASSERT(shift == ecx);
  shrl_cl(srcDest);
}

void MacroAssembler::flexibleRshift32(Register shift, Register srcDest) {
  if (shift == ecx) {
    shrl_cl(srcDest);
  } else {
    // Shift amount must be in ecx.
    xchg(shift, ecx);
    shrl_cl(shift == srcDest ? ecx : srcDest == ecx ? shift : srcDest);
    xchg(shift, ecx);
  }
}

void MacroAssembler::rshift32Arithmetic(Register shift, Register srcDest) {
  MOZ_ASSERT(shift == ecx);
  sarl_cl(srcDest);
}

void MacroAssembler::flexibleRshift32Arithmetic(Register shift,
                                                Register srcDest) {
  if (shift == ecx) {
    sarl_cl(srcDest);
  } else {
    // Shift amount must be in ecx.
    xchg(shift, ecx);
    sarl_cl(shift == srcDest ? ecx : srcDest == ecx ? shift : srcDest);
    xchg(shift, ecx);
  }
}

void MacroAssembler::lshift32(Imm32 shift, Register srcDest) {
  shll(shift, srcDest);
}

void MacroAssembler::rshift32(Imm32 shift, Register srcDest) {
  shrl(shift, srcDest);
}

void MacroAssembler::rshift32Arithmetic(Imm32 shift, Register srcDest) {
  sarl(shift, srcDest);
}

// ===============================================================
// Condition functions

template <typename T1, typename T2>
void MacroAssembler::cmp32Set(Condition cond, T1 lhs, T2 rhs, Register dest) {
  cmp32(lhs, rhs);
  emitSet(cond, dest);
}

// ===============================================================
// Branch instructions

template <class L>
void MacroAssembler::branch32(Condition cond, Register lhs, Register rhs,
                              L label) {
  cmp32(lhs, rhs);
  j(cond, label);
}

template <class L>
void MacroAssembler::branch32(Condition cond, Register lhs, Imm32 rhs,
                              L label) {
  cmp32(lhs, rhs);
  j(cond, label);
}

void MacroAssembler::branch32(Condition cond, const Address& lhs, Register rhs,
                              Label* label) {
  cmp32(Operand(lhs), rhs);
  j(cond, label);
}

void MacroAssembler::branch32(Condition cond, const Address& lhs, Imm32 rhs,
                              Label* label) {
  cmp32(Operand(lhs), rhs);
  j(cond, label);
}

void MacroAssembler::branch32(Condition cond, const BaseIndex& lhs,
                              Register rhs, Label* label) {
  cmp32(Operand(lhs), rhs);
  j(cond, label);
}

void MacroAssembler::branch32(Condition cond, const BaseIndex& lhs, Imm32 rhs,
                              Label* label) {
  cmp32(Operand(lhs), rhs);
  j(cond, label);
}

void MacroAssembler::branch32(Condition cond, const Operand& lhs, Register rhs,
                              Label* label) {
  cmp32(lhs, rhs);
  j(cond, label);
}

void MacroAssembler::branch32(Condition cond, const Operand& lhs, Imm32 rhs,
                              Label* label) {
  cmp32(lhs, rhs);
  j(cond, label);
}

template <class L>
void MacroAssembler::branchPtr(Condition cond, Register lhs, Register rhs,
                               L label) {
  cmpPtr(lhs, rhs);
  j(cond, label);
}

void MacroAssembler::branchPtr(Condition cond, Register lhs, Imm32 rhs,
                               Label* label) {
  branchPtrImpl(cond, lhs, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, Register lhs, ImmPtr rhs,
                               Label* label) {
  branchPtrImpl(cond, lhs, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, Register lhs, ImmGCPtr rhs,
                               Label* label) {
  branchPtrImpl(cond, lhs, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, Register lhs, ImmWord rhs,
                               Label* label) {
  branchPtrImpl(cond, lhs, rhs, label);
}

template <class L>
void MacroAssembler::branchPtr(Condition cond, const Address& lhs, Register rhs,
                               L label) {
  branchPtrImpl(cond, lhs, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmPtr rhs,
                               Label* label) {
  branchPtrImpl(cond, lhs, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmGCPtr rhs,
                               Label* label) {
  branchPtrImpl(cond, lhs, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmWord rhs,
                               Label* label) {
  branchPtrImpl(cond, lhs, rhs, label);
}

void MacroAssembler::branchPtr(Condition cond, const BaseIndex& lhs,
                               ImmWord rhs, Label* label) {
  branchPtrImpl(cond, lhs, rhs, label);
}

template <typename T, typename S, typename L>
void MacroAssembler::branchPtrImpl(Condition cond, const T& lhs, const S& rhs,
                                   L label) {
  cmpPtr(Operand(lhs), rhs);
  j(cond, label);
}

void MacroAssembler::branchFloat(DoubleCondition cond, FloatRegister lhs,
                                 FloatRegister rhs, Label* label) {
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

void MacroAssembler::branchDouble(DoubleCondition cond, FloatRegister lhs,
                                  FloatRegister rhs, Label* label) {
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
void MacroAssembler::branchAdd32(Condition cond, T src, Register dest,
                                 Label* label) {
  addl(src, dest);
  j(cond, label);
}

template <typename T>
void MacroAssembler::branchSub32(Condition cond, T src, Register dest,
                                 Label* label) {
  subl(src, dest);
  j(cond, label);
}

template <typename T>
void MacroAssembler::branchMul32(Condition cond, T src, Register dest,
                                 Label* label) {
  mul32(src, dest);
  j(cond, label);
}

template <typename T>
void MacroAssembler::branchRshift32(Condition cond, T src, Register dest,
                                    Label* label) {
  MOZ_ASSERT(cond == Zero || cond == NonZero);
  rshift32(src, dest);
  j(cond, label);
}

void MacroAssembler::decBranchPtr(Condition cond, Register lhs, Imm32 rhs,
                                  Label* label) {
  subPtr(rhs, lhs);
  j(cond, label);
}

template <class L>
void MacroAssembler::branchTest32(Condition cond, Register lhs, Register rhs,
                                  L label) {
  MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed ||
             cond == NotSigned);
  test32(lhs, rhs);
  j(cond, label);
}

template <class L>
void MacroAssembler::branchTest32(Condition cond, Register lhs, Imm32 rhs,
                                  L label) {
  MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed ||
             cond == NotSigned);
  test32(lhs, rhs);
  j(cond, label);
}

void MacroAssembler::branchTest32(Condition cond, const Address& lhs, Imm32 rhs,
                                  Label* label) {
  MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed ||
             cond == NotSigned);
  test32(Operand(lhs), rhs);
  j(cond, label);
}

template <class L>
void MacroAssembler::branchTestPtr(Condition cond, Register lhs, Register rhs,
                                   L label) {
  testPtr(lhs, rhs);
  j(cond, label);
}

void MacroAssembler::branchTestPtr(Condition cond, Register lhs, Imm32 rhs,
                                   Label* label) {
  testPtr(lhs, rhs);
  j(cond, label);
}

void MacroAssembler::branchTestPtr(Condition cond, const Address& lhs,
                                   Imm32 rhs, Label* label) {
  testPtr(Operand(lhs), rhs);
  j(cond, label);
}

void MacroAssembler::branchTestUndefined(Condition cond, Register tag,
                                         Label* label) {
  branchTestUndefinedImpl(cond, tag, label);
}

void MacroAssembler::branchTestUndefined(Condition cond, const Address& address,
                                         Label* label) {
  branchTestUndefinedImpl(cond, address, label);
}

void MacroAssembler::branchTestUndefined(Condition cond,
                                         const BaseIndex& address,
                                         Label* label) {
  branchTestUndefinedImpl(cond, address, label);
}

void MacroAssembler::branchTestUndefined(Condition cond,
                                         const ValueOperand& value,
                                         Label* label) {
  branchTestUndefinedImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestUndefinedImpl(Condition cond, const T& t,
                                             Label* label) {
  cond = testUndefined(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestInt32(Condition cond, Register tag,
                                     Label* label) {
  branchTestInt32Impl(cond, tag, label);
}

void MacroAssembler::branchTestInt32(Condition cond, const Address& address,
                                     Label* label) {
  branchTestInt32Impl(cond, address, label);
}

void MacroAssembler::branchTestInt32(Condition cond, const BaseIndex& address,
                                     Label* label) {
  branchTestInt32Impl(cond, address, label);
}

void MacroAssembler::branchTestInt32(Condition cond, const ValueOperand& value,
                                     Label* label) {
  branchTestInt32Impl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestInt32Impl(Condition cond, const T& t,
                                         Label* label) {
  cond = testInt32(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestInt32Truthy(bool truthy,
                                           const ValueOperand& value,
                                           Label* label) {
  Condition cond = testInt32Truthy(truthy, value);
  j(cond, label);
}

void MacroAssembler::branchTestDouble(Condition cond, Register tag,
                                      Label* label) {
  branchTestDoubleImpl(cond, tag, label);
}

void MacroAssembler::branchTestDouble(Condition cond, const Address& address,
                                      Label* label) {
  branchTestDoubleImpl(cond, address, label);
}

void MacroAssembler::branchTestDouble(Condition cond, const BaseIndex& address,
                                      Label* label) {
  branchTestDoubleImpl(cond, address, label);
}

void MacroAssembler::branchTestDouble(Condition cond, const ValueOperand& value,
                                      Label* label) {
  branchTestDoubleImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestDoubleImpl(Condition cond, const T& t,
                                          Label* label) {
  cond = testDouble(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestDoubleTruthy(bool truthy, FloatRegister reg,
                                            Label* label) {
  Condition cond = testDoubleTruthy(truthy, reg);
  j(cond, label);
}

void MacroAssembler::branchTestNumber(Condition cond, Register tag,
                                      Label* label) {
  branchTestNumberImpl(cond, tag, label);
}

void MacroAssembler::branchTestNumber(Condition cond, const ValueOperand& value,
                                      Label* label) {
  branchTestNumberImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestNumberImpl(Condition cond, const T& t,
                                          Label* label) {
  cond = testNumber(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestBoolean(Condition cond, Register tag,
                                       Label* label) {
  branchTestBooleanImpl(cond, tag, label);
}

void MacroAssembler::branchTestBoolean(Condition cond, const Address& address,
                                       Label* label) {
  branchTestBooleanImpl(cond, address, label);
}

void MacroAssembler::branchTestBoolean(Condition cond, const BaseIndex& address,
                                       Label* label) {
  branchTestBooleanImpl(cond, address, label);
}

void MacroAssembler::branchTestBoolean(Condition cond,
                                       const ValueOperand& value,
                                       Label* label) {
  branchTestBooleanImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestBooleanImpl(Condition cond, const T& t,
                                           Label* label) {
  cond = testBoolean(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestString(Condition cond, Register tag,
                                      Label* label) {
  branchTestStringImpl(cond, tag, label);
}

void MacroAssembler::branchTestString(Condition cond, const Address& address,
                                      Label* label) {
  branchTestStringImpl(cond, address, label);
}

void MacroAssembler::branchTestString(Condition cond, const BaseIndex& address,
                                      Label* label) {
  branchTestStringImpl(cond, address, label);
}

void MacroAssembler::branchTestString(Condition cond, const ValueOperand& value,
                                      Label* label) {
  branchTestStringImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestStringImpl(Condition cond, const T& t,
                                          Label* label) {
  cond = testString(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestStringTruthy(bool truthy,
                                            const ValueOperand& value,
                                            Label* label) {
  Condition cond = testStringTruthy(truthy, value);
  j(cond, label);
}

void MacroAssembler::branchTestSymbol(Condition cond, Register tag,
                                      Label* label) {
  branchTestSymbolImpl(cond, tag, label);
}

void MacroAssembler::branchTestSymbol(Condition cond, const BaseIndex& address,
                                      Label* label) {
  branchTestSymbolImpl(cond, address, label);
}

void MacroAssembler::branchTestSymbol(Condition cond, const ValueOperand& value,
                                      Label* label) {
  branchTestSymbolImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestSymbolImpl(Condition cond, const T& t,
                                          Label* label) {
  cond = testSymbol(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestBigInt(Condition cond, Register tag,
                                      Label* label) {
  branchTestBigIntImpl(cond, tag, label);
}

void MacroAssembler::branchTestBigInt(Condition cond, const Address& address,
                                      Label* label) {
  branchTestBigIntImpl(cond, address, label);
}

void MacroAssembler::branchTestBigInt(Condition cond, const BaseIndex& address,
                                      Label* label) {
  branchTestBigIntImpl(cond, address, label);
}

void MacroAssembler::branchTestBigInt(Condition cond, const ValueOperand& value,
                                      Label* label) {
  branchTestBigIntImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestBigIntImpl(Condition cond, const T& t,
                                          Label* label) {
  cond = testBigInt(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestBigIntTruthy(bool truthy,
                                            const ValueOperand& value,
                                            Label* label) {
  Condition cond = testBigIntTruthy(truthy, value);
  j(cond, label);
}

void MacroAssembler::branchTestNull(Condition cond, Register tag,
                                    Label* label) {
  branchTestNullImpl(cond, tag, label);
}

void MacroAssembler::branchTestNull(Condition cond, const Address& address,
                                    Label* label) {
  branchTestNullImpl(cond, address, label);
}

void MacroAssembler::branchTestNull(Condition cond, const BaseIndex& address,
                                    Label* label) {
  branchTestNullImpl(cond, address, label);
}

void MacroAssembler::branchTestNull(Condition cond, const ValueOperand& value,
                                    Label* label) {
  branchTestNullImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestNullImpl(Condition cond, const T& t,
                                        Label* label) {
  cond = testNull(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestObject(Condition cond, Register tag,
                                      Label* label) {
  branchTestObjectImpl(cond, tag, label);
}

void MacroAssembler::branchTestObject(Condition cond, const Address& address,
                                      Label* label) {
  branchTestObjectImpl(cond, address, label);
}

void MacroAssembler::branchTestObject(Condition cond, const BaseIndex& address,
                                      Label* label) {
  branchTestObjectImpl(cond, address, label);
}

void MacroAssembler::branchTestObject(Condition cond, const ValueOperand& value,
                                      Label* label) {
  branchTestObjectImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestObjectImpl(Condition cond, const T& t,
                                          Label* label) {
  cond = testObject(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestGCThing(Condition cond, const Address& address,
                                       Label* label) {
  branchTestGCThingImpl(cond, address, label);
}

void MacroAssembler::branchTestGCThing(Condition cond, const BaseIndex& address,
                                       Label* label) {
  branchTestGCThingImpl(cond, address, label);
}

void MacroAssembler::branchTestGCThing(Condition cond,
                                       const ValueOperand& value,
                                       Label* label) {
  branchTestGCThingImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestGCThingImpl(Condition cond, const T& t,
                                           Label* label) {
  cond = testGCThing(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestPrimitive(Condition cond, Register tag,
                                         Label* label) {
  branchTestPrimitiveImpl(cond, tag, label);
}

void MacroAssembler::branchTestPrimitive(Condition cond,
                                         const ValueOperand& value,
                                         Label* label) {
  branchTestPrimitiveImpl(cond, value, label);
}

template <typename T>
void MacroAssembler::branchTestPrimitiveImpl(Condition cond, const T& t,
                                             Label* label) {
  cond = testPrimitive(cond, t);
  j(cond, label);
}

void MacroAssembler::branchTestMagic(Condition cond, Register tag,
                                     Label* label) {
  branchTestMagicImpl(cond, tag, label);
}

void MacroAssembler::branchTestMagic(Condition cond, const Address& address,
                                     Label* label) {
  branchTestMagicImpl(cond, address, label);
}

void MacroAssembler::branchTestMagic(Condition cond, const BaseIndex& address,
                                     Label* label) {
  branchTestMagicImpl(cond, address, label);
}

template <class L>
void MacroAssembler::branchTestMagic(Condition cond, const ValueOperand& value,
                                     L label) {
  branchTestMagicImpl(cond, value, label);
}

template <typename T, class L>
void MacroAssembler::branchTestMagicImpl(Condition cond, const T& t, L label) {
  cond = testMagic(cond, t);
  j(cond, label);
}

void MacroAssembler::cmp32Move32(Condition cond, Register lhs, Register rhs,
                                 Register src, Register dest) {
  cmp32(lhs, rhs);
  cmovCCl(cond, src, dest);
}

void MacroAssembler::cmp32Move32(Condition cond, Register lhs,
                                 const Address& rhs, Register src,
                                 Register dest) {
  cmp32(lhs, Operand(rhs));
  cmovCCl(cond, src, dest);
}

void MacroAssembler::cmp32Load32(Condition cond, Register lhs,
                                 const Address& rhs, const Address& src,
                                 Register dest) {
  cmp32(lhs, Operand(rhs));
  cmovCCl(cond, Operand(src), dest);
}

void MacroAssembler::cmp32Load32(Condition cond, Register lhs, Register rhs,
                                 const Address& src, Register dest) {
  cmp32(lhs, rhs);
  cmovCCl(cond, Operand(src), dest);
}

void MacroAssembler::spectreZeroRegister(Condition cond, Register scratch,
                                         Register dest) {
  // Note: use movl instead of move32/xorl to ensure flags are not clobbered.
  movl(Imm32(0), scratch);
  spectreMovePtr(cond, scratch, dest);
}

// ========================================================================
// Memory access primitives.
void MacroAssembler::storeUncanonicalizedDouble(FloatRegister src,
                                                const Address& dest) {
  vmovsd(src, dest);
}
void MacroAssembler::storeUncanonicalizedDouble(FloatRegister src,
                                                const BaseIndex& dest) {
  vmovsd(src, dest);
}
void MacroAssembler::storeUncanonicalizedDouble(FloatRegister src,
                                                const Operand& dest) {
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

template void MacroAssembler::storeDouble(FloatRegister src,
                                          const Operand& dest);

void MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src,
                                                 const Address& dest) {
  vmovss(src, dest);
}
void MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src,
                                                 const BaseIndex& dest) {
  vmovss(src, dest);
}
void MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src,
                                                 const Operand& dest) {
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

template void MacroAssembler::storeFloat32(FloatRegister src,
                                           const Operand& dest);

void MacroAssembler::storeFloat32x3(FloatRegister src, const Address& dest) {
  Address destZ(dest);
  destZ.offset += 2 * sizeof(int32_t);
  storeDouble(src, dest);
  ScratchSimd128Scope scratch(*this);
  vmovhlps(src, scratch, scratch);
  storeFloat32(scratch, destZ);
}
void MacroAssembler::storeFloat32x3(FloatRegister src, const BaseIndex& dest) {
  BaseIndex destZ(dest);
  destZ.offset += 2 * sizeof(int32_t);
  storeDouble(src, dest);
  ScratchSimd128Scope scratch(*this);
  vmovhlps(src, scratch, scratch);
  storeFloat32(scratch, destZ);
}

void MacroAssembler::memoryBarrier(MemoryBarrierBits barrier) {
  if (barrier & MembarStoreLoad) {
    storeLoadFence();
  }
}

// ========================================================================
// Wasm SIMD
//
// For vector operations of the form "operationSimd128" we currently bias in
// favor of an integer representation on x86, but this is subject to later
// adjustment based on an analysis of use cases and actual programs.
//
// The order of operations here follows the header file.

// Moves.
//
// Some parts of the masm API are currently agnostic as to the data's
// interpretation as int or float, despite the Intel architecture having
// separate functional units and sometimes penalizing type-specific instructions
// that operate on data in the "wrong" unit.
//
// For the time being, we always choose the integer interpretation when we are
// forced to choose blind, but whether that is right or wrong depends on the
// application.  This applies to moveSimd128, zeroSimd128, loadConstantSimd128,
// loadUnalignedSimd128, and storeUnalignedSimd128, at least.

void MacroAssembler::moveSimd128(FloatRegister src, FloatRegister dest) {
  MacroAssemblerX86Shared::moveSimd128Int(src, dest);
}

// Constants.  See comments at moveSimd128, above.

void MacroAssembler::zeroSimd128(FloatRegister dest) {
  MacroAssemblerX86Shared::zeroSimd128Int(dest);
}

void MacroAssembler::loadConstantSimd128(const SimdConstant& c,
                                         FloatRegister dest) {
  loadConstantSimd128Int(c, dest);
}

// Splat

void MacroAssembler::splatX16(Register src, FloatRegister dest) {
  MacroAssemblerX86Shared::splatX16(src, dest);
}

void MacroAssembler::splatX8(Register src, FloatRegister dest) {
  MacroAssemblerX86Shared::splatX8(src, dest);
}

void MacroAssembler::splatX4(Register src, FloatRegister dest) {
  MacroAssemblerX86Shared::splatX4(src, dest);
}

void MacroAssembler::splatX4(FloatRegister src, FloatRegister dest) {
  MacroAssemblerX86Shared::splatX4(src, dest);
}

void MacroAssembler::splatX2(FloatRegister src, FloatRegister dest) {
  MacroAssemblerX86Shared::splatX2(src, dest);
}

// Extract lane as scalar

void MacroAssembler::extractLaneInt8x16(uint32_t lane, FloatRegister src,
                                        Register dest) {
  MacroAssemblerX86Shared::extractLaneInt8x16(src, dest, lane,
                                              SimdSign::Signed);
}

void MacroAssembler::unsignedExtractLaneInt8x16(uint32_t lane,
                                                FloatRegister src,
                                                Register dest) {
  MacroAssemblerX86Shared::extractLaneInt8x16(src, dest, lane,
                                              SimdSign::Unsigned);
}

void MacroAssembler::extractLaneInt16x8(uint32_t lane, FloatRegister src,
                                        Register dest) {
  MacroAssemblerX86Shared::extractLaneInt16x8(src, dest, lane,
                                              SimdSign::Signed);
}

void MacroAssembler::unsignedExtractLaneInt16x8(uint32_t lane,
                                                FloatRegister src,
                                                Register dest) {
  MacroAssemblerX86Shared::extractLaneInt16x8(src, dest, lane,
                                              SimdSign::Unsigned);
}

void MacroAssembler::extractLaneInt32x4(uint32_t lane, FloatRegister src,
                                        Register dest) {
  MacroAssemblerX86Shared::extractLaneInt32x4(src, dest, lane);
}

void MacroAssembler::extractLaneFloat32x4(uint32_t lane, FloatRegister src,
                                          FloatRegister dest) {
  MacroAssemblerX86Shared::extractLaneFloat32x4(src, dest, lane);
}

void MacroAssembler::extractLaneFloat64x2(uint32_t lane, FloatRegister src,
                                          FloatRegister dest) {
  MacroAssemblerX86Shared::extractLaneFloat64x2(src, dest, lane);
}

// Replace lane value

void MacroAssembler::replaceLaneInt8x16(unsigned lane, Register rhs,
                                        FloatRegister lhsDest) {
  MacroAssemblerX86Shared::insertLaneSimdInt(lhsDest, rhs, lhsDest, lane, 16);
}

void MacroAssembler::replaceLaneInt16x8(unsigned lane, Register rhs,
                                        FloatRegister lhsDest) {
  MacroAssemblerX86Shared::insertLaneSimdInt(lhsDest, rhs, lhsDest, lane, 8);
}

void MacroAssembler::replaceLaneInt32x4(unsigned lane, Register rhs,
                                        FloatRegister lhsDest) {
  MacroAssemblerX86Shared::insertLaneSimdInt(lhsDest, rhs, lhsDest, lane, 4);
}

void MacroAssembler::replaceLaneFloat32x4(unsigned lane, FloatRegister rhs,
                                          FloatRegister lhsDest) {
  MacroAssemblerX86Shared::insertLaneFloat32x4(lhsDest, rhs, lhsDest, lane);
}

void MacroAssembler::replaceLaneFloat64x2(unsigned lane, FloatRegister rhs,
                                          FloatRegister lhsDest) {
  MacroAssemblerX86Shared::insertLaneFloat64x2(lhsDest, rhs, lhsDest, lane);
}

// Shuffle - permute with immediate indices

void MacroAssembler::shuffleInt8x16(uint8_t lanes[16], FloatRegister rhs,
                                    FloatRegister lhsDest, FloatRegister temp) {
  MacroAssemblerX86Shared::shuffleInt8x16(
      lhsDest, rhs, lhsDest, mozilla::Some(temp), mozilla::Nothing(), lanes);
}

// Swizzle - permute with variable indices

void MacroAssembler::swizzleInt8x16(FloatRegister rhs, FloatRegister lhsDest,
                                    FloatRegister temp) {
  ScratchSimd128Scope scratch(*this);
  loadConstantSimd128Int(SimdConstant::SplatX16(15), scratch);
  vmovapd(rhs, temp);
  vpcmpgtb(Operand(scratch), temp, temp);  // set high bit
  vpor(Operand(rhs), temp, temp);          //   for values > 15
  vpshufb(temp, lhsDest, lhsDest);         // permute
}

// Integer Add

void MacroAssembler::addInt8x16(FloatRegister rhs, FloatRegister lhsDest) {
  vpaddb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::addInt16x8(FloatRegister rhs, FloatRegister lhsDest) {
  vpaddw(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::addInt32x4(FloatRegister rhs, FloatRegister lhsDest) {
  vpaddd(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::addInt64x2(FloatRegister rhs, FloatRegister lhsDest) {
  vpaddq(Operand(rhs), lhsDest, lhsDest);
}

// Integer subtract

void MacroAssembler::subInt8x16(FloatRegister rhs, FloatRegister lhsDest) {
  vpsubb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::subInt16x8(FloatRegister rhs, FloatRegister lhsDest) {
  vpsubw(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::subInt32x4(FloatRegister rhs, FloatRegister lhsDest) {
  vpsubd(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::subInt64x2(FloatRegister rhs, FloatRegister lhsDest) {
  vpsubq(Operand(rhs), lhsDest, lhsDest);
}

// Integer multiply

void MacroAssembler::mulInt16x8(FloatRegister rhs, FloatRegister lhsDest) {
  vpmullw(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::mulInt32x4(FloatRegister rhs, FloatRegister lhsDest) {
  vpmulld(Operand(rhs), lhsDest, lhsDest);
}

// Integer negate

void MacroAssembler::negInt8x16(FloatRegister src, FloatRegister dest) {
  ScratchSimd128Scope scratch(*this);
  if (src == dest) {
    vmovaps(src, scratch);
    src = scratch;
  }
  vpxor(Operand(dest), dest, dest);
  vpsubb(Operand(src), dest, dest);
}

void MacroAssembler::negInt16x8(FloatRegister src, FloatRegister dest) {
  ScratchSimd128Scope scratch(*this);
  if (src == dest) {
    vmovaps(src, scratch);
    src = scratch;
  }
  vpxor(Operand(dest), dest, dest);
  vpsubw(Operand(src), dest, dest);
}

void MacroAssembler::negInt32x4(FloatRegister src, FloatRegister dest) {
  ScratchSimd128Scope scratch(*this);
  if (src == dest) {
    vmovaps(src, scratch);
    src = scratch;
  }
  vpxor(Operand(dest), dest, dest);
  vpsubd(Operand(src), dest, dest);
}

void MacroAssembler::negInt64x2(FloatRegister src, FloatRegister dest) {
  ScratchSimd128Scope scratch(*this);
  if (src == dest) {
    vmovaps(src, scratch);
    src = scratch;
  }
  vpxor(Operand(dest), dest, dest);
  vpsubq(Operand(src), dest, dest);
}

// Saturating integer add

void MacroAssembler::addSatInt8x16(FloatRegister rhs, FloatRegister lhsDest) {
  vpaddsb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedAddSatInt8x16(FloatRegister rhs,
                                           FloatRegister lhsDest) {
  vpaddusb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::addSatInt16x8(FloatRegister rhs, FloatRegister lhsDest) {
  vpaddsw(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedAddSatInt16x8(FloatRegister rhs,
                                           FloatRegister lhsDest) {
  vpaddusw(Operand(rhs), lhsDest, lhsDest);
}

// Saturating integer subtract

void MacroAssembler::subSatInt8x16(FloatRegister rhs, FloatRegister lhsDest) {
  vpsubsb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedSubSatInt8x16(FloatRegister rhs,
                                           FloatRegister lhsDest) {
  vpsubusb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::subSatInt16x8(FloatRegister rhs, FloatRegister lhsDest) {
  vpsubsw(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedSubSatInt16x8(FloatRegister rhs,
                                           FloatRegister lhsDest) {
  vpsubusw(Operand(rhs), lhsDest, lhsDest);
}

// Lane-wise integer minimum

void MacroAssembler::minInt8x16(FloatRegister rhs, FloatRegister lhsDest) {
  vpminsb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedMinInt8x16(FloatRegister rhs,
                                        FloatRegister lhsDest) {
  vpminub(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::minInt16x8(FloatRegister rhs, FloatRegister lhsDest) {
  vpminsw(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedMinInt16x8(FloatRegister rhs,
                                        FloatRegister lhsDest) {
  vpminuw(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::minInt32x4(FloatRegister rhs, FloatRegister lhsDest) {
  vpminsd(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedMinInt32x4(FloatRegister rhs,
                                        FloatRegister lhsDest) {
  vpminud(Operand(rhs), lhsDest, lhsDest);
}

// Lane-wise integer maximum

void MacroAssembler::maxInt8x16(FloatRegister rhs, FloatRegister lhsDest) {
  vpmaxsb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedMaxInt8x16(FloatRegister rhs,
                                        FloatRegister lhsDest) {
  vpmaxub(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::maxInt16x8(FloatRegister rhs, FloatRegister lhsDest) {
  vpmaxsw(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedMaxInt16x8(FloatRegister rhs,
                                        FloatRegister lhsDest) {
  vpmaxuw(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::maxInt32x4(FloatRegister rhs, FloatRegister lhsDest) {
  vpmaxsd(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedMaxInt32x4(FloatRegister rhs,
                                        FloatRegister lhsDest) {
  vpmaxud(Operand(rhs), lhsDest, lhsDest);
}

// Lane-wise integer rounding average

void MacroAssembler::averageInt8x16(FloatRegister rhs, FloatRegister lhsDest) {
  vpavgb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::averageInt16x8(FloatRegister rhs, FloatRegister lhsDest) {
  vpavgw(Operand(rhs), lhsDest, lhsDest);
}

// Lane-wise integer absolute value

void MacroAssembler::absInt8x16(FloatRegister src, FloatRegister dest) {
  vpabsb(Operand(src), dest);
}

void MacroAssembler::absInt16x8(FloatRegister src, FloatRegister dest) {
  vpabsw(Operand(src), dest);
}

void MacroAssembler::absInt32x4(FloatRegister src, FloatRegister dest) {
  vpabsd(Operand(src), dest);
}

// Left shift by scalar

void MacroAssembler::leftShiftInt8x16(Register rhs, FloatRegister lhsDest,
                                      Register temp1, FloatRegister temp2) {
  MacroAssemblerX86Shared::packedLeftShiftByScalarInt8x16(lhsDest, rhs, temp1,
                                                          temp2, lhsDest);
}

void MacroAssembler::leftShiftInt16x8(Register rhs, FloatRegister lhsDest,
                                      Register temp) {
  MacroAssemblerX86Shared::packedLeftShiftByScalarInt16x8(lhsDest, rhs, temp,
                                                          lhsDest);
}

void MacroAssembler::leftShiftInt32x4(Register rhs, FloatRegister lhsDest,
                                      Register temp) {
  MacroAssemblerX86Shared::packedLeftShiftByScalarInt32x4(lhsDest, rhs, temp,
                                                          lhsDest);
}

void MacroAssembler::leftShiftInt64x2(Register rhs, FloatRegister lhsDest,
                                      Register temp) {
  MacroAssemblerX86Shared::packedLeftShiftByScalarInt64x2(lhsDest, rhs, temp,
                                                          lhsDest);
}

// Right shift by scalar

void MacroAssembler::rightShiftInt8x16(Register rhs, FloatRegister lhsDest,
                                       Register temp1, FloatRegister temp2) {
  MacroAssemblerX86Shared::packedRightShiftByScalarInt8x16(lhsDest, rhs, temp1,
                                                           temp2, lhsDest);
}

void MacroAssembler::unsignedRightShiftInt8x16(Register rhs,
                                               FloatRegister lhsDest,
                                               Register temp1,
                                               FloatRegister temp2) {
  MacroAssemblerX86Shared::packedUnsignedRightShiftByScalarInt8x16(
      lhsDest, rhs, temp1, temp2, lhsDest);
}

void MacroAssembler::rightShiftInt16x8(Register rhs, FloatRegister lhsDest,
                                       Register temp) {
  MacroAssemblerX86Shared::packedRightShiftByScalarInt16x8(lhsDest, rhs, temp,
                                                           lhsDest);
}

void MacroAssembler::unsignedRightShiftInt16x8(Register rhs,
                                               FloatRegister lhsDest,
                                               Register temp) {
  MacroAssemblerX86Shared::packedUnsignedRightShiftByScalarInt16x8(
      lhsDest, rhs, temp, lhsDest);
}

void MacroAssembler::rightShiftInt32x4(Register rhs, FloatRegister lhsDest,
                                       Register temp) {
  MacroAssemblerX86Shared::packedRightShiftByScalarInt32x4(lhsDest, rhs, temp,
                                                           lhsDest);
}

void MacroAssembler::unsignedRightShiftInt32x4(Register rhs,
                                               FloatRegister lhsDest,
                                               Register temp) {
  MacroAssemblerX86Shared::packedUnsignedRightShiftByScalarInt32x4(
      lhsDest, rhs, temp, lhsDest);
}

void MacroAssembler::unsignedRightShiftInt64x2(Register rhs,
                                               FloatRegister lhsDest,
                                               Register temp) {
  MacroAssemblerX86Shared::packedUnsignedRightShiftByScalarInt64x2(
      lhsDest, rhs, temp, lhsDest);
}

// Bitwise and, or, xor, not

void MacroAssembler::bitwiseAndSimd128(FloatRegister rhs,
                                       FloatRegister lhsDest) {
  MacroAssemblerX86Shared::bitwiseAndSimdInt(lhsDest, Operand(rhs), lhsDest);
}

void MacroAssembler::bitwiseOrSimd128(FloatRegister rhs,
                                      FloatRegister lhsDest) {
  MacroAssemblerX86Shared::bitwiseOrSimdInt(lhsDest, Operand(rhs), lhsDest);
}

void MacroAssembler::bitwiseXorSimd128(FloatRegister rhs,
                                       FloatRegister lhsDest) {
  MacroAssemblerX86Shared::bitwiseXorSimdInt(lhsDest, Operand(rhs), lhsDest);
}

void MacroAssembler::bitwiseNotSimd128(FloatRegister src, FloatRegister dest) {
  MacroAssemblerX86Shared::notInt8x16(Operand(src), dest);
}

// Bitwise and-not

void MacroAssembler::bitwiseNotAndSimd128(FloatRegister rhs,
                                          FloatRegister lhsDest) {
  MacroAssemblerX86Shared::bitwiseAndNotSimdInt(lhsDest, Operand(rhs), lhsDest);
}

// Bitwise select

void MacroAssembler::bitwiseSelectSimd128(FloatRegister mask,
                                          FloatRegister onTrue,
                                          FloatRegister onFalse,
                                          FloatRegister dest,
                                          FloatRegister temp) {
  MacroAssemblerX86Shared::selectSimd128(mask, onTrue, onFalse, temp, dest);
}

// Comparisons (integer and floating-point)

void MacroAssembler::compareInt8x16(Assembler::Condition cond,
                                    FloatRegister rhs, FloatRegister lhsDest) {
  MacroAssemblerX86Shared::compareInt8x16(lhsDest, Operand(rhs), cond, lhsDest);
}

void MacroAssembler::unsignedCompareInt8x16(Assembler::Condition cond,
                                            FloatRegister rhs,
                                            FloatRegister lhsDest,
                                            FloatRegister temp1,
                                            FloatRegister temp2) {
  MacroAssemblerX86Shared::unsignedCompareInt8x16(lhsDest, Operand(rhs), cond,
                                                  lhsDest, temp1, temp2);
}

void MacroAssembler::compareInt16x8(Assembler::Condition cond,
                                    FloatRegister rhs, FloatRegister lhsDest) {
  MacroAssemblerX86Shared::compareInt16x8(lhsDest, Operand(rhs), cond, lhsDest);
}

void MacroAssembler::unsignedCompareInt16x8(Assembler::Condition cond,
                                            FloatRegister rhs,
                                            FloatRegister lhsDest,
                                            FloatRegister temp1,
                                            FloatRegister temp2) {
  MacroAssemblerX86Shared::unsignedCompareInt16x8(lhsDest, Operand(rhs), cond,
                                                  lhsDest, temp1, temp2);
}

void MacroAssembler::compareInt32x4(Assembler::Condition cond,
                                    FloatRegister rhs, FloatRegister lhsDest) {
  MacroAssemblerX86Shared::compareInt32x4(lhsDest, Operand(rhs), cond, lhsDest);
}

void MacroAssembler::unsignedCompareInt32x4(Assembler::Condition cond,
                                            FloatRegister src,
                                            FloatRegister lhsDest,
                                            FloatRegister temp1,
                                            FloatRegister temp2) {
  MacroAssemblerX86Shared::unsignedCompareInt32x4(lhsDest, Operand(src), cond,
                                                  lhsDest, temp1, temp2);
}

void MacroAssembler::compareFloat32x4(Assembler::Condition cond,
                                      FloatRegister rhs,
                                      FloatRegister lhsDest) {
  // There's a hack in the assembler to allow operands to be reversed like this.
  if (cond == Assembler::GreaterThan) {
    MacroAssemblerX86Shared::compareFloat32x4(rhs, Operand(lhsDest),
                                              Assembler::LessThan, lhsDest);
  } else if (cond == Assembler::GreaterThanOrEqual) {
    MacroAssemblerX86Shared::compareFloat32x4(
        rhs, Operand(lhsDest), Assembler::LessThanOrEqual, lhsDest);
  } else {
    MacroAssemblerX86Shared::compareFloat32x4(lhsDest, Operand(rhs), cond,
                                              lhsDest);
  }
}

void MacroAssembler::compareFloat64x2(Assembler::Condition cond,
                                      FloatRegister rhs,
                                      FloatRegister lhsDest) {
  // There's a hack in the assembler to allow operands to be reversed like this.
  if (cond == Assembler::GreaterThan) {
    MacroAssemblerX86Shared::compareFloat64x2(rhs, Operand(lhsDest),
                                              Assembler::LessThan, lhsDest);
  } else if (cond == Assembler::GreaterThanOrEqual) {
    MacroAssemblerX86Shared::compareFloat64x2(
        rhs, Operand(lhsDest), Assembler::LessThanOrEqual, lhsDest);
  } else {
    MacroAssemblerX86Shared::compareFloat64x2(lhsDest, Operand(rhs), cond,
                                              lhsDest);
  }
}

// Load.  See comment at moveSimd128, above.

void MacroAssembler::loadUnalignedSimd128(const Address& src,
                                          FloatRegister dest) {
  loadUnalignedSimd128Int(src, dest);
}

void MacroAssembler::loadUnalignedSimd128(const BaseIndex& src,
                                          FloatRegister dest) {
  loadUnalignedSimd128Int(src, dest);
}

// Store.  See comment at moveSimd128, above.

void MacroAssembler::storeUnalignedSimd128(FloatRegister src,
                                           const Address& dest) {
  storeUnalignedSimd128Int(src, dest);
}

void MacroAssembler::storeUnalignedSimd128(FloatRegister src,
                                           const BaseIndex& dest) {
  storeUnalignedSimd128Int(src, dest);
}

// Floating point negation

void MacroAssembler::negFloat32x4(FloatRegister src, FloatRegister dest) {
  MacroAssemblerX86Shared::negFloat32x4(Operand(src), dest);
}

void MacroAssembler::negFloat64x2(FloatRegister src, FloatRegister dest) {
  MacroAssemblerX86Shared::negFloat64x2(Operand(src), dest);
}

// Floating point absolute value

void MacroAssembler::absFloat32x4(FloatRegister src, FloatRegister dest) {
  MacroAssemblerX86Shared::absFloat32x4(Operand(src), dest);
}

void MacroAssembler::absFloat64x2(FloatRegister src, FloatRegister dest) {
  MacroAssemblerX86Shared::absFloat64x2(Operand(src), dest);
}

// NaN-propagating minimum

void MacroAssembler::minFloat32x4(FloatRegister rhs, FloatRegister lhsDest) {
  MacroAssemblerX86Shared::minFloat32x4(lhsDest, Operand(rhs), lhsDest);
}

void MacroAssembler::minFloat64x2(FloatRegister rhs, FloatRegister lhsDest) {
  MacroAssemblerX86Shared::minFloat64x2(lhsDest, Operand(rhs), lhsDest);
}

// NaN-propagating maxium

void MacroAssembler::maxFloat32x4(FloatRegister rhs, FloatRegister lhsDest,
                                  FloatRegister temp) {
  MacroAssemblerX86Shared::maxFloat32x4(lhsDest, Operand(rhs), temp, lhsDest);
}

void MacroAssembler::maxFloat64x2(FloatRegister rhs, FloatRegister lhsDest,
                                  FloatRegister temp) {
  MacroAssemblerX86Shared::maxFloat64x2(lhsDest, Operand(rhs), temp, lhsDest);
}

// Floating add

void MacroAssembler::addFloat32x4(FloatRegister rhs, FloatRegister lhsDest) {
  vaddps(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::addFloat64x2(FloatRegister rhs, FloatRegister lhsDest) {
  vaddpd(Operand(rhs), lhsDest, lhsDest);
}

// Floating subtract

void MacroAssembler::subFloat32x4(FloatRegister rhs, FloatRegister lhsDest) {
  vsubps(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::subFloat64x2(FloatRegister rhs, FloatRegister lhsDest) {
  AssemblerX86Shared::vsubpd(Operand(rhs), lhsDest, lhsDest);
}

// Floating division

void MacroAssembler::divFloat32x4(FloatRegister rhs, FloatRegister lhsDest) {
  vdivps(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::divFloat64x2(FloatRegister rhs, FloatRegister lhsDest) {
  vdivpd(Operand(rhs), lhsDest, lhsDest);
}

// Floating Multiply

void MacroAssembler::mulFloat32x4(FloatRegister rhs, FloatRegister lhsDest) {
  vmulps(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::mulFloat64x2(FloatRegister rhs, FloatRegister lhsDest) {
  vmulpd(Operand(rhs), lhsDest, lhsDest);
}

// Floating square root

void MacroAssembler::sqrtFloat32x4(FloatRegister src, FloatRegister dest) {
  vsqrtps(Operand(src), dest);
}

void MacroAssembler::sqrtFloat64x2(FloatRegister src, FloatRegister dest) {
  vsqrtpd(Operand(src), dest);
}

// Integer to floating point with rounding

void MacroAssembler::convertInt32x4ToFloat32x4(FloatRegister src,
                                               FloatRegister dest) {
  vcvtdq2ps(src, dest);
}

void MacroAssembler::unsignedConvertInt32x4ToFloat32x4(FloatRegister src,
                                                       FloatRegister dest) {
  MacroAssemblerX86Shared::unsignedConvertInt32x4ToFloat32x4(src, dest);
}

// Floating point to integer with saturation

void MacroAssembler::truncSatFloat32x4ToInt32x4(FloatRegister src,
                                                FloatRegister dest) {
  MacroAssemblerX86Shared::truncSatFloat32x4ToInt32x4(src, dest);
}

void MacroAssembler::unsignedTruncSatFloat32x4ToInt32x4(FloatRegister src,
                                                        FloatRegister dest,
                                                        FloatRegister temp) {
  MacroAssemblerX86Shared::unsignedTruncSatFloat32x4ToInt32x4(src, temp, dest);
}

// Integer to integer narrowing

void MacroAssembler::narrowInt16x8(FloatRegister rhs, FloatRegister lhsDest) {
  vpacksswb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedNarrowInt16x8(FloatRegister rhs,
                                           FloatRegister lhsDest) {
  vpackuswb(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::narrowInt32x4(FloatRegister rhs, FloatRegister lhsDest) {
  vpackssdw(Operand(rhs), lhsDest, lhsDest);
}

void MacroAssembler::unsignedNarrowInt32x4(FloatRegister rhs,
                                           FloatRegister lhsDest) {
  vpackusdw(Operand(rhs), lhsDest, lhsDest);
}

// Integer to integer widening

void MacroAssembler::widenLowInt8x16(FloatRegister src, FloatRegister dest) {
  vpmovsxbw(Operand(src), dest);
}

void MacroAssembler::widenHighInt8x16(FloatRegister src, FloatRegister dest) {
  vpalignr(Operand(src), dest, 8);
  vpmovsxbw(Operand(dest), dest);
}

void MacroAssembler::unsignedWidenLowInt8x16(FloatRegister src,
                                             FloatRegister dest) {
  vpmovzxbw(Operand(src), dest);
}

void MacroAssembler::unsignedWidenHighInt8x16(FloatRegister src,
                                              FloatRegister dest) {
  vpalignr(Operand(src), dest, 8);
  vpmovzxbw(Operand(dest), dest);
}

void MacroAssembler::widenLowInt16x8(FloatRegister src, FloatRegister dest) {
  vpmovsxwd(Operand(src), dest);
}

void MacroAssembler::widenHighInt16x8(FloatRegister src, FloatRegister dest) {
  vpalignr(Operand(src), dest, 8);
  vpmovsxwd(Operand(dest), dest);
}

void MacroAssembler::unsignedWidenLowInt16x8(FloatRegister src,
                                             FloatRegister dest) {
  vpmovzxwd(Operand(src), dest);
}

void MacroAssembler::unsignedWidenHighInt16x8(FloatRegister src,
                                              FloatRegister dest) {
  vpalignr(Operand(src), dest, 8);
  vpmovzxwd(Operand(dest), dest);
}

void MacroAssembler::widenLowInt32x4(FloatRegister src, FloatRegister dest) {
  vpmovsxdq(Operand(src), dest);
}

void MacroAssembler::unsignedWidenLowInt32x4(FloatRegister src,
                                             FloatRegister dest) {
  vpmovzxdq(Operand(src), dest);
}

// ========================================================================
// Truncate floating point.

void MacroAssembler::truncateFloat32ToInt64(Address src, Address dest,
                                            Register temp) {
  if (Assembler::HasSSE3()) {
    fld32(Operand(src));
    fisttp(Operand(dest));
    return;
  }

  if (src.base == esp) {
    src.offset += 2 * sizeof(int32_t);
  }
  if (dest.base == esp) {
    dest.offset += 2 * sizeof(int32_t);
  }

  reserveStack(2 * sizeof(int32_t));

  // Set conversion to truncation.
  fnstcw(Operand(esp, 0));
  load32(Operand(esp, 0), temp);
  andl(Imm32(~0xFF00), temp);
  orl(Imm32(0xCFF), temp);
  store32(temp, Address(esp, sizeof(int32_t)));
  fldcw(Operand(esp, sizeof(int32_t)));

  // Load double on fp stack, convert and load regular stack.
  fld32(Operand(src));
  fistp(Operand(dest));

  // Reset the conversion flag.
  fldcw(Operand(esp, 0));

  freeStack(2 * sizeof(int32_t));
}
void MacroAssembler::truncateDoubleToInt64(Address src, Address dest,
                                           Register temp) {
  if (Assembler::HasSSE3()) {
    fld(Operand(src));
    fisttp(Operand(dest));
    return;
  }

  if (src.base == esp) {
    src.offset += 2 * sizeof(int32_t);
  }
  if (dest.base == esp) {
    dest.offset += 2 * sizeof(int32_t);
  }

  reserveStack(2 * sizeof(int32_t));

  // Set conversion to truncation.
  fnstcw(Operand(esp, 0));
  load32(Operand(esp, 0), temp);
  andl(Imm32(~0xFF00), temp);
  orl(Imm32(0xCFF), temp);
  store32(temp, Address(esp, 1 * sizeof(int32_t)));
  fldcw(Operand(esp, 1 * sizeof(int32_t)));

  // Load double on fp stack, convert and load regular stack.
  fld(Operand(src));
  fistp(Operand(dest));

  // Reset the conversion flag.
  fldcw(Operand(esp, 0));

  freeStack(2 * sizeof(int32_t));
}

// ===============================================================
// Clamping functions.

void MacroAssembler::clampIntToUint8(Register reg) {
  Label inRange;
  branchTest32(Assembler::Zero, reg, Imm32(0xffffff00), &inRange);
  {
    sarl(Imm32(31), reg);
    notl(reg);
    andl(Imm32(255), reg);
  }
  bind(&inRange);
}

//}}} check_macroassembler_style
// ===============================================================

}  // namespace jit
}  // namespace js

#endif /* jit_x86_shared_MacroAssembler_x86_shared_inl_h */
