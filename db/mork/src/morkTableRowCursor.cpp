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
 *   Blake Ross (blake@blakeross.com)
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

#ifndef _MORKTABLEROWCURSOR_
#include "morkTableRowCursor.h"
#endif

#ifndef _ORKINTABLEROWCURSOR_
#include "orkinTableRowCursor.h"
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

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkTableRowCursor::CloseMorkNode(morkEnv* ev) // CloseTableRowCursor() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseTableRowCursor(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkTableRowCursor::~morkTableRowCursor() // CloseTableRowCursor() executed earlier
{
  CloseMorkNode(mMorkEnv);
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkTableRowCursor::morkTableRowCursor(morkEnv* ev,
  const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, morkTable* ioTable, mork_pos inRowPos)
: morkCursor(ev, inUsage, ioHeap)
, mTableRowCursor_Table( 0 )
{
  if ( ev->Good() )
  {
    if ( ioTable )
    {
      mCursor_Pos = inRowPos;
      mCursor_Seed = ioTable->TableSeed();
      morkTable::SlotWeakTable(ioTable, ev, &mTableRowCursor_Table);
      if ( ev->Good() )
        mNode_Derived = morkDerived_kTableRowCursor;
    }
    else
      ev->NilPointerError();
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(morkTableRowCursor, morkCursor, nsIMdbTableRowCursor)
/*public non-poly*/ void
morkTableRowCursor::CloseTableRowCursor(morkEnv* ev) 
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mCursor_Pos = -1;
      mCursor_Seed = 0;
      morkTable::SlotWeakTable((morkTable*) 0, ev, &mTableRowCursor_Table);
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
// { ----- begin attribute methods -----
/*virtual*/ mdb_err
morkTableRowCursor::GetCount(nsIMdbEnv* mev, mdb_count* outCount)
{
  mdb_err outErr = 0;
  mdb_count count = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    count = GetMemberCount(ev);
    outErr = ev->AsErr();
  }
  if ( outCount )
    *outCount = count;
  return outErr;
}

/*virtual*/ mdb_err
morkTableRowCursor::GetSeed(nsIMdbEnv* mev, mdb_seed* outSeed)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/*virtual*/ mdb_err
morkTableRowCursor::SetPos(nsIMdbEnv* mev, mdb_pos inPos)
{
  mCursor_Pos = inPos;
  return NS_OK;
}

/*virtual*/ mdb_err
morkTableRowCursor::GetPos(nsIMdbEnv* mev, mdb_pos* outPos)
{
  *outPos = mCursor_Pos;
  return NS_OK;
}

/*virtual*/ mdb_err
morkTableRowCursor::SetDoFailOnSeedOutOfSync(nsIMdbEnv* mev, mdb_bool inFail)
{
  mCursor_DoFailOnSeedOutOfSync = inFail;
  return NS_OK;
}

/*virtual*/ mdb_err
morkTableRowCursor::GetDoFailOnSeedOutOfSync(nsIMdbEnv* mev, mdb_bool* outFail)
{
  NS_ENSURE_ARG_POINTER(outFail);
  *outFail = mCursor_DoFailOnSeedOutOfSync;
  return NS_OK;
}
// } ----- end attribute methods -----


// { ===== begin nsIMdbTableRowCursor methods =====

// { ----- begin attribute methods -----

NS_IMETHODIMP
morkTableRowCursor::GetTable(nsIMdbEnv* mev, nsIMdbTable** acqTable)
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( mTableRowCursor_Table )
      outTable = mTableRowCursor_Table->AcquireTableHandle(ev);
    
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}
// } ----- end attribute methods -----

// { ----- begin oid iteration methods -----
NS_IMETHODIMP
morkTableRowCursor::NextRowOid( // get row id of next row in the table
  nsIMdbEnv* mev, // context
  mdbOid* outOid, // out row oid
  mdb_pos* outRowPos)
{
  mdb_err outErr = 0;
  mork_pos pos = -1;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( outOid )
    {
      pos = NextRowOid(ev, outOid);
    }
    else
      ev->NilPointerError();
    outErr = ev->AsErr();
  }
  if ( outRowPos )
    *outRowPos = pos;
  return outErr;
}

NS_IMETHODIMP
morkTableRowCursor::PrevRowOid( // get row id of previous row in the table
  nsIMdbEnv* mev, // context
  mdbOid* outOid, // out row oid
  mdb_pos* outRowPos)
{
  mdb_err outErr = 0;
  mork_pos pos = -1;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( outOid )
    {
      pos = PrevRowOid(ev, outOid);
    }
    else
      ev->NilPointerError();
    outErr = ev->AsErr();
  }
  if ( outRowPos )
    *outRowPos = pos;
  return outErr;
}
// } ----- end oid iteration methods -----

// { ----- begin row iteration methods -----
NS_IMETHODIMP
morkTableRowCursor::NextRow( // get row cells from table for cells already in row
  nsIMdbEnv* mev, // context
  nsIMdbRow** acqRow, // acquire next row in table
  mdb_pos* outRowPos)
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
      
    mdbOid oid; // place to put oid we intend to ignore
    morkRow* row = NextRow(ev, &oid, outRowPos);
    if ( row )
    {
      morkStore* store = row->GetRowSpaceStore(ev);
      if ( store )
        outRow = row->AcquireRowHandle(ev, store);
    }
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

NS_IMETHODIMP
morkTableRowCursor::PrevRow( // get row cells from table for cells already in row
  nsIMdbEnv* mev, // context
  nsIMdbRow** acqRow, // acquire previous row in table
  mdb_pos* outRowPos)
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
      
    mdbOid oid; // place to put oid we intend to ignore
    morkRow* row = PrevRow(ev, &oid, outRowPos);
    if ( row )
    {
      morkStore* store = row->GetRowSpaceStore(ev);
      if ( store )
        outRow = row->AcquireRowHandle(ev, store);
    }
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

// } ----- end row iteration methods -----


// { ----- begin duplicate row removal methods -----
NS_IMETHODIMP
morkTableRowCursor::CanHaveDupRowMembers(nsIMdbEnv* mev, // cursor might hold dups?
  mdb_bool* outCanHaveDups)
{
  mdb_err outErr = 0;
  mdb_bool canHaveDups = mdbBool_kFalse;
  
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    canHaveDups = CanHaveDupRowMembers(ev);
    outErr = ev->AsErr();
  }
  if ( outCanHaveDups )
    *outCanHaveDups = canHaveDups;
  return outErr;
}
  
NS_IMETHODIMP
morkTableRowCursor::MakeUniqueCursor( // clone cursor, removing duplicate rows
  nsIMdbEnv* mev, // context
  nsIMdbTableRowCursor** acqCursor)    // acquire clone with no dups
  // Note that MakeUniqueCursor() is never necessary for a cursor which was
  // created by table method nsIMdbTable::GetTableRowCursor(), because a table
  // never contains the same row as a member more than once.  However, a cursor
  // created by table method nsIMdbTable::FindRowMatches() might contain the
  // same row more than once, because the same row can generate a hit by more
  // than one column with a matching string prefix.  Note this method can
  // return the very same cursor instance with just an incremented refcount,
  // when the original cursor could not contain any duplicate rows (calling
  // CanHaveDupRowMembers() shows this case on a false return).  Otherwise
  // this method returns a different cursor instance.  Callers should not use
  // this MakeUniqueCursor() method lightly, because it tends to defeat the
  // purpose of lazy programming techniques, since it can force creation of
  // an explicit row collection in a new cursor's representation, in order to
  // inspect the row membership and remove any duplicates; this can have big
  // impact if a collection holds tens of thousands of rows or more, when
  // the original cursor with dups simply referenced rows indirectly by row
  // position ranges, without using an explicit row set representation.
  // Callers are encouraged to use nsIMdbCursor::GetCount() to determine
  // whether the row collection is very large (tens of thousands), and to
  // delay calling MakeUniqueCursor() when possible, until a user interface
  // element actually demands the creation of an explicit set representation.
{
  mdb_err outErr = 0;
  nsIMdbTableRowCursor* outCursor = 0;
  
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    AddRef();
    outCursor = this;
      
    outErr = ev->AsErr();
  }
  if ( acqCursor )
    *acqCursor = outCursor;
  return outErr;
}
// } ----- end duplicate row removal methods -----

// } ===== end nsIMdbTableRowCursor methods =====


/*static*/ void
morkTableRowCursor::NonTableRowCursorTypeError(morkEnv* ev)
{
  ev->NewError("non morkTableRowCursor");
}


mdb_pos
morkTableRowCursor::NextRowOid(morkEnv* ev, mdbOid* outOid)
{
  mdb_pos outPos = -1;
  (void) this->NextRow(ev, outOid, &outPos);
  return outPos;
}

mdb_pos
morkTableRowCursor::PrevRowOid(morkEnv* ev, mdbOid* outOid)
{
  mdb_pos outPos = -1;
  (void) this->PrevRow(ev, outOid, &outPos);
  return outPos;
}

mork_bool
morkTableRowCursor::CanHaveDupRowMembers(morkEnv* ev)
{
  return morkBool_kFalse; // false default is correct
}

mork_count
morkTableRowCursor::GetMemberCount(morkEnv* ev)
{
  morkTable* table = mTableRowCursor_Table;
  if ( table )
    return table->mTable_RowArray.mArray_Fill;
  else
    return 0;
}

morkRow*
morkTableRowCursor::PrevRow(morkEnv* ev, mdbOid* outOid, mdb_pos* outPos)
{
  morkRow* outRow = 0;
  mork_pos pos = -1;
  
  morkTable* table = mTableRowCursor_Table;
  if ( table )
  {
    if ( table->IsOpenNode() )
    {
      morkArray* array = &table->mTable_RowArray;
      pos = mCursor_Pos - 1;
        
      if ( pos >= 0 && pos < (mork_pos)(array->mArray_Fill) )
      {
        mCursor_Pos = pos; // update for next time
        morkRow* row = (morkRow*) array->At(pos);
        if ( row )
        {
          if ( row->IsRow() )
          {
            outRow = row;
            *outOid = row->mRow_Oid;
          }
          else
            row->NonRowTypeError(ev);
        }
        else
          ev->NilPointerError();
      }
      else
      {
        outOid->mOid_Scope = 0;
        outOid->mOid_Id = morkId_kMinusOne;
      }
    }
    else
      table->NonOpenNodeError(ev);
  }
  else
    ev->NilPointerError();

  *outPos = pos;
  return outRow;
}

morkRow*
morkTableRowCursor::NextRow(morkEnv* ev, mdbOid* outOid, mdb_pos* outPos)
{
  morkRow* outRow = 0;
  mork_pos pos = -1;
  
  morkTable* table = mTableRowCursor_Table;
  if ( table )
  {
    if ( table->IsOpenNode() )
    {
      morkArray* array = &table->mTable_RowArray;
      pos = mCursor_Pos;
      if ( pos < 0 )
        pos = 0;
      else
        ++pos;
        
      if ( pos < (mork_pos)(array->mArray_Fill) )
      {
        mCursor_Pos = pos; // update for next time
        morkRow* row = (morkRow*) array->At(pos);
        if ( row )
        {
          if ( row->IsRow() )
          {
            outRow = row;
            *outOid = row->mRow_Oid;
          }
          else
            row->NonRowTypeError(ev);
        }
        else
          ev->NilPointerError();
      }
      else
      {
        outOid->mOid_Scope = 0;
        outOid->mOid_Id = morkId_kMinusOne;
      }
    }
    else
      table->NonOpenNodeError(ev);
  }
  else
    ev->NilPointerError();

  *outPos = pos;
  return outRow;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
