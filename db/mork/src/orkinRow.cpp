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

#ifndef _MORKHANDLE_
#include "morkHandle.h"
#endif

#ifndef _MORKATOM_
#include "morkAtom.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _ORKINROW_
#include "orkinRow.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

#ifndef _MORKCELLOBJECT_
#include "morkCellObject.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _ORKINSTORE_
#include "orkinStore.h"
#endif

#ifndef _MORKROWSPACE_
#include "morkRowSpace.h"
#endif

#ifndef _MORKROWCELLCURSOR_
#include "morkRowCellCursor.h"
#endif

#ifndef _ORKINROWCELLCURSOR_
#include "orkinRowCellCursor.h"
#endif

#ifndef _ORKINCELL_
#include "orkinCell.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinRow:: ~orkinRow() // morkHandle destructor does everything
{
}    

/*protected non-poly construction*/
orkinRow::orkinRow(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkRowObject* ioObject)  // must not be nil, the object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kRow)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinRow*
orkinRow::MakeRow(morkEnv* ev,  morkRowObject* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinRow));
    if ( face )
      return new(face) orkinRow(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinRow*) 0;
}

morkEnv*
orkinRow::CanUseRow(nsIMdbEnv* mev, mork_bool inMutable,
  mdb_err* outErr, morkRow** outRow) const
{
  morkEnv* outEnv = 0;
  morkRow* innerRow = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRowObject* rowObj = (morkRowObject*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kRow,
        /*inClosedOkay*/ morkBool_kFalse);
    if ( rowObj )
    {
      if ( rowObj->IsRowObject() )
      {
        morkRow* row = rowObj->mRowObject_Row;
        if ( row )
        {
          if ( row->IsRow() )
          {
            if ( row->mRow_Object == rowObj )
            {
              outEnv = ev;
              innerRow = row;
            }
            else
              rowObj->RowObjectRowNotSelfError(ev);
          }
          else
            row->NonRowTypeError(ev);
        }
        else
          rowObj->NilRowError(ev);
      }
      else
        rowObj->NonRowObjectTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  if ( outRow )
    *outRow = innerRow;
  MORK_ASSERT(outEnv);
  return outEnv;
}

morkStore*
orkinRow::CanUseRowStore(morkEnv* ev) const
{
  morkStore* outStore = 0;
  morkRowObject* rowObj = (morkRowObject*) mHandle_Object;
  if ( rowObj && rowObj->IsRowObject() )
  {
    morkStore* store = rowObj->mRowObject_Store;
    if ( store )
    {
      if ( store->IsStore() )
      {
        outStore = store;
      }
      else
        store->NonStoreTypeError(ev);
    }
    else
      rowObj->NilStoreError(ev);
  }
  return outStore;
}


// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinRow::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinRow::Release() // cut strong ref
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_CutStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}
// } ===== end nsIMdbObject methods =====


// { ===== begin nsIMdbObject methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinRow::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinRow::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinRow::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinRow::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinRow::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinRow::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinRow::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinRow::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinRow::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinRow::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====


// { ===== begin nsIMdbCollection methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinRow::GetSeed(nsIMdbEnv* mev,
  mdb_seed* outSeed)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    *outSeed = (mdb_seed) row->mRow_Seed;
    outErr = ev->AsErr();
  }
  return outErr;
}
/*virtual*/ mdb_err
orkinRow::GetCount(nsIMdbEnv* mev,
  mdb_count* outCount)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    *outCount = (mdb_count) row->mRow_Length;
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinRow::GetPort(nsIMdbEnv* mev,
  nsIMdbPort** acqPort)
{
  mdb_err outErr = 0;
  nsIMdbPort* outPort = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkRowSpace* rowSpace = row->mRow_Space;
    if ( rowSpace && rowSpace->mSpace_Store )
    {
      morkStore* store = row->GetRowSpaceStore(ev);
      if ( store )
        outPort = store->AcquireStoreHandle(ev);
    }
    else
      ev->NilPointerError();
      
    outErr = ev->AsErr();
  }
  if ( acqPort )
    *acqPort = outPort;
    
  return outErr;
}
// } ----- end attribute methods -----

// { ----- begin cursor methods -----
/*virtual*/ mdb_err
orkinRow::GetCursor( // make a cursor starting iter at inMemberPos
  nsIMdbEnv* mev, // context
  mdb_pos inMemberPos, // zero-based ordinal pos of member in collection
  nsIMdbCursor** acqCursor)
{
  return this->GetRowCellCursor(mev, inMemberPos,
    (nsIMdbRowCellCursor**) acqCursor);
}
// } ----- end cursor methods -----

// { ----- begin ID methods -----
/*virtual*/ mdb_err
orkinRow::GetOid(nsIMdbEnv* mev,
  mdbOid* outOid)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    *outOid = row->mRow_Oid;
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinRow::BecomeContent(nsIMdbEnv* mev,
  const mdbOid* inOid)
{
  MORK_USED_1(inOid);
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    // remember row->MaybeDirtySpaceStoreAndRow();
    
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end ID methods -----

// { ----- begin activity dropping methods -----
/*virtual*/ mdb_err
orkinRow::DropActivity( // tell collection usage no longer expected
  nsIMdbEnv* mev)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // do nothing
    outErr = ev->AsErr();
  }
  return outErr;
    }
// } ----- end activity dropping methods -----

// } ===== end nsIMdbCollection methods =====

// { ===== begin nsIMdbRow methods =====

// { ----- begin cursor methods -----
/*virtual*/ mdb_err
orkinRow::GetRowCellCursor( // make a cursor starting iteration at inRowPos
  nsIMdbEnv* mev, // context
  mdb_pos inPos, // zero-based ordinal position of row in table
  nsIMdbRowCellCursor** acqCursor)
{
  mdb_err outErr = 0;
  nsIMdbRowCellCursor* outCursor = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor = row->NewRowCellCursor(ev, inPos);
    if ( cursor )
    {
      if ( ev->Good() )
      {
        cursor->mCursor_Seed = (mork_seed) inPos;
        outCursor = cursor->AcquireRowCellCursorHandle(ev);
      }
      else
        cursor->CutStrongRef(ev);
    }
    outErr = ev->AsErr();
  }
  if ( acqCursor )
    *acqCursor = outCursor;
  return outErr;
}
// } ----- end cursor methods -----

// { ----- begin column methods -----
/*virtual*/ mdb_err
orkinRow::AddColumn( // make sure a particular column is inside row
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to add
  const mdbYarn* inYarn)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkStore* store = this->CanUseRowStore(ev);
    if ( store )
      row->AddColumn(ev, inColumn, inYarn, store);
      
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinRow::CutColumn( // make sure a column is absent from the row
  nsIMdbEnv* mev, // context
  mdb_column inColumn)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    row->CutColumn(ev, inColumn);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinRow::CutAllColumns( // remove all columns from the row
  nsIMdbEnv* mev)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    row->CutAllColumns(ev);
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end column methods -----

// { ----- begin cell methods -----
/*virtual*/ mdb_err
orkinRow::NewCell( // get cell for specified column, or add new one
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to add
  nsIMdbCell** acqCell)
{
  mdb_err outErr = 0;
  nsIMdbCell* outCell = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    mork_pos pos = 0;
    morkCell* cell = row->GetCell(ev, inColumn, &pos);
    if ( !cell )
    {
      morkStore* store = this->CanUseRowStore(ev);
      if ( store )
      {
        mdbYarn yarn; // to pass empty yarn into morkRow::AddColumn()
        yarn.mYarn_Buf = 0;
        yarn.mYarn_Fill = 0;
        yarn.mYarn_Size = 0;
        yarn.mYarn_More = 0;
        yarn.mYarn_Form = 0;
        yarn.mYarn_Grow = 0;
        row->AddColumn(ev, inColumn, &yarn, store);
        cell = row->GetCell(ev, inColumn, &pos);
      }
    }
    if ( cell )
      outCell = row->AcquireCellHandle(ev, cell, inColumn, pos);
      
    outErr = ev->AsErr();
  }
  if ( acqCell )
    *acqCell = outCell;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinRow::AddCell( // copy a cell from another row to this row
  nsIMdbEnv* mev, // context
  const nsIMdbCell* inCell)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkCell* cell = 0;
    orkinCell* ocell = (orkinCell*) inCell; // must verify this cast:
    if ( ocell->CanUseCell(mev, morkBool_kFalse, &outErr, &cell) )
    {
      morkCellObject* cellObj = (morkCellObject*) ocell->mHandle_Object;

      morkRow* cellRow = cellObj->mCellObject_Row;
      if ( cellRow )
      {
        if ( row != cellRow )
        {
          morkStore* store = row->GetRowSpaceStore(ev);
          morkStore* cellStore = cellRow->GetRowSpaceStore(ev);
          if ( store && cellStore )
          {
            mork_column col = cell->GetColumn();
            morkAtom* atom = cell->mCell_Atom;
            mdbYarn yarn;
            atom->AliasYarn(&yarn); // works even when atom is nil
            
            if ( store != cellStore )
              col = store->CopyToken(ev, col, cellStore);
            if ( ev->Good() )
              row->AddColumn(ev, col, &yarn, store);
          }
          else
            ev->NilPointerError();
        }
      }
      else
        ev->NilPointerError();
    }

    outErr = ev->AsErr();
  }
  return outErr;
}
  
/*virtual*/ mdb_err
orkinRow::GetCell( // find a cell in this row
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to find
  nsIMdbCell** acqCell)
{
  mdb_err outErr = 0;
  nsIMdbCell* outCell = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    if ( inColumn )
    {
      mork_pos pos = 0;
      morkCell* cell = row->GetCell(ev, inColumn, &pos);
      if ( cell )
      {
        outCell = row->AcquireCellHandle(ev, cell, inColumn, pos);
      }
    }
    else
      row->ZeroColumnError(ev);
      
    outErr = ev->AsErr();
  }
  if ( acqCell )
    *acqCell = outCell;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinRow::EmptyAllCells( // make all cells in row empty of content
  nsIMdbEnv* mev)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    row->EmptyAllCells(ev);
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end cell methods -----

// { ----- begin row methods -----
/*virtual*/ mdb_err
orkinRow::AddRow( // add all cells in another row to this one
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioSourceRow)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkRow* source = 0;
    orkinRow* unsafeSource = (orkinRow*) ioSourceRow; // unsafe cast
    if ( unsafeSource->CanUseRow(mev, morkBool_kFalse, &outErr, &source) )
    {
      row->AddRow(ev, source);
    }
    outErr = ev->AsErr();
  }
  return outErr;
}
  
/*virtual*/ mdb_err
orkinRow::SetRow( // make exact duplicate of another row
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioSourceRow)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkRow* source = 0;
    orkinRow* unsafeSource = (orkinRow*) ioSourceRow; // unsafe cast
    if ( unsafeSource->CanUseRow(mev, morkBool_kFalse, &outErr, &source) )
    {
      row->SetRow(ev, source);
    }
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end row methods -----

// { ----- begin blob methods -----
/*virtual*/ mdb_err
orkinRow::SetCellYarn( // synonym for AddColumn()
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to add
  const mdbYarn* inYarn)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkStore* store = this->CanUseRowStore(ev);
    if ( store )
      row->AddColumn(ev, inColumn, inYarn, store);
      
    outErr = ev->AsErr();
  }
  return outErr;
}
/*virtual*/ mdb_err
orkinRow::GetCellYarn(
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to read 
  mdbYarn* outYarn)  // writes some yarn slots 
// copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkStore* store = this->CanUseRowStore(ev);
    if ( store )
    {
	    morkAtom* atom = row->GetColumnAtom(ev, inColumn);
	    atom->GetYarn(outYarn);
	    // note nil atom works and sets yarn correctly
    }
      
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinRow::AliasCellYarn(
  nsIMdbEnv* mev, // context
    mdb_column inColumn, // column to alias
    mdbYarn* outYarn) // writes ALL yarn slots
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkStore* store = this->CanUseRowStore(ev);
    if ( store )
    {
	    morkAtom* atom = row->GetColumnAtom(ev, inColumn);
	    atom->AliasYarn(outYarn);
	    // note nil atom works and sets yarn correctly
    }
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinRow::NextCellYarn(nsIMdbEnv* mev, // iterative version of GetCellYarn()
  mdb_column* ioColumn, // next column to read
  mdbYarn* outYarn)  // writes some yarn slots 
// copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
//
// The ioColumn argument is an inout parameter which initially contains the
// last column accessed and returns the next column corresponding to the
// content read into the yarn.  Callers should start with a zero column
// value to say 'no previous column', which causes the first column to be
// read.  Then the value returned in ioColumn is perfect for the next call
// to NextCellYarn(), since it will then be the previous column accessed.
// Callers need only examine the column token returned to see which cell
// in the row is being read into the yarn.  When no more columns remain,
// and the iteration has ended, ioColumn will return a zero token again.
// So iterating over cells starts and ends with a zero column token.
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkStore* store = this->CanUseRowStore(ev);
    if ( store )
      row->NextColumn(ev, ioColumn, outYarn);
      
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinRow::SeekCellYarn( // resembles nsIMdbRowCellCursor::SeekCell()
  nsIMdbEnv* mev, // context
  mdb_pos inPos, // position of cell in row sequence
  mdb_column* outColumn, // column for this particular cell
  mdbYarn* outYarn) // writes some yarn slots
// copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
// Callers can pass nil for outYarn to indicate no interest in content, so
// only the outColumn value is returned.  NOTE to subclasses: you must be
// able to ignore outYarn when the pointer is nil; please do not crash.

{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = this->CanUseRow(mev, /*inMutable*/ morkBool_kFalse,
    &outErr, &row);
  if ( ev )
  {
    morkStore* store = this->CanUseRowStore(ev);
    if ( store )
      row->SeekColumn(ev, inPos, outColumn, outYarn);
      
    outErr = ev->AsErr();
  }
  return outErr;
}

// } ----- end blob methods -----


// } ===== end nsIMdbRow methods =====



//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

