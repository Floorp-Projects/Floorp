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
    MOZ_ASSERT(!compilationInfo.options.nonSyntacticScope);
    setFlag(ImmutableFlags::IsModule);
  } else if (kind == Kind::Eval) {
    setFlag(ImmutableFlags::IsForEval);
  } else {
    MOZ_ASSERT(kind == Kind::Global);
  }

  // Note: This is a mix of transitive and non-transitive options.
  const JS::ReadOnlyCompileOptions& options = compilationInfo.options;

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

void ScopeContext::computeThisBinding(Scope* scope, JSObject* environment) {
  // If the scope-chain is non-syntactic, we may still determine a more precise
  // effective-scope to use instead.
  Scope* effectiveScope = scope;

  // If this eval is in response to Debugger.Frame.eval, we may have been
  // passed an incomplete scope chain. In order to better determine the 'this'
  // binding type, we traverse the environment chain, looking for a CallObject
  // and recompute the binding type based on its body scope.
  //
  // NOTE: A non-debug eval in a non-syntactic environment will also trigger
  // this code. In that case, we should still compute the same binding type.
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
        effectiveScope = callee->nonLazyScript()->bodyScope();
        break;
      }

      env = env->enclosingEnvironment();
    }
  }

  // Inspect the scope-chain.
  for (ScopeIter si(effectiveScope); si; si++) {
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

EvalSharedContext::EvalSharedContext(JSContext* cx,
                                     CompilationInfo& compilationInfo,
                                     Directives directives, SourceExtent extent)
    : SharedContext(cx, Kind::Eval, compilationInfo, directives, extent),
      bindings(cx) {
  // Eval inherits syntax and binding rules from enclosing environment.
  allowNewTarget_ = compilationInfo.scopeContext.allowNewTarget;
  allowSuperProperty_ = compilationInfo.scopeContext.allowSuperProperty;
  allowSuperCall_ = compilationInfo.scopeContext.allowSuperCall;
  allowArguments_ = compilationInfo.scopeContext.allowArguments;
  thisBinding_ = compilationInfo.scopeContext.thisBinding;
  inWith_ = compilationInfo.scopeContext.inWith;
}

#ifdef DEBUG
bool FunctionBox::atomsAreKept() { return cx_->zone()->hasKeptAtoms(); }
#endif

FunctionBox::FunctionBox(JSContext* cx, FunctionBox* traceListHead,
                         SourceExtent extent, CompilationInfo& compilationInfo,
                         Directives directives, GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind, JSAtom* atom,
                         FunctionFlags flags, FunctionIndex index,
                         TopLevelFunction isTopLevel)
    : SharedContext(cx, Kind::FunctionBox, compilationInfo, directives, extent),
      traceLink_(traceListHead),
      atom_(atom),
      funcDataIndex_(index),
      flags_(flags),
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

  return compilationInfo_.asmJS.putNew(index(), module);
}

/* static */
void FunctionBox::TraceList(JSTracer* trc, FunctionBox* listHead) {
  for (FunctionBox* node = listHead; node; node = node->traceLink_) {
    node->trace(trc);
  }
}

void FunctionBox::trace(JSTracer* trc) {
  if (atom_) {
    TraceRoot(trc, &atom_, "funbox-atom");
  }
}

ModuleSharedContext::ModuleSharedContext(JSContext* cx,
                                         CompilationInfo& compilationInfo,
                                         ModuleBuilder& builder,
                                         SourceExtent extent)
    : SharedContext(cx, Kind::Module, compilationInfo, Directives(true),
                    extent),
      bindings(cx),
      builder(builder) {
  thisBinding_ = ThisBinding::Module;
  setFlag(ImmutableFlags::HasModuleGoal);
}

MutableHandle<ScriptStencil> FunctionBox::functionStencil() const {
  if (isTopLevel_ == TopLevelFunction::Yes) {
    return &compilationInfo_.topLevel;
  }
  return compilationInfo_.funcData[funcDataIndex_];
}

void SharedContext::copyScriptFields(ScriptStencil& stencil) {
  MOZ_ASSERT(!isScriptFieldCopiedToStencil);

  stencil.immutableFlags = immutableFlags_;
  stencil.extent = extent_;

  isScriptFieldCopiedToStencil = true;
}

void FunctionBox::finishScriptFlags() {
  MOZ_ASSERT(!isScriptFieldCopiedToStencil);

  using ImmutableFlags = ImmutableScriptFlagsEnum;
  immutableFlags_.setFlag(ImmutableFlags::HasMappedArgsObj, hasMappedArgsObj());
  immutableFlags_.setFlag(ImmutableFlags::IsLikelyConstructorWrapper,
                          isLikelyConstructorWrapper());
}

void FunctionBox::copyScriptFields(ScriptStencil& stencil) {
  MOZ_ASSERT(&stencil == &functionStencil().get());
  MOZ_ASSERT(!isAsmJSModule());

  SharedContext::copyScriptFields(stencil);

  stencil.memberInitializers = memberInitializers_;

  isScriptFieldCopiedToStencil = true;
}

void FunctionBox::copyFunctionFields(ScriptStencil& stencil) {
  MOZ_ASSERT(&stencil == &functionStencil().get());
  MOZ_ASSERT(!isFunctionFieldCopiedToStencil);

  stencil.functionAtom = atom_;
  stencil.functionFlags = flags_;
  stencil.nargs = nargs_;
  stencil.lazyFunctionEnclosingScopeIndex_ = enclosingScopeIndex_;
  stencil.isStandaloneFunction = isStandalone_;
  stencil.wasFunctionEmitted = wasEmitted_;
  stencil.isSingletonFunction = isSingleton_;

  isFunctionFieldCopiedToStencil = true;
}

void FunctionBox::copyUpdatedImmutableFlags() {
  ScriptStencil& stencil = functionStencil().get();
  stencil.immutableFlags = immutableFlags_;
}

void FunctionBox::copyUpdatedExtent() {
  ScriptStencil& stencil = functionStencil().get();
  stencil.extent = extent_;
}

void FunctionBox::copyUpdatedMemberInitializers() {
  ScriptStencil& stencil = functionStencil().get();
  stencil.memberInitializers = memberInitializers_;
}

void FunctionBox::copyUpdatedEnclosingScopeIndex() {
  ScriptStencil& stencil = functionStencil().get();
  stencil.lazyFunctionEnclosingScopeIndex_ = enclosingScopeIndex_;
}

void FunctionBox::copyUpdatedAtomAndFlags() {
  ScriptStencil& stencil = functionStencil().get();
  stencil.functionAtom = atom_;
  stencil.functionFlags = flags_;
}

void FunctionBox::copyUpdatedWasEmitted() {
  ScriptStencil& stencil = functionStencil().get();
  stencil.wasFunctionEmitted = wasEmitted_;
}

void FunctionBox::copyUpdatedIsSingleton() {
  ScriptStencil& stencil = functionStencil().get();
  stencil.isSingletonFunction = isSingleton_;
}

}  // namespace frontend
}  // namespace js
