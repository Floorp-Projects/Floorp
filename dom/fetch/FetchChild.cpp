/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchChild.h"
#include "FetchLog.h"
#include "FetchObserver.h"
#include "InternalResponse.h"
#include "Request.h"
#include "Response.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/dom/PerformanceTiming.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/SecurityPolicyViolationEventBinding.h"
#include "mozilla/dom/WorkerChannelInfo.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerScope.h"
#include "nsIAsyncInputStream.h"
#include "nsIGlobalObject.h"
#include "nsIObserverService.h"
#include "nsIRunnable.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS0(FetchChild)

mozilla::ipc::IPCResult FetchChild::Recv__delete__(const nsresult&& aResult) {
  FETCH_LOG(("FetchChild::Recv__delete__ [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  MOZ_ASSERT(mWorkerRef->Private());
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  if (mPromise->State() == Promise::PromiseState::Pending) {
    if (NS_FAILED(aResult)) {
      mPromise->MaybeReject(aResult);
      if (mFetchObserver) {
        mFetchObserver->SetState(FetchState::Errored);
      }
    } else {
      mPromise->MaybeResolve(aResult);
      if (mFetchObserver) {
        mFetchObserver->SetState(FetchState::Complete);
      }
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnResponseAvailableInternal(
    ParentToChildInternalResponse&& aResponse) {
  FETCH_LOG(("FetchChild::RecvOnResponseAvailableInternal [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  MOZ_ASSERT(mWorkerRef->Private());
  mWorkerRef->Private()->AssertIsOnWorkerThread();
  SafeRefPtr<InternalResponse> internalResponse =
      InternalResponse::FromIPC(aResponse);
  IgnoredErrorResult result;
  internalResponse->Headers()->SetGuard(HeadersGuardEnum::Immutable, result);
  MOZ_ASSERT(internalResponse);

  if (internalResponse->Type() != ResponseType::Error) {
    if (internalResponse->Type() == ResponseType::Opaque) {
      internalResponse->GeneratePaddingInfo();
    }

    if (mFetchObserver) {
      mFetchObserver->SetState(FetchState::Complete);
    }
    nsCOMPtr<nsIGlobalObject> global;
    global = mWorkerRef->Private()->GlobalScope();
    RefPtr<Response> response =
        new Response(global, internalResponse.clonePtr(), mSignalImpl);
    mPromise->MaybeResolve(response);

    return IPC_OK();
  }

  FETCH_LOG(
      ("FetchChild::RecvOnResponseAvailableInternal [%p] response type is "
       "Error(0x%x)",
       this, static_cast<int32_t>(internalResponse->GetErrorCode())));
  if (mFetchObserver) {
    mFetchObserver->SetState(FetchState::Errored);
  }
  mPromise->MaybeRejectWithTypeError<MSG_FETCH_FAILED>();
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnResponseEnd(ResponseEndArgs&& aArgs) {
  FETCH_LOG(("FetchChild::RecvOnResponseEnd [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  MOZ_ASSERT(mWorkerRef->Private());
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  if (aArgs.endReason() == FetchDriverObserver::eAborted) {
    FETCH_LOG(
        ("FetchChild::RecvOnResponseEnd [%p] endReason is eAborted", this));
    if (mFetchObserver) {
      mFetchObserver->SetState(FetchState::Errored);
    }
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  Unfollow();
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnDataAvailable() {
  FETCH_LOG(("FetchChild::RecvOnDataAvailable [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  MOZ_ASSERT(mWorkerRef->Private());
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  if (mFetchObserver && mFetchObserver->State() == FetchState::Requesting) {
    mFetchObserver->SetState(FetchState::Responding);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnFlushConsoleReport(
    nsTArray<net::ConsoleReportCollected>&& aReports) {
  FETCH_LOG(("FetchChild::RecvOnFlushConsoleReport [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  MOZ_ASSERT(mWorkerRef->Private());
  mWorkerRef->Private()->AssertIsOnWorkerThread();
  MOZ_ASSERT(mReporter);

  RefPtr<ThreadSafeWorkerRef> workerRef = mWorkerRef;
  nsCOMPtr<nsIConsoleReportCollector> reporter = mReporter;

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [reports = std::move(aReports), reporter = std::move(reporter),
                 workerRef = std::move(workerRef)]() mutable {
        for (const auto& report : reports) {
          reporter->AddConsoleReport(
              report.errorFlags(), report.category(),
              static_cast<nsContentUtils::PropertiesFile>(
                  report.propertiesFile()),
              report.sourceFileURI(), report.lineNumber(),
              report.columnNumber(), report.messageName(),
              report.stringParams());
        }

        if (workerRef->Private()->IsServiceWorker()) {
          reporter->FlushReportsToConsoleForServiceWorkerScope(
              workerRef->Private()->ServiceWorkerScope());
        }

        if (workerRef->Private()->IsSharedWorker()) {
          workerRef->Private()
              ->GetRemoteWorkerController()
              ->FlushReportsOnMainThread(reporter);
        }

        reporter->FlushConsoleReports(workerRef->Private()->GetLoadGroup());
      });
  MOZ_ALWAYS_SUCCEEDS(SchedulerGroup::Dispatch(r.forget()));

  return IPC_OK();
}

RefPtr<FetchChild> FetchChild::Create(WorkerPrivate* aWorkerPrivate,
                                      RefPtr<Promise> aPromise,
                                      RefPtr<AbortSignalImpl> aSignalImpl,
                                      RefPtr<FetchObserver> aObserver) {
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<FetchChild> actor = MakeRefPtr<FetchChild>(
      std::move(aPromise), std::move(aSignalImpl), std::move(aObserver));

  RefPtr<StrongWorkerRef> workerRef =
      StrongWorkerRef::Create(aWorkerPrivate, "FetchChild", [actor]() {
        FETCH_LOG(("StrongWorkerRef callback"));
        actor->Shutdown();
      });
  if (NS_WARN_IF(!workerRef)) {
    return nullptr;
  }

  actor->mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  if (NS_WARN_IF(!actor->mWorkerRef)) {
    return nullptr;
  }
  return actor;
}

mozilla::ipc::IPCResult FetchChild::RecvOnCSPViolationEvent(
    const nsAString& aJSON) {
  FETCH_LOG(("FetchChild::RecvOnCSPViolationEvent [%p] aJSON: %s\n", this,
             NS_ConvertUTF16toUTF8(aJSON).BeginReading()));

  nsString JSON(aJSON);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(__func__, [JSON]() mutable {
    SecurityPolicyViolationEventInit violationEventInit;
    if (NS_WARN_IF(!violationEventInit.Init(JSON))) {
      return;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv =
        NS_NewURI(getter_AddRefs(uri), violationEventInit.mBlockedURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (!observerService) {
      return;
    }

    rv = observerService->NotifyObservers(
        uri, CSP_VIOLATION_TOPIC, violationEventInit.mViolatedDirective.get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  });
  MOZ_ALWAYS_SUCCEEDS(SchedulerGroup::Dispatch(r.forget()));

  if (mCSPEventListener) {
    Unused << NS_WARN_IF(
        NS_FAILED(mCSPEventListener->OnCSPViolationEvent(aJSON)));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnReportPerformanceTiming(
    ResponseTiming&& aTiming) {
  FETCH_LOG(("FetchChild::RecvOnReportPerformanceTiming [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  MOZ_ASSERT(mWorkerRef->Private());
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  RefPtr<PerformanceStorage> performanceStorage =
      mWorkerRef->Private()->GetPerformanceStorage();
  if (performanceStorage) {
    performanceStorage->AddEntry(
        aTiming.entryName(), aTiming.initiatorType(),
        MakeUnique<PerformanceTimingData>(aTiming.timingData()));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnNotifyNetworkMonitorAlternateStack(
    uint64_t aChannelID) {
  FETCH_LOG(
      ("FetchChild::RecvOnNotifyNetworkMonitorAlternateStack [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  MOZ_ASSERT(mWorkerRef->Private());
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  if (!mOriginStack) {
    return IPC_OK();
  }

  if (!mWorkerChannelInfo) {
    mWorkerChannelInfo = MakeRefPtr<WorkerChannelInfo>(
        aChannelID, mWorkerRef->Private()->AssociatedBrowsingContextID());
  }

  // Unfortunately, SerializedStackHolder can only be read on the main thread.
  // However, it doesn't block the fetch execution.
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [channel = mWorkerChannelInfo,
                 stack = std::move(mOriginStack)]() mutable {
        NotifyNetworkMonitorAlternateStack(channel, std::move(stack));
      });

  MOZ_ALWAYS_SUCCEEDS(SchedulerGroup::Dispatch(r.forget()));

  return IPC_OK();
}

void FetchChild::SetCSPEventListener(nsICSPEventListener* aListener) {
  MOZ_ASSERT(aListener && !mCSPEventListener);
  mCSPEventListener = aListener;
}

FetchChild::FetchChild(RefPtr<Promise>&& aPromise,
                       RefPtr<AbortSignalImpl>&& aSignalImpl,
                       RefPtr<FetchObserver>&& aObserver)
    : mPromise(std::move(aPromise)),
      mSignalImpl(std::move(aSignalImpl)),
      mFetchObserver(std::move(aObserver)),
      mReporter(new ConsoleReportCollector()) {
  FETCH_LOG(("FetchChild::FetchChild [%p]", this));
}

void FetchChild::RunAbortAlgorithm() {
  FETCH_LOG(("FetchChild::RunAbortAlgorithm [%p]", this));
  if (mIsShutdown) {
    return;
  }
  if (mWorkerRef) {
    Unused << SendAbortFetchOp();
  }
}

void FetchChild::DoFetchOp(const FetchOpArgs& aArgs) {
  FETCH_LOG(("FetchChild::DoFetchOp [%p]", this));
  if (mSignalImpl) {
    if (mSignalImpl->Aborted()) {
      Unused << SendAbortFetchOp();
      return;
    }
    Follow(mSignalImpl);
  }
  Unused << SendFetchOp(aArgs);
}

void FetchChild::Shutdown() {
  FETCH_LOG(("FetchChild::Shutdown [%p]", this));
  if (mIsShutdown) {
    return;
  }
  mIsShutdown.Flip();

  // If mWorkerRef is nullptr here, that means Recv__delete__() must be called
  if (!mWorkerRef) {
    return;
  }
  mPromise = nullptr;
  mFetchObserver = nullptr;
  Unfollow();
  mSignalImpl = nullptr;
  mCSPEventListener = nullptr;
  Unused << SendAbortFetchOp();
  mWorkerRef = nullptr;
}

void FetchChild::ActorDestroy(ActorDestroyReason aReason) {
  FETCH_LOG(("FetchChild::ActorDestroy [%p]", this));
  mPromise = nullptr;
  mFetchObserver = nullptr;
  mSignalImpl = nullptr;
  mCSPEventListener = nullptr;
  mWorkerRef = nullptr;
}

}  // namespace mozilla::dom
