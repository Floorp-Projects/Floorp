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

#ifndef _ORKINSORTING_
#include "orkinSorting.h"
#endif

#ifndef _MORKTABLEROWCURSOR_
#include "morkTableRowCursor.h"
#endif

#ifndef _MORKSORTING_
#include "morkSorting.h"
#endif

#ifndef _MORKQUICKSORT_
#include "morkQuickSort.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkSorting::CloseMorkNode(morkEnv* ev) /*i*/ // CloseSorting() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseSorting(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkSorting::~morkSorting() /*i*/ // assert CloseSorting() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
  MORK_ASSERT(mSorting_Table==0);
}

#define morkSorting_kExtraSlots 2 /* space for two more rows */

/*public non-poly*/
morkSorting::morkSorting(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioNodeHeap, morkTable* ioTable,
    nsIMdbCompare* ioCompare,
    nsIMdbHeap* ioSlotHeap, mork_column inCol)
: morkObject(ev, inUsage, ioNodeHeap, morkColor_kNone, (morkHandle*) 0)
, mSorting_Table( 0 )

, mSorting_Compare( 0 )

, mSorting_RowArray(ev, morkUsage::kMember, (nsIMdbHeap*) 0,
  ioTable->GetRowCount() + morkSorting_kExtraSlots, ioSlotHeap)
  
, mSorting_Col( inCol )
{  
  if ( ev->Good() )
  {
    if ( ioTable && ioSlotHeap && ioCompare )
    {
      if ( inCol )
      {
        nsIMdbCompare_SlotStrongCompare(ioCompare, ev, &mSorting_Compare);
        morkTable::SlotWeakTable(ioTable, ev, &mSorting_Table);
        if ( ev->Good() )
        {
          mNode_Derived = morkDerived_kSorting;
        }
      }
      else
        this->ZeroColError(ev);
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkSorting::CloseSorting(morkEnv* ev) /*i*/ // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      nsIMdbCompare_SlotStrongCompare((nsIMdbCompare*) 0, ev,
        &mSorting_Compare);
      morkTable::SlotWeakTable((morkTable*) 0, ev, &mSorting_Table);
      mSorting_RowArray.CloseMorkNode(ev);
      mSorting_Col = 0;
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
morkSorting::NonSortingTypeError(morkEnv* ev)
{
  ev->NewError("non morkSorting");
}

/*static*/ void
morkSorting::NonSortingTypeWarning(morkEnv* ev)
{
  ev->NewWarning("non morkSorting");
}

/*static*/ void
morkSorting::NilTableError(morkEnv* ev)
{
  ev->NewError("nil mSorting_Table");
}

/*static*/ void
morkSorting::NilCompareError(morkEnv* ev)
{
  ev->NewError("nil mSorting_Compare");
}

/*static*/ void
morkSorting::ZeroColError(morkEnv* ev)
{
  ev->NewError("zero mSorting_Col");
}

nsIMdbSorting*
morkSorting::AcquireSortingHandle(morkEnv* ev)
{
  nsIMdbSorting* outSorting = 0;
  orkinSorting* s = (orkinSorting*) mObject_Handle;
  if ( s ) // have an old handle?
    s->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    s = orkinSorting::MakeSorting(ev, this);
    mObject_Handle = s;
  }
  if ( s )
    outSorting = s;
  return outSorting;
}


class morkSortClosure {
public:

  morkEnv*     mSortClosure_Env;
  morkSorting* mSortClosure_Sorting;
  
public:
  morkSortClosure(morkEnv* ev, morkSorting* ioSorting);
};

morkSortClosure::morkSortClosure(morkEnv* ev, morkSorting* ioSorting)
  : mSortClosure_Env(ev), mSortClosure_Sorting(ioSorting)
{
}

static mdb_order morkRow_Order(const morkRow* inA, const morkRow* inB, 
  morkSortClosure* ioClosure)
{
  return 0;  
}

void morkSorting::sort_rows(morkEnv* ev)
{
  morkTable* table = mSorting_Table;
  if ( table )
  {
    morkArray* tra = &table->mTable_RowArray;
    mork_count count = mSorting_RowArray.mArray_Fill;
    if ( this->is_seed_stale() || count != tra->mArray_Fill )
      count = this->copy_table_row_array(ev);
    
    if ( ev->Good() )
    {
      void** slots = mSorting_RowArray.mArray_Slots;
      morkSortClosure closure(ev, this);
      
      morkQuickSort((mork_u1*) slots, count, sizeof(morkRow*), 
        (mdbAny_Order) morkRow_Order, &closure);
    }
  }
  else
    this->NilTableError(ev);
}

mork_count morkSorting::copy_table_row_array(morkEnv* ev)
{
  morkArray* tra = &mSorting_Table->mTable_RowArray;
  mork_bool bigEnough = mSorting_RowArray.mArray_Size > tra->mArray_Fill;
  if ( !bigEnough )
    bigEnough = mSorting_RowArray.Grow(ev, tra->mArray_Fill);
    
  if ( ev->Good() && bigEnough )
  {
    mSorting_RowArray.mArray_Fill = tra->mArray_Fill;
    morkRow** src = (morkRow**) tra->mArray_Slots;
    morkRow** dst = (morkRow**) mSorting_RowArray.mArray_Slots;
    morkRow** end = dst + tra->mArray_Fill;
    
    while ( dst < end )
      *dst++ = *src++;

    this->sync_with_table_seed();
  }
    
  return mSorting_RowArray.mArray_Fill;
}

mork_pos
morkSorting::ArrayHasOid(morkEnv* ev, const mdbOid* inOid)
{
  MORK_USED_1(ev); 
  mork_count count = mSorting_RowArray.mArray_Fill;
  mork_pos pos = -1;
  while ( ++pos < (mork_pos)count )
  {
    morkRow* row = (morkRow*) mSorting_RowArray.At(pos);
    MORK_ASSERT(row);
    if ( row && row->EqualOid(inOid) )
    {
      return pos;
    }
  }
  return -1;
}

mork_bool
morkSorting::AddRow(morkEnv* ev, morkRow* ioRow)
{
  MORK_USED_1(ioRow);
  return ev->Good();
}

mork_bool
morkSorting::CutRow(morkEnv* ev, morkRow* ioRow)
{
  MORK_USED_1(ioRow);
  return ev->Good();
}


mork_bool
morkSorting::CutAllRows(morkEnv* ev)
{
  return ev->Good();
}

morkSortingRowCursor*
morkSorting::NewSortingRowCursor(morkEnv* ev, mork_pos inRowPos)
{
  morkSortingRowCursor* outCursor = 0;
  if ( ev->Good() )
  {
//    nsIMdbHeap* heap = mSorting_Table->mTable_Store->mPort_Heap;
//    morkSortingRowCursor* cursor = 0;
      
    // $$$$$
    // morkSortingRowCursor* cursor = new(*heap, ev)
    //  morkSortingRowCursor(ev, morkUsage::kHeap, heap, this, inRowPos);
    // if ( cursor )
    // {
    //   if ( ev->Good() )
    //     outCursor = cursor;
    //   else
    //     cursor->CutStrongRef(ev);
    // }
  }
  return outCursor;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


morkSortingMap::~morkSortingMap()
{
}

morkSortingMap::morkSortingMap(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
  : morkNodeMap(ev, inUsage, ioHeap, ioSlotHeap)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kSortingMap;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
