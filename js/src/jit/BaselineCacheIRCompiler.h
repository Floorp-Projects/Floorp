/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineCacheIRCompiler_h
#define jit_BaselineCacheIRCompiler_h

#include "gc/Barrier.h"
#include "jit/CacheIR.h"
#include "jit/CacheIRCompiler.h"

namespace js {
namespace jit {

class ICFallbackStub;
class ICStub;

enum class BaselineCacheIRStubKind { Regular, Monitored, Updated };

ICStub* AttachBaselineCacheIRStub(JSContext* cx, const CacheIRWriter& writer,
                                  CacheKind kind,
                                  BaselineCacheIRStubKind stubKind,
                                  JSScript* outerScript, ICFallbackStub* stub,
                                  bool* attached);

// BaselineCacheIRCompiler compiles CacheIR to BaselineIC native code.
class MOZ_RAII BaselineCacheIRCompiler : public CacheIRCompiler {
  bool makesGCCalls_;
  BaselineCacheIRStubKind kind_;

  void tailCallVMInternal(MacroAssembler& masm, TailCallVMFunctionId id);

  template <typename Fn, Fn fn>
  void tailCallVM(MacroAssembler& masm);

  MOZ_MUST_USE bool callTypeUpdateIC(Register obj, ValueOperand val,
                                     Register scratch,
                                     LiveGeneralRegisterSet saveRegs);

  MOZ_MUST_USE bool emitStoreSlotShared(bool isFixed);
  MOZ_MUST_USE bool emitAddAndStoreSlotShared(CacheOp op);

  bool updateArgc(CallFlags flags, Register argcReg, Register scratch);
  void loadStackObject(ArgumentKind kind, CallFlags flags, size_t stackPushed,
                       Register argcReg, Register dest);
  void pushArguments(Register argcReg, Register calleeReg, Register scratch,
                     Register scratch2, CallFlags flags, bool isJitCall);
  void pushStandardArguments(Register argcReg, Register scratch,
                             Register scratch2, bool isJitCall,
                             bool isConstructing);
  void pushArrayArguments(Register argcReg, Register scratch, Register scratch2,
                          bool isJitCall, bool isConstructing);
  void pushFunCallArguments(Register argcReg, Register calleeReg,
                            Register scratch, Register scratch2,
                            bool isJitCall);
  void pushFunApplyArgs(Register argcReg, Register calleeReg, Register scratch,
                        Register scratch2, bool isJitCall);
  void createThis(Register argcReg, Register calleeReg, Register scratch,
                  CallFlags flags);
  void updateReturnValue();

  enum class NativeCallType { Native, ClassHook };
  bool emitCallNativeShared(NativeCallType callType);

  MOZ_MUST_USE bool emitCallScriptedGetterResultShared(
      TypedOrValueRegister receiver);

  template <typename T, typename CallVM>
  MOZ_MUST_USE bool emitCallNativeGetterResultShared(T receiver,
                                                     const CallVM& emitCallVM);

 public:
  friend class AutoStubFrame;

  BaselineCacheIRCompiler(JSContext* cx, const CacheIRWriter& writer,
                          uint32_t stubDataOffset,
                          BaselineCacheIRStubKind stubKind);

  MOZ_MUST_USE bool init(CacheKind kind);

  template <typename Fn, Fn fn>
  void callVM(MacroAssembler& masm);

  JitCode* compile();

  bool makesGCCalls() const;

  Address stubAddress(uint32_t offset) const;

 private:
#define DEFINE_OP(op) MOZ_MUST_USE bool emit##op();
  CACHE_IR_UNSHARED_OPS(DEFINE_OP)
#undef DEFINE_OP

  CACHE_IR_COMPILER_UNSHARED_GENERATED
};

}  // namespace jit
}  // namespace js

#endif /* jit_BaselineCacheIRCompiler_h */
