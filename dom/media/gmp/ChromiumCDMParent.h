/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumCDMParent_h_
#define ChromiumCDMParent_h_

#include "DecryptJob.h"
#include "GMPCrashHelper.h"
#include "GMPCrashHelperHolder.h"
#include "GMPMessageUtils.h"
#include "mozilla/gmp/PChromiumCDMParent.h"
#include "mozilla/RefPtr.h"
#include "nsDataHashtable.h"
#include "PlatformDecoderModule.h"
#include "ImageContainer.h"
#include "mozilla/Span.h"

namespace mozilla {

class MediaRawData;
class ChromiumCDMProxy;

namespace gmp {

class GMPContentParent;

class ChromiumCDMParent final
  : public PChromiumCDMParent
  , public GMPCrashHelperHolder
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChromiumCDMParent)

  ChromiumCDMParent(GMPContentParent* aContentParent, uint32_t aPluginId);

  uint32_t PluginId() const { return mPluginId; }

  bool Init(ChromiumCDMProxy* aProxy,
            bool aAllowDistinctiveIdentifier,
            bool aAllowPersistentState);

  void CreateSession(uint32_t aCreateSessionToken,
                     uint32_t aSessionType,
                     uint32_t aInitDataType,
                     uint32_t aPromiseId,
                     const nsTArray<uint8_t>& aInitData);

  void LoadSession(uint32_t aPromiseId,
                   uint32_t aSessionType,
                   nsString aSessionId);

  void SetServerCertificate(uint32_t aPromiseId,
                            const nsTArray<uint8_t>& aCert);

  void UpdateSession(const nsCString& aSessionId,
                     uint32_t aPromiseId,
                     const nsTArray<uint8_t>& aResponse);

  void CloseSession(const nsCString& aSessionId, uint32_t aPromiseId);

  void RemoveSession(const nsCString& aSessionId, uint32_t aPromiseId);

  RefPtr<DecryptPromise> Decrypt(MediaRawData* aSample);

  // TODO: Add functions for clients to send data to CDM, and
  // a Close() function.
  RefPtr<MediaDataDecoder::InitPromise> InitializeVideoDecoder(
    const gmp::CDMVideoDecoderConfig& aConfig,
    const VideoInfo& aInfo,
    RefPtr<layers::ImageContainer> aImageContainer);

  RefPtr<MediaDataDecoder::DecodePromise> DecryptAndDecodeFrame(
    MediaRawData* aSample);

  RefPtr<MediaDataDecoder::FlushPromise> FlushVideoDecoder();

  RefPtr<MediaDataDecoder::DecodePromise> Drain();

  RefPtr<ShutdownPromise> ShutdownVideoDecoder();

  void Shutdown();

protected:
  ~ChromiumCDMParent() {}

  ipc::IPCResult Recv__delete__() override;
  ipc::IPCResult RecvOnResolveNewSessionPromise(
    const uint32_t& aPromiseId,
    const nsCString& aSessionId) override;
  ipc::IPCResult RecvResolveLoadSessionPromise(
    const uint32_t& aPromiseId,
    const bool& aSuccessful) override;
  ipc::IPCResult RecvOnResolvePromise(const uint32_t& aPromiseId) override;
  ipc::IPCResult RecvOnRejectPromise(const uint32_t& aPromiseId,
                                     const uint32_t& aError,
                                     const uint32_t& aSystemCode,
                                     const nsCString& aErrorMessage) override;
  ipc::IPCResult RecvOnSessionMessage(const nsCString& aSessionId,
                                      const uint32_t& aMessageType,
                                      nsTArray<uint8_t>&& aMessage) override;
  ipc::IPCResult RecvOnSessionKeysChange(
    const nsCString& aSessionId,
    nsTArray<CDMKeyInformation>&& aKeysInfo) override;
  ipc::IPCResult RecvOnExpirationChange(
    const nsCString& aSessionId,
    const double& aSecondsSinceEpoch) override;
  ipc::IPCResult RecvOnSessionClosed(const nsCString& aSessionId) override;
  ipc::IPCResult RecvOnLegacySessionError(const nsCString& aSessionId,
                                          const uint32_t& aError,
                                          const uint32_t& aSystemCode,
                                          const nsCString& aMessage) override;
  ipc::IPCResult RecvDecrypted(const uint32_t& aId,
                               const uint32_t& aStatus,
                               ipc::Shmem&& aData) override;
  ipc::IPCResult RecvDecryptFailed(const uint32_t& aId,
                                   const uint32_t& aStatus) override;
  ipc::IPCResult RecvOnDecoderInitDone(const uint32_t& aStatus) override;
  ipc::IPCResult RecvDecodedShmem(const CDMVideoFrame& aFrame,
                                  ipc::Shmem&& aShmem) override;
  ipc::IPCResult RecvDecodedData(const CDMVideoFrame& aFrame,
                                 nsTArray<uint8_t>&& aData) override;

  void ProcessDecoded(const CDMVideoFrame& aFrame,
                      Span<uint8_t> aData,
                      ipc::Shmem&& aGiftShmem);

  ipc::IPCResult RecvDecodeFailed(const uint32_t& aStatus) override;
  ipc::IPCResult RecvShutdown() override;
  ipc::IPCResult RecvResetVideoDecoderComplete() override;
  ipc::IPCResult RecvDrainComplete() override;
  void ActorDestroy(ActorDestroyReason aWhy) override;
  bool SendBufferToCDM(uint32_t aSizeInBytes);

  void RejectPromise(uint32_t aPromiseId,
                     nsresult aError,
                     const nsCString& aErrorMessage);

  void ResolvePromise(uint32_t aPromiseId);

  bool InitCDMInputBuffer(gmp::CDMInputBuffer& aBuffer, MediaRawData* aSample);

  const uint32_t mPluginId;
  GMPContentParent* mContentParent;
  // Note: this pointer is a weak reference because otherwise it would cause
  // a cycle, as ChromiumCDMProxy has a strong reference to the
  // ChromiumCDMParent.
  ChromiumCDMProxy* mProxy = nullptr;
  nsDataHashtable<nsUint32HashKey, uint32_t> mPromiseToCreateSessionToken;
  nsTArray<RefPtr<DecryptJob>> mDecrypts;

  MozPromiseHolder<MediaDataDecoder::InitPromise> mInitVideoDecoderPromise;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDecodePromise;

  RefPtr<layers::ImageContainer> mImageContainer;
  VideoInfo mVideoInfo;
  uint64_t mLastStreamOffset = 0;

  MozPromiseHolder<MediaDataDecoder::FlushPromise> mFlushDecoderPromise;

  int32_t mVideoFrameBufferSize = 0;

  // Count of the number of shmems in the set used to return decoded video
  // frames from the CDM to Gecko.
  uint32_t mVideoShmemCount;

  bool mIsShutdown = false;
  bool mVideoDecoderInitialized = false;
  bool mActorDestroyed = false;
};

} // namespace gmp
} // namespace mozilla

#endif // ChromiumCDMParent_h_
