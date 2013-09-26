/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StackArena.h"
#include "nsAlgorithm.h"
#include "nsDebug.h"

namespace mozilla {

// A block of memory that the stack will chop up and hand out.
struct StackBlock {
  // Subtract sizeof(StackBlock*) to give space for the |mNext| field.
  static const size_t MAX_USABLE_SIZE = 4096 - sizeof(StackBlock*);

  // A block of memory.
  char mBlock[MAX_USABLE_SIZE];

  // Another block of memory that would only be created if our stack
  // overflowed.
  StackBlock* mNext;

  StackBlock() : mNext(nullptr) { }
  ~StackBlock() { }
};

static_assert(sizeof(StackBlock) == 4096, "StackBlock must be 4096 bytes");

// We hold an array of marks. A push pushes a mark on the stack.
// A pop pops it off.
struct StackMark {
  // The block of memory from which we are currently handing out chunks.
  StackBlock* mBlock;

  // Our current position in the block.
  size_t mPos;
};

StackArena* AutoStackArena::gStackArena;

StackArena::StackArena()
{
  mMarkLength = 0;
  mMarks = nullptr;

  // Allocate our stack memory.
  mBlocks = new StackBlock();
  mCurBlock = mBlocks;

  mStackTop = 0;
  mPos = 0;
}

StackArena::~StackArena()
{
  // Free up our data.
  delete [] mMarks;
  while (mBlocks) {
    StackBlock* toDelete = mBlocks;
    mBlocks = mBlocks->mNext;
    delete toDelete;
  }
}

size_t
StackArena::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
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

static const int STACK_ARENA_MARK_INCREMENT = 50;

void
StackArena::Push()
{
  // Resize the mark array if we overrun it.  Failure to allocate the
  // mark array is not fatal; we just won't free to that mark.  This
  // allows callers not to worry about error checking.
  if (mStackTop >= mMarkLength) {
    uint32_t newLength = mStackTop + STACK_ARENA_MARK_INCREMENT;
    StackMark* newMarks = new StackMark[newLength];
    if (newMarks) {
      if (mMarkLength) {
        memcpy(newMarks, mMarks, sizeof(StackMark)*mMarkLength);
      }
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

  // Set a mark at the top (if we can).
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

  // Align to a multiple of 8.
  aSize = NS_ROUNDUP<size_t>(aSize, 8);

  // On stack overflow, grab another block.
  if (mPos + aSize >= StackBlock::MAX_USABLE_SIZE) {
    NS_ASSERTION(aSize <= StackBlock::MAX_USABLE_SIZE,
                 "Requested memory is greater that our block size!!");
    if (mCurBlock->mNext == nullptr) {
      mCurBlock->mNext = new StackBlock();
    }

    mCurBlock = mCurBlock->mNext;
    mPos = 0;
  }

  // Return the chunk they need.
  void *result = mCurBlock->mBlock + mPos;
  mPos += aSize;

  return result;
}

void
StackArena::Pop()
{
  // Pop off the mark.
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
