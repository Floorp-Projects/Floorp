/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkSpace::CloseMorkNode(morkEnv* ev) // CloseSpace() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseSpace(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkSpace::~morkSpace() // assert CloseSpace() executed earlier
{
  MORK_ASSERT(SpaceScope()==0);
  MORK_ASSERT(mSpace_Store==0);
  MORK_ASSERT(this->IsShutNode());
}    

/*public non-poly*/
//morkSpace::morkSpace(morkEnv* ev, const morkUsage& inUsage,
//  nsIMdbHeap* ioNodeHeap, const morkMapForm& inForm,
//  nsIMdbHeap* ioSlotHeap)
//: morkNode(ev, inUsage, ioNodeHeap)
//, mSpace_Map(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioSlotHeap)
//{
//  ev->StubMethodOnlyError();
//}

/*public non-poly*/
morkSpace::morkSpace(morkEnv* ev,
  const morkUsage& inUsage, mork_scope inScope, morkStore* ioStore,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkBead(ev, inUsage, ioHeap, inScope)
, mSpace_Store( 0 )
, mSpace_DoAutoIDs( morkBool_kFalse )
, mSpace_HaveDoneAutoIDs( morkBool_kFalse )
, mSpace_CanDirty( morkBool_kFalse ) // only when store can be dirtied
{
  if ( ev->Good() )
  {
    if ( ioStore && ioSlotHeap )
    {
      morkStore::SlotWeakStore(ioStore, ev, &mSpace_Store);

      mSpace_CanDirty = ioStore->mStore_CanDirty;
      if ( mSpace_CanDirty ) // this new space dirties the store?
        this->MaybeDirtyStoreAndSpace();
        
      if ( ev->Good() )
        mNode_Derived = morkDerived_kSpace;
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkSpace::CloseSpace(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      morkStore::SlotWeakStore((morkStore*) 0, ev, &mSpace_Store);
      mBead_Color = 0; // this->CloseBead();
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
morkSpace::NonAsciiSpaceScopeName(morkEnv* ev)
{
  ev->NewError("SpaceScope() > 0x7F");
}

/*static*/ void 
morkSpace::NilSpaceStoreError(morkEnv* ev)
{
  ev->NewError("nil mSpace_Store");
}

morkPool* morkSpace::GetSpaceStorePool() const
{
  return &mSpace_Store->mStore_Pool;
}

mork_bool morkSpace::MaybeDirtyStoreAndSpace()
{
  morkStore* store = mSpace_Store;
  if ( store && store->mStore_CanDirty )
  {
    store->SetStoreDirty();
    mSpace_CanDirty = morkBool_kTrue;
  }
  
  if ( mSpace_CanDirty )
  {
    this->SetSpaceDirty();
    return morkBool_kTrue;
  }
  
  return morkBool_kFalse;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
