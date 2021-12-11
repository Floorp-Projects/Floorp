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
#include "nsTHashMap.h"
#include "PlatformDecoderModule.h"
#include "ImageContainer.h"
#include "mozilla/Maybe.h"
#include "mozilla/Span.h"
#include "ReorderQueue.h"

class ChromiumCDMCallback;

namespace mozilla {

class ErrorResult;
class MediaRawData;
class ChromiumCDMProxy;

namespace gmp {

class GMPContentParent;

/**
 * ChromiumCDMParent is the content process IPC actor used to communicate with a
 * CDM in the GMP process (where ChromiumCDMChild lives). All non-static
 * members of this class are GMP thread only.
 */
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

  // Notifies this parent of the current output protection status. This will
  // update cached status and resolve outstanding queries from the CDM if one
  // exists.
  void NotifyOutputProtectionStatus(bool aSuccess, uint32_t aLinkMask,
                                    uint32_t aProtectionMask);

  void GetStatusForPolicy(uint32_t aPromiseId,
                          const nsCString& aMinHdcpVersion);

  RefPtr<DecryptPromise> Decrypt(MediaRawData* aSample);

  // TODO: Add functions for clients to send data to CDM, and
  // a Close() function.
  RefPtr<MediaDataDecoder::InitPromise> InitializeVideoDecoder(
      const gmp::CDMVideoDecoderConfig& aConfig, const VideoInfo& aInfo,
      RefPtr<layers::ImageContainer> aImageContainer,
      RefPtr<layers::KnowsCompositor> aKnowsCompositor);

  RefPtr<MediaDataDecoder::DecodePromise> DecryptAndDecodeFrame(
      MediaRawData* aSample);

  RefPtr<MediaDataDecoder::FlushPromise> FlushVideoDecoder();

  RefPtr<MediaDataDecoder::DecodePromise> Drain();

  RefPtr<ShutdownPromise> ShutdownVideoDecoder();

  void Shutdown();

 protected:
  ~ChromiumCDMParent() = default;

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
  ipc::IPCResult RecvOnQueryOutputProtectionStatus();
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

  void RejectPromise(uint32_t aPromiseId, ErrorResult&& aException,
                     const nsCString& aErrorMessage);

  void ResolvePromise(uint32_t aPromiseId);
  // Helpers to reject our promise if we are shut down.
  void RejectPromiseShutdown(uint32_t aPromiseId);
  // Helper to reject our promise with an InvalidStateError and the given
  // message.
  void RejectPromiseWithStateError(uint32_t aPromiseId,
                                   const nsCString& aErrorMessage);

  // Complete the CDMs request for us to check protection status by responding
  // to the CDM child with the requested info.
  void CompleteQueryOutputProtectionStatus(bool aSuccess, uint32_t aLinkMask,
                                           uint32_t aProtectionMask);

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
  nsTHashMap<nsUint32HashKey, uint32_t> mPromiseToCreateSessionToken;
  nsTArray<RefPtr<DecryptJob>> mDecrypts;

  MozPromiseHolder<InitPromise> mInitPromise;

  MozPromiseHolder<MediaDataDecoder::InitPromise> mInitVideoDecoderPromise;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDecodePromise;

  RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::KnowsCompositor> mKnowsCompositor;
  VideoInfo mVideoInfo;
  uint64_t mLastStreamOffset = 0;

  MozPromiseHolder<MediaDataDecoder::FlushPromise> mFlushDecoderPromise;

  size_t mVideoFrameBufferSize = 0;

  // Count of the number of shmems in the set used to return decoded video
  // frames from the CDM to Gecko.
  uint32_t mVideoShmemsActive = 0;
  // Maximum number of shmems to use to return decoded video frames.
  uint32_t mVideoShmemLimit;

  // Tracks if we have an outstanding request for output protection information.
  // This will be set to true if the CDM requests the information and we haven't
  // yet received it from up the stack and need to query up.
  bool mAwaitingOutputProtectionInformation = false;
  // The cached link mask for QueryOutputProtectionStatus related calls. If
  // this isn't set we'll call up the stack to MediaKeys to request the
  // information, otherwise we'll use the cached value and rely on MediaKeys
  // to notify us if the mask changes.
  Maybe<uint32_t> mOutputProtectionLinkMask;

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

#ifdef DEBUG
  // The GMP thread. Used to MOZ_ASSERT methods run on the GMP thread.
  const nsCOMPtr<nsISerialEventTarget> mGMPThread;
#endif
};

}  // namespace gmp
}  // namespace mozilla

#endif  // ChromiumCDMParent_h_
