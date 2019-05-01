/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Marking_inl_h
#define gc_Marking_inl_h

#include "gc/Marking.h"

#include "mozilla/Maybe.h"

#include "gc/RelocationOverlay.h"

#include "vm/BigIntType.h"
#include "vm/RegExpShared.h"

namespace js {
namespace gc {

// An abstraction to re-wrap any kind of typed pointer back to the tagged
// pointer it came from with |TaggedPtr<TargetType>::wrap(sourcePtr)|.
template <typename T>
struct TaggedPtr {};

template <>
struct TaggedPtr<JS::Value> {
  static JS::Value wrap(JSObject* obj) { return JS::ObjectOrNullValue(obj); }
  static JS::Value wrap(JSString* str) { return JS::StringValue(str); }
  static JS::Value wrap(JS::Symbol* sym) { return JS::SymbolValue(sym); }
  static JS::Value wrap(JS::BigInt* bi) { return JS::BigIntValue(bi); }
  template <typename T>
  static JS::Value wrap(T* priv) {
    static_assert(mozilla::IsBaseOf<Cell, T>::value,
                  "Type must be a GC thing derived from js::gc::Cell");
    return JS::PrivateGCThingValue(priv);
  }
};

template <>
struct TaggedPtr<jsid> {
  static jsid wrap(JSString* str) {
    return NON_INTEGER_ATOM_TO_JSID(&str->asAtom());
  }
  static jsid wrap(JS::Symbol* sym) { return SYMBOL_TO_JSID(sym); }
};

template <>
struct TaggedPtr<TaggedProto> {
  static TaggedProto wrap(JSObject* obj) { return TaggedProto(obj); }
};

template <typename T>
struct MightBeForwarded {
  static_assert(mozilla::IsBaseOf<Cell, T>::value, "T must derive from Cell");
  static_assert(!mozilla::IsSame<Cell, T>::value &&
                    !mozilla::IsSame<TenuredCell, T>::value,
                "T must not be Cell or TenuredCell");

  static const bool value = mozilla::IsBaseOf<JSObject, T>::value ||
                            mozilla::IsBaseOf<Shape, T>::value ||
                            mozilla::IsBaseOf<BaseShape, T>::value ||
                            mozilla::IsBaseOf<JSString, T>::value ||
                            mozilla::IsBaseOf<JS::BigInt, T>::value ||
                            mozilla::IsBaseOf<JSScript, T>::value ||
                            mozilla::IsBaseOf<js::LazyScript, T>::value ||
                            mozilla::IsBaseOf<js::Scope, T>::value ||
                            mozilla::IsBaseOf<js::RegExpShared, T>::value;
};

template <typename T>
inline bool IsForwarded(const T* t) {
  if (!MightBeForwarded<T>::value) {
    MOZ_ASSERT(!t->isForwarded());
    return false;
  }

  return t->isForwarded();
}

inline bool IsForwarded(const JS::Value& value) {
  auto isForwarded = [](auto t) { return IsForwarded(t); };
  return MapGCThingTyped(value, isForwarded).valueOr(false);
}

template <typename T>
inline T* Forwarded(const T* t) {
  const RelocationOverlay* overlay = RelocationOverlay::fromCell(t);
  MOZ_ASSERT(overlay->isForwarded());
  return reinterpret_cast<T*>(overlay->forwardingAddress());
}

inline Value Forwarded(const JS::Value& value) {
  auto forward = [](auto t) { return TaggedPtr<Value>::wrap(Forwarded(t)); };
  return MapGCThingTyped(value, forward).valueOr(value);
}

template <typename T>
inline T MaybeForwarded(T t) {
  if (IsForwarded(t)) {
    t = Forwarded(t);
  }
  return t;
}

inline void RelocationOverlay::forwardTo(Cell* cell) {
  MOZ_ASSERT(!isForwarded());

  // Preserve old flags because nursery may check them before checking
  // if this is a forwarded Cell.
  //
  // This is pretty terrible and we should find a better way to implement
  // Cell::getTrackKind() that doesn't rely on this behavior.
  uintptr_t gcFlags = dataWithTag_ & Cell::RESERVED_MASK;
  dataWithTag_ = uintptr_t(cell) | gcFlags | Cell::FORWARD_BIT;
}

#ifdef JSGC_HASH_TABLE_CHECKS

template <typename T>
inline bool IsGCThingValidAfterMovingGC(T* t) {
  return !IsInsideNursery(t) && !t->isForwarded();
}

template <typename T>
inline void CheckGCThingAfterMovingGC(T* t) {
  if (t) {
    MOZ_RELEASE_ASSERT(IsGCThingValidAfterMovingGC(t));
  }
}

template <typename T>
inline void CheckGCThingAfterMovingGC(const WeakHeapPtr<T*>& t) {
  CheckGCThingAfterMovingGC(t.unbarrieredGet());
}

inline void CheckValueAfterMovingGC(const JS::Value& value) {
  ApplyGCThingTyped(value, [](auto t) { CheckGCThingAfterMovingGC(t); });
}

#endif  // JSGC_HASH_TABLE_CHECKS

} /* namespace gc */
} /* namespace js */

#endif  // gc_Marking_inl_h
