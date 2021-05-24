/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaDrmCDMProxy_h_
#define MediaDrmCDMProxy_h_

#include <jni.h>
#include "mozilla/jni/Types.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/CDMCaps.h"
#include "mozilla/dom/MediaKeys.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/MediaDrmProxySupport.h"
#include "mozilla/UniquePtr.h"

#include "MediaCodec.h"
#include "nsString.h"

namespace mozilla {

class MediaDrmCDMCallbackProxy;
class MediaDrmCDMProxy : public CDMProxy {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDrmCDMProxy, override)

  MediaDrmCDMProxy(dom::MediaKeys* aKeys, const nsAString& aKeySystem,
                   bool aDistinctiveIdentifierRequired,
                   bool aPersistentStateRequired);

  void Init(PromiseId aPromiseId, const nsAString& aOrigin,
            const nsAString& aTopLevelOrigin,
            const nsAString& aGMPName) override;

  void CreateSession(uint32_t aCreateSessionToken,
                     MediaKeySessionType aSessionType, PromiseId aPromiseId,
                     const nsAString& aInitDataType,
                     nsTArray<uint8_t>& aInitData) override;

  void LoadSession(PromiseId aPromiseId, dom::MediaKeySessionType aSessionType,
                   const nsAString& aSessionId) override;

  void SetServerCertificate(PromiseId aPromiseId,
                            nsTArray<uint8_t>& aCert) override;

  void UpdateSession(const nsAString& aSessionId, PromiseId aPromiseId,
                     nsTArray<uint8_t>& aResponse) override;

  void CloseSession(const nsAString& aSessionId, PromiseId aPromiseId) override;

  void RemoveSession(const nsAString& aSessionId,
                     PromiseId aPromiseId) override;

  void QueryOutputProtectionStatus() override;

  void NotifyOutputProtectionStatus(
      OutputProtectionCheckStatus aCheckStatus,
      OutputProtectionCaptureStatus aCaptureStatus) override;

  void Shutdown() override;

  void Terminated() override;

  const nsCString& GetNodeId() const override;

  void OnSetSessionId(uint32_t aCreateSessionToken,
                      const nsAString& aSessionId) override;

  void OnResolveLoadSessionPromise(uint32_t aPromiseId, bool aSuccess) override;

  void OnSessionMessage(const nsAString& aSessionId,
                        dom::MediaKeyMessageType aMessageType,
                        const nsTArray<uint8_t>& aMessage) override;

  void OnExpirationChange(const nsAString& aSessionId,
                          UnixTime aExpiryTime) override;

  void OnSessionClosed(const nsAString& aSessionId) override;

  void OnSessionError(const nsAString& aSessionId, nsresult aException,
                      uint32_t aSystemCode, const nsAString& aMsg) override;

  void OnRejectPromise(uint32_t aPromiseId, ErrorResult&& aException,
                       const nsCString& aMsg) override;

  RefPtr<DecryptPromise> Decrypt(MediaRawData* aSample) override;
  void OnDecrypted(uint32_t aId, DecryptStatus aResult,
                   const nsTArray<uint8_t>& aDecryptedData) override;

  void RejectPromise(PromiseId aId, ErrorResult&& aException,
                     const nsCString& aReason) override;
  // Reject promise with an InvalidStateError and the given message.
  void RejectPromiseWithStateError(PromiseId aId, const nsCString& aReason);

  // Resolves promise with "undefined".
  // Can be called from any thread.
  void ResolvePromise(PromiseId aId) override;

  // Threadsafe.
  const nsString& KeySystem() const override;

  DataMutex<CDMCaps>& Capabilites() override;

  void OnKeyStatusesChange(const nsAString& aSessionId) override;

  void GetStatusForPolicy(PromiseId aPromiseId,
                          const nsAString& aMinHdcpVersion) override;

#ifdef DEBUG
  bool IsOnOwnerThread() override;
#endif

  const nsString& GetMediaDrmStubId() const;

 private:
  virtual ~MediaDrmCDMProxy();

  void OnCDMCreated(uint32_t aPromiseId);

  template <typename T>
  void ResolvePromiseWithResult(PromiseId aId, const T& aResult);

  struct CreateSessionData {
    MediaKeySessionType mSessionType;
    uint32_t mCreateSessionToken;
    PromiseId mPromiseId;
    nsCString mInitDataType;
    nsTArray<uint8_t> mInitData;
  };

  struct UpdateSessionData {
    PromiseId mPromiseId;
    nsCString mSessionId;
    nsTArray<uint8_t> mResponse;
  };

  struct SessionOpData {
    PromiseId mPromiseId;
    nsCString mSessionId;
  };

  class RejectPromiseTask : public Runnable {
   public:
    RejectPromiseTask(MediaDrmCDMProxy* aProxy, PromiseId aId,
                      ErrorResult&& aException, const nsCString& aReason)
        : Runnable("RejectPromiseTask"),
          mProxy(aProxy),
          mId(aId),
          mException(std::move(aException)),
          mReason(aReason) {}
    NS_IMETHOD Run() override {
      // Moving into or out of a non-copyable ErrorResult will assert that both
      // ErorResults are from our current thread.  Avoid the assertion by moving
      // into a current-thread CopyableErrorResult first.  Note that this is
      // safe, because CopyableErrorResult never holds state that can't move
      // across threads.
      CopyableErrorResult rv(std::move(mException));
      mProxy->RejectPromise(mId, std::move(rv), mReason);
      return NS_OK;
    }

   private:
    RefPtr<MediaDrmCDMProxy> mProxy;
    PromiseId mId;
    // We use a CopyableErrorResult here, because we're going to dispatch to a
    // different thread and normal ErrorResult doesn't support that.
    // CopyableErrorResult ensures that it only stores values that are safe to
    // move across threads.
    CopyableErrorResult mException;
    nsCString mReason;
  };

  nsCString mNodeId;
  UniquePtr<MediaDrmProxySupport> mCDM;
  UniquePtr<MediaDrmCDMCallbackProxy> mCallback;
  bool mShutdownCalled;

  // =====================================================================
  // For MediaDrmProxySupport
  void md_Init(uint32_t aPromiseId);
  void md_CreateSession(UniquePtr<CreateSessionData>&& aData);
  void md_SetServerCertificate(PromiseId aPromiseId,
                               const nsTArray<uint8_t>& aCert);
  void md_UpdateSession(UniquePtr<UpdateSessionData>&& aData);
  void md_CloseSession(UniquePtr<SessionOpData>&& aData);
  void md_Shutdown();
  // =====================================================================
};

}  // namespace mozilla

#endif  // MediaDrmCDMProxy_h_
