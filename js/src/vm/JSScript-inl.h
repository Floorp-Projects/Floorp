/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_JSScript_inl_h
#define vm_JSScript_inl_h

#include "vm/JSScript.h"

#include <utility>

#include "jit/BaselineJIT.h"
#include "jit/IonAnalysis.h"
#include "jit/JitScript.h"
#include "vm/EnvironmentObject.h"
#include "vm/RegExpObject.h"
#include "wasm/AsmJS.h"

#include "vm/Realm-inl.h"
#include "vm/Shape-inl.h"

namespace js {

ScriptCounts::ScriptCounts()
    : pcCounts_(), throwCounts_(), ionCounts_(nullptr) {}

ScriptCounts::ScriptCounts(PCCountsVector&& jumpTargets)
    : pcCounts_(std::move(jumpTargets)), throwCounts_(), ionCounts_(nullptr) {}

ScriptCounts::ScriptCounts(ScriptCounts&& src)
    : pcCounts_(std::move(src.pcCounts_)),
      throwCounts_(std::move(src.throwCounts_)),
      ionCounts_(std::move(src.ionCounts_)) {
  src.ionCounts_ = nullptr;
}

ScriptCounts& ScriptCounts::operator=(ScriptCounts&& src) {
  pcCounts_ = std::move(src.pcCounts_);
  throwCounts_ = std::move(src.throwCounts_);
  ionCounts_ = std::move(src.ionCounts_);
  src.ionCounts_ = nullptr;
  return *this;
}

ScriptCounts::~ScriptCounts() { js_delete(ionCounts_); }

ScriptAndCounts::ScriptAndCounts(JSScript* script)
    : script(script), scriptCounts() {
  script->releaseScriptCounts(&scriptCounts);
}

ScriptAndCounts::ScriptAndCounts(ScriptAndCounts&& sac)
    : script(std::move(sac.script)),
      scriptCounts(std::move(sac.scriptCounts)) {}

void SetFrameArgumentsObject(JSContext* cx, AbstractFramePtr frame,
                             HandleScript script, JSObject* argsobj);

/* static */ inline JSFunction* LazyScript::functionDelazifying(
    JSContext* cx, Handle<LazyScript*> script) {
  RootedFunction fun(cx, script->function_);
  if (script->function_ && !JSFunction::getOrCreateScript(cx, fun)) {
    return nullptr;
  }
  return script->function_;
}

}  // namespace js

inline JSFunction* JSScript::functionDelazifying() const {
  JSFunction* fun = function();
  if (fun && fun->isInterpretedLazy()) {
    fun->setUnlazifiedScript(const_cast<JSScript*>(this));
    // If this script has a LazyScript, make sure the LazyScript has a
    // reference to the script when delazifying its canonical function.
    if (lazyScript && !lazyScript->maybeScript()) {
      lazyScript->initScript(const_cast<JSScript*>(this));
    }
  }
  return fun;
}

inline void JSScript::ensureNonLazyCanonicalFunction() {
  // Infallibly delazify the canonical script.
  JSFunction* fun = function();
  if (fun && fun->isInterpretedLazy()) {
    functionDelazifying();
  }
}

inline JSFunction* JSScript::getFunction(size_t index) {
  JSObject* obj = getObject(index);
  MOZ_RELEASE_ASSERT(obj->is<JSFunction>(), "Script object is not JSFunction");
  JSFunction* fun = &obj->as<JSFunction>();
  MOZ_ASSERT_IF(fun->isNative(), IsAsmJSModuleNative(fun->native()));
  return fun;
}

inline JSFunction* JSScript::getFunction(jsbytecode* pc) {
  return getFunction(GET_UINT32_INDEX(pc));
}

inline js::RegExpObject* JSScript::getRegExp(size_t index) {
  JSObject* obj = getObject(index);
  MOZ_RELEASE_ASSERT(obj->is<js::RegExpObject>(),
                     "Script object is not RegExpObject");
  return &obj->as<js::RegExpObject>();
}

inline js::RegExpObject* JSScript::getRegExp(jsbytecode* pc) {
  JSObject* obj = getObject(pc);
  MOZ_RELEASE_ASSERT(obj->is<js::RegExpObject>(),
                     "Script object is not RegExpObject");
  return &obj->as<js::RegExpObject>();
}

inline js::GlobalObject& JSScript::global() const {
  /*
   * A JSScript always marks its realm's global so we can assert it's non-null
   * here. We don't need a read barrier here for the same reason
   * JSObject::nonCCWGlobal doesn't need one.
   */
  return *realm()->unsafeUnbarrieredMaybeGlobal();
}

inline bool JSScript::hasGlobal(const js::GlobalObject* global) const {
  return global == realm()->unsafeUnbarrieredMaybeGlobal();
}

inline js::LexicalScope* JSScript::maybeNamedLambdaScope() const {
  // Dynamically created Functions via the 'new Function' are considered
  // named lambdas but they do not have the named lambda scope of
  // textually-created named lambdas.
  js::Scope* scope = outermostScope();
  if (scope->kind() == js::ScopeKind::NamedLambda ||
      scope->kind() == js::ScopeKind::StrictNamedLambda) {
    MOZ_ASSERT_IF(!strict(), scope->kind() == js::ScopeKind::NamedLambda);
    MOZ_ASSERT_IF(strict(), scope->kind() == js::ScopeKind::StrictNamedLambda);
    return &scope->as<js::LexicalScope>();
  }
  return nullptr;
}

inline js::Shape* JSScript::initialEnvironmentShape() const {
  js::Scope* scope = bodyScope();
  if (scope->is<js::FunctionScope>()) {
    if (js::Shape* envShape = scope->environmentShape()) {
      return envShape;
    }
    if (js::Scope* namedLambdaScope = maybeNamedLambdaScope()) {
      return namedLambdaScope->environmentShape();
    }
  } else if (scope->is<js::EvalScope>()) {
    return scope->environmentShape();
  }
  return nullptr;
}

inline JSPrincipals* JSScript::principals() { return realm()->principals(); }

inline void JSScript::setBaselineScript(
    JSRuntime* rt, js::jit::BaselineScript* baselineScript) {
  setBaselineScript(rt->defaultFreeOp(), baselineScript);
}

inline void JSScript::setBaselineScript(
    JSFreeOp* fop, js::jit::BaselineScript* baselineScript) {
  if (hasBaselineScript()) {
    js::jit::BaselineScript::writeBarrierPre(zone(), baseline);
    clearBaselineScript(fop);
  }
  MOZ_ASSERT(!ion || ion == ION_DISABLED_SCRIPT);

  baseline = baselineScript;
  if (hasBaselineScript()) {
    AddCellMemory(this, baseline->allocBytes(), js::MemoryUse::BaselineScript);
  }
  resetWarmUpResetCounter();
  updateJitCodeRaw(fop->runtime());
}

inline void JSScript::clearBaselineScript(JSFreeOp* fop) {
  MOZ_ASSERT(hasBaselineScript());
  fop->removeCellMemory(this, baseline->allocBytes(),
                        js::MemoryUse::BaselineScript);
  baseline = nullptr;
}

inline void JSScript::clearIonScript(JSFreeOp* fop) {
  MOZ_ASSERT(hasIonScript());
  fop->removeCellMemory(this, ion->allocBytes(), js::MemoryUse::IonScript);
  ion = nullptr;
}

inline bool JSScript::ensureHasAnalyzedArgsUsage(JSContext* cx) {
  if (analyzedArgsUsage()) {
    return true;
  }
  return js::jit::AnalyzeArgumentsUsage(cx, this);
}

inline bool JSScript::isDebuggee() const {
  return realm_->debuggerObservesAllExecution() || hasDebugScript();
}

#endif /* vm_JSScript_inl_h */
