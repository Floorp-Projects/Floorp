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

#ifndef _MORKROW_
#include "morkRow.h"
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

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

#ifndef _MORKCELLOBJECT_
#include "morkCellObject.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKROWCELLCURSOR_
#include "morkRowCellCursor.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

mork_u2
morkRow::AddTableUse(morkEnv* ev)
{
  if ( mRow_TableUses < morkRow_kMaxTableUses ) // not already maxed out?
    ++mRow_TableUses;
    
  return mRow_TableUses;
}

mork_u2
morkRow::CutTableUse(morkEnv* ev)
{
  if ( mRow_TableUses ) // any outstanding uses to cut?
  {
    if ( mRow_TableUses < morkRow_kMaxTableUses ) // not frozen at max?
      --mRow_TableUses;
  }
  else
    this->TableUsesUnderflowWarning(ev);
    
  return mRow_TableUses;
}

/*static*/ void
morkRow::TableUsesUnderflowWarning(morkEnv* ev)
{
  ev->NewWarning("mRow_TableUses underflow");
}


/*static*/ void
morkRow::NonRowTypeError(morkEnv* ev)
{
  ev->NewError("non morkRow");
}

/*static*/ void
morkRow::NonRowTypeWarning(morkEnv* ev)
{
  ev->NewWarning("non morkRow");
}

/*static*/ void
morkRow::LengthBeyondMaxError(morkEnv* ev)
{
  ev->NewError("mRow_Length over max");
}

/*static*/ void
morkRow::ZeroColumnError(morkEnv* ev)
{
  ev->NewError(" zero mork_column");
}

/*static*/ void
morkRow::NilCellsError(morkEnv* ev)
{
  ev->NewError("nil mRow_Cells");
}

void
morkRow::InitRow(morkEnv* ev, const mdbOid* inOid, morkRowSpace* ioSpace,
  mork_size inLength, morkPool* ioPool)
  // if inLength is nonzero, cells will be allocated from ioPool
{
  if ( ioSpace && ioPool && inOid )
  {
    if ( inLength <= morkRow_kMaxLength )
    {
      if ( inOid->mOid_Id != morkRow_kMinusOneRid )
      {
        mRow_Space = ioSpace;
        mRow_Object = 0;
        mRow_Cells = 0;
        mRow_Oid = *inOid;

        mRow_Length = inLength;
        mRow_Seed = (mork_u2) this; // "random" assignment

        mRow_TableUses = 0;
        mRow_Load = morkLoad_kClean;
        mRow_Tag = morkRow_kTag;

        if ( inLength )
          mRow_Cells = ioPool->NewCells(ev, inLength);
      }
      else
        ioSpace->MinusOneRidError(ev);
    }
    else
      this->LengthBeyondMaxError(ev);
  }
  else
    ev->NilPointerError();
}

morkRowObject*
morkRow::GetRowObject(morkEnv* ev, morkStore* ioStore)
{
  morkRowObject* ro = mRow_Object;
  if ( !ro ) // need new row object?
  {
    nsIMdbHeap* heap = ioStore->mPort_Heap;
    ro = new (*heap, ev)
      morkRowObject(ev, morkUsage::kHeap, heap, this, ioStore);
    mRow_Object = ro;
  }
  return ro;
}

nsIMdbRow*
morkRow::AcquireRowHandle(morkEnv* ev, morkStore* ioStore)
{
  morkRowObject* object = this->GetRowObject(ev, ioStore);
  if ( object )
    return object->AcquireRowHandle(ev);
    
  return (nsIMdbRow*) 0;
}

nsIMdbCell*
morkRow::AcquireCellHandle(morkEnv* ev, morkCell* ioCell,
  mdb_column inCol, mork_pos inPos)
{
  nsIMdbHeap* heap = ev->mEnv_Heap;
  morkCellObject* cellObj = new (*heap, ev)
    morkCellObject(ev, morkUsage::kHeap, heap, this, ioCell, inCol, inPos);
  if ( cellObj )
    return cellObj->AcquireCellHandle(ev);
    
  return (nsIMdbCell*) 0;
}

mork_count
morkRow::CountOverlap(morkEnv* ev, morkCell* ioVector, mork_fill inFill)
  // Count cells in ioVector that change existing cells in this row when
  // ioVector is added to the row (as in TakeCells()).   This is the set
  // of cells with the same columns in ioVector and mRow_Cells, which do
  // not have exactly the same value in mCell_Atom, and which do not both
  // have change status equal to morkChange_kCut (because cutting a cut
  // cell still yields a cell that has been cut).  CountOverlap() also
  // modifies the change attribute of any cell in ioVector to kDup when
  // the change was previously kCut and the same column cell was found
  // in this row with change also equal to kCut; this tells callers later
  // they need not look for that cell in the row again on a second pass.
{
  mork_count outCount = 0;
  mork_pos pos = 0; // needed by GetCell()
  morkCell* cells = ioVector;
  morkCell* end = cells + inFill;
  --cells; // prepare for preincrement
  while ( ++cells < end && ev->Good() )
  {
    mork_column col = cells->GetColumn();
    
    morkCell* old = this->GetCell(ev, col, &pos);
    if ( old ) // same column?
    {
      mork_change newChg = cells->GetChange();
      mork_change oldChg = old->GetChange();
      if ( newChg != morkChange_kCut || oldChg != newChg ) // not cut+cut?
      {
        if ( cells->mCell_Atom != old->mCell_Atom ) // not same atom?
          ++outCount; // cells will replace old significantly when added
      }
      else
        cells->SetColumnAndChange(col, morkChange_kDup); // note dup status
    }
  }
  return outCount;
}

void
morkRow::MergeCells(morkEnv* ev, morkCell* ioVector,
  mork_fill inVecLength, mork_fill inOldRowFill, mork_fill inOverlap)
  // MergeCells() is the part of TakeCells() that does the insertion.
  // inOldRowFill is the old value of mRow_Length, and inOverlap is the
  // number of cells in the intersection that must be updated.
{
  morkCell* newCells = mRow_Cells + inOldRowFill; // 1st new cell in row
  morkCell* newEnd = newCells + mRow_Length; // one past last cell

  morkCell* srcCells = ioVector;
  morkCell* srcEnd = srcCells + inVecLength;
  
  --srcCells; // prepare for preincrement
  while ( ++srcCells < srcEnd && ev->Good() )
  {
    mork_change srcChg = srcCells->GetChange();
    if ( srcChg != morkChange_kDup ) // anything to be done?
    {
      morkCell* dstCell = 0;
      if ( inOverlap )
      {
        mork_pos pos = 0; // needed by GetCell()
        dstCell = this->GetCell(ev, srcCells->GetColumn(), &pos);
      }
      if ( dstCell )
      {
        --inOverlap; // one fewer intersections to resolve
        // swap the atoms in the cells to avoid ref counting here:
        morkAtom* dstAtom = dstCell->mCell_Atom;
        *dstCell = *srcCells; // bitwise copy, taking src atom
        srcCells->mCell_Atom = dstAtom; // forget cell ref, if any
      }
      else if ( newCells < newEnd ) // another new cell exists?
      {
        dstCell = newCells++; // alloc another new cell
        // take atom from source cell, transferring ref to this row:
        *dstCell = *srcCells; // bitwise copy, taking src atom
        srcCells->mCell_Atom = 0; // forget cell ref, if any
      }
      else // oops, we ran out...
        ev->NewError("out of new cells");
    }
  }
}

void
morkRow::TakeCells(morkEnv* ev, morkCell* ioVector, mork_fill inVecLength,
  morkStore* ioStore)
{
  if ( ioVector && inVecLength && ev->Good() )
  {
    ++mRow_Seed; // intend to change structure of mRow_Cells
    mork_pos length = (mork_pos) mRow_Length;
    
    mork_count overlap = this->CountOverlap(ev, ioVector, inVecLength);

    mork_size growth = inVecLength - overlap; // cells to add
    mork_size newLength = length + growth;
    
    if ( growth && ev->Good() ) // need to add any cells?
    {
      morkPool* pool = ioStore->StorePool();
      if ( !pool->AddRowCells(ev, this, length + growth) )
        ev->NewError("cannot take cells");
    }
    if ( ev->Good() )
    {
      if ( mRow_Length >= newLength )
        this->MergeCells(ev, ioVector, inVecLength, length, overlap);
      else
        ev->NewError("not enough new cells");
    }
  }
}

morkCell*
morkRow::NewCell(morkEnv* ev, mdb_column inColumn,
  mork_pos* outPos, morkStore* ioStore)
{
  ++mRow_Seed; // intend to change structure of mRow_Cells
  mork_pos length = (mork_pos) mRow_Length;
  *outPos = length;
  morkPool* pool = ioStore->StorePool();
  if ( pool->AddRowCells(ev, this, length + 1) )
  {
    morkCell* cell = mRow_Cells + length;
    cell->SetColumnAndChange(inColumn, morkChange_kAdd);
    return cell;
  }
    
  return (morkCell*) 0;
}

morkCell*
morkRow::CellAt(morkEnv* ev, mork_pos inPos) const
{
  morkCell* cells = mRow_Cells;
  if ( cells && inPos < mRow_Length )
  {
    return cells + inPos;
  }
  return (morkCell*) 0;
}

morkCell*
morkRow::GetCell(morkEnv* ev, mdb_column inColumn, mork_pos* outPos) const
{
  morkCell* cells = mRow_Cells;
  if ( cells )
  {
    morkCell* end = cells + mRow_Length;
    while ( cells < end )
    {
      mork_column col = cells->GetColumn();
      if ( col == inColumn ) // found the desired column?
      {
        *outPos = cells - mRow_Cells;
        return cells;
      }
      else
        ++cells;
    }
  }
  *outPos = -1;
  return (morkCell*) 0;
}

void
morkRow::EmptyAllCells(morkEnv* ev)
{
  morkCell* cells = mRow_Cells;
  if ( cells )
  {
    morkStore* store = this->GetRowSpaceStore(ev);
    if ( store )
    {
      morkPool* pool = store->StorePool();
      morkCell* end = cells + mRow_Length;
      --cells; // prepare for preincrement:
      while ( ++cells < end )
      {
        if ( cells->mCell_Atom )
          cells->SetAtom(ev, (morkAtom*) 0, pool);
      }
    }
  }
}

void
morkRow::AddRow(morkEnv* ev, const morkRow* inSourceRow)
{
  ev->StubMethodOnlyError();
  // $$$$$ need to iterate over inSourceRow cells adding them to this row.
  // When the atoms are book atoms, we can just incr the use count.
}

void
morkRow::OnZeroTableUse(morkEnv* ev)
// OnZeroTableUse() is called when CutTableUse() returns zero.
{
  // ev->NewWarning("need to implement OnZeroTableUse");
}

void
morkRow::DirtyAllRowContent(morkEnv* ev)
{
  this->SetRowDirty();

  morkCell* cells = mRow_Cells;
  if ( cells )
  {
    morkCell* end = cells + mRow_Length;
    --cells; // prepare for preincrement:
    while ( ++cells < end )
    {
      cells->SetCellDirty();
    }
  }
}

morkStore*
morkRow::GetRowSpaceStore(morkEnv* ev) const
{
  morkRowSpace* rowSpace = mRow_Space;
  if ( rowSpace )
  {
    morkStore* store = rowSpace->mSpace_Store;
    if ( store )
    {
      if ( store->IsStore() )
      {
        return store;
      }
      else
        store->NonStoreTypeError(ev);
    }
    else
      ev->NilPointerError();
  }
  else
    ev->NilPointerError();
    
  return (morkStore*) 0;
}

void morkRow::AddColumn(morkEnv* ev, mdb_column inColumn,
  const mdbYarn* inYarn, morkStore* ioStore)
{
  if ( ev->Good() )
  {
    mork_pos pos = -1;
    morkCell* cell = this->GetCell(ev, inColumn, &pos);
    if ( !cell ) // column does not yet exist?
      cell = this->NewCell(ev, inColumn, &pos, ioStore);
    else
      ++mRow_Seed;
    
    if ( cell )
    {
      // cell->SetYarn(ev, inYarn, ioStore);
      morkAtom* atom = ioStore->YarnToAtom(ev, inYarn);
      if ( atom )
        cell->SetAtom(ev, atom, ioStore->StorePool()); // refcounts atom
    }
  }
}

morkRowCellCursor*
morkRow::NewRowCellCursor(morkEnv* ev, mdb_pos inPos)
{
  morkRowCellCursor* outCursor = 0;
  if ( ev->Good() )
  {
    morkStore* store = this->GetRowSpaceStore(ev);
    if ( store )
    {
      morkRowObject* rowObj = this->GetRowObject(ev, store);
      if ( rowObj )
      {
        nsIMdbHeap* heap = store->mPort_Heap;
        morkRowCellCursor* cursor = new (*heap, ev)
          morkRowCellCursor(ev, morkUsage::kHeap, heap, rowObj);
         
        if ( cursor )
        {
          if ( ev->Good() )
          {
            cursor->mCursor_Pos = inPos;
            outCursor = cursor;
          }
          else
            cursor->CutStrongRef(ev);
        }
      }
    }
  }
  return outCursor;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
