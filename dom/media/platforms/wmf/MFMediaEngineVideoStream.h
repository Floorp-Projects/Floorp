/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H

#include "MFMediaEngineStream.h"

namespace mozilla {

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

 private:
  HRESULT CreateMediaType(const TrackInfo& aInfo,
                          IMFMediaType** aMediaType) override;

  bool HasEnoughRawData() const override;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H
