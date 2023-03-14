/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEAUDIOSTREAM_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEAUDIOSTREAM_H

#include "MFMediaEngineStream.h"

namespace mozilla {

class MFMediaSource;

class MFMediaEngineAudioStream final : public MFMediaEngineStream {
 public:
  MFMediaEngineAudioStream() = default;

  static MFMediaEngineAudioStream* Create(uint64_t aStreamId,
                                          const TrackInfo& aInfo,
                                          MFMediaSource* aParentSource);

  nsCString GetDescriptionName() const override {
    return "media engine audio stream"_ns;
  }

  nsCString GetCodecName() const override;

  TrackInfo::TrackType TrackType() override {
    return TrackInfo::TrackType::kAudioTrack;
  }

  bool IsEncrypted() const override;

 private:
  HRESULT CreateMediaType(const TrackInfo& aInfo,
                          IMFMediaType** aMediaType) override;

  bool HasEnoughRawData() const override;

  already_AddRefed<MediaData> OutputDataInternal() override;

  // For MF_MT_USER_DATA. Currently only used for AAC.
  nsTArray<BYTE> mAACUserData;

  // Set when `CreateMediaType()` is called.
  AudioInfo mAudioInfo;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEAUDIOSTREAM_H
