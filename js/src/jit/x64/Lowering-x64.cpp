/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x64/Lowering-x64.h"

#include "jit/Lowering.h"
#include "jit/MIR.h"
#include "jit/x64/Assembler-x64.h"

#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

LBoxAllocation LIRGeneratorX64::useBoxFixed(MDefinition* mir, Register reg1,
                                            Register, bool useAtStart) {
  MOZ_ASSERT(mir->type() == MIRType::Value);

  ensureDefined(mir);
  return LBoxAllocation(LUse(reg1, mir->virtualRegister(), useAtStart));
}

LAllocation LIRGeneratorX64::useByteOpRegister(MDefinition* mir) {
  return useRegister(mir);
}

LAllocation LIRGeneratorX64::useByteOpRegisterAtStart(MDefinition* mir) {
  return useRegisterAtStart(mir);
}

LAllocation LIRGeneratorX64::useByteOpRegisterOrNonDoubleConstant(
    MDefinition* mir) {
  return useRegisterOrNonDoubleConstant(mir);
}

LDefinition LIRGeneratorX64::tempByteOpRegister() { return temp(); }

LDefinition LIRGeneratorX64::tempToUnbox() { return temp(); }

void LIRGeneratorX64::lowerForALUInt64(
    LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs) {
  ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));
  ins->setInt64Operand(INT64_PIECES, lhs != rhs
                                         ? useInt64OrConstant(rhs)
                                         : useInt64OrConstantAtStart(rhs));
  defineInt64ReuseInput(ins, mir, 0);
}

void LIRGeneratorX64::lowerForMulInt64(LMulI64* ins, MMul* mir,
                                       MDefinition* lhs, MDefinition* rhs) {
  // X64 doesn't need a temp for 64bit multiplication.
  ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));
  ins->setInt64Operand(INT64_PIECES, lhs != rhs
                                         ? useInt64OrConstant(rhs)
                                         : useInt64OrConstantAtStart(rhs));
  defineInt64ReuseInput(ins, mir, 0);
}

void LIRGenerator::visitBox(MBox* box) {
  MDefinition* opd = box->getOperand(0);

  // If the operand is a constant, emit near its uses.
  if (opd->isConstant() && box->canEmitAtUses()) {
    emitAtUses(box);
    return;
  }

  if (opd->isConstant()) {
    define(new (alloc()) LValue(opd->toConstant()->toJSValue()), box,
           LDefinition(LDefinition::BOX));
  } else {
    LBox* ins = new (alloc()) LBox(useRegister(opd), opd->type());
    define(ins, box, LDefinition(LDefinition::BOX));
  }
}

void LIRGenerator::visitUnbox(MUnbox* unbox) {
  MDefinition* box = unbox->getOperand(0);

  if (box->type() == MIRType::ObjectOrNull) {
    LUnboxObjectOrNull* lir =
        new (alloc()) LUnboxObjectOrNull(useRegisterAtStart(box));
    if (unbox->fallible()) {
      assignSnapshot(lir, unbox->bailoutKind());
    }
    defineReuseInput(lir, unbox, 0);
    return;
  }

  MOZ_ASSERT(box->type() == MIRType::Value);

  LUnboxBase* lir;
  if (IsFloatingPointType(unbox->type())) {
    lir = new (alloc())
        LUnboxFloatingPoint(useRegisterAtStart(box), unbox->type());
  } else if (unbox->fallible()) {
    // If the unbox is fallible, load the Value in a register first to
    // avoid multiple loads.
    lir = new (alloc()) LUnbox(useRegisterAtStart(box));
  } else {
    lir = new (alloc()) LUnbox(useAtStart(box));
  }

  if (unbox->fallible()) {
    assignSnapshot(lir, unbox->bailoutKind());
  }

  define(lir, unbox);
}

void LIRGenerator::visitReturn(MReturn* ret) {
  MDefinition* opd = ret->getOperand(0);
  MOZ_ASSERT(opd->type() == MIRType::Value);

  LReturn* ins = new (alloc()) LReturn;
  ins->setOperand(0, useFixed(opd, JSReturnReg));
  add(ins);
}

void LIRGeneratorX64::lowerUntypedPhiInput(MPhi* phi, uint32_t inputPosition,
                                           LBlock* block, size_t lirIndex) {
  lowerTypedPhiInput(phi, inputPosition, block, lirIndex);
}

void LIRGeneratorX64::defineInt64Phi(MPhi* phi, size_t lirIndex) {
  defineTypedPhi(phi, lirIndex);
}

void LIRGeneratorX64::lowerInt64PhiInput(MPhi* phi, uint32_t inputPosition,
                                         LBlock* block, size_t lirIndex) {
  lowerTypedPhiInput(phi, inputPosition, block, lirIndex);
}

void LIRGenerator::visitCompareExchangeTypedArrayElement(
    MCompareExchangeTypedArrayElement* ins) {
  lowerCompareExchangeTypedArrayElement(ins,
                                        /* useI386ByteRegisters = */ false);
}

void LIRGenerator::visitAtomicExchangeTypedArrayElement(
    MAtomicExchangeTypedArrayElement* ins) {
  lowerAtomicExchangeTypedArrayElement(ins, /* useI386ByteRegisters = */ false);
}

void LIRGenerator::visitAtomicTypedArrayElementBinop(
    MAtomicTypedArrayElementBinop* ins) {
  lowerAtomicTypedArrayElementBinop(ins, /* useI386ByteRegisters = */ false);
}

void LIRGenerator::visitWasmUnsignedToDouble(MWasmUnsignedToDouble* ins) {
  MOZ_ASSERT(ins->input()->type() == MIRType::Int32);
  LWasmUint32ToDouble* lir =
      new (alloc()) LWasmUint32ToDouble(useRegisterAtStart(ins->input()));
  define(lir, ins);
}

void LIRGenerator::visitWasmUnsignedToFloat32(MWasmUnsignedToFloat32* ins) {
  MOZ_ASSERT(ins->input()->type() == MIRType::Int32);
  LWasmUint32ToFloat32* lir =
      new (alloc()) LWasmUint32ToFloat32(useRegisterAtStart(ins->input()));
  define(lir, ins);
}

void LIRGenerator::visitWasmHeapBase(MWasmHeapBase* ins) {
  auto* lir = new (alloc()) LWasmHeapBase(LAllocation());
  define(lir, ins);
}

void LIRGenerator::visitWasmLoad(MWasmLoad* ins) {
  MDefinition* base = ins->base();
  MOZ_ASSERT(base->type() == MIRType::Int32);

  if (ins->type() != MIRType::Int64) {
    auto* lir = new (alloc()) LWasmLoad(useRegisterOrZeroAtStart(base));
    define(lir, ins);
    return;
  }

  auto* lir = new (alloc()) LWasmLoadI64(useRegisterOrZeroAtStart(base));
  defineInt64(lir, ins);
}

void LIRGenerator::visitWasmStore(MWasmStore* ins) {
  MDefinition* base = ins->base();
  MOZ_ASSERT(base->type() == MIRType::Int32);

  MDefinition* value = ins->value();
  LAllocation valueAlloc;
  switch (ins->access().type()) {
    case Scalar::Int8:
    case Scalar::Uint8:
    case Scalar::Int16:
    case Scalar::Uint16:
    case Scalar::Int32:
    case Scalar::Uint32:
      valueAlloc = useRegisterOrConstantAtStart(value);
      break;
    case Scalar::Int64:
      // No way to encode an int64-to-memory move on x64.
      if (value->isConstant() && value->type() != MIRType::Int64) {
        valueAlloc = useOrConstantAtStart(value);
      } else {
        valueAlloc = useRegisterAtStart(value);
      }
      break;
    case Scalar::Float32:
    case Scalar::Float64:
      valueAlloc = useRegisterAtStart(value);
      break;
    case Scalar::Simd128:
#ifdef ENABLE_WASM_SIMD
      valueAlloc = useRegisterAtStart(value);
      break;
#else
      MOZ_CRASH("unexpected array type");
#endif
    case Scalar::BigInt64:
    case Scalar::BigUint64:
    case Scalar::Uint8Clamped:
    case Scalar::MaxTypedArrayViewType:
      MOZ_CRASH("unexpected array type");
  }

  LAllocation baseAlloc = useRegisterOrZeroAtStart(base);
  auto* lir = new (alloc()) LWasmStore(baseAlloc, valueAlloc);
  add(lir, ins);
}

void LIRGenerator::visitWasmCompareExchangeHeap(MWasmCompareExchangeHeap* ins) {
  MDefinition* base = ins->base();
  MOZ_ASSERT(base->type() == MIRType::Int32);

  // The output may not be used but will be clobbered regardless, so
  // pin the output to eax.
  //
  // The input values must both be in registers.

  const LAllocation oldval = useRegister(ins->oldValue());
  const LAllocation newval = useRegister(ins->newValue());

  LWasmCompareExchangeHeap* lir =
      new (alloc()) LWasmCompareExchangeHeap(useRegister(base), oldval, newval);

  defineFixed(lir, ins, LAllocation(AnyRegister(eax)));
}

void LIRGenerator::visitWasmAtomicExchangeHeap(MWasmAtomicExchangeHeap* ins) {
  MOZ_ASSERT(ins->base()->type() == MIRType::Int32);

  const LAllocation base = useRegister(ins->base());
  const LAllocation value = useRegister(ins->value());

  // The output may not be used but will be clobbered regardless,
  // so ignore the case where we're not using the value and just
  // use the output register as a temp.

  LWasmAtomicExchangeHeap* lir =
      new (alloc()) LWasmAtomicExchangeHeap(base, value);
  define(lir, ins);
}

void LIRGenerator::visitWasmAtomicBinopHeap(MWasmAtomicBinopHeap* ins) {
  MDefinition* base = ins->base();
  MOZ_ASSERT(base->type() == MIRType::Int32);

  // No support for 64-bit operations with constants at the masm level.

  bool canTakeConstant = ins->access().type() != Scalar::Int64;

  // Case 1: the result of the operation is not used.
  //
  // We'll emit a single instruction: LOCK ADD, LOCK SUB, LOCK AND,
  // LOCK OR, or LOCK XOR.

  if (!ins->hasUses()) {
    LAllocation value = canTakeConstant ? useRegisterOrConstant(ins->value())
                                        : useRegister(ins->value());
    LWasmAtomicBinopHeapForEffect* lir =
        new (alloc()) LWasmAtomicBinopHeapForEffect(useRegister(base), value);
    add(lir, ins);
    return;
  }

  // Case 2: the result of the operation is used.
  //
  // For ADD and SUB we'll use XADD with word and byte ops as
  // appropriate.  Any output register can be used and if value is a
  // register it's best if it's the same as output:
  //
  //    movl       value, output  ; if value != output
  //    lock xaddl output, mem
  //
  // For AND/OR/XOR we need to use a CMPXCHG loop, and the output is
  // always in rax:
  //
  //    movl          *mem, rax
  // L: mov           rax, temp
  //    andl          value, temp
  //    lock cmpxchg  temp, mem  ; reads rax also
  //    jnz           L
  //    ; result in rax
  //
  // Note the placement of L, cmpxchg will update rax with *mem if
  // *mem does not have the expected value, so reloading it at the
  // top of the loop would be redundant.

  bool bitOp = !(ins->operation() == AtomicFetchAddOp ||
                 ins->operation() == AtomicFetchSubOp);
  bool reuseInput = false;
  LAllocation value;

  if (bitOp || ins->value()->isConstant()) {
    value = canTakeConstant ? useRegisterOrConstant(ins->value())
                            : useRegister(ins->value());
  } else {
    reuseInput = true;
    value = useRegisterAtStart(ins->value());
  }

  auto* lir = new (alloc()) LWasmAtomicBinopHeap(
      useRegister(base), value, bitOp ? temp() : LDefinition::BogusTemp());

  if (reuseInput) {
    defineReuseInput(lir, ins, LWasmAtomicBinopHeap::valueOp);
  } else if (bitOp) {
    defineFixed(lir, ins, LAllocation(AnyRegister(rax)));
  } else {
    define(lir, ins);
  }
}

void LIRGenerator::visitSubstr(MSubstr* ins) {
  LSubstr* lir = new (alloc())
      LSubstr(useRegister(ins->string()), useRegister(ins->begin()),
              useRegister(ins->length()), temp(), temp(), tempByteOpRegister());
  define(lir, ins);
  assignSafepoint(lir, ins);
}

void LIRGenerator::visitRandom(MRandom* ins) {
  LRandom* lir = new (alloc()) LRandom(temp(), temp(), temp());
  defineFixed(lir, ins, LFloatReg(ReturnDoubleReg));
}

void LIRGeneratorX64::lowerDivI64(MDiv* div) {
  if (div->isUnsigned()) {
    lowerUDivI64(div);
    return;
  }

  LDivOrModI64* lir = new (alloc()) LDivOrModI64(
      useRegister(div->lhs()), useRegister(div->rhs()), tempFixed(rdx));
  defineInt64Fixed(lir, div, LInt64Allocation(LAllocation(AnyRegister(rax))));
}

void LIRGeneratorX64::lowerModI64(MMod* mod) {
  if (mod->isUnsigned()) {
    lowerUModI64(mod);
    return;
  }

  LDivOrModI64* lir = new (alloc()) LDivOrModI64(
      useRegister(mod->lhs()), useRegister(mod->rhs()), tempFixed(rax));
  defineInt64Fixed(lir, mod, LInt64Allocation(LAllocation(AnyRegister(rdx))));
}

void LIRGeneratorX64::lowerUDivI64(MDiv* div) {
  LUDivOrModI64* lir = new (alloc()) LUDivOrModI64(
      useRegister(div->lhs()), useRegister(div->rhs()), tempFixed(rdx));
  defineInt64Fixed(lir, div, LInt64Allocation(LAllocation(AnyRegister(rax))));
}

void LIRGeneratorX64::lowerUModI64(MMod* mod) {
  LUDivOrModI64* lir = new (alloc()) LUDivOrModI64(
      useRegister(mod->lhs()), useRegister(mod->rhs()), tempFixed(rax));
  defineInt64Fixed(lir, mod, LInt64Allocation(LAllocation(AnyRegister(rdx))));
}

void LIRGenerator::visitWasmTruncateToInt64(MWasmTruncateToInt64* ins) {
  MDefinition* opd = ins->input();
  MOZ_ASSERT(opd->type() == MIRType::Double || opd->type() == MIRType::Float32);

  LDefinition maybeTemp =
      ins->isUnsigned() ? tempDouble() : LDefinition::BogusTemp();
  defineInt64(new (alloc()) LWasmTruncateToInt64(useRegister(opd), maybeTemp),
              ins);
}

void LIRGenerator::visitInt64ToFloatingPoint(MInt64ToFloatingPoint* ins) {
  MDefinition* opd = ins->input();
  MOZ_ASSERT(opd->type() == MIRType::Int64);
  MOZ_ASSERT(IsFloatingPointType(ins->type()));

  LDefinition maybeTemp = ins->isUnsigned() ? temp() : LDefinition::BogusTemp();
  define(new (alloc()) LInt64ToFloatingPoint(useInt64Register(opd), maybeTemp),
         ins);
}

void LIRGenerator::visitExtendInt32ToInt64(MExtendInt32ToInt64* ins) {
  defineInt64(new (alloc()) LExtendInt32ToInt64(useAtStart(ins->input())), ins);
}

void LIRGenerator::visitSignExtendInt64(MSignExtendInt64* ins) {
  defineInt64(new (alloc())
                  LSignExtendInt64(useInt64RegisterAtStart(ins->input())),
              ins);
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

  auto* lir = new (alloc()) LWasmBitselectSimd128(
      useRegisterAtStart(ins->lhs()), useRegister(ins->rhs()),
      useRegister(ins->control()), tempSimd128());
  defineReuseInput(lir, ins, LWasmBitselectSimd128::LhsDest);
}

void LIRGenerator::visitWasmBinarySimd128(MWasmBinarySimd128* ins) {
  MDefinition* lhs = ins->lhs();
  MDefinition* rhs = ins->rhs();

  MOZ_ASSERT(lhs->type() == MIRType::Simd128);
  MOZ_ASSERT(rhs->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  if (ins->isCommutative()) {
    ReorderCommutative(&lhs, &rhs, ins);
  }

  LDefinition tempReg0 = LDefinition::BogusTemp();
  LDefinition tempReg1 = LDefinition::BogusTemp();
  switch (ins->simdOp()) {
    case wasm::SimdOp::V128AndNot: {
      // x86/x64 specific: Code generation requires the operands to be reversed.
      MDefinition* tmp = lhs;
      lhs = rhs;
      rhs = tmp;
      break;
    }
    case wasm::SimdOp::F32x4Max:
    case wasm::SimdOp::F64x2Max:
    case wasm::SimdOp::V8x16Swizzle:
      tempReg0 = tempSimd128();
      break;
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

  LAllocation lhsDestAlloc = useRegisterAtStart(lhs);
  LAllocation rhsAlloc =
      lhs != rhs ? useRegister(rhs) : useRegisterAtStart(rhs);
  if (ins->simdOp() == wasm::SimdOp::I64x2Mul) {
    auto* lir =
        new (alloc()) LWasmI64x2Mul(lhsDestAlloc, rhsAlloc, tempInt64());
    defineReuseInput(lir, ins, LWasmI64x2Mul::LhsDest);
  } else {
    auto* lir = new (alloc())
        LWasmBinarySimd128(lhsDestAlloc, rhsAlloc, tempReg0, tempReg1);
    defineReuseInput(lir, ins, LWasmBinarySimd128::LhsDest);
  }
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
    case wasm::SimdOp::I64x2ShrS:
      break;
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

  LAllocation lhsDestAlloc = useRegisterAtStart(lhs);
  LAllocation rhsAlloc = ins->simdOp() == wasm::SimdOp::I64x2ShrS
                             ? useFixed(rhs, ecx)
                             : useRegister(rhs);
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
    if (rhsConstant.isIntegerZero()) {
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
      if (s.opd == Shuffle::Operand::LEFT) {
        if (*s.permuteOp == LWasmPermuteSimd128::MOVE) {
          src = useRegisterAtStart(ins->lhs());
        } else {
          src = useRegister(ins->lhs());
        }
      } else {
        if (*s.permuteOp == LWasmPermuteSimd128::MOVE) {
          src = useRegisterAtStart(ins->rhs());
        } else {
          src = useRegister(ins->rhs());
        }
      }
      auto* lir =
          new (alloc()) LWasmPermuteSimd128(src, *s.permuteOp, s.control);
      if (*s.permuteOp == LWasmPermuteSimd128::MOVE) {
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
        case LWasmShuffleSimd128::SHUFFLE_BLEND_8x16:
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

// For unary operations we currently avoid using useRegisterAtStart() and
// reusing the input for the output, as that frequently leads to longer code
// sequences as we end up using scratch to hold an intermediate result.

void LIRGenerator::visitWasmScalarToSimd128(MWasmScalarToSimd128* ins) {
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  if (ins->input()->type() == MIRType::Int64) {
    auto* lir =
        new (alloc()) LWasmInt64ToSimd128(useInt64Register(ins->input()));
    define(lir, ins);
  } else {
    auto* lir = new (alloc()) LWasmScalarToSimd128(useRegister(ins->input()));
    define(lir, ins);
  }
}

void LIRGenerator::visitWasmUnarySimd128(MWasmUnarySimd128* ins) {
  MOZ_ASSERT(ins->input()->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  LDefinition tempReg = LDefinition::BogusTemp();
  switch (ins->simdOp()) {
    case wasm::SimdOp::I32x4TruncUSatF32x4:
      tempReg = tempSimd128();
      break;
    default:
      break;
  }

  LWasmUnarySimd128* lir =
      new (alloc()) LWasmUnarySimd128(useRegister(ins->input()), tempReg);
  define(lir, ins);
}

void LIRGenerator::visitWasmReduceSimd128(MWasmReduceSimd128* ins) {
  if (ins->type() == MIRType::Int64) {
    auto* lir =
        new (alloc()) LWasmReduceSimd128ToInt64(useRegister(ins->input()));
    defineInt64(lir, ins);
  } else {
    auto* lir = new (alloc()) LWasmReduceSimd128(useRegister(ins->input()));
    define(lir, ins);
  }
}

#endif  // ENABLE_WASM_SIMD
