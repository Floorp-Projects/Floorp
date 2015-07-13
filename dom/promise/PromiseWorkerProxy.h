/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseWorkerProxy_h
#define mozilla_dom_PromiseWorkerProxy_h

// Required for Promise::PromiseTaskSync.
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/workers/bindings/WorkerFeature.h"
#include "nsProxyRelease.h"

#include "WorkerRunnable.h"

namespace mozilla {
namespace dom {

class Promise;

namespace workers {
class WorkerPrivate;
} // namespace workers

// A proxy to catch the resolved/rejected Promise's result from the main thread
// and resolve/reject that on the worker thread eventually.
//
// How to use:
//
//   1. Create a Promise on the worker thread and return it to the content
//      script:
//
//        nsRefPtr<Promise> promise = Promise::Create(workerPrivate->GlobalScope(), aRv);
//        if (aRv.Failed()) {
//          return nullptr;
//        }
//        // Pass |promise| around to the WorkerMainThreadRunnable
//        return promise.forget();
//
//   2. In your WorkerMainThreadRunnable's ctor, create a PromiseWorkerProxy
//      which holds a nsRefPtr<Promise> to the Promise created at #1.
//
//   3. In your WorkerMainThreadRunnable::MainThreadRun(), obtain a Promise on
//      the main thread and call its AppendNativeHandler(PromiseNativeHandler*)
//      to bind the PromiseWorkerProxy created at #2.
//
//   4. Then the Promise results returned by ResolvedCallback/RejectedCallback
//      will be dispatched as a WorkerRunnable to the worker thread to
//      resolve/reject the Promise created at #1.
//
// PromiseWorkerProxy can also be used in situations where there is no main
// thread Promise, or where special handling is required on the worker thread
// for promise resolution. Create a PromiseWorkerProxy as in steps 1 and
// 2 above. When the main thread is ready to resolve the worker thread promise,
// dispatch a runnable to the worker. Use GetWorkerPrivate() to acquire the
// worker. This might be null! In the WorkerRunnable's WorkerRun() use
// GetWorkerPromise() to access the Promise and resolve/reject it. Then call
// CleanUp() on the worker thread.
//
// IMPORTANT: Dispatching the runnable to the worker thread may fail causing
// the promise to leak. To successfully release the promise on the
// worker thread in this case, use |PromiseWorkerProxyControlRunnable| to
// dispatch a control runnable that will deref the object on the correct thread.

class PromiseWorkerProxy : public PromiseNativeHandler,
                           public workers::WorkerFeature
{
  friend class PromiseWorkerProxyRunnable;

  NS_DECL_THREADSAFE_ISUPPORTS

public:
  static already_AddRefed<PromiseWorkerProxy>
  Create(workers::WorkerPrivate* aWorkerPrivate,
         Promise* aWorkerPromise,
         const JSStructuredCloneCallbacks* aCallbacks = nullptr);

  workers::WorkerPrivate* GetWorkerPrivate() const;

  Promise* GetWorkerPromise() const;

  void StoreISupports(nsISupports* aSupports);

  void CleanUp(JSContext* aCx);

  Mutex& GetCleanUpLock()
  {
    return mCleanUpLock;
  }

  bool IsClean() const
  {
    mCleanUpLock.AssertCurrentThreadOwns();
    return mCleanedUp;
  }

protected:
  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

  virtual bool Notify(JSContext* aCx, workers::Status aStatus) override;

private:
  PromiseWorkerProxy(workers::WorkerPrivate* aWorkerPrivate,
                     Promise* aWorkerPromise,
                     const JSStructuredCloneCallbacks* aCallbacks = nullptr);

  virtual ~PromiseWorkerProxy();

  // Function pointer for calling Promise::{ResolveInternal,RejectInternal}.
  typedef void (Promise::*RunCallbackFunc)(JSContext*,
                                           JS::Handle<JS::Value>);

  void RunCallback(JSContext* aCx,
                   JS::Handle<JS::Value> aValue,
                   RunCallbackFunc aFunc);

  workers::WorkerPrivate* mWorkerPrivate;

  // This lives on the worker thread.
  nsRefPtr<Promise> mWorkerPromise;

  bool mCleanedUp; // To specify if the cleanUp() has been done.

  const JSStructuredCloneCallbacks* mCallbacks;

  // Aimed to keep objects alive when doing the structured-clone read/write,
  // which can be added by calling StoreISupports() on the main thread.
  nsTArray<nsMainThreadPtrHandle<nsISupports>> mSupportsArray;

  // Ensure the worker and the main thread won't race to access |mCleanedUp|.
  Mutex mCleanUpLock;
};

// Helper runnable used for releasing the proxied promise when the worker
// is not accepting runnables and the promise object would leak.
// See the instructions above.
class PromiseWorkerProxyControlRunnable final : public workers::WorkerControlRunnable
{
  nsRefPtr<PromiseWorkerProxy> mProxy;

public:
  PromiseWorkerProxyControlRunnable(workers::WorkerPrivate* aWorkerPrivate,
                                    PromiseWorkerProxy* aProxy)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , mProxy(aProxy)
  {
    MOZ_ASSERT(aProxy);
  }

  virtual bool
  WorkerRun(JSContext* aCx, workers::WorkerPrivate* aWorkerPrivate) override;

private:
  ~PromiseWorkerProxyControlRunnable()
  {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PromiseWorkerProxy_h
