/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

#ifndef _MORKARRAY_
#include "morkArray.h"
#endif

#ifndef _MORKDEQUE_
#include "morkDeque.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kRowSpace  /*i*/ 0x7253 /* ascii 'rS' */

#define morkRowSpace_kStartRowMapSlotCount 11

#define morkRowSpace_kMaxIndexCount 8 /* no more indexes than this */
#define morkRowSpace_kPrimeCacheSize 17 /* should be prime number */

class morkAtomRowMap;

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
  
  // mork_bool   mSpace_DoAutoIDs;    // whether db should assign member IDs
  // mork_bool   mSpace_HaveDoneAutoIDs; // whether actually auto assigned IDs
  // mork_u1     mSpace_Pad[ 2 ];    // pad to u4 alignment

public: // state is public because the entire Mork system is private

  nsIMdbHeap*  mRowSpace_SlotHeap;

#ifdef MORK_ENABLE_PROBE_MAPS
  morkRowProbeMap   mRowSpace_Rows;   // hash table of morkRow instances
#else /*MORK_ENABLE_PROBE_MAPS*/
  morkRowMap   mRowSpace_Rows;   // hash table of morkRow instances
#endif /*MORK_ENABLE_PROBE_MAPS*/
  morkTableMap mRowSpace_Tables; // all the tables in this row scope

  mork_tid     mRowSpace_NextTableId;  // for auto-assigning table IDs
  mork_rid     mRowSpace_NextRowId;    // for auto-assigning row IDs
  
  mork_count   mRowSpace_IndexCount; // if nonzero, row indexes exist
    
  // every nonzero slot in IndexCache is a strong ref to a morkAtomRowMap:
  morkAtomRowMap* mRowSpace_IndexCache[ morkRowSpace_kPrimeCacheSize ];

  morkDeque    mRowSpace_TablesByPriority[ morkPriority_kCount ];

public: // more specific dirty methods for row space:
  void SetRowSpaceDirty() { this->SetNodeDirty(); }
  void SetRowSpaceClean() { this->SetNodeClean(); }
  
  mork_bool IsRowSpaceClean() const { return this->IsNodeClean(); }
  mork_bool IsRowSpaceDirty() const { return this->IsNodeDirty(); }

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

  morkRow* FindRow(morkEnv* ev, mork_column inColumn, const mdbYarn* inYarn);

  morkAtomRowMap* ForceMap(morkEnv* ev, mork_column inColumn);
  morkAtomRowMap* FindMap(morkEnv* ev, mork_column inColumn);

protected: // internal utilities
  morkAtomRowMap* make_index(morkEnv* ev, mork_column inColumn);

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
  { return this->AddNode(ev, ioRowSpace->SpaceScope(), ioRowSpace); }
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
