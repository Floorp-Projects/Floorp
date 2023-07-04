/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NormalOriginOperationBase.h"

#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom::quota {

NS_IMPL_ISUPPORTS_INHERITED0(NormalOriginOperationBase, Runnable)

RefPtr<DirectoryLock> NormalOriginOperationBase::CreateDirectoryLock() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_DirectoryOpenPending);
  MOZ_ASSERT(QuotaManager::Get());

  return QuotaManager::Get()->CreateDirectoryLockInternal(
      mPersistenceType, mOriginScope, mClientType, mExclusive);
}

void NormalOriginOperationBase::Open() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_Initial);
  MOZ_ASSERT(QuotaManager::Get());

  AdvanceState();

  RefPtr<DirectoryLock> directoryLock = CreateDirectoryLock();
  if (directoryLock) {
    directoryLock->Acquire(this);
  } else {
    QM_TRY(MOZ_TO_RESULT(DirectoryOpen()), QM_VOID,
           [this](const nsresult rv) { Finish(rv); });
  }
}

void NormalOriginOperationBase::UnblockOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(QuotaManager::Get());
  MOZ_ASSERT(GetState() == State_UnblockingOpen);

  SendResults();

  if (mDirectoryLock) {
    mDirectoryLock = nullptr;
  }

  QuotaManager::Get()->UnregisterNormalOriginOp(*this);

  AdvanceState();
}

void NormalOriginOperationBase::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLock);
  MOZ_ASSERT(GetState() == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  QM_TRY(MOZ_TO_RESULT(DirectoryOpen()), QM_VOID,
         [this](const nsresult rv) { Finish(rv); });
}

void NormalOriginOperationBase::DirectoryLockFailed() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  Finish(NS_ERROR_FAILURE);
}

}  // namespace mozilla::dom::quota
