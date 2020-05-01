/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchEventOpProxyParent.h"

#include <utility>

#include "nsCOMPtr.h"
#include "nsIInputStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/FetchEventOpParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

void MaybeDeserializeAndReserialize(const Maybe<IPCStream>& aDeserialize,
                                    Maybe<IPCStream>& aReserialize,
                                    UniquePtr<AutoIPCStream>& aAutoStream,
                                    PBackgroundParent* aManager) {
  nsCOMPtr<nsIInputStream> maybeDeserialized =
      DeserializeIPCStream(aDeserialize);

  if (!maybeDeserialized) {
    return;
  }

  aAutoStream.reset(new AutoIPCStream(aReserialize));
  DebugOnly<bool> ok = aAutoStream->Serialize(maybeDeserialized, aManager);
  MOZ_ASSERT(ok);
}

}  // anonymous namespace

/* static */ void FetchEventOpProxyParent::Create(
    PRemoteWorkerParent* aManager, const ServiceWorkerFetchEventOpArgs& aArgs,
    RefPtr<FetchEventOpParent> aReal) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aReal);

  FetchEventOpProxyParent* actor =
      new FetchEventOpProxyParent(std::move(aReal));

  if (aArgs.internalRequest().body().isNothing()) {
    Unused << aManager->SendPFetchEventOpProxyConstructor(actor, aArgs);
    return;
  }

  ServiceWorkerFetchEventOpArgs copyArgs = aArgs;
  IPCInternalRequest& copyRequest = copyArgs.internalRequest();

  PBackgroundParent* bgParent = aManager->Manager();
  MOZ_ASSERT(bgParent);

  UniquePtr<AutoIPCStream> autoBodyStream = MakeUnique<AutoIPCStream>();
  MaybeDeserializeAndReserialize(aArgs.internalRequest().body(),
                                 copyRequest.body(), autoBodyStream, bgParent);

  Unused << aManager->SendPFetchEventOpProxyConstructor(actor, copyArgs);
  autoBodyStream->TakeOptionalValue();
}

FetchEventOpProxyParent::~FetchEventOpProxyParent() {
  AssertIsOnBackgroundThread();
}

FetchEventOpProxyParent::FetchEventOpProxyParent(
    RefPtr<FetchEventOpParent>&& aReal)
    : mReal(std::move(aReal)) {}

mozilla::ipc::IPCResult FetchEventOpProxyParent::RecvAsyncLog(
    const nsCString& aScriptSpec, const uint32_t& aLineNumber,
    const uint32_t& aColumnNumber, const nsCString& aMessageName,
    nsTArray<nsString>&& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mReal);

  Unused << mReal->SendAsyncLog(aScriptSpec, aLineNumber, aColumnNumber,
                                aMessageName, aParams);

  return IPC_OK();
}

mozilla::ipc::IPCResult FetchEventOpProxyParent::RecvRespondWith(
    const IPCFetchEventRespondWithResult& aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mReal);

  // IPCSynthesizeResponseArgs possibly contains an IPCStream. If so,
  // deserialize it and reserialize it before forwarding it to the main thread.
  if (aResult.type() ==
      IPCFetchEventRespondWithResult::TIPCSynthesizeResponseArgs) {
    const IPCSynthesizeResponseArgs& originalArgs =
        aResult.get_IPCSynthesizeResponseArgs();
    const IPCInternalResponse& originalResponse =
        originalArgs.internalResponse();

    // Do nothing if neither the body nor the alt. body can be deserialized.
    if (!originalResponse.body() && !originalResponse.alternativeBody()) {
      Unused << mReal->SendRespondWith(aResult);
      return IPC_OK();
    }

    IPCSynthesizeResponseArgs copyArgs = originalArgs;
    IPCInternalResponse& copyResponse = copyArgs.internalResponse();

    PRemoteWorkerControllerParent* manager = mReal->Manager();
    MOZ_ASSERT(manager);

    PBackgroundParent* bgParent = manager->Manager();
    MOZ_ASSERT(bgParent);

    UniquePtr<AutoIPCStream> autoBodyStream = MakeUnique<AutoIPCStream>();
    UniquePtr<AutoIPCStream> autoAlternativeBodyStream =
        MakeUnique<AutoIPCStream>();

    MaybeDeserializeAndReserialize(originalResponse.body(), copyResponse.body(),
                                   autoBodyStream, bgParent);
    MaybeDeserializeAndReserialize(originalResponse.alternativeBody(),
                                   copyResponse.alternativeBody(),
                                   autoAlternativeBodyStream, bgParent);

    Unused << mReal->SendRespondWith(copyArgs);

    autoBodyStream->TakeOptionalValue();
    autoAlternativeBodyStream->TakeOptionalValue();
  } else {
    Unused << mReal->SendRespondWith(aResult);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult FetchEventOpProxyParent::Recv__delete__(
    const ServiceWorkerFetchEventOpResult& aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mReal);

  Unused << mReal->Send__delete__(mReal, aResult);
  mReal = nullptr;

  return IPC_OK();
}

void FetchEventOpProxyParent::ActorDestroy(ActorDestroyReason) {
  AssertIsOnBackgroundThread();
}

}  // namespace dom
}  // namespace mozilla
