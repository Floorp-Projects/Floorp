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

#ifndef _MORKCELLOBJECT_
#define _MORKCELLOBJECT_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kCellObject  /*i*/ 0x634F /* ascii 'cO' */

class morkCellObject : public morkObject { // blob attribute in column scope

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
  morkRowObject*  mCellObject_RowObject;  // strong ref to row's object
  morkRow*        mCellObject_Row;        // cell's row if still in row object
  morkCell*       mCellObject_Cell;       // cell in row if rowseed matches
  mork_column     mCellObject_Col;        // col of cell last living in pos
  mork_u2         mCellObject_RowSeed;    // copy of row's seed
  mork_u2         mCellObject_Pos;        // position of cell in row
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseCellObject() only if open
  virtual ~morkCellObject(); // assert that CloseCellObject() executed earlier
  
public: // morkCellObject construction & destruction
  morkCellObject(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, morkRow* ioRow, morkCell* ioCell,
    mork_column inCol, mork_pos inPos);
  void CloseCellObject(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkCellObject(const morkCellObject& other);
  morkCellObject& operator=(const morkCellObject& other);

public: // dynamic type identification
  mork_bool IsCellObject() const
  { return IsNode() && mNode_Derived == morkDerived_kCellObject; }
// } ===== end morkNode methods =====

public: // other cell node methods

  mork_bool ResyncWithRow(morkEnv* ev); // return ev->Good()
  morkAtom* GetCellAtom(morkEnv* ev) const;

  static void MissingRowColumnError(morkEnv* ev);
  static void NilRowError(morkEnv* ev);
  static void NilCellError(morkEnv* ev);
  static void NilRowObjectError(morkEnv* ev);
  static void WrongRowObjectRowError(morkEnv* ev);
  static void NonCellObjectTypeError(morkEnv* ev);

  nsIMdbCell* AcquireCellHandle(morkEnv* ev);

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakCellObject(morkCellObject* me,
    morkEnv* ev, morkCellObject** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongCellObject(morkCellObject* me,
    morkEnv* ev, morkCellObject** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKCELLOBJECT_ */
