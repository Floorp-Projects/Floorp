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

// This code is mostly a port to C++ from public domain IronDoc C sources.
// Note many code comments here come verbatim from cut-and-pasted IronDoc.

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif


class morkHashArrays {
public:
  nsIMdbHeap*   mHashArrays_Heap;     // copy of mMap_Heap
  mork_count    mHashArrays_Slots;    // copy of mMap_Slots
  
  mork_u1*      mHashArrays_Keys;     // copy of mMap_Keys
  mork_u1*      mHashArrays_Vals;     // copy of mMap_Vals
  morkAssoc*    mHashArrays_Assocs;   // copy of mMap_Assocs
  mork_change*  mHashArrays_Changes;  // copy of mMap_Changes
  morkAssoc**   mHashArrays_Buckets;  // copy of mMap_Buckets
  morkAssoc*    mHashArrays_FreeList; // copy of mMap_FreeList
  
public:
  void finalize(morkEnv* ev);
};

void morkHashArrays::finalize(morkEnv* ev)
{
  nsIMdbEnv* menv = ev->AsMdbEnv();
  nsIMdbHeap* heap = mHashArrays_Heap;
  
  if ( heap )
  {
    if ( mHashArrays_Keys )
      heap->Free(menv, mHashArrays_Keys);
    if ( mHashArrays_Vals )
      heap->Free(menv, mHashArrays_Vals);
    if ( mHashArrays_Assocs )
      heap->Free(menv, mHashArrays_Assocs);
    if ( mHashArrays_Changes )
      heap->Free(menv, mHashArrays_Changes);
    if ( mHashArrays_Buckets )
      heap->Free(menv, mHashArrays_Buckets);
  }
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkMap::CloseMorkNode(morkEnv* ev) // CloseMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkMap::~morkMap() // assert CloseMap() executed earlier
{
  MORK_ASSERT(mMap_FreeList==0);
  MORK_ASSERT(mMap_Buckets==0);
  MORK_ASSERT(mMap_Keys==0);
  MORK_ASSERT(mMap_Vals==0);
  MORK_ASSERT(mMap_Changes==0);
  MORK_ASSERT(mMap_Assocs==0);
}

/*public non-poly*/ void
morkMap::CloseMap(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      nsIMdbHeap* heap = mMap_Heap;
      if ( heap ) /* need to free the arrays? */
      {
        nsIMdbEnv* menv = ev->AsMdbEnv();
        
        if ( mMap_Keys )
          heap->Free(menv, mMap_Keys);
          
        if ( mMap_Vals )
          heap->Free(menv, mMap_Vals);
          
        if ( mMap_Assocs )
          heap->Free(menv, mMap_Assocs);
          
        if ( mMap_Buckets )
          heap->Free(menv, mMap_Buckets);
          
        if ( mMap_Changes )
          heap->Free(menv, mMap_Changes);
      }
      mMap_Keys = 0;
      mMap_Vals = 0;
      mMap_Buckets = 0;
      mMap_Assocs = 0;
      mMap_Changes = 0;
      mMap_FreeList = 0;
      MORK_MEMSET(&mMap_Form, 0, sizeof(morkMapForm));
      this->MarkShut();
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 

void
morkMap::clear_map(morkEnv* ev, nsIMdbHeap* ioSlotHeap)
{
  mMap_Tag = 0;
  mMap_Seed = 0;
  mMap_Slots = 0;
  mMap_Fill = 0;
  mMap_Keys = 0;
  mMap_Vals = 0;
  mMap_Assocs = 0;
  mMap_Changes = 0;
  mMap_Buckets = 0;
  mMap_FreeList = 0;
  MORK_MEMSET(&mMap_Form, 0, sizeof(morkMapForm));
  
  mMap_Heap = 0;
  if ( ioSlotHeap )
  {
    nsIMdbHeap_SlotStrongHeap(ioSlotHeap, ev, &mMap_Heap);
  }
  else
    ev->NilPointerError();
}

morkMap::morkMap(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
  mork_size inKeySize, mork_size inValSize,
  mork_size inSlots, nsIMdbHeap* ioSlotHeap, mork_bool inHoldChanges)
: morkNode(ev, inUsage, ioHeap)
, mMap_Heap( 0 )
{
  if ( ev->Good() )
  {
    this->clear_map(ev, ioSlotHeap);
    if ( ev->Good() )
    {
      mMap_Form.mMapForm_HoldChanges = inHoldChanges;
      
      mMap_Form.mMapForm_KeySize = inKeySize;
      mMap_Form.mMapForm_ValSize = inValSize;
      mMap_Form.mMapForm_KeyIsIP = ( inKeySize == sizeof(mork_ip) );
      mMap_Form.mMapForm_ValIsIP = ( inValSize == sizeof(mork_ip) );
      
      this->InitMap(ev, inSlots);
      if ( ev->Good() )
        mNode_Derived = morkDerived_kMap;
    }
  }
}

void
morkMap::NewIterOutOfSyncError(morkEnv* ev)
{
  ev->NewError("map iter out of sync");
}

void morkMap::NewBadMapError(morkEnv* ev)
{
  ev->NewError("bad morkMap tag");
  if ( !this )
    ev->NewError("nil morkMap instance");
}

void morkMap::NewSlotsUnderflowWarning(morkEnv* ev)
{
  ev->NewWarning("member count underflow");
}

void morkMap::InitMap(morkEnv* ev, mork_size inSlots)
{
  if ( ev->Good() )
  {
    morkHashArrays old;
    // MORK_MEMCPY(&mMap_Form, &inForm, sizeof(morkMapForm));
    if ( inSlots < 3 ) /* requested capacity absurdly small? */
      inSlots = 3; /* bump it up to a minimum practical level */
    else if ( inSlots > (128 * 1024) ) /* requested slots absurdly big? */
      inSlots = (128 * 1024); /* decrease it to a maximum practical level */
      
    if ( this->new_arrays(ev, &old, inSlots) )
      mMap_Tag = morkMap_kTag;

    MORK_MEMSET(&old, 0, sizeof(morkHashArrays)); // do NOT finalize
  }
}

morkAssoc**
morkMap::find(morkEnv* ev, const void* inKey, mork_u4 inHash) const
{
  mork_u1* keys = mMap_Keys;
  mork_num keySize = this->FormKeySize();
  // morkMap_mEqual equal = this->FormEqual();
  
  morkAssoc** ref = mMap_Buckets + (inHash % mMap_Slots);
  morkAssoc* assoc = *ref;
  while ( assoc ) /* look at another assoc in the bucket? */
  {
    mork_pos i = assoc - mMap_Assocs; /* index of this assoc */
    if ( this->Equal(ev, keys + (i * keySize), inKey) ) /* found? */
      return ref;
      
    ref = &assoc->mAssoc_Next; /* consider next assoc slot in bucket */
    assoc = *ref; /* if this is null, then we are done */
  }
  return 0;
}

/*| get_assoc: read the key and/or value at index inPos
|*/
void
morkMap::get_assoc(void* outKey, void* outVal, mork_pos inPos) const
{
  mork_num valSize = this->FormValSize();
  if ( valSize && outVal ) /* map holds values? caller wants the value? */
  {
    const mork_u1* value = mMap_Vals + (valSize * inPos);
    if ( valSize == sizeof(mork_ip) && this->FormValIsIP() ) /* ip case? */
      *((mork_ip*) outVal) = *((const mork_ip*) value);
    else
      MORK_MEMCPY(outVal, value, valSize);
  }
  if ( outKey ) /* caller wants the key? */
  {
    mork_num keySize = this->FormKeySize();
    const mork_u1* key = mMap_Keys + (keySize * inPos);
    if ( keySize == sizeof(mork_ip) && this->FormKeyIsIP() ) /* ip case? */
      *((mork_ip*) outKey) = *((const mork_ip*) key);
    else
      MORK_MEMCPY(outKey, key, keySize);
  }
}

/*| put_assoc: write the key and/or value at index inPos
|*/
void
morkMap::put_assoc(const void* inKey, const void* inVal, mork_pos inPos) const
{
  mork_num valSize = this->FormValSize();
  if ( valSize && inVal ) /* map holds values? caller sends a value? */
  {
    mork_u1* value = mMap_Vals + (valSize * inPos);
    if ( valSize == sizeof(mork_ip) && this->FormValIsIP() ) /* ip case? */
      *((mork_ip*) value) = *((const mork_ip*) inVal);
    else
      MORK_MEMCPY(value, inVal, valSize);
  }
  if ( inKey ) /* caller sends a key? */
  {
    mork_num keySize = this->FormKeySize();
    mork_u1* key = mMap_Keys + (keySize * inPos);
    if ( keySize == sizeof(mork_ip) && this->FormKeyIsIP() ) /* ip case? */
      *((mork_ip*) key) = *((const mork_ip*) inKey);
    else
      MORK_MEMCPY(key, inKey, keySize);
  }
}

void*
morkMap::clear_alloc(morkEnv* ev, mork_size inSize)
{
  void* p = 0;
  nsIMdbHeap* heap = mMap_Heap;
  if ( heap )
  {
    if ( heap->Alloc(ev->AsMdbEnv(), inSize, (void**) &p) == 0 && p )
    {
      MORK_MEMSET(p, 0, inSize);
      return p;
    }
  }
  else
    ev->NilPointerError();
    
  return (void*) 0;
}

void*
morkMap::alloc(morkEnv* ev, mork_size inSize)
{
  void* p = 0;
  nsIMdbHeap* heap = mMap_Heap;
  if ( heap )
  {
    if ( heap->Alloc(ev->AsMdbEnv(), inSize, (void**) &p) == 0 && p )
      return p;
  }
  else
    ev->NilPointerError();

  return (void*) 0;
}

/*| new_keys: allocate an array of inSlots new keys filled with zero.
|*/
mork_u1*
morkMap::new_keys(morkEnv* ev, mork_num inSlots)
{
  mork_num size = inSlots * this->FormKeySize();
  return (mork_u1*) this->clear_alloc(ev, size);
}

/*| new_values: allocate an array of inSlots new values filled with zero.
**| When values are zero sized, we just return a null pointer.
|*/
mork_u1*
morkMap::new_values(morkEnv* ev, mork_num inSlots)
{
  mork_u1* values = 0;
  mork_num size = inSlots * this->FormValSize();
  if ( size )
    values = (mork_u1*) this->clear_alloc(ev, size);
  return values;
}

mork_change*
morkMap::new_changes(morkEnv* ev, mork_num inSlots)
{
  mork_change* changes = 0;
  mork_num size = inSlots * sizeof(mork_change);
  if ( size && mMap_Form.mMapForm_HoldChanges )
    changes = (mork_change*) this->clear_alloc(ev, size);
  return changes;
}

/*| new_buckets: allocate an array of inSlots new buckets filled with zero.
|*/
morkAssoc**
morkMap::new_buckets(morkEnv* ev, mork_num inSlots)
{
  mork_num size = inSlots * sizeof(morkAssoc*);
  return (morkAssoc**) this->clear_alloc(ev, size);
}

/*| new_assocs: allocate an array of inSlots new assocs, with each assoc
**| linked together in a list with the first array element at the list head
**| and the last element at the list tail. (morkMap::grow() needs this.)
|*/
morkAssoc*
morkMap::new_assocs(morkEnv* ev, mork_num inSlots)
{
  mork_num size = inSlots * sizeof(morkAssoc);
  morkAssoc* assocs = (morkAssoc*) this->alloc(ev, size);
  if ( assocs ) /* able to allocate the array? */
  {
    morkAssoc* a = assocs + (inSlots - 1); /* the last array element */
    a->mAssoc_Next = 0; /* terminate tail element of the list with null */
    while ( --a >= assocs ) /* another assoc to link into the list? */
      a->mAssoc_Next = a + 1; /* each points to the following assoc */
  }
  return assocs;
}

mork_bool
morkMap::new_arrays(morkEnv* ev, morkHashArrays* old, mork_num inSlots)
{
  mork_bool outNew = morkBool_kFalse;
    
  /* see if we can allocate all the new arrays before we go any further: */
  morkAssoc** newBuckets = this->new_buckets(ev, inSlots);
  morkAssoc* newAssocs = this->new_assocs(ev, inSlots);
  mork_u1* newKeys = this->new_keys(ev, inSlots);
  mork_u1* newValues = this->new_values(ev, inSlots);
  mork_change* newChanges = this->new_changes(ev, inSlots);
  
  /* it is okay for newChanges to be null when changes are not held: */
  mork_bool okayChanges = ( newChanges || !this->FormHoldChanges() );
  
  /* it is okay for newValues to be null when values are zero sized: */
  mork_bool okayValues = ( newValues || !this->FormValSize() );
  
  if ( newBuckets && newAssocs && newKeys && okayChanges && okayValues )
  {
    outNew = morkBool_kTrue; /* yes, we created all the arrays we need */

    /* init the old hashArrays with slots from this hash table: */
    old->mHashArrays_Heap = mMap_Heap;
    
    old->mHashArrays_Slots = mMap_Slots;
    old->mHashArrays_Keys = mMap_Keys;
    old->mHashArrays_Vals = mMap_Vals;
    old->mHashArrays_Assocs = mMap_Assocs;
    old->mHashArrays_Buckets = mMap_Buckets;
    old->mHashArrays_Changes = mMap_Changes;
    
    /* now replace all our array slots with the new structures: */
    ++mMap_Seed; /* note the map is now changed */
    mMap_Keys = newKeys;
    mMap_Vals = newValues;
    mMap_Buckets = newBuckets;
    mMap_Assocs = newAssocs;
    mMap_FreeList = newAssocs; /* all are free to start with */
    mMap_Changes = newChanges;
    mMap_Slots = inSlots;
  }
  else /* free the partial set of arrays that were actually allocated */
  {
    nsIMdbEnv* menv = ev->AsMdbEnv();
    nsIMdbHeap* heap = mMap_Heap;
    if ( newBuckets )
      heap->Free(menv, newBuckets);
    if ( newAssocs )
      heap->Free(menv, newAssocs);
    if ( newKeys )
      heap->Free(menv, newKeys);
    if ( newValues )
      heap->Free(menv, newValues);
    if ( newChanges )
      heap->Free(menv, newChanges);
    
    MORK_MEMSET(old, 0, sizeof(morkHashArrays));
  }
  
  return outNew;
}

/*| grow: make the map arrays bigger by 33%.  The old map is completely
**| full, or else we would not have called grow() to get more space.  This
**| means the free list is empty, and also means every old key and value is in
**| use in the old arrays.  So every key and value must be copied to the new
**| arrays, and since they are contiguous in the old arrays, we can efficiently
**| bitwise copy them in bulk from the old arrays to the new arrays, without
**| paying any attention to the structure of the old arrays.
**|
**|| The content of the old bucket and assoc arrays need not be copied because
**| this information is going to be completely rebuilt by rehashing all the
**| keys into their new buckets, given the new larger map capacity.  The new
**| bucket array from new_arrays() is assumed to contain all zeroes, which is
**| necessary to ensure the lists in each bucket stay null terminated as we
**| build the new linked lists.  (Note no old bucket ordering is preserved.)
**|
**|| If the old capacity is N, then in the new arrays the assocs with indexes
**| from zero to N-1 are still allocated and must be rehashed into the map.
**| The new free list contains all the following assocs, starting with the new
**| assoc link at index N.  (We call the links in the link array "assocs"
**| because allocating a link is the same as allocating the key/value pair
**| with the same index as the link.)
**|
**|| The new free list is initialized simply by pointing at the first new link
**| in the assoc array after the size of the old assoc array.  This assumes
**| that FeHashTable_new_arrays() has already linked all the new assocs into a
**| list with the first at the head of the list and the last at the tail of the
**| list.  So by making the free list point to the first of the new uncopied
**| assocs, the list is already well-formed.
|*/
mork_bool morkMap::grow(morkEnv* ev)
{
  if ( mMap_Heap ) /* can we grow the map? */
  {
    mork_num newSlots = (mMap_Slots * 2); /* +100% */
    morkHashArrays old; /* a place to temporarily hold all the old arrays */
    if ( this->new_arrays(ev, &old, newSlots) ) /* have more? */
    {
      // morkMap_mHash hash = this->FormHash(); /* for terse loop use */
      
      /* figure out the bulk volume sizes of old keys and values to move: */
      mork_num oldSlots = old.mHashArrays_Slots; /* number of old assocs */
      mork_num keyBulk = oldSlots * this->FormKeySize(); /* key volume */
      mork_num valBulk = oldSlots * this->FormValSize(); /* values */
      
      /* convenient variables for new arrays that need to be rehashed: */
      morkAssoc** newBuckets = mMap_Buckets; /* new all zeroes */
      morkAssoc* newAssocs = mMap_Assocs; /* hash into buckets */
      morkAssoc* newFreeList = newAssocs + oldSlots; /* new room is free */
      mork_u1* key = mMap_Keys; /* the first key to rehash */
      --newAssocs; /* back up one before preincrement used in while loop */
      
      /* move all old keys and values to the new arrays: */
      MORK_MEMCPY(mMap_Keys, old.mHashArrays_Keys, keyBulk);
      if ( valBulk ) /* are values nonzero sized? */
        MORK_MEMCPY(mMap_Vals, old.mHashArrays_Vals, valBulk);
        
      mMap_FreeList = newFreeList; /* remaining assocs are free */
      
      while ( ++newAssocs < newFreeList ) /* rehash another old assoc? */
      {
        morkAssoc** top = newBuckets + (this->Hash(ev, key) % newSlots);
        key += this->FormKeySize(); /* the next key to rehash */
        newAssocs->mAssoc_Next = *top; /* link to prev bucket top */
        *top = newAssocs; /* push assoc on top of bucket */
      }
      ++mMap_Seed; /* note the map has changed */
      old.finalize(ev); /* remember to free the old arrays */
    }
  }
  else ev->OutOfMemoryError();
  
  return ev->Good();
}


mork_bool
morkMap::Put(morkEnv* ev, const void* inKey, const void* inVal,
  void* outKey, void* outVal, mork_change** outChange)
{
  mork_bool outPut = morkBool_kFalse;
  
  if ( this->GoodMap() ) /* looks good? */
  {
    mork_u4 hash = this->Hash(ev, inKey);
    morkAssoc** ref = this->find(ev, inKey, hash);
    if ( ref ) /* assoc was found? reuse an existing assoc slot? */
    {
      outPut = morkBool_kTrue; /* inKey was indeed already inside the map */
    }
    else /* assoc not found -- need to allocate a new assoc slot */
    {
      morkAssoc* assoc = this->pop_free_assoc();
      if ( !assoc ) /* no slots remaining in free list? must grow map? */
      {
        if ( this->grow(ev) ) /* successfully made map larger? */
          assoc = this->pop_free_assoc();
      }
      if ( assoc ) /* allocated new assoc without error? */
      {
        ref = mMap_Buckets + (hash % mMap_Slots);
        assoc->mAssoc_Next = *ref; /* link to prev bucket top */
        *ref = assoc; /* push assoc on top of bucket */
          
        ++mMap_Fill; /* one more member in the collection */
        ++mMap_Seed; /* note the map has changed */
      }
    }
    if ( ref ) /* did not have an error during possible growth? */
    {
      mork_pos i = (*ref) - mMap_Assocs; /* index of assoc */
      if ( outPut && (outKey || outVal) ) /* copy old before cobbering? */
        this->get_assoc(outKey, outVal, i);

      this->put_assoc(inKey, inVal, i);
      ++mMap_Seed; /* note the map has changed */
      
      if ( outChange )
      {
        if ( mMap_Changes )
          *outChange = mMap_Changes + i;
        else
          *outChange = this->FormDummyChange();
      }
    }
  }
  else this->NewBadMapError(ev);
  
  return outPut;
}

mork_num
morkMap::CutAll(morkEnv* ev)
{
  mork_num outCutAll = 0;
  
  if ( this->GoodMap() ) /* map looks good? */
  {
    mork_num slots = mMap_Slots;
    morkAssoc* before = mMap_Assocs - 1; /* before first member */
    morkAssoc* assoc = before + slots; /* the very last member */

    ++mMap_Seed; /* note the map is changed */

    /* make the assoc array a linked list headed by first & tailed by last: */
    assoc->mAssoc_Next = 0; /* null terminate the new free list */
    while ( --assoc > before ) /* another assoc to link into the list? */
      assoc->mAssoc_Next = assoc + 1;
    mMap_FreeList = mMap_Assocs; /* all are free */

    outCutAll = mMap_Fill; /* we'll cut all of them of course */

    mMap_Fill = 0; /* the map is completely empty */
  }
  else this->NewBadMapError(ev);
  
  return outCutAll;
}

mork_bool
morkMap::Cut(morkEnv* ev, const void* inKey,
  void* outKey, void* outVal, mork_change** outChange)
{
  mork_bool outCut = morkBool_kFalse;
  
  if ( this->GoodMap() ) /* looks good? */
  {
    mork_u4 hash = this->Hash(ev, inKey);
    morkAssoc** ref = this->find(ev, inKey, hash);
    if ( ref ) /* found an assoc for key? */
    {
      outCut = morkBool_kTrue;
      morkAssoc* assoc = *ref;
      mork_pos i = assoc - mMap_Assocs; /* index of assoc */
      if ( outKey || outVal )
        this->get_assoc(outKey, outVal, i);

      *ref = assoc->mAssoc_Next; /* unlink the found assoc */
      this->push_free_assoc(assoc); /* and put it in free list */

      if ( outChange )
      {
        if ( mMap_Changes )
          *outChange = mMap_Changes + i;
        else
          *outChange = this->FormDummyChange();
      }
      
      ++mMap_Seed; /* note the map has changed */
      if ( mMap_Fill ) /* the count shows nonzero members? */
        --mMap_Fill; /* one less member in the collection */
      else
        this->NewSlotsUnderflowWarning(ev);
    }
  }
  else this->NewBadMapError(ev);
  
  return outCut;
}

mork_bool
morkMap::Get(morkEnv* ev, const void* inKey,
  void* outKey, void* outVal, mork_change** outChange)
{
  mork_bool outGet = morkBool_kFalse;
  
  if ( this->GoodMap() ) /* looks good? */
  {
    mork_u4 hash = this->Hash(ev, inKey);
    morkAssoc** ref = this->find(ev, inKey, hash);
    if ( ref ) /* found an assoc for inKey? */
    {
      mork_pos i = (*ref) - mMap_Assocs; /* index of assoc */
      outGet = morkBool_kTrue;
      this->get_assoc(outKey, outVal, i);
      if ( outChange )
      {
        if ( mMap_Changes )
          *outChange = mMap_Changes + i;
        else
          *outChange = this->FormDummyChange();
      }
    }
  }
  else this->NewBadMapError(ev);
  
  return outGet;
}


morkMapIter::morkMapIter( )
: mMapIter_Map( 0 )
, mMapIter_Seed( 0 )
  
, mMapIter_Bucket( 0 )
, mMapIter_AssocRef( 0 )
, mMapIter_Assoc( 0 )
, mMapIter_Next( 0 )
{
}

void
morkMapIter::InitMapIter(morkEnv* ev, morkMap* ioMap)
{
  mMapIter_Map = 0;
  mMapIter_Seed = 0;

  mMapIter_Bucket = 0;
  mMapIter_AssocRef = 0;
  mMapIter_Assoc = 0;
  mMapIter_Next = 0;

  if ( ioMap )
  {
    if ( ioMap->GoodMap() )
    {
      mMapIter_Map = ioMap;
      mMapIter_Seed = ioMap->mMap_Seed;
    }
    else ioMap->NewBadMapError(ev);
  }
  else ev->NilPointerError();
}

morkMapIter::morkMapIter(morkEnv* ev, morkMap* ioMap)
: mMapIter_Map( 0 )
, mMapIter_Seed( 0 )
  
, mMapIter_Bucket( 0 )
, mMapIter_AssocRef( 0 )
, mMapIter_Assoc( 0 )
, mMapIter_Next( 0 )
{
  if ( ioMap )
  {
    if ( ioMap->GoodMap() )
    {
      mMapIter_Map = ioMap;
      mMapIter_Seed = ioMap->mMap_Seed;
    }
    else ioMap->NewBadMapError(ev);
  }
  else ev->NilPointerError();
}

void
morkMapIter::CloseMapIter(morkEnv* ev)
{
  MORK_USED_1(ev);
  mMapIter_Map = 0;
  mMapIter_Seed = 0;
  
  mMapIter_Bucket = 0;
  mMapIter_AssocRef = 0;
  mMapIter_Assoc = 0;
  mMapIter_Next = 0;
}

mork_change*
morkMapIter::First(morkEnv* ev, void* outKey, void* outVal)
{
  mork_change* outFirst = 0;
  
  morkMap* map = mMapIter_Map;
  
  if ( map && map->GoodMap() ) /* map looks good? */
  {
    morkAssoc** bucket = map->mMap_Buckets;
    morkAssoc** end = bucket + map->mMap_Slots; /* one past last */
 
    mMapIter_Seed = map->mMap_Seed; /* sync the seeds */
   
    while ( bucket < end ) /* another bucket in which to look for assocs? */
    {
      morkAssoc* assoc = *bucket++;
      if ( assoc ) /* found the first map assoc in use? */
      {
        mork_pos i = assoc - map->mMap_Assocs;
        mork_change* c = map->mMap_Changes;
        outFirst = ( c )? (c + i) : map->FormDummyChange();
        
        mMapIter_Assoc = assoc; /* current assoc in iteration */
        mMapIter_Next = assoc->mAssoc_Next; /* more in bucket */
        mMapIter_Bucket = --bucket; /* bucket for this assoc */
        mMapIter_AssocRef = bucket; /* slot referencing assoc */
        
        map->get_assoc(outKey, outVal, i);
          
        break; /* end while loop */
      }
    }
  }
  else map->NewBadMapError(ev);

  return outFirst;
}

mork_change*
morkMapIter::Next(morkEnv* ev, void* outKey, void* outVal)
{
  mork_change* outNext = 0;
  
  morkMap* map = mMapIter_Map;
  
  if ( map && map->GoodMap() ) /* map looks good? */
  {
    if ( mMapIter_Seed == map->mMap_Seed ) /* in sync? */
    {
      morkAssoc* here = mMapIter_Assoc; /* current assoc */
      if ( here ) /* iteration is not yet concluded? */
      {
        morkAssoc* next = mMapIter_Next;
        morkAssoc* assoc = next; /* default new mMapIter_Assoc */  
        if ( next ) /* there are more assocs in the same bucket after Here? */
        {
          morkAssoc** ref = mMapIter_AssocRef;
          
          /* (*HereRef) equals Here, except when Here has been cut, after
          ** which (*HereRef) always equals Next.  So if (*HereRef) is not
          ** equal to Next, then HereRef still needs to be updated to point
          ** somewhere else other than Here.  Otherwise it is fine.
          */
          if ( *ref != next ) /* Here was not cut? must update HereRef? */
            mMapIter_AssocRef = &here->mAssoc_Next;
            
          mMapIter_Next = next->mAssoc_Next;
        }
        else /* look for the next assoc in the next nonempty bucket */
        {
          morkAssoc** bucket = map->mMap_Buckets;
          morkAssoc** end = bucket + map->mMap_Slots; /* beyond */
          mMapIter_Assoc = 0; /* default to no more assocs */
          bucket = mMapIter_Bucket; /* last exhausted bucket */
          mMapIter_Assoc = 0; /* default to iteration ended */
          
          while ( ++bucket < end ) /* another bucket to search for assocs? */
          {
            assoc = *bucket;
            if ( assoc ) /* found another map assoc in use? */
            {
              mMapIter_Bucket = bucket;
              mMapIter_AssocRef = bucket; /* ref to assoc */
              mMapIter_Next = assoc->mAssoc_Next; /* more */
              
              break; /* end while loop */
            }
          }
        }
        if ( assoc ) /* did we find another assoc in the iteration? */
        {
          mMapIter_Assoc = assoc; /* current assoc */
          mork_pos i = assoc - map->mMap_Assocs;
          mork_change* c = map->mMap_Changes;
          outNext = ( c )? (c + i) : map->FormDummyChange();
        
          map->get_assoc( outKey, outVal, i);
        }
      }
    }
    else map->NewIterOutOfSyncError(ev);
  }
  else map->NewBadMapError(ev);
  
  return outNext;
}

mork_change*
morkMapIter::Here(morkEnv* ev, void* outKey, void* outVal)
{
  mork_change* outHere = 0;
  
  morkMap* map = mMapIter_Map;
  
  if ( map && map->GoodMap() ) /* map looks good? */
  {
    if ( mMapIter_Seed == map->mMap_Seed ) /* in sync? */
    {
      morkAssoc* here = mMapIter_Assoc; /* current assoc */
      if ( here ) /* iteration is not yet concluded? */
      {
        mork_pos i = here - map->mMap_Assocs;
        mork_change* c = map->mMap_Changes;
        outHere = ( c )? (c + i) : map->FormDummyChange();
          
        map->get_assoc(outKey, outVal, i);
      }
    }
    else map->NewIterOutOfSyncError(ev);
  }
  else map->NewBadMapError(ev);
  
  return outHere;
}

mork_change*
morkMapIter::CutHere(morkEnv* ev, void* outKey, void* outVal)
{
  mork_change* outCutHere = 0;
  morkMap* map = mMapIter_Map;
  
  if ( map && map->GoodMap() ) /* map looks good? */
  {
    if ( mMapIter_Seed == map->mMap_Seed ) /* in sync? */
    {
      morkAssoc* here = mMapIter_Assoc; /* current assoc */
      if ( here ) /* iteration is not yet concluded? */
      {
        morkAssoc** ref = mMapIter_AssocRef;
        if ( *ref != mMapIter_Next ) /* not already cut? */
        {
          mork_pos i = here - map->mMap_Assocs;
          mork_change* c = map->mMap_Changes;
          outCutHere = ( c )? (c + i) : map->FormDummyChange();
          if ( outKey || outVal )
            map->get_assoc(outKey, outVal, i);
            
          map->push_free_assoc(here); /* add to free list */
          *ref = mMapIter_Next; /* unlink here from bucket list */
          
          /* note the map has changed, but we are still in sync: */
          mMapIter_Seed = ++map->mMap_Seed; /* sync */
          
          if ( map->mMap_Fill ) /* still has nonzero value? */
            --map->mMap_Fill; /* one less member in the collection */
          else
            map->NewSlotsUnderflowWarning(ev);
        }
      }
    }
    else map->NewIterOutOfSyncError(ev);
  }
  else map->NewBadMapError(ev);
  
  return outCutHere;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
