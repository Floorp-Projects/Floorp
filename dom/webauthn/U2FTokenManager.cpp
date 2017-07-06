/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/U2FTokenManager.h"
#include "mozilla/dom/U2FTokenTransport.h"
#include "mozilla/dom/U2FHIDTokenManager.h"
#include "mozilla/dom/U2FSoftTokenManager.h"
#include "mozilla/dom/WebAuthnTransactionParent.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/WebAuthnUtil.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Unused.h"
#include "hasht.h"
#include "nsICryptoHash.h"
#include "pkix/Input.h"
#include "pkixutil.h"

// Not named "security.webauth.u2f_softtoken_counter" because setting that
// name causes the window.u2f object to disappear until preferences get
// reloaded, as its pref is a substring!
#define PREF_U2F_NSSTOKEN_COUNTER "security.webauth.softtoken_counter"
#define PREF_WEBAUTHN_SOFTTOKEN_ENABLED "security.webauth.webauthn_enable_softtoken"
#define PREF_WEBAUTHN_USBTOKEN_ENABLED "security.webauth.webauthn_enable_usbtoken"

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
}

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
  }

  Mutex mPrefMutex;
  bool mSoftTokenEnabled;
  int mSoftTokenCounter;
  bool mUsbTokenEnabled;
};

NS_IMPL_ISUPPORTS(U2FPrefManager, nsIObserver);

/***********************************************************************
 * U2FManager Implementation
 **********************************************************************/

U2FTokenManager::U2FTokenManager()
  : mTransactionParent(nullptr)
  , mTransactionId(0)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  // Create on the main thread to make sure ClearOnShutdown() works.
  MOZ_ASSERT(NS_IsMainThread());
  // Create the preference manager while we're initializing.
  U2FPrefManager::GetOrCreate();
}

U2FTokenManager::~U2FTokenManager()
{
  MOZ_ASSERT(NS_IsMainThread());
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
U2FTokenManager::MaybeAbortTransaction(uint64_t aTransactionId,
                                       const nsresult& aError)
{
  if (mTransactionId != aTransactionId) {
    return;
  }

  AbortTransaction(aError);
}

void
U2FTokenManager::AbortTransaction(const nsresult& aError)
{
  Unused << mTransactionParent->SendCancel(aError);
  ClearTransaction();
}

void
U2FTokenManager::MaybeClearTransaction(WebAuthnTransactionParent* aParent)
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
  mTransactionParent = nullptr;
  // Drop managers at the end of all transactions
  mTokenManagerImpl = nullptr;
  // Drop promise.
  mResultPromise = nullptr;
  // Increase in case we're called by the WebAuthnTransactionParent.
  mTransactionId++;
}

RefPtr<U2FTokenTransport>
U2FTokenManager::GetTokenManagerImpl()
{
  if (mTokenManagerImpl) {
    return mTokenManagerImpl;
  }

  auto pm = U2FPrefManager::Get();
  bool useSoftToken = pm->GetSoftTokenEnabled();
  bool useUsbToken = pm->GetUsbTokenEnabled();

  // At least one token type must be enabled.
  // We currently don't support soft and USB tokens enabled at
  // the same time as the softtoken would always win the race to register.
  // We could support it for signing though...
  if (!(useSoftToken ^ useUsbToken)) {
    return nullptr;
  }

  if (useSoftToken) {
    return new U2FSoftTokenManager(pm->GetSoftTokenCounter());
  }

  // TODO Use WebAuthnRequest to aggregate results from all transports,
  //      once we have multiple HW transport types.
  return new U2FHIDTokenManager();
}

void
U2FTokenManager::Register(WebAuthnTransactionParent* aTransactionParent,
                          const WebAuthnTransactionInfo& aTransactionInfo)
{
  MOZ_LOG(gU2FTokenManagerLog, LogLevel::Debug, ("U2FAuthRegister"));
  MOZ_ASSERT(U2FPrefManager::Get());

  uint64_t tid = ++mTransactionId;
  mTransactionParent = aTransactionParent;
  mTokenManagerImpl = GetTokenManagerImpl();

  if (!mTokenManagerImpl) {
    AbortTransaction(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  // Check if all the supplied parameters are syntactically well-formed and
  // of the correct length. If not, return an error code equivalent to
  // UnknownError and terminate the operation.

  if ((aTransactionInfo.RpIdHash().Length() != SHA256_LENGTH) ||
      (aTransactionInfo.ClientDataHash().Length() != SHA256_LENGTH)) {
    AbortTransaction(NS_ERROR_DOM_UNKNOWN_ERR);
    return;
  }

  nsTArray<uint8_t> reg;
  nsTArray<uint8_t> sig;
  mResultPromise = mTokenManagerImpl->Register(aTransactionInfo.Descriptors(),
                                               aTransactionInfo.RpIdHash(),
                                               aTransactionInfo.ClientDataHash(),
                                               reg,
                                               sig);

  mResultPromise->Then(GetCurrentThreadSerialEventTarget(), __func__,
                       [tid, reg, sig](nsresult rv) {
                         MOZ_ASSERT(NS_SUCCEEDED(rv));
                         U2FTokenManager* mgr = U2FTokenManager::Get();
                         mgr->MaybeConfirmRegister(tid, reg, sig);
                       },
                       [tid](nsresult rv) {
                         MOZ_ASSERT(NS_FAILED(rv));
                         U2FTokenManager* mgr = U2FTokenManager::Get();
                         mgr->MaybeAbortTransaction(tid, rv);
                       });
}

void
U2FTokenManager::MaybeConfirmRegister(uint64_t aTransactionId,
                                      const nsTArray<uint8_t>& aRegister,
                                      const nsTArray<uint8_t>& aSignature)
{
  if (mTransactionId != aTransactionId) {
    return;
  }

  Unused << mTransactionParent->SendConfirmRegister(aRegister, aSignature);
  ClearTransaction();
}

void
U2FTokenManager::Sign(WebAuthnTransactionParent* aTransactionParent,
                      const WebAuthnTransactionInfo& aTransactionInfo)
{
  MOZ_LOG(gU2FTokenManagerLog, LogLevel::Debug, ("U2FAuthSign"));
  MOZ_ASSERT(U2FPrefManager::Get());

  uint64_t tid = ++mTransactionId;
  mTransactionParent = aTransactionParent;
  mTokenManagerImpl = GetTokenManagerImpl();

  if (!mTokenManagerImpl) {
    AbortTransaction(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  if ((aTransactionInfo.RpIdHash().Length() != SHA256_LENGTH) ||
      (aTransactionInfo.ClientDataHash().Length() != SHA256_LENGTH)) {
    AbortTransaction(NS_ERROR_DOM_UNKNOWN_ERR);
    return;
  }

  nsTArray<uint8_t> id;
  nsTArray<uint8_t> sig;
  mResultPromise = mTokenManagerImpl->Sign(aTransactionInfo.Descriptors(),
                                           aTransactionInfo.RpIdHash(),
                                           aTransactionInfo.ClientDataHash(),
                                           id,
                                           sig);

  mResultPromise->Then(GetCurrentThreadSerialEventTarget(), __func__,
                       [tid, id, sig](nsresult rv) {
                         MOZ_ASSERT(NS_SUCCEEDED(rv));
                         U2FTokenManager* mgr = U2FTokenManager::Get();
                         mgr->MaybeConfirmSign(tid, id, sig);
                       },
                       [tid](nsresult rv) {
                         MOZ_ASSERT(NS_FAILED(rv));
                         U2FTokenManager* mgr = U2FTokenManager::Get();
                         mgr->MaybeAbortTransaction(tid, rv);
                       });
}

void
U2FTokenManager::MaybeConfirmSign(uint64_t aTransactionId,
                                  const nsTArray<uint8_t>& aKeyHandle,
                                  const nsTArray<uint8_t>& aSignature)
{
  if (mTransactionId != aTransactionId) {
    return;
  }

  Unused << mTransactionParent->SendConfirmSign(aKeyHandle, aSignature);
  ClearTransaction();
}

void
U2FTokenManager::Cancel(WebAuthnTransactionParent* aParent)
{
  if (mTransactionParent == aParent) {
    mTokenManagerImpl->Cancel();
    ClearTransaction();
  }
}

}
}
