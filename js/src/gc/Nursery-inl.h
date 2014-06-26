/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Nursery_inl_h
#define gc_Nursery_inl_h

#ifdef JSGC_GENERATIONAL

#include "gc/Nursery.h"

#include "gc/Heap.h"

namespace js {
namespace gc {

/* static */
inline RelocationOverlay *
RelocationOverlay::fromCell(Cell *cell)
{
    JS_ASSERT(!cell->isTenured());
    return reinterpret_cast<RelocationOverlay *>(cell);
}

inline bool
RelocationOverlay::isForwarded() const
{
    return magic_ == Relocated;
}

inline Cell *
RelocationOverlay::forwardingAddress() const
{
    JS_ASSERT(isForwarded());
    return newLocation_;
}

inline void
RelocationOverlay::forwardTo(Cell *cell)
{
    JS_ASSERT(!isForwarded());
    magic_ = Relocated;
    newLocation_ = cell;
    next_ = nullptr;
}

inline RelocationOverlay *
RelocationOverlay::next() const
{
    return next_;
}

} /* namespace gc */
} /* namespace js */

template <typename T>
MOZ_ALWAYS_INLINE bool
js::Nursery::getForwardedPointer(T **ref)
{
    JS_ASSERT(ref);
    JS_ASSERT(isInside((void *)*ref));
    const gc::RelocationOverlay *overlay = reinterpret_cast<const gc::RelocationOverlay *>(*ref);
    if (!overlay->isForwarded())
        return false;
    /* This static cast from Cell* restricts T to valid (GC thing) types. */
    *ref = static_cast<T *>(overlay->forwardingAddress());
    return true;
}

inline void
js::Nursery::forwardBufferPointer(JSTracer* trc, HeapSlot **pSlotElems)
{
    trc->runtime()->gc.nursery.forwardBufferPointer(pSlotElems);
}

#endif /* JSGC_GENERATIONAL */

#endif /* gc_Nursery_inl_h */
