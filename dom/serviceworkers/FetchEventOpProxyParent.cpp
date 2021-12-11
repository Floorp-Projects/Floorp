/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchEventOpProxyParent.h"

#include <utility>

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIInputStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/PRemoteWorkerParent.h"
#include "mozilla/dom/PRemoteWorkerControllerParent.h"
#include "mozilla/dom/FetchEventOpParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/RemoteLazyInputStreamUtils.h"
#include "mozilla/RemoteLazyInputStreamStorage.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

nsresult MaybeDeserializeAndWrapForMainThread(
    const Maybe<BodyStreamVariant>& aSource, int64_t aBodyStreamSize,
    Maybe<BodyStreamVariant>& aSink, PBackgroundParent* aManager) {
  if (aSource.isNothing()) {
    return NS_OK;
  }

  nsCOMPtr<nsIInputStream> deserialized =
      DeserializeIPCStream(aSource->get_ChildToParentStream().stream());

  aSink = Some(ParentToParentStream());
  auto& uuid = aSink->get_ParentToParentStream().uuid();

  MOZ_TRY(nsContentUtils::GenerateUUIDInPlace(uuid));

  auto storageOrErr = RemoteLazyInputStreamStorage::Get();

  if (NS_WARN_IF(storageOrErr.isErr())) {
    return storageOrErr.unwrapErr();
  }

  auto storage = storageOrErr.unwrap();
  storage->AddStream(deserialized, uuid, aBodyStreamSize, 0);
  return NS_OK;
}

}  // anonymous namespace

/* static */ void FetchEventOpProxyParent::Create(
    PRemoteWorkerParent* aManager,
    RefPtr<ServiceWorkerFetchEventOpPromise::Private>&& aPromise,
    const ServiceWorkerFetchEventOpArgs& aArgs,
    RefPtr<FetchEventOpParent> aReal, nsCOMPtr<nsIInputStream> aBodyStream) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aReal);

  FetchEventOpProxyParent* actor =
      new FetchEventOpProxyParent(std::move(aReal), std::move(aPromise));

  ServiceWorkerFetchEventOpArgs copyArgs = aArgs;
  IPCInternalRequest& copyRequest = copyArgs.internalRequest();

  if (aBodyStream) {
    PBackgroundParent* bgParent = aManager->Manager();
    MOZ_ASSERT(bgParent);

    copyRequest.body() = Some(ParentToChildStream());

    RemoteLazyStream ipdlStream;
    MOZ_ALWAYS_SUCCEEDS(RemoteLazyInputStreamUtils::SerializeInputStream(
        aBodyStream, copyRequest.bodySize(), ipdlStream, bgParent));

    copyRequest.body().ref().get_ParentToChildStream().actorParent() =
        ipdlStream;
  }

  Unused << aManager->SendPFetchEventOpProxyConstructor(actor, copyArgs);
}

FetchEventOpProxyParent::~FetchEventOpProxyParent() {
  AssertIsOnBackgroundThread();
}

FetchEventOpProxyParent::FetchEventOpProxyParent(
    RefPtr<FetchEventOpParent>&& aReal,
    RefPtr<ServiceWorkerFetchEventOpPromise::Private>&& aPromise)
    : mReal(std::move(aReal)), mLifetimePromise(std::move(aPromise)) {}

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

    MOZ_ALWAYS_SUCCEEDS(MaybeDeserializeAndWrapForMainThread(
        originalResponse.body(), copyResponse.bodySize(), copyResponse.body(),
        bgParent));
    MOZ_ALWAYS_SUCCEEDS(MaybeDeserializeAndWrapForMainThread(
        originalResponse.alternativeBody(), InternalResponse::UNKNOWN_BODY_SIZE,
        copyResponse.alternativeBody(), bgParent));

    Unused << mReal->SendRespondWith(copyArgs);
  } else {
    Unused << mReal->SendRespondWith(aResult);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult FetchEventOpProxyParent::Recv__delete__(
    const ServiceWorkerFetchEventOpResult& aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mLifetimePromise);
  MOZ_ASSERT(mReal);

  if (mLifetimePromise) {
    mLifetimePromise->Resolve(aResult, __func__);
    mLifetimePromise = nullptr;
    mReal = nullptr;
  }

  return IPC_OK();
}

void FetchEventOpProxyParent::ActorDestroy(ActorDestroyReason) {
  AssertIsOnBackgroundThread();
  if (mLifetimePromise) {
    mLifetimePromise->Reject(NS_ERROR_DOM_ABORT_ERR, __func__);
    mLifetimePromise = nullptr;
    mReal = nullptr;
  }
}

}  // namespace dom
}  // namespace mozilla
