/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_ATOMICREFCOUNTEDWITHFINALIZE_H_
#define MOZILLA_ATOMICREFCOUNTEDWITHFINALIZE_H_

#include "mozilla/RefPtr.h"

namespace mozilla {

template<typename T>
class AtomicRefCountedWithFinalize
{
  protected:
    AtomicRefCountedWithFinalize()
      : mRefCount(0)
    {}

    ~AtomicRefCountedWithFinalize() {}

  public:
    void AddRef() {
      MOZ_ASSERT(mRefCount >= 0);
      ++mRefCount;
    }

    void Release() {
      MOZ_ASSERT(mRefCount > 0);
      if (0 == --mRefCount) {
#ifdef DEBUG
        mRefCount = detail::DEAD;
#endif
        T* derived = static_cast<T*>(this);
        derived->Finalize();
        delete derived;
      }
    }

private:
    Atomic<int> mRefCount;
};

}

#endif
