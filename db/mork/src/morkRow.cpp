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

// notifications regarding row changes:

void morkRow::NoteRowAddCol(morkEnv* ev, mork_column inColumn)
{
  if ( !this->IsRowRewrite() )
  {
    mork_delta newDelta;
    morkDelta_Init(newDelta, inColumn, morkChange_kAdd);
    
    if ( newDelta != mRow_Delta ) // not repeating existing data?
    {
      if ( this->HasRowDelta() ) // already have one change recorded?
        this->SetRowRewrite(); // just plan to write all row cells
      else
        this->SetRowDelta(inColumn, morkChange_kAdd);
    }
  }
  else
    this->ClearRowDelta();
}

void morkRow::NoteRowCutCol(morkEnv* ev, mork_column inColumn)
{
  if ( !this->IsRowRewrite() )
  {
    mork_delta newDelta;
    morkDelta_Init(newDelta, inColumn, morkChange_kCut);
    
    if ( newDelta != mRow_Delta ) // not repeating existing data?
    {
      if ( this->HasRowDelta() ) // already have one change recorded?
        this->SetRowRewrite(); // just plan to write all row cells
      else
        this->SetRowDelta(inColumn, morkChange_kCut);
    }
  }
  else
    this->ClearRowDelta();
}

void morkRow::NoteRowSetCol(morkEnv* ev, mork_column inColumn)
{
  if ( !this->IsRowRewrite() )
  {
    if ( this->HasRowDelta() ) // already have one change recorded?
      this->SetRowRewrite(); // just plan to write all row cells
    else
      this->SetRowDelta(inColumn, morkChange_kSet);
  }
  else
    this->ClearRowDelta();
}

void morkRow::NoteRowSetAll(morkEnv* ev)
{
  this->SetRowRewrite(); // just plan to write all row cells
  this->ClearRowDelta();
}

mork_u2
morkRow::AddRowGcUse(morkEnv* ev)
{
  if ( this->IsRow() )
  {
    if ( mRow_GcUses < morkRow_kMaxGcUses ) // not already maxed out?
      ++mRow_GcUses;
  }
  else
    this->NonRowTypeError(ev);
    
  return mRow_GcUses;
}

mork_u2
morkRow::CutRowGcUse(morkEnv* ev)
{
  if ( this->IsRow() )
  {
    if ( mRow_GcUses ) // any outstanding uses to cut?
    {
      if ( mRow_GcUses < morkRow_kMaxGcUses ) // not frozen at max?
        --mRow_GcUses;
    }
    else
      this->GcUsesUnderflowWarning(ev);
  }
  else
    this->NonRowTypeError(ev);
    
  return mRow_GcUses;
}

/*static*/ void
morkRow::GcUsesUnderflowWarning(morkEnv* ev)
{
  ev->NewWarning("mRow_GcUses underflow");
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

        mRow_Length = (mork_u2) inLength;
        mRow_Seed = (mork_u2) (mork_ip) this; // "random" assignment

        mRow_GcUses = 0;
        mRow_Pad = 0;
        mRow_Flags = 0;
        mRow_Tag = morkRow_kTag;
        
        morkZone* zone = &ioSpace->mSpace_Store->mStore_Zone;

        if ( inLength )
          mRow_Cells = ioPool->NewCells(ev, inLength, zone);

        if ( this->MaybeDirtySpaceStoreAndRow() ) // new row might dirty store
        {
          this->SetRowRewrite();
          this->NoteRowSetAll(ev);
        }
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
morkRow::AcquireRowObject(morkEnv* ev, morkStore* ioStore)
{
  morkRowObject* ro = mRow_Object;
  if ( ro ) // need new row object?
    ro->AddStrongRef(ev);
  else
  {
    nsIMdbHeap* heap = ioStore->mPort_Heap;
    ro = new(*heap, ev)
      morkRowObject(ev, morkUsage::kHeap, heap, this, ioStore);

    morkRowObject::SlotWeakRowObject(ro, ev, &mRow_Object);
  }
  return ro;
}

nsIMdbRow*
morkRow::AcquireRowHandle(morkEnv* ev, morkStore* ioStore)
{
  morkRowObject* object = this->AcquireRowObject(ev, ioStore);
  if ( object )
  {
    nsIMdbRow* rowHandle = object->AcquireRowHandle(ev);
    object->CutStrongRef(ev);
    return rowHandle;
  }
  return (nsIMdbRow*) 0;
}

nsIMdbCell*
morkRow::AcquireCellHandle(morkEnv* ev, morkCell* ioCell,
  mdb_column inCol, mork_pos inPos)
{
  nsIMdbHeap* heap = ev->mEnv_Heap;
  morkCellObject* cellObj = new(*heap, ev)
    morkCellObject(ev, morkUsage::kHeap, heap, this, ioCell, inCol, inPos);
  if ( cellObj )
  {
    nsIMdbCell* cellHandle = cellObj->AcquireCellHandle(ev);
    cellObj->CutStrongRef(ev);
    return cellHandle;
  }
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
    mork_size length = (mork_size) mRow_Length;
    
    mork_count overlap = this->CountOverlap(ev, ioVector, inVecLength);

    mork_size growth = inVecLength - overlap; // cells to add
    mork_size newLength = length + growth;
    
    if ( growth && ev->Good() ) // need to add any cells?
    {
      morkZone* zone = &ioStore->mStore_Zone;
      morkPool* pool = ioStore->StorePool();
      if ( !pool->AddRowCells(ev, this, length + growth, zone) )
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

mork_bool morkRow::MaybeDirtySpaceStoreAndRow()
{
  morkRowSpace* rowSpace = mRow_Space;
  if ( rowSpace )
  {
    morkStore* store = rowSpace->mSpace_Store;
    if ( store && store->mStore_CanDirty )
    {
      store->SetStoreDirty();
      rowSpace->mSpace_CanDirty = morkBool_kTrue;
    }
    
    if ( rowSpace->mSpace_CanDirty )
    {
      this->SetRowDirty();
      rowSpace->SetRowSpaceDirty();
      return morkBool_kTrue;
    }
  }
  return morkBool_kFalse;
}

morkCell*
morkRow::NewCell(morkEnv* ev, mdb_column inColumn,
  mork_pos* outPos, morkStore* ioStore)
{
  ++mRow_Seed; // intend to change structure of mRow_Cells
  mork_size length = (mork_size) mRow_Length;
  *outPos = (mork_pos) length;
  morkPool* pool = ioStore->StorePool();
  morkZone* zone = &ioStore->mStore_Zone;
  
  mork_bool canDirty = this->MaybeDirtySpaceStoreAndRow();
  
  if ( pool->AddRowCells(ev, this, length + 1, zone) )
  {
    morkCell* cell = mRow_Cells + length;
    // next line equivalent to inline morkCell::SetCellDirty():
    if ( canDirty )
      cell->SetCellColumnDirty(inColumn);
    else
      cell->SetCellColumnClean(inColumn);
      
    if ( canDirty && !this->IsRowRewrite() )
      this->NoteRowAddCol(ev, inColumn);
      
    return cell;
  }
    
  return (morkCell*) 0;
}



void morkRow::SeekColumn(morkEnv* ev, mdb_pos inPos, 
  mdb_column* outColumn, mdbYarn* outYarn)
{
  morkCell* cells = mRow_Cells;
  if ( cells && inPos < mRow_Length && inPos >= 0 )
  {
    morkCell* c = cells + inPos;
    if ( outColumn )
    	*outColumn = c->GetColumn();
    if ( outYarn )
    	c->mCell_Atom->GetYarn(outYarn); // nil atom works okay here
  }
  else
  {
    if ( outColumn )
    	*outColumn = 0;
    if ( outYarn )
    	((morkAtom*) 0)->GetYarn(outYarn); // yes this will work
  }
}

void
morkRow::NextColumn(morkEnv* ev, mdb_column* ioColumn, mdbYarn* outYarn)
{
  morkCell* cells = mRow_Cells;
  if ( cells )
  {
  	mork_column last = 0;
  	mork_column inCol = *ioColumn;
    morkCell* end = cells + mRow_Length;
    while ( cells < end )
    {
      if ( inCol == last ) // found column?
      {
		    if ( outYarn )
		    	cells->mCell_Atom->GetYarn(outYarn); // nil atom works okay here
        *ioColumn = cells->GetColumn();
        return;  // stop, we are done
      }
      else
      {
        last = cells->GetColumn();
        ++cells;
      }
    }
  }
	*ioColumn = 0;
  if ( outYarn )
  	((morkAtom*) 0)->GetYarn(outYarn); // yes this will work
}

morkCell*
morkRow::CellAt(morkEnv* ev, mork_pos inPos) const
{
  MORK_USED_1(ev);
  morkCell* cells = mRow_Cells;
  if ( cells && inPos < mRow_Length && inPos >= 0 )
  {
    return cells + inPos;
  }
  return (morkCell*) 0;
}

morkCell*
morkRow::GetCell(morkEnv* ev, mdb_column inColumn, mork_pos* outPos) const
{
  MORK_USED_1(ev);
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

mork_aid
morkRow::GetCellAtomAid(morkEnv* ev, mdb_column inColumn) const
  // GetCellAtomAid() finds the cell with column inColumn, and sees if the
  // atom has a token ID, and returns the atom's ID if there is one.  Or
  // else zero is returned if there is no such column, or no atom, or if
  // the atom has no ID to return.  This method is intended to support
  // efficient updating of column indexes for rows in a row space.
{
  if ( this && this->IsRow() )
  {
    morkCell* cells = mRow_Cells;
    if ( cells )
    {
      morkCell* end = cells + mRow_Length;
      while ( cells < end )
      {
        mork_column col = cells->GetColumn();
        if ( col == inColumn ) // found desired column?
        {
          morkAtom* atom = cells->mCell_Atom;
          if ( atom && atom->IsBook() )
            return ((morkBookAtom*) atom)->mBookAtom_Id;
          else
            return 0;
        }
        else
          ++cells;
      }
    }
  }
  else
    this->NonRowTypeError(ev);

  return 0;
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
      if ( this->MaybeDirtySpaceStoreAndRow() )
      {
        this->SetRowRewrite();
        this->NoteRowSetAll(ev);
      }
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
morkRow::cut_all_index_entries(morkEnv* ev)
{
  morkRowSpace* rowSpace = mRow_Space;
  if ( rowSpace->mRowSpace_IndexCount ) // any indexes?
  {
    morkCell* cells = mRow_Cells;
    if ( cells )
    {
      morkCell* end = cells + mRow_Length;
      --cells; // prepare for preincrement:
      while ( ++cells < end )
      {
        morkAtom* atom = cells->mCell_Atom;
        if ( atom )
        {
          mork_aid atomAid = atom->GetBookAtomAid();
          if ( atomAid )
          {
            mork_column col = cells->GetColumn();
            morkAtomRowMap* map = rowSpace->FindMap(ev, col);
            if ( map ) // cut row from index for this column?
              map->CutAid(ev, atomAid);
          }
        }
      }
    }
  }
}

void
morkRow::CutAllColumns(morkEnv* ev)
{
  morkStore* store = this->GetRowSpaceStore(ev);
  if ( store )
  {
    if ( this->MaybeDirtySpaceStoreAndRow() )
    {
      this->SetRowRewrite();
      this->NoteRowSetAll(ev);
    }
    morkRowSpace* rowSpace = mRow_Space;
    if ( rowSpace->mRowSpace_IndexCount ) // any indexes?
      this->cut_all_index_entries(ev);
  
    morkPool* pool = store->StorePool();
    pool->CutRowCells(ev, this, /*newSize*/ 0, &store->mStore_Zone);
  }
}

void
morkRow::SetRow(morkEnv* ev, const morkRow* inSourceRow)
{  
  // note inSourceRow might be in another DB, with a different store...
  morkStore* store = this->GetRowSpaceStore(ev);
  morkStore* srcStore = inSourceRow->GetRowSpaceStore(ev);
  if ( store && srcStore )
  {
    if ( this->MaybeDirtySpaceStoreAndRow() )
    {
      this->SetRowRewrite();
      this->NoteRowSetAll(ev);
    }
    morkRowSpace* rowSpace = mRow_Space;
    mork_count indexes = rowSpace->mRowSpace_IndexCount; // any indexes?
    
    mork_bool sameStore = ( store == srcStore ); // identical stores?
    morkPool* pool = store->StorePool();
    if ( pool->CutRowCells(ev, this, /*newSize*/ 0, &store->mStore_Zone) )
    {
      mork_fill fill = inSourceRow->mRow_Length;
      if ( pool->AddRowCells(ev, this, fill, &store->mStore_Zone) )
      {
        morkCell* dst = mRow_Cells;
        morkCell* dstEnd = dst + mRow_Length;
        
        const morkCell* src = inSourceRow->mRow_Cells;
        const morkCell* srcEnd = src + fill;
        --dst; --src; // prepare both for preincrement:
        
        while ( ++dst < dstEnd && ++src < srcEnd && ev->Good() )
        {
          morkAtom* atom = src->mCell_Atom;
          mork_column dstCol = src->GetColumn();
          // Note we modify the mCell_Atom slot directly instead of using
          // morkCell::SetAtom(), because we know it starts equal to nil.
          
          if ( sameStore ) // source and dest in same store?
          {
            // next line equivalent to inline morkCell::SetCellDirty():
            dst->SetCellColumnDirty(dstCol);
            dst->mCell_Atom = atom;
            if ( atom ) // another ref to non-nil atom?
              atom->AddCellUse(ev);
          }
          else // need to dup items from src store in a dest store
          {
            dstCol = store->CopyToken(ev, dstCol, srcStore);
            if ( dstCol )
            {
              // next line equivalent to inline morkCell::SetCellDirty():
              dst->SetCellColumnDirty(dstCol);
              atom = store->CopyAtom(ev, atom);
              dst->mCell_Atom = atom;
              if ( atom ) // another ref?
                atom->AddCellUse(ev);
            }
          }
          if ( indexes && atom )
          {
            mork_aid atomAid = atom->GetBookAtomAid();
            if ( atomAid )
            {
              morkAtomRowMap* map = rowSpace->FindMap(ev, dstCol);
              if ( map )
                map->AddAid(ev, atomAid, this);
            }
          }
        }
      }
    }
  }
}

void
morkRow::AddRow(morkEnv* ev, const morkRow* inSourceRow)
{
  if ( mRow_Length ) // any existing cells we might need to keep?
  {
    ev->StubMethodOnlyError();
  }
  else
    this->SetRow(ev, inSourceRow); // just exactly duplicate inSourceRow
}

void
morkRow::OnZeroRowGcUse(morkEnv* ev)
// OnZeroRowGcUse() is called when CutRowGcUse() returns zero.
{
  MORK_USED_1(ev);
  // ev->NewWarning("need to implement OnZeroRowGcUse");
}

void
morkRow::DirtyAllRowContent(morkEnv* ev)
{
  MORK_USED_1(ev);

  if ( this->MaybeDirtySpaceStoreAndRow() )
  {
    this->SetRowRewrite();
    this->NoteRowSetAll(ev);
  }
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

void morkRow::CutColumn(morkEnv* ev, mdb_column inColumn)
{
  mork_pos pos = -1;
  morkCell* cell = this->GetCell(ev, inColumn, &pos);
  if ( cell ) 
  {
    morkStore* store = this->GetRowSpaceStore(ev);
    if ( store )
    {
      if ( this->MaybeDirtySpaceStoreAndRow() && !this->IsRowRewrite() )
        this->NoteRowCutCol(ev, inColumn);
        
      morkRowSpace* rowSpace = mRow_Space;
      morkAtomRowMap* map = ( rowSpace->mRowSpace_IndexCount )?
        rowSpace->FindMap(ev, inColumn) : (morkAtomRowMap*) 0;
      if ( map ) // this row attribute is indexed by row space?
      {
        morkAtom* oldAtom = cell->mCell_Atom;
        if ( oldAtom ) // need to cut an entry from the index?
        {
          mork_aid oldAid = oldAtom->GetBookAtomAid();
          if ( oldAid ) // cut old row attribute from row index in space?
            map->CutAid(ev, oldAid);
        }
      }
      
      morkPool* pool = store->StorePool();
      cell->SetAtom(ev, (morkAtom*) 0, pool);
      
      mork_fill fill = mRow_Length; // should not be zero
      MORK_ASSERT(fill);
      if ( fill ) // index < fill for last cell exists?
      {
        mork_fill last = fill - 1; // index of last cell in row
        
        if ( pos < (mork_pos)last ) // need to move cells following cut cell?
        {
          morkCell* lastCell = mRow_Cells + last;
          mork_count after = last - pos; // cell count after cut cell
          morkCell* next = cell + 1; // next cell after cut cell
          MORK_MEMMOVE(cell, next, after * sizeof(morkCell));
          lastCell->SetColumnAndChange(0, 0);
          lastCell->mCell_Atom = 0;
        }
        
        if ( ev->Good() )
          pool->CutRowCells(ev, this, fill - 1, &store->mStore_Zone);
      }
    }
  }
}

morkAtom* morkRow::GetColumnAtom(morkEnv* ev, mdb_column inColumn)
{
  if ( ev->Good() )
  {
    mork_pos pos = -1;
    morkCell* cell = this->GetCell(ev, inColumn, &pos);
    if ( cell )
    	return cell->mCell_Atom;
  }
  return (morkAtom*) 0;
}

void morkRow::AddColumn(morkEnv* ev, mdb_column inColumn,
  const mdbYarn* inYarn, morkStore* ioStore)
{
  if ( ev->Good() )
  {
    mork_pos pos = -1;
    morkCell* cell = this->GetCell(ev, inColumn, &pos);
    morkCell* oldCell = cell; // need to know later whether new
    if ( !cell ) // column does not yet exist?
      cell = this->NewCell(ev, inColumn, &pos, ioStore);
    
    if ( cell )
    {
      morkAtom* oldAtom = cell->mCell_Atom;

      morkAtom* atom = ioStore->YarnToAtom(ev, inYarn);
      if ( atom && atom != oldAtom )
      {
        morkRowSpace* rowSpace = mRow_Space;
        morkAtomRowMap* map = ( rowSpace->mRowSpace_IndexCount )?
          rowSpace->FindMap(ev, inColumn) : (morkAtomRowMap*) 0;
        
        if ( map ) // inColumn is indexed by row space?
        {
          if ( oldAtom && oldAtom != atom ) // cut old cell from index?
          {
            mork_aid oldAid = oldAtom->GetBookAtomAid();
            if ( oldAid ) // cut old row attribute from row index in space?
              map->CutAid(ev, oldAid);
          }
        }
        
        cell->SetAtom(ev, atom, ioStore->StorePool()); // refcounts atom

        if ( oldCell ) // we changed a pre-existing cell in the row?
        {
          ++mRow_Seed;
          if ( this->MaybeDirtySpaceStoreAndRow() && !this->IsRowRewrite() )
            this->NoteRowAddCol(ev, inColumn);
        }

        if ( map ) // inColumn is indexed by row space?
        {
          mork_aid newAid = atom->GetBookAtomAid();
          if ( newAid ) // add new row attribute to row index in space?
            map->AddAid(ev, newAid, this);
        }
      }
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
      morkRowObject* rowObj = this->AcquireRowObject(ev, store);
      if ( rowObj )
      {
        nsIMdbHeap* heap = store->mPort_Heap;
        morkRowCellCursor* cursor = new(*heap, ev)
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
        rowObj->CutStrongRef(ev); // always cut ref (cursor has its own)
      }
    }
  }
  return outCursor;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

