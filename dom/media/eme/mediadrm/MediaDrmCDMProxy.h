/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaDrmCDMProxy_h_
#define MediaDrmCDMProxy_h_

#include <jni.h>
#include "mozilla/jni/Types.h"
#include "GeneratedJNINatives.h"

#include "mozilla/CDMProxy.h"
#include "mozilla/CDMCaps.h"
#include "mozilla/dom/MediaKeys.h"
#include "mozilla/MediaDrmProxySupport.h"
#include "mozilla/UniquePtr.h"

#include "MediaCodec.h"
#include "nsString.h"
#include "nsAutoPtr.h"

using namespace mozilla::java;

namespace mozilla {
class MediaDrmCDMCallbackProxy;
class MediaDrmCDMProxy : public CDMProxy {
public:

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDrmCDMProxy)

  MediaDrmCDMProxy(dom::MediaKeys* aKeys,
                   const nsAString& aKeySystem,
                   bool aDistinctiveIdentifierRequired,
                   bool aPersistentStateRequired);

  void Init(PromiseId aPromiseId,
            const nsAString& aOrigin,
            const nsAString& aTopLevelOrigin,
            const nsAString& aGMPName,
            bool aInPrivateBrowsing) override;

  void CreateSession(uint32_t aCreateSessionToken,
                     MediaKeySessionType aSessionType,
                     PromiseId aPromiseId,
                     const nsAString& aInitDataType,
                     nsTArray<uint8_t>& aInitData) override;

  void LoadSession(PromiseId aPromiseId,
                   const nsAString& aSessionId) override;

  void SetServerCertificate(PromiseId aPromiseId,
                            nsTArray<uint8_t>& aCert) override;

  void UpdateSession(const nsAString& aSessionId,
                     PromiseId aPromiseId,
                     nsTArray<uint8_t>& aResponse) override;

  void CloseSession(const nsAString& aSessionId,
                    PromiseId aPromiseId) override;

  void RemoveSession(const nsAString& aSessionId,
                     PromiseId aPromiseId) override;

  void Shutdown() override;

  void Terminated() override;

  const nsCString& GetNodeId() const override;

  void OnSetSessionId(uint32_t aCreateSessionToken,
                      const nsAString& aSessionId) override;

  void OnResolveLoadSessionPromise(uint32_t aPromiseId, bool aSuccess) override;

  void OnSessionMessage(const nsAString& aSessionId,
                        dom::MediaKeyMessageType aMessageType,
                        nsTArray<uint8_t>& aMessage) override;

  void OnExpirationChange(const nsAString& aSessionId,
                          UnixTime aExpiryTime) override;

  void OnSessionClosed(const nsAString& aSessionId) override;

  void OnSessionError(const nsAString& aSessionId,
                      nsresult aException,
                      uint32_t aSystemCode,
                      const nsAString& aMsg) override;

  void OnRejectPromise(uint32_t aPromiseId,
                       nsresult aCode,
                       const nsCString& aMsg) override;

  RefPtr<DecryptPromise> Decrypt(MediaRawData* aSample) override;
  void OnDecrypted(uint32_t aId,
                   DecryptStatus aResult,
                   const nsTArray<uint8_t>& aDecryptedData) override;

  void RejectPromise(PromiseId aId, nsresult aCode,
                     const nsCString& aReason) override;

  // Resolves promise with "undefined".
  // Can be called from any thread.
  void ResolvePromise(PromiseId aId) override;

  // Threadsafe.
  const nsString& KeySystem() const override;

  CDMCaps& Capabilites() override;

  void OnKeyStatusesChange(const nsAString& aSessionId) override;

  void GetSessionIdsForKeyId(const nsTArray<uint8_t>& aKeyId,
                             nsTArray<nsCString>& aSessionIds) override;

#ifdef DEBUG
  bool IsOnOwnerThread() override;
#endif

private:
  virtual ~MediaDrmCDMProxy();

  void OnCDMCreated(uint32_t aPromiseId);

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
    RejectPromiseTask(MediaDrmCDMProxy* aProxy,
                      PromiseId aId,
                      nsresult aCode,
                      const nsCString& aReason)
      : mProxy(aProxy)
      , mId(aId)
      , mCode(aCode)
      , mReason(aReason)
    {
    }
    NS_METHOD Run() {
      mProxy->RejectPromise(mId, mCode, mReason);
      return NS_OK;
    }
  private:
    RefPtr<MediaDrmCDMProxy> mProxy;
    PromiseId mId;
    nsresult mCode;
    nsCString mReason;
  };

  nsCString mNodeId;
  mozilla::UniquePtr<MediaDrmProxySupport> mCDM;
  nsAutoPtr<MediaDrmCDMCallbackProxy> mCallback;
  bool mShutdownCalled;

// =====================================================================
// For MediaDrmProxySupport
  void md_Init(uint32_t aPromiseId);
  void md_CreateSession(nsAutoPtr<CreateSessionData> aData);
  void md_UpdateSession(nsAutoPtr<UpdateSessionData> aData);
  void md_CloseSession(nsAutoPtr<SessionOpData> aData);
  void md_Shutdown();
// =====================================================================
};

} // namespace mozilla
#endif // MediaDrmCDMProxy_h_