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

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKCELLOBJECT_
#include "morkCellObject.h"
#endif

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

#ifndef _ORKINCELL_
#include "orkinCell.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkCellObject::CloseMorkNode(morkEnv* ev) // CloseCellObject() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseCellObject(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkCellObject::~morkCellObject() // assert CloseCellObject() executed earlier
{
  MORK_ASSERT(mCellObject_Row==0);
}

/*public non-poly*/
morkCellObject::morkCellObject(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, morkRow* ioRow, morkCell* ioCell,
  mork_column inCol, mork_pos inPos)
: morkObject(ev, inUsage, ioHeap, morkColor_kNone, (morkHandle*) 0)
, mCellObject_RowObject( 0 )
, mCellObject_Row( 0 )
, mCellObject_Cell( 0 )
, mCellObject_Col( inCol )
, mCellObject_RowSeed( 0 )
, mCellObject_Pos( (mork_u2) inPos )
{
  if ( ev->Good() )
  {
    if ( ioRow && ioCell )
    {
      if ( ioRow->IsRow() )
      {
        morkStore* store = ioRow->GetRowSpaceStore(ev);
        if ( store )
        {
          morkRowObject* rowObj = ioRow->AcquireRowObject(ev, store);
          if ( rowObj )
          {
            mCellObject_Row = ioRow;
            mCellObject_Cell = ioCell;
            mCellObject_RowSeed = ioRow->mRow_Seed;
            
            // morkRowObject::SlotStrongRowObject(rowObj, ev,
            //  &mCellObject_RowObject);
              
            mCellObject_RowObject = rowObj; // assume control of strong ref
          }
          if ( ev->Good() )
            mNode_Derived = morkDerived_kCellObject;
        }
      }
      else
        ioRow->NonRowTypeError(ev);
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkCellObject::CloseCellObject(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      morkRowObject::SlotStrongRowObject((morkRowObject*) 0, ev,
        &mCellObject_RowObject);
      mCellObject_Row = 0;
      mCellObject_Cell = 0;
      mCellObject_RowSeed = 0;
      this->CloseObject(ev);
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

mork_bool
morkCellObject::ResyncWithRow(morkEnv* ev)
{
  morkRow* row = mCellObject_Row;
  mork_pos pos = 0;
  morkCell* cell = row->GetCell(ev, mCellObject_Col, &pos);
  if ( cell )
  {
    mCellObject_Pos = (mork_u2) pos;
    mCellObject_Cell = cell;
    mCellObject_RowSeed = row->mRow_Seed;
  }
  else
  {
    mCellObject_Cell = 0;
    this->MissingRowColumnError(ev);
  }
  return ev->Good();
}

morkAtom*
morkCellObject::GetCellAtom(morkEnv* ev) const
{
  morkCell* cell = mCellObject_Cell;
  if ( cell )
    return cell->GetAtom();
  else
    this->NilCellError(ev);
    
  return (morkAtom*) 0;
}

/*static*/ void
morkCellObject::WrongRowObjectRowError(morkEnv* ev)
{
  ev->NewError("mCellObject_Row != mCellObject_RowObject->mRowObject_Row");
}

/*static*/ void
morkCellObject::NilRowError(morkEnv* ev)
{
  ev->NewError("nil mCellObject_Row");
}

/*static*/ void
morkCellObject::NilRowObjectError(morkEnv* ev)
{
  ev->NewError("nil mCellObject_RowObject");
}

/*static*/ void
morkCellObject::NilCellError(morkEnv* ev)
{
  ev->NewError("nil mCellObject_Cell");
}

/*static*/ void
morkCellObject::NonCellObjectTypeError(morkEnv* ev)
{
  ev->NewError("non morkCellObject");
}

/*static*/ void
morkCellObject::MissingRowColumnError(morkEnv* ev)
{
  ev->NewError("mCellObject_Col not in mCellObject_Row");
}

nsIMdbCell*
morkCellObject::AcquireCellHandle(morkEnv* ev)
{
  nsIMdbCell* outCell = 0;
  orkinCell* c = (orkinCell*) mObject_Handle;
  if ( c ) // have an old handle?
    c->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    c = orkinCell::MakeCell(ev, this);
    mObject_Handle = c;
  }
  if ( c )
    outCell = c;
  return outCell;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
