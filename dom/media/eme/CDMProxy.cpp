/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CDMProxy.h"
#include "nsString.h"
#include "mozilla/dom/MediaKeys.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsContentCID.h"
#include "nsServiceManagerUtils.h"
#include "MainThreadUtils.h"
#include "mozilla/EMEUtils.h"
#include "nsIConsoleService.h"
#include "prenv.h"
#include "mozilla/PodOperations.h"
#include "mozilla/CDMCallbackProxy.h"
#include "MediaData.h"
#include "nsPrintfCString.h"

namespace mozilla {

CDMProxy::CDMProxy(dom::MediaKeys* aKeys, const nsAString& aKeySystem)
  : mKeys(aKeys)
  , mKeySystem(aKeySystem)
  , mCDM(nullptr)
  , mDecryptionJobCount(0)
  , mShutdownCalled(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(CDMProxy);
}

CDMProxy::~CDMProxy()
{
  MOZ_COUNT_DTOR(CDMProxy);
}

void
CDMProxy::Init(PromiseId aPromiseId,
               const nsAString& aOrigin,
               const nsAString& aTopLevelOrigin,
               const nsAString& aGMPName,
               bool aInPrivateBrowsing)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  EME_LOG("CDMProxy::Init (%s, %s) %s",
          NS_ConvertUTF16toUTF8(aOrigin).get(),
          NS_ConvertUTF16toUTF8(aTopLevelOrigin).get(),
          (aInPrivateBrowsing ? "PrivateBrowsing" : "NonPrivateBrowsing"));

  nsCString pluginVersion;
  if (!mGMPThread) {
    nsCOMPtr<mozIGeckoMediaPluginService> mps =
      do_GetService("@mozilla.org/gecko-media-plugin-service;1");
    if (!mps) {
      RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                    NS_LITERAL_CSTRING("Couldn't get MediaPluginService in CDMProxy::Init"));
      return;
    }
    mps->GetThread(getter_AddRefs(mGMPThread));
    if (!mGMPThread) {
      RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                    NS_LITERAL_CSTRING("Couldn't get GMP thread CDMProxy::Init"));
      return;
    }
  }

  if (aGMPName.IsEmpty()) {
    RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
      nsPrintfCString("Unknown GMP for keysystem '%s'", NS_ConvertUTF16toUTF8(mKeySystem).get()));
    return;
  }

  nsAutoPtr<InitData> data(new InitData());
  data->mPromiseId = aPromiseId;
  data->mOrigin = aOrigin;
  data->mTopLevelOrigin = aTopLevelOrigin;
  data->mGMPName = aGMPName;
  data->mInPrivateBrowsing = aInPrivateBrowsing;
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<nsAutoPtr<InitData>>(this,
                                           &CDMProxy::gmp_Init,
                                           Move(data)));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

#ifdef DEBUG
bool
CDMProxy::IsOnGMPThread()
{
  return NS_GetCurrentThread() == mGMPThread;
}
#endif

void
CDMProxy::gmp_InitDone(GMPDecryptorProxy* aCDM, nsAutoPtr<InitData>&& aData)
{
  EME_LOG("CDMProxy::gmp_InitDone");
  if (mShutdownCalled) {
    if (aCDM) {
      aCDM->Close();
    }
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("CDMProxy was shut down before init could complete"));
    return;
  }
  if (!aCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("GetGMPDecryptor failed to return a CDM"));
    return;
  }

  mCDM = aCDM;
  mCallback = new CDMCallbackProxy(this);
  mCDM->Init(mCallback);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<uint32_t>(this,
                                &CDMProxy::OnCDMCreated,
                                aData->mPromiseId));
  NS_DispatchToMainThread(task);
}

class gmp_InitDoneCallback : public GetGMPDecryptorCallback
{
public:
  gmp_InitDoneCallback(CDMProxy* aCDMProxy,
                       nsAutoPtr<CDMProxy::InitData>&& aData)
    : mCDMProxy(aCDMProxy),
      mData(Move(aData))
  {
  }

  void Done(GMPDecryptorProxy* aCDM)
  {
    mCDMProxy->gmp_InitDone(aCDM, Move(mData));
  }

private:
  RefPtr<CDMProxy> mCDMProxy;
  nsAutoPtr<CDMProxy::InitData> mData;
};

class gmp_InitGetGMPDecryptorCallback : public GetNodeIdCallback
{
public:
  gmp_InitGetGMPDecryptorCallback(CDMProxy* aCDMProxy,
                                  nsAutoPtr<CDMProxy::InitData>&& aData)
    : mCDMProxy(aCDMProxy),
      mData(aData)
  {
  }

  void Done(nsresult aResult, const nsACString& aNodeId)
  {
    mCDMProxy->gmp_InitGetGMPDecryptor(aResult, aNodeId, Move(mData));
  }

private:
  RefPtr<CDMProxy> mCDMProxy;
  nsAutoPtr<CDMProxy::InitData> mData;
};

void
CDMProxy::gmp_Init(nsAutoPtr<InitData>&& aData)
{
  MOZ_ASSERT(IsOnGMPThread());

  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (!mps) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Couldn't get MediaPluginService in CDMProxy::gmp_Init"));
    return;
  }

  // Make a copy before we transfer ownership of aData to the
  // gmp_InitGetGMPDecryptorCallback.
  InitData data(*aData);
  UniquePtr<GetNodeIdCallback> callback(
    new gmp_InitGetGMPDecryptorCallback(this, Move(aData)));
  nsresult rv = mps->GetNodeId(data.mOrigin,
                               data.mTopLevelOrigin,
                               data.mGMPName,
                               data.mInPrivateBrowsing,
                               Move(callback));
  if (NS_FAILED(rv)) {
    RejectPromise(data.mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Call to GetNodeId() failed early"));
  }
}

void
CDMProxy::gmp_InitGetGMPDecryptor(nsresult aResult,
                                  const nsACString& aNodeId,
                                  nsAutoPtr<InitData>&& aData)
{
  uint32_t promiseID = aData->mPromiseId;
  if (NS_FAILED(aResult)) {
    RejectPromise(promiseID, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("GetNodeId() called back, but with a failure result"));
    return;
  }

  mNodeId = aNodeId;
  MOZ_ASSERT(!GetNodeId().IsEmpty());

  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (!mps) {
    RejectPromise(promiseID, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Couldn't get MediaPluginService in CDMProxy::gmp_InitGetGMPDecryptor"));
    return;
  }

  EME_LOG("CDMProxy::gmp_Init (%s, %s) %s NodeId=%s",
          NS_ConvertUTF16toUTF8(aData->mOrigin).get(),
          NS_ConvertUTF16toUTF8(aData->mTopLevelOrigin).get(),
          (aData->mInPrivateBrowsing ? "PrivateBrowsing" : "NonPrivateBrowsing"),
          GetNodeId().get());

  nsTArray<nsCString> tags;
  tags.AppendElement(NS_ConvertUTF16toUTF8(mKeySystem));

  UniquePtr<GetGMPDecryptorCallback> callback(new gmp_InitDoneCallback(this,
                                                                       Move(aData)));
  nsresult rv = mps->GetGMPDecryptor(&tags, GetNodeId(), Move(callback));
  if (NS_FAILED(rv)) {
    RejectPromise(promiseID, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Call to GetGMPDecryptor() failed early"));
  }
}

void
CDMProxy::OnCDMCreated(uint32_t aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  MOZ_ASSERT(!GetNodeId().IsEmpty());
  if (mCDM) {
    mKeys->OnCDMCreated(aPromiseId, GetNodeId(), mCDM->GetPluginId());
  } else {
    // No CDM? Just reject the promise.
    mKeys->RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                         NS_LITERAL_CSTRING("Null CDM in OnCDMCreated()"));
  }
}

void
CDMProxy::CreateSession(uint32_t aCreateSessionToken,
                        dom::SessionType aSessionType,
                        PromiseId aPromiseId,
                        const nsAString& aInitDataType,
                        nsTArray<uint8_t>& aInitData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGMPThread);

  nsAutoPtr<CreateSessionData> data(new CreateSessionData());
  data->mSessionType = aSessionType;
  data->mCreateSessionToken = aCreateSessionToken;
  data->mPromiseId = aPromiseId;
  data->mInitDataType = NS_ConvertUTF16toUTF8(aInitDataType);
  data->mInitData = Move(aInitData);

  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<nsAutoPtr<CreateSessionData>>(this, &CDMProxy::gmp_CreateSession, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

GMPSessionType
ToGMPSessionType(dom::SessionType aSessionType) {
  switch (aSessionType) {
    case dom::SessionType::Temporary: return kGMPTemporySession;
    case dom::SessionType::Persistent: return kGMPPersistentSession;
    default: return kGMPTemporySession;
  };
};

void
CDMProxy::gmp_CreateSession(nsAutoPtr<CreateSessionData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in gmp_CreateSession"));
    return;
  }
  mCDM->CreateSession(aData->mCreateSessionToken,
                      aData->mPromiseId,
                      aData->mInitDataType,
                      aData->mInitData,
                      ToGMPSessionType(aData->mSessionType));
}

void
CDMProxy::LoadSession(PromiseId aPromiseId,
                      const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGMPThread);

  nsAutoPtr<SessionOpData> data(new SessionOpData());
  data->mPromiseId = aPromiseId;
  data->mSessionId = NS_ConvertUTF16toUTF8(aSessionId);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<nsAutoPtr<SessionOpData>>(this, &CDMProxy::gmp_LoadSession, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_LoadSession(nsAutoPtr<SessionOpData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in gmp_LoadSession"));
    return;
  }
  mCDM->LoadSession(aData->mPromiseId, aData->mSessionId);
}

void
CDMProxy::SetServerCertificate(PromiseId aPromiseId,
                               nsTArray<uint8_t>& aCert)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGMPThread);

  nsAutoPtr<SetServerCertificateData> data(new SetServerCertificateData());
  data->mPromiseId = aPromiseId;
  data->mCert = Move(aCert);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<nsAutoPtr<SetServerCertificateData>>(this, &CDMProxy::gmp_SetServerCertificate, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_SetServerCertificate(nsAutoPtr<SetServerCertificateData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in gmp_SetServerCertificate"));
    return;
  }
  mCDM->SetServerCertificate(aData->mPromiseId, aData->mCert);
}

void
CDMProxy::UpdateSession(const nsAString& aSessionId,
                        PromiseId aPromiseId,
                        nsTArray<uint8_t>& aResponse)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGMPThread);
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  nsAutoPtr<UpdateSessionData> data(new UpdateSessionData());
  data->mPromiseId = aPromiseId;
  data->mSessionId = NS_ConvertUTF16toUTF8(aSessionId);
  data->mResponse = Move(aResponse);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<nsAutoPtr<UpdateSessionData>>(this, &CDMProxy::gmp_UpdateSession, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_UpdateSession(nsAutoPtr<UpdateSessionData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in gmp_UpdateSession"));
    return;
  }
  mCDM->UpdateSession(aData->mPromiseId,
                      aData->mSessionId,
                      aData->mResponse);
}

void
CDMProxy::CloseSession(const nsAString& aSessionId,
                       PromiseId aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  nsAutoPtr<SessionOpData> data(new SessionOpData());
  data->mPromiseId = aPromiseId;
  data->mSessionId = NS_ConvertUTF16toUTF8(aSessionId);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<nsAutoPtr<SessionOpData>>(this, &CDMProxy::gmp_CloseSession, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_CloseSession(nsAutoPtr<SessionOpData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in gmp_CloseSession"));
    return;
  }
  mCDM->CloseSession(aData->mPromiseId, aData->mSessionId);
}

void
CDMProxy::RemoveSession(const nsAString& aSessionId,
                        PromiseId aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  nsAutoPtr<SessionOpData> data(new SessionOpData());
  data->mPromiseId = aPromiseId;
  data->mSessionId = NS_ConvertUTF16toUTF8(aSessionId);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<nsAutoPtr<SessionOpData>>(this, &CDMProxy::gmp_RemoveSession, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_RemoveSession(nsAutoPtr<SessionOpData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in gmp_RemoveSession"));
    return;
  }
  mCDM->RemoveSession(aData->mPromiseId, aData->mSessionId);
}

void
CDMProxy::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  mKeys.Clear();
  // Note: This may end up being the last owning reference to the CDMProxy.
  nsCOMPtr<nsIRunnable> task(NewRunnableMethod(this, &CDMProxy::gmp_Shutdown));
  if (mGMPThread) {
    mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
  }
}

void
CDMProxy::gmp_Shutdown()
{
  MOZ_ASSERT(IsOnGMPThread());

  mShutdownCalled = true;

  // Abort any pending decrypt jobs, to awaken any clients waiting on a job.
  for (size_t i = 0; i < mDecryptionJobs.Length(); i++) {
    DecryptJob* job = mDecryptionJobs[i];
    job->PostResult(GMPAbortedErr);
  }
  mDecryptionJobs.Clear();

  if (mCDM) {
    mCDM->Close();
    mCDM = nullptr;
  }
}

void
CDMProxy::RejectPromise(PromiseId aId, nsresult aCode,
                        const nsCString& aReason)
{
  if (NS_IsMainThread()) {
    if (!mKeys.IsNull()) {
      mKeys->RejectPromise(aId, aCode, aReason);
    }
  } else {
    nsCOMPtr<nsIRunnable> task(new RejectPromiseTask(this, aId, aCode,
                                                     aReason));
    NS_DispatchToMainThread(task);
  }
}

void
CDMProxy::ResolvePromise(PromiseId aId)
{
  if (NS_IsMainThread()) {
    if (!mKeys.IsNull()) {
      mKeys->ResolvePromise(aId);
    } else {
      NS_WARNING("CDMProxy unable to resolve promise!");
    }
  } else {
    nsCOMPtr<nsIRunnable> task;
    task = NewRunnableMethod<PromiseId>(this,
                                        &CDMProxy::ResolvePromise,
                                        aId);
    NS_DispatchToMainThread(task);
  }
}

const nsCString&
CDMProxy::GetNodeId() const
{
  return mNodeId;
}

void
CDMProxy::OnSetSessionId(uint32_t aCreateSessionToken,
                         const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }

  RefPtr<dom::MediaKeySession> session(mKeys->GetPendingSession(aCreateSessionToken));
  if (session) {
    session->SetSessionId(aSessionId);
  }
}

void
CDMProxy::OnResolveLoadSessionPromise(uint32_t aPromiseId, bool aSuccess)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  mKeys->OnSessionLoaded(aPromiseId, aSuccess);
}

static dom::MediaKeyMessageType
ToMediaKeyMessageType(GMPSessionMessageType aMessageType) {
  switch (aMessageType) {
    case kGMPLicenseRequest: return dom::MediaKeyMessageType::License_request;
    case kGMPLicenseRenewal: return dom::MediaKeyMessageType::License_renewal;
    case kGMPLicenseRelease: return dom::MediaKeyMessageType::License_release;
    case kGMPIndividualizationRequest: return dom::MediaKeyMessageType::Individualization_request;
    default: return dom::MediaKeyMessageType::License_request;
  };
};

void
CDMProxy::OnSessionMessage(const nsAString& aSessionId,
                           GMPSessionMessageType aMessageType,
                           nsTArray<uint8_t>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyMessage(ToMediaKeyMessageType(aMessageType), aMessage);
  }
}

void
CDMProxy::OnKeyStatusesChange(const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyStatusesChange();
  }
}

void
CDMProxy::OnExpirationChange(const nsAString& aSessionId,
                             GMPTimestamp aExpiryTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->SetExpiration(static_cast<double>(aExpiryTime));
  }
}

void
CDMProxy::OnSessionClosed(const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->OnClosed();
  }
}

static void
LogToConsole(const nsAString& aMsg)
{
  nsCOMPtr<nsIConsoleService> console(
    do_GetService("@mozilla.org/consoleservice;1"));
  if (!console) {
    NS_WARNING("Failed to log message to console.");
    return;
  }
  nsAutoString msg(aMsg);
  console->LogStringMessage(msg.get());
}

void
CDMProxy::OnSessionError(const nsAString& aSessionId,
                         nsresult aException,
                         uint32_t aSystemCode,
                         const nsAString& aMsg)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyError(aSystemCode);
  }
  LogToConsole(aMsg);
}

void
CDMProxy::OnRejectPromise(uint32_t aPromiseId,
                          nsresult aDOMException,
                          const nsCString& aMsg)
{
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromise(aPromiseId, aDOMException, aMsg);
}

const nsString&
CDMProxy::KeySystem() const
{
  return mKeySystem;
}

CDMCaps&
CDMProxy::Capabilites() {
  return mCapabilites;
}

RefPtr<CDMProxy::DecryptPromise>
CDMProxy::Decrypt(MediaRawData* aSample)
{
  RefPtr<DecryptJob> job(new DecryptJob(aSample));
  RefPtr<DecryptPromise> promise(job->Ensure());

  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<RefPtr<DecryptJob>>(this, &CDMProxy::gmp_Decrypt, job));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
  return promise;
}

void
CDMProxy::gmp_Decrypt(RefPtr<DecryptJob> aJob)
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!mCDM) {
    aJob->PostResult(GMPAbortedErr);
    return;
  }

  aJob->mId = ++mDecryptionJobCount;
  nsTArray<uint8_t> data;
  data.AppendElements(aJob->mSample->Data(), aJob->mSample->Size());
  mCDM->Decrypt(aJob->mId, aJob->mSample->mCrypto, data);
  mDecryptionJobs.AppendElement(aJob.forget());
}

void
CDMProxy::gmp_Decrypted(uint32_t aId,
                        GMPErr aResult,
                        const nsTArray<uint8_t>& aDecryptedData)
{
  MOZ_ASSERT(IsOnGMPThread());
#ifdef DEBUG
  bool jobIdFound = false;
#endif
  for (size_t i = 0; i < mDecryptionJobs.Length(); i++) {
    DecryptJob* job = mDecryptionJobs[i];
    if (job->mId == aId) {
#ifdef DEBUG
      jobIdFound = true;
#endif
      job->PostResult(aResult, aDecryptedData);
      mDecryptionJobs.RemoveElementAt(i);
    }
  }
#ifdef DEBUG
  if (!jobIdFound) {
    NS_WARNING("GMPDecryptorChild returned incorrect job ID");
  }
#endif
}

void
CDMProxy::DecryptJob::PostResult(GMPErr aResult)
{
  nsTArray<uint8_t> empty;
  PostResult(aResult, empty);
}

void
CDMProxy::DecryptJob::PostResult(GMPErr aResult, const nsTArray<uint8_t>& aDecryptedData)
{
  if (aDecryptedData.Length() != mSample->Size()) {
    NS_WARNING("CDM returned incorrect number of decrypted bytes");
  }
  if (GMP_SUCCEEDED(aResult)) {
    nsAutoPtr<MediaRawDataWriter> writer(mSample->CreateWriter());
    PodCopy(writer->Data(),
            aDecryptedData.Elements(),
            std::min<size_t>(aDecryptedData.Length(), mSample->Size()));
  } else if (aResult == GMPNoKeyErr) {
    NS_WARNING("CDM returned GMPNoKeyErr");
    // We still have the encrypted sample, so we can re-enqueue it to be
    // decrypted again once the key is usable again.
  } else {
    nsAutoCString str("CDM returned decode failure GMPErr=");
    str.AppendInt(aResult);
    NS_WARNING(str.get());
  }
  mPromise.Resolve(DecryptResult(aResult, mSample), __func__);
}

void
CDMProxy::GetSessionIdsForKeyId(const nsTArray<uint8_t>& aKeyId,
                                nsTArray<nsCString>& aSessionIds)
{
  CDMCaps::AutoLock caps(Capabilites());
  caps.GetSessionIdsForKeyId(aKeyId, aSessionIds);
}

void
CDMProxy::Terminated()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_WARNING("CDM terminated");
  if (!mKeys.IsNull()) {
    mKeys->Terminated();
  }
}

} // namespace mozilla
