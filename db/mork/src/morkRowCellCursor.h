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

#ifndef _MORKROWCELLCURSOR_
#define _MORKROWCELLCURSOR_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKCURSOR_
#include "morkCursor.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class orkinRowCellCursor;
#define morkDerived_kRowCellCursor  /*i*/ 0x6343 /* ascii 'cC' */

class morkRowCellCursor : public morkCursor { // row iterator

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

  // morkFactory* mObject_Factory;  // weak ref to suite factory

  // mork_seed  mCursor_Seed;
  // mork_pos   mCursor_Pos;
  // mork_bool  mCursor_DoFailOnSeedOutOfSync;
  // mork_u1    mCursor_Pad[ 3 ]; // explicitly pad to u4 alignment

public: // state is public because the entire Mork system is private

  morkRowObject*   mRowCellCursor_RowObject;  // strong ref to row
  mork_column      mRowCellCursor_Col;        // col of cell last at mCursor_Pos
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseRowCellCursor()
  virtual ~morkRowCellCursor(); // assert that close executed earlier
  
public: // morkRowCellCursor construction & destruction
  morkRowCellCursor(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, morkRowObject* ioRowObject);
  void CloseRowCellCursor(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkRowCellCursor(const morkRowCellCursor& other);
  morkRowCellCursor& operator=(const morkRowCellCursor& other);

public: // dynamic type identification
  mork_bool IsRowCellCursor() const
  { return IsNode() && mNode_Derived == morkDerived_kRowCellCursor; }
// } ===== end morkNode methods =====

public: // errors
  static void NilRowObjectError(morkEnv* ev);
  static void NonRowCellCursorTypeError(morkEnv* ev);

public: // other cell cursor methods
  orkinRowCellCursor* AcquireRowCellCursorHandle(morkEnv* ev);

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakRowCellCursor(morkRowCellCursor* me,
    morkEnv* ev, morkRowCellCursor** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongRowCellCursor(morkRowCellCursor* me,
    morkEnv* ev, morkRowCellCursor** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKROWCELLCURSOR_ */
