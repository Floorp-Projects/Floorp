/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
