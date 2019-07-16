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

template <size_t ArenaSize, typename ObjectId, size_t ObjectIdCount>
class nsPresArena {
 public:
  nsPresArena() = default;
  ~nsPresArena();

  /**
   * Pool allocation with recycler lists indexed by object-type ID (see above).
   * Every aID must always be used with the same object size, aSize.
   */
  void* Allocate(ObjectId aCode, size_t aSize);
  void Free(ObjectId aCode, void* aPtr);

  enum class ArenaKind { PresShell, DisplayList };
  /**
   * Increment nsWindowSizes with sizes of interesting objects allocated in this
   * arena, and put the general unclassified size in the relevant field
   * depending on the arena size.
   */
  void AddSizeOfExcludingThis(nsWindowSizes&, ArenaKind) const;

  void Check() { mPool.Check(); }

 private:
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

  FreeList mFreeLists[ObjectIdCount];
  mozilla::ArenaAllocator<ArenaSize, 8> mPool;
};

#endif
