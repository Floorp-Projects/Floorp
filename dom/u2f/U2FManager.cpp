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

NS_IMPL_ISUPPORTS(U2FManager, nsIDOMEventListener);

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
U2FManager::ClearTransaction()
{
  if (!NS_WARN_IF(mTransaction.isNothing())) {
    StopListeningForVisibilityEvents(mTransaction.ref().mParent, this);
  }

  mTransaction.reset();

  if (mChild) {
    RefPtr<U2FTransactionChild> c;
    mChild.swap(c);
    c->Send__delete__(c);
  }
}

void
U2FManager::RejectTransaction(const nsresult& aError)
{
  if (!NS_WARN_IF(mTransaction.isNothing())) {
    ErrorCode code = ConvertNSResultToErrorCode(aError);
    mTransaction.ref().mPromise.Reject(code, __func__);
  }

  ClearTransaction();
}

void
U2FManager::CancelTransaction(const nsresult& aError)
{
  if (mChild) {
    mChild->SendRequestCancel();
  }

  RejectTransaction(aError);
}

U2FManager::~U2FManager()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome()) {
    RejectTransaction(NS_ERROR_ABORT);
  }
}

void
U2FManager::GetOrCreateBackgroundActor()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mChild) {
    return;
  }

  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    MOZ_CRASH("Failed to create a PBackgroundChild actor!");
  }

  RefPtr<U2FTransactionChild> mgr(new U2FTransactionChild());
  PWebAuthnTransactionChild* constructedMgr =
    actorChild->SendPWebAuthnTransactionConstructor(mgr);

  if (NS_WARN_IF(!constructedMgr)) {
    return;
  }

  MOZ_ASSERT(constructedMgr == mgr);
  mChild = mgr.forget();
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

//static
nsresult
U2FManager::BuildTransactionHashes(const nsCString& aRpId,
                                   const nsCString& aClientDataJSON,
                                   /* out */ CryptoBuffer& aRpIdHash,
                                   /* out */ CryptoBuffer& aClientDataHash)
{
  nsresult srv;
  nsCOMPtr<nsICryptoHash> hashService =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &srv);
  if (NS_FAILED(srv)) {
    return srv;
  }

  if (!aRpIdHash.SetLength(SHA256_LENGTH, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  srv = HashCString(hashService, aRpId, aRpIdHash);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    return NS_ERROR_FAILURE;
  }

  if (!aClientDataHash.SetLength(SHA256_LENGTH, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  srv = HashCString(hashService, aClientDataJSON, aClientDataHash);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    return NS_ERROR_FAILURE;
  }

  if (MOZ_LOG_TEST(gU2FLog, LogLevel::Debug)) {
    nsString base64;
    Unused << NS_WARN_IF(NS_FAILED(aRpIdHash.ToJwkBase64(base64)));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::RpID: %s", aRpId.get()));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::Rp ID Hash (base64): %s",
              NS_ConvertUTF16toUTF8(base64).get()));

    Unused << NS_WARN_IF(NS_FAILED(aClientDataHash.ToJwkBase64(base64)));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::Client Data JSON: %s", aClientDataJSON.get()));

    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("dom::U2FManager::Client Data Hash (base64): %s",
              NS_ConvertUTF16toUTF8(base64).get()));
  }

  return NS_OK;
}

already_AddRefed<U2FPromise>
U2FManager::Register(nsPIDOMWindowInner* aParent, const nsCString& aRpId,
               const nsCString& aClientDataJSON,
               const uint32_t& aTimeoutMillis,
               const nsTArray<WebAuthnScopedCredentialDescriptor>& aExcludeList)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aParent);

  if (mTransaction.isSome()) {
    CancelTransaction(NS_ERROR_ABORT);
  }

  CryptoBuffer rpIdHash, clientDataHash;
  if (NS_FAILED(BuildTransactionHashes(aRpId, aClientDataJSON,
                                       rpIdHash, clientDataHash))) {
    return U2FPromise::CreateAndReject(ErrorCode::OTHER_ERROR, __func__).forget();
  }

  ListenForVisibilityEvents(aParent, this);

  // Always blank for U2F
  nsTArray<WebAuthnExtension> extensions;

  WebAuthnTransactionInfo info(rpIdHash,
                               clientDataHash,
                               aTimeoutMillis,
                               aExcludeList,
                               extensions);

  MOZ_ASSERT(mTransaction.isNothing());

  mTransaction = Some(U2FTransaction(aParent, Move(info), aClientDataJSON));

  GetOrCreateBackgroundActor();

  if (mChild) {
    mChild->SendRequestRegister(mTransaction.ref().mInfo);
  }

  return mTransaction.ref().mPromise.Ensure(__func__);
}

already_AddRefed<U2FPromise>
U2FManager::Sign(nsPIDOMWindowInner* aParent,
                 const nsCString& aRpId,
                 const nsCString& aClientDataJSON,
                 const uint32_t& aTimeoutMillis,
                 const nsTArray<WebAuthnScopedCredentialDescriptor>& aAllowList)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aParent);

  if (mTransaction.isSome()) {
    CancelTransaction(NS_ERROR_ABORT);
  }

  CryptoBuffer rpIdHash, clientDataHash;
  if (NS_FAILED(BuildTransactionHashes(aRpId, aClientDataJSON,
                                       rpIdHash, clientDataHash))) {
    return U2FPromise::CreateAndReject(ErrorCode::OTHER_ERROR, __func__).forget();
  }

  ListenForVisibilityEvents(aParent, this);

  // Always blank for U2F
  nsTArray<WebAuthnExtension> extensions;

  WebAuthnTransactionInfo info(rpIdHash,
                               clientDataHash,
                               aTimeoutMillis,
                               aAllowList,
                               extensions);

  MOZ_ASSERT(mTransaction.isNothing());
  mTransaction = Some(U2FTransaction(aParent, Move(info), aClientDataJSON));

  GetOrCreateBackgroundActor();

  if (mChild) {
    mChild->SendRequestSign(mTransaction.ref().mInfo);
  }

  return mTransaction.ref().mPromise.Ensure(__func__);
}

void
U2FManager::FinishRegister(nsTArray<uint8_t>& aRegBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Check for a valid transaction.
  if (mTransaction.isNothing()) {
    return;
  }

  CryptoBuffer clientDataBuf;
  if (NS_WARN_IF(!clientDataBuf.Assign(mTransaction.ref().mClientData))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  CryptoBuffer regBuf;
  if (NS_WARN_IF(!regBuf.Assign(aRegBuffer))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  nsString clientDataBase64;
  nsString registrationDataBase64;
  nsresult rvClientData = clientDataBuf.ToJwkBase64(clientDataBase64);
  nsresult rvRegistrationData = regBuf.ToJwkBase64(registrationDataBase64);

  if (NS_WARN_IF(NS_FAILED(rvClientData)) ||
      NS_WARN_IF(NS_FAILED(rvRegistrationData))) {
    RejectTransaction(NS_ERROR_ABORT);
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
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  mTransaction.ref().mPromise.Resolve(responseStr, __func__);
  ClearTransaction();
}

void
U2FManager::FinishSign(nsTArray<uint8_t>& aCredentialId,
                       nsTArray<uint8_t>& aSigBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Check for a valid transaction.
  if (mTransaction.isNothing()) {
    return;
  }

  CryptoBuffer clientDataBuf;
  if (NS_WARN_IF(!clientDataBuf.Assign(mTransaction.ref().mClientData))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  CryptoBuffer credBuf;
  if (NS_WARN_IF(!credBuf.Assign(aCredentialId))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  CryptoBuffer sigBuf;
  if (NS_WARN_IF(!sigBuf.Assign(aSigBuffer))) {
    RejectTransaction(NS_ERROR_ABORT);
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
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  SignResponse response;
  response.mKeyHandle.Construct(keyHandleBase64);
  response.mClientData.Construct(clientDataBase64);
  response.mSignatureData.Construct(signatureDataBase64);
  response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

  nsString responseStr;
  if (NS_WARN_IF(!response.ToJSON(responseStr))) {
    RejectTransaction(NS_ERROR_ABORT);
    return;
  }

  mTransaction.ref().mPromise.Resolve(responseStr, __func__);
  ClearTransaction();
}

void
U2FManager::RequestAborted(const nsresult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransaction.isSome()) {
    RejectTransaction(aError);
  }
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

    CancelTransaction(NS_ERROR_ABORT);
  }

  return NS_OK;
}

void
U2FManager::ActorDestroyed()
{
  mChild = nullptr;
}

} // namespace dom
} // namespace mozilla
