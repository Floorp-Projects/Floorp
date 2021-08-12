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
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/system_wrappers/include/clock.h"

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

static const char* PacketModeStr(const webrtc::CodecSpecificInfo& aInfo) {
  MOZ_ASSERT(aInfo.codecType != webrtc::VideoCodecType::kVideoCodecUnknown);

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
      break;
  }
  return Nothing();
}

static MediaDataEncoder::H264Specific::ProfileLevel ConvertProfileLevel(
    webrtc::H264::Profile aProfile) {
  if (aProfile == webrtc::H264::kProfileConstrainedBaseline ||
      aProfile == webrtc::H264::kProfileBaseline) {
    return MediaDataEncoder::H264Specific::ProfileLevel::BaselineAutoLevel;
  }
  return MediaDataEncoder::H264Specific::ProfileLevel::MainAutoLevel;
}

static MediaDataEncoder::H264Specific GetCodecSpecific(
    const webrtc::VideoCodec* aCodecSettings) {
  return MediaDataEncoder::H264Specific(
      aCodecSettings->H264().keyFrameInterval,
      ConvertProfileLevel(aCodecSettings->H264().profile));
}

WebrtcMediaDataEncoder::WebrtcMediaDataEncoder()
    : mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                               "WebrtcMediaDataEncoder::mTaskQueue")),
      mFactory(new PEMFactory()),
      mCallbackMutex("WebrtcMediaDataEncoderCodec encoded callback mutex"),
      // Use the same lower and upper bound as h264_video_toolbox_encoder which
      // is an encoder from webrtc's upstream codebase.
      // 0.5 is set as a mininum to prevent overcompensating for large temporary
      // overshoots. We don't want to degrade video quality too badly.
      // 0.95 is set to prevent oscillations. When a lower bitrate is set on the
      // encoder than previously set, its output seems to have a brief period of
      // drastically reduced bitrate, so we want to avoid that. In steady state
      // conditions, 0.95 seems to give us better overall bitrate over long
      // periods of time.
      mBitrateAdjuster(webrtc::Clock::GetRealTimeClock(), 0.5, 0.95) {}

int32_t WebrtcMediaDataEncoder::InitEncode(
    const webrtc::VideoCodec* aCodecSettings, int32_t aNumberOfCores,
    size_t aMaxPayloadSize) {
  if (mEncoder) {
    // Clean existing encoder.
    Shutdown();
  }

  RefPtr<MediaDataEncoder> encoder = CreateEncoder(aCodecSettings);
  if (!encoder) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  LOG("Init encode, mimeType %s, mode %s", mInfo.mMimeType.get(),
      PacketModeStr(mCodecSpecific));
  if (!media::Await(do_AddRef(mTaskQueue), encoder->Init()).IsResolve()) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  mEncoder = std::move(encoder);
  return WEBRTC_VIDEO_CODEC_OK;
}

static void FillCodecSpecficInfo(const webrtc::VideoCodec* aCodecSettings,
                                 webrtc::CodecSpecificInfo& aInfo) {
  MOZ_ASSERT(aCodecSettings);

  aInfo.codecType = aCodecSettings->codecType;
  if (aCodecSettings->codecType == webrtc::VideoCodecType::kVideoCodecH264) {
    aInfo.codecSpecific.H264.packetization_mode =
        aCodecSettings->H264().packetizationMode == 1
            ? webrtc::H264PacketizationMode::NonInterleaved
            : webrtc::H264PacketizationMode::SingleNalUnit;
  }
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
  FillCodecSpecficInfo(aCodecSettings, mCodecSpecific);
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
  return mFactory->CreateEncoder(CreateEncoderParams(
      mInfo, MediaDataEncoder::Usage::Realtime,
      MakeRefPtr<TaskQueue>(
          GetMediaThreadPool(MediaThreadType::PLATFORM_ENCODER),
          "WebrtcMediaDataEncoder::mEncoder"),
      MediaDataEncoder::PixelFormat::YUV420P, aCodecSettings->maxFramerate,
      mBitrateAdjuster.GetTargetBitrateBps(),
      GetCodecSpecific(aCodecSettings)));
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
  rtc::scoped_refptr<webrtc::I420BufferInterface> i420 =
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

int32_t WebrtcMediaDataEncoder::Encode(
    const webrtc::VideoFrame& aInputFrame,
    const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
    const std::vector<webrtc::FrameType>* aFrameTypes) {
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

  LOG_V("Encode frame, type %d size %u", (*aFrameTypes)[0], aInputFrame.size());
  MOZ_ASSERT(aInputFrame.video_frame_buffer()->type() ==
             webrtc::VideoFrameBuffer::Type::kI420);
  RefPtr<VideoData> data = CreateVideoDataFromWebrtcVideoFrame(
      aInputFrame, (*aFrameTypes)[0] == webrtc::FrameType::kVideoFrameKey,
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
          image._timeStamp = time.value();
          image._frameType = frame->mKeyframe
                                 ? webrtc::FrameType::kVideoFrameKey
                                 : webrtc::FrameType::kVideoFrameDelta;
          image._completeFrame = true;

          nsTArray<AnnexB::NALEntry> entries;
          AnnexB::ParseNALEntries(
              Span<const uint8_t>(frame->Data(), frame->Size()), entries);
          const size_t nalNums = entries.Length();
          LOG_V("NAL nums %zu", nalNums);
          MOZ_ASSERT(nalNums, "Should have at least 1 NALU in encoded frame!");

          webrtc::RTPFragmentationHeader header;
          header.VerifyAndAllocateFragmentationHeader(nalNums);
          for (size_t idx = 0; idx < nalNums; idx++) {
            header.fragmentationOffset[idx] = entries[idx].mOffset;
            header.fragmentationLength[idx] = entries[idx].mSize;
            LOG_V("NAL offset %" PRId64 " size %" PRId64, entries[idx].mOffset,
                  entries[idx].mSize);
          }

          LOG_V("Send encoded image");
          self->mCallback->OnEncodedImage(image, &mCodecSpecific, &header);
          self->mBitrateAdjuster.Update(image._size);
        }
      },
      [self = RefPtr<WebrtcMediaDataEncoder>(this)](const MediaResult aError) {
        self->mError = aError;
      });
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataEncoder::SetChannelParameters(uint32_t aPacketLoss,
                                                     int64_t aRtt) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataEncoder::SetRates(uint32_t aNewBitrateKbps,
                                         uint32_t aFrameRate) {
  if (!aFrameRate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  const uint32_t newBitrateBps = aNewBitrateKbps * 1000;
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
