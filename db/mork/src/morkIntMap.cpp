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
  MORK_USED_1(ev);
  return *((const mork_u4*) inKeyA) == *((const mork_u4*) inKeyB);
}

/*virtual*/ mork_u4 // some integer function of *((mork_u4*) inKey)
morkIntMap::Hash(morkEnv* ev, const void* inKey) const
{
  MORK_USED_1(ev);
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

#ifdef MORK_POINTER_MAP_IMPL

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkPointerMap::CloseMorkNode(morkEnv* ev) // ClosePointerMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->ClosePointerMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkPointerMap::~morkPointerMap() // assert ClosePointerMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkPointerMap::morkPointerMap(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkMap(ev, inUsage, ioHeap, sizeof(void*), sizeof(void*),
  morkPointerMap_kStartSlotCount, ioSlotHeap,
  /*inHoldChanges*/ morkBool_kFalse)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kPointerMap;
}

/*public non-poly*/ void
morkPointerMap::ClosePointerMap(morkEnv* ev) // called by CloseMorkNode();
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
/*virtual*/ mork_bool // *((void**) inKeyA) == *((void**) inKeyB)
morkPointerMap::Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const
{
  MORK_USED_1(ev);
  return *((const void**) inKeyA) == *((const void**) inKeyB);
}

/*virtual*/ mork_u4 // some integer function of *((mork_u4*) inKey)
morkPointerMap::Hash(morkEnv* ev, const void* inKey) const
{
  MORK_USED_1(ev);
  return *((const mork_u4*) inKey);
}
// } ===== end morkMap poly interface =====

mork_bool
morkPointerMap::AddPointer(morkEnv* ev, void* inKey, void* ioAddress)
  // the AddPointer() method return value equals ev->Good().
{
  if ( ev->Good() )
  {
    this->Put(ev, &inKey, &ioAddress, 
      /*key*/ (void*) 0, /*val*/ (void*) 0, (mork_change**) 0);
  }
    
  return ev->Good();
}

mork_bool
morkPointerMap::CutPointer(morkEnv* ev, void* inKey)
{
  return this->Cut(ev, &inKey, /*key*/ (void*) 0, /*val*/ (void*) 0,
    (mork_change**) 0);
}

void*
morkPointerMap::GetPointer(morkEnv* ev, void* inKey)
  // Note the returned val does NOT have an increase in refcount for this.
{
  void* val = 0; // old val in the map
  this->Get(ev, &inKey, /*key*/ (void*) 0, &val, (mork_change**) 0);
  
  return val;
}

mork_bool
morkPointerMap::HasPointer(morkEnv* ev, void* inKey)
  // Note the returned val does NOT have an increase in refcount for this.
{
  return this->Get(ev, &inKey, /*key*/ (void*) 0, /*val*/ (void*) 0, 
    (mork_change**) 0);
}
#endif /*MORK_POINTER_MAP_IMPL*/


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
