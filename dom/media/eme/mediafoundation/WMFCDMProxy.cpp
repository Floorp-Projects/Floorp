/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFCDMProxy.h"

#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/dom/MediaKeySystemAccessBinding.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/WMFCDMProxyCallback.h"
#include "mozilla/WindowsVersion.h"
#include "WMFCDMImpl.h"
#include "WMFCDMProxyCallback.h"

namespace mozilla {

#define LOG(msg, ...) \
  EME_LOG("WMFCDMProxy[%p]@%s: " msg, this, __func__, ##__VA_ARGS__)

#define RETURN_IF_SHUTDOWN()       \
  do {                             \
    MOZ_ASSERT(NS_IsMainThread()); \
    if (mIsShutdown) {             \
      return;                      \
    }                              \
  } while (false)

#define PERFORM_ON_CDM(operation, promiseId, ...)                             \
  do {                                                                        \
    mCDM->operation(promiseId, __VA_ARGS__)                                   \
        ->Then(                                                               \
            mMainThread, __func__,                                            \
            [self = RefPtr{this}, this, promiseId]() {                        \
              RETURN_IF_SHUTDOWN();                                           \
              if (mKeys.IsNull()) {                                           \
                EME_LOG("WMFCDMProxy(this=%p, pid=%" PRIu32                   \
                        ") : abort the " #operation " due to empty key",      \
                        this, promiseId);                                     \
                return;                                                       \
              }                                                               \
              ResolvePromise(promiseId);                                      \
            },                                                                \
            [self = RefPtr{this}, this, promiseId]() {                        \
              RETURN_IF_SHUTDOWN();                                           \
              RejectPromiseWithStateError(                                    \
                  promiseId, nsLiteralCString("WMFCDMProxy::" #operation ": " \
                                              "failed to " #operation));      \
            });                                                               \
  } while (false)

WMFCDMProxy::WMFCDMProxy(dom::MediaKeys* aKeys, const nsAString& aKeySystem,
                         const dom::MediaKeySystemConfiguration& aConfig)
    : CDMProxy(
          aKeys, aKeySystem,
          aConfig.mDistinctiveIdentifier == dom::MediaKeysRequirement::Required,
          aConfig.mPersistentState == dom::MediaKeysRequirement::Required),
      mConfig(aConfig) {
  MOZ_ASSERT(NS_IsMainThread());
}

WMFCDMProxy::~WMFCDMProxy() {}

void WMFCDMProxy::Init(PromiseId aPromiseId, const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       const nsAString& aName) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aOrigin.IsEmpty());

  MOZ_ASSERT(!mOwnerThread);
  if (NS_FAILED(
          NS_NewNamedThread("WMFCDMThread", getter_AddRefs(mOwnerThread)))) {
    RejectPromiseWithStateError(
        aPromiseId,
        nsLiteralCString("WMFCDMProxy::Init: couldn't create CDM thread"));
    return;
  }

  mCDM = MakeRefPtr<WMFCDMImpl>(mKeySystem);
  mProxyCallback = new WMFCDMProxyCallback(this);
  // SWDRM has a PMP process leakage problem on Windows 10 due to the internal
  // issue in the media foundation. Microsoft only lands the solution on Windows
  // 11 and doesn't have plan to port it back to Windows 10. Therefore, for
  // PlayReady, we need to force to covert SL2000 to SL3000 for our underlying
  // CDM to avoid that issue. In addition, as we only support L1 for Widevine,
  // this issue won't happen on it.
  Maybe<nsString> forcedRobustness =
      IsPlayReadyKeySystemAndSupported(mKeySystem) && !IsWin11OrLater()
          ? Some(nsString(u"3000"))
          : Nothing();
  WMFCDMImpl::InitParams params{
      nsString(aOrigin),
      mConfig.mInitDataTypes,
      mPersistentStateRequired,
      mDistinctiveIdentifierRequired,
      mProxyCallback,
      GenerateMFCDMMediaCapabilities(mConfig.mAudioCapabilities),
      GenerateMFCDMMediaCapabilities(mConfig.mVideoCapabilities,
                                     forcedRobustness)};
  mCDM->Init(params)->Then(
      mMainThread, __func__,
      [self = RefPtr{this}, this, aPromiseId](const bool) {
        MOZ_ASSERT(mCDM->Id() > 0);
        mKeys->OnCDMCreated(aPromiseId, mCDM->Id());
      },
      [self = RefPtr{this}, this, aPromiseId](const nsresult rv) {
        RejectPromiseWithStateError(
            aPromiseId,
            nsLiteralCString("WMFCDMProxy::Init: WMFCDM init error"));
      });
}

CopyableTArray<MFCDMMediaCapability>
WMFCDMProxy::GenerateMFCDMMediaCapabilities(
    const dom::Sequence<dom::MediaKeySystemMediaCapability>& aCapabilities,
    const Maybe<nsString>& forcedRobustness) {
  CopyableTArray<MFCDMMediaCapability> outCapabilites;
  for (const auto& capabilities : aCapabilities) {
    if (!forcedRobustness) {
      EME_LOG("WMFCDMProxy::Init %p, robustness=%s", this,
              NS_ConvertUTF16toUTF8(capabilities.mRobustness).get());
      outCapabilites.AppendElement(MFCDMMediaCapability{
          capabilities.mContentType, capabilities.mRobustness});
    } else {
      EME_LOG("WMFCDMProxy::Init %p, force to robustness=%s", this,
              NS_ConvertUTF16toUTF8(*forcedRobustness).get());
      outCapabilites.AppendElement(
          MFCDMMediaCapability{capabilities.mContentType, *forcedRobustness});
    }
  }
  return outCapabilites;
}

void WMFCDMProxy::ResolvePromise(PromiseId aId) {
  auto resolve = [self = RefPtr{this}, this, aId]() {
    RETURN_IF_SHUTDOWN();
    EME_LOG("WMFCDMProxy::ResolvePromise(this=%p, pid=%" PRIu32 ")", this, aId);
    if (!mKeys.IsNull()) {
      mKeys->ResolvePromise(aId);
    } else {
      NS_WARNING("WMFCDMProxy unable to resolve promise!");
    }
  };

  if (NS_IsMainThread()) {
    resolve();
    return;
  }
  mMainThread->Dispatch(
      NS_NewRunnableFunction("WMFCDMProxy::ResolvePromise", resolve));
}

void WMFCDMProxy::RejectPromise(PromiseId aId, ErrorResult&& aException,
                                const nsCString& aReason) {
  if (!NS_IsMainThread()) {
    // Use CopyableErrorResult to store our exception in the runnable,
    // because ErrorResult is not OK to move across threads.
    mMainThread->Dispatch(
        NewRunnableMethod<PromiseId, StoreCopyPassByRRef<CopyableErrorResult>,
                          nsCString>("WMFCDMProxy::RejectPromise", this,
                                     &WMFCDMProxy::RejectPromiseOnMainThread,
                                     aId, std::move(aException), aReason),
        NS_DISPATCH_NORMAL);
    return;
  }
  EME_LOG("WMFCDMProxy::RejectPromise(this=%p, pid=%" PRIu32
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

void WMFCDMProxy::RejectPromiseOnMainThread(PromiseId aId,
                                            CopyableErrorResult&& aException,
                                            const nsCString& aReason) {
  RETURN_IF_SHUTDOWN();
  // Moving into or out of a non-copyable ErrorResult will assert that both
  // ErorResults are from our current thread.  Avoid the assertion by moving
  // into a current-thread CopyableErrorResult first.  Note that this is safe,
  // because CopyableErrorResult never holds state that can't move across
  // threads.
  CopyableErrorResult rv(std::move(aException));
  RejectPromise(aId, std::move(rv), aReason);
}

void WMFCDMProxy::RejectPromiseWithStateError(PromiseId aId,
                                              const nsCString& aReason) {
  ErrorResult rv;
  rv.ThrowInvalidStateError(aReason);
  RejectPromise(aId, std::move(rv), aReason);
}

void WMFCDMProxy::CreateSession(uint32_t aCreateSessionToken,
                                MediaKeySessionType aSessionType,
                                PromiseId aPromiseId,
                                const nsAString& aInitDataType,
                                nsTArray<uint8_t>& aInitData) {
  MOZ_ASSERT(NS_IsMainThread());
  RETURN_IF_SHUTDOWN();
  const auto sessionType = ConvertToKeySystemConfigSessionType(aSessionType);
  EME_LOG("WMFCDMProxy::CreateSession(this=%p, pid=%" PRIu32
          "), sessionType=%s",
          this, aPromiseId, SessionTypeToStr(sessionType));
  mCDM->CreateSession(aPromiseId, sessionType, aInitDataType, aInitData)
      ->Then(
          mMainThread, __func__,
          [self = RefPtr{this}, this, aCreateSessionToken,
           aPromiseId](nsString sessionID) {
            RETURN_IF_SHUTDOWN();
            if (mKeys.IsNull()) {
              EME_LOG("WMFCDMProxy(this=%p, pid=%" PRIu32
                      ") : abort the create session due to "
                      "empty key",
                      this, aPromiseId);
              return;
            }
            if (RefPtr<dom::MediaKeySession> session =
                    mKeys->GetPendingSession(aCreateSessionToken)) {
              session->SetSessionId(std::move(sessionID));
            }
            ResolvePromise(aPromiseId);
          },
          [self = RefPtr{this}, this, aPromiseId]() {
            RETURN_IF_SHUTDOWN();
            RejectPromiseWithStateError(
                aPromiseId,
                nsLiteralCString(
                    "WMFCDMProxy::CreateSession: cannot create session"));
          });
}

void WMFCDMProxy::LoadSession(PromiseId aPromiseId,
                              dom::MediaKeySessionType aSessionType,
                              const nsAString& aSessionId) {
  MOZ_ASSERT(NS_IsMainThread());
  RETURN_IF_SHUTDOWN();
  const auto sessionType = ConvertToKeySystemConfigSessionType(aSessionType);
  EME_LOG("WMFCDMProxy::LoadSession(this=%p, pid=%" PRIu32
          "), sessionType=%s, sessionId=%s",
          this, aPromiseId, SessionTypeToStr(sessionType),
          NS_ConvertUTF16toUTF8(aSessionId).get());
  PERFORM_ON_CDM(LoadSession, aPromiseId, sessionType, aSessionId);
}

void WMFCDMProxy::UpdateSession(const nsAString& aSessionId,
                                PromiseId aPromiseId,
                                nsTArray<uint8_t>& aResponse) {
  MOZ_ASSERT(NS_IsMainThread());
  RETURN_IF_SHUTDOWN();
  EME_LOG("WMFCDMProxy::UpdateSession(this=%p, pid=%" PRIu32
          "), sessionId=%s, responseLen=%zu",
          this, aPromiseId, NS_ConvertUTF16toUTF8(aSessionId).get(),
          aResponse.Length());
  PERFORM_ON_CDM(UpdateSession, aPromiseId, aSessionId, aResponse);
}

void WMFCDMProxy::CloseSession(const nsAString& aSessionId,
                               PromiseId aPromiseId) {
  MOZ_ASSERT(NS_IsMainThread());
  RETURN_IF_SHUTDOWN();
  EME_LOG("WMFCDMProxy::CloseSession(this=%p, pid=%" PRIu32 "), sessionId=%s",
          this, aPromiseId, NS_ConvertUTF16toUTF8(aSessionId).get());
  PERFORM_ON_CDM(CloseSession, aPromiseId, aSessionId);
}

void WMFCDMProxy::RemoveSession(const nsAString& aSessionId,
                                PromiseId aPromiseId) {
  MOZ_ASSERT(NS_IsMainThread());
  RETURN_IF_SHUTDOWN();
  EME_LOG("WMFCDMProxy::RemoveSession(this=%p, pid=%" PRIu32 "), sessionId=%s",
          this, aPromiseId, NS_ConvertUTF16toUTF8(aSessionId).get());
  PERFORM_ON_CDM(RemoveSession, aPromiseId, aSessionId);
}

void WMFCDMProxy::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIsShutdown);
  if (mProxyCallback) {
    mProxyCallback->Shutdown();
    mProxyCallback = nullptr;
  }
  mIsShutdown = true;
}

void WMFCDMProxy::OnSessionMessage(const nsAString& aSessionId,
                                   dom::MediaKeyMessageType aMessageType,
                                   const nsTArray<uint8_t>& aMessage) {
  MOZ_ASSERT(NS_IsMainThread());
  RETURN_IF_SHUTDOWN();
  if (mKeys.IsNull()) {
    return;
  }
  if (RefPtr<dom::MediaKeySession> session = mKeys->GetSession(aSessionId)) {
    LOG("Notify key message for session Id=%s",
        NS_ConvertUTF16toUTF8(aSessionId).get());
    session->DispatchKeyMessage(aMessageType, aMessage);
  }
}

void WMFCDMProxy::OnKeyStatusesChange(const nsAString& aSessionId) {
  MOZ_ASSERT(NS_IsMainThread());
  RETURN_IF_SHUTDOWN();
  if (mKeys.IsNull()) {
    return;
  }
  if (RefPtr<dom::MediaKeySession> session = mKeys->GetSession(aSessionId)) {
    LOG("Notify key statuses for session Id=%s",
        NS_ConvertUTF16toUTF8(aSessionId).get());
    session->DispatchKeyStatusesChange();
  }
}

void WMFCDMProxy::OnExpirationChange(const nsAString& aSessionId,
                                     UnixTime aExpiryTime) {
  MOZ_ASSERT(NS_IsMainThread());
  RETURN_IF_SHUTDOWN();
  if (mKeys.IsNull()) {
    return;
  }
  if (RefPtr<dom::MediaKeySession> session = mKeys->GetSession(aSessionId)) {
    LOG("Notify expiration for session Id=%s",
        NS_ConvertUTF16toUTF8(aSessionId).get());
    session->SetExpiration(static_cast<double>(aExpiryTime));
  }
}

void WMFCDMProxy::SetServerCertificate(PromiseId aPromiseId,
                                       nsTArray<uint8_t>& aCert) {
  MOZ_ASSERT(NS_IsMainThread());
  RETURN_IF_SHUTDOWN();
  EME_LOG("WMFCDMProxy::SetServerCertificate(this=%p, pid=%" PRIu32 ")", this,
          aPromiseId);
  mCDM->SetServerCertificate(aPromiseId, aCert)
      ->Then(
          mMainThread, __func__,
          [self = RefPtr{this}, this, aPromiseId]() {
            RETURN_IF_SHUTDOWN();
            ResolvePromise(aPromiseId);
          },
          [self = RefPtr{this}, this, aPromiseId]() {
            RETURN_IF_SHUTDOWN();
            RejectPromiseWithStateError(
                aPromiseId,
                nsLiteralCString("WMFCDMProxy::SetServerCertificate failed!"));
          });
}

bool WMFCDMProxy::IsHardwareDecryptionSupported() const {
  return mozilla::IsHardwareDecryptionSupported(mConfig);
}

uint64_t WMFCDMProxy::GetCDMProxyId() const {
  MOZ_DIAGNOSTIC_ASSERT(mCDM);
  return mCDM->Id();
}

#undef LOG
#undef RETURN_IF_SHUTDOWN
#undef PERFORM_ON_CDM

}  // namespace mozilla
