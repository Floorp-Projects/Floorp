/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x86-shared/Lowering-x86-shared.h"

#include "mozilla/MathAlgorithms.h"

#include "jit/Lowering.h"
#include "jit/MIR.h"

#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Abs;
using mozilla::FloorLog2;

LTableSwitch* LIRGeneratorX86Shared::newLTableSwitch(
    const LAllocation& in, const LDefinition& inputCopy,
    MTableSwitch* tableswitch) {
  return new (alloc()) LTableSwitch(in, inputCopy, temp(), tableswitch);
}

LTableSwitchV* LIRGeneratorX86Shared::newLTableSwitchV(
    MTableSwitch* tableswitch) {
  return new (alloc()) LTableSwitchV(useBox(tableswitch->getOperand(0)), temp(),
                                     tempDouble(), temp(), tableswitch);
}

void LIRGenerator::visitPowHalf(MPowHalf* ins) {
  MDefinition* input = ins->input();
  MOZ_ASSERT(input->type() == MIRType::Double);
  LPowHalfD* lir = new (alloc()) LPowHalfD(useRegisterAtStart(input));
  define(lir, ins);
}

void LIRGeneratorX86Shared::lowerForShift(LInstructionHelper<1, 2, 0>* ins,
                                          MDefinition* mir, MDefinition* lhs,
                                          MDefinition* rhs) {
  ins->setOperand(0, useRegisterAtStart(lhs));

  // shift operator should be constant or in register ecx
  // x86 can't shift a non-ecx register
  if (rhs->isConstant()) {
    ins->setOperand(1, useOrConstantAtStart(rhs));
  } else {
    ins->setOperand(
        1, lhs != rhs ? useFixed(rhs, ecx) : useFixedAtStart(rhs, ecx));
  }

  defineReuseInput(ins, mir, 0);
}

template <size_t Temps>
void LIRGeneratorX86Shared::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, Temps>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs) {
  ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));
#if defined(JS_NUNBOX32)
  if (mir->isRotate()) {
    ins->setTemp(0, temp());
  }
#endif

  static_assert(LShiftI64::Rhs == INT64_PIECES,
                "Assume Rhs is located at INT64_PIECES.");
  static_assert(LRotateI64::Count == INT64_PIECES,
                "Assume Count is located at INT64_PIECES.");

  // shift operator should be constant or in register ecx
  // x86 can't shift a non-ecx register
  if (rhs->isConstant()) {
    ins->setOperand(INT64_PIECES, useOrConstantAtStart(rhs));
  } else {
    // The operands are int64, but we only care about the lower 32 bits of
    // the RHS. On 32-bit, the code below will load that part in ecx and
    // will discard the upper half.
    ensureDefined(rhs);
    LUse use(ecx);
    use.setVirtualRegister(rhs->virtualRegister());
    ins->setOperand(INT64_PIECES, use);
  }

  defineInt64ReuseInput(ins, mir, 0);
}

template void LIRGeneratorX86Shared::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, 0>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs);
template void LIRGeneratorX86Shared::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, 1>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs);

void LIRGeneratorX86Shared::lowerForALU(LInstructionHelper<1, 1, 0>* ins,
                                        MDefinition* mir, MDefinition* input) {
  ins->setOperand(0, useRegisterAtStart(input));
  defineReuseInput(ins, mir, 0);
}

void LIRGeneratorX86Shared::lowerForALU(LInstructionHelper<1, 2, 0>* ins,
                                        MDefinition* mir, MDefinition* lhs,
                                        MDefinition* rhs) {
  ins->setOperand(0, useRegisterAtStart(lhs));
  ins->setOperand(1,
                  lhs != rhs ? useOrConstant(rhs) : useOrConstantAtStart(rhs));
  defineReuseInput(ins, mir, 0);
}

template <size_t Temps>
void LIRGeneratorX86Shared::lowerForFPU(LInstructionHelper<1, 2, Temps>* ins,
                                        MDefinition* mir, MDefinition* lhs,
                                        MDefinition* rhs) {
  // Without AVX, we'll need to use the x86 encodings where one of the
  // inputs must be the same location as the output.
  if (!Assembler::HasAVX()) {
    ins->setOperand(0, useRegisterAtStart(lhs));
    ins->setOperand(1, lhs != rhs ? use(rhs) : useAtStart(rhs));
    defineReuseInput(ins, mir, 0);
  } else {
    ins->setOperand(0, useRegisterAtStart(lhs));
    ins->setOperand(1, useAtStart(rhs));
    define(ins, mir);
  }
}

template void LIRGeneratorX86Shared::lowerForFPU(
    LInstructionHelper<1, 2, 0>* ins, MDefinition* mir, MDefinition* lhs,
    MDefinition* rhs);
template void LIRGeneratorX86Shared::lowerForFPU(
    LInstructionHelper<1, 2, 1>* ins, MDefinition* mir, MDefinition* lhs,
    MDefinition* rhs);

void LIRGeneratorX86Shared::lowerForBitAndAndBranch(LBitAndAndBranch* baab,
                                                    MInstruction* mir,
                                                    MDefinition* lhs,
                                                    MDefinition* rhs) {
  baab->setOperand(0, useRegisterAtStart(lhs));
  baab->setOperand(1, useRegisterOrConstantAtStart(rhs));
  add(baab, mir);
}

void LIRGeneratorX86Shared::lowerMulI(MMul* mul, MDefinition* lhs,
                                      MDefinition* rhs) {
  // Note: If we need a negative zero check, lhs is used twice.
  LAllocation lhsCopy = mul->canBeNegativeZero() ? use(lhs) : LAllocation();
  LMulI* lir =
      new (alloc()) LMulI(useRegisterAtStart(lhs), useOrConstant(rhs), lhsCopy);
  if (mul->fallible()) {
    assignSnapshot(lir, Bailout_DoubleOutput);
  }
  defineReuseInput(lir, mul, 0);
}

void LIRGeneratorX86Shared::lowerDivI(MDiv* div) {
  if (div->isUnsigned()) {
    lowerUDiv(div);
    return;
  }

  // Division instructions are slow. Division by constant denominators can be
  // rewritten to use other instructions.
  if (div->rhs()->isConstant()) {
    int32_t rhs = div->rhs()->toConstant()->toInt32();

    // Division by powers of two can be done by shifting, and division by
    // other numbers can be done by a reciprocal multiplication technique.
    int32_t shift = FloorLog2(Abs(rhs));
    if (rhs != 0 && uint32_t(1) << shift == Abs(rhs)) {
      LAllocation lhs = useRegisterAtStart(div->lhs());
      LDivPowTwoI* lir;
      // When truncated with maybe a non-zero remainder, we have to round the
      // result toward 0. This requires an extra register to round up/down
      // whether the left-hand-side is signed.
      bool needRoundNeg = div->canBeNegativeDividend() && div->isTruncated();
      if (!needRoundNeg) {
        // Numerator is unsigned, so does not need adjusting.
        lir = new (alloc()) LDivPowTwoI(lhs, lhs, shift, rhs < 0);
      } else {
        // Numerator might be signed, and needs adjusting, and an extra lhs copy
        // is needed to round the result of the integer division towards zero.
        lir = new (alloc())
            LDivPowTwoI(lhs, useRegister(div->lhs()), shift, rhs < 0);
      }
      if (div->fallible()) {
        assignSnapshot(lir, Bailout_DoubleOutput);
      }
      defineReuseInput(lir, div, 0);
      return;
    }
    if (rhs != 0) {
      LDivOrModConstantI* lir;
      lir = new (alloc())
          LDivOrModConstantI(useRegister(div->lhs()), rhs, tempFixed(eax));
      if (div->fallible()) {
        assignSnapshot(lir, Bailout_DoubleOutput);
      }
      defineFixed(lir, div, LAllocation(AnyRegister(edx)));
      return;
    }
  }

  LDivI* lir = new (alloc())
      LDivI(useRegister(div->lhs()), useRegister(div->rhs()), tempFixed(edx));
  if (div->fallible()) {
    assignSnapshot(lir, Bailout_DoubleOutput);
  }
  defineFixed(lir, div, LAllocation(AnyRegister(eax)));
}

void LIRGeneratorX86Shared::lowerModI(MMod* mod) {
  if (mod->isUnsigned()) {
    lowerUMod(mod);
    return;
  }

  if (mod->rhs()->isConstant()) {
    int32_t rhs = mod->rhs()->toConstant()->toInt32();
    int32_t shift = FloorLog2(Abs(rhs));
    if (rhs != 0 && uint32_t(1) << shift == Abs(rhs)) {
      LModPowTwoI* lir =
          new (alloc()) LModPowTwoI(useRegisterAtStart(mod->lhs()), shift);
      if (mod->fallible()) {
        assignSnapshot(lir, Bailout_DoubleOutput);
      }
      defineReuseInput(lir, mod, 0);
      return;
    }
    if (rhs != 0) {
      LDivOrModConstantI* lir;
      lir = new (alloc())
          LDivOrModConstantI(useRegister(mod->lhs()), rhs, tempFixed(edx));
      if (mod->fallible()) {
        assignSnapshot(lir, Bailout_DoubleOutput);
      }
      defineFixed(lir, mod, LAllocation(AnyRegister(eax)));
      return;
    }
  }

  LModI* lir = new (alloc())
      LModI(useRegister(mod->lhs()), useRegister(mod->rhs()), tempFixed(eax));
  if (mod->fallible()) {
    assignSnapshot(lir, Bailout_DoubleOutput);
  }
  defineFixed(lir, mod, LAllocation(AnyRegister(edx)));
}

void LIRGenerator::visitWasmNeg(MWasmNeg* ins) {
  switch (ins->type()) {
    case MIRType::Int32:
      defineReuseInput(new (alloc()) LNegI(useRegisterAtStart(ins->input())),
                       ins, 0);
      break;
    case MIRType::Float32:
      defineReuseInput(new (alloc()) LNegF(useRegisterAtStart(ins->input())),
                       ins, 0);
      break;
    case MIRType::Double:
      defineReuseInput(new (alloc()) LNegD(useRegisterAtStart(ins->input())),
                       ins, 0);
      break;
    default:
      MOZ_CRASH();
  }
}

void LIRGenerator::visitAsmJSLoadHeap(MAsmJSLoadHeap* ins) {
  MDefinition* base = ins->base();
  MOZ_ASSERT(base->type() == MIRType::Int32);

  MDefinition* boundsCheckLimit = ins->boundsCheckLimit();
  MOZ_ASSERT_IF(ins->needsBoundsCheck(),
                boundsCheckLimit->type() == MIRType::Int32);

  // For simplicity, require a register if we're going to emit a bounds-check
  // branch, so that we don't have special cases for constants. This should
  // only happen in rare constant-folding cases since asm.js sets the minimum
  // heap size based when accessed via constant.
  LAllocation baseAlloc = ins->needsBoundsCheck()
                              ? useRegisterAtStart(base)
                              : useRegisterOrZeroAtStart(base);

  LAllocation limitAlloc = ins->needsBoundsCheck()
                               ? useRegisterAtStart(boundsCheckLimit)
                               : LAllocation();
  LAllocation memoryBaseAlloc = ins->hasMemoryBase()
                                    ? useRegisterAtStart(ins->memoryBase())
                                    : LAllocation();

  auto* lir =
      new (alloc()) LAsmJSLoadHeap(baseAlloc, limitAlloc, memoryBaseAlloc);
  define(lir, ins);
}

void LIRGenerator::visitAsmJSStoreHeap(MAsmJSStoreHeap* ins) {
  MDefinition* base = ins->base();
  MOZ_ASSERT(base->type() == MIRType::Int32);

  MDefinition* boundsCheckLimit = ins->boundsCheckLimit();
  MOZ_ASSERT_IF(ins->needsBoundsCheck(),
                boundsCheckLimit->type() == MIRType::Int32);

  // For simplicity, require a register if we're going to emit a bounds-check
  // branch, so that we don't have special cases for constants. This should
  // only happen in rare constant-folding cases since asm.js sets the minimum
  // heap size based when accessed via constant.
  LAllocation baseAlloc = ins->needsBoundsCheck()
                              ? useRegisterAtStart(base)
                              : useRegisterOrZeroAtStart(base);

  LAllocation limitAlloc = ins->needsBoundsCheck()
                               ? useRegisterAtStart(boundsCheckLimit)
                               : LAllocation();
  LAllocation memoryBaseAlloc = ins->hasMemoryBase()
                                    ? useRegisterAtStart(ins->memoryBase())
                                    : LAllocation();

  LAsmJSStoreHeap* lir = nullptr;
  switch (ins->access().type()) {
    case Scalar::Int8:
    case Scalar::Uint8:
#ifdef JS_CODEGEN_X86
      // See comment for LIRGeneratorX86::useByteOpRegister.
      lir = new (alloc()) LAsmJSStoreHeap(
          baseAlloc, useFixed(ins->value(), eax), limitAlloc, memoryBaseAlloc);
      break;
#endif
    case Scalar::Int16:
    case Scalar::Uint16:
    case Scalar::Int32:
    case Scalar::Uint32:
    case Scalar::Float32:
    case Scalar::Float64:
      // For now, don't allow constant values. The immediate operand affects
      // instruction layout which affects patching.
      lir = new (alloc())
          LAsmJSStoreHeap(baseAlloc, useRegisterAtStart(ins->value()),
                          limitAlloc, memoryBaseAlloc);
      break;
    case Scalar::Int64:
    case Scalar::V128:
      MOZ_CRASH("NYI");
    case Scalar::Uint8Clamped:
    case Scalar::BigInt64:
    case Scalar::BigUint64:
    case Scalar::MaxTypedArrayViewType:
      MOZ_CRASH("unexpected array type");
  }
  add(lir, ins);
}

void LIRGeneratorX86Shared::lowerUDiv(MDiv* div) {
  if (div->rhs()->isConstant()) {
    uint32_t rhs = div->rhs()->toConstant()->toInt32();
    int32_t shift = FloorLog2(rhs);

    LAllocation lhs = useRegisterAtStart(div->lhs());
    if (rhs != 0 && uint32_t(1) << shift == rhs) {
      LDivPowTwoI* lir = new (alloc()) LDivPowTwoI(lhs, lhs, shift, false);
      if (div->fallible()) {
        assignSnapshot(lir, Bailout_DoubleOutput);
      }
      defineReuseInput(lir, div, 0);
    } else {
      LUDivOrModConstant* lir = new (alloc())
          LUDivOrModConstant(useRegister(div->lhs()), rhs, tempFixed(eax));
      if (div->fallible()) {
        assignSnapshot(lir, Bailout_DoubleOutput);
      }
      defineFixed(lir, div, LAllocation(AnyRegister(edx)));
    }
    return;
  }

  LUDivOrMod* lir = new (alloc()) LUDivOrMod(
      useRegister(div->lhs()), useRegister(div->rhs()), tempFixed(edx));
  if (div->fallible()) {
    assignSnapshot(lir, Bailout_DoubleOutput);
  }
  defineFixed(lir, div, LAllocation(AnyRegister(eax)));
}

void LIRGeneratorX86Shared::lowerUMod(MMod* mod) {
  if (mod->rhs()->isConstant()) {
    uint32_t rhs = mod->rhs()->toConstant()->toInt32();
    int32_t shift = FloorLog2(rhs);

    if (rhs != 0 && uint32_t(1) << shift == rhs) {
      LModPowTwoI* lir =
          new (alloc()) LModPowTwoI(useRegisterAtStart(mod->lhs()), shift);
      if (mod->fallible()) {
        assignSnapshot(lir, Bailout_DoubleOutput);
      }
      defineReuseInput(lir, mod, 0);
    } else {
      LUDivOrModConstant* lir = new (alloc())
          LUDivOrModConstant(useRegister(mod->lhs()), rhs, tempFixed(edx));
      if (mod->fallible()) {
        assignSnapshot(lir, Bailout_DoubleOutput);
      }
      defineFixed(lir, mod, LAllocation(AnyRegister(eax)));
    }
    return;
  }

  LUDivOrMod* lir = new (alloc()) LUDivOrMod(
      useRegister(mod->lhs()), useRegister(mod->rhs()), tempFixed(eax));
  if (mod->fallible()) {
    assignSnapshot(lir, Bailout_DoubleOutput);
  }
  defineFixed(lir, mod, LAllocation(AnyRegister(edx)));
}

void LIRGeneratorX86Shared::lowerUrshD(MUrsh* mir) {
  MDefinition* lhs = mir->lhs();
  MDefinition* rhs = mir->rhs();

  MOZ_ASSERT(lhs->type() == MIRType::Int32);
  MOZ_ASSERT(rhs->type() == MIRType::Int32);
  MOZ_ASSERT(mir->type() == MIRType::Double);

#ifdef JS_CODEGEN_X64
  MOZ_ASSERT(ecx == rcx);
#endif

  LUse lhsUse = useRegisterAtStart(lhs);
  LAllocation rhsAlloc =
      rhs->isConstant() ? useOrConstant(rhs) : useFixed(rhs, ecx);

  LUrshD* lir = new (alloc()) LUrshD(lhsUse, rhsAlloc, tempCopy(lhs, 0));
  define(lir, mir);
}

void LIRGeneratorX86Shared::lowerTruncateDToInt32(MTruncateToInt32* ins) {
  MDefinition* opd = ins->input();
  MOZ_ASSERT(opd->type() == MIRType::Double);

  LDefinition maybeTemp =
      Assembler::HasSSE3() ? LDefinition::BogusTemp() : tempDouble();
  define(new (alloc()) LTruncateDToInt32(useRegister(opd), maybeTemp), ins);
}

void LIRGeneratorX86Shared::lowerTruncateFToInt32(MTruncateToInt32* ins) {
  MDefinition* opd = ins->input();
  MOZ_ASSERT(opd->type() == MIRType::Float32);

  LDefinition maybeTemp =
      Assembler::HasSSE3() ? LDefinition::BogusTemp() : tempFloat32();
  define(new (alloc()) LTruncateFToInt32(useRegister(opd), maybeTemp), ins);
}

void LIRGeneratorX86Shared::lowerCompareExchangeTypedArrayElement(
    MCompareExchangeTypedArrayElement* ins, bool useI386ByteRegisters) {
  MOZ_ASSERT(ins->arrayType() != Scalar::Float32);
  MOZ_ASSERT(ins->arrayType() != Scalar::Float64);

  MOZ_ASSERT(ins->elements()->type() == MIRType::Elements);
  MOZ_ASSERT(ins->index()->type() == MIRType::Int32);

  const LUse elements = useRegister(ins->elements());
  const LAllocation index = useRegisterOrConstant(ins->index());

  // If the target is a floating register then we need a temp at the
  // lower level; that temp must be eax.
  //
  // Otherwise the target (if used) is an integer register, which
  // must be eax.  If the target is not used the machine code will
  // still clobber eax, so just pretend it's used.
  //
  // oldval must be in a register.
  //
  // newval must be in a register.  If the source is a byte array
  // then newval must be a register that has a byte size: on x86
  // this must be ebx, ecx, or edx (eax is taken for the output).
  //
  // Bug #1077036 describes some further optimization opportunities.

  bool fixedOutput = false;
  LDefinition tempDef = LDefinition::BogusTemp();
  LAllocation newval;
  if (ins->arrayType() == Scalar::Uint32 && IsFloatingPointType(ins->type())) {
    tempDef = tempFixed(eax);
    newval = useRegister(ins->newval());
  } else {
    fixedOutput = true;
    if (useI386ByteRegisters && ins->isByteArray()) {
      newval = useFixed(ins->newval(), ebx);
    } else {
      newval = useRegister(ins->newval());
    }
  }

  const LAllocation oldval = useRegister(ins->oldval());

  LCompareExchangeTypedArrayElement* lir =
      new (alloc()) LCompareExchangeTypedArrayElement(elements, index, oldval,
                                                      newval, tempDef);

  if (fixedOutput) {
    defineFixed(lir, ins, LAllocation(AnyRegister(eax)));
  } else {
    define(lir, ins);
  }
}

void LIRGeneratorX86Shared::lowerAtomicExchangeTypedArrayElement(
    MAtomicExchangeTypedArrayElement* ins, bool useI386ByteRegisters) {
  MOZ_ASSERT(ins->arrayType() <= Scalar::Uint32);

  MOZ_ASSERT(ins->elements()->type() == MIRType::Elements);
  MOZ_ASSERT(ins->index()->type() == MIRType::Int32);

  const LUse elements = useRegister(ins->elements());
  const LAllocation index = useRegisterOrConstant(ins->index());
  const LAllocation value = useRegister(ins->value());

  // The underlying instruction is XCHG, which can operate on any
  // register.
  //
  // If the target is a floating register (for Uint32) then we need
  // a temp into which to exchange.
  //
  // If the source is a byte array then we need a register that has
  // a byte size; in this case -- on x86 only -- pin the output to
  // an appropriate register and use that as a temp in the back-end.

  LDefinition tempDef = LDefinition::BogusTemp();
  if (ins->arrayType() == Scalar::Uint32) {
    // This restriction is bug 1077305.
    MOZ_ASSERT(ins->type() == MIRType::Double);
    tempDef = temp();
  }

  LAtomicExchangeTypedArrayElement* lir = new (alloc())
      LAtomicExchangeTypedArrayElement(elements, index, value, tempDef);

  if (useI386ByteRegisters && ins->isByteArray()) {
    defineFixed(lir, ins, LAllocation(AnyRegister(eax)));
  } else {
    define(lir, ins);
  }
}

void LIRGeneratorX86Shared::lowerAtomicTypedArrayElementBinop(
    MAtomicTypedArrayElementBinop* ins, bool useI386ByteRegisters) {
  MOZ_ASSERT(ins->arrayType() != Scalar::Uint8Clamped);
  MOZ_ASSERT(ins->arrayType() != Scalar::Float32);
  MOZ_ASSERT(ins->arrayType() != Scalar::Float64);

  MOZ_ASSERT(ins->elements()->type() == MIRType::Elements);
  MOZ_ASSERT(ins->index()->type() == MIRType::Int32);

  const LUse elements = useRegister(ins->elements());
  const LAllocation index = useRegisterOrConstant(ins->index());

  // Case 1: the result of the operation is not used.
  //
  // We'll emit a single instruction: LOCK ADD, LOCK SUB, LOCK AND,
  // LOCK OR, or LOCK XOR.  We can do this even for the Uint32 case.

  if (!ins->hasUses()) {
    LAllocation value;
    if (useI386ByteRegisters && ins->isByteArray() &&
        !ins->value()->isConstant()) {
      value = useFixed(ins->value(), ebx);
    } else {
      value = useRegisterOrConstant(ins->value());
    }

    LAtomicTypedArrayElementBinopForEffect* lir = new (alloc())
        LAtomicTypedArrayElementBinopForEffect(elements, index, value);

    add(lir, ins);
    return;
  }

  // Case 2: the result of the operation is used.
  //
  // For ADD and SUB we'll use XADD:
  //
  //    movl       src, output
  //    lock xaddl output, mem
  //
  // For the 8-bit variants XADD needs a byte register for the output.
  //
  // For AND/OR/XOR we need to use a CMPXCHG loop:
  //
  //    movl          *mem, eax
  // L: mov           eax, temp
  //    andl          src, temp
  //    lock cmpxchg  temp, mem  ; reads eax also
  //    jnz           L
  //    ; result in eax
  //
  // Note the placement of L, cmpxchg will update eax with *mem if
  // *mem does not have the expected value, so reloading it at the
  // top of the loop would be redundant.
  //
  // If the array is not a uint32 array then:
  //  - eax should be the output (one result of the cmpxchg)
  //  - there is a temp, which must have a byte register if
  //    the array has 1-byte elements elements
  //
  // If the array is a uint32 array then:
  //  - eax is the first temp
  //  - we also need a second temp
  //
  // There are optimization opportunities:
  //  - better register allocation in the x86 8-bit case, Bug #1077036.

  bool bitOp = !(ins->operation() == AtomicFetchAddOp ||
                 ins->operation() == AtomicFetchSubOp);
  bool fixedOutput = true;
  bool reuseInput = false;
  LDefinition tempDef1 = LDefinition::BogusTemp();
  LDefinition tempDef2 = LDefinition::BogusTemp();
  LAllocation value;

  if (ins->arrayType() == Scalar::Uint32 && IsFloatingPointType(ins->type())) {
    value = useRegisterOrConstant(ins->value());
    fixedOutput = false;
    if (bitOp) {
      tempDef1 = tempFixed(eax);
      tempDef2 = temp();
    } else {
      tempDef1 = temp();
    }
  } else if (useI386ByteRegisters && ins->isByteArray()) {
    if (ins->value()->isConstant()) {
      value = useRegisterOrConstant(ins->value());
    } else {
      value = useFixed(ins->value(), ebx);
    }
    if (bitOp) {
      tempDef1 = tempFixed(ecx);
    }
  } else if (bitOp) {
    value = useRegisterOrConstant(ins->value());
    tempDef1 = temp();
  } else if (ins->value()->isConstant()) {
    fixedOutput = false;
    value = useRegisterOrConstant(ins->value());
  } else {
    fixedOutput = false;
    reuseInput = true;
    value = useRegisterAtStart(ins->value());
  }

  LAtomicTypedArrayElementBinop* lir = new (alloc())
      LAtomicTypedArrayElementBinop(elements, index, value, tempDef1, tempDef2);

  if (fixedOutput) {
    defineFixed(lir, ins, LAllocation(AnyRegister(eax)));
  } else if (reuseInput) {
    defineReuseInput(lir, ins, LAtomicTypedArrayElementBinop::valueOp);
  } else {
    define(lir, ins);
  }
}

void LIRGenerator::visitCopySign(MCopySign* ins) {
  MDefinition* lhs = ins->lhs();
  MDefinition* rhs = ins->rhs();

  MOZ_ASSERT(IsFloatingPointType(lhs->type()));
  MOZ_ASSERT(lhs->type() == rhs->type());
  MOZ_ASSERT(lhs->type() == ins->type());

  LInstructionHelper<1, 2, 2>* lir;
  if (lhs->type() == MIRType::Double) {
    lir = new (alloc()) LCopySignD();
  } else {
    lir = new (alloc()) LCopySignF();
  }

  // As lowerForFPU, but we want rhs to be in a FP register too.
  lir->setOperand(0, useRegisterAtStart(lhs));
  if (!Assembler::HasAVX()) {
    lir->setOperand(1, lhs != rhs ? useRegister(rhs) : useRegisterAtStart(rhs));
    defineReuseInput(lir, ins, 0);
  } else {
    lir->setOperand(1, useRegisterAtStart(rhs));
    define(lir, ins);
  }
}
