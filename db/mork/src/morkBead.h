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

#ifndef _MORKBEAD_
#define _MORKBEAD_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKPROBEMAP_
#include "morkProbeMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kBead   /*i*/ 0x426F /* ascii 'Bo' */

/*| morkBead: subclass of morkNode that adds knowledge of db suite factory
**| and containing port to those objects that are exposed as instances of
**| nsIMdbBead in the public interface.
|*/
class morkBead : public morkNode { 

// public: // slots inherited from morkNode (meant to inform only)
  // nsIMdbHeap*    mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs
  
public: // state is public because the entire Mork system is private

  mork_color      mBead_Color;   // ID for this bead

public: // Hash() and Equal() for bead maps are same for all subclasses:

  mork_u4 BeadHash() const { return (mork_u4) mBead_Color; }
  mork_bool BeadEqual(const morkBead* inBead) const
  { return ( mBead_Color == inBead->mBead_Color); }
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseBead() only if open
  virtual ~morkBead(); // assert that CloseBead() executed earlier
  
public: // special case for stack construction for map usage:
  morkBead(mork_color inBeadColor); // stack-based bead instance
  
protected: // special case for morkObject:
  morkBead(const morkUsage& inUsage, nsIMdbHeap* ioHeap,
    mork_color inBeadColor);
  
public: // morkEnv construction & destruction
  morkBead(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
     mork_color inBeadColor);
  void CloseBead(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkBead(const morkBead& other);
  morkBead& operator=(const morkBead& other);

public: // dynamic type identification
  mork_bool IsBead() const
  { return IsNode() && mNode_Derived == morkDerived_kBead; }
// } ===== end morkNode methods =====

  // void NewNilHandleError(morkEnv* ev); // mBead_Handle is nil
  
public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakBead(morkBead* me,
    morkEnv* ev, morkBead** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongBead(morkBead* me,
    morkEnv* ev, morkBead** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kBeadMap  /*i*/ 0x744D /* ascii 'bM' */

/*| morkBeadMap: maps bead -> bead (key only using mBead_Color)
|*/
class morkBeadMap : public morkMap {


// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseBeadMap() only if open
  virtual ~morkBeadMap(); // assert that CloseBeadMap() executed earlier
  
public: // morkMap construction & destruction
  morkBeadMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);
  void CloseBeadMap(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsBeadMap() const
  { return IsNode() && mNode_Derived == morkDerived_kBeadMap; }
// } ===== end morkNode methods =====

// { ===== begin morkMap poly interface =====
public:
  virtual mork_bool // *((mork_u4*) inKeyA) == *((mork_u4*) inKeyB)
  Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const;

  virtual mork_u4 // some integer function of *((mork_u4*) inKey)
  Hash(morkEnv* ev, const void* inKey) const;
// } ===== end morkMap poly interface =====

public: // other map methods

  mork_bool  AddBead(morkEnv* ev, morkBead* ioBead);
  // the AddBead() boolean return equals ev->Good().

  mork_bool  CutBead(morkEnv* ev, mork_color inColor);
  // The CutBead() boolean return indicates whether removal happened. 
  
  morkBead*  GetBead(morkEnv* ev, mork_color inColor);
  // Note the returned bead does NOT have an increase in refcount for this.

  mork_num CutAllBeads(morkEnv* ev);
  // CutAllBeads() releases all the referenced beads.
};

class morkBeadMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkBeadMapIter(morkEnv* ev, morkBeadMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkBeadMapIter( ) : morkMapIter()  { }
  void InitBeadMapIter(morkEnv* ev, morkBeadMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  morkBead* FirstBead(morkEnv* ev);
  morkBead* NextBead(morkEnv* ev);
  morkBead* HereBead(morkEnv* ev);
  void      CutHereBead(morkEnv* ev);
  
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kBeadProbeMap  /*i*/ 0x6D74 /* ascii 'mb' */

/*| morkBeadProbeMap: maps bead -> bead (key only using mBead_Color)
|*/
class morkBeadProbeMap : public morkProbeMap {


// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseBeadProbeMap() only if open
  virtual ~morkBeadProbeMap(); // assert that CloseBeadProbeMap() executed earlier
  
public: // morkMap construction & destruction
  morkBeadProbeMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);
  void CloseBeadProbeMap(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsBeadProbeMap() const
  { return IsNode() && mNode_Derived == morkDerived_kBeadProbeMap; }
// } ===== end morkNode methods =====

  // { ===== begin morkProbeMap methods =====
public:
  virtual mork_test // hit(a,b) implies hash(a) == hash(b)
  MapTest(morkEnv* ev, const void* inMapKey, const void* inAppKey) const;

  virtual mork_u4 // hit(a,b) implies hash(a) == hash(b)
  MapHash(morkEnv* ev, const void* inAppKey) const;

  virtual mork_u4 ProbeMapHashMapKey(morkEnv* ev, const void* inMapKey) const;

  // virtual mork_bool ProbeMapIsKeyNil(morkEnv* ev, void* ioMapKey);

  // virtual void ProbeMapClearKey(morkEnv* ev, // put 'nil' alls keys inside map
  //   void* ioMapKey, mork_count inKeyCount); // array of keys inside map

  // virtual void ProbeMapPushIn(morkEnv* ev, // move (key,val) into the map
  //   const void* inAppKey, const void* inAppVal, // (key,val) outside map
  //   void* outMapKey, void* outMapVal);      // (key,val) inside map

  // virtual void ProbeMapPullOut(morkEnv* ev, // move (key,val) out from the map
  //   const void* inMapKey, const void* inMapVal, // (key,val) inside map
  //   void* outAppKey, void* outAppVal) const;    // (key,val) outside map
  // } ===== end morkProbeMap methods =====

public: // other map methods

  mork_bool  AddBead(morkEnv* ev, morkBead* ioBead);
  // the AddBead() boolean return equals ev->Good().
  
  morkBead*  GetBead(morkEnv* ev, mork_color inColor);
  // Note the returned bead does NOT have an increase in refcount for this.

  mork_num   CutAllBeads(morkEnv* ev);
  // CutAllBeads() releases all the referenced bead values.
};

class morkBeadProbeMapIter: public morkProbeMapIter { // typesafe wrapper class

public:
  morkBeadProbeMapIter(morkEnv* ev, morkBeadProbeMap* ioMap)
  : morkProbeMapIter(ev, ioMap) { }
 
  morkBeadProbeMapIter( ) : morkProbeMapIter()  { }
  void InitBeadProbeMapIter(morkEnv* ev, morkBeadProbeMap* ioMap)
  { this->InitProbeMapIter(ev, ioMap); }
   
  morkBead* FirstBead(morkEnv* ev)
  { return (morkBead*) this->IterFirstKey(ev); }
  
  morkBead* NextBead(morkEnv* ev)
  { return (morkBead*) this->IterNextKey(ev); }
  
  morkBead* HereBead(morkEnv* ev)
  { return (morkBead*) this->IterHereKey(ev); }
  
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKBEAD_ */
