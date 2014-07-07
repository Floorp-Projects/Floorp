/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_ATOMICREFCOUNTEDWITHFINALIZE_H_
#define MOZILLA_ATOMICREFCOUNTEDWITHFINALIZE_H_

#include "mozilla/RefPtr.h"
#include "mozilla/NullPtr.h"
#include "mozilla/Likely.h"
#include "MainThreadUtils.h"
#include "base/message_loop.h"
#include "base/task.h"

namespace mozilla {

template<typename T>
class AtomicRefCountedWithFinalize
{
  protected:
    AtomicRefCountedWithFinalize()
      : mRecycleCallback(nullptr)
      , mRefCount(0)
      , mMessageLoopToPostDestructionTo(nullptr)
    {}

    ~AtomicRefCountedWithFinalize() {}

    void SetMessageLoopToPostDestructionTo(MessageLoop* l) {
      MOZ_ASSERT(NS_IsMainThread());
      mMessageLoopToPostDestructionTo = l;
    }

    static void DestroyToBeCalledOnMainThread(T* ptr) {
      MOZ_ASSERT(NS_IsMainThread());
      delete ptr;
    }

  public:
    void AddRef() {
      MOZ_ASSERT(mRefCount >= 0);
      ++mRefCount;
    }

    void Release() {
      MOZ_ASSERT(mRefCount > 0);
      // Read mRecycleCallback early so that it does not get set to
      // deleted memory, if the object is goes away.
      RecycleCallback recycleCallback = mRecycleCallback;
      int currCount = --mRefCount;
      if (0 == currCount) {
        // Recycle listeners must call ClearRecycleCallback
        // before releasing their strong reference.
        MOZ_ASSERT(mRecycleCallback == nullptr);
#ifdef DEBUG
        mRefCount = detail::DEAD;
#endif
        T* derived = static_cast<T*>(this);
        derived->Finalize();
        if (MOZ_LIKELY(!mMessageLoopToPostDestructionTo)) {
          delete derived;
        } else {
          if (MOZ_LIKELY(NS_IsMainThread())) {
            delete derived;
          } else {
            mMessageLoopToPostDestructionTo->PostTask(
              FROM_HERE,
              NewRunnableFunction(&DestroyToBeCalledOnMainThread, derived));
          }
        }
      } else if (1 == currCount && recycleCallback) {
        T* derived = static_cast<T*>(this);
        recycleCallback(derived, mClosure);
      }
    }

    typedef void (*RecycleCallback)(T* aObject, void* aClosure);
    /**
     * Set a callback responsible for recycling this object
     * before it is finalized.
     */
    void SetRecycleCallback(RecycleCallback aCallback, void* aClosure)
    {
      mRecycleCallback = aCallback;
      mClosure = aClosure;
    }
    void ClearRecycleCallback() { SetRecycleCallback(nullptr, nullptr); }

    bool HasRecycleCallback() const
    {
      return !!mRecycleCallback;
    }

private:
    RecycleCallback mRecycleCallback;
    void *mClosure;
    Atomic<int> mRefCount;
    MessageLoop *mMessageLoopToPostDestructionTo;
};

}

#endif
