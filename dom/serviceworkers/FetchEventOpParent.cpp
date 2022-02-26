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

Maybe<ParentToParentResponseWithTiming> FetchEventOpParent::OnStart(
    MovingNotNull<RefPtr<FetchEventOpProxyParent>> aFetchEventOpProxyParent) {
  Maybe<ParentToParentResponseWithTiming> preloadResponse =
      std::move(mState.as<Pending>().mPreloadResponse);
  mState = AsVariant(Started{std::move(aFetchEventOpProxyParent)});
  return preloadResponse;
}

void FetchEventOpParent::OnFinish() {
  MOZ_ASSERT(mState.is<Started>());
  mState = AsVariant(Finished());
}

mozilla::ipc::IPCResult FetchEventOpParent::RecvPreloadResponse(
    ParentToParentResponseWithTiming&& aResponse) {
  AssertIsOnBackgroundThread();

  mState.match(
      [&aResponse](Pending& aPending) {
        MOZ_ASSERT(aPending.mPreloadResponse.isNothing());
        aPending.mPreloadResponse = Some(std::move(aResponse));
      },
      [&aResponse](Started& aStarted) {
        ParentToChildResponseWithTiming response;
        auto backgroundParent = WrapNotNull(
            WrapNotNull(aStarted.mFetchEventOpProxyParent->Manager())
                ->Manager());
        response.response() =
            ToParentToChild(aResponse.response(), backgroundParent);
        response.timingData() = aResponse.timingData();
        response.initiatorType() = aResponse.initiatorType();
        response.entryName() = aResponse.entryName();
        Unused << aStarted.mFetchEventOpProxyParent->SendPreloadResponse(
            response);
      },
      [](const Finished&) {});

  return IPC_OK();
}

void FetchEventOpParent::ActorDestroy(ActorDestroyReason) {
  AssertIsOnBackgroundThread();
}

}  // namespace dom
}  // namespace mozilla
