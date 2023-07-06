/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Allocator_h
#define gc_Allocator_h

#include "mozilla/OperatorNewExtensions.h"

#include <stdint.h>

#include "gc/AllocKind.h"
#include "gc/Cell.h"
#include "gc/GCEnum.h"
#include "gc/Zone.h"
#include "js/Class.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace js {

namespace gc {

class AllocSite;
struct Cell;
class TenuredCell;

// Allocator implementation functions. SpiderMonkey code outside this file
// should use:
//
//     cx->newCell<T>(...)
//
// or optionally:
//
//     cx->newCell<T, AllowGC::NoGC>(...)
//
// `friend` js::gc::CellAllocator in a subtype T of Cell in order to allow it to
// be allocated with cx->newCell<T>(...). The friend declaration will allow
// calling T's constructor.
//
// The parameters will be passed to a type-specific function or constructor. For
// nursery-allocatable types, see e.g. the NewString, NewObject, and NewBigInt
// methods. For all other types, the parameters will be forwarded to the
// constructor.
class CellAllocator {
 public:
  template <typename T, js::AllowGC allowGC = CanGC, typename... Args>
  static T* NewCell(JS::RootingContext* rcx, Args&&... args);

 private:
  template <AllowGC allowGC>
  static void* RetryNurseryAlloc(JS::RootingContext* rcx,
                                 JS::TraceKind traceKind, AllocKind allocKind,
                                 size_t thingSize, AllocSite* site);
  template <AllowGC allowGC>
  static void* TryNewTenuredCell(JS::RootingContext* rcx, AllocKind kind,
                                 size_t thingSize);

#if defined(DEBUG) || defined(JS_GC_ZEAL) || defined(JS_OOM_BREAKPOINT)
  template <AllowGC allowGC>
  static bool PreAllocChecks(JS::RootingContext* rcx, AllocKind kind);
#else
  template <AllowGC allowGC>
  static bool PreAllocChecks(JS::RootingContext* rcx, AllocKind kind) {
    return true;
  }
#endif

#ifdef DEBUG
  static void CheckIncrementalZoneState(JSContext* cx, void* ptr);
#endif

  static MOZ_ALWAYS_INLINE gc::Heap CheckedHeap(gc::Heap heap) {
    if (heap > Heap::Tenured) {
      // This helps the compiler to see that nursery allocation is never
      // possible if Heap::Tenured is specified.
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Bad gc::Heap value");
    }
    return heap;
  }

  // Allocate a cell in the nursery, unless |heap| is Heap::Tenured or nursery
  // allocation is disabled for |traceKind| in the current zone.
  template <JS::TraceKind traceKind, AllowGC allowGC = CanGC>
  static void* AllocNurseryOrTenuredCell(JS::RootingContext* rcx,
                                         gc::AllocKind allocKind,
                                         size_t thingSize, gc::Heap heap,
                                         AllocSite* site) {
    MOZ_ASSERT(IsNurseryAllocable(allocKind));
    MOZ_ASSERT(MapAllocToTraceKind(allocKind) == traceKind);
    MOZ_ASSERT(thingSize == Arena::thingSize(allocKind));
    MOZ_ASSERT_IF(site && site->initialHeap() == Heap::Tenured,
                  heap == Heap::Tenured);

    if (!PreAllocChecks<allowGC>(rcx, allocKind)) {
      return nullptr;
    }

    JS::Zone* zone = rcx->zoneUnchecked();
    gc::Heap minHeapToTenure = CheckedHeap(zone->minHeapToTenure(traceKind));
    if (CheckedHeap(heap) < minHeapToTenure) {
      if (!site) {
        site = zone->unknownAllocSite(traceKind);
      }

      void* ptr = rcx->nursery().tryAllocateCell(site, thingSize, traceKind);
      if (MOZ_LIKELY(ptr)) {
        return ptr;
      }

      return RetryNurseryAlloc<allowGC>(rcx, traceKind, allocKind, thingSize,
                                        site);
    }

    return TryNewTenuredCell<allowGC>(rcx, allocKind, thingSize);
  }

  // Allocate a cell in the tenured heap.
  template <AllowGC allowGC = CanGC>
  static void* AllocTenuredCell(JS::RootingContext* rcx, gc::AllocKind kind,
                                size_t size);

  // Allocate a string. Use cx->newCell<T>([heap]).
  //
  // Use for nursery-allocatable strings. Returns a value cast to the correct
  // type. Non-nursery-allocatable strings will go through the fallback
  // tenured-only allocation path.
  template <typename T, AllowGC allowGC = CanGC, typename... Args>
  static T* NewString(JS::RootingContext* rcx, gc::Heap heap, Args&&... args) {
    static_assert(std::is_base_of_v<JSString, T>);
    gc::AllocKind kind = gc::MapTypeToAllocKind<T>::kind;
    void* ptr = AllocNurseryOrTenuredCell<JS::TraceKind::String, allowGC>(
        rcx, kind, sizeof(T), heap, nullptr);
    if (MOZ_UNLIKELY(!ptr)) {
      return nullptr;
    }
    return new (mozilla::KnownNotNull, ptr) T(std::forward<Args>(args)...);
  }

  template <typename T, AllowGC allowGC /* = CanGC */>
  static T* NewBigInt(JS::RootingContext* rcx, Heap heap) {
    void* ptr = AllocNurseryOrTenuredCell<JS::TraceKind::BigInt, allowGC>(
        rcx, gc::AllocKind::BIGINT, sizeof(T), heap, nullptr);
    if (MOZ_UNLIKELY(!ptr)) {
      return nullptr;
    }
    return new (mozilla::KnownNotNull, ptr) T();
  }

  template <typename T, AllowGC allowGC = CanGC>
  static T* NewObject(JS::RootingContext* rcx, gc::AllocKind kind,
                      gc::Heap heap, const JSClass* clasp,
                      gc::AllocSite* site = nullptr) {
    MOZ_ASSERT(IsObjectAllocKind(kind));
    MOZ_ASSERT_IF(heap != gc::Heap::Tenured && clasp->hasFinalize() &&
                      !clasp->isProxyObject(),
                  CanNurseryAllocateFinalizedClass(clasp));
    size_t thingSize = JSObject::thingSize(kind);
    void* cell = AllocNurseryOrTenuredCell<JS::TraceKind::Object, allowGC>(
        rcx, kind, thingSize, heap, site);
    if (MOZ_UNLIKELY(!cell)) {
      return nullptr;
    }
    return new (mozilla::KnownNotNull, cell) T();
  }

  // Allocate all other kinds of GC thing.
  template <typename T, AllowGC allowGC = CanGC, typename... Args>
  static T* NewTenuredCell(JS::RootingContext* rcx, Args&&... args) {
    gc::AllocKind kind = gc::MapTypeToAllocKind<T>::kind;
    void* cell = AllocTenuredCell<allowGC>(rcx, kind, sizeof(T));
    if (MOZ_UNLIKELY(!cell)) {
      return nullptr;
    }
    return new (mozilla::KnownNotNull, cell) T(std::forward<Args>(args)...);
  }
};

}  // namespace gc

// This is the entry point for all allocation, though callers should still not
// use this directly. Use cx->newCell<T>(...) instead.
//
// After a successful allocation the caller must fully initialize the thing
// before calling any function that can potentially trigger GC. This will
// ensure that GC tracing never sees junk values stored in the partially
// initialized thing.
template <typename T, AllowGC allowGC, typename... Args>
T* gc::CellAllocator::NewCell(JS::RootingContext* rcx, Args&&... args) {
  static_assert(std::is_base_of_v<gc::Cell, T>);

  // Objects. See the valid parameter list in NewObject, above.
  if constexpr (std::is_base_of_v<JSObject, T>) {
    return NewObject<T, allowGC>(rcx, std::forward<Args>(args)...);
  }

  // BigInt
  else if constexpr (std::is_base_of_v<JS::BigInt, T>) {
    return NewBigInt<T, allowGC>(rcx, std::forward<Args>(args)...);
  }

  // "Normal" strings (all of which can be nursery allocated). Atoms and
  // external strings will fall through to the generic code below. All other
  // strings go through NewString, which will forward the arguments to the
  // appropriate string class's constructor.
  else if constexpr (std::is_base_of_v<JSString, T> &&
                     !std::is_base_of_v<JSAtom, T> &&
                     !std::is_base_of_v<JSExternalString, T>) {
    return NewString<T, allowGC>(rcx, std::forward<Args>(args)...);
  }

  else {
    // Allocate a new tenured GC thing that's not nursery-allocatable. Use
    // cx->newCell<T>(...), where the parameters are forwarded to the type's
    // constructor.
    return NewTenuredCell<T, allowGC>(rcx, std::forward<Args>(args)...);
  }
}

}  // namespace js

#endif  // gc_Allocator_h
