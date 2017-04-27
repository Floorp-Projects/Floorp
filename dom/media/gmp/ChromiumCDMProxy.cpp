/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMProxy.h"
#include "mozilla/dom/MediaKeySession.h"
#include "GMPUtils.h"
#include "nsPrintfCString.h"
#include "GMPService.h"

namespace mozilla {

ChromiumCDMProxy::ChromiumCDMProxy(dom::MediaKeys* aKeys,
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
  , mCDMMutex("ChromiumCDMProxy")
  , mGMPThread(GetGMPAbstractThread())
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(ChromiumCDMProxy);
}

ChromiumCDMProxy::~ChromiumCDMProxy()
{
  MOZ_COUNT_DTOR(ChromiumCDMProxy);
}

void
ChromiumCDMProxy::Init(PromiseId aPromiseId,
                       const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       const nsAString& aGMPName)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  EME_LOG(
    "ChromiumCDMProxy::Init (pid=%u, origin=%s, topLevelOrigin=%s, gmp=%s)",
    aPromiseId,
    NS_ConvertUTF16toUTF8(aOrigin).get(),
    NS_ConvertUTF16toUTF8(aTopLevelOrigin).get(),
    NS_ConvertUTF16toUTF8(aGMPName).get());

  if (!mGMPThread) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Couldn't get GMP thread ChromiumCDMProxy::Init"));
    return;
  }

  if (aGMPName.IsEmpty()) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  nsPrintfCString("Unknown GMP for keysystem '%s'",
                                  NS_ConvertUTF16toUTF8(mKeySystem).get()));
    return;
  }

  gmp::NodeId nodeId(aOrigin, aTopLevelOrigin, aGMPName);
  RefPtr<AbstractThread> thread = mGMPThread;
  RefPtr<GMPCrashHelper> helper(mCrashHelper);
  RefPtr<ChromiumCDMProxy> self(this);
  nsCString keySystem = NS_ConvertUTF16toUTF8(mKeySystem);
  RefPtr<Runnable> task(NS_NewRunnableFunction(
    [self, nodeId, helper, aPromiseId, thread, keySystem]() -> void {
      MOZ_ASSERT(self->IsOnOwnerThread());

      RefPtr<gmp::GeckoMediaPluginService> service =
        gmp::GeckoMediaPluginService::GetGeckoMediaPluginService();
      if (!service) {
        self->RejectPromise(
          aPromiseId,
          NS_ERROR_DOM_INVALID_STATE_ERR,
          NS_LITERAL_CSTRING(
            "Couldn't get GeckoMediaPluginService in ChromiumCDMProxy::Init"));
        return;
      }
      RefPtr<gmp::GetCDMParentPromise> promise =
        service->GetCDM(nodeId, { keySystem }, helper);
      promise->Then(
        thread,
        __func__,
        [self, aPromiseId](RefPtr<gmp::ChromiumCDMParent> cdm) {
          if (!cdm->Init(self,
                         self->mDistinctiveIdentifierRequired,
                         self->mPersistentStateRequired)) {
            self->RejectPromise(aPromiseId,
                                NS_ERROR_FAILURE,
                                NS_LITERAL_CSTRING("GetCDM failed."));
            return;
          }
          {
            MutexAutoLock lock(self->mCDMMutex);
            self->mCDM = cdm;
          }
          self->OnCDMCreated(aPromiseId);
        },
        [self, aPromiseId](nsresult rv) {
          self->RejectPromise(
            aPromiseId, NS_ERROR_FAILURE, NS_LITERAL_CSTRING("GetCDM failed."));
        });
    }));

  mGMPThread->Dispatch(task.forget());
}

void
ChromiumCDMProxy::OnCDMCreated(uint32_t aPromiseId)
{
  EME_LOG("ChromiumCDMProxy::OnCDMCreated(pid=%u) isMainThread=%d this=%p",
          aPromiseId,
          NS_IsMainThread(),
          this);

  if (!NS_IsMainThread()) {
    mMainThread->Dispatch(NewRunnableMethod<PromiseId>(
                            this, &ChromiumCDMProxy::OnCDMCreated, aPromiseId),
                          NS_DISPATCH_NORMAL);
    return;
  }
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  // This should only be called once the CDM has been created.
  MOZ_ASSERT(cdm);
  if (cdm) {
    mKeys->OnCDMCreated(aPromiseId, cdm->PluginId());
  } else {
    // No CDM? Shouldn't be possible, but reject the promise anyway...
    mKeys->RejectPromise(aPromiseId,
                         NS_ERROR_DOM_INVALID_STATE_ERR,
                         NS_LITERAL_CSTRING("Null CDM in OnCDMCreated()"));
  }
}

#ifdef DEBUG
bool
ChromiumCDMProxy::IsOnOwnerThread()
{
  return mGMPThread->IsCurrentThreadIn();
}
#endif

static uint32_t
ToCDMSessionType(dom::MediaKeySessionType aSessionType)
{
  switch (aSessionType) {
    case dom::MediaKeySessionType::Temporary:
      return static_cast<uint32_t>(cdm::kTemporary);
    case dom::MediaKeySessionType::Persistent_license:
      return static_cast<uint32_t>(cdm::kPersistentLicense);
    default:
      return static_cast<uint32_t>(cdm::kTemporary);
  };
};

static uint32_t
ToCDMInitDataType(const nsAString& aInitDataType)
{
  if (aInitDataType.EqualsLiteral("cenc")) {
    return static_cast<uint32_t>(cdm::kCenc);
  }
  if (aInitDataType.EqualsLiteral("webm")) {
    return static_cast<uint32_t>(cdm::kWebM);
  }
  if (aInitDataType.EqualsLiteral("keyids")) {
    return static_cast<uint32_t>(cdm::kKeyIds);
  }
  return static_cast<uint32_t>(cdm::kCenc);
}

void
ChromiumCDMProxy::CreateSession(uint32_t aCreateSessionToken,
                                dom::MediaKeySessionType aSessionType,
                                PromiseId aPromiseId,
                                const nsAString& aInitDataType,
                                nsTArray<uint8_t>& aInitData)
{
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::CreateSession(token=%u, type=%d, pid=%u) "
          "initDataLen=%zu",
          aCreateSessionToken,
          (int)aSessionType,
          aPromiseId,
          aInitData.Length());

  uint32_t sessionType = ToCDMSessionType(aSessionType);
  uint32_t initDataType = ToCDMInitDataType(aInitDataType);

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in CreateSession"));
    return;
  }

  mGMPThread->Dispatch(
    NewRunnableMethod<uint32_t,
                      uint32_t,
                      uint32_t,
                      uint32_t,
                      nsTArray<uint8_t>>(cdm,
                                         &gmp::ChromiumCDMParent::CreateSession,
                                         aCreateSessionToken,
                                         sessionType,
                                         initDataType,
                                         aPromiseId,
                                         Move(aInitData)));
}

void
ChromiumCDMProxy::LoadSession(PromiseId aPromiseId,
                              dom::MediaKeySessionType aSessionType,
                              const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in LoadSession"));
    return;
  }

  mGMPThread->Dispatch(NewRunnableMethod<uint32_t, uint32_t, nsString>(
    cdm,
    &gmp::ChromiumCDMParent::LoadSession,
    aPromiseId,
    ToCDMSessionType(aSessionType),
    aSessionId));
}

void
ChromiumCDMProxy::SetServerCertificate(PromiseId aPromiseId,
                                       nsTArray<uint8_t>& aCert)
{
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::SetServerCertificate(pid=%u) certLen=%zu",
          aPromiseId,
          aCert.Length());

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in SetServerCertificate"));
    return;
  }

  mGMPThread->Dispatch(NewRunnableMethod<uint32_t, nsTArray<uint8_t>>(
    cdm,
    &gmp::ChromiumCDMParent::SetServerCertificate,
    aPromiseId,
    Move(aCert)));
}

void
ChromiumCDMProxy::UpdateSession(const nsAString& aSessionId,
                                PromiseId aPromiseId,
                                nsTArray<uint8_t>& aResponse)
{
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::UpdateSession(sid='%s', pid=%u) responseLen=%zu",
          NS_ConvertUTF16toUTF8(aSessionId).get(),
          aPromiseId,
          aResponse.Length());

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in UpdateSession"));
    return;
  }
  mGMPThread->Dispatch(
    NewRunnableMethod<nsCString, uint32_t, nsTArray<uint8_t>>(
      cdm,
      &gmp::ChromiumCDMParent::UpdateSession,
      NS_ConvertUTF16toUTF8(aSessionId),
      aPromiseId,
      Move(aResponse)));
}

void
ChromiumCDMProxy::CloseSession(const nsAString& aSessionId,
                               PromiseId aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::CloseSession(sid='%s', pid=%u)",
          NS_ConvertUTF16toUTF8(aSessionId).get(),
          aPromiseId);

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in CloseSession"));
    return;
  }
  mGMPThread->Dispatch(NewRunnableMethod<nsCString, uint32_t>(
    cdm,
    &gmp::ChromiumCDMParent::CloseSession,
    NS_ConvertUTF16toUTF8(aSessionId),
    aPromiseId));
}

void
ChromiumCDMProxy::RemoveSession(const nsAString& aSessionId,
                                PromiseId aPromiseId)
{
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::RemoveSession(sid='%s', pid=%u)",
          NS_ConvertUTF16toUTF8(aSessionId).get(),
          aPromiseId);

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Null CDM in RemoveSession"));
    return;
  }
  mGMPThread->Dispatch(NewRunnableMethod<nsCString, uint32_t>(
    cdm,
    &gmp::ChromiumCDMParent::RemoveSession,
    NS_ConvertUTF16toUTF8(aSessionId),
    aPromiseId));
}

void
ChromiumCDMProxy::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::Shutdown()");
  mKeys.Clear();
  RefPtr<gmp::ChromiumCDMParent> cdm;
  {
    MutexAutoLock lock(mCDMMutex);
    cdm.swap(mCDM);
  }
  if (cdm) {
    // We need to keep this proxy alive until the parent has finished its
    // Shutdown (as it may still try to use the proxy until then).
    RefPtr<ChromiumCDMProxy> self(this);
    nsCOMPtr<nsIRunnable> task =
      NS_NewRunnableFunction([self, cdm]() { cdm->Shutdown(); });
    mGMPThread->Dispatch(task.forget());
  }
}

void
ChromiumCDMProxy::RejectPromise(PromiseId aId,
                                nsresult aCode,
                                const nsCString& aReason)
{
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task;
    task = NewRunnableMethod<PromiseId, nsresult, nsCString>(
      this, &ChromiumCDMProxy::RejectPromise, aId, aCode, aReason);
    NS_DispatchToMainThread(task);
    return;
  }
  EME_LOG("ChromiumCDMProxy::RejectPromise(pid=%u, code=0x%x, reason='%s')",
          aId,
          static_cast<uint32_t>(aCode),
          aReason.get());
  if (!mKeys.IsNull()) {
    mKeys->RejectPromise(aId, aCode, aReason);
  }
}

void
ChromiumCDMProxy::ResolvePromise(PromiseId aId)
{
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task;
    task = NewRunnableMethod<PromiseId>(
      this, &ChromiumCDMProxy::ResolvePromise, aId);
    NS_DispatchToMainThread(task);
    return;
  }

  EME_LOG("ChromiumCDMProxy::ResolvePromise(pid=%u)", aId);
  if (!mKeys.IsNull()) {
    mKeys->ResolvePromise(aId);
  } else {
    NS_WARNING("ChromiumCDMProxy unable to resolve promise!");
  }
}

const nsCString&
ChromiumCDMProxy::GetNodeId() const
{
  return mNodeId;
}

void
ChromiumCDMProxy::OnSetSessionId(uint32_t aCreateSessionToken,
                                 const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::OnSetSessionId(token=%u, sid='%s')",
          aCreateSessionToken,
          NS_ConvertUTF16toUTF8(aSessionId).get());

  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(
    mKeys->GetPendingSession(aCreateSessionToken));
  if (session) {
    session->SetSessionId(aSessionId);
  }
}

void
ChromiumCDMProxy::OnResolveLoadSessionPromise(uint32_t aPromiseId,
                                              bool aSuccess)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  mKeys->OnSessionLoaded(aPromiseId, aSuccess);
}

void
ChromiumCDMProxy::OnSessionMessage(const nsAString& aSessionId,
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
ChromiumCDMProxy::OnKeyStatusesChange(const nsAString& aSessionId)
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
ChromiumCDMProxy::OnExpirationChange(const nsAString& aSessionId,
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
ChromiumCDMProxy::OnSessionClosed(const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());

  bool keyStatusesChange = false;
  {
    CDMCaps::AutoLock caps(Capabilites());
    keyStatusesChange = caps.RemoveKeysForSession(nsString(aSessionId));
  }
  if (keyStatusesChange) {
    OnKeyStatusesChange(aSessionId);
  }
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->OnClosed();
  }
}

void
ChromiumCDMProxy::OnDecrypted(uint32_t aId,
                              DecryptStatus aResult,
                              const nsTArray<uint8_t>& aDecryptedData)
{
}

void
ChromiumCDMProxy::OnSessionError(const nsAString& aSessionId,
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
ChromiumCDMProxy::OnRejectPromise(uint32_t aPromiseId,
                                  nsresult aDOMException,
                                  const nsCString& aMsg)
{
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromise(aPromiseId, aDOMException, aMsg);
}

const nsString&
ChromiumCDMProxy::KeySystem() const
{
  return mKeySystem;
}

CDMCaps&
ChromiumCDMProxy::Capabilites()
{
  return mCapabilites;
}

RefPtr<DecryptPromise>
ChromiumCDMProxy::Decrypt(MediaRawData* aSample)
{
  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    return DecryptPromise::CreateAndReject(DecryptResult(AbortedErr, aSample),
                                           __func__);
  }
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(
    mGMPThread, __func__, [cdm, sample]() { return cdm->Decrypt(sample); });
}

void
ChromiumCDMProxy::GetSessionIdsForKeyId(const nsTArray<uint8_t>& aKeyId,
                                        nsTArray<nsCString>& aSessionIds)
{
  CDMCaps::AutoLock caps(Capabilites());
  caps.GetSessionIdsForKeyId(aKeyId, aSessionIds);
}

void
ChromiumCDMProxy::Terminated()
{
  if (!mKeys.IsNull()) {
    mKeys->Terminated();
  }
}

already_AddRefed<gmp::ChromiumCDMParent>
ChromiumCDMProxy::GetCDMParent()
{
  MutexAutoLock lock(mCDMMutex);
  RefPtr<gmp::ChromiumCDMParent> cdm = mCDM;
  return cdm.forget();
}

} // namespace mozilla
