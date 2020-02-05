/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/SharedContext.h"

#include "frontend/AbstractScope.h"
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
                                     CompilationInfo& compilationInfo,
                                     Scope* enclosingScope,
                                     Directives directives, bool extraWarnings)
    : SharedContext(cx, Kind::Eval, compilationInfo, directives, extraWarnings),
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
                         uint32_t toStringStart,
                         CompilationInfo& compilationInfo,
                         Directives directives, bool extraWarnings,
                         GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind, JSAtom* explicitName,
                         FunctionFlags flags)
    : ObjectBox(nullptr, traceListHead, TraceListNode::NodeType::Function),
      SharedContext(cx, Kind::FunctionBox, compilationInfo, directives,
                    extraWarnings),
      enclosingScope_(),
      namedLambdaBindings_(nullptr),
      functionScopeBindings_(nullptr),
      extraVarScopeBindings_(nullptr),
      functionNode(nullptr),
      sourceStart(0),
      sourceEnd(0),
      startLine(1),
      startColumn(0),
      toStringStart(toStringStart),
      toStringEnd(0),
      length(0),
      isGenerator_(generatorKind == GeneratorKind::Generator),
      isAsync_(asyncKind == FunctionAsyncKind::AsyncFunction),
      hasDestructuringArgs(false),
      hasParameterExprs(false),
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
      nargs_(0),
      explicitName_(explicitName),
      flags_(flags) {}

FunctionBox::FunctionBox(JSContext* cx, TraceListNode* traceListHead,
                         JSFunction* fun, uint32_t toStringStart,
                         CompilationInfo& compilationInfo,
                         Directives directives, bool extraWarnings,
                         GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind)
    : FunctionBox(cx, traceListHead, toStringStart, compilationInfo, directives,
                  extraWarnings, generatorKind, asyncKind, fun->explicitName(),
                  fun->flags()) {
  gcThing = fun;
  // Functions created at parse time may be set singleton after parsing and
  // baked into JIT code, so they must be allocated tenured. They are held by
  // the JSScript so cannot be collected during a minor GC anyway.
  MOZ_ASSERT(fun->isTenured());
}

FunctionBox::FunctionBox(JSContext* cx, TraceListNode* traceListHead,
                         Handle<FunctionCreationData> data,
                         uint32_t toStringStart,
                         CompilationInfo& compilationInfo,
                         Directives directives, bool extraWarnings,
                         GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind)
    : FunctionBox(cx, traceListHead, toStringStart, compilationInfo, directives,
                  extraWarnings, generatorKind, asyncKind, data.get().atom,
                  data.get().flags) {
  functionCreationData_.emplace(data);
}

void FunctionBox::initFromLazyFunction(JSFunction* fun) {
  BaseScript* lazy = fun->baseScript();
  if (lazy->isDerivedClassConstructor()) {
    setDerivedClassConstructor();
  }
  if (lazy->needsHomeObject()) {
    setNeedsHomeObject();
  }
  if (lazy->bindingsAccessedDynamically()) {
    setBindingsAccessedDynamically();
  }
  if (lazy->hasDirectEval()) {
    setHasDirectEval();
  }
  if (lazy->hasModuleGoal()) {
    setHasModuleGoal();
  }

  sourceStart = lazy->sourceStart();
  sourceEnd = lazy->sourceEnd();
  toStringStart = lazy->toStringStart();
  toStringEnd = lazy->toStringEnd();
  startLine = lazy->lineno();
  startColumn = lazy->column();
}

void FunctionBox::initStandaloneFunction(Scope* enclosingScope) {
  // Standalone functions are Function or Generator constructors and are
  // always scoped to the global.
  MOZ_ASSERT(enclosingScope->is<GlobalScope>());
  enclosingScope_ = AbstractScope(enclosingScope);
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
  hasModuleGoal_ = sc->hasModuleGoal();

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
                                       Handle<FunctionCreationData> data,
                                       HasHeritage hasHeritage) {
  this->initWithEnclosingParseContext(enclosing, data,
                                      FunctionSyntaxKind::Expression);
  allowSuperProperty_ = true;
  allowSuperCall_ = false;
  allowArguments_ = false;
  needsThisTDZChecks_ = hasHeritage == HasHeritage::Yes;
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

  enclosingScope_ = AbstractScope(enclosingScope);
}

void FunctionBox::setEnclosingScopeForInnerLazyFunction(
    const AbstractScope& enclosingScope) {
  // For lazy functions inside a function which is being compiled, we cache
  // the incomplete scope object while compiling, and store it to the
  // LazyScript once the enclosing script successfully finishes compilation
  // in FunctionBox::finish.
  MOZ_ASSERT(!enclosingScope_);
  enclosingScope_ = enclosingScope;
}

void FunctionBox::finish() {
  if (isInterpretedLazy()) {
    // Lazy inner functions need to record their enclosing scope for when they
    // eventually are compiled.
    function()->setEnclosingScope(enclosingScope_.getExistingScope());
  } else {
    // Non-lazy inner functions don't use the enclosingScope_ field.
    MOZ_ASSERT(!enclosingScope_);
  }
}

ModuleSharedContext::ModuleSharedContext(JSContext* cx, ModuleObject* module,
                                         CompilationInfo& compilationInfo,
                                         Scope* enclosingScope,
                                         ModuleBuilder& builder)
    : SharedContext(cx, Kind::Module, compilationInfo, Directives(true), false),
      module_(cx, module),
      enclosingScope_(cx, enclosingScope),
      bindings(cx),
      builder(builder) {
  thisBinding_ = ThisBinding::Module;
  hasModuleGoal_ = true;
}

}  // namespace frontend

}  // namespace js
