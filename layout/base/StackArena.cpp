/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StackArena.h"

namespace mozilla {

#define STACK_ARENA_MARK_INCREMENT 50
/* a bit under 4096, for malloc overhead */
#define STACK_ARENA_BLOCK_INCREMENT 4044

/**A block of memory that the stack will 
 * chop up and hand out
 */
struct StackBlock {
   
   // a block of memory.  Note that this must be first so that it will
   // be aligned.
   char mBlock[STACK_ARENA_BLOCK_INCREMENT];

   // another block of memory that would only be created
   // if our stack overflowed. Yes we have the ability
   // to grow on a stack overflow
   StackBlock* mNext;

   StackBlock() : mNext(nullptr) { }
   ~StackBlock() { }
};

/* we hold an array of marks. A push pushes a mark on the stack
 * a pop pops it off.
 */
struct StackMark {
   // the block of memory we are currently handing out chunks of
   StackBlock* mBlock;
   
   // our current position in the memory
   size_t mPos;
};

StackArena* AutoStackArena::gStackArena;

StackArena::StackArena()
{
  mMarkLength = 0;
  mMarks = nullptr;

  // allocate our stack memory
  mBlocks = new StackBlock();
  mCurBlock = mBlocks;

  mStackTop = 0;
  mPos = 0;
}

StackArena::~StackArena()
{
  // free up our data
  delete[] mMarks;
  while(mBlocks)
  {
    StackBlock* toDelete = mBlocks;
    mBlocks = mBlocks->mNext;
    delete toDelete;
  }
} 

size_t
StackArena::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = 0;
  StackBlock *block = mBlocks;
  while (block) {
    n += aMallocSizeOf(block);
    block = block->mNext;
  }
  n += aMallocSizeOf(mMarks);
  return n;
}

void
StackArena::Push()
{
  // Resize the mark array if we overrun it.  Failure to allocate the
  // mark array is not fatal; we just won't free to that mark.  This
  // allows callers not to worry about error checking.
  if (mStackTop >= mMarkLength)
  {
    PRUint32 newLength = mStackTop + STACK_ARENA_MARK_INCREMENT;
    StackMark* newMarks = new StackMark[newLength];
    if (newMarks) {
      if (mMarkLength)
        memcpy(newMarks, mMarks, sizeof(StackMark)*mMarkLength);
      // Fill in any marks that we couldn't allocate during a prior call
      // to Push().
      for (; mMarkLength < mStackTop; ++mMarkLength) {
        NS_NOTREACHED("should only hit this on out-of-memory");
        newMarks[mMarkLength].mBlock = mCurBlock;
        newMarks[mMarkLength].mPos = mPos;
      }
      delete [] mMarks;
      mMarks = newMarks;
      mMarkLength = newLength;
    }
  }

  // set a mark at the top (if we can)
  NS_ASSERTION(mStackTop < mMarkLength, "out of memory");
  if (mStackTop < mMarkLength) {
    mMarks[mStackTop].mBlock = mCurBlock;
    mMarks[mStackTop].mPos = mPos;
  }

  mStackTop++;
}

void*
StackArena::Allocate(size_t aSize)
{
  NS_ASSERTION(mStackTop > 0, "Allocate called without Push");

  // make sure we are aligned. Beard said 8 was safer then 4. 
  // Round size to multiple of 8
  aSize = NS_ROUNDUP<size_t>(aSize, 8);

  // if the size makes the stack overflow. Grab another block for the stack
  if (mPos + aSize >= STACK_ARENA_BLOCK_INCREMENT)
  {
    NS_ASSERTION(aSize <= STACK_ARENA_BLOCK_INCREMENT,
                 "Requested memory is greater that our block size!!");
    if (mCurBlock->mNext == nullptr)
      mCurBlock->mNext = new StackBlock();

    mCurBlock =  mCurBlock->mNext;
    mPos = 0;
  }

  // return the chunk they need.
  void *result = mCurBlock->mBlock + mPos;
  mPos += aSize;

  return result;
}

void
StackArena::Pop()
{
  // pop off the mark
  NS_ASSERTION(mStackTop > 0, "unmatched pop");
  mStackTop--;

  if (mStackTop >= mMarkLength) {
    // We couldn't allocate the marks array at the time of the push, so
    // we don't know where we're freeing to.
    NS_NOTREACHED("out of memory");
    if (mStackTop == 0) {
      // But we do know if we've completely pushed the stack.
      mCurBlock = mBlocks;
      mPos = 0;
    }
    return;
  }

#ifdef DEBUG
  // Mark the "freed" memory with 0xdd to help with debugging of memory
  // allocation problems.
  {
    StackBlock *block = mMarks[mStackTop].mBlock, *block_end = mCurBlock;
    size_t pos = mMarks[mStackTop].mPos;
    for (; block != block_end; block = block->mNext, pos = 0) {
      memset(block->mBlock + pos, 0xdd, sizeof(block->mBlock) - pos);
    }
    memset(block->mBlock + pos, 0xdd, mPos - pos);
  }
#endif

  mCurBlock = mMarks[mStackTop].mBlock;
  mPos      = mMarks[mStackTop].mPos;
}

} // namespace mozilla
