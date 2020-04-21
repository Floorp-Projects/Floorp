/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonCacheIRCompiler.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"

#include <algorithm>

#include "jit/BaselineIC.h"
#include "jit/CacheIRCompiler.h"
#include "jit/IonIC.h"
#include "jit/JSJitFrameIter.h"
#include "jit/Linker.h"
#include "jit/SharedICHelpers.h"
#include "jit/VMFunctions.h"
#include "proxy/DeadObjectProxy.h"
#include "proxy/Proxy.h"
#include "util/Memory.h"

#include "jit/JSJitFrameIter-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "jit/VMFunctionList-inl.h"
#include "vm/Realm-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::Maybe;

namespace js {
namespace jit {

// IonCacheIRCompiler compiles CacheIR to IonIC native code.
IonCacheIRCompiler::IonCacheIRCompiler(
    JSContext* cx, const CacheIRWriter& writer, IonIC* ic, IonScript* ionScript,
    IonICStub* stub, const PropertyTypeCheckInfo* typeCheckInfo,
    uint32_t stubDataOffset)
    : CacheIRCompiler(cx, writer, stubDataOffset, Mode::Ion,
                      StubFieldPolicy::Constant),
      writer_(writer),
      ic_(ic),
      ionScript_(ionScript),
      stub_(stub),
      typeCheckInfo_(typeCheckInfo),
      savedLiveRegs_(false) {
  MOZ_ASSERT(ic_);
  MOZ_ASSERT(ionScript_);
}

template <typename T>
T IonCacheIRCompiler::rawWordStubField(uint32_t offset) {
  static_assert(sizeof(T) == sizeof(uintptr_t), "T must have word size");
  return (T)readStubWord(offset, StubField::Type::RawWord);
}
template <typename T>
T IonCacheIRCompiler::rawInt64StubField(uint32_t offset) {
  static_assert(sizeof(T) == sizeof(int64_t), "T musthave int64 size");
  return (T)readStubInt64(offset, StubField::Type::RawInt64);
}

uint64_t* IonCacheIRCompiler::expandoGenerationStubFieldPtr(uint32_t offset) {
  DebugOnly<uint64_t> generation =
      readStubInt64(offset, StubField::Type::DOMExpandoGeneration);
  uint64_t* ptr = reinterpret_cast<uint64_t*>(stub_->stubDataStart() + offset);
  MOZ_ASSERT(*ptr == generation);
  return ptr;
}

template <typename Fn, Fn fn>
void IonCacheIRCompiler::callVM(MacroAssembler& masm) {
  VMFunctionId id = VMFunctionToId<Fn, fn>::id;
  callVMInternal(masm, id);
}

bool IonCacheIRCompiler::needsPostBarrier() const {
  return ic_->asSetPropertyIC()->needsPostBarrier();
}

void IonCacheIRCompiler::pushStubCodePointer() {
  stubJitCodeOffset_.emplace(masm.PushWithPatch(ImmPtr((void*)-1)));
}

// AutoSaveLiveRegisters must be used when we make a call that can GC. The
// constructor ensures all live registers are stored on the stack (where the GC
// expects them) and the destructor restores these registers.
AutoSaveLiveRegisters::AutoSaveLiveRegisters(IonCacheIRCompiler& compiler)
    : compiler_(compiler) {
  MOZ_ASSERT(compiler_.liveRegs_.isSome());
  MOZ_ASSERT(compiler_.ic_);
  compiler_.allocator.saveIonLiveRegisters(
      compiler_.masm, compiler_.liveRegs_.ref(),
      compiler_.ic_->scratchRegisterForEntryJump(), compiler_.ionScript_);
  compiler_.savedLiveRegs_ = true;
}
AutoSaveLiveRegisters::~AutoSaveLiveRegisters() {
  MOZ_ASSERT(compiler_.stubJitCodeOffset_.isSome(),
             "Must have pushed JitCode* pointer");
  compiler_.allocator.restoreIonLiveRegisters(compiler_.masm,
                                              compiler_.liveRegs_.ref());
  MOZ_ASSERT(compiler_.masm.framePushed() == compiler_.ionScript_->frameSize());
}

}  // namespace jit
}  // namespace js

void CacheRegisterAllocator::saveIonLiveRegisters(MacroAssembler& masm,
                                                  LiveRegisterSet liveRegs,
                                                  Register scratch,
                                                  IonScript* ionScript) {
  // We have to push all registers in liveRegs on the stack. It's possible we
  // stored other values in our live registers and stored operands on the
  // stack (where our live registers should go), so this requires some careful
  // work. Try to keep it simple by taking one small step at a time.

  // Step 1. Discard any dead operands so we can reuse their registers.
  freeDeadOperandLocations(masm);

  // Step 2. Figure out the size of our live regs.
  size_t sizeOfLiveRegsInBytes = liveRegs.gprs().size() * sizeof(intptr_t) +
                                 liveRegs.fpus().getPushSizeInBytes();

  MOZ_ASSERT(sizeOfLiveRegsInBytes > 0);

  // Step 3. Ensure all non-input operands are on the stack.
  size_t numInputs = writer_.numInputOperands();
  for (size_t i = numInputs; i < operandLocations_.length(); i++) {
    OperandLocation& loc = operandLocations_[i];
    if (loc.isInRegister()) {
      spillOperandToStack(masm, &loc);
    }
  }

  // Step 4. Restore the register state, but don't discard the stack as
  // non-input operands are stored there.
  restoreInputState(masm, /* shouldDiscardStack = */ false);

  // We just restored the input state, so no input operands should be stored
  // on the stack.
#ifdef DEBUG
  for (size_t i = 0; i < numInputs; i++) {
    const OperandLocation& loc = operandLocations_[i];
    MOZ_ASSERT(!loc.isOnStack());
  }
#endif

  // Step 5. At this point our register state is correct. Stack values,
  // however, may cover the space where we have to store the live registers.
  // Move them out of the way.

  bool hasOperandOnStack = false;
  for (size_t i = numInputs; i < operandLocations_.length(); i++) {
    OperandLocation& loc = operandLocations_[i];
    if (!loc.isOnStack()) {
      continue;
    }

    hasOperandOnStack = true;

    size_t operandSize = loc.stackSizeInBytes();
    size_t operandStackPushed = loc.stackPushed();
    MOZ_ASSERT(operandSize > 0);
    MOZ_ASSERT(stackPushed_ >= operandStackPushed);
    MOZ_ASSERT(operandStackPushed >= operandSize);

    // If this operand doesn't cover the live register space, there's
    // nothing to do.
    if (operandStackPushed - operandSize >= sizeOfLiveRegsInBytes) {
      MOZ_ASSERT(stackPushed_ > sizeOfLiveRegsInBytes);
      continue;
    }

    // Reserve stack space for the live registers if needed.
    if (sizeOfLiveRegsInBytes > stackPushed_) {
      size_t extraBytes = sizeOfLiveRegsInBytes - stackPushed_;
      MOZ_ASSERT((extraBytes % sizeof(uintptr_t)) == 0);
      masm.subFromStackPtr(Imm32(extraBytes));
      stackPushed_ += extraBytes;
    }

    // Push the operand below the live register space.
    if (loc.kind() == OperandLocation::PayloadStack) {
      masm.push(
          Address(masm.getStackPointer(), stackPushed_ - operandStackPushed));
      stackPushed_ += operandSize;
      loc.setPayloadStack(stackPushed_, loc.payloadType());
      continue;
    }
    MOZ_ASSERT(loc.kind() == OperandLocation::ValueStack);
    masm.pushValue(
        Address(masm.getStackPointer(), stackPushed_ - operandStackPushed));
    stackPushed_ += operandSize;
    loc.setValueStack(stackPushed_);
  }

  // Step 6. If we have any operands on the stack, adjust their stackPushed
  // values to not include sizeOfLiveRegsInBytes (this simplifies code down
  // the line). Then push/store the live registers.
  if (hasOperandOnStack) {
    MOZ_ASSERT(stackPushed_ > sizeOfLiveRegsInBytes);
    stackPushed_ -= sizeOfLiveRegsInBytes;

    for (size_t i = numInputs; i < operandLocations_.length(); i++) {
      OperandLocation& loc = operandLocations_[i];
      if (loc.isOnStack()) {
        loc.adjustStackPushed(-int32_t(sizeOfLiveRegsInBytes));
      }
    }

    size_t stackBottom = stackPushed_ + sizeOfLiveRegsInBytes;
    masm.storeRegsInMask(liveRegs, Address(masm.getStackPointer(), stackBottom),
                         scratch);
    masm.setFramePushed(masm.framePushed() + sizeOfLiveRegsInBytes);
  } else {
    // If no operands are on the stack, discard the unused stack space.
    if (stackPushed_ > 0) {
      masm.addToStackPtr(Imm32(stackPushed_));
      stackPushed_ = 0;
    }
    masm.PushRegsInMask(liveRegs);
  }
  freePayloadSlots_.clear();
  freeValueSlots_.clear();

  MOZ_ASSERT(masm.framePushed() ==
             ionScript->frameSize() + sizeOfLiveRegsInBytes);

  // Step 7. All live registers and non-input operands are stored on the stack
  // now, so at this point all registers except for the input registers are
  // available.
  availableRegs_.set() = GeneralRegisterSet::Not(inputRegisterSet());
  availableRegsAfterSpill_.set() = GeneralRegisterSet();

  // Step 8. We restored our input state, so we have to fix up aliased input
  // registers again.
  fixupAliasedInputs(masm);
}

void CacheRegisterAllocator::restoreIonLiveRegisters(MacroAssembler& masm,
                                                     LiveRegisterSet liveRegs) {
  masm.PopRegsInMask(liveRegs);

  availableRegs_.set() = GeneralRegisterSet();
  availableRegsAfterSpill_.set() = GeneralRegisterSet::All();
}

static void* GetReturnAddressToIonCode(JSContext* cx) {
  JSJitFrameIter frame(cx->activation()->asJit());
  MOZ_ASSERT(frame.type() == FrameType::Exit,
             "An exit frame is expected as update functions are called with a "
             "VMFunction.");

  void* returnAddr = frame.returnAddress();
#ifdef DEBUG
  ++frame;
  MOZ_ASSERT(frame.isIonJS());
#endif
  return returnAddr;
}

// The AutoSaveLiveRegisters parameter is used to ensure registers were saved
void IonCacheIRCompiler::prepareVMCall(MacroAssembler& masm,
                                       const AutoSaveLiveRegisters&) {
  uint32_t descriptor = MakeFrameDescriptor(
      masm.framePushed(), FrameType::IonJS, IonICCallFrameLayout::Size());
  pushStubCodePointer();
  masm.Push(Imm32(descriptor));
  masm.Push(ImmPtr(GetReturnAddressToIonCode(cx_)));

  preparedForVMCall_ = true;
}

bool IonCacheIRCompiler::init() {
  if (!allocator.init()) {
    return false;
  }

  size_t numInputs = writer_.numInputOperands();

  AllocatableGeneralRegisterSet available;

  switch (ic_->kind()) {
    case CacheKind::GetProp:
    case CacheKind::GetElem: {
      IonGetPropertyIC* ic = ic_->asGetPropertyIC();
      TypedOrValueRegister output = ic->output();

      if (output.hasValue()) {
        available.add(output.valueReg());
      } else if (!output.typedReg().isFloat()) {
        available.add(output.typedReg().gpr());
      }

      if (ic->maybeTemp() != InvalidReg) {
        available.add(ic->maybeTemp());
      }

      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(output);

      allowDoubleResult_.emplace(ic->allowDoubleResult());

      MOZ_ASSERT(numInputs == 1 || numInputs == 2);

      allocator.initInputLocation(0, ic->value());
      if (numInputs > 1) {
        allocator.initInputLocation(1, ic->id());
      }
      break;
    }
    case CacheKind::GetPropSuper:
    case CacheKind::GetElemSuper: {
      IonGetPropSuperIC* ic = ic_->asGetPropSuperIC();
      TypedOrValueRegister output = ic->output();

      available.add(output.valueReg());

      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(output);

      allowDoubleResult_.emplace(true);

      MOZ_ASSERT(numInputs == 2 || numInputs == 3);

      allocator.initInputLocation(0, ic->object(), JSVAL_TYPE_OBJECT);

      if (ic->kind() == CacheKind::GetPropSuper) {
        MOZ_ASSERT(numInputs == 2);
        allocator.initInputLocation(1, ic->receiver());
      } else {
        MOZ_ASSERT(numInputs == 3);
        allocator.initInputLocation(1, ic->id());
        allocator.initInputLocation(2, ic->receiver());
      }
      break;
    }
    case CacheKind::SetProp:
    case CacheKind::SetElem: {
      IonSetPropertyIC* ic = ic_->asSetPropertyIC();

      available.add(ic->temp());

      liveRegs_.emplace(ic->liveRegs());

      allocator.initInputLocation(0, ic->object(), JSVAL_TYPE_OBJECT);

      if (ic->kind() == CacheKind::SetProp) {
        MOZ_ASSERT(numInputs == 2);
        allocator.initInputLocation(1, ic->rhs());
      } else {
        MOZ_ASSERT(numInputs == 3);
        allocator.initInputLocation(1, ic->id());
        allocator.initInputLocation(2, ic->rhs());
      }
      break;
    }
    case CacheKind::GetName: {
      IonGetNameIC* ic = ic_->asGetNameIC();
      ValueOperand output = ic->output();

      available.add(output);
      available.add(ic->temp());

      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(output);

      MOZ_ASSERT(numInputs == 1);
      allocator.initInputLocation(0, ic->environment(), JSVAL_TYPE_OBJECT);
      break;
    }
    case CacheKind::BindName: {
      IonBindNameIC* ic = ic_->asBindNameIC();
      Register output = ic->output();

      available.add(output);
      available.add(ic->temp());

      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(
          TypedOrValueRegister(MIRType::Object, AnyRegister(output)));

      MOZ_ASSERT(numInputs == 1);
      allocator.initInputLocation(0, ic->environment(), JSVAL_TYPE_OBJECT);
      break;
    }
    case CacheKind::GetIterator: {
      IonGetIteratorIC* ic = ic_->asGetIteratorIC();
      Register output = ic->output();

      available.add(output);
      available.add(ic->temp1());
      available.add(ic->temp2());

      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(
          TypedOrValueRegister(MIRType::Object, AnyRegister(output)));

      MOZ_ASSERT(numInputs == 1);
      allocator.initInputLocation(0, ic->value());
      break;
    }
    case CacheKind::In: {
      IonInIC* ic = ic_->asInIC();
      Register output = ic->output();

      available.add(output);

      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(
          TypedOrValueRegister(MIRType::Boolean, AnyRegister(output)));

      MOZ_ASSERT(numInputs == 2);
      allocator.initInputLocation(0, ic->key());
      allocator.initInputLocation(
          1, TypedOrValueRegister(MIRType::Object, AnyRegister(ic->object())));
      break;
    }
    case CacheKind::HasOwn: {
      IonHasOwnIC* ic = ic_->asHasOwnIC();
      Register output = ic->output();

      available.add(output);

      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(
          TypedOrValueRegister(MIRType::Boolean, AnyRegister(output)));

      MOZ_ASSERT(numInputs == 2);
      allocator.initInputLocation(0, ic->id());
      allocator.initInputLocation(1, ic->value());
      break;
    }
    case CacheKind::InstanceOf: {
      IonInstanceOfIC* ic = ic_->asInstanceOfIC();
      Register output = ic->output();
      available.add(output);
      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(
          TypedOrValueRegister(MIRType::Boolean, AnyRegister(output)));

      MOZ_ASSERT(numInputs == 2);
      allocator.initInputLocation(0, ic->lhs());
      allocator.initInputLocation(
          1, TypedOrValueRegister(MIRType::Object, AnyRegister(ic->rhs())));
      break;
    }
    case CacheKind::UnaryArith: {
      IonUnaryArithIC* ic = ic_->asUnaryArithIC();
      ValueOperand output = ic->output();

      available.add(output);

      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(TypedOrValueRegister(output));

      MOZ_ASSERT(numInputs == 1);
      allocator.initInputLocation(0, ic->input());
      break;
    }
    case CacheKind::BinaryArith: {
      IonBinaryArithIC* ic = ic_->asBinaryArithIC();
      ValueOperand output = ic->output();

      available.add(output);

      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(TypedOrValueRegister(output));

      MOZ_ASSERT(numInputs == 2);
      allocator.initInputLocation(0, ic->lhs());
      allocator.initInputLocation(1, ic->rhs());
      break;
    }
    case CacheKind::Compare: {
      IonCompareIC* ic = ic_->asCompareIC();
      Register output = ic->output();

      available.add(output);

      liveRegs_.emplace(ic->liveRegs());
      outputUnchecked_.emplace(
          TypedOrValueRegister(MIRType::Boolean, AnyRegister(output)));

      MOZ_ASSERT(numInputs == 2);
      allocator.initInputLocation(0, ic->lhs());
      allocator.initInputLocation(1, ic->rhs());
      break;
    }
    case CacheKind::Call:
    case CacheKind::TypeOf:
    case CacheKind::ToBool:
    case CacheKind::GetIntrinsic:
    case CacheKind::NewObject:
      MOZ_CRASH("Unsupported IC");
  }

  if (liveRegs_) {
    liveFloatRegs_ = LiveFloatRegisterSet(liveRegs_->fpus());
  }

  allocator.initAvailableRegs(available);
  allocator.initAvailableRegsAfterSpill();
  return true;
}

JitCode* IonCacheIRCompiler::compile() {
  masm.setFramePushed(ionScript_->frameSize());
  if (cx_->runtime()->geckoProfiler().enabled()) {
    masm.enableProfilingInstrumentation();
  }

  allocator.fixupAliasedInputs(masm);

  do {
    switch (reader.readOp()) {
#define DEFINE_OP(op, ...)           \
  case CacheOp::op:                  \
    if (!emit##op()) return nullptr; \
    break;
      CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP

      default:
        MOZ_CRASH("Invalid op");
    }
#ifdef DEBUG
    assertAllArgumentsConsumed();
#endif
    allocator.nextOp();
  } while (reader.more());

  masm.assumeUnreachable("Should have returned from IC");

  // Done emitting the main IC code. Now emit the failure paths.
  for (size_t i = 0; i < failurePaths.length(); i++) {
    if (!emitFailurePath(i)) {
      return nullptr;
    }
    Register scratch = ic_->scratchRegisterForEntryJump();
    CodeOffset offset = masm.movWithPatch(ImmWord(-1), scratch);
    masm.jump(Address(scratch, 0));
    if (!nextCodeOffsets_.append(offset)) {
      return nullptr;
    }
  }

  Linker linker(masm);
  Rooted<JitCode*> newStubCode(cx_, linker.newCode(cx_, CodeKind::Ion));
  if (!newStubCode) {
    cx_->recoverFromOutOfMemory();
    return nullptr;
  }

  for (CodeOffset offset : nextCodeOffsets_) {
    Assembler::PatchDataWithValueCheck(CodeLocationLabel(newStubCode, offset),
                                       ImmPtr(stub_->nextCodeRawPtr()),
                                       ImmPtr((void*)-1));
  }
  if (stubJitCodeOffset_) {
    Assembler::PatchDataWithValueCheck(
        CodeLocationLabel(newStubCode, *stubJitCodeOffset_),
        ImmPtr(newStubCode.get()), ImmPtr((void*)-1));
  }

  return newStubCode;
}

bool IonCacheIRCompiler::emitGuardShape(ObjOperandId objId,
                                        uint32_t shapeOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  Shape* shape = shapeStubField(shapeOffset);

  bool needSpectreMitigations = objectGuardNeedsSpectreMitigations(objId);

  Maybe<AutoScratchRegister> maybeScratch;
  if (needSpectreMitigations) {
    maybeScratch.emplace(allocator, masm);
  }

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  if (needSpectreMitigations) {
    masm.branchTestObjShape(Assembler::NotEqual, obj, shape, *maybeScratch, obj,
                            failure->label());
  } else {
    masm.branchTestObjShapeNoSpectreMitigations(Assembler::NotEqual, obj, shape,
                                                failure->label());
  }

  return true;
}

bool IonCacheIRCompiler::emitGuardGroup(ObjOperandId objId,
                                        uint32_t groupOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  ObjectGroup* group = groupStubField(groupOffset);

  bool needSpectreMitigations = objectGuardNeedsSpectreMitigations(objId);

  Maybe<AutoScratchRegister> maybeScratch;
  if (needSpectreMitigations) {
    maybeScratch.emplace(allocator, masm);
  }

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  if (needSpectreMitigations) {
    masm.branchTestObjGroup(Assembler::NotEqual, obj, group, *maybeScratch, obj,
                            failure->label());
  } else {
    masm.branchTestObjGroupNoSpectreMitigations(Assembler::NotEqual, obj, group,
                                                failure->label());
  }

  return true;
}

bool IonCacheIRCompiler::emitGuardProto(ObjOperandId objId,
                                        uint32_t protoOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  JSObject* proto = objectStubField(protoOffset);

  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.loadObjProto(obj, scratch);
  masm.branchPtr(Assembler::NotEqual, scratch, ImmGCPtr(proto),
                 failure->label());
  return true;
}

bool IonCacheIRCompiler::emitGuardCompartment(ObjOperandId objId,
                                              uint32_t globalOffset,
                                              uint32_t compartmentOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  JSObject* globalWrapper = objectStubField(globalOffset);
  JS::Compartment* compartment = compartmentStubField(compartmentOffset);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Verify that the global wrapper is still valid, as
  // it is pre-requisite for doing the compartment check.
  masm.movePtr(ImmGCPtr(globalWrapper), scratch);
  Address handlerAddr(scratch, ProxyObject::offsetOfHandler());
  masm.branchPtr(Assembler::Equal, handlerAddr,
                 ImmPtr(&DeadObjectProxy::singleton), failure->label());

  masm.branchTestObjCompartment(Assembler::NotEqual, obj, compartment, scratch,
                                failure->label());
  return true;
}

bool IonCacheIRCompiler::emitGuardAnyClass(ObjOperandId objId,
                                           uint32_t claspOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  AutoScratchRegister scratch(allocator, masm);

  const JSClass* clasp = classStubField(claspOffset);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  if (objectGuardNeedsSpectreMitigations(objId)) {
    masm.branchTestObjClass(Assembler::NotEqual, obj, clasp, scratch, obj,
                            failure->label());
  } else {
    masm.branchTestObjClassNoSpectreMitigations(Assembler::NotEqual, obj, clasp,
                                                scratch, failure->label());
  }

  return true;
}

bool IonCacheIRCompiler::emitGuardHasProxyHandler(ObjOperandId objId,
                                                  uint32_t handlerOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  const void* handler = proxyHandlerStubField(handlerOffset);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Address handlerAddr(obj, ProxyObject::offsetOfHandler());
  masm.branchPtr(Assembler::NotEqual, handlerAddr, ImmPtr(handler),
                 failure->label());
  return true;
}

bool IonCacheIRCompiler::emitGuardSpecificObject() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  JSObject* expected = objectStubField(reader.stubOffset());

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchPtr(Assembler::NotEqual, obj, ImmGCPtr(expected),
                 failure->label());
  return true;
}

bool IonCacheIRCompiler::emitGuardSpecificAtom(StringOperandId strId,
                                               uint32_t expectedOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register str = allocator.useRegister(masm, strId);
  AutoScratchRegister scratch(allocator, masm);

  JSAtom* atom = &stringStubField(expectedOffset)->asAtom();

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label done;
  masm.branchPtr(Assembler::Equal, str, ImmGCPtr(atom), &done);

  // The pointers are not equal, so if the input string is also an atom it
  // must be a different string.
  masm.branchTest32(Assembler::Zero, Address(str, JSString::offsetOfFlags()),
                    Imm32(JSString::NON_ATOM_BIT), failure->label());

  // Check the length.
  masm.branch32(Assembler::NotEqual, Address(str, JSString::offsetOfLength()),
                Imm32(atom->length()), failure->label());

  // We have a non-atomized string with the same length. Call a helper
  // function to do the comparison.
  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  masm.PushRegsInMask(volatileRegs);

  masm.setupUnalignedABICall(scratch);
  masm.movePtr(ImmGCPtr(atom), scratch);
  masm.passABIArg(scratch);
  masm.passABIArg(str);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, EqualStringsHelperPure));
  masm.mov(ReturnReg, scratch);

  LiveRegisterSet ignore;
  ignore.add(scratch);
  masm.PopRegsInMaskIgnore(volatileRegs, ignore);
  masm.branchIfFalseBool(scratch, failure->label());

  masm.bind(&done);
  return true;
}

bool IonCacheIRCompiler::emitGuardSpecificSymbol(SymbolOperandId symId,
                                                 uint32_t expectedOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register sym = allocator.useRegister(masm, symId);
  JS::Symbol* expected = symbolStubField(expectedOffset);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchPtr(Assembler::NotEqual, sym, ImmGCPtr(expected),
                 failure->label());
  return true;
}

bool IonCacheIRCompiler::emitLoadValueResult(uint32_t valOffset) {
  MOZ_CRASH("Baseline-specific op");
}

bool IonCacheIRCompiler::emitLoadFixedSlotResult(ObjOperandId objId,
                                                 uint32_t offsetOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, objId);
  int32_t offset = int32StubField(offsetOffset);
  masm.loadTypedOrValue(Address(obj, offset), output);
  return true;
}

bool IonCacheIRCompiler::emitLoadDynamicSlotResult(ObjOperandId objId,
                                                   uint32_t offsetOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, objId);
  int32_t offset = int32StubField(offsetOffset);

  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);
  masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), scratch);
  masm.loadTypedOrValue(Address(scratch, offset), output);
  return true;
}

bool IonCacheIRCompiler::emitGuardHasGetterSetter(ObjOperandId objId,
                                                  uint32_t shapeOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  Shape* shape = shapeStubField(shapeOffset);

  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegister scratch2(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  volatileRegs.takeUnchecked(scratch1);
  volatileRegs.takeUnchecked(scratch2);
  masm.PushRegsInMask(volatileRegs);

  masm.setupUnalignedABICall(scratch1);
  masm.loadJSContext(scratch1);
  masm.passABIArg(scratch1);
  masm.passABIArg(obj);
  masm.movePtr(ImmGCPtr(shape), scratch2);
  masm.passABIArg(scratch2);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, ObjectHasGetterSetterPure));
  masm.mov(ReturnReg, scratch1);
  masm.PopRegsInMask(volatileRegs);

  masm.branchIfFalseBool(scratch1, failure->label());
  return true;
}

bool IonCacheIRCompiler::emitCallScriptedGetterResultShared(
    TypedOrValueRegister receiver, uint32_t getterOffset, bool sameRealm,
    TypedOrValueRegister output) {
  JSFunction* target = &objectStubField(getterOffset)->as<JSFunction>();
  AutoScratchRegister scratch(allocator, masm);

  MOZ_ASSERT(sameRealm == (cx_->realm() == target->realm()));

  allocator.discardStack(masm);

  uint32_t framePushedBefore = masm.framePushed();

  // Construct IonICCallFrameLayout.
  uint32_t descriptor = MakeFrameDescriptor(
      masm.framePushed(), FrameType::IonJS, IonICCallFrameLayout::Size());
  pushStubCodePointer();
  masm.Push(Imm32(descriptor));
  masm.Push(ImmPtr(GetReturnAddressToIonCode(cx_)));

  // The JitFrameLayout pushed below will be aligned to JitStackAlignment,
  // so we just have to make sure the stack is aligned after we push the
  // |this| + argument Values.
  uint32_t argSize = (target->nargs() + 1) * sizeof(Value);
  uint32_t padding =
      ComputeByteAlignment(masm.framePushed() + argSize, JitStackAlignment);
  MOZ_ASSERT(padding % sizeof(uintptr_t) == 0);
  MOZ_ASSERT(padding < JitStackAlignment);
  masm.reserveStack(padding);

  for (size_t i = 0; i < target->nargs(); i++) {
    masm.Push(UndefinedValue());
  }
  masm.Push(receiver);

  if (!sameRealm) {
    masm.switchToRealm(target->realm(), scratch);
  }

  masm.movePtr(ImmGCPtr(target), scratch);

  descriptor = MakeFrameDescriptor(argSize + padding, FrameType::IonICCall,
                                   JitFrameLayout::Size());
  masm.Push(Imm32(0));  // argc
  masm.Push(scratch);
  masm.Push(Imm32(descriptor));

  // Check stack alignment. Add sizeof(uintptr_t) for the return address.
  MOZ_ASSERT(((masm.framePushed() + sizeof(uintptr_t)) % JitStackAlignment) ==
             0);

  MOZ_ASSERT(target->hasJitEntry());
  masm.loadJitCodeRaw(scratch, scratch);
  masm.callJit(scratch);

  if (!sameRealm) {
    static_assert(!JSReturnOperand.aliases(ReturnReg),
                  "ReturnReg available as scratch after scripted calls");
    masm.switchToRealm(cx_->realm(), ReturnReg);
  }

  masm.storeCallResultValue(output);
  masm.freeStack(masm.framePushed() - framePushedBefore);
  return true;
}

bool IonCacheIRCompiler::emitCallScriptedGetterResult(ObjOperandId objId,
                                                      uint32_t getterOffset,
                                                      bool sameRealm) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);
  AutoOutputRegister output(*this);

  Register obj = allocator.useRegister(masm, objId);

  return emitCallScriptedGetterResultShared(
      TypedOrValueRegister(MIRType::Object, AnyRegister(obj)), getterOffset,
      sameRealm, output);
}

bool IonCacheIRCompiler::emitCallScriptedGetterByValueResult(
    ValOperandId valId, uint32_t getterOffset, bool sameRealm) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);
  AutoOutputRegister output(*this);

  ValueOperand val = allocator.useValueRegister(masm, valId);

  return emitCallScriptedGetterResultShared(val, getterOffset, sameRealm,
                                            output);
}

bool IonCacheIRCompiler::emitCallNativeGetterResultShared(
    TypedOrValueRegister receiver, uint32_t getterOffset,
    const AutoOutputRegister& output, AutoSaveLiveRegisters& save) {
  JSFunction* target = &objectStubField(getterOffset)->as<JSFunction>();
  MOZ_ASSERT(target->isNative());

  AutoScratchRegisterMaybeOutput argJSContext(allocator, masm, output);
  AutoScratchRegister argUintN(allocator, masm);
  AutoScratchRegister argVp(allocator, masm);
  AutoScratchRegister scratch(allocator, masm);

  allocator.discardStack(masm);

  // Native functions have the signature:
  //  bool (*)(JSContext*, unsigned, Value* vp)
  // Where vp[0] is space for an outparam, vp[1] is |this|, and vp[2] onward
  // are the function arguments.

  // Construct vp array:
  // Push receiver value for |this|
  masm.Push(receiver);
  // Push callee/outparam.
  masm.Push(ObjectValue(*target));

  // Preload arguments into registers.
  masm.loadJSContext(argJSContext);
  masm.move32(Imm32(0), argUintN);
  masm.moveStackPtrTo(argVp.get());

  // Push marking data for later use.
  masm.Push(argUintN);
  pushStubCodePointer();

  if (!masm.icBuildOOLFakeExitFrame(GetReturnAddressToIonCode(cx_), save)) {
    return false;
  }
  masm.enterFakeExitFrame(argJSContext, scratch, ExitFrameType::IonOOLNative);

  if (target->realm() != cx_->realm()) {
    masm.switchToRealm(target->realm(), scratch);
  }

  // Construct and execute call.
  masm.setupUnalignedABICall(scratch);
  masm.passABIArg(argJSContext);
  masm.passABIArg(argUintN);
  masm.passABIArg(argVp);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, target->native()),
                   MoveOp::GENERAL,
                   CheckUnsafeCallWithABI::DontCheckHasExitFrame);

  // Test for failure.
  masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

  if (target->realm() != cx_->realm()) {
    masm.switchToRealm(cx_->realm(), ReturnReg);
  }

  // Load the outparam vp[0] into output register(s).
  Address outparam(masm.getStackPointer(),
                   IonOOLNativeExitFrameLayout::offsetOfResult());
  masm.loadValue(outparam, output.valueReg());

  if (JitOptions.spectreJitToCxxCalls) {
    masm.speculationBarrier();
  }

  masm.adjustStack(IonOOLNativeExitFrameLayout::Size(0));
  return true;
}

bool IonCacheIRCompiler::emitCallNativeGetterResult(ObjOperandId objId,
                                                    uint32_t getterOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);
  AutoOutputRegister output(*this);

  Register obj = allocator.useRegister(masm, objId);

  return emitCallNativeGetterResultShared(
      TypedOrValueRegister(MIRType::Object, AnyRegister(obj)), getterOffset,
      output, save);
}

bool IonCacheIRCompiler::emitCallNativeGetterByValueResult(
    ValOperandId valId, uint32_t getterOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);
  AutoOutputRegister output(*this);

  ValueOperand val = allocator.useValueRegister(masm, valId);

  return emitCallNativeGetterResultShared(val, getterOffset, output, save);
}

bool IonCacheIRCompiler::emitCallProxyGetResult(ObjOperandId objId,
                                                uint32_t idOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);
  AutoOutputRegister output(*this);

  Register obj = allocator.useRegister(masm, objId);
  jsid id = idStubField(idOffset);

  // ProxyGetProperty(JSContext* cx, HandleObject proxy, HandleId id,
  //                  MutableHandleValue vp)
  AutoScratchRegisterMaybeOutput argJSContext(allocator, masm, output);
  AutoScratchRegister argProxy(allocator, masm);
  AutoScratchRegister argId(allocator, masm);
  AutoScratchRegister argVp(allocator, masm);
  AutoScratchRegister scratch(allocator, masm);

  allocator.discardStack(masm);

  // Push stubCode for marking.
  pushStubCodePointer();

  // Push args on stack first so we can take pointers to make handles.
  masm.Push(UndefinedValue());
  masm.moveStackPtrTo(argVp.get());

  masm.Push(id, scratch);
  masm.moveStackPtrTo(argId.get());

  // Push the proxy. Also used as receiver.
  masm.Push(obj);
  masm.moveStackPtrTo(argProxy.get());

  masm.loadJSContext(argJSContext);

  if (!masm.icBuildOOLFakeExitFrame(GetReturnAddressToIonCode(cx_), save)) {
    return false;
  }
  masm.enterFakeExitFrame(argJSContext, scratch, ExitFrameType::IonOOLProxy);

  // Make the call.
  masm.setupUnalignedABICall(scratch);
  masm.passABIArg(argJSContext);
  masm.passABIArg(argProxy);
  masm.passABIArg(argId);
  masm.passABIArg(argVp);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, ProxyGetProperty),
                   MoveOp::GENERAL,
                   CheckUnsafeCallWithABI::DontCheckHasExitFrame);

  // Test for failure.
  masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

  // Load the outparam vp[0] into output register(s).
  Address outparam(masm.getStackPointer(),
                   IonOOLProxyExitFrameLayout::offsetOfResult());
  masm.loadValue(outparam, output.valueReg());

  // Spectre mitigation in case of speculative execution within C++ code.
  if (JitOptions.spectreJitToCxxCalls) {
    masm.speculationBarrier();
  }

  // masm.leaveExitFrame & pop locals
  masm.adjustStack(IonOOLProxyExitFrameLayout::Size());
  return true;
}

bool IonCacheIRCompiler::emitGuardFrameHasNoArgumentsObject() {
  MOZ_CRASH("Baseline-specific op");
}

bool IonCacheIRCompiler::emitLoadFrameCalleeResult() {
  MOZ_CRASH("Baseline-specific op");
}

bool IonCacheIRCompiler::emitLoadFrameNumActualArgsResult() {
  MOZ_CRASH("Baseline-specific op");
}

bool IonCacheIRCompiler::emitLoadFrameArgumentResult(Int32OperandId indexId) {
  MOZ_CRASH("Baseline-specific op");
}

bool IonCacheIRCompiler::emitLoadEnvironmentFixedSlotResult(
    ObjOperandId objId, uint32_t offsetOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, objId);
  int32_t offset = int32StubField(offsetOffset);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Check for uninitialized lexicals.
  Address slot(obj, offset);
  masm.branchTestMagic(Assembler::Equal, slot, failure->label());

  // Load the value.
  masm.loadTypedOrValue(slot, output);
  return true;
}

bool IonCacheIRCompiler::emitLoadEnvironmentDynamicSlotResult(
    ObjOperandId objId, uint32_t offsetOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, objId);
  int32_t offset = int32StubField(offsetOffset);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), scratch);

  // Check for uninitialized lexicals.
  Address slot(scratch, offset);
  masm.branchTestMagic(Assembler::Equal, slot, failure->label());

  // Load the value.
  masm.loadTypedOrValue(slot, output);
  return true;
}

bool IonCacheIRCompiler::emitLoadStringResult(uint32_t strOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  MOZ_CRASH("not used in ion");
}

bool IonCacheIRCompiler::emitCompareStringResult(JSOp op, StringOperandId lhsId,
                                                 StringOperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);
  AutoOutputRegister output(*this);

  Register left = allocator.useRegister(masm, lhsId);
  Register right = allocator.useRegister(masm, rhsId);

  allocator.discardStack(masm);

  Label slow, done;
  MOZ_ASSERT(!output.hasValue());
  masm.compareStrings(op, left, right, output.typedReg().gpr(), &slow);

  masm.jump(&done);
  masm.bind(&slow);

  prepareVMCall(masm, save);

  // Push the operands in reverse order for JSOp::Le and JSOp::Gt:
  // - |left <= right| is implemented as |right >= left|.
  // - |left > right| is implemented as |right < left|.
  if (op == JSOp::Le || op == JSOp::Gt) {
    masm.Push(left);
    masm.Push(right);
  } else {
    masm.Push(right);
    masm.Push(left);
  }

  using Fn = bool (*)(JSContext*, HandleString, HandleString, bool*);
  if (op == JSOp::Eq || op == JSOp::StrictEq) {
    callVM<Fn, jit::StringsEqual<EqualityKind::Equal>>(masm);
  } else if (op == JSOp::Ne || op == JSOp::StrictNe) {
    callVM<Fn, jit::StringsEqual<EqualityKind::NotEqual>>(masm);
  } else if (op == JSOp::Lt || op == JSOp::Gt) {
    callVM<Fn, jit::StringsCompare<ComparisonKind::LessThan>>(masm);
  } else {
    MOZ_ASSERT(op == JSOp::Le || op == JSOp::Ge);
    callVM<Fn, jit::StringsCompare<ComparisonKind::GreaterThanOrEqual>>(masm);
  }

  masm.storeCallBoolResult(output.typedReg().gpr());
  masm.bind(&done);
  return true;
}

static bool GroupHasPropertyTypes(ObjectGroup* group, jsid* id, Value* v) {
  AutoUnsafeCallWithABI unsafe;
  if (group->unknownPropertiesDontCheckGeneration()) {
    return true;
  }
  HeapTypeSet* propTypes = group->maybeGetPropertyDontCheckGeneration(*id);
  if (!propTypes) {
    return true;
  }
  if (!propTypes->nonConstantProperty()) {
    return false;
  }
  return propTypes->hasType(TypeSet::GetValueType(*v));
}

static void EmitCheckPropertyTypes(MacroAssembler& masm,
                                   const PropertyTypeCheckInfo* typeCheckInfo,
                                   Register obj, const ConstantOrRegister& val,
                                   const LiveRegisterSet& liveRegs,
                                   Label* failures) {
  // Emit code to check |val| is part of the property's HeapTypeSet.

  if (!typeCheckInfo->isSet()) {
    return;
  }

  ObjectGroup* group = typeCheckInfo->group();
  AutoSweepObjectGroup sweep(group);
  if (group->unknownProperties(sweep)) {
    return;
  }

  jsid id = typeCheckInfo->id();
  HeapTypeSet* propTypes = group->maybeGetProperty(sweep, id);
  if (propTypes && propTypes->unknown()) {
    return;
  }

  // Use the object register as scratch, as we don't need it here.
  masm.Push(obj);
  Register scratch1 = obj;

  // We may also need a scratch register for guardTypeSet. Additionally,
  // spectreRegToZero is the register that may be zeroed on speculatively
  // executed paths.
  Register objScratch = InvalidReg;
  Register spectreRegToZero = InvalidReg;
  if (propTypes && !propTypes->unknownObject() &&
      propTypes->getObjectCount() > 0) {
    AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
    if (!val.constant()) {
      TypedOrValueRegister valReg = val.reg();
      if (valReg.hasValue()) {
        regs.take(valReg.valueReg());
        spectreRegToZero = valReg.valueReg().payloadOrValueReg();
      } else if (!valReg.typedReg().isFloat()) {
        regs.take(valReg.typedReg().gpr());
        spectreRegToZero = valReg.typedReg().gpr();
      }
    }
    regs.take(scratch1);
    objScratch = regs.takeAny();
    masm.Push(objScratch);
  }

  bool checkTypeSet = true;
  Label failedFastPath;

  if (propTypes && !propTypes->nonConstantProperty()) {
    masm.jump(&failedFastPath);
  }

  if (val.constant()) {
    // If the input is a constant, then don't bother if the barrier will always
    // fail.
    if (!propTypes || !propTypes->hasType(TypeSet::GetValueType(val.value()))) {
      masm.jump(&failedFastPath);
    }
    checkTypeSet = false;
  } else {
    // We can do the same trick as above for primitive types of specialized
    // registers.
    TypedOrValueRegister reg = val.reg();
    if (reg.hasTyped() && reg.type() != MIRType::Object) {
      MIRType type = reg.type();
      if (!propTypes || !propTypes->hasType(TypeSet::PrimitiveType(type))) {
        masm.jump(&failedFastPath);
      }
      checkTypeSet = false;
    }
  }

  Label done;
  if (checkTypeSet) {
    TypedOrValueRegister valReg = val.reg();
    if (propTypes) {
      // guardTypeSet can read from type sets without triggering read barriers.
      TypeSet::readBarrier(propTypes);
      masm.guardTypeSet(valReg, propTypes, BarrierKind::TypeSet, scratch1,
                        objScratch, spectreRegToZero, &failedFastPath);
      masm.jump(&done);
    } else {
      masm.jump(&failedFastPath);
    }
  }

  if (failedFastPath.used()) {
    // The inline type check failed. Do a callWithABI to check the current
    // TypeSet in case the type was added after we generated this stub.
    masm.bind(&failedFastPath);

    AllocatableRegisterSet regs(GeneralRegisterSet::Volatile(),
                                liveRegs.fpus());
    LiveRegisterSet save(regs.asLiveSet());
    masm.PushRegsInMask(save);

    regs.takeUnchecked(scratch1);

    // Push |val| first to make sure everything is fine if |val| aliases
    // scratch2.
    Register scratch2 = regs.takeAnyGeneral();
    masm.Push(val);
    masm.moveStackPtrTo(scratch2);

    Register scratch3 = regs.takeAnyGeneral();
    masm.Push(id, scratch3);
    masm.moveStackPtrTo(scratch3);

    masm.setupUnalignedABICall(scratch1);
    masm.movePtr(ImmGCPtr(group), scratch1);
    masm.passABIArg(scratch1);
    masm.passABIArg(scratch3);
    masm.passABIArg(scratch2);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, GroupHasPropertyTypes));
    masm.mov(ReturnReg, scratch1);

    masm.adjustStack(sizeof(Value) + sizeof(jsid));

    LiveRegisterSet ignore;
    ignore.add(scratch1);
    masm.PopRegsInMaskIgnore(save, ignore);

    masm.branchIfTrueBool(scratch1, &done);
    if (objScratch != InvalidReg) {
      masm.pop(objScratch);
    }
    masm.pop(obj);
    masm.jump(failures);
  }

  masm.bind(&done);
  if (objScratch != InvalidReg) {
    masm.Pop(objScratch);
  }
  masm.Pop(obj);
}

bool IonCacheIRCompiler::emitStoreFixedSlot(ObjOperandId objId,
                                            uint32_t offsetOffset,
                                            ValOperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  int32_t offset = int32StubField(offsetOffset);
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);

  Maybe<AutoScratchRegister> scratch;
  if (needsPostBarrier()) {
    scratch.emplace(allocator, masm);
  }

  if (typeCheckInfo_->isSet()) {
    FailurePath* failure;
    if (!addFailurePath(&failure)) {
      return false;
    }

    EmitCheckPropertyTypes(masm, typeCheckInfo_, obj, val, *liveRegs_,
                           failure->label());
  }

  Address slot(obj, offset);
  EmitPreBarrier(masm, slot, MIRType::Value);
  masm.storeConstantOrRegister(val, slot);
  if (needsPostBarrier()) {
    emitPostBarrierSlot(obj, val, scratch.ref());
  }
  return true;
}

bool IonCacheIRCompiler::emitStoreDynamicSlot(ObjOperandId objId,
                                              uint32_t offsetOffset,
                                              ValOperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  int32_t offset = int32StubField(offsetOffset);
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);
  AutoScratchRegister scratch(allocator, masm);

  if (typeCheckInfo_->isSet()) {
    FailurePath* failure;
    if (!addFailurePath(&failure)) {
      return false;
    }

    EmitCheckPropertyTypes(masm, typeCheckInfo_, obj, val, *liveRegs_,
                           failure->label());
  }

  masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), scratch);
  Address slot(scratch, offset);
  EmitPreBarrier(masm, slot, MIRType::Value);
  masm.storeConstantOrRegister(val, slot);
  if (needsPostBarrier()) {
    emitPostBarrierSlot(obj, val, scratch);
  }
  return true;
}

bool IonCacheIRCompiler::emitAddAndStoreSlotShared(
    CacheOp op, ObjOperandId objId, uint32_t offsetOffset, ValOperandId rhsId,
    bool changeGroup, uint32_t newGroupOffset, uint32_t newShapeOffset,
    Maybe<uint32_t> numNewSlotsOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  int32_t offset = int32StubField(offsetOffset);
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);

  AutoScratchRegister scratch1(allocator, masm);

  Maybe<AutoScratchRegister> scratch2;
  if (op == CacheOp::AllocateAndStoreDynamicSlot) {
    scratch2.emplace(allocator, masm);
  }

  ObjectGroup* newGroup = groupStubField(newGroupOffset);
  Shape* newShape = shapeStubField(newShapeOffset);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  EmitCheckPropertyTypes(masm, typeCheckInfo_, obj, val, *liveRegs_,
                         failure->label());

  if (op == CacheOp::AllocateAndStoreDynamicSlot) {
    // We have to (re)allocate dynamic slots. Do this first, as it's the
    // only fallible operation here. Note that growSlotsPure is
    // fallible but does not GC.
    int32_t numNewSlots = int32StubField(*numNewSlotsOffset);
    MOZ_ASSERT(numNewSlots > 0);

    LiveRegisterSet save(GeneralRegisterSet::Volatile(),
                         liveVolatileFloatRegs());
    masm.PushRegsInMask(save);

    masm.setupUnalignedABICall(scratch1);
    masm.loadJSContext(scratch1);
    masm.passABIArg(scratch1);
    masm.passABIArg(obj);
    masm.move32(Imm32(numNewSlots), scratch2.ref());
    masm.passABIArg(scratch2.ref());
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, NativeObject::growSlotsPure));
    masm.mov(ReturnReg, scratch1);

    LiveRegisterSet ignore;
    ignore.add(scratch1);
    masm.PopRegsInMaskIgnore(save, ignore);

    masm.branchIfFalseBool(scratch1, failure->label());
  }

  if (changeGroup) {
    // Changing object's group from a partially to fully initialized group,
    // per the acquired properties analysis. Only change the group if the
    // old group still has a newScript. This only applies to PlainObjects.
    Label noGroupChange;
    masm.branchIfObjGroupHasNoAddendum(obj, scratch1, &noGroupChange);

    // Update the object's group.
    masm.storeObjGroup(newGroup, obj,
                       [](MacroAssembler& masm, const Address& addr) {
                         EmitPreBarrier(masm, addr, MIRType::ObjectGroup);
                       });

    masm.bind(&noGroupChange);
  }

  // Update the object's shape.
  masm.storeObjShape(newShape, obj,
                     [](MacroAssembler& masm, const Address& addr) {
                       EmitPreBarrier(masm, addr, MIRType::Shape);
                     });

  // Perform the store. No pre-barrier required since this is a new
  // initialization.
  if (op == CacheOp::AddAndStoreFixedSlot) {
    Address slot(obj, offset);
    masm.storeConstantOrRegister(val, slot);
  } else {
    MOZ_ASSERT(op == CacheOp::AddAndStoreDynamicSlot ||
               op == CacheOp::AllocateAndStoreDynamicSlot);
    masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), scratch1);
    Address slot(scratch1, offset);
    masm.storeConstantOrRegister(val, slot);
  }

  if (needsPostBarrier()) {
    emitPostBarrierSlot(obj, val, scratch1);
  }

  return true;
}

bool IonCacheIRCompiler::emitAddAndStoreFixedSlot(
    ObjOperandId objId, uint32_t offsetOffset, ValOperandId rhsId,
    bool changeGroup, uint32_t newGroupOffset, uint32_t newShapeOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Maybe<uint32_t> numNewSlotsOffset = mozilla::Nothing();
  return emitAddAndStoreSlotShared(
      CacheOp::AddAndStoreFixedSlot, objId, offsetOffset, rhsId, changeGroup,
      newGroupOffset, newShapeOffset, numNewSlotsOffset);
}

bool IonCacheIRCompiler::emitAddAndStoreDynamicSlot(
    ObjOperandId objId, uint32_t offsetOffset, ValOperandId rhsId,
    bool changeGroup, uint32_t newGroupOffset, uint32_t newShapeOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Maybe<uint32_t> numNewSlotsOffset = mozilla::Nothing();
  return emitAddAndStoreSlotShared(
      CacheOp::AddAndStoreDynamicSlot, objId, offsetOffset, rhsId, changeGroup,
      newGroupOffset, newShapeOffset, numNewSlotsOffset);
}

bool IonCacheIRCompiler::emitAllocateAndStoreDynamicSlot(
    ObjOperandId objId, uint32_t offsetOffset, ValOperandId rhsId,
    bool changeGroup, uint32_t newGroupOffset, uint32_t newShapeOffset,
    uint32_t numNewSlotsOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  return emitAddAndStoreSlotShared(CacheOp::AllocateAndStoreDynamicSlot, objId,
                                   offsetOffset, rhsId, changeGroup,
                                   newGroupOffset, newShapeOffset,
                                   mozilla::Some(numNewSlotsOffset));
}

bool IonCacheIRCompiler::emitStoreTypedObjectReferenceProperty(
    ObjOperandId objId, uint32_t offsetOffset, TypedThingLayout layout,
    ReferenceType type, ValOperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  int32_t offset = int32StubField(offsetOffset);

  ValueOperand val = allocator.useValueRegister(masm, rhsId);

  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegister scratch2(allocator, masm);

  // We don't need to check property types if the property is always a
  // string.
  if (type != ReferenceType::TYPE_STRING) {
    FailurePath* failure;
    if (!addFailurePath(&failure)) {
      return false;
    }
    EmitCheckPropertyTypes(masm, typeCheckInfo_, obj, TypedOrValueRegister(val),
                           *liveRegs_, failure->label());
  }

  // Compute the address being written to.
  LoadTypedThingData(masm, layout, obj, scratch1);
  Address dest(scratch1, offset);

  emitStoreTypedObjectReferenceProp(val, type, dest, scratch2);

  if (needsPostBarrier() && type != ReferenceType::TYPE_STRING) {
    emitPostBarrierSlot(obj, val, scratch1);
  }
  return true;
}

static void EmitStoreDenseElement(MacroAssembler& masm,
                                  const ConstantOrRegister& value,
                                  Register elements,
                                  BaseObjectElementIndex target) {
  // If the ObjectElements::CONVERT_DOUBLE_ELEMENTS flag is set, int32 values
  // have to be converted to double first. If the value is not int32, it can
  // always be stored directly.

  Address elementsFlags(elements, ObjectElements::offsetOfFlags());
  if (value.constant()) {
    Value v = value.value();
    Label done;
    if (v.isInt32()) {
      Label dontConvert;
      masm.branchTest32(Assembler::Zero, elementsFlags,
                        Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS),
                        &dontConvert);
      masm.storeValue(DoubleValue(v.toInt32()), target);
      masm.jump(&done);
      masm.bind(&dontConvert);
    }
    masm.storeValue(v, target);
    masm.bind(&done);
    return;
  }

  TypedOrValueRegister reg = value.reg();
  if (reg.hasTyped() && reg.type() != MIRType::Int32) {
    masm.storeTypedOrValue(reg, target);
    return;
  }

  Label convert, storeValue, done;
  masm.branchTest32(Assembler::NonZero, elementsFlags,
                    Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS), &convert);
  masm.bind(&storeValue);
  masm.storeTypedOrValue(reg, target);
  masm.jump(&done);

  masm.bind(&convert);
  ScratchDoubleScope fpscratch(masm);
  if (reg.hasValue()) {
    masm.branchTestInt32(Assembler::NotEqual, reg.valueReg(), &storeValue);
    masm.int32ValueToDouble(reg.valueReg(), fpscratch);
    masm.boxDouble(fpscratch, target);
  } else {
    MOZ_ASSERT(reg.type() == MIRType::Int32);
    masm.convertInt32ToDouble(reg.typedReg().gpr(), fpscratch);
    masm.boxDouble(fpscratch, target);
  }

  masm.bind(&done);
}

static void EmitAssertNoCopyOnWriteElements(MacroAssembler& masm,
                                            Register elementsReg) {
#ifdef DEBUG
  // IonBuilder::initOrSetElemTryCache ensures we have no copy-on-write
  // elements. Assert this in debug builds.
  Address elementsFlags(elementsReg, ObjectElements::offsetOfFlags());
  Label ok;
  masm.branchTest32(Assembler::Zero, elementsFlags,
                    Imm32(ObjectElements::COPY_ON_WRITE), &ok);
  masm.assumeUnreachable("Unexpected copy-on-write elements in Ion IC!");
  masm.bind(&ok);
#endif
}

bool IonCacheIRCompiler::emitStoreDenseElement(ObjOperandId objId,
                                               Int32OperandId indexId,
                                               ValOperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  Register index = allocator.useRegister(masm, indexId);
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);

  AutoScratchRegister scratch1(allocator, masm);
  AutoSpectreBoundsScratchRegister spectreScratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  EmitCheckPropertyTypes(masm, typeCheckInfo_, obj, val, *liveRegs_,
                         failure->label());

  // Load obj->elements in scratch1.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch1);

  EmitAssertNoCopyOnWriteElements(masm, scratch1);

  // Bounds check.
  Address initLength(scratch1, ObjectElements::offsetOfInitializedLength());
  masm.spectreBoundsCheck32(index, initLength, spectreScratch,
                            failure->label());

  // Hole check.
  BaseObjectElementIndex element(scratch1, index);
  masm.branchTestMagic(Assembler::Equal, element, failure->label());

  // Check for frozen elements. We have to check this here because we attach
  // this stub also for non-extensible objects, and these can become frozen
  // without triggering a Shape change.
  Address flags(scratch1, ObjectElements::offsetOfFlags());
  masm.branchTest32(Assembler::NonZero, flags, Imm32(ObjectElements::FROZEN),
                    failure->label());

  EmitPreBarrier(masm, element, MIRType::Value);
  EmitStoreDenseElement(masm, val, scratch1, element);
  if (needsPostBarrier()) {
    emitPostBarrierElement(obj, val, scratch1, index);
  }
  return true;
}

bool IonCacheIRCompiler::emitStoreDenseElementHole(ObjOperandId objId,
                                                   Int32OperandId indexId,
                                                   ValOperandId rhsId,
                                                   bool handleAdd) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  Register index = allocator.useRegister(masm, indexId);
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);

  // handleAdd boolean is only relevant for Baseline. Ion ICs can always
  // handle adds as we don't have to set any flags on the fallback stub to
  // track this.

  AutoScratchRegister scratch1(allocator, masm);
  AutoSpectreBoundsScratchRegister spectreScratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  EmitCheckPropertyTypes(masm, typeCheckInfo_, obj, val, *liveRegs_,
                         failure->label());

  // Load obj->elements in scratch1.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch1);

  EmitAssertNoCopyOnWriteElements(masm, scratch1);

  Address initLength(scratch1, ObjectElements::offsetOfInitializedLength());
  BaseObjectElementIndex element(scratch1, index);

  Label inBounds, outOfBounds;
  masm.spectreBoundsCheck32(index, initLength, spectreScratch, &outOfBounds);
  masm.jump(&inBounds);

  masm.bind(&outOfBounds);
  masm.branch32(Assembler::NotEqual, initLength, index, failure->label());

  // If index < capacity, we can add a dense element inline. If not we
  // need to allocate more elements.
  Label capacityOk, allocElement;
  Address capacity(scratch1, ObjectElements::offsetOfCapacity());
  masm.spectreBoundsCheck32(index, capacity, spectreScratch, &allocElement);
  masm.jump(&capacityOk);

  // Check for non-writable array length. We only have to do this if
  // index >= capacity.
  masm.bind(&allocElement);
  Address elementsFlags(scratch1, ObjectElements::offsetOfFlags());
  masm.branchTest32(Assembler::NonZero, elementsFlags,
                    Imm32(ObjectElements::NONWRITABLE_ARRAY_LENGTH),
                    failure->label());

  LiveRegisterSet save(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
  save.takeUnchecked(scratch1);
  masm.PushRegsInMask(save);

  masm.setupUnalignedABICall(scratch1);
  masm.loadJSContext(scratch1);
  masm.passABIArg(scratch1);
  masm.passABIArg(obj);
  masm.callWithABI(
      JS_FUNC_TO_DATA_PTR(void*, NativeObject::addDenseElementPure));
  masm.mov(ReturnReg, scratch1);

  masm.PopRegsInMask(save);
  masm.branchIfFalseBool(scratch1, failure->label());

  // Load the reallocated elements pointer.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch1);

  masm.bind(&capacityOk);

  // Increment initLength.
  masm.add32(Imm32(1), initLength);

  // If length is now <= index, increment length too.
  Label skipIncrementLength;
  Address length(scratch1, ObjectElements::offsetOfLength());
  masm.branch32(Assembler::Above, length, index, &skipIncrementLength);
  masm.add32(Imm32(1), length);
  masm.bind(&skipIncrementLength);

  // Skip EmitPreBarrier as the memory is uninitialized.
  Label doStore;
  masm.jump(&doStore);

  masm.bind(&inBounds);

  EmitPreBarrier(masm, element, MIRType::Value);

  masm.bind(&doStore);
  EmitStoreDenseElement(masm, val, scratch1, element);
  if (needsPostBarrier()) {
    emitPostBarrierElement(obj, val, scratch1, index);
  }
  return true;
}

bool IonCacheIRCompiler::emitArrayPush(ObjOperandId objId, ValOperandId rhsId) {
  MOZ_ASSERT_UNREACHABLE("emitArrayPush not supported for IonCaches.");
  return false;
}

bool IonCacheIRCompiler::emitCallNativeSetter(ObjOperandId objId,
                                              uint32_t setterOffset,
                                              ValOperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);

  Register obj = allocator.useRegister(masm, objId);
  JSFunction* target = &objectStubField(setterOffset)->as<JSFunction>();
  MOZ_ASSERT(target->isNative());
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);

  AutoScratchRegister argJSContext(allocator, masm);
  AutoScratchRegister argVp(allocator, masm);
  AutoScratchRegister argUintN(allocator, masm);
  AutoScratchRegister scratch(allocator, masm);

  allocator.discardStack(masm);

  // Set up the call:
  //  bool (*)(JSContext*, unsigned, Value* vp)
  // vp[0] is callee/outparam
  // vp[1] is |this|
  // vp[2] is the value

  // Build vp and move the base into argVpReg.
  masm.Push(val);
  masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(obj)));
  masm.Push(ObjectValue(*target));
  masm.moveStackPtrTo(argVp.get());

  // Preload other regs.
  masm.loadJSContext(argJSContext);
  masm.move32(Imm32(1), argUintN);

  // Push marking data for later use.
  masm.Push(argUintN);
  pushStubCodePointer();

  if (!masm.icBuildOOLFakeExitFrame(GetReturnAddressToIonCode(cx_), save)) {
    return false;
  }
  masm.enterFakeExitFrame(argJSContext, scratch, ExitFrameType::IonOOLNative);

  if (target->realm() != cx_->realm()) {
    masm.switchToRealm(target->realm(), scratch);
  }

  // Make the call.
  masm.setupUnalignedABICall(scratch);
  masm.passABIArg(argJSContext);
  masm.passABIArg(argUintN);
  masm.passABIArg(argVp);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, target->native()),
                   MoveOp::GENERAL,
                   CheckUnsafeCallWithABI::DontCheckHasExitFrame);

  // Test for failure.
  masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

  if (target->realm() != cx_->realm()) {
    masm.switchToRealm(cx_->realm(), ReturnReg);
  }

  masm.adjustStack(IonOOLNativeExitFrameLayout::Size(1));
  return true;
}

bool IonCacheIRCompiler::emitCallScriptedSetter(ObjOperandId objId,
                                                uint32_t setterOffset,
                                                ValOperandId rhsId,
                                                bool sameRealm) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);

  Register obj = allocator.useRegister(masm, objId);
  JSFunction* target = &objectStubField(setterOffset)->as<JSFunction>();
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);

  MOZ_ASSERT(sameRealm == (cx_->realm() == target->realm()));

  AutoScratchRegister scratch(allocator, masm);

  allocator.discardStack(masm);

  uint32_t framePushedBefore = masm.framePushed();

  // Construct IonICCallFrameLayout.
  uint32_t descriptor = MakeFrameDescriptor(
      masm.framePushed(), FrameType::IonJS, IonICCallFrameLayout::Size());
  pushStubCodePointer();
  masm.Push(Imm32(descriptor));
  masm.Push(ImmPtr(GetReturnAddressToIonCode(cx_)));

  // The JitFrameLayout pushed below will be aligned to JitStackAlignment,
  // so we just have to make sure the stack is aligned after we push the
  // |this| + argument Values.
  size_t numArgs = std::max<size_t>(1, target->nargs());
  uint32_t argSize = (numArgs + 1) * sizeof(Value);
  uint32_t padding =
      ComputeByteAlignment(masm.framePushed() + argSize, JitStackAlignment);
  MOZ_ASSERT(padding % sizeof(uintptr_t) == 0);
  MOZ_ASSERT(padding < JitStackAlignment);
  masm.reserveStack(padding);

  for (size_t i = 1; i < target->nargs(); i++) {
    masm.Push(UndefinedValue());
  }
  masm.Push(val);
  masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(obj)));

  if (!sameRealm) {
    masm.switchToRealm(target->realm(), scratch);
  }

  masm.movePtr(ImmGCPtr(target), scratch);

  descriptor = MakeFrameDescriptor(argSize + padding, FrameType::IonICCall,
                                   JitFrameLayout::Size());
  masm.Push(Imm32(1));  // argc
  masm.Push(scratch);
  masm.Push(Imm32(descriptor));

  // Check stack alignment. Add sizeof(uintptr_t) for the return address.
  MOZ_ASSERT(((masm.framePushed() + sizeof(uintptr_t)) % JitStackAlignment) ==
             0);

  MOZ_ASSERT(target->hasJitEntry());
  masm.loadJitCodeRaw(scratch, scratch);
  masm.callJit(scratch);

  if (!sameRealm) {
    masm.switchToRealm(cx_->realm(), ReturnReg);
  }

  masm.freeStack(masm.framePushed() - framePushedBefore);
  return true;
}

bool IonCacheIRCompiler::emitCallSetArrayLength(ObjOperandId objId, bool strict,
                                                ValOperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);

  Register obj = allocator.useRegister(masm, objId);
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);

  allocator.discardStack(masm);
  prepareVMCall(masm, save);

  masm.Push(Imm32(strict));
  masm.Push(val);
  masm.Push(obj);

  using Fn = bool (*)(JSContext*, HandleObject, HandleValue, bool);
  callVM<Fn, jit::SetArrayLength>(masm);
  return true;
}

bool IonCacheIRCompiler::emitCallProxySet(ObjOperandId objId, uint32_t idOffset,
                                          ValOperandId rhsId, bool strict) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);

  Register obj = allocator.useRegister(masm, objId);
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);
  jsid id = idStubField(idOffset);

  AutoScratchRegister scratch(allocator, masm);

  allocator.discardStack(masm);
  prepareVMCall(masm, save);

  masm.Push(Imm32(strict));
  masm.Push(val);
  masm.Push(id, scratch);
  masm.Push(obj);

  using Fn = bool (*)(JSContext*, HandleObject, HandleId, HandleValue, bool);
  callVM<Fn, ProxySetProperty>(masm);
  return true;
}

bool IonCacheIRCompiler::emitCallProxySetByValue(ObjOperandId objId,
                                                 ValOperandId idId,
                                                 ValOperandId rhsId,
                                                 bool strict) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);

  Register obj = allocator.useRegister(masm, objId);
  ConstantOrRegister idVal = allocator.useConstantOrRegister(masm, idId);
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);

  allocator.discardStack(masm);
  prepareVMCall(masm, save);

  masm.Push(Imm32(strict));
  masm.Push(val);
  masm.Push(idVal);
  masm.Push(obj);

  using Fn = bool (*)(JSContext*, HandleObject, HandleValue, HandleValue, bool);
  callVM<Fn, ProxySetPropertyByValue>(masm);
  return true;
}

bool IonCacheIRCompiler::emitCallAddOrUpdateSparseElementHelper(
    ObjOperandId objId, Int32OperandId idId, ValOperandId rhsId, bool strict) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);

  Register obj = allocator.useRegister(masm, objId);
  Register id = allocator.useRegister(masm, idId);
  ValueOperand val = allocator.useValueRegister(masm, rhsId);

  allocator.discardStack(masm);
  prepareVMCall(masm, save);

  masm.Push(Imm32(strict));
  masm.Push(val);
  masm.Push(id);
  masm.Push(obj);

  using Fn = bool (*)(JSContext * cx, HandleArrayObject obj, int32_t int_id,
                      HandleValue v, bool strict);
  callVM<Fn, AddOrUpdateSparseElementHelper>(masm);
  return true;
}

bool IonCacheIRCompiler::emitMegamorphicSetElement(ObjOperandId objId,
                                                   ValOperandId idId,
                                                   ValOperandId rhsId,
                                                   bool strict) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);

  Register obj = allocator.useRegister(masm, objId);
  ConstantOrRegister idVal = allocator.useConstantOrRegister(masm, idId);
  ConstantOrRegister val = allocator.useConstantOrRegister(masm, rhsId);

  allocator.discardStack(masm);
  prepareVMCall(masm, save);

  masm.Push(Imm32(strict));
  masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(obj)));
  masm.Push(val);
  masm.Push(idVal);
  masm.Push(obj);

  using Fn = bool (*)(JSContext*, HandleObject, HandleValue, HandleValue,
                      HandleValue, bool);
  callVM<Fn, SetObjectElementWithReceiver>(masm);
  return true;
}

bool IonCacheIRCompiler::emitTypeMonitorResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  return emitReturnFromIC();
}

bool IonCacheIRCompiler::emitReturnFromIC() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  if (!savedLiveRegs_) {
    allocator.restoreInputState(masm);
  }

  uint8_t* rejoinAddr = ic_->rejoinAddr(ionScript_);
  masm.jump(ImmPtr(rejoinAddr));
  return true;
}

bool IonCacheIRCompiler::emitGuardAndGetIterator() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());

  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegister scratch2(allocator, masm);
  AutoScratchRegister niScratch(allocator, masm);

  PropertyIteratorObject* iterobj =
      &objectStubField(reader.stubOffset())->as<PropertyIteratorObject>();
  NativeIterator** enumerators =
      rawWordStubField<NativeIterator**>(reader.stubOffset());

  Register output = allocator.defineRegister(masm, reader.objOperandId());

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Load our PropertyIteratorObject* and its NativeIterator.
  masm.movePtr(ImmGCPtr(iterobj), output);
  masm.loadObjPrivate(output, PropertyIteratorObject::NUM_FIXED_SLOTS,
                      niScratch);

  // Ensure the iterator is reusable: see NativeIterator::isReusable.
  masm.branchIfNativeIteratorNotReusable(niScratch, failure->label());

  // Pre-write barrier for store to 'objectBeingIterated_'.
  Address iterObjAddr(niScratch, NativeIterator::offsetOfObjectBeingIterated());
  EmitPreBarrier(masm, iterObjAddr, MIRType::Object);

  // Mark iterator as active.
  Address iterFlagsAddr(niScratch, NativeIterator::offsetOfFlagsAndCount());
  masm.storePtr(obj, iterObjAddr);
  masm.or32(Imm32(NativeIterator::Flags::Active), iterFlagsAddr);

  // Post-write barrier for stores to 'objectBeingIterated_'.
  emitPostBarrierSlot(output,
                      TypedOrValueRegister(MIRType::Object, AnyRegister(obj)),
                      scratch1);

  // Chain onto the active iterator stack.
  masm.loadPtr(AbsoluteAddress(enumerators), scratch1);
  emitRegisterEnumerator(scratch1, niScratch, scratch2);

  return true;
}

bool IonCacheIRCompiler::emitGuardDOMExpandoMissingOrGuardShape(
    ValOperandId expandoId, uint32_t shapeOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValueOperand val = allocator.useValueRegister(masm, expandoId);
  Shape* shape = shapeStubField(shapeOffset);

  AutoScratchRegister objScratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label done;
  masm.branchTestUndefined(Assembler::Equal, val, &done);

  masm.debugAssertIsObject(val);
  masm.unboxObject(val, objScratch);
  // The expando object is not used in this case, so we don't need Spectre
  // mitigations.
  masm.branchTestObjShapeNoSpectreMitigations(Assembler::NotEqual, objScratch,
                                              shape, failure->label());

  masm.bind(&done);
  return true;
}

bool IonCacheIRCompiler::emitLoadDOMExpandoValueGuardGeneration() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  ExpandoAndGeneration* expandoAndGeneration =
      rawWordStubField<ExpandoAndGeneration*>(reader.stubOffset());
  uint64_t* generationFieldPtr =
      expandoGenerationStubFieldPtr(reader.stubOffset());

  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegister scratch2(allocator, masm);
  ValueOperand output =
      allocator.defineValueRegister(masm, reader.valOperandId());

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.loadPtr(Address(obj, ProxyObject::offsetOfReservedSlots()), scratch1);
  Address expandoAddr(scratch1,
                      js::detail::ProxyReservedSlots::offsetOfPrivateSlot());

  // Guard the ExpandoAndGeneration* matches the proxy's ExpandoAndGeneration.
  masm.loadValue(expandoAddr, output);
  masm.branchTestValue(Assembler::NotEqual, output,
                       PrivateValue(expandoAndGeneration), failure->label());

  // Guard expandoAndGeneration->generation matches the expected generation.
  masm.movePtr(ImmPtr(expandoAndGeneration), output.scratchReg());
  masm.movePtr(ImmPtr(generationFieldPtr), scratch1);
  masm.branch64(
      Assembler::NotEqual,
      Address(output.scratchReg(), ExpandoAndGeneration::offsetOfGeneration()),
      Address(scratch1, 0), scratch2, failure->label());

  // Load expandoAndGeneration->expando into the output Value register.
  masm.loadValue(
      Address(output.scratchReg(), ExpandoAndGeneration::offsetOfExpando()),
      output);
  return true;
}

void IonIC::attachCacheIRStub(JSContext* cx, const CacheIRWriter& writer,
                              CacheKind kind, IonScript* ionScript,
                              bool* attached,
                              const PropertyTypeCheckInfo* typeCheckInfo) {
  // We shouldn't GC or report OOM (or any other exception) here.
  AutoAssertNoPendingException aanpe(cx);
  JS::AutoCheckCannotGC nogc;

  MOZ_ASSERT(!*attached);

  // SetProp/SetElem stubs must have non-null typeCheckInfo.
  MOZ_ASSERT(!!typeCheckInfo ==
             (kind == CacheKind::SetProp || kind == CacheKind::SetElem));

  // Do nothing if the IR generator failed or triggered a GC that invalidated
  // the script.
  if (writer.failed() || ionScript->invalidated()) {
    return;
  }

  JitZone* jitZone = cx->zone()->jitZone();
  uint32_t stubDataOffset = sizeof(IonICStub);

  // Try to reuse a previously-allocated CacheIRStubInfo.
  CacheIRStubKey::Lookup lookup(kind, ICStubEngine::IonIC, writer.codeStart(),
                                writer.codeLength());
  CacheIRStubInfo* stubInfo = jitZone->getIonCacheIRStubInfo(lookup);
  if (!stubInfo) {
    // Allocate the shared CacheIRStubInfo. Note that the
    // putIonCacheIRStubInfo call below will transfer ownership to
    // the stub info HashSet, so we don't have to worry about freeing
    // it below.

    // For Ion ICs, we don't track/use the makesGCCalls flag, so just pass true.
    bool makesGCCalls = true;
    stubInfo = CacheIRStubInfo::New(kind, ICStubEngine::IonIC, makesGCCalls,
                                    stubDataOffset, writer);
    if (!stubInfo) {
      return;
    }

    CacheIRStubKey key(stubInfo);
    if (!jitZone->putIonCacheIRStubInfo(lookup, key)) {
      return;
    }
  }

  MOZ_ASSERT(stubInfo);

  // Ensure we don't attach duplicate stubs. This can happen if a stub failed
  // for some reason and the IR generator doesn't check for exactly the same
  // conditions.
  for (IonICStub* stub = firstStub_; stub; stub = stub->next()) {
    if (stub->stubInfo() != stubInfo) {
      continue;
    }
    bool updated = false;
    if (!writer.stubDataEqualsMaybeUpdate(stub->stubDataStart(), &updated)) {
      continue;
    }
    if (updated || (typeCheckInfo && typeCheckInfo->needsTypeBarrier())) {
      // We updated a stub or have a stub that requires property type
      // checks. In this case the stub will likely handle more cases in
      // the future and we shouldn't deoptimize.
      *attached = true;
    }
    return;
  }

  size_t bytesNeeded = stubInfo->stubDataOffset() + stubInfo->stubDataSize();

  // Allocate the IonICStub in the optimized stub space. Ion stubs and
  // CacheIRStubInfo instances for Ion stubs can be purged on GC. That's okay
  // because the stub code is rooted separately when we make a VM call, and
  // stub code should never access the IonICStub after making a VM call. The
  // IonICStub::poison method poisons the stub to catch bugs in this area.
  ICStubSpace* stubSpace = cx->zone()->jitZone()->optimizedStubSpace();
  void* newStubMem = stubSpace->alloc(bytesNeeded);
  if (!newStubMem) {
    return;
  }

  IonICStub* newStub =
      new (newStubMem) IonICStub(fallbackAddr(ionScript), stubInfo);
  writer.copyStubData(newStub->stubDataStart());

  JitContext jctx(cx, nullptr);
  IonCacheIRCompiler compiler(cx, writer, this, ionScript, newStub,
                              typeCheckInfo, stubDataOffset);
  if (!compiler.init()) {
    return;
  }

  JitCode* code = compiler.compile();
  if (!code) {
    return;
  }

  attachStub(newStub, code);
  *attached = true;
}

bool IonCacheIRCompiler::emitCallStringObjectConcatResult(ValOperandId lhsId,
                                                          ValOperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoSaveLiveRegisters save(*this);
  AutoOutputRegister output(*this);

  ValueOperand lhs = allocator.useValueRegister(masm, lhsId);
  ValueOperand rhs = allocator.useValueRegister(masm, rhsId);

  allocator.discardStack(masm);

  prepareVMCall(masm, save);
  masm.Push(rhs);
  masm.Push(lhs);

  using Fn = bool (*)(JSContext*, HandleValue, HandleValue, MutableHandleValue);
  callVM<Fn, DoConcatStringObject>(masm);

  masm.storeCallResultValue(output);
  return true;
}

bool IonCacheIRCompiler::emitCallScriptedFunction(ObjOperandId calleeId,
                                                  Int32OperandId argcId,
                                                  CallFlags flags) {
  MOZ_CRASH("Call ICs not used in ion");
}

bool IonCacheIRCompiler::emitCallNativeFunction() {
  MOZ_CRASH("Call ICs not used in ion");
}

bool IonCacheIRCompiler::emitCallClassHook() {
  MOZ_CRASH("Call ICs not used in ion");
}

bool IonCacheIRCompiler::emitLoadArgumentFixedSlot() {
  MOZ_CRASH("Call ICs not used in ion");
}

bool IonCacheIRCompiler::emitLoadArgumentDynamicSlot() {
  MOZ_CRASH("Call ICs not used in ion");
}

bool IonCacheIRCompiler::emitGuardFunApply(Int32OperandId argcId,
                                           CallFlags flags) {
  MOZ_CRASH("Call ICs not used in ion");
}
