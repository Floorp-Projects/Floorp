/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Marking_inl_h
#define gc_Marking_inl_h

#include "gc/Marking.h"

#include "gc/RelocationOverlay.h"

#ifdef ENABLE_BIGINT
#include "vm/BigIntType.h"
#endif

namespace js {
namespace gc {

template <typename T>
struct MightBeForwarded
{
    static_assert(mozilla::IsBaseOf<Cell, T>::value,
                  "T must derive from Cell");
    static_assert(!mozilla::IsSame<Cell, T>::value && !mozilla::IsSame<TenuredCell, T>::value,
                  "T must not be Cell or TenuredCell");

    static const bool value = mozilla::IsBaseOf<JSObject, T>::value ||
                              mozilla::IsBaseOf<Shape, T>::value ||
                              mozilla::IsBaseOf<BaseShape, T>::value ||
                              mozilla::IsBaseOf<JSString, T>::value ||
                              mozilla::IsBaseOf<JSScript, T>::value ||
                              mozilla::IsBaseOf<js::LazyScript, T>::value ||
                              mozilla::IsBaseOf<js::Scope, T>::value ||
                              mozilla::IsBaseOf<js::RegExpShared, T>::value;
};

template <typename T>
inline bool
IsForwarded(const T* t)
{
    if (!MightBeForwarded<T>::value) {
        MOZ_ASSERT(!t->isForwarded());
        return false;
    }

    return t->isForwarded();
}

struct IsForwardedFunctor : public BoolDefaultAdaptor<Value, false> {
    template <typename T> bool operator()(const T* t) { return IsForwarded(t); }
};

inline bool
IsForwarded(const JS::Value& value)
{
    return DispatchTyped(IsForwardedFunctor(), value);
}

template <typename T>
inline T*
Forwarded(const T* t)
{
    const RelocationOverlay* overlay = RelocationOverlay::fromCell(t);
    MOZ_ASSERT(overlay->isForwarded());
    return reinterpret_cast<T*>(overlay->forwardingAddress());
}

struct ForwardedFunctor : public IdentityDefaultAdaptor<Value> {
    template <typename T> inline Value operator()(const T* t) {
        return js::gc::RewrapTaggedPointer<Value, T>::wrap(Forwarded(t));
    }
};

inline Value
Forwarded(const JS::Value& value)
{
    return DispatchTyped(ForwardedFunctor(), value);
}

template <typename T>
inline T
MaybeForwarded(T t)
{
    if (IsForwarded(t)) {
        t = Forwarded(t);
    }
    return t;
}

inline void
RelocationOverlay::forwardTo(Cell* cell)
{
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
inline bool
IsGCThingValidAfterMovingGC(T* t)
{
    return !IsInsideNursery(t) && !t->isForwarded();
}

template <typename T>
inline void
CheckGCThingAfterMovingGC(T* t)
{
    if (t) {
        MOZ_RELEASE_ASSERT(IsGCThingValidAfterMovingGC(t));
    }
}

template <typename T>
inline void
CheckGCThingAfterMovingGC(const ReadBarriered<T*>& t)
{
    CheckGCThingAfterMovingGC(t.unbarrieredGet());
}

struct CheckValueAfterMovingGCFunctor : public VoidDefaultAdaptor<Value> {
    template <typename T> void operator()(T* t) { CheckGCThingAfterMovingGC(t); }
};

inline void
CheckValueAfterMovingGC(const JS::Value& value)
{
    DispatchTyped(CheckValueAfterMovingGCFunctor(), value);
}

#endif // JSGC_HASH_TABLE_CHECKS

} /* namespace gc */
} /* namespace js */

#endif // gc_Marking_inl_h
