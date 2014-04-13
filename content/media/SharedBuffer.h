/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SHAREDBUFFER_H_
#define MOZILLA_SHAREDBUFFER_H_

#include "mozilla/CheckedInt.h"
#include "mozilla/mozalloc.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

namespace mozilla {

/**
 * Base class for objects with a thread-safe refcount and a virtual
 * destructor.
 */
class ThreadSharedObject {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ThreadSharedObject)

  bool IsShared() { return mRefCnt.get() > 1; }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return 0;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~ThreadSharedObject() {}
};

/**
 * Heap-allocated chunk of arbitrary data with threadsafe refcounting.
 * Typically you would allocate one of these, fill it in, and then treat it as
 * immutable while it's shared.
 * This only guarantees 4-byte alignment of the data. For alignment we
 * simply assume that the refcount is at least 4-byte aligned and its size
 * is divisible by 4.
 */
class SharedBuffer : public ThreadSharedObject {
public:
  void* Data() { return this + 1; }

  static already_AddRefed<SharedBuffer> Create(size_t aSize)
  {
    CheckedInt<size_t> size = sizeof(SharedBuffer);
    size += aSize;
    if (!size.isValid()) {
      MOZ_CRASH();
    }
    void* m = moz_xmalloc(size.value());
    nsRefPtr<SharedBuffer> p = new (m) SharedBuffer();
    NS_ASSERTION((reinterpret_cast<char*>(p.get() + 1) - reinterpret_cast<char*>(p.get())) % 4 == 0,
                 "SharedBuffers should be at least 4-byte aligned");
    return p.forget();
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  SharedBuffer() {}
};

}

#endif /* MOZILLA_SHAREDBUFFER_H_ */
