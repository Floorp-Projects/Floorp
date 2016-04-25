/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "hasht.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/U2F.h"
#include "mozilla/dom/U2FBinding.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsNetCID.h"
#include "nsNSSComponent.h"
#include "nsURLParsers.h"
#include "pk11pub.h"

using mozilla::dom::ContentChild;

namespace mozilla {
namespace dom {

#define PREF_U2F_SOFTTOKEN_ENABLED "security.webauth.u2f_enable_softtoken"
#define PREF_U2F_USBTOKEN_ENABLED  "security.webauth.u2f_enable_usbtoken"

NS_NAMED_LITERAL_STRING(kFinishEnrollment, "navigator.id.finishEnrollment");
NS_NAMED_LITERAL_STRING(kGetAssertion, "navigator.id.getAssertion");

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(U2F)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(U2F)
NS_IMPL_CYCLE_COLLECTING_RELEASE(U2F)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(U2F, mParent)

static mozilla::LazyLogModule gU2FLog("webauth_u2f");

template <class CB, class Rsp>
void
SendError(CB* aCallback, ErrorCode aErrorCode)
{
  Rsp response;
  response.mErrorCode.Construct(static_cast<uint32_t>(aErrorCode));

  ErrorResult rv;
  aCallback->Call(response, rv);
  NS_WARN_IF(rv.Failed());
  // Useful exceptions already got reported.
  rv.SuppressException();
}

static nsresult
AssembleClientData(const nsAString& aOrigin, const nsAString& aTyp,
                   const nsAString& aChallenge, CryptoBuffer& aClientData)
{
  ClientData clientDataObject;
  clientDataObject.mTyp.Construct(aTyp); // "Typ" from the U2F specification
  clientDataObject.mChallenge.Construct(aChallenge);
  clientDataObject.mOrigin.Construct(aOrigin);

  nsAutoString json;
  if (NS_WARN_IF(!clientDataObject.ToJSON(json))) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aClientData.Assign(NS_ConvertUTF16toUTF8(json)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static nsresult
NSSTokenIsCompatible(nsINSSU2FToken* aNSSToken, const nsString& aVersionString,
                     bool* aIsCompatible)
{
  MOZ_ASSERT(aIsCompatible);

  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(aNSSToken);
    return aNSSToken->IsCompatibleVersion(aVersionString, aIsCompatible);
  }

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);
  if (!cc->SendNSSU2FTokenIsCompatibleVersion(aVersionString, aIsCompatible)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

static nsresult
NSSTokenIsRegistered(nsINSSU2FToken* aNSSToken, CryptoBuffer& aKeyHandle,
                     bool* aIsRegistered)
{
  MOZ_ASSERT(aIsRegistered);

  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(aNSSToken);
    return aNSSToken->IsRegistered(aKeyHandle.Elements(), aKeyHandle.Length(),
                                   aIsRegistered);
  }

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);
  if (!cc->SendNSSU2FTokenIsRegistered(aKeyHandle, aIsRegistered)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

static nsresult
NSSTokenSign(nsINSSU2FToken* aNSSToken, CryptoBuffer& aKeyHandle,
             CryptoBuffer& aApplication, CryptoBuffer& aChallenge,
             CryptoBuffer& aSignatureData)
{
  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(aNSSToken);
    uint8_t* buffer;
    uint32_t bufferlen;
    nsresult rv = aNSSToken->Sign(aApplication.Elements(), aApplication.Length(),
                                  aChallenge.Elements(), aChallenge.Length(),
                                  aKeyHandle.Elements(), aKeyHandle.Length(),
                                  &buffer, &bufferlen);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(buffer);
    aSignatureData.Assign(buffer, bufferlen);
    free(buffer);
    return NS_OK;
  }

  nsTArray<uint8_t> signatureBuffer;
  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);
  if (!cc->SendNSSU2FTokenSign(aApplication, aChallenge, aKeyHandle,
                               &signatureBuffer)) {
    return NS_ERROR_FAILURE;
  }

  aSignatureData.Assign(signatureBuffer);
  return NS_OK;
}

static nsresult
NSSTokenRegister(nsINSSU2FToken* aNSSToken, CryptoBuffer& aApplication,
                 CryptoBuffer& aChallenge, CryptoBuffer& aRegistrationData)
{
  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(aNSSToken);
    uint8_t* buffer;
    uint32_t bufferlen;
    nsresult rv;
    rv = aNSSToken->Register(aApplication.Elements(), aApplication.Length(),
                             aChallenge.Elements(), aChallenge.Length(),
                             &buffer, &bufferlen);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(buffer);
    aRegistrationData.Assign(buffer, bufferlen);
    free(buffer);
    return NS_OK;
  }

  nsTArray<uint8_t> registrationBuffer;
  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);
  if (!cc->SendNSSU2FTokenRegister(aApplication, aChallenge,
                                   &registrationBuffer)) {
    return NS_ERROR_FAILURE;
  }

  aRegistrationData.Assign(registrationBuffer);
  return NS_OK;
}

U2FTask::U2FTask(const nsAString& aOrigin, const nsAString& aAppId)
  : mOrigin(aOrigin)
  , mAppId(aAppId)
{}

U2FTask::~U2FTask()
{}

U2FRegisterTask::U2FRegisterTask(const nsAString& aOrigin,
                                 const nsAString& aAppId,
                                 const Sequence<RegisterRequest>& aRegisterRequests,
                                 const Sequence<RegisteredKey>& aRegisteredKeys,
                                 U2FRegisterCallback* aCallback,
                                 const nsCOMPtr<nsINSSU2FToken>& aNSSToken)
  : U2FTask(aOrigin, aAppId)
  , mRegisterRequests(aRegisterRequests)
  , mRegisteredKeys(aRegisteredKeys)
  , mCallback(aCallback)
  , mNSSToken(aNSSToken)
{}

U2FRegisterTask::~U2FRegisterTask()
{
  nsNSSShutDownPreventionLock locker;

  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(calledFromObject);
}

void
U2FRegisterTask::ReturnError(ErrorCode aCode)
{
  SendError<U2FRegisterCallback, RegisterResponse>(mCallback.get(), aCode);
}

NS_IMETHODIMP
U2FRegisterTask::Run()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    ReturnError(ErrorCode::OTHER_ERROR);
    return NS_ERROR_FAILURE;
  }

  // TODO: Implement USB Tokens in Bug 1245527
  const bool softTokenEnabled =
    Preferences::GetBool(PREF_U2F_SOFTTOKEN_ENABLED);

  for (size_t i = 0; i < mRegisteredKeys.Length(); ++i) {
    RegisteredKey request(mRegisteredKeys[i]);

    // Check for required attributes
    if (!(request.mKeyHandle.WasPassed() &&
          request.mVersion.WasPassed())) {
      continue;
    }

    // Do not permit an individual RegisteredKey to assert a different AppID
    if (request.mAppId.WasPassed() && mAppId != request.mAppId.Value()) {
      continue;
    }

    // Decode the key handle
    CryptoBuffer keyHandle;
    nsresult rv = keyHandle.FromJwkBase64(request.mKeyHandle.Value());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ReturnError(ErrorCode::BAD_REQUEST);
      return NS_ERROR_FAILURE;
    }

    // We ignore mTransports, as it is intended to be used for sorting the
    // available devices by preference, but is not an exclusion factor.

    bool isCompatible = false;
    bool isRegistered = false;

    // Determine if the provided keyHandle is registered at any device. If so,
    // then we'll return DEVICE_INELIGIBLE to signify we're already registered.
    if (softTokenEnabled) {
      rv = NSSTokenIsCompatible(mNSSToken, request.mVersion.Value(),
                                &isCompatible);
      if (NS_FAILED(rv)) {
        ReturnError(ErrorCode::OTHER_ERROR);
        return NS_ERROR_FAILURE;
      }

      rv = NSSTokenIsRegistered(mNSSToken, keyHandle, &isRegistered);
      if (NS_FAILED(rv)) {
        ReturnError(ErrorCode::OTHER_ERROR);
        return NS_ERROR_FAILURE;
      }

      if (isCompatible && isRegistered) {
        ReturnError(ErrorCode::DEVICE_INELIGIBLE);
        return NS_OK;
      }
    }
  }

  // Search the requests in order for the first some token can fulfill
  for (size_t i = 0; i < mRegisterRequests.Length(); ++i) {
    RegisterRequest request(mRegisterRequests[i]);

    // Check for equired attributes
    if (!(request.mVersion.WasPassed() &&
        request.mChallenge.WasPassed())) {
      continue;
    }

    CryptoBuffer clientData;
    nsresult rv = AssembleClientData(mOrigin, kFinishEnrollment,
                                     request.mChallenge.Value(),
                                     clientData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }

    // Hash the AppID and the ClientData into the AppParam and ChallengeParam
    SECStatus srv;
    nsCString cAppId = NS_ConvertUTF16toUTF8(mAppId);
    CryptoBuffer appParam;
    CryptoBuffer challengeParam;
    if (!appParam.SetLength(SHA256_LENGTH, fallible) ||
        !challengeParam.SetLength(SHA256_LENGTH, fallible)) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }

    srv = PK11_HashBuf(SEC_OID_SHA256, appParam.Elements(),
                       reinterpret_cast<const uint8_t*>(cAppId.BeginReading()),
                       cAppId.Length());
    if (srv != SECSuccess) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }

    srv = PK11_HashBuf(SEC_OID_SHA256, challengeParam.Elements(),
                       clientData.Elements(), clientData.Length());
    if (srv != SECSuccess) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }

    // Get the registration data from the token
    CryptoBuffer regData;
    bool registerSuccess = false;
    bool isCompatible = false;

    if (!registerSuccess && softTokenEnabled) {
      rv = NSSTokenIsCompatible(mNSSToken, request.mVersion.Value(),
                                &isCompatible);
      if (NS_FAILED(rv)) {
        ReturnError(ErrorCode::OTHER_ERROR);
        return NS_ERROR_FAILURE;
      }

      if (isCompatible) {
        rv = NSSTokenRegister(mNSSToken, appParam, challengeParam, regData);
        if (NS_FAILED(rv)) {
          ReturnError(ErrorCode::OTHER_ERROR);
          return NS_ERROR_FAILURE;
        }
        registerSuccess = true;
      }
    }

    if (!registerSuccess) {
      // Try another request
      continue;
    }

    // Assemble a response object to return
    nsString clientDataBase64, registrationDataBase64;
    nsresult rvClientData =
      clientData.ToJwkBase64(clientDataBase64);
    nsresult rvRegistrationData =
      regData.ToJwkBase64(registrationDataBase64);
    if (NS_WARN_IF(NS_FAILED(rvClientData)) ||
        NS_WARN_IF(NS_FAILED(rvRegistrationData))) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }

    RegisterResponse response;
    response.mClientData.Construct(clientDataBase64);
    response.mRegistrationData.Construct(registrationDataBase64);
    response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

    ErrorResult result;
    mCallback->Call(response, result);
    NS_WARN_IF(result.Failed());
    // Useful exceptions already got reported.
    result.SuppressException();
    return NS_OK;
  }

  // Nothing could satisfy
  ReturnError(ErrorCode::BAD_REQUEST);
  return NS_ERROR_FAILURE;
}

U2FSignTask::U2FSignTask(const nsAString& aOrigin,
                         const nsAString& aAppId,
                         const nsAString& aChallenge,
                         const Sequence<RegisteredKey>& aRegisteredKeys,
                         U2FSignCallback* aCallback,
                         const nsCOMPtr<nsINSSU2FToken>& aNSSToken)
  : U2FTask(aOrigin, aAppId)
  , mChallenge(aChallenge)
  , mRegisteredKeys(aRegisteredKeys)
  , mCallback(aCallback)
  , mNSSToken(aNSSToken)
{}

U2FSignTask::~U2FSignTask()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(calledFromObject);
}

void
U2FSignTask::ReturnError(ErrorCode aCode)
{
  SendError<U2FSignCallback, SignResponse>(mCallback.get(), aCode);
}

NS_IMETHODIMP
U2FSignTask::Run()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    ReturnError(ErrorCode::OTHER_ERROR);
    return NS_ERROR_FAILURE;
  }

  // TODO: Implement USB Tokens in Bug 1245527
  const bool softTokenEnabled =
    Preferences::GetBool(PREF_U2F_SOFTTOKEN_ENABLED);

  // Search the requests for one a token can fulfill
  for (size_t i = 0; i < mRegisteredKeys.Length(); i += 1) {
    RegisteredKey request(mRegisteredKeys[i]);

    // Check for required attributes
    if (!(request.mVersion.WasPassed() &&
          request.mKeyHandle.WasPassed())) {
      continue;
    }

    // Do not permit an individual RegisteredKey to assert a different AppID
    if (request.mAppId.WasPassed() && mAppId != request.mAppId.Value()) {
      continue;
    }

    // Assemble a clientData object
    CryptoBuffer clientData;
    nsresult rv = AssembleClientData(mOrigin, kGetAssertion, mChallenge,
                                     clientData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }

    // Hash the AppID and the ClientData into the AppParam and ChallengeParam
    SECStatus srv;
    nsCString cAppId = NS_ConvertUTF16toUTF8(mAppId);
    CryptoBuffer appParam;
    CryptoBuffer challengeParam;
    if (!appParam.SetLength(SHA256_LENGTH, fallible) ||
        !challengeParam.SetLength(SHA256_LENGTH, fallible)) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }

    srv = PK11_HashBuf(SEC_OID_SHA256, appParam.Elements(),
                       reinterpret_cast<const uint8_t*>(cAppId.BeginReading()),
                       cAppId.Length());
    if (srv != SECSuccess) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }

    srv = PK11_HashBuf(SEC_OID_SHA256, challengeParam.Elements(),
                       clientData.Elements(), clientData.Length());
    if (srv != SECSuccess) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }

    // Decode the key handle
    CryptoBuffer keyHandle;
    rv = keyHandle.FromJwkBase64(request.mKeyHandle.Value());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }

    // Get the signature from the token
    CryptoBuffer signatureData;
    bool signSuccess = false;

    // We ignore mTransports, as it is intended to be used for sorting the
    // available devices by preference, but is not an exclusion factor.

    if (!signSuccess && softTokenEnabled) {
      bool isCompatible = false;
      bool isRegistered = false;

      rv = NSSTokenIsCompatible(mNSSToken, request.mVersion.Value(),
                                &isCompatible);
      if (NS_FAILED(rv)) {
        ReturnError(ErrorCode::OTHER_ERROR);
        return NS_ERROR_FAILURE;
      }

      rv = NSSTokenIsRegistered(mNSSToken, keyHandle, &isRegistered);
      if (NS_FAILED(rv)) {
        ReturnError(ErrorCode::OTHER_ERROR);
        return NS_ERROR_FAILURE;
      }

      if (isCompatible && isRegistered) {
        rv = NSSTokenSign(mNSSToken, keyHandle, appParam, challengeParam,
                          signatureData);
        if (NS_FAILED(rv)) {
          ReturnError(ErrorCode::OTHER_ERROR);
          return NS_ERROR_FAILURE;
        }
        signSuccess = true;
      }
    }

    if (!signSuccess) {
      // Try another request
      continue;
    }

    // Assemble a response object to return
    nsString clientDataBase64, signatureDataBase64;
    nsresult rvClientData =
      clientData.ToJwkBase64(clientDataBase64);
    nsresult rvSignatureData =
      signatureData.ToJwkBase64(signatureDataBase64);
    if (NS_WARN_IF(NS_FAILED(rvClientData)) ||
        NS_WARN_IF(NS_FAILED(rvSignatureData))) {
      ReturnError(ErrorCode::OTHER_ERROR);
      return NS_ERROR_FAILURE;
    }
    SignResponse response;
    response.mKeyHandle.Construct(request.mKeyHandle.Value());
    response.mClientData.Construct(clientDataBase64);
    response.mSignatureData.Construct(signatureDataBase64);
    response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

    ErrorResult result;
    mCallback->Call(response, result);
    NS_WARN_IF(result.Failed());
    // Useful exceptions already got reported.
    result.SuppressException();
    return NS_OK;
  }

  // Nothing could satisfy
  ReturnError(ErrorCode::DEVICE_INELIGIBLE);
  return NS_ERROR_FAILURE;
}

// EvaluateAppIDAndRunTask determines whether the supplied FIDO AppID is valid for
// the current FacetID, e.g., the current origin.
// See https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-appid-and-facets.html
// for a description of the algorithm.
static void
EvaluateAppIDAndRunTask(U2FTask* aTask)
{
  MOZ_ASSERT(aTask);

  nsCOMPtr<nsIURLParser> urlParser =
      do_GetService(NS_STDURLPARSER_CONTRACTID);

  MOZ_ASSERT(urlParser);

  uint32_t facetSchemePos;
  int32_t facetSchemeLen;
  uint32_t facetAuthPos;
  int32_t facetAuthLen;
  // Facet is the specification's way of referring to the web origin.
  nsAutoCString facetUrl = NS_ConvertUTF16toUTF8(aTask->mOrigin);
  nsresult rv = urlParser->ParseURL(facetUrl.get(), aTask->mOrigin.Length(),
                                    &facetSchemePos, &facetSchemeLen,
                                    &facetAuthPos, &facetAuthLen,
                                    nullptr, nullptr);      // ignore path
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aTask->ReturnError(ErrorCode::BAD_REQUEST);
    return;
  }

  nsAutoCString facetScheme(Substring(facetUrl, facetSchemePos, facetSchemeLen));
  nsAutoCString facetAuth(Substring(facetUrl, facetAuthPos, facetAuthLen));

  uint32_t appIdSchemePos;
  int32_t appIdSchemeLen;
  uint32_t appIdAuthPos;
  int32_t appIdAuthLen;
  // AppID is user-supplied. It's quite possible for this parse to fail.
  nsAutoCString appIdUrl = NS_ConvertUTF16toUTF8(aTask->mAppId);
  rv = urlParser->ParseURL(appIdUrl.get(), aTask->mAppId.Length(),
                           &appIdSchemePos, &appIdSchemeLen,
                           &appIdAuthPos, &appIdAuthLen,
                           nullptr, nullptr);      // ignore path
  if (NS_FAILED(rv)) {
    aTask->ReturnError(ErrorCode::BAD_REQUEST);
    return;
  }

  nsAutoCString appIdScheme(Substring(appIdUrl, appIdSchemePos, appIdSchemeLen));
  nsAutoCString appIdAuth(Substring(appIdUrl, appIdAuthPos, appIdAuthLen));

  // If the facetId (origin) is not HTTPS, reject
  if (!facetScheme.LowerCaseEqualsLiteral("https")) {
    aTask->ReturnError(ErrorCode::BAD_REQUEST);
    return;
  }

  // If the appId is empty or null, overwrite it with the facetId and accept
  if (aTask->mAppId.IsEmpty() || aTask->mAppId.EqualsLiteral("null")) {
    aTask->mAppId.Assign(aTask->mOrigin);
    aTask->Run();
    return;
  }

  // if the appId URL is not HTTPS, reject.
  if (!appIdScheme.LowerCaseEqualsLiteral("https")) {
    aTask->ReturnError(ErrorCode::BAD_REQUEST);
    return;
  }

  // If the facetId and the appId auths match, accept
  if (facetAuth == appIdAuth) {
    aTask->Run();
    return;
  }

  // TODO(Bug 1244959) Implement the remaining algorithm.
  aTask->ReturnError(ErrorCode::BAD_REQUEST);
  return;
}

U2F::U2F()
{}

U2F::~U2F()
{
  nsNSSShutDownPreventionLock locker;

  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(calledFromObject);
}

/* virtual */ JSObject*
U2F::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return U2FBinding::Wrap(aCx, this, aGivenProto);
}

void
U2F::Init(nsPIDOMWindowInner* aParent, ErrorResult& aRv)
{
  MOZ_ASSERT(!mParent);
  mParent = do_QueryInterface(aParent);
  MOZ_ASSERT(mParent);

  nsCOMPtr<nsIDocument> doc = mParent->GetDoc();
  MOZ_ASSERT(doc);

  nsIPrincipal* principal = doc->NodePrincipal();
  aRv = nsContentUtils::GetUTFOrigin(principal, mOrigin);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (NS_WARN_IF(mOrigin.IsEmpty())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (!EnsureNSSInitializedChromeOrContent()) {
    MOZ_LOG(gU2FLog, LogLevel::Debug, ("Failed to get NSS context for U2F"));
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (XRE_IsParentProcess()) {
    mNSSToken = do_GetService(NS_NSSU2FTOKEN_CONTRACTID);
    if (NS_WARN_IF(!mNSSToken)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }

  aRv = mUSBToken.Init();
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

void
U2F::Register(const nsAString& aAppId,
              const Sequence<RegisterRequest>& aRegisterRequests,
              const Sequence<RegisteredKey>& aRegisteredKeys,
              U2FRegisterCallback& aCallback,
              const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
              ErrorResult& aRv)
{
  RefPtr<U2FRegisterTask> registerTask = new U2FRegisterTask(mOrigin, aAppId,
                                                             aRegisterRequests,
                                                             aRegisteredKeys,
                                                             &aCallback,
                                                             mNSSToken);

  EvaluateAppIDAndRunTask(registerTask);
}

void
U2F::Sign(const nsAString& aAppId,
          const nsAString& aChallenge,
          const Sequence<RegisteredKey>& aRegisteredKeys,
          U2FSignCallback& aCallback,
          const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
          ErrorResult& aRv)
{
  RefPtr<U2FSignTask> signTask = new U2FSignTask(mOrigin, aAppId, aChallenge,
                                                 aRegisteredKeys, &aCallback,
                                                 mNSSToken);

  EvaluateAppIDAndRunTask(signTask);
}

} // namespace dom
} // namespace mozilla
