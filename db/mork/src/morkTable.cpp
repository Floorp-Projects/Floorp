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

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKROWSPACE_
#include "morkRowSpace.h"
#endif

#ifndef _MORKARRAY_
#include "morkArray.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _ORKINTABLE_
#include "orkinTable.h"
#endif

#ifndef _MORKTABLEROWCURSOR_
#include "morkTableRowCursor.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkTable::CloseMorkNode(morkEnv* ev) /*i*/ // CloseTable() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseTable(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkTable::~morkTable() /*i*/ // assert CloseTable() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
  MORK_ASSERT(mTable_Store==0);
  MORK_ASSERT(mTable_RowSpace==0);
}

/*public non-poly*/
morkTable::morkTable(morkEnv* ev, /*i*/
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
  morkStore* ioStore, nsIMdbHeap* ioSlotHeap, morkRowSpace* ioRowSpace,
  const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
  mork_tid inTid, mork_kind inKind, mork_bool inMustBeUnique)
: morkObject(ev, inUsage, ioHeap, (mork_color) inTid, (morkHandle*) 0)
, mTable_Store( 0 )
, mTable_RowSpace( 0 )
, mTable_MetaRow( 0 )

, mTable_RowMap( 0 )
// , mTable_RowMap(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioSlotHeap,
//   morkTable_kStartRowMapSlotCount)
, mTable_RowArray(ev, morkUsage::kMember, (nsIMdbHeap*) 0,
  morkTable_kStartRowArraySize, ioSlotHeap)
  
, mTable_ChangeList()
, mTable_ChangesCount( 0 )
, mTable_ChangesMax( 3 ) // any very small number greater than zero

, mTable_Kind( inKind )

, mTable_Flags( 0 )
, mTable_Priority( morkPriority_kLo ) // NOT high priority
, mTable_GcUses( 0 )
, mTable_Pad( 0 )
{
  this->mLink_Next = 0;
  this->mLink_Prev = 0;
  
  if ( ev->Good() )
  {
    if ( ioStore && ioSlotHeap && ioRowSpace )
    {
      if ( inKind )
      {
        if ( inMustBeUnique )
          this->SetTableUnique();
        morkStore::SlotWeakStore(ioStore, ev, &mTable_Store);
        morkRowSpace::SlotWeakRowSpace(ioRowSpace, ev, &mTable_RowSpace);
        if ( inOptionalMetaRowOid )
          mTable_MetaRowOid = *inOptionalMetaRowOid;
        else
        {
          mTable_MetaRowOid.mOid_Scope = 0;
          mTable_MetaRowOid.mOid_Id = morkRow_kMinusOneRid;
        }
        if ( ev->Good() )
        {
          if ( this->MaybeDirtySpaceStoreAndTable() )
            this->SetTableRewrite(); // everything is dirty
            
          mNode_Derived = morkDerived_kTable;
        }
        this->MaybeDirtySpaceStoreAndTable(); // new table might dirty store
      }
      else
        ioRowSpace->ZeroKindError(ev);
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkTable::CloseTable(morkEnv* ev) /*i*/ // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      morkRowMap::SlotStrongRowMap((morkRowMap*) 0, ev, &mTable_RowMap);
      // mTable_RowMap.CloseMorkNode(ev);
      mTable_RowArray.CloseMorkNode(ev);
      morkStore::SlotWeakStore((morkStore*) 0, ev, &mTable_Store);
      morkRowSpace::SlotWeakRowSpace((morkRowSpace*) 0,
         ev, &mTable_RowSpace);
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

mork_u2
morkTable::AddTableGcUse(morkEnv* ev)
{
  MORK_USED_1(ev); 
  if ( mTable_GcUses < morkTable_kMaxTableGcUses ) // not already maxed out?
    ++mTable_GcUses;
    
  return mTable_GcUses;
}

mork_u2
morkTable::CutTableGcUse(morkEnv* ev)
{
  if ( mTable_GcUses ) // any outstanding uses to cut?
  {
    if ( mTable_GcUses < morkTable_kMaxTableGcUses ) // not frozen at max?
      --mTable_GcUses;
  }
  else
    this->TableGcUsesUnderflowWarning(ev);
    
  return mTable_GcUses;
}

// table dirty handling more complex thatn morkNode::SetNodeDirty() etc.

void morkTable::SetTableClean(morkEnv* ev)
{
  if ( mTable_ChangeList.HasListMembers() )
  {
    nsIMdbHeap* heap = mTable_Store->mPort_Heap;
    mTable_ChangeList.CutAndZapAllListMembers(ev, heap); // forget changes
  }
  mTable_ChangesCount = 0;
  
  mTable_Flags = 0;
  this->SetNodeClean();
}

// notifications regarding table changes:

void morkTable::NoteTableMoveRow(morkEnv* ev, morkRow* ioRow, mork_pos inPos)
{
  nsIMdbHeap* heap = mTable_Store->mPort_Heap;
  if ( this->IsTableRewrite() || this->HasChangeOverflow() )
    this->NoteTableSetAll(ev);
  else
  {
    morkTableChange* tableChange = new(*heap, ev)
      morkTableChange(ev, ioRow, inPos);
    if ( tableChange )
    {
      if ( ev->Good() )
      {
        mTable_ChangeList.PushTail(tableChange);
        ++mTable_ChangesCount;
      }
      else
      {
        tableChange->ZapOldNext(ev, heap);
        this->SetTableRewrite(); // just plan to write all table rows
      }
    }
  }
}

void morkTable::note_row_move(morkEnv* ev, morkRow* ioRow, mork_pos inNewPos)
{
  if ( this->IsTableRewrite() || this->HasChangeOverflow() )
    this->NoteTableSetAll(ev);
  else
  {
    nsIMdbHeap* heap = mTable_Store->mPort_Heap;
    morkTableChange* tableChange = new(*heap, ev)
      morkTableChange(ev, ioRow, inNewPos);
    if ( tableChange )
    {
      if ( ev->Good() )
      {
        mTable_ChangeList.PushTail(tableChange);
        ++mTable_ChangesCount;
      }
      else
      {
        tableChange->ZapOldNext(ev, heap);
        this->NoteTableSetAll(ev);
      }
    }
  }
}

void morkTable::note_row_change(morkEnv* ev, mork_change inChange,
  morkRow* ioRow)
{
  if ( this->IsTableRewrite() || this->HasChangeOverflow() )
    this->NoteTableSetAll(ev);
  else
  {
    nsIMdbHeap* heap = mTable_Store->mPort_Heap;
    morkTableChange* tableChange = new(*heap, ev)
      morkTableChange(ev, inChange, ioRow);
    if ( tableChange )
    {
      if ( ev->Good() )
      {
        mTable_ChangeList.PushTail(tableChange);
        ++mTable_ChangesCount;
      }
      else
      {
        tableChange->ZapOldNext(ev, heap);
        this->NoteTableSetAll(ev);
      }
    }
  }
}

void morkTable::NoteTableSetAll(morkEnv* ev)
{
  if ( mTable_ChangeList.HasListMembers() )
  {
    nsIMdbHeap* heap = mTable_Store->mPort_Heap;
    mTable_ChangeList.CutAndZapAllListMembers(ev, heap); // forget changes
  }
  mTable_ChangesCount = 0;
  this->SetTableRewrite();
}

/*static*/ void
morkTable::TableGcUsesUnderflowWarning(morkEnv* ev)
{
  ev->NewWarning("mTable_GcUses underflow");
}

/*static*/ void
morkTable::NonTableTypeError(morkEnv* ev)
{
  ev->NewError("non morkTable");
}

/*static*/ void
morkTable::NonTableTypeWarning(morkEnv* ev)
{
  ev->NewWarning("non morkTable");
}

/*static*/ void
morkTable::NilRowSpaceError(morkEnv* ev)
{
  ev->NewError("nil mTable_RowSpace");
}

mork_bool morkTable::MaybeDirtySpaceStoreAndTable()
{
  morkRowSpace* rowSpace = mTable_RowSpace;
  if ( rowSpace )
  {
    morkStore* store = rowSpace->mSpace_Store;
    if ( store && store->mStore_CanDirty )
    {
      store->SetStoreDirty();
      rowSpace->mSpace_CanDirty = morkBool_kTrue;
    }
    
    if ( rowSpace->mSpace_CanDirty ) // first time being dirtied?
    {
      if ( this->IsTableClean() )
      {
        mork_count rowCount = this->GetRowCount();
        mork_count oneThird = rowCount / 4; // one third of rows
        if ( oneThird > 0x07FFF ) // more than half max u2?
          oneThird = 0x07FFF;
          
        mTable_ChangesMax = (mork_u2) oneThird;
      }
      this->SetTableDirty();
      rowSpace->SetRowSpaceDirty();
      
      return morkBool_kTrue;
    }
  }
  return morkBool_kFalse;
}

morkRow*
morkTable::GetMetaRow(morkEnv* ev, const mdbOid* inOptionalMetaRowOid)
{
  morkRow* outRow = mTable_MetaRow;
  if ( !outRow )
  {
    morkStore* store = mTable_Store;
    mdbOid* oid = &mTable_MetaRowOid;
    if ( inOptionalMetaRowOid && !oid->mOid_Scope )
      *oid = *inOptionalMetaRowOid;
      
    if ( oid->mOid_Scope ) // oid already recorded in table?
      outRow = store->OidToRow(ev, oid);
    else
    {
      outRow = store->NewRow(ev, morkStore_kMetaScope);
      if ( outRow ) // need to record new oid in table?
        *oid = outRow->mRow_Oid;
    }
    mTable_MetaRow = outRow;
    if ( outRow ) // need to note another use of this row?
    {
      outRow->AddRowGcUse(ev);

      this->SetTableNewMeta();
      if ( this->IsTableClean() ) // catch dirty status of meta row?
        this->MaybeDirtySpaceStoreAndTable();
    }
  }
  
  return outRow;
}

void
morkTable::GetTableOid(morkEnv* ev, mdbOid* outOid)
{
  morkRowSpace* space = mTable_RowSpace;
  if ( space )
  {
    outOid->mOid_Scope = space->SpaceScope();
    outOid->mOid_Id = this->TableId();
  }
  else
    this->NilRowSpaceError(ev);
}

nsIMdbTable*
morkTable::AcquireTableHandle(morkEnv* ev)
{
  nsIMdbTable* outTable = 0;
  orkinTable* t = (orkinTable*) mObject_Handle;
  if ( t ) // have an old handle?
    t->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    t = orkinTable::MakeTable(ev, this);
    mObject_Handle = t;
  }
  if ( t )
    outTable = t;
  return outTable;
}

mork_pos
morkTable::ArrayHasOid(morkEnv* ev, const mdbOid* inOid)
{
  MORK_USED_1(ev); 
  mork_count count = mTable_RowArray.mArray_Fill;
  mork_pos pos = -1;
  while ( ++pos < (mork_pos)count )
  {
    morkRow* row = (morkRow*) mTable_RowArray.At(pos);
    MORK_ASSERT(row);
    if ( row && row->EqualOid(inOid) )
    {
      return pos;
    }
  }
  return -1;
}

mork_bool
morkTable::MapHasOid(morkEnv* ev, const mdbOid* inOid)
{
  if ( mTable_RowMap )
    return ( mTable_RowMap->GetOid(ev, inOid) != 0 );
  else
    return ( ArrayHasOid(ev, inOid) >= 0 );
}

void morkTable::build_row_map(morkEnv* ev)
{
  morkRowMap* map = mTable_RowMap;
  if ( !map )
  {
    mork_count count = mTable_RowArray.mArray_Fill + 3;
    nsIMdbHeap* heap = mTable_Store->mPort_Heap;
    map = new(*heap, ev) morkRowMap(ev, morkUsage::kHeap, heap, heap, count);
    if ( map )
    {
      if ( ev->Good() )
      {
        mTable_RowMap = map; // put strong ref here
        count = mTable_RowArray.mArray_Fill;
        mork_pos pos = -1;
        while ( ++pos < (mork_pos)count )
        {
          morkRow* row = (morkRow*) mTable_RowArray.At(pos);
          if ( row && row->IsRow() )
            map->AddRow(ev, row);
          else
            row->NonRowTypeError(ev);
        }
      }
      else
        map->CutStrongRef(ev);
    }
  }
}

morkRow* morkTable::find_member_row(morkEnv* ev, morkRow* ioRow)
{
  if ( mTable_RowMap )
    return mTable_RowMap->GetRow(ev, ioRow);
  else
  {
    mork_count count = mTable_RowArray.mArray_Fill;
    mork_pos pos = -1;
    while ( ++pos < (mork_pos)count )
    {
      morkRow* row = (morkRow*) mTable_RowArray.At(pos);
      if ( row == ioRow )
        return row;
    }
  }
  return (morkRow*) 0;
}

mork_pos
morkTable::MoveRow(morkEnv* ev, morkRow* ioRow, // change row position
  mork_pos inHintFromPos, // suggested hint regarding start position
  mork_pos inToPos) // desired new position for row ioRow
  // MoveRow() returns the actual position of ioRow afterwards; this
  // position is -1 if and only if ioRow was not found as a member.     
{
  mork_pos outPos = -1; // means ioRow was not a table member
  mork_bool canDirty = ( this->IsTableClean() )?
    this->MaybeDirtySpaceStoreAndTable() : morkBool_kTrue;
  
  morkRow** rows = (morkRow**) mTable_RowArray.mArray_Slots;
  mork_count count = mTable_RowArray.mArray_Fill;
  if ( count && rows && ev->Good() ) // any members at all? no errors?
  {
    mork_pos lastPos = count - 1; // index of last row slot
      
    if ( inToPos > lastPos ) // beyond last used array slot?
      inToPos = lastPos; // put row into last available slot
    else if ( inToPos < 0 ) // before first usable slot?
      inToPos = 0; // put row in very first slow
      
    if ( inHintFromPos > lastPos ) // beyond last used array slot?
      inHintFromPos = lastPos; // seek row in last available slot
    else if ( inHintFromPos < 0 ) // before first usable slot?
      inHintFromPos = 0; // seek row in very first slow

    morkRow** fromSlot = 0; // becomes nonzero of ioRow is ever found
    morkRow** rowsEnd = rows + count; // one past last used array slot
    
    if ( inHintFromPos <= 0 ) // start of table? just scan for row?
    {
      morkRow** cursor = rows - 1; // before first array slot
      while ( ++cursor < rowsEnd )
      {
        if ( *cursor == ioRow )
        {
          fromSlot = cursor;
          break; // end while loop
        }
      }
    }
    else // search near the start position and work outwards
    {
      morkRow** lo = rows + inHintFromPos; // lowest search point
      morkRow** hi = lo; // highest search point starts at lowest point
      
      // Seek ioRow in spiral widening search below and above inHintFromPos.
      // This is faster when inHintFromPos is at all accurate, but is slower
      // than a straightforward scan when inHintFromPos is nearly random.
      
      while ( lo >= rows || hi < rowsEnd ) // keep searching?
      {
        if ( lo >= rows ) // low direction search still feasible?
        {
          if ( *lo == ioRow ) // actually found the row?
          {
            fromSlot = lo;
            break; // end while loop
          }
          --lo; // advance further lower
        }
        if ( hi < rowsEnd ) // high direction search still feasible?
        {
          if ( *hi == ioRow ) // actually found the row?
          {
            fromSlot = hi;
            break; // end while loop
          }
          ++hi; // advance further higher
        }
      }
    }
    
    if ( fromSlot ) // ioRow was found as a table member?
    {
      outPos = fromSlot - rows; // actual position where row was found
      if ( outPos != inToPos ) // actually need to move this row?
      {
        morkRow** toSlot = rows + inToPos; // slot where row must go
        
        ++mTable_RowArray.mArray_Seed; // we modify the array now:
        
        if ( fromSlot < toSlot ) // row is moving upwards?
        {
          morkRow** up = fromSlot; // leading pointer going upward
          while ( ++up <= toSlot ) // have not gone above destination?
          {
            *fromSlot = *up; // shift down one
            fromSlot = up; // shift trailing pointer up
          }
        }
        else // ( fromSlot > toSlot ) // row is moving downwards
        {
          morkRow** down = fromSlot; // leading pointer going downward
          while ( --down >= toSlot ) // have not gone below destination?
          {
            *fromSlot = *down; // shift up one
            fromSlot = down; // shift trailing pointer
          }
        }
        *toSlot = ioRow;
        outPos = inToPos; // okay, we actually moved the row here

        if ( canDirty )
          this->note_row_move(ev, ioRow, inToPos);
      }
    }
  }
  return outPos;
}

mork_bool
morkTable::AddRow(morkEnv* ev, morkRow* ioRow)
{
  morkRow* row = this->find_member_row(ev, ioRow);
  if ( !row && ev->Good() )
  {
    mork_bool canDirty = ( this->IsTableClean() )?
      this->MaybeDirtySpaceStoreAndTable() : morkBool_kTrue;
      
    mork_pos pos = mTable_RowArray.AppendSlot(ev, ioRow);
    if ( ev->Good() && pos >= 0 )
    {
      ioRow->AddRowGcUse(ev);
      if ( mTable_RowMap )
      {
        if ( mTable_RowMap->AddRow(ev, ioRow) )
        {
          // okay, anything else?
        }
        else
          mTable_RowArray.CutSlot(ev, pos);
      }
      else if ( mTable_RowArray.mArray_Fill >= morkTable_kMakeRowMapThreshold )
        this->build_row_map(ev);

      if ( canDirty && ev->Good() )
        this->NoteTableAddRow(ev, ioRow);
    }
  }
  return ev->Good();
}

mork_bool
morkTable::CutRow(morkEnv* ev, morkRow* ioRow)
{
  morkRow* row = this->find_member_row(ev, ioRow);
  if ( row )
  {
    mork_bool canDirty = ( this->IsTableClean() )?
      this->MaybeDirtySpaceStoreAndTable() : morkBool_kTrue;
      
    mork_count count = mTable_RowArray.mArray_Fill;
    morkRow** rowSlots = (morkRow**) mTable_RowArray.mArray_Slots;
    if ( rowSlots ) // array has vector as expected?
    {
      mork_pos pos = -1;
      morkRow** end = rowSlots + count;
      morkRow** slot = rowSlots - 1; // prepare for preincrement:
      while ( ++slot < end ) // another slot to check?
      {
        if ( *slot == row ) // found the slot containing row?
        {
          pos = slot - rowSlots; // record absolute position
          break; // end while loop
        }
      }
      if ( pos >= 0 ) // need to cut if from the array?
        mTable_RowArray.CutSlot(ev, pos);
      else
        ev->NewWarning("row not found in array");
    }
    else
      mTable_RowArray.NilSlotsAddressError(ev);
      
    if ( mTable_RowMap )
      mTable_RowMap->CutRow(ev, ioRow);

    if ( canDirty )
      this->NoteTableCutRow(ev, ioRow);

    if ( ioRow->CutRowGcUse(ev) == 0 )
      ioRow->OnZeroRowGcUse(ev);
  }
  return ev->Good();
}


mork_bool
morkTable::CutAllRows(morkEnv* ev)
{
  if ( this->MaybeDirtySpaceStoreAndTable() )
  {
    this->SetTableRewrite(); // everything is dirty
    this->NoteTableSetAll(ev);
  }
    
  if ( ev->Good() )
  {
    mTable_RowArray.CutAllSlots(ev);
    if ( mTable_RowMap )
    {
      morkRowMapIter i(ev, mTable_RowMap);
      mork_change* c = 0;
      morkRow* r = 0;
      
      for ( c = i.FirstRow(ev, &r); c;  c = i.NextRow(ev, &r) )
      {
        if ( r )
        {
          if ( r->CutRowGcUse(ev) == 0 )
            r->OnZeroRowGcUse(ev);
            
          i.CutHereRow(ev, (morkRow**) 0);
        }
        else
          ev->NewWarning("nil row in table map");
      }
    }
  }
  return ev->Good();
}

morkTableRowCursor*
morkTable::NewTableRowCursor(morkEnv* ev, mork_pos inRowPos)
{
  morkTableRowCursor* outCursor = 0;
  if ( ev->Good() )
  {
    nsIMdbHeap* heap = mTable_Store->mPort_Heap;
    morkTableRowCursor* cursor = new(*heap, ev)
      morkTableRowCursor(ev, morkUsage::kHeap, heap, this, inRowPos);
    if ( cursor )
    {
      if ( ev->Good() )
        outCursor = cursor;
      else
        cursor->CutStrongRef(ev);
    }
  }
  return outCursor;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

morkTableChange::morkTableChange(morkEnv* ev, mork_change inChange,
  morkRow* ioRow)
// use this constructor for inChange == morkChange_kAdd or morkChange_kCut
: morkNext()
, mTableChange_Row( ioRow )
, mTableChange_Pos( morkTableChange_kNone )
{
  if ( ioRow )
  {
    if ( ioRow->IsRow() )
    {
      if ( inChange == morkChange_kAdd )
        mTableChange_Pos = morkTableChange_kAdd;
      else if ( inChange == morkChange_kCut )
        mTableChange_Pos = morkTableChange_kCut;
      else
        this->UnknownChangeError(ev);
    }
    else
      ioRow->NonRowTypeError(ev);
  }
  else
    ev->NilPointerError();
}

morkTableChange::morkTableChange(morkEnv* ev, morkRow* ioRow, mork_pos inPos)
// use this constructor when the row is moved
: morkNext()
, mTableChange_Row( ioRow )
, mTableChange_Pos( inPos )
{
  if ( ioRow )
  {
    if ( ioRow->IsRow() )
    {
      if ( inPos < 0 )
        this->NegativeMovePosError(ev);
    }
    else
      ioRow->NonRowTypeError(ev);
  }
  else
    ev->NilPointerError();
}

void morkTableChange::UnknownChangeError(morkEnv* ev) const
// morkChange_kAdd or morkChange_kCut
{
  ev->NewError("mTableChange_Pos neither kAdd nor kCut");
}

void morkTableChange::NegativeMovePosError(morkEnv* ev) const
// move must be non-neg position
{
  ev->NewError("negative mTableChange_Pos for row move");
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


morkTableMap::~morkTableMap()
{
}

morkTableMap::morkTableMap(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
#ifdef MORK_BEAD_OVER_NODE_MAPS
  : morkBeadMap(ev, inUsage, ioHeap, ioSlotHeap)
#else /*MORK_BEAD_OVER_NODE_MAPS*/
  : morkNodeMap(ev, inUsage, ioHeap, ioSlotHeap)
#endif /*MORK_BEAD_OVER_NODE_MAPS*/
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kTableMap;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

