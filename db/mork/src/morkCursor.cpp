/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights 
 * Reserved. 
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
: morkObject(ev, inUsage, ioHeap, (morkHandle*) 0)
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
