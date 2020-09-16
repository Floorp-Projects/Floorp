/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TrialInlining_h
#define jit_TrialInlining_h

#include "jit/CacheIR.h"
#include "jit/ICStubSpace.h"
#include "vm/BytecodeLocation.h"

/*
 * [SMDOC] Trial Inlining
 *
 * WarpBuilder relies on transpiling CacheIR. When inlining scripted
 * functions in WarpBuilder, we want our ICs to be as monomorphic as
 * possible. Functions with multiple callers complicate this. An IC in
 * such a function might be monomorphic for any given caller, but
 * polymorphic overall. This make the input to WarpBuilder less precise.
 *
 * To solve this problem, we do trial inlining. During baseline
 * execution, we identify call sites for which it would be useful to
 * have more precise inlining data. For each such call site, we
 * allocate a fresh ICScript and replace the existing call IC with a
 * new specialized IC that invokes the callee using the new
 * ICScript. Other callers of the callee will continue using the
 * default ICScript. When we eventually Warp-compile the script, we
 * can generate code for the callee using the IC information in our
 * private ICScript, which is specialized for its caller.
 *
 * The same approach can be used to inline recursively.
 */

namespace js {
namespace jit {

class ICEntry;
class ICScript;

/*
 * An InliningRoot is owned by a JitScript. In turn, it owns the set
 * of ICScripts that are candidates for being inlined in that JitScript.
 */
class InliningRoot {
 public:
  explicit InliningRoot(JSContext* cx, JSScript* owningScript)
      : owningScript_(owningScript),
        inlinedScripts_(cx),
        totalBytecodeSize_(owningScript->length()) {}

  FallbackICStubSpace* fallbackStubSpace() { return &fallbackStubSpace_; }

  void trace(JSTracer* trc);

  bool addInlinedScript(js::UniquePtr<ICScript> icScript);
  void removeInlinedScript(ICScript* icScript);

  uint32_t numInlinedScripts() const { return inlinedScripts_.length(); }

  void purgeOptimizedStubs(Zone* zone);
  void resetWarmUpCounts(uint32_t count);

  JSScript* owningScript() const { return owningScript_; }

  size_t totalBytecodeSize() const { return totalBytecodeSize_; }

  void addToTotalBytecodeSize(size_t size) { totalBytecodeSize_ += size; }

 private:
  FallbackICStubSpace fallbackStubSpace_ = {};
  HeapPtr<JSScript*> owningScript_;
  js::Vector<js::UniquePtr<ICScript>> inlinedScripts_;

  // Bytecode size of outer script and all inlined scripts.
  size_t totalBytecodeSize_;
};

class InlinableCallData {
 public:
  ObjOperandId calleeOperand;
  CallFlags callFlags;
  const uint8_t* endOfSharedPrefix = nullptr;
  JSFunction* target = nullptr;
  ICScript* icScript = nullptr;
};

mozilla::Maybe<InlinableCallData> FindInlinableCallData(ICStub* stub);

class MOZ_RAII TrialInliner {
 public:
  TrialInliner(JSContext* cx, HandleScript script, ICScript* icScript,
               InliningRoot* root)
      : cx_(cx), script_(script), icScript_(icScript), root_(root) {}

  JSContext* cx() { return cx_; }

  MOZ_MUST_USE bool tryInlining();
  MOZ_MUST_USE bool maybeInlineCall(const ICEntry& entry, BytecodeLocation loc);

  static bool canInline(JSFunction* target, HandleScript caller);

 private:
  ICStub* maybeSingleStub(const ICEntry& entry);
  void cloneSharedPrefix(ICStub* stub, const uint8_t* endOfPrefix,
                         CacheIRWriter& writer);
  ICScript* createInlinedICScript(JSFunction* target, BytecodeLocation loc);
  void replaceICStub(const ICEntry& entry, CacheIRWriter& writer,
                     CacheKind kind);

  bool shouldInline(JSFunction* target, ICStub* stub, BytecodeLocation loc);

  JSContext* cx_;
  HandleScript script_;
  ICScript* icScript_;
  InliningRoot* root_;
};

bool DoTrialInlining(JSContext* cx, BaselineFrame* frame);

}  // namespace jit
}  // namespace js

#endif /* jit_TrialInlining_h */
