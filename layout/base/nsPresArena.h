/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef nsPresArena_h___
#define nsPresArena_h___

#include "nscore.h"
#include "nsQueryFrame.h"

#include "mozilla/StandardInteger.h"

struct nsArenaMemoryStats;

// Uncomment this to disable arenas, instead forwarding to
// malloc for every allocation.
//#define DEBUG_TRACEMALLOC_PRESARENA 1

// The debugging version of nsPresArena does not free all the memory it
// allocated when the arena itself is destroyed.
#ifdef DEBUG_TRACEMALLOC_PRESARENA
#define PRESARENA_MUST_FREE_DURING_DESTROY true
#else
#define PRESARENA_MUST_FREE_DURING_DESTROY false
#endif

class nsPresArena {
public:
  nsPresArena();
  ~nsPresArena();

  // Pool allocation with recycler lists indexed by object size, aSize.
  NS_HIDDEN_(void*) AllocateBySize(size_t aSize);
  NS_HIDDEN_(void)  FreeBySize(size_t aSize, void* aPtr);

  // Pool allocation with recycler lists indexed by frame-type ID.
  // Every aID must always be used with the same object size, aSize.
  NS_HIDDEN_(void*) AllocateByFrameID(nsQueryFrame::FrameIID aID, size_t aSize);
  NS_HIDDEN_(void)  FreeByFrameID(nsQueryFrame::FrameIID aID, void* aPtr);

  enum ObjectID {
    nsLineBox_id = nsQueryFrame::NON_FRAME_MARKER,
    nsRuleNode_id,
    nsStyleContext_id,

    // The PresArena implementation uses this bit to distinguish objects
    // allocated by size from objects allocated by type ID (that is, frames
    // using AllocateByFrameID and other objects using AllocateByObjectID).
    // It should not collide with any Object ID (above) or frame ID (in
    // nsQueryFrame.h).  It is not 0x80000000 to avoid the question of
    // whether enumeration constants are signed.
    NON_OBJECT_MARKER = 0x40000000
  };

  // Pool allocation with recycler lists indexed by object-type ID (see above).
  // Every aID must always be used with the same object size, aSize.
  NS_HIDDEN_(void*) AllocateByObjectID(ObjectID aID, size_t aSize);
  NS_HIDDEN_(void)  FreeByObjectID(ObjectID aID, void* aPtr);

  /**
   * Fill aArenaStats with sizes of interesting objects allocated in
   * this arena and its mOther field with the size of everything else.
   */
  void SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                           nsArenaMemoryStats* aArenaStats);

  /**
   * Get the poison value that can be used to fill a memory space with
   * an address that leads to a safe crash when dereferenced.
   *
   * The caller is responsible for ensuring that a pres shell has been
   * initialized before calling this.
   */
  static uintptr_t GetPoisonValue();

private:
  struct State;
  State* mState;
};

#endif
