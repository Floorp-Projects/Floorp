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
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

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
  LMulI* lir = new (alloc()) LMulI(
      useRegisterAtStart(lhs),
      lhs != rhs ? useOrConstant(rhs) : useOrConstantAtStart(rhs), lhsCopy);
  if (mul->fallible()) {
    assignSnapshot(lir, BailoutKind::DoubleOutput);
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
        assignSnapshot(lir, BailoutKind::DoubleOutput);
      }
      defineReuseInput(lir, div, 0);
      return;
    }
    if (rhs != 0) {
      LDivOrModConstantI* lir;
      lir = new (alloc())
          LDivOrModConstantI(useRegister(div->lhs()), rhs, tempFixed(eax));
      if (div->fallible()) {
        assignSnapshot(lir, BailoutKind::DoubleOutput);
      }
      defineFixed(lir, div, LAllocation(AnyRegister(edx)));
      return;
    }
  }

  LDivI* lir = new (alloc())
      LDivI(useRegister(div->lhs()), useRegister(div->rhs()), tempFixed(edx));
  if (div->fallible()) {
    assignSnapshot(lir, BailoutKind::DoubleOutput);
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
        assignSnapshot(lir, BailoutKind::DoubleOutput);
      }
      defineReuseInput(lir, mod, 0);
      return;
    }
    if (rhs != 0) {
      LDivOrModConstantI* lir;
      lir = new (alloc())
          LDivOrModConstantI(useRegister(mod->lhs()), rhs, tempFixed(edx));
      if (mod->fallible()) {
        assignSnapshot(lir, BailoutKind::DoubleOutput);
      }
      defineFixed(lir, mod, LAllocation(AnyRegister(eax)));
      return;
    }
  }

  LModI* lir = new (alloc())
      LModI(useRegister(mod->lhs()), useRegister(mod->rhs()), tempFixed(eax));
  if (mod->fallible()) {
    assignSnapshot(lir, BailoutKind::DoubleOutput);
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
    case Scalar::Simd128:
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
        assignSnapshot(lir, BailoutKind::DoubleOutput);
      }
      defineReuseInput(lir, div, 0);
    } else {
      LUDivOrModConstant* lir = new (alloc())
          LUDivOrModConstant(useRegister(div->lhs()), rhs, tempFixed(eax));
      if (div->fallible()) {
        assignSnapshot(lir, BailoutKind::DoubleOutput);
      }
      defineFixed(lir, div, LAllocation(AnyRegister(edx)));
    }
    return;
  }

  LUDivOrMod* lir = new (alloc()) LUDivOrMod(
      useRegister(div->lhs()), useRegister(div->rhs()), tempFixed(edx));
  if (div->fallible()) {
    assignSnapshot(lir, BailoutKind::DoubleOutput);
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
        assignSnapshot(lir, BailoutKind::DoubleOutput);
      }
      defineReuseInput(lir, mod, 0);
    } else {
      LUDivOrModConstant* lir = new (alloc())
          LUDivOrModConstant(useRegister(mod->lhs()), rhs, tempFixed(edx));
      if (mod->fallible()) {
        assignSnapshot(lir, BailoutKind::DoubleOutput);
      }
      defineFixed(lir, mod, LAllocation(AnyRegister(eax)));
    }
    return;
  }

  LUDivOrMod* lir = new (alloc()) LUDivOrMod(
      useRegister(mod->lhs()), useRegister(mod->rhs()), tempFixed(eax));
  if (mod->fallible()) {
    assignSnapshot(lir, BailoutKind::DoubleOutput);
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

void LIRGeneratorX86Shared::lowerPowOfTwoI(MPow* mir) {
  int32_t base = mir->input()->toConstant()->toInt32();
  MDefinition* power = mir->power();

  // shift operator should be in register ecx;
  // x86 can't shift a non-ecx register.
  auto* lir = new (alloc()) LPowOfTwoI(base, useFixed(power, ecx));
  assignSnapshot(lir, BailoutKind::PrecisionLoss);
  define(lir, mir);
}

void LIRGeneratorX86Shared::lowerWasmBuiltinTruncateToInt32(
    MWasmBuiltinTruncateToInt32* ins) {
  MDefinition* opd = ins->input();
  MOZ_ASSERT(opd->type() == MIRType::Double || opd->type() == MIRType::Float32);

  LDefinition maybeTemp =
      Assembler::HasSSE3() ? LDefinition::BogusTemp() : tempDouble();
  if (opd->type() == MIRType::Double) {
    define(new (alloc()) LWasmBuiltinTruncateDToInt32(
               useRegister(opd), useFixed(ins->tls(), WasmTlsReg), maybeTemp),
           ins);
    return;
  }

  define(new (alloc()) LWasmBuiltinTruncateFToInt32(
             useRegister(opd), useFixed(ins->tls(), WasmTlsReg), maybeTemp),
         ins);
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

#ifdef ENABLE_WASM_SIMD

// These lowerings are really x86-shared but some Masm APIs are not yet
// available on x86.

// Ternary and binary operators require the dest register to be the same as
// their first input register, leading to a pattern of useRegisterAtStart +
// defineReuseInput.

void LIRGenerator::visitWasmBitselectSimd128(MWasmBitselectSimd128* ins) {
  MOZ_ASSERT(ins->lhs()->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->rhs()->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->control()->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  // Enforcing lhs == output avoids one setup move.  We would like to also
  // enforce merging the control with the temp (with usRegisterAtStart(control)
  // and tempCopy()), but the register allocator ignores those constraints
  // at present.

  auto* lir = new (alloc()) LWasmBitselectSimd128(
      useRegisterAtStart(ins->lhs()), useRegister(ins->rhs()),
      useRegister(ins->control()), tempSimd128());
  defineReuseInput(lir, ins, LWasmBitselectSimd128::LhsDest);
}

void LIRGenerator::visitWasmBinarySimd128(MWasmBinarySimd128* ins) {
  MDefinition* lhs = ins->lhs();
  MDefinition* rhs = ins->rhs();
  wasm::SimdOp op = ins->simdOp();

  MOZ_ASSERT(lhs->type() == MIRType::Simd128);
  MOZ_ASSERT(rhs->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  if (ins->isCommutative()) {
    ReorderCommutative(&lhs, &rhs, ins);
  }

  // Swap operands and change operation if necessary, these are all x86/x64
  // dependent transformations.  Except where noted, this is about avoiding
  // unnecessary moves and fixups in the code generator macros.
  bool swap = false;
  switch (op) {
    case wasm::SimdOp::V128AndNot: {
      // Code generation requires the operands to be reversed.
      swap = true;
      break;
    }
    case wasm::SimdOp::I8x16LtS: {
      swap = true;
      op = wasm::SimdOp::I8x16GtS;
      break;
    }
    case wasm::SimdOp::I8x16GeS: {
      swap = true;
      op = wasm::SimdOp::I8x16LeS;
      break;
    }
    case wasm::SimdOp::I16x8LtS: {
      swap = true;
      op = wasm::SimdOp::I16x8GtS;
      break;
    }
    case wasm::SimdOp::I16x8GeS: {
      swap = true;
      op = wasm::SimdOp::I16x8LeS;
      break;
    }
    case wasm::SimdOp::I32x4LtS: {
      swap = true;
      op = wasm::SimdOp::I32x4GtS;
      break;
    }
    case wasm::SimdOp::I32x4GeS: {
      swap = true;
      op = wasm::SimdOp::I32x4LeS;
      break;
    }
    case wasm::SimdOp::F32x4Gt: {
      swap = true;
      op = wasm::SimdOp::F32x4Lt;
      break;
    }
    case wasm::SimdOp::F32x4Ge: {
      swap = true;
      op = wasm::SimdOp::F32x4Le;
      break;
    }
    case wasm::SimdOp::F64x2Gt: {
      swap = true;
      op = wasm::SimdOp::F64x2Lt;
      break;
    }
    case wasm::SimdOp::F64x2Ge: {
      swap = true;
      op = wasm::SimdOp::F64x2Le;
      break;
    }
    case wasm::SimdOp::F32x4PMin:
    case wasm::SimdOp::F32x4PMax:
    case wasm::SimdOp::F64x2PMin:
    case wasm::SimdOp::F64x2PMax: {
      // Code generation requires the operations to be reversed (the rhs is the
      // output register).
      swap = true;
      break;
    }
    default:
      break;
  }
  if (swap) {
    MDefinition* tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  // Allocate temp registers
  LDefinition tempReg0 = LDefinition::BogusTemp();
  LDefinition tempReg1 = LDefinition::BogusTemp();
  switch (op) {
    case wasm::SimdOp::I64x2Mul:
    case wasm::SimdOp::V8x16Swizzle:
      tempReg0 = tempSimd128();
      break;
    case wasm::SimdOp::F32x4Min:
    case wasm::SimdOp::F32x4Max:
    case wasm::SimdOp::F64x2Min:
    case wasm::SimdOp::F64x2Max:
    case wasm::SimdOp::I8x16LtU:
    case wasm::SimdOp::I8x16GtU:
    case wasm::SimdOp::I8x16LeU:
    case wasm::SimdOp::I8x16GeU:
    case wasm::SimdOp::I16x8LtU:
    case wasm::SimdOp::I16x8GtU:
    case wasm::SimdOp::I16x8LeU:
    case wasm::SimdOp::I16x8GeU:
    case wasm::SimdOp::I32x4LtU:
    case wasm::SimdOp::I32x4GtU:
    case wasm::SimdOp::I32x4LeU:
    case wasm::SimdOp::I32x4GeU:
      tempReg0 = tempSimd128();
      tempReg1 = tempSimd128();
      break;
    default:
      break;
  }

  // For binary ops, the Masm API always is usually (rhs, lhsDest) and requires
  // AtStart+ReuseInput for the lhs.
  //
  // The rhs is tricky due to register allocator restrictions:
  //  - if lhs == rhs and lhs is AtStart then rhs must be AtStart too
  //  - if lhs != rhs and lhs is AtStart then rhs must not be AtStart,
  //    this appears to have something to do with risk of the rhs
  //    being clobbered.  Anyway it doesn't matter much, since the
  //    liveness of rhs will not prevent the lhs register to be reused
  //    for the output.
  //
  // For a few ops, the API is actually (rhsDest, lhs) and the rules are the
  // same but the reversed.  We swapped operands above; they will be swapped
  // again in the code generator to emit the right code.

  LAllocation lhsDestAlloc = useRegisterAtStart(lhs);
  LAllocation rhsAlloc =
      lhs != rhs ? useRegister(rhs) : useRegisterAtStart(rhs);
  auto* lir = new (alloc())
      LWasmBinarySimd128(op, lhsDestAlloc, rhsAlloc, tempReg0, tempReg1);
  defineReuseInput(lir, ins, LWasmBinarySimd128::LhsDest);
}

bool MWasmBinarySimd128::specializeForConstantRhs() {
  // The order follows MacroAssembler.h, generally
  switch (simdOp()) {
    // Operations implemented by a single native instruction where it is
    // plausible that the rhs (after commutation if available) could be a
    // constant.
    //
    // Swizzle is not here because it was handled earlier in the pipeline.
    //
    // Integer compares >= and < are not here because they are not supported in
    // the hardware.
    //
    // Floating compares are not here because our patching machinery can't
    // handle them yet.
    //
    // Floating-point min and max (including pmin and pmax) are not here because
    // they are not straightforward to implement.
    case wasm::SimdOp::I8x16Add:
    case wasm::SimdOp::I16x8Add:
    case wasm::SimdOp::I32x4Add:
    case wasm::SimdOp::I64x2Add:
    case wasm::SimdOp::I8x16Sub:
    case wasm::SimdOp::I16x8Sub:
    case wasm::SimdOp::I32x4Sub:
    case wasm::SimdOp::I64x2Sub:
    case wasm::SimdOp::I16x8Mul:
    case wasm::SimdOp::I32x4Mul:
    case wasm::SimdOp::I8x16AddSaturateS:
    case wasm::SimdOp::I8x16AddSaturateU:
    case wasm::SimdOp::I16x8AddSaturateS:
    case wasm::SimdOp::I16x8AddSaturateU:
    case wasm::SimdOp::I8x16SubSaturateS:
    case wasm::SimdOp::I8x16SubSaturateU:
    case wasm::SimdOp::I16x8SubSaturateS:
    case wasm::SimdOp::I16x8SubSaturateU:
    case wasm::SimdOp::I8x16MinS:
    case wasm::SimdOp::I8x16MinU:
    case wasm::SimdOp::I16x8MinS:
    case wasm::SimdOp::I16x8MinU:
    case wasm::SimdOp::I32x4MinS:
    case wasm::SimdOp::I32x4MinU:
    case wasm::SimdOp::I8x16MaxS:
    case wasm::SimdOp::I8x16MaxU:
    case wasm::SimdOp::I16x8MaxS:
    case wasm::SimdOp::I16x8MaxU:
    case wasm::SimdOp::I32x4MaxS:
    case wasm::SimdOp::I32x4MaxU:
    case wasm::SimdOp::V128And:
    case wasm::SimdOp::V128Or:
    case wasm::SimdOp::V128Xor:
    case wasm::SimdOp::I8x16Eq:
    case wasm::SimdOp::I8x16Ne:
    case wasm::SimdOp::I8x16GtS:
    case wasm::SimdOp::I8x16LeS:
    case wasm::SimdOp::I16x8Eq:
    case wasm::SimdOp::I16x8Ne:
    case wasm::SimdOp::I16x8GtS:
    case wasm::SimdOp::I16x8LeS:
    case wasm::SimdOp::I32x4Eq:
    case wasm::SimdOp::I32x4Ne:
    case wasm::SimdOp::I32x4GtS:
    case wasm::SimdOp::I32x4LeS:
    case wasm::SimdOp::I32x4DotSI16x8:
    case wasm::SimdOp::F32x4Add:
    case wasm::SimdOp::F64x2Add:
    case wasm::SimdOp::F32x4Sub:
    case wasm::SimdOp::F64x2Sub:
    case wasm::SimdOp::F32x4Div:
    case wasm::SimdOp::F64x2Div:
    case wasm::SimdOp::F32x4Mul:
    case wasm::SimdOp::F64x2Mul:
    case wasm::SimdOp::I8x16NarrowSI16x8:
    case wasm::SimdOp::I8x16NarrowUI16x8:
    case wasm::SimdOp::I16x8NarrowSI32x4:
    case wasm::SimdOp::I16x8NarrowUI32x4:
      return true;
    default:
      return false;
  }
}

void LIRGenerator::visitWasmBinarySimd128WithConstant(
    MWasmBinarySimd128WithConstant* ins) {
  MDefinition* lhs = ins->lhs();

  MOZ_ASSERT(lhs->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  // Always beneficial to reuse the lhs register here, see discussion in
  // visitWasmBinarySimd128() and also code in specializeForConstantRhs().

  LAllocation lhsDestAlloc = useRegisterAtStart(lhs);
  auto* lir =
      new (alloc()) LWasmBinarySimd128WithConstant(lhsDestAlloc, ins->rhs());
  defineReuseInput(lir, ins, LWasmBinarySimd128WithConstant::LhsDest);
}

void LIRGenerator::visitWasmShiftSimd128(MWasmShiftSimd128* ins) {
  MDefinition* lhs = ins->lhs();
  MDefinition* rhs = ins->rhs();

  MOZ_ASSERT(lhs->type() == MIRType::Simd128);
  MOZ_ASSERT(rhs->type() == MIRType::Int32);
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  if (rhs->isConstant()) {
    LDefinition temp = LDefinition::BogusTemp();
    int32_t shiftCount = rhs->toConstant()->toInt32();
    switch (ins->simdOp()) {
      case wasm::SimdOp::I8x16Shl:
      case wasm::SimdOp::I8x16ShrU:
        shiftCount &= 7;
        break;
      case wasm::SimdOp::I8x16ShrS:
        shiftCount &= 7;
        temp = tempSimd128();
        break;
      case wasm::SimdOp::I16x8Shl:
      case wasm::SimdOp::I16x8ShrU:
      case wasm::SimdOp::I16x8ShrS:
        shiftCount &= 15;
        break;
      case wasm::SimdOp::I32x4Shl:
      case wasm::SimdOp::I32x4ShrU:
      case wasm::SimdOp::I32x4ShrS:
        shiftCount &= 31;
        break;
      case wasm::SimdOp::I64x2Shl:
      case wasm::SimdOp::I64x2ShrU:
      case wasm::SimdOp::I64x2ShrS:
        shiftCount &= 63;
        break;
      default:
        MOZ_CRASH("Unexpected shift operation");
    }
#  ifdef DEBUG
    js::wasm::ReportSimdAnalysis("shift -> constant shift");
#  endif
    // Almost always beneficial, and never detrimental, to reuse the input if
    // possible.
    auto* lir = new (alloc())
        LWasmConstantShiftSimd128(useRegisterAtStart(lhs), temp, shiftCount);
    defineReuseInput(lir, ins, LWasmConstantShiftSimd128::Src);
    return;
  }

#  ifdef DEBUG
  js::wasm::ReportSimdAnalysis("shift -> variable shift");
#  endif

  LDefinition tempReg0 = LDefinition::BogusTemp();
  LDefinition tempReg1 = LDefinition::BogusTemp();
  switch (ins->simdOp()) {
    case wasm::SimdOp::I8x16Shl:
    case wasm::SimdOp::I8x16ShrS:
    case wasm::SimdOp::I8x16ShrU:
      tempReg0 = temp();
      tempReg1 = tempSimd128();
      break;
    default:
      tempReg0 = temp();
      break;
  }

  // Reusing the input if possible is never detrimental.
  LAllocation lhsDestAlloc = useRegisterAtStart(lhs);
  LAllocation rhsAlloc = useRegisterAtStart(rhs);
  auto* lir = new (alloc())
      LWasmVariableShiftSimd128(lhsDestAlloc, rhsAlloc, tempReg0, tempReg1);
  defineReuseInput(lir, ins, LWasmVariableShiftSimd128::LhsDest);
}

// Optimization of v8x16.shuffle.  The general byte shuffle+blend is very
// expensive (equivalent to at least a dozen instructions), and we want to avoid
// that if we can.  So look for special cases - there are many.
//
// The strategy is to sort the operation into one of three buckets depending
// on the shuffle pattern and inputs:
//
//  - single operand; shuffles on these values are rotations, reversals,
//    transpositions, and general permutations
//  - single-operand-with-interesting-constant (especially zero); shuffles on
//    these values are often byte shift or scatter operations
//  - dual operand; shuffles on these operations are blends, catenated
//    shifts, and (in the worst case) general shuffle+blends
//
// We're not trying to solve the general problem, only to lower reasonably
// expressed patterns that express common operations.  Producers that produce
// dense and convoluted patterns will end up with the general byte shuffle.
// Producers that produce simpler patterns that easily map to hardware will
// get faster code.
//
// In particular, these matchers do not try to combine transformations, so a
// shuffle that optimally is lowered to rotate + permute32x4 + rotate, say, is
// usually going to end up as a general byte shuffle.

// Representation of the result of the analysis.
struct Shuffle {
  enum class Operand {
    // Both inputs, in the original lhs-rhs order
    BOTH,
    // Both inputs, but in rhs-lhs order
    BOTH_SWAPPED,
    // Only the lhs input
    LEFT,
    // Only the rhs input
    RIGHT,
  };

  Operand opd;
  SimdConstant control;
  Maybe<LWasmPermuteSimd128::Op> permuteOp;  // Single operands
  Maybe<LWasmShuffleSimd128::Op> shuffleOp;  // Double operands

  static Shuffle permute(Operand opd, SimdConstant control,
                         LWasmPermuteSimd128::Op op) {
    MOZ_ASSERT(opd == Operand::LEFT || opd == Operand::RIGHT);
    Shuffle s{opd, control, Some(op), Nothing()};
    return s;
  }

  static Shuffle shuffle(Operand opd, SimdConstant control,
                         LWasmShuffleSimd128::Op op) {
    MOZ_ASSERT(opd == Operand::BOTH || opd == Operand::BOTH_SWAPPED);
    Shuffle s{opd, control, Nothing(), Some(op)};
    return s;
  }
};

// Reduce a 0..31 byte mask to a 0..15 word mask if possible and if so return
// true, updating *control.
static bool ByteMaskToWordMask(SimdConstant* control) {
  const SimdConstant::I8x16& lanes = control->asInt8x16();
  int16_t controlWords[8];
  for (int i = 0; i < 16; i += 2) {
    if (!((lanes[i] & 1) == 0 && lanes[i + 1] == lanes[i] + 1)) {
      return false;
    }
    controlWords[i / 2] = lanes[i] / 2;
  }
  *control = SimdConstant::CreateX8(controlWords);
  return true;
}

// Reduce a 0..31 byte mask to a 0..7 dword mask if possible and if so return
// true, updating *control.
static bool ByteMaskToDWordMask(SimdConstant* control) {
  const SimdConstant::I8x16& lanes = control->asInt8x16();
  int32_t controlDWords[4];
  for (int i = 0; i < 16; i += 4) {
    if (!((lanes[i] & 3) == 0 && lanes[i + 1] == lanes[i] + 1 &&
          lanes[i + 2] == lanes[i] + 2 && lanes[i + 3] == lanes[i] + 3)) {
      return false;
    }
    controlDWords[i / 4] = lanes[i] / 4;
  }
  *control = SimdConstant::CreateX4(controlDWords);
  return true;
}

// Skip across consecutive values in lanes starting at i, returning the index
// after the last element.  Lane values must be <= len-1 ("masked").
//
// Since every element is a 1-element run, the return value is never the same as
// the starting i.
template <typename T>
static int ScanIncreasingMasked(const T* lanes, int i) {
  int len = int(16 / sizeof(T));
  MOZ_ASSERT(i < len);
  MOZ_ASSERT(lanes[i] <= len - 1);
  i++;
  while (i < len && lanes[i] == lanes[i - 1] + 1) {
    MOZ_ASSERT(lanes[i] <= len - 1);
    i++;
  }
  return i;
}

// Skip across consecutive values in lanes starting at i, returning the index
// after the last element.  Lane values must be <= len*2-1 ("unmasked"); the
// values len-1 and len are not considered consecutive.
//
// Since every element is a 1-element run, the return value is never the same as
// the starting i.
template <typename T>
static int ScanIncreasingUnmasked(const T* lanes, int i) {
  int len = int(16 / sizeof(T));
  MOZ_ASSERT(i < len);
  if (lanes[i] < len) {
    i++;
    while (i < len && lanes[i] < len && lanes[i - 1] == lanes[i] - 1) {
      i++;
    }
  } else {
    i++;
    while (i < len && lanes[i] >= len && lanes[i - 1] == lanes[i] - 1) {
      i++;
    }
  }
  return i;
}

// Skip lanes that equal v starting at i, returning the index just beyond the
// last of those.  There is no requirement that the initial lanes[i] == v.
template <typename T>
static int ScanConstant(const T* lanes, int v, int i) {
  int len = int(16 / sizeof(T));
  MOZ_ASSERT(i <= len);
  while (i < len && lanes[i] == v) {
    i++;
  }
  return i;
}

// Mask lane values denoting rhs elements into lhs elements.
template <typename T>
static void MaskLanes(T* result, const T* input) {
  int len = int(16 / sizeof(T));
  for (int i = 0; i < len; i++) {
    result[i] = input[i] & (len - 1);
  }
}

// Apply a transformation to each lane value.
template <typename T>
static void MapLanes(T* result, const T* input, int (*f)(int)) {
  int len = int(16 / sizeof(T));
  for (int i = 0; i < len; i++) {
    result[i] = f(input[i]);
  }
}

// Recognize an identity permutation, assuming lanes is masked.
template <typename T>
static bool IsIdentity(const T* lanes) {
  return ScanIncreasingMasked(lanes, 0) == int(16 / sizeof(T));
}

// Recognize part of an identity permutation starting at start, with
// the first value of the permutation expected to be bias.
template <typename T>
static bool IsIdentity(const T* lanes, int start, int len, int bias) {
  if (lanes[start] != bias) {
    return false;
  }
  for (int i = start + 1; i < start + len; i++) {
    if (lanes[i] != lanes[i - 1] + 1) {
      return false;
    }
  }
  return true;
}

// We can permute by dwords if the mask is reducible to a dword mask, and in
// this case a single PSHUFD is enough.
static bool TryPermute32x4(SimdConstant* control) {
  SimdConstant tmp = *control;
  if (!ByteMaskToDWordMask(&tmp)) {
    return false;
  }
  *control = tmp;
  return true;
}

// Can we perform a byte rotate right?  We can use PALIGNR.  The shift count is
// just lanes[0], and *control is unchanged.
static bool TryRotateRight8x16(SimdConstant* control) {
  const SimdConstant::I8x16& lanes = control->asInt8x16();
  // Look for the first run of consecutive bytes.
  int i = ScanIncreasingMasked(lanes, 0);

  // If we reach the end of the vector, the vector must start at 0.
  if (i == 16) {
    return lanes[0] == 0;
  }

  // Second run must start at source lane zero
  if (lanes[i] != 0) {
    return false;
  }

  // Second run must end at the end of the lane vector.
  return ScanIncreasingMasked(lanes, i) == 16;
}

// We can permute by words if the mask is reducible to a word mask, but the x64
// lowering is only efficient if we can permute the high and low quadwords
// separately, possibly after swapping quadwords.
static bool TryPermute16x8(SimdConstant* control) {
  SimdConstant tmp = *control;
  if (!ByteMaskToWordMask(&tmp)) {
    return false;
  }
  const SimdConstant::I16x8& lanes = tmp.asInt16x8();
  SimdConstant::I16x8 mapped;
  MapLanes(mapped, lanes, [](int x) -> int { return x < 4 ? 0 : 1; });
  int i = ScanConstant(mapped, mapped[0], 0);
  if (i != 4) {
    return false;
  }
  i = ScanConstant(mapped, mapped[4], 4);
  if (i != 8) {
    return false;
  }
  // Now compute the operation bits.  `mapped` holds the adjusted lane mask.
  memcpy(mapped, lanes, sizeof(mapped));
  int16_t op = 0;
  if (mapped[0] > mapped[4]) {
    op |= LWasmPermuteSimd128::SWAP_QWORDS;
  }
  for (int i = 0; i < 8; i++) {
    mapped[i] &= 3;
  }
  if (!IsIdentity(mapped, 0, 4, 0)) {
    op |= LWasmPermuteSimd128::PERM_LOW;
  }
  if (!IsIdentity(mapped, 4, 4, 0)) {
    op |= LWasmPermuteSimd128::PERM_HIGH;
  }
  MOZ_ASSERT(op != 0);
  mapped[0] |= op << 8;
  *control = SimdConstant::CreateX8(mapped);
  return true;
}

// A single word lane is copied into all the other lanes: PSHUF*W + PSHUFD.
static bool TryBroadcast16x8(SimdConstant* control) {
  SimdConstant tmp = *control;
  if (!ByteMaskToWordMask(&tmp)) {
    return false;
  }
  const SimdConstant::I16x8& lanes = tmp.asInt16x8();
  if (ScanConstant(lanes, lanes[0], 0) < 8) {
    return false;
  }
  *control = tmp;
  return true;
}

// A single byte lane is copied int all the other lanes: PUNPCK*BW + PSHUF*W +
// PSHUFD.
static bool TryBroadcast8x16(SimdConstant* control) {
  const SimdConstant::I8x16& lanes = control->asInt8x16();
  if (ScanConstant(lanes, lanes[0], 0) < 16) {
    return false;
  }
  return true;
}

// Look for permutations of a single operand.
static LWasmPermuteSimd128::Op AnalyzePermute(SimdConstant* control) {
  // Lane indices are input-agnostic for single-operand permutations.
  SimdConstant::I8x16 controlBytes;
  MaskLanes(controlBytes, control->asInt8x16());

  // Get rid of no-ops immediately, so nobody else needs to check.
  if (IsIdentity(controlBytes)) {
    return LWasmPermuteSimd128::MOVE;
  }

  // Default control is the masked bytes.
  *control = SimdConstant::CreateX16(controlBytes);

  // Analysis order matters here and is architecture-dependent or even
  // microarchitecture-dependent: ideally the cheapest implementation first.
  // The Intel manual says that the cost of a PSHUFB is about five other
  // operations, so make that our cutoff.
  //
  // Word, dword, and qword reversals are handled optimally by general permutes.
  //
  // Byte reversals are probably best left to PSHUFB, no alternative rendition
  // seems to reliably go below five instructions.  (Discuss.)
  //
  // Word swaps within doublewords and dword swaps within quadwords are handled
  // optimally by general permutes.
  //
  // Dword and qword broadcasts are handled by dword permute.

  if (TryPermute32x4(control)) {
    return LWasmPermuteSimd128::PERMUTE_32x4;
  }
  if (TryRotateRight8x16(control)) {
    return LWasmPermuteSimd128::ROTATE_RIGHT_8x16;
  }
  if (TryPermute16x8(control)) {
    return LWasmPermuteSimd128::PERMUTE_16x8;
  }
  if (TryBroadcast16x8(control)) {
    return LWasmPermuteSimd128::BROADCAST_16x8;
  }
  if (TryBroadcast8x16(control)) {
    return LWasmPermuteSimd128::BROADCAST_8x16;
  }

  // TODO: (From v8) Unzip and transpose generally have renditions that slightly
  // beat a general permute (three or four instructions)
  //
  // TODO: (From MacroAssemblerX86Shared::ShuffleX4): MOVLHPS and MOVHLPS can be
  // used when merging two values.
  //
  // TODO: Byteswap is MOV + PSLLW + PSRLW + POR, a small win over PSHUFB.

  // The default operation is to permute bytes with the default control.
  return LWasmPermuteSimd128::PERMUTE_8x16;
}

// Can we shift the bytes left or right by a constant?  A shift is a run of
// lanes from the rhs (which is zero) on one end and a run of values from the
// lhs on the other end.
static Maybe<LWasmPermuteSimd128::Op> TryShift8x16(SimdConstant* control) {
  const SimdConstant::I8x16& lanes = control->asInt8x16();

  // Represent all zero lanes by 16
  SimdConstant::I8x16 zeroesMasked;
  MapLanes(zeroesMasked, lanes, [](int x) -> int { return x >= 16 ? 16 : x; });

  int i = ScanConstant(zeroesMasked, 16, 0);
  int shiftLeft = i;
  if (shiftLeft > 0 && lanes[shiftLeft] != 0) {
    return Nothing();
  }

  i = ScanIncreasingUnmasked(zeroesMasked, i);
  int shiftRight = 16 - i;
  if (shiftRight > 0 && lanes[i - 1] != 15) {
    return Nothing();
  }

  i = ScanConstant(zeroesMasked, 16, i);
  if (i < 16 || (shiftRight > 0 && shiftLeft > 0) ||
      (shiftRight == 0 && shiftLeft == 0)) {
    return Nothing();
  }

  if (shiftRight) {
    *control = SimdConstant::SplatX16(shiftRight);
    return Some(LWasmPermuteSimd128::SHIFT_RIGHT_8x16);
  }
  *control = SimdConstant::SplatX16(shiftLeft);
  return Some(LWasmPermuteSimd128::SHIFT_LEFT_8x16);
}

static Maybe<LWasmPermuteSimd128::Op> AnalyzeShuffleWithZero(
    SimdConstant* control) {
  Maybe<LWasmPermuteSimd128::Op> op;
  op = TryShift8x16(control);
  if (op) {
    return op;
  }

  // TODO: Optimization opportunity? A byte-blend-with-zero is just a CONST;
  // PAND.  This may beat the general byte blend code below.
  return Nothing();
}

// Concat: if the result is the suffix (high bytes) of the rhs in front of a
// prefix (low bytes) of the lhs then this is PALIGNR; ditto if the operands are
// swapped.
static Maybe<LWasmShuffleSimd128::Op> TryConcatRightShift8x16(
    SimdConstant* control, bool* swapOperands) {
  const SimdConstant::I8x16& lanes = control->asInt8x16();
  int i = ScanIncreasingUnmasked(lanes, 0);
  MOZ_ASSERT(i < 16, "Single-operand run should have been handled elswhere");
  // First run must end with 15 % 16
  if ((lanes[i - 1] & 15) != 15) {
    return Nothing();
  }
  // Second run must start with 0 % 16
  if ((lanes[i] & 15) != 0) {
    return Nothing();
  }
  // The two runs must come from different inputs
  if ((lanes[i] & 16) == (lanes[i - 1] & 16)) {
    return Nothing();
  }
  int suffixLength = i;

  i = ScanIncreasingUnmasked(lanes, i);
  // Must end at the left end
  if (i != 16) {
    return Nothing();
  }

  // If the suffix is from the lhs then swap the operands
  if (lanes[0] < 16) {
    *swapOperands = !*swapOperands;
  }
  *control = SimdConstant::SplatX16(suffixLength);
  return Some(LWasmShuffleSimd128::CONCAT_RIGHT_SHIFT_8x16);
}

// Blend words: if we pick words from both operands without a pattern but all
// the input words stay in their position then this is PBLENDW (immediate mask);
// this also handles all larger sizes on x64.
static Maybe<LWasmShuffleSimd128::Op> TryBlendInt16x8(SimdConstant* control) {
  SimdConstant tmp(*control);
  if (!ByteMaskToWordMask(&tmp)) {
    return Nothing();
  }
  SimdConstant::I16x8 masked;
  MaskLanes(masked, tmp.asInt16x8());
  if (!IsIdentity(masked)) {
    return Nothing();
  }
  SimdConstant::I16x8 mapped;
  MapLanes(mapped, tmp.asInt16x8(),
           [](int x) -> int { return x < 8 ? 0 : -1; });
  *control = SimdConstant::CreateX8(mapped);
  return Some(LWasmShuffleSimd128::BLEND_16x8);
}

// Blend bytes: if we pick bytes ditto then this is a byte blend, which can be
// handled with a CONST, PAND, PANDNOT, and POR.
//
// TODO: Optimization opportunity? If we pick all but one lanes from one with at
// most one from the other then it could be a MOV + PEXRB + PINSRB (also if this
// element is not in its source location).
static Maybe<LWasmShuffleSimd128::Op> TryBlendInt8x16(SimdConstant* control) {
  SimdConstant::I8x16 masked;
  MaskLanes(masked, control->asInt8x16());
  if (!IsIdentity(masked)) {
    return Nothing();
  }
  SimdConstant::I8x16 mapped;
  MapLanes(mapped, control->asInt8x16(),
           [](int x) -> int { return x < 16 ? 0 : -1; });
  *control = SimdConstant::CreateX16(mapped);
  return Some(LWasmShuffleSimd128::BLEND_8x16);
}

template <typename T>
static bool MatchInterleave(const T* lanes, int lhs, int rhs, int len) {
  for (int i = 0; i < len; i++) {
    if (lanes[i * 2] != lhs + i || lanes[i * 2 + 1] != rhs + i) {
      return false;
    }
  }
  return true;
}

// Unpack/interleave:
//  - if we interleave the low (bytes/words/doublewords) of the inputs into
//    the output then this is UNPCKL*W (possibly with a swap of operands).
//  - if we interleave the high ditto then it is UNPCKH*W (ditto)
template <typename T>
static Maybe<LWasmShuffleSimd128::Op> TryInterleave(
    const T* lanes, int lhs, int rhs, bool* swapOperands,
    LWasmShuffleSimd128::Op lowOp, LWasmShuffleSimd128::Op highOp) {
  int len = int(32 / (sizeof(T) * 4));
  if (MatchInterleave(lanes, lhs, rhs, len)) {
    return Some(lowOp);
  }
  if (MatchInterleave(lanes, rhs, lhs, len)) {
    *swapOperands = !*swapOperands;
    return Some(lowOp);
  }
  if (MatchInterleave(lanes, lhs + len, rhs + len, len)) {
    return Some(highOp);
  }
  if (MatchInterleave(lanes, rhs + len, lhs + len, len)) {
    *swapOperands = !*swapOperands;
    return Some(highOp);
  }
  return Nothing();
}

static Maybe<LWasmShuffleSimd128::Op> TryInterleave32x4(SimdConstant* control,
                                                        bool* swapOperands) {
  SimdConstant tmp = *control;
  if (!ByteMaskToDWordMask(&tmp)) {
    return Nothing();
  }
  const SimdConstant::I32x4& lanes = tmp.asInt32x4();
  return TryInterleave(lanes, 0, 4, swapOperands,
                       LWasmShuffleSimd128::INTERLEAVE_LOW_32x4,
                       LWasmShuffleSimd128::INTERLEAVE_HIGH_32x4);
}

static Maybe<LWasmShuffleSimd128::Op> TryInterleave16x8(SimdConstant* control,
                                                        bool* swapOperands) {
  SimdConstant tmp = *control;
  if (!ByteMaskToWordMask(&tmp)) {
    return Nothing();
  }
  const SimdConstant::I16x8& lanes = tmp.asInt16x8();
  return TryInterleave(lanes, 0, 8, swapOperands,
                       LWasmShuffleSimd128::INTERLEAVE_LOW_16x8,
                       LWasmShuffleSimd128::INTERLEAVE_HIGH_16x8);
}

static Maybe<LWasmShuffleSimd128::Op> TryInterleave8x16(SimdConstant* control,
                                                        bool* swapOperands) {
  const SimdConstant::I8x16& lanes = control->asInt8x16();
  return TryInterleave(lanes, 0, 16, swapOperands,
                       LWasmShuffleSimd128::INTERLEAVE_LOW_8x16,
                       LWasmShuffleSimd128::INTERLEAVE_HIGH_8x16);
}

static LWasmShuffleSimd128::Op AnalyzeTwoArgShuffle(SimdConstant* control,
                                                    bool* swapOperands) {
  Maybe<LWasmShuffleSimd128::Op> op;
  op = TryConcatRightShift8x16(control, swapOperands);
  if (!op) {
    op = TryBlendInt16x8(control);
  }
  if (!op) {
    op = TryBlendInt8x16(control);
  }
  if (!op) {
    op = TryInterleave32x4(control, swapOperands);
  }
  if (!op) {
    op = TryInterleave16x8(control, swapOperands);
  }
  if (!op) {
    op = TryInterleave8x16(control, swapOperands);
  }
  if (!op) {
    op = Some(LWasmShuffleSimd128::SHUFFLE_BLEND_8x16);
  }
  return *op;
}

// Reorder the operands if that seems useful, notably, move a constant to the
// right hand side.  Rewrites the control to account for any move.
static bool MaybeReorderShuffleOperands(MDefinition** lhs, MDefinition** rhs,
                                        SimdConstant* control) {
  if ((*lhs)->isWasmFloatConstant()) {
    MDefinition* tmp = *lhs;
    *lhs = *rhs;
    *rhs = tmp;

    int8_t controlBytes[16];
    const SimdConstant::I8x16& lanes = control->asInt8x16();
    for (unsigned i = 0; i < 16; i++) {
      controlBytes[i] = lanes[i] ^ 16;
    }
    *control = SimdConstant::CreateX16(controlBytes);

    return true;
  }
  return false;
}

static Shuffle AnalyzeShuffle(MWasmShuffleSimd128* ins) {
  // Control may be updated, but only once we commit to an operation or when we
  // swap operands.
  SimdConstant control = ins->control();
  MDefinition* lhs = ins->lhs();
  MDefinition* rhs = ins->rhs();

  // If only one of the inputs is used, determine which.
  bool useLeft = true;
  bool useRight = true;
  if (lhs == rhs) {
    useRight = false;
  } else {
    bool allAbove = true;
    bool allBelow = true;
    const SimdConstant::I8x16& lanes = control.asInt8x16();
    for (unsigned i = 0; i < 16; i++) {
      allAbove = allAbove && lanes[i] >= 16;
      allBelow = allBelow && lanes[i] < 16;
    }
    if (allAbove) {
      useLeft = false;
    } else if (allBelow) {
      useRight = false;
    }
  }

  // Deal with one-ignored-input.
  if (!(useLeft && useRight)) {
    LWasmPermuteSimd128::Op op = AnalyzePermute(&control);
    return Shuffle::permute(
        useLeft ? Shuffle::Operand::LEFT : Shuffle::Operand::RIGHT, control,
        op);
  }

  // Move constants to rhs.
  bool swapOperands = MaybeReorderShuffleOperands(&lhs, &rhs, &control);

  // Deal with constant rhs.
  if (rhs->isWasmFloatConstant()) {
    SimdConstant rhsConstant = rhs->toWasmFloatConstant()->toSimd128();
    if (rhsConstant.isZeroBits()) {
      Maybe<LWasmPermuteSimd128::Op> op = AnalyzeShuffleWithZero(&control);
      if (op) {
        return Shuffle::permute(
            swapOperands ? Shuffle::Operand::RIGHT : Shuffle::Operand::LEFT,
            control, *op);
      }
    }
  }

  // Two operands both of which are used.  If there's one constant operand it is
  // now on the rhs.
  LWasmShuffleSimd128::Op op = AnalyzeTwoArgShuffle(&control, &swapOperands);
  return Shuffle::shuffle(
      swapOperands ? Shuffle::Operand::BOTH_SWAPPED : Shuffle::Operand::BOTH,
      control, op);
}

#  ifdef DEBUG
static void ReportShuffleSpecialization(const Shuffle& s) {
  switch (s.opd) {
    case Shuffle::Operand::BOTH:
    case Shuffle::Operand::BOTH_SWAPPED:
      switch (*s.shuffleOp) {
        case LWasmShuffleSimd128::SHUFFLE_BLEND_8x16:
          js::wasm::ReportSimdAnalysis("shuffle -> shuffle+blend 8x16");
          break;
        case LWasmShuffleSimd128::BLEND_8x16:
          js::wasm::ReportSimdAnalysis("shuffle -> blend 8x16");
          break;
        case LWasmShuffleSimd128::BLEND_16x8:
          js::wasm::ReportSimdAnalysis("shuffle -> blend 16x8");
          break;
        case LWasmShuffleSimd128::CONCAT_RIGHT_SHIFT_8x16:
          js::wasm::ReportSimdAnalysis("shuffle -> concat+shift-right 8x16");
          break;
        case LWasmShuffleSimd128::INTERLEAVE_HIGH_8x16:
          js::wasm::ReportSimdAnalysis("shuffle -> interleave-high 8x16");
          break;
        case LWasmShuffleSimd128::INTERLEAVE_HIGH_16x8:
          js::wasm::ReportSimdAnalysis("shuffle -> interleave-high 16x8");
          break;
        case LWasmShuffleSimd128::INTERLEAVE_HIGH_32x4:
          js::wasm::ReportSimdAnalysis("shuffle -> interleave-high 32x4");
          break;
        case LWasmShuffleSimd128::INTERLEAVE_LOW_8x16:
          js::wasm::ReportSimdAnalysis("shuffle -> interleave-low 8x16");
          break;
        case LWasmShuffleSimd128::INTERLEAVE_LOW_16x8:
          js::wasm::ReportSimdAnalysis("shuffle -> interleave-low 16x8");
          break;
        case LWasmShuffleSimd128::INTERLEAVE_LOW_32x4:
          js::wasm::ReportSimdAnalysis("shuffle -> interleave-low 32x4");
          break;
        default:
          MOZ_CRASH("Unexpected shuffle op");
      }
      break;
    case Shuffle::Operand::LEFT:
    case Shuffle::Operand::RIGHT:
      switch (*s.permuteOp) {
        case LWasmPermuteSimd128::BROADCAST_8x16:
          js::wasm::ReportSimdAnalysis("shuffle -> broadcast 8x16");
          break;
        case LWasmPermuteSimd128::BROADCAST_16x8:
          js::wasm::ReportSimdAnalysis("shuffle -> broadcast 16x8");
          break;
        case LWasmPermuteSimd128::MOVE:
          js::wasm::ReportSimdAnalysis("shuffle -> move");
          break;
        case LWasmPermuteSimd128::PERMUTE_8x16:
          js::wasm::ReportSimdAnalysis("shuffle -> permute 8x16");
          break;
        case LWasmPermuteSimd128::PERMUTE_16x8: {
          int op = s.control.asInt16x8()[0] >> 8;
          char buf[256];
          sprintf(buf, "shuffle -> permute 16x8%s%s%s",
                  op & LWasmPermuteSimd128::SWAP_QWORDS ? " swap" : "",
                  op & LWasmPermuteSimd128::PERM_HIGH ? " high" : "",
                  op & LWasmPermuteSimd128::PERM_LOW ? " low" : "");
          js::wasm::ReportSimdAnalysis(buf);
          break;
        }
        case LWasmPermuteSimd128::PERMUTE_32x4:
          js::wasm::ReportSimdAnalysis("shuffle -> permute 32x4");
          break;
        case LWasmPermuteSimd128::ROTATE_RIGHT_8x16:
          js::wasm::ReportSimdAnalysis("shuffle -> rotate-right 8x16");
          break;
        case LWasmPermuteSimd128::SHIFT_LEFT_8x16:
          js::wasm::ReportSimdAnalysis("shuffle -> shift-left 8x16");
          break;
        case LWasmPermuteSimd128::SHIFT_RIGHT_8x16:
          js::wasm::ReportSimdAnalysis("shuffle -> shift-right 8x16");
          break;
        default:
          MOZ_CRASH("Unexpected permute op");
      }
      break;
  }
}
#  endif

void LIRGenerator::visitWasmShuffleSimd128(MWasmShuffleSimd128* ins) {
  MOZ_ASSERT(ins->lhs()->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->rhs()->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  Shuffle s = AnalyzeShuffle(ins);
#  ifdef DEBUG
  ReportShuffleSpecialization(s);
#  endif
  switch (s.opd) {
    case Shuffle::Operand::LEFT:
    case Shuffle::Operand::RIGHT: {
      LAllocation src;
      // All permute operators currently favor reusing the input register so
      // we're not currently exercising code paths below that do not reuse.
      // Those paths have been exercised in the past however and are believed
      // to be correct.
      bool useAtStartAndReuse = false;
      switch (*s.permuteOp) {
        case LWasmPermuteSimd128::MOVE:
        case LWasmPermuteSimd128::BROADCAST_8x16:
        case LWasmPermuteSimd128::BROADCAST_16x8:
        case LWasmPermuteSimd128::PERMUTE_8x16:
        case LWasmPermuteSimd128::PERMUTE_16x8:
        case LWasmPermuteSimd128::PERMUTE_32x4:
        case LWasmPermuteSimd128::ROTATE_RIGHT_8x16:
        case LWasmPermuteSimd128::SHIFT_LEFT_8x16:
        case LWasmPermuteSimd128::SHIFT_RIGHT_8x16:
          useAtStartAndReuse = true;
          break;
        default:
          MOZ_CRASH("Unexpected operator");
      }
      if (s.opd == Shuffle::Operand::LEFT) {
        if (useAtStartAndReuse) {
          src = useRegisterAtStart(ins->lhs());
        } else {
          src = useRegister(ins->lhs());
        }
      } else {
        if (useAtStartAndReuse) {
          src = useRegisterAtStart(ins->rhs());
        } else {
          src = useRegister(ins->rhs());
        }
      }
      auto* lir =
          new (alloc()) LWasmPermuteSimd128(src, *s.permuteOp, s.control);
      if (useAtStartAndReuse) {
        defineReuseInput(lir, ins, LWasmPermuteSimd128::Src);
      } else {
        define(lir, ins);
      }
      break;
    }
    case Shuffle::Operand::BOTH:
    case Shuffle::Operand::BOTH_SWAPPED: {
      LDefinition temp = LDefinition::BogusTemp();
      switch (*s.shuffleOp) {
        case LWasmShuffleSimd128::BLEND_8x16:
          temp = tempSimd128();
          break;
        default:
          break;
      }
      LAllocation lhs;
      LAllocation rhs;
      if (s.opd == Shuffle::Operand::BOTH) {
        lhs = useRegisterAtStart(ins->lhs());
        rhs = useRegister(ins->rhs());
      } else {
        lhs = useRegisterAtStart(ins->rhs());
        rhs = useRegister(ins->lhs());
      }
      auto* lir = new (alloc())
          LWasmShuffleSimd128(lhs, rhs, temp, *s.shuffleOp, s.control);
      defineReuseInput(lir, ins, LWasmShuffleSimd128::LhsDest);
      break;
    }
  }
}

void LIRGenerator::visitWasmReplaceLaneSimd128(MWasmReplaceLaneSimd128* ins) {
  MOZ_ASSERT(ins->lhs()->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  // The Masm API is (rhs, lhsDest) and requires AtStart+ReuseInput for the lhs.
  // For type reasons, the rhs will never be the same as the lhs and is
  // therefore a plain Use.

  if (ins->rhs()->type() == MIRType::Int64) {
    auto* lir = new (alloc()) LWasmReplaceInt64LaneSimd128(
        useRegisterAtStart(ins->lhs()), useInt64Register(ins->rhs()));
    defineReuseInput(lir, ins, LWasmReplaceInt64LaneSimd128::LhsDest);
  } else {
    auto* lir = new (alloc()) LWasmReplaceLaneSimd128(
        useRegisterAtStart(ins->lhs()), useRegister(ins->rhs()));
    defineReuseInput(lir, ins, LWasmReplaceLaneSimd128::LhsDest);
  }
}

void LIRGenerator::visitWasmScalarToSimd128(MWasmScalarToSimd128* ins) {
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  switch (ins->input()->type()) {
    case MIRType::Int64: {
      // 64-bit integer splats.
      // Load-and-(sign|zero)extend.
      auto* lir = new (alloc())
          LWasmInt64ToSimd128(useInt64RegisterAtStart(ins->input()));
      define(lir, ins);
      break;
    }
    case MIRType::Float32:
    case MIRType::Double: {
      // Floating-point splats.
      // Ideally we save a move on SSE systems by reusing the input register,
      // but since the input and output register types differ, we can't.
      auto* lir =
          new (alloc()) LWasmScalarToSimd128(useRegisterAtStart(ins->input()));
      define(lir, ins);
      break;
    }
    default: {
      // 32-bit integer splats.
      auto* lir =
          new (alloc()) LWasmScalarToSimd128(useRegisterAtStart(ins->input()));
      define(lir, ins);
      break;
    }
  }
}

void LIRGenerator::visitWasmUnarySimd128(MWasmUnarySimd128* ins) {
  MOZ_ASSERT(ins->input()->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  bool useAtStart = false;
  bool reuseInput = false;
  LDefinition tempReg = LDefinition::BogusTemp();
  switch (ins->simdOp()) {
    case wasm::SimdOp::I8x16Neg:
    case wasm::SimdOp::I16x8Neg:
    case wasm::SimdOp::I32x4Neg:
    case wasm::SimdOp::I64x2Neg:
      // Prefer src != dest to avoid an unconditional src->temp move.
      MOZ_ASSERT(!useAtStart && !reuseInput);
      break;
    case wasm::SimdOp::F32x4Neg:
    case wasm::SimdOp::F64x2Neg:
    case wasm::SimdOp::F32x4Abs:
    case wasm::SimdOp::F64x2Abs:
    case wasm::SimdOp::V128Not:
    case wasm::SimdOp::F32x4Sqrt:
    case wasm::SimdOp::F64x2Sqrt:
    case wasm::SimdOp::I8x16Abs:
    case wasm::SimdOp::I16x8Abs:
    case wasm::SimdOp::I32x4Abs:
    case wasm::SimdOp::I32x4TruncSSatF32x4:
    case wasm::SimdOp::F32x4ConvertUI32x4:
      // Prefer src == dest to avoid an unconditional src->dest move.
      useAtStart = true;
      reuseInput = true;
      break;
    case wasm::SimdOp::I32x4TruncUSatF32x4:
      tempReg = tempSimd128();
      // Prefer src == dest to avoid an unconditional src->dest move.
      useAtStart = true;
      reuseInput = true;
      break;
    case wasm::SimdOp::I16x8WidenLowSI8x16:
    case wasm::SimdOp::I16x8WidenHighSI8x16:
    case wasm::SimdOp::I16x8WidenLowUI8x16:
    case wasm::SimdOp::I16x8WidenHighUI8x16:
    case wasm::SimdOp::I32x4WidenLowSI16x8:
    case wasm::SimdOp::I32x4WidenHighSI16x8:
    case wasm::SimdOp::I32x4WidenLowUI16x8:
    case wasm::SimdOp::I32x4WidenHighUI16x8:
    case wasm::SimdOp::F32x4ConvertSI32x4:
    case wasm::SimdOp::F32x4Ceil:
    case wasm::SimdOp::F32x4Floor:
    case wasm::SimdOp::F32x4Trunc:
    case wasm::SimdOp::F32x4Nearest:
    case wasm::SimdOp::F64x2Ceil:
    case wasm::SimdOp::F64x2Floor:
    case wasm::SimdOp::F64x2Trunc:
    case wasm::SimdOp::F64x2Nearest:
      // Prefer src == dest to exert the lowest register pressure on the
      // surrounding code.
      useAtStart = true;
      MOZ_ASSERT(!reuseInput);
      break;
    default:
      MOZ_CRASH("Unary SimdOp not implemented");
  }

  LUse inputUse =
      useAtStart ? useRegisterAtStart(ins->input()) : useRegister(ins->input());
  LWasmUnarySimd128* lir = new (alloc()) LWasmUnarySimd128(inputUse, tempReg);
  if (reuseInput) {
    defineReuseInput(lir, ins, LWasmUnarySimd128::Src);
  } else {
    define(lir, ins);
  }
}

bool LIRGeneratorX86Shared::canFoldReduceSimd128AndBranch(wasm::SimdOp op) {
  switch (op) {
    case wasm::SimdOp::I8x16AnyTrue:
    case wasm::SimdOp::I16x8AnyTrue:
    case wasm::SimdOp::I32x4AnyTrue:
    case wasm::SimdOp::I8x16AllTrue:
    case wasm::SimdOp::I16x8AllTrue:
    case wasm::SimdOp::I32x4AllTrue:
    case wasm::SimdOp::I16x8Bitmask:
      return true;
    default:
      return false;
  }
}

bool LIRGeneratorX86Shared::canEmitWasmReduceSimd128AtUses(
    MWasmReduceSimd128* ins) {
  if (!ins->canEmitAtUses()) {
    return false;
  }
  // Only specific ops generating int32.
  if (ins->type() != MIRType::Int32) {
    return false;
  }
  if (!canFoldReduceSimd128AndBranch(ins->simdOp())) {
    return false;
  }
  // If never used then defer (it will be removed).
  MUseIterator iter(ins->usesBegin());
  if (iter == ins->usesEnd()) {
    return true;
  }
  // We require an MTest consumer.
  MNode* node = iter->consumer();
  if (!node->isDefinition() || !node->toDefinition()->isTest()) {
    return false;
  }
  // Defer only if there's only one use.
  iter++;
  return iter == ins->usesEnd();
}

void LIRGenerator::visitWasmReduceSimd128(MWasmReduceSimd128* ins) {
  if (canEmitWasmReduceSimd128AtUses(ins)) {
    emitAtUses(ins);
    return;
  }

  // Reductions (any_true, all_true, bitmask, extract_lane) uniformly prefer
  // useRegisterAtStart:
  //
  // - In most cases, the input type differs from the output type, so there's no
  //   conflict and it doesn't really matter.
  //
  // - For extract_lane(0) on F32x4 and F64x2, input == output results in zero
  //   code being generated.
  //
  // - For extract_lane(k > 0) on F32x4 and F64x2, allowing the input register
  //   to be targeted lowers register pressure if it's the last use of the
  //   input.

  if (ins->type() == MIRType::Int64) {
    auto* lir = new (alloc())
        LWasmReduceSimd128ToInt64(useRegisterAtStart(ins->input()));
    defineInt64(lir, ins);
  } else {
    // Ideally we would reuse the input register for floating extract_lane if
    // the lane is zero, but constraints in the register allocator require the
    // input and output register types to be the same.
    auto* lir =
        new (alloc()) LWasmReduceSimd128(useRegisterAtStart(ins->input()));
    define(lir, ins);
  }
}

#endif  // ENABLE_WASM_SIMD
