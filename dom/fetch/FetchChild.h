/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fetchChild_h__
#define mozilla_dom_fetchChild_h__

#include "mozilla/dom/AbortSignal.h"
#include "mozilla/dom/AbortFollower.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/PFetchChild.h"
#include "nsIConsoleReportCollector.h"
#include "nsIContentSecurityPolicy.h"
#include "nsISupports.h"

namespace mozilla::dom {

class FetchObserver;
class ThreadSafeWorkerRef;
class Promise;
class WorkerPrivate;

class FetchChild final : public PFetchChild, public AbortFollower {
  friend class PFetchChild;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  mozilla::ipc::IPCResult Recv__delete__(const nsresult&& aResult);

  mozilla::ipc::IPCResult RecvOnResponseAvailableInternal(
      ParentToChildInternalResponse&& aResponse);

  mozilla::ipc::IPCResult RecvOnResponseEnd(ResponseEndArgs&& aArgs);

  mozilla::ipc::IPCResult RecvOnDataAvailable();

  mozilla::ipc::IPCResult RecvOnFlushConsoleReport(
      nsTArray<net::ConsoleReportCollected>&& aReports);

  mozilla::ipc::IPCResult RecvOnCSPViolationEvent(const nsAString& aJSon);

  void SetCSPEventListener(nsICSPEventListener* aListener);

  static RefPtr<FetchChild> Create(WorkerPrivate* aWorkerPrivate,
                                   RefPtr<Promise> aPromise,
                                   RefPtr<AbortSignalImpl> aSignalImpl,
                                   RefPtr<FetchObserver> aObserver);

  FetchChild(RefPtr<Promise>&& aPromise, RefPtr<AbortSignalImpl>&& aSignalImpl,
             RefPtr<FetchObserver>&& aObserver);

  // AbortFollower
  void RunAbortAlgorithm() override;

  void DoFetchOp(const FetchOpArgs& aArgs);

 private:
  ~FetchChild() = default;

  // WorkerPrivate shutdown callback.
  void Shutdown();
  void ActorDestroy(ActorDestroyReason aReason) override;

  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
  RefPtr<Promise> mPromise;
  RefPtr<AbortSignalImpl> mSignalImpl;
  RefPtr<FetchObserver> mFetchObserver;
  nsCOMPtr<nsICSPEventListener> mCSPEventListener;
  nsCOMPtr<nsIConsoleReportCollector> mReporter;
  FlippedOnce<false> mIsShutdown;
};

}  // namespace mozilla::dom

#endif
