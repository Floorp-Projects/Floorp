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

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _ORKINROW_
#include "orkinRow.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkRowObject::CloseMorkNode(morkEnv* ev) // CloseRowObject() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseRowObject(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkRowObject::~morkRowObject() // assert CloseRowObject() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkRowObject::morkRowObject(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap,
     morkRow* ioRow, morkStore* ioStore)
: morkObject(ev, inUsage, ioHeap, morkColor_kNone, (morkHandle*) 0)
, mRowObject_Row( 0 )
, mRowObject_Store( 0 )
{
  if ( ev->Good() )
  {
    if ( ioRow && ioStore )
    {
      mRowObject_Row = ioRow;
      morkStore::SlotWeakStore(ioStore, ev, &mRowObject_Store);
      
      if ( ev->Good() )
        mNode_Derived = morkDerived_kRowObject;
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkRowObject::CloseRowObject(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      morkRow* row = mRowObject_Row;
      mRowObject_Row = 0;
      this->CloseObject(ev);
      this->MarkShut();

      if ( row )
      {
        MORK_ASSERT(row->mRow_Object == this);
        if ( row->mRow_Object == this )
        {
          row->mRow_Object = 0; // just nil this slot -- cut ref down below
          
          morkStore::SlotWeakStore((morkStore*) 0, ev,
            &mRowObject_Store);
            
          this->CutWeakRef(ev); // do last, because it might self destroy
        }
      }
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
morkRowObject::NonRowObjectTypeError(morkEnv* ev)
{
  ev->NewError("non morkRowObject");
}

/*static*/ void
morkRowObject::NilRowError(morkEnv* ev)
{
  ev->NewError("nil mRowObject_Row");
}

/*static*/ void
morkRowObject::NilStoreError(morkEnv* ev)
{
  ev->NewError("nil mRowObject_Store");
}

/*static*/ void
morkRowObject::RowObjectRowNotSelfError(morkEnv* ev)
{
  ev->NewError("mRowObject_Row->mRow_Object != self");
}


nsIMdbRow*
morkRowObject::AcquireRowHandle(morkEnv* ev) // mObject_Handle
{
  nsIMdbRow* outRow = 0;
  orkinRow* r = (orkinRow*) mObject_Handle;
  if ( r ) // have an old handle?
    r->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    r = orkinRow::MakeRow(ev, this);
    mObject_Handle = r;
  }
  if ( r )
    outRow = r;
  return outRow;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
