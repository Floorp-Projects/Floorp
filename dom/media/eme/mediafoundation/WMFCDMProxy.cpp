/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFCDMProxy.h"

#include "mozilla/dom/MediaKeySession.h"
#include "WMFCDMImpl.h"

namespace mozilla {

WMFCDMProxy::WMFCDMProxy(dom::MediaKeys* aKeys, const nsAString& aKeySystem,
                         const dom::MediaKeySystemConfiguration& aConfig)
    : CDMProxy(
          aKeys, aKeySystem,
          aConfig.mDistinctiveIdentifier == dom::MediaKeysRequirement::Required,
          aConfig.mPersistentState == dom::MediaKeysRequirement::Required),
      mConfig(aConfig) {
  MOZ_ASSERT(NS_IsMainThread());
}

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
  WMFCDMImpl::InitParams params{
      nsString(aOrigin), mConfig.mInitDataTypes, mPersistentStateRequired,
      mDistinctiveIdentifierRequired, false /* HW secure? */};
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

void WMFCDMProxy::ResolvePromise(PromiseId aId) {
  auto resolve = [self = RefPtr{this}, this, aId]() {
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

  static auto ConvertSessionType = [](dom::MediaKeySessionType aType) {
    switch (aType) {
      case dom::MediaKeySessionType::Temporary:
        return KeySystemConfig::SessionType::Temporary;
      case dom::MediaKeySessionType::Persistent_license:
        return KeySystemConfig::SessionType::PersistentLicense;
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid session type");
        return KeySystemConfig::SessionType::Temporary;
    }
  };
  static auto SessionTypeToStr = [](dom::MediaKeySessionType aType) {
    switch (aType) {
      case dom::MediaKeySessionType::Temporary:
        return "Temporary";
      case dom::MediaKeySessionType::Persistent_license:
        return "PersistentLicense";
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid session type");
        return "Invalid";
    }
  };
  EME_LOG("WMFCDMProxy::CreateSession(this=%p, pid=%" PRIu32
          "), sessionType=%s",
          this, aPromiseId, SessionTypeToStr(aSessionType));
  mCDM->CreateSession(ConvertSessionType(aSessionType), aInitDataType,
                      aInitData)
      ->Then(
          mMainThread, __func__,
          [self = RefPtr{this}, this, aCreateSessionToken,
           aPromiseId](nsString sessionID) {
            mCreateSessionRequest.Complete();
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
            mCreateSessionRequest.Complete();
            RejectPromiseWithStateError(
                aPromiseId,
                nsLiteralCString(
                    "WMFCDMProxy::CreateSession: cannot create session"));
          })
      ->Track(mCreateSessionRequest);
}

void WMFCDMProxy::Shutdown() {
  // TODO: reject pending promise.
  mCreateSessionRequest.DisconnectIfExists();
}

}  // namespace mozilla
