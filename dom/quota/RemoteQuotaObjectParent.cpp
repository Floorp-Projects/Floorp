/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteQuotaObjectParent.h"

#include "CanonicalQuotaObject.h"
#include "mozilla/dom/quota/RemoteQuotaObjectParentTracker.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla::dom::quota {

RemoteQuotaObjectParent::RemoteQuotaObjectParent(
    RefPtr<CanonicalQuotaObject> aCanonicalQuotaObject,
    nsCOMPtr<RemoteQuotaObjectParentTracker> aTracker)
    : mCanonicalQuotaObject(std::move(aCanonicalQuotaObject)),
      mTracker(std::move(aTracker)) {}

RemoteQuotaObjectParent::~RemoteQuotaObjectParent() { MOZ_ASSERT(!CanSend()); }

mozilla::ipc::IPCResult RemoteQuotaObjectParent::RecvMaybeUpdateSize(
    int64_t aSize, bool aTruncate, bool* aResult) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mozilla::ipc::IsOnBackgroundThread());
  MOZ_ASSERT(!GetCurrentThreadWorkerPrivate());

  *aResult = mCanonicalQuotaObject->MaybeUpdateSize(aSize, aTruncate);
  return IPC_OK();
}

void RemoteQuotaObjectParent::ActorDestroy(ActorDestroyReason aWhy) {
  QM_WARNONLY_TRY(MOZ_TO_RESULT(CheckFileAfterClose()));

  mCanonicalQuotaObject = nullptr;

  if (mTracker) {
    mTracker->UnregisterRemoteQuotaObjectParent(WrapNotNullUnchecked(this));
  }
}

nsresult RemoteQuotaObjectParent::CheckFileAfterClose() {
  MOZ_ASSERT(mCanonicalQuotaObject);

  QM_TRY_INSPECT(const auto& file,
                 QM_NewLocalFile(mCanonicalQuotaObject->Path()));

  QM_TRY_UNWRAP(auto size, MOZ_TO_RESULT_INVOKE_MEMBER(file, GetFileSize));

  DebugOnly<bool> res =
      mCanonicalQuotaObject->MaybeUpdateSize(size, /* aTruncate */ true);
  MOZ_ASSERT(res);

  return NS_OK;
}

}  // namespace mozilla::dom::quota
