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

// This code is a port to NS Mork from public domain Mithril C++ sources.
// Note many code comments here come verbatim from cut-and-pasted Mithril.
// In many places, code is identical; Mithril versions stay public domain.
// Changes in porting are mainly class type and scalar type name changes.

#include "nscore.h"

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKPROBEMAP_
#include "morkProbeMap.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

/*============================================================================*/
/* morkMapScratch */

void morkMapScratch::halt_map_scratch(morkEnv* ev)
{
  nsIMdbHeap* heap = sMapScratch_Heap;
  
  if ( heap )
  {
    if ( sMapScratch_Keys )
      heap->Free(ev->AsMdbEnv(), sMapScratch_Keys);
    if ( sMapScratch_Vals )
      heap->Free(ev->AsMdbEnv(), sMapScratch_Vals);
  }
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


/*============================================================================*/
/* morkProbeMap */

void morkProbeMap::ProbeMapBadTagError(morkEnv* ev) const
{
  ev->NewError("bad sProbeMap_Tag");
  if ( !this )
    ev->NewError("nil morkProbeMap");
}

void morkProbeMap::WrapWithNoVoidSlotError(morkEnv* ev) const
{
  ev->NewError("wrap without void morkProbeMap slot");
}

void morkProbeMap::GrowFailsMaxFillError(morkEnv* ev) const
{
  ev->NewError("grow fails morkEnv > sMap_Fill");
}

void morkProbeMap::MapKeyIsNotIPError(morkEnv* ev) const
{
  ev->NewError("not sMap_KeyIsIP");
}

void morkProbeMap::MapValIsNotIPError(morkEnv* ev) const
{
  ev->NewError("not sMap_ValIsIP");
}

void morkProbeMap::rehash_old_map(morkEnv* ev, morkMapScratch* ioScratch)
{
  mork_size keySize = sMap_KeySize; // size of every key bucket
  mork_size valSize = sMap_ValSize; // size of every associated value
  
  mork_count slots = sMap_Slots; // number of new buckets
  mork_u1* keys = sMap_Keys; // destination for rehashed keys
  mork_u1* vals = sMap_Vals; // destination for any copied values
  
  mork_bool keyIsIP = ( keys && keySize == sizeof(mork_ip) && sMap_KeyIsIP );
  mork_bool valIsIP = ( vals && valSize == sizeof(mork_ip) && sMap_ValIsIP );

  mork_count oldSlots = ioScratch->sMapScratch_Slots; // sMap_Slots
  mork_u1* oldKeys = ioScratch->sMapScratch_Keys; // sMap_Keys
  mork_u1* oldVals = ioScratch->sMapScratch_Vals; // sMap_Vals
  mork_u1* end = oldKeys + (keySize * oldSlots); // one byte past last key
  
  mork_fill fill = 0; // let's count the actual fill for a double check
  
  while ( oldKeys < end ) // another old key bucket to rehash if non-nil?
  {
    if ( !this->ProbeMapIsKeyNil(ev, oldKeys) ) // need to rehash?
    {
      ++fill; // this had better match sMap_Fill when we are all done
      mork_u4 hash = this->ProbeMapHashMapKey(ev, oldKeys);

      mork_pos i = hash % slots;   // target hash bucket
      mork_pos startPos = i;       // remember start to detect
      
      mork_u1* k = keys + (i * keySize);
      while ( !this->ProbeMapIsKeyNil(ev, k) )
      {
        if ( ++i >= (mork_pos)slots ) // advanced past end? need to wrap around now?
          i = 0; // wrap around to first slot in map's hash table
          
        if ( i == startPos ) // no void slots were found anywhere in map?
        {
          this->WrapWithNoVoidSlotError(ev); // should never happen
          return; // this is bad, and we can't go on with the rehash
        }
        k = keys + (i * keySize);
      }
      if ( keyIsIP ) // int special case?
        *((mork_ip*) k) = *((const mork_ip*) oldKeys); // fast bitwise copy
      else
        MORK_MEMCPY(k, oldKeys, keySize); // slow bitwise copy

      if ( oldVals ) // need to copy values as well?
      {
        mork_size valOffset = (i * valSize);
        mork_u1* v = vals + valOffset;
        mork_u1* ov = oldVals + valOffset;
        if ( valIsIP ) // int special case?
          *((mork_ip*) v) = *((const mork_ip*) ov); // fast bitwise copy
        else
          MORK_MEMCPY(v, ov, valSize); // slow bitwise copy
      }
    }
    oldKeys += keySize; // advance to next key bucket in old map
  }
  if ( fill != sMap_Fill ) // is the recorded value of sMap_Fill wrong?
  {
    ev->NewWarning("fill != sMap_Fill");
    sMap_Fill = fill;
  }
}

mork_bool morkProbeMap::grow_probe_map(morkEnv* ev)
{
  if ( sMap_Heap ) // can we grow the map?
  {
    mork_num newSlots = ((sMap_Slots * 4) / 3) + 1; // +25%
    morkMapScratch old; // a place to temporarily hold all the old arrays
    if ( this->new_slots(ev, &old, newSlots) ) // have more?
    {      
      ++sMap_Seed; // note the map has changed
      this->rehash_old_map(ev, &old);
      
      if ( ev->Good() ) 
      {
        mork_count slots = sMap_Slots;
        mork_num emptyReserve = (slots / 7) + 1; // keep this many empty
        mork_fill maxFill = slots - emptyReserve; // new max occupancy
        if ( maxFill > sMap_Fill ) // new max is bigger than old occupancy?
          sProbeMap_MaxFill = maxFill; // we can install new max for fill
        else
          this->GrowFailsMaxFillError(ev); // we have invariant failure
      }
      
      if ( ev->Bad() ) // rehash failed? need to revert map to last state?
        this->revert_map(ev, &old); // swap the vectors back again

      old.halt_map_scratch(ev); // remember to free the old arrays
    }
  }
  else ev->OutOfMemoryError();
  
  return ev->Good();
}

void morkProbeMap::revert_map(morkEnv* ev, morkMapScratch* ioScratch)
{
  mork_count tempSlots = ioScratch->sMapScratch_Slots; // sMap_Slots  
  mork_u1* tempKeys = ioScratch->sMapScratch_Keys;     // sMap_Keys
  mork_u1* tempVals = ioScratch->sMapScratch_Vals;     // sMap_Vals
  
  ioScratch->sMapScratch_Slots = sMap_Slots;
  ioScratch->sMapScratch_Keys = sMap_Keys;
  ioScratch->sMapScratch_Vals = sMap_Vals;
  
  sMap_Slots = tempSlots;
  sMap_Keys = tempKeys;
  sMap_Vals = tempVals;
}

void morkProbeMap::put_probe_kv(morkEnv* ev,
  const void* inAppKey, const void* inAppVal, mork_pos inPos)
{
  mork_u1* mapVal = 0;
  mork_u1* mapKey = 0;

  mork_num valSize = sMap_ValSize;
  if ( valSize && inAppVal ) // map holds values? caller sends value?
  {
    mork_u1* val = sMap_Vals + (valSize * inPos);
    if ( valSize == sizeof(mork_ip) && sMap_ValIsIP ) // int special case? 
      *((mork_ip*) val) = *((const mork_ip*) inAppVal);
    else
      mapVal = val; // show possible need to call ProbeMapPushIn()
  }
  if ( inAppKey ) // caller sends the key? 
  {
    mork_num keySize = sMap_KeySize;
    mork_u1* key = sMap_Keys + (keySize * inPos);
    if ( keySize == sizeof(mork_ip) && sMap_KeyIsIP ) // int special case? 
      *((mork_ip*) key) = *((const mork_ip*) inAppKey);
    else
      mapKey = key; // show possible need to call ProbeMapPushIn()
  }
  else
    ev->NilPointerError();

  if ( (  inAppVal && mapVal ) || ( inAppKey && mapKey ) )
    this->ProbeMapPushIn(ev, inAppKey, inAppVal, mapKey, mapVal);

  if ( sMap_Fill > sProbeMap_MaxFill )
    this->grow_probe_map(ev);
}

void morkProbeMap::get_probe_kv(morkEnv* ev,
  void* outAppKey, void* outAppVal, mork_pos inPos) const
{
  const mork_u1* mapVal = 0;
  const mork_u1* mapKey = 0;

  mork_num valSize = sMap_ValSize;
  if ( valSize && outAppVal ) // map holds values? caller wants value?
  {
    const mork_u1* val = sMap_Vals + (valSize * inPos);
    if ( valSize == sizeof(mork_ip) && sMap_ValIsIP ) // int special case? 
      *((mork_ip*) outAppVal) = *((const mork_ip*) val);
    else
      mapVal = val; // show possible need to call ProbeMapPullOut()
  }
  if ( outAppKey ) // caller wants the key? 
  {
    mork_num keySize = sMap_KeySize;
    const mork_u1* key = sMap_Keys + (keySize * inPos);
    if ( keySize == sizeof(mork_ip) && sMap_KeyIsIP ) // int special case? 
      *((mork_ip*) outAppKey) = *((const mork_ip*) key);
    else
      mapKey = key; // show possible need to call ProbeMapPullOut()
  }
  if ( ( outAppVal && mapVal ) || ( outAppKey && mapKey ) )
    this->ProbeMapPullOut(ev, mapKey, mapVal, outAppKey, outAppVal);
}

mork_test
morkProbeMap::find_key_pos(morkEnv* ev, const void* inAppKey,
  mork_u4 inHash, mork_pos* outPos) const
{
  mork_u1* k = sMap_Keys;        // array of keys, each of size sMap_KeySize
  mork_num size = sMap_KeySize;  // number of bytes in each key
  mork_count slots = sMap_Slots; // total number of key buckets
  mork_pos i = inHash % slots;   // target hash bucket
  mork_pos startPos = i;         // remember start to detect
  
  mork_test outTest = this->MapTest(ev, k + (i * size), inAppKey);
  while ( outTest == morkTest_kMiss )
  {
    if ( ++i >= (mork_pos)slots ) // advancing goes beyond end? need to wrap around now?
      i = 0; // wrap around to first slot in map's hash table
      
    if ( i == startPos ) // no void slots were found anywhere in map?
    {
      this->WrapWithNoVoidSlotError(ev); // should never happen
      break; // end loop on kMiss; note caller expects either kVoid or kHit
    }
    outTest = this->MapTest(ev, k + (i * size), inAppKey);
  }
  *outPos = i;
  
  return outTest;
}
 
void morkProbeMap::probe_map_lazy_init(morkEnv* ev)
{
  if ( this->need_lazy_init() && sMap_Fill == 0 ) // pending lazy action?
  {
    // The constructor cannot successfully call virtual ProbeMapClearKey(),
    // so we lazily do so now, when we add the first member to the map.
    
    mork_u1* keys = sMap_Keys;
    if ( keys ) // okay to call lazy virtual clear method on new map keys?
    {
      if ( sProbeMap_ZeroIsClearKey ) // zero is good enough to clear keys?
      {
        mork_num keyVolume = sMap_Slots * sMap_KeySize;
        if ( keyVolume )
          MORK_MEMSET(keys, 0, keyVolume);
      }
      else
        this->ProbeMapClearKey(ev, keys, sMap_Slots);
    }
    else
      this->MapNilKeysError(ev);
  }
  sProbeMap_LazyClearOnAdd = 0; // don't do this ever again
}

mork_bool
morkProbeMap::MapAtPut(morkEnv* ev,
  const void* inAppKey, const void* inAppVal,
  void* outAppKey, void* outAppVal)
{
  mork_bool outPut = morkBool_kFalse;
  
  if ( this->GoodProbeMap() ) /* looks good? */
  {
    if ( this->need_lazy_init() && sMap_Fill == 0 ) // pending lazy action?
      this->probe_map_lazy_init(ev);
          
    if ( ev->Good() )
    {
      mork_pos slotPos = 0;
      mork_u4 hash = this->MapHash(ev, inAppKey);
      mork_test test = this->find_key_pos(ev, inAppKey, hash, &slotPos);
      outPut = ( test == morkTest_kHit );

      if ( outPut ) // replacing an old assoc? no change in member count?
      {
        if ( outAppKey || outAppVal ) /* copy old before cobber? */
          this->get_probe_kv(ev, outAppKey, outAppVal, slotPos);
      }
      else // adding a new assoc increases membership by one
      {
        ++sMap_Fill; /* one more member in the collection */
      }
      
      if ( test != morkTest_kMiss ) /* found slot to hold new assoc? */
      {
        ++sMap_Seed; /* note the map has changed */
        this->put_probe_kv(ev, inAppKey, inAppVal, slotPos);
      }
    }
  }
  else this->ProbeMapBadTagError(ev);
  
  return outPut;
}
    
mork_bool
morkProbeMap::MapAt(morkEnv* ev, const void* inAppKey,
    void* outAppKey, void* outAppVal)
{
  if ( this->GoodProbeMap() ) /* looks good? */
  {
    if ( this->need_lazy_init() && sMap_Fill == 0 ) // pending lazy action?
      this->probe_map_lazy_init(ev);
          
    mork_pos slotPos = 0;
    mork_u4 hash = this->MapHash(ev, inAppKey);
    mork_test test = this->find_key_pos(ev, inAppKey, hash, &slotPos);
    if ( test == morkTest_kHit ) /* found an assoc pair for inAppKey? */
    {
      this->get_probe_kv(ev, outAppKey, outAppVal, slotPos);
      return morkBool_kTrue;
    }
  }
  else this->ProbeMapBadTagError(ev);
  
  return morkBool_kFalse;
}
    
mork_num
morkProbeMap::MapCutAll(morkEnv* ev)
{
  mork_num outCutAll = 0;
  
  if ( this->GoodProbeMap() ) /* looks good? */
  {
    outCutAll = sMap_Fill; /* number of members cut, which is all of them */
    
    if ( sMap_Keys && !sProbeMap_ZeroIsClearKey )
      this->ProbeMapClearKey(ev, sMap_Keys, sMap_Slots);

    sMap_Fill = 0; /* map now has no members */
  }
  else this->ProbeMapBadTagError(ev);
  
  return outCutAll;
}
    
// { ===== node interface =====

/*virtual*/
morkProbeMap::~morkProbeMap() // assert NodeStop() finished earlier
{
  MORK_ASSERT(sMap_Keys==0);
  MORK_ASSERT(sProbeMap_Tag==0);
}

/*public virtual*/ void
morkProbeMap::CloseMorkNode(morkEnv* ev) // CloseMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseProbeMap(ev);
    this->MarkShut();
  }
}

void morkProbeMap::CloseProbeMap(morkEnv* ev)
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      nsIMdbHeap* heap = sMap_Heap;
      if ( heap ) // able to free map arrays?
      {
        void* block = sMap_Keys;
        if ( block )
        {
          heap->Free(ev->AsMdbEnv(), block);
          sMap_Keys = 0;
        }
          
        block = sMap_Vals;
        if ( block )
        {
          heap->Free(ev->AsMdbEnv(), block);
          sMap_Vals = 0;
        }
      }
      sMap_Keys = 0;
      sMap_Vals = 0;
      
      this->CloseNode(ev);
      sProbeMap_Tag = 0;
      sProbeMap_MaxFill = 0;
      
      this->MarkShut();
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

void*
morkProbeMap::clear_alloc(morkEnv* ev, mork_size inSize)
{
  void* p = 0;
  nsIMdbHeap* heap = sMap_Heap;
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

/*| map_new_keys: allocate an array of inSlots new keys filled with zero.
**| (cf IronDoc's FeHashTable_new_keys())
|*/
mork_u1*
morkProbeMap::map_new_keys(morkEnv* ev, mork_num inSlots)
{
  mork_num size = inSlots * sMap_KeySize;
  return (mork_u1*) this->clear_alloc(ev, size);
}

/*| map_new_vals: allocate an array of inSlots new values filled with zero.
**| When values are zero sized, we just return a null pointer.
**|
**| (cf IronDoc's FeHashTable_new_values())
|*/
mork_u1*
morkProbeMap::map_new_vals(morkEnv* ev, mork_num inSlots)
{
  mork_u1* values = 0;
  mork_num size = inSlots * sMap_ValSize;
  if ( size )
    values = (mork_u1*) this->clear_alloc(ev, size);
  return values;
}


void morkProbeMap::MapSeedOutOfSyncError(morkEnv* ev)
{
  ev->NewError("sMap_Seed out of sync");
}

void morkProbeMap::MapFillUnderflowWarning(morkEnv* ev)
{
  ev->NewWarning("sMap_Fill underflow");
}

void morkProbeMap::MapNilKeysError(morkEnv* ev)
{
  ev->NewError("nil sMap_Keys");
}

void morkProbeMap::MapZeroKeySizeError(morkEnv* ev)
{
  ev->NewError("zero sMap_KeySize");
}

/*static*/
void morkProbeMap::ProbeMapCutError(morkEnv* ev)
{
  ev->NewError("morkProbeMap cannot cut");
}


void morkProbeMap::init_probe_map(morkEnv* ev, mork_size inSlots)
{
  // Note we cannot successfully call virtual ProbeMapClearKey() when we
  // call init_probe_map() inside the constructor; so we leave this problem
  // to the caller.  (The constructor will call ProbeMapClearKey() later
  // after setting a suitable lazy flag to show this action is pending.)

  if ( ev->Good() )
  {
    morkMapScratch old;

    if ( inSlots < 7 ) // capacity too small?
      inSlots = 7; // increase to reasonable minimum
    else if ( inSlots > (128 * 1024) ) // requested capacity too big?
      inSlots = (128 * 1024); // decrease to reasonable maximum
      
    if ( this->new_slots(ev, &old, inSlots) )
      sProbeMap_Tag = morkProbeMap_kTag;
      
    mork_count slots = sMap_Slots;
    mork_num emptyReserve = (slots / 7) + 1; // keep this many empty
    sProbeMap_MaxFill = slots - emptyReserve;

    MORK_MEMSET(&old, 0, sizeof(morkMapScratch)); // don't bother halting
  }
}

mork_bool
morkProbeMap::new_slots(morkEnv* ev, morkMapScratch* old, mork_num inSlots)
{
  mork_bool outNew = morkBool_kFalse;
  
  // Note we cannot successfully call virtual ProbeMapClearKey() when we
  // call new_slots() inside the constructor; so we leave this problem
  // to the caller.  (The constructor will call ProbeMapClearKey() later
  // after setting a suitable lazy flag to show this action is pending.)
    
  // allocate every new array before we continue:
  mork_u1* newKeys = this->map_new_keys(ev, inSlots);
  mork_u1* newVals = this->map_new_vals(ev, inSlots);
  
  // okay for newVals to be null when values are zero sized?
  mork_bool okayValues = ( newVals || !sMap_ValSize );
  
  if ( newKeys && okayValues )
  {
    outNew = morkBool_kTrue; // we created every array needed

    // init mapScratch using slots from current map:
    old->sMapScratch_Heap = sMap_Heap;
    
    old->sMapScratch_Slots = sMap_Slots;
    old->sMapScratch_Keys = sMap_Keys;
    old->sMapScratch_Vals = sMap_Vals;
    
    // replace all map array slots using the newly allocated members:
    ++sMap_Seed; // the map has changed
    sMap_Keys = newKeys;
    sMap_Vals = newVals;
    sMap_Slots = inSlots;
  }
  else // free any allocations if only partially successful
  {
    nsIMdbHeap* heap = sMap_Heap;
    if ( newKeys )
      heap->Free(ev->AsMdbEnv(), newKeys);
    if ( newVals )
      heap->Free(ev->AsMdbEnv(), newVals);
    
    MORK_MEMSET(old, 0, sizeof(morkMapScratch)); // zap scratch space
  }
  
  return outNew;
}

void
morkProbeMap::clear_probe_map(morkEnv* ev, nsIMdbHeap* ioMapHeap)
{
  sProbeMap_Tag = 0;
  sMap_Seed = 0;
  sMap_Slots = 0;
  sMap_Fill = 0;
  sMap_Keys = 0;
  sMap_Vals = 0;
  sProbeMap_MaxFill = 0;
  
  sMap_Heap = ioMapHeap;
  if ( !ioMapHeap )
    ev->NilPointerError();
}

morkProbeMap::morkProbeMap(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioNodeHeap,
  mork_size inKeySize, mork_size inValSize,
  nsIMdbHeap* ioMapHeap, mork_size inSlots,
  mork_bool inZeroIsClearKey)
  
: morkNode(ev, inUsage, ioNodeHeap)
, sMap_Heap( ioMapHeap )
    
, sMap_Keys( 0 )
, sMap_Vals( 0 )
  
, sMap_Seed( 0 )   // change count of members or structure
    
, sMap_Slots( 0 )  // count of slots in the hash table
, sMap_Fill( 0 )   // number of used slots in the hash table

, sMap_KeySize( 0 ) // size of each key (cannot be zero)
, sMap_ValSize( 0 ) // size of each val (zero allowed)
  
, sMap_KeyIsIP( morkBool_kFalse ) // sMap_KeySize == sizeof(mork_ip)
, sMap_ValIsIP( morkBool_kFalse ) // sMap_ValSize == sizeof(mork_ip)

, sProbeMap_MaxFill( 0 )
, sProbeMap_LazyClearOnAdd( 0 )
, sProbeMap_ZeroIsClearKey( inZeroIsClearKey )
, sProbeMap_Tag( 0 )
{
  // Note we cannot successfully call virtual ProbeMapClearKey() when we
  // call init_probe_map() inside the constructor; so we leave this problem
  // to the caller.  (The constructor will call ProbeMapClearKey() later
  // after setting a suitable lazy flag to show this action is pending.)

  if ( ev->Good() )
  {
    this->clear_probe_map(ev, ioMapHeap);
    if ( ev->Good() )
    {      
      sMap_KeySize = inKeySize;
      sMap_ValSize = inValSize;
      sMap_KeyIsIP = ( inKeySize == sizeof(mork_ip) );
      sMap_ValIsIP = ( inValSize == sizeof(mork_ip) );
      
      this->init_probe_map(ev, inSlots);
      if ( ev->Good() )
      {
        if ( !inZeroIsClearKey ) // must lazy clear later with virtual method?
          sProbeMap_LazyClearOnAdd = morkProbeMap_kLazyClearOnAdd;
          
        mNode_Derived = morkDerived_kProbeMap;
      }
    }
  }
}

/*============================================================================*/

/*virtual*/ mork_test // hit(a,b) implies hash(a) == hash(b)
morkProbeMap::MapTest(morkEnv* ev,
  const void* inMapKey, const void* inAppKey) const
  // Note inMapKey is always a key already stored in the map, while inAppKey
  //   is always a method argument parameter from a client method call.
  //   This matters the most in morkProbeMap subclasses, which have the
  //   responsibility of putting 'app' keys into slots for 'map' keys, and
  //   the bit pattern representation might be different in such cases.
  // morkTest_kHit means that inMapKey equals inAppKey (and this had better
  //   also imply that hash(inMapKey) == hash(inAppKey)).
  // morkTest_kMiss means that inMapKey does NOT equal inAppKey (but this
  //   implies nothing at all about hash(inMapKey) and hash(inAppKey)).
  // morkTest_kVoid means that inMapKey is not a valid key bit pattern,
  //   which means that key slot in the map is not being used.  Note that
  //   kVoid is only expected as a return value in morkProbeMap subclasses,
  //   because morkProbeMap must ask whether a key slot is used or not. 
  //   morkChainMap however, always knows when a key slot is used, so only
  //   key slots expected to have valid bit patterns will be presented to
  //   the MapTest() methods for morkChainMap subclasses.
  //
  // NOTE: it is very important that subclasses correctly return the value
  // morkTest_kVoid whenever the slot for inMapKey contains a bit pattern
  // that means the slot is not being used, because this is the only way a
  // probe map can terminate an unsuccessful search for a key in the map.
{
  mork_size keySize = sMap_KeySize;
  if ( keySize == sizeof(mork_ip) && sMap_KeyIsIP )
  {
    mork_ip mapKey = *((const mork_ip*) inMapKey);
    if ( mapKey == *((const mork_ip*) inAppKey) )
      return morkTest_kHit;
    else
    {
      return ( mapKey )? morkTest_kMiss : morkTest_kVoid;
    }
  }
  else
  {
    mork_bool allSame = morkBool_kTrue;
    mork_bool allZero = morkBool_kTrue;
    const mork_u1* ak = (const mork_u1*) inAppKey;
    const mork_u1* mk = (const mork_u1*) inMapKey;
    const mork_u1* end = mk + keySize;
    --mk; // prepare for preincrement:
    while ( ++mk < end )
    {
      mork_u1 byte = *mk;
      if ( byte ) // any nonzero byte in map key means slot is not nil?
        allZero = morkBool_kFalse;
      if ( byte != *ak++ ) // bytes differ in map and app keys?
        allSame = morkBool_kFalse;
    }
    if ( allSame )
      return morkTest_kHit;
    else
      return ( allZero )? morkTest_kVoid : morkTest_kMiss;
  }
}

/*virtual*/ mork_u4 // hit(a,b) implies hash(a) == hash(b)
morkProbeMap::MapHash(morkEnv* ev, const void* inAppKey) const
{
  mork_size keySize = sMap_KeySize;
  if ( keySize == sizeof(mork_ip) && sMap_KeyIsIP )
  {
    return *((const mork_ip*) inAppKey);
  }
  else
  {
    const mork_u1* key = (const mork_u1*) inAppKey;
    const mork_u1* end = key + keySize;
    --key; // prepare for preincrement:
    while ( ++key < end )
    {
      if ( *key ) // any nonzero byte in map key means slot is not nil?
        return morkBool_kFalse;
    }
    return morkBool_kTrue;
  }
  return (mork_u4) NS_PTR_TO_INT32(inAppKey);
}


/*============================================================================*/

/*virtual*/ mork_u4
morkProbeMap::ProbeMapHashMapKey(morkEnv* ev, const void* inMapKey) const
  // ProbeMapHashMapKey() does logically the same thing as MapHash(), and
  // the default implementation actually calls virtual MapHash().  However,
  // Subclasses must override this method whenever the formats of keys in
  // the map differ from app keys outside the map, because MapHash() only
  // works on keys in 'app' format, while ProbeMapHashMapKey() only works
  // on keys in 'map' format.  This method is called in order to rehash all
  // map keys when a map is grown, and this causes all old map members to
  // move into new slot locations.
  //
  // Note it is absolutely imperative that a hash for a key in 'map' format
  // be exactly the same the hash of the same key in 'app' format, or else
  // maps will seem corrupt later when keys in 'app' format cannot be found.
{
  return this->MapHash(ev, inMapKey);
}

/*virtual*/ mork_bool
morkProbeMap::ProbeMapIsKeyNil(morkEnv* ev, void* ioMapKey)
  // ProbeMapIsKeyNil() must say whether the representation of logical 'nil'
  // is currently found inside the key at ioMapKey, for a key found within
  // the map.  The the map iterator uses this method to find map keys that
  // are actually being used for valid map associations; otherwise the
  // iterator cannot determine which map slots actually denote used keys.
  // The default method version returns true if all the bits equal zero.
{
  if ( sMap_KeySize == sizeof(mork_ip) && sMap_KeyIsIP )
  {
    return !*((const mork_ip*) ioMapKey);
  }
  else
  {
    const mork_u1* key = (const mork_u1*) ioMapKey;
    const mork_u1* end = key + sMap_KeySize;
    --key; // prepare for preincrement:
    while ( ++key < end )
    {
      if ( *key ) // any nonzero byte in map key means slot is not nil?
        return morkBool_kFalse;
    }
    return morkBool_kTrue;
  }
}

/*virtual*/ void
morkProbeMap::ProbeMapClearKey(morkEnv* ev, // put 'nil' alls keys inside map
  void* ioMapKey, mork_count inKeyCount) // array of keys inside map
  // ProbeMapClearKey() must put some representation of logical 'nil' into
  // every key slot in the map, such that MapTest() will later recognize
  // that this bit pattern shows each key slot is not actually being used.
  //
  // This method is typically called whenever the map is either created or
  // grown into a larger size, where ioMapKey is a pointer to an array of
  // inKeyCount keys, where each key is this->MapKeySize() bytes in size.
  // Note that keys are assumed immediately adjacent with no padding, so
  // if any alignment requirements must be met, then subclasses should have
  // already accounted for this when specifying a key size in the map.
  //
  // Since this method will be called when a map is being grown in size,
  // nothing should be assumed about the state slots of the map, since the
  // ioMapKey array might not yet live in sMap_Keys, and the array length
  // inKeyCount might not yet live in sMap_Slots.  However, the value kept
  // in sMap_KeySize never changes, so this->MapKeySize() is always correct.
{
  if ( ioMapKey && inKeyCount )
  {
    MORK_MEMSET(ioMapKey, 0, (inKeyCount * sMap_KeySize));
  }
  else
    ev->NilPointerWarning();
}

/*virtual*/ void
morkProbeMap::ProbeMapPushIn(morkEnv* ev, // move (key,val) into the map
  const void* inAppKey, const void* inAppVal, // (key,val) outside map
  void* outMapKey, void* outMapVal)      // (key,val) inside map
  // This method actually puts keys and vals in the map in suitable format.
  //
  // ProbeMapPushIn() must copy a caller key and value in 'app' format
  // into the map slots provided, which are in 'map' format.  When the
  // 'app' and 'map' formats are identical, then this is just a bitwise
  // copy of this->MapKeySize() key bytes and this->MapValSize() val bytes,
  // and this is exactly what the default implementation performs.  However,
  // if 'app' and 'map' formats are different, and MapTest() depends on this
  // difference in format, then subclasses must override this method to do
  // whatever is necessary to store the input app key in output map format.
  //
  // Do NOT write more than this->MapKeySize() bytes of a map key, or more
  // than this->MapValSize() bytes of a map val, or corruption might ensue.
  //
  // The inAppKey and inAppVal parameters are the same ones passed into a
  // call to MapAtPut(), and the outMapKey and outMapVal parameters are ones
  // determined by how the map currently positions key inAppKey in the map.
  //
  // Note any key or val parameter can be a null pointer, in which case
  // this method must do nothing with those parameters.  In particular, do
  // no key move at all when either inAppKey or outMapKey is nil, and do
  // no val move at all when either inAppVal or outMapVal is nil.  Note that
  // outMapVal should always be nil when this->MapValSize() is nil.
{
}

/*virtual*/ void
morkProbeMap::ProbeMapPullOut(morkEnv* ev, // move (key,val) out from the map
  const void* inMapKey, const void* inMapVal, // (key,val) inside map
  void* outAppKey, void* outAppVal) const    // (key,val) outside map
  // This method actually gets keys and vals from the map in suitable format.
  //
  // ProbeMapPullOut() must copy a key and val in 'map' format into the
  // caller key and val slots provided, which are in 'app' format.  When the
  // 'app' and 'map' formats are identical, then this is just a bitwise
  // copy of this->MapKeySize() key bytes and this->MapValSize() val bytes,
  // and this is exactly what the default implementation performs.  However,
  // if 'app' and 'map' formats are different, and MapTest() depends on this
  // difference in format, then subclasses must override this method to do
  // whatever is necessary to store the input map key in output app format.
  //
  // The outAppKey and outAppVal parameters are the same ones passed into a
  // call to either MapAtPut() or MapAt(), while inMapKey and inMapVal are
  // determined by how the map currently positions the target key in the map.
  //
  // Note any key or val parameter can be a null pointer, in which case
  // this method must do nothing with those parameters.  In particular, do
  // no key move at all when either inMapKey or outAppKey is nil, and do
  // no val move at all when either inMapVal or outAppVal is nil.  Note that
  // inMapVal should always be nil when this->MapValSize() is nil.
{
}


/*============================================================================*/
/* morkProbeMapIter */

morkProbeMapIter::morkProbeMapIter(morkEnv* ev, morkProbeMap* ioMap)
: sProbeMapIter_Map( 0 )
, sProbeMapIter_Seed( 0 )
, sProbeMapIter_HereIx( morkProbeMapIter_kBeforeIx )
{
  if ( ioMap )
  {
    if ( ioMap->GoodProbeMap() )
    {
      if ( ioMap->need_lazy_init() ) // pending lazy action?
        ioMap->probe_map_lazy_init(ev);
        
      sProbeMapIter_Map = ioMap;
      sProbeMapIter_Seed = ioMap->sMap_Seed;
    }
    else ioMap->ProbeMapBadTagError(ev);
  }
  else ev->NilPointerError();
}

void morkProbeMapIter::CloseMapIter(morkEnv* ev)
{
  MORK_USED_1(ev);
  sProbeMapIter_Map = 0;
  sProbeMapIter_Seed = 0;

  sProbeMapIter_HereIx = morkProbeMapIter_kAfterIx;
}

morkProbeMapIter::morkProbeMapIter( )
// zero most slots; caller must call InitProbeMapIter()
{
  sProbeMapIter_Map = 0;
  sProbeMapIter_Seed = 0;

  sProbeMapIter_HereIx = morkProbeMapIter_kBeforeIx;
}

void morkProbeMapIter::InitProbeMapIter(morkEnv* ev, morkProbeMap* ioMap)
{
  sProbeMapIter_Map = 0;
  sProbeMapIter_Seed = 0;

  sProbeMapIter_HereIx = morkProbeMapIter_kBeforeIx;

  if ( ioMap )
  {
    if ( ioMap->GoodProbeMap() )
    {
      if ( ioMap->need_lazy_init() ) // pending lazy action?
        ioMap->probe_map_lazy_init(ev);
        
      sProbeMapIter_Map = ioMap;
      sProbeMapIter_Seed = ioMap->sMap_Seed;
    }
    else ioMap->ProbeMapBadTagError(ev);
  }
  else ev->NilPointerError();
}
 
mork_bool morkProbeMapIter::IterFirst(morkEnv* ev,
  void* outAppKey, void* outAppVal)
{
  sProbeMapIter_HereIx = morkProbeMapIter_kAfterIx; // default to done
  morkProbeMap* map = sProbeMapIter_Map;
  
  if ( map && map->GoodProbeMap() ) /* looks good? */
  {
    sProbeMapIter_Seed = map->sMap_Seed; /* sync the seeds */
    
    mork_u1* k = map->sMap_Keys;  // array of keys, each of size sMap_KeySize
    mork_num size = map->sMap_KeySize;  // number of bytes in each key
    mork_count slots = map->sMap_Slots; // total number of key buckets
    mork_pos here = 0;  // first hash bucket
    
    while ( here < (mork_pos)slots )
    {
      if ( !map->ProbeMapIsKeyNil(ev, k + (here * size)) )
      {
        map->get_probe_kv(ev, outAppKey, outAppVal, here);
        
        sProbeMapIter_HereIx = (mork_i4) here;
        return morkBool_kTrue;
      }
      ++here; // next bucket
    } 
  }
  else map->ProbeMapBadTagError(ev);

  return morkBool_kFalse;
}

mork_bool morkProbeMapIter::IterNext(morkEnv* ev,
  void* outAppKey, void* outAppVal)
{
  morkProbeMap* map = sProbeMapIter_Map;
  
  if ( map && map->GoodProbeMap() ) /* looks good? */
  {    
    if ( sProbeMapIter_Seed == map->sMap_Seed ) /* in sync? */
    {
      if ( sProbeMapIter_HereIx != morkProbeMapIter_kAfterIx )
      {
        mork_pos here = (mork_pos) sProbeMapIter_HereIx;
        if ( sProbeMapIter_HereIx < 0 )
          here = 0;
        else
          ++here;
          
        sProbeMapIter_HereIx = morkProbeMapIter_kAfterIx; // default to done

        mork_u1* k = map->sMap_Keys;  // key array, each of size sMap_KeySize
        mork_num size = map->sMap_KeySize;  // number of bytes in each key
        mork_count slots = map->sMap_Slots; // total number of key buckets
        
        while ( here < (mork_pos)slots )
        {
          if ( !map->ProbeMapIsKeyNil(ev, k + (here * size)) )
          {
            map->get_probe_kv(ev, outAppKey, outAppVal, here);
            
            sProbeMapIter_HereIx = (mork_i4) here;
            return morkBool_kTrue;
          }
          ++here; // next bucket
        } 
      }
    }
    else map->MapSeedOutOfSyncError(ev);
  }
  else map->ProbeMapBadTagError(ev);

  return morkBool_kFalse;
}

mork_bool morkProbeMapIter::IterHere(morkEnv* ev,
  void* outAppKey, void* outAppVal)
{
  morkProbeMap* map = sProbeMapIter_Map;
  
  if ( map && map->GoodProbeMap() ) /* looks good? */
  {    
    if ( sProbeMapIter_Seed == map->sMap_Seed ) /* in sync? */
    {
      mork_pos here = (mork_pos) sProbeMapIter_HereIx;
      mork_count slots = map->sMap_Slots; // total number of key buckets
      if ( sProbeMapIter_HereIx >= 0 && (here < (mork_pos)slots))
      {
        mork_u1* k = map->sMap_Keys;  // key array, each of size sMap_KeySize
        mork_num size = map->sMap_KeySize;  // number of bytes in each key

        if ( !map->ProbeMapIsKeyNil(ev, k + (here * size)) )
        {
          map->get_probe_kv(ev, outAppKey, outAppVal, here);
          return morkBool_kTrue;
        }
      }
    }
    else map->MapSeedOutOfSyncError(ev);
  }
  else map->ProbeMapBadTagError(ev);

  return morkBool_kFalse;
}

mork_change*
morkProbeMapIter::First(morkEnv* ev, void* outKey, void* outVal)
{
  if ( this->IterFirst(ev, outKey, outVal) )
    return &sProbeMapIter_Change;
  
  return (mork_change*) 0;
}

mork_change*
morkProbeMapIter::Next(morkEnv* ev, void* outKey, void* outVal)
{
  if ( this->IterNext(ev, outKey, outVal) )
    return &sProbeMapIter_Change;
  
  return (mork_change*) 0;
}

mork_change*
morkProbeMapIter::Here(morkEnv* ev, void* outKey, void* outVal)
{
  if ( this->IterHere(ev, outKey, outVal) )
    return &sProbeMapIter_Change;
  
  return (mork_change*) 0;
}

mork_change*
morkProbeMapIter::CutHere(morkEnv* ev, void* outKey, void* outVal)
{
  morkProbeMap::ProbeMapCutError(ev);
  
  return (mork_change*) 0;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// NOTE: the following methods ONLY work for sMap_ValIsIP pointer values.
// (Note the implied assumption that zero is never a good value pattern.)

void* morkProbeMapIter::IterFirstVal(morkEnv* ev, void* outKey)
// equivalent to { void* v=0; this->IterFirst(ev, outKey, &v); return v; }
{
  morkProbeMap* map = sProbeMapIter_Map;
  if ( map )
  {
    if ( map->sMap_ValIsIP )
    {
      void* v = 0;
      this->IterFirst(ev, outKey, &v);
      return v;
    }
    else
      map->MapValIsNotIPError(ev);
  }
  return (void*) 0;
}

void* morkProbeMapIter::IterNextVal(morkEnv* ev, void* outKey)
// equivalent to { void* v=0; this->IterNext(ev, outKey, &v); return v; }
{
  morkProbeMap* map = sProbeMapIter_Map;
  if ( map )
  {
    if ( map->sMap_ValIsIP )
    {
      void* v = 0;
      this->IterNext(ev, outKey, &v);
      return v;
    }
    else
      map->MapValIsNotIPError(ev);
  }
  return (void*) 0;
}

void* morkProbeMapIter::IterHereVal(morkEnv* ev, void* outKey)
// equivalent to { void* v=0; this->IterHere(ev, outKey, &v); return v; }
{
  morkProbeMap* map = sProbeMapIter_Map;
  if ( map )
  {
    if ( map->sMap_ValIsIP )
    {
      void* v = 0;
      this->IterHere(ev, outKey, &v);
      return v;
    }
    else
      map->MapValIsNotIPError(ev);
  }
  return (void*) 0;
}

// NOTE: the following methods ONLY work for sMap_KeyIsIP pointer values.
// (Note the implied assumption that zero is never a good key pattern.)

void* morkProbeMapIter::IterFirstKey(morkEnv* ev)
// equivalent to { void* k=0; this->IterFirst(ev, &k, 0); return k; }
{
  morkProbeMap* map = sProbeMapIter_Map;
  if ( map )
  {
    if ( map->sMap_KeyIsIP )
    {
      void* k = 0;
      this->IterFirst(ev, &k, (void*) 0);
      return k;
    }
    else
      map->MapKeyIsNotIPError(ev);
  }
  return (void*) 0;
}

void* morkProbeMapIter::IterNextKey(morkEnv* ev)
// equivalent to { void* k=0; this->IterNext(ev, &k, 0); return k; }
{
  morkProbeMap* map = sProbeMapIter_Map;
  if ( map )
  {
    if ( map->sMap_KeyIsIP )
    {
      void* k = 0;
      this->IterNext(ev, &k, (void*) 0);
      return k;
    }
    else
      map->MapKeyIsNotIPError(ev);
  }
  return (void*) 0;
}

void* morkProbeMapIter::IterHereKey(morkEnv* ev)
// equivalent to { void* k=0; this->IterHere(ev, &k, 0); return k; }
{
  morkProbeMap* map = sProbeMapIter_Map;
  if ( map )
  {
    if ( map->sMap_KeyIsIP )
    {
      void* k = 0;
      this->IterHere(ev, &k, (void*) 0);
      return k;
    }
    else
      map->MapKeyIsNotIPError(ev);
  }
  return (void*) 0;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
