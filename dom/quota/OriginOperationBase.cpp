/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginOperationBase.h"

#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsError.h"
#include "nsIThread.h"

namespace mozilla::dom::quota {

void OriginOperationBase::Dispatch() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(GetState() == State_Initial);

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
}

NS_IMETHODIMP
OriginOperationBase::Run() {
  nsresult rv;

  switch (mState) {
    case State_Initial: {
      rv = Init();
      break;
    }

    case State_DirectoryOpenPending: {
      rv = DirectoryOpen();
      break;
    }

    case State_DirectoryWorkOpen: {
      rv = DirectoryWork();
      break;
    }

    case State_UnblockingOpen: {
      UnblockOpen();
      return NS_OK;
    }

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State_UnblockingOpen) {
    Finish(rv);
  }

  return NS_OK;
}

nsresult OriginOperationBase::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  return NS_OK;
}

nsresult OriginOperationBase::DirectoryOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_DirectoryOpenPending);

  QuotaManager* const quotaManager = QuotaManager::Get();
  QM_TRY(OkIf(quotaManager), NS_ERROR_FAILURE);

  // Must set this before dispatching otherwise we will race with the IO thread.
  AdvanceState();

  QM_TRY(MOZ_TO_RESULT(
             quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL)),
         NS_ERROR_FAILURE);

  return NS_OK;
}

void OriginOperationBase::Finish(nsresult aResult) {
  if (NS_SUCCEEDED(mResultCode)) {
    mResultCode = aResult;
  }

  // Must set mState before dispatching otherwise we will race with the main
  // thread.
  mState = State_UnblockingOpen;

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
}

nsresult OriginOperationBase::Init() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_Initial);

  if (QuotaManager::IsShuttingDown()) {
    return NS_ERROR_ABORT;
  }

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  QM_TRY(MOZ_TO_RESULT(DoInit(*quotaManager)));

  Open();

  return NS_OK;
}

nsresult OriginOperationBase::DirectoryWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State_DirectoryWorkOpen);

  QuotaManager* const quotaManager = QuotaManager::Get();
  QM_TRY(OkIf(quotaManager), NS_ERROR_FAILURE);

  QM_TRY(MOZ_TO_RESULT(DoDirectoryWork(*quotaManager)));

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  AdvanceState();

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

}  // namespace mozilla::dom::quota
