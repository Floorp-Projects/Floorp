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

#ifndef _MORKINTMAP_
#define _MORKINTMAP_ 1

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

#define morkDerived_kIntMap  /*i*/ 0x694D /* ascii 'iM' */

#define morkIntMap_kStartSlotCount 256

/*| morkIntMap: maps mork_token -> morkNode
|*/
class morkIntMap : public morkMap { // for mapping tokens to maps

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseIntMap() only if open
  virtual ~morkIntMap(); // assert that CloseIntMap() executed earlier
  
public: // morkMap construction & destruction

  // keySize for morkIntMap equals sizeof(mork_u4)
  morkIntMap(morkEnv* ev, const morkUsage& inUsage, mork_size inValSize,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_bool inHoldChanges);
  void CloseIntMap(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsIntMap() const
  { return IsNode() && mNode_Derived == morkDerived_kIntMap; }
// } ===== end morkNode methods =====

// { ===== begin morkMap poly interface =====
  virtual mork_bool // *((mork_u4*) inKeyA) == *((mork_u4*) inKeyB)
  Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const;

  virtual mork_u4 // some integer function of *((mork_u4*) inKey)
  Hash(morkEnv* ev, const void* inKey) const;
// } ===== end morkMap poly interface =====

public: // other map methods

  mork_bool  AddInt(morkEnv* ev, mork_u4 inKey, void* ioAddress);
  // the AddInt() boolean return equals ev->Good().

  mork_bool  CutInt(morkEnv* ev, mork_u4 inKey);
  // The CutInt() boolean return indicates whether removal happened. 
  
  void*      GetInt(morkEnv* ev, mork_u4 inKey);
  // Note the returned node does NOT have an increase in refcount for this.
  
  mork_bool  HasInt(morkEnv* ev, mork_u4 inKey);
  // Note the returned node does NOT have an increase in refcount for this.

};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#ifdef MORK_POINTER_MAP_IMPL

#define morkDerived_kPointerMap  /*i*/ 0x704D /* ascii 'pM' */

#define morkPointerMap_kStartSlotCount 256

/*| morkPointerMap: maps void* -> void*
**|
**| This pointer map class is equivalent to morkIntMap when sizeof(mork_u4)
**| equals sizeof(void*).  However, when these two sizes are different,
**| then we cannot use the same hash table structure very easily without
**| being very careful about the size and usage assumptions of those
**| clients using the smaller data type.  So we just go ahead and use
**| morkPointerMap for hash tables using pointer key types.
|*/
class morkPointerMap : public morkMap { // for mapping tokens to maps

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // ClosePointerMap() only if open
  virtual ~morkPointerMap(); // assert that ClosePointerMap() executed earlier
  
public: // morkMap construction & destruction

  // keySize for morkPointerMap equals sizeof(mork_u4)
  morkPointerMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);
  void ClosePointerMap(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsPointerMap() const
  { return IsNode() && mNode_Derived == morkDerived_kPointerMap; }
// } ===== end morkNode methods =====

// { ===== begin morkMap poly interface =====
  virtual mork_bool // *((void**) inKeyA) == *((void**) inKeyB)
  Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const;

  virtual mork_u4 // some integer function of *((mork_u4*) inKey)
  Hash(morkEnv* ev, const void* inKey) const;
// } ===== end morkMap poly interface =====

public: // other map methods

  mork_bool  AddPointer(morkEnv* ev, void* inKey, void* ioAddress);
  // the AddPointer() boolean return equals ev->Good().

  mork_bool  CutPointer(morkEnv* ev, void* inKey);
  // The CutPointer() boolean return indicates whether removal happened. 
  
  void*      GetPointer(morkEnv* ev, void* inKey);
  // Note the returned node does NOT have an increase in refcount for this.
  
  mork_bool  HasPointer(morkEnv* ev, void* inKey);
  // Note the returned node does NOT have an increase in refcount for this.

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakIntMap(morkIntMap* me,
    morkEnv* ev, morkIntMap** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongIntMap(morkIntMap* me,
    morkEnv* ev, morkIntMap** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }

};
#endif /*MORK_POINTER_MAP_IMPL*/


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKINTMAP_ */
