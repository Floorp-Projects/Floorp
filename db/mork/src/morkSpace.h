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

#ifndef _MORKSPACE_
#define _MORKSPACE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKBEAD_
#include "morkBead.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkSpace_kInitialSpaceSlots  /*i*/ 1024 /* default */
#define morkDerived_kSpace  /*i*/ 0x5370 /* ascii 'Sp' */

/*| morkSpace:
|*/
class morkSpace : public morkBead { // 

// public: // slots inherited from morkNode (meant to inform only)
  // nsIMdbHeap*       mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs

  // mork_color      mBead_Color;   // ID for this bead

public: // bead color setter & getter replace obsolete member mTable_Id:

  mork_tid     SpaceScope() const { return mBead_Color; }
  void         SetSpaceScope(mork_scope inScope) { mBead_Color = inScope; }

public: // state is public because the entire Mork system is private

  morkStore*  mSpace_Store; // weak ref to containing store
    
  mork_bool   mSpace_DoAutoIDs;    // whether db should assign member IDs
  mork_bool   mSpace_HaveDoneAutoIDs; // whether actually auto assigned IDs
  mork_bool   mSpace_CanDirty; // changes imply the store becomes dirty?
  mork_u1     mSpace_Pad;    // pad to u4 alignment

public: // more specific dirty methods for space:
  void SetSpaceDirty() { this->SetNodeDirty(); }
  void SetSpaceClean() { this->SetNodeClean(); }
  
  mork_bool IsSpaceClean() const { return this->IsNodeClean(); }
  mork_bool IsSpaceDirty() const { return this->IsNodeDirty(); }

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseSpace() only if open
  virtual ~morkSpace(); // assert that CloseSpace() executed earlier
  
public: // morkMap construction & destruction
  //morkSpace(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioNodeHeap,
  //  const morkMapForm& inForm, nsIMdbHeap* ioSlotHeap);
  
  morkSpace(morkEnv* ev, const morkUsage& inUsage,mork_scope inScope, 
    morkStore* ioStore, nsIMdbHeap* ioNodeHeap, nsIMdbHeap* ioSlotHeap);
  void CloseSpace(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsSpace() const
  { return IsNode() && mNode_Derived == morkDerived_kSpace; }
// } ===== end morkNode methods =====

public: // other space methods
  
  mork_bool MaybeDirtyStoreAndSpace();

  static void NonAsciiSpaceScopeName(morkEnv* ev);
  static void NilSpaceStoreError(morkEnv* ev);

  morkPool* GetSpaceStorePool() const;

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakSpace(morkSpace* me,
    morkEnv* ev, morkSpace** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongSpace(morkSpace* me,
    morkEnv* ev, morkSpace** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKSPACE_ */
