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

#ifndef _MORKPORTTABLECURSOR_
#define _MORKPORTTABLECURSOR_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKCURSOR_
#include "morkCursor.h"
#endif

#ifndef _MORKROWSPACE_
#include "morkRowSpace.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class orkinPortTableCursor;
#define morkDerived_kPortTableCursor  /*i*/ 0x7443 /* ascii 'tC' */

class morkPortTableCursor : public morkCursor { // row iterator

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
  morkStore*    mPortTableCursor_Store;  // weak ref to store
  
  mdb_scope     mPortTableCursor_RowScope;
  mdb_kind      mPortTableCursor_TableKind;
  
  // We only care if LastTablle is non-nil, so it is not refcounted;
  // so you must never access table state or methods using LastTable:
  
  morkTable* mPortTableCursor_LastTable; // nil or last table (no refcount)
  morkRowSpace* mPortTableCursor_RowSpace; // current space (strong ref)

  morkRowSpaceMapIter mPortTableCursor_SpaceIter; // iter over spaces
  morkTableMapIter    mPortTableCursor_TableIter; // iter over tables 
  
  // these booleans indicate when the table or space iterator is exhausted:
  
  mork_bool           mPortTableCursor_TablesDidEnd; // no more tables?
  mork_bool           mPortTableCursor_SpacesDidEnd; // no more spaces?
  mork_u1             mPortTableCursor_Pad[ 2 ]; // for u4 alignment
   
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // ClosePortTableCursor()
  virtual ~morkPortTableCursor(); // assert that close executed earlier
  
public: // morkPortTableCursor construction & destruction
  morkPortTableCursor(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, morkStore* ioStore, mdb_scope inRowScope,
      mdb_kind inTableKind, nsIMdbHeap* ioSlotHeap);
  void ClosePortTableCursor(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkPortTableCursor(const morkPortTableCursor& other);
  morkPortTableCursor& operator=(const morkPortTableCursor& other);

public: // dynamic type identification
  mork_bool IsPortTableCursor() const
  { return IsNode() && mNode_Derived == morkDerived_kPortTableCursor; }
// } ===== end morkNode methods =====

protected: // utilities

  void init_space_tables_map(morkEnv* ev);

public: // other cursor methods

  static void NilCursorStoreError(morkEnv* ev);
  static void NonPortTableCursorTypeError(morkEnv* ev);
  
  orkinPortTableCursor* AcquirePortTableCursorHandle(morkEnv* ev);
  morkRowSpace* NextSpace(morkEnv* ev);
  morkTable* NextTable(morkEnv* ev);

  mork_bool SetRowScope(morkEnv* ev, mork_scope inRowScope);
  mork_bool SetTableKind(morkEnv* ev, mork_kind inTableKind);

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakPortTableCursor(morkPortTableCursor* me,
    morkEnv* ev, morkPortTableCursor** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongPortTableCursor(morkPortTableCursor* me,
    morkEnv* ev, morkPortTableCursor** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};



//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKPORTTABLECURSOR_ */
