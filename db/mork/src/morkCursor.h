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

#ifndef _MORKCURSOR_
#define _MORKCURSOR_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kCursor  /*i*/ 0x4375 /* ascii 'Cu' */

class morkCursor : public morkObject { // collection iterator

// public: // slots inherited from morkObject (meant to inform only)
  // nsIMdbHeap*     mNode_Heap;
  // mork_able    mNode_Mutable; // can this node be modified?
  // mork_load    mNode_Load;    // is this node clean or dirty?
  // mork_base    mNode_Base;    // must equal morkBase_kNode
  // mork_derived mNode_Derived; // depends on specific node subclass
  // mork_access  mNode_Access;  // kOpen, kClosing, kShut, or kDead
  // mork_usage   mNode_Usage;   // kHeap, kStack, kMember, kGlobal, kNone
  // mork_uses    mNode_Uses;    // refcount for strong refs
  // mork_refs    mNode_Refs;    // refcount for strong refs + weak refs

  // mork_color   mBead_Color;   // ID for this bead
  // morkHandle*  mObject_Handle;  // weak ref to handle for this object

public: // state is public because the entire Mork system is private
  mork_seed  mCursor_Seed;
  mork_pos   mCursor_Pos;
  mork_bool  mCursor_DoFailOnSeedOutOfSync;
  mork_u1    mCursor_Pad[ 3 ]; // explicitly pad to u4 alignment
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseCursor() only if open
  virtual ~morkCursor(); // assert that CloseCursor() executed earlier
  
public: // morkCursor construction & destruction
  morkCursor(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap);
  void CloseCursor(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkCursor(const morkCursor& other);
  morkCursor& operator=(const morkCursor& other);

public: // dynamic type identification
  mork_bool IsCursor() const
  { return IsNode() && mNode_Derived == morkDerived_kCursor; }
// } ===== end morkNode methods =====

public: // other cursor methods

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakCursor(morkCursor* me,
    morkEnv* ev, morkCursor** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongCursor(morkCursor* me,
    morkEnv* ev, morkCursor** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKCURSOR_ */
