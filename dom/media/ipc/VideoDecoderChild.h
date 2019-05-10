/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_ipc_VideoDecoderChild_h
#define include_ipc_VideoDecoderChild_h

#include "MediaResult.h"
#include "PlatformDecoderModule.h"
#include "mozilla/PVideoDecoderChild.h"
#include "IRemoteDecoderChild.h"

namespace mozilla {

class RemoteVideoDecoder;
class RemoteDecoderModule;
class VideoDecoderManagerChild;
using mozilla::ipc::IPCResult;

class VideoDecoderChild final : public PVideoDecoderChild,
                                public IRemoteDecoderChild {
  friend class PVideoDecoderChild;

 public:
  explicit VideoDecoderChild();

  // PVideoDecoderChild
  IPCResult RecvOutput(const VideoDataIPDL& aData);
  IPCResult RecvInputExhausted();
  IPCResult RecvDrainComplete();
  IPCResult RecvError(const nsresult& aError);
  IPCResult RecvInitComplete(const nsCString& aDecoderDescription,
                             const bool& aHardware,
                             const nsCString& aHardwareReason,
                             const uint32_t& aConversion);
  IPCResult RecvInitFailed(const nsresult& aReason);
  IPCResult RecvFlushComplete();
  IPCResult RecvShutdownComplete();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<MediaDataDecoder::InitPromise> Init() override;
  RefPtr<MediaDataDecoder::DecodePromise> Decode(
      MediaRawData* aSample) override;
  RefPtr<MediaDataDecoder::DecodePromise> Drain() override;
  RefPtr<MediaDataDecoder::FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  nsCString GetDescriptionName() const override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  MediaDataDecoder::ConversionRequired NeedsConversion() const override;
  void DestroyIPDL() override;

  MOZ_IS_CLASS_INIT
  MediaResult InitIPDL(const VideoInfo& aVideoInfo, float aFramerate,
                       const CreateDecoderParams::OptionSet& aOptions,
                       const layers::TextureFactoryIdentifier& aIdentifier);

  // Called from IPDL when our actor has been destroyed
  void IPDLActorDestroyed();

  VideoDecoderManagerChild* GetManager();

 private:
  ~VideoDecoderChild();

  void AssertOnManagerThread() const;

  RefPtr<VideoDecoderChild> mIPDLSelfRef;
  RefPtr<nsIThread> mThread;

  MozPromiseHolder<MediaDataDecoder::InitPromise> mInitPromise;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDecodePromise;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDrainPromise;
  MozPromiseHolder<MediaDataDecoder::FlushPromise> mFlushPromise;
  MozPromiseHolder<ShutdownPromise> mShutdownPromise;

  nsCString mHardwareAcceleratedReason;
  nsCString mDescription;
  bool mCanSend;
  bool mInitialized;
  bool mIsHardwareAccelerated;
  MediaDataDecoder::ConversionRequired mConversion;

  // Set to true if the actor got destroyed and we haven't yet notified the
  // caller.
  bool mNeedNewDecoder;
  MediaDataDecoder::DecodedData mDecodedData;

  nsCString mBlacklistedD3D11Driver;
  nsCString mBlacklistedD3D9Driver;
  TimeStamp mGPUCrashTime;
  // Keep this instance alive during SendShutdown RecvShutdownComplete
  // handshake.
  RefPtr<VideoDecoderChild> mShutdownSelfRef;
};

}  // namespace mozilla

#endif  // include_ipc_VideoDecoderChild_h
