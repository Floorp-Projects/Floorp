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

#ifndef _MORKTABLE_
#define _MORKTABLE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

#ifndef _MORKARRAY_
#include "morkArray.h"
#endif

#ifndef _MORKROWMAP_
#include "morkRowMap.h"
#endif

#ifndef _MORKNODEMAP_
#include "morkNodeMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class nsIMdbTable;
#define morkDerived_kTable  /*i*/ 0x5462 /* ascii 'Tb' */

#define morkTable_kStartRowArraySize 11 /* modest starting size for array */

#define morkTable_kStartRowMapSlotCount 128
#define morkTable_kMaxCellUses 0x0FFFF /* max for 16-bit unsigned int */

class morkTable : public morkObject { 

// public: // slots inherited from morkObject (meant to inform only)
  // nsIMdbHeap*    mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs

  // morkHandle*    mObject_Handle;   // weak ref to handle for this object

public: // state is public because the entire Mork system is private

  morkStore*      mTable_Store;   // weak ref to port
  // mork_seed       mTable_Seed; // use TableSeed() instead

  // mTable_RowSpace->mSpace_Scope is row scope 
  morkRowSpace*   mTable_RowSpace; // weak ref to containing space

  morkRow*        mTable_MetaRow; // table's actual meta row
  mdbOid          mTable_MetaRowOid; // oid for meta row
  
  morkRowMap      mTable_RowMap;    // hash table of all members
  morkArray       mTable_RowArray;   // array of morkRow pointers
  
  mork_tid        mTable_Id;
  mork_kind       mTable_Kind;
  
  mork_bool       mTable_MustBeUnique;
  mork_u1         mTable_Pad;       // padding for u4 alignment
  mork_u2         mTable_CellUses;  // persistent references from cells
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseTable() if open
  virtual ~morkTable(); // assert that close executed earlier
  
public: // morkTable construction & destruction
  morkTable(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioNodeHeap, morkStore* ioStore,
    nsIMdbHeap* ioSlotHeap, morkRowSpace* ioRowSpace,
    const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
    mork_tid inTableId,
    mork_kind inKind, mork_bool inMustBeUnique);
  void CloseTable(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkTable(const morkTable& other);
  morkTable& operator=(const morkTable& other);

public: // dynamic type identification
  mork_bool IsTable() const
  { return IsNode() && mNode_Derived == morkDerived_kTable; }
// } ===== end morkNode methods =====

public: // errors
  static void NonTableTypeError(morkEnv* ev);
  static void NonTableTypeWarning(morkEnv* ev);
  static void NilRowSpaceError(morkEnv* ev);

public: // warnings
  static void CellUsesUnderflowWarning(morkEnv* ev);

public: // other table methods

  morkRow* GetMetaRow(morkEnv* ev, const mdbOid* inOptionalMetaRowOid);
  
  mork_u2 AddCellUse(morkEnv* ev);
  mork_u2 CutCellUse(morkEnv* ev);

  // void DirtyAllTableContent(morkEnv* ev);

  mork_seed TableSeed() const { return mTable_RowArray.mArray_Seed; }

  nsIMdbTable* AcquireTableHandle(morkEnv* ev); // mObject_Handle
  
  mork_count GetRowCount() const { return mTable_RowArray.mArray_Fill; }

  void GetTableOid(morkEnv* ev, mdbOid* outOid);
  mork_pos  ArrayHasOid(morkEnv* ev, const mdbOid* inOid);
  mork_bool MapHasOid(morkEnv* ev, const mdbOid* inOid);
  mork_bool AddRow(morkEnv* ev, morkRow* ioRow); // returns ev->Good()
  mork_bool CutRow(morkEnv* ev, morkRow* ioRow); // returns ev->Good()
  
  morkTableRowCursor* NewTableRowCursor(morkEnv* ev, mork_pos inRowPos);

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakTable(morkTable* me,
    morkEnv* ev, morkTable** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongTable(morkTable* me,
    morkEnv* ev, morkTable** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kTableMap  /*i*/ 0x744D /* ascii 'tM' */

/*| morkTableMap: maps mork_token -> morkTable
|*/
class morkTableMap : public morkNodeMap { // for mapping tokens to tables

public:

  virtual ~morkTableMap();
  morkTableMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);

public: // other map methods

  mork_bool  AddTable(morkEnv* ev, morkTable* ioTable)
  { return this->AddNode(ev, ioTable->mTable_Id, ioTable); }
  // the AddTable() boolean return equals ev->Good().

  mork_bool  CutTable(morkEnv* ev, mork_tid inTid)
  { return this->CutNode(ev, inTid); }
  // The CutTable() boolean return indicates whether removal happened. 
  
  morkTable*  GetTable(morkEnv* ev, mork_tid inTid)
  { return (morkTable*) this->GetNode(ev, inTid); }
  // Note the returned table does NOT have an increase in refcount for this.

  mork_num CutAllTables(morkEnv* ev)
  { return this->CutAllNodes(ev); }
  // CutAllTables() releases all the referenced table values.
};

class morkTableMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkTableMapIter(morkEnv* ev, morkTableMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkTableMapIter( ) : morkMapIter()  { }
  void InitTableMapIter(morkEnv* ev, morkTableMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change*
  FirstTable(morkEnv* ev, mork_tid* outTid, morkTable** outTable)
  { return this->First(ev, outTid, outTable); }
  
  mork_change*
  NextTable(morkEnv* ev, mork_tid* outTid, morkTable** outTable)
  { return this->Next(ev, outTid, outTable); }
  
  mork_change*
  HereTable(morkEnv* ev, mork_tid* outTid, morkTable** outTable)
  { return this->Here(ev, outTid, outTable); }
  
  mork_change*
  CutHereTable(morkEnv* ev, mork_tid* outTid, morkTable** outTable)
  { return this->CutHere(ev, outTid, outTable); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKTABLE_ */
