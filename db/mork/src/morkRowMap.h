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

#ifndef _MORKROWMAP_
#define _MORKROWMAP_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kRowMap  /*i*/ 0x724D /* ascii 'rM' */

/*| morkRowMap: maps a set of morkRow by contained Oid
|*/
class morkRowMap : public morkMap { // for mapping row IDs to rows

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseRowMap() only if open
  virtual ~morkRowMap(); // assert that CloseRowMap() executed earlier
  
public: // morkMap construction & destruction
  morkRowMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_size inSlots);
  void CloseRowMap(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsRowMap() const
  { return IsNode() && mNode_Derived == morkDerived_kRowMap; }
// } ===== end morkNode methods =====

// { ===== begin morkMap poly interface =====
  virtual mork_bool // note: equal(a,b) implies hash(a) == hash(b)
  Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const;
  // implemented using morkRow::EqualRow()

  virtual mork_u4 // note: equal(a,b) implies hash(a) == hash(b)
  Hash(morkEnv* ev, const void* inKey) const;
  // implemented using morkRow::HashRow()
// } ===== end morkMap poly interface =====

public: // other map methods

  mork_bool AddRow(morkEnv* ev, morkRow* ioRow);
  // AddRow() returns ev->Good()

  morkRow*  CutOid(morkEnv* ev, const mdbOid* inOid);
  // CutRid() returns the row removed equal to inRid, if there was one

  morkRow*  CutRow(morkEnv* ev, const morkRow* ioRow);
  // CutRow() returns the row removed equal to ioRow, if there was one
  
  morkRow*  GetOid(morkEnv* ev, const mdbOid* inOid);
  // GetOid() returns the row equal to inRid, or else nil
  
  morkRow*  GetRow(morkEnv* ev, const morkRow* ioRow);
  // GetRow() returns the row equal to ioRow, or else nil
  
  // note the rows are owned elsewhere, usuall by morkRowSpace

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakRowMap(morkMap* me,
    morkEnv* ev, morkRowMap** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongRowMap(morkMap* me,
    morkEnv* ev, morkRowMap** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

class morkRowMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkRowMapIter(morkEnv* ev, morkRowMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkRowMapIter( ) : morkMapIter()  { }
  void InitRowMapIter(morkEnv* ev, morkRowMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change* FirstRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->First(ev, outRowPtr, /*val*/ (void*) 0); }
  
  mork_change* NextRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->Next(ev, outRowPtr, /*val*/ (void*) 0); }
  
  mork_change* HereRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->Here(ev, outRowPtr, /*val*/ (void*) 0); }
  
  mork_change* CutHereRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->CutHere(ev, outRowPtr, /*val*/ (void*) 0); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKROWMAP_ */
