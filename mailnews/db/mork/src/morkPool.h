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

#ifndef _MORKPOOL_
#define _MORKPOOL_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKDEQUE_
#include "morkDeque.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class morkHandle;
class morkHandleFrame;
class morkHandleFace; // just an opaque cookie type
class morkBigBookAtom;

#define morkDerived_kPool     /*i*/ 0x706C /* ascii 'pl' */

/*| morkPool: a place to manage pools of non-node objects that are memory
**| managed out of large chunks of space, so that per-object management
**| space overhead has no signficant cost.
|*/
class morkPool : public morkNode {
  
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

public: // state is public because the entire Mork system is private
  nsIMdbHeap*  mPool_Heap; // NON-refcounted heap instance
  
  morkDeque    mPool_Blocks;      // linked list of large blocks from heap
  
  // These two lists contain instances of morkHandleFrame:
  morkDeque    mPool_UsedHandleFrames; // handle frames currently being used
  morkDeque    mPool_FreeHandleFrames; // handle frames currently in free list
    
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // ClosePool() only if open
  virtual ~morkPool(); // assert that ClosePool() executed earlier
  
public: // morkPool construction & destruction
  morkPool(const morkUsage& inUsage, nsIMdbHeap* ioHeap,
    nsIMdbHeap* ioSlotHeap);
  morkPool(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap,
    nsIMdbHeap* ioSlotHeap);
  void ClosePool(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkPool(const morkPool& other);
  morkPool& operator=(const morkPool& other);

public: // dynamic type identification
  mork_bool IsPool() const
  { return IsNode() && mNode_Derived == morkDerived_kPool; }
// } ===== end morkNode methods =====

public: // typing
  void NonPoolTypeError(morkEnv* ev);

public: // morkNode memory management operators
  void* operator new(size_t inSize, nsIMdbHeap& ioHeap, morkEnv* ev)
  { return morkNode::MakeNew(inSize, ioHeap, ev); }
  
  void* operator new(size_t inSize)
  { return ::operator new(inSize); }
  
  void operator delete(void* ioAddress)
  { morkNode::OnDeleteAssert(ioAddress); }
  // do NOT call delete on morkNode instances.  Call ZapOld() instead.

public: // other pool methods

  // alloc and free individual instances of handles (inside hand frames):
  morkHandleFace*  NewHandle(morkEnv* ev, mork_size inSize);
  void             ZapHandle(morkEnv* ev, morkHandleFace* ioHandle);

  // alloc and free individual instances of rows:
  morkRow*  NewRow(morkEnv* ev); // allocate a new row instance
  void      ZapRow(morkEnv* ev, morkRow* ioRow); // free old row instance

  // alloc and free entire vectors of cells (not just one cell at a time)
  morkCell* NewCells(morkEnv* ev, mork_size inSize);
  void      ZapCells(morkEnv* ev, morkCell* ioVector, mork_size inSize);
  
  // resize (grow or trim) cell vectors inside a containing row instance
  mork_bool AddRowCells(morkEnv* ev, morkRow* ioRow, mork_size inNewSize);
  mork_bool CutRowCells(morkEnv* ev, morkRow* ioRow, mork_size inNewSize);
  
  // alloc & free individual instances of atoms (lots of atom subclasses):
  void ZapAtom(morkEnv* ev, morkAtom* ioAtom); // any subclass (by kind)
  
  morkOidAtom* NewRowOidAtom(morkEnv* ev, const mdbOid& inOid); 
  morkOidAtom* NewTableOidAtom(morkEnv* ev, const mdbOid& inOid);
  
  morkAtom* NewAnonAtom(morkEnv* ev, const morkBuf& inBuf,
    mork_cscode inForm);
    // if inForm is zero, and inBuf.mBuf_Fill is less than 256, then a 'wee'
    // anon atom will be created, and otherwise a 'big' anon atom.
    
  morkBookAtom* NewBookAtom(morkEnv* ev, const morkBuf& inBuf,
    mork_cscode inForm, morkAtomSpace* ioSpace, mork_aid inAid);
    // if inForm is zero, and inBuf.mBuf_Fill is less than 256, then a 'wee'
    // book atom will be created, and otherwise a 'big' book atom.
    
  morkBookAtom* NewBookAtomCopy(morkEnv* ev, const morkBigBookAtom& inAtom);
    // make the smallest kind of book atom that can hold content in inAtom.
    // The inAtom parameter is often expected to be a staged book atom in
    // the store, which was used to search an atom space for existing atoms.

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakPool(morkPool* me,
    morkEnv* ev, morkPool** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongPool(morkPool* me,
    morkEnv* ev, morkPool** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKPOOL_ */
