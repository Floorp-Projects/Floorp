/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/U2FTokenManager.h"
#include "mozilla/dom/U2FTokenTransport.h"
#include "mozilla/dom/U2FHIDTokenManager.h"
#include "mozilla/dom/U2FSoftTokenManager.h"
#include "mozilla/dom/PWebAuthnTransactionParent.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/WebAuthnUtil.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Unused.h"
#include "nsTextFormatter.h"

// Not named "security.webauth.u2f_softtoken_counter" because setting that
// name causes the window.u2f object to disappear until preferences get
// reloaded, as its pref is a substring!
#define PREF_U2F_NSSTOKEN_COUNTER "security.webauth.softtoken_counter"
#define PREF_WEBAUTHN_SOFTTOKEN_ENABLED "security.webauth.webauthn_enable_softtoken"
#define PREF_WEBAUTHN_USBTOKEN_ENABLED "security.webauth.webauthn_enable_usbtoken"
#define PREF_WEBAUTHN_ALLOW_DIRECT_ATTESTATION "security.webauth.webauthn_testing_allow_direct_attestation"

namespace mozilla {
namespace dom {

/***********************************************************************
 * Statics
 **********************************************************************/

class U2FPrefManager;

namespace {
static mozilla::LazyLogModule gU2FTokenManagerLog("u2fkeymanager");
StaticRefPtr<U2FTokenManager> gU2FTokenManager;
StaticRefPtr<U2FPrefManager> gPrefManager;
static nsIThread* gBackgroundThread;
}

// Data for WebAuthn UI prompt notifications.
static const char16_t kRegisterPromptNotifcation[] =
  u"{\"action\":\"register\",\"tid\":%llu,\"origin\":\"%s\"}";
static const char16_t kRegisterDirectPromptNotifcation[] =
  u"{\"action\":\"register-direct\",\"tid\":%llu,\"origin\":\"%s\"}";
static const char16_t kSignPromptNotifcation[] =
  u"{\"action\":\"sign\",\"tid\":%llu,\"origin\":\"%s\"}";
static const char16_t kCancelPromptNotifcation[] =
  u"{\"action\":\"cancel\",\"tid\":%llu}";

class U2FPrefManager final : public nsIObserver
{
private:
  U2FPrefManager() :
    mPrefMutex("U2FPrefManager Mutex")
  {
    UpdateValues();
  }
  ~U2FPrefManager() = default;

public:
  NS_DECL_ISUPPORTS

  static U2FPrefManager* GetOrCreate()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (!gPrefManager) {
      gPrefManager = new U2FPrefManager();
      Preferences::AddStrongObserver(gPrefManager, PREF_WEBAUTHN_SOFTTOKEN_ENABLED);
      Preferences::AddStrongObserver(gPrefManager, PREF_U2F_NSSTOKEN_COUNTER);
      Preferences::AddStrongObserver(gPrefManager, PREF_WEBAUTHN_USBTOKEN_ENABLED);
      Preferences::AddStrongObserver(gPrefManager, PREF_WEBAUTHN_ALLOW_DIRECT_ATTESTATION);
      ClearOnShutdown(&gPrefManager, ShutdownPhase::ShutdownThreads);
    }
    return gPrefManager;
  }

  static U2FPrefManager* Get()
  {
    return gPrefManager;
  }

  bool GetSoftTokenEnabled()
  {
    MutexAutoLock lock(mPrefMutex);
    return mSoftTokenEnabled;
  }

  int GetSoftTokenCounter()
  {
    MutexAutoLock lock(mPrefMutex);
    return mSoftTokenCounter;
  }

  bool GetUsbTokenEnabled()
  {
    MutexAutoLock lock(mPrefMutex);
    return mUsbTokenEnabled;
  }

  bool GetAllowDirectAttestationForTesting()
  {
    MutexAutoLock lock(mPrefMutex);
    return mAllowDirectAttestation;
  }

  NS_IMETHODIMP
  Observe(nsISupports* aSubject,
          const char* aTopic,
          const char16_t* aData) override
  {
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
    mAllowDirectAttestation = Preferences::GetBool(PREF_WEBAUTHN_ALLOW_DIRECT_ATTESTATION);
  }

  Mutex mPrefMutex;
  bool mSoftTokenEnabled;
  int mSoftTokenCounter;
  bool mUsbTokenEnabled;
  bool mAllowDirectAttestation;
};

NS_IMPL_ISUPPORTS(U2FPrefManager, nsIObserver);

/***********************************************************************
 * U2FManager Implementation
 **********************************************************************/

NS_IMPL_ISUPPORTS(U2FTokenManager, nsIU2FTokenManager);

U2FTokenManager::U2FTokenManager()
  : mTransactionParent(nullptr)
  , mLastTransactionId(0)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  // Create on the main thread to make sure ClearOnShutdown() works.
  MOZ_ASSERT(NS_IsMainThread());
  // Create the preference manager while we're initializing.
  U2FPrefManager::GetOrCreate();
}

//static
void
U2FTokenManager::Initialize()
{
  if (!XRE_IsParentProcess()) {
    return;
  }
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gU2FTokenManager);
  gU2FTokenManager = new U2FTokenManager();
  ClearOnShutdown(&gU2FTokenManager);
}

//static
U2FTokenManager*
U2FTokenManager::Get()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  // We should only be accessing this on the background thread
  MOZ_ASSERT(!NS_IsMainThread());
  return gU2FTokenManager;
}

void
U2FTokenManager::AbortTransaction(const uint64_t& aTransactionId,
                                  const nsresult& aError)
{
  Unused << mTransactionParent->SendAbort(aTransactionId, aError);
  ClearTransaction();
}

void
U2FTokenManager::MaybeClearTransaction(PWebAuthnTransactionParent* aParent)
{
  // Only clear if we've been requested to do so by our current transaction
  // parent.
  if (mTransactionParent == aParent) {
    ClearTransaction();
  }
}

void
U2FTokenManager::ClearTransaction()
{
  if (mLastTransactionId > 0) {
    // Remove any prompts we might be showing for the current transaction.
    SendPromptNotification(kCancelPromptNotifcation, mLastTransactionId);
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
}

template<typename ...T> void
U2FTokenManager::SendPromptNotification(const char16_t* aFormat, T... aArgs)
{
  mozilla::ipc::AssertIsOnBackgroundThread();

  nsAutoString json;
  nsTextFormatter::ssprintf(json, aFormat, aArgs...);

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<nsString>(
      "U2FTokenManager::RunSendPromptNotification", this,
      &U2FTokenManager::RunSendPromptNotification, json));

  MOZ_ALWAYS_SUCCEEDS(
    GetMainThreadEventTarget()->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
}

void
U2FTokenManager::RunSendPromptNotification(nsString aJSON)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (NS_WARN_IF(!os)) {
    return;
  }

  nsCOMPtr<nsIU2FTokenManager> self = do_QueryInterface(this);
  MOZ_ALWAYS_SUCCEEDS(os->NotifyObservers(self, "webauthn-prompt", aJSON.get()));
}

RefPtr<U2FTokenTransport>
U2FTokenManager::GetTokenManagerImpl()
{
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

  // Prefer the HW token, even if the softtoken is enabled too.
  // We currently don't support soft and USB tokens enabled at the
  // same time as the softtoken would always win the race to register.
  // We could support it for signing though...
  if (pm->GetUsbTokenEnabled()) {
    return new U2FHIDTokenManager();
  }

  if (pm->GetSoftTokenEnabled()) {
    return new U2FSoftTokenManager(pm->GetSoftTokenCounter());
  }

  // TODO Use WebAuthnRequest to aggregate results from all transports,
  //      once we have multiple HW transport types.

  return nullptr;
}

void
U2FTokenManager::Register(PWebAuthnTransactionParent* aTransactionParent,
                          const uint64_t& aTransactionId,
                          const WebAuthnMakeCredentialInfo& aTransactionInfo)
{
  MOZ_LOG(gU2FTokenManagerLog, LogLevel::Debug, ("U2FAuthRegister"));

  ClearTransaction();
  mTransactionParent = aTransactionParent;
  mTokenManagerImpl = GetTokenManagerImpl();

  if (!mTokenManagerImpl) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  mLastTransactionId = aTransactionId;

  // Determine whether direct attestation was requested.
  bool directAttestationRequested = false;
  if (aTransactionInfo.Extra().type() == WebAuthnMaybeMakeCredentialExtraInfo::TWebAuthnMakeCredentialExtraInfo) {
    const auto& extra = aTransactionInfo.Extra().get_WebAuthnMakeCredentialExtraInfo();
    directAttestationRequested = extra.RequestDirectAttestation();
  }

  // Start a register request immediately if direct attestation
  // wasn't requested or the test pref is set.
  if (!directAttestationRequested ||
      U2FPrefManager::Get()->GetAllowDirectAttestationForTesting()) {
    // Force "none" attestation when "direct" attestation wasn't requested.
    DoRegister(aTransactionInfo, !directAttestationRequested);
    return;
  }

  // If the RP request direct attestation, ask the user for permission and
  // store the transaction info until the user proceeds or cancels.
  NS_ConvertUTF16toUTF8 origin(aTransactionInfo.Origin());
  SendPromptNotification(kRegisterDirectPromptNotifcation,
                         aTransactionId,
                         origin.get());

  MOZ_ASSERT(mPendingRegisterInfo.isNothing());
  mPendingRegisterInfo = Some(aTransactionInfo);
}

void
U2FTokenManager::DoRegister(const WebAuthnMakeCredentialInfo& aInfo,
                            bool aForceNoneAttestation)
{
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(mLastTransactionId > 0);

  // Show a prompt that lets the user cancel the ongoing transaction.
  NS_ConvertUTF16toUTF8 origin(aInfo.Origin());
  SendPromptNotification(kRegisterPromptNotifcation,
                         mLastTransactionId,
                         origin.get());

  uint64_t tid = mLastTransactionId;
  mozilla::TimeStamp startTime = mozilla::TimeStamp::Now();

  mTokenManagerImpl
    ->Register(aInfo, aForceNoneAttestation)
    ->Then(GetCurrentThreadSerialEventTarget(), __func__,
          [tid, startTime](WebAuthnMakeCredentialResult&& aResult) {
            U2FTokenManager* mgr = U2FTokenManager::Get();
            mgr->MaybeConfirmRegister(tid, aResult);
            Telemetry::ScalarAdd(
              Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
              NS_LITERAL_STRING("U2FRegisterFinish"), 1);
            Telemetry::AccumulateTimeDelta(
              Telemetry::WEBAUTHN_CREATE_CREDENTIAL_MS,
              startTime);
          },
          [tid](nsresult rv) {
            MOZ_ASSERT(NS_FAILED(rv));
            U2FTokenManager* mgr = U2FTokenManager::Get();
            mgr->MaybeAbortRegister(tid, rv);
            Telemetry::ScalarAdd(
              Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
              NS_LITERAL_STRING("U2FRegisterAbort"), 1);
          })
    ->Track(mRegisterPromise);
}

void
U2FTokenManager::MaybeConfirmRegister(const uint64_t& aTransactionId,
                                      const WebAuthnMakeCredentialResult& aResult)
{
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mRegisterPromise.Complete();

  Unused << mTransactionParent->SendConfirmRegister(aTransactionId, aResult);
  ClearTransaction();
}

void
U2FTokenManager::MaybeAbortRegister(const uint64_t& aTransactionId,
                                    const nsresult& aError)
{
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mRegisterPromise.Complete();
  AbortTransaction(aTransactionId, aError);
}

void
U2FTokenManager::Sign(PWebAuthnTransactionParent* aTransactionParent,
                      const uint64_t& aTransactionId,
                      const WebAuthnGetAssertionInfo& aTransactionInfo)
{
  MOZ_LOG(gU2FTokenManagerLog, LogLevel::Debug, ("U2FAuthSign"));

  ClearTransaction();
  mTransactionParent = aTransactionParent;
  mTokenManagerImpl = GetTokenManagerImpl();

  if (!mTokenManagerImpl) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  // Show a prompt that lets the user cancel the ongoing transaction.
  NS_ConvertUTF16toUTF8 origin(aTransactionInfo.Origin());
  SendPromptNotification(kSignPromptNotifcation,
                         aTransactionId,
                         origin.get());

  uint64_t tid = mLastTransactionId = aTransactionId;
  mozilla::TimeStamp startTime = mozilla::TimeStamp::Now();

  mTokenManagerImpl
    ->Sign(aTransactionInfo)
    ->Then(GetCurrentThreadSerialEventTarget(), __func__,
      [tid, startTime](WebAuthnGetAssertionResult&& aResult) {
        U2FTokenManager* mgr = U2FTokenManager::Get();
        mgr->MaybeConfirmSign(tid, aResult);
        Telemetry::ScalarAdd(
          Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
          NS_LITERAL_STRING("U2FSignFinish"), 1);
        Telemetry::AccumulateTimeDelta(
          Telemetry::WEBAUTHN_GET_ASSERTION_MS,
          startTime);
      },
      [tid](nsresult rv) {
        MOZ_ASSERT(NS_FAILED(rv));
        U2FTokenManager* mgr = U2FTokenManager::Get();
        mgr->MaybeAbortSign(tid, rv);
        Telemetry::ScalarAdd(
          Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
          NS_LITERAL_STRING("U2FSignAbort"), 1);
      })
    ->Track(mSignPromise);
}

void
U2FTokenManager::MaybeConfirmSign(const uint64_t& aTransactionId,
                                  const WebAuthnGetAssertionResult& aResult)
{
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mSignPromise.Complete();

  Unused << mTransactionParent->SendConfirmSign(aTransactionId, aResult);
  ClearTransaction();
}

void
U2FTokenManager::MaybeAbortSign(const uint64_t& aTransactionId,
                                const nsresult& aError)
{
  MOZ_ASSERT(mLastTransactionId == aTransactionId);
  mSignPromise.Complete();
  AbortTransaction(aTransactionId, aError);
}

void
U2FTokenManager::Cancel(PWebAuthnTransactionParent* aParent,
                        const uint64_t& aTransactionId)
{
  if (mTransactionParent != aParent || mLastTransactionId != aTransactionId) {
    return;
  }

  mTokenManagerImpl->Cancel();
  ClearTransaction();
}

// nsIU2FTokenManager

NS_IMETHODIMP
U2FTokenManager::ResumeRegister(uint64_t aTransactionId,
                                bool aForceNoneAttestation)
{
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

void
U2FTokenManager::RunResumeRegister(uint64_t aTransactionId,
                                   bool aForceNoneAttestation)
{
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mPendingRegisterInfo.isNothing())) {
    return;
  }

  if (mLastTransactionId != aTransactionId) {
    return;
  }

  // Resume registration and cleanup.
  DoRegister(mPendingRegisterInfo.ref(), aForceNoneAttestation);
  mPendingRegisterInfo.reset();
}

NS_IMETHODIMP
U2FTokenManager::Cancel(uint64_t aTransactionId)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!gBackgroundThread) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<uint64_t>(
      "U2FTokenManager::RunCancel", this,
      &U2FTokenManager::RunCancel, aTransactionId));

  return gBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void
U2FTokenManager::RunCancel(uint64_t aTransactionId)
{
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (mLastTransactionId != aTransactionId) {
    return;
  }

  // Cancel the request.
  mTokenManagerImpl->Cancel();

  // Reject the promise.
  AbortTransaction(aTransactionId, NS_ERROR_DOM_ABORT_ERR);
}

}
}
