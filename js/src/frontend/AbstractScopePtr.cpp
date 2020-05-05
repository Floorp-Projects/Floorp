/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/AbstractScopePtr.h"

#include "mozilla/Maybe.h"

#include "frontend/CompilationInfo.h"
#include "frontend/Stencil.h"
#include "js/GCPolicyAPI.h"
#include "js/GCVariant.h"

using namespace js;
using namespace js::frontend;

MutableHandle<ScopeCreationData> AbstractScopePtr::scopeCreationData() const {
  const Deferred& data = scope_.as<Deferred>();
  return data.compilationInfo.scopeCreationData[data.index.index];
}

// This is used during allocation of the scopes to ensure that we only
// allocate GC scopes with GC-enclosing scopes. This can recurse through
// the scope chain.
//
// Once all ScopeCreation for a compilation tree is centralized, this
// will go away, to be replaced with a single top down GC scope allocation.
//
// This uses an outparam to disambiguate between the case where we have a
// real nullptr scope and we failed to allocate a new scope because of OOM.
bool AbstractScopePtr::getOrCreateScope(JSContext* cx,
                                        MutableHandleScope scope) {
  if (isScopeCreationData()) {
    MutableHandle<ScopeCreationData> scd = scopeCreationData();
    if (scd.get().hasScope()) {
      scope.set(scd.get().getScope());
      return true;
    }

    scope.set(scd.get().createScope(cx));
    return scope;
  }

  scope.set(this->scope());
  return true;
}

Scope* AbstractScopePtr::getExistingScope() const {
  if (scope_.is<HeapPtrScope>()) {
    return scope_.as<HeapPtrScope>();
  }
  MOZ_ASSERT(isScopeCreationData());
  // This should only be called post-reification, as it needs to return a real
  // Scope* unless it represents nullptr (in which case the variant should be
  // in HeapPtrScope and handled above)
  MOZ_ASSERT(scopeCreationData().get().getScope());
  return scopeCreationData().get().getScope();
}

ScopeKind AbstractScopePtr::kind() const {
  MOZ_ASSERT(!isNullptr());
  if (isScopeCreationData()) {
    return scopeCreationData().get().kind();
  }
  return scope()->kind();
}

AbstractScopePtr AbstractScopePtr::enclosing() const {
  MOZ_ASSERT(!isNullptr());
  if (isScopeCreationData()) {
    return scopeCreationData().get().enclosing();
  }
  return AbstractScopePtr(scope()->enclosing());
}

bool AbstractScopePtr::hasEnvironment() const {
  MOZ_ASSERT(!isNullptr());
  if (isScopeCreationData()) {
    return scopeCreationData().get().hasEnvironment();
  }
  return scope()->hasEnvironment();
}

bool AbstractScopePtr::isArrow() const {
  // nullptr will also fail the below assert, so effectively also checking
  // !isNullptr()
  MOZ_ASSERT(is<FunctionScope>());
  if (isScopeCreationData()) {
    return scopeCreationData().get().isArrow();
  }
  return scope()->as<FunctionScope>().canonicalFunction()->isArrow();
}

bool AbstractScopePtr::isClassConstructor() const {
  MOZ_ASSERT(is<FunctionScope>());
  if (isScopeCreationData()) {
    return scopeCreationData().get().isClassConstructor();
  }
  return scope()->as<FunctionScope>().canonicalFunction()->isClassConstructor();
}

const FieldInitializers& AbstractScopePtr::fieldInitializers() const {
  MOZ_ASSERT(is<FunctionScope>());
  if (isScopeCreationData()) {
    return scopeCreationData().get().fieldInitializers();
  }
  return scope()
      ->as<FunctionScope>()
      .canonicalFunction()
      ->baseScript()
      ->getFieldInitializers();
}

uint32_t AbstractScopePtr::nextFrameSlot() const {
  if (isScopeCreationData()) {
    return scopeCreationData().get().nextFrameSlot();
  }

  switch (kind()) {
    case ScopeKind::Function:
      return scope()->as<FunctionScope>().nextFrameSlot();
    case ScopeKind::FunctionBodyVar:
      return scope()->as<VarScope>().nextFrameSlot();
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::FunctionLexical:
      return scope()->as<LexicalScope>().nextFrameSlot();
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
      // Named lambda scopes cannot have frame slots.
      return 0;
    case ScopeKind::Eval:
    case ScopeKind::StrictEval:
      return scope()->as<EvalScope>().nextFrameSlot();
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic:
      return 0;
    case ScopeKind::Module:
      return scope()->as<ModuleScope>().nextFrameSlot();
    case ScopeKind::WasmInstance:
      MOZ_CRASH("WasmInstanceScope doesn't have nextFrameSlot()");
      return 0;
    case ScopeKind::WasmFunction:
      MOZ_CRASH("WasmFunctionScope doesn't have nextFrameSlot()");
      return 0;
    case ScopeKind::With:
      MOZ_CRASH("With Scopes don't get nextFrameSlot()");
      return 0;
  }
  MOZ_CRASH("Not an enclosing intra-frame scope");
}

void AbstractScopePtr::trace(JSTracer* trc) {
  JS::GCPolicy<ScopeType>::trace(trc, &scope_, "AbstractScopePtr");
}

bool AbstractScopePtrIter::hasSyntacticEnvironment() const {
  return abstractScopePtr().hasEnvironment() &&
         abstractScopePtr().kind() != ScopeKind::NonSyntactic;
}
