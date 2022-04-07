/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_AsyncBlockers_h
#define mozilla_ipc_AsyncBlockers_h

// FIXME: when bug 1760855 is fixed, it should not be required anymore

namespace mozilla::ipc {

/**
 * AsyncBlockers provide a simple registration service that allows to suspend
 * completion of a particular task until all registered entries have been
 * cleared. This can be used to implement a similar service to
 * nsAsyncShutdownService in processes where it wouldn't normally be available.
 * This class is thread-safe.
 */
class AsyncBlockers {
 public:
  AsyncBlockers()
      : mLock("AsyncRegistrar"),
        mPromise(new GenericPromise::Private(__func__)) {}
  void Register(void* aBlocker) {
    MutexAutoLock lock(mLock);
    if (mResolved) {
      // Too late.
      return;
    }
    mBlockers.insert({aBlocker, true});
  }
  void Deregister(void* aBlocker) {
    MutexAutoLock lock(mLock);
    if (mResolved) {
      // Too late.
      return;
    }
    auto it = mBlockers.find(aBlocker);
    MOZ_ASSERT(it != mBlockers.end());

    mBlockers.erase(it);
    MaybeResolve();
  }
  RefPtr<GenericPromise> WaitUntilClear(uint32_t aTimeOutInMs = 0) {
    if (!aTimeOutInMs) {
      // We don't need to wait, resolve the promise right away.
      MutexAutoLock lock(mLock);
      if (!mResolved) {
        mPromise->Resolve(true, __func__);
        mResolved = true;
      }
    } else {
      GetCurrentEventTarget()->DelayedDispatch(
          NS_NewRunnableFunction("AsyncBlockers::WaitUntilClear",
                                 [promise = mPromise]() {
                                   // The AsyncBlockers object may have been
                                   // deleted by now and the object isn't
                                   // refcounted (nor do we want it to be). We
                                   // can unconditionally resolve the promise
                                   // even it has already been resolved as
                                   // MozPromise are thread-safe and will just
                                   // ignore the action if already resolved.
                                   promise->Resolve(true, __func__);
                                 }),
          aTimeOutInMs);
    }
    return mPromise;
  }

  virtual ~AsyncBlockers() {
    if (!mResolved) {
      mPromise->Resolve(true, __func__);
    }
  }

 private:
  void MaybeResolve() {
    mLock.AssertCurrentThreadOwns();
    if (mResolved) {
      return;
    }
    if (!mBlockers.empty()) {
      return;
    }
    mPromise->Resolve(true, __func__);
    mResolved = true;
  }
  Mutex mLock MOZ_UNANNOTATED;  // protects mBlockers and mResolved.
  std::map<void*, bool> mBlockers;
  bool mResolved = false;
  const RefPtr<GenericPromise::Private> mPromise;
};

}  // namespace mozilla::ipc

#endif  // mozilla_ipc_AsyncBlockers_h
