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

#ifndef _MORKNODE_
#define _MORKNODE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkUsage_kHeap    'h'
#define morkUsage_kStack   's'
#define morkUsage_kMember  'm'
#define morkUsage_kGlobal  'g'
#define morkUsage_kPool    'p'
#define morkUsage_kNone    'n'

class morkUsage {
public:
  mork_usage     mUsage_Code;  // kHeap, kStack, kMember, or kGhost
  
public:
  morkUsage(mork_usage inCode);

  morkUsage(); // does nothing except maybe call EnsureReadyStaticUsage()
  void InitUsage( mork_usage inCode)
  { mUsage_Code = inCode; }
  
  ~morkUsage() { }
  mork_usage Code() const { return mUsage_Code; }
  
  static void EnsureReadyStaticUsage();
  
public:
  static const morkUsage& kHeap;      // morkUsage_kHeap
  static const morkUsage& kStack;     // morkUsage_kStack
  static const morkUsage& kMember;    // morkUsage_kMember
  static const morkUsage& kGlobal;    // morkUsage_kGlobal
  static const morkUsage& kPool;      // morkUsage_kPool
  static const morkUsage& kNone;      // morkUsage_kNone
  
  static const morkUsage& GetHeap();   // kHeap, safe at static init time
  static const morkUsage& GetStack();  // kStack, safe at static init time
  static const morkUsage& GetMember(); // kMember, safe at static init time
  static const morkUsage& GetGlobal(); // kGlobal, safe at static init time
  static const morkUsage& GetPool();   // kPool, safe at static init time
  static const morkUsage& GetNone();   // kNone, safe at static init time
    
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkNode_kMaxRefCount 0x0FFFF /* count sticks if it hits this */

#define morkBase_kNode      /*i*/ 0x4E64 /* ascii 'Nd' */

/*| morkNode: several groups of two-byte integers that track the basic
**| status of an object that can be used to compose in-memory graphs.
**| This is the base class for nsIMdbObject (which adds members that fit
**| the needs of an nsIMdbObject subclass).  The morkNode class is also used
**| as the base class for other Mork db classes with no strong relation to
**| the MDB class hierarchy.
**|
**|| Heap: the heap in which this node was allocated, when the usage equals
**| morkUsage_kHeap to show dynamic allocation.  Note this heap is NOT ref-
**| counted, because that would be too great and complex a burden for all
**| the nodes allocated in that heap.  So heap users should take care to
**| understand that nodes allocated in that heap are considered protected
**| by some inclusive context in which all those nodes are allocated, and
**| that context must maintain at least one strong refcount for the heap.
**| Occasionally a node subclass will indeed wish to hold a refcounted
**| reference to a heap, and possibly the same heap that is in mNode_Heap,
**| but this is always done in a separate slot that explicitly refcounts,
**| so we avoid confusing what is meant by the mNode_Heap slot.
|*/
class morkNode { // base class for constructing Mork object graphs

public: // state is public because the entire Mork system is private
  
  nsIMdbHeap*    mNode_Heap;     // NON-refcounted heap pointer

  mork_base      mNode_Base;     // must equal morkBase_kNode
  mork_derived   mNode_Derived;  // depends on specific node subclass
  
  mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  mork_able      mNode_Mutable;  // can this node be modified?
  mork_load      mNode_Load;     // is this node clean or dirty?
  
  mork_uses      mNode_Uses;     // refcount for strong refs
  mork_refs      mNode_Refs;     // refcount for strong refs + weak refs
  
protected: // special case empty construction for morkHandleFrame
  friend class morkHandleFrame;
  morkNode() { }
  
public: // inlines for weird mNode_Mutable and mNode_Load constants

  void SetFrozen() { mNode_Mutable = morkAble_kDisabled; }
  void SetMutable() { mNode_Mutable = morkAble_kEnabled; }
  void SetAsleep() { mNode_Mutable = morkAble_kAsleep; }
  
  mork_bool IsFrozen() const { return mNode_Mutable == morkAble_kDisabled; }
  mork_bool IsMutable() const { return mNode_Mutable == morkAble_kEnabled; }
  mork_bool IsAsleep() const { return mNode_Mutable == morkAble_kAsleep; }

  void SetNodeClean() { mNode_Load = morkLoad_kClean; }
  void SetNodeDirty() { mNode_Load = morkLoad_kDirty; }
  
  mork_bool IsNodeClean() const { return mNode_Load == morkLoad_kClean; }
  mork_bool IsNodeDirty() const { return mNode_Load == morkLoad_kDirty; }
  
public: // morkNode memory management methods
  static void* MakeNew(size_t inSize, nsIMdbHeap& ioHeap, morkEnv* ev);
  
  static void OnDeleteAssert(void* ioAddress); // cannot operator delete()
  void ZapOld(morkEnv* ev, nsIMdbHeap* ioHeap); // replaces operator delete()
  // this->morkNode::~morkNode(); // first call polymorphic destructor
  // if ( ioHeap ) // was this node heap allocated?
  //    ioHeap->Free(ev->AsMdbEnv(), this);

public: // morkNode memory management operators
  void* operator new(size_t inSize, nsIMdbHeap& ioHeap, morkEnv* ev)
  { return morkNode::MakeNew(inSize, ioHeap, ev); }
  
  void operator delete(void* ioAddress)
  { morkNode::OnDeleteAssert(ioAddress); }
  // do NOT call delete on morkNode instances.  Call ZapOld() instead.

protected: // construction without an anv needed for first env constructed:
  morkNode(const morkUsage& inUsage, nsIMdbHeap* ioHeap);

  morkNode(mork_usage inCode); // usage == inCode, heap == nil

// { ===== begin basic node interface =====
public: // morkNode virtual methods
  // virtual FlushMorkNode(morkEnv* ev, morkStream* ioStream);
  // virtual WriteMorkNode(morkEnv* ev, morkStream* ioStream);
  
  virtual ~morkNode(); // assert that CloseNode() executed earlier
  virtual void CloseMorkNode(morkEnv* ev); // CloseNode() only if open
  
  // CloseMorkNode() is the polymorphic close method called when uses==0,
  // which must do NOTHING at all when IsOpenNode() is not true.  Otherwise,
  // CloseMorkNode() should call a static close method specific to an object.
  // Each such static close method should either call inherited static close
  // methods, or else perform the consolidated effect of calling them, where
  // subclasses should closely track any changes in base classes with care.
  
public: // morkNode construction
  morkNode(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap);
  void CloseNode(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkNode(const morkNode& other);
  morkNode& operator=(const morkNode& other);

public: // dynamic type identification
  mork_bool IsNode() const
  { return mNode_Base == morkBase_kNode; }
// } ===== end basic node methods =====
    
public: // public error & warning methods

  void RefsUnderUsesWarning(morkEnv* ev) const; // call if mNode_Refs < mNode_Uses
  void NonNodeError(morkEnv* ev) const; // call when IsNode() is false
  void NilHeapError(morkEnv* ev) const; // zero mNode_Heap when usage is kHeap
  void NonOpenNodeError(morkEnv* ev) const; // call when IsOpenNode() is false

  void NonMutableNodeError(morkEnv* ev) const; // when IsMutable() is false

  void RefsOverflowWarning(morkEnv* ev) const; // call on mNode_Refs overflow
  void UsesOverflowWarning(morkEnv* ev) const; // call on mNode_Uses overflow
  void RefsUnderflowWarning(morkEnv* ev) const; // call on mNode_Refs underflow
  void UsesUnderflowWarning(morkEnv* ev) const; // call on mNode_Uses underflow

private: // private refcounting methods
  mork_bool  cut_use_count(morkEnv* ev); // just one part of CutStrongRef()

public: // other morkNode methods

  mork_bool  GoodRefs() const { return mNode_Refs >= mNode_Uses; }
  mork_bool  BadRefs() const { return mNode_Refs < mNode_Uses; }

  mork_uses  StrongRefsOnly() const { return mNode_Uses; }
  mork_refs  WeakRefsOnly() const { return (mork_refs) ( mNode_Refs - mNode_Uses ); }

  // (this refcounting derives from public domain IronDoc node refcounts)
  mork_refs    AddStrongRef(morkEnv* ev);
  mork_refs    CutStrongRef(morkEnv* ev);
  mork_refs    AddWeakRef(morkEnv* ev);
  mork_refs    CutWeakRef(morkEnv* ev);

  const char* GetNodeAccessAsString() const; // e.g. "open", "shut", etc.
  const char* GetNodeUsageAsString() const; // e.g. "heap", "stack", etc.

  mork_usage NodeUsage() const { return mNode_Usage; }

  mork_bool IsHeapNode() const
  { return mNode_Usage == morkUsage_kHeap; }

  mork_bool IsStackNode() const
  { return mNode_Usage == morkUsage_kStack; }

  mork_bool IsGlobalNode() const
  { return mNode_Usage == morkUsage_kGlobal; }

  mork_bool IsMemberNode() const
  { return mNode_Usage == morkUsage_kMember; }

  mork_bool IsOpenNode() const
  { return mNode_Access == morkAccess_kOpen; }

  mork_bool IsShutNode() const
  { return mNode_Access == morkAccess_kShut; }

  mork_bool IsDeadNode() const
  { return mNode_Access == morkAccess_kDead; }

  mork_bool IsClosingNode() const
  { return mNode_Access == morkAccess_kClosing; }

  mork_bool IsOpenOrClosingNode() const
  { return IsOpenNode() || IsClosingNode(); }

  mork_bool HasNodeAccess() const
  { return ( IsOpenNode() || IsShutNode() || IsClosingNode() ); }
    
  void MarkShut() { mNode_Access = morkAccess_kShut; }
  void MarkClosing() { mNode_Access = morkAccess_kClosing; }
  void MarkDead() { mNode_Access = morkAccess_kDead; }

public: // refcounting for typesafe subclass inline methods
  static void SlotWeakNode(morkNode* me, morkEnv* ev, morkNode** ioSlot);
  // If *ioSlot is non-nil, that node is released by CutWeakRef() and
  // then zeroed out.  Then if me is non-nil, this is acquired by
  // calling AddWeakRef(), and if positive is returned to show success,
  // then this is put into slot *ioSlot.  Note me can be nil, so we
  // permit expression '((morkNode*) 0L)->SlotWeakNode(ev, &slot)'.
  
  static void SlotStrongNode(morkNode* me, morkEnv* ev, morkNode** ioSlot);
  // If *ioSlot is non-nil, that node is released by CutStrongRef() and
  // then zeroed out.  Then if me is non-nil, this is acquired by
  // calling AddStrongRef(), and if positive is returned to show success,
  // then me is put into slot *ioSlot.  Note me can be nil, so we take
  // expression 'morkNode::SlotStrongNode((morkNode*) 0, ev, &slot)'.
};

extern void // utility method very similar to morkNode::SlotStrongNode():
nsIMdbHeap_SlotStrongHeap(nsIMdbHeap* self, morkEnv* ev, nsIMdbHeap** ioSlot);
  // If *ioSlot is non-nil, that heap is released by CutStrongRef() and
  // then zeroed out.  Then if self is non-nil, this is acquired by
  // calling AddStrongRef(), and if the return value shows success,
  // then self is put into slot *ioSlot.  Note self can be nil, so we take
  // expression 'nsIMdbHeap_SlotStrongHeap(0, ev, &slot)'.

extern void // utility method very similar to morkNode::SlotStrongNode():
nsIMdbFile_SlotStrongFile(nsIMdbFile* self, morkEnv* ev, nsIMdbFile** ioSlot);
  // If *ioSlot is non-nil, that file is released by CutStrongRef() and
  // then zeroed out.  Then if self is non-nil, this is acquired by
  // calling AddStrongRef(), and if the return value shows success,
  // then self is put into slot *ioSlot.  Note self can be nil, so we take
  // expression 'nsIMdbFile_SlotStrongFile(0, ev, &slot)'.

extern void // utility method very similar to morkNode::SlotStrongNode():
nsIMdbCompare_SlotStrongCompare(nsIMdbCompare* self, morkEnv* ev,
  nsIMdbCompare** ioSlot);
  // If *ioSlot is non-nil, that compare is released by CutStrongRef() and
  // then zeroed out.  Then if self is non-nil, this is acquired by
  // calling AddStrongRef(), and if the return value shows success,
  // then self is put into slot *ioSlot.  Note self can be nil, so we take
  // expression 'nsIMdbCompare_SlotStrongCompare(0, ev, &slot)'.

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKNODE_ */
