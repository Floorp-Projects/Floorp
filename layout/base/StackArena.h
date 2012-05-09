/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "nsAlgorithm.h"
#include "nsDebug.h"

namespace mozilla {

class StackBlock;
class StackMark;
class AutoStackArena;
 
/**
 * Private helper class for AutoStackArena.
 */
class StackArena {
private:
  friend class AutoStackArena;
  StackArena();
  ~StackArena();

  nsresult Init() { return mBlocks ? NS_OK : NS_ERROR_OUT_OF_MEMORY; }

  // Memory management functions
  void* Allocate(size_t aSize);
  void Push();
  void Pop();

  size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  // our current position in memory
  size_t mPos;

  // a list of memory block. Usually there is only one
  // but if we overrun our stack size we can get more memory.
  StackBlock* mBlocks;

  // the current block of memory we are passing our chucks of
  StackBlock* mCurBlock;

  // our stack of mark where push has been called
  StackMark* mMarks;

  // the current top of the mark list
  PRUint32 mStackTop;

  // the size of the mark array
  PRUint32 mMarkLength;
};


/**
 * Class for stack scoped arena memory allocations.
 *
 * Callers who wish to allocate memory whose lifetime corresponds to
 * the lifetime of a stack-allocated object can use this class.
 * First, declare an AutoStackArena object on the stack.
 * Then all subsequent calls to Allocate will allocate memory from an
 * arena pool that will be freed when that variable goes out of scope.
 * Nesting is allowed.
 *
 * The allocations cannot be for more than 4044 bytes.
 */
class NS_STACK_CLASS AutoStackArena {
public:
  AutoStackArena() : mOwnsStackArena(false) {
    if (!gStackArena) {
      gStackArena = new StackArena();
      mOwnsStackArena = true;
      gStackArena->Init();
    }
    gStackArena->Push();
  }

  ~AutoStackArena() {
    gStackArena->Pop();
    if (mOwnsStackArena) {
      delete gStackArena;
      gStackArena = nsnull;
    }
  }

  static void* Allocate(size_t aSize) {
    MOZ_ASSERT(aSize <= 4044);
    return gStackArena->Allocate(aSize);
  }

private:
  static StackArena* gStackArena;
  bool mOwnsStackArena;
};


} // namespace mozilla
