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
#include "mozilla/dom/MediaKeySession.h"

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
  , mCDM(nullptr)
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

  EME_LOG("ChromiumCDMProxy::Init(%s, %s)",
          NS_ConvertUTF16toUTF8(aOrigin).get(),
          NS_ConvertUTF16toUTF8(aTopLevelOrigin).get());

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
          self->mCDM = cdm;
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
  // This should only be called once the CDM has been created.
  MOZ_ASSERT(mCDM);
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  mKeys->OnCDMCreated(aPromiseId, mCDM->PluginId());
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

  uint32_t sessionType = ToCDMSessionType(aSessionType);
  uint32_t initDataType = ToCDMInitDataType(aInitDataType);

  mGMPThread->Dispatch(
    NewRunnableMethod<uint32_t,
                      uint32_t,
                      uint32_t,
                      uint32_t,
                      nsTArray<uint8_t>>(mCDM,
                                         &gmp::ChromiumCDMParent::CreateSession,
                                         aCreateSessionToken,
                                         sessionType,
                                         initDataType,
                                         aPromiseId,
                                         Move(aInitData)));
}

void
ChromiumCDMProxy::LoadSession(PromiseId aPromiseId, const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());

  RejectPromise(aPromiseId,
                NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                NS_LITERAL_CSTRING("loadSession is not supported"));
}

void
ChromiumCDMProxy::SetServerCertificate(PromiseId aPromiseId,
                                       nsTArray<uint8_t>& aCert)
{
  MOZ_ASSERT(NS_IsMainThread());

  mGMPThread->Dispatch(NewRunnableMethod<uint32_t, nsTArray<uint8_t>>(
    mCDM,
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

  mGMPThread->Dispatch(NewRunnableMethod<nsCString, uint32_t, nsTArray<uint8_t>>(
      mCDM,
      &gmp::ChromiumCDMParent::UpdateSession,
      NS_ConvertUTF16toUTF8(aSessionId),
      aPromiseId,
      Move(aResponse)));
}

void
ChromiumCDMProxy::CloseSession(const nsAString& aSessionId,
                               PromiseId aPromiseId)
{
  mGMPThread->Dispatch(NewRunnableMethod<nsCString, uint32_t>(
    mCDM,
    &gmp::ChromiumCDMParent::CloseSession,
    NS_ConvertUTF16toUTF8(aSessionId),
    aPromiseId));
}

void
ChromiumCDMProxy::RemoveSession(const nsAString& aSessionId,
                                PromiseId aPromiseId)
{
  mGMPThread->Dispatch(NewRunnableMethod<nsCString, uint32_t>(
    mCDM,
    &gmp::ChromiumCDMParent::RemoveSession,
    NS_ConvertUTF16toUTF8(aSessionId),
    aPromiseId));
}

void
ChromiumCDMProxy::Shutdown()
{
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
  return DecryptPromise::CreateAndReject(DecryptResult(GenericErr, nullptr),
                                         __func__);
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

} // namespace mozilla
