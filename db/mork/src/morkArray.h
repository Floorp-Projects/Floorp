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

#ifndef _MORKARRAY_
#define _MORKARRAY_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kArray  /*i*/ 0x4179 /* ascii 'Ay' */

class morkArray : public morkNode { // row iterator

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

public: // state is public because the entire Mork system is private
  void**       mArray_Slots; // array of pointers
  nsIMdbHeap*  mArray_Heap;  // required heap for allocating mArray_Slots
  mork_fill    mArray_Fill;  // logical count of used slots in mArray_Slots
  mork_size    mArray_Size;  // physical count of mArray_Slots ( >= Fill)
  mork_seed    mArray_Seed;  // change counter for syncing with iterators
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseArray()
  virtual ~morkArray(); // assert that close executed earlier
  
public: // morkArray construction & destruction
  morkArray(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, mork_size inSize, nsIMdbHeap* ioSlotHeap);
  void CloseArray(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkArray(const morkArray& other);
  morkArray& operator=(const morkArray& other);

public: // dynamic type identification
  mork_bool IsArray() const
  { return IsNode() && mNode_Derived == morkDerived_kArray; }
// } ===== end morkNode methods =====

public: // typing & errors
  static void NonArrayTypeError(morkEnv* ev);
  static void IndexBeyondEndError(morkEnv* ev);
  static void NilSlotsAddressError(morkEnv* ev);
  static void FillBeyondSizeError(morkEnv* ev);

public: // other table row cursor methods

  mork_fill  Length() const { return mArray_Fill; }
  mork_size  Capacity() const { return mArray_Size; }
  
  mork_bool  Grow(morkEnv* ev, mork_size inNewSize);
  // Grow() returns true if capacity becomes >= inNewSize and ev->Good()
  
  void*      At(mork_pos inPos) const { return mArray_Slots[ inPos ]; }
  void       AtPut(mork_pos inPos, void* ioSlot)
  { mArray_Slots[ inPos ] = ioSlot; }
  
  void*      SafeAt(morkEnv* ev, mork_pos inPos);
  void       SafeAtPut(morkEnv* ev, mork_pos inPos, void* ioSlot);
  
  mork_pos   AppendSlot(morkEnv* ev, void* ioSlot);
  void       AddSlot(morkEnv* ev, mork_pos inPos, void* ioSlot);
  void       CutSlot(morkEnv* ev, mork_pos inPos);
  void       CutAllSlots(morkEnv* ev);

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakArray(morkArray* me,
    morkEnv* ev, morkArray** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongArray(morkArray* me,
    morkEnv* ev, morkArray** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKTABLEROWCURSOR_ */
