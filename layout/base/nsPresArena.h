/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* arena allocation for the frame tree and closely-related objects */

#ifndef nsPresArena_h___
#define nsPresArena_h___

#include "mozilla/ArenaObjectID.h"
#include "mozilla/ArenaRefPtr.h"
#include "mozilla/MemoryChecking.h" // Note: Do not remove this, needed for MOZ_HAVE_MEM_CHECKS below
#include "mozilla/MemoryReporting.h"
#include <stdint.h>
#include "nscore.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "plarena.h"

struct nsArenaMemoryStats;

class nsPresArena {
public:
  nsPresArena();
  ~nsPresArena();

  /**
   * Pool allocation with recycler lists indexed by object size, aSize.
   */
  void* AllocateBySize(size_t aSize)
  {
    return Allocate(uint32_t(aSize) |
                    uint32_t(mozilla::eArenaObjectID_NON_OBJECT_MARKER), aSize);
  }
  void FreeBySize(size_t aSize, void* aPtr)
  {
    Free(uint32_t(aSize) |
         uint32_t(mozilla::eArenaObjectID_NON_OBJECT_MARKER), aPtr);
  }

  /**
   * Pool allocation with recycler lists indexed by frame-type ID.
   * Every aID must always be used with the same object size, aSize.
   */
  void* AllocateByFrameID(nsQueryFrame::FrameIID aID, size_t aSize)
  {
    return Allocate(aID, aSize);
  }
  void FreeByFrameID(nsQueryFrame::FrameIID aID, void* aPtr)
  {
    Free(aID, aPtr);
  }

  /**
   * Pool allocation with recycler lists indexed by object-type ID (see above).
   * Every aID must always be used with the same object size, aSize.
   */
  void* AllocateByObjectID(mozilla::ArenaObjectID aID, size_t aSize)
  {
    return Allocate(aID, aSize);
  }
  void FreeByObjectID(mozilla::ArenaObjectID aID, void* aPtr)
  {
    Free(aID, aPtr);
  }

  /**
   * Register an ArenaRefPtr to be cleared when this arena is about to
   * be destroyed.
   *
   * (Defined in ArenaRefPtrInlines.h.)
   *
   * @param aPtr The ArenaRefPtr to clear.
   * @param aObjectID The nsPresArena::ObjectID value that uniquely identifies
   *   the type of object the ArenaRefPtr holds.
   */
  template<typename T>
  void RegisterArenaRefPtr(mozilla::ArenaRefPtr<T>* aPtr);

  /**
   * Deregister an ArenaRefPtr that was previously registered with
   * RegisterArenaRefPtr.
   */
  template<typename T>
  void DeregisterArenaRefPtr(mozilla::ArenaRefPtr<T>* aPtr)
  {
    MOZ_ASSERT(mArenaRefPtrs.Contains(aPtr));
    mArenaRefPtrs.Remove(aPtr);
  }

  /**
   * Clears all currently registered ArenaRefPtrs.  This will be called during
   * the destructor, but can be called by users of nsPresArena who want to
   * ensure arena-allocated objects are released earlier.
   */
  void ClearArenaRefPtrs();

  /**
   * Increment aArenaStats with sizes of interesting objects allocated in this
   * arena and its mOther field with the size of everything else.
   */
  void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              nsArenaMemoryStats* aArenaStats);

private:
  void* Allocate(uint32_t aCode, size_t aSize);
  void Free(uint32_t aCode, void* aPtr);

  inline void ClearArenaRefPtrWithoutDeregistering(
      void* aPtr,
      mozilla::ArenaObjectID aObjectID);

  // All keys to this hash table fit in 32 bits (see below) so we do not
  // bother actually hashing them.
  class FreeList : public PLDHashEntryHdr
  {
  public:
    typedef uint32_t KeyType;
    nsTArray<void *> mEntries;
    size_t mEntrySize;
    size_t mEntriesEverAllocated;

    typedef const void* KeyTypePointer;
    KeyTypePointer mKey;

    explicit FreeList(KeyTypePointer aKey)
    : mEntrySize(0), mEntriesEverAllocated(0), mKey(aKey) {}
    // Default copy constructor and destructor are ok.

    bool KeyEquals(KeyTypePointer const aKey) const
    { return mKey == aKey; }

    static KeyTypePointer KeyToPointer(KeyType aKey)
    { return NS_INT32_TO_PTR(aKey); }

    static PLDHashNumber HashKey(KeyTypePointer aKey)
    { return NS_PTR_TO_INT32(aKey); }

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
    { return mEntries.ShallowSizeOfExcludingThis(aMallocSizeOf); }

    enum { ALLOW_MEMMOVE = false };
  };

  nsTHashtable<FreeList> mFreeLists;
  PLArenaPool mPool;
  nsDataHashtable<nsPtrHashKey<void>, mozilla::ArenaObjectID> mArenaRefPtrs;
};

#endif
