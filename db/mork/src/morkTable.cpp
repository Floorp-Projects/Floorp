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

#ifndef _MORKTABLEROWCURSOR_
#include "morkTableRowCursor.h"
#endif

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkTable::CloseMorkNode(morkEnv* ev) /*i*/ // CloseTable() only if open
{
  if ( this->IsOpenNode() )
  {
    morkObject::CloseMorkNode(ev); // give base class a chance.
    this->MarkClosing();
    this->CloseTable(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkTable::~morkTable() /*i*/ // assert CloseTable() executed earlier
{
  CloseMorkNode(mMorkEnv);
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
        mTable_Store = ioStore;
        mTable_RowSpace = ioRowSpace;
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

NS_IMPL_ISUPPORTS_INHERITED1(morkTable, morkObject, nsIMdbTable)

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
      mTable_Store = 0;
      mTable_RowSpace = 0;
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

// { ===== begin nsIMdbCollection methods =====

// { ----- begin attribute methods -----
NS_IMETHODIMP
morkTable::GetSeed(nsIMdbEnv* mev,
  mdb_seed* outSeed)    // member change count
{
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    *outSeed = mTable_RowArray.mArray_Seed;
    outErr = ev->AsErr();
  }
  return outErr;
}
  
NS_IMETHODIMP
morkTable::GetCount(nsIMdbEnv* mev,
  mdb_count* outCount) // member count
{
  NS_ENSURE_ARG_POINTER(outCount);
  *outCount = mTable_RowArray.mArray_Fill;
  return NS_OK;
}

NS_IMETHODIMP
morkTable::GetPort(nsIMdbEnv* mev,
  nsIMdbPort** acqPort) // collection container
{
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  NS_ENSURE_ARG_POINTER(acqPort);    
  *acqPort = mTable_Store;
  return NS_OK;
}
// } ----- end attribute methods -----

// { ----- begin cursor methods -----
NS_IMETHODIMP
morkTable::GetCursor( // make a cursor starting iter at inMemberPos
  nsIMdbEnv* mev, // context
  mdb_pos inMemberPos, // zero-based ordinal pos of member in collection
  nsIMdbCursor** acqCursor) // acquire new cursor instance
{
  return this->GetTableRowCursor(mev, inMemberPos,
    (nsIMdbTableRowCursor**) acqCursor);
}
// } ----- end cursor methods -----

// { ----- begin ID methods -----
NS_IMETHODIMP
morkTable::GetOid(nsIMdbEnv* mev,
  mdbOid* outOid) // read object identity
{
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  GetTableOid(ev, outOid);
  return NS_OK;
}

NS_IMETHODIMP
morkTable::BecomeContent(nsIMdbEnv* mev,
  const mdbOid* inOid) // exchange content
{
  NS_ASSERTION(PR_FALSE, "not implemented"); 
  return NS_ERROR_NOT_IMPLEMENTED;
  // remember table->MaybeDirtySpaceStoreAndTable();
}

// } ----- end ID methods -----

// { ----- begin activity dropping methods -----
NS_IMETHODIMP
morkTable::DropActivity( // tell collection usage no longer expected
  nsIMdbEnv* mev)
{
  NS_ASSERTION(PR_FALSE, "not implemented"); 
  return NS_ERROR_NOT_IMPLEMENTED;
}

// } ----- end activity dropping methods -----

// } ===== end nsIMdbCollection methods =====

// { ===== begin nsIMdbTable methods =====

// { ----- begin attribute methods -----

NS_IMETHODIMP
morkTable::SetTablePriority(nsIMdbEnv* mev, mdb_priority inPrio)
{
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( inPrio > morkPriority_kMax )
      inPrio = morkPriority_kMax;
      
    mTable_Priority = inPrio;
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkTable::GetTablePriority(nsIMdbEnv* mev, mdb_priority* outPrio)
{
  mdb_err outErr = 0;
  mork_priority prio = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    prio = mTable_Priority;
    if ( prio > morkPriority_kMax )
    {
      prio = morkPriority_kMax;
      mTable_Priority = prio;
    }
    outErr = ev->AsErr();
  }
  if ( outPrio )
    *outPrio = prio;
  return outErr;
}


NS_IMETHODIMP
morkTable:: GetTableBeVerbose(nsIMdbEnv* mev, mdb_bool* outBeVerbose)
{
  NS_ENSURE_ARG_POINTER(outBeVerbose);
  *outBeVerbose = IsTableVerbose();
  return NS_OK;
}

NS_IMETHODIMP
morkTable::SetTableBeVerbose(nsIMdbEnv* mev, mdb_bool inBeVerbose)
{
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( inBeVerbose )
      SetTableVerbose();
    else
      ClearTableVerbose();
   
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkTable::GetTableIsUnique(nsIMdbEnv* mev, mdb_bool* outIsUnique)
{
  NS_ENSURE_ARG_POINTER(outIsUnique);
  *outIsUnique = IsTableUnique();
  return NS_OK;
}

NS_IMETHODIMP
morkTable::GetTableKind(nsIMdbEnv* mev, mdb_kind* outTableKind)
{
  NS_ENSURE_ARG_POINTER(outTableKind);
  *outTableKind = mTable_Kind;
  return NS_OK;
}

NS_IMETHODIMP
morkTable::GetRowScope(nsIMdbEnv* mev, mdb_scope* outRowScope)
{
  mdb_err outErr = 0;
  mdb_scope rowScope = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( mTable_RowSpace )
      rowScope = mTable_RowSpace->SpaceScope();
    else
      NilRowSpaceError(ev);

    outErr = ev->AsErr();
  }
  if ( outRowScope )
    *outRowScope = rowScope;
  return outErr;
}

NS_IMETHODIMP
morkTable::GetMetaRow( nsIMdbEnv* mev,
  const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
  mdbOid* outOid, // output meta row oid, can be nil to suppress output
  nsIMdbRow** acqRow) // acquire table's unique singleton meta row
  // The purpose of a meta row is to support the persistent recording of
  // meta info about a table as cells put into the distinguished meta row.
  // Each table has exactly one meta row, which is not considered a member
  // of the collection of rows inside the table.  The only way to tell
  // whether a row is a meta row is by the fact that it is returned by this
  // GetMetaRow() method from some table. Otherwise nothing distinguishes
  // a meta row from any other row.  A meta row can be used anyplace that
  // any other row can be used, and can even be put into other tables (or
  // the same table) as a table member, if this is useful for some reason.
  // The first attempt to access a table's meta row using GetMetaRow() will
  // cause the meta row to be created if it did not already exist.  When the
  // meta row is created, it will have the row oid that was previously
  // requested for this table's meta row; or if no oid was ever explicitly
  // specified for this meta row, then a unique oid will be generated in
  // the row scope named "metaScope" (so obviously MDB clients should not
  // manually allocate any row IDs from that special meta scope namespace).
  // The meta row oid can be specified either when the table is created, or
  // else the first time that GetMetaRow() is called, by passing a non-nil
  // pointer to an oid for parameter inOptionalMetaRowOid.  The meta row's
  // actual oid is returned in outOid (if this is a non-nil pointer), and
  // it will be different from inOptionalMetaRowOid when the meta row was
  // already given a different oid earlier.
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRow* row = GetMetaRow(ev, inOptionalMetaRowOid);
    if ( row && ev->Good() )
    {
      if ( outOid )
        *outOid = row->mRow_Oid;
        
      outRow = row->AcquireRowHandle(ev, mTable_Store);
    }
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
    
  if ( ev->Bad() && outOid )
  {
    outOid->mOid_Scope = 0;
    outOid->mOid_Id = morkRow_kMinusOneRid;
  }
  return outErr;
}

// } ----- end attribute methods -----

// { ----- begin cursor methods -----
NS_IMETHODIMP
morkTable::GetTableRowCursor( // make a cursor, starting iteration at inRowPos
  nsIMdbEnv* mev, // context
  mdb_pos inRowPos, // zero-based ordinal position of row in table
  nsIMdbTableRowCursor** acqCursor) // acquire new cursor instance
{
  mdb_err outErr = 0;
  nsIMdbTableRowCursor* outCursor = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkTableRowCursor* cursor = NewTableRowCursor(ev, inRowPos);
    if ( cursor )
    {
      if ( ev->Good() )
      {
        // cursor->mCursor_Seed = (mork_seed) inRowPos;
        outCursor = cursor;
        outCursor->AddRef();
      }
    }
      
    outErr = ev->AsErr();
  }
  if ( acqCursor )
    *acqCursor = outCursor;
  return outErr;
}
// } ----- end row position methods -----

// { ----- begin row position methods -----
NS_IMETHODIMP
morkTable::PosToOid( // get row member for a table position
  nsIMdbEnv* mev, // context
  mdb_pos inRowPos, // zero-based ordinal position of row in table
  mdbOid* outOid) // row oid at the specified position
{
  mdb_err outErr = 0;
  mdbOid roid;
  roid.mOid_Scope = 0;
  roid.mOid_Id = (mork_id) -1;
  
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRow* row = SafeRowAt(ev, inRowPos);
    if ( row )
      roid = row->mRow_Oid;
    
    outErr = ev->AsErr();
  }
  if ( outOid )
    *outOid = roid;
  return outErr;
}

NS_IMETHODIMP
morkTable::OidToPos( // test for the table position of a row member
  nsIMdbEnv* mev, // context
  const mdbOid* inOid, // row to find in table
  mdb_pos* outPos) // zero-based ordinal position of row in table
{
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    mork_pos pos = ArrayHasOid(ev, inOid);
    if ( outPos )
      *outPos = pos;
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkTable::PosToRow( // get row member for a table position
  nsIMdbEnv* mev, // context
  mdb_pos inRowPos, // zero-based ordinal position of row in table
  nsIMdbRow** acqRow) // acquire row at table position inRowPos
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRow* row = SafeRowAt(ev, inRowPos);
    if ( row && mTable_Store )
      outRow = row->AcquireRowHandle(ev, mTable_Store);
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

NS_IMETHODIMP
morkTable::RowToPos( // test for the table position of a row member
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow, // row to find in table
  mdb_pos* outPos) // zero-based ordinal position of row in table
{
  mdb_err outErr = 0;
  mork_pos pos = -1;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRowObject* row = (morkRowObject*) ioRow;
    pos = ArrayHasOid(ev, &row->mRowObject_Row->mRow_Oid);
    outErr = ev->AsErr();
  }
  if ( outPos )
    *outPos = pos;
  return outErr;
}
  
// Note that HasRow() performs the inverse oid->pos mapping
// } ----- end row position methods -----

// { ----- begin oid set methods -----
NS_IMETHODIMP
morkTable::AddOid( // make sure the row with inOid is a table member 
  nsIMdbEnv* mev, // context
  const mdbOid* inOid) // row to ensure membership in table
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkTable::HasOid( // test for the table position of a row member
  nsIMdbEnv* mev, // context
  const mdbOid* inOid, // row to find in table
  mdb_bool* outHasOid) // whether inOid is a member row
{
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( outHasOid )
      *outHasOid = MapHasOid(ev, inOid);
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkTable::CutOid( // make sure the row with inOid is not a member 
  nsIMdbEnv* mev, // context
  const mdbOid* inOid) // row to remove from table
{
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( inOid && mTable_Store )
    {
      morkRow* row = mTable_Store->GetRow(ev, inOid);
      if ( row )
        CutRow(ev, row);
    }
    else
      ev->NilPointerError();
      
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end oid set methods -----

// { ----- begin row set methods -----
NS_IMETHODIMP
morkTable::NewRow( // create a new row instance in table
  nsIMdbEnv* mev, // context
  mdbOid* ioOid, // please use zero (unbound) rowId for db-assigned IDs
  nsIMdbRow** acqRow) // create new row
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( ioOid && mTable_Store )
    {
      morkRow* row = 0;
      if ( ioOid->mOid_Id == morkRow_kMinusOneRid )
        row = mTable_Store->NewRow(ev, ioOid->mOid_Scope);
      else
        row = mTable_Store->NewRowWithOid(ev, ioOid);
        
      if ( row && AddRow(ev, row) )
        outRow = row->AcquireRowHandle(ev, mTable_Store);
    }
    else
      ev->NilPointerError();
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

NS_IMETHODIMP
morkTable::AddRow( // make sure the row with inOid is a table member 
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow) // row to ensure membership in table
{
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRowObject *rowObj = (morkRowObject *) ioRow;
    morkRow* row = rowObj->mRowObject_Row;
    AddRow(ev, row);
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkTable::HasRow( // test for the table position of a row member
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow, // row to find in table
  mdb_bool* outBool) // zero-based ordinal position of row in table
{
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRowObject *rowObj = (morkRowObject *) ioRow;
    morkRow* row = rowObj->mRowObject_Row;
    if ( outBool )
      *outBool = MapHasOid(ev, &row->mRow_Oid);
    outErr = ev->AsErr();
  }
  return outErr;
}


NS_IMETHODIMP
morkTable::CutRow( // make sure the row with inOid is not a member 
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow) // row to remove from table
{
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRowObject *rowObj = (morkRowObject *) ioRow;
    morkRow* row = rowObj->mRowObject_Row;
    CutRow(ev, row);
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkTable::CutAllRows( // remove all rows from the table 
  nsIMdbEnv* mev) // context
{
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    CutAllRows(ev);
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end row set methods -----

// { ----- begin searching methods -----
NS_IMETHODIMP
morkTable::FindRowMatches( // search variable number of sorted cols
  nsIMdbEnv* mev, // context
  const mdbYarn* inPrefix, // content to find as prefix in row's column cell
  nsIMdbTableRowCursor** acqCursor) // set of matching rows
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
  
NS_IMETHODIMP
morkTable::GetSearchColumns( // query columns used by FindRowMatches()
  nsIMdbEnv* mev, // context
  mdb_count* outCount, // context
  mdbColumnSet* outColSet) // caller supplied space to put columns
  // GetSearchColumns() returns the columns actually searched when the
  // FindRowMatches() method is called.  No more than mColumnSet_Count
  // slots of mColumnSet_Columns will be written, since mColumnSet_Count
  // indicates how many slots are present in the column array.  The
  // actual number of search column used by the table is returned in
  // the outCount parameter; if this number exceeds mColumnSet_Count,
  // then a caller needs a bigger array to read the entire column set.
  // The minimum of mColumnSet_Count and outCount is the number slots
  // in mColumnSet_Columns that were actually written by this method.
  //
  // Callers are expected to change this set of columns by calls to
  // nsIMdbTable::SearchColumnsHint() or SetSearchSorting(), or both.
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
// } ----- end searching methods -----

// { ----- begin hinting methods -----
NS_IMETHODIMP
morkTable::SearchColumnsHint( // advise re future expected search cols  
  nsIMdbEnv* mev, // context
  const mdbColumnSet* inColumnSet) // columns likely to be searched
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
  
NS_IMETHODIMP
morkTable::SortColumnsHint( // advise re future expected sort columns  
  nsIMdbEnv* mev, // context
  const mdbColumnSet* inColumnSet) // columns for likely sort requests
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkTable::StartBatchChangeHint( // advise before many adds and cuts  
  nsIMdbEnv* mev, // context
  const void* inLabel) // intend unique address to match end call
  // If batch starts nest by virtue of nesting calls in the stack, then
  // the address of a local variable makes a good batch start label that
  // can be used at batch end time, and such addresses remain unique.
{
  // we don't do anything here.
  return NS_OK;
}

NS_IMETHODIMP
morkTable::EndBatchChangeHint( // advise before many adds and cuts  
  nsIMdbEnv* mev, // context
  const void* inLabel) // label matching start label
  // Suppose a table is maintaining one or many sort orders for a table,
  // so that every row added to the table must be inserted in each sort,
  // and every row cut must be removed from each sort.  If a db client
  // intends to make many such changes before needing any information
  // about the order or positions of rows inside a table, then a client
  // might tell the table to start batch changes in order to disable
  // sorting of rows for the interim.  Presumably a table will then do
  // a full sort of all rows at need when the batch changes end, or when
  // a surprise request occurs for row position during batch changes.
{
  // we don't do anything here.
  return NS_OK;
}
// } ----- end hinting methods -----

// { ----- begin sorting methods -----
// sorting: note all rows are assumed sorted by row ID as a secondary
// sort following the primary column sort, when table rows are sorted.

NS_IMETHODIMP
morkTable::CanSortColumn( // query which column is currently used for sorting
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to query sorting potential
  mdb_bool* outCanSort) // whether the column can be sorted
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkTable::GetSorting( // view same table in particular sorting
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // requested new column for sorting table
  nsIMdbSorting** acqSorting) // acquire sorting for column
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkTable::SetSearchSorting( // use this sorting in FindRowMatches()
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // often same as nsIMdbSorting::GetSortColumn()
  nsIMdbSorting* ioSorting) // requested sorting for some column
  // SetSearchSorting() attempts to inform the table that ioSorting
  // should be used during calls to FindRowMatches() for searching
  // the column which is actually sorted by ioSorting.  This method
  // is most useful in conjunction with nsIMdbSorting::SetCompare(),
  // because otherwise a caller would not be able to override the
  // comparison ordering method used during searchs.  Note that some
  // database implementations might be unable to use an arbitrarily
  // specified sort order, either due to schema or runtime interface
  // constraints, in which case ioSorting might not actually be used.
  // Presumably ioSorting is an instance that was returned from some
  // earlier call to nsIMdbTable::GetSorting().  A caller can also
  // use nsIMdbTable::SearchColumnsHint() to specify desired change
  // in which columns are sorted and searched by FindRowMatches().
  //
  // A caller can pass a nil pointer for ioSorting to request that
  // column inColumn no longer be used at all by FindRowMatches().
  // But when ioSorting is non-nil, then inColumn should match the
  // column actually sorted by ioSorting; when these do not agree,
  // implementations are instructed to give precedence to the column
  // specified by ioSorting (so this means callers might just pass
  // zero for inColumn when ioSorting is also provided, since then
  // inColumn is both redundant and ignored).
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// } ----- end sorting methods -----

// { ----- begin moving methods -----
// moving a row does nothing unless a table is currently unsorted

NS_IMETHODIMP
morkTable::MoveOid( // change position of row in unsorted table
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // row oid to find in table
  mdb_pos inHintFromPos, // suggested hint regarding start position
  mdb_pos inToPos,       // desired new position for row inOid
  mdb_pos* outActualPos) // actual new position of row in table
{
  mdb_err outErr = 0;
  mdb_pos actualPos = -1; // meaning it was never found in table
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( inOid && mTable_Store )
    {
      morkRow* row = mTable_Store->GetRow(ev, inOid);
      if ( row )
        actualPos = MoveRow(ev, row, inHintFromPos, inToPos);
    }
    else
      ev->NilPointerError();

    outErr = ev->AsErr();
  }
  if ( outActualPos )
    *outActualPos = actualPos;
  return outErr;
}

NS_IMETHODIMP
morkTable::MoveRow( // change position of row in unsorted table
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow,  // row oid to find in table
  mdb_pos inHintFromPos, // suggested hint regarding start position
  mdb_pos inToPos,       // desired new position for row ioRow
  mdb_pos* outActualPos) // actual new position of row in table
{
  mdb_pos actualPos = -1; // meaning it was never found in table
  mdb_err outErr = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRowObject *rowObj = (morkRowObject *) ioRow;
    morkRow* row = rowObj->mRowObject_Row;
    actualPos = MoveRow(ev, row, inHintFromPos, inToPos);
    outErr = ev->AsErr();
  }
  if ( outActualPos )
    *outActualPos = actualPos;
  return outErr;
}
// } ----- end moving methods -----

// { ----- begin index methods -----
NS_IMETHODIMP
morkTable::AddIndex( // create a sorting index for column if possible
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // the column to sort by index
  nsIMdbThumb** acqThumb) // acquire thumb for incremental index building
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the index addition will be finished.
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkTable::CutIndex( // stop supporting a specific column index
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // the column with index to be removed
  nsIMdbThumb** acqThumb) // acquire thumb for incremental index destroy
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the index removal will be finished.
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkTable::HasIndex( // query for current presence of a column index
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // the column to investigate
  mdb_bool* outHasIndex) // whether column has index for this column
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkTable::EnableIndexOnSort( // create an index for col on first sort
  nsIMdbEnv* mev, // context
  mdb_column inColumn) // the column to index if ever sorted
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkTable::QueryIndexOnSort( // check whether index on sort is enabled
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // the column to investigate
  mdb_bool* outIndexOnSort) // whether column has index-on-sort enabled
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkTable::DisableIndexOnSort( // prevent future index creation on sort
  nsIMdbEnv* mev, // context
  mdb_column inColumn) // the column to index if ever sorted
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
// } ----- end index methods -----

// } ===== end nsIMdbTable methods =====

// we override these so that we'll use the xpcom add and release ref.
mork_refs
morkTable::AddStrongRef(morkEnv *ev)
{
  return (mork_refs) AddRef();
}

mork_refs
morkTable::CutStrongRef(morkEnv *ev)
{
  return (mork_refs) Release();
}

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
  AddRef();
  return this;
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
        cursor->CutStrongRef((nsIMdbEnv *) ev);
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

