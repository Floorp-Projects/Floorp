/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/SharedContext.h"

#include "frontend/AbstractScopePtr.h"
#include "frontend/ModuleSharedContext.h"
#include "wasm/AsmJS.h"

#include "frontend/ParseContext-inl.h"
#include "vm/EnvironmentObject-inl.h"

namespace js {
namespace frontend {

void SharedContext::computeAllowSyntax(Scope* scope) {
  for (ScopeIter si(scope); si; si++) {
    if (si.kind() == ScopeKind::Function) {
      FunctionScope* funScope = &si.scope()->as<FunctionScope>();
      JSFunction* fun = funScope->canonicalFunction();
      if (fun->isArrow()) {
        continue;
      }
      allowNewTarget_ = true;
      allowSuperProperty_ = fun->allowSuperProperty();
      allowSuperCall_ = fun->isDerivedClassConstructor();
      if (funScope->isFieldInitializer() == IsFieldInitializer::Yes) {
        allowSuperCall_ = false;
        allowArguments_ = false;
      }
      return;
    }
  }
}

void SharedContext::computeThisBinding(Scope* scope) {
  for (ScopeIter si(scope); si; si++) {
    if (si.kind() == ScopeKind::Module) {
      thisBinding_ = ThisBinding::Module;
      return;
    }

    if (si.kind() == ScopeKind::Function) {
      JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();

      // Arrow functions don't have their own `this` binding.
      if (fun->isArrow()) {
        continue;
      }

      // Derived class constructors (including nested arrow functions and
      // eval) need TDZ checks when accessing |this|.
      if (fun->isDerivedClassConstructor()) {
        needsThisTDZChecks_ = true;
      }

      thisBinding_ = ThisBinding::Function;
      return;
    }
  }

  thisBinding_ = ThisBinding::Global;
}

void SharedContext::computeInWith(Scope* scope) {
  for (ScopeIter si(scope); si; si++) {
    if (si.kind() == ScopeKind::With) {
      inWith_ = true;
      break;
    }
  }
}

EvalSharedContext::EvalSharedContext(JSContext* cx, JSObject* enclosingEnv,
                                     CompilationInfo& compilationInfo,
                                     Scope* enclosingScope,
                                     Directives directives)
    : SharedContext(cx, Kind::Eval, compilationInfo, directives),
      enclosingScope_(cx, enclosingScope),
      bindings(cx) {
  computeAllowSyntax(enclosingScope);
  computeInWith(enclosingScope);
  computeThisBinding(enclosingScope);

  // If this eval is in response to Debugger.Frame.eval, we may have been
  // passed an incomplete scope chain. In order to better determine the 'this'
  // binding type, we traverse the environment chain, looking for a CallObject
  // and recompute the binding type based on its body scope.
  //
  // NOTE: A non-debug eval in a non-syntactic environment will also trigger
  // this code. In that case, we should still compute the same binding type.
  if (enclosingEnv && enclosingScope->hasOnChain(ScopeKind::NonSyntactic)) {
    JSObject* env = enclosingEnv;
    while (env) {
      // Look at target of any DebugEnvironmentProxy, but be sure to use
      // enclosingEnvironment() of the proxy itself.
      JSObject* unwrapped = env;
      if (env->is<DebugEnvironmentProxy>()) {
        unwrapped = &env->as<DebugEnvironmentProxy>().environment();
      }

      if (unwrapped->is<CallObject>()) {
        JSFunction* callee = &unwrapped->as<CallObject>().callee();
        computeThisBinding(callee->nonLazyScript()->bodyScope());
        break;
      }

      env = env->enclosingEnvironment();
    }
  }
}

#ifdef DEBUG
bool FunctionBox::atomsAreKept() { return cx_->zone()->hasKeptAtoms(); }
#endif

FunctionBox::FunctionBox(JSContext* cx, FunctionBox* traceListHead,
                         uint32_t toStringStart,
                         CompilationInfo& compilationInfo,
                         Directives directives, GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind, JSAtom* explicitName,
                         FunctionFlags flags, size_t index)
    : SharedContext(cx, Kind::FunctionBox, compilationInfo, directives),
      traceLink_(traceListHead),
      emitLink_(nullptr),
      enclosingScope_(),
      namedLambdaBindings_(nullptr),
      functionScopeBindings_(nullptr),
      extraVarScopeBindings_(nullptr),
      funcDataIndex_(index),
      functionNode(nullptr),
      extent{0, 0, toStringStart, 0, 1, 0},
      length(0),
      hasDestructuringArgs(false),
      hasParameterExprs(false),
      hasDuplicateParameters(false),
      useAsm(false),
      isAnnexB(false),
      wasEmitted(false),
      emitBytecode(false),
      usesArguments(false),
      usesApply(false),
      usesThis(false),
      usesReturn(false),
      hasExprBody_(false),
      isAsmJSModule_(false),
      nargs_(0),
      explicitName_(explicitName),
      flags_(flags) {
  immutableFlags_.setFlag(ImmutableFlags::IsGenerator,
                          generatorKind == GeneratorKind::Generator);
  immutableFlags_.setFlag(ImmutableFlags::IsAsync,
                          asyncKind == FunctionAsyncKind::AsyncFunction);
  immutableFlags_.setFlag(ImmutableFlags::IsFunction);
}

bool FunctionBox::hasFunctionCreationData() const {
  return compilationInfo_.funcData[funcDataIndex_]
      .get()
      .is<FunctionCreationData>();
}

bool FunctionBox::hasFunction() const {
  return compilationInfo_.funcData[funcDataIndex_].get().is<JSFunction*>();
}

void FunctionBox::initFromLazyFunction(JSFunction* fun) {
  BaseScript* lazy = fun->baseScript();
  immutableFlags_ = lazy->immutableFlags();
  extent = lazy->extent();

  if (fun->isClassConstructor()) {
    fieldInitializers = mozilla::Some(lazy->getFieldInitializers());
  }
}

void FunctionBox::initStandaloneFunction(Scope* enclosingScope) {
  // Standalone functions are Function or Generator constructors and are
  // always scoped to the global.
  MOZ_ASSERT(enclosingScope->is<GlobalScope>());
  enclosingScope_ = AbstractScopePtr(enclosingScope);
  allowNewTarget_ = true;
  thisBinding_ = ThisBinding::Function;
}

void FunctionBox::initWithEnclosingParseContext(ParseContext* enclosing,
                                                FunctionSyntaxKind kind,
                                                bool isArrow,
                                                bool allowSuperProperty) {
  SharedContext* sc = enclosing->sc();
  useAsm = sc->isFunctionBox() && sc->asFunctionBox()->useAsmOrInsideUseAsm();

  // Arrow functions don't have their own `this` binding.
  if (isArrow) {
    allowNewTarget_ = sc->allowNewTarget();
    allowSuperProperty_ = sc->allowSuperProperty();
    allowSuperCall_ = sc->allowSuperCall();
    allowArguments_ = sc->allowArguments();
    needsThisTDZChecks_ = sc->needsThisTDZChecks();
    thisBinding_ = sc->thisBinding();
  } else {
    allowNewTarget_ = true;
    allowSuperProperty_ = allowSuperProperty;

    if (IsConstructorKind(kind)) {
      auto stmt =
          enclosing->findInnermostStatement<ParseContext::ClassStatement>();
      MOZ_ASSERT(stmt);
      stmt->constructorBox = this;

      if (kind == FunctionSyntaxKind::DerivedClassConstructor) {
        setDerivedClassConstructor();
        allowSuperCall_ = true;
        needsThisTDZChecks_ = true;
      }
    }

    thisBinding_ = ThisBinding::Function;
  }

  // We inherit the parse goal from our top-level.
  setHasModuleGoal(sc->hasModuleGoal());

  if (sc->inWith()) {
    inWith_ = true;
  } else {
    auto isWith = [](ParseContext::Statement* stmt) {
      return stmt->kind() == StatementKind::With;
    };

    inWith_ = enclosing->findInnermostStatement(isWith);
  }
}

void FunctionBox::initFieldInitializer(ParseContext* enclosing,
                                       Handle<FunctionCreationData> data) {
  this->initWithEnclosingParseContext(enclosing, data,
                                      FunctionSyntaxKind::Method);
  allowArguments_ = false;
}

void FunctionBox::initWithEnclosingScope(JSFunction* fun) {
  Scope* enclosingScope = fun->enclosingScope();
  MOZ_ASSERT(enclosingScope);

  if (!isArrow()) {
    allowNewTarget_ = true;
    allowSuperProperty_ = fun->allowSuperProperty();

    if (isDerivedClassConstructor()) {
      setDerivedClassConstructor();
      allowSuperCall_ = true;
      needsThisTDZChecks_ = true;
    }

    thisBinding_ = ThisBinding::Function;
  } else {
    computeAllowSyntax(enclosingScope);
    computeThisBinding(enclosingScope);
  }

  computeInWith(enclosingScope);

  enclosingScope_ = AbstractScopePtr(enclosingScope);
}

void FunctionBox::setEnclosingScopeForInnerLazyFunction(
    const AbstractScopePtr& enclosingScope) {
  // For lazy functions inside a function which is being compiled, we cache
  // the incomplete scope object while compiling, and store it to the
  // BaseScript once the enclosing script successfully finishes compilation
  // in FunctionBox::finish.
  MOZ_ASSERT(!enclosingScope_);
  enclosingScope_ = enclosingScope;
}

void FunctionBox::setAsmJSModule(JSFunction* function) {
  MOZ_ASSERT(IsAsmJSModule(function));
  isAsmJSModule_ = true;
  clobberFunction(function);
}

void FunctionBox::finish() {
  if (!emitBytecode) {
    // Apply updates from FunctionEmitter::emitLazy().
    function()->setEnclosingScope(enclosingScope_.getExistingScope());
    function()->baseScript()->setTreatAsRunOnce(treatAsRunOnce());

    if (fieldInitializers) {
      function()->baseScript()->setFieldInitializers(*fieldInitializers);
    }
  } else {
    // Non-lazy inner functions don't use the enclosingScope_ field.
    MOZ_ASSERT(!enclosingScope_);
  }
}

/* static */
void FunctionBox::TraceList(JSTracer* trc, FunctionBox* listHead) {
  for (FunctionBox* node = listHead; node; node = node->traceLink_) {
    node->trace(trc);
  }
}

void FunctionBox::trace(JSTracer* trc) {
  if (enclosingScope_) {
    enclosingScope_.trace(trc);
  }
  if (explicitName_) {
    TraceRoot(trc, &explicitName_, "funbox-explicitName");
  }
}

JSFunction* FunctionBox::function() const {
  return compilationInfo_.funcData[funcDataIndex_].as<JSFunction*>();
}

void FunctionBox::clobberFunction(JSFunction* function) {
  compilationInfo_.funcData[funcDataIndex_].set(mozilla::AsVariant(function));
  // After clobbering, these flags need to be updated
  setIsInterpreted(function->isInterpreted());
}

ModuleSharedContext::ModuleSharedContext(JSContext* cx, ModuleObject* module,
                                         CompilationInfo& compilationInfo,
                                         Scope* enclosingScope,
                                         ModuleBuilder& builder)
    : SharedContext(cx, Kind::Module, compilationInfo, Directives(true)),
      module_(cx, module),
      enclosingScope_(cx, enclosingScope),
      bindings(cx),
      builder(builder) {
  thisBinding_ = ThisBinding::Module;
  immutableFlags_.setFlag(ImmutableFlags::HasModuleGoal);
}

MutableHandle<FunctionCreationData> FunctionBox::functionCreationData() const {
  MOZ_ASSERT(hasFunctionCreationData());
  // Marked via CompilationData::funcData rooting.
  return MutableHandle<FunctionCreationData>::fromMarkedLocation(
      &compilationInfo_.funcData[funcDataIndex_]
           .get()
           .as<FunctionCreationData>());
}

}  // namespace frontend
}  // namespace js
