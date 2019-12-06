/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/AbstractScope.h"

#include "mozilla/Maybe.h"

#include "js/GCPolicyAPI.h"
#include "vm/JSFunction.h"
#include "vm/Scope.h"

using namespace js;
using namespace js::frontend;

Scope* AbstractScope::maybeScope() const { return scope_; }

ScopeKind AbstractScope::kind() const {
  MOZ_ASSERT(maybeScope());
  return maybeScope()->kind();
}

AbstractScope AbstractScope::enclosing() const {
  MOZ_ASSERT(maybeScope());
  return AbstractScope(maybeScope()->enclosing());
}

bool AbstractScope::hasEnvironment() const {
  MOZ_ASSERT(maybeScope());
  return maybeScope()->hasEnvironment();
}

bool AbstractScope::isArrow() const { return canonicalFunction()->isArrow(); }

JSFunction* AbstractScope::canonicalFunction() const {
  MOZ_ASSERT(is<FunctionScope>());
  MOZ_ASSERT(maybeScope());
  return maybeScope()->as<FunctionScope>().canonicalFunction();
}

void AbstractScope::trace(JSTracer* trc) {
  JS::GCPolicy<ScopeType>::trace(trc, &scope_, "AbstractScope");
}

bool AbstractScopeIter::hasSyntacticEnvironment() const {
  return abstractScope().hasEnvironment() &&
         abstractScope().kind() != ScopeKind::NonSyntactic;
}