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

#ifndef _MORKDEQUE_
#include "morkDeque.h"
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

/*| kStartRowArraySize: starting physical size of array for mTable_RowArray.
**| We want this number very small, so that a table containing exactly one
**| row member will not pay too significantly in space overhead.  But we want
**| a number bigger than one, so there is some space for growth.
|*/
#define morkTable_kStartRowArraySize 3 /* modest starting size for array */

/*| kMakeRowMapThreshold: this is the number of rows in a table which causes
**| a hash table (mTable_RowMap) to be lazily created for faster member row
**| identification, during such operations as cuts and adds.  This number must
**| be small enough that linear searches are not bad for member counts less
**| than this; but this number must also be large enough that creating a hash
**| table does not increase the per-row space overhead by a big percentage.
**| For speed, numbers on the order of ten to twenty are all fine; for space,
**| I believe a number as small as ten will have too much space overhead.
|*/
#define morkTable_kMakeRowMapThreshold 17 /* when to build mTable_RowMap */

#define morkTable_kStartRowMapSlotCount 13
#define morkTable_kMaxTableGcUses 0x0FF /* max for 8-bit unsigned int */

#define morkTable_kUniqueBit   ((mork_u1) (1 << 0))
#define morkTable_kVerboseBit  ((mork_u1) (1 << 1))
#define morkTable_kNotedBit    ((mork_u1) (1 << 2)) /* space has change notes */
#define morkTable_kRewriteBit  ((mork_u1) (1 << 3)) /* must rewrite all rows */
#define morkTable_kNewMetaBit  ((mork_u1) (1 << 4)) /* new table meta row */

class morkTable : public morkObject, public morkLink { 

  // NOTE the morkLink base is for morkRowSpace::mRowSpace_TablesByPriority

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

  // mTable_RowSpace->mSpace_Scope is row scope 
  morkRowSpace*   mTable_RowSpace; // weak ref to containing space

  morkRow*        mTable_MetaRow; // table's actual meta row
  mdbOid          mTable_MetaRowOid; // oid for meta row
  
  morkRowMap*     mTable_RowMap;     // (strong ref) hash table of all members
  morkArray       mTable_RowArray;   // array of morkRow pointers
  
  morkList        mTable_ChangeList;      // list of table changes
  mork_u2         mTable_ChangesCount; // length of changes list 
  mork_u2         mTable_ChangesMax;   // max list length before rewrite 
  
  mork_tid        mTable_Id;
  mork_kind       mTable_Kind;
  
  mork_u1         mTable_Flags;         // bit flags
  mork_priority   mTable_Priority;      // 0..9, any other value equals 9
  mork_u1         mTable_GcUses;        // persistent references from cells
  mork_u1         mTable_Pad;      // for u4 alignment

public: // flags bit twiddling

  void SetTableUnique() { mTable_Flags |= morkTable_kUniqueBit; }
  void SetTableVerbose() { mTable_Flags |= morkTable_kVerboseBit; }
  void SetTableNoted() { mTable_Flags |= morkTable_kNotedBit; }
  void SetTableRewrite() { mTable_Flags |= morkTable_kRewriteBit; }
  void SetTableNewMeta() { mTable_Flags |= morkTable_kNewMetaBit; }

  void ClearTableUnique() { mTable_Flags &= (mork_u1) ~morkTable_kUniqueBit; }
  void ClearTableVerbose() { mTable_Flags &= (mork_u1) ~morkTable_kVerboseBit; }
  void ClearTableNoted() { mTable_Flags &= (mork_u1) ~morkTable_kNotedBit; }
  void ClearTableRewrite() { mTable_Flags &= (mork_u1) ~morkTable_kRewriteBit; }
  void ClearTableNewMeta() { mTable_Flags &= (mork_u1) ~morkTable_kNewMetaBit; }

  mork_bool IsTableUnique() const
  { return ( mTable_Flags & morkTable_kUniqueBit ) != 0; }
  
  mork_bool IsTableVerbose() const
  { return ( mTable_Flags & morkTable_kVerboseBit ) != 0; }
  
  mork_bool IsTableNoted() const
  { return ( mTable_Flags & morkTable_kNotedBit ) != 0; }
  
  mork_bool IsTableRewrite() const
  { return ( mTable_Flags & morkTable_kRewriteBit ) != 0; }
  
  mork_bool IsTableNewMeta() const
  { return ( mTable_Flags & morkTable_kNewMetaBit ) != 0; }

public: // table dirty handling more complex than morkNode::SetNodeDirty() etc.

  void SetTableDirty() { this->SetNodeDirty(); }
  void SetTableClean(morkEnv* ev);
   
  mork_bool IsTableClean() const { return this->IsNodeClean(); }
  mork_bool IsTableDirty() const { return this->IsNodeDirty(); }

public: // morkNode memory management operators
  void* operator new(size_t inSize, nsIMdbHeap& ioHeap, morkEnv* ev)
  { return morkNode::MakeNew(inSize, ioHeap, ev); }
  
  void operator delete(void* ioAddress)
  { morkNode::OnDeleteAssert(ioAddress); }
  // do NOT call delete on morkNode instances.  Call ZapOld() instead.
 
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
  static void TableGcUsesUnderflowWarning(morkEnv* ev);

public: // noting table changes

  mork_bool HasChangeOverflow() const
  { return mTable_ChangesCount >= mTable_ChangesMax; }

  void NoteTableSetAll(morkEnv* ev);
  void NoteTableMoveRow(morkEnv* ev, morkRow* ioRow, mork_pos inPos);

  void note_row_change(morkEnv* ev, mork_change inChange, morkRow* ioRow);
  
  void NoteTableAddRow(morkEnv* ev, morkRow* ioRow)
  { this->note_row_change(ev, morkChange_kAdd, ioRow); }
  
  void NoteTableCutRow(morkEnv* ev, morkRow* ioRow)
  { this->note_row_change(ev, morkChange_kCut, ioRow); }

protected: // internal row map methods

  morkRow* find_member_row(morkEnv* ev, morkRow* ioRow);
  void build_row_map(morkEnv* ev);

public: // other table methods
  
  mork_bool MaybeDirtySpaceStoreAndTable();

  morkRow* GetMetaRow(morkEnv* ev, const mdbOid* inOptionalMetaRowOid);
  
  mork_u2 AddTableGcUse(morkEnv* ev);
  mork_u2 CutTableGcUse(morkEnv* ev);

  // void DirtyAllTableContent(morkEnv* ev);

  mork_seed TableSeed() const { return mTable_RowArray.mArray_Seed; }
  
  morkRow* SafeRowAt(morkEnv* ev, mork_pos inPos)
  { return (morkRow*) mTable_RowArray.SafeAt(ev, inPos); }

  nsIMdbTable* AcquireTableHandle(morkEnv* ev); // mObject_Handle
  
  mork_count GetRowCount() const { return mTable_RowArray.mArray_Fill; }

  void GetTableOid(morkEnv* ev, mdbOid* outOid);
  mork_pos  ArrayHasOid(morkEnv* ev, const mdbOid* inOid);
  mork_bool MapHasOid(morkEnv* ev, const mdbOid* inOid);
  mork_bool AddRow(morkEnv* ev, morkRow* ioRow); // returns ev->Good()
  mork_bool CutRow(morkEnv* ev, morkRow* ioRow); // returns ev->Good()
  mork_bool CutAllRows(morkEnv* ev); // returns ev->Good()

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

// use negative values for kCut and kAdd, to keep non-neg move pos distinct:
#define morkTableChange_kCut ((mork_pos) -1) /* shows row was cut */
#define morkTableChange_kAdd ((mork_pos) -2) /* shows row was added */
#define morkTableChange_kNone ((mork_pos) -3) /* unknown change */

class morkTableChange : public morkNext { 
public: // state is public because the entire Mork system is private

  morkRow*  mTableChange_Row; // the row in the change
  
  mork_pos  mTableChange_Pos; // kAdd, kCut, or non-neg for row move

public:
  morkTableChange(morkEnv* ev, mork_change inChange, morkRow* ioRow);
  // use this constructor for inChange == morkChange_kAdd or morkChange_kCut
  
  morkTableChange(morkEnv* ev, morkRow* ioRow, mork_pos inPos);
  // use this constructor when the row is moved

public:
  void UnknownChangeError(morkEnv* ev) const; // morkChange_kAdd or morkChange_kCut
  void NegativeMovePosError(morkEnv* ev) const; // move must be non-neg position
  
public:
  
  mork_bool IsAddRowTableChange() const
  { return ( mTableChange_Pos == morkTableChange_kAdd ); }
  
  mork_bool IsCutRowTableChange() const
  { return ( mTableChange_Pos == morkTableChange_kCut ); }
  
  mork_bool IsMoveRowTableChange() const
  { return ( mTableChange_Pos >= 0 ); }

public:
  
  mork_pos GetMovePos() const { return mTableChange_Pos; }
  // GetMovePos() assumes that IsMoveRowTableChange() is true.
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
  
  // cutting while iterating hash map might dirty the parent table:
  mork_change*
  CutHereTable(morkEnv* ev, mork_tid* outTid, morkTable** outTable)
  { return this->CutHere(ev, outTid, outTable); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKTABLE_ */
