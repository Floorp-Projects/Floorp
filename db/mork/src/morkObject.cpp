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

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

#ifndef _MORKHANDLE_
#include "morkHandle.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkObject::CloseMorkNode(morkEnv* ev) // CloseObject() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseObject(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkObject::~morkObject() // assert CloseObject() executed earlier
{
  MORK_ASSERT(mObject_Handle==0);
}

/*public non-poly*/
morkObject::morkObject(const morkUsage& inUsage, nsIMdbHeap* ioHeap,
  mork_color inBeadColor)
: morkBead(inUsage, ioHeap, inBeadColor)
, mObject_Handle( 0 )
{
}

/*public non-poly*/
morkObject::morkObject(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
  mork_color inBeadColor, morkHandle* ioHandle)
: morkBead(ev, inUsage, ioHeap, inBeadColor)
, mObject_Handle( 0 )
{
  if ( ev->Good() )
  {
    if ( ioHandle )
      morkHandle::SlotWeakHandle(ioHandle, ev, &mObject_Handle);
      
    if ( ev->Good() )
      mNode_Derived = morkDerived_kObject;
  }
}

/*public non-poly*/ void
morkObject::CloseObject(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      if ( !this->IsShutNode() )
      {
        if ( mObject_Handle )
          morkHandle::SlotWeakHandle((morkHandle*) 0L, ev, &mObject_Handle);
          
        mBead_Color = 0; // this->CloseBead(ev);
        this->MarkShut();
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


//void morkObject::NewNilHandleError(morkEnv* ev) // mObject_Handle is nil
//{
//  ev->NewError("nil mObject_Handle");
//}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
