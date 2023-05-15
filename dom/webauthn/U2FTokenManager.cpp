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
#include "mozilla/dom/WebAuthnUtil.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsEscape.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIThread.h"
#include "nsTextFormatter.h"
#include "mozilla/Telemetry.h"
#include "WebAuthnEnumStrings.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/dom/AndroidWebAuthnTokenManager.h"
#endif

#define PREF_WEBAUTHN_USBTOKEN_ENABLED \
  "security.webauth.webauthn_enable_usbtoken"
#define PREF_WEBAUTHN_ALLOW_DIRECT_ATTESTATION \
  "security.webauth.webauthn_testing_allow_direct_attestation"
#define PREF_WEBAUTHN_ANDROID_FIDO2_ENABLED \
  "security.webauth.webauthn_enable_android_fido2"
namespace mozilla::dom {

/***********************************************************************
 * Statics
 **********************************************************************/

class U2FPrefManager;

namespace {
static mozilla::LazyLogModule gU2FTokenManagerLog("u2fkeymanager");
StaticRefPtr<U2FTokenManager> gU2FTokenManager;
StaticRefPtr<U2FPrefManager> gPrefManager;
static nsIThread* gBackgroundThread;
}  // namespace

// Data for WebAuthn UI prompt notifications.
static const char16_t kPresencePromptNotificationU2F[] =
    u"{\"is_ctap2\":false,\"action\":\"presence\",\"tid\":%llu,"
    u"\"origin\":\"%s\",\"browsingContextId\":%llu}";
static const char16_t kRegisterDirectPromptNotificationU2F[] =
    u"{\"is_ctap2\":false,\"action\":\"register-direct\",\"tid\":%llu,"
    u"\"origin\":\"%s\",\"browsingContextId\":%llu}";
static const char16_t kCancelPromptNotificationU2F[] =
    u"{\"is_ctap2\":false,\"action\":\"cancel\",\"tid\":%llu}";

class U2FPrefManager final : public nsIObserver {
 private:
  U2FPrefManager() : mPrefMutex("U2FPrefManager Mutex") { UpdateValues(); }
  ~U2FPrefManager() = default;

 public:
  NS_DECL_ISUPPORTS

  static U2FPrefManager* GetOrCreate() {
    MOZ_ASSERT(NS_IsMainThread());
    if (!gPrefManager) {
      gPrefManager = new U2FPrefManager();
      Preferences::AddStrongObserver(gPrefManager,
                                     PREF_WEBAUTHN_USBTOKEN_ENABLED);
      Preferences::AddStrongObserver(gPrefManager,
                                     PREF_WEBAUTHN_ANDROID_FIDO2_ENABLED);
      Preferences::AddStrongObserver(gPrefManager,
                                     PREF_WEBAUTHN_ALLOW_DIRECT_ATTESTATION);
      ClearOnShutdown(&gPrefManager, ShutdownPhase::XPCOMShutdownThreads);
    }
    return gPrefManager;
  }

  static U2FPrefManager* Get() { return gPrefManager; }

  bool GetAndroidFido2Enabled() {
    MutexAutoLock lock(mPrefMutex);
    return mAndroidFido2Enabled;
  }

  bool GetAllowDirectAttestationForTesting() {
    MutexAutoLock lock(mPrefMutex);
    return mAllowDirectAttestation;
  }

  NS_IMETHODIMP
  Observe(nsISupports* aSubject, const char* aTopic,
          const char16_t* aData) override {
    UpdateValues();
    return NS_OK;
  }

 private:
  void UpdateValues() {
    MOZ_ASSERT(NS_IsMainThread());
    MutexAutoLock lock(mPrefMutex);
    mUsbTokenEnabled = Preferences::GetBool(PREF_WEBAUTHN_USBTOKEN_ENABLED);
    mAndroidFido2Enabled =
        Preferences::GetBool(PREF_WEBAUTHN_ANDROID_FIDO2_ENABLED);
    mAllowDirectAttestation =
        Preferences::GetBool(PREF_WEBAUTHN_ALLOW_DIRECT_ATTESTATION);
  }

  Mutex mPrefMutex MOZ_UNANNOTATED;
  bool mUsbTokenEnabled;
  bool mAndroidFido2Enabled;
  bool mAllowDirectAttestation;
};

NS_IMPL_ISUPPORTS(U2FPrefManager, nsIObserver);

/***********************************************************************
 * U2FManager Implementation
 **********************************************************************/

NS_IMPL_ISUPPORTS(U2FTokenManager, nsIU2FTokenManager);

U2FTokenManager::U2FTokenManager()
    : mTransactionParent(nullptr), mLastTransactionId(0) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Create on the main thread to make sure ClearOnShutdown() works.
  MOZ_ASSERT(NS_IsMainThread());
  // Create the preference manager while we're initializing.
  U2FPrefManager::GetOrCreate();
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
                                       const nsresult& aError,
                                       bool shouldCancelActiveDialog) {
  Unused << mTransactionParent->SendAbort(aTransactionId, aError);
  ClearTransaction(shouldCancelActiveDialog);
}

void U2FTokenManager::AbortOngoingTransaction() {
  if (mLastTransactionId > 0 && mTransactionParent) {
    // Send an abort to any other ongoing transaction
    Unused << mTransactionParent->SendAbort(mLastTransactionId,
                                            NS_ERROR_DOM_ABORT_ERR);
  }
  ClearTransaction(true);
}

void U2FTokenManager::MaybeClearTransaction(
    PWebAuthnTransactionParent* aParent) {
  // Only clear if we've been requested to do so by our current transaction
  // parent.
  if (mTransactionParent == aParent) {
    ClearTransaction(true);
  }
}

void U2FTokenManager::ClearTransaction(bool send_cancel) {
  if (mLastTransactionId && send_cancel) {
    // Remove any prompts we might be showing for the current transaction.
    SendPromptNotification(kCancelPromptNotificationU2F, mLastTransactionId);
  }
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

  // Forget any pending registration.
  mPendingRegisterInfo.reset();
  mPendingSignInfo.reset();
  mPendingSignResults.Clear();
}

template <typename... T>
void U2FTokenManager::SendPromptNotification(const char16_t* aFormat,
                                             T... aArgs) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  nsAutoString json;
  nsTextFormatter::ssprintf(json, aFormat, aArgs...);

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<nsString>(
      "U2FTokenManager::RunSendPromptNotification", this,
      &U2FTokenManager::RunSendPromptNotification, json));

  MOZ_ALWAYS_SUCCEEDS(GetMainThreadSerialEventTarget()->Dispatch(
      r.forget(), NS_DISPATCH_NORMAL));
}

void U2FTokenManager::RunSendPromptNotification(const nsString& aJSON) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (NS_WARN_IF(!os)) {
    return;
  }

  nsCOMPtr<nsIU2FTokenManager> self = this;
  MOZ_ALWAYS_SUCCEEDS(
      os->NotifyObservers(self, "webauthn-prompt", aJSON.get()));
}

RefPtr<U2FTokenTransport> U2FTokenManager::GetTokenManagerImpl() {
  MOZ_ASSERT(U2FPrefManager::Get());
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (mTokenManagerImpl) {
    return mTokenManagerImpl;
  }

  if (!gBackgroundThread) {
    gBackgroundThread = NS_GetCurrentThread();
    MOZ_ASSERT(gBackgroundThread, "This should never be null!");
  }

#ifdef MOZ_WIDGET_ANDROID
  // On Android, prefer the platform support if enabled.
  if (U2FPrefManager::Get()->GetAndroidFido2Enabled()) {
    return AndroidWebAuthnTokenManager::GetInstance();
  }
#endif

  return nullptr;
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
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  mLastTransactionId = aTransactionId;

  // Determine whether direct attestation was requested.
  bool noneAttestationRequested = true;

// On Android, let's always reject direct attestations until we have a
// mechanism to solicit user consent, from Bug 1550164
#ifndef MOZ_WIDGET_ANDROID
  // The default attestation type is "none", so set
  // noneAttestationRequested=false only if the RP's preference matches one of
  // the other known types. This needs to be reviewed if values are added to
  // the AttestationConveyancePreference enum.
  const nsString& attestation =
      aTransactionInfo.attestationConveyancePreference();
  static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
  if (attestation.EqualsLiteral(
          MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT) ||
      attestation.EqualsLiteral(
          MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_INDIRECT) ||
      attestation.EqualsLiteral(
          MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ENTERPRISE)) {
    noneAttestationRequested = false;
  }
#endif  // not MOZ_WIDGET_ANDROID

  // Start a register request immediately if direct attestation
  // wasn't requested or the test pref is set.
  if (noneAttestationRequested ||
      U2FPrefManager::Get()->GetAllowDirectAttestationForTesting()) {
    MOZ_ASSERT(mPendingRegisterInfo.isNothing());
    mPendingRegisterInfo = Some(aTransactionInfo);
    DoRegister(aTransactionInfo, noneAttestationRequested);
    return;
  }

  // If the RP request direct attestation, ask the user for permission and
  // store the transaction info until the user proceeds or cancels.
  NS_ConvertUTF16toUTF8 origin(aTransactionInfo.Origin());
  SendPromptNotification(kRegisterDirectPromptNotificationU2F, aTransactionId,
                         origin.get(), aTransactionInfo.BrowsingContextId());

  MOZ_ASSERT(mPendingRegisterInfo.isNothing());
  mPendingRegisterInfo = Some(aTransactionInfo);
}

void U2FTokenManager::DoRegister(const WebAuthnMakeCredentialInfo& aInfo,
                                 bool aForceNoneAttestation) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(mLastTransactionId > 0);

  // Show a prompt that lets the user cancel the ongoing transaction.
  NS_ConvertUTF16toUTF8 origin(aInfo.Origin());
  SendPromptNotification(kPresencePromptNotificationU2F, mLastTransactionId,
                         origin.get(), aInfo.BrowsingContextId(), "false");

  uint64_t tid = mLastTransactionId;

  mTokenManagerImpl->Register(aInfo, aForceNoneAttestation)
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
            bool shouldCancelActiveDialog = true;
            if (rv == NS_ERROR_DOM_OPERATION_ERR) {
              // PIN-related errors. Let the dialog show to inform the user
              shouldCancelActiveDialog = false;
            }
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"U2FRegisterAbort"_ns, 1);
            mgr->MaybeAbortRegister(tid, rv, shouldCancelActiveDialog);
          })
      ->Track(mRegisterPromise);
}

void U2FTokenManager::MaybeConfirmRegister(
    const uint64_t& aTransactionId,
    const WebAuthnMakeCredentialResult& aResult) {
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mRegisterPromise.Complete();

  Unused << mTransactionParent->SendConfirmRegister(aTransactionId, aResult);
  ClearTransaction(true);
}

void U2FTokenManager::MaybeAbortRegister(const uint64_t& aTransactionId,
                                         const nsresult& aError,
                                         bool shouldCancelActiveDialog) {
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mRegisterPromise.Complete();
  AbortTransaction(aTransactionId, aError, shouldCancelActiveDialog);
}

void U2FTokenManager::Sign(PWebAuthnTransactionParent* aTransactionParent,
                           const uint64_t& aTransactionId,
                           const WebAuthnGetAssertionInfo& aTransactionInfo) {
  MOZ_LOG(gU2FTokenManagerLog, LogLevel::Debug, ("U2FAuthSign"));

  AbortOngoingTransaction();
  mTransactionParent = aTransactionParent;
  mTokenManagerImpl = GetTokenManagerImpl();

  if (!mTokenManagerImpl) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  mLastTransactionId = aTransactionId;
  mPendingSignInfo = Some(aTransactionInfo);
  DoSign(aTransactionInfo);
}

void U2FTokenManager::DoSign(const WebAuthnGetAssertionInfo& aTransactionInfo) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(mLastTransactionId > 0);
  uint64_t tid = mLastTransactionId;

  NS_ConvertUTF16toUTF8 origin(aTransactionInfo.Origin());
  uint64_t browserCtxId = aTransactionInfo.BrowsingContextId();

  // Show a prompt that lets the user cancel the ongoing transaction.
  SendPromptNotification(kPresencePromptNotificationU2F, tid, origin.get(),
                         browserCtxId, "false");

  mTokenManagerImpl->Sign(aTransactionInfo)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [tid, origin](nsTArray<WebAuthnGetAssertionResultWrapper>&& aResult) {
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
            bool shouldCancelActiveDialog = true;
            if (rv == NS_ERROR_DOM_OPERATION_ERR) {
              // PIN-related errors. Let the dialog show to inform the user
              shouldCancelActiveDialog = false;
            }
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"U2FSignAbort"_ns, 1);
            mgr->MaybeAbortSign(tid, rv, shouldCancelActiveDialog);
          })
      ->Track(mSignPromise);
}

void U2FTokenManager::MaybeConfirmSign(
    const uint64_t& aTransactionId, const WebAuthnGetAssertionResult& aResult) {
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mSignPromise.Complete();

  Unused << mTransactionParent->SendConfirmSign(aTransactionId, aResult);
  ClearTransaction(true);
}

void U2FTokenManager::MaybeAbortSign(const uint64_t& aTransactionId,
                                     const nsresult& aError,
                                     bool shouldCancelActiveDialog) {
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mSignPromise.Complete();
  AbortTransaction(aTransactionId, aError, shouldCancelActiveDialog);
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
  ClearTransaction(true);
}

// nsIU2FTokenManager

NS_IMETHODIMP
U2FTokenManager::ResumeRegister(uint64_t aTransactionId,
                                bool aForceNoneAttestation) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!gBackgroundThread) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<uint64_t, bool>(
      "U2FTokenManager::RunResumeRegister", this,
      &U2FTokenManager::RunResumeRegister, aTransactionId,
      aForceNoneAttestation));

  return gBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void U2FTokenManager::RunResumeRegister(uint64_t aTransactionId,
                                        bool aForceNoneAttestation) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mPendingRegisterInfo.isNothing())) {
    return;
  }

  if (mLastTransactionId != aTransactionId) {
    return;
  }

  // Resume registration and cleanup.
  DoRegister(mPendingRegisterInfo.ref(), aForceNoneAttestation);
}

void U2FTokenManager::RunResumeSign(uint64_t aTransactionId) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mPendingSignInfo.isNothing())) {
    return;
  }

  if (mLastTransactionId != aTransactionId) {
    return;
  }

  // Resume sign and cleanup.
  DoSign(mPendingSignInfo.ref());
}

NS_IMETHODIMP
U2FTokenManager::Cancel(uint64_t aTransactionId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!gBackgroundThread) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> r(
      NewRunnableMethod<uint64_t>("U2FTokenManager::RunCancel", this,
                                  &U2FTokenManager::RunCancel, aTransactionId));

  return gBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void U2FTokenManager::RunCancel(uint64_t aTransactionId) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (mLastTransactionId != aTransactionId) {
    return;
  }

  // Cancel the request.
  mTokenManagerImpl->Cancel();

  // Reject the promise.
  AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
}

}  // namespace mozilla::dom
