/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_ATOMICREFCOUNTEDWITHFINALIZE_H_
#define MOZILLA_ATOMICREFCOUNTEDWITHFINALIZE_H_

#include "mozilla/RefPtr.h"
#include "mozilla/Likely.h"
#include "MainThreadUtils.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "mozilla/gfx/Logging.h"

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

    ~AtomicRefCountedWithFinalize() {
      if (mRefCount >= 0) {
        gfxCriticalError() << "Deleting referenced object? " << mRefCount;
      }
    }

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
      ++mRefCount;
    }

    void Release() {
      // Read mRecycleCallback early so that it does not get set to
      // deleted memory, if the object is goes away.  See bug 994903.
      // This saves us in the case where there is no callback, so that
      // we can do the "else if" below.
      RecycleCallback recycleCallback = mRecycleCallback;
      int currCount = --mRefCount;
      if (currCount < 0) {
        gfxCriticalError() << "Invalid reference count release" << currCount;
        ++mRefCount;
        return;
      }

      if (0 == currCount) {
        mRefCount = detail::DEAD;
        MOZ_ASSERT(IsDead());

        // Recycle listeners must call ClearRecycleCallback
        // before releasing their strong reference.
        if (mRecycleCallback) {
          gfxCriticalError() << "About to release with valid callback";
          mRecycleCallback = nullptr;
        }

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
        // There is nothing enforcing this in the code, except how the callers
        // are being careful to never let the reference count go down if there
        // is a callback.
        MOZ_ASSERT(!IsDead());
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
      MOZ_ASSERT(!IsDead());
      mRecycleCallback = aCallback;
      mClosure = aClosure;
    }
    void ClearRecycleCallback()
    {
      MOZ_ASSERT(!IsDead());
      SetRecycleCallback(nullptr, nullptr);
    }

    bool HasRecycleCallback() const
    {
      MOZ_ASSERT(!IsDead());
      return !!mRecycleCallback;
    }

    bool IsDead() const
    {
      return mRefCount < 0;
    }

private:
    RecycleCallback mRecycleCallback;
    void *mClosure;
    Atomic<int> mRefCount;
    MessageLoop *mMessageLoopToPostDestructionTo;
};

}

#endif
