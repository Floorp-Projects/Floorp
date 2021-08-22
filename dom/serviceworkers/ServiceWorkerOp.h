/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerop_h__
#define mozilla_dom_serviceworkerop_h__

#include <functional>

#include "nsISupportsImpl.h"

#include "ServiceWorkerEvents.h"
#include "ServiceWorkerOpPromise.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"
#include "mozilla/dom/WorkerRunnable.h"

namespace mozilla {
namespace dom {

class FetchEventOpProxyChild;

class ServiceWorkerOp : public RemoteWorkerChild::Op {
 public:
  // `aCallback` will be called when the operation completes or is canceled.
  static already_AddRefed<ServiceWorkerOp> Create(
      ServiceWorkerOpArgs&& aArgs,
      std::function<void(const ServiceWorkerOpResult&)>&& aCallback);

  ServiceWorkerOp(
      ServiceWorkerOpArgs&& aArgs,
      std::function<void(const ServiceWorkerOpResult&)>&& aCallback);

  ServiceWorkerOp(const ServiceWorkerOp&) = delete;

  ServiceWorkerOp& operator=(const ServiceWorkerOp&) = delete;

  ServiceWorkerOp(ServiceWorkerOp&&) = default;

  ServiceWorkerOp& operator=(ServiceWorkerOp&&) = default;

  // Returns `true` if the operation has started and `false` otherwise.
  bool MaybeStart(RemoteWorkerChild* aOwner,
                  RemoteWorkerChild::State& aState) final;

  void Cancel() final;

 protected:
  ~ServiceWorkerOp();

  bool Started() const;

  bool IsTerminationOp() const;

  // Override to provide a runnable that's not a `ServiceWorkerOpRunnable.`
  virtual RefPtr<WorkerRunnable> GetRunnable(WorkerPrivate* aWorkerPrivate);

  // Overridden by ServiceWorkerOp subclasses, it should return true when
  // the ServiceWorkerOp was executed successfully (and false if it did fail).
  // Content throwing an exception during event dispatch is still considered
  // success.
  virtual bool Exec(JSContext* aCx, WorkerPrivate* aWorkerPrivate) = 0;

  // Override to reject any additional MozPromises that subclasses may contain.
  virtual void RejectAll(nsresult aStatus);

  ServiceWorkerOpArgs mArgs;

  // Subclasses must settle this promise when appropriate.
  MozPromiseHolder<ServiceWorkerOpPromise> mPromiseHolder;

 private:
  class ServiceWorkerOpRunnable;

  bool mStarted = false;
};

class ExtendableEventOp : public ServiceWorkerOp,
                          public ExtendableEventCallback {
  using ServiceWorkerOp::ServiceWorkerOp;

 protected:
  ~ExtendableEventOp() = default;

  void FinishedWithResult(ExtendableEventResult aResult) override;
};

class FetchEventOp final : public ExtendableEventOp,
                           public PromiseNativeHandler {
  using ExtendableEventOp::ExtendableEventOp;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  /**
   * This must be called once and only once before the first call to
   * `MaybeStart()`; `aActor` will be used for `AsyncLog()` and
   * `ReportCanceled().`
   */
  void SetActor(RefPtr<FetchEventOpProxyChild> aActor);

  void RevokeActor(FetchEventOpProxyChild* aActor);

  // This must be called at most once before the first call to `MaybeStart().`
  RefPtr<FetchEventRespondWithPromise> GetRespondWithPromise();

  // This must be called when `FetchEvent::RespondWith()` is called.
  void RespondWithCalledAt(const nsCString& aRespondWithScriptSpec,
                           uint32_t aRespondWithLineNumber,
                           uint32_t aRespondWithColumnNumber);

  void ReportCanceled(const nsCString& aPreventDefaultScriptSpec,
                      uint32_t aPreventDefaultLineNumber,
                      uint32_t aPreventDefaultColumnNumber);

 private:
  class AutoCancel;

  ~FetchEventOp();

  bool Exec(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

  void RejectAll(nsresult aStatus) override;

  void FinishedWithResult(ExtendableEventResult aResult) override;

  /**
   * `{Resolved,Reject}Callback()` are use to handle the
   * `FetchEvent::RespondWith()` promise.
   */
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void MaybeFinished();

  // Requires mRespondWithClosure to be non-empty.
  void AsyncLog(const nsCString& aMessageName, nsTArray<nsString> aParams);

  void AsyncLog(const nsCString& aScriptSpec, uint32_t aLineNumber,
                uint32_t aColumnNumber, const nsCString& aMessageName,
                nsTArray<nsString> aParams);

  void GetRequestURL(nsAString& aOutRequestURL);

  // A failure code means that the dispatch failed.
  nsresult DispatchFetchEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

  // Worker Launcher thread only. Used for `AsyncLog().`
  RefPtr<FetchEventOpProxyChild> mActor;

  /**
   * Created on the Worker Launcher thread and settled on the worker thread.
   * If this isn't settled before `mPromiseHolder` (which it should be),
   * `FetchEventOpChild` will cancel the intercepted network request.
   */
  MozPromiseHolder<FetchEventRespondWithPromise> mRespondWithPromiseHolder;

  // Worker thread only.
  Maybe<ExtendableEventResult> mResult;
  bool mPostDispatchChecksDone = false;

  // Worker thread only; set when `FetchEvent::RespondWith()` is called.
  Maybe<FetchEventRespondWithClosure> mRespondWithClosure;

  // Must be set to `nullptr` on the worker thread because `Promise`'s
  // destructor must be called on the worker thread.
  RefPtr<Promise> mHandled;

  TimeStamp mFetchHandlerStart;
  TimeStamp mFetchHandlerFinish;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_serviceworkerop_h__
