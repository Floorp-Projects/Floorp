/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _MORKZONE_
#define _MORKZONE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKDEQUE_
#include "morkDeque.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*| CONFIG_DEBUG: do paranoid debug checks if defined.
|*/
#ifdef MORK_DEBUG
#define morkZone_CONFIG_DEBUG 1 /* debug paranoid if defined */
#endif /*MORK_DEBUG*/

/*| CONFIG_STATS: keep volume and usage statistics.
|*/
#define morkZone_CONFIG_VOL_STATS 1 /* count space used by zone instance */

/*| CONFIG_ARENA: if this is defined, then the morkZone class will alloc big
**| blocks from the zone's heap, and suballocate from these.  If undefined,
**| then morkZone will just pass all calls through to the zone's heap.
|*/
#ifdef MORK_ENABLE_ZONE_ARENAS
#define morkZone_CONFIG_ARENA 1 /* be arena, if defined; otherwise no-op */
#endif /*MORK_ENABLE_ZONE_ARENAS*/

/*| CONFIG_ALIGN_8: if this is defined, then the morkZone class will give
**| blocks 8 byte alignment instead of only 4 byte alignment.
|*/
#ifdef MORK_CONFIG_ALIGN_8
#define morkZone_CONFIG_ALIGN_8 1 /* ifdef: align to 8 bytes, otherwise 4 */
#endif /*MORK_CONFIG_ALIGN_8*/

/*| CONFIG_PTR_SIZE_4: if this is defined, then the morkZone class will
**| assume sizeof(void*) == 4, so a tag slot for padding is needed.
|*/
#ifdef MORK_CONFIG_PTR_SIZE_4
#define morkZone_CONFIG_PTR_SIZE_4 1 /* ifdef: sizeof(void*) == 4 */
#endif /*MORK_CONFIG_PTR_SIZE_4*/

/*| morkZone_USE_TAG_SLOT: if this is defined, then define slot mRun_Tag
**| in order to achieve eight byte alignment after the mRun_Next slot.
|*/
#if defined(morkZone_CONFIG_ALIGN_8) && defined(morkZone_CONFIG_PTR_SIZE_4)
#define morkRun_USE_TAG_SLOT 1  /* need mRun_Tag slot inside morkRun */
#define morkHunk_USE_TAG_SLOT 1 /* need mHunk_Tag slot inside morkHunk */
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkRun_kTag ((mork_u4) 0x6D52754E ) /* ascii 'mRuN' */

/*| morkRun: structure used by morkZone for sized blocks
|*/
class morkRun {

protected: // member variable slots
#ifdef morkRun_USE_TAG_SLOT
  mork_u4   mRun_Tag; // force 8 byte alignment after mRun_Next
#endif /* morkRun_USE_TAG_SLOT */

  morkRun*  mRun_Next;
  
public: // pointer interpretation of mRun_Next (when inside a list):
  morkRun*   RunNext() const { return mRun_Next; }
  void       RunSetNext(morkRun* ioNext) { mRun_Next = ioNext; }
  
public: // size interpretation of mRun_Next (when not inside a list):
  mork_size  RunSize() const { return (mork_size) ((mork_ip) mRun_Next); }
  void       RunSetSize(mork_size inSize)
             { mRun_Next = (morkRun*) ((mork_ip) inSize); }
  
public: // maintenance and testing of optional tag magic signature slot:
#ifdef morkRun_USE_TAG_SLOT
  void       RunInitTag() { mRun_Tag = morkRun_kTag; }
  mork_bool  RunGoodTag() { return ( mRun_Tag == morkRun_kTag ); }
#endif /* morkRun_USE_TAG_SLOT */
  
public: // conversion back and forth to inline block following run instance:
  void* RunAsBlock() { return (((mork_u1*) this) + sizeof(morkRun)); }
  
  static morkRun* BlockAsRun(void* ioBlock)
  { return (morkRun*) (((mork_u1*) ioBlock) - sizeof(morkRun)); }

public: // typing & errors
  static void BadRunTagError(morkEnv* ev);
  static void RunSizeAlignError(morkEnv* ev);
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


/*| morkOldRun: more space to record size when run is put into old free list
|*/
class morkOldRun : public morkRun {

protected: // need another size field when mRun_Next is used for linkage:
  mdb_size mOldRun_Size;
  
public: // size getter/setter
  mork_size  OldSize() const { return mOldRun_Size; }
  void       OldSetSize(mork_size inSize) { mOldRun_Size = inSize; }
  
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkHunk_kTag ((mork_u4) 0x68556E4B ) /* ascii 'hUnK' */

/*| morkHunk: structure used by morkZone for heap allocations.
|*/
class morkHunk {

protected: // member variable slots

#ifdef morkHunk_USE_TAG_SLOT
  mork_u4   mHunk_Tag; // force 8 byte alignment after mHunk_Next
#endif /* morkHunk_USE_TAG_SLOT */

  morkHunk* mHunk_Next;
  
  morkRun   mHunk_Run;
  
public: // setters
  void      HunkSetNext(morkHunk* ioNext) { mHunk_Next = ioNext; }
  
public: // getters
  morkHunk* HunkNext() const { return mHunk_Next; }

  morkRun*  HunkRun() { return &mHunk_Run; }
  
public: // maintenance and testing of optional tag magic signature slot:
#ifdef morkHunk_USE_TAG_SLOT
  void       HunkInitTag() { mHunk_Tag = morkHunk_kTag; }
  mork_bool  HunkGoodTag() { return ( mHunk_Tag == morkHunk_kTag ); }
#endif /* morkHunk_USE_TAG_SLOT */

public: // typing & errors
  static void BadHunkTagWarning(morkEnv* ev);
  
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*| kNewHunkSize: the default size for a hunk, assuming we must allocate
**| a new one whenever the free hunk list does not already have.  Note this
**| number should not be changed without also considering suitable changes
**| in the related kMaxHunkWaste and kMinHunkSize constants. 
|*/
#define morkZone_kNewHunkSize ((mork_size) (64 * 1024)) /* 64K per hunk */

/*| kMaxFreeVolume: some number of bytes of free space in the free hunk list
**| over which we no longer want to add more free hunks to the list, for fear
**| of accumulating too much unused, fragmented free space.  This should be a
**| small multiple of kNewHunkSize, say about two to four times as great, to
**| allow for no more free hunk space than fits in a handful of new hunks.
**| This strategy will let us usefully accumulate "some" free space in the
**| free hunk list, but without accumulating "too much" free space that way.
|*/
#define morkZone_kMaxFreeVolume (morkZone_kNewHunkSize * 3)

/*| kMaxHunkWaste: if a current request is larger than this, and we cannot
**| satisfy the request with the current hunk, then we just allocate the
**| block from the heap without changing the current hunk.  Basically this
**| number represents the largest amount of memory we are willing to waste,
**| since a block request barely less than this can cause the current hunk
**| to be retired (with any unused space wasted) as well get a new hunk.
|*/
#define morkZone_kMaxHunkWaste ((mork_size) 4096) /* 1/16 kNewHunkSize */

/*| kRound*: the algorithm for rounding up allocation sizes for caching
**| in free lists works like the following.  We add kRoundAdd to any size
**| requested, and then bitwise AND with kRoundMask, and this will give us
**| the smallest multiple of kRoundSize that is at least as large as the
**| requested size.  Then if we rightshift this number by kRoundBits, we
**| will have the index into the mZone_FreeRuns array which will hold any
**| cache runs of that size.  So 4 bits of shift gives us a granularity
**| of 16 bytes, so that free lists will hold successive runs that are
**| 16 bytes greater than the next smaller run size.  If we have 256 free
**| lists of nonzero sized runs altogether, then the largest run that can
**| be cached is 4096, or 4K (since 4096 == 16 * 256).  A larger run that
**| gets freed will go in to the free hunk list (or back to the heap).
|*/
#define morkZone_kRoundBits 4 /* bits to round-up size for free lists */
#define morkZone_kRoundSize (1 << morkZone_kRoundBits)
#define morkZone_kRoundAdd ((1 << morkZone_kRoundBits) - 1)
#define morkZone_kRoundMask (~ ((mork_ip) morkZone_kRoundAdd))

#define morkZone_kBuckets 256 /* number of distinct free lists */

/*| kMaxCachedRun: the largest run that will be stored inside a free
**| list of old zapped runs.  A run larger than this cannot be put in
**| a free list, and must be allocated from the heap at need, and put
**| into the free hunk list when discarded.
|*/
#define morkZone_kMaxCachedRun (morkZone_kBuckets * morkZone_kRoundSize)

#define morkDerived_kZone     /*i*/ 0x5A6E /* ascii 'Zn' */

/*| morkZone: a pooled memory allocator like an NSPR arena.  The term 'zone'
**| is roughly synonymous with 'heap'.  I avoid calling this class a "heap"
**| to avoid any confusion with nsIMdbHeap, and I avoid calling this class
**| an arean to avoid confusion with NSPR usage.
|*/
class morkZone : public morkNode, public nsIMdbHeap {

// public: // slots inherited from morkNode (meant to inform only)
  // nsIMdbHeap*       mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs

public: // state is public because the entire Mork system is private

  nsIMdbHeap*  mZone_Heap; // strong ref to heap allocating all space
  
  mork_size    mZone_HeapVolume;  // total bytes allocated from heap
  mork_size    mZone_BlockVolume; // total bytes in all zone blocks
  mork_size    mZone_RunVolume;   // total bytes in all zone runs
  mork_size    mZone_ChipVolume;  // total bytes in all zone chips
  
  mork_size    mZone_FreeOldRunVolume; // total bytes in all used hunks
  
  mork_count   mZone_HunkCount;        // total number of used hunks
  mork_count   mZone_FreeOldRunCount;  // total free old runs

  morkHunk*    mZone_HunkList;       // linked list of all used hunks  
  morkRun*     mZone_FreeOldRunList; // linked list of free old runs
  
  // note mZone_At is a byte pointer for single byte address arithmetic:
  mork_u1*     mZone_At;     // current position in most recent hunk
  mork_size    mZone_AtSize; // number of bytes remaining in this hunk
  
  // kBuckets+1 so indexes zero through kBuckets are all okay to use:
  
  morkRun*     mZone_FreeRuns[ morkZone_kBuckets + 1 ];
  // Each piece of memory stored in list mZone_FreeRuns[ i ] has an
  // allocation size equal to sizeof(morkRun) + (i * kRoundSize), so
  // that callers can be given a piece of memory with (i * kRoundSize)
  // bytes of writeable space while reserving the first sizeof(morkRun)
  // bytes to keep track of size information for later re-use.  Note
  // that mZone_FreeRuns[ 0 ] is unused because no run will be zero
  // bytes in size (and morkZone plans to complain about zero sizes). 

protected: // zone utilities
  
  mork_size zone_grow_at(morkEnv* ev, mork_size inNeededSize);

  void*     zone_new_chip(morkEnv* ev, mdb_size inSize); // alloc  
  morkHunk* zone_new_hunk(morkEnv* ev, mdb_size inRunSize); // alloc  

// { ===== begin nsIMdbHeap methods =====
public:
  virtual mdb_err Alloc(nsIMdbEnv* ev, // allocate a piece of memory
    mdb_size inSize,   // requested size of new memory block 
    void** outBlock);  // memory block of inSize bytes, or nil
    
  virtual mdb_err Free(nsIMdbEnv* ev, // free block allocated earlier by Alloc()
    void* inBlock);
    
  virtual mdb_err HeapAddStrongRef(nsIMdbEnv* ev); // does nothing
  virtual mdb_err HeapCutStrongRef(nsIMdbEnv* ev); // does nothing
// } ===== end nsIMdbHeap methods =====
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseZone() only if open
  virtual ~morkZone(); // assert that CloseMap() executed earlier
  
public: // morkMap construction & destruction
  morkZone(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioNodeHeap, 
    nsIMdbHeap* ioZoneHeap);
  
  void CloseZone(morkEnv* ev); // called by CloseMorkNode()
  
public: // dynamic type identification
  mork_bool IsZone() const
  { return IsNode() && mNode_Derived == morkDerived_kZone; }
// } ===== end morkNode methods =====

// { ===== begin morkZone methods =====
public: // chips do not know how big they are...
  void* ZoneNewChip(morkEnv* ev, mdb_size inSize); // alloc  
    
public: // ...but runs do indeed know how big they are
  void* ZoneNewRun(morkEnv* ev, mdb_size inSize); // alloc  
  void  ZoneZapRun(morkEnv* ev, void* ioRunBody); // free
  void* ZoneGrowRun(morkEnv* ev, void* ioRunBody, mdb_size inSize); // realloc
    
// } ===== end morkZone methods =====

public: // typing & errors
  static void NonZoneTypeError(morkEnv* ev);
  static void NilZoneHeapError(morkEnv* ev);
  static void BadZoneTagError(morkEnv* ev);
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKZONE_ */
