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

#ifndef _MORKATOMMAP_
#include "morkAtomMap.h"
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
, mRowSpace_SlotHeap( ioSlotHeap )
, mRowSpace_Rows(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioSlotHeap,
  morkRowSpace_kStartRowMapSlotCount)
, mRowSpace_Tables(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioSlotHeap)
, mRowSpace_NextTableId( 1 )
, mRowSpace_NextRowId( 1 )

, mRowSpace_IndexCount( 0 )
{
  morkAtomRowMap** cache = mRowSpace_IndexCache;
  morkAtomRowMap** cacheEnd = cache + morkRowSpace_kPrimeCacheSize;
  while ( cache < cacheEnd )
    *cache++ = 0; // put nil into every slot of cache table
    
  if ( ev->Good() )
  {
    if ( ioSlotHeap )
    {
      mNode_Derived = morkDerived_kRowSpace;
      
      // the morkSpace base constructor handles any dirty propagation
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkRowSpace::CloseRowSpace(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      morkAtomRowMap** cache = mRowSpace_IndexCache;
      morkAtomRowMap** cacheEnd = cache + morkRowSpace_kPrimeCacheSize;
      --cache; // prepare for preincrement:
      while ( ++cache < cacheEnd )
      {
        if ( *cache )
          morkAtomRowMap::SlotStrongAtomRowMap(0, ev, cache);
      }
      
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
  if ( this->IsRowSpaceClean() )
    this->MaybeDirtyStoreAndSpace();
  
  mork_num outSlots = mRowSpace_Rows.MapFill();

#ifdef MORK_ENABLE_ZONE_ARENAS
  MORK_USED_2(ev, ioPool);
  return 0;
#else /*MORK_ENABLE_ZONE_ARENAS*/
  morkZone* zone = &mSpace_Store->mStore_Zone;
  morkRow* r = 0; // old key row in the map
  mork_change* c = 0;

#ifdef MORK_ENABLE_PROBE_MAPS
  morkRowProbeMapIter i(ev, &mRowSpace_Rows);
#else /*MORK_ENABLE_PROBE_MAPS*/
  morkRowMapIter i(ev, &mRowSpace_Rows);
#endif /*MORK_ENABLE_PROBE_MAPS*/
  
  for ( c = i.FirstRow(ev, &r); c && ev->Good();
        c = i.NextRow(ev, &r) )
  {
    if ( r )
    {
      if ( r->IsRow() )
      {
        if ( r->mRow_Object )
        {
          morkRowObject::SlotWeakRowObject((morkRowObject*) 0, ev,
            &r->mRow_Object);
        }
        ioPool->ZapRow(ev, r, zone);
      }
      else
        r->NonRowTypeWarning(ev);
    }
    else
      ev->NilPointerError();
    
#ifdef MORK_ENABLE_PROBE_MAPS
    // cut nothing from the map
#else /*MORK_ENABLE_PROBE_MAPS*/
    i.CutHereRow(ev, /*key*/ (morkRow**) 0);
#endif /*MORK_ENABLE_PROBE_MAPS*/
  }
#endif /*MORK_ENABLE_ZONE_ARENAS*/
  
  
  return outSlots;
}

morkTable*
morkRowSpace::FindTableByKind(morkEnv* ev, mork_kind inTableKind)
{
  if ( inTableKind )
  {
#ifdef MORK_BEAD_OVER_NODE_MAPS

    morkTableMapIter i(ev, &mRowSpace_Tables);
    morkTable* table = i.FirstTable(ev);
    for ( ; table && ev->Good(); table = i.NextTable(ev) )
          
#else /*MORK_BEAD_OVER_NODE_MAPS*/
    mork_tid* key = 0; // nil pointer to suppress key access
    morkTable* table = 0; // old table in the map

    mork_change* c = 0;
    morkTableMapIter i(ev, &mRowSpace_Tables);
    for ( c = i.FirstTable(ev, key, &table); c && ev->Good();
          c = i.NextTable(ev, key, &table) )
#endif /*MORK_BEAD_OVER_NODE_MAPS*/
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
morkRowSpace::NewTableWithTid(morkEnv* ev, mork_tid inTid,
  mork_kind inTableKind,
  const mdbOid* inOptionalMetaRowOid) // can be nil to avoid specifying 
{
  morkTable* outTable = 0;
  morkStore* store = mSpace_Store;
  
  if ( inTableKind && store )
  {
    mdb_bool mustBeUnique = morkBool_kFalse;
    nsIMdbHeap* heap = store->mPort_Heap;
    morkTable* table = new(*heap, ev)
      morkTable(ev, morkUsage::kHeap, heap, store, heap, this,
        inOptionalMetaRowOid, inTid, inTableKind, mustBeUnique);
    if ( table )
    {
      if ( mRowSpace_Tables.AddTable(ev, table) )
      {
        outTable = table;
        if ( mRowSpace_NextTableId <= inTid )
          mRowSpace_NextTableId = inTid + 1;
      }
      table->CutStrongRef(ev); // always cut ref; AddTable() adds its own
        
      if ( this->IsRowSpaceClean() && store->mStore_CanDirty )
        this->MaybeDirtyStoreAndSpace(); // morkTable does already

    }
  }
  else if ( store )
    this->ZeroKindError(ev);
  else
    this->NilSpaceStoreError(ev);
    
  return outTable;
}

morkTable*
morkRowSpace::NewTable(morkEnv* ev, mork_kind inTableKind,
  mdb_bool inMustBeUnique,
  const mdbOid* inOptionalMetaRowOid) // can be nil to avoid specifying 
{
  morkTable* outTable = 0;
  morkStore* store = mSpace_Store;
  
  if ( inTableKind && store )
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
            inOptionalMetaRowOid, id, inTableKind, inMustBeUnique);
        if ( table )
        {
          if ( mRowSpace_Tables.AddTable(ev, table) )
            outTable = table;
          else
            table->CutStrongRef(ev);

          if ( this->IsRowSpaceClean() && store->mStore_CanDirty )
            this->MaybeDirtyStoreAndSpace(); // morkTable does already
        }
      }
    }
  }
  else if ( store )
    this->ZeroKindError(ev);
  else
    this->NilSpaceStoreError(ev);
    
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
  oid.mOid_Scope = this->SpaceScope();
  
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

morkAtomRowMap*
morkRowSpace::make_index(morkEnv* ev, mork_column inCol)
{
  morkAtomRowMap* outMap = 0;
  nsIMdbHeap* heap = mRowSpace_SlotHeap;
  if ( heap ) // have expected heap for allocations?
  {
    morkAtomRowMap* map = new(*heap, ev)
      morkAtomRowMap(ev, morkUsage::kHeap, heap, heap, inCol);
    
    if ( map ) // able to create new map index?
    {
      if ( ev->Good() ) // no errors during construction?
      {
#ifdef MORK_ENABLE_PROBE_MAPS
        morkRowProbeMapIter i(ev, &mRowSpace_Rows);
#else /*MORK_ENABLE_PROBE_MAPS*/
        morkRowMapIter i(ev, &mRowSpace_Rows);
#endif /*MORK_ENABLE_PROBE_MAPS*/
        mork_change* c = 0;
        morkRow* row = 0;
        mork_aid aidKey = 0;
        
        for ( c = i.FirstRow(ev, &row); c && ev->Good();
              c = i.NextRow(ev, &row) ) // another row in space?
        {
          aidKey = row->GetCellAtomAid(ev, inCol);
          if ( aidKey ) // row has indexed attribute?
            map->AddAid(ev, aidKey, row); // include in map
        }
      }
      if ( ev->Good() ) // no errors constructing index?
        outMap = map; // return from function
      else
        map->CutStrongRef(ev); // discard map on error
    }
  }
  else
    ev->NilPointerError();
  
  return outMap;
}

morkAtomRowMap*
morkRowSpace::ForceMap(morkEnv* ev, mork_column inCol)
{
  morkAtomRowMap* outMap = this->FindMap(ev, inCol);
  
  if ( !outMap && ev->Good() ) // no such existing index?
  {
    if ( mRowSpace_IndexCount < morkRowSpace_kMaxIndexCount )
    {
      morkAtomRowMap* map = this->make_index(ev, inCol);
      if ( map ) // created a new index for col?
      {
        mork_count wrap = 0; // count times wrap-around occurs
        morkAtomRowMap** slot = mRowSpace_IndexCache; // table
        morkAtomRowMap** end = slot + morkRowSpace_kPrimeCacheSize;
        slot += ( inCol % morkRowSpace_kPrimeCacheSize ); // hash
        while ( *slot ) // empty slot not yet found?
        {
          if ( ++slot >= end ) // wrap around?
          {
            slot = mRowSpace_IndexCache; // back to table start
            if ( ++wrap > 1 ) // wrapped more than once?
            {
              ev->NewError("no free cache slots"); // disaster
              break; // end while loop
            }
          }
        }
        if ( ev->Good() ) // everything went just fine?
        {
          ++mRowSpace_IndexCount; // note another new map
          *slot = map; // install map in the hash table
          outMap = map; // return the new map from function
        }
        else
          map->CutStrongRef(ev); // discard map on error
      }
    }
    else
      ev->NewError("too many indexes"); // why so many indexes?
  }
  return outMap;
}

morkAtomRowMap*
morkRowSpace::FindMap(morkEnv* ev, mork_column inCol)
{
  if ( mRowSpace_IndexCount && ev->Good() )
  {
    mork_count wrap = 0; // count times wrap-around occurs
    morkAtomRowMap** slot = mRowSpace_IndexCache; // table
    morkAtomRowMap** end = slot + morkRowSpace_kPrimeCacheSize;
    slot += ( inCol % morkRowSpace_kPrimeCacheSize ); // hash
    morkAtomRowMap* map = *slot;
    while ( map ) // another used slot to examine?
    {
      if ( inCol == map->mAtomRowMap_IndexColumn ) // found col?
        return map;
      if ( ++slot >= end ) // wrap around?
      {
        slot = mRowSpace_IndexCache;
        if ( ++wrap > 1 ) // wrapped more than once?
          return (morkAtomRowMap*) 0; // stop searching
      }
      map = *slot;
    }
  }
  return (morkAtomRowMap*) 0;
}

morkRow*
morkRowSpace::FindRow(morkEnv* ev, mork_column inCol, const mdbYarn* inYarn)
{
  morkRow* outRow = 0;
  
  morkAtom* atom = mSpace_Store->YarnToAtom(ev, inYarn);
  if ( atom ) // have or created an atom corresponding to input yarn?
  {
    mork_aid atomAid = atom->GetBookAtomAid();
    if ( atomAid ) // atom has an identity for use in hash table?
    {
      morkAtomRowMap* map = this->ForceMap(ev, inCol);
      if ( map ) // able to find or create index for col?
      {
        outRow = map->GetAid(ev, atomAid); // search for row
      }
    }
  }
  
  return outRow;
}

morkRow*
morkRowSpace::NewRowWithOid(morkEnv* ev, const mdbOid* inOid)
{
  morkRow* outRow = mRowSpace_Rows.GetOid(ev, inOid);
  MORK_ASSERT(outRow==0);
  if ( !outRow && ev->Good() )
  {
    morkStore* store = mSpace_Store;
    if ( store )
    {
      morkPool* pool = this->GetSpaceStorePool();
      morkRow* row = pool->NewRow(ev, &store->mStore_Zone);
      if ( row )
      {
        row->InitRow(ev, inOid, this, /*length*/ 0, pool);
        
        if ( ev->Good() && mRowSpace_Rows.AddRow(ev, row) )
        {
          outRow = row;
          mork_rid rid = inOid->mOid_Id;
          if ( mRowSpace_NextRowId <= rid )
            mRowSpace_NextRowId = rid + 1;
        }
        else
          pool->ZapRow(ev, row, &store->mStore_Zone);

        if ( this->IsRowSpaceClean() && store->mStore_CanDirty )
          this->MaybeDirtyStoreAndSpace(); // InitRow() does already
      }
    }
    else
      this->NilSpaceStoreError(ev);
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
      morkStore* store = mSpace_Store;
      if ( store )
      {
        mdbOid oid;
        oid.mOid_Scope = this->SpaceScope();
        oid.mOid_Id = id;
        morkPool* pool = this->GetSpaceStorePool();
        morkRow* row = pool->NewRow(ev, &store->mStore_Zone);
        if ( row )
        {
          row->InitRow(ev, &oid, this, /*length*/ 0, pool);
          
          if ( ev->Good() && mRowSpace_Rows.AddRow(ev, row) )
            outRow = row;
          else
            pool->ZapRow(ev, row, &store->mStore_Zone);

          if ( this->IsRowSpaceClean() && store->mStore_CanDirty )
            this->MaybeDirtyStoreAndSpace(); // InitRow() does already
        }
      }
      else
        this->NilSpaceStoreError(ev);
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
