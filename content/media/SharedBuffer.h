/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SHAREDBUFFER_H_
#define MOZILLA_SHAREDBUFFER_H_

#include "mozilla/mozalloc.h"

namespace mozilla {

/**
 * Heap-allocated chunk of arbitrary data with threadsafe refcounting.
 * Typically you would allocate one of these, fill it in, and then treat it as
 * immutable while it's shared.
 * This only guarantees 4-byte alignment of the data. For alignment we
 * simply assume that the refcount is at least 4-byte aligned and its size
 * is divisible by 4.
 */
class SharedBuffer {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedBuffer)
  ~SharedBuffer() {}

  void* Data() { return this + 1; }

  // Takes ownership of aData (which will be freed via moz_free()).
  // aData consists of aChannels consecutive buffers, each of aLength samples.
  static already_AddRefed<SharedBuffer> Create(size_t aSize)
  {
    void* m = moz_xmalloc(sizeof(SharedBuffer) + aSize);
    nsRefPtr<SharedBuffer> p = new (m) SharedBuffer();
    NS_ASSERTION((reinterpret_cast<char*>(p.get() + 1) - reinterpret_cast<char*>(p.get())) % 4 == 0,
                 "SharedBuffers should be at least 4-byte aligned");
    return p.forget();
  }

private:
  SharedBuffer() {}
};

}

#endif /* MOZILLA_SHAREDBUFFER_H_ */
