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

namespace mozilla {

CDMProxy::CDMProxy(dom::MediaKeys* aKeys, const nsAString& aKeySystem)
  : mKeys(aKeys)
  , mKeySystem(aKeySystem)
  , mCDM(nullptr)
  , mDecryptionJobCount(0)
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
               bool aInPrivateBrowsing)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  EME_LOG("CDMProxy::Init (%s, %s) %s",
          NS_ConvertUTF16toUTF8(aOrigin).get(),
          NS_ConvertUTF16toUTF8(aTopLevelOrigin).get(),
          (aInPrivateBrowsing ? "PrivateBrowsing" : "NonPrivateBrowsing"));

  if (!mGMPThread) {
    nsCOMPtr<mozIGeckoMediaPluginService> mps =
      do_GetService("@mozilla.org/gecko-media-plugin-service;1");
    if (!mps) {
      RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
    mps->GetThread(getter_AddRefs(mGMPThread));
    if (!mGMPThread) {
      RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
  }
  nsAutoPtr<InitData> data(new InitData());
  data->mPromiseId = aPromiseId;
  data->mOrigin = aOrigin;
  data->mTopLevelOrigin = aTopLevelOrigin;
  data->mInPrivateBrowsing = aInPrivateBrowsing;
  nsRefPtr<nsIRunnable> task(
    NS_NewRunnableMethodWithArg<nsAutoPtr<InitData>>(this,
                                                     &CDMProxy::gmp_Init,
                                                     data));
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
CDMProxy::gmp_Init(nsAutoPtr<InitData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());

  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (!mps) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsresult rv = mps->GetNodeId(aData->mOrigin,
                               aData->mTopLevelOrigin,
                               aData->mInPrivateBrowsing,
                               mNodeId);
  MOZ_ASSERT(!GetNodeId().IsEmpty());
  if (NS_FAILED(rv)) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  EME_LOG("CDMProxy::gmp_Init (%s, %s) %s NodeId=%s",
          NS_ConvertUTF16toUTF8(aData->mOrigin).get(),
          NS_ConvertUTF16toUTF8(aData->mTopLevelOrigin).get(),
          (aData->mInPrivateBrowsing ? "PrivateBrowsing" : "NonPrivateBrowsing"),
          GetNodeId().get());

  nsTArray<nsCString> tags;
  tags.AppendElement(NS_ConvertUTF16toUTF8(mKeySystem));
  rv = mps->GetGMPDecryptor(&tags, GetNodeId(), &mCDM);
  if (NS_FAILED(rv) || !mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
  } else {
    mCallback = new CDMCallbackProxy(this);
    mCDM->Init(mCallback);
    nsRefPtr<nsIRunnable> task(
      NS_NewRunnableMethodWithArg<uint32_t>(this,
                                            &CDMProxy::OnCDMCreated,
                                            aData->mPromiseId));
    NS_DispatchToMainThread(task);
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
  mKeys->OnCDMCreated(aPromiseId, GetNodeId());
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

  nsRefPtr<nsIRunnable> task(
    NS_NewRunnableMethodWithArg<nsAutoPtr<CreateSessionData>>(this, &CDMProxy::gmp_CreateSession, data));
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
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
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
  nsRefPtr<nsIRunnable> task(
    NS_NewRunnableMethodWithArg<nsAutoPtr<SessionOpData>>(this, &CDMProxy::gmp_LoadSession, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_LoadSession(nsAutoPtr<SessionOpData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
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

  nsAutoPtr<SetServerCertificateData> data;
  data->mPromiseId = aPromiseId;
  data->mCert = Move(aCert);
  nsRefPtr<nsIRunnable> task(
    NS_NewRunnableMethodWithArg<nsAutoPtr<SetServerCertificateData>>(this, &CDMProxy::gmp_SetServerCertificate, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_SetServerCertificate(nsAutoPtr<SetServerCertificateData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
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
  nsRefPtr<nsIRunnable> task(
    NS_NewRunnableMethodWithArg<nsAutoPtr<UpdateSessionData>>(this, &CDMProxy::gmp_UpdateSession, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_UpdateSession(nsAutoPtr<UpdateSessionData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
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
  nsRefPtr<nsIRunnable> task(
    NS_NewRunnableMethodWithArg<nsAutoPtr<SessionOpData>>(this, &CDMProxy::gmp_CloseSession, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_CloseSession(nsAutoPtr<SessionOpData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
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
  nsRefPtr<nsIRunnable> task(
    NS_NewRunnableMethodWithArg<nsAutoPtr<SessionOpData>>(this, &CDMProxy::gmp_RemoveSession, data));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_RemoveSession(nsAutoPtr<SessionOpData> aData)
{
  MOZ_ASSERT(IsOnGMPThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR);
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
  nsRefPtr<nsIRunnable> task(NS_NewRunnableMethod(this, &CDMProxy::gmp_Shutdown));
  if (mGMPThread) {
    mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
  }
}

void
CDMProxy::gmp_Shutdown()
{
  MOZ_ASSERT(IsOnGMPThread());

  // Abort any pending decrypt jobs, to awaken any clients waiting on a job.
  for (size_t i = 0; i < mDecryptionJobs.Length(); i++) {
    DecryptJob* job = mDecryptionJobs[i];
    job->mClient->Decrypted(GMPAbortedErr, nullptr);
  }
  mDecryptionJobs.Clear();

  if (mCDM) {
    mCDM->Close();
    mCDM = nullptr;
  }
}

void
CDMProxy::RejectPromise(PromiseId aId, nsresult aCode)
{
  if (NS_IsMainThread()) {
    if (!mKeys.IsNull()) {
      mKeys->RejectPromise(aId, aCode);
    }
  } else {
    nsRefPtr<nsIRunnable> task(new RejectPromiseTask(this, aId, aCode));
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
    nsRefPtr<nsIRunnable> task;
    task = NS_NewRunnableMethodWithArg<PromiseId>(this,
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

  nsRefPtr<dom::MediaKeySession> session(mKeys->GetPendingSession(aCreateSessionToken));
  session->SetSessionId(aSessionId);
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
  nsRefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
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
  nsRefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyStatusesChange();
  }
}

void
CDMProxy::OnExpirationChange(const nsAString& aSessionId,
                             GMPTimestamp aExpiryTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_WARNING("CDMProxy::OnExpirationChange() not implemented");
}

void
CDMProxy::OnSessionClosed(const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  nsRefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
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
  nsRefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyError(aSystemCode);
  }
  LogToConsole(aMsg);
}

void
CDMProxy::OnRejectPromise(uint32_t aPromiseId,
                          nsresult aDOMException,
                          const nsAString& aMsg)
{
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromise(aPromiseId, aDOMException);
  LogToConsole(aMsg);
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

void
CDMProxy::Decrypt(mp4_demuxer::MP4Sample* aSample,
                  DecryptionClient* aClient)
{
  nsAutoPtr<DecryptJob> job(new DecryptJob(aSample, aClient));
  nsRefPtr<nsIRunnable> task(
    NS_NewRunnableMethodWithArg<nsAutoPtr<DecryptJob>>(this, &CDMProxy::gmp_Decrypt, job));
  mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
CDMProxy::gmp_Decrypt(nsAutoPtr<DecryptJob> aJob)
{
  MOZ_ASSERT(IsOnGMPThread());
  MOZ_ASSERT(aJob->mClient);
  MOZ_ASSERT(aJob->mSample);

  if (!mCDM) {
    aJob->mClient->Decrypted(GMPAbortedErr, nullptr);
    return;
  }

  aJob->mId = ++mDecryptionJobCount;
  nsTArray<uint8_t> data;
  data.AppendElements(aJob->mSample->data, aJob->mSample->size);
  mCDM->Decrypt(aJob->mId, aJob->mSample->crypto, data);
  mDecryptionJobs.AppendElement(aJob.forget());
}

void
CDMProxy::gmp_Decrypted(uint32_t aId,
                        GMPErr aResult,
                        const nsTArray<uint8_t>& aDecryptedData)
{
  MOZ_ASSERT(IsOnGMPThread());
  for (size_t i = 0; i < mDecryptionJobs.Length(); i++) {
    DecryptJob* job = mDecryptionJobs[i];
    if (job->mId == aId) {
      if (aDecryptedData.Length() != job->mSample->size) {
        NS_WARNING("CDM returned incorrect number of decrypted bytes");
      }
      if (GMP_SUCCEEDED(aResult)) {
        PodCopy(job->mSample->data,
                aDecryptedData.Elements(),
                std::min<size_t>(aDecryptedData.Length(), job->mSample->size));
        job->mClient->Decrypted(GMPNoErr, job->mSample.forget());
      } else if (aResult == GMPNoKeyErr) {
        NS_WARNING("CDM returned GMPNoKeyErr");
        // We still have the encrypted sample, so we can re-enqueue it to be
        // decrypted again once the key is usable again.
        job->mClient->Decrypted(GMPNoKeyErr, job->mSample.forget());
      } else {
        nsAutoCString str("CDM returned decode failure GMPErr=");
        str.AppendInt(aResult);
        NS_WARNING(str.get());
        job->mClient->Decrypted(aResult, nullptr);
      }
      mDecryptionJobs.RemoveElementAt(i);
      return;
    }
  }
  NS_WARNING("GMPDecryptorChild returned incorrect job ID");
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
