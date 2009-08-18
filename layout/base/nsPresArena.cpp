/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Clark <buster@netscape.com>
 *   Håkan Waara <hwaara@chello.se>
 *   Dan Rosen <dr@netscape.com>
 *   Daniel Glazman <glazman@netscape.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

/* arena allocation for the frame tree and closely-related objects */

#include "nsPresArena.h"
#include "nsCRT.h"
#include "nsDebug.h"
#include "prmem.h"

// Uncomment this to disable arenas, instead forwarding to
// malloc for every allocation.
//#define DEBUG_TRACEMALLOC_PRESARENA 1

#ifndef DEBUG_TRACEMALLOC_PRESARENA

// Even on 32-bit systems, we allocate objects from the frame arena
// that require 8-byte alignment.  The cast to PRUword is needed
// because plarena isn't as careful about mask construction as it
// ought to be.
#define ALIGN_SHIFT 3
#define PL_ARENA_CONST_ALIGN_MASK ((PRUword(1) << ALIGN_SHIFT) - 1)
#include "plarena.h"

// Largest chunk size we recycle
static const size_t MAX_RECYCLED_SIZE = 400;

// Recycler array entry N (0 <= N < NUM_RECYCLERS) holds chunks of
// size (N+1) << ALIGN_SHIFT, thus we need this many array entries.
static const size_t NUM_RECYCLERS = MAX_RECYCLED_SIZE >> ALIGN_SHIFT;

// Size to use for PLArena block allocations.
static const size_t ARENA_PAGE_SIZE = 4096;

struct nsPresArena::State {
  void*       mRecyclers[NUM_RECYCLERS];
  PLArenaPool mPool;

  State()
  {
    PL_INIT_ARENA_POOL(&mPool, "PresArena", ARENA_PAGE_SIZE);
    memset(mRecyclers, 0, sizeof(mRecyclers));
  }

  ~State()
  {
    PL_FinishArenaPool(&mPool);
  }

  void* Allocate(size_t aSize)
  {
    void* result = nsnull;

    // Recycler lists are indexed by aligned size
    aSize = PL_ARENA_ALIGN(&mPool, aSize);

    // Check recyclers first
    if (aSize <= MAX_RECYCLED_SIZE) {
      const size_t index = (aSize >> ALIGN_SHIFT) - 1;
      result = mRecyclers[index];
      if (result) {
        // Need to move to the next object
        void* next = *((void**)result);
        mRecyclers[index] = next;
      }
    }

    if (!result) {
      // Allocate a new chunk from the arena
      PL_ARENA_ALLOCATE(result, &mPool, aSize);
    }

    return result;
  }

  void Free(size_t aSize, void* aPtr)
  {
    // Recycler lists are indexed by aligned size
    aSize = PL_ARENA_ALIGN(&mPool, aSize);

    // See if it's a size that we recycle
    if (aSize <= MAX_RECYCLED_SIZE) {
      const size_t index = (aSize >> ALIGN_SHIFT) - 1;
      void* currentTop = mRecyclers[index];
      mRecyclers[index] = aPtr;
      *((void**)aPtr) = currentTop;
    }
#if defined DEBUG_dbaron || defined DEBUG_zack
    else {
      fprintf(stderr,
              "WARNING: nsPresArena::FreeFrame leaking chunk of %lu bytes.\n",
              aSize);
    }
#endif
  }
};

#else
// Stub implementation that just forwards everything to malloc.

struct nsPresArena::State
{
  void* Allocate(size_t aSize)
  {
    return PR_Malloc(aSize);
  }

  void Free(size_t /*unused*/, void* aPtr)
  {
    PR_Free(aPtr);
  }
};

#endif // DEBUG_TRACEMALLOC_PRESARENA

// Public interface
nsPresArena::nsPresArena()
  : mState(new nsPresArena::State())
#ifdef DEBUG
  , mAllocCount(0)
#endif
{}

nsPresArena::~nsPresArena()
{
#ifdef DEBUG
  NS_ASSERTION(mAllocCount == 0,
               "Some PresArena objects were not freed");
#endif
  delete mState;
}

void*
nsPresArena::Allocate(size_t aSize)
{
  NS_ABORT_IF_FALSE(aSize > 0, "PresArena cannot allocate zero bytes");
  void* result = mState->Allocate(aSize);
#ifdef DEBUG
  if (result)
    mAllocCount++;
#endif
  return result;
}

void
nsPresArena::Free(size_t aSize, void* aPtr)
{
  NS_ABORT_IF_FALSE(aSize > 0, "PresArena cannot free zero bytes");
#ifdef DEBUG
  // Mark the memory with 0xdd in DEBUG builds so that there will be
  // problems if someone tries to access memory that they've freed.
  memset(aPtr, 0xdd, aSize);
  mAllocCount--;
#endif
  mState->Free(aSize, aPtr);
}
