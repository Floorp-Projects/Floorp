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

ScopeStencil& AbstractScopePtr::scopeData() const {
  const Deferred& data = scope_.as<Deferred>();
  return data.compilationState.scopeData[data.index];
}

CompilationState& AbstractScopePtr::compilationState() const {
  const Deferred& data = scope_.as<Deferred>();
  return data.compilationState;
}

ScopeKind AbstractScopePtr::kind() const {
  MOZ_ASSERT(!isNullptr());
  if (isScopeStencil()) {
    return scopeData().kind();
  }
  return scope()->kind();
}

AbstractScopePtr AbstractScopePtr::enclosing() const {
  MOZ_ASSERT(!isNullptr());
  if (isScopeStencil()) {
    return scopeData().enclosing(compilationState());
  }
  return AbstractScopePtr(scope()->enclosing());
}

bool AbstractScopePtr::hasEnvironment() const {
  MOZ_ASSERT(!isNullptr());
  if (isScopeStencil()) {
    return scopeData().hasEnvironment();
  }
  return scope()->hasEnvironment();
}

bool AbstractScopePtr::isArrow() const {
  // nullptr will also fail the below assert, so effectively also checking
  // !isNullptr()
  MOZ_ASSERT(is<FunctionScope>());
  if (isScopeStencil()) {
    return scopeData().isArrow();
  }
  MOZ_ASSERT(scope()->as<FunctionScope>().canonicalFunction());
  return scope()->as<FunctionScope>().canonicalFunction()->isArrow();
}

void AbstractScopePtr::trace(JSTracer* trc) {
  JS::GCPolicy<ScopeType>::trace(trc, &scope_, "AbstractScopePtr");
}
