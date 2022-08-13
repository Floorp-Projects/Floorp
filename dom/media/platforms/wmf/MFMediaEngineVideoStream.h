/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H

#include "MFMediaEngineStream.h"
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

  TrackInfo::TrackType TrackType() override {
    return TrackInfo::TrackType::kVideoTrack;
  }

  void SetKnowsCompositor(layers::KnowsCompositor* aKnowsCompositor) {
    MutexAutoLock lock(mMutex);
    mKnowsCompositor = aKnowsCompositor;
  }

  HANDLE GetDcompSurfaceHandle() {
    MutexAutoLock lock(mMutex);
    return mDCompSurfaceHandle;
  }
  void SetDCompSurfaceHandle(HANDLE aDCompSurfaceHandle);

  MFMediaEngineVideoStream* AsVideoStream() override { return this; }

  layers::Image* GetDcompSurfaceImage();

  already_AddRefed<MediaData> OutputData(MediaRawData* aSample) override;

  MediaDataDecoder::ConversionRequired NeedsConversion() const override;

 private:
  HRESULT
  CreateMediaType(const TrackInfo& aInfo, IMFMediaType** aMediaType) override;

  bool HasEnoughRawData() const override;

  // This info will be set inside the ctor of video stream, and shouldn't be
  // changed after that.
  VideoInfo mInfo;

  Mutex mMutex{"MFMediaEngineVideoStream"};

  HANDLE mDCompSurfaceHandle MOZ_GUARDED_BY(mMutex);
  bool mNeedRecreateImage MOZ_GUARDED_BY(mMutex);
  RefPtr<layers::KnowsCompositor> mKnowsCompositor MOZ_GUARDED_BY(mMutex);

  // Created and accessed in the decoder thread.
  RefPtr<layers::DcompSurfaceImage> mDcompSurfaceImage;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H
