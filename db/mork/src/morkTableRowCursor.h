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
 *   Blake Ross (blake@blakeross.com)
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

class morkTableRowCursor : public morkCursor, public nsIMdbTableRowCursor { // row iterator

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

public:
  NS_DECL_ISUPPORTS_INHERITED

  // { ----- begin attribute methods -----
  NS_IMETHOD GetCount(nsIMdbEnv* ev, mdb_count* outCount); // readonly
  NS_IMETHOD GetSeed(nsIMdbEnv* ev, mdb_seed* outSeed);    // readonly
  
  NS_IMETHOD SetPos(nsIMdbEnv* ev, mdb_pos inPos);   // mutable
  NS_IMETHOD GetPos(nsIMdbEnv* ev, mdb_pos* outPos);
  
  NS_IMETHOD SetDoFailOnSeedOutOfSync(nsIMdbEnv* ev, mdb_bool inFail);
  NS_IMETHOD GetDoFailOnSeedOutOfSync(nsIMdbEnv* ev, mdb_bool* outFail);

  // } ----- end attribute methods -----
    NS_IMETHOD GetTable(nsIMdbEnv* ev, nsIMdbTable** acqTable);
  // } ----- end attribute methods -----

  // { ----- begin duplicate row removal methods -----
  NS_IMETHOD CanHaveDupRowMembers(nsIMdbEnv* ev, // cursor might hold dups?
    mdb_bool* outCanHaveDups);
    
  NS_IMETHOD MakeUniqueCursor( // clone cursor, removing duplicate rows
    nsIMdbEnv* ev, // context
    nsIMdbTableRowCursor** acqCursor);    // acquire clone with no dups
  // } ----- end duplicate row removal methods -----

  // { ----- begin oid iteration methods -----
  NS_IMETHOD NextRowOid( // get row id of next row in the table
    nsIMdbEnv* ev, // context
    mdbOid* outOid, // out row oid
    mdb_pos* outRowPos);    // zero-based position of the row in table
  NS_IMETHOD PrevRowOid( // get row id of previous row in the table
    nsIMdbEnv* ev, // context
    mdbOid* outOid, // out row oid
    mdb_pos* outRowPos);    // zero-based position of the row in table
  // } ----- end oid iteration methods -----

  // { ----- begin row iteration methods -----
  NS_IMETHOD NextRow( // get row cells from table for cells already in row
    nsIMdbEnv* ev, // context
    nsIMdbRow** acqRow, // acquire next row in table
    mdb_pos* outRowPos);    // zero-based position of the row in table
  NS_IMETHOD PrevRow( // get row cells from table for cells already in row
    nsIMdbEnv* ev, // context
    nsIMdbRow** acqRow, // acquire previous row in table
    mdb_pos* outRowPos);    // zero-based position of the row in table
  // } ----- end row iteration methods -----


public: // dynamic type identification
  mork_bool IsTableRowCursor() const
  { return IsNode() && mNode_Derived == morkDerived_kTableRowCursor; }
// } ===== end morkNode methods =====

public: // typing
  static void NonTableRowCursorTypeError(morkEnv* ev);

public: // oid only iteration
  mdb_pos NextRowOid(morkEnv* ev, mdbOid* outOid);
  mdb_pos PrevRowOid(morkEnv* ev, mdbOid* outOid);

public: // other table row cursor methods

  virtual mork_bool CanHaveDupRowMembers(morkEnv* ev);
  virtual mork_count GetMemberCount(morkEnv* ev);

  virtual morkRow* NextRow(morkEnv* ev, mdbOid* outOid, mdb_pos* outPos);
  virtual morkRow* PrevRow(morkEnv* ev, mdbOid* outOid, mdb_pos* outPos);

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
