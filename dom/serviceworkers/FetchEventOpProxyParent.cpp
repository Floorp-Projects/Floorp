/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchEventOpProxyParent.h"

#include <utility>

#include "mozilla/dom/FetchTypes.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"
#include "nsCOMPtr.h"
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
    const Maybe<ChildToParentStream>& aSource, int64_t aBodyStreamSize,
    Maybe<ParentToParentStream>& aSink, PBackgroundParent* aManager) {
  if (aSource.isNothing()) {
    return NS_OK;
  }

  nsCOMPtr<nsIInputStream> deserialized =
      DeserializeIPCStream(aSource->stream());

  aSink = Some(ParentToParentStream());
  auto& uuid = aSink->uuid();

  MOZ_TRY(nsID::GenerateUUIDInPlace(uuid));

  auto storageOrErr = RemoteLazyInputStreamStorage::Get();

  if (NS_WARN_IF(storageOrErr.isErr())) {
    return storageOrErr.unwrapErr();
  }

  auto storage = storageOrErr.unwrap();
  storage->AddStream(deserialized, uuid, aBodyStreamSize, 0);
  return NS_OK;
}

ParentToParentInternalResponse ToParentToParent(
    const ChildToParentInternalResponse& aResponse,
    NotNull<PBackgroundParent*> aBackgroundParent) {
  ParentToParentInternalResponse parentToParentResponse(
      aResponse.metadata(), Nothing(), aResponse.bodySize(), Nothing());

  MOZ_ALWAYS_SUCCEEDS(MaybeDeserializeAndWrapForMainThread(
      aResponse.body(), aResponse.bodySize(), parentToParentResponse.body(),
      aBackgroundParent));
  MOZ_ALWAYS_SUCCEEDS(MaybeDeserializeAndWrapForMainThread(
      aResponse.alternativeBody(), InternalResponse::UNKNOWN_BODY_SIZE,
      parentToParentResponse.alternativeBody(), aBackgroundParent));

  return parentToParentResponse;
}

ParentToParentSynthesizeResponseArgs ToParentToParent(
    const ChildToParentSynthesizeResponseArgs& aArgs,
    NotNull<PBackgroundParent*> aBackgroundParent) {
  return ParentToParentSynthesizeResponseArgs(
      ToParentToParent(aArgs.internalResponse(), aBackgroundParent),
      aArgs.closure(), aArgs.timeStamps());
}

ParentToParentFetchEventRespondWithResult ToParentToParent(
    const ChildToParentFetchEventRespondWithResult& aResult,
    NotNull<PBackgroundParent*> aBackgroundParent) {
  switch (aResult.type()) {
    case ChildToParentFetchEventRespondWithResult::
        TChildToParentSynthesizeResponseArgs:
      return ToParentToParent(aResult.get_ChildToParentSynthesizeResponseArgs(),
                              aBackgroundParent);

    case ChildToParentFetchEventRespondWithResult::TResetInterceptionArgs:
      return aResult.get_ResetInterceptionArgs();

    case ChildToParentFetchEventRespondWithResult::TCancelInterceptionArgs:
      return aResult.get_CancelInterceptionArgs();

    default:
      MOZ_CRASH("Invalid ParentToParentFetchEventRespondWithResult");
  }
}

}  // anonymous namespace

/* static */ void FetchEventOpProxyParent::Create(
    PRemoteWorkerParent* aManager,
    RefPtr<ServiceWorkerFetchEventOpPromise::Private>&& aPromise,
    const ParentToParentServiceWorkerFetchEventOpArgs& aArgs,
    RefPtr<FetchEventOpParent> aReal, nsCOMPtr<nsIInputStream> aBodyStream) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aReal);

  ParentToChildServiceWorkerFetchEventOpArgs copyArgs(aArgs.common(),
                                                      Nothing());
  if (aArgs.preloadResponse().isSome()) {
    // Convert the preload response to ParentToChildResponseWithTiming.
    ParentToChildResponseWithTiming response;
    response.response() =
        ToParentToChild(aArgs.preloadResponse().ref().response(),
                        WrapNotNull(aManager->Manager()));
    response.timingData() = aArgs.preloadResponse().ref().timingData();
    response.initiatorType() = aArgs.preloadResponse().ref().initiatorType();
    response.entryName() = aArgs.preloadResponse().ref().entryName();
    copyArgs.preloadResponse() = Some(response);
  }

  FetchEventOpProxyParent* actor =
      new FetchEventOpProxyParent(std::move(aReal), std::move(aPromise));

  // As long as the fetch event was pending, the FetchEventOpParent was
  // responsible for keeping the preload response, if it already arrived. Once
  // the fetch event starts it gives up the preload response (if any) and we
  // need to add it to the arguments. Note that we have to make sure that the
  // arguments don't contain the preload response already, otherwise we'll end
  // up overwriting it with a Nothing.
  Maybe<ParentToParentResponseWithTiming> preloadResponse =
      actor->mReal->OnStart(WrapNotNull(actor));
  if (copyArgs.preloadResponse().isNothing() && preloadResponse.isSome()) {
    ParentToChildResponseWithTiming response;
    response.response() = ToParentToChild(preloadResponse.ref().response(),
                                          WrapNotNull(aManager->Manager()));
    response.timingData() = preloadResponse.ref().timingData();
    response.initiatorType() = preloadResponse.ref().initiatorType();
    response.entryName() = preloadResponse.ref().entryName();
    copyArgs.preloadResponse() = Some(response);
  }

  IPCInternalRequest& copyRequest = copyArgs.common().internalRequest();

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
    const ChildToParentFetchEventRespondWithResult& aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mReal);

  auto manager = WrapNotNull(mReal->Manager());
  auto backgroundParent = WrapNotNull(manager->Manager());
  Unused << mReal->SendRespondWith(ToParentToParent(aResult, backgroundParent));
  mReal->OnFinish();
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
