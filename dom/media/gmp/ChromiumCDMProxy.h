/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumCDMProxy_h_
#define ChromiumCDMProxy_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/CDMProxy.h"
#include "ChromiumCDMParent.h"

namespace mozilla {

class ErrorResult;
class MediaRawData;
class DecryptJob;
class ChromiumCDMCallbackProxy;
class ChromiumCDMProxy : public CDMProxy {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChromiumCDMProxy, override)

  ChromiumCDMProxy(dom::MediaKeys* aKeys, const nsAString& aKeySystem,
                   GMPCrashHelper* aCrashHelper,
                   bool aAllowDistinctiveIdentifier,
                   bool aAllowPersistentState);

  void Init(PromiseId aPromiseId, const nsAString& aOrigin,
            const nsAString& aTopLevelOrigin,
            const nsAString& aGMPName) override;

  void CreateSession(uint32_t aCreateSessionToken,
                     dom::MediaKeySessionType aSessionType,
                     PromiseId aPromiseId, const nsAString& aInitDataType,
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
  // For use for moving rejections from off-main thread.
  void RejectPromiseOnMainThread(PromiseId aId,
                                 CopyableErrorResult&& aException,
                                 const nsCString& aReason);

  void ResolvePromise(PromiseId aId) override;

  const nsString& KeySystem() const override;

  DataMutex<CDMCaps>& Capabilites() override;

  void OnKeyStatusesChange(const nsAString& aSessionId) override;

  void GetStatusForPolicy(PromiseId aPromiseId,
                          const nsAString& aMinHdcpVersion) override;

#ifdef DEBUG
  bool IsOnOwnerThread() override;
#endif

  ChromiumCDMProxy* AsChromiumCDMProxy() override { return this; }

  // Threadsafe. Note this may return a reference to a shutdown
  // CDM, which will fail on all operations.
  already_AddRefed<gmp::ChromiumCDMParent> GetCDMParent();

  void OnResolvePromiseWithKeyStatus(uint32_t aPromiseId,
                                     dom::MediaKeyStatus aKeyStatus);

 private:
  void OnCDMCreated(uint32_t aPromiseId);
  void ShutdownCDMIfExists();

  ~ChromiumCDMProxy();

  // True if Shutdown() has been called. Should only be read and written on
  // main thread.
  bool mIsShutdown = false;

  RefPtr<GMPCrashHelper> mCrashHelper;

  Mutex mCDMMutex;
  RefPtr<gmp::ChromiumCDMParent> mCDM;
  nsCOMPtr<nsISerialEventTarget> mGMPThread;
  UniquePtr<ChromiumCDMCallbackProxy> mCallback;
};

}  // namespace mozilla

#endif  // ChromiumCDMProxy_h_
