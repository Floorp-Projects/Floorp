/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * GC-internal definition of relocation overlay used while moving cells.
 */

#ifndef gc_RelocationOverlay_h
#define gc_RelocationOverlay_h

#include "mozilla/Assertions.h"

#include <stdint.h>

namespace js {
namespace gc {

struct Cell;

/*
 * This structure overlays a Cell that has been moved and provides a way to find
 * its new location. It's used during generational and compacting GC.
 */
class RelocationOverlay
{
    /* The low bit is set so this should never equal a normal pointer. */
    static const uintptr_t Relocated = uintptr_t(0xbad0bad1);

    /* Set to Relocated when moved. */
    uintptr_t magic_;

    /* The location |this| was moved to. */
    Cell* newLocation_;

    /* A list entry to track all relocated things. */
    RelocationOverlay* next_;

  public:
    static RelocationOverlay* fromCell(Cell* cell) {
        return reinterpret_cast<RelocationOverlay*>(cell);
    }

    bool isForwarded() const {
        return magic_ == Relocated;
    }

    Cell* forwardingAddress() const {
        MOZ_ASSERT(isForwarded());
        return newLocation_;
    }

    void forwardTo(Cell* cell);

    RelocationOverlay*& nextRef() {
        MOZ_ASSERT(isForwarded());
        return next_;
    }

    RelocationOverlay* next() const {
        MOZ_ASSERT(isForwarded());
        return next_;
    }

    static bool isCellForwarded(Cell* cell) {
        return fromCell(cell)->isForwarded();
    }
};

} // namespace gc
} // namespace js

#endif /* gc_RelocationOverlay_h */
