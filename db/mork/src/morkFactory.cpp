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
: morkObject(morkUsage::kGlobal, (nsIMdbHeap*) 0, morkColor_kNone)
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
: morkObject(morkUsage::kHeap, ioHeap, morkColor_kNone)
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
: morkObject(ev, inUsage, ioHeap, morkColor_kNone, (morkHandle*) 0)
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
