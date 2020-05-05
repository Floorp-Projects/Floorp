/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitContext_h
#define jit_JitContext_h
#include "mozilla/MemoryReporting.h"
#include "mozilla/Result.h"

#include "jit/CompileWrappers.h"
#include "jit/JitOptions.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/TypeInference.h"

namespace js {
namespace jit {

class TempAllocator;

enum MethodStatus {
  Method_Error,
  Method_CantCompile,
  Method_Skipped,
  Method_Compiled
};

enum class AbortReason : uint8_t {
  Alloc,
  Inlining,
  PreliminaryObjects,
  Disable,
  Error,
  NoAbort
};

template <typename V>
using AbortReasonOr = mozilla::Result<V, AbortReason>;
using mozilla::Err;
using mozilla::Ok;

static_assert(sizeof(AbortReasonOr<Ok>) <= sizeof(uintptr_t),
              "Unexpected size of AbortReasonOr<Ok>");
static_assert(sizeof(AbortReasonOr<bool>) <= sizeof(uintptr_t),
              "Unexpected size of AbortReasonOr<bool>");

// A JIT context is needed to enter into either an JIT method or an instance
// of a JIT compiler. It points to a temporary allocator and the active
// JSContext, either of which may be nullptr, and the active realm, which
// will not be nullptr.

class JitContext {
  JitContext* prev_ = nullptr;
  CompileRealm* realm_ = nullptr;
  int assemblerCount_ = 0;

#ifdef DEBUG
  // Whether this thread is actively Ion compiling (does not include Wasm or
  // IonBuilder).
  bool inIonBackend_ = false;

  // Whether this thread is actively Ion compiling in a context where a minor
  // GC could happen simultaneously. If this is true, this thread cannot use
  // any pointers into the nursery.
  bool inIonBackendSafeForMinorGC_ = false;

  bool isCompilingWasm_ = false;
  bool oom_ = false;
#endif

 public:
  // Running context when executing on the main thread. Not available during
  // compilation.
  JSContext* cx = nullptr;

  // Allocator for temporary memory during compilation.
  TempAllocator* temp = nullptr;

  // Wrappers with information about the current runtime/realm for use
  // during compilation.
  CompileRuntime* runtime = nullptr;

  // Constructor for compilations happening on the main thread.
  JitContext(JSContext* cx, TempAllocator* temp);

  // Constructor for off-thread Ion compilations.
  JitContext(CompileRuntime* rt, CompileRealm* realm, TempAllocator* temp);

  // Constructors for Wasm compilation.
  explicit JitContext(TempAllocator* temp);
  JitContext();

  ~JitContext();

  int getNextAssemblerId() { return assemblerCount_++; }

  CompileRealm* maybeRealm() const { return realm_; }
  CompileRealm* realm() const {
    MOZ_ASSERT(maybeRealm());
    return maybeRealm();
  }

#ifdef DEBUG
  bool isCompilingWasm() { return isCompilingWasm_; }
  bool hasOOM() { return oom_; }
  void setOOM() { oom_ = true; }

  bool inIonBackend() const { return inIonBackend_; }

  bool inIonBackendSafeForMinorGC() const {
    return inIonBackendSafeForMinorGC_;
  }

  void enterIonBackend(bool safeForMinorGC) {
    MOZ_ASSERT(!inIonBackend_);
    MOZ_ASSERT(!inIonBackendSafeForMinorGC_);
    inIonBackend_ = true;
    inIonBackendSafeForMinorGC_ = safeForMinorGC;
  }
  void leaveIonBackend() {
    MOZ_ASSERT(inIonBackend_);
    inIonBackend_ = false;
    inIonBackendSafeForMinorGC_ = false;
  }
#endif
};

// Process-wide initialization of JIT data structures.
MOZ_MUST_USE bool InitializeJit();

// Get and set the current JIT context.
JitContext* GetJitContext();
JitContext* MaybeGetJitContext();

void SetJitContext(JitContext* ctx);

enum JitExecStatus {
  // The method call had to be aborted due to a stack limit check. This
  // error indicates that Ion never attempted to clean up frames.
  JitExec_Aborted,

  // The method call resulted in an error, and IonMonkey has cleaned up
  // frames.
  JitExec_Error,

  // The method call succeeded and returned a value.
  JitExec_Ok
};

static inline bool IsErrorStatus(JitExecStatus status) {
  return status == JitExec_Error || status == JitExec_Aborted;
}

bool JitSupportsWasmSimd();
bool JitSupportsAtomics();

}  // namespace jit
}  // namespace js

#endif /* jit_JitContext_h */
