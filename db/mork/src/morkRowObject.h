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

#ifndef _MORKROWOBJECT_
#define _MORKROWOBJECT_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class nsIMdbRow;
#define morkDerived_kRowObject  /*i*/ 0x724F /* ascii 'rO' */

class morkRowObject : public morkObject { //

public: // state is public because the entire Mork system is private
  morkRow*    mRowObject_Row;     // non-refcounted alias to morkRow
  morkStore*  mRowObject_Store;   // weak ref to store containing row
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseRowObject() only if open
  virtual ~morkRowObject(); // assert that CloseRowObject() executed earlier
  
public: // morkRowObject construction & destruction
  morkRowObject(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, morkRow* ioRow, morkStore* ioStore);
  void CloseRowObject(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkRowObject(const morkRowObject& other);
  morkRowObject& operator=(const morkRowObject& other);

public: // dynamic type identification
  mork_bool IsRowObject() const
  { return IsNode() && mNode_Derived == morkDerived_kRowObject; }
// } ===== end morkNode methods =====

public: // typing
  static void NonRowObjectTypeError(morkEnv* ev);
  static void NilRowError(morkEnv* ev);
  static void NilStoreError(morkEnv* ev);
  static void RowObjectRowNotSelfError(morkEnv* ev);

public: // other row node methods

  nsIMdbRow* AcquireRowHandle(morkEnv* ev); // mObject_Handle
  
public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakRowObject(morkRowObject* me,
    morkEnv* ev, morkRowObject** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongRowObject(morkRowObject* me,
    morkEnv* ev, morkRowObject** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKROWOBJECT_ */
