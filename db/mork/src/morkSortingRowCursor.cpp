/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKCURSOR_
#include "morkCursor.h"
#endif

#ifndef _MORKSORTINGROWCURSOR_
#include "morkSortingRowCursor.h"
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

#ifndef _MORKSORTING_
#include "morkSorting.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkSortingRowCursor::CloseMorkNode(morkEnv* ev) // CloseSortingRowCursor() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseSortingRowCursor(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkSortingRowCursor::~morkSortingRowCursor() // CloseSortingRowCursor() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkSortingRowCursor::morkSortingRowCursor(morkEnv* ev,
  const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, morkTable* ioTable, mork_pos inRowPos,
  morkSorting* ioSorting)
: morkTableRowCursor(ev, inUsage, ioHeap, ioTable, inRowPos)
, mSortingRowCursor_Sorting( 0 )
{
  if ( ev->Good() )
  {
    if ( ioSorting )
    {
      morkSorting::SlotWeakSorting(ioSorting, ev, &mSortingRowCursor_Sorting);
      if ( ev->Good() )
      {
        // mNode_Derived = morkDerived_kTableRowCursor;
        // mNode_Derived must stay equal to  kTableRowCursor
      }
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkSortingRowCursor::CloseSortingRowCursor(morkEnv* ev) 
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mCursor_Pos = -1;
      mCursor_Seed = 0;
      morkSorting::SlotWeakSorting((morkSorting*) 0, ev, &mSortingRowCursor_Sorting);
      this->CloseTableRowCursor(ev);
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
morkSortingRowCursor::NonSortingRowCursorTypeError(morkEnv* ev)
{
  ev->NewError("non morkSortingRowCursor");
}

orkinTableRowCursor*
morkSortingRowCursor::AcquireUniqueRowCursorHandle(morkEnv* ev)
{
  return this->AcquireTableRowCursorHandle(ev);
}

mork_bool
morkSortingRowCursor::CanHaveDupRowMembers(morkEnv* ev)
{
  return morkBool_kFalse; // false is correct
}

mork_count
morkSortingRowCursor::GetMemberCount(morkEnv* ev)
{
  morkTable* table = mTableRowCursor_Table;
  if ( table )
    return table->mTable_RowArray.mArray_Fill;
  else
    return 0;

  // morkSorting* sorting = mSortingRowCursor_Sorting;
  // if ( sorting )
  //   return sorting->mSorting_RowArray.mArray_Fill;
  // else
  //   return 0;
}

morkRow*
morkSortingRowCursor::NextRow(morkEnv* ev, mdbOid* outOid, mdb_pos* outPos)
{
  morkRow* outRow = 0;
  mork_pos pos = -1;
  
  morkSorting* sorting = mSortingRowCursor_Sorting;
  if ( sorting )
  {
    if ( sorting->IsOpenNode() )
    {
      morkArray* array = &sorting->mSorting_RowArray;
      pos = mCursor_Pos;
      if ( pos < 0 )
        pos = 0;
      else
        ++pos;
        
      if ( pos < (mork_pos)(array->mArray_Fill))
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
      sorting->NonOpenNodeError(ev);
  }
  else
    ev->NilPointerError();

  *outPos = pos;
  return outRow;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
