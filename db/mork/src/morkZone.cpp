/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKZONE_
#include "morkZone.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// { ===== begin morkNode interface =====
// public: // morkNode virtual methods
void morkZone::CloseMorkNode(morkEnv* ev) // CloseZone() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseZone(ev);
    this->MarkShut();
  }
}

morkZone::~morkZone() // assert that CloseZone() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

// public: // morkMap construction & destruction
morkZone::morkZone(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioNodeHeap, nsIMdbHeap* ioZoneHeap)
: morkNode(ev, inUsage, ioNodeHeap)
, mZone_Heap( 0 )
, mZone_HeapVolume( 0 )
, mZone_BlockVolume( 0 )
, mZone_RunVolume( 0 )
, mZone_ChipVolume( 0 )
  
, mZone_FreeOldRunVolume( 0 )
  
, mZone_HunkCount( 0 )
, mZone_FreeOldRunCount( 0 )

, mZone_HunkList( 0 )
, mZone_FreeOldRunList( 0 )
  
, mZone_At( 0 )
, mZone_AtSize( 0 )
    
  // morkRun*     mZone_FreeRuns[ morkZone_kBuckets + 1 ];
{

  morkRun** runs = mZone_FreeRuns;
  morkRun** end = runs + (morkZone_kBuckets + 1); // one past last slot
  --runs; // prepare for preincrement
  while ( ++runs < end ) // another slot in array?
    *runs = 0; // clear all the slots
  
  if ( ev->Good() )
  {
    if ( ioZoneHeap )
    {
      nsIMdbHeap_SlotStrongHeap(ioZoneHeap, ev, &mZone_Heap);
      if ( ev->Good() )
        mNode_Derived = morkDerived_kZone;
    }
    else
      ev->NilPointerError();
  }
}

void morkZone::CloseZone(morkEnv* ev) // called by CloseMorkNode()
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      nsIMdbHeap* heap = mZone_Heap;
      if ( heap )
      {
        morkHunk* hunk = 0;
        nsIMdbEnv* mev = ev->AsMdbEnv();
        
        morkHunk* next = mZone_HunkList;
        while ( ( hunk = next ) != 0 )
        {
#ifdef morkHunk_USE_TAG_SLOT
          if ( !hunk->HunkGoodTag()  )
            hunk->BadHunkTagWarning(ev);
#endif /* morkHunk_USE_TAG_SLOT */

          next = hunk->HunkNext();
          heap->Free(mev, hunk);
        }
      }
      nsIMdbHeap_SlotStrongHeap((nsIMdbHeap*) 0, ev, &mZone_Heap);
      this->MarkShut();
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====

/*static*/ void
morkZone::NonZoneTypeError(morkEnv* ev)
{
  ev->NewError("non morkZone");
}

/*static*/ void
morkZone::NilZoneHeapError(morkEnv* ev)
{
  ev->NewError("nil mZone_Heap");
}

/*static*/ void
morkHunk::BadHunkTagWarning(morkEnv* ev)
{
  ev->NewWarning("bad mHunk_Tag");
}

/*static*/ void
morkRun::BadRunTagError(morkEnv* ev)
{
  ev->NewError("bad mRun_Tag");
}

/*static*/ void
morkRun::RunSizeAlignError(morkEnv* ev)
{
  ev->NewError("bad RunSize() alignment");
}

// { ===== begin morkZone methods =====


mork_size morkZone::zone_grow_at(morkEnv* ev, mork_size inNeededSize)
{
  mZone_At = 0;       // remove any ref to current hunk
  mZone_AtSize = 0;   // zero available bytes in current hunk
  
  mork_size runSize = 0; // actual size of a particular run
  
  // try to find a run in old run list with at least inNeededSize bytes:
  morkRun* run = mZone_FreeOldRunList; // cursor in list scan
  morkRun* prev = 0; // the node before run in the list scan
  
  while ( run ) // another run in list to check?
  {
    morkOldRun* oldRun = (morkOldRun*) run;
    mork_size oldSize = oldRun->OldSize();
    if ( oldSize >= inNeededSize ) // found one big enough?
    {
      runSize = oldSize;
      break; // end while loop early
    }
    prev = run; // remember last position in singly linked list
    run = run->RunNext(); // advance cursor to next node in list
  }
  if ( runSize && run ) // found a usable old run?
  {
    morkRun* next = run->RunNext();
    if ( prev ) // another node in free list precedes run?
      prev->RunSetNext(next); // unlink run
    else
      mZone_FreeOldRunList = run; // unlink run from head of list
      
    run->RunSetSize(runSize);
    mZone_At = (mork_u1*) run->RunAsBlock();
    mZone_AtSize = runSize;

#ifdef morkZone_CONFIG_DEBUG
#ifdef morkZone_CONFIG_ALIGN_8
    mork_ip lowThree = ((mork_ip) mZone_At) & 7;
    if ( lowThree ) // not 8 byte aligned?
#else /*morkZone_CONFIG_ALIGN_8*/
    mork_ip lowTwo = ((mork_ip) mZone_At) & 3;
    if ( lowTwo ) // not 4 byte aligned?
#endif /*morkZone_CONFIG_ALIGN_8*/
      ev->NewWarning("mZone_At not aligned");
#endif /*morkZone_CONFIG_DEBUG*/
  }
  else // need to allocate a brand new run
  {
    inNeededSize += 7; // allow for possible alignment padding
    mork_size newSize = ( inNeededSize > morkZone_kNewHunkSize )?
      inNeededSize : morkZone_kNewHunkSize;
      
    morkHunk* hunk = this->zone_new_hunk(ev, newSize);
    if ( hunk )
    {
      morkRun* hunkRun = hunk->HunkRun();
      mork_u1* at = (mork_u1*) hunkRun->RunAsBlock();
      mork_ip lowBits = ((mork_ip) at) & 7;
      if ( lowBits ) // not 8 byte aligned?
      {
        mork_ip skip = (8 - lowBits); // skip the complement to align
        at += skip;
        newSize -= skip;
      }
      mZone_At = at;
      mZone_AtSize = newSize;
    }
  }
  
  return mZone_AtSize;
}

morkHunk* morkZone::zone_new_hunk(morkEnv* ev, mdb_size inSize) // alloc  
{
  mdb_size hunkSize = inSize + sizeof(morkHunk);
  void* outBlock = 0; // we are going straight to the heap:
  mZone_Heap->Alloc(ev->AsMdbEnv(), hunkSize, &outBlock);
  if ( outBlock )
  {
#ifdef morkZone_CONFIG_VOL_STATS
    mZone_HeapVolume += hunkSize; // track all heap allocations
#endif /* morkZone_CONFIG_VOL_STATS */
  
    morkHunk* hunk = (morkHunk*) outBlock;
#ifdef morkHunk_USE_TAG_SLOT
    hunk->HunkInitTag();
#endif /* morkHunk_USE_TAG_SLOT */
  
    hunk->HunkSetNext(mZone_HunkList);
    mZone_HunkList = hunk;
    ++mZone_HunkCount;
    
    morkRun* run = hunk->HunkRun();
    run->RunSetSize(inSize);
#ifdef morkRun_USE_TAG_SLOT
    run->RunInitTag();
#endif /* morkRun_USE_TAG_SLOT */
    
    return hunk;
  }
  if ( ev->Good() ) // got this far without any error reported yet?
    ev->OutOfMemoryError();
  return (morkHunk*) 0;
}

void* morkZone::zone_new_chip(morkEnv* ev, mdb_size inSize) // alloc  
{
#ifdef morkZone_CONFIG_VOL_STATS
  mZone_BlockVolume += inSize; // sum sizes of both chips and runs
#endif /* morkZone_CONFIG_VOL_STATS */
  
  mork_u1* at = mZone_At;
  mork_size atSize = mZone_AtSize; // available bytes in current hunk
  if ( atSize >= inSize ) // current hunk can satisfy request?
  {
    mZone_At = at + inSize;
    mZone_AtSize = atSize - inSize;
    return at;
  }
  else if ( atSize > morkZone_kMaxHunkWaste ) // over max waste allowed?
  {
    morkHunk* hunk = this->zone_new_hunk(ev, inSize);
    if ( hunk )
      return hunk->HunkRun();
      
    return (void*) 0; // show allocation has failed
  }
  else // get ourselves a new hunk for suballocation:
  {
    atSize = this->zone_grow_at(ev, inSize); // get a new hunk
  }

  if ( atSize >= inSize ) // current hunk can satisfy request?
  {
    at = mZone_At;
    mZone_At = at + inSize;
    mZone_AtSize = atSize - inSize;
    return at;
  }
  
  if ( ev->Good() ) // got this far without any error reported yet?
    ev->OutOfMemoryError();
    
  return (void*) 0; // show allocation has failed
}

void* morkZone::ZoneNewChip(morkEnv* ev, mdb_size inSize) // alloc  
{
#ifdef morkZone_CONFIG_ARENA

#ifdef morkZone_CONFIG_DEBUG
  if ( !this->IsZone() )
    this->NonZoneTypeError(ev);
  else if ( !mZone_Heap )
    this->NilZoneHeapError(ev);
#endif /*morkZone_CONFIG_DEBUG*/

#ifdef morkZone_CONFIG_ALIGN_8
  inSize += 7;
  inSize &= ~((mork_ip) 7); // force to multiple of 8 bytes
#else /*morkZone_CONFIG_ALIGN_8*/
  inSize += 3;
  inSize &= ~((mork_ip) 3); // force to multiple of 4 bytes
#endif /*morkZone_CONFIG_ALIGN_8*/

#ifdef morkZone_CONFIG_VOL_STATS
  mZone_ChipVolume += inSize; // sum sizes of chips only
#endif /* morkZone_CONFIG_VOL_STATS */

  return this->zone_new_chip(ev, inSize);

#else /*morkZone_CONFIG_ARENA*/
  void* outBlock = 0;
  mZone_Heap->Alloc(ev->AsMdbEnv(), inSize, &outBlock);
  return outBlock;
#endif /*morkZone_CONFIG_ARENA*/
  
}
  
// public: // ...but runs do indeed know how big they are
void* morkZone::ZoneNewRun(morkEnv* ev, mdb_size inSize) // alloc  
{
#ifdef morkZone_CONFIG_ARENA

#ifdef morkZone_CONFIG_DEBUG
  if ( !this->IsZone() )
    this->NonZoneTypeError(ev);
  else if ( !mZone_Heap )
    this->NilZoneHeapError(ev);
#endif /*morkZone_CONFIG_DEBUG*/

  inSize += morkZone_kRoundAdd;
  inSize &= morkZone_kRoundMask;
  if ( inSize <= morkZone_kMaxCachedRun )
  {
    morkRun** bucket = mZone_FreeRuns + (inSize >> morkZone_kRoundBits);
    morkRun* hit = *bucket;
    if ( hit ) // cache hit?
    {
      *bucket = hit->RunNext();
      hit->RunSetSize(inSize);
      return hit->RunAsBlock();
    }
  }
  mdb_size blockSize = inSize + sizeof(morkRun); // plus run overhead
#ifdef morkZone_CONFIG_VOL_STATS
  mZone_RunVolume += blockSize; // sum sizes of runs only
#endif /* morkZone_CONFIG_VOL_STATS */
  morkRun* run = (morkRun*) this->zone_new_chip(ev, blockSize);
  if ( run )
  {
    run->RunSetSize(inSize);
#ifdef morkRun_USE_TAG_SLOT
    run->RunInitTag();
#endif /* morkRun_USE_TAG_SLOT */
    return run->RunAsBlock();
  }
  
  if ( ev->Good() ) // got this far without any error reported yet?
    ev->OutOfMemoryError();
  
  return (void*) 0; // indicate failed allocation

#else /*morkZone_CONFIG_ARENA*/
  void* outBlock = 0;
  mZone_Heap->Alloc(ev->AsMdbEnv(), inSize, &outBlock);
  return outBlock;
#endif /*morkZone_CONFIG_ARENA*/
}

void morkZone::ZoneZapRun(morkEnv* ev, void* ioRunBlock) // free
{
#ifdef morkZone_CONFIG_ARENA

  morkRun* run = morkRun::BlockAsRun(ioRunBlock);
  mdb_size runSize = run->RunSize();
#ifdef morkZone_CONFIG_VOL_STATS
  mZone_BlockVolume -= runSize; // tracking sizes of both chips and runs
#endif /* morkZone_CONFIG_VOL_STATS */

#ifdef morkZone_CONFIG_DEBUG
  if ( !this->IsZone() )
    this->NonZoneTypeError(ev);
  else if ( !mZone_Heap )
    this->NilZoneHeapError(ev);
  else if ( !ioRunBlock )
    ev->NilPointerError();
  else if ( runSize & morkZone_kRoundAdd )
    run->RunSizeAlignError(ev);
#ifdef morkRun_USE_TAG_SLOT
  else if ( !run->RunGoodTag() )
    run->BadRunTagError(ev);
#endif /* morkRun_USE_TAG_SLOT */
#endif /*morkZone_CONFIG_DEBUG*/

  if ( runSize <= morkZone_kMaxCachedRun ) // goes into free run list?
  {
    morkRun** bucket = mZone_FreeRuns + (runSize >> morkZone_kRoundBits);
    run->RunSetNext(*bucket); // push onto free run list
    *bucket = run;
  }
  else // free old run list
  {
    run->RunSetNext(mZone_FreeOldRunList); // push onto free old run list
    mZone_FreeOldRunList = run;
    ++mZone_FreeOldRunCount;
#ifdef morkZone_CONFIG_VOL_STATS
    mZone_FreeOldRunVolume += runSize;
#endif /* morkZone_CONFIG_VOL_STATS */

    morkOldRun* oldRun = (morkOldRun*) run; // to access extra size slot
    oldRun->OldSetSize(runSize); // so we know how big this is later
  }

#else /*morkZone_CONFIG_ARENA*/
  mZone_Heap->Free(ev->AsMdbEnv(), ioRunBlock);
#endif /*morkZone_CONFIG_ARENA*/
}

void* morkZone::ZoneGrowRun(morkEnv* ev, void* ioRunBlock, mdb_size inSize)
{
#ifdef morkZone_CONFIG_ARENA

  morkRun* run = morkRun::BlockAsRun(ioRunBlock);
  mdb_size runSize = run->RunSize();

#ifdef morkZone_CONFIG_DEBUG
  if ( !this->IsZone() )
    this->NonZoneTypeError(ev);
  else if ( !mZone_Heap )
    this->NilZoneHeapError(ev);
#endif /*morkZone_CONFIG_DEBUG*/

#ifdef morkZone_CONFIG_ALIGN_8
  inSize += 7;
  inSize &= ~((mork_ip) 7); // force to multiple of 8 bytes
#else /*morkZone_CONFIG_ALIGN_8*/
  inSize += 3;
  inSize &= ~((mork_ip) 3); // force to multiple of 4 bytes
#endif /*morkZone_CONFIG_ALIGN_8*/

  if ( inSize > runSize )
  {
    void* newBuf = this->ZoneNewRun(ev, inSize);
    if ( newBuf )
    {
      MORK_MEMCPY(newBuf, ioRunBlock, runSize);
      this->ZoneZapRun(ev, ioRunBlock);
      
      return newBuf;
    }
  }
  else
    return ioRunBlock; // old size is big enough
  
  if ( ev->Good() ) // got this far without any error reported yet?
    ev->OutOfMemoryError();
  
  return (void*) 0; // indicate failed allocation

#else /*morkZone_CONFIG_ARENA*/
  void* outBlock = 0;
  mZone_Heap->Free(ev->AsMdbEnv(), ioRunBlock);
  return outBlock;
#endif /*morkZone_CONFIG_ARENA*/
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// { ===== begin nsIMdbHeap methods =====
/*virtual*/ mdb_err
morkZone::Alloc(nsIMdbEnv* mev, // allocate a piece of memory
  mdb_size inSize,   // requested size of new memory block 
  void** outBlock)  // memory block of inSize bytes, or nil
{
  mdb_err outErr = 0;
  void* block = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    block = this->ZoneNewRun(ev, inSize);
    outErr = ev->AsErr();
  }
  else
    outErr = 1;
    
  if ( outBlock )
    *outBlock = block;
    
  return outErr;
}
  
/*virtual*/ mdb_err
morkZone::Free(nsIMdbEnv* mev, // free block allocated earlier by Alloc()
  void* inBlock)
{
  mdb_err outErr = 0;
  if ( inBlock )
  {
    morkEnv* ev = morkEnv::FromMdbEnv(mev);
    if ( ev )
    {
      this->ZoneZapRun(ev, inBlock);
      outErr = ev->AsErr();
    }
    else
      outErr = 1;
  }
    
  return outErr;
}

/*virtual*/ mdb_err
morkZone::HeapAddStrongRef(nsIMdbEnv* mev) // does nothing
{
  MORK_USED_1(mev);
  return 0;
}

/*virtual*/ mdb_err
morkZone::HeapCutStrongRef(nsIMdbEnv* mev) // does nothing
{
  MORK_USED_1(mev);
  return 0;
}

// } ===== end nsIMdbHeap methods =====

