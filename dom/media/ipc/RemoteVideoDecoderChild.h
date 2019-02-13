/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteVideoDecoderChild_h
#define include_dom_media_ipc_RemoteVideoDecoderChild_h
#include "mozilla/PRemoteVideoDecoderChild.h"
#include "IRemoteDecoderChild.h"

#include "MediaResult.h"

namespace mozilla {
namespace layers {
class BufferRecycleBin;
}
}  // namespace mozilla

namespace mozilla {

class RemoteDecoderManagerChild;
using mozilla::MediaDataDecoder;
using mozilla::ipc::IPCResult;

class RemoteVideoDecoderChild final : public PRemoteVideoDecoderChild,
                                      public IRemoteDecoderChild {
  friend class PRemoteVideoDecoderChild;

 public:
  explicit RemoteVideoDecoderChild();

  // PRemoteVideoDecoderChild
  IPCResult RecvVideoOutput(const RemoteVideoDataIPDL& aData);
  IPCResult RecvInputExhausted();
  IPCResult RecvDrainComplete();
  IPCResult RecvError(const nsresult& aError);
  IPCResult RecvInitComplete(const nsCString& aDecoderDescription,
                             const ConversionRequired& aConversion);
  IPCResult RecvInitFailed(const nsresult& aReason);
  IPCResult RecvFlushComplete();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  // IRemoteDecoderChild
  RefPtr<MediaDataDecoder::InitPromise> Init() override;
  RefPtr<MediaDataDecoder::DecodePromise> Decode(
      MediaRawData* aSample) override;
  RefPtr<MediaDataDecoder::DecodePromise> Drain() override;
  RefPtr<MediaDataDecoder::FlushPromise> Flush() override;
  void Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  nsCString GetDescriptionName() const override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  MediaDataDecoder::ConversionRequired NeedsConversion() const override;
  void DestroyIPDL() override;

  MOZ_IS_CLASS_INIT
  MediaResult InitIPDL(const VideoInfo& aVideoInfo, float aFramerate,
                       const CreateDecoderParams::OptionSet& aOptions);

  // Called from IPDL when our actor has been destroyed
  void IPDLActorDestroyed();

  RemoteDecoderManagerChild* GetManager();

 private:
  ~RemoteVideoDecoderChild();

  void AssertOnManagerThread() const;
  RefPtr<mozilla::layers::Image> DeserializeImage(
      const SurfaceDescriptorBuffer& sdBuffer, const IntSize& aPicSize);

  RefPtr<RemoteVideoDecoderChild> mIPDLSelfRef;
  RefPtr<nsIThread> mThread;

  MozPromiseHolder<MediaDataDecoder::InitPromise> mInitPromise;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDecodePromise;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDrainPromise;
  MozPromiseHolder<MediaDataDecoder::FlushPromise> mFlushPromise;

  nsCString mHardwareAcceleratedReason;
  nsCString mDescription;
  bool mCanSend;
  bool mInitialized;
  bool mIsHardwareAccelerated;
  MediaDataDecoder::ConversionRequired mConversion;
  MediaDataDecoder::DecodedData mDecodedData;
  RefPtr<mozilla::layers::BufferRecycleBin> mBufferRecycleBin;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteVideoDecoderChild_h
