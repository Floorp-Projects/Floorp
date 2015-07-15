/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* arena allocation for the frame tree and closely-related objects */

// Even on 32-bit systems, we allocate objects from the frame arena
// that require 8-byte alignment.  The cast to uintptr_t is needed
// because plarena isn't as careful about mask construction as it
// ought to be.
#define ALIGN_SHIFT 3
#define PL_ARENA_CONST_ALIGN_MASK ((uintptr_t(1) << ALIGN_SHIFT) - 1)
#include "plarena.h"
// plarena.h needs to be included first to make it use the above
// PL_ARENA_CONST_ALIGN_MASK in this file.

#include "nsPresArena.h"

#include "mozilla/Poison.h"
#include "nsDebug.h"
#include "nsArenaMemoryStats.h"
#include "nsPrintfCString.h"

#include <inttypes.h>

// Size to use for PLArena block allocations.
static const size_t ARENA_PAGE_SIZE = 8192;

nsPresArena::nsPresArena()
{
  PL_INIT_ARENA_POOL(&mPool, "PresArena", ARENA_PAGE_SIZE);
}

nsPresArena::~nsPresArena()
{
#if defined(MOZ_HAVE_MEM_CHECKS)
  for (auto iter = mFreeLists.Iter(); !iter.Done(); iter.Next()) {
    FreeList* entry = iter.Get();
    nsTArray<void*>::index_type len;
    while ((len = entry->mEntries.Length())) {
      void* result = entry->mEntries.ElementAt(len - 1);
      entry->mEntries.RemoveElementAt(len - 1);
      MOZ_MAKE_MEM_UNDEFINED(result, entry->mEntrySize);
    }
  }
#endif
  PL_FinishArenaPool(&mPool);
}

void*
nsPresArena::Allocate(uint32_t aCode, size_t aSize)
{
  MOZ_ASSERT(aSize > 0, "PresArena cannot allocate zero bytes");

  // We only hand out aligned sizes
  aSize = PL_ARENA_ALIGN(&mPool, aSize);

  // If there is no free-list entry for this type already, we have
  // to create one now, to record its size.
  FreeList* list = mFreeLists.PutEntry(aCode);

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
  PL_ARENA_ALLOCATE(result, &mPool, aSize);
  if (!result) {
    NS_ABORT_OOM(aSize);
  }
  return result;
}

void
nsPresArena::Free(uint32_t aCode, void* aPtr)
{
  // Try to recycle this entry.
  FreeList* list = mFreeLists.GetEntry(aCode);
  MOZ_ASSERT(list, "no free list for pres arena object");
  MOZ_ASSERT(list->mEntrySize > 0, "PresArena cannot free zero bytes");

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

  size_t mallocSize = PL_SizeOfArenaPoolExcludingPool(&mPool, aMallocSizeOf);
  mallocSize += mFreeLists.SizeOfExcludingThis(aMallocSizeOf);

  size_t totalSizeInFreeLists = 0;
  for (auto iter = mFreeLists.Iter(); !iter.Done(); iter.Next()) {
    FreeList* entry = iter.Get();

    // Note that we're not measuring the size of the entries on the free
    // list here.  The free list knows how many objects we've allocated
    // ever (which includes any objects that may be on the FreeList's
    // |mEntries| at this point) and we're using that to determine the
    // total size of objects allocated with a given ID.
    size_t totalSize = entry->mEntrySize * entry->mEntriesEverAllocated;
    size_t* p;

    switch (NS_PTR_TO_INT32(entry->mKey)) {
#define FRAME_ID(classname)                               \
      case nsQueryFrame::classname##_id:                  \
        p = &aArenaStats->FRAME_ID_STAT_FIELD(classname); \
        break;
#include "nsFrameIdList.h"
#undef FRAME_ID
      case nsLineBox_id:
        p = &aArenaStats->mLineBoxes;
        break;
      case nsRuleNode_id:
        p = &aArenaStats->mRuleNodes;
        break;
      case nsStyleContext_id:
        p = &aArenaStats->mStyleContexts;
        break;
#define STYLE_STRUCT(name_, checkdata_cb_)      \
        case nsStyle##name_##_id:
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
