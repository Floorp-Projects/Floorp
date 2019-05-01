/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS Garbage Collector. */

#ifndef gc_Policy_h
#define gc_Policy_h

#include "mozilla/TypeTraits.h"
#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "js/GCPolicyAPI.h"

namespace js {

// Define the GCPolicy for all internal pointers.
template <typename T>
struct InternalGCPointerPolicy : public JS::GCPointerPolicy<T> {
  using Type = typename mozilla::RemovePointer<T>::Type;

#define IS_BASE_OF_OR(_1, BaseType, _2, _3) \
  mozilla::IsBaseOf<BaseType, Type>::value ||
  static_assert(
      JS_FOR_EACH_TRACEKIND(IS_BASE_OF_OR) false,
      "InternalGCPointerPolicy must only be used for GC thing pointers");
#undef IS_BASE_OF_OR

  static void preBarrier(T v) {
    if (v) {
      Type::writeBarrierPre(v);
    }
  }
  static void postBarrier(T* vp, T prev, T next) {
    if (*vp) {
      Type::writeBarrierPost(vp, prev, next);
    }
  }
  static void readBarrier(T v) {
    if (v) {
      Type::readBarrier(v);
    }
  }
  static void trace(JSTracer* trc, T* vp, const char* name) {
    if (*vp) {
      TraceManuallyBarrieredEdge(trc, vp, name);
    }
  }
};

}  // namespace js

namespace JS {

// Internally, all pointer types are treated as pointers to GC things by
// default.
template <typename T>
struct GCPolicy<T*> : public js::InternalGCPointerPolicy<T*> {};
template <typename T>
struct GCPolicy<T* const> : public js::InternalGCPointerPolicy<T* const> {};

template <typename T>
struct GCPolicy<js::HeapPtr<T>> {
  static void trace(JSTracer* trc, js::HeapPtr<T>* thingp, const char* name) {
    js::TraceNullableEdge(trc, thingp, name);
  }
  static bool needsSweep(js::HeapPtr<T>* thingp) {
    return js::gc::IsAboutToBeFinalized(thingp);
  }
};

template <typename T>
struct GCPolicy<js::WeakHeapPtr<T>> {
  static void trace(JSTracer* trc, js::WeakHeapPtr<T>* thingp,
                    const char* name) {
    js::TraceEdge(trc, thingp, name);
  }
  static bool needsSweep(js::WeakHeapPtr<T>* thingp) {
    return js::gc::IsAboutToBeFinalized(thingp);
  }
};

}  // namespace JS

#endif  // gc_Policy_h
