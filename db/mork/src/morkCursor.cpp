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

#ifndef _MORKCURSOR_
#include "morkCursor.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkCursor::CloseMorkNode(morkEnv* ev) // CloseCursor() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseCursor(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkCursor::~morkCursor() // assert CloseCursor() executed earlier
{
}

/*public non-poly*/
morkCursor::morkCursor(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap)
: morkObject(ev, inUsage, ioHeap, morkColor_kNone, (morkHandle*) 0)
, mCursor_Seed( 0 )
, mCursor_Pos( -1 )
, mCursor_DoFailOnSeedOutOfSync( morkBool_kFalse )
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kCursor;
}

/*public non-poly*/ void
morkCursor::CloseCursor(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mCursor_Seed = 0;
      mCursor_Pos = -1;
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


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
