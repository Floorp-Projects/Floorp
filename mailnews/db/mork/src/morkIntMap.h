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

#define morkIntMap_kStartSlotCount 512

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

  mork_bool  CutInt(morkEnv* ev, mork_token inKey);
  // The CutInt() boolean return indicates whether removal happened. 
  
  void*      GetInt(morkEnv* ev, mork_token inKey);
  // Note the returned node does NOT have an increase in refcount for this.
  
  mork_bool  HasInt(morkEnv* ev, mork_u4 inKey);
  // Note the returned node does NOT have an increase in refcount for this.

};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKINTMAP_ */
