/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsChild.h"

// Local includes
#include "SDBConnection.h"
#include "SDBRequest.h"
#include "SDBResults.h"

// Global includes
#include "mozilla/Assertions.h"
#include "mozilla/dom/PBackgroundSDBRequest.h"
#include "nsError.h"
#include "nsID.h"
#include "nsISDBResults.h"
#include "nsVariant.h"

namespace mozilla::dom {

/*******************************************************************************
 * SDBConnectionChild
 ******************************************************************************/

SDBConnectionChild::SDBConnectionChild(SDBConnection* aConnection)
    : mConnection(aConnection) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aConnection);

  MOZ_COUNT_CTOR(SDBConnectionChild);
}

SDBConnectionChild::~SDBConnectionChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(SDBConnectionChild);
}

void SDBConnectionChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();

  if (mConnection) {
    mConnection->ClearBackgroundActor();
    mConnection = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundSDBConnectionChild::SendDeleteMe());
  }
}

void SDBConnectionChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mConnection) {
    mConnection->ClearBackgroundActor();
#ifdef DEBUG
    mConnection = nullptr;
#endif
  }
}

PBackgroundSDBRequestChild* SDBConnectionChild::AllocPBackgroundSDBRequestChild(
    const SDBRequestParams& aParams) {
  AssertIsOnOwningThread();

  MOZ_CRASH(
      "PBackgroundSDBRequestChild actors should be manually "
      "constructed!");
}

bool SDBConnectionChild::DeallocPBackgroundSDBRequestChild(
    PBackgroundSDBRequestChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);

  delete static_cast<SDBRequestChild*>(aActor);
  return true;
}

mozilla::ipc::IPCResult SDBConnectionChild::RecvAllowToClose() {
  AssertIsOnOwningThread();

  if (mConnection) {
    mConnection->AllowToClose();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult SDBConnectionChild::RecvClosed() {
  AssertIsOnOwningThread();

  if (mConnection) {
    mConnection->OnClose(/* aAbnormal */ true);
  }

  return IPC_OK();
}

/*******************************************************************************
 * SDBRequestChild
 ******************************************************************************/

SDBRequestChild::SDBRequestChild(SDBRequest* aRequest)
    : mConnection(aRequest->GetConnection()), mRequest(aRequest) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aRequest);

  MOZ_COUNT_CTOR(SDBRequestChild);
}

SDBRequestChild::~SDBRequestChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(SDBRequestChild);
}

#ifdef DEBUG

void SDBRequestChild::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mRequest);
  mRequest->AssertIsOnOwningThread();
}

#endif  // DEBUG

void SDBRequestChild::HandleResponse(nsresult aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(mRequest);

  mRequest->SetError(aResponse);
}

void SDBRequestChild::HandleResponse() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  RefPtr<nsVariant> variant = new nsVariant();
  variant->SetAsVoid();

  mRequest->SetResult(variant);
}

void SDBRequestChild::HandleResponse(const nsCString& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  RefPtr<SDBResult> result = new SDBResult(aResponse);

  RefPtr<nsVariant> variant = new nsVariant();
  variant->SetAsInterface(NS_GET_IID(nsISDBResult), result);

  mRequest->SetResult(variant);
}

void SDBRequestChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mConnection) {
    mConnection->AssertIsOnOwningThread();

    mConnection->OnRequestFinished();
#ifdef DEBUG
    mConnection = nullptr;
#endif
  }
}

mozilla::ipc::IPCResult SDBRequestChild::Recv__delete__(
    const SDBRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mConnection);

  switch (aResponse.type()) {
    case SDBRequestResponse::Tnsresult:
      HandleResponse(aResponse.get_nsresult());
      break;

    case SDBRequestResponse::TSDBRequestOpenResponse:
      HandleResponse();

      mConnection->OnOpen();

      break;

    case SDBRequestResponse::TSDBRequestSeekResponse:
      HandleResponse();
      break;

    case SDBRequestResponse::TSDBRequestReadResponse:
      HandleResponse(aResponse.get_SDBRequestReadResponse().data());
      break;

    case SDBRequestResponse::TSDBRequestWriteResponse:
      HandleResponse();
      break;

    case SDBRequestResponse::TSDBRequestCloseResponse:
      HandleResponse();

      mConnection->OnClose(/* aAbnormal */ false);

      break;

    default:
      MOZ_CRASH("Unknown response type!");
  }

  mConnection->OnRequestFinished();

  // Null this out so that we don't try to call OnRequestFinished() again in
  // ActorDestroy.
  mConnection = nullptr;

  return IPC_OK();
}

}  // namespace mozilla::dom
