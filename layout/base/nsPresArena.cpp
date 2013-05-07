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
#include "nsCRT.h"
#include "nsDebug.h"
#include "nsArenaMemoryStats.h"
#include "nsPrintfCString.h"

// Size to use for PLArena block allocations.
static const size_t ARENA_PAGE_SIZE = 8192;

nsPresArena::nsPresArena()
{
  mFreeLists.Init();
  PL_INIT_ARENA_POOL(&mPool, "PresArena", ARENA_PAGE_SIZE);
}

nsPresArena::~nsPresArena()
{
#if defined(MOZ_HAVE_MEM_CHECKS)
  mFreeLists.EnumerateEntries(UnpoisonFreeList, nullptr);
#endif
  PL_FinishArenaPool(&mPool);
}

NS_HIDDEN_(void*)
nsPresArena::Allocate(uint32_t aCode, size_t aSize)
{
  NS_ABORT_IF_FALSE(aSize > 0, "PresArena cannot allocate zero bytes");

  // We only hand out aligned sizes
  aSize = PL_ARENA_ALIGN(&mPool, aSize);

  // If there is no free-list entry for this type already, we have
  // to create one now, to record its size.
  FreeList* list = mFreeLists.PutEntry(aCode);

  nsTArray<void*>::index_type len = list->mEntries.Length();
  if (list->mEntrySize == 0) {
    NS_ABORT_IF_FALSE(len == 0, "list with entries but no recorded size");
    list->mEntrySize = aSize;
  } else {
    NS_ABORT_IF_FALSE(list->mEntrySize == aSize,
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
        NS_ABORT_IF_FALSE(val == mozPoisonValue(),
                          nsPrintfCString("PresArena: poison overwritten; "
                                          "wanted %.16llx "
                                          "found %.16llx "
                                          "errors in bits %.16llx",
                                          uint64_t(mozPoisonValue()),
                                          uint64_t(val),
                                          uint64_t(mozPoisonValue() ^ val)
                                          ).get());
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
    NS_RUNTIMEABORT("out of memory");
  }
  return result;
}

NS_HIDDEN_(void)
nsPresArena::Free(uint32_t aCode, void* aPtr)
{
  // Try to recycle this entry.
  FreeList* list = mFreeLists.GetEntry(aCode);
  NS_ABORT_IF_FALSE(list, "no free list for pres arena object");
  NS_ABORT_IF_FALSE(list->mEntrySize > 0, "PresArena cannot free zero bytes");

  mozWritePoison(aPtr, list->mEntrySize);

  MOZ_MAKE_MEM_NOACCESS(aPtr, list->mEntrySize);
  list->mEntries.AppendElement(aPtr);
}

/* static */ size_t
nsPresArena::SizeOfFreeListEntryExcludingThis(
  FreeList* aEntry, nsMallocSizeOfFun aMallocSizeOf, void*)
{
  return aEntry->mEntries.SizeOfExcludingThis(aMallocSizeOf);
}

struct EnumerateData {
  nsArenaMemoryStats* stats;
  size_t total;
};

#if defined(MOZ_HAVE_MEM_CHECKS)
/* static */ PLDHashOperator
nsPresArena::UnpoisonFreeList(FreeList* aEntry, void*)
{
  nsTArray<void*>::index_type len;
  while ((len = aEntry->mEntries.Length())) {
    void* result = aEntry->mEntries.ElementAt(len - 1);
    aEntry->mEntries.RemoveElementAt(len - 1);
    MOZ_MAKE_MEM_UNDEFINED(result, aEntry->mEntrySize);
  }
  return PL_DHASH_NEXT;
}
#endif

/* static */ PLDHashOperator
nsPresArena::FreeListEnumerator(FreeList* aEntry, void* aData)
{
  EnumerateData* data = static_cast<EnumerateData*>(aData);
  // Note that we're not measuring the size of the entries on the free
  // list here.  The free list knows how many objects we've allocated
  // ever (which includes any objects that may be on the FreeList's
  // |mEntries| at this point) and we're using that to determine the
  // total size of objects allocated with a given ID.
  size_t totalSize = aEntry->mEntrySize * aEntry->mEntriesEverAllocated;
  size_t* p;

  switch (NS_PTR_TO_INT32(aEntry->mKey)) {
#define FRAME_ID(classname)                                      \
    case nsQueryFrame::classname##_id:                           \
      p = &data->stats->FRAME_ID_STAT_FIELD(classname);          \
      break;
#include "nsFrameIdList.h"
#undef FRAME_ID
  case nsLineBox_id:
    p = &data->stats->mLineBoxes;
    break;
  case nsRuleNode_id:
    p = &data->stats->mRuleNodes;
    break;
  case nsStyleContext_id:
    p = &data->stats->mStyleContexts;
    break;
  default:
    return PL_DHASH_NEXT;
  }

  *p += totalSize;
  data->total += totalSize;

  return PL_DHASH_NEXT;
}

void
nsPresArena::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
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
  mallocSize += mFreeLists.SizeOfExcludingThis(SizeOfFreeListEntryExcludingThis,
                                               aMallocSizeOf);

  EnumerateData data = { aArenaStats, 0 };
  mFreeLists.EnumerateEntries(FreeListEnumerator, &data);
  aArenaStats->mOther = mallocSize - data.total;
}
