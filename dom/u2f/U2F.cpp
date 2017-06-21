/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "hasht.h"
#include "mozilla/dom/CallbackFunction.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/NSSU2FTokenRemote.h"
#include "mozilla/dom/U2F.h"
#include "mozilla/Preferences.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/SizePrintfMacros.h"
#include "nsContentUtils.h"
#include "nsINSSU2FToken.h"
#include "nsNetCID.h"
#include "nsNSSComponent.h"
#include "nsThreadUtils.h"
#include "nsURLParsers.h"
#include "nsXPCOMCIDInternal.h"
#include "pk11pub.h"

using mozilla::dom::ContentChild;

namespace mozilla {
namespace dom {

#define PREF_U2F_SOFTTOKEN_ENABLED "security.webauth.u2f_enable_softtoken"
#define PREF_U2F_USBTOKEN_ENABLED  "security.webauth.u2f_enable_usbtoken"

NS_NAMED_LITERAL_CSTRING(kPoolName, "WebAuth_U2F-IO");
NS_NAMED_LITERAL_STRING(kFinishEnrollment, "navigator.id.finishEnrollment");
NS_NAMED_LITERAL_STRING(kGetAssertion, "navigator.id.getAssertion");

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(U2F)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(U2F)
NS_IMPL_CYCLE_COLLECTING_RELEASE(U2F)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(U2F, mParent)

static mozilla::LazyLogModule gU2FLog("u2f");

static nsresult
AssembleClientData(const nsAString& aOrigin, const nsAString& aTyp,
                   const nsAString& aChallenge, CryptoBuffer& aClientData)
{
  MOZ_ASSERT(NS_IsMainThread());
  U2FClientData clientDataObject;
  clientDataObject.mTyp.Construct(aTyp); // "Typ" from the U2F specification
  clientDataObject.mChallenge.Construct(aChallenge);
  clientDataObject.mOrigin.Construct(aOrigin);

  nsAutoString json;
  if (NS_WARN_IF(!clientDataObject.ToJSON(json))) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aClientData.Assign(NS_ConvertUTF16toUTF8(json)))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

U2FStatus::U2FStatus()
  : mCount(0)
  , mIsStopped(false)
  , mReentrantMonitor("U2FStatus")
{}

U2FStatus::~U2FStatus()
{}

void
U2FStatus::WaitGroupAdd()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  mCount += 1;
  MOZ_LOG(gU2FLog, LogLevel::Debug,
          ("U2FStatus::WaitGroupAdd, now %d", mCount));
}

void
U2FStatus::WaitGroupDone()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  MOZ_ASSERT(mCount > 0);
  mCount -= 1;
  MOZ_LOG(gU2FLog, LogLevel::Debug,
          ("U2FStatus::WaitGroupDone, now %d", mCount));
  if (mCount == 0) {
    mReentrantMonitor.NotifyAll();
  }
}

void
U2FStatus::WaitGroupWait()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  MOZ_LOG(gU2FLog, LogLevel::Debug,
          ("U2FStatus::WaitGroupWait, now %d", mCount));

  while (mCount > 0) {
    mReentrantMonitor.Wait();
  }

  MOZ_ASSERT(mCount == 0);
  MOZ_LOG(gU2FLog, LogLevel::Debug,
          ("U2FStatus::Wait completed, now count=%d stopped=%d", mCount,
           mIsStopped));
}

void
U2FStatus::Stop(const ErrorCode aErrorCode)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  MOZ_ASSERT(!mIsStopped);
  mIsStopped = true;
  mErrorCode = aErrorCode;

  // TODO: Let WaitGroupWait exit early upon a Stop. Requires consideration of
  // threads calling IsStopped() followed by WaitGroupDone(). Right now, Stop
  // prompts work tasks to end early, but it could also prompt an immediate
  // "Go ahead" to the thread waiting at WaitGroupWait.
}

void
U2FStatus::Stop(const ErrorCode aErrorCode, const nsAString& aResponse)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  Stop(aErrorCode);
  mResponse = aResponse;
}

bool
U2FStatus::IsStopped()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  return mIsStopped;
}

ErrorCode
U2FStatus::GetErrorCode()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  MOZ_ASSERT(mIsStopped);
  return mErrorCode;
}

nsString
U2FStatus::GetResponse()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  MOZ_ASSERT(mIsStopped);
  return mResponse;
}

U2FTask::U2FTask(const nsAString& aOrigin, const nsAString& aAppId,
                 const Authenticator& aAuthenticator,
                 nsISerialEventTarget* aEventTarget)
  : mOrigin(aOrigin)
  , mAppId(aAppId)
  , mAuthenticator(aAuthenticator)
  , mEventTarget(aEventTarget)
{}

U2FTask::~U2FTask()
{}

RefPtr<U2FPromise>
U2FTask::Execute()
{
  RefPtr<U2FPromise> p = mPromise.Ensure(__func__);

  nsCOMPtr<nsIRunnable> r(this);

  // TODO: Use a thread pool here, but we have to solve the PContentChild issues
  // of being in a worker thread.
  mEventTarget->Dispatch(r.forget());
  return p;
}

U2FPrepTask::U2FPrepTask(const Authenticator& aAuthenticator,
                         nsISerialEventTarget* aEventTarget)
  : mAuthenticator(aAuthenticator)
  , mEventTarget(aEventTarget)
{}

U2FPrepTask::~U2FPrepTask()
{}

RefPtr<U2FPrepPromise>
U2FPrepTask::Execute()
{
  RefPtr<U2FPrepPromise> p = mPromise.Ensure(__func__);

  nsCOMPtr<nsIRunnable> r(this);

  // TODO: Use a thread pool here, but we have to solve the PContentChild issues
  // of being in a worker thread.
  mEventTarget->Dispatch(r.forget());
  return p;
}

U2FIsRegisteredTask::U2FIsRegisteredTask(const Authenticator& aAuthenticator,
                                         const LocalRegisteredKey& aRegisteredKey,
                                         const CryptoBuffer& aAppParam,
                                         nsISerialEventTarget* aEventTarget)
  : U2FPrepTask(aAuthenticator, aEventTarget)
  , mRegisteredKey(aRegisteredKey)
  , mAppParam(aAppParam)
{}

U2FIsRegisteredTask::~U2FIsRegisteredTask()
{}

NS_IMETHODIMP
U2FIsRegisteredTask::Run()
{
  bool isCompatible = false;
  nsresult rv = mAuthenticator->IsCompatibleVersion(mRegisteredKey.mVersion,
                                                    &isCompatible);
  if (NS_FAILED(rv)) {
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_FAILURE;
  }

  if (!isCompatible) {
    mPromise.Reject(ErrorCode::BAD_REQUEST, __func__);
    return NS_ERROR_FAILURE;
  }

  // Decode the key handle
  CryptoBuffer keyHandle;
  rv = keyHandle.FromJwkBase64(mRegisteredKey.mKeyHandle);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPromise.Reject(ErrorCode::BAD_REQUEST, __func__);
    return NS_ERROR_FAILURE;
  }

  // We ignore mTransports, as it is intended to be used for sorting the
  // available devices by preference, but is not an exclusion factor.

  bool isRegistered = false;
  rv = mAuthenticator->IsRegistered(keyHandle.Elements(), keyHandle.Length(),
                                    mAppParam.Elements(), mAppParam.Length(),
                                    &isRegistered);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_FAILURE;
  }

  if (isRegistered) {
    mPromise.Reject(ErrorCode::DEVICE_INELIGIBLE, __func__);
    return NS_OK;
  }

  mPromise.Resolve(mAuthenticator, __func__);
  return NS_OK;
}

U2FRegisterTask::U2FRegisterTask(const nsAString& aOrigin,
                                 const nsAString& aAppId,
                                 const Authenticator& aAuthenticator,
                                 const CryptoBuffer& aAppParam,
                                 const CryptoBuffer& aChallengeParam,
                                 const LocalRegisterRequest& aRegisterEntry,
                                 nsISerialEventTarget* aEventTarget)
  : U2FTask(aOrigin, aAppId, aAuthenticator, aEventTarget)
  , mAppParam(aAppParam)
  , mChallengeParam(aChallengeParam)
  , mRegisterEntry(aRegisterEntry)
{}

U2FRegisterTask::~U2FRegisterTask()
{}

NS_IMETHODIMP
U2FRegisterTask::Run()
{
  bool isCompatible = false;
  nsresult rv = mAuthenticator->IsCompatibleVersion(mRegisterEntry.mVersion,
                                                    &isCompatible);
  if (NS_FAILED(rv)) {
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_FAILURE;
  }

  if (!isCompatible) {
    mPromise.Reject(ErrorCode::BAD_REQUEST, __func__);
    return NS_ERROR_FAILURE;
  }

  uint8_t* buffer;
  uint32_t bufferlen;
  rv = mAuthenticator->Register(mAppParam.Elements(),
                                mAppParam.Length(),
                                mChallengeParam.Elements(),
                                mChallengeParam.Length(),
                                &buffer, &bufferlen);
  if (NS_WARN_IF(NS_FAILED(rv)))  {
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(buffer);
  CryptoBuffer regData;
  if (NS_WARN_IF(!regData.Assign(buffer, bufferlen))) {
    free(buffer);
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  free(buffer);

  // Assemble a response object to return
  nsString clientDataBase64;
  nsString registrationDataBase64;
  nsresult rvClientData = mRegisterEntry.mClientData.ToJwkBase64(clientDataBase64);
  nsresult rvRegistrationData = regData.ToJwkBase64(registrationDataBase64);

  if (NS_WARN_IF(NS_FAILED(rvClientData)) ||
      NS_WARN_IF(NS_FAILED(rvRegistrationData))) {
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_FAILURE;
  }

  RegisterResponse response;
  response.mClientData.Construct(clientDataBase64);
  response.mRegistrationData.Construct(registrationDataBase64);
  response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

  nsString responseStr;
  if (NS_WARN_IF(!response.ToJSON(responseStr))) {
    return NS_ERROR_FAILURE;
  }
  mPromise.Resolve(responseStr, __func__);
  return NS_OK;
}

U2FSignTask::U2FSignTask(const nsAString& aOrigin,
                         const nsAString& aAppId,
                         const nsAString& aVersion,
                         const Authenticator& aAuthenticator,
                         const CryptoBuffer& aAppParam,
                         const CryptoBuffer& aChallengeParam,
                         const CryptoBuffer& aClientData,
                         const CryptoBuffer& aKeyHandle,
                         nsISerialEventTarget* aEventTarget)
  : U2FTask(aOrigin, aAppId, aAuthenticator, aEventTarget)
  , mVersion(aVersion)
  , mAppParam(aAppParam)
  , mChallengeParam(aChallengeParam)
  , mClientData(aClientData)
  , mKeyHandle(aKeyHandle)
{}

U2FSignTask::~U2FSignTask()
{}

NS_IMETHODIMP
U2FSignTask::Run()
{
  bool isCompatible = false;
  nsresult rv = mAuthenticator->IsCompatibleVersion(mVersion, &isCompatible);
  if (NS_FAILED(rv)) {
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_FAILURE;
  }

  if (!isCompatible) {
    mPromise.Reject(ErrorCode::BAD_REQUEST, __func__);
    return NS_ERROR_FAILURE;
  }

  bool isRegistered = false;
  rv = mAuthenticator->IsRegistered(mKeyHandle.Elements(), mKeyHandle.Length(),
                                    mAppParam.Elements(), mAppParam.Length(),
                                    &isRegistered);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_FAILURE;
  }

  if (!isRegistered) {
    mPromise.Reject(ErrorCode::DEVICE_INELIGIBLE, __func__);
    return NS_OK;
  }

  CryptoBuffer signatureData;
  uint8_t* buffer;
  uint32_t bufferlen;
  rv = mAuthenticator->Sign(mAppParam.Elements(), mAppParam.Length(),
                            mChallengeParam.Elements(), mChallengeParam.Length(),
                            mKeyHandle.Elements(), mKeyHandle.Length(),
                            &buffer, &bufferlen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(buffer);
  if (NS_WARN_IF(!signatureData.Assign(buffer, bufferlen))) {
    free(buffer);
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  free(buffer);

  // Assemble a response object to return
  nsString clientDataBase64;
  nsString signatureDataBase64;
  nsString keyHandleBase64;
  nsresult rvClientData = mClientData.ToJwkBase64(clientDataBase64);
  nsresult rvSignatureData = signatureData.ToJwkBase64(signatureDataBase64);
  nsresult rvKeyHandle = mKeyHandle.ToJwkBase64(keyHandleBase64);
  if (NS_WARN_IF(NS_FAILED(rvClientData)) ||
      NS_WARN_IF(NS_FAILED(rvSignatureData) ||
      NS_WARN_IF(NS_FAILED(rvKeyHandle)))) {
    mPromise.Reject(ErrorCode::OTHER_ERROR, __func__);
    return NS_ERROR_FAILURE;
  }

  SignResponse response;
  response.mKeyHandle.Construct(keyHandleBase64);
  response.mClientData.Construct(clientDataBase64);
  response.mSignatureData.Construct(signatureDataBase64);
  response.mErrorCode.Construct(static_cast<uint32_t>(ErrorCode::OK));

  nsString responseStr;
  if (NS_WARN_IF(!response.ToJSON(responseStr))) {
    return NS_ERROR_FAILURE;
  }
  mPromise.Resolve(responseStr, __func__);
  return NS_OK;
}

U2FRunnable::U2FRunnable(const nsAString& aOrigin, const nsAString& aAppId,
                         nsISerialEventTarget* aEventTarget)
  : mOrigin(aOrigin)
  , mAppId(aAppId)
  , mEventTarget(aEventTarget)
{}

U2FRunnable::~U2FRunnable()
{}

// EvaluateAppIDAndRunTask determines whether the supplied FIDO AppID is valid for
// the current FacetID, e.g., the current origin.
// See https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-appid-and-facets.html
// for a description of the algorithm.
ErrorCode
U2FRunnable::EvaluateAppID()
{
  nsCOMPtr<nsIURLParser> urlParser =
      do_GetService(NS_STDURLPARSER_CONTRACTID);

  MOZ_ASSERT(urlParser);

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
    return ErrorCode::BAD_REQUEST;
  }

  nsAutoCString facetScheme(Substring(facetUrl, facetSchemePos, facetSchemeLen));
  nsAutoCString facetAuth(Substring(facetUrl, facetAuthPos, facetAuthLen));

  uint32_t appIdSchemePos;
  int32_t appIdSchemeLen;
  uint32_t appIdAuthPos;
  int32_t appIdAuthLen;
  // AppID is user-supplied. It's quite possible for this parse to fail.
  nsAutoCString appIdUrl = NS_ConvertUTF16toUTF8(mAppId);
  rv = urlParser->ParseURL(appIdUrl.get(), mAppId.Length(),
                           &appIdSchemePos, &appIdSchemeLen,
                           &appIdAuthPos, &appIdAuthLen,
                           nullptr, nullptr);      // ignore path
  if (NS_FAILED(rv)) {
    return ErrorCode::BAD_REQUEST;
  }

  nsAutoCString appIdScheme(Substring(appIdUrl, appIdSchemePos, appIdSchemeLen));
  nsAutoCString appIdAuth(Substring(appIdUrl, appIdAuthPos, appIdAuthLen));

  // If the facetId (origin) is not HTTPS, reject
  if (!facetScheme.LowerCaseEqualsLiteral("https")) {
    return ErrorCode::BAD_REQUEST;
  }

  // If the appId is empty or null, overwrite it with the facetId and accept
  if (mAppId.IsEmpty() || mAppId.EqualsLiteral("null")) {
    mAppId.Assign(mOrigin);
    return ErrorCode::OK;
  }

  // if the appId URL is not HTTPS, reject.
  if (!appIdScheme.LowerCaseEqualsLiteral("https")) {
    return ErrorCode::BAD_REQUEST;
  }

  // If the facetId and the appId auths match, accept
  if (facetAuth == appIdAuth) {
    return ErrorCode::OK;
  }

  // TODO(Bug 1244959) Implement the remaining algorithm.
  return ErrorCode::BAD_REQUEST;
}

U2FRegisterRunnable::U2FRegisterRunnable(const nsAString& aOrigin,
                                         const nsAString& aAppId,
                                         const Sequence<RegisterRequest>& aRegisterRequests,
                                         const Sequence<RegisteredKey>& aRegisteredKeys,
                                         const Sequence<Authenticator>& aAuthenticators,
                                         U2FRegisterCallback* aCallback,
                                         nsISerialEventTarget* aEventTarget)
  : U2FRunnable(aOrigin, aAppId, aEventTarget)
  , mAuthenticators(aAuthenticators)
  // U2FRegisterCallback does not support threadsafe refcounting, and must be
  // used and destroyed on main.
  , mCallback(new nsMainThreadPtrHolder<U2FRegisterCallback>(
      "U2FRegisterRunnable::mCallback", aCallback))
{
  MOZ_ASSERT(NS_IsMainThread());

  // The WebIDL dictionary types RegisterRequest and RegisteredKey cannot
  // be copied to this thread, so store them serialized.
  for (const RegisterRequest& req : aRegisterRequests) {
    // Check for required attributes
    if (!req.mChallenge.WasPassed() || !req.mVersion.WasPassed()) {
      continue;
    }

    LocalRegisterRequest localReq;
    localReq.mVersion = req.mVersion.Value();
    localReq.mChallenge = req.mChallenge.Value();

    nsresult rv = AssembleClientData(mOrigin, kFinishEnrollment,
                                     localReq.mChallenge, localReq.mClientData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    mRegisterRequests.AppendElement(localReq);
  }

  for (const RegisteredKey& key : aRegisteredKeys) {
    // Check for required attributes
    if (!key.mVersion.WasPassed() || !key.mKeyHandle.WasPassed()) {
      continue;
    }

    LocalRegisteredKey localKey;
    localKey.mVersion = key.mVersion.Value();
    localKey.mKeyHandle = key.mKeyHandle.Value();
    if (key.mAppId.WasPassed()) {
      localKey.mAppId.SetValue(key.mAppId.Value());
    }

    mRegisteredKeys.AppendElement(localKey);
  }
}

U2FRegisterRunnable::~U2FRegisterRunnable()
{
  nsNSSShutDownPreventionLock locker;

  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(ShutdownCalledFrom::Object);
}

void
U2FRegisterRunnable::SetTimeout(const int32_t aTimeoutMillis)
{
  opt_mTimeoutSeconds.SetValue(aTimeoutMillis);
}

void
U2FRegisterRunnable::SendResponse(const RegisterResponse& aResponse)
{
  MOZ_ASSERT(NS_IsMainThread());

  ErrorResult rv;
  mCallback->Call(aResponse, rv);
  NS_WARNING_ASSERTION(!rv.Failed(), "callback failed");
  // Useful exceptions already got reported.
  rv.SuppressException();
}

NS_IMETHODIMP
U2FRegisterRunnable::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_FAILURE;
  }

  // Create a Status object to keep track of when we're done
  RefPtr<U2FStatus> status = new U2FStatus();

  // Evaluate the AppID
  ErrorCode appIdResult = EvaluateAppID();
  if (appIdResult != ErrorCode::OK) {
    status->Stop(appIdResult);
  }

  // Produce the AppParam from the current AppID
  nsCString cAppId = NS_ConvertUTF16toUTF8(mAppId);
  CryptoBuffer appParam;
  if (!appParam.SetLength(SHA256_LENGTH, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Note: This could use nsICryptoHash to avoid having to interact with NSS
  // directly.
  SECStatus srv;
  srv = PK11_HashBuf(SEC_OID_SHA256, appParam.Elements(),
                     reinterpret_cast<const uint8_t*>(cAppId.BeginReading()),
                     cAppId.Length());
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  // First, we must determine if any of the RegisteredKeys are already
  // registered, e.g., in the whitelist.
  for (LocalRegisteredKey key : mRegisteredKeys) {
    nsTArray<RefPtr<U2FPrepPromise>> prepPromiseList;
    for (const Authenticator& token : mAuthenticators) {
      RefPtr<U2FIsRegisteredTask> compTask =
        new U2FIsRegisteredTask(token, key, appParam, mEventTarget);
      prepPromiseList.AppendElement(compTask->Execute());
    }

    // Treat each call to Promise::All as a work unit, as it completes together
    status->WaitGroupAdd();

    U2FPrepPromise::All(mEventTarget, prepPromiseList)
    ->Then(mEventTarget, __func__,
      [&status] (const nsTArray<Authenticator>& aTokens) {
        MOZ_LOG(gU2FLog, LogLevel::Debug,
                ("ALL: None of the RegisteredKeys were recognized. n=%" PRIuSIZE,
                 aTokens.Length()));

        status->WaitGroupDone();
      },
      [&status] (ErrorCode aErrorCode) {
        status->Stop(aErrorCode);
        status->WaitGroupDone();
    });
  }

  // Wait for all the IsRegistered tasks to complete
  status->WaitGroupWait();

  // Check to see whether we're supposed to stop, because one of the keys was
  // recognized.
  if (status->IsStopped()) {
    status->WaitGroupAdd();
    mEventTarget->Dispatch(NS_NewRunnableFunction(
      [&status, this] () {
        RegisterResponse response;
        response.mErrorCode.Construct(
            static_cast<uint32_t>(status->GetErrorCode()));
        SendResponse(response);
        status->WaitGroupDone();
      }
    ));

    // Don't exit until the main thread runnable completes
    status->WaitGroupWait();
    return NS_OK;
  }

  // Now proceed to actually register a new key.
  for (LocalRegisterRequest req : mRegisterRequests) {
    // Hash the ClientData into the ChallengeParam
    CryptoBuffer challengeParam;
    if (!challengeParam.SetLength(SHA256_LENGTH, fallible)) {
      continue;
    }

    srv = PK11_HashBuf(SEC_OID_SHA256, challengeParam.Elements(),
                       req.mClientData.Elements(), req.mClientData.Length());
    if (srv != SECSuccess) {
      continue;
    }

    for (const Authenticator& token : mAuthenticators) {
      RefPtr<U2FRegisterTask> registerTask = new U2FRegisterTask(mOrigin, mAppId,
                                                                 token, appParam,
                                                                 challengeParam,
                                                                 req,
                                                                 mEventTarget);
      status->WaitGroupAdd();

      registerTask->Execute()->Then(mEventTarget, __func__,
        [&status] (nsString aResponse) {
          if (!status->IsStopped()) {
            status->Stop(ErrorCode::OK, aResponse);
          }
          status->WaitGroupDone();
        },
        [&status] (ErrorCode aErrorCode) {
          // Ignore the failing error code, as we only want the first success.
          // U2F devices don't provide much for error codes anyway, so if
          // they all fail we'll return DEVICE_INELIGIBLE.
          status->WaitGroupDone();
     });
    }
  }

  // Wait until the first key is successfuly generated
  status->WaitGroupWait();

  // If none of the tasks completed, then nothing could satisfy.
  if (!status->IsStopped()) {
    status->Stop(ErrorCode::BAD_REQUEST);
  }

  // Transmit back to the JS engine from the Main Thread
  status->WaitGroupAdd();
  mEventTarget->Dispatch(NS_NewRunnableFunction(
    [&status, this] () {
      RegisterResponse response;
      if (status->GetErrorCode() == ErrorCode::OK) {
        response.Init(status->GetResponse());
      } else {
        response.mErrorCode.Construct(
            static_cast<uint32_t>(status->GetErrorCode()));
      }
      SendResponse(response);
      status->WaitGroupDone();
    }
  ));

  // TODO: Add timeouts, Bug 1301793
  status->WaitGroupWait();
  return NS_OK;
}

U2FSignRunnable::U2FSignRunnable(const nsAString& aOrigin,
                                 const nsAString& aAppId,
                                 const nsAString& aChallenge,
                                 const Sequence<RegisteredKey>& aRegisteredKeys,
                                 const Sequence<Authenticator>& aAuthenticators,
                                 U2FSignCallback* aCallback,
                                 nsISerialEventTarget* aEventTarget)
  : U2FRunnable(aOrigin, aAppId, aEventTarget)
  , mAuthenticators(aAuthenticators)
  // U2FSignCallback does not support threadsafe refcounting, and must be used
  // and destroyed on main.
  , mCallback(new nsMainThreadPtrHolder<U2FSignCallback>(
      "U2FSignRunnable::mCallback", aCallback))
{
  MOZ_ASSERT(NS_IsMainThread());

  // Convert WebIDL objects to generic structs to pass between threads
  for (const RegisteredKey& key : aRegisteredKeys) {
    // Check for required attributes
    if (!key.mVersion.WasPassed() || !key.mKeyHandle.WasPassed()) {
      continue;
    }

    LocalRegisteredKey localKey;
    localKey.mVersion = key.mVersion.Value();
    localKey.mKeyHandle = key.mKeyHandle.Value();
    if (key.mAppId.WasPassed()) {
      localKey.mAppId.SetValue(key.mAppId.Value());
    }

    mRegisteredKeys.AppendElement(localKey);
  }

  // Assemble a clientData object
  nsresult rv = AssembleClientData(aOrigin, kGetAssertion, aChallenge,
                                   mClientData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gU2FLog, LogLevel::Warning,
            ("Failed to AssembleClientData for the U2FSignRunnable."));
    return;
  }
}

U2FSignRunnable::~U2FSignRunnable()
{
  nsNSSShutDownPreventionLock locker;

  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(ShutdownCalledFrom::Object);
}

void
U2FSignRunnable::SetTimeout(const int32_t aTimeoutMillis)
{
  opt_mTimeoutSeconds.SetValue(aTimeoutMillis);
}

void
U2FSignRunnable::SendResponse(const SignResponse& aResponse)
{
  MOZ_ASSERT(NS_IsMainThread());

  ErrorResult rv;
  mCallback->Call(aResponse, rv);
  NS_WARNING_ASSERTION(!rv.Failed(), "callback failed");
  // Useful exceptions already got reported.
  rv.SuppressException();
}

NS_IMETHODIMP
U2FSignRunnable::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_FAILURE;
  }

  // Create a Status object to keep track of when we're done
  RefPtr<U2FStatus> status = new U2FStatus();

  // Evaluate the AppID
  ErrorCode appIdResult = EvaluateAppID();
  if (appIdResult != ErrorCode::OK) {
    status->Stop(appIdResult);
  }

  // Hash the AppID and the ClientData into the AppParam and ChallengeParam
  nsCString cAppId = NS_ConvertUTF16toUTF8(mAppId);
  CryptoBuffer appParam;
  CryptoBuffer challengeParam;
  if (!appParam.SetLength(SHA256_LENGTH, fallible) ||
      !challengeParam.SetLength(SHA256_LENGTH, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SECStatus srv;
  srv = PK11_HashBuf(SEC_OID_SHA256, appParam.Elements(),
                     reinterpret_cast<const uint8_t*>(cAppId.BeginReading()),
                     cAppId.Length());
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  srv = PK11_HashBuf(SEC_OID_SHA256, challengeParam.Elements(),
                     mClientData.Elements(), mClientData.Length());
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  // Search the signing requests for one a token can fulfill
  for (LocalRegisteredKey key : mRegisteredKeys) {
    // Do not permit an individual RegisteredKey to assert a different AppID
    if (!key.mAppId.IsNull() && mAppId != key.mAppId.Value()) {
      continue;
    }

    // Decode the key handle
    CryptoBuffer keyHandle;
    nsresult rv = keyHandle.FromJwkBase64(key.mKeyHandle);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    // We ignore mTransports, as it is intended to be used for sorting the
    // available devices by preference, but is not an exclusion factor.

    for (const Authenticator& token : mAuthenticators) {
      RefPtr<U2FSignTask> signTask = new U2FSignTask(mOrigin, mAppId,
                                                     key.mVersion, token,
                                                     appParam, challengeParam,
                                                     mClientData, keyHandle,
                                                     mEventTarget);
      status->WaitGroupAdd();

      signTask->Execute()->Then(mEventTarget, __func__,
        [&status] (nsString aResponse) {
          if (!status->IsStopped()) {
            status->Stop(ErrorCode::OK, aResponse);
          }
          status->WaitGroupDone();
        },
        [&status] (ErrorCode aErrorCode) {
          // Ignore the failing error code, as we only want the first success.
          // U2F devices don't provide much for error codes anyway, so if
          // they all fail we'll return DEVICE_INELIGIBLE.
          status->WaitGroupDone();
      });
    }
  }

  // Wait for the authenticators to finish
  status->WaitGroupWait();

  // If none of the tasks completed, then nothing could satisfy.
  if (!status->IsStopped()) {
    status->Stop(ErrorCode::DEVICE_INELIGIBLE);
  }

  // Transmit back to the JS engine from the Main Thread
  status->WaitGroupAdd();
  mEventTarget->Dispatch(NS_NewRunnableFunction(
    [&status, this] () {
      SignResponse response;
      if (status->GetErrorCode() == ErrorCode::OK) {
        response.Init(status->GetResponse());
      } else {
        response.mErrorCode.Construct(
          static_cast<uint32_t>(status->GetErrorCode()));
      }
      SendResponse(response);
      status->WaitGroupDone();
    }
  ));

  // TODO: Add timeouts, Bug 1301793
  status->WaitGroupWait();
  return NS_OK;
}

U2F::U2F()
  : mInitialized(false)
{}

U2F::~U2F()
{
  nsNSSShutDownPreventionLock locker;

  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(ShutdownCalledFrom::Object);
}

/* virtual */ JSObject*
U2F::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return U2FBinding::Wrap(aCx, this, aGivenProto);
}

void
U2F::Init(nsPIDOMWindowInner* aParent, ErrorResult& aRv)
{
  MOZ_ASSERT(!mInitialized);
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
    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("Failed to get NSS context for U2F"));
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // This only functions in e10s mode
  if (XRE_IsParentProcess()) {
    MOZ_LOG(gU2FLog, LogLevel::Debug,
            ("Is non-e10s Process, U2F not available"));
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Monolithically insert compatible nsIU2FToken objects into mAuthenticators.
  // In future functionality expansions, this is where we could add a dynamic
  // add/remove interface.
  if (Preferences::GetBool(PREF_U2F_SOFTTOKEN_ENABLED)) {
    if (!mAuthenticators.AppendElement(new NSSU2FTokenRemote(),
                                       mozilla::fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  }

  mEventTarget = doc->EventTargetFor(TaskCategory::Other);

  mInitialized = true;
}

void
U2F::Register(const nsAString& aAppId,
              const Sequence<RegisterRequest>& aRegisterRequests,
              const Sequence<RegisteredKey>& aRegisteredKeys,
              U2FRegisterCallback& aCallback,
              const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
              ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mInitialized) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  RefPtr<SharedThreadPool> pool = SharedThreadPool::Get(kPoolName);
  RefPtr<U2FRegisterRunnable> task = new U2FRegisterRunnable(mOrigin, aAppId,
                                                             aRegisterRequests,
                                                             aRegisteredKeys,
                                                             mAuthenticators,
                                                             &aCallback,
                                                             mEventTarget);
  pool->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

void
U2F::Sign(const nsAString& aAppId,
          const nsAString& aChallenge,
          const Sequence<RegisteredKey>& aRegisteredKeys,
          U2FSignCallback& aCallback,
          const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
          ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mInitialized) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  RefPtr<SharedThreadPool> pool = SharedThreadPool::Get(kPoolName);
  RefPtr<U2FSignRunnable> task = new U2FSignRunnable(mOrigin, aAppId, aChallenge,
                                                     aRegisteredKeys,
                                                     mAuthenticators, &aCallback,
                                                     mEventTarget);
  pool->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

} // namespace dom
} // namespace mozilla
