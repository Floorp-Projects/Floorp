/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebrtcMediaDataEncoderCodec_h__
#define WebrtcMediaDataEncoderCodec_h__

#include "MediaConduitInterface.h"
#include "MediaInfo.h"
#include "MediaResult.h"
#include "PlatformEncoderModule.h"
#include "WebrtcGmpVideoCodec.h"
#include "common_video/include/bitrate_adjuster.h"
#include "modules/video_coding/include/video_codec_interface.h"

namespace mozilla {

class MediaData;
class PEMFactory;
class SharedThreadPool;
class TaskQueue;

class WebrtcMediaDataEncoder : public RefCountedWebrtcVideoEncoder {
 public:
  static bool CanCreate(const webrtc::VideoCodecType aCodecType);

  explicit WebrtcMediaDataEncoder(const webrtc::SdpVideoFormat& aFormat);

  int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                     const webrtc::VideoEncoder::Settings& aSettings) override;

  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) override;

  int32_t Shutdown() override;

  int32_t Encode(
      const webrtc::VideoFrame& aFrame,
      const std::vector<webrtc::VideoFrameType>* aFrameTypes) override;

  int32_t SetRates(
      const webrtc::VideoEncoder::RateControlParameters& aParameters) override;

  MediaEventSource<uint64_t>* InitPluginEvent() override { return nullptr; }

  MediaEventSource<uint64_t>* ReleasePluginEvent() override { return nullptr; }

 private:
  virtual ~WebrtcMediaDataEncoder() = default;

  bool SetupConfig(const webrtc::VideoCodec* aCodecSettings);
  already_AddRefed<MediaDataEncoder> CreateEncoder(
      const webrtc::VideoCodec* aCodecSettings);
  bool InitEncoder();
  /*
    webrtc::RTPFragmentationHeader GetFragHeader(
        const webrtc::VideoCodecType aCodecType,
        const RefPtr<MediaRawData>& aFrame);
  */

  const RefPtr<TaskQueue> mTaskQueue;
  const RefPtr<PEMFactory> mFactory;
  RefPtr<MediaDataEncoder> mEncoder;

  Mutex mCallbackMutex;  // Protects mCallback and mError.
  webrtc::EncodedImageCallback* mCallback = nullptr;
  MediaResult mError = NS_OK;

  VideoInfo mInfo;
  webrtc::SdpVideoFormat::Parameters mFormatParams;
  webrtc::CodecSpecificInfo mCodecSpecific;
  webrtc::BitrateAdjuster mBitrateAdjuster;
  uint32_t mMaxFrameRate;
  uint32_t mMinBitrateBps;
  uint32_t mMaxBitrateBps;
};

}  // namespace mozilla

#endif  // WebrtcMediaDataEncoderCodec_h__
