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

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKFACTORY_
#include "morkFactory.h"
#endif

#ifndef _ORKINFACTORY_
#include "orkinFactory.h"
#endif

#ifndef _ORKINHEAP_
#include "orkinHeap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkFactory::CloseMorkNode(morkEnv* ev) /*i*/ // CloseFactory() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseFactory(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkFactory::~morkFactory() /*i*/ // assert CloseFactory() executed earlier
{
  MORK_ASSERT(mFactory_Env.IsShutNode());
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkFactory::morkFactory() // uses orkinHeap
: morkObject(morkUsage::kGlobal, (nsIMdbHeap*) 0)
, mFactory_Env(morkUsage::kMember, (nsIMdbHeap*) 0, this,
  new orkinHeap())
, mFactory_Heap()
{
  if ( mFactory_Env.Good() )
  {
    mNode_Derived = morkDerived_kFactory;
    mNode_Refs += morkFactory_kWeakRefCountBonus;
  }
}

/*public non-poly*/
morkFactory::morkFactory(nsIMdbHeap* ioHeap)
: morkObject(morkUsage::kHeap, ioHeap)
, mFactory_Env(morkUsage::kMember, (nsIMdbHeap*) 0, this, ioHeap)
, mFactory_Heap()
{
  if ( mFactory_Env.Good() )
  {
    mNode_Derived = morkDerived_kFactory;
    mNode_Refs += morkFactory_kWeakRefCountBonus;
  }
}

/*public non-poly*/
morkFactory::morkFactory(morkEnv* ev, /*i*/
  const morkUsage& inUsage, nsIMdbHeap* ioHeap)
: morkObject(ev, inUsage, ioHeap, (morkHandle*) 0)
, mFactory_Env(morkUsage::kMember, (nsIMdbHeap*) 0, this, ioHeap)
, mFactory_Heap()
{
  if ( ev->Good() )
  {
    mNode_Derived = morkDerived_kFactory;
    mNode_Refs += morkFactory_kWeakRefCountBonus;
  }
}

/*public non-poly*/ void
morkFactory::CloseFactory(morkEnv* ev) /*i*/ // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mFactory_Env.CloseMorkNode(ev);
      this->CloseObject(ev);
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

nsIMdbFactory*
morkFactory::AcquireFactoryHandle(morkEnv* ev) // mObject_Handle
{
  nsIMdbFactory* outFactory = 0;
  orkinFactory* f = (orkinFactory*) mObject_Handle;
  if ( f ) // have an old handle?
    f->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    f = orkinFactory::MakeFactory(ev, this);
    mObject_Handle = f;
  }
  if ( f )
    outFactory = f;
  return outFactory;
}

void
morkFactory::NonFactoryTypeError(morkEnv* ev)
{
  ev->NewError("non morkFactory");
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
