/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMProxy.h"
#include "ChromiumCDMCallbackProxy.h"
#include "MediaResult.h"
#include "mozilla/dom/MediaKeySession.h"
#include "GMPUtils.h"
#include "nsPrintfCString.h"
#include "GMPService.h"
#include "content_decryption_module.h"

#define NS_DispatchToMainThread(...) CompileError_UseAbstractMainThreadInstead

namespace mozilla {

ChromiumCDMProxy::ChromiumCDMProxy(dom::MediaKeys* aKeys,
                                   const nsAString& aKeySystem,
                                   GMPCrashHelper* aCrashHelper,
                                   bool aDistinctiveIdentifierRequired,
                                   bool aPersistentStateRequired)
    : CDMProxy(aKeys, aKeySystem, aDistinctiveIdentifierRequired,
               aPersistentStateRequired),
      mCrashHelper(aCrashHelper),
      mCDMMutex("ChromiumCDMProxy"),
      mGMPThread(GetGMPThread()) {
  MOZ_ASSERT(NS_IsMainThread());
}

ChromiumCDMProxy::~ChromiumCDMProxy() {
  EME_LOG("ChromiumCDMProxy::~ChromiumCDMProxy(this=%p)", this);
}

void ChromiumCDMProxy::Init(PromiseId aPromiseId, const nsAString& aOrigin,
                            const nsAString& aTopLevelOrigin,
                            const nsAString& aGMPName) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<GMPCrashHelper> helper(std::move(mCrashHelper));

  NS_ENSURE_TRUE_VOID(!mKeys.IsNull());

  EME_LOG("ChromiumCDMProxy::Init(this=%p, pid=%" PRIu32
          ", origin=%s, topLevelOrigin=%s, "
          "gmp=%s)",
          this, aPromiseId, NS_ConvertUTF16toUTF8(aOrigin).get(),
          NS_ConvertUTF16toUTF8(aTopLevelOrigin).get(),
          NS_ConvertUTF16toUTF8(aGMPName).get());

  if (!mGMPThread) {
    RejectPromiseWithStateError(
        aPromiseId, "Couldn't get GMP thread ChromiumCDMProxy::Init"_ns);
    return;
  }

  if (aGMPName.IsEmpty()) {
    RejectPromiseWithStateError(
        aPromiseId, nsPrintfCString("Unknown GMP for keysystem '%s'",
                                    NS_ConvertUTF16toUTF8(mKeySystem).get()));
    return;
  }

  gmp::NodeIdParts nodeIdParts{nsString(aOrigin), nsString(aTopLevelOrigin),
                               nsString(aGMPName)};
  nsCOMPtr<nsISerialEventTarget> thread = mGMPThread;
  RefPtr<ChromiumCDMProxy> self(this);
  nsCString keySystem = NS_ConvertUTF16toUTF8(mKeySystem);
  RefPtr<Runnable> task(NS_NewRunnableFunction(
      "ChromiumCDMProxy::Init",
      [self, nodeIdParts, helper, aPromiseId, thread, keySystem]() -> void {
        MOZ_ASSERT(self->IsOnOwnerThread());

        RefPtr<gmp::GeckoMediaPluginService> service =
            gmp::GeckoMediaPluginService::GetGeckoMediaPluginService();
        if (!service) {
          self->RejectPromiseWithStateError(
              aPromiseId,
              nsLiteralCString("Couldn't get GeckoMediaPluginService in "
                               "ChromiumCDMProxy::Init"));
          return;
        }
        RefPtr<gmp::GetCDMParentPromise> promise =
            service->GetCDM(nodeIdParts, {keySystem}, helper);
        promise->Then(
            thread, __func__,
            [self, aPromiseId, thread](RefPtr<gmp::ChromiumCDMParent> cdm) {
              // service->GetCDM succeeded
              self->mCallback =
                  MakeUnique<ChromiumCDMCallbackProxy>(self, self->mMainThread);
              cdm->Init(self->mCallback.get(),
                        self->mDistinctiveIdentifierRequired,
                        self->mPersistentStateRequired, self->mMainThread)
                  ->Then(
                      self->mMainThread, __func__,
                      [self, aPromiseId, cdm](bool /* unused */) {
                        // CDM init succeeded
                        {
                          MutexAutoLock lock(self->mCDMMutex);
                          self->mCDM = cdm;
                        }
                        if (self->mIsShutdown) {
                          self->RejectPromiseWithStateError(
                              aPromiseId, nsLiteralCString(
                                              "ChromiumCDMProxy shutdown "
                                              "during ChromiumCDMProxy::Init"));
                          // If shutdown happened while waiting to init, we
                          // need to explicitly shutdown the CDM to avoid it
                          // referencing this proxy which is on its way out.
                          self->ShutdownCDMIfExists();
                          return;
                        }
                        self->OnCDMCreated(aPromiseId);
                      },
                      [self, aPromiseId](MediaResult aResult) {
                        // CDM init failed.
                        ErrorResult rv;
                        // XXXbz MediaResult should really store a
                        // CopyableErrorResult or something.  See
                        // <https://bugzilla.mozilla.org/show_bug.cgi?id=1612216>.
                        rv.Throw(aResult.Code());
                        self->RejectPromise(aPromiseId, std::move(rv),
                                            aResult.Message());
                      });
            },
            [self, aPromiseId](MediaResult rv) {
              // service->GetCDM failed
              ErrorResult result;
              // XXXbz MediaResult should really store a CopyableErrorResult or
              // something.  See
              // <https://bugzilla.mozilla.org/show_bug.cgi?id=1612216>.
              result.Throw(rv.Code());
              self->RejectPromise(aPromiseId, std::move(result),
                                  rv.Description());
            });
      }));

  mGMPThread->Dispatch(task.forget());
}

void ChromiumCDMProxy::OnCDMCreated(uint32_t aPromiseId) {
  EME_LOG("ChromiumCDMProxy::OnCDMCreated(this=%p, pid=%" PRIu32
          ") isMainThread=%d",
          this, aPromiseId, NS_IsMainThread());
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
    constexpr auto err = "Null CDM in OnCDMCreated()"_ns;
    ErrorResult rv;
    rv.ThrowInvalidStateError(err);
    mKeys->RejectPromise(aPromiseId, std::move(rv), err);
  }
}

void ChromiumCDMProxy::ShutdownCDMIfExists() {
  EME_LOG(
      "ChromiumCDMProxy::ShutdownCDMIfExists(this=%p) mCDM=%p, mIsShutdown=%s",
      this, mCDM.get(), mIsShutdown ? "true" : "false");
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mGMPThread);
  MOZ_ASSERT(mIsShutdown,
             "Should only shutdown the CDM if the proxy is shutting down");
  RefPtr<gmp::ChromiumCDMParent> cdm;
  {
    MutexAutoLock lock(mCDMMutex);
    cdm.swap(mCDM);
  }
  if (cdm) {
    // We need to keep this proxy alive until the parent has finished its
    // Shutdown (as it may still try to use the proxy until then).
    RefPtr<ChromiumCDMProxy> self(this);
    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
        "ChromiumCDMProxy::Shutdown", [self, cdm]() { cdm->Shutdown(); });
    mGMPThread->Dispatch(task.forget());
  }
}

#ifdef DEBUG
bool ChromiumCDMProxy::IsOnOwnerThread() {
  return mGMPThread && mGMPThread->IsOnCurrentThread();
}
#endif

static uint32_t ToCDMSessionType(dom::MediaKeySessionType aSessionType) {
  switch (aSessionType) {
    case dom::MediaKeySessionType::Temporary:
      return static_cast<uint32_t>(cdm::kTemporary);
    case dom::MediaKeySessionType::Persistent_license:
      return static_cast<uint32_t>(cdm::kPersistentLicense);
    default:
      return static_cast<uint32_t>(cdm::kTemporary);
  };
};

static uint32_t ToCDMInitDataType(const nsAString& aInitDataType) {
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

void ChromiumCDMProxy::CreateSession(uint32_t aCreateSessionToken,
                                     dom::MediaKeySessionType aSessionType,
                                     PromiseId aPromiseId,
                                     const nsAString& aInitDataType,
                                     nsTArray<uint8_t>& aInitData) {
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::CreateSession(this=%p, token=%" PRIu32
          ", type=%d, pid=%" PRIu32
          ") "
          "initDataLen=%zu",
          this, aCreateSessionToken, (int)aSessionType, aPromiseId,
          aInitData.Length());

  uint32_t sessionType = ToCDMSessionType(aSessionType);
  uint32_t initDataType = ToCDMInitDataType(aInitDataType);

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromiseWithStateError(aPromiseId, "Null CDM in CreateSession"_ns);
    return;
  }

  mGMPThread->Dispatch(NewRunnableMethod<uint32_t, uint32_t, uint32_t, uint32_t,
                                         nsTArray<uint8_t>>(
      "gmp::ChromiumCDMParent::CreateSession", cdm,
      &gmp::ChromiumCDMParent::CreateSession, aCreateSessionToken, sessionType,
      initDataType, aPromiseId, std::move(aInitData)));
}

void ChromiumCDMProxy::LoadSession(PromiseId aPromiseId,
                                   dom::MediaKeySessionType aSessionType,
                                   const nsAString& aSessionId) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromiseWithStateError(aPromiseId, "Null CDM in LoadSession"_ns);
    return;
  }

  mGMPThread->Dispatch(NewRunnableMethod<uint32_t, uint32_t, nsString>(
      "gmp::ChromiumCDMParent::LoadSession", cdm,
      &gmp::ChromiumCDMParent::LoadSession, aPromiseId,
      ToCDMSessionType(aSessionType), aSessionId));
}

void ChromiumCDMProxy::SetServerCertificate(PromiseId aPromiseId,
                                            nsTArray<uint8_t>& aCert) {
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::SetServerCertificate(this=%p, pid=%" PRIu32
          ") certLen=%zu",
          this, aPromiseId, aCert.Length());

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromiseWithStateError(aPromiseId,
                                "Null CDM in SetServerCertificate"_ns);
    return;
  }

  mGMPThread->Dispatch(NewRunnableMethod<uint32_t, nsTArray<uint8_t>>(
      "gmp::ChromiumCDMParent::SetServerCertificate", cdm,
      &gmp::ChromiumCDMParent::SetServerCertificate, aPromiseId,
      std::move(aCert)));
}

void ChromiumCDMProxy::UpdateSession(const nsAString& aSessionId,
                                     PromiseId aPromiseId,
                                     nsTArray<uint8_t>& aResponse) {
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::UpdateSession(this=%p, sid='%s', pid=%" PRIu32
          ") "
          "responseLen=%zu",
          this, NS_ConvertUTF16toUTF8(aSessionId).get(), aPromiseId,
          aResponse.Length());

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromiseWithStateError(aPromiseId, "Null CDM in UpdateSession"_ns);
    return;
  }
  mGMPThread->Dispatch(
      NewRunnableMethod<nsCString, uint32_t, nsTArray<uint8_t>>(
          "gmp::ChromiumCDMParent::UpdateSession", cdm,
          &gmp::ChromiumCDMParent::UpdateSession,
          NS_ConvertUTF16toUTF8(aSessionId), aPromiseId, std::move(aResponse)));
}

void ChromiumCDMProxy::CloseSession(const nsAString& aSessionId,
                                    PromiseId aPromiseId) {
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::CloseSession(this=%p, sid='%s', pid=%" PRIu32 ")",
          this, NS_ConvertUTF16toUTF8(aSessionId).get(), aPromiseId);

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromiseWithStateError(aPromiseId, "Null CDM in CloseSession"_ns);
    return;
  }
  mGMPThread->Dispatch(NewRunnableMethod<nsCString, uint32_t>(
      "gmp::ChromiumCDMParent::CloseSession", cdm,
      &gmp::ChromiumCDMParent::CloseSession, NS_ConvertUTF16toUTF8(aSessionId),
      aPromiseId));
}

void ChromiumCDMProxy::RemoveSession(const nsAString& aSessionId,
                                     PromiseId aPromiseId) {
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::RemoveSession(this=%p, sid='%s', pid=%" PRIu32 ")",
          this, NS_ConvertUTF16toUTF8(aSessionId).get(), aPromiseId);

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromiseWithStateError(aPromiseId, "Null CDM in RemoveSession"_ns);
    return;
  }
  mGMPThread->Dispatch(NewRunnableMethod<nsCString, uint32_t>(
      "gmp::ChromiumCDMParent::RemoveSession", cdm,
      &gmp::ChromiumCDMParent::RemoveSession, NS_ConvertUTF16toUTF8(aSessionId),
      aPromiseId));
}

void ChromiumCDMProxy::QueryOutputProtectionStatus() {
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::QueryOutputProtectionStatus(this=%p)", this);

  if (mKeys.IsNull()) {
    EME_LOG(
        "ChromiumCDMProxy::QueryOutputProtectionStatus(this=%p), mKeys "
        "missing!",
        this);
    // If we can't get mKeys, we're probably in shutdown. But do our best to
    // respond to the request and indicate the check failed.
    NotifyOutputProtectionStatus(OutputProtectionCheckStatus::CheckFailed,
                                 OutputProtectionCaptureStatus::Unused);
    return;
  }
  // The keys will call back via `NotifyOutputProtectionStatus` to notify the
  // result of the check.
  mKeys->CheckIsElementCapturePossible();
}

void ChromiumCDMProxy::NotifyOutputProtectionStatus(
    OutputProtectionCheckStatus aCheckStatus,
    OutputProtectionCaptureStatus aCaptureStatus) {
  MOZ_ASSERT(NS_IsMainThread());
  // If the check failed aCaptureStatus should be unused, otherwise not.
  MOZ_ASSERT_IF(aCheckStatus == OutputProtectionCheckStatus::CheckFailed,
                aCaptureStatus == OutputProtectionCaptureStatus::Unused);
  MOZ_ASSERT_IF(aCheckStatus == OutputProtectionCheckStatus::CheckSuccessful,
                aCaptureStatus != OutputProtectionCaptureStatus::Unused);
  EME_LOG(
      "ChromiumCDMProxy::NotifyOutputProtectionStatus(this=%p) "
      "aCheckStatus=%" PRIu8 " aCaptureStatus=%" PRIu8,
      this, static_cast<uint8_t>(aCheckStatus),
      static_cast<uint8_t>(aCaptureStatus));

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    // If we're in shutdown the CDM may have been cleared while a notification
    // is in flight. If this happens outside of shutdown we have a bug.
    MOZ_ASSERT(mIsShutdown);
    return;
  }

  uint32_t linkMask{};
  uint32_t protectionMask{};  // Unused/always zeroed.
  if (aCheckStatus == OutputProtectionCheckStatus::CheckSuccessful &&
      aCaptureStatus == OutputProtectionCaptureStatus::CapturePossilbe) {
    // The result indicates the capture is possible, so set the mask
    // to indicate this.
    linkMask |= cdm::OutputLinkTypes::kLinkTypeNetwork;
  }
  mGMPThread->Dispatch(NewRunnableMethod<bool, uint32_t, uint32_t>(
      "gmp::ChromiumCDMParent::NotifyOutputProtectionStatus", cdm,
      &gmp::ChromiumCDMParent::NotifyOutputProtectionStatus,
      aCheckStatus == OutputProtectionCheckStatus::CheckSuccessful, linkMask,
      protectionMask));
}

void ChromiumCDMProxy::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::Shutdown(this=%p) mCDM=%p, mIsShutdown=%s", this,
          mCDM.get(), mIsShutdown ? "true" : "false");
  if (mIsShutdown) {
    return;
  }
  mIsShutdown = true;
  mKeys.Clear();
  ShutdownCDMIfExists();
}

void ChromiumCDMProxy::RejectPromise(PromiseId aId, ErrorResult&& aException,
                                     const nsCString& aReason) {
  if (!NS_IsMainThread()) {
    // Use CopyableErrorResult to store our exception in the runnable,
    // because ErrorResult is not OK to move across threads.
    mMainThread->Dispatch(
        NewRunnableMethod<PromiseId, StoreCopyPassByRRef<CopyableErrorResult>,
                          nsCString>(
            "ChromiumCDMProxy::RejectPromise", this,
            &ChromiumCDMProxy::RejectPromiseOnMainThread, aId,
            std::move(aException), aReason),
        NS_DISPATCH_NORMAL);
    return;
  }
  EME_LOG("ChromiumCDMProxy::RejectPromise(this=%p, pid=%" PRIu32
          ", code=0x%x, "
          "reason='%s')",
          this, aId, aException.ErrorCodeAsInt(), aReason.get());
  if (!mKeys.IsNull()) {
    mKeys->RejectPromise(aId, std::move(aException), aReason);
  } else {
    // We don't have a MediaKeys object to pass the exception to, so silence
    // the exception to avoid it asserting due to being unused.
    aException.SuppressException();
  }
}

void ChromiumCDMProxy::RejectPromiseWithStateError(PromiseId aId,
                                                   const nsCString& aReason) {
  ErrorResult rv;
  rv.ThrowInvalidStateError(aReason);
  RejectPromise(aId, std::move(rv), aReason);
}

void ChromiumCDMProxy::RejectPromiseOnMainThread(
    PromiseId aId, CopyableErrorResult&& aException, const nsCString& aReason) {
  // Moving into or out of a non-copyable ErrorResult will assert that both
  // ErorResults are from our current thread.  Avoid the assertion by moving
  // into a current-thread CopyableErrorResult first.  Note that this is safe,
  // because CopyableErrorResult never holds state that can't move across
  // threads.
  CopyableErrorResult rv(std::move(aException));
  RejectPromise(aId, std::move(rv), aReason);
}

void ChromiumCDMProxy::ResolvePromise(PromiseId aId) {
  if (!NS_IsMainThread()) {
    mMainThread->Dispatch(
        NewRunnableMethod<PromiseId>("ChromiumCDMProxy::ResolvePromise", this,
                                     &ChromiumCDMProxy::ResolvePromise, aId),
        NS_DISPATCH_NORMAL);
    return;
  }

  EME_LOG("ChromiumCDMProxy::ResolvePromise(this=%p, pid=%" PRIu32 ")", this,
          aId);
  if (!mKeys.IsNull()) {
    mKeys->ResolvePromise(aId);
  } else {
    NS_WARNING("ChromiumCDMProxy unable to resolve promise!");
  }
}

const nsCString& ChromiumCDMProxy::GetNodeId() const { return mNodeId; }

void ChromiumCDMProxy::OnSetSessionId(uint32_t aCreateSessionToken,
                                      const nsAString& aSessionId) {
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::OnSetSessionId(this=%p, token=%" PRIu32
          ", sid='%s')",
          this, aCreateSessionToken, NS_ConvertUTF16toUTF8(aSessionId).get());

  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(
      mKeys->GetPendingSession(aCreateSessionToken));
  if (session) {
    session->SetSessionId(aSessionId);
  }
}

void ChromiumCDMProxy::OnResolveLoadSessionPromise(uint32_t aPromiseId,
                                                   bool aSuccess) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  mKeys->OnSessionLoaded(aPromiseId, aSuccess);
}

void ChromiumCDMProxy::OnResolvePromiseWithKeyStatus(
    uint32_t aPromiseId, dom::MediaKeyStatus aKeyStatus) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  mKeys->ResolvePromiseWithKeyStatus(aPromiseId, aKeyStatus);
}

void ChromiumCDMProxy::OnSessionMessage(const nsAString& aSessionId,
                                        dom::MediaKeyMessageType aMessageType,
                                        const nsTArray<uint8_t>& aMessage) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyMessage(aMessageType, aMessage);
  }
}

void ChromiumCDMProxy::OnKeyStatusesChange(const nsAString& aSessionId) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mKeys.IsNull()) {
    return;
  }
  RefPtr<dom::MediaKeySession> session(mKeys->GetSession(aSessionId));
  if (session) {
    session->DispatchKeyStatusesChange();
  }
}

void ChromiumCDMProxy::OnExpirationChange(const nsAString& aSessionId,
                                          UnixTime aExpiryTime) {
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

void ChromiumCDMProxy::OnSessionClosed(const nsAString& aSessionId) {
  MOZ_ASSERT(NS_IsMainThread());

  bool keyStatusesChange = false;
  {
    auto caps = Capabilites().Lock();
    keyStatusesChange = caps->RemoveKeysForSession(nsString(aSessionId));
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

void ChromiumCDMProxy::OnDecrypted(uint32_t aId, DecryptStatus aResult,
                                   const nsTArray<uint8_t>& aDecryptedData) {}

void ChromiumCDMProxy::OnSessionError(const nsAString& aSessionId,
                                      nsresult aException, uint32_t aSystemCode,
                                      const nsAString& aMsg) {
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

void ChromiumCDMProxy::OnRejectPromise(uint32_t aPromiseId,
                                       ErrorResult&& aException,
                                       const nsCString& aMsg) {
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromise(aPromiseId, std::move(aException), aMsg);
}

const nsString& ChromiumCDMProxy::KeySystem() const { return mKeySystem; }

DataMutex<CDMCaps>& ChromiumCDMProxy::Capabilites() { return mCapabilites; }

RefPtr<DecryptPromise> ChromiumCDMProxy::Decrypt(MediaRawData* aSample) {
  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    return DecryptPromise::CreateAndReject(
        DecryptResult(eme::AbortedErr, aSample), __func__);
  }
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mGMPThread, __func__,
                     [cdm, sample]() { return cdm->Decrypt(sample); });
}

void ChromiumCDMProxy::GetStatusForPolicy(PromiseId aPromiseId,
                                          const nsAString& aMinHdcpVersion) {
  MOZ_ASSERT(NS_IsMainThread());
  EME_LOG("ChromiumCDMProxy::GetStatusForPolicy(this=%p, pid=%" PRIu32
          ") minHdcpVersion=%s",
          this, aPromiseId, NS_ConvertUTF16toUTF8(aMinHdcpVersion).get());

  RefPtr<gmp::ChromiumCDMParent> cdm = GetCDMParent();
  if (!cdm) {
    RejectPromiseWithStateError(aPromiseId,
                                "Null CDM in GetStatusForPolicy"_ns);
    return;
  }

  mGMPThread->Dispatch(NewRunnableMethod<uint32_t, nsCString>(
      "gmp::ChromiumCDMParent::GetStatusForPolicy", cdm,
      &gmp::ChromiumCDMParent::GetStatusForPolicy, aPromiseId,
      NS_ConvertUTF16toUTF8(aMinHdcpVersion)));
}

void ChromiumCDMProxy::Terminated() {
  if (!mKeys.IsNull()) {
    mKeys->Terminated();
  }
}

already_AddRefed<gmp::ChromiumCDMParent> ChromiumCDMProxy::GetCDMParent() {
  MutexAutoLock lock(mCDMMutex);
  RefPtr<gmp::ChromiumCDMParent> cdm = mCDM;
  return cdm.forget();
}

}  // namespace mozilla

#undef NS_DispatchToMainThread
