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
class morkFarBookAtom;

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
  
  mork_count   mPool_UsedFramesCount; // length of mPool_UsedHandleFrames
  mork_count   mPool_FreeFramesCount; // length of mPool_UsedHandleFrames
    
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
  morkHandleFace*  NewHandle(morkEnv* ev, mork_size inSize, morkZone* ioZone);
  void             ZapHandle(morkEnv* ev, morkHandleFace* ioHandle);

  // alloc and free individual instances of rows:
  morkRow*  NewRow(morkEnv* ev, morkZone* ioZone); // alloc new row instance
  void      ZapRow(morkEnv* ev, morkRow* ioRow, morkZone* ioZone); // free old row instance

  // alloc and free entire vectors of cells (not just one cell at a time)
  morkCell* NewCells(morkEnv* ev, mork_size inSize, morkZone* ioZone);
  void      ZapCells(morkEnv* ev, morkCell* ioVector, mork_size inSize, morkZone* ioZone);
  
  // resize (grow or trim) cell vectors inside a containing row instance
  mork_bool AddRowCells(morkEnv* ev, morkRow* ioRow, mork_size inNewSize, morkZone* ioZone);
  mork_bool CutRowCells(morkEnv* ev, morkRow* ioRow, mork_size inNewSize, morkZone* ioZone);
  
  // alloc & free individual instances of atoms (lots of atom subclasses):
  void ZapAtom(morkEnv* ev, morkAtom* ioAtom, morkZone* ioZone); // any subclass (by kind)
  
  morkOidAtom* NewRowOidAtom(morkEnv* ev, const mdbOid& inOid, morkZone* ioZone); 
  morkOidAtom* NewTableOidAtom(morkEnv* ev, const mdbOid& inOid, morkZone* ioZone);
  
  morkAtom* NewAnonAtom(morkEnv* ev, const morkBuf& inBuf,
    mork_cscode inForm, morkZone* ioZone);
    // if inForm is zero, and inBuf.mBuf_Fill is less than 256, then a 'wee'
    // anon atom will be created, and otherwise a 'big' anon atom.
    
  morkBookAtom* NewBookAtom(morkEnv* ev, const morkBuf& inBuf,
    mork_cscode inForm, morkAtomSpace* ioSpace, mork_aid inAid, morkZone* ioZone);
    // if inForm is zero, and inBuf.mBuf_Fill is less than 256, then a 'wee'
    // book atom will be created, and otherwise a 'big' book atom.
    
  morkBookAtom* NewBookAtomCopy(morkEnv* ev, const morkBigBookAtom& inAtom, morkZone* ioZone);
    // make the smallest kind of book atom that can hold content in inAtom.
    // The inAtom parameter is often expected to be a staged book atom in
    // the store, which was used to search an atom space for existing atoms.
    
  morkBookAtom* NewFarBookAtomCopy(morkEnv* ev, const morkFarBookAtom& inAtom, morkZone* ioZone);
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

