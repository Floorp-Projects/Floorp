/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_AbstractScope_h
#define frontend_AbstractScope_h

#include "mozilla/Variant.h"

#include "gc/Barrier.h"
#include "gc/Rooting.h"
#include "gc/Tracer.h"
#include "vm/Scope.h"

#include "vm/ScopeKind.h"  // For ScopeKind

namespace js {
class Scope;
class GlobalScope;
class EvalScope;
class GCMarker;
using HeapPtrScope = HeapPtr<Scope*>;

// An interface class to support Scope queries in the frontend without requiring
// a GC Allocated scope to necessarily exist.
//
// This abstracts Scope* (and a future ScopeCreationData type used within the
// frontend before the Scope is allocated)
class AbstractScope {
 public:
  using ScopeType = HeapPtrScope;

 private:
  ScopeType scope_ = {};

 public:
  friend class js::Scope;

  AbstractScope() {}

  explicit AbstractScope(Scope* scope) : scope_(scope) {}

  // Return true if this AbstractScope represents a Scope, either existant
  // or to be reified. This indicates that queries can be executed on this
  // scope data. Returning false is the equivalent to a nullptr, and usually
  // indicates the end of the scope chain.
  explicit operator bool() const { return maybeScope(); }

  Scope* maybeScope() const;

  // This allows us to check whether or not this abstract scope wraps
  // or otherwise would reify to a particular scope type.
  template <typename T>
  bool is() const {
    static_assert(std::is_base_of<Scope, T>::value,
                  "Trying to ask about non-Scope type");
    if (!maybeScope()) {
      return false;
    }
    return kind() == T::classScopeKind_;
  }

  ScopeKind kind() const;
  AbstractScope enclosing() const;
  bool hasEnvironment() const;
  // Valid iff is<FunctionScope>
  bool isArrow() const;
  JSFunction* canonicalFunction() const;

  bool hasOnChain(ScopeKind kind) const {
    for (AbstractScope it = *this; it; it = it.enclosing()) {
      if (it.kind() == kind) {
        return true;
      }
    }
    return false;
  }

  void trace(JSTracer* trc);
};

// Specializations of AbstractScope::is
template <>
inline bool AbstractScope::is<GlobalScope>() const {
  return maybeScope() &&
         (kind() == ScopeKind::Global || kind() == ScopeKind::NonSyntactic);
}

template <>
inline bool AbstractScope::is<EvalScope>() const {
  return maybeScope() &&
         (kind() == ScopeKind::Eval || kind() == ScopeKind::StrictEval);
}

// Iterate over abstract scopes rather than scopes.
class AbstractScopeIter {
  AbstractScope scope_;

 public:
  explicit AbstractScopeIter(const AbstractScope& f) : scope_(f) {}
  explicit operator bool() const { return !done(); }

  bool done() const { return !scope_; }

  ScopeKind kind() const {
    MOZ_ASSERT(!done());
    MOZ_ASSERT(scope_);
    return scope_.kind();
  }

  AbstractScope abstractScope() const { return scope_; }

  void operator++(int) {
    MOZ_ASSERT(!done());
    scope_ = scope_.enclosing();
  }

  // Returns whether this scope has a syntactic environment (i.e., an
  // Environment that isn't a non-syntactic With or NonSyntacticVariables)
  // on the environment chain.
  bool hasSyntacticEnvironment() const;

  void trace(JSTracer* trc) {
    if (scope_) {
      scope_.trace(trc);
    }
  };
};

}  // namespace js

#endif  // frontend_AbstractScope_h