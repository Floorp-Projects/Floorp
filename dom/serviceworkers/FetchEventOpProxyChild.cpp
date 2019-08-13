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

  PBackgroundChild* bgChild = BackgroundChild::GetOrCreateForCurrentThread();

  if (NS_WARN_IF(!bgChild)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  aArgs.first()->ToIPC(&aIPCArgs->internalResponse(), bgChild, aAutoBodyStream,
                       aAutoAlternativeBodyStream);
  aIPCArgs->closure() = std::move(aArgs.second());

  return NS_OK;
}

}  // anonymous namespace

void FetchEventOpProxyChild::Initialize(
    const ServiceWorkerFetchEventOpArgs& aArgs) {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());

  mInternalRequest = new InternalRequest(aArgs.internalRequest());

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
}

RefPtr<InternalRequest> FetchEventOpProxyChild::ExtractInternalRequest() {
  MOZ_ASSERT(IsCurrentThreadRunningWorker());
  MOZ_ASSERT(mInternalRequest);

  return RefPtr<InternalRequest>(std::move(mInternalRequest));
}

void FetchEventOpProxyChild::ActorDestroy(ActorDestroyReason) {
  Unused << NS_WARN_IF(mRespondWithPromiseRequestHolder.Exists());
  mRespondWithPromiseRequestHolder.DisconnectIfExists();
}

}  // namespace dom
}  // namespace mozilla
