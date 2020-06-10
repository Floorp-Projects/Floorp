/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* arena allocation for the frame tree and closely-related objects */

#include "nsPresArena.h"

#include "mozilla/Poison.h"
#include "nsDebug.h"
#include "nsDisplayList.h"
#include "nsPrintfCString.h"
#include "FrameLayerBuilder.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/ComputedStyleInlines.h"
#include "nsWindowSizes.h"

#include <inttypes.h>

using namespace mozilla;

template <size_t ArenaSize, typename ObjectId, size_t ObjectIdCount>
nsPresArena<ArenaSize, ObjectId, ObjectIdCount>::~nsPresArena() {
#if defined(MOZ_HAVE_MEM_CHECKS)
  for (FreeList* entry = mFreeLists; entry != ArrayEnd(mFreeLists); ++entry) {
    for (void* result : entry->mEntries) {
      MOZ_MAKE_MEM_UNDEFINED(result, entry->mEntrySize);
    }
    entry->mEntries.Clear();
  }
#endif
}

template <size_t ArenaSize, typename ObjectId, size_t ObjectIdCount>
void* nsPresArena<ArenaSize, ObjectId, ObjectIdCount>::Allocate(ObjectId aCode,
                                                                size_t aSize) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSize > 0, "PresArena cannot allocate zero bytes");
  MOZ_ASSERT(size_t(aCode) < ArrayLength(mFreeLists));

  // We only hand out aligned sizes
  aSize = mPool.AlignedSize(aSize);

  FreeList* list = &mFreeLists[size_t(aCode)];

  nsTArray<void*>::index_type len = list->mEntries.Length();
  if (list->mEntrySize == 0) {
    MOZ_ASSERT(len == 0, "list with entries but no recorded size");
    list->mEntrySize = aSize;
  } else {
    MOZ_ASSERT(list->mEntrySize == aSize,
               "different sizes for same object type code");
  }

  void* result;
  if (len > 0) {
    // Remove from the end of the mEntries array to avoid memmoving entries,
    // and use SetLengthAndRetainStorage to avoid a lot of malloc/free
    // from ShrinkCapacity on smaller sizes.  500 pointers means the malloc size
    // for the array is 4096 bytes or more on a 64-bit system.  The next smaller
    // size is 2048 (with jemalloc), which we consider not worth compacting.
    result = list->mEntries.ElementAt(len - 1);
    if (list->mEntries.Capacity() > 500) {
      list->mEntries.RemoveElementAt(len - 1);
    } else {
      list->mEntries.SetLengthAndRetainStorage(len - 1);
    }
#if defined(DEBUG)
    {
      MOZ_MAKE_MEM_DEFINED(result, list->mEntrySize);
      char* p = reinterpret_cast<char*>(result);
      char* limit = p + list->mEntrySize;
      for (; p < limit; p += sizeof(uintptr_t)) {
        uintptr_t val = *reinterpret_cast<uintptr_t*>(p);
        if (val != mozPoisonValue()) {
          MOZ_ReportAssertionFailure(
              nsPrintfCString("PresArena: poison overwritten; "
                              "wanted %.16" PRIx64 " "
                              "found %.16" PRIx64 " "
                              "errors in bits %.16" PRIx64 " ",
                              uint64_t(mozPoisonValue()), uint64_t(val),
                              uint64_t(mozPoisonValue() ^ val))
                  .get(),
              __FILE__, __LINE__);
          MOZ_CRASH();
        }
      }
    }
#endif
    MOZ_MAKE_MEM_UNDEFINED(result, list->mEntrySize);
    return result;
  }

  // Allocate a new chunk from the arena
  list->mEntriesEverAllocated++;
  return mPool.Allocate(aSize);
}

template <size_t ArenaSize, typename ObjectId, size_t ObjectIdCount>
void nsPresArena<ArenaSize, ObjectId, ObjectIdCount>::Free(ObjectId aCode,
                                                           void* aPtr) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(size_t(aCode) < ArrayLength(mFreeLists));

  // Try to recycle this entry.
  FreeList* list = &mFreeLists[size_t(aCode)];
  MOZ_ASSERT(list->mEntrySize > 0, "object of this type was never allocated");

  mozWritePoison(aPtr, list->mEntrySize);

  MOZ_MAKE_MEM_NOACCESS(aPtr, list->mEntrySize);
  list->mEntries.AppendElement(aPtr);
}

template <size_t ArenaSize, typename ObjectId, size_t ObjectIdCount>
void nsPresArena<ArenaSize, ObjectId, ObjectIdCount>::AddSizeOfExcludingThis(
    nsWindowSizes& aSizes, ArenaKind aKind) const {
  // We do a complicated dance here because we want to measure the
  // space taken up by the different kinds of objects in the arena,
  // but we don't have pointers to those objects.  And even if we did,
  // we wouldn't be able to use mMallocSizeOf on them, since they were
  // allocated out of malloc'd chunks of memory.  So we compute the
  // size of the arena as known by malloc and we add up the sizes of
  // all the objects that we care about.  Subtracting these two
  // quantities gives us a catch-all "other" number, which includes
  // slop in the arena itself as well as the size of objects that
  // we've not measured explicitly.

  size_t mallocSize = mPool.SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

  size_t totalSizeInFreeLists = 0;
  for (const FreeList* entry = mFreeLists; entry != ArrayEnd(mFreeLists);
       ++entry) {
    mallocSize += entry->SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

    // Note that we're not measuring the size of the entries on the free
    // list here.  The free list knows how many objects we've allocated
    // ever (which includes any objects that may be on the FreeList's
    // |mEntries| at this point) and we're using that to determine the
    // total size of objects allocated with a given ID.
    size_t totalSize = entry->mEntrySize * entry->mEntriesEverAllocated;

    if (aKind == ArenaKind::PresShell) {
      switch (entry - mFreeLists) {
#define PRES_ARENA_OBJECT(name_)                                 \
  case eArenaObjectID_##name_:                                   \
    aSizes.mArenaSizes.NS_ARENA_SIZES_FIELD(name_) += totalSize; \
    break;
#include "nsPresArenaObjectList.h"
#undef PRES_ARENA_OBJECT
        default:
          MOZ_ASSERT_UNREACHABLE("Unknown arena object type");
      }
    } else {
      MOZ_ASSERT(aKind == ArenaKind::DisplayList);
      switch (DisplayListArenaObjectId(entry - mFreeLists)) {
#define DISPLAY_LIST_ARENA_OBJECT(name_)                         \
  case DisplayListArenaObjectId::name_:                          \
    aSizes.mArenaSizes.NS_ARENA_SIZES_FIELD(name_) += totalSize; \
    break;
#include "nsDisplayListArenaTypes.h"
#undef DISPLAY_LIST_ARENA_OBJECT
        default:
          MOZ_ASSERT_UNREACHABLE("Unknown display item arena type");
      }
    }

    totalSizeInFreeLists += totalSize;
  }

  auto& field = aKind == ArenaKind::PresShell
                    ? aSizes.mLayoutPresShellSize
                    : aSizes.mLayoutRetainedDisplayListSize;

  field += mallocSize - totalSizeInFreeLists;
}

// Explicitly instantiate templates for the used nsPresArena allocator sizes.
// This is needed because nsPresArena definition is split across multiple files.
template class nsPresArena<8192, ArenaObjectID, eArenaObjectID_COUNT>;
template class nsPresArena<32768, DisplayListArenaObjectId,
                           size_t(DisplayListArenaObjectId::COUNT)>;
