/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/WarpCacheIRTranspiler.h"

#include "jit/CacheIR.h"
#include "jit/CacheIRCompiler.h"
#include "jit/CacheIROpsGenerated.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/WarpSnapshot.h"

using namespace js;
using namespace js::jit;

// The CacheIR transpiler generates MIR from Baseline CacheIR.
class MOZ_RAII WarpCacheIRTranspiler {
  TempAllocator& alloc_;
  BytecodeLocation loc_;
  const CacheIRStubInfo* stubInfo_;
  const uint8_t* stubData_;

  // Vector mapping OperandId to corresponding MDefinition.
  MDefinitionStackVector operands_;

  MBasicBlock* current_;

#ifdef DEBUG
  // Used to assert that there is only one effectful instruction
  // per stub. And that this instruction has a resume point.
  MInstruction* effectful_ = nullptr;
  bool pushedResult_ = false;
#endif

  TempAllocator& alloc() { return alloc_; }

  inline void add(MInstruction* ins) {
    MOZ_ASSERT(!ins->isEffectful());
    current_->add(ins);
  }

  inline void addEffectful(MInstruction* ins) {
    MOZ_ASSERT(ins->isEffectful());
    MOZ_ASSERT(!effectful_, "Can only have one effectful instruction");
    current_->add(ins);
#ifdef DEBUG
    effectful_ = ins;
#endif
  }

  MOZ_MUST_USE bool resumeAfter(MInstruction* ins);

  // CacheIR instructions writing to the IC's result register (the *Result
  // instructions) must call this to push the result onto the virtual stack.
  void pushResult(MDefinition* result) {
    MOZ_ASSERT(!pushedResult_, "Can't have more than one result");
    current_->push(result);
#ifdef DEBUG
    pushedResult_ = true;
#endif
  }

  MDefinition* getOperand(OperandId id) const { return operands_[id.id()]; }

  void setOperand(OperandId id, MDefinition* def) { operands_[id.id()] = def; }

  MOZ_MUST_USE bool defineOperand(OperandId id, MDefinition* def) {
    MOZ_ASSERT(id.id() == operands_.length());
    return operands_.append(def);
  }

  uintptr_t readStubWord(uint32_t offset) {
    return stubInfo_->getStubRawWord(stubData_, offset);
  }

  Shape* shapeStubField(uint32_t offset) {
    return reinterpret_cast<Shape*>(readStubWord(offset));
  }
  JSString* stringStubField(uint32_t offset) {
    return reinterpret_cast<JSString*>(readStubWord(offset));
  }
  JSObject* objectStubField(uint32_t offset) {
    return reinterpret_cast<JSObject*>(readStubWord(offset));
  }
  int32_t int32StubField(uint32_t offset) {
    return static_cast<int32_t>(readStubWord(offset));
  }

  MOZ_MUST_USE bool emitGuardTo(ValOperandId inputId, MIRType type);

  template <typename T>
  MOZ_MUST_USE bool emitDoubleBinaryArithResult(NumberOperandId lhsId,
                                                NumberOperandId rhsId);

  template <typename T>
  MOZ_MUST_USE bool emitInt32BinaryArithResult(Int32OperandId lhsId,
                                               Int32OperandId rhsId);

  MOZ_MUST_USE bool emitCompareResult(JSOp op, OperandId lhsId, OperandId rhsId,
                                      MCompare::CompareType compareType);

  MInstruction* addBoundsCheck(MDefinition* index, MDefinition* length);

  CACHE_IR_TRANSPILER_GENERATED

 public:
  WarpCacheIRTranspiler(MIRGenerator& mirGen, BytecodeLocation loc,
                        MBasicBlock* current, const WarpCacheIR* snapshot)
      : alloc_(mirGen.alloc()),
        loc_(loc),
        stubInfo_(snapshot->stubInfo()),
        stubData_(snapshot->stubData()),
        current_(current) {}

  MOZ_MUST_USE bool transpile(const MDefinitionStackVector& inputs);
};

bool WarpCacheIRTranspiler::transpile(const MDefinitionStackVector& inputs) {
  if (!operands_.appendAll(inputs)) {
    return false;
  }

  CacheIRReader reader(stubInfo_);
  do {
    CacheOp op = reader.readOp();
    switch (op) {
#define DEFINE_OP(op, ...)   \
  case CacheOp::op:          \
    if (!emit##op(reader)) { \
      return false;          \
    }                        \
    break;
      CACHE_IR_TRANSPILER_OPS(DEFINE_OP)
#undef DEFINE_OP

      default:
        fprintf(stderr, "Unsupported op: %s\n", CacheIROpNames[size_t(op)]);
        MOZ_CRASH("Unsupported op");
    }
  } while (reader.more());

  // Effectful instructions should have a resume point.
  MOZ_ASSERT_IF(effectful_, effectful_->resumePoint());
  return true;
}

bool WarpCacheIRTranspiler::resumeAfter(MInstruction* ins) {
  MOZ_ASSERT(ins->isEffectful());
  MOZ_ASSERT(effectful_ == ins);

  MResumePoint* resumePoint = MResumePoint::New(
      alloc(), ins->block(), loc_.toRawBytecode(), MResumePoint::ResumeAfter);
  if (!resumePoint) {
    return false;
  }

  ins->setResumePoint(resumePoint);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardClass(ObjOperandId objId,
                                           GuardClassKind kind) {
  MDefinition* def = getOperand(objId);

  const JSClass* classp = nullptr;
  switch (kind) {
    case GuardClassKind::Array:
      classp = &ArrayObject::class_;
      break;
    default:
      MOZ_CRASH("not yet supported");
  }

  auto* ins = MGuardToClass::New(alloc(), def, classp);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardShape(ObjOperandId objId,
                                           uint32_t shapeOffset) {
  MDefinition* def = getOperand(objId);
  Shape* shape = shapeStubField(shapeOffset);

  auto* ins = MGuardShape::New(alloc(), def, shape, Bailout_ShapeGuard);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardSpecificAtom(StringOperandId strId,
                                                  uint32_t expectedOffset) {
  MDefinition* str = getOperand(strId);
  JSString* expected = stringStubField(expectedOffset);

  // TODO: Improved code-gen and folding.
  auto* ins = MGuardSpecificAtom::New(alloc(), str, &expected->asAtom());
  add(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardType(ValOperandId inputId,
                                          ValueType type) {
  switch (type) {
    case ValueType::String:
    case ValueType::Symbol:
    case ValueType::BigInt:
    case ValueType::Int32:
    case ValueType::Double:
    case ValueType::Boolean:
      return emitGuardTo(inputId, MIRTypeFromValueType(JSValueType(type)));
    case ValueType::Undefined:
      return emitGuardIsUndefined(inputId);
    case ValueType::Null:
      return emitGuardIsNull(inputId);
    case ValueType::Magic:
    case ValueType::PrivateGCThing:
    case ValueType::Object:
      break;
  }

  MOZ_CRASH("unexpected type");
}

bool WarpCacheIRTranspiler::emitGuardTo(ValOperandId inputId, MIRType type) {
  MDefinition* def = getOperand(inputId);
  if (def->type() == type) {
    return true;
  }

  auto* ins = MUnbox::New(alloc(), def, type, MUnbox::Fallible);
  add(ins);

  setOperand(inputId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardToObject(ValOperandId inputId) {
  return emitGuardTo(inputId, MIRType::Object);
}

bool WarpCacheIRTranspiler::emitGuardToString(ValOperandId inputId) {
  return emitGuardTo(inputId, MIRType::String);
}

bool WarpCacheIRTranspiler::emitGuardToBoolean(ValOperandId inputId,
                                               Int32OperandId resultId) {
  MDefinition* input = getOperand(inputId);

  MDefinition* boolean;
  if (input->type() == MIRType::Boolean) {
    boolean = input;
  } else {
    auto* unbox =
        MUnbox::New(alloc(), input, MIRType::Boolean, MUnbox::Fallible);
    add(unbox);
    boolean = unbox;
  }

  // This is actually a no-op, but still required to get the correct type.
  auto* ins = MToIntegerInt32::New(alloc(), boolean);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitGuardToInt32(ValOperandId inputId,
                                             Int32OperandId resultId) {
  MDefinition* input = getOperand(inputId);
  if (input->type() == MIRType::Int32) {
    return defineOperand(resultId, input);
  }

  auto* ins = MUnbox::New(alloc(), input, MIRType::Int32, MUnbox::Fallible);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitGuardIsNumber(ValOperandId inputId) {
  // MIRType::Double also implies int32 in Ion.
  return emitGuardTo(inputId, MIRType::Double);
}

bool WarpCacheIRTranspiler::emitGuardIsNullOrUndefined(ValOperandId inputId) {
  MDefinition* input = getOperand(inputId);
  if (input->type() == MIRType::Null || input->type() == MIRType::Undefined) {
    return true;
  }

  auto* ins = MGuardNullOrUndefined::New(alloc(), input);
  add(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardIsNull(ValOperandId inputId) {
  MDefinition* input = getOperand(inputId);
  if (input->type() == MIRType::Null) {
    return true;
  }

  auto* ins = MGuardValue::New(alloc(), input, NullValue());
  add(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardIsUndefined(ValOperandId inputId) {
  MDefinition* input = getOperand(inputId);
  if (input->type() == MIRType::Undefined) {
    return true;
  }

  auto* ins = MGuardValue::New(alloc(), input, UndefinedValue());
  add(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardToInt32Index(ValOperandId inputId,
                                                  Int32OperandId resultId) {
  MDefinition* input = getOperand(inputId);
  auto* ins =
      MToNumberInt32::New(alloc(), input, IntConversionInputKind::NumbersOnly);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitGuardToTypedArrayIndex(
    ValOperandId inputId, Int32OperandId resultId) {
  MDefinition* input = getOperand(inputId);
  auto* ins = MTypedArrayIndexToInt32::New(alloc(), input);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitTruncateDoubleToUInt32(
    ValOperandId valId, Int32OperandId resultId) {
  MDefinition* input = getOperand(valId);
  auto* ins = MTruncateToInt32::New(alloc(), input);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitLoadInt32Result(Int32OperandId valId) {
  MDefinition* val = getOperand(valId);
  MOZ_ASSERT(val->type() == MIRType::Int32);
  pushResult(val);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadObjectResult(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);
  MOZ_ASSERT(obj->type() == MIRType::Object);
  pushResult(obj);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadBooleanResult(bool val) {
  auto* constant = MConstant::New(alloc(), BooleanValue(val));
  add(constant);
  pushResult(constant);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadEnclosingEnvironment(
    ObjOperandId objId, ObjOperandId resultId) {
  MDefinition* env = getOperand(objId);
  auto* ins = MEnclosingEnvironment::New(alloc(), env);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitLoadObject(ObjOperandId resultId,
                                           uint32_t objOffset) {
  JSObject* obj = objectStubField(objOffset);

  auto* ins = MConstant::NewConstraintlessObject(alloc(), obj);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitLoadDynamicSlotResult(ObjOperandId objId,
                                                      uint32_t offsetOffset) {
  int32_t offset = int32StubField(offsetOffset);

  MDefinition* obj = getOperand(objId);
  size_t slotIndex = NativeObject::getDynamicSlotIndexFromOffset(offset);

  auto* slots = MSlots::New(alloc(), obj);
  add(slots);

  auto* load = MLoadSlot::New(alloc(), slots, slotIndex);
  add(load);

  pushResult(load);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadFixedSlotResult(ObjOperandId objId,
                                                    uint32_t offsetOffset) {
  int32_t offset = int32StubField(offsetOffset);

  MDefinition* obj = getOperand(objId);
  uint32_t slotIndex = NativeObject::getFixedSlotIndexFromOffset(offset);

  auto* load = MLoadFixedSlot::New(alloc(), obj, slotIndex);
  add(load);

  pushResult(load);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadEnvironmentFixedSlotResult(
    ObjOperandId objId, uint32_t offsetOffset) {
  int32_t offset = int32StubField(offsetOffset);

  MDefinition* obj = getOperand(objId);
  uint32_t slotIndex = NativeObject::getFixedSlotIndexFromOffset(offset);

  auto* load = MLoadFixedSlot::New(alloc(), obj, slotIndex);
  add(load);

  auto* lexicalCheck = MLexicalCheck::New(alloc(), load);
  add(lexicalCheck);

  pushResult(lexicalCheck);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadEnvironmentDynamicSlotResult(
    ObjOperandId objId, uint32_t offsetOffset) {
  int32_t offset = int32StubField(offsetOffset);

  MDefinition* obj = getOperand(objId);
  size_t slotIndex = NativeObject::getDynamicSlotIndexFromOffset(offset);

  auto* slots = MSlots::New(alloc(), obj);
  add(slots);

  auto* load = MLoadSlot::New(alloc(), slots, slotIndex);
  add(load);

  auto* lexicalCheck = MLexicalCheck::New(alloc(), load);
  add(lexicalCheck);

  pushResult(lexicalCheck);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadInt32ArrayLengthResult(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* elements = MElements::New(alloc(), obj);
  add(elements);

  auto* length = MArrayLength::New(alloc(), elements);
  add(length);

  pushResult(length);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadTypedArrayLengthResult(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* length = MTypedArrayLength::New(alloc(), obj);
  add(length);

  pushResult(length);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadStringLengthResult(StringOperandId strId) {
  MDefinition* str = getOperand(strId);

  auto* length = MStringLength::New(alloc(), str);
  add(length);

  pushResult(length);
  return true;
}

MInstruction* WarpCacheIRTranspiler::addBoundsCheck(MDefinition* index,
                                                    MDefinition* length) {
  MInstruction* check = MBoundsCheck::New(alloc(), index, length);
  add(check);

  if (JitOptions.spectreIndexMasking) {
    // Use a separate MIR instruction for the index masking. Doing this as
    // part of MBoundsCheck would be unsound because bounds checks can be
    // optimized or eliminated completely. Consider this:
    //
    //   for (var i = 0; i < x; i++)
    //        res = arr[i];
    //
    // If we can prove |x < arr.length|, we are able to eliminate the bounds
    // check, but we should not get rid of the index masking because the
    // |i < x| branch could still be mispredicted.
    //
    // Using a separate instruction lets us eliminate the bounds check
    // without affecting the index masking.
    check = MSpectreMaskIndex::New(alloc(), check, length);
    add(check);
  }

  return check;
}

bool WarpCacheIRTranspiler::emitLoadDenseElementResult(ObjOperandId objId,
                                                       Int32OperandId indexId) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);

  auto* elements = MElements::New(alloc(), obj);
  add(elements);

  auto* length = MInitializedLength::New(alloc(), elements);
  add(length);

  index = addBoundsCheck(index, length);

  bool needsHoleCheck = true;
  bool loadDouble = false;  // TODO: Ion-only optimization.
  auto* load =
      MLoadElement::New(alloc(), elements, index, needsHoleCheck, loadDouble);
  add(load);

  pushResult(load);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadTypedArrayElementResult(
    ObjOperandId objId, Int32OperandId indexId, Scalar::Type elementType,
    bool handleOOB) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);

  if (handleOOB) {
    bool allowDouble = true;
    auto* load = MLoadTypedArrayElementHole::New(alloc(), obj, index,
                                                 elementType, allowDouble);
    add(load);

    pushResult(load);
    return true;
  }

  auto* length = MTypedArrayLength::New(alloc(), obj);
  add(length);

  index = addBoundsCheck(index, length);

  auto* elements = MTypedArrayElements::New(alloc(), obj);
  add(elements);

  auto* load = MLoadUnboxedScalar::New(alloc(), elements, index, elementType);
  // TODO: Uint32 always loaded as double.
  load->setResultType(MIRTypeForTypedArrayRead(elementType, true));
  add(load);

  pushResult(load);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadStringCharResult(StringOperandId strId,
                                                     Int32OperandId indexId) {
  MDefinition* str = getOperand(strId);
  MDefinition* index = getOperand(indexId);

  auto* length = MStringLength::New(alloc(), str);
  add(length);

  index = addBoundsCheck(index, length);

  auto* charCode = MCharCodeAt::New(alloc(), str, index);
  add(charCode);

  auto* fromCharCode = MFromCharCode::New(alloc(), charCode);
  add(fromCharCode);

  pushResult(fromCharCode);
  return true;
}

bool WarpCacheIRTranspiler::emitStoreDynamicSlot(ObjOperandId objId,
                                                 uint32_t offsetOffset,
                                                 ValOperandId rhsId) {
  int32_t offset = int32StubField(offsetOffset);

  MDefinition* obj = getOperand(objId);
  size_t slotIndex = NativeObject::getDynamicSlotIndexFromOffset(offset);
  MDefinition* rhs = getOperand(rhsId);

  auto* barrier = MPostWriteBarrier::New(alloc(), obj, rhs);
  add(barrier);

  auto* slots = MSlots::New(alloc(), obj);
  add(slots);

  auto* store = MStoreSlot::NewBarriered(alloc(), slots, slotIndex, rhs);
  addEffectful(store);
  return resumeAfter(store);
}

bool WarpCacheIRTranspiler::emitStoreFixedSlot(ObjOperandId objId,
                                               uint32_t offsetOffset,
                                               ValOperandId rhsId) {
  int32_t offset = int32StubField(offsetOffset);

  MDefinition* obj = getOperand(objId);
  size_t slotIndex = NativeObject::getFixedSlotIndexFromOffset(offset);
  MDefinition* rhs = getOperand(rhsId);

  auto* barrier = MPostWriteBarrier::New(alloc(), obj, rhs);
  add(barrier);

  auto* store = MStoreFixedSlot::NewBarriered(alloc(), obj, slotIndex, rhs);
  addEffectful(store);
  return resumeAfter(store);
}

bool WarpCacheIRTranspiler::emitStoreDenseElement(ObjOperandId objId,
                                                  Int32OperandId indexId,
                                                  ValOperandId rhsId) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);
  MDefinition* rhs = getOperand(rhsId);

  auto* elements = MElements::New(alloc(), obj);
  add(elements);

  auto* length = MInitializedLength::New(alloc(), elements);
  add(length);

  index = addBoundsCheck(index, length);

  auto* barrier = MPostWriteBarrier::New(alloc(), obj, rhs);
  add(barrier);

  bool needsHoleCheck = true;
  auto* store =
      MStoreElement::New(alloc(), elements, index, rhs, needsHoleCheck);
  store->setNeedsBarrier();
  addEffectful(store);
  return resumeAfter(store);
}

bool WarpCacheIRTranspiler::emitInt32IncResult(Int32OperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* constOne = MConstant::New(alloc(), Int32Value(1));
  add(constOne);

  auto* ins = MAdd::New(alloc(), input, constOne, MIRType::Int32);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitInt32DecResult(Int32OperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* constOne = MConstant::New(alloc(), Int32Value(1));
  add(constOne);

  auto* ins = MSub::New(alloc(), input, constOne, MIRType::Int32);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitInt32NegationResult(Int32OperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* constNegOne = MConstant::New(alloc(), Int32Value(-1));
  add(constNegOne);

  auto* ins = MMul::New(alloc(), input, constNegOne, MIRType::Int32);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitInt32NotResult(Int32OperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MBitNot::New(alloc(), input);
  add(ins);

  pushResult(ins);
  return true;
}

template <typename T>
bool WarpCacheIRTranspiler::emitDoubleBinaryArithResult(NumberOperandId lhsId,
                                                        NumberOperandId rhsId) {
  MDefinition* lhs = getOperand(lhsId);
  MDefinition* rhs = getOperand(rhsId);

  auto* ins = T::New(alloc(), lhs, rhs, MIRType::Double);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitDoubleAddResult(NumberOperandId lhsId,
                                                NumberOperandId rhsId) {
  return emitDoubleBinaryArithResult<MAdd>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitDoubleSubResult(NumberOperandId lhsId,
                                                NumberOperandId rhsId) {
  return emitDoubleBinaryArithResult<MSub>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitDoubleMulResult(NumberOperandId lhsId,
                                                NumberOperandId rhsId) {
  return emitDoubleBinaryArithResult<MMul>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitDoubleDivResult(NumberOperandId lhsId,
                                                NumberOperandId rhsId) {
  return emitDoubleBinaryArithResult<MDiv>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitDoubleModResult(NumberOperandId lhsId,
                                                NumberOperandId rhsId) {
  return emitDoubleBinaryArithResult<MMod>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitDoublePowResult(NumberOperandId lhsId,
                                                NumberOperandId rhsId) {
  return emitDoubleBinaryArithResult<MPow>(lhsId, rhsId);
}

template <typename T>
bool WarpCacheIRTranspiler::emitInt32BinaryArithResult(Int32OperandId lhsId,
                                                       Int32OperandId rhsId) {
  MDefinition* lhs = getOperand(lhsId);
  MDefinition* rhs = getOperand(rhsId);

  auto* ins = T::New(alloc(), lhs, rhs, MIRType::Int32);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitInt32AddResult(Int32OperandId lhsId,
                                               Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MAdd>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitInt32SubResult(Int32OperandId lhsId,
                                               Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MSub>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitInt32MulResult(Int32OperandId lhsId,
                                               Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MMul>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitInt32DivResult(Int32OperandId lhsId,
                                               Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MDiv>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitInt32ModResult(Int32OperandId lhsId,
                                               Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MMod>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitInt32BitOrResult(Int32OperandId lhsId,
                                                 Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MBitOr>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitInt32BitXorResult(Int32OperandId lhsId,
                                                  Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MBitXor>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitInt32BitAndResult(Int32OperandId lhsId,
                                                  Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MBitAnd>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitInt32LeftShiftResult(Int32OperandId lhsId,
                                                     Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MLsh>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitInt32RightShiftResult(Int32OperandId lhsId,
                                                      Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MRsh>(lhsId, rhsId);
}

bool WarpCacheIRTranspiler::emitCallStringConcatResult(StringOperandId lhsId,
                                                       StringOperandId rhsId) {
  MDefinition* lhs = getOperand(lhsId);
  MDefinition* rhs = getOperand(rhsId);

  auto* ins = MConcat::New(alloc(), lhs, rhs);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitCompareResult(
    JSOp op, OperandId lhsId, OperandId rhsId,
    MCompare::CompareType compareType) {
  MDefinition* lhs = getOperand(lhsId);
  MDefinition* rhs = getOperand(rhsId);

  auto* ins = MCompare::New(alloc(), lhs, rhs, op);
  ins->setCompareType(compareType);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitCompareInt32Result(JSOp op,
                                                   Int32OperandId lhsId,
                                                   Int32OperandId rhsId) {
  return emitCompareResult(op, lhsId, rhsId, MCompare::Compare_Int32);
}

bool WarpCacheIRTranspiler::emitCompareDoubleResult(JSOp op,
                                                    NumberOperandId lhsId,
                                                    NumberOperandId rhsId) {
  return emitCompareResult(op, lhsId, rhsId, MCompare::Compare_Double);
}

bool WarpCacheIRTranspiler::emitCompareObjectResult(JSOp op, ObjOperandId lhsId,
                                                    ObjOperandId rhsId) {
  MOZ_ASSERT(IsEqualityOp(op));
  return emitCompareResult(op, lhsId, rhsId, MCompare::Compare_Object);
}

bool WarpCacheIRTranspiler::emitCompareStringResult(JSOp op,
                                                    StringOperandId lhsId,
                                                    StringOperandId rhsId) {
  return emitCompareResult(op, lhsId, rhsId, MCompare::Compare_String);
}

bool WarpCacheIRTranspiler::emitTypeMonitorResult() {
  MOZ_ASSERT(pushedResult_, "Didn't push result MDefinition");
  return true;
}

bool WarpCacheIRTranspiler::emitReturnFromIC() { return true; }

bool jit::TranspileCacheIRToMIR(MIRGenerator& mirGen, BytecodeLocation loc,
                                MBasicBlock* current,
                                const WarpCacheIR* snapshot,
                                const MDefinitionStackVector& inputs) {
  WarpCacheIRTranspiler transpiler(mirGen, loc, current, snapshot);
  return transpiler.transpile(inputs);
}
