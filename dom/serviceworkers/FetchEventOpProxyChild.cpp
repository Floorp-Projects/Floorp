/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchEventOpProxyChild.h"

#include <utility>

#include "mozilla/dom/FetchTypes.h"
#include "mozilla/dom/ServiceWorkerOpPromise.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/RemoteWorkerService.h"
#include "mozilla/dom/ServiceWorkerOp.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/IPCStreamUtils.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

nsresult GetIPCSynthesizeResponseArgs(
    ChildToParentSynthesizeResponseArgs* aIPCArgs,
    SynthesizeResponseArgs&& aArgs, UniquePtr<AutoIPCStream>& aAutoBodyStream,
    UniquePtr<AutoIPCStream>& aAutoAlternativeBodyStream) {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());

  SafeRefPtr<InternalResponse> internalResponse;
  FetchEventRespondWithClosure closure;
  FetchEventTimeStamps timeStamps;
  Tie(internalResponse, closure, timeStamps) = std::move(aArgs);

  aIPCArgs->closure() = std::move(closure);
  aIPCArgs->timeStamps() = std::move(timeStamps);

  PBackgroundChild* bgChild = BackgroundChild::GetOrCreateForCurrentThread();

  if (NS_WARN_IF(!bgChild)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  internalResponse->ToChildToParentInternalResponse(
      &aIPCArgs->internalResponse(), bgChild, aAutoBodyStream,
      aAutoAlternativeBodyStream);
  return NS_OK;
}

}  // anonymous namespace

void FetchEventOpProxyChild::Initialize(
    const ParentToChildServiceWorkerFetchEventOpArgs& aArgs) {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());
  MOZ_ASSERT(!mOp);

  mInternalRequest =
      MakeSafeRefPtr<InternalRequest>(aArgs.common().internalRequest());

  if (aArgs.common().preloadNavigation()) {
    // We use synchronous task dispatch here to make sure that if the preload
    // response arrived before we dispatch the fetch event, then the JS preload
    // response promise will get resolved immediately.
    mPreloadResponsePromise =
        MakeRefPtr<FetchEventPreloadResponsePromise::Private>(__func__);
    mPreloadResponsePromise->UseSynchronousTaskDispatch(__func__);
    if (aArgs.preloadResponse().isSome()) {
      FetchEventPreloadResponseArgs response = MakeTuple(
          InternalResponse::FromIPC(aArgs.preloadResponse().ref().response()),
          aArgs.preloadResponse().ref().timingData(),
          aArgs.preloadResponse().ref().initiatorType(),
          aArgs.preloadResponse().ref().entryName());

      mPreloadResponsePromise->Resolve(std::move(response), __func__);
    }
  }

  RemoteWorkerChild* manager = static_cast<RemoteWorkerChild*>(Manager());
  MOZ_ASSERT(manager);

  RefPtr<FetchEventOpProxyChild> self = this;

  auto callback = [self](const ServiceWorkerOpResult& aResult) {
    if (!self->CanSend()) {
      return;
    }

    if (NS_WARN_IF(aResult.type() == ServiceWorkerOpResult::Tnsresult)) {
      Unused << self->Send__delete__(self, aResult.get_nsresult());
      return;
    }

    MOZ_ASSERT(aResult.type() ==
               ServiceWorkerOpResult::TServiceWorkerFetchEventOpResult);

    Unused << self->Send__delete__(self, aResult);
  };

  RefPtr<FetchEventOp> op = ServiceWorkerOp::Create(aArgs, std::move(callback))
                                .template downcast<FetchEventOp>();

  MOZ_ASSERT(op);

  op->SetActor(this);
  mOp = op;

  op->GetRespondWithPromise()
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = std::move(self)](
                 FetchEventRespondWithPromise::ResolveOrRejectValue&& aResult) {
               self->mRespondWithPromiseRequestHolder.Complete();

               if (NS_WARN_IF(aResult.IsReject())) {
                 MOZ_ASSERT(NS_FAILED(aResult.RejectValue().status()));

                 Unused << self->SendRespondWith(aResult.RejectValue());
                 return;
               }

               auto& result = aResult.ResolveValue();

               if (result.is<SynthesizeResponseArgs>()) {
                 ChildToParentSynthesizeResponseArgs ipcArgs;
                 UniquePtr<AutoIPCStream> autoBodyStream =
                     MakeUnique<AutoIPCStream>();
                 UniquePtr<AutoIPCStream> autoAlternativeBodyStream =
                     MakeUnique<AutoIPCStream>();
                 nsresult rv = GetIPCSynthesizeResponseArgs(
                     &ipcArgs, result.extract<SynthesizeResponseArgs>(),
                     autoBodyStream, autoAlternativeBodyStream);

                 if (NS_WARN_IF(NS_FAILED(rv))) {
                   Unused << self->SendRespondWith(
                       CancelInterceptionArgs(rv, ipcArgs.timeStamps()));
                   return;
                 }

                 Unused << self->SendRespondWith(ipcArgs);

                 if (ipcArgs.internalResponse().body()) {
                   autoBodyStream->TakeValue();
                 }

                 if (ipcArgs.internalResponse().alternativeBody()) {
                   autoAlternativeBodyStream->TakeValue();
                 }
               } else if (result.is<ResetInterceptionArgs>()) {
                 Unused << self->SendRespondWith(
                     result.extract<ResetInterceptionArgs>());
               } else {
                 Unused << self->SendRespondWith(
                     result.extract<CancelInterceptionArgs>());
               }
             })
      ->Track(mRespondWithPromiseRequestHolder);

  manager->MaybeStartOp(std::move(op));
}

SafeRefPtr<InternalRequest> FetchEventOpProxyChild::ExtractInternalRequest() {
  MOZ_ASSERT(IsCurrentThreadRunningWorker());
  MOZ_ASSERT(mInternalRequest);

  return std::move(mInternalRequest);
}

RefPtr<FetchEventPreloadResponsePromise>
FetchEventOpProxyChild::GetPreloadResponsePromise() {
  return mPreloadResponsePromise;
}

mozilla::ipc::IPCResult FetchEventOpProxyChild::RecvPreloadResponse(
    ParentToChildResponseWithTiming&& aResponse) {
  // Receiving this message implies that navigation preload is enabled, so
  // Initialize() should have created this promise.
  MOZ_ASSERT(mPreloadResponsePromise);

  FetchEventPreloadResponseArgs response = MakeTuple(
      InternalResponse::FromIPC(aResponse.response()), aResponse.timingData(),
      aResponse.initiatorType(), aResponse.entryName());

  mPreloadResponsePromise->Resolve(std::move(response), __func__);

  return IPC_OK();
}

void FetchEventOpProxyChild::ActorDestroy(ActorDestroyReason) {
  Unused << NS_WARN_IF(mRespondWithPromiseRequestHolder.Exists());
  mRespondWithPromiseRequestHolder.DisconnectIfExists();

  // If mPreloadResponsePromise exists, navigation preloading response will not
  // be valid anymore since it is too late to respond to the FetchEvent.
  // Resolve the preload response promise with NS_ERROR_DOM_ABORT_ERR.
  if (mPreloadResponsePromise) {
    IPCPerformanceTimingData timingData;
    FetchEventPreloadResponseArgs response =
        MakeTuple(InternalResponse::NetworkError(NS_ERROR_DOM_ABORT_ERR),
                  timingData, EmptyString(), EmptyString());

    mPreloadResponsePromise->Resolve(std::move(response), __func__);
  }

  mOp->RevokeActor(this);
  mOp = nullptr;
}

}  // namespace dom
}  // namespace mozilla
