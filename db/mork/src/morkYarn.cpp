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

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKYARN_
#include "morkYarn.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkYarn::CloseMorkNode(morkEnv* ev) /*i*/ // CloseYarn() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseYarn(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkYarn::~morkYarn() /*i*/ // assert CloseYarn() executed earlier
{
  MORK_ASSERT(mYarn_Body.mYarn_Buf==0);
}

/*public non-poly*/
morkYarn::morkYarn(morkEnv* ev, /*i*/
  const morkUsage& inUsage, nsIMdbHeap* ioHeap)
  : morkNode(ev, inUsage, ioHeap)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kYarn;
}

/*public non-poly*/ void
morkYarn::CloseYarn(morkEnv* ev) /*i*/ // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
      this->MarkShut();
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 

/*static*/ void
morkYarn::NonYarnTypeError(morkEnv* ev)
{
  ev->NewError("non morkYarn");
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
