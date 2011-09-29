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
#include "prinit.h"
#include "prlog.h"

#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsPrintfCString.h"
#endif

// Even on 32-bit systems, we allocate objects from the frame arena
// that require 8-byte alignment.  The cast to PRUword is needed
// because plarena isn't as careful about mask construction as it
// ought to be.
#define ALIGN_SHIFT 3
#define PL_ARENA_CONST_ALIGN_MASK ((PRUword(1) << ALIGN_SHIFT) - 1)
#include "plarena.h"

#ifdef _WIN32
# include <windows.h>
#elif !defined(__OS2__)
# include <unistd.h>
# include <sys/mman.h>
# ifndef MAP_ANON
#  ifdef MAP_ANONYMOUS
#   define MAP_ANON MAP_ANONYMOUS
#  else
#   error "Don't know how to get anonymous memory"
#  endif
# endif
#endif

// Size to use for PLArena block allocations.
static const size_t ARENA_PAGE_SIZE = 8192;

// Freed memory is filled with a poison value, which we arrange to
// form a pointer either to an always-unmapped region of the address
// space, or to a page that has been reserved and rendered
// inaccessible via OS primitives.  See tests/TestPoisonArea.cpp for
// extensive discussion of the requirements for this page.  The code
// from here to 'class FreeList' needs to be kept in sync with that
// file.

#ifdef _WIN32
static void *
ReserveRegion(PRUword region, PRUword size)
{
  return VirtualAlloc((void *)region, size, MEM_RESERVE, PAGE_NOACCESS);
}

static void
ReleaseRegion(void *region, PRUword size)
{
  VirtualFree(region, size, MEM_RELEASE);
}

static bool
ProbeRegion(PRUword region, PRUword size)
{
  SYSTEM_INFO sinfo;
  GetSystemInfo(&sinfo);
  if (region >= (PRUword)sinfo.lpMaximumApplicationAddress &&
      region + size >= (PRUword)sinfo.lpMaximumApplicationAddress) {
    return true;
  } else {
    return false;
  }
}

static PRUword
GetDesiredRegionSize()
{
  SYSTEM_INFO sinfo;
  GetSystemInfo(&sinfo);
  return sinfo.dwAllocationGranularity;
}

#define RESERVE_FAILED 0

#elif defined(__OS2__)
static void *
ReserveRegion(PRUword region, PRUword size)
{
  // OS/2 doesn't support allocation at an arbitrary address,
  // so return an address that is known to be invalid.
  return (void*)0xFFFD0000;
}

static void
ReleaseRegion(void *region, PRUword size)
{
  return;
}

static bool
ProbeRegion(PRUword region, PRUword size)
{
  // There's no reliable way to probe an address in the system
  // arena other than by touching it and seeing if a trap occurs.
  return false;
}

static PRUword
GetDesiredRegionSize()
{
  // Page size is fixed at 4k.
  return 0x1000;
}

#define RESERVE_FAILED 0

#else // Unix

static void *
ReserveRegion(PRUword region, PRUword size)
{
  return mmap((caddr_t)region, size, PROT_NONE, MAP_PRIVATE|MAP_ANON, -1, 0);
}

static void
ReleaseRegion(void *region, PRUword size)
{
  munmap((caddr_t)region, size);
}

static bool
ProbeRegion(PRUword region, PRUword size)
{
  if (madvise((caddr_t)region, size, MADV_NORMAL)) {
    return true;
  } else {
    return false;
  }
}

static PRUword
GetDesiredRegionSize()
{
  return sysconf(_SC_PAGESIZE);
}

#define RESERVE_FAILED MAP_FAILED

#endif // system dependencies

PR_STATIC_ASSERT(sizeof(PRUword) == 4 || sizeof(PRUword) == 8);
PR_STATIC_ASSERT(sizeof(PRUword) == sizeof(void *));

static PRUword
ReservePoisonArea(PRUword rgnsize)
{
  if (sizeof(PRUword) == 8) {
    // Use the hardware-inaccessible region.
    // We have to avoid 64-bit constants and shifts by 32 bits, since this
    // code is compiled in 32-bit mode, although it is never executed there.
    return
      (((PRUword(0x7FFFFFFFu) << 31) << 1 | PRUword(0xF0DEAFFFu))
       & ~(rgnsize-1));

  } else {
    // First see if we can allocate the preferred poison address from the OS.
    PRUword candidate = (0xF0DEAFFF & ~(rgnsize-1));
    void *result = ReserveRegion(candidate, rgnsize);
    if (result == (void *)candidate) {
      // success - inaccessible page allocated
      return candidate;
    }

    // That didn't work, so see if the preferred address is within a range
    // of permanently inacessible memory.
    if (ProbeRegion(candidate, rgnsize)) {
      // success - selected page cannot be usable memory
      if (result != RESERVE_FAILED)
        ReleaseRegion(result, rgnsize);
      return candidate;
    }

    // The preferred address is already in use.  Did the OS give us a
    // consolation prize?
    if (result != RESERVE_FAILED) {
      return PRUword(result);
    }

    // It didn't, so try to allocate again, without any constraint on
    // the address.
    result = ReserveRegion(0, rgnsize);
    if (result != RESERVE_FAILED) {
      return PRUword(result);
    }

    NS_RUNTIMEABORT("no usable poison region identified");
    return 0;
  }
}

static PRUword ARENA_POISON;
static PRCallOnceType ARENA_POISON_guard;

static PRStatus
ARENA_POISON_init()
{
  PRUword rgnsize = GetDesiredRegionSize();
  PRUword rgnbase = ReservePoisonArea(rgnsize);

  if (rgnsize == 0) // can't happen
    return PR_FAILURE;

  ARENA_POISON = rgnbase + rgnsize/2 - 1;

#ifdef MOZ_CRASHREPORTER
  nsCOMPtr<nsICrashReporter> cr =
    do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  bool enabled;
  if (cr && NS_SUCCEEDED(cr->GetEnabled(&enabled)) && enabled) {
    cr->AnnotateCrashReport(NS_LITERAL_CSTRING("FramePoisonBase"),
                            nsPrintfCString(17, "%.16llx", PRUint64(rgnbase)));
    cr->AnnotateCrashReport(NS_LITERAL_CSTRING("FramePoisonSize"),
                            nsPrintfCString("%lu", PRUint32(rgnsize)));
  }
#endif
  return PR_SUCCESS;
}

#ifndef DEBUG_TRACEMALLOC_PRESARENA

// All keys to this hash table fit in 32 bits (see below) so we do not
// bother actually hashing them.

namespace {

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

  bool KeyEquals(KeyTypePointer const aKey) const
  { return mKey == aKey; }

  static KeyTypePointer KeyToPointer(KeyType aKey)
  { return NS_INT32_TO_PTR(aKey); }

  static PLDHashNumber HashKey(KeyTypePointer aKey)
  { return NS_PTR_TO_INT32(aKey); }

  enum { ALLOW_MEMMOVE = PR_FALSE };
  friend class nsTHashtable<FreeList>;
};

}

struct nsPresArena::State {
  nsTHashtable<FreeList> mFreeLists;
  PLArenaPool mPool;

  State()
  {
    mFreeLists.Init();
    PL_INIT_ARENA_POOL(&mPool, "PresArena", ARENA_PAGE_SIZE);
    PR_CallOnce(&ARENA_POISON_guard, ARENA_POISON_init);
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

PRUint32
nsPresArena::Size()
{
  PLArena *arena = &mState->mPool.first;
  PRUint32 result = 0;

  while (arena) {
    result += arena->limit - arena->base;
    arena = arena->next;
  }

  return result;
}

#else
// Stub implementation that forwards everything to malloc and does not
// poison allocations (it still initializes the poison value though,
// for external use through GetPoisonValue()).

struct nsPresArena::State
{

  State()
  {
    PR_CallOnce(&ARENA_POISON_guard, ARENA_POISON_init);
  }

  void* Allocate(PRUint32 /* unused */, size_t aSize)
  {
    return PR_Malloc(aSize);
  }

  void Free(PRUint32 /* unused */, void* aPtr)
  {
    PR_Free(aPtr);
  }
};

PRUint32
nsPresArena::Size()
{
  return 0;
}

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

/* static */ PRUword
nsPresArena::GetPoisonValue()
{
  return ARENA_POISON;
}
