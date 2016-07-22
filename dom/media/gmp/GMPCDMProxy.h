/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPCDMProxy_h_
#define GMPCDMProxy_h_

#include "mozilla/CDMProxy.h"
#include "GMPCDMCallbackProxy.h"
#include "GMPDecryptorProxy.h"

namespace mozilla {
class MediaRawData;

// Implementation of CDMProxy which is based on GMP architecture.
class GMPCDMProxy : public CDMProxy {
public:

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPCDMProxy, override)

  typedef MozPromise<DecryptResult, DecryptResult, /* IsExclusive = */ true> DecryptPromise;

  GMPCDMProxy(dom::MediaKeys* aKeys,
              const nsAString& aKeySystem,
              GMPCrashHelper* aCrashHelper);

  void Init(PromiseId aPromiseId,
            const nsAString& aOrigin,
            const nsAString& aTopLevelOrigin,
            const nsAString& aGMPName,
            bool aInPrivateBrowsing) override;

  void CreateSession(uint32_t aCreateSessionToken,
                     dom::SessionType aSessionType,
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
                          GMPTimestamp aExpiryTime) override;

  void OnSessionClosed(const nsAString& aSessionId) override;

  void OnSessionError(const nsAString& aSessionId,
                      nsresult aException,
                      uint32_t aSystemCode,
                      const nsAString& aMsg) override;

  void OnRejectPromise(uint32_t aPromiseId,
                       nsresult aDOMException,
                       const nsCString& aMsg) override;

  RefPtr<DecryptPromise> Decrypt(MediaRawData* aSample) override;

  void OnDecrypted(uint32_t aId,
                   DecryptStatus aResult,
                   const nsTArray<uint8_t>& aDecryptedData) override;

  void RejectPromise(PromiseId aId, nsresult aExceptionCode,
                     const nsCString& aReason) override;

  void ResolvePromise(PromiseId aId) override;

  const nsString& KeySystem() const override;

  CDMCaps& Capabilites() override;

  void OnKeyStatusesChange(const nsAString& aSessionId) override;

  void GetSessionIdsForKeyId(const nsTArray<uint8_t>& aKeyId,
                             nsTArray<nsCString>& aSessionIds) override;

#ifdef DEBUG
  bool IsOnOwnerThread() override;
#endif

private:
  friend class gmp_InitDoneCallback;
  friend class gmp_InitGetGMPDecryptorCallback;

  struct InitData {
    uint32_t mPromiseId;
    nsString mOrigin;
    nsString mTopLevelOrigin;
    nsString mGMPName;
    RefPtr<GMPCrashHelper> mCrashHelper;
    bool mInPrivateBrowsing;
  };

  // GMP thread only.
  void gmp_Init(nsAutoPtr<InitData>&& aData);
  void gmp_InitDone(GMPDecryptorProxy* aCDM, nsAutoPtr<InitData>&& aData);
  void gmp_InitGetGMPDecryptor(nsresult aResult,
                               const nsACString& aNodeId,
                               nsAutoPtr<InitData>&& aData);

  // GMP thread only.
  void gmp_Shutdown();

  // Main thread only.
  void OnCDMCreated(uint32_t aPromiseId);

  struct CreateSessionData {
    dom::SessionType mSessionType;
    uint32_t mCreateSessionToken;
    PromiseId mPromiseId;
    nsCString mInitDataType;
    nsTArray<uint8_t> mInitData;
  };
  // GMP thread only.
  void gmp_CreateSession(nsAutoPtr<CreateSessionData> aData);

  struct SessionOpData {
    PromiseId mPromiseId;
    nsCString mSessionId;
  };
  // GMP thread only.
  void gmp_LoadSession(nsAutoPtr<SessionOpData> aData);

  struct SetServerCertificateData {
    PromiseId mPromiseId;
    nsTArray<uint8_t> mCert;
  };
  // GMP thread only.
  void gmp_SetServerCertificate(nsAutoPtr<SetServerCertificateData> aData);

  struct UpdateSessionData {
    PromiseId mPromiseId;
    nsCString mSessionId;
    nsTArray<uint8_t> mResponse;
  };
  // GMP thread only.
  void gmp_UpdateSession(nsAutoPtr<UpdateSessionData> aData);

  // GMP thread only.
  void gmp_CloseSession(nsAutoPtr<SessionOpData> aData);

  // GMP thread only.
  void gmp_RemoveSession(nsAutoPtr<SessionOpData> aData);

  class DecryptJob {
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecryptJob)

    explicit DecryptJob(MediaRawData* aSample)
      : mId(0)
      , mSample(aSample)
    {
    }

    void PostResult(DecryptStatus aResult,
                    const nsTArray<uint8_t>& aDecryptedData);
    void PostResult(DecryptStatus aResult);

    RefPtr<DecryptPromise> Ensure() {
      return mPromise.Ensure(__func__);
    }

    uint32_t mId;
    RefPtr<MediaRawData> mSample;
  private:
    ~DecryptJob() {}
    MozPromiseHolder<DecryptPromise> mPromise;
  };
  // GMP thread only.
  void gmp_Decrypt(RefPtr<DecryptJob> aJob);

  // GMP thread only.
  void gmp_Decrypted(uint32_t aId,
                     DecryptStatus aResult,
                     const nsTArray<uint8_t>& aDecryptedData);

  class RejectPromiseTask : public Runnable {
  public:
    RejectPromiseTask(GMPCDMProxy* aProxy,
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
    RefPtr<GMPCDMProxy> mProxy;
    PromiseId mId;
    nsresult mCode;
    nsCString mReason;
  };

  ~GMPCDMProxy();

  GMPCrashHelper* mCrashHelper;

  GMPDecryptorProxy* mCDM;

  CDMCaps mCapabilites;

  nsAutoPtr<GMPCDMCallbackProxy> mCallback;

  // Decryption jobs sent to CDM, awaiting result.
  // GMP thread only.
  nsTArray<RefPtr<DecryptJob>> mDecryptionJobs;

  // Number of buffers we've decrypted. Used to uniquely identify
  // decryption jobs sent to CDM. Note we can't just use the length of
  // mDecryptionJobs as that shrinks as jobs are completed and removed
  // from it.
  // GMP thread only.
  uint32_t mDecryptionJobCount;

  // True if GMPCDMProxy::gmp_Shutdown was called.
  // GMP thread only.
  bool mShutdownCalled;
};


} // namespace mozilla

#endif // GMPCDMProxy_h_
