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

#ifndef _MORKOBJECT_
#define _MORKOBJECT_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKBEAD_
#include "morkBead.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kObject   /*i*/ 0x6F42 /* ascii 'oB' */

/*| morkObject: subclass of morkNode that adds knowledge of db suite factory
**| and containing port to those objects that are exposed as instances of
**| nsIMdbObject in the public interface.
|*/
class morkObject : public morkBead { 

// public: // slots inherited from morkNode (meant to inform only)
  // nsIMdbHeap*    mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs

  // mork_color      mBead_Color;   // ID for this bead
  
public: // state is public because the entire Mork system is private

  morkHandle*      mObject_Handle;   // weak ref to handle for this object
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseObject() only if open
  virtual ~morkObject(); // assert that CloseObject() executed earlier
  
protected: // special case construction of first env without preceding env
  morkObject(const morkUsage& inUsage, nsIMdbHeap* ioHeap,
    mork_color inBeadColor);
  
public: // morkEnv construction & destruction
  morkObject(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
     mork_color inBeadColor, morkHandle* ioHandle); // ioHandle can be nil
  void CloseObject(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkObject(const morkObject& other);
  morkObject& operator=(const morkObject& other);

public: // dynamic type identification
  mork_bool IsObject() const
  { return IsNode() && mNode_Derived == morkDerived_kObject; }
// } ===== end morkNode methods =====

  // void NewNilHandleError(morkEnv* ev); // mObject_Handle is nil
  
public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakObject(morkObject* me,
    morkEnv* ev, morkObject** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongObject(morkObject* me,
    morkEnv* ev, morkObject** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKOBJECT_ */
