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
#include "nsIEffectiveTLDService.h"
#include "nsNetCID.h"
#include "nsNSSComponent.h"
#include "nsURLParsers.h"
#include "pk11pub.h"

using mozilla::dom::ContentChild;

namespace mozilla {
namespace dom {

// These enumerations are defined in the FIDO U2F Javascript API under the
// interface "ErrorCode" as constant integers, and thus in the U2F.webidl file.
// Any changes to these must occur in both locations.
enum class ErrorCode {
  OK = 0,
  OTHER_ERROR = 1,
  BAD_REQUEST = 2,
  CONFIGURATION_UNSUPPORTED = 3,
  DEVICE_INELIGIBLE = 4,
  TIMEOUT = 5
};

#define PREF_U2F_SOFTTOKEN_ENABLED "security.webauth.u2f_enable_softtoken"
#define PREF_U2F_USBTOKEN_ENABLED  "security.webauth.u2f_enable_usbtoken"

const nsString U2F::FinishEnrollment =
  NS_LITERAL_STRING("navigator.id.finishEnrollment");

const nsString U2F::GetAssertion =
  NS_LITERAL_STRING("navigator.id.getAssertion");

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(U2F)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(U2F)
NS_IMPL_CYCLE_COLLECTING_RELEASE(U2F)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(U2F, mParent)

static mozilla::LazyLogModule gU2FLog("fido_u2f");

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

nsresult
U2F::NSSTokenIsCompatible(const nsString& aVersionString, bool* aIsCompatible)
{
  MOZ_ASSERT(aIsCompatible);

  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(mNSSToken);
    return mNSSToken->IsCompatibleVersion(aVersionString, aIsCompatible);
  }

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);
  if (!cc->SendNSSU2FTokenIsCompatibleVersion(aVersionString, aIsCompatible)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
U2F::NSSTokenIsRegistered(CryptoBuffer& aKeyHandle, bool* aIsRegistered)
{
  MOZ_ASSERT(aIsRegistered);

  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(mNSSToken);
    return mNSSToken->IsRegistered(aKeyHandle.Elements(), aKeyHandle.Length(),
                                   aIsRegistered);
  }

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);
  if (!cc->SendNSSU2FTokenIsRegistered(aKeyHandle, aIsRegistered)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
U2F::NSSTokenRegister(CryptoBuffer& aApplication, CryptoBuffer& aChallenge,
                      CryptoBuffer& aRegistrationData)
{
  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(mNSSToken);
    uint8_t* buffer;
    uint32_t bufferlen;
    nsresult rv;
    rv = mNSSToken->Register(aApplication.Elements(), aApplication.Length(),
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

nsresult
U2F::NSSTokenSign(CryptoBuffer& aKeyHandle, CryptoBuffer& aApplication,
                  CryptoBuffer& aChallenge, CryptoBuffer& aSignatureData)
{
  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(mNSSToken);
    uint8_t* buffer;
    uint32_t bufferlen;
    nsresult rv = mNSSToken->Sign(aApplication.Elements(), aApplication.Length(),
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

nsresult
U2F::AssembleClientData(const nsAString& aTyp,
                        const nsAString& aChallenge,
                        CryptoBuffer& aClientData) const
{
  ClientData clientDataObject;
  clientDataObject.mTyp.Construct(aTyp); // "Typ" from the U2F specification
  clientDataObject.mChallenge.Construct(aChallenge);
  clientDataObject.mOrigin.Construct(mOrigin);

  nsAutoString json;
  if (NS_WARN_IF(!clientDataObject.ToJSON(json))) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aClientData.Assign(NS_ConvertUTF16toUTF8(json)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

bool
U2F::ValidAppID(/* in/out */ nsString& aAppId) const
{
  nsCOMPtr<nsIURLParser> urlParser =
      do_GetService(NS_STDURLPARSER_CONTRACTID);
  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);

  MOZ_ASSERT(urlParser);
  MOZ_ASSERT(tldService);

  uint32_t facetSchemePos;
  int32_t facetSchemeLen;
  uint32_t facetAuthPos;
  int32_t facetAuthLen;
  // Facet is the specification's way of referring to the web origin.
  nsAutoCString facetUrl = NS_ConvertUTF16toUTF8(mOrigin);
  nsresult rv = urlParser->ParseURL(facetUrl.get(), mOrigin.Length(),
                                    &facetSchemePos, &facetSchemeLen,
                                    &facetAuthPos, &facetAuthLen,
                                    nullptr, nullptr);      // ignore path
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsAutoCString facetScheme(Substring(facetUrl, facetSchemePos, facetSchemeLen));
  nsAutoCString facetAuth(Substring(facetUrl, facetAuthPos, facetAuthLen));

  uint32_t appIdSchemePos;
  int32_t appIdSchemeLen;
  uint32_t appIdAuthPos;
  int32_t appIdAuthLen;
  nsAutoCString appIdUrl = NS_ConvertUTF16toUTF8(aAppId);
  rv = urlParser->ParseURL(appIdUrl.get(), aAppId.Length(),
                           &appIdSchemePos, &appIdSchemeLen,
                           &appIdAuthPos, &appIdAuthLen,
                           nullptr, nullptr);      // ignore path
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsAutoCString appIdScheme(Substring(appIdUrl, appIdSchemePos, appIdSchemeLen));
  nsAutoCString appIdAuth(Substring(appIdUrl, appIdAuthPos, appIdAuthLen));

  // If the facetId (origin) is not HTTPS, reject
  if (!facetScheme.LowerCaseEqualsLiteral("https")) {
    return false;
  }

  // If the appId is empty or null, overwrite it with the facetId and accept
  if (aAppId.IsEmpty() || aAppId.EqualsLiteral("null")) {
    aAppId.Assign(mOrigin);
    return true;
  }

  // if the appId URL is not HTTPS, reject.
  if (!appIdScheme.LowerCaseEqualsLiteral("https")) {
    return false;
  }

  // If the facetId and the appId auths match, accept
  if (facetAuth == appIdAuth) {
    return true;
  }

  // TODO(Bug 1244959) Implement the remaining algorithm.
  return false;
}

template <class CB, class Rsp>
void
SendError(CB& aCallback, ErrorCode aErrorCode)
{
  Rsp response;
  response.mErrorCode.Construct(static_cast<uint32_t>(aErrorCode));

  ErrorResult rv;
  aCallback.Call(response, rv);
  NS_WARN_IF(rv.Failed());
  // Useful exceptions already got reported.
  rv.SuppressException();
}

void
U2F::Register(const nsAString& aAppId,
              const Sequence<RegisterRequest>& aRegisterRequests,
              const Sequence<RegisteredKey>& aRegisteredKeys,
              U2FRegisterCallback& aCallback,
              const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
              ErrorResult& aRv)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                     ErrorCode::OTHER_ERROR);
    return;
  }

  const bool softTokenEnabled =
    Preferences::GetBool(PREF_U2F_SOFTTOKEN_ENABLED);

  const bool usbTokenEnabled =
    Preferences::GetBool(PREF_U2F_USBTOKEN_ENABLED);

  nsAutoString appId(aAppId);

  // Verify the global appId first.
  if (!ValidAppID(appId)) {
    SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                     ErrorCode::BAD_REQUEST);
    return;
  }

  for (size_t i = 0; i < aRegisteredKeys.Length(); ++i) {
    RegisteredKey request(aRegisteredKeys[i]);

    // Check for required attributes
    if (!(request.mKeyHandle.WasPassed() &&
          request.mVersion.WasPassed())) {
      continue;
    }

    // Verify the appId for this Registered Key, if set
    if (request.mAppId.WasPassed() &&
        !ValidAppID(request.mAppId.Value())) {
      continue;
    }

    // Decode the key handle
    CryptoBuffer keyHandle;
    nsresult rv = keyHandle.FromJwkBase64(request.mKeyHandle.Value());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                       ErrorCode::BAD_REQUEST);
      return;
    }

    // We ignore mTransports, as it is intended to be used for sorting the
    // available devices by preference, but is not an exclusion factor.

    bool isCompatible = false;
    bool isRegistered = false;

    // Determine if the provided keyHandle is registered at any device. If so,
    // then we'll return DEVICE_INELIGIBLE to signify we're already registered.
    if (usbTokenEnabled &&
        mUSBToken.IsCompatibleVersion(request.mVersion.Value()) &&
        mUSBToken.IsRegistered(keyHandle)) {
      SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                  ErrorCode::DEVICE_INELIGIBLE);
      return;
    }
    if (softTokenEnabled) {
      rv = NSSTokenIsCompatible(request.mVersion.Value(), &isCompatible);
      if (NS_FAILED(rv)) {
        SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                         ErrorCode::OTHER_ERROR);
        return;
      }

      rv = NSSTokenIsRegistered(keyHandle, &isRegistered);
      if (NS_FAILED(rv)) {
        SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                         ErrorCode::OTHER_ERROR);
        return;
      }

      if (isCompatible && isRegistered) {
        SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                         ErrorCode::DEVICE_INELIGIBLE);
        return;
      }
    }
  }

  // Search the requests in order for the first some token can fulfill
  for (size_t i = 0; i < aRegisterRequests.Length(); ++i) {
    RegisterRequest request(aRegisterRequests[i]);

    // Check for equired attributes
    if (!(request.mVersion.WasPassed() &&
        request.mChallenge.WasPassed())) {
      continue;
    }

    CryptoBuffer clientData;
    nsresult rv = AssembleClientData(FinishEnrollment,
                                     request.mChallenge.Value(),
                                     clientData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                       ErrorCode::OTHER_ERROR);
      return;
    }

    // Hash the AppID and the ClientData into the AppParam and ChallengeParam
    SECStatus srv;
    nsCString cAppId = NS_ConvertUTF16toUTF8(appId);
    CryptoBuffer appParam;
    CryptoBuffer challengeParam;
    if (!appParam.SetLength(SHA256_LENGTH, fallible) ||
        !challengeParam.SetLength(SHA256_LENGTH, fallible)) {
      SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                       ErrorCode::OTHER_ERROR);
      return;
    }

    srv = PK11_HashBuf(SEC_OID_SHA256, appParam.Elements(),
                       reinterpret_cast<const uint8_t*>(cAppId.BeginReading()),
                       cAppId.Length());
    if (srv != SECSuccess) {
      SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                       ErrorCode::OTHER_ERROR);
      return;
    }

    srv = PK11_HashBuf(SEC_OID_SHA256, challengeParam.Elements(),
                       clientData.Elements(), clientData.Length());
    if (srv != SECSuccess) {
      SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                       ErrorCode::OTHER_ERROR);
      return;
    }

    // Get the registration data from the token
    CryptoBuffer registrationData;
    bool registerSuccess = false;
    bool isCompatible = false;
    if (usbTokenEnabled) {
      // TODO: Implement in Bug 1245527
      SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                       ErrorCode::OTHER_ERROR);
      return;
    }

    if (!registerSuccess && softTokenEnabled) {
      rv = NSSTokenIsCompatible(request.mVersion.Value(), &isCompatible);
      if (NS_FAILED(rv)) {
        SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                        ErrorCode::OTHER_ERROR);
        return;
      }

      if (isCompatible) {
        rv = NSSTokenRegister(appParam, challengeParam, registrationData);
        if (NS_FAILED(rv)) {
          SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                        ErrorCode::OTHER_ERROR);
          return;
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
      registrationData.ToJwkBase64(registrationDataBase64);
    if (NS_WARN_IF(NS_FAILED(rvClientData)) ||
        NS_WARN_IF(NS_FAILED(rvRegistrationData))) {
      SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                       ErrorCode::OTHER_ERROR);
      return;
    }

    RegisterResponse response;
    response.mClientData.Construct(clientDataBase64);
    response.mRegistrationData.Construct(registrationDataBase64);
    response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

    ErrorResult result;
    aCallback.Call(response, result);
    NS_WARN_IF(result.Failed());
    // Useful exceptions already got reported.
    result.SuppressException();
    return;
  }

  // Nothing could satisfy
  SendError<U2FRegisterCallback, RegisterResponse>(aCallback,
                                                   ErrorCode::BAD_REQUEST);
  return;
}

void
U2F::Sign(const nsAString& aAppId,
          const nsAString& aChallenge,
          const Sequence<RegisteredKey>& aRegisteredKeys,
          U2FSignCallback& aCallback,
          const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
          ErrorResult& aRv)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    SendError<U2FSignCallback, SignResponse>(aCallback,
                                             ErrorCode::OTHER_ERROR);
    return;
  }

  const bool softTokenEnabled =
    Preferences::GetBool(PREF_U2F_SOFTTOKEN_ENABLED);

  const bool usbTokenEnabled =
    Preferences::GetBool(PREF_U2F_USBTOKEN_ENABLED);

  nsAutoString appId(aAppId);

  // Verify the global appId first.
  if (!ValidAppID(appId)) {
    SendError<U2FSignCallback, SignResponse>(aCallback,
                                             ErrorCode::BAD_REQUEST);
    return;
  }

  // Search the requests for one a token can fulfill
  for (size_t i = 0; i < aRegisteredKeys.Length(); i += 1) {
    RegisteredKey request(aRegisteredKeys[i]);

    // Check for required attributes
    if (!(request.mVersion.WasPassed() &&
          request.mKeyHandle.WasPassed())) {
      SendError<U2FSignCallback, SignResponse>(aCallback,
                                               ErrorCode::OTHER_ERROR);
      continue;
    }

    // Allow an individual RegisteredKey to assert a different AppID
    nsAutoString regKeyAppId(appId);
    if (request.mAppId.WasPassed()) {
      regKeyAppId.Assign(request.mAppId.Value());
      if (!ValidAppID(regKeyAppId)) {
        continue;
      }
    }

    // Assemble a clientData object
    CryptoBuffer clientData;
    nsresult rv = AssembleClientData(GetAssertion, aChallenge, clientData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      SendError<U2FSignCallback, SignResponse>(aCallback,
                                               ErrorCode::OTHER_ERROR);
      return;
    }

    // Hash the AppID and the ClientData into the AppParam and ChallengeParam
    SECStatus srv;
    nsCString cAppId = NS_ConvertUTF16toUTF8(regKeyAppId);
    CryptoBuffer appParam;
    CryptoBuffer challengeParam;
    if (!appParam.SetLength(SHA256_LENGTH, fallible) ||
        !challengeParam.SetLength(SHA256_LENGTH, fallible)) {
      SendError<U2FSignCallback, SignResponse>(aCallback,
                                               ErrorCode::OTHER_ERROR);
      return;
    }

    srv = PK11_HashBuf(SEC_OID_SHA256, appParam.Elements(),
                       reinterpret_cast<const uint8_t*>(cAppId.BeginReading()),
                       cAppId.Length());
    if (srv != SECSuccess) {
      SendError<U2FSignCallback, SignResponse>(aCallback,
                                               ErrorCode::OTHER_ERROR);
      return;
    }

    srv = PK11_HashBuf(SEC_OID_SHA256, challengeParam.Elements(),
                       clientData.Elements(), clientData.Length());
    if (srv != SECSuccess) {
      SendError<U2FSignCallback, SignResponse>(aCallback,
                                               ErrorCode::OTHER_ERROR);
      return;
    }

    // Decode the key handle
    CryptoBuffer keyHandle;
    rv = keyHandle.FromJwkBase64(request.mKeyHandle.Value());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      SendError<U2FSignCallback, SignResponse>(aCallback,
                                               ErrorCode::OTHER_ERROR);
      return;
    }

    // Get the signature from the token
    CryptoBuffer signatureData;
    bool signSuccess = false;

    // We ignore mTransports, as it is intended to be used for sorting the
    // available devices by preference, but is not an exclusion factor.

    if (usbTokenEnabled &&
        mUSBToken.IsCompatibleVersion(request.mVersion.Value())) {
      // TODO: Implement in Bug 1245527
      SendError<U2FSignCallback, SignResponse>(aCallback,
                                               ErrorCode::OTHER_ERROR);
      return;
    }

    if (!signSuccess && softTokenEnabled) {
      bool isCompatible = false;
      bool isRegistered = false;

      rv = NSSTokenIsCompatible(request.mVersion.Value(), &isCompatible);
      if (NS_FAILED(rv)) {
        SendError<U2FSignCallback, SignResponse>(aCallback,
                                                 ErrorCode::OTHER_ERROR);
        return;
      }

      rv = NSSTokenIsRegistered(keyHandle, &isRegistered);
      if (NS_FAILED(rv)) {
        SendError<U2FSignCallback, SignResponse>(aCallback,
                                                 ErrorCode::OTHER_ERROR);
        return;
      }

      if (isCompatible && isRegistered) {
        rv = NSSTokenSign(keyHandle, appParam, challengeParam, signatureData);
        if (NS_FAILED(rv)) {
          SendError<U2FSignCallback, SignResponse>(aCallback,
                                                   ErrorCode::OTHER_ERROR);
          return;
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
      SendError<U2FSignCallback, SignResponse>(aCallback,
                                               ErrorCode::OTHER_ERROR);
      return;
    }
    SignResponse response;
    response.mKeyHandle.Construct(request.mKeyHandle.Value());
    response.mClientData.Construct(clientDataBase64);
    response.mSignatureData.Construct(signatureDataBase64);
    response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

    ErrorResult result;
    aCallback.Call(response, result);
    NS_WARN_IF(result.Failed());
    // Useful exceptions already got reported.
    result.SuppressException();
    return;
  }

  // Nothing could satisfy
  SendError<U2FSignCallback, SignResponse>(aCallback,
                                           ErrorCode::DEVICE_INELIGIBLE);
  return;
}

} // namespace dom
} // namespace mozilla
