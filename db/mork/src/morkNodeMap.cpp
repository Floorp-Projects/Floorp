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

#ifndef _MORKNODEMAP_
#include "morkNodeMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkNodeMap::CloseMorkNode(morkEnv* ev) // CloseNodeMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseNodeMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkNodeMap::~morkNodeMap() // assert CloseNodeMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkNodeMap::morkNodeMap(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkIntMap(ev, inUsage, /*valsize*/ sizeof(morkNode*), ioHeap, ioSlotHeap,
  /*inHoldChanges*/ morkBool_kTrue)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kNodeMap;
}

/*public non-poly*/ void
morkNodeMap::CloseNodeMap(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      this->CutAllNodes(ev);
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

mork_bool
morkNodeMap::AddNode(morkEnv* ev, mork_token inToken, morkNode* ioNode)
  // the AddNode() method return value equals ev->Good().
{
  if ( ioNode && ev->Good() )
  {
    morkNode* node = 0; // old val in the map
    
    mork_bool put = this->Put(ev, &inToken, &ioNode,
      /*key*/ (void*) 0, &node, (mork_change**) 0);
      
    if ( put ) // replaced an existing value for key inToken?
    {
      if ( node && node != ioNode ) // need to release old node?
        node->CutStrongRef(ev);
    }
    
    if ( ev->Bad() || !ioNode->AddStrongRef(ev) )
    {
      // problems adding node or increasing refcount?
      this->Cut(ev, &inToken,  // make sure not in map
        /*key*/ (void*) 0, /*val*/ (void*) 0, (mork_change**) 0);
    }
  }
  else if ( !ioNode )
    ev->NilPointerError();
    
  return ev->Good();
}

mork_bool
morkNodeMap::CutNode(morkEnv* ev, mork_token inToken)
{
  morkNode* node = 0; // old val in the map
  mork_bool outCutNode = this->Cut(ev, &inToken, 
    /*key*/ (void*) 0, &node, (mork_change**) 0);
  if ( node )
    node->CutStrongRef(ev);
  
  return outCutNode;
}

morkNode*
morkNodeMap::GetNode(morkEnv* ev, mork_token inToken)
  // Note the returned node does NOT have an increase in refcount for this.
{
  morkNode* node = 0; // old val in the map
  this->Get(ev, &inToken, /*key*/ (void*) 0, &node, (mork_change**) 0);
  
  return node;
}

mork_num
morkNodeMap::CutAllNodes(morkEnv* ev)
  // CutAllNodes() releases all the reference node values.
{
  mork_num outSlots = mMap_Slots;
  mork_token key = 0; // old key token in the map
  morkNode* val = 0; // old val node in the map
  
  mork_change* c = 0;
  morkNodeMapIter i(ev, this);
  for ( c = i.FirstNode(ev, &key, &val); c ; c = i.NextNode(ev, &key, &val) )
  {
    if ( val )
      val->CutStrongRef(ev);
    i.CutHereNode(ev, /*key*/ (mork_token*) 0, /*val*/ (morkNode**) 0);
  }
  
  return outSlots;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

