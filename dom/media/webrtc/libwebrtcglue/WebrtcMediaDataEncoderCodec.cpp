/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcMediaDataEncoderCodec.h"

#include "AnnexB.h"
#include "ImageContainer.h"
#include "MediaData.h"
#include "PEMFactory.h"
#include "VideoUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/Span.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/media/MediaUtils.h"
#include "media/base/media_constants.h"
#include "media/base/h264_profile_level_id.h"
#include "system_wrappers/include/clock.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"

namespace mozilla {

extern LazyLogModule sPEMLog;

#undef LOG
#define LOG(msg, ...)               \
  MOZ_LOG(sPEMLog, LogLevel::Debug, \
          ("WebrtcMediaDataEncoder=%p, " msg, this, ##__VA_ARGS__))

#undef LOG_V
#define LOG_V(msg, ...)               \
  MOZ_LOG(sPEMLog, LogLevel::Verbose, \
          ("WebrtcMediaDataEncoder=%p, " msg, this, ##__VA_ARGS__))

using namespace media;
using namespace layers;
using MimeTypeResult = Maybe<nsLiteralCString>;

static MimeTypeResult ConvertWebrtcCodecTypeToMimeType(
    const webrtc::VideoCodecType& aType) {
  switch (aType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      return Some("video/vp8"_ns);
    case webrtc::VideoCodecType::kVideoCodecVP9:
      return Some("video/vp9"_ns);
    case webrtc::VideoCodecType::kVideoCodecH264:
      return Some("video/avc"_ns);
    default:
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unsupported codec type");
  }
  return Nothing();
}

bool WebrtcMediaDataEncoder::CanCreate(
    const webrtc::VideoCodecType aCodecType) {
  auto factory = MakeRefPtr<PEMFactory>();
  MimeTypeResult mimeType = ConvertWebrtcCodecTypeToMimeType(aCodecType);
  return mimeType ? factory->SupportsMimeType(mimeType.ref()) : false;
}

static const char* PacketModeStr(const webrtc::CodecSpecificInfo& aInfo) {
  MOZ_ASSERT(aInfo.codecType != webrtc::VideoCodecType::kVideoCodecGeneric);

  if (aInfo.codecType != webrtc::VideoCodecType::kVideoCodecH264) {
    return "N/A";
  }
  switch (aInfo.codecSpecific.H264.packetization_mode) {
    case webrtc::H264PacketizationMode::SingleNalUnit:
      return "SingleNalUnit";
    case webrtc::H264PacketizationMode::NonInterleaved:
      return "NonInterleaved";
    default:
      return "Unknown";
  }
}

static MediaDataEncoder::H264Specific::ProfileLevel ConvertProfileLevel(
    const webrtc::SdpVideoFormat::Parameters& aParameters) {
  const absl::optional<webrtc::H264::ProfileLevelId> profileLevel =
      webrtc::H264::ParseSdpProfileLevelId(aParameters);
  if (profileLevel &&
      (profileLevel->profile == webrtc::H264::Profile::kProfileBaseline ||
       profileLevel->profile ==
           webrtc::H264::Profile::kProfileConstrainedBaseline)) {
    return MediaDataEncoder::H264Specific::ProfileLevel::BaselineAutoLevel;
  }
  return MediaDataEncoder::H264Specific::ProfileLevel::MainAutoLevel;
}

static MediaDataEncoder::VPXSpecific::Complexity MapComplexity(
    webrtc::VideoCodecComplexity aComplexity) {
  switch (aComplexity) {
    case webrtc::VideoCodecComplexity::kComplexityNormal:
      return MediaDataEncoder::VPXSpecific::Complexity::Normal;
    case webrtc::VideoCodecComplexity::kComplexityHigh:
      return MediaDataEncoder::VPXSpecific::Complexity::High;
    case webrtc::VideoCodecComplexity::kComplexityHigher:
      return MediaDataEncoder::VPXSpecific::Complexity::Higher;
    case webrtc::VideoCodecComplexity::kComplexityMax:
      return MediaDataEncoder::VPXSpecific::Complexity::Max;
    default:
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Bad complexity value");
  }
}

WebrtcMediaDataEncoder::WebrtcMediaDataEncoder(
    const webrtc::SdpVideoFormat& aFormat)
    : mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                               "WebrtcMediaDataEncoder::mTaskQueue")),
      mFactory(new PEMFactory()),
      mCallbackMutex("WebrtcMediaDataEncoderCodec encoded callback mutex"),
      mFormatParams(aFormat.parameters),
      // Use the same lower and upper bound as h264_video_toolbox_encoder which
      // is an encoder from webrtc's upstream codebase.
      // 0.5 is set as a mininum to prevent overcompensating for large temporary
      // overshoots. We don't want to degrade video quality too badly.
      // 0.95 is set to prevent oscillations. When a lower bitrate is set on the
      // encoder than previously set, its output seems to have a brief period of
      // drastically reduced bitrate, so we want to avoid that. In steady state
      // conditions, 0.95 seems to give us better overall bitrate over long
      // periods of time.
      mBitrateAdjuster(0.5, 0.95) {
  PodZero(&mCodecSpecific.codecSpecific);
}

static void InitCodecSpecficInfo(webrtc::CodecSpecificInfo& aInfo,
                                 const webrtc::VideoCodec* aCodecSettings) {
  MOZ_ASSERT(aCodecSettings);

  aInfo.codecType = aCodecSettings->codecType;
  switch (aCodecSettings->codecType) {
    case webrtc::VideoCodecType::kVideoCodecH264: {
      aInfo.codecSpecific.H264.packetization_mode =
          aCodecSettings->H264().packetizationMode == 1
              ? webrtc::H264PacketizationMode::NonInterleaved
              : webrtc::H264PacketizationMode::SingleNalUnit;
      break;
    }
    case webrtc::VideoCodecType::kVideoCodecVP9: {
      MOZ_ASSERT(aCodecSettings->VP9().numberOfSpatialLayers == 1);
      aInfo.codecSpecific.VP9.flexible_mode =
          aCodecSettings->VP9().flexibleMode;
      break;
    }
    default:
      break;
  }
}

int32_t WebrtcMediaDataEncoder::InitEncode(
    const webrtc::VideoCodec* aCodecSettings,
    const webrtc::VideoEncoder::Settings& aSettings) {
  MOZ_ASSERT(aCodecSettings);
  MOZ_ASSERT(aCodecSettings->numberOfSimulcastStreams == 1,
             "Simulcast not implemented for H264");

  if (mEncoder) {
    // Clean existing encoder.
    Shutdown();
  }

  RefPtr<MediaDataEncoder> encoder = CreateEncoder(aCodecSettings);
  if (!encoder) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  InitCodecSpecficInfo(mCodecSpecific, aCodecSettings);
  LOG("Init encode, mimeType %s, mode %s", mInfo.mMimeType.get(),
      PacketModeStr(mCodecSpecific));
  if (!media::Await(do_AddRef(mTaskQueue), encoder->Init()).IsResolve()) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  mEncoder = std::move(encoder);
  return WEBRTC_VIDEO_CODEC_OK;
}

bool WebrtcMediaDataEncoder::SetupConfig(
    const webrtc::VideoCodec* aCodecSettings) {
  MimeTypeResult mimeType =
      ConvertWebrtcCodecTypeToMimeType(aCodecSettings->codecType);
  if (!mimeType) {
    LOG("Get incorrect mime type");
    return false;
  }
  mInfo = VideoInfo(aCodecSettings->width, aCodecSettings->height);
  mInfo.mMimeType = mimeType.extract();
  mMaxFrameRate = aCodecSettings->maxFramerate;
  // Those bitrates in codec setting are all kbps, so we have to covert them to
  // bps.
  mMaxBitrateBps = aCodecSettings->maxBitrate * 1000;
  mMinBitrateBps = aCodecSettings->minBitrate * 1000;
  mBitrateAdjuster.SetTargetBitrateBps(aCodecSettings->startBitrate * 1000);
  return true;
}

already_AddRefed<MediaDataEncoder> WebrtcMediaDataEncoder::CreateEncoder(
    const webrtc::VideoCodec* aCodecSettings) {
  if (!SetupConfig(aCodecSettings)) {
    return nullptr;
  }
  LOG("Request platform encoder for %s, bitRate=%u bps, frameRate=%u",
      mInfo.mMimeType.get(), mBitrateAdjuster.GetTargetBitrateBps(),
      aCodecSettings->maxFramerate);

  size_t keyframeInterval = 1;
  switch (aCodecSettings->codecType) {
    case webrtc::VideoCodecType::kVideoCodecH264: {
      keyframeInterval = aCodecSettings->H264().keyFrameInterval;
      break;
    }
    case webrtc::VideoCodecType::kVideoCodecVP8: {
      keyframeInterval = aCodecSettings->VP8().keyFrameInterval;
      break;
    }
    case webrtc::VideoCodecType::kVideoCodecVP9: {
      keyframeInterval = aCodecSettings->VP9().keyFrameInterval;
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported codec type");
      return nullptr;
  }
  CreateEncoderParams params(
      mInfo, MediaDataEncoder::Usage::Realtime,
      MakeRefPtr<TaskQueue>(
          GetMediaThreadPool(MediaThreadType::PLATFORM_ENCODER),
          "WebrtcMediaDataEncoder::mEncoder"),
      MediaDataEncoder::PixelFormat::YUV420P, aCodecSettings->maxFramerate,
      keyframeInterval, mBitrateAdjuster.GetTargetBitrateBps());
  switch (aCodecSettings->codecType) {
    case webrtc::VideoCodecType::kVideoCodecH264: {
      params.SetCodecSpecific(
          MediaDataEncoder::H264Specific(ConvertProfileLevel(mFormatParams)));
      break;
    }
    case webrtc::VideoCodecType::kVideoCodecVP8: {
      const webrtc::VideoCodecVP8& vp8 = aCodecSettings->VP8();
      params.SetCodecSpecific(MediaDataEncoder::VPXSpecific::VP8(
          MapComplexity(vp8.complexity), false, vp8.numberOfTemporalLayers,
          vp8.denoisingOn, vp8.automaticResizeOn, vp8.frameDroppingOn));
      break;
    }
    case webrtc::VideoCodecType::kVideoCodecVP9: {
      const webrtc::VideoCodecVP9& vp9 = aCodecSettings->VP9();
      params.SetCodecSpecific(MediaDataEncoder::VPXSpecific::VP9(
          MapComplexity(vp9.complexity), false, vp9.numberOfTemporalLayers,
          vp9.denoisingOn, vp9.automaticResizeOn, vp9.frameDroppingOn,
          vp9.adaptiveQpMode, vp9.numberOfSpatialLayers, vp9.flexibleMode));
      break;
    }
    default:
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unsupported codec type");
  }
  return mFactory->CreateEncoder(params);
}

int32_t WebrtcMediaDataEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* aCallback) {
  MutexAutoLock lock(mCallbackMutex);
  mCallback = aCallback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataEncoder::Shutdown() {
  LOG("Release encoder");
  {
    MutexAutoLock lock(mCallbackMutex);
    mCallback = nullptr;
    mError = NS_OK;
  }
  if (mEncoder) {
    media::Await(do_AddRef(mTaskQueue), mEncoder->Shutdown());
    mEncoder = nullptr;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

static already_AddRefed<VideoData> CreateVideoDataFromWebrtcVideoFrame(
    const webrtc::VideoFrame& aFrame, const bool aIsKeyFrame,
    const TimeUnit aDuration) {
  MOZ_ASSERT(aFrame.video_frame_buffer()->type() ==
                 webrtc::VideoFrameBuffer::Type::kI420,
             "Only support YUV420!");
  const webrtc::I420BufferInterface* i420 =
      aFrame.video_frame_buffer()->GetI420();

  PlanarYCbCrData yCbCrData;
  yCbCrData.mYChannel = const_cast<uint8_t*>(i420->DataY());
  yCbCrData.mYSize = gfx::IntSize(i420->width(), i420->height());
  yCbCrData.mYStride = i420->StrideY();
  yCbCrData.mCbChannel = const_cast<uint8_t*>(i420->DataU());
  yCbCrData.mCrChannel = const_cast<uint8_t*>(i420->DataV());
  yCbCrData.mCbCrSize = gfx::IntSize(i420->ChromaWidth(), i420->ChromaHeight());
  MOZ_ASSERT(i420->StrideU() == i420->StrideV());
  yCbCrData.mCbCrStride = i420->StrideU();
  yCbCrData.mPicSize = gfx::IntSize(i420->width(), i420->height());

  RefPtr<PlanarYCbCrImage> image =
      new RecyclingPlanarYCbCrImage(new BufferRecycleBin());
  image->CopyData(yCbCrData);

  return VideoData::CreateFromImage(
      image->GetSize(), 0, TimeUnit::FromMicroseconds(aFrame.timestamp_us()),
      aDuration, image, aIsKeyFrame,
      TimeUnit::FromMicroseconds(aFrame.timestamp()));
}

static void UpdateCodecSpecificInfo(webrtc::CodecSpecificInfo& aInfo,
                                    const gfx::IntSize& aSize,
                                    const bool aIsKeyframe) {
  switch (aInfo.codecType) {
    case webrtc::VideoCodecType::kVideoCodecVP8: {
      // See webrtc::VP8EncoderImpl::PopulateCodecSpecific().
      webrtc::CodecSpecificInfoVP8& vp8 = aInfo.codecSpecific.VP8;
      vp8.keyIdx = webrtc::kNoKeyIdx;
      // Cannot be 100% sure unless parsing significant portion of the
      // bitstream. Treat all frames as referenced just to be safe.
      vp8.nonReference = false;
      // One temporal layer only.
      vp8.temporalIdx = webrtc::kNoTemporalIdx;
      vp8.layerSync = false;
      break;
    }
    case webrtc::VideoCodecType::kVideoCodecVP9: {
      // See webrtc::VP9EncoderImpl::PopulateCodecSpecific().
      webrtc::CodecSpecificInfoVP9& vp9 = aInfo.codecSpecific.VP9;
      vp9.inter_pic_predicted = aIsKeyframe;
      vp9.ss_data_available = aIsKeyframe && !vp9.flexible_mode;
      // One temporal & spatial layer only.
      vp9.temporal_idx = webrtc::kNoTemporalIdx;
      vp9.temporal_up_switch = false;
      vp9.num_spatial_layers = 1;
      vp9.gof_idx = webrtc::kNoGofIdx;
      vp9.width[0] = aSize.width;
      vp9.height[0] = aSize.height;
      break;
    }
    default:
      break;
  }
}

static void GetVPXQp(const webrtc::VideoCodecType aType,
                     webrtc::EncodedImage& aImage) {
  switch (aType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      webrtc::vp8::GetQp(aImage.data(), aImage.size(), &(aImage.qp_));
      break;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      webrtc::vp9::GetQp(aImage.data(), aImage.size(), &(aImage.qp_));
      break;
    default:
      break;
  }
}

int32_t WebrtcMediaDataEncoder::Encode(
    const webrtc::VideoFrame& aInputFrame,
    const std::vector<webrtc::VideoFrameType>* aFrameTypes) {
  if (!aInputFrame.size() || !aInputFrame.video_frame_buffer() ||
      aFrameTypes->empty()) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  if (!mEncoder) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  {
    MutexAutoLock lock(mCallbackMutex);
    if (!mCallback) {
      return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (NS_FAILED(mError)) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  LOG_V("Encode frame, type %d size %u", static_cast<int>((*aFrameTypes)[0]),
        aInputFrame.size());
  MOZ_ASSERT(aInputFrame.video_frame_buffer()->type() ==
             webrtc::VideoFrameBuffer::Type::kI420);
  RefPtr<VideoData> data = CreateVideoDataFromWebrtcVideoFrame(
      aInputFrame, (*aFrameTypes)[0] == webrtc::VideoFrameType::kVideoFrameKey,
      TimeUnit::FromSeconds(1.0 / mMaxFrameRate));
  const gfx::IntSize displaySize = data->mDisplay;

  mEncoder->Encode(data)->Then(
      mTaskQueue, __func__,
      [self = RefPtr<WebrtcMediaDataEncoder>(this), this,
       displaySize](MediaDataEncoder::EncodedData aFrames) {
        LOG_V("Received encoded frame, nums %zu width %d height %d",
              aFrames.Length(), displaySize.width, displaySize.height);
        for (auto& frame : aFrames) {
          MutexAutoLock lock(mCallbackMutex);
          if (!mCallback) {
            break;
          }
          webrtc::EncodedImage image(const_cast<uint8_t*>(frame->Data()),
                                     frame->Size(), frame->Size());
          image._encodedWidth = displaySize.width;
          image._encodedHeight = displaySize.height;
          CheckedInt64 time =
              TimeUnitToFrames(frame->mTime, cricket::kVideoCodecClockrate);
          if (!time.isValid()) {
            self->mError = MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                       "invalid timestamp from encoder");
            break;
          }
          image.SetTimestamp(time.value());
          image._frameType = frame->mKeyframe
                                 ? webrtc::VideoFrameType::kVideoFrameKey
                                 : webrtc::VideoFrameType::kVideoFrameDelta;
          image._completeFrame = true;
          GetVPXQp(mCodecSpecific.codecType, image);
          UpdateCodecSpecificInfo(mCodecSpecific, displaySize,
                                  frame->mKeyframe);

          LOG_V("Send encoded image");
          self->mCallback->OnEncodedImage(image, &mCodecSpecific);
          self->mBitrateAdjuster.Update(image.size());
        }
      },
      [self = RefPtr<WebrtcMediaDataEncoder>(this)](const MediaResult aError) {
        self->mError = aError;
      });
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataEncoder::SetRates(
    const webrtc::VideoEncoder::RateControlParameters& aParameters) {
  MOZ_ASSERT(aParameters.bitrate.IsSpatialLayerUsed(0));
  MOZ_ASSERT(!aParameters.bitrate.HasBitrate(0, 1),
             "No simulcast support for H264");
  MOZ_ASSERT(!aParameters.bitrate.IsSpatialLayerUsed(1),
             "No simulcast support for H264");

  const uint32_t newBitrateBps = aParameters.bitrate.GetBitrate(0, 0);
  if (newBitrateBps < mMinBitrateBps || newBitrateBps > mMaxBitrateBps) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // We have already been in this bitrate.
  if (mBitrateAdjuster.GetAdjustedBitrateBps() == newBitrateBps) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  if (!mEncoder) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  {
    MutexAutoLock lock(mCallbackMutex);
    if (NS_FAILED(mError)) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  mBitrateAdjuster.SetTargetBitrateBps(newBitrateBps);
  LOG("Set bitrate %u bps, minBitrate %u bps, maxBitrate %u bps", newBitrateBps,
      mMinBitrateBps, mMaxBitrateBps);
  auto rv =
      media::Await(do_AddRef(mTaskQueue), mEncoder->SetBitrate(newBitrateBps));
  return rv.IsResolve() ? WEBRTC_VIDEO_CODEC_OK : WEBRTC_VIDEO_CODEC_ERROR;
}

}  // namespace mozilla
