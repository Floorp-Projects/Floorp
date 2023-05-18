/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H

#include "MFMediaEngineStream.h"
#include "WMFUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace layers {

class Image;
class DcompSurfaceImage;

}  // namespace layers

class MFMediaSource;

class MFMediaEngineVideoStream final : public MFMediaEngineStream {
 public:
  MFMediaEngineVideoStream() = default;

  static MFMediaEngineVideoStream* Create(uint64_t aStreamId,
                                          const TrackInfo& aInfo,
                                          MFMediaSource* aParentSource);
  nsCString GetDescriptionName() const override {
    return "media engine video stream"_ns;
  }

  nsCString GetCodecName() const override;

  TrackInfo::TrackType TrackType() override {
    return TrackInfo::TrackType::kVideoTrack;
  }

  void SetKnowsCompositor(layers::KnowsCompositor* aKnowsCompositor);

  void SetDCompSurfaceHandle(HANDLE aDCompSurfaceHandle, gfx::IntSize aDisplay);

  MFMediaEngineVideoStream* AsVideoStream() override { return this; }

  MediaDataDecoder::ConversionRequired NeedsConversion() const override;

  // Called by MFMediaEngineParent when we are creating a video decoder for
  // the remote decoder. This is used to detect if the inband video config
  // change happens during playback.
  void SetConfig(const TrackInfo& aConfig);

  RefPtr<MediaDataDecoder::DecodePromise> Drain() override;

  bool IsEncrypted() const override;

 private:
  HRESULT
  CreateMediaType(const TrackInfo& aInfo, IMFMediaType** aMediaType) override;

  bool HasEnoughRawData() const override;

  void UpdateConfig(const VideoInfo& aInfo);

  already_AddRefed<MediaData> OutputDataInternal() override;

  bool IsDCompImageReady();

  void ResolvePendingDrainPromiseIfNeeded();

  void ShutdownCleanUpOnTaskQueue() override;

  bool IsEnded() const override;

  // Task queue only members.
  HANDLE mDCompSurfaceHandle;
  bool mNeedRecreateImage;
  RefPtr<layers::KnowsCompositor> mKnowsCompositor;

  Mutex mMutex{"MFMediaEngineVideoStream"};
  gfx::IntSize mDisplay MOZ_GUARDED_BY(mMutex);

  // Set on the initialization, won't be changed after that.
  WMFStreamType mStreamType;

  // Created and accessed in the decoder thread.
  RefPtr<layers::DcompSurfaceImage> mDcompSurfaceImage;

  // This flag is used to check if the video config changes detected by the
  // media config monitor. When the video decoder get created first, we will set
  // this flag to true, then we know any config being set afterward indicating
  // a new config change.
  bool mHasReceivedInitialCreateDecoderConfig;

  // When draining, the track should return all decoded data. However, if the
  // dcomp image hasn't been ready yet, then we won't have any decoded data to
  // return. This promise is used for that case, and will be resolved once we
  // have dcomp image.
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mPendingDrainPromise;

  // Set when `CreateMediaType()` is called.
  bool mIsEncrypted = false;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H
