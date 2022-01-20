/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Allocator_h
#define gc_Allocator_h

#include <stdint.h>

#include "gc/AllocKind.h"
#include "js/RootingAPI.h"

class JSFatInlineString;

namespace js {

namespace gc {
class AllocSite;
}  // namespace gc

// [SMDOC] AllowGC template parameter
//
// AllowGC is a template parameter for functions that support both with and
// without GC operation.
//
// The CanGC variant of the function can trigger a garbage collection, and
// should set a pending exception on failure.
//
// The NoGC variant of the function cannot trigger a garbage collection, and
// should not set any pending exception on failure.  This variant can be called
// in fast paths where the caller has unrooted pointers.  The failure means we
// need to perform GC to allocate an object. The caller can fall back to a slow
// path that roots pointers before calling a CanGC variant of the function,
// without having to clear a pending exception.
enum AllowGC { NoGC = 0, CanGC = 1 };

// Allocator implementation functions.
template <AllowGC allowGC = CanGC>
gc::Cell* AllocateTenuredImpl(JSContext* cx, gc::AllocKind kind, size_t size);
template <AllowGC allowGC = CanGC>
JSString* AllocateStringImpl(JSContext* cx, gc::AllocKind kind, size_t size,
                             gc::InitialHeap heap);

// Allocate a new tenured GC thing that's not nursery-allocatable.
//
// After a successful allocation the caller must fully initialize the thing
// before calling any function that can potentially trigger GC. This will ensure
// that GC tracing never sees junk values stored in the partially initialized
// thing.
template <typename T, AllowGC allowGC = CanGC>
T* Allocate(JSContext* cx) {
  static_assert(std::is_base_of_v<gc::Cell, T>);
  gc::AllocKind kind = gc::MapTypeToAllocKind<T>::kind;
  gc::Cell* cell = AllocateTenuredImpl<allowGC>(cx, kind, sizeof(T));
  return static_cast<T*>(cell);
}

// Allocate a JSObject.
//
// A longer signature that includes additional information in support of various
// optimizations. If dynamic slots are requested they will be allocated and the
// pointer stored directly in |NativeObject::slots_|.
template <AllowGC allowGC = CanGC>
JSObject* AllocateObject(JSContext* cx, gc::AllocKind kind,
                         size_t nDynamicSlots, gc::InitialHeap heap,
                         const JSClass* clasp, gc::AllocSite* site = nullptr);

// Allocate a string.
//
// Use for nursery-allocatable strings. Returns a value cast to the correct
// type.
template <typename StringT, AllowGC allowGC = CanGC>
StringT* AllocateString(JSContext* cx, gc::InitialHeap heap) {
  static_assert(std::is_base_of_v<JSString, StringT>);
  gc::AllocKind kind = gc::MapTypeToAllocKind<StringT>::kind;
  JSString* string =
      AllocateStringImpl<allowGC>(cx, kind, sizeof(StringT), heap);
  return static_cast<StringT*>(string);
}

// Allocate a BigInt.
//
// Use for nursery-allocatable BigInt.
template <AllowGC allowGC = CanGC>
JS::BigInt* AllocateBigInt(JSContext* cx, gc::InitialHeap heap);

}  // namespace js

#endif  // gc_Allocator_h
