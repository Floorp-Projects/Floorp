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

#ifndef _MORKINTMAP_
#include "morkIntMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkIntMap::CloseMorkNode(morkEnv* ev) // CloseIntMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseIntMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkIntMap::~morkIntMap() // assert CloseIntMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkIntMap::morkIntMap(morkEnv* ev,
  const morkUsage& inUsage, mork_size inValSize,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_bool inHoldChanges)
: morkMap(ev, inUsage, ioHeap, sizeof(mork_u4), inValSize,
  morkIntMap_kStartSlotCount, ioSlotHeap, inHoldChanges)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kIntMap;
}

/*public non-poly*/ void
morkIntMap::CloseIntMap(morkEnv* ev) // called by CloseMorkNode();
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
/*virtual*/ mork_bool // *((mork_u4*) inKeyA) == *((mork_u4*) inKeyB)
morkIntMap::Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const
{
  return *((const mork_u4*) inKeyA) == *((const mork_u4*) inKeyB);
}

/*virtual*/ mork_u4 // some integer function of *((mork_u4*) inKey)
morkIntMap::Hash(morkEnv* ev, const void* inKey) const
{
  return *((const mork_u4*) inKey);
}
// } ===== end morkMap poly interface =====

mork_bool
morkIntMap::AddInt(morkEnv* ev, mork_u4 inKey, void* ioAddress)
  // the AddInt() method return value equals ev->Good().
{
  if ( ev->Good() )
  {
    this->Put(ev, &inKey, &ioAddress, 
      /*key*/ (void*) 0, /*val*/ (void*) 0, (mork_change**) 0);
  }
    
  return ev->Good();
}

mork_bool
morkIntMap::CutInt(morkEnv* ev, mork_u4 inKey)
{
  return this->Cut(ev, &inKey, /*key*/ (void*) 0, /*val*/ (void*) 0,
    (mork_change**) 0);
}

void*
morkIntMap::GetInt(morkEnv* ev, mork_u4 inKey)
  // Note the returned val does NOT have an increase in refcount for this.
{
  void* val = 0; // old val in the map
  this->Get(ev, &inKey, /*key*/ (void*) 0, &val, (mork_change**) 0);
  
  return val;
}

mork_bool
morkIntMap::HasInt(morkEnv* ev, mork_u4 inKey)
  // Note the returned val does NOT have an increase in refcount for this.
{
  return this->Get(ev, &inKey, /*key*/ (void*) 0, /*val*/ (void*) 0, 
    (mork_change**) 0);
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
