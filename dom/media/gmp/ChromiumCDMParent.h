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
#include "ReorderQueue.h"

class ChromiumCDMCallback;

namespace mozilla {

class MediaRawData;
class ChromiumCDMProxy;

namespace gmp {

class GMPContentParent;

class ChromiumCDMParent final : public PChromiumCDMParent,
                                public GMPCrashHelperHolder {
  friend class PChromiumCDMParent;

 public:
  typedef MozPromise<bool, MediaResult, /* IsExclusive = */ true> InitPromise;

  // Mark AddRef and Release as `final`, as they overload pure virtual
  // implementations in PChromiumCDMParent.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChromiumCDMParent, final)

  ChromiumCDMParent(GMPContentParent* aContentParent, uint32_t aPluginId);

  uint32_t PluginId() const { return mPluginId; }

  RefPtr<InitPromise> Init(ChromiumCDMCallback* aCDMCallback,
                           bool aAllowDistinctiveIdentifier,
                           bool aAllowPersistentState,
                           nsIEventTarget* aMainThread);

  void CreateSession(uint32_t aCreateSessionToken, uint32_t aSessionType,
                     uint32_t aInitDataType, uint32_t aPromiseId,
                     const nsTArray<uint8_t>& aInitData);

  void LoadSession(uint32_t aPromiseId, uint32_t aSessionType,
                   nsString aSessionId);

  void SetServerCertificate(uint32_t aPromiseId,
                            const nsTArray<uint8_t>& aCert);

  void UpdateSession(const nsCString& aSessionId, uint32_t aPromiseId,
                     const nsTArray<uint8_t>& aResponse);

  void CloseSession(const nsCString& aSessionId, uint32_t aPromiseId);

  void RemoveSession(const nsCString& aSessionId, uint32_t aPromiseId);

  void GetStatusForPolicy(uint32_t aPromiseId,
                          const nsCString& aMinHdcpVersion);

  RefPtr<DecryptPromise> Decrypt(MediaRawData* aSample);

  // TODO: Add functions for clients to send data to CDM, and
  // a Close() function.
  RefPtr<MediaDataDecoder::InitPromise> InitializeVideoDecoder(
      const gmp::CDMVideoDecoderConfig& aConfig, const VideoInfo& aInfo,
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
  ipc::IPCResult RecvOnResolvePromiseWithKeyStatus(const uint32_t& aPromiseId,
                                                   const uint32_t& aKeyStatus);
  ipc::IPCResult RecvOnResolveNewSessionPromise(const uint32_t& aPromiseId,
                                                const nsCString& aSessionId);
  ipc::IPCResult RecvResolveLoadSessionPromise(const uint32_t& aPromiseId,
                                               const bool& aSuccessful);
  ipc::IPCResult RecvOnResolvePromise(const uint32_t& aPromiseId);
  ipc::IPCResult RecvOnRejectPromise(const uint32_t& aPromiseId,
                                     const uint32_t& aError,
                                     const uint32_t& aSystemCode,
                                     const nsCString& aErrorMessage);
  ipc::IPCResult RecvOnSessionMessage(const nsCString& aSessionId,
                                      const uint32_t& aMessageType,
                                      nsTArray<uint8_t>&& aMessage);
  ipc::IPCResult RecvOnSessionKeysChange(
      const nsCString& aSessionId, nsTArray<CDMKeyInformation>&& aKeysInfo);
  ipc::IPCResult RecvOnExpirationChange(const nsCString& aSessionId,
                                        const double& aSecondsSinceEpoch);
  ipc::IPCResult RecvOnSessionClosed(const nsCString& aSessionId);
  ipc::IPCResult RecvDecrypted(const uint32_t& aId, const uint32_t& aStatus,
                               ipc::Shmem&& aData);
  ipc::IPCResult RecvDecryptFailed(const uint32_t& aId,
                                   const uint32_t& aStatus);
  ipc::IPCResult RecvOnDecoderInitDone(const uint32_t& aStatus);
  ipc::IPCResult RecvDecodedShmem(const CDMVideoFrame& aFrame,
                                  ipc::Shmem&& aShmem);
  ipc::IPCResult RecvDecodedData(const CDMVideoFrame& aFrame,
                                 nsTArray<uint8_t>&& aData);
  ipc::IPCResult RecvDecodeFailed(const uint32_t& aStatus);
  ipc::IPCResult RecvShutdown();
  ipc::IPCResult RecvResetVideoDecoderComplete();
  ipc::IPCResult RecvDrainComplete();
  ipc::IPCResult RecvIncreaseShmemPoolSize();
  void ActorDestroy(ActorDestroyReason aWhy) override;
  bool SendBufferToCDM(uint32_t aSizeInBytes);

  void ReorderAndReturnOutput(RefPtr<VideoData>&& aFrame);

  void RejectPromise(uint32_t aPromiseId, nsresult aError,
                     const nsCString& aErrorMessage);

  void ResolvePromise(uint32_t aPromiseId);

  bool InitCDMInputBuffer(gmp::CDMInputBuffer& aBuffer, MediaRawData* aSample);

  bool PurgeShmems();
  bool EnsureSufficientShmems(size_t aVideoFrameSize);
  already_AddRefed<VideoData> CreateVideoFrame(const CDMVideoFrame& aFrame,
                                               Span<uint8_t> aData);

  const uint32_t mPluginId;
  GMPContentParent* mContentParent;
  // Note: this pointer is a weak reference as ChromiumCDMProxy has a strong
  // reference to the ChromiumCDMCallback.
  ChromiumCDMCallback* mCDMCallback = nullptr;
  nsDataHashtable<nsUint32HashKey, uint32_t> mPromiseToCreateSessionToken;
  nsTArray<RefPtr<DecryptJob>> mDecrypts;

  MozPromiseHolder<InitPromise> mInitPromise;

  MozPromiseHolder<MediaDataDecoder::InitPromise> mInitVideoDecoderPromise;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDecodePromise;

  RefPtr<layers::ImageContainer> mImageContainer;
  VideoInfo mVideoInfo;
  uint64_t mLastStreamOffset = 0;

  MozPromiseHolder<MediaDataDecoder::FlushPromise> mFlushDecoderPromise;

  size_t mVideoFrameBufferSize = 0;

  // Count of the number of shmems in the set used to return decoded video
  // frames from the CDM to Gecko.
  uint32_t mVideoShmemsActive = 0;
  // Maximum number of shmems to use to return decoded video frames.
  uint32_t mVideoShmemLimit;

  bool mIsShutdown = false;
  bool mVideoDecoderInitialized = false;
  bool mActorDestroyed = false;
  bool mAbnormalShutdown = false;

  // The H.264 decoder in Widevine CDM versions 970 and later output in decode
  // order rather than presentation order, so we reorder in presentation order
  // before presenting. mMaxRefFrames is non-zero if we have an initialized
  // decoder and we are decoding H.264. If so, it stores the maximum length of
  // the reorder queue that we need. Note we may have multiple decoders for the
  // life time of this object, but never more than one active at once.
  uint32_t mMaxRefFrames = 0;
  ReorderQueue mReorderQueue;
};

}  // namespace gmp
}  // namespace mozilla

#endif  // ChromiumCDMParent_h_
