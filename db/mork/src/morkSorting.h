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

#ifndef _MORKSORTING_
#define _MORKSORTING_ 1

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

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class nsIMdbSorting;
#define morkDerived_kSorting  /*i*/ 0x536F /* ascii 'So' */

class morkSorting : public morkObject { 

  // NOTE the morkLink base is for morkRowSpace::mRowSpace_SortingsByPriority

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

  // mork_color   mBead_Color;   // ID for this bead
  // morkHandle*  mObject_Handle;  // weak ref to handle for this object

public: // state is public because the entire Mork system is private

  morkTable*        mSorting_Table;    // weak ref to table
  
  nsIMdbCompare*    mSorting_Compare;

  morkArray         mSorting_RowArray;  // array of morkRow pointers
  
  mork_column       mSorting_Col;       // column that gets sorted

public: // sorting dirty handling more than morkNode::SetNodeDirty() etc.

  void SetSortingDirty() { this->SetNodeDirty(); }
   
  mork_bool IsSortingClean() const { return this->IsNodeClean(); }
  mork_bool IsSortingDirty() const { return this->IsNodeDirty(); }

public: // morkNode memory management operators
  void* operator new(size_t inSize, nsIMdbHeap& ioHeap, morkEnv* ev)
  { return morkNode::MakeNew(inSize, ioHeap, ev); }
  
  void operator delete(void* ioAddress)
  { morkNode::OnDeleteAssert(ioAddress); }
  // do NOT call delete on morkNode instances.  Call ZapOld() instead.
 
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseSorting() if open
  virtual ~morkSorting(); // assert that close executed earlier
  
public: // morkSorting construction & destruction
  morkSorting(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioNodeHeap, morkTable* ioTable,
    nsIMdbCompare* ioCompare,
    nsIMdbHeap* ioSlotHeap, mork_column inCol);
  void CloseSorting(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkSorting(const morkSorting& other);
  morkSorting& operator=(const morkSorting& other);

public: // dynamic type identification
  mork_bool IsSorting() const
  { return IsNode() && mNode_Derived == morkDerived_kSorting; }
// } ===== end morkNode methods =====

public: // errors
  static void NonSortingTypeError(morkEnv* ev);
  static void NonSortingTypeWarning(morkEnv* ev);
  static void ZeroColError(morkEnv* ev);
  static void NilTableError(morkEnv* ev);
  static void NilCompareError(morkEnv* ev);

public: // utilities

  void sort_rows(morkEnv* ev);
  mork_count copy_table_row_array(morkEnv* ev);

public: // other sorting methods
   
  mork_seed SortingSeed() const { return mSorting_RowArray.mArray_Seed; }
  
  morkRow* SafeRowAt(morkEnv* ev, mork_pos inPos)
  { return (morkRow*) mSorting_RowArray.SafeAt(ev, inPos); }

  nsIMdbSorting* AcquireSortingHandle(morkEnv* ev); // mObject_Handle
  
  mork_count GetRowCount() const { return mSorting_RowArray.mArray_Fill; }
  mork_pos  ArrayHasOid(morkEnv* ev, const mdbOid* inOid);

  morkSortingRowCursor* NewSortingRowCursor(morkEnv* ev, mork_pos inRowPos);

  mork_bool AddRow(morkEnv* ev, morkRow* ioRow);

  mork_bool CutRow(morkEnv* ev, morkRow* ioRow);

  mork_bool CutAllRows(morkEnv* ev);

protected: // table seed sync management
   
  mork_bool is_seed_stale() const
  { return mSorting_RowArray.mArray_Seed != mSorting_Table->TableSeed(); }
   
  void sync_with_table_seed()
  { mSorting_RowArray.mArray_Seed = mSorting_Table->TableSeed(); }

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakSorting(morkSorting* me,
    morkEnv* ev, morkSorting** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongSorting(morkSorting* me,
    morkEnv* ev, morkSorting** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kSortingMap  /*i*/ 0x734D /* ascii 'sM' */

/*| morkSortingMap: maps mork_column -> morkSorting
|*/
class morkSortingMap : public morkNodeMap { // for mapping cols to sortings

public:

  virtual ~morkSortingMap();
  morkSortingMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);

public: // other map methods

  mork_bool  AddSorting(morkEnv* ev, morkSorting* ioSorting)
  { return this->AddNode(ev, ioSorting->mSorting_Col, ioSorting); }
  // the AddSorting() boolean return equals ev->Good().

  mork_bool  CutSorting(morkEnv* ev, mork_column inCol)
  { return this->CutNode(ev, inCol); }
  // The CutSorting() boolean return indicates whether removal happened. 
  
  morkSorting*  GetSorting(morkEnv* ev, mork_column inCol)
  { return (morkSorting*) this->GetNode(ev, inCol); }
  // Note the returned table does NOT have an increase in refcount for this.

  mork_num CutAllSortings(morkEnv* ev)
  { return this->CutAllNodes(ev); }
  // CutAllSortings() releases all the referenced table values.
};

class morkSortingMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkSortingMapIter(morkEnv* ev, morkSortingMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkSortingMapIter( ) : morkMapIter()  { }
  void InitSortingMapIter(morkEnv* ev, morkSortingMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change*
  FirstSorting(morkEnv* ev, mork_column* outCol, morkSorting** outSorting)
  { return this->First(ev, outCol, outSorting); }
  
  mork_change*
  NextSorting(morkEnv* ev, mork_column* outCol, morkSorting** outSorting)
  { return this->Next(ev, outCol, outSorting); }
  
  mork_change*
  HereSorting(morkEnv* ev, mork_column* outCol, morkSorting** outSorting)
  { return this->Here(ev, outCol, outSorting); }
  
  // cutting while iterating hash map might dirty the parent table:
  mork_change*
  CutHereSorting(morkEnv* ev, mork_column* outCol, morkSorting** outSorting)
  { return this->CutHere(ev, outCol, outSorting); }
};


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKSORTING_ */
