/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonCacheIRCompiler_h
#define jit_IonCacheIRCompiler_h

#include "mozilla/Maybe.h"

#include "jit/CacheIR.h"
#include "jit/CacheIRCompiler.h"
#include "jit/IonIC.h"

namespace js {
namespace jit {

// IonCacheIRCompiler compiles CacheIR to IonIC native code.
class MOZ_RAII IonCacheIRCompiler : public CacheIRCompiler {
 public:
  friend class AutoSaveLiveRegisters;
  friend class AutoCallVM;

  IonCacheIRCompiler(JSContext* cx, const CacheIRWriter& writer, IonIC* ic,
                     IonScript* ionScript, IonICStub* stub,
                     const PropertyTypeCheckInfo* typeCheckInfo,
                     uint32_t stubDataOffset);

  MOZ_MUST_USE bool init();
  JitCode* compile();

 private:
  const CacheIRWriter& writer_;
  IonIC* ic_;
  IonScript* ionScript_;

  // The stub we're generating code for.
  IonICStub* stub_;

  // Information necessary to generate property type checks. Non-null iff
  // this is a SetProp/SetElem stub.
  const PropertyTypeCheckInfo* typeCheckInfo_;

  Vector<CodeOffset, 4, SystemAllocPolicy> nextCodeOffsets_;
  mozilla::Maybe<LiveRegisterSet> liveRegs_;
  mozilla::Maybe<CodeOffset> stubJitCodeOffset_;

  bool savedLiveRegs_;

  template <typename T>
  T rawWordStubField(uint32_t offset);

  template <typename T>
  T rawInt64StubField(uint32_t offset);

  uint64_t* expandoGenerationStubFieldPtr(uint32_t offset);

  void prepareVMCall(MacroAssembler& masm, const AutoSaveLiveRegisters&);

  template <typename Fn, Fn fn>
  void callVM(MacroAssembler& masm);

  MOZ_MUST_USE bool emitAddAndStoreSlotShared(
      CacheOp op, ObjOperandId objId, uint32_t offsetOffset, ValOperandId rhsId,
      bool changeGroup, uint32_t newGroupOffset, uint32_t newShapeOffset,
      mozilla::Maybe<uint32_t> numNewSlotsOffset);
  MOZ_MUST_USE bool emitCallScriptedGetterResultShared(
      TypedOrValueRegister receiver, uint32_t getterOffset, bool sameRealm,
      TypedOrValueRegister output);
  MOZ_MUST_USE bool emitCallNativeGetterResultShared(
      TypedOrValueRegister receiver, uint32_t getterOffset,
      const AutoOutputRegister& output, AutoSaveLiveRegisters& save);

  bool needsPostBarrier() const;

  void pushStubCodePointer();

#define DEFINE_OP(op) MOZ_MUST_USE bool emit##op();
  CACHE_IR_UNSHARED_OPS(DEFINE_OP)
#undef DEFINE_OP

  CACHE_IR_COMPILER_UNSHARED_GENERATED
};

}  // namespace jit
}  // namespace js

#endif /* jit_IonCacheIRCompiler_h */
