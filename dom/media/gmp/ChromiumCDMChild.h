/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumCDMChild_h_
#define ChromiumCDMChild_h_

#include "content_decryption_module.h"
#include "mozilla/gmp/PChromiumCDMChild.h"
#include "SimpleMap.h"
#include "WidevineVideoFrame.h"

namespace mozilla {
namespace gmp {

class GMPContentChild;

class ChromiumCDMChild : public PChromiumCDMChild, public cdm::Host_10 {
 public:
  // Mark AddRef and Release as `final`, as they overload pure virtual
  // implementations in PChromiumCDMChild.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChromiumCDMChild, final);

  explicit ChromiumCDMChild(GMPContentChild* aPlugin);

  void Init(cdm::ContentDecryptionModule_10* aCDM, const nsCString& aStorageId);

  void TimerExpired(void* aContext);

  // cdm::Host_10 implementation
  cdm::Buffer* Allocate(uint32_t aCapacity) override;
  void SetTimer(int64_t aDelayMs, void* aContext) override;
  cdm::Time GetCurrentWallTime() override;
  void OnResolveKeyStatusPromise(uint32_t aPromiseId,
                                 cdm::KeyStatus aKeyStatus) override;
  void OnResolveNewSessionPromise(uint32_t aPromiseId, const char* aSessionId,
                                  uint32_t aSessionIdSize) override;
  void OnResolvePromise(uint32_t aPromiseId) override;
  void OnRejectPromise(uint32_t aPromiseId, cdm::Exception aException,
                       uint32_t aSystemCode, const char* aErrorMessage,
                       uint32_t aErrorMessageSize) override;
  void OnSessionMessage(const char* aSessionId, uint32_t aSessionIdSize,
                        cdm::MessageType aMessageType, const char* aMessage,
                        uint32_t aMessageSize) override;
  void OnSessionKeysChange(const char* aSessionId, uint32_t aSessionIdSize,
                           bool aHasAdditionalUsableKey,
                           const cdm::KeyInformation* aKeysInfo,
                           uint32_t aKeysInfoCount) override;
  void OnExpirationChange(const char* aSessionId, uint32_t aSessionIdSize,
                          cdm::Time aNewExpiryTime) override;
  void OnSessionClosed(const char* aSessionId,
                       uint32_t aSessionIdSize) override;
  void SendPlatformChallenge(const char* aServiceId, uint32_t aServiceIdSize,
                             const char* aChallenge,
                             uint32_t aChallengeSize) override {}
  void EnableOutputProtection(uint32_t aDesiredProtectionMask) override {}
  void QueryOutputProtectionStatus() override {}
  void OnDeferredInitializationDone(cdm::StreamType aStreamType,
                                    cdm::Status aDecoderStatus) override {}
  void RequestStorageId(uint32_t aVersion) override;
  cdm::FileIO* CreateFileIO(cdm::FileIOClient* aClient) override;
  void OnInitialized(bool aSuccess) override;
  // end cdm::Host_10 specific methods

  void GiveBuffer(ipc::Shmem&& aBuffer);

 protected:
  ~ChromiumCDMChild();

  bool OnResolveNewSessionPromiseInternal(uint32_t aPromiseId,
                                          const nsCString& aSessionId);

  bool IsOnMessageLoopThread();

  ipc::IPCResult RecvGiveBuffer(ipc::Shmem&& aShmem) override;
  ipc::IPCResult RecvPurgeShmems() override;
  void PurgeShmems();
  ipc::IPCResult RecvInit(const bool& aAllowDistinctiveIdentifier,
                          const bool& aAllowPersistentState,
                          InitResolver&& aResolver) override;
  ipc::IPCResult RecvSetServerCertificate(
      const uint32_t& aPromiseId, nsTArray<uint8_t>&& aServerCert) override;
  ipc::IPCResult RecvCreateSessionAndGenerateRequest(
      const uint32_t& aPromiseId, const uint32_t& aSessionType,
      const uint32_t& aInitDataType, nsTArray<uint8_t>&& aInitData) override;
  ipc::IPCResult RecvLoadSession(const uint32_t& aPromiseId,
                                 const uint32_t& aSessionType,
                                 const nsCString& aSessionId) override;
  ipc::IPCResult RecvUpdateSession(const uint32_t& aPromiseId,
                                   const nsCString& aSessionId,
                                   nsTArray<uint8_t>&& aResponse) override;
  ipc::IPCResult RecvCloseSession(const uint32_t& aPromiseId,
                                  const nsCString& aSessionId) override;
  ipc::IPCResult RecvRemoveSession(const uint32_t& aPromiseId,
                                   const nsCString& aSessionId) override;
  ipc::IPCResult RecvGetStatusForPolicy(
      const uint32_t& aPromiseId,
      const cdm::HdcpVersion& aMinHdcpVersion) override;
  ipc::IPCResult RecvDecrypt(const uint32_t& aId,
                             const CDMInputBuffer& aBuffer) override;
  ipc::IPCResult RecvInitializeVideoDecoder(
      const CDMVideoDecoderConfig& aConfig) override;
  ipc::IPCResult RecvDeinitializeVideoDecoder() override;
  ipc::IPCResult RecvResetVideoDecoder() override;
  ipc::IPCResult RecvDecryptAndDecodeFrame(
      const CDMInputBuffer& aBuffer) override;
  ipc::IPCResult RecvDrain() override;
  ipc::IPCResult RecvDestroy() override;

  void ReturnOutput(WidevineVideoFrame& aFrame);
  bool HasShmemOfSize(size_t aSize) const;

  template <typename MethodType, typename... ParamType>
  void CallMethod(MethodType, ParamType&&...);

  template <typename MethodType, typename... ParamType>
  void CallOnMessageLoopThread(const char* const, MethodType, ParamType&&...);

  GMPContentChild* mPlugin = nullptr;
  cdm::ContentDecryptionModule_10* mCDM = nullptr;

  typedef SimpleMap<uint64_t> DurationMap;
  DurationMap mFrameDurations;
  nsTArray<uint32_t> mLoadSessionPromiseIds;

  cdm::Size mCodedSize = {0, 0};
  nsTArray<ipc::Shmem> mBuffers;

  bool mDecoderInitialized = false;
  bool mPersistentStateAllowed = false;
  bool mDestroyed = false;
  nsCString mStorageId;

  typedef MozPromise<bool, nsresult, /* IsExclusive = */ true> InitPromise;
  // Created when we RecvInit, should be resolved once the CDM is initialized
  // or rejected if init fails.
  MozPromiseHolder<InitPromise> mInitPromise;
};

}  // namespace gmp
}  // namespace mozilla

#endif  // ChromiumCDMChild_h_
