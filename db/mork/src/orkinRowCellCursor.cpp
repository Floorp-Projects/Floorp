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

#ifndef _MORKHANDLE_
#include "morkHandle.h"
#endif

#ifndef _MORKROWCELLCURSOR_
#include "morkRowCellCursor.h"
#endif

#ifndef _ORKINROWCELLCURSOR_
#include "orkinRowCellCursor.h"
#endif

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _ORKINROW_
#include "orkinRow.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinRowCellCursor:: ~orkinRowCellCursor() // morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinRowCellCursor::orkinRowCellCursor(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkRowCellCursor* ioObject)  // must not be nil, the object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kRowCellCursor)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinRowCellCursor*
orkinRowCellCursor::MakeRowCellCursor(morkEnv* ev, morkRowCellCursor* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinRowCellCursor));
    if ( face )
      return new(face) orkinRowCellCursor(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinRowCellCursor*) 0;
}

morkEnv*
orkinRowCellCursor::CanUseRowCellCursor(nsIMdbEnv* mev, mork_bool inMutable,
  mdb_err* outErr, morkRow** outRow) const
{
  morkEnv* outEnv = 0;
  morkRow* row = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRowCellCursor* self = (morkRowCellCursor*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kRowCellCursor,
        /*inClosedOkay*/ morkBool_kFalse);
    if ( self )
    {
      if ( self->IsRowCellCursor() )
      {
        if ( self->IsMutable() || !inMutable )
        {
          morkRowObject* rowObj = self->mRowCellCursor_RowObject;
          if ( rowObj )
          {
            morkRow* theRow = rowObj->mRowObject_Row;
            if ( theRow )
            {
              if ( theRow->IsRow() )
              {
                outEnv = ev;
                row = theRow;
              }
              else
                theRow->NonRowTypeError(ev);
            }
            else
              rowObj->NilRowError(ev);
          }
          else
            self->NilRowObjectError(ev);
        }
        else
          self->NonMutableNodeError(ev);
      }
      else
        self->NonRowCellCursorTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  *outRow = row;
  MORK_ASSERT(outEnv);
  return outEnv;
}

// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinRowCellCursor::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinRowCellCursor::Release() // cut strong ref
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
orkinRowCellCursor::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinRowCellCursor::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinRowCellCursor::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinRowCellCursor::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinRowCellCursor::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinRowCellCursor::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinRowCellCursor::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinRowCellCursor::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinRowCellCursor::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinRowCellCursor::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbCursor methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinRowCellCursor::GetCount(nsIMdbEnv* mev, mdb_count* outCount)
{
  mdb_err outErr = 0;
  mdb_count count = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    count = row->mRow_Length;
    outErr = ev->AsErr();
  }
  if ( outCount )
    *outCount = count;
  return outErr;
}

/*virtual*/ mdb_err
orkinRowCellCursor::GetSeed(nsIMdbEnv* mev, mdb_seed* outSeed)
{
  mdb_err outErr = 0;
  mdb_seed seed = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    seed = row->mRow_Seed;
    outErr = ev->AsErr();
  }
  if ( outSeed )
    *outSeed = seed;
  return outErr;
}

/*virtual*/ mdb_err
orkinRowCellCursor::SetPos(nsIMdbEnv* mev, mdb_pos inPos)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor = (morkRowCellCursor*) mHandle_Object;
    cursor->mCursor_Pos = inPos;
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinRowCellCursor::GetPos(nsIMdbEnv* mev, mdb_pos* outPos)
{
  mdb_err outErr = 0;
  mdb_pos pos = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor = (morkRowCellCursor*) mHandle_Object;
    pos = cursor->mCursor_Pos;
    outErr = ev->AsErr();
  }
  if ( outPos )
    *outPos = pos;
  return outErr;
}

/*virtual*/ mdb_err
orkinRowCellCursor::SetDoFailOnSeedOutOfSync(nsIMdbEnv* mev, mdb_bool inFail)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor = (morkRowCellCursor*) mHandle_Object;
    cursor->mCursor_DoFailOnSeedOutOfSync = inFail;
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinRowCellCursor::GetDoFailOnSeedOutOfSync(nsIMdbEnv* mev, mdb_bool* outFail)
{
  mdb_err outErr = 0;
  mdb_bool doFail = morkBool_kFalse;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor = (morkRowCellCursor*) mHandle_Object;
    doFail = cursor->mCursor_DoFailOnSeedOutOfSync;
    outErr = ev->AsErr();
  }
  if ( outFail )
    *outFail = doFail;
  return outErr;
}
// } ----- end attribute methods -----

// } ===== end nsIMdbCursor methods =====

// { ===== begin nsIMdbRowCellCursor methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinRowCellCursor::SetRow(nsIMdbEnv* mev, nsIMdbRow* ioRow)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor = (morkRowCellCursor*) mHandle_Object;
    row = 0;
    orkinRow* orow = (orkinRow*) ioRow;
    if ( orow->CanUseRow(mev, /*inMutable*/ morkBool_kFalse, &outErr, &row) )
    {
      morkStore* store = row->GetRowSpaceStore(ev);
      if ( store )
      {
        morkRowObject* rowObj = row->AcquireRowObject(ev, store);
        if ( rowObj )
        {
          morkRowObject::SlotStrongRowObject((morkRowObject*) 0, ev,
            &cursor->mRowCellCursor_RowObject);
            
          cursor->mRowCellCursor_RowObject = rowObj; // take this strong ref
          cursor->mCursor_Seed = row->mRow_Seed;
          
          row->GetCell(ev, cursor->mRowCellCursor_Col, &cursor->mCursor_Pos);
        }
      }
    }
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinRowCellCursor::GetRow(nsIMdbEnv* mev, nsIMdbRow** acqRow)
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor = (morkRowCellCursor*) mHandle_Object;
    morkRowObject* rowObj = cursor->mRowCellCursor_RowObject;
    if ( rowObj )
      outRow = rowObj->AcquireRowHandle(ev);

    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}
// } ----- end attribute methods -----

// { ----- begin cell creation methods -----
/*virtual*/ mdb_err
orkinRowCellCursor::MakeCell( // get cell at current pos in the row
  nsIMdbEnv* mev, // context
  mdb_column* outColumn, // column for this particular cell
  mdb_pos* outPos, // position of cell in row sequence
  nsIMdbCell** acqCell)
{
  mdb_err outErr = 0;
  nsIMdbCell* outCell = 0;
  mdb_pos pos = 0;
  mdb_column col = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor = (morkRowCellCursor*) mHandle_Object;
    pos = cursor->mCursor_Pos;
    morkCell* cell = row->CellAt(ev, pos);
    if ( cell )
    {
      col = cell->GetColumn();
      outCell = row->AcquireCellHandle(ev, cell, col, pos);
    }
    outErr = ev->AsErr();
  }
  if ( acqCell )
    *acqCell = outCell;
   if ( outPos )
     *outPos = pos;
   if ( outColumn )
     *outColumn = col;
     
  return outErr;
}
// } ----- end cell creation methods -----

// { ----- begin cell seeking methods -----
/*virtual*/ mdb_err
orkinRowCellCursor::SeekCell( // same as SetRow() followed by MakeCell()
  nsIMdbEnv* mev, // context
  mdb_pos inPos, // position of cell in row sequence
  mdb_column* outColumn, // column for this particular cell
  nsIMdbCell** acqCell)
{
  MORK_USED_1(inPos);
  mdb_err outErr = 0;
  mdb_column column = 0;
  nsIMdbCell* outCell = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor;
    cursor = (morkRowCellCursor*) mHandle_Object;
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( acqCell )
    *acqCell = outCell;
  if ( outColumn )
    *outColumn = column;
  return outErr;
}
// } ----- end cell seeking methods -----

// { ----- begin cell iteration methods -----
/*virtual*/ mdb_err
orkinRowCellCursor::NextCell( // get next cell in the row
  nsIMdbEnv* mev, // context
  nsIMdbCell* ioCell, // changes to the next cell in the iteration
  mdb_column* outColumn, // column for this particular cell
  mdb_pos* outPos)
{
  MORK_USED_1(ioCell);
  mdb_err outErr = 0;
  mdb_pos pos = -1;
  mdb_column column = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor;
    cursor = (morkRowCellCursor*) mHandle_Object;
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outColumn )
    *outColumn = column;
  if ( outPos )
    *outPos = pos;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinRowCellCursor::PickNextCell( // get next cell in row within filter set
  nsIMdbEnv* mev, // context
  nsIMdbCell* ioCell, // changes to the next cell in the iteration
  const mdbColumnSet* inFilterSet, // col set of actual caller interest
  mdb_column* outColumn, // column for this particular cell
  mdb_pos* outPos)
// Note that inFilterSet should not have too many (many more than 10?)
// cols, since this might imply a potential excessive consumption of time
// over many cursor calls when looking for column and filter intersection.
{
  MORK_USED_2(ioCell,inFilterSet);
  mdb_pos pos = -1;
  mdb_column column = 0;
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev =
    this->CanUseRowCellCursor(mev, /*mut*/ morkBool_kFalse, &outErr, &row);
  if ( ev )
  {
    morkRowCellCursor* cursor;
    cursor = (morkRowCellCursor*) mHandle_Object;
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outColumn )
    *outColumn = column;
  if ( outPos )
    *outPos = pos;
  return outErr;
}

// } ----- end cell iteration methods -----

// } ===== end nsIMdbRowCellCursor methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
