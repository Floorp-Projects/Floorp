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

#ifndef _MORKATOMSPACE_
#define _MORKATOMSPACE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKSPACE_
#include "morkSpace.h"
#endif

#ifndef _MORKATOMMAP_
#include "morkAtomMap.h"
#endif

#ifndef _MORKNODEMAP_
#include "morkNodeMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*| kMinUnderId: the smallest ID we auto-assign to the 'under' namespace
**| reserved for tokens expected to occur very frequently, such as the names
**| of columns.  We reserve single byte ids in the ASCII range to correspond
**| one-to-one to those tokens consisting single ASCII characters (so that
**| this assignment is always known and constant).  So we start at 0x80, and
**| then reserve the upper half of two hex digit ids and all the three hex
**| digit IDs for the 'under' namespace for common tokens.
|*/
#define morkAtomSpace_kMinUnderId 0x80   /* low 7 bits mean byte tokens */

#define morkAtomSpace_kMaxSevenBitAid 0x7F  /* low seven bit integer ID */

/*| kMinOverId: the smallest ID we auto-assign to the 'over' namespace that
**| might include very large numbers of tokens that are used infrequently,
**| so that we care less whether the shortest hex representation is used.
**| So we start all IDs for 'over' category tokens at a value range that
**| needs at least four hex digits, so we can reserve three hex digits and
**| shorter for more commonly occuring tokens in the 'under' category.
|*/
#define morkAtomSpace_kMinOverId 0x1000  /* using at least four hex bytes */

#define morkDerived_kAtomSpace  /*i*/ 0x6153 /* ascii 'aS' */

#define morkAtomSpace_kColumnScope ((mork_scope) 'c') /* column scope is forever */

/*| morkAtomSpace:
|*/
class morkAtomSpace : public morkSpace { // 

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
  // mork_u1     mSpace_Pad[ 2 ];     // pad to u4 alignment

public: // state is public because the entire Mork system is private

  mork_aid         mAtomSpace_HighUnderId; // high ID in 'under' range
  mork_aid         mAtomSpace_HighOverId;  // high ID in 'over' range
  
  morkAtomAidMap   mAtomSpace_AtomAids; // all atoms in space by ID
  morkAtomBodyMap  mAtomSpace_AtomBodies; // all atoms in space by body

public: // more specific dirty methods for atom space:
  void SetAtomSpaceDirty() { this->SetNodeDirty(); }
  void SetAtomSpaceClean() { this->SetNodeClean(); }
  
  mork_bool IsAtomSpaceClean() const { return this->IsNodeClean(); }
  mork_bool IsAtomSpaceDirty() const { return this->IsNodeDirty(); }

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseAtomSpace() only if open
  virtual ~morkAtomSpace(); // assert that CloseAtomSpace() executed earlier
  
public: // morkMap construction & destruction
  morkAtomSpace(morkEnv* ev, const morkUsage& inUsage, mork_scope inScope, 
    morkStore* ioStore, nsIMdbHeap* ioNodeHeap, nsIMdbHeap* ioSlotHeap);
  void CloseAtomSpace(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsAtomSpace() const
  { return IsNode() && mNode_Derived == morkDerived_kAtomSpace; }
// } ===== end morkNode methods =====

public: // typing
  void NonAtomSpaceTypeError(morkEnv* ev);

public: // setup

  mork_bool MarkAllAtomSpaceContentDirty(morkEnv* ev);
  // MarkAllAtomSpaceContentDirty() visits every space object and marks 
  // them dirty, including every table, row, cell, and atom.  The return
  // equals ev->Good(), to show whether any error happened.  This method is
  // intended for use in the beginning of a "compress commit" which writes
  // all store content, whether dirty or not.  We dirty everything first so
  // that later iterations over content can mark things clean as they are
  // written, and organize the process of serialization so that objects are
  // written only at need (because of being dirty).

public: // other space methods

  // void ReserveColumnAidCount(mork_count inCount)
  // {
  //   mAtomSpace_HighUnderId = morkAtomSpace_kMinUnderId + inCount;
  //   mAtomSpace_HighOverId = morkAtomSpace_kMinOverId + inCount;
  // }

  mork_num CutAllAtoms(morkEnv* ev, morkPool* ioPool);
  // CutAllAtoms() puts all the atoms back in the pool.
  
  morkBookAtom* MakeBookAtomCopyWithAid(morkEnv* ev,
     const morkFarBookAtom& inAtom,  mork_aid inAid);
  // Make copy of inAtom and put it in both maps, using specified ID.
  
  morkBookAtom* MakeBookAtomCopy(morkEnv* ev, const morkFarBookAtom& inAtom);
  // Make copy of inAtom and put it in both maps, using a new ID as needed.

  mork_aid MakeNewAtomId(morkEnv* ev, morkBookAtom* ioAtom);
  // generate an unused atom id.

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakAtomSpace(morkAtomSpace* me,
    morkEnv* ev, morkAtomSpace** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongAtomSpace(morkAtomSpace* me,
    morkEnv* ev, morkAtomSpace** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kAtomSpaceMap  /*i*/ 0x615A /* ascii 'aZ' */

/*| morkAtomSpaceMap: maps mork_scope -> morkAtomSpace
|*/
class morkAtomSpaceMap : public morkNodeMap { // for mapping tokens to tables

public:

  virtual ~morkAtomSpaceMap();
  morkAtomSpaceMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);

public: // other map methods

  mork_bool  AddAtomSpace(morkEnv* ev, morkAtomSpace* ioAtomSpace)
  { return this->AddNode(ev, ioAtomSpace->SpaceScope(), ioAtomSpace); }
  // the AddAtomSpace() boolean return equals ev->Good().

  mork_bool  CutAtomSpace(morkEnv* ev, mork_scope inScope)
  { return this->CutNode(ev, inScope); }
  // The CutAtomSpace() boolean return indicates whether removal happened. 
  
  morkAtomSpace*  GetAtomSpace(morkEnv* ev, mork_scope inScope)
  { return (morkAtomSpace*) this->GetNode(ev, inScope); }
  // Note the returned space does NOT have an increase in refcount for this.

  mork_num CutAllAtomSpaces(morkEnv* ev)
  { return this->CutAllNodes(ev); }
  // CutAllAtomSpaces() releases all the referenced table values.
};

class morkAtomSpaceMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkAtomSpaceMapIter(morkEnv* ev, morkAtomSpaceMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkAtomSpaceMapIter( ) : morkMapIter()  { }
  void InitAtomSpaceMapIter(morkEnv* ev, morkAtomSpaceMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change*
  FirstAtomSpace(morkEnv* ev, mork_scope* outScope, morkAtomSpace** outAtomSpace)
  { return this->First(ev, outScope, outAtomSpace); }
  
  mork_change*
  NextAtomSpace(morkEnv* ev, mork_scope* outScope, morkAtomSpace** outAtomSpace)
  { return this->Next(ev, outScope, outAtomSpace); }
  
  mork_change*
  HereAtomSpace(morkEnv* ev, mork_scope* outScope, morkAtomSpace** outAtomSpace)
  { return this->Here(ev, outScope, outAtomSpace); }
  
  mork_change*
  CutHereAtomSpace(morkEnv* ev, mork_scope* outScope, morkAtomSpace** outAtomSpace)
  { return this->CutHere(ev, outScope, outAtomSpace); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKATOMSPACE_ */

