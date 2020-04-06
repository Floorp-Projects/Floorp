/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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

#include "gc/Cell.h"

namespace js {
namespace gc {

/*
 * This structure overlays a Cell that has been moved and provides a way to find
 * its new location. It's used during generational and compacting GC.
 */
class RelocationOverlay : public Cell {
  // First word of a Cell has additional requirements from GC. The GC flags
  // determine if a Cell is a normal entry or is a RelocationOverlay.
  //                3         0
  //  -------------------------
  //  | NewLocation | GCFlags |
  //  -------------------------
  uintptr_t dataWithTag_;

  /* A list entry to track all relocated things. */
  RelocationOverlay* next_;

 public:
  static const RelocationOverlay* fromCell(const Cell* cell) {
    return static_cast<const RelocationOverlay*>(cell);
  }

  static RelocationOverlay* fromCell(Cell* cell) {
    return static_cast<RelocationOverlay*>(cell);
  }

  Cell* forwardingAddress() const {
    MOZ_ASSERT(isForwarded());
    uintptr_t newLocation = dataWithTag_ & ~CellHeader::RESERVED_MASK;
    return reinterpret_cast<Cell*>(newLocation);
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
};

}  // namespace gc
}  // namespace js

#endif /* gc_RelocationOverlay_h */
