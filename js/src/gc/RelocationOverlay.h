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
 protected:
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
    uintptr_t newLocation = dataWithTag_ & ~Cell::RESERVED_MASK;
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

// StringRelocationOverlay is created to assist with dependent string chars
// relocation due to its base string deduplication, and it stores:
// - nursery chars of a root base (root base is a base that doesn't have bases)
// or
// - nursery base of a dependent/undepended string
// StringRelocationOverlay exploits the fact that the 3rd word of JSString
// RelocationOverlay is not utilized and can be used to store extra information.
class StringRelocationOverlay : public RelocationOverlay {
  union {
    // nursery chars of a root base
    const JS::Latin1Char* nurseryCharsLatin1;
    const char16_t* nurseryCharsTwoByte;

    // The nursery base can be forwarded, which becomes a string relocation
    // overlay, or it is not yet forwarded, which is simply the base.
    JSLinearString* nurseryBaseOrRelocOverlay;
  };

 public:
  static const StringRelocationOverlay* fromCell(const Cell* cell) {
    return static_cast<const StringRelocationOverlay*>(cell);
  }

  static StringRelocationOverlay* fromCell(Cell* cell) {
    return static_cast<StringRelocationOverlay*>(cell);
  }

  StringRelocationOverlay*& nextRef() {
    MOZ_ASSERT(isForwarded());
    return (StringRelocationOverlay*&)next_;
  }

  StringRelocationOverlay* next() const {
    MOZ_ASSERT(isForwarded());
    return (StringRelocationOverlay*)next_;
  }

  // For the forwarded and potentially deduplicated string,
  // save its nursery chars if it is the root base of a dependent string,
  // or save its nursery base if it is a dependent/undepended string.
  // The saved nursery base helps to preserve the nursery base chain for
  // a dependent string, so that its nursery root base can be recovered.
  // And the root base nursery chars is required when calculating
  // the base offset for the dependent string, so that its chars
  // can get relocated: new root base chars + base offset.
  void saveCharsOrBase(JSString* src) {
    JS::AutoCheckCannotGC nogc;
    if (src->hasBase()) {
      nurseryBaseOrRelocOverlay = src->nurseryBaseOrRelocOverlay();
    } else if (src->canBeRootBase()) {
      if (src->hasTwoByteChars()) {
        nurseryCharsTwoByte = src->asLinear().twoByteChars(nogc);
      } else {
        nurseryCharsLatin1 = src->asLinear().latin1Chars(nogc);
      }
    }
  }

  template <typename CharT>
  MOZ_ALWAYS_INLINE const CharT* savedNurseryChars() const;

  const MOZ_ALWAYS_INLINE JS::Latin1Char* savedNurseryCharsLatin1() const {
    return nurseryCharsLatin1;
  }

  const MOZ_ALWAYS_INLINE char16_t* savedNurseryCharsTwoByte() const {
    return nurseryCharsTwoByte;
  }

  JSLinearString* savedNurseryBaseOrRelocOverlay() const {
    return nurseryBaseOrRelocOverlay;
  }
};

template <>
MOZ_ALWAYS_INLINE const JS::Latin1Char*
StringRelocationOverlay::savedNurseryChars() const {
  return savedNurseryCharsLatin1();
}

template <>
MOZ_ALWAYS_INLINE const char16_t* StringRelocationOverlay::savedNurseryChars()
    const {
  return savedNurseryCharsTwoByte();
}

}  // namespace gc
}  // namespace js

#endif /* gc_RelocationOverlay_h */
