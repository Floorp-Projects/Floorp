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
#include "mozilla/dom/StructuredCloneHelper.h"
#include "mozilla/dom/workers/bindings/WorkerFeature.h"
#include "nsProxyRelease.h"

#include "WorkerRunnable.h"

namespace mozilla {
namespace dom {

class Promise;

namespace workers {
class WorkerPrivate;
} // namespace workers

// A proxy to (eventually) mirror a resolved/rejected Promise's result from the
// main thread to a Promise on the worker thread.
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
//
//   2. Create a PromiseWorkerProxy wrapping the Promise. If this fails, the
//      worker is shutting down and you should fail the original call. This is
//      only likely to happen in (Gecko-specific) worker onclose handlers.
//
//        nsRefPtr<PromiseWorkerProxy> proxy =
//          PromiseWorkerProxy::Create(workerPrivate, promise);
//        if (!proxy) {
//          // You may also reject the Promise with an AbortError or similar.
//          return nullptr;
//        }
//
//   3. Dispatch a runnable to the main thread, with a reference to the proxy to
//      perform the main thread operation. PromiseWorkerProxy is thread-safe
//      refcounted.
//
//   4. Return the worker thread promise to the JS caller:
//
//        return promise.forget();
//
//   5. In your main thread runnable Run(), obtain a Promise on
//      the main thread and call its AppendNativeHandler(PromiseNativeHandler*)
//      to bind the PromiseWorkerProxy created at #2.
//
//   4. Then the Promise results returned by ResolvedCallback/RejectedCallback
//      will be dispatched as a WorkerRunnable to the worker thread to
//      resolve/reject the Promise created at #1.
//
// PromiseWorkerProxy can also be used in situations where there is no main
// thread Promise, or where special handling is required on the worker thread
// for promise resolution. Create a PromiseWorkerProxy as in steps 1 to 3
// above. When the main thread is ready to resolve the worker thread promise:
//
//   1. Acquire the mutex before attempting to access the worker private.
//
//        AssertIsOnMainThread();
//        MutexAutoLock lock(proxy->Lock());
//        if (proxy->CleanedUp()) {
//          // Worker has already shut down, can't access worker private.
//          return;
//        }
//
//   2. Dispatch a runnable to the worker. Use GetWorkerPrivate() to acquire the
//      worker.
//
//        nsRefPtr<FinishTaskWorkerRunnable> runnable =
//          new FinishTaskWorkerRunnable(proxy->GetWorkerPrivate(), proxy, result);
//        AutoJSAPI jsapi;
//        jsapi.Init();
//        if (!r->Dispatch(jsapi.cx())) {
//          // Worker is alive but not Running any more, so the Promise can't
//          // be resolved, give up. The proxy will get Release()d at some
//          // point.
//
//          // Usually do nothing, but you may want to log the fact.
//        }
//
//   3. In the WorkerRunnable's WorkerRun() use WorkerPromise() to access the
//      Promise and resolve/reject it. Then call CleanUp().
//
//        bool
//        WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
//        {
//          aWorkerPrivate->AssertIsOnWorkerThread();
//          nsRefPtr<Promise> promise = mProxy->WorkerPromise();
//          promise->MaybeResolve(mResult);
//          mProxy->CleanUp(aCx);
//        }
//
// Note: If a PromiseWorkerProxy is not cleaned up by a WorkerRunnable - this
// can happen if the main thread Promise is never fulfilled - it will
// stay alive till the worker reaches a Canceling state, even if all external
// references to it are dropped.

class PromiseWorkerProxy : public PromiseNativeHandler
                         , public workers::WorkerFeature
                         , public StructuredCloneHelperInternal
{
  friend class PromiseWorkerProxyRunnable;

  NS_DECL_THREADSAFE_ISUPPORTS

public:
  typedef JSObject* (*ReadCallbackOp)(JSContext* aCx,
                                      JSStructuredCloneReader* aReader,
                                      const PromiseWorkerProxy* aProxy,
                                      uint32_t aTag,
                                      uint32_t aData);
  typedef bool (*WriteCallbackOp)(JSContext* aCx,
                                  JSStructuredCloneWriter* aWorker,
                                  PromiseWorkerProxy* aProxy,
                                  JS::HandleObject aObj);

  struct PromiseWorkerProxyStructuredCloneCallbacks
  {
    ReadCallbackOp Read;
    WriteCallbackOp Write;
  };

  static already_AddRefed<PromiseWorkerProxy>
  Create(workers::WorkerPrivate* aWorkerPrivate,
         Promise* aWorkerPromise,
         const PromiseWorkerProxyStructuredCloneCallbacks* aCallbacks = nullptr);

  // Main thread callers must hold Lock() and check CleanUp() before calling this.
  // Worker thread callers, this will assert that the proxy has not been cleaned
  // up.
  workers::WorkerPrivate* GetWorkerPrivate() const;

  // This should only be used within WorkerRunnable::WorkerRun() running on the
  // worker thread! Do not call this after calling CleanUp().
  Promise* WorkerPromise() const;

  void StoreISupports(nsISupports* aSupports);

  // Worker thread only. Calling this invalidates several assumptions, so be
  // sure this is the last thing you do.
  // 1. WorkerPrivate() will no longer return a valid worker.
  // 2. WorkerPromise() will crash!
  void CleanUp(JSContext* aCx);

  Mutex& Lock()
  {
    return mCleanUpLock;
  }

  bool CleanedUp() const
  {
    mCleanUpLock.AssertCurrentThreadOwns();
    return mCleanedUp;
  }

  // StructuredCloneHelperInternal

  JSObject* ReadCallback(JSContext* aCx,
                         JSStructuredCloneReader* aReader,
                         uint32_t aTag,
                         uint32_t aIndex) override;

  bool WriteCallback(JSContext* aCx,
                     JSStructuredCloneWriter* aWriter,
                     JS::Handle<JSObject*> aObj) override;

protected:
  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

  virtual bool Notify(JSContext* aCx, workers::Status aStatus) override;

private:
  PromiseWorkerProxy(workers::WorkerPrivate* aWorkerPrivate,
                     Promise* aWorkerPromise,
                     const PromiseWorkerProxyStructuredCloneCallbacks* aCallbacks = nullptr);

  virtual ~PromiseWorkerProxy();

  bool AddRefObject();

  // If not called from Create(), be sure to hold Lock().
  void CleanProperties();

  // Function pointer for calling Promise::{ResolveInternal,RejectInternal}.
  typedef void (Promise::*RunCallbackFunc)(JSContext*,
                                           JS::Handle<JS::Value>);

  void RunCallback(JSContext* aCx,
                   JS::Handle<JS::Value> aValue,
                   RunCallbackFunc aFunc);

  // Any thread with appropriate checks.
  workers::WorkerPrivate* mWorkerPrivate;

  // Worker thread only.
  nsRefPtr<Promise> mWorkerPromise;

  // Modified on the worker thread.
  // It is ok to *read* this without a lock on the worker.
  // Main thread must always acquire a lock.
  bool mCleanedUp; // To specify if the cleanUp() has been done.

  const PromiseWorkerProxyStructuredCloneCallbacks* mCallbacks;

  // Aimed to keep objects alive when doing the structured-clone read/write,
  // which can be added by calling StoreISupports() on the main thread.
  nsTArray<nsMainThreadPtrHandle<nsISupports>> mSupportsArray;

  // Ensure the worker and the main thread won't race to access |mCleanedUp|.
  Mutex mCleanUpLock;

  // Maybe get rid of this entirely and rely on mCleanedUp
  DebugOnly<bool> mFeatureAdded;
};
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PromiseWorkerProxy_h
