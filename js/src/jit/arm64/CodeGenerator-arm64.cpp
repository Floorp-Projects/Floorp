/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm64/CodeGenerator-arm64.h"

#include "mozilla/MathAlgorithms.h"

#include "jsnum.h"

#include "jit/CodeGenerator.h"
#include "jit/JitFrames.h"
#include "jit/JitRealm.h"
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

  masm.test32(input, input);

  // Jump to the True block if NonZero.
  // Jump to the False block if Zero.
  if (isNextBlock(mirFalse->lir())) {
    jumpToBlock(mirTrue, Assembler::NonZero);
  } else {
    jumpToBlock(mirFalse, Assembler::Zero);
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

  if (type == MCompare::Compare_Object || type == MCompare::Compare_Symbol) {
    masm.cmpPtrSet(cond, leftreg, ToRegister(right), defreg);
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

  if (type == MCompare::Compare_Object || type == MCompare::Compare_Symbol) {
    masm.cmpPtr(ToRegister(left), ToRegister(right));
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

void CodeGenerator::visitAbsD(LAbsD* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 64);
  masm.Fabs(input, input);
}

void CodeGenerator::visitAbsF(LAbsF* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 32);
  masm.Fabs(input, input);
}

void CodeGenerator::visitSqrtD(LSqrtD* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 64);
  ARMFPRegister output(ToFloatRegister(ins->output()), 64);
  masm.Fsqrt(output, input);
}

void CodeGenerator::visitSqrtF(LSqrtF* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 32);
  ARMFPRegister output(ToFloatRegister(ins->output()), 32);
  masm.Fsqrt(output, input);
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
    masm.Asr(output64, output64, rmc.shiftAmount);
  } else {
    // (M * n) >> (32 + shift) is the truncated division answer.
    masm.Asr(output64, output64, 32 + rmc.shiftAmount);
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

void CodeGeneratorARM64::modICommon(MMod* mir, Register lhs, Register rhs,
                                    Register output, LSnapshot* snapshot,
                                    Label& done) {
  MOZ_CRASH("CodeGeneratorARM64::modICommon");
}

void CodeGenerator::visitModI(LModI* ins) {
  if (gen->compilingWasm()) {
    MOZ_CRASH("visitModI while compilingWasm");
  }

  MMod* mir = ins->mir();
  ARMRegister lhs = toWRegister(ins->lhs());
  ARMRegister rhs = toWRegister(ins->rhs());
  ARMRegister output = toWRegister(ins->output());
  Label done;

  if (mir->canBeDivideByZero() && !mir->isTruncated()) {
    // Non-truncated division by zero produces a non-integer.
    masm.Cmp(rhs, Operand(0));
    bailoutIf(Assembler::Equal, ins->snapshot());
  } else if (mir->canBeDivideByZero()) {
    // Truncated division by zero yields integer zero.
    masm.Mov(output, rhs);
    masm.Cbz(rhs, &done);
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

  return MoveOperand(AsRegister(masm.getStackPointer()), ToStackOffset(a),
                     kind);
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

void CodeGenerator::visitFloor(LFloor* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout;
  masm.floor(input, output, &bailout);
  bailoutFrom(&bailout, lir->snapshot());
}

void CodeGenerator::visitFloorF(LFloorF* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout;
  masm.floorf(input, output, &bailout);
  bailoutFrom(&bailout, lir->snapshot());
}

void CodeGenerator::visitCeil(LCeil* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout;
  masm.ceil(input, output, &bailout);
  bailoutFrom(&bailout, lir->snapshot());
}

void CodeGenerator::visitCeilF(LCeilF* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout;
  masm.ceilf(input, output, &bailout);
  bailoutFrom(&bailout, lir->snapshot());
}

void CodeGenerator::visitRound(LRound* lir) {
  const FloatRegister input = ToFloatRegister(lir->input());
  const ARMFPRegister input64(input, 64);
  const FloatRegister temp = ToFloatRegister(lir->temp());
  const Register output = ToRegister(lir->output());
  ScratchDoubleScope scratch(masm);

  Label negative, done;

  // Branch to a slow path if input < 0.0 due to complicated rounding rules.
  // Note that Fcmp with NaN unsets the negative flag.
  masm.Fcmp(input64, 0.0);
  masm.B(&negative, Assembler::Condition::lo);

  // Handle the simple case of a positive input, and also -0 and NaN.
  // Rounding proceeds with consideration of the fractional part of the input:
  // 1. If > 0.5, round to integer with higher absolute value (so, up).
  // 2. If < 0.5, round to integer with lower absolute value (so, down).
  // 3. If = 0.5, round to +Infinity (so, up).
  {
    // Convert to signed 32-bit integer, rounding halfway cases away from zero.
    // In the case of overflow, the output is saturated.
    // In the case of NaN and -0, the output is zero.
    masm.Fcvtas(ARMRegister(output, 32), input64);
    // If the output potentially saturated, take a bailout.
    bailoutCmp32(Assembler::Equal, output, Imm32(INT_MAX), lir->snapshot());

    // If the result of the rounding was non-zero, return the output.
    // In the case of zero, the input may have been NaN or -0, which must bail.
    masm.branch32(Assembler::NotEqual, output, Imm32(0), &done);
    {
      // If input is NaN, comparisons set the C and V bits of the NZCV flags.
      masm.Fcmp(input64, 0.0);
      bailoutIf(Assembler::Overflow, lir->snapshot());

      // Move all 64 bits of the input into a scratch register to check for -0.
      vixl::UseScratchRegisterScope temps(&masm.asVIXL());
      const ARMRegister scratchGPR64 = temps.AcquireX();
      masm.Fmov(scratchGPR64, input64);
      masm.Cmp(scratchGPR64, vixl::Operand(uint64_t(0x8000000000000000)));
      bailoutIf(Assembler::Equal, lir->snapshot());
    }

    masm.jump(&done);
  }

  // Handle the complicated case of a negative input.
  // Rounding proceeds with consideration of the fractional part of the input:
  // 1. If > 0.5, round to integer with higher absolute value (so, down).
  // 2. If < 0.5, round to integer with lower absolute value (so, up).
  // 3. If = 0.5, round to +Infinity (so, up).
  masm.bind(&negative);
  {
    // Inputs in [-0.5, 0) need 0.5 added; other negative inputs need
    // the biggest double less than 0.5.
    Label join;
    masm.loadConstantDouble(GetBiggestNumberLessThan(0.5), temp);
    masm.loadConstantDouble(-0.5, scratch);
    masm.branchDouble(Assembler::DoubleLessThan, input, scratch, &join);
    masm.loadConstantDouble(0.5, temp);
    masm.bind(&join);

    masm.addDouble(input, temp);
    // Round all values toward -Infinity.
    // In the case of overflow, the output is saturated.
    // NaN and -0 are already handled by the "positive number" path above.
    masm.Fcvtms(ARMRegister(output, 32), temp);
    // If the output potentially saturated, take a bailout.
    bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot());

    // If output is zero, then the actual result is -0. Bail.
    bailoutTest32(Assembler::Zero, output, output, lir->snapshot());
  }

  masm.bind(&done);
}

void CodeGenerator::visitRoundF(LRoundF* lir) {
  const FloatRegister input = ToFloatRegister(lir->input());
  const ARMFPRegister input32(input, 32);
  const FloatRegister temp = ToFloatRegister(lir->temp());
  const Register output = ToRegister(lir->output());
  ScratchFloat32Scope scratch(masm);

  Label negative, done;

  // Branch to a slow path if input < 0.0 due to complicated rounding rules.
  // Note that Fcmp with NaN unsets the negative flag.
  masm.Fcmp(input32, 0.0);
  masm.B(&negative, Assembler::Condition::lo);

  // Handle the simple case of a positive input, and also -0 and NaN.
  // Rounding proceeds with consideration of the fractional part of the input:
  // 1. If > 0.5, round to integer with higher absolute value (so, up).
  // 2. If < 0.5, round to integer with lower absolute value (so, down).
  // 3. If = 0.5, round to +Infinity (so, up).
  {
    // Convert to signed 32-bit integer, rounding halfway cases away from zero.
    // In the case of overflow, the output is saturated.
    // In the case of NaN and -0, the output is zero.
    masm.Fcvtas(ARMRegister(output, 32), input32);
    // If the output potentially saturated, take a bailout.
    bailoutCmp32(Assembler::Equal, output, Imm32(INT_MAX), lir->snapshot());

    // If the result of the rounding was non-zero, return the output.
    // In the case of zero, the input may have been NaN or -0, which must bail.
    masm.branch32(Assembler::NotEqual, output, Imm32(0), &done);
    {
      // If input is NaN, comparisons set the C and V bits of the NZCV flags.
      masm.Fcmp(input32, 0.0f);
      bailoutIf(Assembler::Overflow, lir->snapshot());

      // Move all 32 bits of the input into a scratch register to check for -0.
      vixl::UseScratchRegisterScope temps(&masm.asVIXL());
      const ARMRegister scratchGPR32 = temps.AcquireW();
      masm.Fmov(scratchGPR32, input32);
      masm.Cmp(scratchGPR32, vixl::Operand(uint32_t(0x80000000)));
      bailoutIf(Assembler::Equal, lir->snapshot());
    }

    masm.jump(&done);
  }

  // Handle the complicated case of a negative input.
  // Rounding proceeds with consideration of the fractional part of the input:
  // 1. If > 0.5, round to integer with higher absolute value (so, down).
  // 2. If < 0.5, round to integer with lower absolute value (so, up).
  // 3. If = 0.5, round to +Infinity (so, up).
  masm.bind(&negative);
  {
    // Inputs in [-0.5, 0) need 0.5 added; other negative inputs need
    // the biggest double less than 0.5.
    Label join;
    masm.loadConstantFloat32(GetBiggestNumberLessThan(0.5f), temp);
    masm.loadConstantFloat32(-0.5f, scratch);
    masm.branchFloat(Assembler::DoubleLessThan, input, scratch, &join);
    masm.loadConstantFloat32(0.5f, temp);
    masm.bind(&join);

    masm.addFloat32(input, temp);
    // Round all values toward -Infinity.
    // In the case of overflow, the output is saturated.
    // NaN and -0 are already handled by the "positive number" path above.
    masm.Fcvtms(ARMRegister(output, 32), temp);
    // If the output potentially saturated, take a bailout.
    bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot());

    // If output is zero, then the actual result is -0. Bail.
    bailoutTest32(Assembler::Zero, output, output, lir->snapshot());
  }

  masm.bind(&done);
}

void CodeGenerator::visitTrunc(LTrunc* lir) {
  const FloatRegister input = ToFloatRegister(lir->input());
  const ARMFPRegister input64(input, 64);
  const Register output = ToRegister(lir->output());
  const ARMRegister output32(output, 32);
  const ARMRegister output64(output, 64);

  Label done, zeroCase;

  // Convert scalar to signed 32-bit fixed-point, rounding toward zero.
  // In the case of overflow, the output is saturated.
  // In the case of NaN and -0, the output is zero.
  masm.Fcvtzs(output32, input64);

  // If the output was zero, worry about special cases.
  masm.branch32(Assembler::Equal, output, Imm32(0), &zeroCase);

  // Bail on overflow cases.
  bailoutCmp32(Assembler::Equal, output, Imm32(INT_MAX), lir->snapshot());
  bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot());

  // If the output was non-zero and wasn't saturated, just return it.
  masm.jump(&done);

  // Handle the case of a zero output:
  // 1. The input may have been NaN, requiring a bail.
  // 2. The input may have been in (-1,-0], requiring a bail.
  {
    masm.bind(&zeroCase);

    // If input is a negative number that truncated to zero, the real
    // output should be the non-integer -0.
    // The use of "lt" instead of "lo" also catches unordered NaN input.
    masm.Fcmp(input64, 0.0);
    bailoutIf(vixl::lt, lir->snapshot());

    // Check explicitly for -0, bitwise.
    masm.Fmov(output64, input64);
    bailoutTestPtr(Assembler::Signed, output, output, lir->snapshot());
    masm.movePtr(ImmPtr(0), output);
  }

  masm.bind(&done);
}

void CodeGenerator::visitTruncF(LTruncF* lir) {
  const FloatRegister input = ToFloatRegister(lir->input());
  const ARMFPRegister input32(input, 32);
  const Register output = ToRegister(lir->output());
  const ARMRegister output32(output, 32);

  Label done, zeroCase;

  // Convert scalar to signed 32-bit fixed-point, rounding toward zero.
  // In the case of overflow, the output is saturated.
  // In the case of NaN and -0, the output is zero.
  masm.Fcvtzs(output32, input32);

  // If the output was zero, worry about special cases.
  masm.branch32(Assembler::Equal, output, Imm32(0), &zeroCase);

  // Bail on overflow cases.
  bailoutCmp32(Assembler::Equal, output, Imm32(INT_MAX), lir->snapshot());
  bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot());

  // If the output was non-zero and wasn't saturated, just return it.
  masm.jump(&done);

  // Handle the case of a zero output:
  // 1. The input may have been NaN, requiring a bail.
  // 2. The input may have been in (-1,-0], requiring a bail.
  {
    masm.bind(&zeroCase);

    // If input is a negative number that truncated to zero, the real
    // output should be the non-integer -0.
    // The use of "lt" instead of "lo" also catches unordered NaN input.
    masm.Fcmp(input32, 0.0f);
    bailoutIf(vixl::lt, lir->snapshot());

    // Check explicitly for -0, bitwise.
    masm.Fmov(output32, input32);
    bailoutTest32(Assembler::Signed, output, output, lir->snapshot());
    masm.move32(Imm32(0), output);
  }

  masm.bind(&done);
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

void CodeGenerator::visitTruncateFToInt32(LTruncateFToInt32* ins) {
  emitTruncateFloat32(ToFloatRegister(ins->input()), ToRegister(ins->output()),
                      ins->mir());
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

  if (mir->fallible()) {
    const ValueOperand value = ToValue(unbox, LUnbox::Input);
    Assembler::Condition cond;
    switch (mir->type()) {
      case MIRType::Int32:
        cond = masm.testInt32(Assembler::NotEqual, value);
        break;
      case MIRType::Boolean:
        cond = masm.testBoolean(Assembler::NotEqual, value);
        break;
      case MIRType::Object:
        cond = masm.testObject(Assembler::NotEqual, value);
        break;
      case MIRType::String:
        cond = masm.testString(Assembler::NotEqual, value);
        break;
      case MIRType::Symbol:
        cond = masm.testSymbol(Assembler::NotEqual, value);
        break;
      case MIRType::BigInt:
        cond = masm.testBigInt(Assembler::NotEqual, value);
        break;
      default:
        MOZ_CRASH("Given MIRType cannot be unboxed.");
    }
    bailoutIf(cond, unbox->snapshot());
  } else {
#ifdef DEBUG
    JSValueTag tag = MIRTypeToTag(mir->type());
    Label ok;

    ValueOperand input = ToValue(unbox, LUnbox::Input);
    ScratchTagScope scratch(masm, input);
    masm.splitTagForTest(input, scratch);
    masm.cmpTag(scratch, ImmTag(tag));
    masm.B(&ok, Assembler::Condition::Equal);
    masm.assumeUnreachable("Infallible unbox type mismatch");
    masm.bind(&ok);
#endif
  }

  ValueOperand input = ToValue(unbox, LUnbox::Input);
  Register result = ToRegister(unbox->output());
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

void CodeGenerator::visitCompareB(LCompareB* lir) {
  MCompare* mir = lir->mir();
  const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
  const LAllocation* rhs = lir->rhs();
  const Register output = ToRegister(lir->output());
  const Assembler::Condition cond =
      JSOpToCondition(mir->compareType(), mir->jsop());

  vixl::UseScratchRegisterScope temps(&masm.asVIXL());
  const Register scratch = temps.AcquireX().asUnsized();

  MOZ_ASSERT(mir->jsop() == JSOp::StrictEq || mir->jsop() == JSOp::StrictNe);

  // Load boxed boolean into scratch.
  if (rhs->isConstant()) {
    masm.moveValue(rhs->toConstant()->toJSValue(), ValueOperand(scratch));
  } else {
    masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), scratch);
  }

  // Compare the entire Value.
  masm.cmpPtrSet(cond, lhs.valueReg(), scratch, output);
}

void CodeGenerator::visitCompareBAndBranch(LCompareBAndBranch* lir) {
  MCompare* mir = lir->cmpMir();
  const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
  const LAllocation* rhs = lir->rhs();
  const Assembler::Condition cond =
      JSOpToCondition(mir->compareType(), mir->jsop());

  vixl::UseScratchRegisterScope temps(&masm.asVIXL());
  const Register scratch = temps.AcquireX().asUnsized();

  MOZ_ASSERT(mir->jsop() == JSOp::StrictEq || mir->jsop() == JSOp::StrictNe);

  // Load boxed boolean into scratch.
  if (rhs->isConstant()) {
    masm.moveValue(rhs->toConstant()->toJSValue(), ValueOperand(scratch));
  } else {
    masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), scratch);
  }

  // Compare the entire Value.
  masm.cmpPtr(lhs.valueReg(), scratch);
  emitBranch(cond, lir->ifTrue(), lir->ifFalse());
}

void CodeGenerator::visitCompareBitwise(LCompareBitwise* lir) {
  MCompare* mir = lir->mir();
  Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
  const ValueOperand lhs = ToValue(lir, LCompareBitwise::LhsInput);
  const ValueOperand rhs = ToValue(lir, LCompareBitwise::RhsInput);
  const Register output = ToRegister(lir->output());

  MOZ_ASSERT(IsEqualityOp(mir->jsop()));

  masm.cmpPtrSet(cond, lhs.valueReg(), rhs.valueReg(), output);
}

void CodeGenerator::visitCompareBitwiseAndBranch(
    LCompareBitwiseAndBranch* lir) {
  MCompare* mir = lir->cmpMir();
  Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
  const ValueOperand lhs = ToValue(lir, LCompareBitwiseAndBranch::LhsInput);
  const ValueOperand rhs = ToValue(lir, LCompareBitwiseAndBranch::RhsInput);

  MOZ_ASSERT(mir->jsop() == JSOp::Eq || mir->jsop() == JSOp::StrictEq ||
             mir->jsop() == JSOp::Ne || mir->jsop() == JSOp::StrictNe);

  masm.cmpPtr(lhs.valueReg(), rhs.valueReg());
  emitBranch(cond, lir->ifTrue(), lir->ifFalse());
}

void CodeGenerator::visitBitAndAndBranch(LBitAndAndBranch* baab) {
  if (baab->right()->isConstant()) {
    masm.Tst(toWRegister(baab->left()), Operand(ToInt32(baab->right())));
  } else {
    masm.Tst(toWRegister(baab->left()), toWRegister(baab->right()));
  }
  emitBranch(baab->cond(), baab->ifTrue(), baab->ifFalse());
}

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

void CodeGeneratorARM64::storeElementTyped(const LAllocation* value,
                                           MIRType valueType,
                                           MIRType elementType,
                                           Register elements,
                                           const LAllocation* index) {
  MOZ_CRASH("CodeGeneratorARM64::storeElementTyped");
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
  MOZ_CRASH("visitAsmJSLoadHeap");
}

void CodeGenerator::visitAsmJSStoreHeap(LAsmJSStoreHeap* ins) {
  MOZ_CRASH("visitAsmJSStoreHeap");
}

void CodeGenerator::visitWasmCompareExchangeHeap(
    LWasmCompareExchangeHeap* ins) {
  MOZ_CRASH("visitWasmCompareExchangeHeap");
}

void CodeGenerator::visitWasmAtomicBinopHeap(LWasmAtomicBinopHeap* ins) {
  MOZ_CRASH("visitWasmAtomicBinopHeap");
}

void CodeGenerator::visitWasmStackArg(LWasmStackArg* ins) {
  MOZ_CRASH("visitWasmStackArg");
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

  if (mir->canBeDivideByZero() && !mir->isTruncated()) {
    // Non-truncated division by zero produces a non-integer.
    masm.Cmp(rhs, Operand(0));
    bailoutIf(Assembler::Equal, ins->snapshot());
  } else if (mir->canBeDivideByZero()) {
    // Truncated division by zero yields integer zero.
    masm.Mov(output, rhs);
    masm.Cbz(rhs, &done);
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
  const ARMFPRegister output(ToFloatRegister(ins->input()), 64);
  masm.Fneg(output, input);
}

void CodeGenerator::visitNegF(LNegF* ins) {
  const ARMFPRegister input(ToFloatRegister(ins->input()), 32);
  const ARMFPRegister output(ToFloatRegister(ins->input()), 32);
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
  size_t width = Scalar::byteSize(arrayType);

  if (lir->index()->isConstant()) {
    Address dest(elements, ToInt32(lir->index()) * width);
    masm.compareExchangeJS(arrayType, Synchronization::Full(), dest, oldval,
                           newval, temp, output);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromElemWidth(width));
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
  size_t width = Scalar::byteSize(arrayType);

  if (lir->index()->isConstant()) {
    Address dest(elements, ToInt32(lir->index()) * width);
    masm.atomicExchangeJS(arrayType, Synchronization::Full(), dest, value, temp,
                          output);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromElemWidth(width));
    masm.atomicExchangeJS(arrayType, Synchronization::Full(), dest, value, temp,
                          output);
  }
}

void CodeGenerator::visitAddI64(LAddI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitClzI64(LClzI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitCtzI64(LCtzI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitMulI64(LMulI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitNotI64(LNotI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitSubI64(LSubI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitPopcntI(LPopcntI*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitBitOpI64(LBitOpI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitShiftI64(LShiftI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmHeapBase(LWasmHeapBase* ins) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmLoad(LWasmLoad*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitCopySignD(LCopySignD*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitCopySignF(LCopySignF*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitNearbyInt(LNearbyInt*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitPopcntI64(LPopcntI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitRotateI64(LRotateI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmStore(LWasmStore*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitCompareI64(LCompareI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitNearbyIntF(LNearbyIntF*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmSelect(LWasmSelect*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmLoadI64(LWasmLoadI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmStoreI64(LWasmStoreI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitMemoryBarrier(LMemoryBarrier* ins) {
  masm.memoryBarrier(ins->type());
}

void CodeGenerator::visitWasmAddOffset(LWasmAddOffset*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmSelectI64(LWasmSelectI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitSignExtendInt64(LSignExtendInt64*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmReinterpret(LWasmReinterpret*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmStackArgI64(LWasmStackArgI64*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitTestI64AndBranch(LTestI64AndBranch*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWrapInt64ToInt32(LWrapInt64ToInt32*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitExtendInt32ToInt64(LExtendInt32ToInt64*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitCompareI64AndBranch(LCompareI64AndBranch*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmTruncateToInt32(LWasmTruncateToInt32*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmReinterpretToI64(LWasmReinterpretToI64*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmAtomicExchangeHeap(LWasmAtomicExchangeHeap*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmReinterpretFromI64(LWasmReinterpretFromI64*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitAtomicTypedArrayElementBinop(
    LAtomicTypedArrayElementBinop* lir) {
  MOZ_ASSERT(lir->mir()->hasUses());

  AnyRegister output = ToAnyRegister(lir->output());
  Register elements = ToRegister(lir->elements());
  Register flagTemp = ToRegister(lir->temp1());
  Register outTemp =
      lir->temp2()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp2());
  Register value = ToRegister(lir->value());

  Scalar::Type arrayType = lir->mir()->arrayType();
  size_t width = Scalar::byteSize(arrayType);

  if (lir->index()->isConstant()) {
    Address mem(elements, ToInt32(lir->index()) * width);
    masm.atomicFetchOpJS(arrayType, Synchronization::Full(),
                         lir->mir()->operation(), value, mem, flagTemp, outTemp,
                         output);
  } else {
    BaseIndex mem(elements, ToRegister(lir->index()),
                  ScaleFromElemWidth(width));
    masm.atomicFetchOpJS(arrayType, Synchronization::Full(),
                         lir->mir()->operation(), value, mem, flagTemp, outTemp,
                         output);
  }
}

void CodeGenerator::visitWasmAtomicBinopHeapForEffect(
    LWasmAtomicBinopHeapForEffect*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitAtomicTypedArrayElementBinopForEffect(
    LAtomicTypedArrayElementBinopForEffect*) {
  MOZ_CRASH("NYI");
}
