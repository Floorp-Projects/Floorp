/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_AbstractScopePtr_h
#define frontend_AbstractScopePtr_h

#include "mozilla/Maybe.h"
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
struct MemberInitializers;
class GCMarker;

namespace frontend {
struct CompilationState;
struct CompilationGCOutput;
class ScopeStencil;
}  // namespace frontend

using ScopeIndex = frontend::TypedIndex<Scope>;
using HeapPtrScope = HeapPtr<Scope*>;

// An interface class to support Scope queries in the frontend without requiring
// a GC Allocated scope to necessarily exist.
//
// This abstracts Scope* and a ScopeStencil type used within the frontend before
// the Scope is allocated.
//
// Because a AbstractScopePtr may hold onto a Scope, it must be rooted if a GC
// may occur to ensure that the scope is traced.
class AbstractScopePtr {
 public:
  // Used to hold index and the compilationState together to avoid having a
  // potentially nullable compilationState.
  struct Deferred {
    ScopeIndex index;
    frontend::CompilationState& compilationState;
  };

  // To make writing code and managing invariants easier, we require that
  // any nullptr scopes be stored on the HeapPtrScope arm of the variant.
  using ScopeType = mozilla::Variant<HeapPtrScope, Deferred>;

 private:
  ScopeType scope_ = ScopeType(HeapPtrScope());

  Scope* scope() const { return scope_.as<HeapPtrScope>(); }

 public:
  friend class js::Scope;

  AbstractScopePtr() = default;

  explicit AbstractScopePtr(Scope* scope) : scope_(HeapPtrScope(scope)) {}

  AbstractScopePtr(frontend::CompilationState& compilationState,
                   ScopeIndex scope)
      : scope_(Deferred{scope, compilationState}) {}

  bool isNullptr() const {
    if (isScopeStencil()) {
      return false;
    }
    return scope_.as<HeapPtrScope>() == nullptr;
  }

  // Return true if this AbstractScopePtr represents a Scope, either existant
  // or to be reified. This indicates that queries can be executed on this
  // scope data. Returning false is the equivalent to a nullptr, and usually
  // indicates the end of the scope chain.
  explicit operator bool() const { return !isNullptr(); }

  bool isScopeStencil() const { return scope_.is<Deferred>(); }

  // Note: this handle is rooted in the CompilationState.
  frontend::ScopeStencil& scopeData() const;
  frontend::CompilationState& compilationState() const;

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
  // Valid iff is<FunctionScope>
  bool isArrow() const;

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

}  // namespace js

namespace JS {
template <>
struct GCPolicy<js::AbstractScopePtr::Deferred>
    : JS::IgnoreGCPolicy<js::AbstractScopePtr::Deferred> {};
}  // namespace JS

#endif  // frontend_AbstractScopePtr_h
