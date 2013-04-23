/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JSGC_GENERATIONAL
#ifndef gc_Nursery_inl_h__
#define gc_Nursery_inl_h__

#include "gc/Heap.h"
#include "gc/Nursery.h"

namespace js {
namespace gc {

/*
 * This structure overlays a Cell in the Nursery and re-purposes its memory
 * for managing the Nursery collection process.
 */
class RelocationOverlay
{
    friend struct MinorCollectionTracer;

    /* The low bit is set so this should never equal a normal pointer. */
    const static uintptr_t Relocated = uintptr_t(0xbad0bad1);

    /* Set to Relocated when moved. */
    uintptr_t magic_;

    /* The location |this| was moved to. */
    Cell *newLocation_;

    /* A list entry to track all relocated things. */
    RelocationOverlay *next_;

  public:
    static RelocationOverlay *fromCell(Cell *cell) {
        JS_ASSERT(!cell->isTenured());
        return reinterpret_cast<RelocationOverlay *>(cell);
    }

    bool isForwarded() const {
        return magic_ == Relocated;
    }

    Cell *forwardingAddress() const {
        JS_ASSERT(isForwarded());
        return newLocation_;
    }

    void forwardTo(Cell *cell) {
        JS_ASSERT(!isForwarded());
        magic_ = Relocated;
        newLocation_ = cell;
        next_ = NULL;
    }

    RelocationOverlay *next() const {
        return next_;
    }
};

} /* namespace gc */
} /* namespace js */

template <typename T>
JS_ALWAYS_INLINE bool
js::Nursery::getForwardedPointer(T **ref)
{
    JS_ASSERT(ref);
    JS_ASSERT(isInside(*ref));
    const gc::RelocationOverlay *overlay = reinterpret_cast<const gc::RelocationOverlay *>(*ref);
    if (!overlay->isForwarded())
        return false;
    /* This static cast from Cell* restricts T to valid (GC thing) types. */
    *ref = static_cast<T *>(overlay->forwardingAddress());
    return true;
}

#endif /* gc_Nursery_inl_h__ */
#endif /* JSGC_GENERATIONAL */
