/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Ion_h
#define jit_Ion_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/Result.h"

#include "jit/BaselineJIT.h"
#include "jit/CompileWrappers.h"
#include "jit/JitContext.h"
#include "jit/JitOptions.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/TypeInference.h"

namespace js {
namespace jit {

bool CanIonCompileScript(JSContext* cx, JSScript* script);
bool CanIonInlineScript(JSScript* script);

MOZ_MUST_USE bool IonCompileScriptForBaselineAtEntry(JSContext* cx,
                                                     BaselineFrame* frame);

struct IonOsrTempData {
  void* jitcode;
  uint8_t* baselineFrame;

  static constexpr size_t offsetOfJitCode() {
    return offsetof(IonOsrTempData, jitcode);
  }
  static constexpr size_t offsetOfBaselineFrame() {
    return offsetof(IonOsrTempData, baselineFrame);
  }
};

MOZ_MUST_USE bool IonCompileScriptForBaselineOSR(JSContext* cx,
                                                 BaselineFrame* frame,
                                                 uint32_t frameSize,
                                                 jsbytecode* pc,
                                                 IonOsrTempData** infoPtr);

MethodStatus CanEnterIon(JSContext* cx, RunState& state);

MethodStatus Recompile(JSContext* cx, HandleScript script, bool force);

struct EnterJitData;

// Walk the stack and invalidate active Ion frames for the invalid scripts.
void Invalidate(TypeZone& types, JSFreeOp* fop,
                const RecompileInfoVector& invalid, bool resetUses = true,
                bool cancelOffThread = true);
void Invalidate(JSContext* cx, const RecompileInfoVector& invalid,
                bool resetUses = true, bool cancelOffThread = true);
void Invalidate(JSContext* cx, JSScript* script, bool resetUses = true,
                bool cancelOffThread = true);

class MIRGenerator;
class LIRGraph;
class CodeGenerator;
class LazyLinkExitFrameLayout;
class WarpSnapshot;

MOZ_MUST_USE bool OptimizeMIR(MIRGenerator* mir);
LIRGraph* GenerateLIR(MIRGenerator* mir);
CodeGenerator* GenerateCode(MIRGenerator* mir, LIRGraph* lir);
CodeGenerator* CompileBackEnd(MIRGenerator* mir, WarpSnapshot* snapshot);

void LinkIonScript(JSContext* cx, HandleScript calleescript);
uint8_t* LazyLinkTopActivation(JSContext* cx, LazyLinkExitFrameLayout* frame);

inline bool IsIonInlinableGetterOrSetterOp(JSOp op) {
  // GETPROP, CALLPROP, LENGTH, GETELEM, and JSOp::CallElem. (Inlined Getters)
  // SETPROP, SETNAME, SETGNAME (Inlined Setters)
  return IsGetPropOp(op) || IsGetElemOp(op) || IsSetPropOp(op);
}

inline bool IsIonInlinableOp(JSOp op) {
  // CALL, FUNCALL, FUNAPPLY, EVAL, NEW (Normal Callsites)
  // or an inlinable getter or setter.
  return (IsInvokeOp(op) && !IsSpreadOp(op)) ||
         IsIonInlinableGetterOrSetterOp(op);
}

inline bool TooManyActualArguments(unsigned nargs) {
  return nargs > JitOptions.maxStackArgs;
}

inline bool TooManyFormalArguments(unsigned nargs) {
  return nargs >= SNAPSHOT_MAX_NARGS || TooManyActualArguments(nargs);
}

inline size_t NumLocalsAndArgs(JSScript* script) {
  size_t num = 1 /* this */ + script->nfixed();
  if (JSFunction* fun = script->function()) {
    num += fun->nargs();
  }
  return num;
}

// Debugging RAII class which marks the current thread as performing an Ion
// backend compilation.
class MOZ_RAII AutoEnterIonBackend {
 public:
  explicit AutoEnterIonBackend(bool safeForMinorGC) {
#ifdef DEBUG
    JitContext* jcx = GetJitContext();
    jcx->enterIonBackend(safeForMinorGC);
#endif
  }

#ifdef DEBUG
  ~AutoEnterIonBackend() {
    JitContext* jcx = GetJitContext();
    jcx->leaveIonBackend();
  }
#endif
};

bool OffThreadCompilationAvailable(JSContext* cx);

void ForbidCompilation(JSContext* cx, JSScript* script);

size_t SizeOfIonData(JSScript* script, mozilla::MallocSizeOf mallocSizeOf);

inline bool IsIonEnabled(JSContext* cx) {
  if (MOZ_UNLIKELY(!IsBaselineJitEnabled(cx) || cx->options().disableIon())) {
    return false;
  }

  // If TI is disabled, Ion can only be used if WarpBuilder is enabled.
  if (MOZ_LIKELY(IsTypeInferenceEnabled())) {
    MOZ_ASSERT(!JitOptions.warpBuilder,
               "Shouldn't enable WarpBuilder without disabling TI!");
  } else {
    if (!JitOptions.warpBuilder) {
      return false;
    }
  }

  if (MOZ_LIKELY(JitOptions.ion)) {
    return true;
  }
  if (JitOptions.jitForTrustedPrincipals) {
    JS::Realm* realm = js::GetContextRealm(cx);
    return realm && JS::GetRealmPrincipals(realm) &&
           JS::GetRealmPrincipals(realm)->isSystemOrAddonPrincipal();
  }
  return false;
}

}  // namespace jit
}  // namespace js

#endif /* jit_Ion_h */
