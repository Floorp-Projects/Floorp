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

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKROWMAP_
#include "morkRowMap.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkRowMap::CloseMorkNode(morkEnv* ev) // CloseRowMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseRowMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkRowMap::~morkRowMap() // assert CloseRowMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkRowMap::morkRowMap(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_size inSlots)
: morkMap(ev, inUsage,  ioHeap,
  /*inKeySize*/ sizeof(morkRow*), /*inValSize*/ 0,
  inSlots, ioSlotHeap, /*inHoldChanges*/ morkBool_kFalse)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kRowMap;
}

/*public non-poly*/ void
morkRowMap::CloseRowMap(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      this->CloseMap(ev);
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


// { ===== begin morkMap poly interface =====
/*virtual*/ mork_bool // 
morkRowMap::Equal(morkEnv* ev, const void* inKeyA,
  const void* inKeyB) const
{
  return (*(const morkRow**) inKeyA)->EqualRow(*(const morkRow**) inKeyB);
}

/*virtual*/ mork_u4 // 
morkRowMap::Hash(morkEnv* ev, const void* inKey) const
{
  return (*(const morkRow**) inKey)->HashRow();
}
// } ===== end morkMap poly interface =====


mork_bool
morkRowMap::AddRow(morkEnv* ev, morkRow* ioRow)
{
  if ( ev->Good() )
  {
    this->Put(ev, &ioRow, /*val*/ (void*) 0, 
      /*key*/ (void*) 0, /*val*/ (void*) 0, (mork_change**) 0);
  }
  return ev->Good();
}

morkRow*
morkRowMap::CutOid(morkEnv* ev, const mdbOid* inOid)
{
  morkRow row(inOid);
  morkRow* key = &row;
  morkRow* oldKey = 0;
  this->Cut(ev, &key, &oldKey, /*val*/ (void*) 0,
    (mork_change**) 0);
    
  return oldKey;
}

morkRow*
morkRowMap::CutRow(morkEnv* ev, const morkRow* ioRow)
{
  morkRow* oldKey = 0;
  this->Cut(ev, &ioRow, &oldKey, /*val*/ (void*) 0,
    (mork_change**) 0);
    
  return oldKey;
}

morkRow*
morkRowMap::GetOid(morkEnv* ev, const mdbOid* inOid)
{
  morkRow row(inOid);
  morkRow* key = &row;
  morkRow* oldKey = 0;
  this->Get(ev, &key, &oldKey, /*val*/ (void*) 0, (mork_change**) 0);
  
  return oldKey;
}

morkRow*
morkRowMap::GetRow(morkEnv* ev, const morkRow* ioRow)
{
  morkRow* oldKey = 0;
  this->Get(ev, &ioRow, &oldKey, /*val*/ (void*) 0, (mork_change**) 0);
  
  return oldKey;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
