/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/WarpCacheIRTranspiler.h"

#include "jit/CacheIR.h"
#include "jit/CacheIRCompiler.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/WarpSnapshot.h"

using namespace js;
using namespace js::jit;

// The CacheIR transpiler generates MIR from Baseline CacheIR.
class MOZ_RAII WarpCacheIRTranspiler {
  TempAllocator& alloc_;
  const CacheIRStubInfo* stubInfo_;
  const uint8_t* stubData_;
  TranspilerOutput& output_;

  // Vector mapping OperandId to corresponding MDefinition.
  MDefinitionStackVector operands_;

  CacheIRReader reader;
  MBasicBlock* current;

  TempAllocator& alloc() { return alloc_; }

  // CacheIR instructions writing to the IC's result register (the *Result
  // instructions) must call this to pass the corresponding MIR node back to
  // WarpBuilder.
  void setResult(MDefinition* result) {
    MOZ_ASSERT(!output_.result, "Can't have more than one result");
    output_.result = result;
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
  int32_t int32StubField(uint32_t offset) {
    return static_cast<int32_t>(readStubWord(offset));
  }

  bool transpileGuardTo(MIRType type);

  MInstruction* addBoundsCheck(MDefinition* index, MDefinition* length);

#define DEFINE_OP(op, ...) MOZ_MUST_USE bool transpile_##op();
  WARP_CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP

 public:
  WarpCacheIRTranspiler(MIRGenerator& mirGen, MBasicBlock* current,
                        const WarpCacheIR* snapshot, TranspilerOutput& output)
      : alloc_(mirGen.alloc()),
        stubInfo_(snapshot->stubInfo()),
        stubData_(snapshot->stubData()),
        output_(output),
        reader(stubInfo_),
        current(current) {}

  MOZ_MUST_USE bool transpile(const MDefinitionStackVector& inputs);
};

bool WarpCacheIRTranspiler::transpile(const MDefinitionStackVector& inputs) {
  if (!operands_.appendAll(inputs)) {
    return false;
  }

  do {
    CacheOp op = reader.readOp();
    switch (op) {
#define DEFINE_OP(op, ...)   \
  case CacheOp::op:          \
    if (!transpile_##op()) { \
      return false;          \
    }                        \
    break;
      WARP_CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP

      default:
        fprintf(stderr, "Unsupported op: %s\n", CacheIrOpNames[size_t(op)]);
        MOZ_CRASH("Unsupported op");
    }
  } while (reader.more());

  return true;
}

bool WarpCacheIRTranspiler::transpile_GuardClass() {
  ObjOperandId objId = reader.objOperandId();
  MDefinition* def = getOperand(objId);
  GuardClassKind classKind = reader.guardClassKind();

  const JSClass* classp = nullptr;
  switch (classKind) {
    case GuardClassKind::Array:
      classp = &ArrayObject::class_;
      break;
    default:
      MOZ_CRASH("not yet supported");
  }

  auto* ins = MGuardToClass::New(alloc(), def, classp);
  current->add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::transpile_GuardShape() {
  ObjOperandId objId = reader.objOperandId();
  MDefinition* def = getOperand(objId);
  Shape* shape = shapeStubField(reader.stubOffset());

  auto* ins = MGuardShape::New(alloc(), def, shape, Bailout_ShapeGuard);
  current->add(ins);

  setOperand(objId, ins);
  return true;
}

bool WarpCacheIRTranspiler::transpileGuardTo(MIRType type) {
  ValOperandId inputId = reader.valOperandId();

  MDefinition* def = getOperand(inputId);
  if (def->type() == type) {
    return true;
  }

  auto* ins = MUnbox::New(alloc(), def, type, MUnbox::Fallible);
  current->add(ins);

  setOperand(inputId, ins);
  return true;
}

bool WarpCacheIRTranspiler::transpile_GuardToObject() {
  return transpileGuardTo(MIRType::Object);
}

bool WarpCacheIRTranspiler::transpile_GuardToString() {
  return transpileGuardTo(MIRType::String);
}

bool WarpCacheIRTranspiler::transpile_GuardToInt32Index() {
  ValOperandId inputId = reader.valOperandId();
  Int32OperandId outputId = reader.int32OperandId();

  MDefinition* input = getOperand(inputId);
  auto* ins =
      MToNumberInt32::New(alloc(), input, IntConversionInputKind::NumbersOnly);
  current->add(ins);

  return defineOperand(outputId, ins);
}

bool WarpCacheIRTranspiler::transpile_LoadEnclosingEnvironment() {
  ObjOperandId inputId = reader.objOperandId();
  ObjOperandId outputId = reader.objOperandId();

  MDefinition* env = getOperand(inputId);
  auto* ins = MEnclosingEnvironment::New(alloc(), env);
  current->add(ins);

  return defineOperand(outputId, ins);
}

bool WarpCacheIRTranspiler::transpile_LoadDynamicSlotResult() {
  ObjOperandId objId = reader.objOperandId();
  int32_t offset = int32StubField(reader.stubOffset());

  MDefinition* obj = getOperand(objId);
  size_t slotIndex = NativeObject::getDynamicSlotIndexFromOffset(offset);

  auto* slots = MSlots::New(alloc(), obj);
  current->add(slots);

  auto* load = MLoadSlot::New(alloc(), slots, slotIndex);
  current->add(load);

  setResult(load);
  return true;
}

bool WarpCacheIRTranspiler::transpile_LoadFixedSlotResult() {
  ObjOperandId objId = reader.objOperandId();
  int32_t offset = int32StubField(reader.stubOffset());

  MDefinition* obj = getOperand(objId);
  uint32_t slotIndex = NativeObject::getFixedSlotIndexFromOffset(offset);

  auto* load = MLoadFixedSlot::New(alloc(), obj, slotIndex);
  current->add(load);

  setResult(load);
  return true;
}

bool WarpCacheIRTranspiler::transpile_LoadEnvironmentFixedSlotResult() {
  ObjOperandId objId = reader.objOperandId();
  int32_t offset = int32StubField(reader.stubOffset());

  MDefinition* obj = getOperand(objId);
  uint32_t slotIndex = NativeObject::getFixedSlotIndexFromOffset(offset);

  auto* load = MLoadFixedSlot::New(alloc(), obj, slotIndex);
  current->add(load);

  auto* lexicalCheck = MLexicalCheck::New(alloc(), load);
  current->add(lexicalCheck);

  setResult(lexicalCheck);
  return true;
}

bool WarpCacheIRTranspiler::transpile_LoadEnvironmentDynamicSlotResult() {
  ObjOperandId objId = reader.objOperandId();
  int32_t offset = int32StubField(reader.stubOffset());

  MDefinition* obj = getOperand(objId);
  size_t slotIndex = NativeObject::getDynamicSlotIndexFromOffset(offset);

  auto* slots = MSlots::New(alloc(), obj);
  current->add(slots);

  auto* load = MLoadSlot::New(alloc(), slots, slotIndex);
  current->add(load);

  auto* lexicalCheck = MLexicalCheck::New(alloc(), load);
  current->add(lexicalCheck);

  setResult(lexicalCheck);
  return true;
}

bool WarpCacheIRTranspiler::transpile_LoadInt32ArrayLengthResult() {
  ObjOperandId objId = reader.objOperandId();
  MDefinition* obj = getOperand(objId);

  auto* elements = MElements::New(alloc(), obj);
  current->add(elements);

  auto* length = MArrayLength::New(alloc(), elements);
  current->add(length);

  setResult(length);
  return true;
}

bool WarpCacheIRTranspiler::transpile_LoadStringLengthResult() {
  StringOperandId strId = reader.stringOperandId();
  MDefinition* str = getOperand(strId);

  auto* length = MStringLength::New(alloc(), str);
  current->add(length);

  setResult(length);
  return true;
}

MInstruction* WarpCacheIRTranspiler::addBoundsCheck(MDefinition* index,
                                                    MDefinition* length) {
  MInstruction* check = MBoundsCheck::New(alloc(), index, length);
  current->add(check);

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
    current->add(check);
  }

  return check;
}

bool WarpCacheIRTranspiler::transpile_LoadDenseElementResult() {
  ObjOperandId objId = reader.objOperandId();
  Int32OperandId indexId = reader.int32OperandId();
  MDefinition* obj = getOperand(objId);
  MDefinition* index = getOperand(indexId);

  auto* elements = MElements::New(alloc(), obj);
  current->add(elements);

  auto* length = MInitializedLength::New(alloc(), elements);
  current->add(length);

  index = addBoundsCheck(index, length);

  bool needsHoleCheck = true;
  bool loadDouble = false;  // TODO: Ion-only optimization.
  auto* load =
      MLoadElement::New(alloc(), elements, index, needsHoleCheck, loadDouble);
  current->add(load);

  setResult(load);
  return true;
}

bool WarpCacheIRTranspiler::transpile_LoadStringCharResult() {
  StringOperandId strId = reader.stringOperandId();
  Int32OperandId indexId = reader.int32OperandId();
  MDefinition* str = getOperand(strId);
  MDefinition* index = getOperand(indexId);

  auto* length = MStringLength::New(alloc(), str);
  current->add(length);

  index = addBoundsCheck(index, length);

  auto* charCode = MCharCodeAt::New(alloc(), str, index);
  current->add(charCode);

  auto* fromCharCode = MFromCharCode::New(alloc(), charCode);
  current->add(fromCharCode);

  setResult(fromCharCode);
  return true;
}

bool WarpCacheIRTranspiler::transpile_TypeMonitorResult() {
  MOZ_ASSERT(output_.result, "Didn't set result MDefinition");
  return true;
}

bool WarpCacheIRTranspiler::transpile_ReturnFromIC() { return true; }

bool jit::TranspileCacheIRToMIR(MIRGenerator& mirGen, MBasicBlock* current,
                                const WarpCacheIR* snapshot,
                                const MDefinitionStackVector& inputs,
                                TranspilerOutput& output) {
  WarpCacheIRTranspiler transpiler(mirGen, current, snapshot, output);
  return transpiler.transpile(inputs);
}
