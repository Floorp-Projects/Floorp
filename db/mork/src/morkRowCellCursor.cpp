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

orkinRowCellCursor*
morkRowCellCursor::AcquireRowCellCursorHandle(morkEnv* ev)
{
  orkinRowCellCursor* outCursor = 0;
  orkinRowCellCursor* c = (orkinRowCellCursor*) mObject_Handle;
  if ( c ) // have an old handle?
    c->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    c = orkinRowCellCursor::MakeRowCellCursor(ev, this);
    mObject_Handle = c;
  }
  if ( c )
    outCursor = c;
  return outCursor;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
