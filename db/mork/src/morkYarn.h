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

#ifndef _MORKYARN_
#define _MORKYARN_ 1

#ifndef _MORK_
#include "mork.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


#define morkDerived_kYarn     /*i*/ 0x7952 /* ascii 'yR' */

/*| morkYarn: a reference counted nsIMdbYarn C struct.  This is for use in those
**| few cases where single instances of reference counted buffers are needed
**| in Mork, and we expect few enough instances that overhead is not a factor
**| in decided whether to use such a thing.
|*/
class morkYarn : public morkNode { // refcounted yarn
  
// public: // slots inherited from morkNode (meant to inform only)
  // nsIMdbHeap*       mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs

public: // state is public because the entire Mork system is private
  mdbYarn  mYarn_Body;
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseYarn() only if open
  virtual ~morkYarn(); // assert that CloseYarn() executed earlier
  
public: // morkYarn construction & destruction
  morkYarn(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap);
  void CloseYarn(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkYarn(const morkYarn& other);
  morkYarn& operator=(const morkYarn& other);

public: // dynamic type identification
  mork_bool IsYarn() const
  { return IsNode() && mNode_Derived == morkDerived_kYarn; }
// } ===== end morkNode methods =====

public: // typing
  static void NonYarnTypeError(morkEnv* ev);

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakYarn(morkYarn* me,
    morkEnv* ev, morkYarn** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongYarn(morkYarn* me,
    morkEnv* ev, morkYarn** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKYARN_ */
