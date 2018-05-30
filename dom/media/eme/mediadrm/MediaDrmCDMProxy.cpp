/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/MediaDrmCDMProxy.h"
#include "MediaDrmCDMCallbackProxy.h"

using namespace mozilla::java::sdk;

namespace mozilla {

MediaDrmSessionType
ToMediaDrmSessionType(dom::MediaKeySessionType aSessionType)
{
  switch (aSessionType) {
    case dom::MediaKeySessionType::Temporary: return kKeyStreaming;
    case dom::MediaKeySessionType::Persistent_license: return kKeyOffline;
    default: return kKeyStreaming;
  };
}

MediaDrmCDMProxy::MediaDrmCDMProxy(dom::MediaKeys* aKeys,
                                   const nsAString& aKeySystem,
                                   bool aDistinctiveIdentifierRequired,
                                   bool aPersistentStateRequired,
                                   nsIEventTarget* aMainThread)
  : CDMProxy(aKeys,
             aKeySystem,
             aDistinctiveIdentifierRequired,
             aPersistentStateRequired,
             aMainThread)
  , mCDM(nullptr)
  , mShutdownCalled(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(MediaDrmCDMProxy);
}

MediaDrmCDMProxy::~MediaDrmCDMProxy()
{
  MOZ_COUNT_DTOR(MediaDrmCDMProxy);
}

void
MediaDrmCDMProxy::Init(PromiseId aPromiseId,
                       const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       const nsAString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  EME_LOG("MediaDrmCDMProxy::Init (%s, %s) %s",
          NS_ConvertUTF16toUTF8(aOrigin).get(),
          NS_ConvertUTF16toUTF8(aTopLevelOrigin).get(),
          NS_ConvertUTF16toUTF8(aName).get());

  // Create a thread to work with cdm.
  if (!mOwnerThread) {
    nsresult rv = NS_NewNamedThread("MDCDMThread", getter_AddRefs(mOwnerThread));
    if (NS_FAILED(rv)) {
      RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                    NS_LITERAL_CSTRING("Couldn't create CDM thread MediaDrmCDMProxy::Init"));
      return;
    }
  }

  mCDM = mozilla::MakeUnique<MediaDrmProxySupport>(mKeySystem);
  nsCOMPtr<nsIRunnable> task(NewRunnableMethod<uint32_t>("MediaDrmCDMProxy::md_Init",
                                                         this,
                                                         &MediaDrmCDMProxy::md_Init,
                                                         aPromiseId));
  mOwnerThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
MediaDrmCDMProxy::CreateSession(uint32_t aCreateSessionToken,
                                MediaKeySessionType aSessionType,
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
  data->mInitData = std::move(aInitData);

  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<UniquePtr<CreateSessionData>&&>("MediaDrmCDMProxy::md_CreateSession",
                                                      this,
                                                      &MediaDrmCDMProxy::md_CreateSession,
                                                      std::move(data)));
  mOwnerThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
MediaDrmCDMProxy::LoadSession(PromiseId aPromiseId,
                              dom::MediaKeySessionType aSessionType,
                              const nsAString& aSessionId)
{
  // TODO: Implement LoadSession.
  RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                NS_LITERAL_CSTRING("Currently Fennec does not support LoadSession"));
}

void
MediaDrmCDMProxy::SetServerCertificate(PromiseId aPromiseId,
                                     nsTArray<uint8_t>& aCert)
{
  // TODO: Implement SetServerCertificate.
  RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                NS_LITERAL_CSTRING("Currently Fennec does not support SetServerCertificate"));
}

void
MediaDrmCDMProxy::UpdateSession(const nsAString& aSessionId,
                              PromiseId aPromiseId,
                              nsTArray<uint8_t>& aResponse)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  UniquePtr<UpdateSessionData> data(new UpdateSessionData());
  data->mPromiseId = aPromiseId;
  data->mSessionId = NS_ConvertUTF16toUTF8(aSessionId);
  data->mResponse = std::move(aResponse);

  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<UniquePtr<UpdateSessionData>&&>("MediaDrmCDMProxy::md_UpdateSession",
                                                      this,
                                                      &MediaDrmCDMProxy::md_UpdateSession,
                                                      std::move(data)));
  mOwnerThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
MediaDrmCDMProxy::CloseSession(const nsAString& aSessionId,
                             PromiseId aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  UniquePtr<SessionOpData> data(new SessionOpData());
  data->mPromiseId = aPromiseId;
  data->mSessionId = NS_ConvertUTF16toUTF8(aSessionId);

  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<UniquePtr<SessionOpData>&&>("MediaDrmCDMProxy::md_CloseSession",
                                                  this,
                                                  &MediaDrmCDMProxy::md_CloseSession,
                                                  std::move(data)));
  mOwnerThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
MediaDrmCDMProxy::RemoveSession(const nsAString& aSessionId,
                              PromiseId aPromiseId)
{
  // TODO: Implement RemoveSession.
  RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                NS_LITERAL_CSTRING("Currently Fennec does not support RemoveSession"));
}

void
MediaDrmCDMProxy::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerThread);
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod("MediaDrmCDMProxy::md_Shutdown",
                      this, &MediaDrmCDMProxy::md_Shutdown));

  mOwnerThread->Dispatch(task, NS_DISPATCH_NORMAL);
  mOwnerThread->Shutdown();
  mOwnerThread = nullptr;
}

void
MediaDrmCDMProxy::Terminated()
{
  // TODO: Implement Terminated.
  // Should find a way to handle the case when remote side MediaDrm crashed.
}

const nsCString&
MediaDrmCDMProxy::GetNodeId() const
{
  return mNodeId;
}

void
MediaDrmCDMProxy::OnSetSessionId(uint32_t aCreateSessionToken,
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
MediaDrmCDMProxy::OnResolveLoadSessionPromise(uint32_t aPromiseId, bool aSuccess)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  mKeys->OnSessionLoaded(aPromiseId, aSuccess);
}

void
MediaDrmCDMProxy::OnSessionMessage(const nsAString& aSessionId,
                                   dom::MediaKeyMessageType aMessageType,
                                   const nsTArray<uint8_t>& aMessage)
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
MediaDrmCDMProxy::OnExpirationChange(const nsAString& aSessionId,
                                     UnixTime aExpiryTime)
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
MediaDrmCDMProxy::OnSessionClosed(const nsAString& aSessionId)
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
MediaDrmCDMProxy::OnSessionError(const nsAString& aSessionId,
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
}

void
MediaDrmCDMProxy::OnRejectPromise(uint32_t aPromiseId,
                                  nsresult aDOMException,
                                  const nsCString& aMsg)
{
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromise(aPromiseId, aDOMException, aMsg);
}

RefPtr<DecryptPromise>
MediaDrmCDMProxy::Decrypt(MediaRawData* aSample)
{
  MOZ_ASSERT_UNREACHABLE("Fennec could not handle decrypting individually");
  return nullptr;
}

void
MediaDrmCDMProxy::OnDecrypted(uint32_t aId,
                              DecryptStatus aResult,
                              const nsTArray<uint8_t>& aDecryptedData)
{
  MOZ_ASSERT_UNREACHABLE("Fennec could not handle decrypted event");
}

void
MediaDrmCDMProxy::RejectPromise(PromiseId aId, nsresult aCode,
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
MediaDrmCDMProxy::ResolvePromise(PromiseId aId)
{
  if (NS_IsMainThread()) {
    if (!mKeys.IsNull()) {
      mKeys->ResolvePromise(aId);
    } else {
      NS_WARNING("MediaDrmCDMProxy unable to resolve promise!");
    }
  } else {
    nsCOMPtr<nsIRunnable> task;
    task = NewRunnableMethod<PromiseId>("MediaDrmCDMProxy::ResolvePromise",
                                        this,
                                        &MediaDrmCDMProxy::ResolvePromise,
                                        aId);
    mMainThread->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
  }
}

const nsString&
MediaDrmCDMProxy::KeySystem() const
{
  return mKeySystem;
}

DataMutex<CDMCaps>&
MediaDrmCDMProxy::Capabilites()
{
  return mCapabilites;
}

void
MediaDrmCDMProxy::OnKeyStatusesChange(const nsAString& aSessionId)
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
MediaDrmCDMProxy::GetStatusForPolicy(PromiseId aPromiseId,
                                     const nsAString& aMinHdcpVersion)
{
  // TODO: Implement GetStatusForPolicy.
  RejectPromise(aPromiseId, NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                NS_LITERAL_CSTRING("Currently Fennec does not support GetStatusForPolicy"));
}

#ifdef DEBUG
bool
MediaDrmCDMProxy::IsOnOwnerThread()
{
  return NS_GetCurrentThread() == mOwnerThread;
}
#endif

const nsString&
MediaDrmCDMProxy::GetMediaDrmStubId() const
{
  MOZ_ASSERT(mCDM);
  return mCDM->GetMediaDrmStubId();
}

void
MediaDrmCDMProxy::OnCDMCreated(uint32_t aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }

  if (mCDM) {
    mKeys->OnCDMCreated(aPromiseId, 0);
    return;
  }

  // No CDM? Just reject the promise.
  mKeys->RejectPromise(aPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                       NS_LITERAL_CSTRING("Null CDM in OnCDMCreated()"));
}

void
MediaDrmCDMProxy::md_Init(uint32_t aPromiseId)
{
  MOZ_ASSERT(IsOnOwnerThread());
  MOZ_ASSERT(mCDM);

  mCallback.reset(new MediaDrmCDMCallbackProxy(this));
  mCDM->Init(mCallback.get());
  nsCOMPtr<nsIRunnable> task(
    NewRunnableMethod<uint32_t>("MediaDrmCDMProxy::OnCDMCreated",
                                this,
                                &MediaDrmCDMProxy::OnCDMCreated,
                                aPromiseId));
  mMainThread->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

void
MediaDrmCDMProxy::md_CreateSession(UniquePtr<CreateSessionData>&& aData)
{
  MOZ_ASSERT(IsOnOwnerThread());

  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in md_CreateSession"));
    return;
  }

  mCDM->CreateSession(aData->mCreateSessionToken,
                      aData->mPromiseId,
                      aData->mInitDataType,
                      aData->mInitData,
                      ToMediaDrmSessionType(aData->mSessionType));
}

void
MediaDrmCDMProxy::md_UpdateSession(UniquePtr<UpdateSessionData>&& aData)
{
  MOZ_ASSERT(IsOnOwnerThread());

  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in md_UpdateSession"));
    return;
  }
  mCDM->UpdateSession(aData->mPromiseId,
                      aData->mSessionId,
                      aData->mResponse);
}

void
MediaDrmCDMProxy::md_CloseSession(UniquePtr<SessionOpData>&& aData)
{
  MOZ_ASSERT(IsOnOwnerThread());

  if (!mCDM) {
    RejectPromise(aData->mPromiseId, NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in md_CloseSession"));
    return;
  }
  mCDM->CloseSession(aData->mPromiseId, aData->mSessionId);
}

void
MediaDrmCDMProxy::md_Shutdown()
{
  MOZ_ASSERT(IsOnOwnerThread());
  MOZ_ASSERT(mCDM);
  if (mShutdownCalled) {
    return;
  }
  mShutdownCalled = true;
  mCDM->Shutdown();
  mCDM = nullptr;
}

} // namespace mozilla
