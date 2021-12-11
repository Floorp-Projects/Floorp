/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchEventOpProxyChild.h"

#include <utility>

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
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
    IPCSynthesizeResponseArgs* aIPCArgs, SynthesizeResponseArgs&& aArgs,
    UniquePtr<AutoIPCStream>& aAutoBodyStream,
    UniquePtr<AutoIPCStream>& aAutoAlternativeBodyStream) {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());

  RefPtr<InternalResponse> internalResponse;
  FetchEventRespondWithClosure closure;
  FetchEventTimeStamps timeStamps;
  Tie(internalResponse, closure, timeStamps) = aArgs;

  aIPCArgs->closure() = std::move(closure);
  aIPCArgs->timeStamps() = std::move(timeStamps);

  PBackgroundChild* bgChild = BackgroundChild::GetOrCreateForCurrentThread();

  if (NS_WARN_IF(!bgChild)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  internalResponse->ToIPC(&aIPCArgs->internalResponse(), bgChild,
                          aAutoBodyStream, aAutoAlternativeBodyStream);
  return NS_OK;
}

}  // anonymous namespace

void FetchEventOpProxyChild::Initialize(
    const ServiceWorkerFetchEventOpArgs& aArgs) {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());
  MOZ_ASSERT(!mOp);

  mInternalRequest = MakeSafeRefPtr<InternalRequest>(aArgs.internalRequest());

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
                 IPCSynthesizeResponseArgs ipcArgs;
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

void FetchEventOpProxyChild::ActorDestroy(ActorDestroyReason) {
  Unused << NS_WARN_IF(mRespondWithPromiseRequestHolder.Exists());
  mRespondWithPromiseRequestHolder.DisconnectIfExists();

  mOp->RevokeActor(this);
  mOp = nullptr;
}

}  // namespace dom
}  // namespace mozilla
