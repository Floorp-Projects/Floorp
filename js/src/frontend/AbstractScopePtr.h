/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_AbstractScopePtr_h
#define frontend_AbstractScopePtr_h

#include "mozilla/Variant.h"

#include "frontend/TypedIndex.h"
#include "gc/Barrier.h"
#include "gc/Rooting.h"
#include "gc/Tracer.h"
#include "vm/Scope.h"
#include "vm/ScopeKind.h"  // For ScopeKind

namespace js {
class Scope;
class GlobalScope;
class EvalScope;
struct FieldInitializers;
class GCMarker;

namespace frontend {
struct CompilationInfo;
class FunctionBox;
class ScopeCreationData;
}  // namespace frontend

using ScopeIndex = frontend::TypedIndex<Scope>;
using HeapPtrScope = HeapPtr<Scope*>;

// An interface class to support Scope queries in the frontend without requiring
// a GC Allocated scope to necessarily exist.
//
// This abstracts Scope* (and a future ScopeCreationData type used within the
// frontend before the Scope is allocated)
//
// Because a AbstractScopePtr may hold onto a Scope, it must be rooted if a GC
// may occur to ensure that the scope is traced.
class AbstractScopePtr {
 public:
  // Used to hold index and the compilationInfo together to avoid having a
  // potentially nullable compilationInfo.
  struct Deferred {
    ScopeIndex index;
    frontend::CompilationInfo& compilationInfo;
  };

  // To make writing code and managing invariants easier, we require that
  // any nullptr scopes be stored on the HeapPtrScope arm of the variant.
  using ScopeType = mozilla::Variant<HeapPtrScope, Deferred>;

 private:
  ScopeType scope_ = ScopeType(HeapPtrScope());

  // Extract the Scope* represented by this; may be nullptr, and will
  // forward through to the ScopeCreationData if it has a Scope*
  //
  // Should only be used after getOrCreate() has been used to reify this into a
  // Scope.
  Scope* getExistingScope() const;

 public:
  friend class js::Scope;
  friend class js::frontend::FunctionBox;

  AbstractScopePtr() = default;

  explicit AbstractScopePtr(Scope* scope) : scope_(HeapPtrScope(scope)) {}

  AbstractScopePtr(frontend::CompilationInfo& compilationInfo, ScopeIndex scope)
      : scope_(Deferred{scope, compilationInfo}) {}

  bool isNullptr() const {
    if (isScopeCreationData()) {
      return false;
    }
    return scope_.as<HeapPtrScope>() == nullptr;
  }

  // Return true if this AbstractScopePtr represents a Scope, either existant
  // or to be reified. This indicates that queries can be executed on this
  // scope data. Returning false is the equivalent to a nullptr, and usually
  // indicates the end of the scope chain.
  explicit operator bool() const { return !isNullptr(); }

  bool isScopeCreationData() const { return scope_.is<Deferred>(); }

  // Note: this handle is rooted in the CompilationInfo.
  MutableHandle<frontend::ScopeCreationData> scopeCreationData() const;

  Scope* scope() const { return scope_.as<HeapPtrScope>(); }

  // Get a Scope*, creating it from a ScopeCreationData if required.
  // Used to allow us to ensure that Scopes are always allocated with
  // real GC allocated Enclosing scopes.
  bool getOrCreateScope(JSContext* cx, MutableHandleScope scope);

  // This allows us to check whether or not this provider wraps
  // or otherwise would reify to a particular scope type.
  template <typename T>
  bool is() const {
    static_assert(std::is_base_of_v<Scope, T>,
                  "Trying to ask about non-Scope type");
    if (isNullptr()) {
      return false;
    }
    return kind() == T::classScopeKind_;
  }

  ScopeKind kind() const;
  AbstractScopePtr enclosing() const;
  bool hasEnvironment() const;
  uint32_t nextFrameSlot() const;
  // Valid iff is<FunctionScope>
  bool isArrow() const;
  bool isClassConstructor() const;
  const FieldInitializers& fieldInitializers() const;

  bool hasOnChain(ScopeKind kind) const {
    for (AbstractScopePtr it = *this; it; it = it.enclosing()) {
      if (it.kind() == kind) {
        return true;
      }
    }
    return false;
  }

  void trace(JSTracer* trc);
};

// Specializations of AbstractScopePtr::is
template <>
inline bool AbstractScopePtr::is<GlobalScope>() const {
  return !isNullptr() &&
         (kind() == ScopeKind::Global || kind() == ScopeKind::NonSyntactic);
}

template <>
inline bool AbstractScopePtr::is<EvalScope>() const {
  return !isNullptr() &&
         (kind() == ScopeKind::Eval || kind() == ScopeKind::StrictEval);
}

// Iterate over abstract scopes rather than scopes.
class AbstractScopePtrIter {
  AbstractScopePtr scope_;

 public:
  explicit AbstractScopePtrIter(const AbstractScopePtr& f) : scope_(f) {}
  explicit operator bool() const { return !done(); }

  bool done() const { return !scope_; }

  ScopeKind kind() const {
    MOZ_ASSERT(!done());
    MOZ_ASSERT(scope_);
    return scope_.kind();
  }

  AbstractScopePtr abstractScopePtr() const { return scope_; }

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

namespace JS {
template <>
struct GCPolicy<js::AbstractScopePtr::Deferred>
    : JS::IgnoreGCPolicy<js::AbstractScopePtr::Deferred> {};
}  // namespace JS

#endif  // frontend_AbstractScopePtr_h
