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

/*static*/ void
morkTableRowCursor::NonTableRowCursorTypeError(morkEnv* ev)
{
  ev->NewError("non morkTableRowCursor");
}


orkinTableRowCursor*
morkTableRowCursor::AcquireUniqueRowCursorHandle(morkEnv* ev)
{
  return this->AcquireTableRowCursorHandle(ev);
}

orkinTableRowCursor*
morkTableRowCursor::AcquireTableRowCursorHandle(morkEnv* ev)
{
  orkinTableRowCursor* outCursor = 0;
  orkinTableRowCursor* c = (orkinTableRowCursor*) mObject_Handle;
  if ( c ) // have an old handle?
    c->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    c = orkinTableRowCursor::MakeTableRowCursor(ev, this);
    mObject_Handle = c;
  }
  if ( c )
    outCursor = c;
  return outCursor;
}

mdb_pos
morkTableRowCursor::NextRowOid(morkEnv* ev, mdbOid* outOid)
{
  mdb_pos outPos = -1;
  (void) this->NextRow(ev, outOid, &outPos);
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
