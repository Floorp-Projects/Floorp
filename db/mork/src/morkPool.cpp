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

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKPOOL_
#include "morkPool.h"
#endif

#ifndef _MORKATOM_
#include "morkAtom.h"
#endif

#ifndef _MORKHANDLE_
#include "morkHandle.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _MORKBLOB_
#include "morkBlob.h"
#endif

#ifndef _MORKDEQUE_
#include "morkDeque.h"
#endif

#ifndef _MORKZONE_
#include "morkZone.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkPool::CloseMorkNode(morkEnv* ev) // ClosePool() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->ClosePool(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkPool::~morkPool() // assert ClosePool() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkPool::morkPool(const morkUsage& inUsage, nsIMdbHeap* ioHeap,
  nsIMdbHeap* ioSlotHeap)
: morkNode(inUsage, ioHeap)
, mPool_Heap( ioSlotHeap )
, mPool_UsedFramesCount( 0 )
, mPool_FreeFramesCount( 0 )
{
  // mPool_Heap is NOT refcounted
  MORK_ASSERT(ioSlotHeap);
  if ( ioSlotHeap )
    mNode_Derived = morkDerived_kPool;
}

/*public non-poly*/
morkPool::morkPool(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkNode(ev, inUsage, ioHeap)
, mPool_Heap( ioSlotHeap )
, mPool_UsedFramesCount( 0 )
, mPool_FreeFramesCount( 0 )
{
  if ( ioSlotHeap )
  {
    // mPool_Heap is NOT refcounted:
    // nsIMdbHeap_SlotStrongHeap(ioSlotHeap, ev, &mPool_Heap);
    if ( ev->Good() )
      mNode_Derived = morkDerived_kPool;
  }
  else
    ev->NilPointerError();
}

/*public non-poly*/ void
morkPool::ClosePool(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
#ifdef morkZone_CONFIG_ARENA
#else /*morkZone_CONFIG_ARENA*/
    //MORK_USED_1(ioZone);
#endif /*morkZone_CONFIG_ARENA*/

      nsIMdbHeap* heap = mPool_Heap;
      nsIMdbEnv* mev = ev->AsMdbEnv();
      morkLink* aLink;
      morkDeque* d = &mPool_FreeHandleFrames;
      while ( (aLink = d->RemoveFirst()) != 0 )
        heap->Free(mev, aLink);
  
      this->MarkShut();
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 


// alloc and free individual instances of handles (inside hand frames):
morkHandleFace*
morkPool::NewHandle(morkEnv* ev, mork_size inSize, morkZone* ioZone)
{
  void* newBlock = 0;
  if ( inSize <= sizeof(morkHandleFrame) )
  {
    morkLink* firstLink = mPool_FreeHandleFrames.RemoveFirst();
    if ( firstLink )
    {
      newBlock = firstLink;
      if ( mPool_FreeFramesCount )
        --mPool_FreeFramesCount;
      else
        ev->NewWarning("mPool_FreeFramesCount underflow");
    }
    else
      mPool_Heap->Alloc(ev->AsMdbEnv(), sizeof(morkHandleFrame),
        (void**) &newBlock);
  }
  else
  {
    ev->NewWarning("inSize > sizeof(morkHandleFrame)");
    mPool_Heap->Alloc(ev->AsMdbEnv(), inSize, (void**) &newBlock);
  }
#ifdef morkZone_CONFIG_ARENA
#else /*morkZone_CONFIG_ARENA*/
  MORK_USED_1(ioZone);
#endif /*morkZone_CONFIG_ARENA*/

  return (morkHandleFace*) newBlock;
}

void
morkPool::ZapHandle(morkEnv* ev, morkHandleFace* ioHandle)
{
  if ( ioHandle )
  {
    morkLink* handleLink = (morkLink*) ioHandle;
    mPool_FreeHandleFrames.AddLast(handleLink);
    ++mPool_FreeFramesCount;
  }
}


// alloc and free individual instances of rows:
morkRow*
morkPool::NewRow(morkEnv* ev, morkZone* ioZone) // allocate a new row instance
{
  morkRow* newRow = 0;
  
#ifdef morkZone_CONFIG_ARENA
  // a zone 'chip' remembers no size, and so cannot be deallocated:
  newRow = (morkRow*) ioZone->ZoneNewChip(ev, sizeof(morkRow));
#else /*morkZone_CONFIG_ARENA*/
  MORK_USED_1(ioZone);
  mPool_Heap->Alloc(ev->AsMdbEnv(), sizeof(morkRow), (void**) &newRow);
#endif /*morkZone_CONFIG_ARENA*/

  if ( newRow )
    MORK_MEMSET(newRow, 0, sizeof(morkRow));
  
  return newRow;
}

void
morkPool::ZapRow(morkEnv* ev, morkRow* ioRow,
  morkZone* ioZone) // free old row instance
{
#ifdef morkZone_CONFIG_ARENA
  if ( !ioRow )
    ev->NilPointerWarning(); // a zone 'chip' cannot be freed
#else /*morkZone_CONFIG_ARENA*/
  MORK_USED_1(ioZone);
  if ( ioRow )
    mPool_Heap->Free(ev->AsMdbEnv(), ioRow);
#endif /*morkZone_CONFIG_ARENA*/
}

// alloc and free entire vectors of cells (not just one cell at a time)
morkCell*
morkPool::NewCells(morkEnv* ev, mork_size inSize,
  morkZone* ioZone)
{
  morkCell* newCells = 0;

  mork_size size = inSize * sizeof(morkCell);
  if ( size )
  {
#ifdef morkZone_CONFIG_ARENA
    // a zone 'run' knows its size, and can indeed be deallocated:
    newCells = (morkCell*) ioZone->ZoneNewRun(ev, size);
#else /*morkZone_CONFIG_ARENA*/
    MORK_USED_1(ioZone);
    mPool_Heap->Alloc(ev->AsMdbEnv(), size, (void**) &newCells);
#endif /*morkZone_CONFIG_ARENA*/
  }
    
  // note morkAtom depends on having nil stored in all new mCell_Atom slots:
  if ( newCells )
    MORK_MEMSET(newCells, 0, size);
  return newCells;
}

void
morkPool::ZapCells(morkEnv* ev, morkCell* ioVector, mork_size inSize,
  morkZone* ioZone)
{
  MORK_USED_1(inSize);

  if ( ioVector )
  {
#ifdef morkZone_CONFIG_ARENA
    // a zone 'run' knows its size, and can indeed be deallocated:
    ioZone->ZoneZapRun(ev, ioVector);
#else /*morkZone_CONFIG_ARENA*/
    MORK_USED_1(ioZone);
    mPool_Heap->Free(ev->AsMdbEnv(), ioVector);
#endif /*morkZone_CONFIG_ARENA*/
  }
}

// resize (grow or trim) cell vectors inside a containing row instance
mork_bool
morkPool::AddRowCells(morkEnv* ev, morkRow* ioRow, mork_size inNewSize,
  morkZone* ioZone)
{
  // note strong implementation similarity to morkArray::Grow()

  MORK_USED_1(ioZone);
#ifdef morkZone_CONFIG_ARENA
#else /*morkZone_CONFIG_ARENA*/
#endif /*morkZone_CONFIG_ARENA*/

  mork_fill fill = ioRow->mRow_Length;
  if ( ev->Good() && fill < inNewSize ) // need more cells?
  {
    morkCell* newCells = this->NewCells(ev, inNewSize, ioZone);
    if ( newCells )
    {
      morkCell* c = newCells; // for iterating during copy
      morkCell* oldCells = ioRow->mRow_Cells;
      morkCell* end = oldCells + fill; // copy all the old cells
      while ( oldCells < end )
      {
        *c++ = *oldCells++; // bitwise copy each old cell struct
      }
      oldCells = ioRow->mRow_Cells;
      ioRow->mRow_Cells = newCells;
      ioRow->mRow_Length = (mork_u2) inNewSize;
      ++ioRow->mRow_Seed;
      
      if ( oldCells )
        this->ZapCells(ev, oldCells, fill, ioZone);
    }
  }
  return ( ev->Good() && ioRow->mRow_Length >= inNewSize );
}

mork_bool
morkPool::CutRowCells(morkEnv* ev, morkRow* ioRow,
  mork_size inNewSize,
  morkZone* ioZone)
{
  MORK_USED_1(ioZone);
#ifdef morkZone_CONFIG_ARENA
#else /*morkZone_CONFIG_ARENA*/
#endif /*morkZone_CONFIG_ARENA*/

  mork_fill fill = ioRow->mRow_Length;
  if ( ev->Good() && fill > inNewSize ) // need fewer cells?
  {
    if ( inNewSize ) // want any row cells at all?
    {
      morkCell* newCells = this->NewCells(ev, inNewSize, ioZone);
      if ( newCells )
      {
        morkCell* saveNewCells = newCells; // Keep newcell pos
        morkCell* oldCells = ioRow->mRow_Cells;
        morkCell* oldEnd = oldCells + fill; // one past all old cells
        morkCell* newEnd = oldCells + inNewSize; // copy only kept old cells
        while ( oldCells < newEnd )
        {
          *newCells++ = *oldCells++; // bitwise copy each old cell struct
        }
        while ( oldCells < oldEnd )
        {
          if ( oldCells->mCell_Atom ) // need to unref old cell atom?
            oldCells->SetAtom(ev, (morkAtom*) 0, this); // unref cell atom
          ++oldCells;
        }
        oldCells = ioRow->mRow_Cells;
        ioRow->mRow_Cells = saveNewCells;
        ioRow->mRow_Length = (mork_u2) inNewSize;
        ++ioRow->mRow_Seed;
        
        if ( oldCells )
          this->ZapCells(ev, oldCells, fill, ioZone);
      }
    }
    else // get rid of all row cells
    {
      morkCell* oldCells = ioRow->mRow_Cells;
      ioRow->mRow_Cells = 0;
      ioRow->mRow_Length = 0;
      ++ioRow->mRow_Seed;
      
      if ( oldCells )
        this->ZapCells(ev, oldCells, fill, ioZone);
    }
  }
  return ( ev->Good() && ioRow->mRow_Length <= inNewSize );
}

// alloc & free individual instances of atoms (lots of atom subclasses):
void
morkPool::ZapAtom(morkEnv* ev, morkAtom* ioAtom,
  morkZone* ioZone) // any subclass (by kind)
{
#ifdef morkZone_CONFIG_ARENA
  if ( !ioAtom )
    ev->NilPointerWarning(); // a zone 'chip' cannot be freed
#else /*morkZone_CONFIG_ARENA*/
  MORK_USED_1(ioZone);
  if ( ioAtom )
    mPool_Heap->Free(ev->AsMdbEnv(), ioAtom);
#endif /*morkZone_CONFIG_ARENA*/
}

morkOidAtom*
morkPool::NewRowOidAtom(morkEnv* ev, const mdbOid& inOid,
  morkZone* ioZone)
{
  morkOidAtom* newAtom = 0;
  
#ifdef morkZone_CONFIG_ARENA
  // a zone 'chip' remembers no size, and so cannot be deallocated:
  newAtom = (morkOidAtom*) ioZone->ZoneNewChip(ev, sizeof(morkOidAtom));
#else /*morkZone_CONFIG_ARENA*/
  MORK_USED_1(ioZone);
  mPool_Heap->Alloc(ev->AsMdbEnv(), sizeof(morkOidAtom),(void**) &newAtom);
#endif /*morkZone_CONFIG_ARENA*/

  if ( newAtom )
    newAtom->InitRowOidAtom(ev, inOid);
  return newAtom;
}

morkOidAtom*
morkPool::NewTableOidAtom(morkEnv* ev, const mdbOid& inOid,
  morkZone* ioZone)
{
  morkOidAtom* newAtom = 0;

#ifdef morkZone_CONFIG_ARENA
  // a zone 'chip' remembers no size, and so cannot be deallocated:
  newAtom = (morkOidAtom*) ioZone->ZoneNewChip(ev, sizeof(morkOidAtom));
#else /*morkZone_CONFIG_ARENA*/
  MORK_USED_1(ioZone);
  mPool_Heap->Alloc(ev->AsMdbEnv(), sizeof(morkOidAtom), (void**) &newAtom);
#endif /*morkZone_CONFIG_ARENA*/
  if ( newAtom )
    newAtom->InitTableOidAtom(ev, inOid);
  return newAtom;
}

morkAtom*
morkPool::NewAnonAtom(morkEnv* ev, const morkBuf& inBuf,
  mork_cscode inForm,
  morkZone* ioZone)
// if inForm is zero, and inBuf.mBuf_Fill is less than 256, then a 'wee'
// anon atom will be created, and otherwise a 'big' anon atom.
{
  morkAtom* newAtom = 0;

  mork_bool needBig = ( inForm || inBuf.mBuf_Fill > 255 );
  mork_size size = ( needBig )?
    morkBigAnonAtom::SizeForFill(inBuf.mBuf_Fill) :
    morkWeeAnonAtom::SizeForFill(inBuf.mBuf_Fill);

#ifdef morkZone_CONFIG_ARENA
  // a zone 'chip' remembers no size, and so cannot be deallocated:
  newAtom = (morkAtom*) ioZone->ZoneNewChip(ev, size);
#else /*morkZone_CONFIG_ARENA*/
  MORK_USED_1(ioZone);
  mPool_Heap->Alloc(ev->AsMdbEnv(), size, (void**) &newAtom);
#endif /*morkZone_CONFIG_ARENA*/
  if ( newAtom )
  {
    if ( needBig )
      ((morkBigAnonAtom*) newAtom)->InitBigAnonAtom(ev, inBuf, inForm);
    else
      ((morkWeeAnonAtom*) newAtom)->InitWeeAnonAtom(ev, inBuf);
  }
  return newAtom;
}

morkBookAtom*
morkPool::NewBookAtom(morkEnv* ev, const morkBuf& inBuf,
  mork_cscode inForm, morkAtomSpace* ioSpace, mork_aid inAid,
  morkZone* ioZone)
// if inForm is zero, and inBuf.mBuf_Fill is less than 256, then a 'wee'
// book atom will be created, and otherwise a 'big' book atom.
{
  morkBookAtom* newAtom = 0;

  mork_bool needBig = ( inForm || inBuf.mBuf_Fill > 255 );
  mork_size size = ( needBig )?
    morkBigBookAtom::SizeForFill(inBuf.mBuf_Fill) :
    morkWeeBookAtom::SizeForFill(inBuf.mBuf_Fill);

#ifdef morkZone_CONFIG_ARENA
  // a zone 'chip' remembers no size, and so cannot be deallocated:
  newAtom = (morkBookAtom*) ioZone->ZoneNewChip(ev, size);
#else /*morkZone_CONFIG_ARENA*/
  MORK_USED_1(ioZone);
  mPool_Heap->Alloc(ev->AsMdbEnv(), size, (void**) &newAtom);
#endif /*morkZone_CONFIG_ARENA*/
  if ( newAtom )
  {
    if ( needBig )
      ((morkBigBookAtom*) newAtom)->InitBigBookAtom(ev,
        inBuf, inForm, ioSpace, inAid);
    else
      ((morkWeeBookAtom*) newAtom)->InitWeeBookAtom(ev,
        inBuf, ioSpace, inAid);
  }
  return newAtom;
}

morkBookAtom*
morkPool::NewBookAtomCopy(morkEnv* ev, const morkBigBookAtom& inAtom,
  morkZone* ioZone)
  // make the smallest kind of book atom that can hold content in inAtom.
  // The inAtom parameter is often expected to be a staged book atom in
  // the store, which was used to search an atom space for existing atoms.
{
  morkBookAtom* newAtom = 0;

  mork_cscode form = inAtom.mBigBookAtom_Form;
  mork_fill fill = inAtom.mBigBookAtom_Size;
  mork_bool needBig = ( form || fill > 255 );
  mork_size size = ( needBig )?
    morkBigBookAtom::SizeForFill(fill) :
    morkWeeBookAtom::SizeForFill(fill);

#ifdef morkZone_CONFIG_ARENA
  // a zone 'chip' remembers no size, and so cannot be deallocated:
  newAtom = (morkBookAtom*) ioZone->ZoneNewChip(ev, size);
#else /*morkZone_CONFIG_ARENA*/
  MORK_USED_1(ioZone);
  mPool_Heap->Alloc(ev->AsMdbEnv(), size, (void**) &newAtom);
#endif /*morkZone_CONFIG_ARENA*/
  if ( newAtom )
  {
    morkBuf buf(inAtom.mBigBookAtom_Body, fill);
    if ( needBig )
      ((morkBigBookAtom*) newAtom)->InitBigBookAtom(ev,
        buf, form, inAtom.mBookAtom_Space, inAtom.mBookAtom_Id);
    else
      ((morkWeeBookAtom*) newAtom)->InitWeeBookAtom(ev,
        buf, inAtom.mBookAtom_Space, inAtom.mBookAtom_Id);
  }
  return newAtom;
}

morkBookAtom*
morkPool::NewFarBookAtomCopy(morkEnv* ev, const morkFarBookAtom& inAtom,
  morkZone* ioZone)
  // make the smallest kind of book atom that can hold content in inAtom.
  // The inAtom parameter is often expected to be a staged book atom in
  // the store, which was used to search an atom space for existing atoms.
{
  morkBookAtom* newAtom = 0;

  mork_cscode form = inAtom.mFarBookAtom_Form;
  mork_fill fill = inAtom.mFarBookAtom_Size;
  mork_bool needBig = ( form || fill > 255 );
  mork_size size = ( needBig )?
    morkBigBookAtom::SizeForFill(fill) :
    morkWeeBookAtom::SizeForFill(fill);

#ifdef morkZone_CONFIG_ARENA
  // a zone 'chip' remembers no size, and so cannot be deallocated:
  newAtom = (morkBookAtom*) ioZone->ZoneNewChip(ev, size);
#else /*morkZone_CONFIG_ARENA*/
  MORK_USED_1(ioZone);
  mPool_Heap->Alloc(ev->AsMdbEnv(), size, (void**) &newAtom);
#endif /*morkZone_CONFIG_ARENA*/
  if ( newAtom )
  {
    morkBuf buf(inAtom.mFarBookAtom_Body, fill);
    if ( needBig )
      ((morkBigBookAtom*) newAtom)->InitBigBookAtom(ev,
        buf, form, inAtom.mBookAtom_Space, inAtom.mBookAtom_Id);
    else
      ((morkWeeBookAtom*) newAtom)->InitWeeBookAtom(ev,
        buf, inAtom.mBookAtom_Space, inAtom.mBookAtom_Id);
  }
  return newAtom;
}



//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

