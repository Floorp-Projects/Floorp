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

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKCURSOR_
#include "morkCursor.h"
#endif

#ifndef _MORKROWCELLCURSOR_
#include "morkRowCellCursor.h"
#endif

#ifndef _ORKINROWCELLCURSOR_
#include "orkinRowCellCursor.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkRowCellCursor::CloseMorkNode(morkEnv* ev) // CloseRowCellCursor() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseRowCellCursor(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkRowCellCursor::~morkRowCellCursor() // CloseRowCellCursor() executed earlier
{
  CloseMorkNode(mMorkEnv);
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkRowCellCursor::morkRowCellCursor(morkEnv* ev,
  const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, morkRowObject* ioRowObject)
: morkCursor(ev, inUsage, ioHeap)
, mRowCellCursor_RowObject( 0 )
, mRowCellCursor_Col( 0 )
{
  if ( ev->Good() )
  {
    if ( ioRowObject )
    {
      morkRow* row = ioRowObject->mRowObject_Row;
      if ( row )
      {
        if ( row->IsRow() )
        {
          mCursor_Pos = -1;
          mCursor_Seed = row->mRow_Seed;
          
          morkRowObject::SlotStrongRowObject(ioRowObject, ev,
            &mRowCellCursor_RowObject);
          if ( ev->Good() )
            mNode_Derived = morkDerived_kRowCellCursor;
        }
        else
          row->NonRowTypeError(ev);
      }
      else
        ioRowObject->NilRowError(ev);
    }
    else
      ev->NilPointerError();
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(morkRowCellCursor, morkCursor, nsIMdbRowCellCursor)

/*public non-poly*/ void
morkRowCellCursor::CloseRowCellCursor(morkEnv* ev) 
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mCursor_Pos = -1;
      mCursor_Seed = 0;
      morkRowObject::SlotStrongRowObject((morkRowObject*) 0, ev,
        &mRowCellCursor_RowObject);
      this->CloseCursor(ev);
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
morkRowCellCursor::NilRowObjectError(morkEnv* ev)
{
  ev->NewError("nil mRowCellCursor_RowObject");
}

/*static*/ void
morkRowCellCursor::NonRowCellCursorTypeError(morkEnv* ev)
{
  ev->NewError("non morkRowCellCursor");
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
// { ----- begin attribute methods -----
NS_IMETHODIMP
morkRowCellCursor::SetRow(nsIMdbEnv* mev, nsIMdbRow* ioRow)
{
  mdb_err outErr = 0;
  morkRow* row = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    row = (morkRow *) ioRow;
    morkStore* store = row->GetRowSpaceStore(ev);
    if ( store )
    {
      morkRowObject* rowObj = row->AcquireRowObject(ev, store);
      if ( rowObj )
      {
        morkRowObject::SlotStrongRowObject((morkRowObject*) 0, ev,
          &mRowCellCursor_RowObject);
          
        mRowCellCursor_RowObject = rowObj; // take this strong ref
        mCursor_Seed = row->mRow_Seed;
        
        row->GetCell(ev, mRowCellCursor_Col, &mCursor_Pos);
      }
    }
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkRowCellCursor::GetRow(nsIMdbEnv* mev, nsIMdbRow** acqRow)
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRowObject* rowObj = mRowCellCursor_RowObject;
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
NS_IMETHODIMP
morkRowCellCursor::MakeCell( // get cell at current pos in the row
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
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    pos = mCursor_Pos;
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
NS_IMETHODIMP
morkRowCellCursor::SeekCell( // same as SetRow() followed by MakeCell()
  nsIMdbEnv* mev, // context
  mdb_pos inPos, // position of cell in row sequence
  mdb_column* outColumn, // column for this particular cell
  nsIMdbCell** acqCell)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
// } ----- end cell seeking methods -----

// { ----- begin cell iteration methods -----
NS_IMETHODIMP
morkRowCellCursor::NextCell( // get next cell in the row
  nsIMdbEnv* mev, // context
  nsIMdbCell** acqCell, // changes to the next cell in the iteration
  mdb_column* outColumn, // column for this particular cell
  mdb_pos* outPos)
{
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  mdb_column col = 0;
  mdb_pos pos = mRowCellCursor_Col;
  if ( pos < 0 )
    pos = 0;
  else
    ++pos;

  morkCell* cell = mRowCellCursor_RowObject->mRowObject_Row->CellAt(ev, pos);
  if ( cell )
  {
    col = cell->GetColumn();
    *acqCell = mRowCellCursor_RowObject->mRowObject_Row->AcquireCellHandle(ev, cell, col, pos);
  }
  else
  {
    *acqCell = nsnull;
    pos = -1;
  }
 if ( outPos )
   *outPos = pos;
 if ( outColumn )
   *outColumn = col;
     
  mRowCellCursor_Col = pos;
  *outPos = pos;
  return NS_OK;
}
  
NS_IMETHODIMP
morkRowCellCursor::PickNextCell( // get next cell in row within filter set
  nsIMdbEnv* mev, // context
  nsIMdbCell* ioCell, // changes to the next cell in the iteration
  const mdbColumnSet* inFilterSet, // col set of actual caller interest
  mdb_column* outColumn, // column for this particular cell
  mdb_pos* outPos)
// Note that inFilterSet should not have too many (many more than 10?)
// cols, since this might imply a potential excessive consumption of time
// over many cursor calls when looking for column and filter intersection.
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// } ----- end cell iteration methods -----

// } ===== end nsIMdbRowCellCursor methods =====

