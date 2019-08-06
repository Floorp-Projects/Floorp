/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/SharedContext.h"

#include "frontend/ModuleSharedContext.h"

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
                                     Scope* enclosingScope,
                                     Directives directives, bool extraWarnings)
    : SharedContext(cx, Kind::Eval, directives, extraWarnings),
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

FunctionBox::FunctionBox(JSContext* cx, TraceListNode* traceListHead,
                         uint32_t toStringStart, Directives directives,
                         bool extraWarnings, GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind, bool isArrow,
                         bool isNamedLambda, bool isGetter, bool isSetter,
                         bool isMethod, bool isInterpreted,
                         bool isInterpretedLazy,
                         FunctionFlags::FunctionKind kind, JSAtom* explicitName)
    : ObjectBox(traceListHead),
      SharedContext(cx, Kind::FunctionBox, directives, extraWarnings),
      enclosingScope_(nullptr),
      namedLambdaBindings_(nullptr),
      functionScopeBindings_(nullptr),
      extraVarScopeBindings_(nullptr),
      functionNode(nullptr),
      bufStart(0),
      bufEnd(0),
      startLine(1),
      startColumn(0),
      toStringStart(toStringStart),
      toStringEnd(0),
      length(0),
      isGenerator_(generatorKind == GeneratorKind::Generator),
      isAsync_(asyncKind == FunctionAsyncKind::AsyncFunction),
      hasDestructuringArgs(false),
      hasParameterExprs(false),
      hasDirectEvalInParameterExpr(false),
      hasDuplicateParameters(false),
      useAsm(false),
      isAnnexB(false),
      wasEmitted(false),
      declaredArguments(false),
      usesArguments(false),
      usesApply(false),
      usesThis(false),
      usesReturn(false),
      hasRest_(false),
      hasExprBody_(false),
      hasExtensibleScope_(false),
      argumentsHasLocalBinding_(false),
      definitelyNeedsArgsObj_(false),
      needsHomeObject_(false),
      isDerivedClassConstructor_(false),
      hasThisBinding_(false),
      hasInnerFunctions_(false),
      isArrow_(isArrow),
      isNamedLambda_(isNamedLambda),
      isGetter_(isGetter),
      isSetter_(isSetter),
      isMethod_(isMethod),
      isInterpreted_(isInterpreted),
      isInterpretedLazy_(isInterpretedLazy),
      kind_(kind),
      explicitName_(explicitName),
      nargs_(0) {}

FunctionBox::FunctionBox(JSContext* cx, TraceListNode* traceListHead,
                         JSFunction* fun, uint32_t toStringStart,
                         Directives directives, bool extraWarnings,
                         GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind)
    : FunctionBox(cx, traceListHead, toStringStart, directives, extraWarnings,
                  generatorKind, asyncKind, fun->isArrow(),
                  fun->isNamedLambda(), fun->isGetter(), fun->isSetter(),
                  fun->isMethod(), fun->isInterpreted(),
                  fun->isInterpretedLazy(), fun->kind(), fun->explicitName()) {
  gcThing = fun;
  // Functions created at parse time may be set singleton after parsing and
  // baked into JIT code, so they must be allocated tenured. They are held by
  // the JSScript so cannot be collected during a minor GC anyway.
  MOZ_ASSERT(fun->isTenured());
}

FunctionBox::FunctionBox(JSContext* cx, TraceListNode* traceListHead,
                         FunctionCreationData& data, uint32_t toStringStart,
                         Directives directives, bool extraWarnings,
                         GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind)
    : FunctionBox(cx, traceListHead, toStringStart, directives, extraWarnings,
                  generatorKind, asyncKind, data.flags.isArrow(),
                  data.isNamedLambda(), data.flags.isGetter(),
                  data.flags.isSetter(), data.flags.isMethod(),
                  data.flags.isInterpreted(), data.flags.isInterpretedLazy(),
                  data.flags.kind(), data.atom) {
  functionCreationData_.emplace(data);
}

void FunctionBox::initFromLazyFunction(JSFunction* fun) {
  if (fun->lazyScript()->isDerivedClassConstructor()) {
    setDerivedClassConstructor();
  }
  if (fun->lazyScript()->needsHomeObject()) {
    setNeedsHomeObject();
  }
  if (fun->lazyScript()->hasEnclosingScope()) {
    enclosingScope_ = fun->lazyScript()->enclosingScope();
  } else {
    enclosingScope_ = nullptr;
  }
  initWithEnclosingScope(enclosingScope_, fun);
}

void FunctionBox::initStandaloneFunction(Scope* enclosingScope) {
  // Standalone functions are Function or Generator constructors and are
  // always scoped to the global.
  MOZ_ASSERT(enclosingScope->is<GlobalScope>());
  enclosingScope_ = enclosingScope;
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
                                       FunctionCreationData& data,
                                       HasHeritage hasHeritage) {
  this->initWithEnclosingParseContext(enclosing, data,
                                      FunctionSyntaxKind::Expression);
  allowSuperProperty_ = true;
  allowSuperCall_ = false;
  allowArguments_ = false;
  needsThisTDZChecks_ = hasHeritage == HasHeritage::Yes;
}

void FunctionBox::initWithEnclosingScope(Scope* enclosingScope,
                                         JSFunction* fun) {
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
}

void FunctionBox::setEnclosingScopeForInnerLazyFunction(Scope* enclosingScope) {
  MOZ_ASSERT(isLazyFunctionWithoutEnclosingScope());

  // For lazy functions inside a function which is being compiled, we cache
  // the incomplete scope object while compiling, and store it to the
  // LazyScript once the enclosing script successfully finishes compilation
  // in FunctionBox::finish.
  enclosingScope_ = enclosingScope;
}

void FunctionBox::finish() {
  if (!isLazyFunctionWithoutEnclosingScope()) {
    return;
  }
  MOZ_ASSERT(enclosingScope_);
  function()->lazyScript()->setEnclosingScope(enclosingScope_);
}

ModuleSharedContext::ModuleSharedContext(JSContext* cx, ModuleObject* module,
                                         Scope* enclosingScope,
                                         ModuleBuilder& builder)
    : SharedContext(cx, Kind::Module, Directives(true), false),
      module_(cx, module),
      enclosingScope_(cx, enclosingScope),
      bindings(cx),
      builder(builder) {
  thisBinding_ = ThisBinding::Module;
}

}  // namespace frontend

}  // namespace js
