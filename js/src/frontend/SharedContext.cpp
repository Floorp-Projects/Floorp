/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/SharedContext.h"

#include "mozilla/RefPtr.h"

#include "frontend/AbstractScopePtr.h"
#include "frontend/FunctionSyntaxKind.h"  // FunctionSyntaxKind
#include "frontend/ModuleSharedContext.h"
#include "vm/FunctionFlags.h"          // js::FunctionFlags
#include "vm/GeneratorAndAsyncKind.h"  // js::GeneratorKind, js::FunctionAsyncKind
#include "vm/StencilEnums.h"           // ImmutableScriptFlagsEnum
#include "wasm/AsmJS.h"
#include "wasm/WasmModule.h"

#include "frontend/ParseContext-inl.h"
#include "vm/EnvironmentObject-inl.h"

namespace js {
namespace frontend {

SharedContext::SharedContext(JSContext* cx, Kind kind,
                             CompilationInfo& compilationInfo,
                             Directives directives, SourceExtent extent)
    : cx_(cx),
      compilationInfo_(compilationInfo),
      extent_(extent),
      allowNewTarget_(false),
      allowSuperProperty_(false),
      allowSuperCall_(false),
      allowArguments_(true),
      inWith_(false),
      inClass_(false),
      localStrict(false),
      hasExplicitUseStrict_(false),
      isScriptFieldCopiedToStencil(false) {
  // Compute the script kind "input" flags.
  if (kind == Kind::FunctionBox) {
    setFlag(ImmutableFlags::IsFunction);
  } else if (kind == Kind::Module) {
    MOZ_ASSERT(!compilationInfo.input.options.nonSyntacticScope);
    setFlag(ImmutableFlags::IsModule);
  } else if (kind == Kind::Eval) {
    setFlag(ImmutableFlags::IsForEval);
  } else {
    MOZ_ASSERT(kind == Kind::Global);
  }

  // Note: This is a mix of transitive and non-transitive options.
  const JS::ReadOnlyCompileOptions& options = compilationInfo.input.options;

  // Initialize the transitive "input" flags. These are applied to all
  // SharedContext in this compilation and generally cannot be determined from
  // the source text alone.
  setFlag(ImmutableFlags::SelfHosted, options.selfHostingMode);
  setFlag(ImmutableFlags::ForceStrict, options.forceStrictMode());
  setFlag(ImmutableFlags::HasNonSyntacticScope, options.nonSyntacticScope);

  // Initialize the non-transistive "input" flags if this is a top-level.
  if (isTopLevelContext()) {
    setFlag(ImmutableFlags::TreatAsRunOnce, options.isRunOnce);
    setFlag(ImmutableFlags::NoScriptRval, options.noScriptRval);
  }

  // Initialize the strict flag. This may be updated by the parser as we observe
  // further directives in the body.
  setFlag(ImmutableFlags::Strict, directives.strict());
}

void ScopeContext::computeAllowSyntax(Scope* scope) {
  for (ScopeIter si(scope); si; si++) {
    if (si.kind() == ScopeKind::Function) {
      FunctionScope* funScope = &si.scope()->as<FunctionScope>();
      JSFunction* fun = funScope->canonicalFunction();

      // Arrow function inherit syntax restrictions of enclosing scope.
      if (fun->isArrow()) {
        continue;
      }

      allowNewTarget = true;
      allowSuperProperty = fun->allowSuperProperty();

      if (fun->isDerivedClassConstructor()) {
        allowSuperCall = true;
      }

      if (fun->isFieldInitializer()) {
        allowArguments = false;
      }

      return;
    }
  }
}

void ScopeContext::computeThisBinding(Scope* scope) {
  // Inspect the scope-chain.
  for (ScopeIter si(scope); si; si++) {
    if (si.kind() == ScopeKind::Module) {
      thisBinding = ThisBinding::Module;
      return;
    }

    if (si.kind() == ScopeKind::Function) {
      JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();

      // Arrow functions don't have their own `this` binding.
      if (fun->isArrow()) {
        continue;
      }

      // Derived class constructors (and their nested arrow functions and evals)
      // use ThisBinding::DerivedConstructor, which ensures TDZ checks happen
      // when accessing |this|.
      if (fun->isDerivedClassConstructor()) {
        thisBinding = ThisBinding::DerivedConstructor;
      } else {
        thisBinding = ThisBinding::Function;
      }

      return;
    }
  }

  thisBinding = ThisBinding::Global;
}

void ScopeContext::computeInWith(Scope* scope) {
  for (ScopeIter si(scope); si; si++) {
    if (si.kind() == ScopeKind::With) {
      inWith = true;
      break;
    }
  }
}

void ScopeContext::computeInClass(Scope* scope) {
  for (ScopeIter si(scope); si; si++) {
    if (si.kind() == ScopeKind::ClassBody) {
      inClass = true;
      break;
    }
  }
}

void ScopeContext::computeExternalInitializers(Scope* scope) {
  for (ScopeIter si(scope); si; si++) {
    if (si.scope()->is<FunctionScope>()) {
      FunctionScope& funcScope = si.scope()->as<FunctionScope>();
      JSFunction* fun = funcScope.canonicalFunction();

      // Arrows can call `super()` on behalf on parent so keep searching.
      if (fun->isArrow()) {
        continue;
      }

      if (fun->isClassConstructor()) {
        memberInitializers =
            mozilla::Some(fun->baseScript()->getMemberInitializers());
        MOZ_ASSERT(memberInitializers->valid);
      }

      break;
    }
  }
}

/* static */
Scope* ScopeContext::determineEffectiveScope(Scope* scope,
                                             JSObject* environment) {
  // If the scope-chain is non-syntactic, we may still determine a more precise
  // effective-scope to use instead.
  if (environment && scope->hasOnChain(ScopeKind::NonSyntactic)) {
    JSObject* env = environment;
    while (env) {
      // Look at target of any DebugEnvironmentProxy, but be sure to use
      // enclosingEnvironment() of the proxy itself.
      JSObject* unwrapped = env;
      if (env->is<DebugEnvironmentProxy>()) {
        unwrapped = &env->as<DebugEnvironmentProxy>().environment();
      }

      if (unwrapped->is<CallObject>()) {
        JSFunction* callee = &unwrapped->as<CallObject>().callee();
        return callee->nonLazyScript()->bodyScope();
      }

      env = env->enclosingEnvironment();
    }
  }

  return scope;
}

GlobalSharedContext::GlobalSharedContext(JSContext* cx, ScopeKind scopeKind,
                                         CompilationInfo& compilationInfo,
                                         Directives directives,
                                         SourceExtent extent)
    : SharedContext(cx, Kind::Global, compilationInfo, directives, extent),
      scopeKind_(scopeKind),
      bindings(nullptr) {
  MOZ_ASSERT(scopeKind == ScopeKind::Global ||
             scopeKind == ScopeKind::NonSyntactic);
  MOZ_ASSERT(thisBinding_ == ThisBinding::Global);
}

EvalSharedContext::EvalSharedContext(JSContext* cx,
                                     CompilationInfo& compilationInfo,
                                     CompilationState& compilationState,
                                     SourceExtent extent)
    : SharedContext(cx, Kind::Eval, compilationInfo,
                    compilationState.directives, extent),
      bindings(nullptr) {
  // Eval inherits syntax and binding rules from enclosing environment.
  allowNewTarget_ = compilationState.scopeContext.allowNewTarget;
  allowSuperProperty_ = compilationState.scopeContext.allowSuperProperty;
  allowSuperCall_ = compilationState.scopeContext.allowSuperCall;
  allowArguments_ = compilationState.scopeContext.allowArguments;
  thisBinding_ = compilationState.scopeContext.thisBinding;
  inWith_ = compilationState.scopeContext.inWith;
}

FunctionBox::FunctionBox(JSContext* cx, SourceExtent extent,
                         CompilationInfo& compilationInfo,
                         CompilationState& compilationState,
                         Directives directives, GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind, const ParserAtom* atom,
                         FunctionFlags flags, FunctionIndex index,
                         TopLevelFunction isTopLevel)
    : SharedContext(cx, Kind::FunctionBox, compilationInfo, directives, extent),
      atom_(atom),
      funcDataIndex_(index),
      flags_(FunctionFlags::clearMutableflags(flags)),
      isTopLevel_(isTopLevel),
      emitBytecode(false),
      isStandalone_(false),
      wasEmitted_(false),
      isSingleton_(false),
      isAnnexB(false),
      useAsm(false),
      hasParameterExprs(false),
      hasDestructuringArgs(false),
      hasDuplicateParameters(false),
      hasExprBody_(false),
      usesApply(false),
      usesThis(false),
      usesReturn(false),
      isFunctionFieldCopiedToStencil(false) {
  setFlag(ImmutableFlags::IsGenerator,
          generatorKind == GeneratorKind::Generator);
  setFlag(ImmutableFlags::IsAsync,
          asyncKind == FunctionAsyncKind::AsyncFunction);
}

void FunctionBox::initFromLazyFunction(JSFunction* fun) {
  BaseScript* lazy = fun->baseScript();
  immutableFlags_ = lazy->immutableFlags();
  extent_ = lazy->extent();
}

void FunctionBox::initWithEnclosingParseContext(ParseContext* enclosing,
                                                FunctionFlags flags,
                                                FunctionSyntaxKind kind) {
  SharedContext* sc = enclosing->sc();

  // HasModuleGoal and useAsm are inherited from enclosing context.
  useAsm = sc->isFunctionBox() && sc->asFunctionBox()->useAsmOrInsideUseAsm();
  setHasModuleGoal(sc->hasModuleGoal());

  // Arrow functions don't have their own `this` binding.
  if (flags.isArrow()) {
    allowNewTarget_ = sc->allowNewTarget();
    allowSuperProperty_ = sc->allowSuperProperty();
    allowSuperCall_ = sc->allowSuperCall();
    allowArguments_ = sc->allowArguments();
    thisBinding_ = sc->thisBinding();
  } else {
    if (IsConstructorKind(kind)) {
      auto stmt =
          enclosing->findInnermostStatement<ParseContext::ClassStatement>();
      MOZ_ASSERT(stmt);
      stmt->constructorBox = this;
    }

    allowNewTarget_ = true;
    allowSuperProperty_ = flags.allowSuperProperty();

    if (kind == FunctionSyntaxKind::DerivedClassConstructor) {
      setDerivedClassConstructor();
      allowSuperCall_ = true;
      thisBinding_ = ThisBinding::DerivedConstructor;
    } else {
      thisBinding_ = ThisBinding::Function;
    }

    if (kind == FunctionSyntaxKind::FieldInitializer) {
      setFieldInitializer();
      allowArguments_ = false;
    }
  }

  if (sc->inWith()) {
    inWith_ = true;
  } else {
    auto isWith = [](ParseContext::Statement* stmt) {
      return stmt->kind() == StatementKind::With;
    };

    inWith_ = enclosing->findInnermostStatement(isWith);
  }

  if (sc->inClass()) {
    inClass_ = true;
  } else {
    auto isClass = [](ParseContext::Statement* stmt) {
      return stmt->kind() == StatementKind::Class;
    };

    inClass_ = enclosing->findInnermostStatement(isClass);
  }
}

void FunctionBox::initStandalone(ScopeContext& scopeContext,
                                 FunctionFlags flags, FunctionSyntaxKind kind) {
  if (flags.isArrow()) {
    allowNewTarget_ = scopeContext.allowNewTarget;
    allowSuperProperty_ = scopeContext.allowSuperProperty;
    allowSuperCall_ = scopeContext.allowSuperCall;
    allowArguments_ = scopeContext.allowArguments;
    thisBinding_ = scopeContext.thisBinding;
  } else {
    allowNewTarget_ = true;
    allowSuperProperty_ = flags.allowSuperProperty();

    if (kind == FunctionSyntaxKind::DerivedClassConstructor) {
      setDerivedClassConstructor();
      allowSuperCall_ = true;
      thisBinding_ = ThisBinding::DerivedConstructor;
    } else {
      thisBinding_ = ThisBinding::Function;
    }

    if (kind == FunctionSyntaxKind::FieldInitializer) {
      setFieldInitializer();
      allowArguments_ = false;
    }
  }

  inWith_ = scopeContext.inWith;
  inClass_ = scopeContext.inClass;
}

void FunctionBox::setEnclosingScopeForInnerLazyFunction(ScopeIndex scopeIndex) {
  // For lazy functions inside a function which is being compiled, we cache
  // the incomplete scope object while compiling, and store it to the
  // BaseScript once the enclosing script successfully finishes compilation
  // in FunctionBox::finish.
  MOZ_ASSERT(enclosingScopeIndex_.isNothing());
  enclosingScopeIndex_ = mozilla::Some(scopeIndex);
  if (isFunctionFieldCopiedToStencil) {
    copyUpdatedEnclosingScopeIndex();
  }
}

bool FunctionBox::setAsmJSModule(const JS::WasmModule* module) {
  MOZ_ASSERT(!isFunctionFieldCopiedToStencil);

  MOZ_ASSERT(flags_.kind() == FunctionFlags::NormalFunction);

  // Update flags we will use to allocate the JSFunction.
  flags_.clearBaseScript();
  flags_.setIsExtended();
  flags_.setKind(FunctionFlags::AsmJS);

  if (!compilationInfo_.stencil.asmJS.putNew(index(), module)) {
    js::ReportOutOfMemory(cx_);
    return false;
  }
  return true;
}

ModuleSharedContext::ModuleSharedContext(JSContext* cx,
                                         CompilationInfo& compilationInfo,
                                         ModuleBuilder& builder,
                                         SourceExtent extent)
    : SharedContext(cx, Kind::Module, compilationInfo, Directives(true),
                    extent),
      bindings(nullptr),
      builder(builder) {
  thisBinding_ = ThisBinding::Module;
  setFlag(ImmutableFlags::HasModuleGoal);
}

ScriptStencil& FunctionBox::functionStencil() const {
  return compilationInfo_.stencil.scriptData[funcDataIndex_];
}

void SharedContext::copyScriptFields(ScriptStencil& script) {
  MOZ_ASSERT(!isScriptFieldCopiedToStencil);

  script.immutableFlags = immutableFlags_;
  script.extent = extent_;

  isScriptFieldCopiedToStencil = true;
}

void FunctionBox::finishScriptFlags() {
  MOZ_ASSERT(!isScriptFieldCopiedToStencil);

  using ImmutableFlags = ImmutableScriptFlagsEnum;
  immutableFlags_.setFlag(ImmutableFlags::HasMappedArgsObj, hasMappedArgsObj());
  immutableFlags_.setFlag(ImmutableFlags::IsLikelyConstructorWrapper,
                          isLikelyConstructorWrapper());
}

void FunctionBox::copyScriptFields(ScriptStencil& script) {
  MOZ_ASSERT(&script == &functionStencil());
  MOZ_ASSERT(!isAsmJSModule());

  SharedContext::copyScriptFields(script);

  script.memberInitializers = memberInitializers_;

  isScriptFieldCopiedToStencil = true;
}

void FunctionBox::copyFunctionFields(ScriptStencil& script) {
  MOZ_ASSERT(&script == &functionStencil());
  MOZ_ASSERT(!isFunctionFieldCopiedToStencil);

  script.functionAtom = atom_;
  script.functionFlags = flags_;
  script.nargs = nargs_;
  script.lazyFunctionEnclosingScopeIndex_ = enclosingScopeIndex_;
  script.isStandaloneFunction = isStandalone_;
  script.wasFunctionEmitted = wasEmitted_;
  script.isSingletonFunction = isSingleton_;

  isFunctionFieldCopiedToStencil = true;
}

void FunctionBox::copyUpdatedImmutableFlags() {
  ScriptStencil& script = functionStencil();
  script.immutableFlags = immutableFlags_;
}

void FunctionBox::copyUpdatedExtent() {
  ScriptStencil& script = functionStencil();
  script.extent = extent_;
}

void FunctionBox::copyUpdatedMemberInitializers() {
  ScriptStencil& script = functionStencil();
  script.memberInitializers = memberInitializers_;
}

void FunctionBox::copyUpdatedEnclosingScopeIndex() {
  ScriptStencil& script = functionStencil();
  script.lazyFunctionEnclosingScopeIndex_ = enclosingScopeIndex_;
}

void FunctionBox::copyUpdatedAtomAndFlags() {
  ScriptStencil& script = functionStencil();
  script.functionAtom = atom_;
  script.functionFlags = flags_;
}

void FunctionBox::copyUpdatedWasEmitted() {
  ScriptStencil& script = functionStencil();
  script.wasFunctionEmitted = wasEmitted_;
}

void FunctionBox::copyUpdatedIsSingleton() {
  ScriptStencil& script = functionStencil();
  script.isSingletonFunction = isSingleton_;
}

}  // namespace frontend
}  // namespace js
