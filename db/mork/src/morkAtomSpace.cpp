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
  // the morkSpace base constructor handles any dirty propagation
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
#ifdef MORK_ENABLE_ZONE_ARENAS
  MORK_USED_2(ev, ioPool);
  return 0;
#else /*MORK_ENABLE_ZONE_ARENAS*/
  if ( this->IsAtomSpaceClean() )
    this->MaybeDirtyStoreAndSpace();
  
  mork_num outSlots = mAtomSpace_AtomAids.MapFill();
  morkBookAtom* a = 0; // old key atom in the map
  
  morkStore* store = mSpace_Store;
  mork_change* c = 0;
  morkAtomAidMapIter i(ev, &mAtomSpace_AtomAids);
  for ( c = i.FirstAtom(ev, &a); c ; c = i.NextAtom(ev, &a) )
  {
    if ( a )
      ioPool->ZapAtom(ev, a, &store->mStore_Zone);

#ifdef MORK_ENABLE_PROBE_MAPS
    // do not cut anything from the map
#else /*MORK_ENABLE_PROBE_MAPS*/
    i.CutHereAtom(ev, /*key*/ (morkBookAtom**) 0);
#endif /*MORK_ENABLE_PROBE_MAPS*/
  }
  
  return outSlots;
#endif /*MORK_ENABLE_ZONE_ARENAS*/
}


morkBookAtom*
morkAtomSpace::MakeBookAtomCopyWithAid(morkEnv* ev,
   const morkFarBookAtom& inAtom,  mork_aid inAid)
// Make copy of inAtom and put it in both maps, using specified ID.
{
  morkBookAtom* outAtom = 0;
  morkStore* store = mSpace_Store;
  if ( ev->Good() && store )
  {
    morkPool* pool = this->GetSpaceStorePool();
    outAtom = pool->NewFarBookAtomCopy(ev, inAtom, &store->mStore_Zone);
    if ( outAtom )
    {
      if ( store->mStore_CanDirty )
      {
        outAtom->SetAtomDirty();
        if ( this->IsAtomSpaceClean() )
          this->MaybeDirtyStoreAndSpace();
      }
  
      outAtom->mBookAtom_Id = inAid;
      outAtom->mBookAtom_Space = this;
      mAtomSpace_AtomAids.AddAtom(ev, outAtom);
      mAtomSpace_AtomBodies.AddAtom(ev, outAtom);
      if ( this->SpaceScope() == morkAtomSpace_kColumnScope )
        outAtom->MakeCellUseForever(ev);

      if ( mAtomSpace_HighUnderId <= inAid )
        mAtomSpace_HighUnderId = inAid + 1;
    }
  }
  return outAtom;
}

morkBookAtom*
morkAtomSpace::MakeBookAtomCopy(morkEnv* ev, const morkFarBookAtom& inAtom)
// make copy of inAtom and put it in both maps, using a new ID as needed.
{
  morkBookAtom* outAtom = 0;
  morkStore* store = mSpace_Store;
  if ( ev->Good() && store )
  {
    if ( store->mStore_CanAutoAssignAtomIdentity )
    {
      morkPool* pool = this->GetSpaceStorePool();
      morkBookAtom* atom = pool->NewFarBookAtomCopy(ev, inAtom, &mSpace_Store->mStore_Zone);
      if ( atom )
      {
        mork_aid id = this->MakeNewAtomId(ev, atom);
        if ( id )
        {
          if ( store->mStore_CanDirty )
          {
            atom->SetAtomDirty();
            if ( this->IsAtomSpaceClean() )
              this->MaybeDirtyStoreAndSpace();
          }
            
          outAtom = atom; 
          atom->mBookAtom_Space = this;
          mAtomSpace_AtomAids.AddAtom(ev, atom);
          mAtomSpace_AtomBodies.AddAtom(ev, atom);
          if ( this->SpaceScope() == morkAtomSpace_kColumnScope )
            outAtom->MakeCellUseForever(ev);
        }
        else
          pool->ZapAtom(ev, atom, &mSpace_Store->mStore_Zone);
      }
    }
    else
      mSpace_Store->CannotAutoAssignAtomIdentityError(ev);
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

