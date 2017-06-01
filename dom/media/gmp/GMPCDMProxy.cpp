/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPCDMProxy.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/PodOperations.h"

#include "mozilla/dom/MediaKeys.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "prenv.h"
#include "GMPCDMCallbackProxy.h"
#include "GMPService.h"
#include "MainThreadUtils.h"
#include "MediaData.h"
#include "DecryptJob.h"
#include "GMPUtils.h"

namespace mozilla {

GMPCDMProxy::GMPCDMProxy(dom::MediaKeys* aKeys,
                         const nsAString& aKeySystem,
                         GMPCrashHelper* aCrashHelper,
                         bool aDistinctiveIdentifierRequired,
                         bool aPersistentStateRequired,
                         nsIEventTarget* aMainThread)
  : CDMProxy(aKeys,
             aKeySystem,
             aDistinctiveIdentifierRequired,
             aPersistentStateRequired,
             aMainThread)
  , mCrashHelper(aCrashHelper)
  , mCDM(nullptr)
  , mShutdownCalled(false)
  , mDecryptorId(0)
  , mCreatePromiseId(0)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(GMPCDMProxy);
}

GMPCDMProxy::~GMPCDMProxy()
{
  MOZ_COUNT_DTOR(GMPCDMProxy);
}

void
GMPCDMProxy::Init(PromiseId aPromiseId,
                  const nsAString& aOrigin,
                  const nsAString& aTopLevelOrigin,
                  const nsAString& aGMPName)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  EME_LOG("GMPCDMProxy::Init (%s, %s)",
          NS_ConvertUTF16toUTF8(aOrigin).get(),
          NS_ConvertUTF16toUTF8(aTopLevelOrigin).get());

  nsCString pluginVersion;
  if (!mOwnerThread) {
    nsCOMPtr<mozIGeckoMediaPluginService> mps =
      do_GetService("@mozilla.org/gecko-media-plugin-service;1");
    if (!mps) {
      RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                    NS_LITERAL_CSTRING("Couldn't get MediaPluginService in GMPCDMProxy::Init"));
      return;
    }
    mps->GetThread(getter_AddRefs(mOwnerThread));
    if (!mOwnerThread) {
      RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                    NS_LITERAL_CSTRING("Couldn't get GMP thread GMPCDMProxy::Init"));
      return;
    }
  }

  if (aGMPName.IsEmpty()) {
    RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
      nsPrintfCString("Unknown GMP for keysystem '%s'", NS_ConvertUTF16toUTF8(mKeySystem).get()));
    return;
  }

  UniquePtr<InitData> data(new InitData());
  data->mPromiseId = aPromiseId;
  data->mOrigin = aOrigin;
  data->mTopLevelOrigin = aTopLevelOrigin;
  data->mGMPName = aGMPName;
  data->mCrashHelper = mCrashHelper;
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<UniquePtr<InitData>&&>(this,
                                             &GMPCDMProxy::gmp_Init,
                                             Move(data)));
  mOwnerThread->EventTarget()->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

#ifdef DEBUG
bool
GMPCDMProxy::IsOnOwnerThread()
{
  return mOwnerThread->IsOnCurrentThread();
}
#endif

void
GMPCDMProxy::gmp_InitDone(GMPDecryptorProxy* aCDM, UniquePtr<InitData>&& aData)
{
  EME_LOG("GMPCDMProxy::gmp_InitDone");
  if (mShutdownCalled) {
    if (aCDM) {
      aCDM->Close();
    }
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("GMPCDMProxy was shut down before init could complete"));
    return;
  }
  if (!aCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("GetGMPDecryptor failed to return a CDM"));
    return;
  }

  mCDM = aCDM;
  mCallback.reset(new GMPCDMCallbackProxy(this, mMainThread));
  mCDM->Init(mCallback.get(),
             mDistinctiveIdentifierRequired,
             mPersistentStateRequired);

  // Await the OnSetDecryptorId callback.
  mCreatePromiseId = aData->mPromiseId;
}

void GMPCDMProxy::OnSetDecryptorId(uint32_t aId)
{
  MOZ_ASSERT(mCreatePromiseId);
  mDecryptorId = aId;
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<uint32_t>(this,
                                &GMPCDMProxy::OnCDMCreated,
                                mCreatePromiseId));
  mMainThread->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

class gmp_InitDoneCallback : public GetGMPDecryptorCallback
{
public:
  gmp_InitDoneCallback(GMPCDMProxy* aGMPCDMProxy,
                       UniquePtr<GMPCDMProxy::InitData>&& aData)
    : mGMPCDMProxy(aGMPCDMProxy),
      mData(Move(aData))
  {
  }

  void Done(GMPDecryptorProxy* aCDM)
  {
    mGMPCDMProxy->gmp_InitDone(aCDM, Move(mData));
  }

private:
  RefPtr<GMPCDMProxy> mGMPCDMProxy;
  UniquePtr<GMPCDMProxy::InitData> mData;
};

class gmp_InitGetGMPDecryptorCallback : public GetNodeIdCallback
{
public:
  gmp_InitGetGMPDecryptorCallback(GMPCDMProxy* aGMPCDMProxy,
                                  UniquePtr<GMPCDMProxy::InitData>&& aData)
    : mGMPCDMProxy(aGMPCDMProxy),
      mData(Move(aData))
  {
  }

  void Done(nsresult aResult, const nsACString& aNodeId)
  {
    mGMPCDMProxy->gmp_InitGetGMPDecryptor(aResult, aNodeId, Move(mData));
  }

private:
  RefPtr<GMPCDMProxy> mGMPCDMProxy;
  UniquePtr<GMPCDMProxy::InitData> mData;
};

void
GMPCDMProxy::gmp_Init(UniquePtr<InitData>&& aData)
{
  MOZ_ASSERT(IsOnOwnerThread());

  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (!mps) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Couldn't get MediaPluginService in GMPCDMProxy::gmp_Init"));
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
                               Move(callback));
  if (NS_FAILED(rv)) {
    RejectPromise(data.mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Call to GetNodeId() failed early"));
  }
}

void
GMPCDMProxy::gmp_InitGetGMPDecryptor(nsresult aResult,
                                     const nsACString& aNodeId,
                                     UniquePtr<InitData>&& aData)
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
                  NS_LITERAL_CSTRING("Couldn't get MediaPluginService in GMPCDMProxy::gmp_InitGetGMPDecryptor"));
    return;
  }

  EME_LOG("GMPCDMProxy::gmp_Init (%s, %s) NodeId=%s",
          NS_ConvertUTF16toUTF8(aData->mOrigin).get(),
          NS_ConvertUTF16toUTF8(aData->mTopLevelOrigin).get(),
          GetNodeId().get());

  nsTArray<nsCString> tags;
  tags.AppendElement(NS_ConvertUTF16toUTF8(mKeySystem));

  // Note: must capture helper refptr here, before the Move()
  // when we create the GetGMPDecryptorCallback below.
  RefPtr<GMPCrashHelper> crashHelper = Move(aData->mCrashHelper);
  UniquePtr<GetGMPDecryptorCallback> callback(new gmp_InitDoneCallback(this,
                                                                       Move(aData)));
  nsresult rv = mps->GetGMPDecryptor(crashHelper,
                                     &tags,
                                     GetNodeId(),
                                     Move(callback));
  if (NS_FAILED(rv)) {
    RejectPromise(promiseID, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Call to GetGMPDecryptor() failed early"));
  }
}

void
GMPCDMProxy::OnCDMCreated(uint32_t aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  MOZ_ASSERT(!GetNodeId().IsEmpty());
  if (mCDM) {
    mKeys->OnCDMCreated(aPromiseId, mCDM->GetPluginId());
  } else {
    // No CDM? Just reject the promise.
    mKeys->RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                         NS_LITERAL_CSTRING("Null CDM in OnCDMCreated()"));
  }
}

void
GMPCDMProxy::CreateSession(uint32_t aCreateSessionToken,
                           dom::MediaKeySessionType aSessionType,
                           PromiseId aPromiseId,
                           const nsAString& aInitDataType,
                           nsTArray<uint8_t>& aInitData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);

  UniquePtr<CreateSessionData> data(new CreateSessionData());
  data->mSessionType = aSessionType;
  data->mCreateSessionToken = aCreateSessionToken;
  data->mPromiseId = aPromiseId;
  data->mInitDataType = NS_ConvertUTF16toUTF8(aInitDataType);
  data->mInitData = Move(aInitData);

  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<UniquePtr<CreateSessionData>&&>(this,
                                                      &GMPCDMProxy::gmp_CreateSession,
                                                      Move(data)));
  mOwnerThread->EventTarget()->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

GMPSessionType
ToGMPSessionType(dom::MediaKeySessionType aSessionType) {
  switch (aSessionType) {
    case dom::MediaKeySessionType::Temporary: return kGMPTemporySession;
    case dom::MediaKeySessionType::Persistent_license: return kGMPPersistentSession;
    default: return kGMPTemporySession;
  };
};

void
GMPCDMProxy::gmp_CreateSession(UniquePtr<CreateSessionData>&& aData)
{
  MOZ_ASSERT(IsOnOwnerThread());
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
GMPCDMProxy::LoadSession(PromiseId aPromiseId,
                         dom::MediaKeySessionType aSessionType,
                         const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);

  UniquePtr<SessionOpData> data(new SessionOpData());
  data->mPromiseId = aPromiseId;
  data->mSessionId = NS_ConvertUTF16toUTF8(aSessionId);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<UniquePtr<SessionOpData>&&>(this,
                                                  &GMPCDMProxy::gmp_LoadSession,
                                                  Move(data)));
  mOwnerThread->EventTarget()->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

void
GMPCDMProxy::gmp_LoadSession(UniquePtr<SessionOpData>&& aData)
{
  MOZ_ASSERT(IsOnOwnerThread());

  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in gmp_LoadSession"));
    return;
  }
  mCDM->LoadSession(aData->mPromiseId, aData->mSessionId);
}

void
GMPCDMProxy::SetServerCertificate(PromiseId aPromiseId,
                                  nsTArray<uint8_t>& aCert)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);

  UniquePtr<SetServerCertificateData> data(new SetServerCertificateData());
  data->mPromiseId = aPromiseId;
  data->mCert = Move(aCert);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<UniquePtr<SetServerCertificateData>&&>(this,
                                                             &GMPCDMProxy::gmp_SetServerCertificate,
                                                             Move(data)));
  mOwnerThread->EventTarget()->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

void
GMPCDMProxy::gmp_SetServerCertificate(UniquePtr<SetServerCertificateData>&& aData)
{
  MOZ_ASSERT(IsOnOwnerThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in gmp_SetServerCertificate"));
    return;
  }
  mCDM->SetServerCertificate(aData->mPromiseId, aData->mCert);
}

void
GMPCDMProxy::UpdateSession(const nsAString& aSessionId,
                           PromiseId aPromiseId,
                           nsTArray<uint8_t>& aResponse)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  UniquePtr<UpdateSessionData> data(new UpdateSessionData());
  data->mPromiseId = aPromiseId;
  data->mSessionId = NS_ConvertUTF16toUTF8(aSessionId);
  data->mResponse = Move(aResponse);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<UniquePtr<UpdateSessionData>&&>(this,
                                                      &GMPCDMProxy::gmp_UpdateSession,
                                                      Move(data)));
  mOwnerThread->EventTarget()->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

void
GMPCDMProxy::gmp_UpdateSession(UniquePtr<UpdateSessionData>&& aData)
{
  MOZ_ASSERT(IsOnOwnerThread());
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
GMPCDMProxy::CloseSession(const nsAString& aSessionId,
                          PromiseId aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  UniquePtr<SessionOpData> data(new SessionOpData());
  data->mPromiseId = aPromiseId;
  data->mSessionId = NS_ConvertUTF16toUTF8(aSessionId);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<UniquePtr<SessionOpData>&&>(this,
                                                  &GMPCDMProxy::gmp_CloseSession,
                                                  Move(data)));
  mOwnerThread->EventTarget()->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

void
GMPCDMProxy::gmp_CloseSession(UniquePtr<SessionOpData>&& aData)
{
  MOZ_ASSERT(IsOnOwnerThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in gmp_CloseSession"));
    return;
  }
  mCDM->CloseSession(aData->mPromiseId, aData->mSessionId);
}

void
GMPCDMProxy::RemoveSession(const nsAString& aSessionId,
                           PromiseId aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  UniquePtr<SessionOpData> data(new SessionOpData());
  data->mPromiseId = aPromiseId;
  data->mSessionId = NS_ConvertUTF16toUTF8(aSessionId);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<UniquePtr<SessionOpData>&&>(this,
                                                  &GMPCDMProxy::gmp_RemoveSession,
                                                  Move(data)));
  mOwnerThread->EventTarget()->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

void
GMPCDMProxy::gmp_RemoveSession(UniquePtr<SessionOpData>&& aData)
{
  MOZ_ASSERT(IsOnOwnerThread());
  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in gmp_RemoveSession"));
    return;
  }
  mCDM->RemoveSession(aData->mPromiseId, aData->mSessionId);
}

void
GMPCDMProxy::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  mKeys.Clear();
  // Note: This may end up being the last owning reference to the GMPCDMProxy.
  nsCOMPtr<nsIRunnable> task(NewRunnableMethod(this, &GMPCDMProxy::gmp_Shutdown));
  if (mOwnerThread) {
    mOwnerThread->EventTarget()->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
  }
}

void
GMPCDMProxy::gmp_Shutdown()
{
  MOZ_ASSERT(IsOnOwnerThread());

  mShutdownCalled = true;

  // Abort any pending decrypt jobs, to awaken any clients waiting on a job.
  for (size_t i = 0; i < mDecryptionJobs.Length(); i++) {
    DecryptJob* job = mDecryptionJobs[i];
    job->PostResult(eme::AbortedErr);
  }
  mDecryptionJobs.Clear();

  if (mCDM) {
    mCDM->Close();
    mCDM = nullptr;
  }
}

void
GMPCDMProxy::RejectPromise(PromiseId aId, nsresult aCode,
                           const nsCString& aReason)
{
  if (NS_IsMainThread()) {
    if (!mKeys.IsNull()) {
      mKeys->RejectPromise(aId, aCode, aReason);
    }
  } else {
    nsCOMPtr<nsIRunnable> task(new RejectPromiseTask(this, aId, aCode,
                                                     aReason));
    mMainThread->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
  }
}

void
GMPCDMProxy::ResolvePromise(PromiseId aId)
{
  if (NS_IsMainThread()) {
    if (!mKeys.IsNull()) {
      mKeys->ResolvePromise(aId);
    } else {
      NS_WARNING("GMPCDMProxy unable to resolve promise!");
    }
  } else {
    nsCOMPtr<nsIRunnable> task;
    task = NewRunnableMethod<PromiseId>(this,
                                        &GMPCDMProxy::ResolvePromise,
                                        aId);
    mMainThread->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
  }
}

const nsCString&
GMPCDMProxy::GetNodeId() const
{
  return mNodeId;
}

void
GMPCDMProxy::OnSetSessionId(uint32_t aCreateSessionToken,
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
GMPCDMProxy::OnResolveLoadSessionPromise(uint32_t aPromiseId, bool aSuccess)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  mKeys->OnSessionLoaded(aPromiseId, aSuccess);
}

void
GMPCDMProxy::OnSessionMessage(const nsAString& aSessionId,
                              dom::MediaKeyMessageType aMessageType,
                              nsTArray<uint8_t>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyMessage(aMessageType, aMessage);
  }
}

void
GMPCDMProxy::OnKeyStatusesChange(const nsAString& aSessionId)
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
GMPCDMProxy::OnExpirationChange(const nsAString& aSessionId,
                                GMPTimestamp aExpiryTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    // Expiry of 0 is interpreted as "never expire". See bug 1345341.
    double t = (aExpiryTime == 0) ? std::numeric_limits<double>::quiet_NaN()
                                  : static_cast<double>(aExpiryTime);
    session->SetExpiration(t);
  }
}

void
GMPCDMProxy::OnSessionClosed(const nsAString& aSessionId)
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

void
GMPCDMProxy::OnDecrypted(uint32_t aId,
                         DecryptStatus aResult,
                         const nsTArray<uint8_t>& aDecryptedData)
{
  MOZ_ASSERT(IsOnOwnerThread());
  gmp_Decrypted(aId, aResult, aDecryptedData);
}

void
GMPCDMProxy::OnSessionError(const nsAString& aSessionId,
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
GMPCDMProxy::OnRejectPromise(uint32_t aPromiseId,
                          nsresult aDOMException,
                          const nsCString& aMsg)
{
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromise(aPromiseId, aDOMException, aMsg);
}

const nsString&
GMPCDMProxy::KeySystem() const
{
  return mKeySystem;
}

CDMCaps&
GMPCDMProxy::Capabilites() {
  return mCapabilites;
}

RefPtr<DecryptPromise>
GMPCDMProxy::Decrypt(MediaRawData* aSample)
{
  RefPtr<DecryptJob> job(new DecryptJob(aSample));
  RefPtr<DecryptPromise> promise(job->Ensure());

  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<RefPtr<DecryptJob>>(this, &GMPCDMProxy::gmp_Decrypt, job));
  mOwnerThread->EventTarget()->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
  return promise;
}

void
GMPCDMProxy::gmp_Decrypt(RefPtr<DecryptJob> aJob)
{
  MOZ_ASSERT(IsOnOwnerThread());

  if (!mCDM) {
    aJob->PostResult(eme::AbortedErr);
    return;
  }

  nsTArray<uint8_t> data;
  data.AppendElements(aJob->mSample->Data(), aJob->mSample->Size());
  mCDM->Decrypt(aJob->mId, aJob->mSample->mCrypto, data);
  mDecryptionJobs.AppendElement(aJob.forget());
}

void
GMPCDMProxy::gmp_Decrypted(uint32_t aId,
                           DecryptStatus aResult,
                           const nsTArray<uint8_t>& aDecryptedData)
{
  MOZ_ASSERT(IsOnOwnerThread());
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
GMPCDMProxy::GetSessionIdsForKeyId(const nsTArray<uint8_t>& aKeyId,
                                   nsTArray<nsCString>& aSessionIds)
{
  CDMCaps::AutoLock caps(Capabilites());
  caps.GetSessionIdsForKeyId(aKeyId, aSessionIds);
}

void
GMPCDMProxy::Terminated()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_WARNING("CDM terminated");
  if (mCreatePromiseId) {
    RejectPromise(mCreatePromiseId,
                  NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  NS_LITERAL_CSTRING("Crashed waiting for CDM to initialize"));
    mCreatePromiseId = 0;
  }
  if (!mKeys.IsNull()) {
    mKeys->Terminated();
  }
}

uint32_t
GMPCDMProxy::GetDecryptorId()
{
  return mDecryptorId;
}

} // namespace mozilla
