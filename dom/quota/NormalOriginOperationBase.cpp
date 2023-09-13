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
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager, const char* aName)
    : OriginOperationBase(std::move(aQuotaManager), aName) {
  AssertIsOnOwningThread();
}

NormalOriginOperationBase::~NormalOriginOperationBase() {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> NormalOriginOperationBase::Open() {
  AssertIsOnOwningThread();

  mDirectoryLock = CreateDirectoryLock();

  if (mDirectoryLock) {
    return mDirectoryLock->Acquire();
  }

  return BoolPromise::CreateAndResolve(true, __func__);
}

void NormalOriginOperationBase::UnblockOpen() {
  AssertIsOnOwningThread();

  SendResults();

  if (mDirectoryLock) {
    mDirectoryLock = nullptr;
  }

  mQuotaManager->UnregisterNormalOriginOp(*this);
}

}  // namespace mozilla::dom::quota
