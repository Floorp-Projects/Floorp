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

#ifndef _MORKATOMMAP_
#include "morkAtomMap.h"
#endif

#ifndef _MORKATOM_
#include "morkAtom.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkAtomAidMap::CloseMorkNode(morkEnv* ev) // CloseAtomAidMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseAtomAidMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkAtomAidMap::~morkAtomAidMap() // assert CloseAtomAidMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkAtomAidMap::morkAtomAidMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkMap(ev, inUsage,  ioHeap,
  /*inKeySize*/ sizeof(morkBookAtom*), /*inValSize*/ 0,
  morkAtomAidMap_kStartSlotCount, ioSlotHeap,
  /*inHoldChanges*/ morkBool_kFalse)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kAtomAidMap;
}

/*public non-poly*/ void
morkAtomAidMap::CloseAtomAidMap(morkEnv* ev) // called by CloseMorkNode();
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
morkAtomAidMap::Equal(morkEnv* ev, const void* inKeyA,
  const void* inKeyB) const
{
  return (*(const morkBookAtom**) inKeyA)->EqualAid(
    *(const morkBookAtom**) inKeyB);
}

/*virtual*/ mork_u4 // 
morkAtomAidMap::Hash(morkEnv* ev, const void* inKey) const
{
  return (*(const morkBookAtom**) inKey)->HashAid();
}
// } ===== end morkMap poly interface =====


mork_bool
morkAtomAidMap::AddAtom(morkEnv* ev, morkBookAtom* ioAtom)
{
  if ( ev->Good() )
  {
    this->Put(ev, &ioAtom, /*val*/ (void*) 0, 
      /*key*/ (void*) 0, /*val*/ (void*) 0, (mork_change**) 0);
  }
  return ev->Good();
}

morkBookAtom*
morkAtomAidMap::CutAtom(morkEnv* ev, const morkBookAtom* inAtom)
{
  morkBookAtom* oldKey = 0;
  this->Cut(ev, &inAtom, &oldKey, /*val*/ (void*) 0,
    (mork_change**) 0);
    
  return oldKey;
}

morkBookAtom*
morkAtomAidMap::GetAtom(morkEnv* ev, const morkBookAtom* inAtom)
{
  morkBookAtom* key = 0; // old val in the map
  this->Get(ev, &inAtom, &key, /*val*/ (void*) 0, (mork_change**) 0);
  
  return key;
}

morkBookAtom*
morkAtomAidMap::GetAid(morkEnv* ev, mork_aid inAid)
{
  morkWeeBookAtom weeAtom(inAid);
  morkBookAtom* key = &weeAtom; // we need a pointer
  morkBookAtom* oldKey = 0; // old key in the map
  this->Get(ev, &key, &oldKey, /*val*/ (void*) 0, (mork_change**) 0);
  
  return oldKey;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkAtomBodyMap::CloseMorkNode(morkEnv* ev) // CloseAtomBodyMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseAtomBodyMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkAtomBodyMap::~morkAtomBodyMap() // assert CloseAtomBodyMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkAtomBodyMap::morkAtomBodyMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkMap(ev, inUsage,  ioHeap,
  /*inKeySize*/ sizeof(morkBookAtom*), /*inValSize*/ 0,
  morkAtomBodyMap_kStartSlotCount, ioSlotHeap,
  /*inHoldChanges*/ morkBool_kFalse)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kAtomBodyMap;
}

/*public non-poly*/ void
morkAtomBodyMap::CloseAtomBodyMap(morkEnv* ev) // called by CloseMorkNode();
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
morkAtomBodyMap::Equal(morkEnv* ev, const void* inKeyA,
  const void* inKeyB) const
{
  return (*(const morkBookAtom**) inKeyA)->EqualFormAndBody(ev,
    *(const morkBookAtom**) inKeyB);
}

/*virtual*/ mork_u4 // 
morkAtomBodyMap::Hash(morkEnv* ev, const void* inKey) const
{
  return (*(const morkBookAtom**) inKey)->HashFormAndBody(ev);
}
// } ===== end morkMap poly interface =====


mork_bool
morkAtomBodyMap::AddAtom(morkEnv* ev, morkBookAtom* ioAtom)
{
  if ( ev->Good() )
  {
    this->Put(ev, &ioAtom, /*val*/ (void*) 0, 
      /*key*/ (void*) 0, /*val*/ (void*) 0, (mork_change**) 0);
  }
  return ev->Good();
}

morkBookAtom*
morkAtomBodyMap::CutAtom(morkEnv* ev, const morkBookAtom* inAtom)
{
  morkBookAtom* oldKey = 0;
  this->Cut(ev, &inAtom, &oldKey, /*val*/ (void*) 0,
    (mork_change**) 0);
    
  return oldKey;
}

morkBookAtom*
morkAtomBodyMap::GetAtom(morkEnv* ev, const morkBookAtom* inAtom)
{
  morkBookAtom* key = 0; // old val in the map
  this->Get(ev, &inAtom, &key, /*val*/ (void*) 0, (mork_change**) 0);
  
  return key;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
