/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm64/CodeGenerator-arm64.h"

#include "mozilla/MathAlgorithms.h"

#include "jsnum.h"

#include "jit/CodeGenerator.h"
#include "jit/InlineScriptTree.h"
#include "jit/JitRuntime.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/Shape.h"
#include "vm/TraceLogging.h"

#include "jit/shared/CodeGenerator-shared-inl.h"
#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::jit;

using JS::GenericNaN;
using mozilla::FloorLog2;
using mozilla::NegativeInfinity;

// shared
CodeGeneratorARM64::CodeGeneratorARM64(MIRGenerator* gen, LIRGraph* graph,
                                       MacroAssembler* masm)
    : CodeGeneratorShared(gen, graph, masm) {}

bool CodeGeneratorARM64::generateOutOfLineCode() {
  if (!CodeGeneratorShared::generateOutOfLineCode()) {
    return false;
  }

  if (deoptLabel_.used()) {
    // All non-table-based bailouts will go here.
    masm.bind(&deoptLabel_);

    // Store the frame size, so the handler can recover the IonScript.
    masm.push(Imm32(frameSize()));

    TrampolinePtr handler = gen->jitRuntime()->getGenericBailoutHandler();
    masm.jump(handler);
  }

  return !masm.oom();
}

void CodeGeneratorARM64::emitBranch(Assembler::Condition cond,
                                    MBasicBlock* mirTrue,
                                    MBasicBlock* mirFalse) {
  if (isNextBlock(mirFalse->lir())) {
    jumpToBlock(mirTrue, cond);
  } else {
    jumpToBlock(mirFalse, Assembler::InvertCondition(cond));
    jumpToBlock(mirTrue);
  }
}

void OutOfLineBailout::accept(CodeGeneratorARM64* codegen) {
  codegen->visitOutOfLineBailout(this);
}

void CodeGenerator::visitTestIAndBranch(LTestIAndBranch* test) {
  Register input = ToRegister(test->input());
  MBasicBlock* mirTrue = test->ifTrue();
  MBasicBlock* mirFalse = test->ifFalse();

  // Jump to the True block if NonZero.
  // Jump to the False block if Zero.
  if (isNextBlock(mirFalse->lir())) {
    masm.branch32(Assembler::NonZero, input, Imm32(0),
                  getJumpLabelForBranch(mirTrue));
  } else {
    masm.branch32(Assembler::Zero, input, Imm32(0),
                  getJumpLabelForBranch(mirFalse));
    if (!isNextBlock(mirTrue->lir())) {
      jumpToBlock(mirTrue);
    }
  }
}

void CodeGenerator::visitCompare(LCompare* comp) {
  const MCompare* mir = comp->mir();
  const MCompare::CompareType type = mir->compareType();
  const Assembler::Condition cond = JSOpToCondition(type, comp->jsop());
  const Register leftreg = ToRegister(comp->getOperand(0));
  const LAllocation* right = comp->getOperand(1);
  const Register defreg = ToRegister(comp->getDef(0));

  if (type == MCompare::Compare_Object || type == MCompare::Compare_Symbol ||
      type == MCompare::Compare_UIntPtr) {
    if (right->isConstant()) {
      MOZ_ASSERT(type == MCompare::Compare_UIntPtr);
      masm.cmpPtrSet(cond, leftreg, Imm32(ToInt32(right)), defreg);
    } else {
      masm.cmpPtrSet(cond, leftreg, ToRegister(right), defreg);
    }
    return;
  }

  if (right->isConstant()) {
    masm.cmp32Set(cond, leftreg, Imm32(ToInt32(right)), defreg);
  } else {
    masm.cmp32Set(cond, leftreg, ToRegister(right), defreg);
  }
}

void CodeGenerator::visitCompareAndBranch(LCompareAndBranch* comp) {
  const MCompare* mir = comp->cmpMir();
  const MCompare::CompareType type = mir->compareType();
  const LAllocation* left = comp->left();
  const LAllocation* right = comp->right();

  if (type == MCompare::Compare_Object || type == MCompare::Compare_Symbol ||
      type == MCompare::Compare_UIntPtr) {
    if (right->isConstant()) {
      MOZ_ASSERT(type == MCompare::Compare_UIntPtr);
      masm.cmpPtr(ToRegister(left), Imm32(ToInt32(right)));
    } else {
      masm.cmpPtr(ToRegister(left), ToRegister(right));
    }
  } else if (right->isConstant()) {
    masm.cmp32(ToRegister(left), Imm32(ToInt32(right)));
  } else {
    masm.cmp32(ToRegister(left), ToRegister(right));
  }

  Assembler::Condition cond = JSOpToCondition(type, comp->jsop());
  emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void CodeGeneratorARM64::bailoutIf(Assembler::Condition condition,
                                   LSnapshot* snapshot) {
  encode(snapshot);

  // Though the assembler doesn't track all frame pushes, at least make sure
  // the known value makes sense.
  MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None() && deoptTable_,
                frameClass_.frameSize() == masm.framePushed());

  // ARM64 doesn't use a bailout table.
  InlineScriptTree* tree = snapshot->mir()->block()->trackedTree();
  OutOfLineBailout* ool = new (alloc()) OutOfLineBailout(snapshot);
  addOutOfLineCode(ool,
                   new (alloc()) BytecodeSite(tree, tree->script()->code()));

  masm.B(ool->entry(), condition);
}

void CodeGeneratorARM64::bailoutFrom(Label* label, LSnapshot* snapshot) {
  MOZ_ASSERT_IF(!masm.oom(), label->used());
  MOZ_ASSERT_IF(!masm.oom(), !label->bound());

  encode(snapshot);

  // Though the assembler doesn't track all frame pushes, at least make sure
  // the known value makes sense.
  MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None() && deoptTable_,
                frameClass_.frameSize() == masm.framePushed());

  // ARM64 doesn't use a bailout table.
  InlineScriptTree* tree = snapshot->mir()->block()->trackedTree();
  OutOfLineBailout* ool = new (alloc()) OutOfLineBailout(snapshot);
  addOutOfLineCode(ool,
                   new (alloc()) BytecodeSite(tree, tree->script()->code()));

  masm.retarget(label, ool->entry());
}

void CodeGeneratorARM64::bailout(LSnapshot* snapshot) {
  Label label;
  masm.b(&label);
  bailoutFrom(&label, snapshot);
}

void CodeGeneratorARM64::visitOutOfLineBailout(OutOfLineBailout* ool) {
  masm.push(Imm32(ool->snapshot()->snapshotOffset()));
  masm.B(&deoptLabel_);
}

void CodeGenerator::visitMinMaxD(LMinMaxD* ins) {
  ARMFPRegister lhs(ToFloatRegister(ins->first()), 64);
  ARMFPRegister rhs(ToFloatRegister(ins->second()), 64);
  ARMFPRegister output(ToFloatRegister(ins->output()), 64);
  if (ins->mir()->isMax()) {
    masm.Fmax(output, lhs, rhs);
  } else {
    masm.Fmin(output, lhs, rhs);
  }
}

void CodeGenerator::visitMinMaxF(LMinMaxF* ins) {
  ARMFPRegister lhs(ToFloatRegister(ins->first()), 32);
  ARMFPRegister rhs(ToFloatRegister(ins->second()), 32);
  ARMFPRegister output(ToFloatRegister(ins->output()), 32);
  if (ins->mir()->isMax()) {
    masm.Fmax(output, lhs, rhs);
  } else {
    masm.Fmin(output, lhs, rhs);
  }
}

// FIXME: Uh, is this a static function? It looks like it is...
template <typename T>
ARMRegister toWRegister(const T* a) {
  return ARMRegister(ToRegister(a), 32);
}

// FIXME: Uh, is this a static function? It looks like it is...
template <typename T>
ARMRegister toXRegister(const T* a) {
  return ARMRegister(ToRegister(a), 64);
}

Operand toWOperand(const LAllocation* a) {
  if (a->isConstant()) {
    return Operand(ToInt32(a));
  }
  return Operand(toWRegister(a));
}

vixl::CPURegister ToCPURegister(const LAllocation* a, Scalar::Type type) {
  if (a->isFloatReg() && type == Scalar::Float64) {
    return ARMFPRegister(ToFloatRegister(a), 64);
  }
  if (a->isFloatReg() && type == Scalar::Float32) {
    return ARMFPRegister(ToFloatRegister(a), 32);
  }
  if (a->isGeneralReg()) {
    return ARMRegister(ToRegister(a), 32);
  }
  MOZ_CRASH("Unknown LAllocation");
}

vixl::CPURegister ToCPURegister(const LDefinition* d, Scalar::Type type) {
  return ToCPURegister(d->output(), type);
}

// Let |cond| be an ARM64 condition code that we could reasonably use in a
// conditional branch or select following a comparison instruction.  This
// function returns the condition to use in the case where we swap the two
// operands of the comparison instruction.
Assembler::Condition GetCondForSwappedOperands(Assembler::Condition cond) {
  // EQ and NE map to themselves
  // Of the remaining 14 cases, 4 other pairings can meaningfully swap:
  // HS -- LS
  // LO -- HI
  // GE -- LE
  // GT -- LT
  switch (cond) {
    case vixl::eq:
    case vixl::ne:
      return cond;
    case vixl::hs:
      return vixl::ls;
    case vixl::ls:
      return vixl::hs;
    case vixl::lo:
      return vixl::hi;
    case vixl::hi:
      return vixl::lo;
    case vixl::ge:
      return vixl::le;
    case vixl::le:
      return vixl::ge;
    case vixl::gt:
      return vixl::lt;
    case vixl::lt:
      return vixl::gt;
    default:
      MOZ_CRASH("no meaningful swapped-operand condition");
  }
}

void CodeGenerator::visitAddI(LAddI* ins) {
  const LAllocation* lhs = ins->getOperand(0);
  const LAllocation* rhs = ins->getOperand(1);
  const LDefinition* dest = ins->getDef(0);

  // Platforms with three-operand arithmetic ops don't need recovery.
  MOZ_ASSERT(!ins->recoversInput());

  if (ins->snapshot()) {
    masm.Adds(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
    bailoutIf(Assembler::Overflow, ins->snapshot());
  } else {
    masm.Add(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
  }
}

void CodeGenerator::visitSubI(LSubI* ins) {
  const LAllocation* lhs = ins->getOperand(0);
  const LAllocation* rhs = ins->getOperand(1);
  const LDefinition* dest = ins->getDef(0);

  // Platforms with three-operand arithmetic ops don't need recovery.
  MOZ_ASSERT(!ins->recoversInput());

  if (ins->snapshot()) {
    masm.Subs(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
    bailoutIf(Assembler::Overflow, ins->snapshot());
  } else {
    masm.Sub(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
  }
}

void CodeGenerator::visitMulI(LMulI* ins) {
  const LAllocation* lhs = ins->getOperand(0);
  const LAllocation* rhs = ins->getOperand(1);
  const LDefinition* dest = ins->getDef(0);
  MMul* mul = ins->mir();
  MOZ_ASSERT_IF(mul->mode() == MMul::Integer,
                !mul->canBeNegativeZero() && !mul->canOverflow());

  Register lhsreg = ToRegister(lhs);
  const ARMRegister lhsreg32 = ARMRegister(lhsreg, 32);
  Register destreg = ToRegister(dest);
  const ARMRegister destreg32 = ARMRegister(destreg, 32);

  if (rhs->isConstant()) {
    // Bailout on -0.0.
    int32_t constant = ToInt32(rhs);
    if (mul->canBeNegativeZero() && constant <= 0) {
      Assembler::Condition bailoutCond =
          (constant == 0) ? Assembler::LessThan : Assembler::Equal;
      masm.Cmp(toWRegister(lhs), Operand(0));
      bailoutIf(bailoutCond, ins->snapshot());
    }

    switch (constant) {
      case -1:
        masm.Negs(destreg32, Operand(lhsreg32));
        break;  // Go to overflow check.
      case 0:
        masm.Mov(destreg32, wzr);
        return;  // Avoid overflow check.
      case 1:
        if (destreg != lhsreg) {
          masm.Mov(destreg32, lhsreg32);
        }
        return;  // Avoid overflow check.
      case 2:
        masm.Adds(destreg32, lhsreg32, Operand(lhsreg32));
        break;  // Go to overflow check.
      default:
        // Use shift if cannot overflow and constant is a power of 2
        if (!mul->canOverflow() && constant > 0) {
          int32_t shift = FloorLog2(constant);
          if ((1 << shift) == constant) {
            masm.Lsl(destreg32, lhsreg32, shift);
            return;
          }
        }

        // Otherwise, just multiply. We have to check for overflow.
        // Negative zero was handled above.
        Label bailout;
        Label* onOverflow = mul->canOverflow() ? &bailout : nullptr;

        vixl::UseScratchRegisterScope temps(&masm.asVIXL());
        const Register scratch = temps.AcquireW().asUnsized();

        masm.move32(Imm32(constant), scratch);
        masm.mul32(lhsreg, scratch, destreg, onOverflow);

        if (onOverflow) {
          MOZ_ASSERT(lhsreg != destreg);
          bailoutFrom(&bailout, ins->snapshot());
        }
        return;
    }

    // Overflow check.
    if (mul->canOverflow()) {
      bailoutIf(Assembler::Overflow, ins->snapshot());
    }
  } else {
    Register rhsreg = ToRegister(rhs);
    const ARMRegister rhsreg32 = ARMRegister(rhsreg, 32);

    Label bailout;
    Label* onOverflow = mul->canOverflow() ? &bailout : nullptr;

    if (mul->canBeNegativeZero()) {
      // The product of two integer operands is negative zero iff one
      // operand is zero, and the other is negative. Therefore, the
      // sum of the two operands will also be negative (specifically,
      // it will be the non-zero operand). If the result of the
      // multiplication is 0, we can check the sign of the sum to
      // determine whether we should bail out.

      // This code can bailout, so lowering guarantees that the input
      // operands are not overwritten.
      MOZ_ASSERT(destreg != lhsreg);
      MOZ_ASSERT(destreg != rhsreg);

      // Do the multiplication.
      masm.mul32(lhsreg, rhsreg, destreg, onOverflow);

      // Set Zero flag if destreg is 0.
      masm.test32(destreg, destreg);

      // ccmn is 'conditional compare negative'.
      // If the Zero flag is set:
      //    perform a compare negative (compute lhs+rhs and set flags)
      // else:
      //    clear flags
      masm.Ccmn(lhsreg32, rhsreg32, vixl::NoFlag, Assembler::Zero);

      // Bails out if (lhs * rhs == 0) && (lhs + rhs < 0):
      bailoutIf(Assembler::LessThan, ins->snapshot());

    } else {
      masm.mul32(lhsreg, rhsreg, destreg, onOverflow);
    }
    if (onOverflow) {
      bailoutFrom(&bailout, ins->snapshot());
    }
  }
}

void CodeGenerator::visitDivI(LDivI* ins) {
  const Register lhs = ToRegister(ins->lhs());
  const Register rhs = ToRegister(ins->rhs());
  const Register output = ToRegister(ins->output());

  const ARMRegister lhs32 = toWRegister(ins->lhs());
  const ARMRegister rhs32 = toWRegister(ins->rhs());
  const ARMRegister temp32 = toWRegister(ins->getTemp(0));
  const ARMRegister output32 = toWRegister(ins->output());

  MDiv* mir = ins->mir();

  Label done;

  // Handle division by zero.
  if (mir->canBeDivideByZero()) {
    masm.test32(rhs, rhs);
    if (mir->trapOnError()) {
      Label nonZero;
      masm.j(Assembler::NonZero, &nonZero);
      masm.wasmTrap(wasm::Trap::IntegerDivideByZero, mir->bytecodeOffset());
      masm.bind(&nonZero);
    } else if (mir->canTruncateInfinities()) {
      // Truncated division by zero is zero: (Infinity|0 = 0).
      Label nonZero;
      masm.j(Assembler::NonZero, &nonZero);
      masm.Mov(output32, wzr);
      masm.jump(&done);
      masm.bind(&nonZero);
    } else {
      MOZ_ASSERT(mir->fallible());
      bailoutIf(Assembler::Zero, ins->snapshot());
    }
  }

  // Handle an integer overflow from (INT32_MIN / -1).
  // The integer division gives INT32_MIN, but should be -(double)INT32_MIN.
  if (mir->canBeNegativeOverflow()) {
    Label notOverflow;

    // Branch to handle the non-overflow cases.
    masm.branch32(Assembler::NotEqual, lhs, Imm32(INT32_MIN), &notOverflow);
    masm.branch32(Assembler::NotEqual, rhs, Imm32(-1), &notOverflow);

    // Handle overflow.
    if (mir->trapOnError()) {
      masm.wasmTrap(wasm::Trap::IntegerOverflow, mir->bytecodeOffset());
    } else if (mir->canTruncateOverflow()) {
      // (-INT32_MIN)|0 == INT32_MIN, which is already in lhs.
      masm.move32(lhs, output);
      masm.jump(&done);
    } else {
      MOZ_ASSERT(mir->fallible());
      bailout(ins->snapshot());
    }
    masm.bind(&notOverflow);
  }

  // Handle negative zero: lhs == 0 && rhs < 0.
  if (!mir->canTruncateNegativeZero() && mir->canBeNegativeZero()) {
    Label nonZero;
    masm.branch32(Assembler::NotEqual, lhs, Imm32(0), &nonZero);
    masm.cmp32(rhs, Imm32(0));
    bailoutIf(Assembler::LessThan, ins->snapshot());
    masm.bind(&nonZero);
  }

  // Perform integer division.
  if (mir->canTruncateRemainder()) {
    masm.Sdiv(output32, lhs32, rhs32);
  } else {
    vixl::UseScratchRegisterScope temps(&masm.asVIXL());
    ARMRegister scratch32 = temps.AcquireW();

    // ARM does not automatically calculate the remainder.
    // The ISR suggests multiplication to determine whether a remainder exists.
    masm.Sdiv(scratch32, lhs32, rhs32);
    masm.Mul(temp32, scratch32, rhs32);
    masm.Cmp(lhs32, temp32);
    bailoutIf(Assembler::NotEqual, ins->snapshot());
    masm.Mov(output32, scratch32);
  }

  masm.bind(&done);
}

void CodeGenerator::visitDivPowTwoI(LDivPowTwoI* ins) {
  const Register numerator = ToRegister(ins->numerator());
  const ARMRegister numerator32 = toWRegister(ins->numerator());
  const ARMRegister output32 = toWRegister(ins->output());

  int32_t shift = ins->shift();
  bool negativeDivisor = ins->negativeDivisor();
  MDiv* mir = ins->mir();

  if (!mir->isTruncated() && negativeDivisor) {
    // 0 divided by a negative number returns a -0 double.
    bailoutTest32(Assembler::Zero, numerator, numerator, ins->snapshot());
  }

  if (shift) {
    if (!mir->isTruncated()) {
      // If the remainder is != 0, bailout since this must be a double.
      bailoutTest32(Assembler::NonZero, numerator,
                    Imm32(UINT32_MAX >> (32 - shift)), ins->snapshot());
    }

    if (mir->isUnsigned()) {
      // shift right
      masm.Lsr(output32, numerator32, shift);
    } else {
      ARMRegister temp32 = numerator32;
      // Adjust the value so that shifting produces a correctly
      // rounded result when the numerator is negative. See 10-1
      // "Signed Division by a Known Power of 2" in Henry
      // S. Warren, Jr.'s Hacker's Delight.
      if (mir->canBeNegativeDividend() && mir->isTruncated()) {
        if (shift > 1) {
          // Copy the sign bit of the numerator. (= (2^32 - 1) or 0)
          masm.Asr(output32, numerator32, 31);
          temp32 = output32;
        }
        // Divide by 2^(32 - shift)
        // i.e. (= (2^32 - 1) / 2^(32 - shift) or 0)
        // i.e. (= (2^shift - 1) or 0)
        masm.Lsr(output32, temp32, 32 - shift);
        // If signed, make any 1 bit below the shifted bits to bubble up, such
        // that once shifted the value would be rounded towards 0.
        masm.Add(output32, output32, numerator32);
        temp32 = output32;
      }
      masm.Asr(output32, temp32, shift);

      if (negativeDivisor) {
        masm.Neg(output32, output32);
      }
    }
    return;
  }

  if (negativeDivisor) {
    // INT32_MIN / -1 overflows.
    if (!mir->isTruncated()) {
      masm.Negs(output32, numerator32);
      bailoutIf(Assembler::Overflow, ins->snapshot());
    } else if (mir->trapOnError()) {
      Label ok;
      masm.Negs(output32, numerator32);
      masm.branch(Assembler::NoOverflow, &ok);
      masm.wasmTrap(wasm::Trap::IntegerOverflow, mir->bytecodeOffset());
      masm.bind(&ok);
    } else {
      // Do not set condition flags.
      masm.Neg(output32, numerator32);
    }
  } else {
    if (mir->isUnsigned() && !mir->isTruncated()) {
      // Copy and set flags.
      masm.Adds(output32, numerator32, 0);
      // Unsigned division by 1 can overflow if output is not truncated, as we
      // do not have an Unsigned type for MIR instructions.
      bailoutIf(Assembler::Signed, ins->snapshot());
    } else {
      // Copy the result.
      masm.Mov(output32, numerator32);
    }
  }
}

void CodeGenerator::visitDivConstantI(LDivConstantI* ins) {
  const ARMRegister lhs32 = toWRegister(ins->numerator());
  const ARMRegister lhs64 = toXRegister(ins->numerator());
  const ARMRegister const32 = toWRegister(ins->temp());
  const ARMRegister output32 = toWRegister(ins->output());
  const ARMRegister output64 = toXRegister(ins->output());
  int32_t d = ins->denominator();

  // The absolute value of the denominator isn't a power of 2.
  using mozilla::Abs;
  MOZ_ASSERT((Abs(d) & (Abs(d) - 1)) != 0);

  // We will first divide by Abs(d), and negate the answer if d is negative.
  // If desired, this can be avoided by generalizing computeDivisionConstants.
  ReciprocalMulConstants rmc =
      computeDivisionConstants(Abs(d), /* maxLog = */ 31);

  // We first compute (M * n) >> 32, where M = rmc.multiplier.
  masm.Mov(const32, int32_t(rmc.multiplier));
  if (rmc.multiplier > INT32_MAX) {
    MOZ_ASSERT(rmc.multiplier < (int64_t(1) << 32));

    // We actually compute (int32_t(M) * n) instead, without the upper bit.
    // Thus, (M * n) = (int32_t(M) * n) + n << 32.
    //
    // ((int32_t(M) * n) + n << 32) can't overflow, as both operands have
    // opposite signs because int32_t(M) is negative.
    masm.Lsl(output64, lhs64, 32);

    // Store (M * n) in output64.
    masm.Smaddl(output64, const32, lhs32, output64);
  } else {
    // Store (M * n) in output64.
    masm.Smull(output64, const32, lhs32);
  }

  // (M * n) >> (32 + shift) is the truncated division answer if n is
  // non-negative, as proved in the comments of computeDivisionConstants. We
  // must add 1 later if n is negative to get the right answer in all cases.
  masm.Asr(output64, output64, 32 + rmc.shiftAmount);

  // We'll subtract -1 instead of adding 1, because (n < 0 ? -1 : 0) can be
  // computed with just a sign-extending shift of 31 bits.
  if (ins->canBeNegativeDividend()) {
    masm.Asr(const32, lhs32, 31);
    masm.Sub(output32, output32, const32);
  }

  // After this, output32 contains the correct truncated division result.
  if (d < 0) {
    masm.Neg(output32, output32);
  }

  if (!ins->mir()->isTruncated()) {
    // This is a division op. Multiply the obtained value by d to check if
    // the correct answer is an integer. This cannot overflow, since |d| > 1.
    masm.Mov(const32, d);
    masm.Msub(const32, output32, const32, lhs32);
    // bailout if (lhs - output * d != 0)
    masm.Cmp(const32, wzr);
    auto bailoutCond = Assembler::NonZero;

    // If lhs is zero and the divisor is negative, the answer should have
    // been -0.
    if (d < 0) {
      // or bailout if (lhs == 0).
      // ^                  ^
      // |                  '-- masm.Ccmp(lhs32, lhs32, .., ..)
      // '-- masm.Ccmp(.., .., vixl::ZFlag, ! bailoutCond)
      masm.Ccmp(lhs32, wzr, vixl::ZFlag, Assembler::Zero);
      bailoutCond = Assembler::Zero;
    }

    // bailout if (lhs - output * d != 0) or (d < 0 && lhs == 0)
    bailoutIf(bailoutCond, ins->snapshot());
  }
}

void CodeGenerator::visitUDivConstantI(LUDivConstantI* ins) {
  const ARMRegister lhs32 = toWRegister(ins->numerator());
  const ARMRegister lhs64 = toXRegister(ins->numerator());
  const ARMRegister const32 = toWRegister(ins->temp());
  const ARMRegister output32 = toWRegister(ins->output());
  const ARMRegister output64 = toXRegister(ins->output());
  uint32_t d = ins->denominator();

  if (d == 0) {
    if (ins->mir()->isTruncated()) {
      if (ins->mir()->trapOnError()) {
        masm.wasmTrap(wasm::Trap::IntegerDivideByZero,
                      ins->mir()->bytecodeOffset());
      } else {
        masm.Mov(output32, wzr);
      }
    } else {
      bailout(ins->snapshot());
    }
    return;
  }

  // The denominator isn't a power of 2 (see LDivPowTwoI).
  MOZ_ASSERT((d & (d - 1)) != 0);

  ReciprocalMulConstants rmc = computeDivisionConstants(d, /* maxLog = */ 32);

  // We first compute (M * n) >> 32, where M = rmc.multiplier.
  masm.Mov(const32, int32_t(rmc.multiplier));
  masm.Umull(output64, const32, lhs32);
  if (rmc.multiplier > UINT32_MAX) {
    // M >= 2^32 and shift == 0 is impossible, as d >= 2 implies that
    // ((M * n) >> (32 + shift)) >= n > floor(n/d) whenever n >= d,
    // contradicting the proof of correctness in computeDivisionConstants.
    MOZ_ASSERT(rmc.shiftAmount > 0);
    MOZ_ASSERT(rmc.multiplier < (int64_t(1) << 33));

    // We actually compute (uint32_t(M) * n) instead, without the upper bit.
    // Thus, (M * n) = (uint32_t(M) * n) + n << 32.
    //
    // ((uint32_t(M) * n) + n << 32) can overflow. Hacker's Delight explains a
    // trick to avoid this overflow case, but we can avoid it by computing the
    // addition on 64 bits registers.
    //
    // Compute ((uint32_t(M) * n) >> 32 + n)
    masm.Add(output64, lhs64, Operand(output64, vixl::LSR, 32));

    // (M * n) >> (32 + shift) is the truncated division answer.
    masm.Lsr(output64, output64, rmc.shiftAmount);
  } else {
    // (M * n) >> (32 + shift) is the truncated division answer.
    masm.Lsr(output64, output64, 32 + rmc.shiftAmount);
  }

  // We now have the truncated division value. We are checking whether the
  // division resulted in an integer, we multiply the obtained value by d and
  // check the remainder of the division.
  if (!ins->mir()->isTruncated()) {
    masm.Mov(const32, d);
    masm.Msub(const32, output32, const32, lhs32);
    // bailout if (lhs - output * d != 0)
    masm.Cmp(const32, const32);
    bailoutIf(Assembler::NonZero, ins->snapshot());
  }
}

void CodeGenerator::visitModI(LModI* ins) {
  ARMRegister lhs = toWRegister(ins->lhs());
  ARMRegister rhs = toWRegister(ins->rhs());
  ARMRegister output = toWRegister(ins->output());
  Label done;

  MMod* mir = ins->mir();

  // Prevent divide by zero.
  if (mir->canBeDivideByZero()) {
    if (mir->isTruncated()) {
      if (mir->trapOnError()) {
        Label nonZero;
        masm.Cbnz(rhs, &nonZero);
        masm.wasmTrap(wasm::Trap::IntegerDivideByZero, mir->bytecodeOffset());
        masm.bind(&nonZero);
      } else {
        // Truncated division by zero yields integer zero.
        masm.Mov(output, rhs);
        masm.Cbz(rhs, &done);
      }
    } else {
      // Non-truncated division by zero produces a non-integer.
      MOZ_ASSERT(!gen->compilingWasm());
      masm.Cmp(rhs, Operand(0));
      bailoutIf(Assembler::Equal, ins->snapshot());
    }
  }

  // Signed division.
  masm.Sdiv(output, lhs, rhs);

  // Compute the remainder: output = lhs - (output * rhs).
  masm.Msub(output, output, rhs, lhs);

  if (mir->canBeNegativeDividend() && !mir->isTruncated()) {
    // If output == 0 and lhs < 0, then the result should be double -0.0.
    // Note that this guard handles lhs == INT_MIN and rhs == -1:
    //   output = INT_MIN - (INT_MIN / -1) * -1
    //          = INT_MIN - INT_MIN
    //          = 0
    masm.Cbnz(output, &done);
    bailoutCmp32(Assembler::LessThan, lhs, Imm32(0), ins->snapshot());
  }

  if (done.used()) {
    masm.bind(&done);
  }
}

void CodeGenerator::visitModPowTwoI(LModPowTwoI* ins) {
  Register lhs = ToRegister(ins->getOperand(0));
  ARMRegister lhsw = toWRegister(ins->getOperand(0));
  ARMRegister outw = toWRegister(ins->output());

  int32_t shift = ins->shift();
  bool canBeNegative =
      !ins->mir()->isUnsigned() && ins->mir()->canBeNegativeDividend();

  Label negative;
  if (canBeNegative) {
    // Switch based on sign of the lhs.
    // Positive numbers are just a bitmask.
    masm.branchTest32(Assembler::Signed, lhs, lhs, &negative);
  }

  masm.And(outw, lhsw, Operand((uint32_t(1) << shift) - 1));

  if (canBeNegative) {
    Label done;
    masm.jump(&done);

    // Negative numbers need a negate, bitmask, negate.
    masm.bind(&negative);
    masm.Neg(outw, Operand(lhsw));
    masm.And(outw, outw, Operand((uint32_t(1) << shift) - 1));

    // Since a%b has the same sign as b, and a is negative in this branch,
    // an answer of 0 means the correct result is actually -0. Bail out.
    if (!ins->mir()->isTruncated()) {
      masm.Negs(outw, Operand(outw));
      bailoutIf(Assembler::Zero, ins->snapshot());
    } else {
      masm.Neg(outw, Operand(outw));
    }

    masm.bind(&done);
  }
}

void CodeGenerator::visitModMaskI(LModMaskI* ins) {
  MMod* mir = ins->mir();
  int32_t shift = ins->shift();

  const Register src = ToRegister(ins->getOperand(0));
  const Register dest = ToRegister(ins->getDef(0));
  const Register hold = ToRegister(ins->getTemp(0));
  const Register remain = ToRegister(ins->getTemp(1));

  const ARMRegister src32 = ARMRegister(src, 32);
  const ARMRegister dest32 = ARMRegister(dest, 32);
  const ARMRegister remain32 = ARMRegister(remain, 32);

  vixl::UseScratchRegisterScope temps(&masm.asVIXL());
  const ARMRegister scratch32 = temps.AcquireW();
  const Register scratch = scratch32.asUnsized();

  // We wish to compute x % (1<<y) - 1 for a known constant, y.
  //
  // 1. Let b = (1<<y) and C = (1<<y)-1, then think of the 32 bit dividend as
  // a number in base b, namely c_0*1 + c_1*b + c_2*b^2 ... c_n*b^n
  //
  // 2. Since both addition and multiplication commute with modulus:
  //   x % C == (c_0 + c_1*b + ... + c_n*b^n) % C ==
  //    (c_0 % C) + (c_1%C) * (b % C) + (c_2 % C) * (b^2 % C)...
  //
  // 3. Since b == C + 1, b % C == 1, and b^n % C == 1 the whole thing
  // simplifies to: c_0 + c_1 + c_2 ... c_n % C
  //
  // Each c_n can easily be computed by a shift/bitextract, and the modulus
  // can be maintained by simply subtracting by C whenever the number gets
  // over C.
  int32_t mask = (1 << shift) - 1;
  Label loop;

  // Register 'hold' holds -1 if the value was negative, 1 otherwise.
  // The remain reg holds the remaining bits that have not been processed.
  // The scratch reg serves as a temporary location to store extracted bits.
  // The dest reg is the accumulator, becoming final result.
  //
  // Move the whole value into the remain.
  masm.Mov(remain32, src32);
  // Zero out the dest.
  masm.Mov(dest32, wzr);
  // Set the hold appropriately.
  {
    Label negative;
    masm.branch32(Assembler::Signed, remain, Imm32(0), &negative);
    masm.move32(Imm32(1), hold);
    masm.jump(&loop);

    masm.bind(&negative);
    masm.move32(Imm32(-1), hold);
    masm.neg32(remain);
  }

  // Begin the main loop.
  masm.bind(&loop);
  {
    // Extract the bottom bits into scratch.
    masm.And(scratch32, remain32, Operand(mask));
    // Add those bits to the accumulator.
    masm.Add(dest32, dest32, scratch32);
    // Do a trial subtraction. This functions as a cmp but remembers the result.
    masm.Subs(scratch32, dest32, Operand(mask));
    // If (sum - C) > 0, store sum - C back into sum, thus performing a modulus.
    {
      Label sumSigned;
      masm.branch32(Assembler::Signed, scratch, scratch, &sumSigned);
      masm.Mov(dest32, scratch32);
      masm.bind(&sumSigned);
    }
    // Get rid of the bits that we extracted before.
    masm.Lsr(remain32, remain32, shift);
    // If the shift produced zero, finish, otherwise, continue in the loop.
    masm.branchTest32(Assembler::NonZero, remain, remain, &loop);
  }

  // Check the hold to see if we need to negate the result.
  {
    Label done;

    // If the hold was non-zero, negate the result to match JS expectations.
    masm.branchTest32(Assembler::NotSigned, hold, hold, &done);
    if (mir->canBeNegativeDividend() && !mir->isTruncated()) {
      // Bail in case of negative zero hold.
      bailoutTest32(Assembler::Zero, hold, hold, ins->snapshot());
    }

    masm.neg32(dest);
    masm.bind(&done);
  }
}

void CodeGeneratorARM64::emitBigIntDiv(LBigIntDiv* ins, Register dividend,
                                       Register divisor, Register output,
                                       Label* fail) {
  // Callers handle division by zero and integer overflow.

  const ARMRegister dividend64(dividend, 64);
  const ARMRegister divisor64(divisor, 64);

  masm.Sdiv(/* result= */ dividend64, dividend64, divisor64);

  // Create and return the result.
  masm.newGCBigInt(output, divisor, fail, bigIntsCanBeInNursery());
  masm.initializeBigInt(output, dividend);
}

void CodeGeneratorARM64::emitBigIntMod(LBigIntMod* ins, Register dividend,
                                       Register divisor, Register output,
                                       Label* fail) {
  // Callers handle division by zero and integer overflow.

  const ARMRegister dividend64(dividend, 64);
  const ARMRegister divisor64(divisor, 64);
  const ARMRegister output64(output, 64);

  // Signed division.
  masm.Sdiv(output64, dividend64, divisor64);

  // Compute the remainder: output = dividend - (output * divisor).
  masm.Msub(/* result= */ dividend64, output64, divisor64, dividend64);

  // Create and return the result.
  masm.newGCBigInt(output, divisor, fail, bigIntsCanBeInNursery());
  masm.initializeBigInt(output, dividend);
}

void CodeGenerator::visitBitNotI(LBitNotI* ins) {
  const LAllocation* input = ins->getOperand(0);
  const LDefinition* output = ins->getDef(0);
  masm.Mvn(toWRegister(output), toWOperand(input));
}

void CodeGenerator::visitBitOpI(LBitOpI* ins) {
  const ARMRegister lhs = toWRegister(ins->getOperand(0));
  const Operand rhs = toWOperand(ins->getOperand(1));
  const ARMRegister dest = toWRegister(ins->getDef(0));

  switch (ins->bitop()) {
    case JSOp::BitOr:
      masm.Orr(dest, lhs, rhs);
      break;
    case JSOp::BitXor:
      masm.Eor(dest, lhs, rhs);
      break;
    case JSOp::BitAnd:
      masm.And(dest, lhs, rhs);
      break;
    default:
      MOZ_CRASH("unexpected binary opcode");
  }
}

void CodeGenerator::visitShiftI(LShiftI* ins) {
  const ARMRegister lhs = toWRegister(ins->lhs());
  const LAllocation* rhs = ins->rhs();
  const ARMRegister dest = toWRegister(ins->output());

  if (rhs->isConstant()) {
    int32_t shift = ToInt32(rhs) & 0x1F;
    switch (ins->bitop()) {
      case JSOp::Lsh:
        masm.Lsl(dest, lhs, shift);
        break;
      case JSOp::Rsh:
        masm.Asr(dest, lhs, shift);
        break;
      case JSOp::Ursh:
        if (shift) {
          masm.Lsr(dest, lhs, shift);
        } else if (ins->mir()->toUrsh()->fallible()) {
          // x >>> 0 can overflow.
          masm.Ands(dest, lhs, Operand(0xFFFFFFFF));
          bailoutIf(Assembler::Signed, ins->snapshot());
        } else {
          masm.Mov(dest, lhs);
        }
        break;
      default:
        MOZ_CRASH("Unexpected shift op");
    }
  } else {
    const ARMRegister rhsreg = toWRegister(rhs);
    switch (ins->bitop()) {
      case JSOp::Lsh:
        masm.Lsl(dest, lhs, rhsreg);
        break;
      case JSOp::Rsh:
        masm.Asr(dest, lhs, rhsreg);
        break;
      case JSOp::Ursh:
        masm.Lsr(dest, lhs, rhsreg);
        if (ins->mir()->toUrsh()->fallible()) {
          /// x >>> 0 can overflow.
          masm.Cmp(dest, Operand(0));
          bailoutIf(Assembler::LessThan, ins->snapshot());
        }
        break;
      default:
        MOZ_CRASH("Unexpected shift op");
    }
  }
}

void CodeGenerator::visitUrshD(LUrshD* ins) {
  const ARMRegister lhs = toWRegister(ins->lhs());
  const LAllocation* rhs = ins->rhs();
  const FloatRegister out = ToFloatRegister(ins->output());

  const Register temp = ToRegister(ins->temp());
  const ARMRegister temp32 = toWRegister(ins->temp());

  if (rhs->isConstant()) {
    int32_t shift = ToInt32(rhs) & 0x1F;
    if (shift) {
      masm.Lsr(temp32, lhs, shift);
      masm.convertUInt32ToDouble(temp, out);
    } else {
      masm.convertUInt32ToDouble(ToRegister(ins->lhs()), out);
    }
  } else {
    masm.And(temp32, toWRegister(rhs), Operand(0x1F));
    masm.Lsr(temp32, lhs, temp32);
    masm.convertUInt32ToDouble(temp, out);
  }
}

void CodeGenerator::visitPowHalfD(LPowHalfD* ins) {
  FloatRegister input = ToFloatRegister(ins->input());
  FloatRegister output = ToFloatRegister(ins->output());

  ScratchDoubleScope scratch(masm);

  Label done, sqrt;

  if (!ins->mir()->operandIsNeverNegativeInfinity()) {
    // Branch if not -Infinity.
    masm.loadConstantDouble(NegativeInfinity<double>(), scratch);

    Assembler::DoubleCondition cond = Assembler::DoubleNotEqualOrUnordered;
    if (ins->mir()->operandIsNeverNaN()) {
      cond = Assembler::DoubleNotEqual;
    }
    masm.branchDouble(cond, input, scratch, &sqrt);

    // Math.pow(-Infinity, 0.5) == Infinity.
    masm.zeroDouble(output);
    masm.subDouble(scratch, output);
    masm.jump(&done);

    masm.bind(&sqrt);
  }

  if (!ins->mir()->operandIsNeverNegativeZero()) {
    // Math.pow(-0, 0.5) == 0 == Math.pow(0, 0.5).
    // Adding 0 converts any -0 to 0.
    masm.zeroDouble(scratch);
    masm.addDouble(input, scratch);
    masm.sqrtDouble(scratch, output);
  } else {
    masm.sqrtDouble(input, output);
  }

  masm.bind(&done);
}

MoveOperand CodeGeneratorARM64::toMoveOperand(const LAllocation a) const {
  if (a.isGeneralReg()) {
    return MoveOperand(ToRegister(a));
  }
  if (a.isFloatReg()) {
    return MoveOperand(ToFloatRegister(a));
  }
  MoveOperand::Kind kind =
      a.isStackArea() ? MoveOperand::EFFECTIVE_ADDRESS : MoveOperand::MEMORY;
  return MoveOperand(ToAddress(a), kind);
}

class js::jit::OutOfLineTableSwitch
    : public OutOfLineCodeBase<CodeGeneratorARM64> {
  MTableSwitch* mir_;
  CodeLabel jumpLabel_;

  void accept(CodeGeneratorARM64* codegen) override {
    codegen->visitOutOfLineTableSwitch(this);
  }

 public:
  explicit OutOfLineTableSwitch(MTableSwitch* mir) : mir_(mir) {}

  MTableSwitch* mir() const { return mir_; }

  CodeLabel* jumpLabel() { return &jumpLabel_; }
};

void CodeGeneratorARM64::visitOutOfLineTableSwitch(OutOfLineTableSwitch* ool) {
  MTableSwitch* mir = ool->mir();

  // Prevent nop and pools sequences to appear in the jump table.
  AutoForbidPoolsAndNops afp(
      &masm, (mir->numCases() + 1) * (sizeof(void*) / vixl::kInstructionSize));
  masm.haltingAlign(sizeof(void*));
  masm.bind(ool->jumpLabel());
  masm.addCodeLabel(*ool->jumpLabel());

  for (size_t i = 0; i < mir->numCases(); i++) {
    LBlock* caseblock = skipTrivialBlocks(mir->getCase(i))->lir();
    Label* caseheader = caseblock->label();
    uint32_t caseoffset = caseheader->offset();

    // The entries of the jump table need to be absolute addresses,
    // and thus must be patched after codegen is finished.
    CodeLabel cl;
    masm.writeCodePointer(&cl);
    cl.target()->bind(caseoffset);
    masm.addCodeLabel(cl);
  }
}

void CodeGeneratorARM64::emitTableSwitchDispatch(MTableSwitch* mir,
                                                 Register index,
                                                 Register base) {
  Label* defaultcase = skipTrivialBlocks(mir->getDefault())->lir()->label();

  // Let the lowest table entry be indexed at 0.
  if (mir->low() != 0) {
    masm.sub32(Imm32(mir->low()), index);
  }

  // Jump to the default case if input is out of range.
  int32_t cases = mir->numCases();
  masm.branch32(Assembler::AboveOrEqual, index, Imm32(cases), defaultcase);

  // Because the target code has not yet been generated, we cannot know the
  // instruction offsets for use as jump targets. Therefore we construct
  // an OutOfLineTableSwitch that winds up holding the jump table.
  //
  // Because the jump table is generated as part of out-of-line code,
  // it is generated after all the regular codegen, so the jump targets
  // are guaranteed to exist when generating the jump table.
  OutOfLineTableSwitch* ool = new (alloc()) OutOfLineTableSwitch(mir);
  addOutOfLineCode(ool, mir);

  // Use the index to get the address of the jump target from the table.
  masm.mov(ool->jumpLabel(), base);
  BaseIndex pointer(base, index, ScalePointer);

  // Load the target from the jump table and branch to it.
  masm.branchToComputedAddress(pointer);
}

void CodeGenerator::visitMathD(LMathD* math) {
  ARMFPRegister lhs(ToFloatRegister(math->lhs()), 64);
  ARMFPRegister rhs(ToFloatRegister(math->rhs()), 64);
  ARMFPRegister output(ToFloatRegister(math->output()), 64);

  switch (math->jsop()) {
    case JSOp::Add:
      masm.Fadd(output, lhs, rhs);
      break;
    case JSOp::Sub:
      masm.Fsub(output, lhs, rhs);
      break;
    case JSOp::Mul:
      masm.Fmul(output, lhs, rhs);
      break;
    case JSOp::Div:
      masm.Fdiv(output, lhs, rhs);
      break;
    default:
      MOZ_CRASH("unexpected opcode");
  }
}

void CodeGenerator::visitMathF(LMathF* math) {
  ARMFPRegister lhs(ToFloatRegister(math->lhs()), 32);
  ARMFPRegister rhs(ToFloatRegister(math->rhs()), 32);
  ARMFPRegister output(ToFloatRegister(math->output()), 32);

  switch (math->jsop()) {
    case JSOp::Add:
      masm.Fadd(output, lhs, rhs);
      break;
    case JSOp::Sub:
      masm.Fsub(output, lhs, rhs);
      break;
    case JSOp::Mul:
      masm.Fmul(output, lhs, rhs);
      break;
    case JSOp::Div:
      masm.Fdiv(output, lhs, rhs);
      break;
    default:
      MOZ_CRASH("unexpected opcode");
  }
}

void CodeGenerator::visitClzI(LClzI* lir) {
  ARMRegister input = toWRegister(lir->input());
  ARMRegister output = toWRegister(lir->output());
  masm.Clz(output, input);
}

void CodeGenerator::visitCtzI(LCtzI* lir) {
  Register input = ToRegister(lir->input());
  Register output = ToRegister(lir->output());
  masm.ctz32(input, output, /* knownNotZero = */ false);
}

void CodeGenerator::visitTruncateDToInt32(LTruncateDToInt32* ins) {
  emitTruncateDouble(ToFloatRegister(ins->input()), ToRegister(ins->output()),
                     ins->mir());
}

void CodeGenerator::visitNearbyInt(LNearbyInt* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  FloatRegister output = ToFloatRegister(lir->output());

  RoundingMode roundingMode = lir->mir()->roundingMode();
  masm.nearbyIntDouble(roundingMode, input, output);
}

void CodeGenerator::visitNearbyIntF(LNearbyIntF* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  FloatRegister output = ToFloatRegister(lir->output());

  RoundingMode roundingMode = lir->mir()->roundingMode();
  masm.nearbyIntFloat32(roundingMode, input, output);
}

void CodeGenerator::visitWasmBuiltinTruncateDToInt32(
    LWasmBuiltinTruncateDToInt32* lir) {
  emitTruncateDouble(ToFloatRegister(lir->getOperand(0)),
                     ToRegister(lir->getDef(0)), lir->mir());
}

void CodeGenerator::visitTruncateFToInt32(LTruncateFToInt32* ins) {
  emitTruncateFloat32(ToFloatRegister(ins->input()), ToRegister(ins->output()),
                      ins->mir());
}

void CodeGenerator::visitWasmBuiltinTruncateFToInt32(
    LWasmBuiltinTruncateFToInt32* lir) {
  emitTruncateFloat32(ToFloatRegister(lir->getOperand(0)),
                      ToRegister(lir->getDef(0)), lir->mir());
}

FrameSizeClass FrameSizeClass::FromDepth(uint32_t frameDepth) {
  return FrameSizeClass::None();
}

FrameSizeClass FrameSizeClass::ClassLimit() { return FrameSizeClass(0); }

uint32_t FrameSizeClass::frameSize() const {
  MOZ_CRASH("arm64 does not use frame size classes");
}

ValueOperand CodeGeneratorARM64::ToValue(LInstruction* ins, size_t pos) {
  return ValueOperand(ToRegister(ins->getOperand(pos)));
}

ValueOperand CodeGeneratorARM64::ToTempValue(LInstruction* ins, size_t pos) {
  MOZ_CRASH("CodeGeneratorARM64::ToTempValue");
}

void CodeGenerator::visitValue(LValue* value) {
  ValueOperand result = ToOutValue(value);
  masm.moveValue(value->value(), result);
}

void CodeGenerator::visitBox(LBox* box) {
  const LAllocation* in = box->getOperand(0);
  ValueOperand result = ToOutValue(box);

  masm.moveValue(TypedOrValueRegister(box->type(), ToAnyRegister(in)), result);
}

void CodeGenerator::visitUnbox(LUnbox* unbox) {
  MUnbox* mir = unbox->mir();

  Register result = ToRegister(unbox->output());

  if (mir->fallible()) {
    const ValueOperand value = ToValue(unbox, LUnbox::Input);
    Label bail;
    switch (mir->type()) {
      case MIRType::Int32:
        masm.fallibleUnboxInt32(value, result, &bail);
        break;
      case MIRType::Boolean:
        masm.fallibleUnboxBoolean(value, result, &bail);
        break;
      case MIRType::Object:
        masm.fallibleUnboxObject(value, result, &bail);
        break;
      case MIRType::String:
        masm.fallibleUnboxString(value, result, &bail);
        break;
      case MIRType::Symbol:
        masm.fallibleUnboxSymbol(value, result, &bail);
        break;
      case MIRType::BigInt:
        masm.fallibleUnboxBigInt(value, result, &bail);
        break;
      default:
        MOZ_CRASH("Given MIRType cannot be unboxed.");
    }
    bailoutFrom(&bail, unbox->snapshot());
    return;
  }

  // Infallible unbox.

  ValueOperand input = ToValue(unbox, LUnbox::Input);

#ifdef DEBUG
  // Assert the types match.
  JSValueTag tag = MIRTypeToTag(mir->type());
  Label ok;
  {
    ScratchTagScope scratch(masm, input);
    masm.splitTagForTest(input, scratch);
    masm.cmpTag(scratch, ImmTag(tag));
  }
  masm.B(&ok, Assembler::Condition::Equal);
  masm.assumeUnreachable("Infallible unbox type mismatch");
  masm.bind(&ok);
#endif

  switch (mir->type()) {
    case MIRType::Int32:
      masm.unboxInt32(input, result);
      break;
    case MIRType::Boolean:
      masm.unboxBoolean(input, result);
      break;
    case MIRType::Object:
      masm.unboxObject(input, result);
      break;
    case MIRType::String:
      masm.unboxString(input, result);
      break;
    case MIRType::Symbol:
      masm.unboxSymbol(input, result);
      break;
    case MIRType::BigInt:
      masm.unboxBigInt(input, result);
      break;
    default:
      MOZ_CRASH("Given MIRType cannot be unboxed.");
  }
}

void CodeGenerator::visitDouble(LDouble* ins) {
  ARMFPRegister output(ToFloatRegister(ins->getDef(0)), 64);
  masm.Fmov(output, ins->getDouble());
}

void CodeGenerator::visitFloat32(LFloat32* ins) {
  ARMFPRegister output(ToFloatRegister(ins->getDef(0)), 32);
  masm.Fmov(output, ins->getFloat());
}

void CodeGenerator::visitTestDAndBranch(LTestDAndBranch* test) {
  const LAllocation* opd = test->input();
  MBasicBlock* ifTrue = test->ifTrue();
  MBasicBlock* ifFalse = test->ifFalse();

  masm.Fcmp(ARMFPRegister(ToFloatRegister(opd), 64), 0.0);

  // If the compare set the 0 bit, then the result is definitely false.
  jumpToBlock(ifFalse, Assembler::Zero);

  // Overflow means one of the operands was NaN, which is also false.
  jumpToBlock(ifFalse, Assembler::Overflow);
  jumpToBlock(ifTrue);
}

void CodeGenerator::visitTestFAndBranch(LTestFAndBranch* test) {
  const LAllocation* opd = test->input();
  MBasicBlock* ifTrue = test->ifTrue();
  MBasicBlock* ifFalse = test->ifFalse();

  masm.Fcmp(ARMFPRegister(ToFloatRegister(opd), 32), 0.0);

  // If the compare set the 0 bit, then the result is definitely false.
  jumpToBlock(ifFalse, Assembler::Zero);

  // Overflow means one of the operands was NaN, which is also false.
  jumpToBlock(ifFalse, Assembler::Overflow);
  jumpToBlock(ifTrue);
}

void CodeGenerator::visitCompareD(LCompareD* comp) {
  const FloatRegister left = ToFloatRegister(comp->left());
  const FloatRegister right = ToFloatRegister(comp->right());
  ARMRegister output = toWRegister(comp->output());
  Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

  masm.compareDouble(cond, left, right);
  masm.cset(output, Assembler::ConditionFromDoubleCondition(cond));
}

void CodeGenerator::visitCompareF(LCompareF* comp) {
  const FloatRegister left = ToFloatRegister(comp->left());
  const FloatRegister right = ToFloatRegister(comp->right());
  ARMRegister output = toWRegister(comp->output());
  Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

  masm.compareFloat(cond, left, right);
  masm.cset(output, Assembler::ConditionFromDoubleCondition(cond));
}

void CodeGenerator::visitCompareDAndBranch(LCompareDAndBranch* comp) {
  const FloatRegister left = ToFloatRegister(comp->left());
  const FloatRegister right = ToFloatRegister(comp->right());
  Assembler::DoubleCondition doubleCond =
      JSOpToDoubleCondition(comp->cmpMir()->jsop());
  Assembler::Condition cond =
      Assembler::ConditionFromDoubleCondition(doubleCond);

  masm.compareDouble(doubleCond, left, right);
  emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void CodeGenerator::visitCompareFAndBranch(LCompareFAndBranch* comp) {
  const FloatRegister left = ToFloatRegister(comp->left());
  const FloatRegister right = ToFloatRegister(comp->right());
  Assembler::DoubleCondition doubleCond =
      JSOpToDoubleCondition(comp->cmpMir()->jsop());
  Assembler::Condition cond =
      Assembler::ConditionFromDoubleCondition(doubleCond);

  masm.compareFloat(doubleCond, left, right);
  emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void CodeGenerator::visitBitAndAndBranch(LBitAndAndBranch* baab) {
  if (baab->right()->isConstant()) {
    masm.Tst(toWRegister(baab->left()), Operand(ToInt32(baab->right())));
  } else {
    masm.Tst(toWRegister(baab->left()), toWRegister(baab->right()));
  }
  emitBranch(baab->cond(), baab->ifTrue(), baab->ifFalse());
}

// See ../CodeGenerator.cpp for more information.
void CodeGenerator::visitWasmRegisterResult(LWasmRegisterResult* lir) {}

void CodeGenerator::visitWasmUint32ToDouble(LWasmUint32ToDouble* lir) {
  masm.convertUInt32ToDouble(ToRegister(lir->input()),
                             ToFloatRegister(lir->output()));
}

void CodeGenerator::visitWasmUint32ToFloat32(LWasmUint32ToFloat32* lir) {
  masm.convertUInt32ToFloat32(ToRegister(lir->input()),
                              ToFloatRegister(lir->output()));
}

void CodeGenerator::visitNotI(LNotI* ins) {
  ARMRegister input = toWRegister(ins->input());
  ARMRegister output = toWRegister(ins->output());

  masm.Cmp(input, ZeroRegister32);
  masm.Cset(output, Assembler::Zero);
}

//        NZCV
// NAN -> 0011
// ==  -> 0110
// <   -> 1000
// >   -> 0010
void CodeGenerator::visitNotD(LNotD* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 64);
  ARMRegister output = toWRegister(ins->output());

  // Set output to 1 if input compares equal to 0.0, else 0.
  masm.Fcmp(input, 0.0);
  masm.Cset(output, Assembler::Equal);

  // Comparison with NaN sets V in the NZCV register.
  // If the input was NaN, output must now be zero, so it can be incremented.
  // The instruction is read: "output = if NoOverflow then output else 0+1".
  masm.Csinc(output, output, ZeroRegister32, Assembler::NoOverflow);
}

void CodeGenerator::visitNotF(LNotF* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 32);
  ARMRegister output = toWRegister(ins->output());

  // Set output to 1 input compares equal to 0.0, else 0.
  masm.Fcmp(input, 0.0);
  masm.Cset(output, Assembler::Equal);

  // Comparison with NaN sets V in the NZCV register.
  // If the input was NaN, output must now be zero, so it can be incremented.
  // The instruction is read: "output = if NoOverflow then output else 0+1".
  masm.Csinc(output, output, ZeroRegister32, Assembler::NoOverflow);
}

void CodeGeneratorARM64::generateInvalidateEpilogue() {
  // Ensure that there is enough space in the buffer for the OsiPoint patching
  // to occur. Otherwise, we could overwrite the invalidation epilogue.
  for (size_t i = 0; i < sizeof(void*); i += Assembler::NopSize()) {
    masm.nop();
  }

  masm.bind(&invalidate_);

  // Push the return address of the point that we bailout out onto the stack.
  masm.push(lr);

  // Push the Ion script onto the stack (when we determine what that pointer
  // is).
  invalidateEpilogueData_ = masm.pushWithPatch(ImmWord(uintptr_t(-1)));

  // Jump to the invalidator which will replace the current frame.
  TrampolinePtr thunk = gen->jitRuntime()->getInvalidationThunk();
  masm.jump(thunk);
}

template <class U>
Register getBase(U* mir) {
  switch (mir->base()) {
    case U::Heap:
      return HeapReg;
  }
  return InvalidReg;
}

void CodeGenerator::visitAsmJSLoadHeap(LAsmJSLoadHeap* ins) {
  const MAsmJSLoadHeap* mir = ins->mir();
  MOZ_ASSERT(!mir->hasMemoryBase());

  const LAllocation* ptr = ins->ptr();
  const LAllocation* boundsCheckLimit = ins->boundsCheckLimit();

  Register ptrReg = ToRegister(ptr);
  Scalar::Type accessType = mir->accessType();
  bool isFloat = accessType == Scalar::Float32 || accessType == Scalar::Float64;
  Label done;

  if (mir->needsBoundsCheck()) {
    Label boundsCheckPassed;
    Register boundsCheckLimitReg = ToRegister(boundsCheckLimit);
    masm.wasmBoundsCheck32(Assembler::Below, ptrReg, boundsCheckLimitReg,
                           &boundsCheckPassed);
    // Return a default value in case of a bounds-check failure.
    if (isFloat) {
      if (accessType == Scalar::Float32) {
        masm.loadConstantFloat32(GenericNaN(), ToFloatRegister(ins->output()));
      } else {
        masm.loadConstantDouble(GenericNaN(), ToFloatRegister(ins->output()));
      }
    } else {
      masm.Mov(ARMRegister(ToRegister(ins->output()), 64), 0);
    }
    masm.jump(&done);
    masm.bind(&boundsCheckPassed);
  }

  MemOperand addr(ARMRegister(HeapReg, 64), ARMRegister(ptrReg, 64));
  switch (accessType) {
    case Scalar::Int8:
      masm.Ldrb(toWRegister(ins->output()), addr);
      masm.Sxtb(toWRegister(ins->output()), toWRegister(ins->output()));
      break;
    case Scalar::Uint8:
      masm.Ldrb(toWRegister(ins->output()), addr);
      break;
    case Scalar::Int16:
      masm.Ldrh(toWRegister(ins->output()), addr);
      masm.Sxth(toWRegister(ins->output()), toWRegister(ins->output()));
      break;
    case Scalar::Uint16:
      masm.Ldrh(toWRegister(ins->output()), addr);
      break;
    case Scalar::Int32:
    case Scalar::Uint32:
      masm.Ldr(toWRegister(ins->output()), addr);
      break;
    case Scalar::Float64:
      masm.Ldr(ARMFPRegister(ToFloatRegister(ins->output()), 64), addr);
      break;
    case Scalar::Float32:
      masm.Ldr(ARMFPRegister(ToFloatRegister(ins->output()), 32), addr);
      break;
    default:
      MOZ_CRASH("unexpected array type");
  }
  if (done.used()) {
    masm.bind(&done);
  }
}

void CodeGenerator::visitAsmJSStoreHeap(LAsmJSStoreHeap* ins) {
  const MAsmJSStoreHeap* mir = ins->mir();
  MOZ_ASSERT(!mir->hasMemoryBase());

  const LAllocation* ptr = ins->ptr();
  const LAllocation* boundsCheckLimit = ins->boundsCheckLimit();

  Register ptrReg = ToRegister(ptr);

  Label done;
  if (mir->needsBoundsCheck()) {
    Register boundsCheckLimitReg = ToRegister(boundsCheckLimit);
    masm.wasmBoundsCheck32(Assembler::AboveOrEqual, ptrReg, boundsCheckLimitReg,
                           &done);
  }

  MemOperand addr(ARMRegister(HeapReg, 64), ARMRegister(ptrReg, 64));
  switch (mir->accessType()) {
    case Scalar::Int8:
    case Scalar::Uint8:
      masm.Strb(toWRegister(ins->value()), addr);
      break;
    case Scalar::Int16:
    case Scalar::Uint16:
      masm.Strh(toWRegister(ins->value()), addr);
      break;
    case Scalar::Int32:
    case Scalar::Uint32:
      masm.Str(toWRegister(ins->value()), addr);
      break;
    case Scalar::Float64:
      masm.Str(ARMFPRegister(ToFloatRegister(ins->value()), 64), addr);
      break;
    case Scalar::Float32:
      masm.Str(ARMFPRegister(ToFloatRegister(ins->value()), 32), addr);
      break;
    default:
      MOZ_CRASH("unexpected array type");
  }
  if (done.used()) {
    masm.bind(&done);
  }
}

void CodeGenerator::visitWasmCompareExchangeHeap(
    LWasmCompareExchangeHeap* ins) {
  MWasmCompareExchangeHeap* mir = ins->mir();

  Register ptr = ToRegister(ins->ptr());
  Register oldval = ToRegister(ins->oldValue());
  Register newval = ToRegister(ins->newValue());
  Register out = ToRegister(ins->output());
  MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

  BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->access().offset());

  if (mir->access().type() == Scalar::Int64) {
    masm.wasmCompareExchange64(mir->access(), srcAddr, Register64(oldval),
                               Register64(newval), Register64(out));
  } else {
    masm.wasmCompareExchange(mir->access(), srcAddr, oldval, newval, out);
  }
}

void CodeGenerator::visitWasmAtomicExchangeHeap(LWasmAtomicExchangeHeap* ins) {
  MWasmAtomicExchangeHeap* mir = ins->mir();

  Register ptr = ToRegister(ins->ptr());
  Register oldval = ToRegister(ins->value());
  Register out = ToRegister(ins->output());
  MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

  BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->access().offset());

  if (mir->access().type() == Scalar::Int64) {
    masm.wasmAtomicExchange64(mir->access(), srcAddr, Register64(oldval),
                              Register64(out));
  } else {
    masm.wasmAtomicExchange(mir->access(), srcAddr, oldval, out);
  }
}

void CodeGenerator::visitWasmAtomicBinopHeap(LWasmAtomicBinopHeap* ins) {
  MWasmAtomicBinopHeap* mir = ins->mir();

  MOZ_ASSERT(mir->hasUses());

  Register ptr = ToRegister(ins->ptr());
  Register value = ToRegister(ins->value());
  Register flagTemp = ToRegister(ins->flagTemp());
  Register out = ToRegister(ins->output());
  MOZ_ASSERT(ins->temp()->isBogusTemp());
  MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

  BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->access().offset());
  AtomicOp op = mir->operation();

  if (mir->access().type() == Scalar::Int64) {
    masm.wasmAtomicFetchOp64(mir->access(), op, Register64(value), srcAddr,
                             Register64(flagTemp), Register64(out));
  } else {
    masm.wasmAtomicFetchOp(mir->access(), op, value, srcAddr, flagTemp, out);
  }
}

void CodeGenerator::visitWasmAtomicBinopHeapForEffect(
    LWasmAtomicBinopHeapForEffect* ins) {
  MWasmAtomicBinopHeap* mir = ins->mir();

  MOZ_ASSERT(!mir->hasUses());

  Register ptr = ToRegister(ins->ptr());
  Register value = ToRegister(ins->value());
  Register flagTemp = ToRegister(ins->flagTemp());
  MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

  BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->access().offset());
  AtomicOp op = mir->operation();

  if (mir->access().type() == Scalar::Int64) {
    masm.wasmAtomicEffectOp64(mir->access(), op, Register64(value), srcAddr,
                              Register64(flagTemp));
  } else {
    masm.wasmAtomicEffectOp(mir->access(), op, value, srcAddr, flagTemp);
  }
}

void CodeGenerator::visitWasmStackArg(LWasmStackArg* ins) {
  const MWasmStackArg* mir = ins->mir();
  Address dst(masm.getStackPointer(), mir->spOffset());
  if (ins->arg()->isConstant()) {
    masm.storePtr(ImmWord(ToInt32(ins->arg())), dst);
  } else if (ins->arg()->isGeneralReg()) {
    masm.storePtr(ToRegister(ins->arg()), dst);
  } else {
    switch (mir->input()->type()) {
      case MIRType::Double:
        masm.storeDouble(ToFloatRegister(ins->arg()), dst);
        return;
      case MIRType::Float32:
        masm.storeFloat32(ToFloatRegister(ins->arg()), dst);
        return;
#ifdef ENABLE_WASM_SIMD
      case MIRType::Simd128:
        masm.storeUnalignedSimd128(ToFloatRegister(ins->arg()), dst);
        return;
#endif
      default:
        break;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE(
        "unexpected mir type in WasmStackArg");
  }
}

void CodeGenerator::visitUDiv(LUDiv* ins) {
  MDiv* mir = ins->mir();
  Register lhs = ToRegister(ins->lhs());
  Register rhs = ToRegister(ins->rhs());
  Register output = ToRegister(ins->output());
  ARMRegister lhs32 = ARMRegister(lhs, 32);
  ARMRegister rhs32 = ARMRegister(rhs, 32);
  ARMRegister output32 = ARMRegister(output, 32);

  // Prevent divide by zero.
  if (mir->canBeDivideByZero()) {
    if (mir->isTruncated()) {
      if (mir->trapOnError()) {
        Label nonZero;
        masm.branchTest32(Assembler::NonZero, rhs, rhs, &nonZero);
        masm.wasmTrap(wasm::Trap::IntegerDivideByZero, mir->bytecodeOffset());
        masm.bind(&nonZero);
      } else {
        // ARM64 UDIV instruction will return 0 when divided by 0.
        // No need for extra tests.
      }
    } else {
      bailoutTest32(Assembler::Zero, rhs, rhs, ins->snapshot());
    }
  }

  // Unsigned division.
  masm.Udiv(output32, lhs32, rhs32);

  // If the remainder is > 0, bailout since this must be a double.
  if (!mir->canTruncateRemainder()) {
    Register remainder = ToRegister(ins->remainder());
    ARMRegister remainder32 = ARMRegister(remainder, 32);

    // Compute the remainder: remainder = lhs - (output * rhs).
    masm.Msub(remainder32, output32, rhs32, lhs32);

    bailoutTest32(Assembler::NonZero, remainder, remainder, ins->snapshot());
  }

  // Unsigned div can return a value that's not a signed int32.
  // If our users aren't expecting that, bail.
  if (!mir->isTruncated()) {
    bailoutTest32(Assembler::Signed, output, output, ins->snapshot());
  }
}

void CodeGenerator::visitUMod(LUMod* ins) {
  MMod* mir = ins->mir();
  ARMRegister lhs = toWRegister(ins->lhs());
  ARMRegister rhs = toWRegister(ins->rhs());
  ARMRegister output = toWRegister(ins->output());
  Label done;

  if (mir->canBeDivideByZero()) {
    if (mir->isTruncated()) {
      if (mir->trapOnError()) {
        Label nonZero;
        masm.Cbnz(rhs, &nonZero);
        masm.wasmTrap(wasm::Trap::IntegerDivideByZero, mir->bytecodeOffset());
        masm.bind(&nonZero);
      } else {
        // Truncated division by zero yields integer zero.
        masm.Mov(output, rhs);
        masm.Cbz(rhs, &done);
      }
    } else {
      // Non-truncated division by zero produces a non-integer.
      masm.Cmp(rhs, Operand(0));
      bailoutIf(Assembler::Equal, ins->snapshot());
    }
  }

  // Unsigned division.
  masm.Udiv(output, lhs, rhs);

  // Compute the remainder: output = lhs - (output * rhs).
  masm.Msub(output, output, rhs, lhs);

  if (!mir->isTruncated()) {
    // Bail if the output would be negative.
    //
    // LUMod inputs may be Uint32, so care is taken to ensure the result
    // is not unexpectedly signed.
    bailoutCmp32(Assembler::LessThan, output, Imm32(0), ins->snapshot());
  }

  if (done.used()) {
    masm.bind(&done);
  }
}

void CodeGenerator::visitEffectiveAddress(LEffectiveAddress* ins) {
  const MEffectiveAddress* mir = ins->mir();
  const ARMRegister base = toWRegister(ins->base());
  const ARMRegister index = toWRegister(ins->index());
  const ARMRegister output = toWRegister(ins->output());

  masm.Add(output, base, Operand(index, vixl::LSL, mir->scale()));
  masm.Add(output, output, Operand(mir->displacement()));
}

void CodeGenerator::visitNegI(LNegI* ins) {
  const ARMRegister input = toWRegister(ins->input());
  const ARMRegister output = toWRegister(ins->output());
  masm.Neg(output, input);
}

void CodeGenerator::visitNegD(LNegD* ins) {
  const ARMFPRegister input(ToFloatRegister(ins->input()), 64);
  const ARMFPRegister output(ToFloatRegister(ins->output()), 64);
  masm.Fneg(output, input);
}

void CodeGenerator::visitNegF(LNegF* ins) {
  const ARMFPRegister input(ToFloatRegister(ins->input()), 32);
  const ARMFPRegister output(ToFloatRegister(ins->output()), 32);
  masm.Fneg(output, input);
}

void CodeGenerator::visitCompareExchangeTypedArrayElement(
    LCompareExchangeTypedArrayElement* lir) {
  Register elements = ToRegister(lir->elements());
  AnyRegister output = ToAnyRegister(lir->output());
  Register temp =
      lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());

  Register oldval = ToRegister(lir->oldval());
  Register newval = ToRegister(lir->newval());

  Scalar::Type arrayType = lir->mir()->arrayType();

  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), arrayType);
    masm.compareExchangeJS(arrayType, Synchronization::Full(), dest, oldval,
                           newval, temp, output);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(arrayType));
    masm.compareExchangeJS(arrayType, Synchronization::Full(), dest, oldval,
                           newval, temp, output);
  }
}

void CodeGenerator::visitAtomicExchangeTypedArrayElement(
    LAtomicExchangeTypedArrayElement* lir) {
  Register elements = ToRegister(lir->elements());
  AnyRegister output = ToAnyRegister(lir->output());
  Register temp =
      lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());

  Register value = ToRegister(lir->value());

  Scalar::Type arrayType = lir->mir()->arrayType();

  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), arrayType);
    masm.atomicExchangeJS(arrayType, Synchronization::Full(), dest, value, temp,
                          output);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(arrayType));
    masm.atomicExchangeJS(arrayType, Synchronization::Full(), dest, value, temp,
                          output);
  }
}

void CodeGenerator::visitAtomicLoad64(LAtomicLoad64* lir) {
  Register elements = ToRegister(lir->elements());
  Register temp = ToRegister(lir->temp());
  Register64 temp64 = ToRegister64(lir->temp64());
  Register out = ToRegister(lir->output());

  const MLoadUnboxedScalar* mir = lir->mir();

  Scalar::Type storageType = mir->storageType();

  auto sync = Synchronization::Load();

  masm.memoryBarrierBefore(sync);
  if (lir->index()->isConstant()) {
    Address source =
        ToAddress(elements, lir->index(), storageType, mir->offsetAdjustment());
    masm.load64(source, temp64);
  } else {
    BaseIndex source(elements, ToRegister(lir->index()),
                     ScaleFromScalarType(storageType), mir->offsetAdjustment());
    masm.load64(source, temp64);
  }
  masm.memoryBarrierAfter(sync);

  emitCreateBigInt(lir, storageType, temp64, out, temp);
}

void CodeGenerator::visitAtomicStore64(LAtomicStore64* lir) {
  Register elements = ToRegister(lir->elements());
  Register value = ToRegister(lir->value());
  Register64 temp1 = ToRegister64(lir->temp1());

  Scalar::Type writeType = lir->mir()->writeType();

  masm.loadBigInt64(value, temp1);

  auto sync = Synchronization::Store();

  masm.memoryBarrierBefore(sync);
  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), writeType);
    masm.store64(temp1, dest);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(writeType));
    masm.store64(temp1, dest);
  }
  masm.memoryBarrierAfter(sync);
}

void CodeGenerator::visitCompareExchangeTypedArrayElement64(
    LCompareExchangeTypedArrayElement64* lir) {
  Register elements = ToRegister(lir->elements());
  Register oldval = ToRegister(lir->oldval());
  Register newval = ToRegister(lir->newval());
  Register64 temp1 = ToRegister64(lir->temp1());
  Register64 temp2 = ToRegister64(lir->temp2());
  Register out = ToRegister(lir->output());
  Register64 tempOut(out);

  Scalar::Type arrayType = lir->mir()->arrayType();

  masm.loadBigInt64(oldval, temp1);
  masm.loadBigInt64(newval, tempOut);

  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), arrayType);
    masm.compareExchange64(Synchronization::Full(), dest, temp1, tempOut,
                           temp2);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(arrayType));
    masm.compareExchange64(Synchronization::Full(), dest, temp1, tempOut,
                           temp2);
  }

  emitCreateBigInt(lir, arrayType, temp2, out, temp1.scratchReg());
}

void CodeGenerator::visitAtomicExchangeTypedArrayElement64(
    LAtomicExchangeTypedArrayElement64* lir) {
  Register elements = ToRegister(lir->elements());
  Register value = ToRegister(lir->value());
  Register64 temp1 = ToRegister64(lir->temp1());
  Register64 temp2 = Register64(ToRegister(lir->temp2()));
  Register out = ToRegister(lir->output());

  Scalar::Type arrayType = lir->mir()->arrayType();

  masm.loadBigInt64(value, temp1);

  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), arrayType);
    masm.atomicExchange64(Synchronization::Full(), dest, temp1, temp2);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(arrayType));
    masm.atomicExchange64(Synchronization::Full(), dest, temp1, temp2);
  }

  emitCreateBigInt(lir, arrayType, temp2, out, temp1.scratchReg());
}

void CodeGenerator::visitAtomicTypedArrayElementBinop64(
    LAtomicTypedArrayElementBinop64* lir) {
  MOZ_ASSERT(!lir->mir()->isForEffect());

  Register elements = ToRegister(lir->elements());
  Register value = ToRegister(lir->value());
  Register64 temp1 = ToRegister64(lir->temp1());
  Register64 temp2 = ToRegister64(lir->temp2());
  Register out = ToRegister(lir->output());
  Register64 tempOut = Register64(out);

  Scalar::Type arrayType = lir->mir()->arrayType();
  AtomicOp atomicOp = lir->mir()->operation();

  masm.loadBigInt64(value, temp1);

  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), arrayType);
    masm.atomicFetchOp64(Synchronization::Full(), atomicOp, temp1, dest,
                         tempOut, temp2);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(arrayType));
    masm.atomicFetchOp64(Synchronization::Full(), atomicOp, temp1, dest,
                         tempOut, temp2);
  }

  emitCreateBigInt(lir, arrayType, temp2, out, temp1.scratchReg());
}

void CodeGenerator::visitAtomicTypedArrayElementBinopForEffect64(
    LAtomicTypedArrayElementBinopForEffect64* lir) {
  MOZ_ASSERT(lir->mir()->isForEffect());

  Register elements = ToRegister(lir->elements());
  Register value = ToRegister(lir->value());
  Register64 temp1 = ToRegister64(lir->temp1());
  Register64 temp2 = ToRegister64(lir->temp2());

  Scalar::Type arrayType = lir->mir()->arrayType();
  AtomicOp atomicOp = lir->mir()->operation();

  masm.loadBigInt64(value, temp1);

  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), arrayType);
    masm.atomicEffectOp64(Synchronization::Full(), atomicOp, temp1, dest,
                          temp2);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(arrayType));
    masm.atomicEffectOp64(Synchronization::Full(), atomicOp, temp1, dest,
                          temp2);
  }
}

void CodeGenerator::visitAddI64(LAddI64* lir) {
  const LInt64Allocation lhs = lir->getInt64Operand(LAddI64::Lhs);
  const LInt64Allocation rhs = lir->getInt64Operand(LAddI64::Rhs);

  MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

  if (IsConstant(rhs)) {
    masm.add64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
    return;
  }

  masm.add64(ToRegister64(rhs), ToRegister64(lhs));
}

void CodeGenerator::visitClzI64(LClzI64* ins) {
  masm.clz64(ToRegister64(ins->getInt64Operand(0)), ToRegister(ins->output()));
}

void CodeGenerator::visitCtzI64(LCtzI64* ins) {
  masm.ctz64(ToRegister64(ins->getInt64Operand(0)), ToRegister(ins->output()));
}

void CodeGenerator::visitMulI64(LMulI64* lir) {
  const Register64 lhs = ToRegister64(lir->getInt64Operand(LMulI64::Lhs));
  const Register64 rhs = ToRegister64(lir->getInt64Operand(LMulI64::Rhs));
  const mozilla::DebugOnly<Register64> output = ToOutRegister64(lir);
  MOZ_ASSERT(lhs == output);

  masm.mul64(rhs, lhs, Register::Invalid());
}

void CodeGenerator::visitNotI64(LNotI64* lir) {
  const Register64 input = ToRegister64(lir->getInt64Operand(0));
  const Register64 output = ToOutRegister64(lir);
  masm.Cmp(ARMRegister(input.reg, 64), ZeroRegister64);
  masm.Cset(ARMRegister(output.reg, 64), Assembler::Zero);
}

void CodeGenerator::visitSubI64(LSubI64* lir) {
  const LInt64Allocation lhs = lir->getInt64Operand(LSubI64::Lhs);
  const LInt64Allocation rhs = lir->getInt64Operand(LSubI64::Rhs);

  MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

  if (IsConstant(rhs)) {
    masm.sub64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
    return;
  }

  masm.sub64(ToRegister64(rhs), ToRegister64(lhs));
}

void CodeGenerator::visitPopcntI(LPopcntI* ins) {
  Register input = ToRegister(ins->input());
  Register output = ToRegister(ins->output());
  Register temp = ToRegister(ins->temp());
  masm.popcnt32(input, output, temp);
}

void CodeGenerator::visitBitOpI64(LBitOpI64* lir) {
  const LInt64Allocation lhs = lir->getInt64Operand(LBitOpI64::Lhs);
  const LInt64Allocation rhs = lir->getInt64Operand(LBitOpI64::Rhs);

  MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

  switch (lir->bitop()) {
    case JSOp::BitOr:
      if (IsConstant(rhs)) {
        masm.or64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
      } else {
        masm.or64(ToRegister64(rhs), ToRegister64(lhs));
      }
      break;
    case JSOp::BitXor:
      if (IsConstant(rhs)) {
        masm.xor64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
      } else {
        masm.xor64(ToRegister64(rhs), ToRegister64(lhs));
      }
      break;
    case JSOp::BitAnd:
      if (IsConstant(rhs)) {
        masm.and64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
      } else {
        masm.and64(ToRegister64(rhs), ToRegister64(lhs));
      }
      break;
    default:
      MOZ_CRASH("unexpected binary opcode");
  }
}

void CodeGenerator::visitShiftI64(LShiftI64* lir) {
  const LInt64Allocation lhs = lir->getInt64Operand(LShiftI64::Lhs);
  LAllocation* rhs = lir->getOperand(LShiftI64::Rhs);

  MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

  if (rhs->isConstant()) {
    int32_t shift = int32_t(rhs->toConstant()->toInt64() & 0x3F);
    switch (lir->bitop()) {
      case JSOp::Lsh:
        if (shift) {
          masm.lshift64(Imm32(shift), ToRegister64(lhs));
        }
        break;
      case JSOp::Rsh:
        if (shift) {
          masm.rshift64Arithmetic(Imm32(shift), ToRegister64(lhs));
        }
        break;
      case JSOp::Ursh:
        if (shift) {
          masm.rshift64(Imm32(shift), ToRegister64(lhs));
        }
        break;
      default:
        MOZ_CRASH("Unexpected shift op");
    }
    return;
  }

  switch (lir->bitop()) {
    case JSOp::Lsh:
      masm.lshift64(ToRegister(rhs), ToRegister64(lhs));
      break;
    case JSOp::Rsh:
      masm.rshift64Arithmetic(ToRegister(rhs), ToRegister64(lhs));
      break;
    case JSOp::Ursh:
      masm.rshift64(ToRegister(rhs), ToRegister64(lhs));
      break;
    default:
      MOZ_CRASH("Unexpected shift op");
  }
}

void CodeGenerator::visitWasmHeapBase(LWasmHeapBase* ins) {
  MOZ_ASSERT(ins->tlsPtr()->isBogus());
  masm.movePtr(HeapReg, ToRegister(ins->output()));
}

void CodeGenerator::visitWasmLoad(LWasmLoad* lir) {
  const MWasmLoad* mir = lir->mir();
  masm.wasmLoad(mir->access(), HeapReg, ToRegister(lir->ptr()),
                ToAnyRegister(lir->output()));
}

void CodeGenerator::visitCopySignD(LCopySignD* ins) {
  MOZ_ASSERT(ins->getTemp(0)->isBogusTemp());
  MOZ_ASSERT(ins->getTemp(1)->isBogusTemp());
  masm.copySignDouble(ToFloatRegister(ins->getOperand(0)),
                      ToFloatRegister(ins->getOperand(1)),
                      ToFloatRegister(ins->getDef(0)));
}

void CodeGenerator::visitCopySignF(LCopySignF* ins) {
  MOZ_ASSERT(ins->getTemp(0)->isBogusTemp());
  MOZ_ASSERT(ins->getTemp(1)->isBogusTemp());
  masm.copySignFloat32(ToFloatRegister(ins->getOperand(0)),
                       ToFloatRegister(ins->getOperand(1)),
                       ToFloatRegister(ins->getDef(0)));
}

void CodeGenerator::visitPopcntI64(LPopcntI64* lir) {
  Register64 input = ToRegister64(lir->getInt64Operand(0));
  Register64 output = ToOutRegister64(lir);
  Register temp = ToRegister(lir->getTemp(0));
  masm.popcnt64(input, output, temp);
}

void CodeGenerator::visitRotateI64(LRotateI64* lir) {
  bool rotateLeft = lir->mir()->isLeftRotate();
  Register64 input = ToRegister64(lir->input());
  Register64 output = ToOutRegister64(lir);
  const LAllocation* count = lir->count();

  if (count->isConstant()) {
    int32_t c = int32_t(count->toConstant()->toInt64() & 0x3F);
    if (c == 0) {
      if (input != output) {
        masm.move64(input, output);
        return;
      }
    }
    if (rotateLeft) {
      masm.rotateLeft64(Imm32(c), input, output, InvalidReg);
    } else {
      masm.rotateRight64(Imm32(c), input, output, InvalidReg);
    }
  } else {
    Register c = ToRegister(count);
    if (rotateLeft) {
      masm.rotateLeft64(c, input, output, InvalidReg);
    } else {
      masm.rotateRight64(c, input, output, InvalidReg);
    }
  }
}

void CodeGenerator::visitWasmStore(LWasmStore* lir) {
  const MWasmStore* mir = lir->mir();
  masm.wasmStore(mir->access(), ToAnyRegister(lir->value()), HeapReg,
                 ToRegister(lir->ptr()));
}

void CodeGenerator::visitCompareI64(LCompareI64* lir) {
  MCompare* mir = lir->mir();
  MOZ_ASSERT(mir->compareType() == MCompare::Compare_Int64 ||
             mir->compareType() == MCompare::Compare_UInt64);

  const LInt64Allocation lhs = lir->getInt64Operand(LCompareI64::Lhs);
  const LInt64Allocation rhs = lir->getInt64Operand(LCompareI64::Rhs);
  Register lhsReg = ToRegister64(lhs).reg;
  Register output = ToRegister(lir->output());
  bool isSigned = mir->compareType() == MCompare::Compare_Int64;

  if (IsConstant(rhs)) {
    masm.cmpPtrSet(JSOpToCondition(lir->jsop(), isSigned), lhsReg,
                   ImmWord(ToInt64(rhs)), output);
  } else if (rhs.value().isGeneralReg()) {
    masm.cmpPtrSet(JSOpToCondition(lir->jsop(), isSigned), lhsReg,
                   ToRegister64(rhs).reg, output);
  } else {
    masm.cmpPtrSet(
        GetCondForSwappedOperands(JSOpToCondition(lir->jsop(), isSigned)),
        ToAddress(rhs.value()), lhsReg, output);
  }
}

void CodeGenerator::visitWasmSelect(LWasmSelect* lir) {
  MIRType mirType = lir->mir()->type();
  Register condReg = ToRegister(lir->condExpr());

  masm.test32(condReg, condReg);

  switch (mirType) {
    case MIRType::Int32:
    case MIRType::RefOrNull: {
      Register outReg = ToRegister(lir->output());
      Register trueReg = ToRegister(lir->trueExpr());
      Register falseReg = ToRegister(lir->falseExpr());

      // TODO: Not desirable on ARM64 to have this restriction, since CSEL can
      // have three independent registers as operands.
      MOZ_ASSERT(trueReg == outReg, "true reg is reused for input");

      if (mirType == MIRType::Int32) {
        masm.Csel(ARMRegister(outReg, 32), ARMRegister(trueReg, 32),
                  ARMRegister(falseReg, 32), Assembler::NonZero);
      } else {
        masm.Csel(ARMRegister(outReg, 64), ARMRegister(trueReg, 64),
                  ARMRegister(falseReg, 64), Assembler::NonZero);
      }
      break;
    }

    case MIRType::Float32:
    case MIRType::Double: {
      FloatRegister outReg = ToFloatRegister(lir->output());
      mozilla::DebugOnly<FloatRegister> trueReg =
          ToFloatRegister(lir->trueExpr());
      FloatRegister falseReg = ToFloatRegister(lir->falseExpr());

      // TODO: Again, not desirable, we can use FCSEL to do better.
      MOZ_ASSERT(trueReg.inspect() == outReg, "true reg is reused for input");

      Label done;
      masm.j(Assembler::NonZero, &done);

      if (mirType == MIRType::Float32) {
        masm.moveFloat32(falseReg, outReg);
      } else /* mirType == MIRType::Double */ {
        masm.moveDouble(falseReg, outReg);
      }
      masm.bind(&done);
      break;
    }

    default: {
      MOZ_CRASH("unhandled type in visitWasmSelect!");
    }
  }
}

void CodeGenerator::visitWasmLoadI64(LWasmLoadI64* lir) {
  const MWasmLoad* mir = lir->mir();
  masm.wasmLoadI64(mir->access(), HeapReg, ToRegister(lir->ptr()),
                   ToOutRegister64(lir));
}

void CodeGenerator::visitWasmStoreI64(LWasmStoreI64* lir) {
  const MWasmStore* mir = lir->mir();
  masm.wasmStoreI64(mir->access(), ToRegister64(lir->value()), HeapReg,
                    ToRegister(lir->ptr()));
}

void CodeGenerator::visitMemoryBarrier(LMemoryBarrier* ins) {
  masm.memoryBarrier(ins->type());
}

void CodeGenerator::visitWasmAddOffset(LWasmAddOffset* lir) {
  MWasmAddOffset* mir = lir->mir();
  Register base = ToRegister(lir->base());
  Register out = ToRegister(lir->output());

  masm.Adds(ARMRegister(out, 32), ARMRegister(base, 32),
            Operand(mir->offset()));

  Label ok;
  masm.j(Assembler::CarryClear, &ok);
  masm.wasmTrap(wasm::Trap::OutOfBounds, mir->bytecodeOffset());
  masm.bind(&ok);
}

void CodeGenerator::visitWasmSelectI64(LWasmSelectI64* lir) {
  MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);
  Register condReg = ToRegister(lir->condExpr());
  Register falseReg = ToRegister64(lir->falseExpr()).reg;
  Register64 outReg = ToOutRegister64(lir);

  // TODO: Not desirable on ARM64 to have this restriction, since CSEL can
  // have three independent registers as operands.
  MOZ_ASSERT(ToRegister64(lir->trueExpr()) == outReg,
             "true expr is reused for input");

  masm.test32(condReg, condReg);
  masm.Csel(ARMRegister(outReg.reg, 64), ARMRegister(outReg.reg, 64),
            ARMRegister(falseReg, 64), Assembler::NonZero);
}

void CodeGenerator::visitSignExtendInt64(LSignExtendInt64* ins) {
  Register64 input = ToRegister64(ins->getInt64Operand(0));
  Register64 output = ToOutRegister64(ins);
  switch (ins->mode()) {
    case MSignExtendInt64::Byte:
      masm.move8To64SignExtend(input.reg, output);
      break;
    case MSignExtendInt64::Half:
      masm.move16To64SignExtend(input.reg, output);
      break;
    case MSignExtendInt64::Word:
      masm.move32To64SignExtend(input.reg, output);
      break;
  }
}

void CodeGenerator::visitWasmReinterpret(LWasmReinterpret* lir) {
  MOZ_ASSERT(gen->compilingWasm());
  MWasmReinterpret* ins = lir->mir();

  MIRType to = ins->type();
  mozilla::DebugOnly<MIRType> from = ins->input()->type();

  switch (to) {
    case MIRType::Int32:
      MOZ_ASSERT(from == MIRType::Float32);
      masm.moveFloat32ToGPR(ToFloatRegister(lir->input()),
                            ToRegister(lir->output()));
      break;
    case MIRType::Float32:
      MOZ_ASSERT(from == MIRType::Int32);
      masm.moveGPRToFloat32(ToRegister(lir->input()),
                            ToFloatRegister(lir->output()));
      break;
    case MIRType::Double:
    case MIRType::Int64:
      MOZ_CRASH("not handled by this LIR opcode");
    default:
      MOZ_CRASH("unexpected WasmReinterpret");
  }
}

void CodeGenerator::visitWasmStackArgI64(LWasmStackArgI64* ins) {
  const MWasmStackArg* mir = ins->mir();
  Address dst(masm.getStackPointer(), mir->spOffset());
  if (IsConstant(ins->arg())) {
    masm.store64(Imm64(ToInt64(ins->arg())), dst);
  } else {
    masm.store64(ToRegister64(ins->arg()), dst);
  }
}

void CodeGenerator::visitTestI64AndBranch(LTestI64AndBranch* lir) {
  Register64 input = ToRegister64(lir->getInt64Operand(0));
  MBasicBlock* mirTrue = lir->ifTrue();
  MBasicBlock* mirFalse = lir->ifFalse();

  // Jump to the True block if NonZero.
  // Jump to the False block if Zero.
  if (isNextBlock(mirFalse->lir())) {
    masm.Cbnz(ARMRegister(input.reg, 64), getJumpLabelForBranch(mirTrue));
  } else {
    masm.Cbz(ARMRegister(input.reg, 64), getJumpLabelForBranch(mirFalse));
    if (!isNextBlock(mirTrue->lir())) {
      jumpToBlock(mirTrue);
    }
  }
}

void CodeGenerator::visitWrapInt64ToInt32(LWrapInt64ToInt32* lir) {
  const LAllocation* input = lir->getOperand(0);
  Register output = ToRegister(lir->output());

  if (lir->mir()->bottomHalf()) {
    if (input->isMemory()) {
      masm.load32(ToAddress(input), output);
    } else {
      // Really this is a 64-bit input register and we could use move64To32.
      masm.Mov(ARMRegister(output, 32), ARMRegister(ToRegister(input), 32));
    }
  } else {
    MOZ_CRASH("Not implemented.");
  }
}

void CodeGenerator::visitExtendInt32ToInt64(LExtendInt32ToInt64* lir) {
  Register input = ToRegister(lir->getOperand(0));
  Register64 output = ToOutRegister64(lir);

  if (lir->mir()->isUnsigned()) {
    masm.move32To64ZeroExtend(input, output);
  } else {
    masm.move32To64SignExtend(input, output);
  }
}

void CodeGenerator::visitCompareI64AndBranch(LCompareI64AndBranch* comp) {
  const MCompare* mir = comp->cmpMir();
  const mozilla::DebugOnly<MCompare::CompareType> type = mir->compareType();
  const LInt64Allocation left =
      comp->getInt64Operand(LCompareI64AndBranch::Lhs);
  const LInt64Allocation right =
      comp->getInt64Operand(LCompareI64AndBranch::Rhs);

  MOZ_ASSERT(type == MCompare::Compare_Int64 ||
             type == MCompare::Compare_UInt64);
  if (IsConstant(right)) {
    masm.Cmp(ARMRegister(ToRegister64(left).reg, 64), ToInt64(right));
  } else {
    masm.Cmp(ARMRegister(ToRegister64(left).reg, 64),
             ARMRegister(ToRegister64(right).reg, 64));
  }

  bool isSigned = mir->compareType() == MCompare::Compare_Int64;
  Assembler::Condition cond = JSOpToCondition(comp->jsop(), isSigned);
  emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void CodeGenerator::visitWasmTruncateToInt32(LWasmTruncateToInt32* lir) {
  auto input = ToFloatRegister(lir->input());
  auto output = ToRegister(lir->output());

  MWasmTruncateToInt32* mir = lir->mir();
  MIRType fromType = mir->input()->type();

  MOZ_ASSERT(fromType == MIRType::Double || fromType == MIRType::Float32);

  auto* ool = new (alloc()) OutOfLineWasmTruncateCheck(mir, input, output);
  addOutOfLineCode(ool, mir);

  Label* oolEntry = ool->entry();
  if (mir->isUnsigned()) {
    if (fromType == MIRType::Double) {
      masm.wasmTruncateDoubleToUInt32(input, output, mir->isSaturating(),
                                      oolEntry);
    } else if (fromType == MIRType::Float32) {
      masm.wasmTruncateFloat32ToUInt32(input, output, mir->isSaturating(),
                                       oolEntry);
    } else {
      MOZ_CRASH("unexpected type");
    }

    masm.bind(ool->rejoin());
    return;
  }

  if (fromType == MIRType::Double) {
    masm.wasmTruncateDoubleToInt32(input, output, mir->isSaturating(),
                                   oolEntry);
  } else if (fromType == MIRType::Float32) {
    masm.wasmTruncateFloat32ToInt32(input, output, mir->isSaturating(),
                                    oolEntry);
  } else {
    MOZ_CRASH("unexpected type");
  }

  masm.bind(ool->rejoin());
}

void CodeGenerator::visitWasmTruncateToInt64(LWasmTruncateToInt64* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register64 output = ToOutRegister64(lir);

  MWasmTruncateToInt64* mir = lir->mir();
  MIRType fromType = mir->input()->type();

  MOZ_ASSERT(fromType == MIRType::Double || fromType == MIRType::Float32);

  auto* ool = new (alloc()) OutOfLineWasmTruncateCheck(mir, input, output);
  addOutOfLineCode(ool, mir);

  Label* oolEntry = ool->entry();
  Label* oolRejoin = ool->rejoin();
  bool isSaturating = mir->isSaturating();

  if (fromType == MIRType::Double) {
    if (mir->isUnsigned()) {
      masm.wasmTruncateDoubleToUInt64(input, output, isSaturating, oolEntry,
                                      oolRejoin, InvalidFloatReg);
    } else {
      masm.wasmTruncateDoubleToInt64(input, output, isSaturating, oolEntry,
                                     oolRejoin, InvalidFloatReg);
    }
  } else {
    if (mir->isUnsigned()) {
      masm.wasmTruncateFloat32ToUInt64(input, output, isSaturating, oolEntry,
                                       oolRejoin, InvalidFloatReg);
    } else {
      masm.wasmTruncateFloat32ToInt64(input, output, isSaturating, oolEntry,
                                      oolRejoin, InvalidFloatReg);
    }
  }
}

void CodeGeneratorARM64::visitOutOfLineWasmTruncateCheck(
    OutOfLineWasmTruncateCheck* ool) {
  FloatRegister input = ool->input();
  Register output = ool->output();
  Register64 output64 = ool->output64();
  MIRType fromType = ool->fromType();
  MIRType toType = ool->toType();
  Label* oolRejoin = ool->rejoin();
  TruncFlags flags = ool->flags();
  wasm::BytecodeOffset off = ool->bytecodeOffset();

  if (fromType == MIRType::Float32) {
    if (toType == MIRType::Int32) {
      masm.oolWasmTruncateCheckF32ToI32(input, output, flags, off, oolRejoin);
    } else if (toType == MIRType::Int64) {
      masm.oolWasmTruncateCheckF32ToI64(input, output64, flags, off, oolRejoin);
    } else {
      MOZ_CRASH("unexpected type");
    }
  } else if (fromType == MIRType::Double) {
    if (toType == MIRType::Int32) {
      masm.oolWasmTruncateCheckF64ToI32(input, output, flags, off, oolRejoin);
    } else if (toType == MIRType::Int64) {
      masm.oolWasmTruncateCheckF64ToI64(input, output64, flags, off, oolRejoin);
    } else {
      MOZ_CRASH("unexpected type");
    }
  } else {
    MOZ_CRASH("unexpected type");
  }
}

void CodeGenerator::visitWasmReinterpretToI64(LWasmReinterpretToI64* lir) {
  MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);
  MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Double);
  masm.moveDoubleToGPR64(ToFloatRegister(lir->input()), ToOutRegister64(lir));
}

void CodeGenerator::visitWasmReinterpretFromI64(LWasmReinterpretFromI64* lir) {
  MOZ_ASSERT(lir->mir()->type() == MIRType::Double);
  MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Int64);
  masm.moveGPR64ToDouble(
      ToRegister64(lir->getInt64Operand(LWasmReinterpretFromI64::Input)),
      ToFloatRegister(lir->output()));
}

void CodeGenerator::visitAtomicTypedArrayElementBinop(
    LAtomicTypedArrayElementBinop* lir) {
  MOZ_ASSERT(!lir->mir()->isForEffect());

  AnyRegister output = ToAnyRegister(lir->output());
  Register elements = ToRegister(lir->elements());
  Register flagTemp = ToRegister(lir->temp1());
  Register outTemp =
      lir->temp2()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp2());
  Register value = ToRegister(lir->value());

  Scalar::Type arrayType = lir->mir()->arrayType();

  if (lir->index()->isConstant()) {
    Address mem = ToAddress(elements, lir->index(), arrayType);
    masm.atomicFetchOpJS(arrayType, Synchronization::Full(),
                         lir->mir()->operation(), value, mem, flagTemp, outTemp,
                         output);
  } else {
    BaseIndex mem(elements, ToRegister(lir->index()),
                  ScaleFromScalarType(arrayType));
    masm.atomicFetchOpJS(arrayType, Synchronization::Full(),
                         lir->mir()->operation(), value, mem, flagTemp, outTemp,
                         output);
  }
}

void CodeGenerator::visitAtomicTypedArrayElementBinopForEffect(
    LAtomicTypedArrayElementBinopForEffect* lir) {
  MOZ_ASSERT(lir->mir()->isForEffect());

  Register elements = ToRegister(lir->elements());
  Register flagTemp = ToRegister(lir->flagTemp());
  Register value = ToRegister(lir->value());

  Scalar::Type arrayType = lir->mir()->arrayType();

  if (lir->index()->isConstant()) {
    Address mem = ToAddress(elements, lir->index(), arrayType);
    masm.atomicEffectOpJS(arrayType, Synchronization::Full(),
                          lir->mir()->operation(), value, mem, flagTemp);
  } else {
    BaseIndex mem(elements, ToRegister(lir->index()),
                  ScaleFromScalarType(arrayType));
    masm.atomicEffectOpJS(arrayType, Synchronization::Full(),
                          lir->mir()->operation(), value, mem, flagTemp);
  }
}

void CodeGenerator::visitInt64ToFloatingPoint(LInt64ToFloatingPoint* lir) {
  Register64 input = ToRegister64(lir->getInt64Operand(0));
  FloatRegister output = ToFloatRegister(lir->output());

  MIRType outputType = lir->mir()->type();
  MOZ_ASSERT(outputType == MIRType::Double || outputType == MIRType::Float32);

  if (outputType == MIRType::Double) {
    if (lir->mir()->isUnsigned()) {
      masm.convertUInt64ToDouble(input, output, Register::Invalid());
    } else {
      masm.convertInt64ToDouble(input, output);
    }
  } else {
    if (lir->mir()->isUnsigned()) {
      masm.convertUInt64ToFloat32(input, output, Register::Invalid());
    } else {
      masm.convertInt64ToFloat32(input, output);
    }
  }
}

void CodeGenerator::visitDivOrModI64(LDivOrModI64* lir) {
  Register lhs = ToRegister(lir->lhs());
  Register rhs = ToRegister(lir->rhs());
  Register output = ToRegister(lir->output());

  Label done;

  // Handle divide by zero.
  if (lir->canBeDivideByZero()) {
    Label isNotDivByZero;
    masm.Cbnz(ARMRegister(rhs, 64), &isNotDivByZero);
    masm.wasmTrap(wasm::Trap::IntegerDivideByZero, lir->bytecodeOffset());
    masm.bind(&isNotDivByZero);
  }

  // Handle an integer overflow exception from INT64_MIN / -1.
  if (lir->canBeNegativeOverflow()) {
    Label noOverflow;
    masm.branchPtr(Assembler::NotEqual, lhs, ImmWord(INT64_MIN), &noOverflow);
    masm.branchPtr(Assembler::NotEqual, rhs, ImmWord(-1), &noOverflow);
    if (lir->mir()->isMod()) {
      masm.movePtr(ImmWord(0), output);
    } else {
      masm.wasmTrap(wasm::Trap::IntegerOverflow, lir->bytecodeOffset());
    }
    masm.jump(&done);
    masm.bind(&noOverflow);
  }

  masm.Sdiv(ARMRegister(output, 64), ARMRegister(lhs, 64),
            ARMRegister(rhs, 64));
  if (lir->mir()->isMod()) {
    masm.Msub(ARMRegister(output, 64), ARMRegister(output, 64),
              ARMRegister(rhs, 64), ARMRegister(lhs, 64));
  }
  masm.bind(&done);
}

void CodeGenerator::visitUDivOrModI64(LUDivOrModI64* lir) {
  Register lhs = ToRegister(lir->lhs());
  Register rhs = ToRegister(lir->rhs());
  Register output = ToRegister(lir->output());

  Label done;

  // Handle divide by zero.
  if (lir->canBeDivideByZero()) {
    Label isNotDivByZero;
    masm.Cbnz(ARMRegister(rhs, 64), &isNotDivByZero);
    masm.wasmTrap(wasm::Trap::IntegerDivideByZero, lir->bytecodeOffset());
    masm.bind(&isNotDivByZero);
  }

  masm.Udiv(ARMRegister(output, 64), ARMRegister(lhs, 64),
            ARMRegister(rhs, 64));
  if (lir->mir()->isMod()) {
    masm.Msub(ARMRegister(output, 64), ARMRegister(output, 64),
              ARMRegister(rhs, 64), ARMRegister(lhs, 64));
  }
  masm.bind(&done);
}

void CodeGenerator::visitSimd128(LSimd128* ins) { MOZ_CRASH("No SIMD"); }

void CodeGenerator::visitWasmBitselectSimd128(LWasmBitselectSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmBinarySimd128(LWasmBinarySimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmBinarySimd128WithConstant(
    LWasmBinarySimd128WithConstant* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmVariableShiftSimd128(
    LWasmVariableShiftSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmConstantShiftSimd128(
    LWasmConstantShiftSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmShuffleSimd128(LWasmShuffleSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmPermuteSimd128(LWasmPermuteSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmReplaceLaneSimd128(LWasmReplaceLaneSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmReplaceInt64LaneSimd128(
    LWasmReplaceInt64LaneSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmScalarToSimd128(LWasmScalarToSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmInt64ToSimd128(LWasmInt64ToSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmUnarySimd128(LWasmUnarySimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmReduceSimd128(LWasmReduceSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmReduceAndBranchSimd128(
    LWasmReduceAndBranchSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmReduceSimd128ToInt64(
    LWasmReduceSimd128ToInt64* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmLoadLaneSimd128(LWasmLoadLaneSimd128* ins) {
  MOZ_CRASH("No SIMD");
}

void CodeGenerator::visitWasmStoreLaneSimd128(LWasmStoreLaneSimd128* ins) {
  MOZ_CRASH("No SIMD");
}
