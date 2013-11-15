/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StackArena_h
#define StackArena_h

#include "nsError.h"
#include "mozilla/Assertions.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/NullPtr.h"

namespace mozilla {

struct StackBlock;
struct StackMark;
class AutoStackArena;

// Private helper class for AutoStackArena.
class StackArena {
private:
  friend class AutoStackArena;
  StackArena();
  ~StackArena();

  nsresult Init() { return mBlocks ? NS_OK : NS_ERROR_OUT_OF_MEMORY; }

  // Memory management functions.
  void* Allocate(size_t aSize);
  void Push();
  void Pop();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Our current position in memory.
  size_t mPos;

  // A list of memory blocks. Usually there is only one
  // but if we overrun our stack size we can get more memory.
  StackBlock* mBlocks;

  // The current block.
  StackBlock* mCurBlock;

  // Our stack of mark where push has been called.
  StackMark* mMarks;

  // The current top of the mark list.
  uint32_t mStackTop;

  // The size of the mark array.
  uint32_t mMarkLength;
};

// Class for stack scoped arena memory allocations.
//
// Callers who wish to allocate memory whose lifetime corresponds to the
// lifetime of a stack-allocated object can use this class.  First,
// declare an AutoStackArena object on the stack.  Then all subsequent
// calls to Allocate will allocate memory from an arena pool that will
// be freed when that variable goes out of scope.  Nesting is allowed.
//
// Individual allocations cannot exceed StackBlock::MAX_USABLE_SIZE
// bytes.
//
class MOZ_STACK_CLASS AutoStackArena {
public:
  AutoStackArena()
    : mOwnsStackArena(false)
  {
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
      gStackArena = nullptr;
    }
  }

  static void* Allocate(size_t aSize) {
    return gStackArena->Allocate(aSize);
  }

private:
  static StackArena* gStackArena;
  bool mOwnsStackArena;
};

} // namespace mozilla

#endif
