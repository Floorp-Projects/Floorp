/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* arena allocation for the frame tree and closely-related objects */

#include "nsPresArena.h"

#include "mozilla/Poison.h"
#include "nsDebug.h"
#include "nsArenaMemoryStats.h"
#include "nsPrintfCString.h"
#include "nsStyleContext.h"
#include "FrameLayerBuilder.h"
#include "mozilla/ArrayUtils.h"

#include <inttypes.h>

using namespace mozilla;

nsPresArena::nsPresArena()
{
}

nsPresArena::~nsPresArena()
{
  ClearArenaRefPtrs();

#if defined(MOZ_HAVE_MEM_CHECKS)
  for (FreeList* entry = mFreeLists; entry != ArrayEnd(mFreeLists); ++entry) {
    nsTArray<void*>::index_type len;
    while ((len = entry->mEntries.Length())) {
      void* result = entry->mEntries.ElementAt(len - 1);
      entry->mEntries.RemoveElementAt(len - 1);
      MOZ_MAKE_MEM_UNDEFINED(result, entry->mEntrySize);
    }
  }
#endif
}

/* inline */ void
nsPresArena::ClearArenaRefPtrWithoutDeregistering(void* aPtr,
                                                  ArenaObjectID aObjectID)
{
  switch (aObjectID) {
#define PRES_ARENA_OBJECT_WITH_ARENAREFPTR_SUPPORT(name_)                     \
    case eArenaObjectID_##name_:                                              \
      static_cast<ArenaRefPtr<name_>*>(aPtr)->ClearWithoutDeregistering();    \
      return;
#include "nsPresArenaObjectList.h"
#undef PRES_ARENA_OBJECT_WITH_ARENAREFPTR_SUPPORT
    default:
      break;
  }
  switch (aObjectID) {
#define PRES_ARENA_OBJECT_WITHOUT_ARENAREFPTR_SUPPORT(name_)                  \
    case eArenaObjectID_##name_:                                              \
      MOZ_ASSERT(false, #name_ " must be declared in nsPresArenaObjectList.h "\
                        "with PRES_ARENA_OBJECT_SUPPORTS_ARENAREFPTR");       \
      break;
#include "nsPresArenaObjectList.h"
#undef PRES_ARENA_OBJECT_WITHOUT_ARENAREFPTR_SUPPORT
    default:
      MOZ_ASSERT(false, "unexpected ArenaObjectID value");
      break;
  }
}

void
nsPresArena::ClearArenaRefPtrs()
{
  for (auto iter = mArenaRefPtrs.Iter(); !iter.Done(); iter.Next()) {
    void* ptr = iter.Key();
    ArenaObjectID id = iter.UserData();
    ClearArenaRefPtrWithoutDeregistering(ptr, id);
  }
  mArenaRefPtrs.Clear();
}

void
nsPresArena::ClearArenaRefPtrs(ArenaObjectID aObjectID)
{
  for (auto iter = mArenaRefPtrs.Iter(); !iter.Done(); iter.Next()) {
    void* ptr = iter.Key();
    ArenaObjectID id = iter.UserData();
    if (id == aObjectID) {
      ClearArenaRefPtrWithoutDeregistering(ptr, id);
      iter.Remove();
    }
  }
}

void*
nsPresArena::Allocate(uint32_t aCode, size_t aSize)
{
  MOZ_ASSERT(aSize > 0, "PresArena cannot allocate zero bytes");
  MOZ_ASSERT(aCode < ArrayLength(mFreeLists));

  // We only hand out aligned sizes
  aSize = mPool.AlignedSize(aSize);

  FreeList* list = &mFreeLists[aCode];

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
    // LIFO behavior for best cache utilization
    result = list->mEntries.ElementAt(len - 1);
    list->mEntries.RemoveElementAt(len - 1);
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
                            uint64_t(mozPoisonValue()),
                            uint64_t(val),
                            uint64_t(mozPoisonValue() ^ val)).get(),
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

void
nsPresArena::Free(uint32_t aCode, void* aPtr)
{
  MOZ_ASSERT(aCode < ArrayLength(mFreeLists));

  // Try to recycle this entry.
  FreeList* list = &mFreeLists[aCode];
  MOZ_ASSERT(list->mEntrySize > 0, "object of this type was never allocated");

  mozWritePoison(aPtr, list->mEntrySize);

  MOZ_MAKE_MEM_NOACCESS(aPtr, list->mEntrySize);
  list->mEntries.AppendElement(aPtr);
}

void
nsPresArena::AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                    nsArenaMemoryStats* aArenaStats)
{
  // We do a complicated dance here because we want to measure the
  // space taken up by the different kinds of objects in the arena,
  // but we don't have pointers to those objects.  And even if we did,
  // we wouldn't be able to use aMallocSizeOf on them, since they were
  // allocated out of malloc'd chunks of memory.  So we compute the
  // size of the arena as known by malloc and we add up the sizes of
  // all the objects that we care about.  Subtracting these two
  // quantities gives us a catch-all "other" number, which includes
  // slop in the arena itself as well as the size of objects that
  // we've not measured explicitly.

  size_t mallocSize = mPool.SizeOfExcludingThis(aMallocSizeOf);

  size_t totalSizeInFreeLists = 0;
  for (FreeList* entry = mFreeLists; entry != ArrayEnd(mFreeLists); ++entry) {
    mallocSize += entry->SizeOfExcludingThis(aMallocSizeOf);

    // Note that we're not measuring the size of the entries on the free
    // list here.  The free list knows how many objects we've allocated
    // ever (which includes any objects that may be on the FreeList's
    // |mEntries| at this point) and we're using that to determine the
    // total size of objects allocated with a given ID.
    size_t totalSize = entry->mEntrySize * entry->mEntriesEverAllocated;
    size_t* p;

    switch (entry - mFreeLists) {
#define FRAME_ID(classname, ...)                          \
      case nsQueryFrame::classname##_id:                  \
        p = &aArenaStats->FRAME_ID_STAT_FIELD(classname); \
        break;
#define ABSTRACT_FRAME_ID(...)
#include "nsFrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID
      case eArenaObjectID_nsLineBox:
        p = &aArenaStats->mLineBoxes;
        break;
      case eArenaObjectID_nsRuleNode:
        p = &aArenaStats->mRuleNodes;
        break;
      case eArenaObjectID_nsStyleContext:
        p = &aArenaStats->mStyleContexts;
        break;
#define STYLE_STRUCT(name_, checkdata_cb_)      \
        case eArenaObjectID_nsStyle##name_:
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
        p = &aArenaStats->mStyleStructs;
        break;
      default:
        continue;
    }

    *p += totalSize;
    totalSizeInFreeLists += totalSize;
  }

  aArenaStats->mOther += mallocSize - totalSizeInFreeLists;
}
