/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/SharedContext.h"

#include "frontend/AbstractScopePtr.h"
#include "frontend/FunctionSyntaxKind.h"  // FunctionSyntaxKind
#include "frontend/ModuleSharedContext.h"
#include "vm/FunctionFlags.h"          // js::FunctionFlags
#include "vm/GeneratorAndAsyncKind.h"  // js::GeneratorKind, js::FunctionAsyncKind
#include "wasm/AsmJS.h"

#include "frontend/ParseContext-inl.h"
#include "vm/EnvironmentObject-inl.h"

namespace js {
namespace frontend {

SharedContext::SharedContext(JSContext* cx, Kind kind,
                             CompilationInfo& compilationInfo,
                             Directives directives, SourceExtent extent)
    : cx_(cx),
      compilationInfo_(compilationInfo),
      extent(extent),
      allowNewTarget_(false),
      allowSuperProperty_(false),
      allowSuperCall_(false),
      allowArguments_(true),
      inWith_(false),
      needsThisTDZChecks_(false),
      localStrict(false),
      hasExplicitUseStrict_(false) {
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
                                     Directives directives, SourceExtent extent)
    : SharedContext(cx, Kind::Eval, compilationInfo, directives, extent),
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
                         SourceExtent extent, CompilationInfo& compilationInfo,
                         Directives directives, GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind, JSAtom* explicitName,
                         FunctionFlags flags, size_t index)
    : SharedContext(cx, Kind::FunctionBox, compilationInfo, directives, extent),
      traceLink_(traceListHead),
      explicitName_(explicitName),
      funcDataIndex_(index),
      flags_(flags),
      emitBytecode(false),
      emitLazy(false),
      wasEmitted(false),
      exposeScript(false),
      isAnnexB(false),
      useAsm(false),
      isAsmJSModule_(false),
      hasParameterExprs(false),
      hasDestructuringArgs(false),
      hasDuplicateParameters(false),
      hasExprBody_(false),
      usesApply(false),
      usesThis(false),
      usesReturn(false) {
  setFlag(ImmutableFlags::IsGenerator,
          generatorKind == GeneratorKind::Generator);
  setFlag(ImmutableFlags::IsAsync,
          asyncKind == FunctionAsyncKind::AsyncFunction);
}

JSFunction* FunctionBox::createFunction(JSContext* cx) {
  RootedObject proto(cx);
  if (!GetFunctionPrototype(cx, generatorKind(), asyncKind(), &proto)) {
    return nullptr;
  }

  RootedAtom atom(cx, explicitName());
  gc::AllocKind allocKind = flags_.isExtended()
                                ? gc::AllocKind::FUNCTION_EXTENDED
                                : gc::AllocKind::FUNCTION;

  return NewFunctionWithProto(cx, nullptr, nargs_, flags_, nullptr, atom, proto,
                              allocKind, TenuredObject);
}

bool FunctionBox::hasFunctionStencil() const {
  return compilationInfo_.funcData[funcDataIndex_]
      .get()
      .is<ScriptStencilBase>();
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
                                                FunctionFlags flags,
                                                FunctionSyntaxKind kind) {
  SharedContext* sc = enclosing->sc();
  useAsm = sc->isFunctionBox() && sc->asFunctionBox()->useAsmOrInsideUseAsm();

  // Arrow functions don't have their own `this` binding.
  if (flags.isArrow()) {
    allowNewTarget_ = sc->allowNewTarget();
    allowSuperProperty_ = sc->allowSuperProperty();
    allowSuperCall_ = sc->allowSuperCall();
    allowArguments_ = sc->allowArguments();
    needsThisTDZChecks_ = sc->needsThisTDZChecks();
    thisBinding_ = sc->thisBinding();
  } else {
    allowNewTarget_ = true;
    allowSuperProperty_ = flags.allowSuperProperty();

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
                                       FunctionFlags flags) {
  this->initWithEnclosingParseContext(enclosing, flags,
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
    function()->baseScript()->initTreatAsRunOnce(treatAsRunOnce());

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
                                         ModuleBuilder& builder,
                                         SourceExtent extent)
    : SharedContext(cx, Kind::Module, compilationInfo, Directives(true),
                    extent),
      module_(cx, module),
      enclosingScope_(cx, enclosingScope),
      bindings(cx),
      builder(builder) {
  thisBinding_ = ThisBinding::Module;
  setFlag(ImmutableFlags::HasModuleGoal);
}

MutableHandle<ScriptStencilBase> FunctionBox::functionStencil() const {
  return compilationInfo_.funcData[funcDataIndex_].as<ScriptStencilBase>();
}

}  // namespace frontend
}  // namespace js
