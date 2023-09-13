/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NormalOriginOperationBase.h"

#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/QuotaManager.h"

namespace mozilla::dom::quota {

NormalOriginOperationBase::NormalOriginOperationBase(
    const char* aName, const Nullable<PersistenceType>& aPersistenceType,
    const OriginScope& aOriginScope, const Nullable<Client::Type>& aClientType,
    bool aExclusive)
    : OriginOperationBase(aName),
      mOriginScope(aOriginScope),
      mPersistenceType(aPersistenceType),
      mClientType(aClientType),
      mExclusive(aExclusive) {
  AssertIsOnOwningThread();
}

NormalOriginOperationBase::~NormalOriginOperationBase() {
  AssertIsOnOwningThread();
}

RefPtr<DirectoryLock> NormalOriginOperationBase::CreateDirectoryLock() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(QuotaManager::Get());

  return QuotaManager::Get()->CreateDirectoryLockInternal(
      mPersistenceType, mOriginScope, mClientType, mExclusive);
}

RefPtr<BoolPromise> NormalOriginOperationBase::Open() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(QuotaManager::Get());

  RefPtr<DirectoryLock> directoryLock = CreateDirectoryLock();

  if (directoryLock) {
    return directoryLock->Acquire()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [self = RefPtr(this), directoryLock = directoryLock](
            const BoolPromise::ResolveOrRejectValue& aValue) mutable {
          if (aValue.IsReject()) {
            return BoolPromise::CreateAndReject(aValue.RejectValue(), __func__);
          }

          self->mDirectoryLock = std::move(directoryLock);

          return BoolPromise::CreateAndResolve(true, __func__);
        });
  }

  return BoolPromise::CreateAndResolve(true, __func__);
}

void NormalOriginOperationBase::UnblockOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(QuotaManager::Get());

  SendResults();

  if (mDirectoryLock) {
    mDirectoryLock = nullptr;
  }

  QuotaManager::Get()->UnregisterNormalOriginOp(*this);
}

}  // namespace mozilla::dom::quota
