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

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKCURSOR_
#include "morkCursor.h"
#endif

#ifndef _MORKSEARCHROWCURSOR_
#include "morkSearchRowCursor.h"
#endif

#ifndef _MORKUNIQROWCURSOR_
#include "morkUniqRowCursor.h"
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
morkSearchRowCursor::CloseMorkNode(morkEnv* ev) // CloseSearchRowCursor() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseSearchRowCursor(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkSearchRowCursor::~morkSearchRowCursor() // CloseSearchRowCursor() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkSearchRowCursor::morkSearchRowCursor(morkEnv* ev,
  const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, morkTable* ioTable, mork_pos inRowPos)
: morkTableRowCursor(ev, inUsage, ioHeap, ioTable, inRowPos)
// , mSortingRowCursor_Sorting( 0 )
{
  if ( ev->Good() )
  {
    if ( ioTable )
    {
      // morkSorting::SlotWeakSorting(ioSorting, ev, &mSortingRowCursor_Sorting);
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
morkSearchRowCursor::CloseSearchRowCursor(morkEnv* ev) 
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      // morkSorting::SlotWeakSorting((morkSorting*) 0, ev, &mSortingRowCursor_Sorting);
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
morkSearchRowCursor::NonSearchRowCursorTypeError(morkEnv* ev)
{
  ev->NewError("non morkSearchRowCursor");
}

morkUniqRowCursor*
morkSearchRowCursor::MakeUniqCursor(morkEnv* ev)
{
  morkUniqRowCursor* outCursor = 0;
  
  return outCursor;
}


orkinTableRowCursor*
morkSearchRowCursor::AcquireUniqueRowCursorHandle(morkEnv* ev)
{
  orkinTableRowCursor* outCursor = 0;
  
  morkUniqRowCursor* uniqCursor = this->MakeUniqCursor(ev);
  if ( uniqCursor )
  {
    outCursor = uniqCursor->AcquireTableRowCursorHandle(ev);
    uniqCursor->CutStrongRef(ev);
  }
  return outCursor;
}

mork_bool
morkSearchRowCursor::CanHaveDupRowMembers(morkEnv* ev)
{
  return morkBool_kTrue; // true is correct
}

mork_count
morkSearchRowCursor::GetMemberCount(morkEnv* ev)
{
  morkTable* table = mTableRowCursor_Table;
  if ( table )
    return table->mTable_RowArray.mArray_Fill;
  else
    return 0;
}

morkRow*
morkSearchRowCursor::NextRow(morkEnv* ev, mdbOid* outOid, mdb_pos* outPos)
{
  morkRow* outRow = 0;
  mork_pos pos = -1;
  
  morkTable* table = mTableRowCursor_Table;
  if ( table )
  {
  }
  else
    ev->NilPointerError();

  *outPos = pos;
  return outRow;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
