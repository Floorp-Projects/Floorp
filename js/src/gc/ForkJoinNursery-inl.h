/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_ForkJoinNursery_inl_h
#define gc_ForkJoinNursery_inl_h

#ifdef JSGC_FJGENERATIONAL

#include "gc/ForkJoinNursery.h"

namespace js {
namespace gc {

// For the following two predicates we can't check the attributes on
// the chunk trailer because it's not known whether addr points into a
// chunk.
//
// A couple of optimizations are possible if performance is an issue:
//
//  - The loop can be unrolled, and we can arrange for all array entries
//    to be valid for this purpose so that the bound is constant.
//  - The per-chunk test can be reduced to testing whether the high bits
//    of the object pointer and the high bits of the chunk pointer are
//    the same (and the latter value is essentially space[i]).
//    Note, experiments with that do not show an improvement yet.
//  - Taken together, those optimizations yield code that is one LOAD,
//    one XOR, and one AND for each chunk, with the result being true
//    iff the resulting value is zero.
//  - We can have multiple versions of the predicates, and those that
//    take known-good GCThing types can go directly to the attributes;
//    it may be possible to ensure that more calls use GCThing types.
//    Note, this requires the worker ID to be part of the chunk
//    attribute bit vector.
//
// Performance may not be an issue as there may be few survivors of a
// collection in the ForkJoinNursery and few objects will be tested.
// If so then the bulk of the calls may come from the code that scans
// the roots.  Behavior will be workload-dependent however.

MOZ_ALWAYS_INLINE bool
ForkJoinNursery::isInsideNewspace(const void *addr)
{
    uintptr_t p = reinterpret_cast<uintptr_t>(addr);
    for (unsigned i = 0 ; i <= currentChunk_ ; i++) {
        if (p >= newspace[i]->start() && p < newspace[i]->end())
            return true;
    }
    return false;
}

MOZ_ALWAYS_INLINE bool
ForkJoinNursery::isInsideFromspace(const void *addr)
{
    uintptr_t p = reinterpret_cast<uintptr_t>(addr);
    for (unsigned i = 0 ; i < numFromspaceChunks_ ; i++) {
        if (p >= fromspace[i]->start() && p < fromspace[i]->end())
            return true;
    }
    return false;
}

MOZ_ALWAYS_INLINE bool
ForkJoinNursery::isForwarded(Cell *cell)
{
    JS_ASSERT(isInsideFromspace(cell));
    const RelocationOverlay *overlay = RelocationOverlay::fromCell(cell);
    return overlay->isForwarded();
}

template <typename T>
MOZ_ALWAYS_INLINE bool
ForkJoinNursery::getForwardedPointer(T **ref)
{
    JS_ASSERT(ref);
    JS_ASSERT(isInsideFromspace(*ref));
    const RelocationOverlay *overlay = RelocationOverlay::fromCell(*ref);
    if (!overlay->isForwarded())
        return false;
    // This static_cast from Cell* restricts T to valid (GC thing) types.
    *ref = static_cast<T *>(overlay->forwardingAddress());
    return true;
}

} // namespace gc
} // namespace js

#endif // JSGC_FJGENERATIONAL

#endif // gc_ForkJoinNursery_inl_h
