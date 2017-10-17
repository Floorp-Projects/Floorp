/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "hasht.h"
#include "nsICryptoHash.h"
#include "nsNetCID.h"
#include "U2FManager.h"
#include "U2FTransactionChild.h"
#include "U2FUtil.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PWebAuthnTransaction.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/BackgroundChild.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

/***********************************************************************
 * Statics
 **********************************************************************/

namespace {
StaticRefPtr<U2FManager> gU2FManager;
static mozilla::LazyLogModule gU2FManagerLog("u2fmanager");
}

NS_NAMED_LITERAL_STRING(kVisibilityChange, "visibilitychange");

NS_IMPL_ISUPPORTS(U2FManager, nsIIPCBackgroundChildCreateCallback,
                  nsIDOMEventListener);

/***********************************************************************
 * Utility Functions
 **********************************************************************/

static void
ListenForVisibilityEvents(nsPIDOMWindowInner* aParent,
                          U2FManager* aListener)
{
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aListener);

  nsCOMPtr<nsIDocument> doc = aParent->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  nsresult rv = doc->AddSystemEventListener(kVisibilityChange, aListener,
                                            /* use capture */ true,
                                            /* wants untrusted */ false);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

static void
StopListeningForVisibilityEvents(nsPIDOMWindowInner* aParent,
                                 U2FManager* aListener)
{
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aListener);

  nsCOMPtr<nsIDocument> doc = aParent->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  nsresult rv = doc->RemoveSystemEventListener(kVisibilityChange, aListener,
                                               /* use capture */ true);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

static ErrorCode
ConvertNSResultToErrorCode(const nsresult& aError)
{
  if (aError == NS_ERROR_DOM_TIMEOUT_ERR) {
    return ErrorCode::TIMEOUT;
  }
  /* Emitted by U2F{Soft,HID}TokenManager when we really mean ineligible */
  if (aError == NS_ERROR_DOM_NOT_ALLOWED_ERR) {
    return ErrorCode::DEVICE_INELIGIBLE;
  }
  return ErrorCode::OTHER_ERROR;
}

/***********************************************************************
 * U2FManager Implementation
 **********************************************************************/

U2FManager::U2FManager()
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
U2FManager::MaybeClearTransaction()
{
  mClientData.reset();
  mInfo.reset();
  mTransactionPromise.RejectIfExists(ErrorCode::OTHER_ERROR, __func__);
  if (mCurrentParent) {
    StopListeningForVisibilityEvents(mCurrentParent, this);
    mCurrentParent = nullptr;
  }

  if (mChild) {
    RefPtr<U2FTransactionChild> c;
    mChild.swap(c);
    c->Send__delete__(c);
  }
}

U2FManager::~U2FManager()
{
  MaybeClearTransaction();
}

RefPtr<U2FManager::BackgroundActorPromise>
U2FManager::GetOrCreateBackgroundActor()
{
  MOZ_ASSERT(NS_IsMainThread());

  PBackgroundChild *actor = BackgroundChild::GetForCurrentThread();
  RefPtr<U2FManager::BackgroundActorPromise> promise =
    mPBackgroundCreationPromise.Ensure(__func__);

  if (actor) {
    ActorCreated(actor);
  } else {
    bool ok = BackgroundChild::GetOrCreateForCurrentThread(this);
    if (NS_WARN_IF(!ok)) {
      ActorFailed();
    }
  }

  return promise;
}

//static
U2FManager*
U2FManager::GetOrCreate()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (gU2FManager) {
    return gU2FManager;
  }

  gU2FManager = new U2FManager();
  ClearOnShutdown(&gU2FManager);
  return gU2FManager;
}

//static
U2FManager*
U2FManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());
  return gU2FManager;
}

nsresult
U2FManager::PopulateTransactionInfo(const nsCString& aRpId,
                      const nsCString& aClientDataJSON,
                      const uint32_t& aTimeoutMillis,
                      const nsTArray<WebAuthnScopedCredentialDescriptor>& aList)
{
  MOZ_ASSERT(mInfo.isNothing());

  nsresult srv;
  nsCOMPtr<nsICryptoHash> hashService =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &srv);
  if (NS_FAILED(srv)) {
    return srv;
  }

  CryptoBuffer rpIdHash;
  if (!rpIdHash.SetLength(SHA256_LENGTH, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  srv = HashCString(hashService, aRpId, rpIdHash);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    return NS_ERROR_FAILURE;
  }

  CryptoBuffer clientDataHash;
  if (!clientDataHash.SetLength(SHA256_LENGTH, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  srv = HashCString(hashService, aClientDataJSON, clientDataHash);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    return NS_ERROR_FAILURE;
  }

  if (MOZ_LOG_TEST(gU2FLog, LogLevel::Debug)) {
    nsString base64;
    Unused << NS_WARN_IF(NS_FAILED(rpIdHash.ToJwkBase64(base64)));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::RpID: %s", aRpId.get()));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::Rp ID Hash (base64): %s",
              NS_ConvertUTF16toUTF8(base64).get()));

    Unused << NS_WARN_IF(NS_FAILED(clientDataHash.ToJwkBase64(base64)));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::Client Data JSON: %s", aClientDataJSON.get()));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::Client Data Hash (base64): %s",
              NS_ConvertUTF16toUTF8(base64).get()));
  }

  // Always blank for U2F
  nsTArray<WebAuthnExtension> extensions;

  WebAuthnTransactionInfo info(rpIdHash,
                               clientDataHash,
                               aTimeoutMillis,
                               aList,
                               extensions);
  mInfo = Some(info);
  return NS_OK;
}


already_AddRefed<U2FPromise>
U2FManager::Register(nsPIDOMWindowInner* aParent, const nsCString& aRpId,
               const nsCString& aClientDataJSON,
               const uint32_t& aTimeoutMillis,
               const nsTArray<WebAuthnScopedCredentialDescriptor>& aExcludeList)
{
  MOZ_ASSERT(aParent);

  MaybeClearTransaction();

  if (NS_FAILED(PopulateTransactionInfo(aRpId, aClientDataJSON, aTimeoutMillis,
                                        aExcludeList))) {
    return U2FPromise::CreateAndReject(ErrorCode::OTHER_ERROR, __func__).forget();
  }

  RefPtr<MozPromise<nsresult, nsresult, false>> p = GetOrCreateBackgroundActor();
  p->Then(GetMainThreadSerialEventTarget(), __func__,
          []() {
            U2FManager* mgr = U2FManager::Get();
            if (mgr && mgr->mChild) {
              mgr->mChild->SendRequestRegister(mgr->mInfo.ref());
            }
          },
          []() {
            // This case can't actually happen, we'll have crashed if the child
            // failed to create.
          });

  // Only store off the promise if we've succeeded in sending the IPC event.
  RefPtr<U2FPromise> promise = mTransactionPromise.Ensure(__func__);
  mClientData = Some(aClientDataJSON);
  mCurrentParent = aParent;
  ListenForVisibilityEvents(aParent, this);
  return promise.forget();
}

already_AddRefed<U2FPromise>
U2FManager::Sign(nsPIDOMWindowInner* aParent,
                 const nsCString& aRpId,
                 const nsCString& aClientDataJSON,
                 const uint32_t& aTimeoutMillis,
                 const nsTArray<WebAuthnScopedCredentialDescriptor>& aAllowList)
{
  MOZ_ASSERT(aParent);

  MaybeClearTransaction();

  if (NS_FAILED(PopulateTransactionInfo(aRpId, aClientDataJSON, aTimeoutMillis,
                                        aAllowList))) {
    return U2FPromise::CreateAndReject(ErrorCode::OTHER_ERROR, __func__).forget();
  }

  RefPtr<MozPromise<nsresult, nsresult, false>> p = GetOrCreateBackgroundActor();
  p->Then(GetMainThreadSerialEventTarget(), __func__,
          []() {
            U2FManager* mgr = U2FManager::Get();
            if (mgr && mgr->mChild) {
              mgr->mChild->SendRequestSign(mgr->mInfo.ref());
            }
          },
          []() {
            // This case can't actually happen, we'll have crashed if the child
            // failed to create.
          });

  // Only store off the promise if we've succeeded in sending the IPC event.
  RefPtr<U2FPromise> promise = mTransactionPromise.Ensure(__func__);
  mClientData = Some(aClientDataJSON);
  mCurrentParent = aParent;
  ListenForVisibilityEvents(aParent, this);
  return promise.forget();
}

void
U2FManager::FinishRegister(nsTArray<uint8_t>& aRegBuffer)
{
  MOZ_ASSERT(!mTransactionPromise.IsEmpty());
  MOZ_ASSERT(mInfo.isSome());

  CryptoBuffer clientDataBuf;
  if (NS_WARN_IF(!clientDataBuf.Assign(mClientData.ref()))) {
    mTransactionPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return;
  }

  CryptoBuffer regBuf;
  if (NS_WARN_IF(!regBuf.Assign(aRegBuffer))) {
    mTransactionPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return;
  }

  nsString clientDataBase64;
  nsString registrationDataBase64;
  nsresult rvClientData = clientDataBuf.ToJwkBase64(clientDataBase64);
  nsresult rvRegistrationData = regBuf.ToJwkBase64(registrationDataBase64);

  if (NS_WARN_IF(NS_FAILED(rvClientData)) ||
      NS_WARN_IF(NS_FAILED(rvRegistrationData))) {
    mTransactionPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return;
  }

  // Assemble a response object to return
  RegisterResponse response;
  response.mVersion.Construct(kRequiredU2FVersion);
  response.mClientData.Construct(clientDataBase64);
  response.mRegistrationData.Construct(registrationDataBase64);
  response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

  nsString responseStr;
  if (NS_WARN_IF(!response.ToJSON(responseStr))) {
    mTransactionPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return;
  }

  mTransactionPromise.Resolve(responseStr, __func__);
  MaybeClearTransaction();
}

void
U2FManager::FinishSign(nsTArray<uint8_t>& aCredentialId,
                       nsTArray<uint8_t>& aSigBuffer)
{
  MOZ_ASSERT(!mTransactionPromise.IsEmpty());
  MOZ_ASSERT(mInfo.isSome());

  CryptoBuffer clientDataBuf;
  if (NS_WARN_IF(!clientDataBuf.Assign(mClientData.ref()))) {
    mTransactionPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return;
  }

  CryptoBuffer credBuf;
  if (NS_WARN_IF(!credBuf.Assign(aCredentialId))) {
    mTransactionPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return;
  }

  CryptoBuffer sigBuf;
  if (NS_WARN_IF(!sigBuf.Assign(aSigBuffer))) {
    mTransactionPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return;
  }

  // Assemble a response object to return
  nsString clientDataBase64;
  nsString signatureDataBase64;
  nsString keyHandleBase64;
  nsresult rvClientData = clientDataBuf.ToJwkBase64(clientDataBase64);
  nsresult rvSignatureData = sigBuf.ToJwkBase64(signatureDataBase64);
  nsresult rvKeyHandle = credBuf.ToJwkBase64(keyHandleBase64);
  if (NS_WARN_IF(NS_FAILED(rvClientData)) ||
      NS_WARN_IF(NS_FAILED(rvSignatureData) ||
      NS_WARN_IF(NS_FAILED(rvKeyHandle)))) {
    mTransactionPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return;
  }

  SignResponse response;
  response.mKeyHandle.Construct(keyHandleBase64);
  response.mClientData.Construct(clientDataBase64);
  response.mSignatureData.Construct(signatureDataBase64);
  response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

  nsString responseStr;
  if (NS_WARN_IF(!response.ToJSON(responseStr))) {
    mTransactionPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return;
  }

  mTransactionPromise.Resolve(responseStr, __func__);
  MaybeClearTransaction();
}

void
U2FManager::RequestAborted(const nsresult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  Cancel(aError);
}

void
U2FManager::Cancel(const nsresult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  ErrorCode code = ConvertNSResultToErrorCode(aError);

  if (!mTransactionPromise.IsEmpty()) {
    mTransactionPromise.RejectIfExists(code, __func__);
  }

  MaybeClearTransaction();
}

NS_IMETHODIMP
U2FManager::HandleEvent(nsIDOMEvent* aEvent)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aEvent);
  MOZ_ASSERT(mChild);

  nsAutoString type;
  aEvent->GetType(type);
  if (!type.Equals(kVisibilityChange)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc =
    do_QueryInterface(aEvent->InternalDOMEvent()->GetTarget());
  MOZ_ASSERT(doc);

  if (doc && doc->Hidden()) {
    MOZ_LOG(gU2FManagerLog, LogLevel::Debug,
            ("Visibility change: U2F window is hidden, cancelling job."));

    mChild->SendRequestCancel();

    Cancel(NS_ERROR_DOM_TIMEOUT_ERR);
  }

  return NS_OK;
}

void
U2FManager::ActorCreated(PBackgroundChild* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  if (mChild) {
    return;
  }

  RefPtr<U2FTransactionChild> mgr(new U2FTransactionChild());
  PWebAuthnTransactionChild* constructedMgr =
    aActor->SendPWebAuthnTransactionConstructor(mgr);

  if (NS_WARN_IF(!constructedMgr)) {
    ActorFailed();
    return;
  }
  MOZ_ASSERT(constructedMgr == mgr);
  mChild = mgr.forget();
  mPBackgroundCreationPromise.Resolve(NS_OK, __func__);
}

void
U2FManager::ActorDestroyed()
{
  mChild = nullptr;
}

void
U2FManager::ActorFailed()
{
  MOZ_CRASH("We shouldn't be here!");
}

} // namespace dom
} // namespace mozilla
