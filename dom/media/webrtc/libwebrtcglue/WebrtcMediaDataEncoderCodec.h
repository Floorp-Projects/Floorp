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
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcMediaDataEncoder, final);

  static bool CanCreate(const webrtc::VideoCodecType aCodecType);

  explicit WebrtcMediaDataEncoder(const webrtc::SdpVideoFormat& aFormat);

  int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                     const webrtc::VideoEncoder::Settings& aSettings) override;

  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) override;

  int32_t Shutdown() override;

  int32_t Encode(
      const webrtc::VideoFrame& aInputFrame,
      const std::vector<webrtc::VideoFrameType>* aFrameTypes) override;

  int32_t SetRates(
      const webrtc::VideoEncoder::RateControlParameters& aParameters) override;

  WebrtcVideoEncoder::EncoderInfo GetEncoderInfo() const override;
  MediaEventSource<uint64_t>* InitPluginEvent() override { return nullptr; }

  MediaEventSource<uint64_t>* ReleasePluginEvent() override { return nullptr; }

 private:
  virtual ~WebrtcMediaDataEncoder();

  bool SetupConfig(const webrtc::VideoCodec* aCodecSettings);
  already_AddRefed<MediaDataEncoder> CreateEncoder(
      const webrtc::VideoCodec* aCodecSettings);
  bool InitEncoder();

  const RefPtr<TaskQueue> mTaskQueue;
  const RefPtr<PEMFactory> mFactory;
  RefPtr<MediaDataEncoder> mEncoder;

  Mutex mCallbackMutex MOZ_UNANNOTATED;  // Protects mCallback and mError.
  webrtc::EncodedImageCallback* mCallback = nullptr;
  MediaResult mError = NS_OK;

  VideoInfo mInfo;
  webrtc::SdpVideoFormat::Parameters mFormatParams;
  webrtc::CodecSpecificInfo mCodecSpecific;
  webrtc::BitrateAdjuster mBitrateAdjuster;
  uint32_t mMaxFrameRate = {0};
  uint32_t mMinBitrateBps = {0};
  uint32_t mMaxBitrateBps = {0};
};

}  // namespace mozilla

#endif  // WebrtcMediaDataEncoderCodec_h__
