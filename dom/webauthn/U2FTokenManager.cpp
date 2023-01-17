/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "json/json.h"
#include "mozilla/dom/U2FTokenManager.h"
#include "mozilla/dom/U2FTokenTransport.h"
#include "mozilla/dom/CTAPHIDTokenManager.h"
#include "mozilla/dom/U2FHIDTokenManager.h"
#include "mozilla/dom/U2FSoftTokenManager.h"
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

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/dom/AndroidWebAuthnTokenManager.h"
#endif

// Not named "security.webauth.u2f_softtoken_counter" because setting that
// name causes the window.u2f object to disappear until preferences get
// reloaded, as its pref is a substring!
#define PREF_U2F_NSSTOKEN_COUNTER "security.webauth.softtoken_counter"
#define PREF_WEBAUTHN_SOFTTOKEN_ENABLED \
  "security.webauth.webauthn_enable_softtoken"
#define PREF_WEBAUTHN_USBTOKEN_ENABLED \
  "security.webauth.webauthn_enable_usbtoken"
#define PREF_WEBAUTHN_ALLOW_DIRECT_ATTESTATION \
  "security.webauth.webauthn_testing_allow_direct_attestation"
#define PREF_WEBAUTHN_ANDROID_FIDO2_ENABLED \
  "security.webauth.webauthn_enable_android_fido2"
#define PREF_WEBAUTHN_CTAP2 "security.webauthn.ctap2"
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
static const char16_t kRegisterPromptNotifcation[] =
    u"{\"action\":\"register\",\"tid\":%llu,\"origin\":\"%s\","
    u"\"browsingContextId\":%llu,\"is_ctap2\":%s, \"device_selected\":%s}";
static const char16_t kRegisterDirectPromptNotifcation[] =
    u"{\"action\":\"register-direct\",\"tid\":%llu,\"origin\":\"%s\","
    u"\"browsingContextId\":%llu}";
static const char16_t kSignPromptNotifcation[] =
    u"{\"action\":\"sign\",\"tid\":%llu,\"origin\":\"%s\","
    u"\"browsingContextId\":%llu,\"is_ctap2\":%s, \"device_selected\":%s}";
static const char16_t kCancelPromptNotifcation[] =
    u"{\"action\":\"cancel\",\"tid\":%llu}";
static const char16_t kPinRequiredNotifcation[] =
    u"{\"action\":\"pin-required\",\"tid\":%llu,\"origin\":\"%s\","
    u"\"browsingContextId\":%llu,\"wasInvalid\":%s,\"retriesLeft\":%i}";
static const char16_t kSelectDeviceNotifcation[] =
    u"{\"action\":\"select-device\",\"tid\":%llu,\"origin\":\"%s\","
    u"\"browsingContextId\":%llu}";
static const char16_t kSelectSignResultNotifcation[] =
    u"{\"action\":\"select-sign-result\",\"tid\":%llu,\"origin\":\"%s\","
    u"\"browsingContextId\":%llu,\"usernames\":[%s]}";
static const char16_t kPinErrorNotifications[] =
    u"{\"action\":\"%s\",\"tid\":%llu,\"origin\":\"%s\","
    u"\"browsingContextId\":%llu}";

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
                                     PREF_WEBAUTHN_SOFTTOKEN_ENABLED);
      Preferences::AddStrongObserver(gPrefManager, PREF_U2F_NSSTOKEN_COUNTER);
      Preferences::AddStrongObserver(gPrefManager,
                                     PREF_WEBAUTHN_USBTOKEN_ENABLED);
      Preferences::AddStrongObserver(gPrefManager,
                                     PREF_WEBAUTHN_ANDROID_FIDO2_ENABLED);
      Preferences::AddStrongObserver(gPrefManager,
                                     PREF_WEBAUTHN_ALLOW_DIRECT_ATTESTATION);
      Preferences::AddStrongObserver(gPrefManager, PREF_WEBAUTHN_CTAP2);
      ClearOnShutdown(&gPrefManager, ShutdownPhase::XPCOMShutdownThreads);
    }
    return gPrefManager;
  }

  static U2FPrefManager* Get() { return gPrefManager; }

  bool GetSoftTokenEnabled() {
    MutexAutoLock lock(mPrefMutex);
    return mSoftTokenEnabled;
  }

  int GetSoftTokenCounter() {
    MutexAutoLock lock(mPrefMutex);
    return mSoftTokenCounter;
  }

  bool GetUsbTokenEnabled() {
    MutexAutoLock lock(mPrefMutex);
    return mUsbTokenEnabled;
  }

  bool GetAndroidFido2Enabled() {
    MutexAutoLock lock(mPrefMutex);
    return mAndroidFido2Enabled;
  }

  bool GetAllowDirectAttestationForTesting() {
    MutexAutoLock lock(mPrefMutex);
    return mAllowDirectAttestation;
  }

  bool GetIsCtap2() {
    MutexAutoLock lock(mPrefMutex);
    return mCtap2;
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
    mSoftTokenEnabled = Preferences::GetBool(PREF_WEBAUTHN_SOFTTOKEN_ENABLED);
    mSoftTokenCounter = Preferences::GetUint(PREF_U2F_NSSTOKEN_COUNTER);
    mUsbTokenEnabled = Preferences::GetBool(PREF_WEBAUTHN_USBTOKEN_ENABLED);
    mAndroidFido2Enabled =
        Preferences::GetBool(PREF_WEBAUTHN_ANDROID_FIDO2_ENABLED);
    mAllowDirectAttestation =
        Preferences::GetBool(PREF_WEBAUTHN_ALLOW_DIRECT_ATTESTATION);
    mCtap2 = Preferences::GetBool(PREF_WEBAUTHN_CTAP2);
  }

  Mutex mPrefMutex MOZ_UNANNOTATED;
  bool mSoftTokenEnabled;
  int mSoftTokenCounter;
  bool mUsbTokenEnabled;
  bool mAndroidFido2Enabled;
  bool mAllowDirectAttestation;
  bool mCtap2;
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
    SendPromptNotification(kCancelPromptNotifcation, mLastTransactionId);
  }

  mTransactionParent = nullptr;

  // We have to "hang up" in case auth-rs is still waiting for us to send a PIN
  // so it can exit cleanly
  status_update_result = nullptr;
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

  MOZ_ALWAYS_SUCCEEDS(
      GetMainThreadEventTarget()->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
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

void U2FTokenManager::StatusUpdateResFreePolicy::operator()(
    rust_ctap2_status_update_res* p) {
  rust_ctap2_destroy_status_update_res(p);
}

static void status_callback(rust_ctap2_status_update_res* status) {
  if (!U2FPrefManager::Get()->GetIsCtap2() || (NS_WARN_IF(!status))) {
    return;
  }

  // The result will be cleared automatically upon exiting this function,
  // unless we have a Pin error, then we need it for a callback from JS.
  // Then we move ownership of it to U2FTokenManager.
  UniquePtr<rust_ctap2_status_update_res,
            U2FTokenManager::StatusUpdateResFreePolicy>
      status_result_update(status);
  size_t len;
  if (NS_WARN_IF(!rust_ctap2_status_update_len(status, &len))) {
    return;
  }
  nsCString st;
  if (NS_WARN_IF(!st.SetLength(len, fallible))) {
    return;
  }
  if (NS_WARN_IF(!rust_ctap2_status_update_copy_json(status, st.Data()))) {
    return;
  }

  auto* gInstance = U2FTokenManager::Get();

  Json::Value jsonRoot;
  Json::Reader reader;
  if (NS_WARN_IF(!reader.parse(st.Data(), jsonRoot))) {
    return;
  }
  if (NS_WARN_IF(!jsonRoot.isObject())) {
    return;
  }

  nsAutoString notification_json;

  uint64_t browsingCtxId = gInstance->GetCurrentBrowsingCtxId().value();
  NS_ConvertUTF16toUTF8 origin(gInstance->GetCurrentOrigin().value());
  if (jsonRoot.isMember("PinError")) {
    uint64_t tid = gInstance->GetCurrentTransactionId();
    bool pinRequired = (jsonRoot["PinError"].isString() &&
                        jsonRoot["PinError"].asString() == "PinRequired");
    bool pinInvalid = (jsonRoot["PinError"].isObject() &&
                       jsonRoot["PinError"].isMember("InvalidPin"));
    if (pinRequired || pinInvalid) {
      bool wasInvalid = false;
      int retries = -1;
      if (pinInvalid) {
        wasInvalid = true;
        if (jsonRoot["PinError"]["InvalidPin"].isInt()) {
          retries = jsonRoot["PinError"]["InvalidPin"].asInt();
        }
      }
      gInstance->status_update_result = std::move(status_result_update);
      nsTextFormatter::ssprintf(notification_json, kPinRequiredNotifcation, tid,
                                origin.get(), browsingCtxId,
                                wasInvalid ? "true" : "false", retries);
    } else if (jsonRoot["PinError"].isString()) {
      // Not saving the status_result, so the callback will error out and cancel
      // the transaction, because these errors are not recoverable by
      // user-input.
      if (jsonRoot["PinError"].asString() == "PinAuthBlocked") {
        // Pin authentication blocked. Device needs power cycle!
        nsTextFormatter::ssprintf(notification_json, kPinErrorNotifications,
                                  "pin-auth-blocked", tid, origin.get(),
                                  browsingCtxId);
      } else if (jsonRoot["PinError"].asString() == "PinBlocked") {
        // No retries left. Pin blocked. Device needs reset!
        nsTextFormatter::ssprintf(notification_json, kPinErrorNotifications,
                                  "device-blocked", tid, origin.get(),
                                  browsingCtxId);
      }
    }
  } else if (jsonRoot.isMember("SelectDeviceNotice")) {
    nsTextFormatter::ssprintf(notification_json, kSelectDeviceNotifcation,
                              gInstance->GetCurrentTransactionId(),
                              origin.get(), browsingCtxId);
  } else if (jsonRoot.isMember("DeviceSelected")) {
    if (gInstance->CurrentTransactionIsRegister()) {
      nsTextFormatter::ssprintf(notification_json, kRegisterPromptNotifcation,
                                gInstance->GetCurrentTransactionId(),
                                origin.get(), browsingCtxId, "true", "true");
    } else if (gInstance->CurrentTransactionIsSign()) {
      nsTextFormatter::ssprintf(notification_json, kSignPromptNotifcation,
                                gInstance->GetCurrentTransactionId(),
                                origin.get(), browsingCtxId, "true", "true");
    }
  } else {
    // No-op for now
  }

  if (!notification_json.IsEmpty()) {
    nsCOMPtr<nsIRunnable> r(NewRunnableMethod<nsString>(
        "U2FTokenManager::RunSendPromptNotification", gInstance,
        &U2FTokenManager::RunSendPromptNotification, notification_json));
    MOZ_ALWAYS_SUCCEEDS(
        GetMainThreadEventTarget()->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
  }
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

  auto pm = U2FPrefManager::Get();

#ifdef MOZ_WIDGET_ANDROID
  // On Android, prefer the platform support if enabled.
  if (pm->GetAndroidFido2Enabled()) {
    return AndroidWebAuthnTokenManager::GetInstance();
  }
#endif

  // Prefer the HW token, even if the softtoken is enabled too.
  // We currently don't support soft and USB tokens enabled at the
  // same time as the softtoken would always win the race to register.
  // We could support it for signing though...
  if (pm->GetUsbTokenEnabled()) {
    U2FTokenTransport* manager;
    if (U2FPrefManager::Get()->GetIsCtap2()) {
      manager = new CTAPHIDTokenManager();
    } else {
      manager = new U2FHIDTokenManager();
    }
    return manager;
  }

  if (pm->GetSoftTokenEnabled()) {
    return new U2FSoftTokenManager(pm->GetSoftTokenCounter());
  }

  // TODO Use WebAuthnRequest to aggregate results from all transports,
  //      once we have multiple HW transport types.

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
  if (aTransactionInfo.Extra().isSome()) {
    const auto& extra = aTransactionInfo.Extra().ref();

    AttestationConveyancePreference attestation =
        extra.attestationConveyancePreference();

    noneAttestationRequested =
        attestation == AttestationConveyancePreference::None;
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
  SendPromptNotification(kRegisterDirectPromptNotifcation, aTransactionId,
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
  const char* is_ctap2 = U2FPrefManager::Get()->GetIsCtap2() ? "true" : "false";
  SendPromptNotification(kRegisterPromptNotifcation, mLastTransactionId,
                         origin.get(), aInfo.BrowsingContextId(), is_ctap2,
                         "false");

  uint64_t tid = mLastTransactionId;
  mozilla::TimeStamp startTime = mozilla::TimeStamp::Now();

  mTokenManagerImpl->Register(aInfo, aForceNoneAttestation, status_callback)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [tid, startTime](WebAuthnMakeCredentialResult&& aResult) {
            U2FTokenManager* mgr = U2FTokenManager::Get();
            mgr->MaybeConfirmRegister(tid, aResult);
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"U2FRegisterFinish"_ns, 1);
            Telemetry::AccumulateTimeDelta(
                Telemetry::WEBAUTHN_CREATE_CREDENTIAL_MS, startTime);
          },
          [tid](nsresult rv) {
            MOZ_ASSERT(NS_FAILED(rv));
            U2FTokenManager* mgr = U2FTokenManager::Get();
            bool shouldCancelActiveDialog = true;
            if (rv == NS_ERROR_DOM_OPERATION_ERR) {
              // PIN-related errors. Let the dialog show to inform the user
              shouldCancelActiveDialog = false;
            }
            mgr->MaybeAbortRegister(tid, rv, shouldCancelActiveDialog);
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"U2FRegisterAbort"_ns, 1);
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
  const char* is_ctap2 = U2FPrefManager::Get()->GetIsCtap2() ? "true" : "false";
  SendPromptNotification(kSignPromptNotifcation, tid, origin.get(),
                         browserCtxId, is_ctap2, "false");

  mozilla::TimeStamp startTime = mozilla::TimeStamp::Now();

  mTokenManagerImpl->Sign(aTransactionInfo, status_callback)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [tid, origin, startTime, browserCtxId](
              nsTArray<WebAuthnGetAssertionResultWrapper>&& aResult) {
            U2FTokenManager* mgr = U2FTokenManager::Get();
            if (aResult.Length() == 1) {
              WebAuthnGetAssertionResult result = aResult[0].assertion;
              mgr->MaybeConfirmSign(tid, result);
            } else {
              nsCString res;
              StringJoinAppend(
                  res, ","_ns, aResult,
                  [](nsACString& dst,
                     const WebAuthnGetAssertionResultWrapper& assertion) {
                    nsCString username =
                        assertion.username.valueOr("<Unknown username>"_ns);
                    nsCString escaped_username;
                    NS_Escape(username, escaped_username, url_XAlphas);
                    dst.Append("\""_ns + escaped_username + "\""_ns);
                  });
              mgr->mPendingSignResults.Assign(aResult);
              mgr->SendPromptNotification(kSelectSignResultNotifcation, tid,
                                          origin.get(), browserCtxId,
                                          res.get());
            }
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"U2FSignFinish"_ns, 1);
            Telemetry::AccumulateTimeDelta(Telemetry::WEBAUTHN_GET_ASSERTION_MS,
                                           startTime);
          },
          [tid](nsresult rv) {
            MOZ_ASSERT(NS_FAILED(rv));
            U2FTokenManager* mgr = U2FTokenManager::Get();
            bool shouldCancelActiveDialog = true;
            if (rv == NS_ERROR_DOM_OPERATION_ERR) {
              // PIN-related errors. Let the dialog show to inform the user
              shouldCancelActiveDialog = false;
            }
            mgr->MaybeAbortSign(tid, rv, shouldCancelActiveDialog);
            Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                                 u"U2FSignAbort"_ns, 1);
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

NS_IMETHODIMP
U2FTokenManager::ResumeWithSelectedSignResult(uint64_t aTransactionId,
                                              uint64_t idx) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!gBackgroundThread) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<uint64_t, uint64_t>(
      "U2FTokenManager::RunResumeWithSelectedSignResult", this,
      &U2FTokenManager::RunResumeWithSelectedSignResult, aTransactionId, idx));

  return gBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void U2FTokenManager::RunResumeWithSelectedSignResult(uint64_t aTransactionId,
                                                      uint64_t idx) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (NS_WARN_IF(mPendingSignResults.IsEmpty())) {
    return;
  }

  if (NS_WARN_IF(mPendingSignResults.Length() <= idx)) {
    return;
  }

  if (mLastTransactionId != aTransactionId) {
    return;
  }

  WebAuthnGetAssertionResult result = mPendingSignResults[idx].assertion;
  MaybeConfirmSign(aTransactionId, result);
}

NS_IMETHODIMP
U2FTokenManager::PinCallback(const nsACString& aPin) {
  if (!U2FPrefManager::Get()->GetIsCtap2()) {
    // Not used in CTAP1
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  if (!gBackgroundThread) {
    return NS_ERROR_FAILURE;
  }
  // Move the result here locally, so it will be freed either way
  UniquePtr<rust_ctap2_status_update_res,
            U2FTokenManager::StatusUpdateResFreePolicy>
      result = nullptr;
  std::swap(result, status_update_result);

  if (NS_WARN_IF(
          !rust_ctap2_status_update_send_pin(result.get(), aPin.Data()))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
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

  // We have to "hang up" in case auth-rs is still waiting for us to send a PIN
  // so it can exit cleanly
  status_update_result = nullptr;
  // Cancel the request.
  mTokenManagerImpl->Cancel();

  // Reject the promise.
  AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
}

}  // namespace mozilla::dom
