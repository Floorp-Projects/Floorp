/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoEncoder.h"

#include <aom/aomcx.h>

#include "BufferReader.h"
#include "FFmpegLog.h"
#include "FFmpegUtils.h"
#include "H264.h"
#include "ImageContainer.h"
#include "libavutil/error.h"
#include "libavutil/pixfmt.h"
#include "mozilla/dom/ImageUtils.h"
#include "nsPrintfCString.h"
#include "ImageConversion.h"
#include "libyuv.h"
#include "FFmpegRuntimeLinker.h"

// The ffmpeg namespace is introduced to avoid the PixelFormat's name conflicts
// with MediaDataEncoder::PixelFormat in MediaDataEncoder class scope.
namespace ffmpeg {

// TODO: WebCodecs' I420A should map to MediaDataEncoder::PixelFormat and then
// to AV_PIX_FMT_YUVA420P here.
#if LIBAVCODEC_VERSION_MAJOR < 54
using FFmpegPixelFormat = enum PixelFormat;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NONE = FFmpegPixelFormat::PIX_FMT_NONE;
const FFmpegPixelFormat FFMPEG_PIX_FMT_RGBA = FFmpegPixelFormat::PIX_FMT_RGBA;
const FFmpegPixelFormat FFMPEG_PIX_FMT_BGRA = FFmpegPixelFormat::PIX_FMT_BGRA;
const FFmpegPixelFormat FFMPEG_PIX_FMT_RGB24 = FFmpegPixelFormat::PIX_FMT_RGB24;
const FFmpegPixelFormat FFMPEG_PIX_FMT_BGR24 = FFmpegPixelFormat::PIX_FMT_BGR24;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV444P =
    FFmpegPixelFormat::PIX_FMT_YUV444P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV422P =
    FFmpegPixelFormat::PIX_FMT_YUV422P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV420P =
    FFmpegPixelFormat::PIX_FMT_YUV420P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NV12 = FFmpegPixelFormat::PIX_FMT_NV12;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NV21 = FFmpegPixelFormat::PIX_FMT_NV21;
#else
using FFmpegPixelFormat = enum AVPixelFormat;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NONE =
    FFmpegPixelFormat::AV_PIX_FMT_NONE;
const FFmpegPixelFormat FFMPEG_PIX_FMT_RGBA =
    FFmpegPixelFormat::AV_PIX_FMT_RGBA;
const FFmpegPixelFormat FFMPEG_PIX_FMT_BGRA =
    FFmpegPixelFormat::AV_PIX_FMT_BGRA;
const FFmpegPixelFormat FFMPEG_PIX_FMT_RGB24 =
    FFmpegPixelFormat::AV_PIX_FMT_RGB24;
const FFmpegPixelFormat FFMPEG_PIX_FMT_BGR24 =
    FFmpegPixelFormat::AV_PIX_FMT_BGR24;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV444P =
    FFmpegPixelFormat::AV_PIX_FMT_YUV444P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV422P =
    FFmpegPixelFormat::AV_PIX_FMT_YUV422P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV420P =
    FFmpegPixelFormat::AV_PIX_FMT_YUV420P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NV12 =
    FFmpegPixelFormat::AV_PIX_FMT_NV12;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NV21 =
    FFmpegPixelFormat::AV_PIX_FMT_NV21;
#endif

static const char* GetPixelFormatString(FFmpegPixelFormat aFormat) {
  switch (aFormat) {
    case FFMPEG_PIX_FMT_NONE:
      return "none";
    case FFMPEG_PIX_FMT_RGBA:
      return "packed RGBA 8:8:8:8 (32bpp, RGBARGBA...)";
    case FFMPEG_PIX_FMT_BGRA:
      return "packed BGRA 8:8:8:8 (32bpp, BGRABGRA...)";
    case FFMPEG_PIX_FMT_RGB24:
      return "packed RGB 8:8:8 (24bpp, RGBRGB...)";
    case FFMPEG_PIX_FMT_BGR24:
      return "packed RGB 8:8:8 (24bpp, BGRBGR...)";
    case FFMPEG_PIX_FMT_YUV444P:
      return "planar YUV 4:4:4 (24bpp, 1 Cr & Cb sample per 1x1 Y samples)";
    case FFMPEG_PIX_FMT_YUV422P:
      return "planar YUV 4:2:2 (16bpp, 1 Cr & Cb sample per 2x1 Y samples)";
    case FFMPEG_PIX_FMT_YUV420P:
      return "planar YUV 4:2:0 (12bpp, 1 Cr & Cb sample per 2x2 Y samples)";
    case FFMPEG_PIX_FMT_NV12:
      return "planar YUV 4:2:0 (12bpp, 1 interleaved UV components per 1x1 Y "
             "samples)";
    case FFMPEG_PIX_FMT_NV21:
      return "planar YUV 4:2:0 (12bpp, 1 interleaved VU components per 1x1 Y "
             "samples)";
    default:
      break;
  }
  MOZ_ASSERT_UNREACHABLE("Unsupported pixel format");
  return "unsupported";
}

};  // namespace ffmpeg

namespace mozilla {

struct H264Setting {
  int mValue;
  nsCString mString;
};

static const H264Setting H264Profiles[]{
    {FF_PROFILE_H264_BASELINE, "baseline"_ns},
    {FF_PROFILE_H264_MAIN, "main"_ns},
    {FF_PROFILE_H264_EXTENDED, EmptyCString()},
    {FF_PROFILE_H264_HIGH, "high"_ns}};

static Maybe<H264Setting> GetH264Profile(const H264_PROFILE& aProfile) {
  switch (aProfile) {
    case H264_PROFILE::H264_PROFILE_UNKNOWN:
      return Nothing();
    case H264_PROFILE::H264_PROFILE_BASE:
      return Some(H264Profiles[0]);
    case H264_PROFILE::H264_PROFILE_MAIN:
      return Some(H264Profiles[1]);
    case H264_PROFILE::H264_PROFILE_EXTENDED:
      return Some(H264Profiles[2]);
    case H264_PROFILE::H264_PROFILE_HIGH:
      return Some(H264Profiles[3]);
    default:
      break;
  }
  MOZ_ASSERT_UNREACHABLE("undefined profile");
  return Nothing();
}

static Maybe<H264Setting> GetH264Level(const H264_LEVEL& aLevel) {
  int val = static_cast<int>(aLevel);
  // Make sure value is in [10, 13] or [20, 22] or [30, 32] or [40, 42] or [50,
  // 52].
  if (val < 10 || val > 52) {
    return Nothing();
  }
  if ((val % 10) > 2) {
    if (val != 13) {
      return Nothing();
    }
  }
  nsPrintfCString str("%d", val);
  str.Insert('.', 1);
  return Some(H264Setting{val, str});
}

struct VPXSVCAppendix {
  uint8_t mLayeringMode;
};

struct SVCLayerSettings {
  using CodecAppendix = Variant<VPXSVCAppendix, aom_svc_params_t>;
  size_t mNumberSpatialLayers;
  size_t mNumberTemporalLayers;
  uint8_t mPeriodicity;
  nsTArray<uint8_t> mLayerIds;
  // libvpx: ts_rate_decimator, libaom: framerate_factor
  nsTArray<uint8_t> mRateDecimators;
  nsTArray<uint32_t> mTargetBitrates;
  Maybe<CodecAppendix> mCodecAppendix;
};

static Maybe<SVCLayerSettings> GetSVCLayerSettings(CodecType aCodec,
                                                   const ScalabilityMode& aMode,
                                                   uint32_t aBitPerSec) {
  if (aMode == ScalabilityMode::None) {
    return Nothing();
  }

  // TODO: Apply more sophisticated bitrate allocation, like SvcRateAllocator:
  // https://searchfox.org/mozilla-central/rev/3bd65516eb9b3a9568806d846ba8c81a9402a885/third_party/libwebrtc/modules/video_coding/svc/svc_rate_allocator.h#26

  size_t layers = 0;
  const uint32_t kbps = aBitPerSec / 1000;  // ts_target_bitrate requies kbps.

  uint8_t periodicity;
  nsTArray<uint8_t> layerIds;
  nsTArray<uint8_t> rateDecimators;
  nsTArray<uint32_t> bitrates;

  Maybe<SVCLayerSettings::CodecAppendix> appendix;

  if (aMode == ScalabilityMode::L1T2) {
    // Two temporal layers. 0-1...
    //
    // Frame pattern:
    // Layer 0: |0| |2| |4| |6| |8|
    // Layer 1: | |1| |3| |5| |7| |

    layers = 2;

    // 2 frames per period.
    periodicity = 2;

    // Assign layer ids.
    layerIds.AppendElement(0);
    layerIds.AppendElement(1);

    // Set rate decimators.
    rateDecimators.AppendElement(2);
    rateDecimators.AppendElement(1);

    // Bitrate allocation: L0 - 60%, L1 - 40%.
    bitrates.AppendElement(kbps * 3 / 5);
    bitrates.AppendElement(kbps);

    if (aCodec == CodecType::VP8 || aCodec == CodecType::VP9) {
      appendix.emplace(VPXSVCAppendix{
          .mLayeringMode = 2 /* VP9E_TEMPORAL_LAYERING_MODE_0101 */
      });
    }
  } else {
    MOZ_ASSERT(aMode == ScalabilityMode::L1T3);
    // Three temporal layers. 0-2-1-2...
    //
    // Frame pattern:
    // Layer 0: |0| | | |4| | | |8| |  |  |12|
    // Layer 1: | | |2| | | |6| | | |10|  |  |
    // Layer 2: | |1| |3| |5| |7| |9|  |11|  |

    layers = 3;

    // 4 frames per period
    periodicity = 4;

    // Assign layer ids.
    layerIds.AppendElement(0);
    layerIds.AppendElement(2);
    layerIds.AppendElement(1);
    layerIds.AppendElement(2);

    // Set rate decimators.
    rateDecimators.AppendElement(4);
    rateDecimators.AppendElement(2);
    rateDecimators.AppendElement(1);

    // Bitrate allocation: L0 - 50%, L1 - 20%, L2 - 30%.
    bitrates.AppendElement(kbps / 2);
    bitrates.AppendElement(kbps * 7 / 10);
    bitrates.AppendElement(kbps);

    if (aCodec == CodecType::VP8 || aCodec == CodecType::VP9) {
      appendix.emplace(VPXSVCAppendix{
          .mLayeringMode = 3 /* VP9E_TEMPORAL_LAYERING_MODE_0212 */
      });
    }
  }

  MOZ_ASSERT(layers == bitrates.Length(),
             "Bitrate must be assigned to each layer");
  return Some(SVCLayerSettings{1, layers, periodicity, std::move(layerIds),
                               std::move(rateDecimators), std::move(bitrates),
                               appendix});
}

uint8_t FFmpegVideoEncoder<LIBAV_VER>::SVCInfo::UpdateTemporalLayerId() {
  MOZ_ASSERT(!mTemporalLayerIds.IsEmpty());

  size_t currentIndex = mNextIndex % mTemporalLayerIds.Length();
  mNextIndex += 1;
  return static_cast<uint8_t>(mTemporalLayerIds[currentIndex]);
}

FFmpegVideoEncoder<LIBAV_VER>::FFmpegVideoEncoder(
    const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
    const RefPtr<TaskQueue>& aTaskQueue, const EncoderConfig& aConfig)
    : FFmpegDataEncoder(aLib, aCodecID, aTaskQueue, aConfig) {}

nsCString FFmpegVideoEncoder<LIBAV_VER>::GetDescriptionName() const {
#ifdef USING_MOZFFVPX
  return "ffvpx video encoder"_ns;
#else
  const char* lib =
#  if defined(MOZ_FFMPEG)
      FFmpegRuntimeLinker::LinkStatusLibraryName();
#  else
      "no library: ffmpeg disabled during build";
#  endif
  return nsPrintfCString("ffmpeg video encoder (%s)", lib);
#endif
}

nsresult FFmpegVideoEncoder<LIBAV_VER>::InitSpecific() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("FFmpegVideoEncoder::InitSpecific");

  // Initialize the common members of the encoder instance
  AVCodec* codec = FFmpegDataEncoder<LIBAV_VER>::InitCommon();
  if (!codec) {
    FFMPEGV_LOG("FFmpegDataEncoder::InitCommon failed");
    return NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR;
  }

  // And now the video-specific part
  mCodecContext->pix_fmt = ffmpeg::FFMPEG_PIX_FMT_YUV420P;
  mCodecContext->width = static_cast<int>(mConfig.mSize.width);
  mCodecContext->height = static_cast<int>(mConfig.mSize.height);
  mCodecContext->gop_size = static_cast<int>(mConfig.mKeyframeInterval);
  // TODO(bug 1869560): The recommended time_base is the reciprocal of the frame
  // rate, but we set it to microsecond for now.
  mCodecContext->time_base =
      AVRational{.num = 1, .den = static_cast<int>(USECS_PER_S)};
#if LIBAVCODEC_VERSION_MAJOR >= 57
  // Note that sometimes framerate can be zero (from webcodecs).
  mCodecContext->framerate =
      AVRational{.num = static_cast<int>(mConfig.mFramerate), .den = 1};
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 60
  mCodecContext->flags |= AV_CODEC_FLAG_FRAME_DURATION;
#endif
  mCodecContext->gop_size = static_cast<int>(mConfig.mKeyframeInterval);

  if (mConfig.mUsage == Usage::Realtime) {
    mLib->av_opt_set(mCodecContext->priv_data, "deadline", "realtime", 0);
    // Explicitly ask encoder do not keep in flight at any one time for
    // lookahead purposes.
    mLib->av_opt_set(mCodecContext->priv_data, "lag-in-frames", "0", 0);
  }

  if (Maybe<SVCSettings> settings = GetSVCSettings()) {
    if (mCodecName == "libaom-av1") {
      if (mConfig.mBitrateMode != BitrateMode::Constant) {
        return NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR;
      }
    }

    SVCSettings s = settings.extract();
    mLib->av_opt_set(mCodecContext->priv_data, s.mSettingKeyValue.first.get(),
                     s.mSettingKeyValue.second.get(), 0);

    // FFmpegVideoEncoder is reset after Drain(), so mSVCInfo should be reset()
    // before emplace().
    mSVCInfo.reset();
    mSVCInfo.emplace(std::move(s.mTemporalLayerIds));

    // TODO: layer settings should be changed dynamically when the frame's
    // color space changed.
  }

  nsAutoCString h264Log;
  if (mConfig.mCodecSpecific && mConfig.mCodecSpecific->is<H264Specific>()) {
    // TODO: Set profile, level, avcc/annexb for openh264 and others.
    if (mCodecName == "libx264") {
      const H264Specific& h264Specific =
          mConfig.mCodecSpecific->as<H264Specific>();
      H264Settings s = GetH264Settings(h264Specific);
      mCodecContext->profile = s.mProfile;
      mCodecContext->level = s.mLevel;
      for (const auto& pair : s.mSettingKeyValuePairs) {
        mLib->av_opt_set(mCodecContext->priv_data, pair.first.get(),
                         pair.second.get(), 0);
      }

      // Log the settings.
      // When using profile other than EXTENDED, the profile string is in the
      // first element of mSettingKeyValuePairs, while EXTENDED profile has no
      // profile string.

      MOZ_ASSERT_IF(
          s.mSettingKeyValuePairs.Length() != 3,
          h264Specific.mProfile == H264_PROFILE::H264_PROFILE_EXTENDED);
      const char* profileStr = s.mSettingKeyValuePairs.Length() == 3
                                   ? s.mSettingKeyValuePairs[0].second.get()
                                   : "extended";
      const char* levelStr = s.mSettingKeyValuePairs.Length() == 3
                                 ? s.mSettingKeyValuePairs[1].second.get()
                                 : s.mSettingKeyValuePairs[0].second.get();
      const char* formatStr =
          h264Specific.mFormat == H264BitStreamFormat::AVC ? "AVCC" : "AnnexB";
      h264Log.AppendPrintf(", H264: profile - %d (%s), level %d (%s), %s",
                           mCodecContext->profile, profileStr,
                           mCodecContext->level, levelStr, formatStr);
    }
  }

  // TODO: keyint_min, max_b_frame?
  // - if mConfig.mDenoising is set: av_opt_set_int(mCodecContext->priv_data,
  // "noise_sensitivity", x, 0), where the x is from 0(disabled) to 6.
  // - if mConfig.mAdaptiveQp is set: av_opt_set_int(mCodecContext->priv_data,
  // "aq_mode", x, 0), where x is from 0 to 3: 0 - Disabled, 1 - Variance
  // AQ(default), 2 - Complexity AQ, 3 - Cycle AQ.
  // - if min and max rates are known (VBR?),
  // av_opt_set(mCodecContext->priv_data, "minrate", x, 0) and
  // av_opt_set(mCodecContext->priv_data, "maxrate", y, 0)
  // TODO: AV1 specific settings.

  // Our old version of libaom-av1 is considered experimental by the recent
  // ffmpeg we use. Allow experimental codecs for now until we decide on an AV1
  // encoder.
  mCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

  MediaResult rv = FinishInitCommon(codec);
  if (NS_FAILED(rv)) {
    FFMPEGV_LOG("FFmpeg video encoder initialization failure.");
    return rv;
  }

  FFMPEGV_LOG("%s has been initialized with format: %s, bitrate: %" PRIi64
              ", width: %d, height: %d, time_base: %d/%d%s",
              codec->name, ffmpeg::GetPixelFormatString(mCodecContext->pix_fmt),
              static_cast<int64_t>(mCodecContext->bit_rate),
              mCodecContext->width, mCodecContext->height,
              mCodecContext->time_base.num, mCodecContext->time_base.den,
              h264Log.IsEmpty() ? "" : h264Log.get());

  return NS_OK;
}

bool FFmpegVideoEncoder<LIBAV_VER>::ScaleInputFrame() {
  AVFrame* source = mFrame;
  mFrame = nullptr;
  // Allocate AVFrame.
  if (!PrepareFrame()) {
    FFMPEGV_LOG("failed to allocate frame");
    return false;
  }

  // Set AVFrame properties for its internal data allocation. For now, we always
  // convert into ffmpeg's buffer.
  mFrame->format = ffmpeg::FFMPEG_PIX_FMT_YUV420P;
  mFrame->width = static_cast<int>(mConfig.mSize.Width());
  mFrame->height = static_cast<int>(mConfig.mSize.Height());

  // Allocate AVFrame data.
  if (int ret = mLib->av_frame_get_buffer(mFrame, 16); ret < 0) {
    FFMPEGV_LOG("failed to allocate frame data: %s",
                MakeErrorString(mLib, ret).get());
    return false;
  }

  // Make sure AVFrame is writable.
  if (int ret = mLib->av_frame_make_writable(mFrame); ret < 0) {
    FFMPEGV_LOG("failed to make frame writable: %s",
                MakeErrorString(mLib, ret).get());
    return false;
  }
  int rv = I420Scale(source->data[0], source->linesize[0], source->data[1],
                     source->linesize[1], source->data[2], source->linesize[2],
                     source->width, source->height, mFrame->data[0],
                     mFrame->linesize[0], mFrame->data[1], mFrame->linesize[1],
                     mFrame->data[2], mFrame->linesize[2], mFrame->width,
                     mFrame->height, libyuv::FilterMode::kFilterBox);
  if (!rv) {
    FFMPEGV_LOG("YUV scale error");
  }
  mLib->av_frame_unref(source);
  mLib->av_frame_free(&source);
  return true;
}

// avcodec_send_frame and avcodec_receive_packet were introduced in version 58.
#if LIBAVCODEC_VERSION_MAJOR >= 58
Result<MediaDataEncoder::EncodedData, nsresult> FFmpegVideoEncoder<
    LIBAV_VER>::EncodeInputWithModernAPIs(RefPtr<const MediaData> aSample) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(mCodecContext);
  MOZ_ASSERT(aSample);

  RefPtr<const VideoData> sample(aSample->As<VideoData>());

  // Validate input.
  if (!sample->mImage) {
    FFMPEGV_LOG("No image");
    return Result<MediaDataEncoder::EncodedData, nsresult>(
        NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }
  if (sample->mImage->GetSize().IsEmpty()) {
    FFMPEGV_LOG("image width or height is invalid");
    return Result<MediaDataEncoder::EncodedData, nsresult>(
        NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }

  // Allocate AVFrame.
  if (!PrepareFrame()) {
    FFMPEGV_LOG("failed to allocate frame");
    return Result<MediaDataEncoder::EncodedData, nsresult>(
        NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }

  // Set AVFrame properties for its internal data allocation. For now, we always
  // convert into ffmpeg's buffer.
  mFrame->format = ffmpeg::FFMPEG_PIX_FMT_YUV420P;
  mFrame->width = static_cast<int>(sample->mImage->GetSize().width);
  mFrame->height = static_cast<int>(sample->mImage->GetSize().height);

  // Allocate AVFrame data.
  if (int ret = mLib->av_frame_get_buffer(mFrame, 0); ret < 0) {
    FFMPEGV_LOG("failed to allocate frame data: %s",
                MakeErrorString(mLib, ret).get());
    return Result<MediaDataEncoder::EncodedData, nsresult>(
        NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }

  // Make sure AVFrame is writable.
  if (int ret = mLib->av_frame_make_writable(mFrame); ret < 0) {
    FFMPEGV_LOG("failed to make frame writable: %s",
                MakeErrorString(mLib, ret).get());
    return Result<MediaDataEncoder::EncodedData, nsresult>(
        NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }

  nsresult rv = ConvertToI420(
      sample->mImage, mFrame->data[0], mFrame->linesize[0], mFrame->data[1],
      mFrame->linesize[1], mFrame->data[2], mFrame->linesize[2]);
  if (NS_FAILED(rv)) {
    FFMPEGV_LOG("Conversion error!");
    return Result<MediaDataEncoder::EncodedData, nsresult>(
        NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }

  // Scale the YUV input frame if needed -- the encoded frame will have the
  // dimensions configured at encoded initialization.
  if (mFrame->width != mConfig.mSize.Width() ||
      mFrame->height != mConfig.mSize.Height()) {
    if (!ScaleInputFrame()) {
      return Result<MediaDataEncoder::EncodedData, nsresult>(
          NS_ERROR_DOM_MEDIA_FATAL_ERR);
    }
  }

  // Set presentation timestamp and duration of the AVFrame. The unit of pts is
  // time_base.
  // TODO(bug 1869560): The recommended time_base is the reciprocal of the frame
  // rate, but we set it to microsecond for now.
#  if LIBAVCODEC_VERSION_MAJOR >= 59
  mFrame->time_base =
      AVRational{.num = 1, .den = static_cast<int>(USECS_PER_S)};
#  endif
  mFrame->pts = aSample->mTime.ToMicroseconds();
#  if LIBAVCODEC_VERSION_MAJOR >= 60
  mFrame->duration = aSample->mDuration.ToMicroseconds();
#  else
  // Save duration in the time_base unit.
  mDurationMap.Insert(mFrame->pts, aSample->mDuration.ToMicroseconds());
#  endif
  Duration(mFrame) = aSample->mDuration.ToMicroseconds();

  // Now send the AVFrame to ffmpeg for encoding, same code for audio and video.
  return FFmpegDataEncoder<LIBAV_VER>::EncodeWithModernAPIs();
}
#endif  // if LIBAVCODEC_VERSION_MAJOR >= 58

RefPtr<MediaRawData> FFmpegVideoEncoder<LIBAV_VER>::ToMediaRawData(
    AVPacket* aPacket) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(aPacket);

  RefPtr<MediaRawData> data = ToMediaRawDataCommon(aPacket);

  // TODO: Is it possible to retrieve temporal layer id from underlying codec
  // instead?
  if (mSVCInfo) {
    uint8_t temporalLayerId = mSVCInfo->UpdateTemporalLayerId();
    data->mTemporalLayerId.emplace(temporalLayerId);
  }

  return data;
}

Result<already_AddRefed<MediaByteBuffer>, nsresult>
FFmpegVideoEncoder<LIBAV_VER>::GetExtraData(AVPacket* aPacket) {
  MOZ_ASSERT(aPacket);

  // H264 Extra data comes with the key frame and we only extract it when
  // encoding into AVCC format.
  if (mCodecID != AV_CODEC_ID_H264 || !mConfig.mCodecSpecific ||
      !mConfig.mCodecSpecific->is<H264Specific>() ||
      mConfig.mCodecSpecific->as<H264Specific>().mFormat !=
          H264BitStreamFormat::AVC ||
      !(aPacket->flags & AV_PKT_FLAG_KEY)) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  if (mCodecName != "libx264") {
    FFMPEGV_LOG("Get extra data from codec %s has not been implemented yet",
                mCodecName.get());
    return Err(NS_ERROR_NOT_IMPLEMENTED);
  }

  bool useGlobalHeader =
#if LIBAVCODEC_VERSION_MAJOR >= 57
      mCodecContext->flags & AV_CODEC_FLAG_GLOBAL_HEADER;
#else
      false;
#endif

  Span<const uint8_t> buf;
  if (useGlobalHeader) {
    buf =
        Span<const uint8_t>(mCodecContext->extradata,
                            static_cast<size_t>(mCodecContext->extradata_size));
  } else {
    buf =
        Span<const uint8_t>(aPacket->data, static_cast<size_t>(aPacket->size));
  }
  if (buf.empty()) {
    FFMPEGV_LOG("fail to get H264 AVCC header in key frame!");
    return Err(NS_ERROR_UNEXPECTED);
  }

  BufferReader reader(buf);

  // The first part is sps.
  uint32_t spsSize;
  MOZ_TRY_VAR(spsSize, reader.ReadU32());
  Span<const uint8_t> spsData;
  MOZ_TRY_VAR(spsData,
              reader.ReadSpan<const uint8_t>(static_cast<size_t>(spsSize)));

  // The second part is pps.
  uint32_t ppsSize;
  MOZ_TRY_VAR(ppsSize, reader.ReadU32());
  Span<const uint8_t> ppsData;
  MOZ_TRY_VAR(ppsData,
              reader.ReadSpan<const uint8_t>(static_cast<size_t>(ppsSize)));

  // Ensure we have profile, constraints and level needed to create the extra
  // data.
  if (spsData.Length() < 4) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  FFMPEGV_LOG(
      "Generate extra data: profile - %u, constraints: %u, level: %u for pts @ "
      "%" PRId64,
      spsData[1], spsData[2], spsData[3], aPacket->pts);

  // Create extra data.
  auto extraData = MakeRefPtr<MediaByteBuffer>();
  H264::WriteExtraData(extraData, spsData[1], spsData[2], spsData[3], spsData,
                       ppsData);
  MOZ_ASSERT(extraData);
  return extraData.forget();
}

void FFmpegVideoEncoder<LIBAV_VER>::ForceEnablingFFmpegDebugLogs() {
#if DEBUG
  if (!getenv("MOZ_AV_LOG_LEVEL") &&
      MOZ_LOG_TEST(sFFmpegVideoLog, LogLevel::Debug)) {
    mLib->av_log_set_level(AV_LOG_DEBUG);
  }
#endif  // DEBUG
}

Maybe<FFmpegVideoEncoder<LIBAV_VER>::SVCSettings>
FFmpegVideoEncoder<LIBAV_VER>::GetSVCSettings() {
  MOZ_ASSERT(!mCodecName.IsEmpty());

  CodecType codecType = CodecType::Unknown;
  if (mCodecName == "libvpx") {
    codecType = CodecType::VP8;
  } else if (mCodecName == "libvpx-vp9") {
    codecType = CodecType::VP9;
  } else if (mCodecName == "libaom-av1") {
    codecType = CodecType::AV1;
  }

  if (codecType == CodecType::Unknown) {
    FFMPEGV_LOG("SVC setting is not implemented for %s codec",
                mCodecName.get());
    return Nothing();
  }

  Maybe<SVCLayerSettings> svc = GetSVCLayerSettings(
      codecType, mConfig.mScalabilityMode, mConfig.mBitrate);
  if (!svc) {
    FFMPEGV_LOG("No SVC settings obtained. Skip");
    return Nothing();
  }

  nsAutoCString name;
  nsAutoCString parameters;

  if (codecType == CodecType::VP8 || codecType == CodecType::VP9) {
    // Check if the number of temporal layers in codec specific settings
    // matches
    // the number of layers for the given scalability mode.
    if (mConfig.mCodecSpecific) {
      if (mConfig.mCodecSpecific->is<VP8Specific>()) {
        MOZ_ASSERT(
            mConfig.mCodecSpecific->as<VP8Specific>().mNumTemporalLayers ==
            svc->mNumberTemporalLayers);
      } else if (mConfig.mCodecSpecific->is<VP9Specific>()) {
        MOZ_ASSERT(
            mConfig.mCodecSpecific->as<VP9Specific>().mNumTemporalLayers ==
            svc->mNumberTemporalLayers);
      }
    }

    // Form an SVC setting string for libvpx.
    name = "ts-parameters"_ns;
    parameters.AppendPrintf(
        "ts_layering_mode=%u",
        svc->mCodecAppendix->as<VPXSVCAppendix>().mLayeringMode);
    parameters.Append(":ts_target_bitrate=");
    for (size_t i = 0; i < svc->mTargetBitrates.Length(); ++i) {
      if (i > 0) {
        parameters.Append(",");
      }
      parameters.AppendPrintf("%d", svc->mTargetBitrates[i]);
    }

    // TODO: Set ts_number_layers, ts_periodicity, ts_layer_id and
    // ts_rate_decimator if they are different from the preset values in
    // ts_layering_mode.
  }

  if (codecType == CodecType::AV1) {
    // Form an SVC setting string for libaom.
    name = "svc-parameters"_ns;
    parameters.AppendPrintf("number_spatial_layers=%zu",
                            svc->mNumberSpatialLayers);
    parameters.AppendPrintf(":number_temporal_layers=%zu",
                            svc->mNumberTemporalLayers);
    parameters.Append(":framerate_factor=");
    for (size_t i = 0; i < svc->mRateDecimators.Length(); ++i) {
      if (i > 0) {
        parameters.Append(",");
      }
      parameters.AppendPrintf("%d", svc->mRateDecimators[i]);
    }
    parameters.Append(":layer_target_bitrate=");
    for (size_t i = 0; i < svc->mTargetBitrates.Length(); ++i) {
      if (i > 0) {
        parameters.Append(",");
      }
      parameters.AppendPrintf("%d", svc->mTargetBitrates[i]);
    }
  }

  return Some(
      SVCSettings{std::move(svc->mLayerIds),
                  std::make_pair(std::move(name), std::move(parameters))});
}

FFmpegVideoEncoder<LIBAV_VER>::H264Settings FFmpegVideoEncoder<
    LIBAV_VER>::GetH264Settings(const H264Specific& aH264Specific) {
  MOZ_ASSERT(mCodecName == "libx264",
             "GetH264Settings is libx264-only for now");

  nsTArray<std::pair<nsCString, nsCString>> keyValuePairs;

  Maybe<H264Setting> profile = GetH264Profile(aH264Specific.mProfile);
  MOZ_RELEASE_ASSERT(profile.isSome());
  if (!profile->mString.IsEmpty()) {
    keyValuePairs.AppendElement(std::make_pair("profile"_ns, profile->mString));
  } else {
    MOZ_RELEASE_ASSERT(aH264Specific.mProfile ==
                       H264_PROFILE::H264_PROFILE_EXTENDED);
  }

  Maybe<H264Setting> level = GetH264Level(aH264Specific.mLevel);
  MOZ_RELEASE_ASSERT(level.isSome());
  MOZ_RELEASE_ASSERT(!level->mString.IsEmpty());
  keyValuePairs.AppendElement(std::make_pair("level"_ns, level->mString));

  // Set format: libx264's default format is annexb.
  if (aH264Specific.mFormat == H264BitStreamFormat::AVC) {
    keyValuePairs.AppendElement(std::make_pair("x264-params"_ns, "annexb=0"));
    // mCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER
    // if we don't want to append SPS/PPS data in all keyframe
    // (LIBAVCODEC_VERSION_MAJOR >= 57 only).
  } else {
    // Set annexb explicitly even if it's default format.
    keyValuePairs.AppendElement(std::make_pair("x264-params"_ns, "annexb=1"));
  }

  return H264Settings{.mProfile = profile->mValue,
                      .mLevel = level->mValue,
                      .mSettingKeyValuePairs = std::move(keyValuePairs)};
}

}  // namespace mozilla
