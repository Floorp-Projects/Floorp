/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchEventOpParent.h"

#include "mozilla/dom/FetchTypes.h"
#include "nsDebug.h"

#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/FetchEventOpProxyParent.h"
#include "mozilla/dom/FetchStreamUtils.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/RemoteWorkerControllerParent.h"
#include "mozilla/dom/RemoteWorkerParent.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

std::tuple<Maybe<ParentToParentInternalResponse>, Maybe<ResponseEndArgs>>
FetchEventOpParent::OnStart(
    MovingNotNull<RefPtr<FetchEventOpProxyParent>> aFetchEventOpProxyParent) {
  Maybe<ParentToParentInternalResponse> preloadResponse =
      std::move(mState.as<Pending>().mPreloadResponse);
  Maybe<ResponseEndArgs> preloadResponseEndArgs =
      std::move(mState.as<Pending>().mEndArgs);
  mState = AsVariant(Started{std::move(aFetchEventOpProxyParent)});
  return std::make_tuple(preloadResponse, preloadResponseEndArgs);
}

void FetchEventOpParent::OnFinish() {
  MOZ_ASSERT(mState.is<Started>());
  mState = AsVariant(Finished());
}

mozilla::ipc::IPCResult FetchEventOpParent::RecvPreloadResponse(
    ParentToParentInternalResponse&& aResponse) {
  AssertIsOnBackgroundThread();

  mState.match(
      [&aResponse](Pending& aPending) {
        MOZ_ASSERT(aPending.mPreloadResponse.isNothing());
        aPending.mPreloadResponse = Some(std::move(aResponse));
      },
      [&aResponse](Started& aStarted) {
        auto backgroundParent = WrapNotNull(
            WrapNotNull(aStarted.mFetchEventOpProxyParent->Manager())
                ->Manager());
        Unused << aStarted.mFetchEventOpProxyParent->SendPreloadResponse(
            ToParentToChild(aResponse, backgroundParent));
      },
      [](const Finished&) {});

  return IPC_OK();
}

mozilla::ipc::IPCResult FetchEventOpParent::RecvPreloadResponseTiming(
    ResponseTiming&& aTiming) {
  AssertIsOnBackgroundThread();

  mState.match(
      [&aTiming](Pending& aPending) {
        MOZ_ASSERT(aPending.mTiming.isNothing());
        aPending.mTiming = Some(std::move(aTiming));
      },
      [&aTiming](Started& aStarted) {
        Unused << aStarted.mFetchEventOpProxyParent->SendPreloadResponseTiming(
            std::move(aTiming));
      },
      [](const Finished&) {});

  return IPC_OK();
}

mozilla::ipc::IPCResult FetchEventOpParent::RecvPreloadResponseEnd(
    ResponseEndArgs&& aArgs) {
  AssertIsOnBackgroundThread();

  mState.match(
      [&aArgs](Pending& aPending) {
        MOZ_ASSERT(aPending.mEndArgs.isNothing());
        aPending.mEndArgs = Some(std::move(aArgs));
      },
      [&aArgs](Started& aStarted) {
        Unused << aStarted.mFetchEventOpProxyParent->SendPreloadResponseEnd(
            std::move(aArgs));
      },
      [](const Finished&) {});

  return IPC_OK();
}

void FetchEventOpParent::ActorDestroy(ActorDestroyReason) {
  AssertIsOnBackgroundThread();
}

}  // namespace dom
}  // namespace mozilla
