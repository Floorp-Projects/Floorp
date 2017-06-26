/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/U2FTokenManager.h"
#include "mozilla/dom/U2FTokenTransport.h"
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
// reloaded, as its' pref is a substring!
#define PREF_U2F_NSSTOKEN_COUNTER "security.webauth.softtoken_counter"
#define PREF_WEBAUTHN_SOFTTOKEN_ENABLED "security.webauth.webauthn_enable_softtoken"

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
    MOZ_ASSERT(NS_IsMainThread());
    MutexAutoLock lock(mPrefMutex);
    mSoftTokenEnabled = Preferences::GetBool(PREF_WEBAUTHN_SOFTTOKEN_ENABLED);
    mSoftTokenCounter = Preferences::GetUint(PREF_U2F_NSSTOKEN_COUNTER);
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

  NS_IMETHODIMP
  Observe(nsISupports* aSubject,
          const char* aTopic,
          const char16_t* aData) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MutexAutoLock lock(mPrefMutex);
    mSoftTokenEnabled = Preferences::GetBool(PREF_WEBAUTHN_SOFTTOKEN_ENABLED);
    mSoftTokenCounter = Preferences::GetUint(PREF_U2F_NSSTOKEN_COUNTER);
    return NS_OK;
  }
private:
  Mutex mPrefMutex;
  bool mSoftTokenEnabled;
  int mSoftTokenCounter;
};

NS_IMPL_ISUPPORTS(U2FPrefManager, nsIObserver);

/***********************************************************************
 * U2FManager Implementation
 **********************************************************************/

U2FTokenManager::U2FTokenManager() :
  mTransactionParent(nullptr)
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
}

void
U2FTokenManager::Register(WebAuthnTransactionParent* aTransactionParent,
                          const WebAuthnTransactionInfo& aTransactionInfo)
{
  MOZ_LOG(gU2FTokenManagerLog, LogLevel::Debug, ("U2FAuthRegister"));
  MOZ_ASSERT(U2FPrefManager::Get());
  mTransactionParent = aTransactionParent;

  // Since we only have soft token available at the moment, use that if the pref
  // is on.
  //
  // TODO Check all transports and use WebAuthnRequest to aggregate
  // replies
  if (!U2FPrefManager::Get()->GetSoftTokenEnabled()) {
    AbortTransaction(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  if (!mTokenManagerImpl) {
    mTokenManagerImpl = new U2FSoftTokenManager(U2FPrefManager::Get()->GetSoftTokenCounter());
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
  nsresult rv = mTokenManagerImpl->Register(aTransactionInfo.Descriptors(),
                                            aTransactionInfo.RpIdHash(),
                                            aTransactionInfo.ClientDataHash(),
                                            reg,
                                            sig);
  if (NS_FAILED(rv)) {
    AbortTransaction(rv);
    return;
  }

  Unused << mTransactionParent->SendConfirmRegister(reg, sig);
  ClearTransaction();
}

void
U2FTokenManager::Sign(WebAuthnTransactionParent* aTransactionParent,
                      const WebAuthnTransactionInfo& aTransactionInfo)
{
  MOZ_LOG(gU2FTokenManagerLog, LogLevel::Debug, ("U2FAuthSign"));
  MOZ_ASSERT(U2FPrefManager::Get());
  mTransactionParent = aTransactionParent;

  // Since we only have soft token available at the moment, use that if the pref
  // is on.
  //
  // TODO Check all transports and use WebAuthnRequest to aggregate
  // replies
  if (!U2FPrefManager::Get()->GetSoftTokenEnabled()) {
    AbortTransaction(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  if (!mTokenManagerImpl) {
    mTokenManagerImpl = new U2FSoftTokenManager(U2FPrefManager::Get()->GetSoftTokenCounter());
  }

  if ((aTransactionInfo.RpIdHash().Length() != SHA256_LENGTH) ||
      (aTransactionInfo.ClientDataHash().Length() != SHA256_LENGTH)) {
    AbortTransaction(NS_ERROR_DOM_UNKNOWN_ERR);
    return;
  }

  nsTArray<uint8_t> id;
  nsTArray<uint8_t> sig;
  nsresult rv = mTokenManagerImpl->Sign(aTransactionInfo.Descriptors(),
                                        aTransactionInfo.RpIdHash(),
                                        aTransactionInfo.ClientDataHash(),
                                        id,
                                        sig);
  if (NS_FAILED(rv)) {
    AbortTransaction(rv);
    return;
  }

  Unused << mTransactionParent->SendConfirmSign(id, sig);
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
