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

#ifndef _MORKNODEMAP_
#include "morkNodeMap.h"
#endif

#ifndef _MORKROWMAP_
#include "morkRowMap.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKROWSPACE_
#include "morkRowSpace.h"
#endif

#ifndef _MORKPOOL_
#include "morkPool.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkRowSpace::CloseMorkNode(morkEnv* ev) // CloseRowSpace() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseRowSpace(ev);
    this->MarkShut();  
  }
}

/*public virtual*/
morkRowSpace::~morkRowSpace() // assert CloseRowSpace() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkRowSpace::morkRowSpace(morkEnv* ev, 
  const morkUsage& inUsage, mork_scope inScope, morkStore* ioStore,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkSpace(ev, inUsage, inScope, ioStore, ioHeap, ioSlotHeap)
, mRowSpace_Tables(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioSlotHeap)
, mRowSpace_Rows(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioSlotHeap,
  morkRowSpace_kStartRowMapSlotCount)
, mRowSpace_NextTableId( 1 )
, mRowSpace_NextRowId( 1 )
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kRowSpace;
}

/*public non-poly*/ void
morkRowSpace::CloseRowSpace(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mRowSpace_Tables.CloseMorkNode(ev);
      
      morkStore* store = mSpace_Store;
      if ( store )
          this->CutAllRows(ev, &store->mStore_Pool);
      
      mRowSpace_Rows.CloseMorkNode(ev);
      this->CloseSpace(ev);
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
morkRowSpace::NonRowSpaceTypeError(morkEnv* ev)
{
  ev->NewError("non morkRowSpace");
}

/*static*/ void
morkRowSpace::ZeroKindError(morkEnv* ev)
{
  ev->NewError("zero table kind");
}

/*static*/ void
morkRowSpace::ZeroScopeError(morkEnv* ev)
{
  ev->NewError("zero row scope");
}

/*static*/ void
morkRowSpace::ZeroTidError(morkEnv* ev)
{
  ev->NewError("zero table ID");
}

/*static*/ void
morkRowSpace::MinusOneRidError(morkEnv* ev)
{
  ev->NewError("row ID is -1");
}

///*static*/ void
//morkRowSpace::ExpectAutoIdOnlyError(morkEnv* ev)
//{
//  ev->NewError("zero row ID");
//}

///*static*/ void
//morkRowSpace::ExpectAutoIdNeverError(morkEnv* ev)
//{
//}

mork_num
morkRowSpace::CutAllRows(morkEnv* ev, morkPool* ioPool)
{
  mork_num outSlots = mRowSpace_Rows.mMap_Fill;
  morkRow* r = 0; // old key row in the map
  
  mork_change* c = 0;
  morkRowMapIter i(ev, &mRowSpace_Rows);
  for ( c = i.FirstRow(ev, &r); c && ev->Good();
        c = i.NextRow(ev, &r) )
  {
    if ( r->IsRow() )
    {
      if ( r->mRow_Object )
      {
        morkRowObject::SlotWeakRowObject((morkRowObject*) 0, ev,
          &r->mRow_Object);
      }
      if ( r )
        ioPool->ZapRow(ev, r);
    }
    else
      r->NonRowTypeWarning(ev);
    
    i.CutHereRow(ev, /*key*/ (morkRow**) 0);
  }
  
  return outSlots;
}


morkTable*
morkRowSpace::FindTableByKind(morkEnv* ev, mork_kind inTableKind)
{
  if ( inTableKind )
  {
    mork_tid* key = 0; // nil pointer to suppress key access
    morkTable* table = 0; // old table in the map

    mork_change* c = 0;
    morkTableMapIter i(ev, &mRowSpace_Tables);
    for ( c = i.FirstTable(ev, key, &table); c && ev->Good();
          c = i.NextTable(ev, key, &table) )
    {
      if ( table->mTable_Kind == inTableKind )
        return table;
    }
  }
  else
    this->ZeroKindError(ev);
    
  return (morkTable*) 0;
}

morkTable*
morkRowSpace::NewTable(morkEnv* ev, mork_kind inTableKind,
  mdb_bool inMustBeUnique)
{
  morkTable* outTable = 0;
  
  if ( inTableKind )
  {
    if ( inMustBeUnique ) // need to look for existing table first?
      outTable = this->FindTableByKind(ev, inTableKind);
      
    if ( !outTable && ev->Good() )
    {
      mork_tid id = this->MakeNewTableId(ev);
      if ( id )
      {
        nsIMdbHeap* heap = mSpace_Store->mPort_Heap;
        morkTable* table = new(*heap, ev)
          morkTable(ev, morkUsage::kHeap, heap, mSpace_Store, heap, this,
            id, inTableKind, inMustBeUnique);
        if ( table )
        {
          if ( mRowSpace_Tables.AddTable(ev, table) )
            outTable = table;
          else
            table->CutStrongRef(ev);
        }
      }
    }
  }
  else
    this->ZeroKindError(ev);
    
  return outTable;
}

mork_tid
morkRowSpace::MakeNewTableId(morkEnv* ev)
{
  mork_tid outTid = 0;
  mork_tid id = mRowSpace_NextTableId;
  mork_num count = 9; // try up to eight times
  
  while ( !outTid && --count ) // still trying to find an unused table ID?
  {
    if ( !mRowSpace_Tables.GetTable(ev, id) )
      outTid = id;
    else
    {
      MORK_ASSERT(morkBool_kFalse); // alert developer about ID problems
      ++id;
    }
  }
  
  mRowSpace_NextTableId = id + 1;
  return outTid;
}

mork_rid
morkRowSpace::MakeNewRowId(morkEnv* ev)
{
  mork_rid outRid = 0;
  mork_rid id = mRowSpace_NextRowId;
  mork_num count = 9; // try up to eight times
  mdbOid oid;
  oid.mOid_Scope = mSpace_Scope;
  
  while ( !outRid && --count ) // still trying to find an unused row ID?
  {
    oid.mOid_Id = id;
    if ( !mRowSpace_Rows.GetOid(ev, &oid) )
      outRid = id;
    else
    {
      MORK_ASSERT(morkBool_kFalse); // alert developer about ID problems
      ++id;
    }
  }
  
  mRowSpace_NextRowId = id + 1;
  return outRid;
}


morkRow*
morkRowSpace::NewRowWithOid(morkEnv* ev, const mdbOid* inOid)
{
  morkRow* outRow = mRowSpace_Rows.GetOid(ev, inOid);
  MORK_ASSERT(outRow==0);
  if ( !outRow && ev->Good() )
  {
    morkPool* pool = this->GetSpaceStorePool();
    morkRow* row = pool->NewRow(ev);
    if ( row )
    {
      row->InitRow(ev, inOid, this, /*length*/ 0, pool);
      
      if ( ev->Good() && mRowSpace_Rows.AddRow(ev, row) )
        outRow = row;
      else
        pool->ZapRow(ev, row);
    }
  }
  return outRow;
}

morkRow*
morkRowSpace::NewRow(morkEnv* ev)
{
  morkRow* outRow = 0;
  if ( ev->Good() )
  {
    mork_rid id = this->MakeNewRowId(ev);
    if ( id )
    {
      mdbOid oid;
      oid.mOid_Scope = mSpace_Scope;
      oid.mOid_Id = id;
      morkPool* pool = this->GetSpaceStorePool();
      morkRow* row = pool->NewRow(ev);
      if ( row )
      {
        row->InitRow(ev, &oid, this, /*length*/ 0, pool);
        
        if ( ev->Good() && mRowSpace_Rows.AddRow(ev, row) )
          outRow = row;
        else
          pool->ZapRow(ev, row);
      }
    }
  }
  return outRow;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


morkRowSpaceMap::~morkRowSpaceMap()
{
}

morkRowSpaceMap::morkRowSpaceMap(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
  : morkNodeMap(ev, inUsage, ioHeap, ioSlotHeap)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kRowSpaceMap;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
