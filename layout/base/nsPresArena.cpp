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
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "prmem.h"

#ifndef DEBUG_TRACEMALLOC_PRESARENA

// Even on 32-bit systems, we allocate objects from the frame arena
// that require 8-byte alignment.  The cast to PRUword is needed
// because plarena isn't as careful about mask construction as it
// ought to be.
#define ALIGN_SHIFT 3
#define PL_ARENA_CONST_ALIGN_MASK ((PRUword(1) << ALIGN_SHIFT) - 1)
#include "plarena.h"

// Size to use for PLArena block allocations.
static const size_t ARENA_PAGE_SIZE = 4096;

// Freed memory is filled with a poison value, which is believed to
// form a pointer to an always-unmapped region of the address space on
// all platforms of interest. The low 12 bits of this number are
// chosen to fall in the middle of the typical 4096-byte page, and
// make the address odd.
//
// With the possible exception of PPC64, current 64-bit CPUs permit
// only a subset (2^48 to 2^56, depending) of the full virtual address
// space to be used.  x86-64 has the inaccessible region in the
// *middle* of the address space, whereas all others are believed to
// have it at the highest addresses.  Use an address in this region if
// we possibly can; if the hardware doesn't let anyone use it, we
// needn't worry about the OS.
//
// TODO: Confirm that this value is a pointer to an always-unmapped
// address space region on (at least) Win32, Win64, WinCE, ARM Linux,
// MacOSX, and add #ifdefs below as necessary. (Bug 507294.)

#if defined(__x86_64__) || defined(_M_AMD64)
const PRUword ARENA_POISON = 0x7FFFFFFFF0DEA7FF;
#else
// This evaluates to 0xF0DE_A7FF when PRUword is 32 bits long, but to
// 0xFFFF_FFFF_F0DE_A7FF when it's 64 bits.
const PRUword ARENA_POISON = (~PRUword(0x0FFFFF00) | PRUword(0x0DEA700));
#endif

// All keys to this hash table fit in 32 bits (see below) so we do not
// bother actually hashing them.
class FreeList : public PLDHashEntryHdr
{
public:
  typedef PRUint32 KeyType;
  nsTArray<void *> mEntries;
  size_t mEntrySize;

protected:
  typedef const void* KeyTypePointer;
  KeyTypePointer mKey;

  FreeList(KeyTypePointer aKey) : mEntrySize(0), mKey(aKey) {}
  // Default copy constructor and destructor are ok.

  PRBool KeyEquals(KeyTypePointer const aKey) const
  { return mKey == aKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey)
  { return NS_INT32_TO_PTR(aKey); }

  static PLDHashNumber HashKey(KeyTypePointer aKey)
  { return NS_PTR_TO_INT32(aKey); }

  enum { ALLOW_MEMMOVE = PR_FALSE };
  friend class nsTHashtable<FreeList>;
};

struct nsPresArena::State {
  nsTHashtable<FreeList> mFreeLists;
  PLArenaPool mPool;

  State()
  {
    mFreeLists.Init();
    PL_INIT_ARENA_POOL(&mPool, "PresArena", ARENA_PAGE_SIZE);
  }

  ~State()
  {
    PL_FinishArenaPool(&mPool);
  }

  void* Allocate(PRUint32 aCode, size_t aSize)
  {
    NS_ABORT_IF_FALSE(aSize > 0, "PresArena cannot allocate zero bytes");

    // We only hand out aligned sizes
    aSize = PL_ARENA_ALIGN(&mPool, aSize);

    // If there is no free-list entry for this type already, we have
    // to create one now, to record its size.
    FreeList* list = mFreeLists.PutEntry(aCode);
    if (!list) {
      return nsnull;
    }

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
#ifdef DEBUG
      {
        char* p = reinterpret_cast<char*>(result);
        char* limit = p + list->mEntrySize;
        for (; p < limit; p += sizeof(PRUword)) {
          NS_ABORT_IF_FALSE(*reinterpret_cast<PRUword*>(p) == ARENA_POISON,
                            "PresArena: poison overwritten");
        }
      }
#endif
      return result;
    }

    // Allocate a new chunk from the arena
    PL_ARENA_ALLOCATE(result, &mPool, aSize);
    return result;
  }

  void Free(PRUint32 aCode, void* aPtr)
  {
    // Try to recycle this entry.
    FreeList* list = mFreeLists.GetEntry(aCode);
    NS_ABORT_IF_FALSE(list, "no free list for pres arena object");
    NS_ABORT_IF_FALSE(list->mEntrySize > 0, "PresArena cannot free zero bytes");

    char* p = reinterpret_cast<char*>(aPtr);
    char* limit = p + list->mEntrySize;
    for (; p < limit; p += sizeof(PRUword)) {
      *reinterpret_cast<PRUword*>(p) = ARENA_POISON;
    }

    list->mEntries.AppendElement(aPtr);
  }
};

#else
// Stub implementation that forwards everything to malloc and does not
// poison.

struct nsPresArena::State
{
  void* Allocate(PRUint32 /* unused */, size_t aSize)
  {
    return PR_Malloc(aSize);
  }

  void Free(PRUint32 /* unused */, void* aPtr)
  {
    PR_Free(aPtr);
  }
};

#endif // DEBUG_TRACEMALLOC_PRESARENA

// Public interface
nsPresArena::nsPresArena()
  : mState(new nsPresArena::State())
{}

nsPresArena::~nsPresArena()
{
  delete mState;
}

void*
nsPresArena::AllocateBySize(size_t aSize)
{
  return mState->Allocate(PRUint32(aSize) |
                          PRUint32(nsQueryFrame::NON_FRAME_MARKER),
                          aSize);
}

void
nsPresArena::FreeBySize(size_t aSize, void* aPtr)
{
  mState->Free(PRUint32(aSize) |
               PRUint32(nsQueryFrame::NON_FRAME_MARKER), aPtr);
}

void*
nsPresArena::AllocateByCode(nsQueryFrame::FrameIID aCode, size_t aSize)
{
  return mState->Allocate(aCode, aSize);
}

void
nsPresArena::FreeByCode(nsQueryFrame::FrameIID aCode, void* aPtr)
{
  mState->Free(aCode, aPtr);
}
