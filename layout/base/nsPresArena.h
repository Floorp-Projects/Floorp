/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* arena allocation for the frame tree and closely-related objects */

#ifndef nsPresArena_h___
#define nsPresArena_h___

#include "mozilla/ArenaAllocator.h"
#include "mozilla/ArenaObjectID.h"
#include "mozilla/Assertions.h"
#include "mozilla/MemoryChecking.h"  // Note: Do not remove this, needed for MOZ_HAVE_MEM_CHECKS below
#include "mozilla/MemoryReporting.h"
#include <stdint.h>
#include "nscore.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsTHashtable.h"

class nsWindowSizes;

template <size_t ArenaSize>
class nsPresArena {
 public:
  nsPresArena() = default;
  ~nsPresArena();

  /**
   * Pool allocation with recycler lists indexed by frame-type ID.
   * Every aID must always be used with the same object size, aSize.
   */
  void* AllocateByFrameID(nsQueryFrame::FrameIID aID, size_t aSize) {
    return Allocate(aID, aSize);
  }
  void FreeByFrameID(nsQueryFrame::FrameIID aID, void* aPtr) {
    Free(aID, aPtr);
  }

  /**
   * Pool allocation with recycler lists indexed by object-type ID (see above).
   * Every aID must always be used with the same object size, aSize.
   */
  void* AllocateByObjectID(mozilla::ArenaObjectID aID, size_t aSize) {
    return Allocate(aID, aSize);
  }
  void FreeByObjectID(mozilla::ArenaObjectID aID, void* aPtr) {
    Free(aID, aPtr);
  }

  void* AllocateByCustomID(uint32_t aID, size_t aSize) {
    return Allocate(aID, aSize);
  }
  void FreeByCustomID(uint32_t aID, void* ptr) { Free(aID, ptr); }

  /**
   * Increment nsWindowSizes with sizes of interesting objects allocated in this
   * arena, and put the general unclassified size in `aArenaSize`.
   */
  void AddSizeOfExcludingThis(nsWindowSizes&,
                              size_t nsWindowSizes::*aArenaSize) const;

  void Check() { mPool.Check(); }

 private:
  void* Allocate(uint32_t aCode, size_t aSize);
  void Free(uint32_t aCode, void* aPtr);

  class FreeList {
   public:
    nsTArray<void*> mEntries;
    size_t mEntrySize;
    size_t mEntriesEverAllocated;

    FreeList() : mEntrySize(0), mEntriesEverAllocated(0) {}

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
      return mEntries.ShallowSizeOfExcludingThis(aMallocSizeOf);
    }
  };

  FreeList mFreeLists[mozilla::eArenaObjectID_COUNT];
  mozilla::ArenaAllocator<ArenaSize, 8> mPool;
};

#endif
