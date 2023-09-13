/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginOperationBase.h"

#include "mozilla/Assertions.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/fs/TargetPtrHolder.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsError.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::quota {

OriginOperationBase::OriginOperationBase(
    MovingNotNull<RefPtr<QuotaManager>>&& aQuotaManager, const char* aName)
    : BackgroundThreadObject(GetCurrentSerialEventTarget()),
      mQuotaManager(std::move(aQuotaManager)),
      mResultCode(NS_OK),
      mActorDestroyed(false)
#ifdef QM_COLLECTING_OPERATION_TELEMETRY
      ,
      mName(aName)
#endif
{
  AssertIsOnOwningThread();
}

OriginOperationBase::~OriginOperationBase() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActorDestroyed);
}

void OriginOperationBase::RunImmediately() {
  AssertIsOnOwningThread();

  [self = RefPtr(this)]() {
    if (QuotaManager::IsShuttingDown()) {
      return BoolPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
    }

    QM_TRY(MOZ_TO_RESULT(self->DoInit(*self->mQuotaManager)),
           CreateAndRejectBoolPromise);

    return self->Open();
  }()
#ifdef DEBUG
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr(this)](
                 const BoolPromise::ResolveOrRejectValue& aValue) {
               if (aValue.IsReject()) {
                 return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                     __func__);
               }

               // Give derived classes the occasion to add additional debug
               // only checks after the opening was successfully finished on
               // the current thread before passing the work to the IO thread.
               QM_TRY(MOZ_TO_RESULT(self->DirectoryOpen()),
                      CreateAndRejectBoolPromise);

               return BoolPromise::CreateAndResolve(true, __func__);
             })
#endif
      ->Then(mQuotaManager->IOThread(), __func__,
             [selfHolder = fs::TargetPtrHolder(this)](
                 const BoolPromise::ResolveOrRejectValue& aValue) {
               if (aValue.IsReject()) {
                 return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                     __func__);
               }

               QM_TRY(MOZ_TO_RESULT(selfHolder->DoDirectoryWork(
                          *selfHolder->mQuotaManager)),
                      CreateAndRejectBoolPromise);

               return BoolPromise::CreateAndResolve(true, __func__);
             })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr(this)](
                 const BoolPromise::ResolveOrRejectValue& aValue) {
               if (aValue.IsReject()) {
                 MOZ_ASSERT(NS_SUCCEEDED(self->mResultCode));

                 self->mResultCode = aValue.RejectValue();
               }

               self->UnblockOpen();
             });
}

nsresult OriginOperationBase::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  return NS_OK;
}

#ifdef DEBUG
nsresult OriginOperationBase::DirectoryOpen() {
  AssertIsOnOwningThread();

  return NS_OK;
}
#endif

}  // namespace mozilla::dom::quota
