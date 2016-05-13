/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_ITERABLEARENA_H_
#define MOZILLA_GFX_ITERABLEARENA_H_

#include "mozilla/Move.h"
#include "mozilla/Assertions.h"
#include "mozilla/gfx/Logging.h"

#include <string.h>
#include <vector>
#include <stdint.h>
#include <stdio.h>

namespace mozilla {
namespace gfx {

/// A simple pool allocator for plain data structures.
///
/// Beware that the pool will not attempt to run the destructors. It is the
/// responsibility of the user of this class to either use objects with no
/// destructor or to manually call the allocated objects destructors.
/// If the pool is growable, its allocated objects must be safely moveable in
/// in memory (through memcpy).
class IterableArena {
protected:
  struct Header
  {
    size_t mBlocSize;
  };
public:
  enum ArenaType {
    FIXED_SIZE,
    GROWABLE
  };

  IterableArena(ArenaType aType, size_t aStorageSize)
  : mSize(aStorageSize)
  , mCursor(0)
  , mIsGrowable(aType == GROWABLE)
  {
    if (mSize == 0) {
      mSize = 128;
    }

    mStorage = (uint8_t*)malloc(mSize);
    if (mStorage == nullptr) {
      gfxCriticalError() << "Not enough Memory allocate a memory pool of size " << aStorageSize;
      MOZ_CRASH("GFX: Out of memory IterableArena");
    }
  }

  ~IterableArena()
  {
    free(mStorage);
  }

  /// Constructs a new item in the pool and returns a positive offset in case of
  /// success.
  ///
  /// The offset never changes even if the storage is reallocated, so users
  /// of this class should prefer storing offsets rather than direct pointers
  /// to the allocated objects.
  /// Alloc can cause the storage to be reallocated if the pool was initialized
  /// with IterableArena::GROWABLE.
  /// If for any reason the pool fails to allocate enough space for the new item
  /// Alloc returns a negative offset and the object's constructor is not called.
  template<typename T, typename... Args>
  ptrdiff_t
  Alloc(Args&&... aArgs)
  {
    void* storage = nullptr;
    auto offset = AllocRaw(sizeof(T), &storage);
    if (offset < 0) {
      return offset;
    }
    new (storage) T(Forward<Args>(aArgs)...);
    return offset;
  }

  ptrdiff_t AllocRaw(size_t aSize, void** aOutPtr = nullptr)
  {
    const size_t blocSize = AlignedSize(sizeof(Header) + aSize);

    if (AlignedSize(mCursor + blocSize) > mSize) {
      if (!mIsGrowable) {
        return -1;
      }

      size_t newSize = mSize * 2;
      while (AlignedSize(mCursor + blocSize) > newSize) {
        newSize *= 2;
      }

      uint8_t* newStorage = (uint8_t*)realloc(mStorage, newSize);
      if (!newStorage) {
         gfxCriticalError() << "Not enough Memory to grow the memory pool, size: " << newSize;
        return -1;
      }

      mStorage = newStorage;
      mSize = newSize;
    }
    ptrdiff_t offset = mCursor;
    GetHeader(offset)->mBlocSize = blocSize;
    mCursor += blocSize;
    if (aOutPtr) {
        *aOutPtr = GetStorage(offset);
    }
    return offset;
  }

  /// Get access to an allocated item at a given offset (only use offsets returned
  /// by Alloc or AllocRaw).
  ///
  /// If the pool is growable, the returned pointer is only valid temporarily. The
  /// underlying storage can be reallocated in Alloc or AllocRaw, so do not keep
  /// these pointers around and store the offset instead.
  void* GetStorage(ptrdiff_t offset = 0)
  {
    MOZ_ASSERT(offset >= 0);
    MOZ_ASSERT(offset < mCursor);
    return offset >= 0 ? mStorage + offset + sizeof(Header) : nullptr;
  }

  /// Clears the storage without running any destructor and without deallocating it.
  void Clear()
  {
    mCursor = 0;
  }

  /// Iterate over the elements allocated in this pool.
  ///
  /// Takes a lambda or function object accepting a void* as parameter.
  template<typename Func>
  void ForEach(Func cb)
  {
    Iterator it;
    while (void* ptr = it.Next(this)) {
      cb(ptr);
    }
  }

  /// A simple iterator over an arena.
  class Iterator {
  public:
    Iterator()
    : mCursor(0)
    {}

    void* Next(IterableArena* aArena)
    {
      if (mCursor >= aArena->mCursor) {
        return nullptr;
      }
      void* result = aArena->GetStorage(mCursor);
      const size_t blocSize = aArena->GetHeader(mCursor)->mBlocSize;
      MOZ_ASSERT(blocSize != 0);
      mCursor += blocSize;
      return result;
    }

  private:
    ptrdiff_t mCursor;
  };

protected:
  Header* GetHeader(ptrdiff_t offset)
  {
    return (Header*) (mStorage + offset);
  }

  size_t AlignedSize(size_t aSize) const
  {
    const size_t alignment = sizeof(uintptr_t);
    return aSize + (alignment - (aSize % alignment)) % alignment;
  }

  uint8_t* mStorage;
  uint32_t mSize;
  ptrdiff_t mCursor;
  bool mIsGrowable;

  friend class Iterator;
};

} // namespace
} // namespace

#endif
