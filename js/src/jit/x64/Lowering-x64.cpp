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

void LIRGenerator::visitWasmShuffleSimd128(MWasmShuffleSimd128* ins) {
  MOZ_ASSERT(ins->lhs()->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->rhs()->type() == MIRType::Simd128);
  MOZ_ASSERT(ins->type() == MIRType::Simd128);

  auto* lir = new (alloc()) LWasmShuffleSimd128(
      useRegisterAtStart(ins->lhs()), useRegister(ins->rhs()), tempSimd128());
  defineReuseInput(lir, ins, LWasmShuffleSimd128::LhsDest);
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
