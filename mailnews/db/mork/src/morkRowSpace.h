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

#ifndef _MORKROWSPACE_
#define _MORKROWSPACE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKSPACE_
#include "morkSpace.h"
#endif

#ifndef _MORKNODEMAP_
#include "morkNodeMap.h"
#endif

#ifndef _MORKROWMAP_
#include "morkRowMap.h"
#endif

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kRowSpace  /*i*/ 0x7253 /* ascii 'rS' */

#define morkRowSpace_kStartRowMapSlotCount 512

/*| morkRowSpace:
|*/
class morkRowSpace : public morkSpace { // 

// public: // slots inherited from morkSpace (meant to inform only)
  // nsIMdbHeap*    mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs
  
  // morkStore*  mSpace_Store; // weak ref to containing store
  // mork_scope  mSpace_Scope; // the scope for this space
  
  // mork_bool   mSpace_DoAutoIDs;    // whether db should assign member IDs
  // mork_bool   mSpace_HaveDoneAutoIDs; // whether actually auto assigned IDs
  // mork_u1     mSpace_Pad[ 2 ];     // pad to u4 alignment

public: // state is public because the entire Mork system is private

  morkRowMap   mRowSpace_Rows;   // hash table of morkRow instances
  morkTableMap mRowSpace_Tables; // all the tables in this row scope

  mork_tid     mRowSpace_NextTableId; // for auto-assigning table IDs
  mork_rid     mRowSpace_NextRowId;   // for auto-assigning row IDs

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseRowSpace() only if open
  virtual ~morkRowSpace(); // assert that CloseRowSpace() executed earlier
  
public: // morkMap construction & destruction
  morkRowSpace(morkEnv* ev, const morkUsage& inUsage, mork_scope inScope,
    morkStore* ioStore, nsIMdbHeap* ioNodeHeap, nsIMdbHeap* ioSlotHeap);
  void CloseRowSpace(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsRowSpace() const
  { return IsNode() && mNode_Derived == morkDerived_kRowSpace; }
// } ===== end morkNode methods =====

public: // typing
  static void NonRowSpaceTypeError(morkEnv* ev);
  static void ZeroScopeError(morkEnv* ev);
  static void ZeroKindError(morkEnv* ev);
  static void ZeroTidError(morkEnv* ev);
  static void MinusOneRidError(morkEnv* ev);

  //static void ExpectAutoIdOnlyError(morkEnv* ev);
  //static void ExpectAutoIdNeverError(morkEnv* ev);

public: // other space methods

  mork_num CutAllRows(morkEnv* ev, morkPool* ioPool);
  // CutAllRows() puts all rows and cells back into the pool.
  
  morkTable* NewTable(morkEnv* ev, mork_kind inTableKind,
    mdb_bool inMustBeUnique, const mdbOid* inOptionalMetaRowOid);
  
  morkTable* NewTableWithTid(morkEnv* ev, mork_tid inTid,
    mork_kind inTableKind, const mdbOid* inOptionalMetaRowOid);
  
  morkTable* FindTableByKind(morkEnv* ev, mork_kind inTableKind);
  morkTable* FindTableByTid(morkEnv* ev, mork_tid inTid)
  { return mRowSpace_Tables.GetTable(ev, inTid); }

  mork_tid MakeNewTableId(morkEnv* ev);
  mork_rid MakeNewRowId(morkEnv* ev);

  // morkRow* FindRowByRid(morkEnv* ev, mork_rid inRid)
  // { return (morkRow*) mRowSpace_Rows.GetRow(ev, inRid); }

  morkRow* NewRowWithOid(morkEnv* ev, const mdbOid* inOid);
  morkRow* NewRow(morkEnv* ev);

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakRowSpace(morkRowSpace* me,
    morkEnv* ev, morkRowSpace** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongRowSpace(morkRowSpace* me,
    morkEnv* ev, morkRowSpace** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kRowSpaceMap  /*i*/ 0x725A /* ascii 'rZ' */

/*| morkRowSpaceMap: maps mork_scope -> morkRowSpace
|*/
class morkRowSpaceMap : public morkNodeMap { // for mapping tokens to tables

public:

  virtual ~morkRowSpaceMap();
  morkRowSpaceMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);

public: // other map methods

  mork_bool  AddRowSpace(morkEnv* ev, morkRowSpace* ioRowSpace)
  { return this->AddNode(ev, ioRowSpace->mSpace_Scope, ioRowSpace); }
  // the AddRowSpace() boolean return equals ev->Good().

  mork_bool  CutRowSpace(morkEnv* ev, mork_scope inScope)
  { return this->CutNode(ev, inScope); }
  // The CutRowSpace() boolean return indicates whether removal happened. 
  
  morkRowSpace*  GetRowSpace(morkEnv* ev, mork_scope inScope)
  { return (morkRowSpace*) this->GetNode(ev, inScope); }
  // Note the returned space does NOT have an increase in refcount for this.

  mork_num CutAllRowSpaces(morkEnv* ev)
  { return this->CutAllNodes(ev); }
  // CutAllRowSpaces() releases all the referenced table values.
};

class morkRowSpaceMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkRowSpaceMapIter(morkEnv* ev, morkRowSpaceMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkRowSpaceMapIter( ) : morkMapIter()  { }
  void InitRowSpaceMapIter(morkEnv* ev, morkRowSpaceMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change*
  FirstRowSpace(morkEnv* ev, mork_scope* outScope, morkRowSpace** outRowSpace)
  { return this->First(ev, outScope, outRowSpace); }
  
  mork_change*
  NextRowSpace(morkEnv* ev, mork_scope* outScope, morkRowSpace** outRowSpace)
  { return this->Next(ev, outScope, outRowSpace); }
  
  mork_change*
  HereRowSpace(morkEnv* ev, mork_scope* outScope, morkRowSpace** outRowSpace)
  { return this->Here(ev, outScope, outRowSpace); }
  
  mork_change*
  CutHereRowSpace(morkEnv* ev, mork_scope* outScope, morkRowSpace** outRowSpace)
  { return this->CutHere(ev, outScope, outRowSpace); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKROWSPACE_ */
