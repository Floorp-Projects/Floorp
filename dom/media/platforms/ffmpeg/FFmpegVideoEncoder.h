/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_

#include "FFmpegDataEncoder.h"
#include "FFmpegLibWrapper.h"
#include "PlatformEncoderModule.h"
#include "SimpleMap.h"

// This must be the last header included
#include "FFmpegLibs.h"

namespace mozilla {

template <int V>
class FFmpegVideoEncoder : public MediaDataEncoder {};

template <>
class FFmpegVideoEncoder<LIBAV_VER> : public FFmpegDataEncoder<LIBAV_VER> {
  using DurationMap = SimpleMap<int64_t>;

 public:
  FFmpegVideoEncoder(const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
                     const RefPtr<TaskQueue>& aTaskQueue,
                     const EncoderConfig& aConfig);

  nsCString GetDescriptionName() const override;

 protected:
  // Methods only called on mTaskQueue.
  virtual nsresult InitSpecific() override;
#if LIBAVCODEC_VERSION_MAJOR >= 58
  Result<EncodedData, nsresult> EncodeInputWithModernAPIs(
      RefPtr<const MediaData> aSample) override;
#endif
  bool ScaleInputFrame();
  virtual RefPtr<MediaRawData> ToMediaRawData(AVPacket* aPacket) override;
  Result<already_AddRefed<MediaByteBuffer>, nsresult> GetExtraData(
      AVPacket* aPacket) override;
  void ForceEnablingFFmpegDebugLogs();
  struct SVCSettings {
    nsTArray<uint8_t> mTemporalLayerIds;
    // A key-value pair for av_opt_set.
    std::pair<nsCString, nsCString> mSettingKeyValue;
  };
  Maybe<SVCSettings> GetSVCSettings();
  struct H264Settings {
    int mProfile;
    int mLevel;
    // A list of key-value pairs for av_opt_set.
    nsTArray<std::pair<nsCString, nsCString>> mSettingKeyValuePairs;
  };
  H264Settings GetH264Settings(const H264Specific& aH264Specific);
  struct SVCInfo {
    explicit SVCInfo(nsTArray<uint8_t>&& aTemporalLayerIds)
        : mTemporalLayerIds(std::move(aTemporalLayerIds)), mNextIndex(0) {}
    const nsTArray<uint8_t> mTemporalLayerIds;
    size_t mNextIndex;
    // Return the current temporal layer id and update the next.
    uint8_t UpdateTemporalLayerId();
  };
  Maybe<SVCInfo> mSVCInfo{};
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_
