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

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKBEAD_
#include "morkBead.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkBead::CloseMorkNode(morkEnv* ev) // CloseBead() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseBead(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkBead::~morkBead() // assert CloseBead() executed earlier
{
  MORK_ASSERT(mBead_Color==0 || mNode_Usage == morkUsage_kStack );
}

/*public non-poly*/
morkBead::morkBead(mork_color inBeadColor)
: morkNode( morkUsage_kStack )
, mBead_Color( inBeadColor )
{
}

/*public non-poly*/
morkBead::morkBead(const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
  mork_color inBeadColor)
: morkNode( inUsage, ioHeap )
, mBead_Color( inBeadColor )
{
}

/*public non-poly*/
morkBead::morkBead(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, mork_color inBeadColor)
: morkNode(ev, inUsage, ioHeap)
, mBead_Color( inBeadColor )
{
  if ( ev->Good() )
  {
    if ( ev->Good() )
      mNode_Derived = morkDerived_kBead;
  }
}

/*public non-poly*/ void
morkBead::CloseBead(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      if ( !this->IsShutNode() )
      {
        mBead_Color = 0;
        this->MarkShut();
      }
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkBeadMap::CloseMorkNode(morkEnv* ev) // CloseBeadMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseBeadMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkBeadMap::~morkBeadMap() // assert CloseBeadMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkBeadMap::morkBeadMap(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkMap(ev, inUsage, ioHeap, sizeof(morkBead*), /*inValSize*/ 0,
  /*slotCount*/ 11, ioSlotHeap, /*holdChanges*/ morkBool_kFalse)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kBeadMap;
}

/*public non-poly*/ void
morkBeadMap::CloseBeadMap(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      this->CutAllBeads(ev);
      this->CloseMap(ev);
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

mork_bool
morkBeadMap::AddBead(morkEnv* ev, morkBead* ioBead)
  // the AddBead() boolean return equals ev->Good().
{
  if ( ioBead && ev->Good() )
  {
    morkBead* oldBead = 0; // old key in the map

    mork_bool put = this->Put(ev, &ioBead, /*val*/ (void*) 0,
      /*key*/ &oldBead, /*val*/ (void*) 0, (mork_change**) 0);
      
    if ( put ) // replaced an existing key?
    {
      if ( oldBead != ioBead ) // new bead was not already in table?
        ioBead->AddStrongRef(ev); // now there's another ref
        
      if ( oldBead && oldBead != ioBead ) // need to release old node?
        oldBead->CutStrongRef(ev);
    }
    else
      ioBead->AddStrongRef(ev); // another ref if not already in table
  }
  else if ( !ioBead )
    ev->NilPointerError();
    
  return ev->Good();
}

mork_bool
morkBeadMap::CutBead(morkEnv* ev, mork_color inColor)
{
  morkBead* oldBead = 0; // old key in the map
  morkBead bead(inColor);
  morkBead* key = &bead;
  
  mork_bool outCutNode = this->Cut(ev, &key, 
    /*key*/ &oldBead, /*val*/ (void*) 0, (mork_change**) 0);
    
  if ( oldBead )
    oldBead->CutStrongRef(ev);
  
  bead.CloseBead(ev);
  return outCutNode;
}

morkBead*
morkBeadMap::GetBead(morkEnv* ev, mork_color inColor)
  // Note the returned bead does NOT have an increase in refcount for this.
{
  morkBead* oldBead = 0; // old key in the map
  morkBead bead(inColor);
  morkBead* key = &bead;

  this->Get(ev, &key, /*key*/ &oldBead, /*val*/ (void*) 0, (mork_change**) 0);
  
  bead.CloseBead(ev);
  return oldBead;
}

mork_num
morkBeadMap::CutAllBeads(morkEnv* ev)
  // CutAllBeads() releases all the referenced beads.
{
  mork_num outSlots = mMap_Slots;
  
  morkBeadMapIter i(ev, this);
  morkBead* b = i.FirstBead(ev);

  while ( b )
  {
    b->CutStrongRef(ev);
    i.CutHereBead(ev);
    b = i.NextBead(ev);
  }
  
  return outSlots;
}


// { ===== begin morkMap poly interface =====
/*virtual*/ mork_bool
morkBeadMap::Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const
{
  MORK_USED_1(ev);
  return (*(const morkBead**) inKeyA)->BeadEqual(
    *(const morkBead**) inKeyB);
}

/*virtual*/ mork_u4
morkBeadMap::Hash(morkEnv* ev, const void* inKey) const
{
  MORK_USED_1(ev);
    return (*(const morkBead**) inKey)->BeadHash();
}
// } ===== end morkMap poly interface =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

 
morkBead* morkBeadMapIter::FirstBead(morkEnv* ev)
{
  morkBead* bead = 0;
  this->First(ev, &bead, /*val*/ (void*) 0);
  return bead;
}

morkBead* morkBeadMapIter::NextBead(morkEnv* ev)
{
  morkBead* bead = 0;
  this->Next(ev, &bead, /*val*/ (void*) 0);
  return bead;
}

morkBead* morkBeadMapIter::HereBead(morkEnv* ev)
{
  morkBead* bead = 0;
  this->Here(ev, &bead, /*val*/ (void*) 0);
  return bead;
}

void morkBeadMapIter::CutHereBead(morkEnv* ev)
{
  this->CutHere(ev, /*key*/ (void*) 0, /*val*/ (void*) 0);
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkBeadProbeMap::CloseMorkNode(morkEnv* ev) // CloseBeadProbeMap() if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseBeadProbeMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkBeadProbeMap::~morkBeadProbeMap() // assert CloseBeadProbeMap() earlier
{
  MORK_ASSERT(this->IsShutNode());
}


/*public non-poly*/
morkBeadProbeMap::morkBeadProbeMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkProbeMap(ev, inUsage, ioHeap,
  /*inKeySize*/ sizeof(morkBead*), /*inValSize*/ 0,
  ioSlotHeap, /*startSlotCount*/ 11, 
  /*inZeroIsClearKey*/ morkBool_kTrue)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kBeadProbeMap;
}

/*public non-poly*/ void
morkBeadProbeMap::CloseBeadProbeMap(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      this->CutAllBeads(ev);
      this->CloseProbeMap(ev);
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

/*virtual*/ mork_test // hit(a,b) implies hash(a) == hash(b)
morkBeadProbeMap::MapTest(morkEnv* ev, const void* inMapKey,
  const void* inAppKey) const
{
  MORK_USED_1(ev);
  const morkBead* key = *(const morkBead**) inMapKey;
  if ( key )
  {
    mork_bool hit = key->BeadEqual(*(const morkBead**) inAppKey);
    return ( hit ) ? morkTest_kHit : morkTest_kMiss;
  }
  else
    return morkTest_kVoid;
}

/*virtual*/ mork_u4 // hit(a,b) implies hash(a) == hash(b)
morkBeadProbeMap::MapHash(morkEnv* ev, const void* inAppKey) const
{
  const morkBead* key = *(const morkBead**) inAppKey;
  if ( key )
    return key->BeadHash();
  else
  {
    ev->NilPointerWarning();
    return 0;
  }
}

/*virtual*/ mork_u4 
morkBeadProbeMap::ProbeMapHashMapKey(morkEnv* ev,
  const void* inMapKey) const
{
  const morkBead* key = *(const morkBead**) inMapKey;
  if ( key )
    return key->BeadHash();
  else
  {
    ev->NilPointerWarning();
    return 0;
  }
}

mork_bool
morkBeadProbeMap::AddBead(morkEnv* ev, morkBead* ioBead)
{
  if ( ioBead && ev->Good() )
  {
    morkBead* bead = 0; // old key in the map
    
    mork_bool put = this->MapAtPut(ev, &ioBead, /*val*/ (void*) 0, 
      /*key*/ &bead, /*val*/ (void*) 0);
          
    if ( put ) // replaced an existing key?
    {
      if ( bead != ioBead ) // new bead was not already in table?
        ioBead->AddStrongRef(ev); // now there's another ref
        
      if ( bead && bead != ioBead ) // need to release old node?
        bead->CutStrongRef(ev);
    }
    else
      ioBead->AddStrongRef(ev); // now there's another ref
  }
  else if ( !ioBead )
    ev->NilPointerError();
    
  return ev->Good();
}

morkBead*
morkBeadProbeMap::GetBead(morkEnv* ev, mork_color inColor)
{
  morkBead* oldBead = 0; // old key in the map
  morkBead bead(inColor);
  morkBead* key = &bead;

  this->MapAt(ev, &key, &oldBead, /*val*/ (void*) 0);
  
  bead.CloseBead(ev);
  return oldBead;
}

mork_num
morkBeadProbeMap::CutAllBeads(morkEnv* ev)
  // CutAllBeads() releases all the referenced bead values.
{
  mork_num outSlots = sMap_Slots;
  
  morkBeadProbeMapIter i(ev, this);
  morkBead* b = i.FirstBead(ev);

  while ( b )
  {
    b->CutStrongRef(ev);
    b = i.NextBead(ev);
  }
  this->MapCutAll(ev);
  
  return outSlots;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


