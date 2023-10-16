/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "json/json.h"
#include "mozilla/dom/U2FTokenManager.h"
#include "mozilla/dom/U2FTokenTransport.h"
#include "mozilla/dom/PWebAuthnTransactionParent.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsEscape.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIThread.h"
#include "nsTextFormatter.h"
#include "mozilla/Telemetry.h"
#include "WebAuthnEnumStrings.h"

#include "mozilla/dom/AndroidWebAuthnTokenManager.h"

namespace mozilla::dom {

/***********************************************************************
 * Statics
 **********************************************************************/

namespace {
static mozilla::LazyLogModule gU2FTokenManagerLog("u2fkeymanager");
StaticAutoPtr<U2FTokenManager> gU2FTokenManager;
static nsIThread* gBackgroundThread;
}  // namespace

/***********************************************************************
 * U2FManager Implementation
 **********************************************************************/

U2FTokenManager::U2FTokenManager()
    : mTransactionParent(nullptr), mLastTransactionId(0) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Create on the main thread to make sure ClearOnShutdown() works.
  MOZ_ASSERT(NS_IsMainThread());
}

// static
void U2FTokenManager::Initialize() {
  if (!XRE_IsParentProcess()) {
    return;
  }
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gU2FTokenManager);
  gU2FTokenManager = new U2FTokenManager();
  ClearOnShutdown(&gU2FTokenManager);
}

// static
U2FTokenManager* U2FTokenManager::Get() {
  MOZ_ASSERT(XRE_IsParentProcess());
  // We should only be accessing this on the background thread
  MOZ_ASSERT(!NS_IsMainThread());
  return gU2FTokenManager;
}

void U2FTokenManager::AbortTransaction(const uint64_t& aTransactionId,
                                       const nsresult& aError) {
  Unused << mTransactionParent->SendAbort(aTransactionId, aError);
  ClearTransaction();
}

void U2FTokenManager::AbortOngoingTransaction() {
  if (mLastTransactionId > 0 && mTransactionParent) {
    // Send an abort to any other ongoing transaction
    Unused << mTransactionParent->SendAbort(mLastTransactionId,
                                            NS_ERROR_DOM_ABORT_ERR);
  }
  ClearTransaction();
}

void U2FTokenManager::MaybeClearTransaction(
    PWebAuthnTransactionParent* aParent) {
  // Only clear if we've been requested to do so by our current transaction
  // parent.
  if (mTransactionParent == aParent) {
    ClearTransaction();
  }
}

void U2FTokenManager::ClearTransaction() {
  mTransactionParent = nullptr;

  // Drop managers at the end of all transactions
  if (mTokenManagerImpl) {
    mTokenManagerImpl->Drop();
    mTokenManagerImpl = nullptr;
  }

  // Forget promises, if necessary.
  mRegisterPromise.DisconnectIfExists();
  mSignPromise.DisconnectIfExists();

  // Clear transaction id.
  mLastTransactionId = 0;
}

RefPtr<U2FTokenTransport> U2FTokenManager::GetTokenManagerImpl() {
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (mTokenManagerImpl) {
    return mTokenManagerImpl;
  }

  if (!gBackgroundThread) {
    gBackgroundThread = NS_GetCurrentThread();
    MOZ_ASSERT(gBackgroundThread, "This should never be null!");
  }

  return AndroidWebAuthnTokenManager::GetInstance();
}

void U2FTokenManager::Register(
    PWebAuthnTransactionParent* aTransactionParent,
    const uint64_t& aTransactionId,
    const WebAuthnMakeCredentialInfo& aTransactionInfo) {
  MOZ_LOG(gU2FTokenManagerLog, LogLevel::Debug, ("U2FAuthRegister"));

  AbortOngoingTransaction();
  mTransactionParent = aTransactionParent;
  mTokenManagerImpl = GetTokenManagerImpl();

  if (!mTokenManagerImpl) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  mLastTransactionId = aTransactionId;

  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(mLastTransactionId > 0);

  uint64_t tid = mLastTransactionId;

  mTokenManagerImpl
      ->Register(aTransactionInfo, /* aForceNoneAttestation */ true)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [tid](WebAuthnMakeCredentialResult&& aResult) {
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"U2FRegisterFinish"_ns, 1);
            U2FTokenManager* mgr = U2FTokenManager::Get();
            mgr->MaybeConfirmRegister(tid, aResult);
          },
          [tid](nsresult rv) {
            MOZ_ASSERT(NS_FAILED(rv));
            U2FTokenManager* mgr = U2FTokenManager::Get();
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"U2FRegisterAbort"_ns, 1);
            mgr->MaybeAbortRegister(tid, rv);
          })
      ->Track(mRegisterPromise);
}

void U2FTokenManager::MaybeConfirmRegister(
    const uint64_t& aTransactionId,
    const WebAuthnMakeCredentialResult& aResult) {
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mRegisterPromise.Complete();

  Unused << mTransactionParent->SendConfirmRegister(aTransactionId, aResult);
  ClearTransaction();
}

void U2FTokenManager::MaybeAbortRegister(const uint64_t& aTransactionId,
                                         const nsresult& aError) {
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mRegisterPromise.Complete();
  AbortTransaction(aTransactionId, aError);
}

void U2FTokenManager::Sign(PWebAuthnTransactionParent* aTransactionParent,
                           const uint64_t& aTransactionId,
                           const WebAuthnGetAssertionInfo& aTransactionInfo) {
  MOZ_LOG(gU2FTokenManagerLog, LogLevel::Debug, ("U2FAuthSign"));

  AbortOngoingTransaction();
  mTransactionParent = aTransactionParent;
  mTokenManagerImpl = GetTokenManagerImpl();

  if (!mTokenManagerImpl) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  mLastTransactionId = aTransactionId;

  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(mLastTransactionId > 0);
  uint64_t tid = mLastTransactionId;

  mTokenManagerImpl->Sign(aTransactionInfo)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [tid](nsTArray<WebAuthnGetAssertionResultWrapper>&& aResult) {
            U2FTokenManager* mgr = U2FTokenManager::Get();
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"U2FSignFinish"_ns, 1);
            if (aResult.Length() == 1) {
              WebAuthnGetAssertionResult result = aResult[0].assertion;
              mgr->MaybeConfirmSign(tid, result);
            }
          },
          [tid](nsresult rv) {
            MOZ_ASSERT(NS_FAILED(rv));
            U2FTokenManager* mgr = U2FTokenManager::Get();
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"U2FSignAbort"_ns, 1);
            mgr->MaybeAbortSign(tid, rv);
          })
      ->Track(mSignPromise);
}

void U2FTokenManager::MaybeConfirmSign(
    const uint64_t& aTransactionId, const WebAuthnGetAssertionResult& aResult) {
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mSignPromise.Complete();

  Unused << mTransactionParent->SendConfirmSign(aTransactionId, aResult);
  ClearTransaction();
}

void U2FTokenManager::MaybeAbortSign(const uint64_t& aTransactionId,
                                     const nsresult& aError) {
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mSignPromise.Complete();
  AbortTransaction(aTransactionId, aError);
}

void U2FTokenManager::Cancel(PWebAuthnTransactionParent* aParent,
                             const Tainted<uint64_t>& aTransactionId) {
  // The last transaction ID also suffers from the issue described in Bug
  // 1696159. A content process could cancel another content processes
  // transaction by guessing the last transaction ID.
  if (mTransactionParent != aParent ||
      !MOZ_IS_VALID(aTransactionId, mLastTransactionId == aTransactionId)) {
    return;
  }

  mTokenManagerImpl->Cancel();
  ClearTransaction();
}

}  // namespace mozilla::dom
