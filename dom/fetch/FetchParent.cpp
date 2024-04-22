/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchLog.h"
#include "FetchParent.h"
#include "FetchService.h"
#include "InternalRequest.h"
#include "InternalResponse.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/FetchTypes.h"
#include "mozilla/dom/PerformanceTimingTypes.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(FetchParent::FetchParentCSPEventListener, nsICSPEventListener)

FetchParent::FetchParentCSPEventListener::FetchParentCSPEventListener(
    const nsID& aActorID, nsCOMPtr<nsISerialEventTarget> aEventTarget)
    : mActorID(aActorID), mEventTarget(aEventTarget) {
  MOZ_ASSERT(mEventTarget);
  FETCH_LOG(("FetchParentCSPEventListener [%p] actor ID: %s", this,
             mActorID.ToString().get()));
}

NS_IMETHODIMP FetchParent::FetchParentCSPEventListener::OnCSPViolationEvent(
    const nsAString& aJSON) {
  AssertIsOnMainThread();
  FETCH_LOG(("FetchParentCSPEventListener::OnCSPViolationEvent [%p]", this));

  nsAutoString json(aJSON);
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction(__func__, [actorID = mActorID, json]() mutable {
        FETCH_LOG(
            ("FetchParentCSPEventListener::OnCSPViolationEvent, Runnale"));
        RefPtr<FetchParent> actor = FetchParent::GetActorByID(actorID);
        if (actor) {
          actor->OnCSPViolationEvent(json);
        }
      });

  MOZ_ALWAYS_SUCCEEDS(mEventTarget->Dispatch(r, nsIThread::DISPATCH_NORMAL));
  return NS_OK;
}

nsTHashMap<nsIDHashKey, RefPtr<FetchParent>> FetchParent::sActorTable;

/*static*/
RefPtr<FetchParent> FetchParent::GetActorByID(const nsID& aID) {
  AssertIsOnBackgroundThread();
  auto entry = sActorTable.Lookup(aID);
  if (entry) {
    return entry.Data();
  }
  return nullptr;
}

FetchParent::FetchParent() : mID(nsID::GenerateUUID()) {
  FETCH_LOG(("FetchParent::FetchParent [%p]", this));
  AssertIsOnBackgroundThread();
  mBackgroundEventTarget = GetCurrentSerialEventTarget();
  MOZ_ASSERT(mBackgroundEventTarget);
  if (!sActorTable.WithEntryHandle(mID, [&](auto&& entry) {
        if (entry.HasEntry()) {
          return false;
        }
        entry.Insert(this);
        return true;
      })) {
    FETCH_LOG(("FetchParent::FetchParent entry[%p] already exists", this));
  }
}

FetchParent::~FetchParent() {
  FETCH_LOG(("FetchParent::~FetchParent [%p]", this));
  // MOZ_ASSERT(!mBackgroundEventTarget);
  MOZ_ASSERT(!mResponsePromises);
  MOZ_ASSERT(mActorDestroyed && mIsDone);
}

IPCResult FetchParent::RecvFetchOp(FetchOpArgs&& aArgs) {
  FETCH_LOG(("FetchParent::RecvFetchOp [%p]", this));
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(!mIsDone);
  if (mActorDestroyed) {
    return IPC_OK();
  }

  mRequest = MakeSafeRefPtr<InternalRequest>(std::move(aArgs.request()));
  mPrincipalInfo = std::move(aArgs.principalInfo());
  mWorkerScript = aArgs.workerScript();
  mClientInfo = Some(ClientInfo(aArgs.clientInfo()));
  if (aArgs.controller().isSome()) {
    mController = Some(ServiceWorkerDescriptor(aArgs.controller().ref()));
  }
  mCookieJarSettings = aArgs.cookieJarSettings();
  mNeedOnDataAvailable = aArgs.needOnDataAvailable();
  mHasCSPEventListener = aArgs.hasCSPEventListener();
  mIsThirdPartyContext = aArgs.isThirdPartyContext();

  if (mHasCSPEventListener) {
    mCSPEventListener =
        MakeRefPtr<FetchParentCSPEventListener>(mID, mBackgroundEventTarget);
  }
  mAssociatedBrowsingContextID = aArgs.associatedBrowsingContextID();

  MOZ_ASSERT(!mPromise);
  mPromise = new GenericPromise::Private(__func__);

  RefPtr<FetchParent> self = this;
  mPromise->Then(
      mBackgroundEventTarget, __func__,
      [self](const bool&& result) mutable {
        FETCH_LOG(
            ("FetchParent::RecvFetchOp [%p] Success Callback", self.get()));
        AssertIsOnBackgroundThread();
        self->mPromise = nullptr;
        if (self->mIsDone) {
          FETCH_LOG(("FetchParent::RecvFetchOp [%p] Fetch has already aborted",
                     self.get()));
          if (!self->mActorDestroyed) {
            Unused << NS_WARN_IF(
                !self->Send__delete__(self, NS_ERROR_DOM_ABORT_ERR));
          }
          return;
        }
        self->mIsDone = true;
        if (!self->mActorDestroyed && !self->mExtendForCSPEventListener) {
          FETCH_LOG(("FetchParent::RecvFetchOp [%p] Send__delete__(NS_OK)",
                     self.get()));
          Unused << NS_WARN_IF(!self->Send__delete__(self, NS_OK));
        }
      },
      [self](const nsresult&& aErr) mutable {
        FETCH_LOG(
            ("FetchParent::RecvFetchOp [%p] Failure Callback", self.get()));
        AssertIsOnBackgroundThread();
        self->mIsDone = true;
        self->mPromise = nullptr;
        if (!self->mActorDestroyed) {
          FETCH_LOG(("FetchParent::RecvFetchOp [%p] Send__delete__(aErr)",
                     self.get()));
          Unused << NS_WARN_IF(!self->Send__delete__(self, aErr));
        }
      });

  RefPtr<nsIRunnable> r = NS_NewRunnableFunction(__func__, [self]() mutable {
    FETCH_LOG(
        ("FetchParent::RecvFetchOp [%p], Main Thread Runnable", self.get()));
    AssertIsOnMainThread();
    if (self->mIsDone) {
      MOZ_ASSERT(!self->mResponsePromises);
      MOZ_ASSERT(self->mPromise);
      FETCH_LOG(
          ("FetchParent::RecvFetchOp [%p], Main Thread Runnable, "
           "already aborted",
           self.get()));
      self->mPromise->Reject(NS_ERROR_DOM_ABORT_ERR, __func__);
      return;
    }
    RefPtr<FetchService> fetchService = FetchService::GetInstance();
    MOZ_ASSERT(fetchService);
    MOZ_ASSERT(!self->mResponsePromises);
    self->mResponsePromises =
        fetchService->Fetch(AsVariant(FetchService::WorkerFetchArgs(
            {self->mRequest.clonePtr(), self->mPrincipalInfo,
             self->mWorkerScript, self->mClientInfo, self->mController,
             self->mCookieJarSettings, self->mNeedOnDataAvailable,
             self->mCSPEventListener, self->mAssociatedBrowsingContextID,
             self->mBackgroundEventTarget, self->mID,
             self->mIsThirdPartyContext})));

    self->mResponsePromises->GetResponseEndPromise()->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [self](ResponseEndArgs&& aArgs) mutable {
          AssertIsOnMainThread();
          MOZ_ASSERT(self->mPromise);
          self->mPromise->Resolve(true, __func__);
          self->mResponsePromises = nullptr;
        },
        [self](CopyableErrorResult&& aErr) mutable {
          AssertIsOnMainThread();
          MOZ_ASSERT(self->mPromise);
          self->mPromise->Reject(aErr.StealNSResult(), __func__);
          self->mResponsePromises = nullptr;
        });
  });

  MOZ_ALWAYS_SUCCEEDS(
      NS_DispatchToMainThread(r.forget(), nsIThread::DISPATCH_NORMAL));

  return IPC_OK();
}

IPCResult FetchParent::RecvAbortFetchOp() {
  FETCH_LOG(("FetchParent::RecvAbortFetchOp [%p]", this));
  AssertIsOnBackgroundThread();

  if (mIsDone) {
    FETCH_LOG(("FetchParent::RecvAbortFetchOp [%p], Already aborted", this));
    return IPC_OK();
  }
  mIsDone = true;

  RefPtr<FetchParent> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(__func__, [self]() mutable {
    FETCH_LOG(("FetchParent::RecvAbortFetchOp Runnable"));
    AssertIsOnMainThread();
    if (self->mResponsePromises) {
      RefPtr<FetchService> fetchService = FetchService::GetInstance();
      MOZ_ASSERT(fetchService);
      fetchService->CancelFetch(std::move(self->mResponsePromises));
    }
  });

  MOZ_ALWAYS_SUCCEEDS(
      NS_DispatchToMainThread(r.forget(), nsIThread::DISPATCH_NORMAL));

  return IPC_OK();
}

void FetchParent::OnResponseAvailableInternal(
    SafeRefPtr<InternalResponse>&& aResponse) {
  FETCH_LOG(("FetchParent::OnResponseAvailableInternal [%p]", this));
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aResponse);
  MOZ_ASSERT(!mActorDestroyed);

  if (mIsDone && aResponse->Type() != ResponseType::Error) {
    FETCH_LOG(
        ("FetchParent::OnResponseAvailableInternal [%p] "
         "Fetch has already aborted",
         this));
    return;
  }

  // To monitor the stream status between processes, response's body can not be
  // serialized as RemoteLazyInputStream. Such that stream close can be
  // propagated to FetchDriver in the parent process.
  aResponse->SetSerializeAsLazy(false);

  // CSP violation notification is asynchronous. Extending the FetchParent's
  // life cycle for the notificaiton.
  if (aResponse->Type() == ResponseType::Error &&
      aResponse->GetErrorCode() == NS_ERROR_CONTENT_BLOCKED &&
      mCSPEventListener) {
    FETCH_LOG(
        ("FetchParent::OnResponseAvailableInternal [%p] "
         "NS_ERROR_CONTENT_BLOCKED",
         this));
    mExtendForCSPEventListener = true;
  }

  Unused << SendOnResponseAvailableInternal(
      aResponse->ToParentToChildInternalResponse(WrapNotNull(Manager())));
}

void FetchParent::OnResponseEnd(const ResponseEndArgs& aArgs) {
  FETCH_LOG(("FetchParent::OnResponseEnd [%p]", this));
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  if (mIsDone && aArgs.endReason() != FetchDriverObserver::eAborted) {
    FETCH_LOG(
        ("FetchParent::OnResponseEnd [%p] "
         "Fetch has already aborted",
         this));
    return;
  }

  Unused << SendOnResponseEnd(aArgs);
}

void FetchParent::OnDataAvailable() {
  FETCH_LOG(("FetchParent::OnDataAvailable [%p]", this));
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  Unused << SendOnDataAvailable();
}

void FetchParent::OnFlushConsoleReport(
    const nsTArray<net::ConsoleReportCollected>& aReports) {
  FETCH_LOG(("FetchParent::OnFlushConsoleReport [%p]", this));
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  Unused << SendOnFlushConsoleReport(aReports);
}

void FetchParent::OnReportPerformanceTiming(const ResponseTiming&& aTiming) {
  FETCH_LOG(("FetchParent::OnReportPerformanceTiming [%p]", this));
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  Unused << SendOnReportPerformanceTiming(aTiming);
}

void FetchParent::OnNotifyNetworkMonitorAlternateStack(uint64_t aChannelID) {
  FETCH_LOG(("FetchParent::OnNotifyNetworkMonitorAlternateStack [%p]", this));
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  Unused << SendOnNotifyNetworkMonitorAlternateStack(aChannelID);
}

void FetchParent::ActorDestroy(ActorDestroyReason aReason) {
  FETCH_LOG(("FetchParent::ActorDestroy [%p]", this));
  AssertIsOnBackgroundThread();
  mActorDestroyed = true;
  auto entry = sActorTable.Lookup(mID);
  if (entry) {
    entry.Remove();
    FETCH_LOG(("FetchParent::ActorDestroy entry [%p] removed", this));
  }
  // Force to abort the existing fetch.
  // Actor can be destoried by shutdown when still fetching.
  RecvAbortFetchOp();
  // mBackgroundEventTarget = nullptr;
}

nsICSPEventListener* FetchParent::GetCSPEventListener() {
  return mCSPEventListener;
}

void FetchParent::OnCSPViolationEvent(const nsAString& aJSON) {
  FETCH_LOG(("FetchParent::OnCSPViolationEvent [%p]", this));
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mHasCSPEventListener);
  MOZ_ASSERT(!mActorDestroyed);

  Unused << SendOnCSPViolationEvent(aJSON);
}

}  // namespace mozilla::dom
