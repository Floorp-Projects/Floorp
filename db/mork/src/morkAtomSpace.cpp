/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights 
 * Reserved. 
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

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKSPACE_
#include "morkSpace.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKSPACE_
#include "morkSpace.h"
#endif

#ifndef _MORKATOMSPACE_
#include "morkAtomSpace.h"
#endif

#ifndef _MORKPOOL_
#include "morkPool.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKATOM_
#include "morkAtom.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkAtomSpace::CloseMorkNode(morkEnv* ev) // CloseAtomSpace() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseAtomSpace(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkAtomSpace::~morkAtomSpace() // assert CloseAtomSpace() executed earlier
{
  MORK_ASSERT(mAtomSpace_HighUnderId==0);
  MORK_ASSERT(mAtomSpace_HighOverId==0);
  MORK_ASSERT(this->IsShutNode());
  MORK_ASSERT(mAtomSpace_AtomAids.IsShutNode());
  MORK_ASSERT(mAtomSpace_AtomBodies.IsShutNode());
}

/*public non-poly*/
morkAtomSpace::morkAtomSpace(morkEnv* ev, const morkUsage& inUsage,
  mork_scope inScope, morkStore* ioStore,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkSpace(ev, inUsage, inScope, ioStore, ioHeap, ioSlotHeap)
, mAtomSpace_HighUnderId( morkAtomSpace_kMinUnderId )
, mAtomSpace_HighOverId( morkAtomSpace_kMinOverId )
, mAtomSpace_AtomAids(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioSlotHeap)
, mAtomSpace_AtomBodies(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioSlotHeap)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kAtomSpace;
}

/*public non-poly*/ void
morkAtomSpace::CloseAtomSpace(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mAtomSpace_AtomBodies.CloseMorkNode(ev);
      morkStore* store = mSpace_Store;
      if ( store )
        this->CutAllAtoms(ev, &store->mStore_Pool);
      
      mAtomSpace_AtomAids.CloseMorkNode(ev);
      this->CloseSpace(ev);
      mAtomSpace_HighUnderId = 0;
      mAtomSpace_HighOverId = 0;
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

/*static*/ void
morkAtomSpace::NonAtomSpaceTypeError(morkEnv* ev)
{
  ev->NewError("non morkAtomSpace");
}

mork_num
morkAtomSpace::CutAllAtoms(morkEnv* ev, morkPool* ioPool)
{
  mork_num outSlots = mAtomSpace_AtomAids.mMap_Fill;
  morkBookAtom* a = 0; // old key atom in the map
  
  mork_change* c = 0;
  morkAtomAidMapIter i(ev, &mAtomSpace_AtomAids);
  for ( c = i.FirstAtom(ev, &a); c ; c = i.NextAtom(ev, &a) )
  {
    if ( a )
      ioPool->ZapAtom(ev, a);
    i.CutHereAtom(ev, /*key*/ (morkBookAtom**) 0);
  }
  
  return outSlots;
}


morkBookAtom*
morkAtomSpace::MakeBookAtomCopyWithAid(morkEnv* ev,
   const morkBigBookAtom& inAtom,  mork_aid inAid)
// Make copy of inAtom and put it in both maps, using specified ID.
{
  morkBookAtom* outAtom = 0;
  if ( ev->Good() )
  {
    morkPool* pool = this->GetSpaceStorePool();
    outAtom = pool->NewBookAtomCopy(ev, inAtom);
    if ( outAtom )
    {
      outAtom->mBookAtom_Id = inAid;
      outAtom->mBookAtom_Space = this;
      mAtomSpace_AtomAids.AddAtom(ev, outAtom);
      mAtomSpace_AtomBodies.AddAtom(ev, outAtom);
      if ( mSpace_Scope == morkAtomSpace_kColumnScope )
        outAtom->MakeCellUseForever(ev);

      if ( mAtomSpace_HighUnderId <= inAid )
        mAtomSpace_HighUnderId = inAid + 1;
    }
  }
  return outAtom;
}

morkBookAtom*
morkAtomSpace::MakeBookAtomCopy(morkEnv* ev, const morkBigBookAtom& inAtom)
// make copy of inAtom and put it in both maps, using a new ID as needed.
{
  morkBookAtom* outAtom = 0;
  if ( ev->Good() )
  {
    morkPool* pool = this->GetSpaceStorePool();
    morkBookAtom* atom = pool->NewBookAtomCopy(ev, inAtom);
    if ( atom )
    {
      mork_aid id = this->MakeNewAtomId(ev, atom);
      if ( id )
      {
        outAtom = atom; 
        atom->mBookAtom_Space = this;
        mAtomSpace_AtomAids.AddAtom(ev, atom);
        mAtomSpace_AtomBodies.AddAtom(ev, atom);
        if ( mSpace_Scope == morkAtomSpace_kColumnScope )
          outAtom->MakeCellUseForever(ev);
      }
    }
  }
  return outAtom;
}


mork_aid
morkAtomSpace::MakeNewAtomId(morkEnv* ev, morkBookAtom* ioAtom)
{
  mork_aid outAid = 0;
  mork_tid id = mAtomSpace_HighUnderId;
  mork_num count = 8; // try up to eight times
  
  while ( !outAid && count ) // still trying to find an unused table ID?
  {
    --count;
    ioAtom->mBookAtom_Id = id;
    if ( !mAtomSpace_AtomAids.GetAtom(ev, ioAtom) )
      outAid = id;
    else
    {
      MORK_ASSERT(morkBool_kFalse); // alert developer about ID problems
      ++id;
    }
  }
  
  mAtomSpace_HighUnderId = id + 1;
  return outAid;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


morkAtomSpaceMap::~morkAtomSpaceMap()
{
}

morkAtomSpaceMap::morkAtomSpaceMap(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
  : morkNodeMap(ev, inUsage, ioHeap, ioSlotHeap)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kAtomSpaceMap;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
