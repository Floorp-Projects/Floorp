/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/WarpCacheIRTranspiler.h"

#include "jsmath.h"

#include "builtin/DataViewObject.h"
#include "jit/AtomicOp.h"
#include "jit/CacheIR.h"
#include "jit/CacheIRCompiler.h"
#include "jit/CacheIROpsGenerated.h"
#include "jit/MIR.h"
#include "jit/MIRBuilderShared.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/WarpBuilder.h"
#include "jit/WarpBuilderShared.h"
#include "jit/WarpSnapshot.h"
#include "js/ScalarType.h"  // js::Scalar::Type

#include "gc/ObjectKind-inl.h"

using namespace js;
using namespace js::jit;

// The CacheIR transpiler generates MIR from Baseline CacheIR.
class MOZ_RAII WarpCacheIRTranspiler : public WarpBuilderShared {
  WarpBuilder* builder_;
  BytecodeLocation loc_;
  const CacheIRStubInfo* stubInfo_;
  const uint8_t* stubData_;

  // Vector mapping OperandId to corresponding MDefinition.
  MDefinitionStackVector operands_;

  CallInfo* callInfo_;

#ifdef DEBUG
  // Used to assert that there is only one effectful instruction
  // per stub. And that this instruction has a resume point.
  MInstruction* effectful_ = nullptr;
  bool pushedResult_ = false;
#endif

  inline void add(MInstruction* ins) {
    MOZ_ASSERT(!ins->isEffectful());
    current->add(ins);
  }

  inline void addEffectful(MInstruction* ins) {
    MOZ_ASSERT(ins->isEffectful());
    MOZ_ASSERT(!effectful_, "Can only have one effectful instruction");
    current->add(ins);
#ifdef DEBUG
    effectful_ = ins;
#endif
  }

  // Bypasses all checks in addEffectful. Only used for testing functions.
  inline void addEffectfulUnsafe(MInstruction* ins) {
    MOZ_ASSERT(ins->isEffectful());
    current->add(ins);
  }

  MOZ_MUST_USE bool resumeAfter(MInstruction* ins) {
    MOZ_ASSERT(effectful_ == ins);
    return WarpBuilderShared::resumeAfter(ins, loc_);
  }

  // CacheIR instructions writing to the IC's result register (the *Result
  // instructions) must call this to push the result onto the virtual stack.
  void pushResult(MDefinition* result) {
    MOZ_ASSERT(!pushedResult_, "Can't have more than one result");
    current->push(result);
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
  const JSClass* classStubField(uint32_t offset) {
    return reinterpret_cast<const JSClass*>(readStubWord(offset));
  }
  JSString* stringStubField(uint32_t offset) {
    return reinterpret_cast<JSString*>(readStubWord(offset));
  }
  JS::Symbol* symbolStubField(uint32_t offset) {
    return reinterpret_cast<JS::Symbol*>(readStubWord(offset));
  }
  ObjectGroup* groupStubField(uint32_t offset) {
    return reinterpret_cast<ObjectGroup*>(readStubWord(offset));
  }
  const void* rawPointerField(uint32_t offset) {
    return reinterpret_cast<const void*>(readStubWord(offset));
  }
  jsid idStubField(uint32_t offset) {
    return jsid::fromRawBits(readStubWord(offset));
  }
  int32_t int32StubField(uint32_t offset) {
    return static_cast<int32_t>(readStubWord(offset));
  }
  uint32_t uint32StubField(uint32_t offset) {
    return static_cast<uint32_t>(readStubWord(offset));
  }

  // This must only be called when the caller knows the object is tenured and
  // not a nursery index.
  JSObject* tenuredObjectStubField(uint32_t offset) {
    WarpObjectField field = WarpObjectField::fromData(readStubWord(offset));
    return field.toObject();
  }

  // Returns either MConstant or MNurseryIndex. See WarpObjectField.
  MInstruction* objectStubField(uint32_t offset);

  MOZ_MUST_USE bool emitGuardTo(ValOperandId inputId, MIRType type);

  MOZ_MUST_USE bool emitToString(OperandId inputId, StringOperandId resultId);

  template <typename T>
  MOZ_MUST_USE bool emitDoubleBinaryArithResult(NumberOperandId lhsId,
                                                NumberOperandId rhsId);

  template <typename T>
  MOZ_MUST_USE bool emitInt32BinaryArithResult(Int32OperandId lhsId,
                                               Int32OperandId rhsId);

  MOZ_MUST_USE bool emitCompareResult(JSOp op, OperandId lhsId, OperandId rhsId,
                                      MCompare::CompareType compareType);

  MOZ_MUST_USE bool emitNewIteratorResult(MNewIterator::Type type,
                                          uint32_t templateObjectOffset);

  MInstruction* addBoundsCheck(MDefinition* index, MDefinition* length);

  bool emitAddAndStoreSlotShared(MAddAndStoreSlot::Kind kind,
                                 ObjOperandId objId, uint32_t offsetOffset,
                                 ValOperandId rhsId, uint32_t newShapeOffset);

  void addDataViewData(MDefinition* obj, Scalar::Type type,
                       MDefinition** offset, MInstruction** elements);

  MOZ_MUST_USE bool emitAtomicsBinaryOp(ObjOperandId objId,
                                        Int32OperandId indexId,
                                        Int32OperandId valueId,
                                        Scalar::Type elementType, AtomicOp op);

  MOZ_MUST_USE bool emitLoadArgumentSlot(ValOperandId resultId,
                                         uint32_t slotIndex);

  // Calls are either Native (native function without a JitEntry) or Scripted
  // (scripted function or native function with a JitEntry).
  enum class CallKind { Native, Scripted };

  MOZ_MUST_USE bool emitCallFunction(ObjOperandId calleeId,
                                     Int32OperandId argcId, CallFlags flags,
                                     CallKind kind);

  CACHE_IR_TRANSPILER_GENERATED

 public:
  WarpCacheIRTranspiler(WarpBuilder* builder, BytecodeLocation loc,
                        CallInfo* callInfo, const WarpCacheIR* cacheIRSnapshot)
      : WarpBuilderShared(builder->snapshot(), builder->mirGen(),
                          builder->currentBlock()),
        builder_(builder),
        loc_(loc),
        stubInfo_(cacheIRSnapshot->stubInfo()),
        stubData_(cacheIRSnapshot->stubData()),
        callInfo_(callInfo) {}

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

MInstruction* WarpCacheIRTranspiler::objectStubField(uint32_t offset) {
  WarpObjectField field = WarpObjectField::fromData(readStubWord(offset));

  if (field.isNurseryIndex()) {
    auto* ins = MNurseryObject::New(alloc(), field.toNurseryIndex());
    add(ins);
    return ins;
  }

  auto* ins = MConstant::NewConstraintlessObject(alloc(), field.toObject());
  add(ins);
  return ins;
}

bool WarpCacheIRTranspiler::emitGuardClass(ObjOperandId objId,
                                           GuardClassKind kind) {
  MDefinition* def = getOperand(objId);

  const JSClass* classp = nullptr;
  switch (kind) {
    case GuardClassKind::Array:
      classp = &ArrayObject::class_;
      break;
    case GuardClassKind::ArrayBuffer:
      classp = &ArrayBufferObject::class_;
      break;
    case GuardClassKind::SharedArrayBuffer:
      classp = &SharedArrayBufferObject::class_;
      break;
    case GuardClassKind::DataView:
      classp = &DataViewObject::class_;
      break;
    default:
      MOZ_CRASH("not yet supported");
  }

  auto* ins = MGuardToClass::New(alloc(), def, classp);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardAnyClass(ObjOperandId objId,
                                              uint32_t claspOffset) {
  MDefinition* def = getOperand(objId);
  const JSClass* classp = classStubField(claspOffset);

  auto* ins = MGuardToClass::New(alloc(), def, classp);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardShape(ObjOperandId objId,
                                           uint32_t shapeOffset) {
  MDefinition* def = getOperand(objId);
  Shape* shape = shapeStubField(shapeOffset);

  auto* ins = MGuardShape::New(alloc(), def, shape);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardGroup(ObjOperandId objId,
                                           uint32_t groupOffset) {
  MDefinition* def = getOperand(objId);
  ObjectGroup* group = groupStubField(groupOffset);

  auto* ins = MGuardObjectGroup::New(alloc(), def, group,
                                     /* bailOnEquality = */ false,
                                     BailoutKind::ObjectIdentityOrTypeGuard);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardNullProto(ObjOperandId objId) {
  MDefinition* def = getOperand(objId);

  auto* ins = MGuardNullProto::New(alloc(), def);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardIsProxy(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MGuardIsProxy::New(alloc(), obj);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardIsNotProxy(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MGuardIsNotProxy::New(alloc(), obj);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardIsNotDOMProxy(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MGuardIsNotDOMProxy::New(alloc(), obj);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitProxyGetResult(ObjOperandId objId,
                                               uint32_t idOffset) {
  MDefinition* obj = getOperand(objId);
  jsid id = idStubField(idOffset);

  auto* ins = MProxyGet::New(alloc(), obj, id);
  addEffectful(ins);

  pushResult(ins);
  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitProxyGetByValueResult(ObjOperandId objId,
                                                      ValOperandId idId) {
  MDefinition* obj = getOperand(objId);
  MDefinition* id = getOperand(idId);

  auto* ins = MProxyGetByValue::New(alloc(), obj, id);
  addEffectful(ins);

  pushResult(ins);
  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitProxyHasPropResult(ObjOperandId objId,
                                                   ValOperandId idId,
                                                   bool hasOwn) {
  MDefinition* obj = getOperand(objId);
  MDefinition* id = getOperand(idId);

  auto* ins = MProxyHasProp::New(alloc(), obj, id, hasOwn);
  addEffectful(ins);

  pushResult(ins);
  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitProxySet(ObjOperandId objId, uint32_t idOffset,
                                         ValOperandId rhsId, bool strict) {
  MDefinition* obj = getOperand(objId);
  jsid id = idStubField(idOffset);
  MDefinition* rhs = getOperand(rhsId);

  auto* ins = MProxySet::New(alloc(), obj, id, rhs, strict);
  addEffectful(ins);

  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitProxySetByValue(ObjOperandId objId,
                                                ValOperandId idId,
                                                ValOperandId rhsId,
                                                bool strict) {
  MDefinition* obj = getOperand(objId);
  MDefinition* id = getOperand(idId);
  MDefinition* rhs = getOperand(rhsId);

  auto* ins = MProxySetByValue::New(alloc(), obj, id, rhs, strict);
  addEffectful(ins);

  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitGuardIsNotArrayBufferMaybeShared(
    ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MGuardIsNotArrayBufferMaybeShared::New(alloc(), obj);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardProto(ObjOperandId objId,
                                           uint32_t protoOffset) {
  MDefinition* def = getOperand(objId);
  MDefinition* proto = objectStubField(protoOffset);

  auto* ins = MGuardProto::New(alloc(), def, proto);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardDynamicSlotIsSpecificObject(
    ObjOperandId objId, ObjOperandId expectedId, uint32_t slotOffset) {
  size_t slotIndex = int32StubField(slotOffset);
  MDefinition* obj = getOperand(objId);
  MDefinition* expected = getOperand(expectedId);

  auto* slots = MSlots::New(alloc(), obj);
  add(slots);

  auto* load = MLoadDynamicSlot::New(alloc(), slots, slotIndex);
  add(load);

  auto* unbox = MUnbox::New(alloc(), load, MIRType::Object, MUnbox::Fallible);
  add(unbox);

  auto* guard = MGuardObjectIdentity::New(alloc(), unbox, expected,
                                          /* bailOnEquality = */ false);
  add(guard);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardSpecificAtom(StringOperandId strId,
                                                  uint32_t expectedOffset) {
  MDefinition* str = getOperand(strId);
  JSString* expected = stringStubField(expectedOffset);

  auto* ins = MGuardSpecificAtom::New(alloc(), str, &expected->asAtom());
  add(ins);

  setOperand(strId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardSpecificSymbol(SymbolOperandId symId,
                                                    uint32_t expectedOffset) {
  MDefinition* symbol = getOperand(symId);
  JS::Symbol* expected = symbolStubField(expectedOffset);

  auto* ins = MGuardSpecificSymbol::New(alloc(), symbol, expected);
  add(ins);

  setOperand(symId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardSpecificObject(ObjOperandId objId,
                                                    uint32_t expectedOffset) {
  MDefinition* obj = getOperand(objId);
  MDefinition* expected = objectStubField(expectedOffset);

  auto* ins = MGuardObjectIdentity::New(alloc(), obj, expected,
                                        /* bailOnEquality = */ false);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardSpecificFunction(
    ObjOperandId objId, uint32_t expectedOffset, uint32_t nargsAndFlagsOffset) {
  MDefinition* obj = getOperand(objId);
  MDefinition* expected = objectStubField(expectedOffset);
  uint32_t nargsAndFlags = uint32StubField(nargsAndFlagsOffset);

  uint16_t nargs = nargsAndFlags >> 16;
  FunctionFlags flags = FunctionFlags(uint16_t(nargsAndFlags));

  auto* ins = MGuardSpecificFunction::New(alloc(), obj, expected, nargs, flags);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardNoDenseElements(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MGuardNoDenseElements::New(alloc(), obj);
  add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardMagicValue(ValOperandId valId,
                                                JSWhyMagic magic) {
  MDefinition* val = getOperand(valId);

  auto* ins = MGuardValue::New(alloc(), val, MagicValue(magic));
  add(ins);

  setOperand(valId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardFrameHasNoArgumentsObject() {
  // WarpOracle ensures this op isn't transpiled in functions that need an
  // arguments object.
  MOZ_ASSERT(!currentBlock()->info().needsArgsObj());
  return true;
}

bool WarpCacheIRTranspiler::emitLoadFrameCalleeResult() {
  if (const CallInfo* callInfo = builder_->inlineCallInfo()) {
    pushResult(callInfo->callee());
    return true;
  }

  auto* ins = MCallee::New(alloc());
  add(ins);
  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadFrameNumActualArgsResult() {
  if (const CallInfo* callInfo = builder_->inlineCallInfo()) {
    auto* ins = constant(Int32Value(callInfo->argc()));
    pushResult(ins);
    return true;
  }

  auto* ins = MArgumentsLength::New(alloc());
  add(ins);
  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadFrameArgumentResult(
    Int32OperandId indexId) {
  // We don't support arguments[i] in inlined functions. Scripts using
  // arguments[i] are marked as uninlineable in arguments analysis.
  MOZ_ASSERT(!builder_->inlineCallInfo());

  MDefinition* index = getOperand(indexId);

  auto* length = MArgumentsLength::New(alloc());
  add(length);

  index = addBoundsCheck(index, length);

  auto* load = MGetFrameArgument::New(alloc(), index);
  add(load);

  pushResult(load);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardNonDoubleType(ValOperandId inputId,
                                                   ValueType type) {
  switch (type) {
    case ValueType::String:
    case ValueType::Symbol:
    case ValueType::BigInt:
    case ValueType::Int32:
    case ValueType::Boolean:
      return emitGuardTo(inputId, MIRTypeFromValueType(JSValueType(type)));
    case ValueType::Undefined:
      return emitGuardIsUndefined(inputId);
    case ValueType::Null:
      return emitGuardIsNull(inputId);
    case ValueType::Double:
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

bool WarpCacheIRTranspiler::emitGuardToSymbol(ValOperandId inputId) {
  return emitGuardTo(inputId, MIRType::Symbol);
}

bool WarpCacheIRTranspiler::emitGuardToBigInt(ValOperandId inputId) {
  return emitGuardTo(inputId, MIRType::BigInt);
}

bool WarpCacheIRTranspiler::emitGuardToBoolean(ValOperandId inputId) {
  return emitGuardTo(inputId, MIRType::Boolean);
}

bool WarpCacheIRTranspiler::emitGuardToInt32(ValOperandId inputId) {
  return emitGuardTo(inputId, MIRType::Int32);
}

bool WarpCacheIRTranspiler::emitGuardBooleanToInt32(ValOperandId inputId,
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

  auto* ins = MToIntegerInt32::New(alloc(), boolean);
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

  setOperand(inputId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardIsNull(ValOperandId inputId) {
  MDefinition* input = getOperand(inputId);
  if (input->type() == MIRType::Null) {
    return true;
  }

  auto* ins = MGuardValue::New(alloc(), input, NullValue());
  add(ins);
  setOperand(inputId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardIsUndefined(ValOperandId inputId) {
  MDefinition* input = getOperand(inputId);
  if (input->type() == MIRType::Undefined) {
    return true;
  }

  auto* ins = MGuardValue::New(alloc(), input, UndefinedValue());
  add(ins);
  setOperand(inputId, ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGuardTagNotEqual(ValueTagOperandId lhsId,
                                                 ValueTagOperandId rhsId) {
  MDefinition* lhs = getOperand(lhsId);
  MDefinition* rhs = getOperand(rhsId);

  auto* ins = MGuardTagNotEqual::New(alloc(), lhs, rhs);
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

  MDefinition* number;
  if (input->type() == MIRType::Int32 || input->type() == MIRType::Double) {
    number = input;
  } else {
    auto* unbox =
        MUnbox::New(alloc(), input, MIRType::Double, MUnbox::Fallible);
    add(unbox);
    number = unbox;
  }

  auto* ins = MTypedArrayIndexToInt32::New(alloc(), number);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitTruncateDoubleToUInt32(
    NumberOperandId inputId, Int32OperandId resultId) {
  MDefinition* input = getOperand(inputId);
  auto* ins = MTruncateToInt32::New(alloc(), input);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitGuardToInt32ModUint32(ValOperandId valId,
                                                      Int32OperandId resultId) {
  MDefinition* input = getOperand(valId);
  auto* ins = MTruncateToInt32::New(alloc(), input);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitGuardToUint8Clamped(ValOperandId valId,
                                                    Int32OperandId resultId) {
  MDefinition* input = getOperand(valId);
  auto* ins = MClampToUint8::New(alloc(), input);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitToString(OperandId inputId,
                                         StringOperandId resultId) {
  MDefinition* input = getOperand(inputId);
  auto* ins =
      MToString::New(alloc(), input, MToString::SideEffectHandling::Bailout);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitCallInt32ToString(Int32OperandId inputId,
                                                  StringOperandId resultId) {
  return emitToString(inputId, resultId);
}

bool WarpCacheIRTranspiler::emitCallNumberToString(NumberOperandId inputId,
                                                   StringOperandId resultId) {
  return emitToString(inputId, resultId);
}

bool WarpCacheIRTranspiler::emitBooleanToString(BooleanOperandId inputId,
                                                StringOperandId resultId) {
  return emitToString(inputId, resultId);
}

bool WarpCacheIRTranspiler::emitLoadInt32Result(Int32OperandId valId) {
  MDefinition* val = getOperand(valId);
  MOZ_ASSERT(val->type() == MIRType::Int32);
  pushResult(val);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadDoubleResult(NumberOperandId valId) {
  MDefinition* val = getOperand(valId);
  MOZ_ASSERT(val->type() == MIRType::Double);
  pushResult(val);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadBigIntResult(BigIntOperandId valId) {
  MDefinition* val = getOperand(valId);
  MOZ_ASSERT(val->type() == MIRType::BigInt);
  pushResult(val);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadObjectResult(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);
  MOZ_ASSERT(obj->type() == MIRType::Object);
  pushResult(obj);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadStringResult(StringOperandId strId) {
  MDefinition* str = getOperand(strId);
  MOZ_ASSERT(str->type() == MIRType::String);
  pushResult(str);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadSymbolResult(SymbolOperandId symId) {
  MDefinition* sym = getOperand(symId);
  MOZ_ASSERT(sym->type() == MIRType::Symbol);
  pushResult(sym);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadUndefinedResult() {
  pushResult(constant(UndefinedValue()));
  return true;
}

bool WarpCacheIRTranspiler::emitLoadBooleanResult(bool val) {
  pushResult(constant(BooleanValue(val)));
  return true;
}

bool WarpCacheIRTranspiler::emitLoadInt32Constant(uint32_t valOffset,
                                                  Int32OperandId resultId) {
  int32_t val = int32StubField(valOffset);
  auto* valConst = constant(Int32Value(val));
  return defineOperand(resultId, valConst);
}

bool WarpCacheIRTranspiler::emitLoadBooleanConstant(bool val,
                                                    BooleanOperandId resultId) {
  auto* valConst = constant(BooleanValue(val));
  return defineOperand(resultId, valConst);
}

bool WarpCacheIRTranspiler::emitLoadUndefined(ValOperandId resultId) {
  auto* valConst = constant(UndefinedValue());
  return defineOperand(resultId, valConst);
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
  MInstruction* ins = objectStubField(objOffset);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitLoadProto(ObjOperandId objId,
                                          ObjOperandId resultId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MObjectStaticProto::New(alloc(), obj);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitLoadValueTag(ValOperandId valId,
                                             ValueTagOperandId resultId) {
  MDefinition* val = getOperand(valId);

  auto* ins = MLoadValueTag::New(alloc(), val);
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

  auto* load = MLoadDynamicSlot::New(alloc(), slots, slotIndex);
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

bool WarpCacheIRTranspiler::emitLoadFixedSlotTypedResult(ObjOperandId objId,
                                                         uint32_t offsetOffset,
                                                         ValueType type) {
  int32_t offset = int32StubField(offsetOffset);

  MDefinition* obj = getOperand(objId);
  uint32_t slotIndex = NativeObject::getFixedSlotIndexFromOffset(offset);

  auto* load = MLoadFixedSlot::New(alloc(), obj, slotIndex);
  load->setResultType(MIRTypeFromValueType(JSValueType(type)));
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

  if (snapshot().bailoutInfo().failedLexicalCheck()) {
    lexicalCheck->setNotMovable();
  }

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

  auto* load = MLoadDynamicSlot::New(alloc(), slots, slotIndex);
  add(load);

  auto* lexicalCheck = MLexicalCheck::New(alloc(), load);
  add(lexicalCheck);

  if (snapshot().bailoutInfo().failedLexicalCheck()) {
    lexicalCheck->setNotMovable();
  }

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

bool WarpCacheIRTranspiler::emitLoadInt32ArrayLength(ObjOperandId objId,
                                                     Int32OperandId resultId) {
  MDefinition* obj = getOperand(objId);

  auto* elements = MElements::New(alloc(), obj);
  add(elements);

  auto* length = MArrayLength::New(alloc(), elements);
  add(length);

  return defineOperand(resultId, length);
}

bool WarpCacheIRTranspiler::emitLoadTypedArrayLengthResult(
    ObjOperandId objId, uint32_t getterOffset) {
  MDefinition* obj = getOperand(objId);

  auto* length = MArrayBufferViewLength::New(alloc(), obj);
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

  if (snapshot().bailoutInfo().failedBoundsCheck()) {
    check->setNotMovable();
  }

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

bool WarpCacheIRTranspiler::emitLoadDenseElementHoleResult(
    ObjOperandId objId, Int32OperandId indexId) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);

  auto* elements = MElements::New(alloc(), obj);
  add(elements);

  auto* length = MInitializedLength::New(alloc(), elements);
  add(length);

  bool needsHoleCheck = true;
  auto* load =
      MLoadElementHole::New(alloc(), elements, index, length, needsHoleCheck);
  add(load);

  pushResult(load);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadTypedArrayElementResult(
    ObjOperandId objId, Int32OperandId indexId, Scalar::Type elementType,
    bool handleOOB, bool allowDoubleForUint32) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);

  if (handleOOB) {
    auto* load = MLoadTypedArrayElementHole::New(
        alloc(), obj, index, elementType, allowDoubleForUint32);
    add(load);

    pushResult(load);
    return true;
  }

  auto* length = MArrayBufferViewLength::New(alloc(), obj);
  add(length);

  index = addBoundsCheck(index, length);

  auto* elements = MArrayBufferViewElements::New(alloc(), obj);
  add(elements);

  auto* load = MLoadUnboxedScalar::New(alloc(), elements, index, elementType);
  load->setResultType(
      MIRTypeForArrayBufferViewRead(elementType, allowDoubleForUint32));
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

bool WarpCacheIRTranspiler::emitLoadStringCharCodeResult(
    StringOperandId strId, Int32OperandId indexId) {
  MDefinition* str = getOperand(strId);
  MDefinition* index = getOperand(indexId);

  auto* length = MStringLength::New(alloc(), str);
  add(length);

  index = addBoundsCheck(index, length);

  auto* charCode = MCharCodeAt::New(alloc(), str, index);
  add(charCode);

  pushResult(charCode);
  return true;
}

bool WarpCacheIRTranspiler::emitStringFromCharCodeResult(
    Int32OperandId codeId) {
  MDefinition* code = getOperand(codeId);

  auto* fromCharCode = MFromCharCode::New(alloc(), code);
  add(fromCharCode);

  pushResult(fromCharCode);
  return true;
}

bool WarpCacheIRTranspiler::emitStringFromCodePointResult(
    Int32OperandId codeId) {
  MDefinition* code = getOperand(codeId);

  auto* fromCodePoint = MFromCodePoint::New(alloc(), code);
  add(fromCodePoint);

  pushResult(fromCodePoint);
  return true;
}

bool WarpCacheIRTranspiler::emitStringToLowerCaseResult(StringOperandId strId) {
  MDefinition* str = getOperand(strId);

  auto* convert =
      MStringConvertCase::New(alloc(), str, MStringConvertCase::LowerCase);
  add(convert);

  pushResult(convert);
  return true;
}

bool WarpCacheIRTranspiler::emitStringToUpperCaseResult(StringOperandId strId) {
  MDefinition* str = getOperand(strId);

  auto* convert =
      MStringConvertCase::New(alloc(), str, MStringConvertCase::UpperCase);
  add(convert);

  pushResult(convert);
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

  auto* store = MStoreDynamicSlot::NewBarriered(alloc(), slots, slotIndex, rhs);
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

bool WarpCacheIRTranspiler::emitStoreFixedSlotUndefinedResult(
    ObjOperandId objId, uint32_t offsetOffset, ValOperandId rhsId) {
  int32_t offset = int32StubField(offsetOffset);

  MDefinition* obj = getOperand(objId);
  size_t slotIndex = NativeObject::getFixedSlotIndexFromOffset(offset);
  MDefinition* rhs = getOperand(rhsId);

  auto* barrier = MPostWriteBarrier::New(alloc(), obj, rhs);
  add(barrier);

  auto* store = MStoreFixedSlot::NewBarriered(alloc(), obj, slotIndex, rhs);
  addEffectful(store);

  auto* undef = constant(UndefinedValue());
  pushResult(undef);

  return resumeAfter(store);
}

bool WarpCacheIRTranspiler::emitAddAndStoreSlotShared(
    MAddAndStoreSlot::Kind kind, ObjOperandId objId, uint32_t offsetOffset,
    ValOperandId rhsId, uint32_t newShapeOffset) {
  int32_t offset = int32StubField(offsetOffset);
  Shape* shape = shapeStubField(newShapeOffset);

  MDefinition* obj = getOperand(objId);
  MDefinition* rhs = getOperand(rhsId);

  auto* barrier = MPostWriteBarrier::New(alloc(), obj, rhs);
  add(barrier);

  auto* addAndStore =
      MAddAndStoreSlot::New(alloc(), obj, rhs, kind, offset, shape);
  addEffectful(addAndStore);

  return resumeAfter(addAndStore);
}

bool WarpCacheIRTranspiler::emitAddAndStoreFixedSlot(
    ObjOperandId objId, uint32_t offsetOffset, ValOperandId rhsId,
    bool changeGroup, uint32_t newGroupOffset, uint32_t newShapeOffset) {
  MOZ_ASSERT(!changeGroup);

  return emitAddAndStoreSlotShared(MAddAndStoreSlot::Kind::FixedSlot, objId,
                                   offsetOffset, rhsId, newShapeOffset);
}

bool WarpCacheIRTranspiler::emitAddAndStoreDynamicSlot(
    ObjOperandId objId, uint32_t offsetOffset, ValOperandId rhsId,
    bool changeGroup, uint32_t newGroupOffset, uint32_t newShapeOffset) {
  MOZ_ASSERT(!changeGroup);

  return emitAddAndStoreSlotShared(MAddAndStoreSlot::Kind::DynamicSlot, objId,
                                   offsetOffset, rhsId, newShapeOffset);
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

bool WarpCacheIRTranspiler::emitStoreTypedArrayElement(ObjOperandId objId,
                                                       Scalar::Type elementType,
                                                       Int32OperandId indexId,
                                                       uint32_t rhsId,
                                                       bool handleOOB) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);
  MDefinition* rhs = getOperand(ValOperandId(rhsId));

  auto* length = MArrayBufferViewLength::New(alloc(), obj);
  add(length);

  if (!handleOOB) {
    // MStoreTypedArrayElementHole does the bounds checking.
    index = addBoundsCheck(index, length);
  }

  auto* elements = MArrayBufferViewElements::New(alloc(), obj);
  add(elements);

  MInstruction* store;
  if (handleOOB) {
    store = MStoreTypedArrayElementHole::New(alloc(), elements, length, index,
                                             rhs, elementType);
  } else {
    store =
        MStoreUnboxedScalar::New(alloc(), elements, index, rhs, elementType);
  }
  addEffectful(store);
  return resumeAfter(store);
}

void WarpCacheIRTranspiler::addDataViewData(MDefinition* obj, Scalar::Type type,
                                            MDefinition** offset,
                                            MInstruction** elements) {
  MInstruction* length = MArrayBufferViewLength::New(alloc(), obj);
  add(length);

  // Adjust the length to account for accesses near the end of the dataview.
  if (size_t byteSize = Scalar::byteSize(type); byteSize > 1) {
    // To ensure |0 <= offset && offset + byteSize <= length|, we can either
    // emit |BoundsCheck(offset, length)| followed by
    // |BoundsCheck(offset + (byteSize - 1), length)|, or alternatively emit
    // |BoundsCheck(offset, Max(length - (byteSize - 1), 0))|. The latter should
    // result in faster code when LICM moves the length adjustment and also
    // ensures Spectre index masking occurs after all bounds checks.

    auto* byteSizeMinusOne = MConstant::New(alloc(), Int32Value(byteSize - 1));
    add(byteSizeMinusOne);

    length = MSub::New(alloc(), length, byteSizeMinusOne, MIRType::Int32);
    length->toSub()->setTruncateKind(MDefinition::Truncate);
    add(length);

    // |length| mustn't be negative for MBoundsCheck.
    auto* zero = MConstant::New(alloc(), Int32Value(0));
    add(zero);

    length = MMinMax::New(alloc(), length, zero, MIRType::Int32, true);
    add(length);
  }

  *offset = addBoundsCheck(*offset, length);

  *elements = MArrayBufferViewElements::New(alloc(), obj);
  add(*elements);
}

bool WarpCacheIRTranspiler::emitLoadDataViewValueResult(
    ObjOperandId objId, Int32OperandId offsetId,
    BooleanOperandId littleEndianId, Scalar::Type elementType,
    bool allowDoubleForUint32) {
  MDefinition* obj = getOperand(objId);
  MDefinition* offset = getOperand(offsetId);
  MDefinition* littleEndian = getOperand(littleEndianId);

  // Add bounds check and get the DataViewObject's elements.
  MInstruction* elements;
  addDataViewData(obj, elementType, &offset, &elements);

  // Load the element.
  MInstruction* load;
  if (Scalar::byteSize(elementType) == 1) {
    load = MLoadUnboxedScalar::New(alloc(), elements, offset, elementType);
  } else {
    load = MLoadDataViewElement::New(alloc(), elements, offset, littleEndian,
                                     elementType);
  }
  add(load);

  MIRType knownType =
      MIRTypeForArrayBufferViewRead(elementType, allowDoubleForUint32);
  load->setResultType(knownType);

  pushResult(load);
  return true;
}

bool WarpCacheIRTranspiler::emitStoreDataViewValueResult(
    ObjOperandId objId, Int32OperandId offsetId, uint32_t valueId,
    BooleanOperandId littleEndianId, Scalar::Type elementType) {
  MDefinition* obj = getOperand(objId);
  MDefinition* offset = getOperand(offsetId);
  MDefinition* value = getOperand(ValOperandId(valueId));
  MDefinition* littleEndian = getOperand(littleEndianId);

  // Add bounds check and get the DataViewObject's elements.
  MInstruction* elements;
  addDataViewData(obj, elementType, &offset, &elements);

  // Store the element.
  MInstruction* store;
  if (Scalar::byteSize(elementType) == 1) {
    store =
        MStoreUnboxedScalar::New(alloc(), elements, offset, value, elementType);
  } else {
    store = MStoreDataViewElement::New(alloc(), elements, offset, value,
                                       littleEndian, elementType);
  }
  addEffectful(store);

  pushResult(constant(UndefinedValue()));

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

bool WarpCacheIRTranspiler::emitDoubleIncResult(NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* constOne = MConstant::New(alloc(), DoubleValue(1.0));
  add(constOne);

  auto* ins = MAdd::New(alloc(), input, constOne, MIRType::Double);
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

bool WarpCacheIRTranspiler::emitDoubleDecResult(NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* constOne = MConstant::New(alloc(), DoubleValue(1.0));
  add(constOne);

  auto* ins = MSub::New(alloc(), input, constOne, MIRType::Double);
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

bool WarpCacheIRTranspiler::emitDoubleNegationResult(NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* constNegOne = MConstant::New(alloc(), DoubleValue(-1.0));
  add(constNegOne);

  auto* ins = MMul::New(alloc(), input, constNegOne, MIRType::Double);
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

bool WarpCacheIRTranspiler::emitInt32PowResult(Int32OperandId lhsId,
                                               Int32OperandId rhsId) {
  return emitInt32BinaryArithResult<MPow>(lhsId, rhsId);
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

bool WarpCacheIRTranspiler::emitInt32URightShiftResult(Int32OperandId lhsId,
                                                       Int32OperandId rhsId,
                                                       bool allowDouble) {
  MDefinition* lhs = getOperand(lhsId);
  MDefinition* rhs = getOperand(rhsId);

  MIRType specialization = allowDouble ? MIRType::Double : MIRType::Int32;
  auto* ins = MUrsh::New(alloc(), lhs, rhs, specialization);
  add(ins);

  pushResult(ins);
  return true;
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

bool WarpCacheIRTranspiler::emitCompareSymbolResult(JSOp op,
                                                    SymbolOperandId lhsId,
                                                    SymbolOperandId rhsId) {
  MOZ_ASSERT(IsEqualityOp(op));
  return emitCompareResult(op, lhsId, rhsId, MCompare::Compare_Symbol);
}

bool WarpCacheIRTranspiler::emitCompareDoubleSameValueResult(
    NumberOperandId lhsId, NumberOperandId rhsId) {
  MDefinition* lhs = getOperand(lhsId);
  MDefinition* rhs = getOperand(rhsId);

  auto* sameValue = MSameValue::New(alloc(), lhs, rhs);
  add(sameValue);

  pushResult(sameValue);
  return true;
}

bool WarpCacheIRTranspiler::emitMathHypot2NumberResult(
    NumberOperandId firstId, NumberOperandId secondId) {
  MDefinitionVector vector(alloc());
  if (!vector.reserve(2)) {
    return false;
  }

  vector.infallibleAppend(getOperand(firstId));
  vector.infallibleAppend(getOperand(secondId));

  auto* ins = MHypot::New(alloc(), vector);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathHypot3NumberResult(
    NumberOperandId firstId, NumberOperandId secondId,
    NumberOperandId thirdId) {
  MDefinitionVector vector(alloc());
  if (!vector.reserve(3)) {
    return false;
  }

  vector.infallibleAppend(getOperand(firstId));
  vector.infallibleAppend(getOperand(secondId));
  vector.infallibleAppend(getOperand(thirdId));

  auto* ins = MHypot::New(alloc(), vector);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathHypot4NumberResult(
    NumberOperandId firstId, NumberOperandId secondId, NumberOperandId thirdId,
    NumberOperandId fourthId) {
  MDefinitionVector vector(alloc());
  if (!vector.reserve(4)) {
    return false;
  }

  vector.infallibleAppend(getOperand(firstId));
  vector.infallibleAppend(getOperand(secondId));
  vector.infallibleAppend(getOperand(thirdId));
  vector.infallibleAppend(getOperand(fourthId));

  auto* ins = MHypot::New(alloc(), vector);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathRandomResult(uint32_t rngOffset) {
#ifdef DEBUG
  // CodeGenerator uses CompileRealm::addressOfRandomNumberGenerator. Assert it
  // matches the RNG pointer stored in the stub field.
  const void* rng = rawPointerField(rngOffset);
  MOZ_ASSERT(rng == mirGen().realm->addressOfRandomNumberGenerator());
#endif

  auto* ins = MRandom::New(alloc());
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitInt32MinMax(bool isMax, Int32OperandId firstId,
                                            Int32OperandId secondId,
                                            Int32OperandId resultId) {
  MDefinition* first = getOperand(firstId);
  MDefinition* second = getOperand(secondId);

  auto* ins = MMinMax::New(alloc(), first, second, MIRType::Int32, isMax);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitNumberMinMax(bool isMax,
                                             NumberOperandId firstId,
                                             NumberOperandId secondId,
                                             NumberOperandId resultId) {
  MDefinition* first = getOperand(firstId);
  MDefinition* second = getOperand(secondId);

  auto* ins = MMinMax::New(alloc(), first, second, MIRType::Double, isMax);
  add(ins);

  return defineOperand(resultId, ins);
}

bool WarpCacheIRTranspiler::emitMathAbsInt32Result(Int32OperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MAbs::New(alloc(), input, MIRType::Int32);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathAbsNumberResult(NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MAbs::New(alloc(), input, MIRType::Double);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathClz32Result(Int32OperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MClz::New(alloc(), input, MIRType::Int32);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathSignInt32Result(Int32OperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MSign::New(alloc(), input, MIRType::Int32);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathSignNumberResult(NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MSign::New(alloc(), input, MIRType::Double);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathSignNumberToInt32Result(
    NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MSign::New(alloc(), input, MIRType::Int32);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathImulResult(Int32OperandId lhsId,
                                               Int32OperandId rhsId) {
  MDefinition* lhs = getOperand(lhsId);
  MDefinition* rhs = getOperand(rhsId);

  auto* ins = MMul::New(alloc(), lhs, rhs, MIRType::Int32, MMul::Integer);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathFloorToInt32Result(
    NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MFloor::New(alloc(), input);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathCeilToInt32Result(NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MCeil::New(alloc(), input);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathTruncToInt32Result(
    NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MTrunc::New(alloc(), input);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathRoundToInt32Result(
    NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MRound::New(alloc(), input);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathSqrtNumberResult(NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MSqrt::New(alloc(), input, MIRType::Double);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathFRoundNumberResult(
    NumberOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MToFloat32::New(alloc(), input);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathAtan2NumberResult(NumberOperandId yId,
                                                      NumberOperandId xId) {
  MDefinition* y = getOperand(yId);
  MDefinition* x = getOperand(xId);

  auto* ins = MAtan2::New(alloc(), y, x);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitMathFunctionNumberResult(
    NumberOperandId inputId, UnaryMathFunction fun) {
  MDefinition* input = getOperand(inputId);

  auto* ins = MMathFunction::New(alloc(), input, fun);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitReflectGetPrototypeOfResult(
    ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MGetPrototypeOf::New(alloc(), obj);
  addEffectful(ins);
  pushResult(ins);

  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitArrayPush(ObjOperandId objId,
                                          ValOperandId rhsId) {
  MDefinition* obj = getOperand(objId);
  MDefinition* value = getOperand(rhsId);

  auto* elements = MElements::New(alloc(), obj);
  add(elements);

  auto* initLength = MInitializedLength::New(alloc(), elements);
  add(initLength);

  auto* barrier =
      MPostWriteElementBarrier::New(alloc(), obj, value, initLength);
  add(barrier);

  auto* ins = MArrayPush::New(alloc(), obj, value);
  addEffectful(ins);
  pushResult(ins);

  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitPackedArrayPopResult(ObjOperandId arrayId) {
  MDefinition* array = getOperand(arrayId);

  // TODO(Warp): these flags only make sense for the Ion implementation. Remove
  // them when IonBuilder is gone.
  bool needsHoleCheck = true;
  bool maybeUndefined = true;
  auto* ins = MArrayPopShift::New(alloc(), array, MArrayPopShift::Pop,
                                  needsHoleCheck, maybeUndefined);
  addEffectful(ins);

  pushResult(ins);
  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitPackedArrayShiftResult(ObjOperandId arrayId) {
  MDefinition* array = getOperand(arrayId);

  // TODO(Warp): these flags only make sense for the Ion implementation. Remove
  // them when IonBuilder is gone.
  bool needsHoleCheck = true;
  bool maybeUndefined = true;
  auto* ins = MArrayPopShift::New(alloc(), array, MArrayPopShift::Shift,
                                  needsHoleCheck, maybeUndefined);
  addEffectful(ins);

  pushResult(ins);
  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitPackedArraySliceResult(
    uint32_t templateObjectOffset, ObjOperandId arrayId, Int32OperandId beginId,
    Int32OperandId endId) {
  JSObject* templateObj = tenuredObjectStubField(templateObjectOffset);

  MDefinition* array = getOperand(arrayId);
  MDefinition* begin = getOperand(beginId);
  MDefinition* end = getOperand(endId);

  // TODO: support pre-tenuring.
  gc::InitialHeap heap = gc::DefaultHeap;

  auto* ins = MArraySlice::New(alloc(), array, begin, end, templateObj, heap);
  addEffectful(ins);

  pushResult(ins);
  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitHasClassResult(ObjOperandId objId,
                                               uint32_t claspOffset) {
  MDefinition* obj = getOperand(objId);
  const JSClass* clasp = classStubField(claspOffset);

  auto* hasClass = MHasClass::New(alloc(), obj, clasp);
  add(hasClass);

  pushResult(hasClass);
  return true;
}

bool WarpCacheIRTranspiler::emitCallRegExpMatcherResult(
    ObjOperandId regexpId, StringOperandId inputId,
    Int32OperandId lastIndexId) {
  MDefinition* regexp = getOperand(regexpId);
  MDefinition* input = getOperand(inputId);
  MDefinition* lastIndex = getOperand(lastIndexId);

  auto* matcher = MRegExpMatcher::New(alloc(), regexp, input, lastIndex);
  addEffectful(matcher);
  pushResult(matcher);

  return resumeAfter(matcher);
}

bool WarpCacheIRTranspiler::emitCallRegExpSearcherResult(
    ObjOperandId regexpId, StringOperandId inputId,
    Int32OperandId lastIndexId) {
  MDefinition* regexp = getOperand(regexpId);
  MDefinition* input = getOperand(inputId);
  MDefinition* lastIndex = getOperand(lastIndexId);

  auto* searcher = MRegExpSearcher::New(alloc(), regexp, input, lastIndex);
  addEffectful(searcher);
  pushResult(searcher);

  return resumeAfter(searcher);
}

bool WarpCacheIRTranspiler::emitCallRegExpTesterResult(
    ObjOperandId regexpId, StringOperandId inputId,
    Int32OperandId lastIndexId) {
  MDefinition* regexp = getOperand(regexpId);
  MDefinition* input = getOperand(inputId);
  MDefinition* lastIndex = getOperand(lastIndexId);

  auto* tester = MRegExpTester::New(alloc(), regexp, input, lastIndex);
  addEffectful(tester);
  pushResult(tester);

  return resumeAfter(tester);
}

bool WarpCacheIRTranspiler::emitCallSubstringKernelResult(
    StringOperandId strId, Int32OperandId beginId, Int32OperandId lengthId) {
  MDefinition* str = getOperand(strId);
  MDefinition* begin = getOperand(beginId);
  MDefinition* length = getOperand(lengthId);

  auto* substr = MSubstr::New(alloc(), str, begin, length);
  add(substr);

  pushResult(substr);
  return true;
}

bool WarpCacheIRTranspiler::emitStringReplaceStringResult(
    StringOperandId strId, StringOperandId patternId,
    StringOperandId replacementId) {
  MDefinition* str = getOperand(strId);
  MDefinition* pattern = getOperand(patternId);
  MDefinition* replacement = getOperand(replacementId);

  auto* replace = MStringReplace::New(alloc(), str, pattern, replacement);
  add(replace);

  pushResult(replace);
  return true;
}

bool WarpCacheIRTranspiler::emitStringSplitStringResult(
    StringOperandId strId, StringOperandId separatorId, uint32_t groupOffset) {
  MDefinition* str = getOperand(strId);
  MDefinition* separator = getOperand(separatorId);
  ObjectGroup* group = groupStubField(groupOffset);

  auto* split = MStringSplit::New(alloc(), /* constraints = */ nullptr, str,
                                  separator, group);
  add(split);

  pushResult(split);
  return true;
}

bool WarpCacheIRTranspiler::emitRegExpPrototypeOptimizableResult(
    ObjOperandId protoId) {
  MDefinition* proto = getOperand(protoId);

  auto* optimizable = MRegExpPrototypeOptimizable::New(alloc(), proto);
  add(optimizable);

  pushResult(optimizable);
  return true;
}

bool WarpCacheIRTranspiler::emitRegExpInstanceOptimizableResult(
    ObjOperandId regexpId, ObjOperandId protoId) {
  MDefinition* regexp = getOperand(regexpId);
  MDefinition* proto = getOperand(protoId);

  auto* optimizable = MRegExpInstanceOptimizable::New(alloc(), regexp, proto);
  add(optimizable);

  pushResult(optimizable);
  return true;
}

bool WarpCacheIRTranspiler::emitGetFirstDollarIndexResult(
    StringOperandId strId) {
  MDefinition* str = getOperand(strId);

  auto* firstDollarIndex = MGetFirstDollarIndex::New(alloc(), str);
  add(firstDollarIndex);

  pushResult(firstDollarIndex);
  return true;
}

bool WarpCacheIRTranspiler::emitIsArrayResult(ValOperandId inputId) {
  MDefinition* value = getOperand(inputId);

  auto* isArray = MIsArray::New(alloc(), value);
  addEffectful(isArray);
  pushResult(isArray);

  return resumeAfter(isArray);
}

bool WarpCacheIRTranspiler::emitIsObjectResult(ValOperandId inputId) {
  MDefinition* value = getOperand(inputId);

  if (value->type() == MIRType::Object) {
    pushResult(constant(BooleanValue(true)));
  } else {
    auto* isObject = MIsObject::New(alloc(), value);
    add(isObject);
    pushResult(isObject);
  }

  return true;
}

bool WarpCacheIRTranspiler::emitIsPackedArrayResult(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* isPackedArray = MIsPackedArray::New(alloc(), obj);
  add(isPackedArray);

  pushResult(isPackedArray);
  return true;
}

bool WarpCacheIRTranspiler::emitIsCallableResult(ValOperandId inputId) {
  MDefinition* value = getOperand(inputId);

  auto* isCallable = MIsCallable::New(alloc(), value);
  add(isCallable);

  pushResult(isCallable);
  return true;
}

bool WarpCacheIRTranspiler::emitIsConstructorResult(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* isConstructor = MIsConstructor::New(alloc(), obj);
  add(isConstructor);

  pushResult(isConstructor);
  return true;
}

bool WarpCacheIRTranspiler::emitIsCrossRealmArrayConstructorResult(
    ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MIsCrossRealmArrayConstructor::New(alloc(), obj);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitIsTypedArrayResult(ObjOperandId objId,
                                                   bool isPossiblyWrapped) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MIsTypedArray::New(alloc(), obj, isPossiblyWrapped);
  if (isPossiblyWrapped) {
    addEffectful(ins);
  } else {
    add(ins);
  }

  pushResult(ins);

  if (isPossiblyWrapped) {
    if (!resumeAfter(ins)) {
      return false;
    }
  }

  return true;
}

bool WarpCacheIRTranspiler::emitTypedArrayByteOffsetResult(ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MArrayBufferViewByteOffset::New(alloc(), obj);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitTypedArrayElementShiftResult(
    ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MTypedArrayElementShift::New(alloc(), obj);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitIsTypedArrayConstructorResult(
    ObjOperandId objId) {
  MDefinition* obj = getOperand(objId);

  auto* ins = MIsTypedArrayConstructor::New(alloc(), obj);
  add(ins);

  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitGetNextMapSetEntryForIteratorResult(
    ObjOperandId iterId, ObjOperandId resultArrId, bool isMap) {
  MDefinition* iter = getOperand(iterId);
  MDefinition* resultArr = getOperand(resultArrId);

  MGetNextEntryForIterator::Mode mode =
      isMap ? MGetNextEntryForIterator::Map : MGetNextEntryForIterator::Set;
  auto* ins = MGetNextEntryForIterator::New(alloc(), iter, resultArr, mode);
  addEffectful(ins);
  pushResult(ins);

  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitFrameIsConstructingResult() {
  if (const CallInfo* callInfo = builder_->inlineCallInfo()) {
    auto* ins = constant(BooleanValue(callInfo->constructing()));
    pushResult(ins);
    return true;
  }

  auto* ins = MIsConstructing::New(alloc());
  add(ins);
  pushResult(ins);
  return true;
}

bool WarpCacheIRTranspiler::emitFinishBoundFunctionInitResult(
    ObjOperandId boundId, ObjOperandId targetId, Int32OperandId argCountId) {
  MDefinition* bound = getOperand(boundId);
  MDefinition* target = getOperand(targetId);
  MDefinition* argCount = getOperand(argCountId);

  auto* ins = MFinishBoundFunctionInit::New(alloc(), bound, target, argCount);
  addEffectful(ins);

  pushResult(constant(UndefinedValue()));
  return resumeAfter(ins);
}

bool WarpCacheIRTranspiler::emitNewIteratorResult(
    MNewIterator::Type type, uint32_t templateObjectOffset) {
  JSObject* templateObj = tenuredObjectStubField(templateObjectOffset);

  auto* templateConst = constant(ObjectValue(*templateObj));
  auto* iter = MNewIterator::New(alloc(), /* constraints = */ nullptr,
                                 templateConst, type);
  add(iter);

  pushResult(iter);
  return true;
}

bool WarpCacheIRTranspiler::emitNewArrayIteratorResult(
    uint32_t templateObjectOffset) {
  return emitNewIteratorResult(MNewIterator::ArrayIterator,
                               templateObjectOffset);
}

bool WarpCacheIRTranspiler::emitNewStringIteratorResult(
    uint32_t templateObjectOffset) {
  return emitNewIteratorResult(MNewIterator::StringIterator,
                               templateObjectOffset);
}

bool WarpCacheIRTranspiler::emitNewRegExpStringIteratorResult(
    uint32_t templateObjectOffset) {
  return emitNewIteratorResult(MNewIterator::RegExpStringIterator,
                               templateObjectOffset);
}

bool WarpCacheIRTranspiler::emitObjectCreateResult(
    uint32_t templateObjectOffset) {
  JSObject* templateObj = tenuredObjectStubField(templateObjectOffset);

  auto* templateConst = constant(ObjectValue(*templateObj));

  // TODO: support pre-tenuring.
  gc::InitialHeap heap = gc::DefaultHeap;
  auto* obj = MNewObject::New(alloc(), /* constraints = */ nullptr,
                              templateConst, heap, MNewObject::ObjectCreate);
  addEffectful(obj);

  pushResult(obj);
  return resumeAfter(obj);
}

bool WarpCacheIRTranspiler::emitNewArrayFromLengthResult(
    uint32_t templateObjectOffset, Int32OperandId lengthId) {
  JSObject* templateObj = tenuredObjectStubField(templateObjectOffset);
  MDefinition* length = getOperand(lengthId);

  // TODO: support pre-tenuring.
  gc::InitialHeap heap = gc::DefaultHeap;

  if (length->isConstant()) {
    int32_t lenInt32 = length->toConstant()->toInt32();
    if (lenInt32 >= 0 &&
        uint32_t(lenInt32) == templateObj->as<ArrayObject>().length()) {
      uint32_t len = uint32_t(lenInt32);
      auto* templateConst = constant(ObjectValue(*templateObj));

      size_t inlineLength =
          gc::GetGCKindSlots(templateObj->asTenured().getAllocKind()) -
          ObjectElements::VALUES_PER_HEADER;

      MNewArray* obj;
      if (len > inlineLength) {
        obj = MNewArray::NewVM(alloc(), /* constraints = */ nullptr, len,
                               templateConst, heap, loc_.toRawBytecode());
      } else {
        obj = MNewArray::New(alloc(), /* constraints = */ nullptr, len,
                             templateConst, heap, loc_.toRawBytecode());
      }
      add(obj);
      pushResult(obj);
      return true;
    }
  }

  auto* obj = MNewArrayDynamicLength::New(alloc(), /* constraints = */ nullptr,
                                          templateObj, heap, length);
  addEffectful(obj);
  pushResult(obj);
  return resumeAfter(obj);
}

bool WarpCacheIRTranspiler::emitNewTypedArrayFromLengthResult(
    uint32_t templateObjectOffset, Int32OperandId lengthId) {
  JSObject* templateObj = tenuredObjectStubField(templateObjectOffset);
  MDefinition* length = getOperand(lengthId);

  // TODO: support pre-tenuring.
  gc::InitialHeap heap = gc::DefaultHeap;

  if (length->isConstant()) {
    int32_t len = length->toConstant()->toInt32();
    if (len > 0 &&
        uint32_t(len) == templateObj->as<TypedArrayObject>().length()) {
      auto* templateConst = constant(ObjectValue(*templateObj));
      auto* obj = MNewTypedArray::New(alloc(), /* constraints = */ nullptr,
                                      templateConst, heap);
      add(obj);
      pushResult(obj);
      return true;
    }
  }

  auto* obj = MNewTypedArrayDynamicLength::New(
      alloc(), /* constraints = */ nullptr, templateObj, heap, length);
  addEffectful(obj);
  pushResult(obj);
  return resumeAfter(obj);
}

bool WarpCacheIRTranspiler::emitNewTypedArrayFromArrayBufferResult(
    uint32_t templateObjectOffset, ObjOperandId bufferId,
    ValOperandId byteOffsetId, ValOperandId lengthId) {
  JSObject* templateObj = tenuredObjectStubField(templateObjectOffset);
  MDefinition* buffer = getOperand(bufferId);
  MDefinition* byteOffset = getOperand(byteOffsetId);
  MDefinition* length = getOperand(lengthId);

  // TODO: support pre-tenuring.
  gc::InitialHeap heap = gc::DefaultHeap;

  auto* obj = MNewTypedArrayFromArrayBuffer::New(
      alloc(), /* constraints = */ nullptr, templateObj, heap, buffer,
      byteOffset, length);
  addEffectful(obj);

  pushResult(obj);
  return resumeAfter(obj);
}

bool WarpCacheIRTranspiler::emitNewTypedArrayFromArrayResult(
    uint32_t templateObjectOffset, ObjOperandId arrayId) {
  JSObject* templateObj = tenuredObjectStubField(templateObjectOffset);
  MDefinition* array = getOperand(arrayId);

  // TODO: support pre-tenuring.
  gc::InitialHeap heap = gc::DefaultHeap;

  auto* obj = MNewTypedArrayFromArray::New(alloc(), /* constraints = */ nullptr,
                                           templateObj, heap, array);
  addEffectful(obj);

  pushResult(obj);
  return resumeAfter(obj);
}

bool WarpCacheIRTranspiler::emitAtomicsCompareExchangeResult(
    ObjOperandId objId, Int32OperandId indexId, Int32OperandId expectedId,
    Int32OperandId replacementId, Scalar::Type elementType) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);
  MDefinition* expected = getOperand(expectedId);
  MDefinition* replacement = getOperand(replacementId);

  auto* length = MArrayBufferViewLength::New(alloc(), obj);
  add(length);

  index = addBoundsCheck(index, length);

  auto* elements = MArrayBufferViewElements::New(alloc(), obj);
  add(elements);

  bool allowDoubleForUint32 = true;
  MIRType knownType =
      MIRTypeForArrayBufferViewRead(elementType, allowDoubleForUint32);

  auto* cas = MCompareExchangeTypedArrayElement::New(
      alloc(), elements, index, elementType, expected, replacement);
  cas->setResultType(knownType);
  addEffectful(cas);

  pushResult(cas);
  return resumeAfter(cas);
}

bool WarpCacheIRTranspiler::emitAtomicsExchangeResult(
    ObjOperandId objId, Int32OperandId indexId, Int32OperandId valueId,
    Scalar::Type elementType) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);
  MDefinition* value = getOperand(valueId);

  auto* length = MArrayBufferViewLength::New(alloc(), obj);
  add(length);

  index = addBoundsCheck(index, length);

  auto* elements = MArrayBufferViewElements::New(alloc(), obj);
  add(elements);

  bool allowDoubleForUint32 = true;
  MIRType knownType =
      MIRTypeForArrayBufferViewRead(elementType, allowDoubleForUint32);

  auto* exchange = MAtomicExchangeTypedArrayElement::New(
      alloc(), elements, index, value, elementType);
  exchange->setResultType(knownType);
  addEffectful(exchange);

  pushResult(exchange);
  return resumeAfter(exchange);
}

bool WarpCacheIRTranspiler::emitAtomicsBinaryOp(ObjOperandId objId,
                                                Int32OperandId indexId,
                                                Int32OperandId valueId,
                                                Scalar::Type elementType,
                                                AtomicOp op) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);
  MDefinition* value = getOperand(valueId);

  auto* length = MArrayBufferViewLength::New(alloc(), obj);
  add(length);

  index = addBoundsCheck(index, length);

  auto* elements = MArrayBufferViewElements::New(alloc(), obj);
  add(elements);

  bool allowDoubleForUint32 = true;
  MIRType knownType =
      MIRTypeForArrayBufferViewRead(elementType, allowDoubleForUint32);

  auto* binop = MAtomicTypedArrayElementBinop::New(alloc(), op, elements, index,
                                                   elementType, value);
  binop->setResultType(knownType);
  addEffectful(binop);

  pushResult(binop);
  return resumeAfter(binop);
}

bool WarpCacheIRTranspiler::emitAtomicsAddResult(ObjOperandId objId,
                                                 Int32OperandId indexId,
                                                 Int32OperandId valueId,
                                                 Scalar::Type elementType) {
  return emitAtomicsBinaryOp(objId, indexId, valueId, elementType,
                             AtomicFetchAddOp);
}

bool WarpCacheIRTranspiler::emitAtomicsSubResult(ObjOperandId objId,
                                                 Int32OperandId indexId,
                                                 Int32OperandId valueId,
                                                 Scalar::Type elementType) {
  return emitAtomicsBinaryOp(objId, indexId, valueId, elementType,
                             AtomicFetchSubOp);
}

bool WarpCacheIRTranspiler::emitAtomicsAndResult(ObjOperandId objId,
                                                 Int32OperandId indexId,
                                                 Int32OperandId valueId,
                                                 Scalar::Type elementType) {
  return emitAtomicsBinaryOp(objId, indexId, valueId, elementType,
                             AtomicFetchAndOp);
}

bool WarpCacheIRTranspiler::emitAtomicsOrResult(ObjOperandId objId,
                                                Int32OperandId indexId,
                                                Int32OperandId valueId,
                                                Scalar::Type elementType) {
  return emitAtomicsBinaryOp(objId, indexId, valueId, elementType,
                             AtomicFetchOrOp);
}

bool WarpCacheIRTranspiler::emitAtomicsXorResult(ObjOperandId objId,
                                                 Int32OperandId indexId,
                                                 Int32OperandId valueId,
                                                 Scalar::Type elementType) {
  return emitAtomicsBinaryOp(objId, indexId, valueId, elementType,
                             AtomicFetchXorOp);
}

bool WarpCacheIRTranspiler::emitAtomicsLoadResult(ObjOperandId objId,
                                                  Int32OperandId indexId,
                                                  Scalar::Type elementType) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);

  auto* length = MArrayBufferViewLength::New(alloc(), obj);
  add(length);

  index = addBoundsCheck(index, length);

  auto* elements = MArrayBufferViewElements::New(alloc(), obj);
  add(elements);

  bool allowDoubleForUint32 = true;
  MIRType knownType =
      MIRTypeForArrayBufferViewRead(elementType, allowDoubleForUint32);

  auto* load = MLoadUnboxedScalar::New(alloc(), elements, index, elementType,
                                       DoesRequireMemoryBarrier);
  load->setResultType(knownType);
  addEffectful(load);

  pushResult(load);
  return resumeAfter(load);
}

bool WarpCacheIRTranspiler::emitAtomicsStoreResult(ObjOperandId objId,
                                                   Int32OperandId indexId,
                                                   Int32OperandId valueId,
                                                   Scalar::Type elementType) {
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);
  MDefinition* value = getOperand(valueId);

  auto* length = MArrayBufferViewLength::New(alloc(), obj);
  add(length);

  index = addBoundsCheck(index, length);

  auto* elements = MArrayBufferViewElements::New(alloc(), obj);
  add(elements);

  auto* store = MStoreUnboxedScalar::New(alloc(), elements, index, value,
                                         elementType, DoesRequireMemoryBarrier);
  addEffectful(store);

  pushResult(value);
  return resumeAfter(store);
}

bool WarpCacheIRTranspiler::emitAtomicsIsLockFreeResult(
    Int32OperandId valueId) {
  MDefinition* value = getOperand(valueId);

  auto* ilf = MAtomicIsLockFree::New(alloc(), value);
  add(ilf);

  pushResult(ilf);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadValueTruthyResult(ValOperandId inputId) {
  MDefinition* input = getOperand(inputId);

  // Convert to bool with the '!!' idiom.
  auto* resultInverted = MNot::New(alloc(), input);
  add(resultInverted);
  auto* result = MNot::New(alloc(), resultInverted);
  add(result);

  pushResult(result);
  return true;
}

bool WarpCacheIRTranspiler::emitLoadArgumentSlot(ValOperandId resultId,
                                                 uint32_t slotIndex) {
  // Reverse of GetIndexOfArgument specialized to !hasArgumentArray.
  MOZ_ASSERT(!loc_.isSpreadOp());

  // Layout:
  // NewTarget | Args.. (reversed)      | ThisValue | Callee
  // 0         | ArgC .. Arg1 Arg0 (+1) | argc (+1) | argc + 1 (+ 1)
  // ^ (if constructing)

  // NewTarget (optional)
  if (callInfo_->constructing()) {
    if (slotIndex == 0) {
      return defineOperand(resultId, callInfo_->getNewTarget());
    }

    slotIndex -= 1;  // Adjust slot index to match non-constructing calls.
  }

  // Args..
  if (slotIndex < callInfo_->argc()) {
    uint32_t arg = callInfo_->argc() - 1 - slotIndex;
    return defineOperand(resultId, callInfo_->getArg(arg));
  }

  // ThisValue
  if (slotIndex == callInfo_->argc()) {
    return defineOperand(resultId, callInfo_->thisArg());
  }

  // Callee
  MOZ_ASSERT(slotIndex == callInfo_->argc() + 1);
  return defineOperand(resultId, callInfo_->callee());
}

bool WarpCacheIRTranspiler::emitLoadArgumentFixedSlot(ValOperandId resultId,
                                                      uint8_t slotIndex) {
  return emitLoadArgumentSlot(resultId, slotIndex);
}

bool WarpCacheIRTranspiler::emitLoadArgumentDynamicSlot(ValOperandId resultId,
                                                        Int32OperandId argcId,
                                                        uint8_t slotIndex) {
#ifdef DEBUG
  MDefinition* argc = getOperand(argcId);
  MOZ_ASSERT(argc->toConstant()->toInt32() ==
             static_cast<int32_t>(callInfo_->argc()));
#endif

  return emitLoadArgumentSlot(resultId, callInfo_->argc() + slotIndex);
}

bool WarpCacheIRTranspiler::emitCallFunction(ObjOperandId calleeId,
                                             Int32OperandId argcId,
                                             CallFlags flags, CallKind kind) {
  MDefinition* callee = getOperand(calleeId);
#ifdef DEBUG
  MDefinition* argc = getOperand(argcId);
  MOZ_ASSERT(argc->toConstant()->toInt32() ==
             static_cast<int32_t>(callInfo_->argc()));
#endif

  // The transpilation will add various guards to the callee.
  // We replace the callee referenced by the CallInfo, so that
  // the resulting MCall instruction depends on these guards.
  callInfo_->setCallee(callee);

  MOZ_ASSERT(flags.getArgFormat() == CallFlags::Standard ||
             flags.getArgFormat() == CallFlags::FunCall);

  if (flags.getArgFormat() == CallFlags::FunCall) {
    MOZ_ASSERT(!callInfo_->constructing());

    // Note: setCallee above already changed the callee to the target
    // function instead of the |call| function.

    if (callInfo_->argc() == 0) {
      // Special case for fun.call() with no arguments.
      auto* undef = constant(UndefinedValue());
      callInfo_->setThis(undef);
    } else {
      // The first argument for |call| is the new this value.
      callInfo_->setThis(callInfo_->getArg(0));

      // Shift down all other arguments by removing the first.
      callInfo_->removeArg(0);
    }
  }

  // CacheIR emits the following for specialized calls:
  //     GuardSpecificFunction <callee> <func> ..
  //     Call(Native|Scripted)Function <callee> ..
  // We can use the <func> JSFunction object to specialize this call.
  WrappedFunction* wrappedTarget = nullptr;
  if (callee->isGuardSpecificFunction()) {
    auto* guard = callee->toGuardSpecificFunction();

    MDefinition* expectedDef = guard->expected();
    MOZ_ASSERT(expectedDef->isConstant() || expectedDef->isNurseryObject());

    // If this is a native without a JitEntry, WrappedFunction needs to know the
    // target JSFunction.
    // TODO: support nursery-allocated natives with WrappedFunction, maybe by
    // storing the JSNative in the Baseline stub like flags/nargs.
    bool isNative = guard->flags().isNativeWithoutJitEntry();
    if (!isNative || expectedDef->isConstant()) {
      JSFunction* nativeTarget = nullptr;
      if (isNative) {
        nativeTarget = &expectedDef->toConstant()->toObject().as<JSFunction>();
      }
      wrappedTarget = new (alloc())
          WrappedFunction(nativeTarget, guard->nargs(), guard->flags());
      MOZ_ASSERT_IF(kind == CallKind::Native,
                    wrappedTarget->isNativeWithoutJitEntry());
      MOZ_ASSERT_IF(kind == CallKind::Scripted, wrappedTarget->hasJitEntry());
    }
  }

  bool needsThisCheck = false;
  if (callInfo_->constructing()) {
    MOZ_ASSERT(flags.isConstructing());

    MDefinition* thisArg = callInfo_->thisArg();

    if (kind == CallKind::Native) {
      // Native functions keep the is-constructing MagicValue as |this|.
      // If one of the arguments uses spread syntax this can be a loop phi with
      // MIRType::Value.
      MOZ_ASSERT_IF(!thisArg->isPhi(),
                    thisArg->type() == MIRType::MagicIsConstructing);
    } else {
      MOZ_ASSERT(kind == CallKind::Scripted);

      // TODO: if wrappedTarget->constructorNeedsUninitializedThis(), use an
      // uninitialized-lexical constant as |this|. To do this we need to either
      // store a new flag in the GuardSpecificFunction CacheIR/MIR instructions
      // or we could add a new op similar to MetaTwoByte.

      if (!thisArg->isCreateThisWithTemplate()) {
        // Note: guard against Value loop phis similar to the Native case above.
        MOZ_ASSERT_IF(!thisArg->isPhi(),
                      thisArg->type() == MIRType::MagicIsConstructing);

        MDefinition* newTarget = callInfo_->getNewTarget();
        auto* createThis = MCreateThis::New(alloc(), callee, newTarget);
        add(createThis);

        thisArg->setImplicitlyUsedUnchecked();
        callInfo_->setThis(createThis);

        wrappedTarget = nullptr;
        needsThisCheck = true;
      }
    }
  }

  MCall* call = makeCall(*callInfo_, needsThisCheck, wrappedTarget);
  if (!call) {
    return false;
  }

  if (flags.isSameRealm()) {
    call->setNotCrossRealm();
  }

  addEffectful(call);
  pushResult(call);

  return resumeAfter(call);
}

#ifndef JS_SIMULATOR
bool WarpCacheIRTranspiler::emitCallNativeFunction(ObjOperandId calleeId,
                                                   Int32OperandId argcId,
                                                   CallFlags flags,
                                                   bool ignoresReturnValue) {
  // Instead of ignoresReturnValue we use CallInfo::ignoresReturnValue.
  return emitCallFunction(calleeId, argcId, flags, CallKind::Native);
}
#else
bool WarpCacheIRTranspiler::emitCallNativeFunction(ObjOperandId calleeId,
                                                   Int32OperandId argcId,
                                                   CallFlags flags,
                                                   uint32_t targetOffset) {
  return emitCallFunction(calleeId, argcId, flags, CallKind::Native);
}
#endif

bool WarpCacheIRTranspiler::emitCallScriptedFunction(ObjOperandId calleeId,
                                                     Int32OperandId argcId,
                                                     CallFlags flags) {
  return emitCallFunction(calleeId, argcId, flags, CallKind::Scripted);
}

bool WarpCacheIRTranspiler::emitCallInlinedFunction(ObjOperandId calleeId,
                                                    Int32OperandId argcId,
                                                    uint32_t icScriptOffset,
                                                    CallFlags flags) {
  if (callInfo_->isInlined()) {
    // We are transpiling to generate the correct guards. Code for the inlined
    // function itself will be generated in WarpBuilder::buildInlinedCall.
    return true;
  }
  return emitCallFunction(calleeId, argcId, flags, CallKind::Scripted);
}

// TODO: rename the MetaTwoByte op when IonBuilder is gone.
bool WarpCacheIRTranspiler::emitMetaTwoByte(MetaTwoByteKind kind,
                                            uint32_t functionObjectOffset,
                                            uint32_t templateObjectOffset) {
  if (kind != MetaTwoByteKind::ScriptedTemplateObject) {
    return true;
  }

  JSObject* templateObj = tenuredObjectStubField(templateObjectOffset);
  MConstant* templateConst = constant(ObjectValue(*templateObj));

  // TODO: support pre-tenuring.
  gc::InitialHeap heap = gc::DefaultHeap;

  auto* createThis = MCreateThisWithTemplate::New(
      alloc(), /* constraints = */ nullptr, templateConst, heap);
  add(createThis);

  callInfo_->thisArg()->setImplicitlyUsedUnchecked();
  callInfo_->setThis(createThis);
  return true;
}

bool WarpCacheIRTranspiler::emitTypeMonitorResult() {
  MOZ_ASSERT(pushedResult_, "Didn't push result MDefinition");
  return true;
}

bool WarpCacheIRTranspiler::emitReturnFromIC() { return true; }

bool WarpCacheIRTranspiler::emitBailout() {
  auto* bail = MBail::New(alloc());
  add(bail);

  return true;
}

bool WarpCacheIRTranspiler::emitAssertRecoveredOnBailoutResult(
    ValOperandId valId, bool mustBeRecovered) {
  MDefinition* val = getOperand(valId);

  // Don't assert for recovered instructions when recovering is disabled.
  if (JitOptions.disableRecoverIns) {
    pushResult(constant(UndefinedValue()));
    return true;
  }

  if (JitOptions.checkRangeAnalysis) {
    // If we are checking the range of all instructions, then the guards
    // inserted by Range Analysis prevent the use of recover instruction. Thus,
    // we just disable these checks.
    pushResult(constant(UndefinedValue()));
    return true;
  }

  auto* assert = MAssertRecoveredOnBailout::New(alloc(), val, mustBeRecovered);
  addEffectfulUnsafe(assert);
  current->push(assert);

  // Create an instruction sequence which implies that the argument of the
  // assertRecoveredOnBailout function would be encoded at least in one
  // Snapshot.
  auto* nop = MNop::New(alloc());
  add(nop);

  auto* resumePoint = MResumePoint::New(
      alloc(), nop->block(), loc_.toRawBytecode(), MResumePoint::ResumeAfter);
  if (!resumePoint) {
    return false;
  }
  nop->setResumePoint(resumePoint);

  auto* encode = MEncodeSnapshot::New(alloc());
  addEffectfulUnsafe(encode);

  current->pop();

  pushResult(constant(UndefinedValue()));
  return true;
}

static void MaybeSetImplicitlyUsed(uint32_t numInstructionIdsBefore,
                                   MDefinition* input) {
  // When building MIR from bytecode, for each MDefinition that's an operand to
  // a bytecode instruction, we must either add an SSA use or set the
  // ImplicitlyUsed flag on that definition. The ImplicitlyUsed flag prevents
  // the backend from optimizing-out values that will be used by Baseline after
  // a bailout.
  //
  // WarpBuilder uses WarpPoppedValueUseChecker to assert this invariant in
  // debug builds.
  //
  // This function is responsible for setting the ImplicitlyUsed flag for an
  // input when using the transpiler. It looks at the input's most recent use
  // and if that's an instruction that was added while transpiling this JSOp
  // (based on the MIR instruction id) we don't set the ImplicitlyUsed flag.

  if (input->isImplicitlyUsed()) {
    // Nothing to do.
    return;
  }

  // If the most recent use of 'input' is an instruction we just added, there is
  // nothing to do.
  MDefinition* inputUse = input->maybeMostRecentlyAddedDefUse();
  if (inputUse && inputUse->id() >= numInstructionIdsBefore) {
    return;
  }

  // The transpiler didn't add a use for 'input'.
  input->setImplicitlyUsed();
}

bool jit::TranspileCacheIRToMIR(WarpBuilder* builder, BytecodeLocation loc,
                                const WarpCacheIR* cacheIRSnapshot,
                                const MDefinitionStackVector& inputs) {
  uint32_t numInstructionIdsBefore =
      builder->mirGen().graph().getNumInstructionIds();

  WarpCacheIRTranspiler transpiler(builder, loc, nullptr, cacheIRSnapshot);
  if (!transpiler.transpile(inputs)) {
    return false;
  }

  for (MDefinition* input : inputs) {
    MaybeSetImplicitlyUsed(numInstructionIdsBefore, input);
  }

  return true;
}

bool jit::TranspileCacheIRToMIR(WarpBuilder* builder, BytecodeLocation loc,
                                const WarpCacheIR* cacheIRSnapshot,
                                CallInfo& callInfo) {
  uint32_t numInstructionIdsBefore =
      builder->mirGen().graph().getNumInstructionIds();

  // Synthesize the constant number of arguments for this call op.
  auto* argc = MConstant::New(builder->alloc(), Int32Value(callInfo.argc()));
  builder->currentBlock()->add(argc);

  MDefinitionStackVector inputs;
  if (!inputs.append(argc)) {
    return false;
  }

  WarpCacheIRTranspiler transpiler(builder, loc, &callInfo, cacheIRSnapshot);
  if (!transpiler.transpile(inputs)) {
    return false;
  }

  auto maybeSetFlag = [numInstructionIdsBefore](MDefinition* def) {
    MaybeSetImplicitlyUsed(numInstructionIdsBefore, def);
  };
  callInfo.forEachCallOperand(maybeSetFlag);
  return true;
}
