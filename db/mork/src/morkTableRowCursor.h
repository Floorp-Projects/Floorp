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

#ifndef _MORKTABLEROWCURSOR_
#define _MORKTABLEROWCURSOR_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKCURSOR_
#include "morkCursor.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class orkinTableRowCursor;
#define morkDerived_kTableRowCursor  /*i*/ 0x7243 /* ascii 'rC' */

class morkTableRowCursor : public morkCursor { // row iterator

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
  morkTable*  mTableRowCursor_Table; // weak ref to table
    
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseTableRowCursor()
  virtual ~morkTableRowCursor(); // assert that close executed earlier
  
public: // morkTableRowCursor construction & destruction
  morkTableRowCursor(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, morkTable* ioTable, mork_pos inRowPos);
  void CloseTableRowCursor(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkTableRowCursor(const morkTableRowCursor& other);
  morkTableRowCursor& operator=(const morkTableRowCursor& other);

public: // dynamic type identification
  mork_bool IsTableRowCursor() const
  { return IsNode() && mNode_Derived == morkDerived_kTableRowCursor; }
// } ===== end morkNode methods =====

public: // typing
  static void NonTableRowCursorTypeError(morkEnv* ev);

public: // handle attachment
  orkinTableRowCursor* AcquireTableRowCursorHandle(morkEnv* ev);

public: // oid only iteration
  mdb_pos NextRowOid(morkEnv* ev, mdbOid* outOid);

public: // other table row cursor methods

  virtual mork_bool CanHaveDupRowMembers(morkEnv* ev);
  virtual mork_count GetMemberCount(morkEnv* ev);

  virtual orkinTableRowCursor* AcquireUniqueRowCursorHandle(morkEnv* ev);
  
  virtual morkRow* NextRow(morkEnv* ev, mdbOid* outOid, mdb_pos* outPos);

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakTableRowCursor(morkTableRowCursor* me,
    morkEnv* ev, morkTableRowCursor** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongTableRowCursor(morkTableRowCursor* me,
    morkEnv* ev, morkTableRowCursor** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKTABLEROWCURSOR_ */
