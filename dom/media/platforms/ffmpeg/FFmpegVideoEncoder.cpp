/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoEncoder.h"

#include "BufferReader.h"
#include "FFmpegLog.h"
#include "FFmpegRuntimeLinker.h"
#include "H264.h"
#include "ImageContainer.h"
#include "libavutil/error.h"
#include "libavutil/pixfmt.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/PodOperations.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ImageUtils.h"
#include "nsPrintfCString.h"
#include "ImageToI420.h"
#include "libyuv.h"

// The ffmpeg namespace is introduced to avoid the PixelFormat's name conflicts
// with MediaDataEncoder::PixelFormat in MediaDataEncoder class scope.
namespace ffmpeg {

#if LIBAVCODEC_VERSION_MAJOR >= 57
using FFmpegBitRate = int64_t;
constexpr size_t FFmpegErrorMaxStringSize = AV_ERROR_MAX_STRING_SIZE;
#else
using FFmpegBitRate = int;
constexpr size_t FFmpegErrorMaxStringSize = 64;
#endif

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

struct VPXSVCSetting {
  uint8_t mLayeringMode;
  size_t mNumberLayers;
  uint8_t mPeriodicity;
  nsTArray<uint8_t> mLayerIds;
  nsTArray<uint8_t> mRateDecimators;
  nsTArray<uint32_t> mTargetBitrates;
};

static Maybe<VPXSVCSetting> GetVPXSVCSetting(
    const MediaDataEncoder::ScalabilityMode& aMode, uint32_t aBitPerSec) {
  if (aMode == MediaDataEncoder::ScalabilityMode::None) {
    return Nothing();
  }

  // TODO: Apply more sophisticated bitrate allocation, like SvcRateAllocator:
  // https://searchfox.org/mozilla-central/rev/3bd65516eb9b3a9568806d846ba8c81a9402a885/third_party/libwebrtc/modules/video_coding/svc/svc_rate_allocator.h#26

  uint8_t mode = 0;
  size_t layers = 0;
  uint32_t kbps = aBitPerSec / 1000;  // ts_target_bitrate requies kbps.

  uint8_t periodicity;
  nsTArray<uint8_t> layerIds;
  nsTArray<uint8_t> rateDecimators;
  nsTArray<uint32_t> bitrates;
  if (aMode == MediaDataEncoder::ScalabilityMode::L1T2) {
    // Two temporal layers. 0-1...
    //
    // Frame pattern:
    // Layer 0: |0| |2| |4| |6| |8|
    // Layer 1: | |1| |3| |5| |7| |

    mode = 2;  // VP9E_TEMPORAL_LAYERING_MODE_0101
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
  } else {
    MOZ_ASSERT(aMode == MediaDataEncoder::ScalabilityMode::L1T3);
    // Three temporal layers. 0-2-1-2...
    //
    // Frame pattern:
    // Layer 0: |0| | | |4| | | |8| |  |  |12|
    // Layer 1: | | |2| | | |6| | | |10|  |  |
    // Layer 2: | |1| |3| |5| |7| |9|  |11|  |

    mode = 3;  // VP9E_TEMPORAL_LAYERING_MODE_0212
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
  }

  MOZ_ASSERT(layers == bitrates.Length(),
             "Bitrate must be assigned to each layer");
  return Some(VPXSVCSetting{mode, layers, periodicity, std::move(layerIds),
                            std::move(rateDecimators), std::move(bitrates)});
}

static nsCString MakeErrorString(const FFmpegLibWrapper* aLib, int aErrNum) {
  MOZ_ASSERT(aLib);

  char errStr[ffmpeg::FFmpegErrorMaxStringSize];
  aLib->av_strerror(aErrNum, errStr, ffmpeg::FFmpegErrorMaxStringSize);
  return nsCString(errStr);
}

// TODO: Remove this function and simply use `avcodec_find_encoder` once
// libopenh264 is supported.
static AVCodec* FindEncoderWithPreference(const FFmpegLibWrapper* aLib,
                                          AVCodecID aCodecId) {
  MOZ_ASSERT(aLib);

  AVCodec* codec = nullptr;

  // Prioritize libx264 for now since it's the only h264 codec we tested.
  if (aCodecId == AV_CODEC_ID_H264) {
    codec = aLib->avcodec_find_encoder_by_name("libx264");
    if (codec) {
      FFMPEGV_LOG("Prefer libx264 for h264 codec");
      return codec;
    }
  }

  FFMPEGV_LOG("Fallback to other h264 library. Fingers crossed");
  return aLib->avcodec_find_encoder(aCodecId);
}

template <>
AVCodecID GetFFmpegEncoderCodecId<LIBAV_VER>(CodecType aCodec) {
#if LIBAVCODEC_VERSION_MAJOR >= 58
  if (aCodec == CodecType::VP8) {
    return AV_CODEC_ID_VP8;
  }

  if (aCodec == CodecType::VP9) {
    return AV_CODEC_ID_VP9;
  }

#  if !defined(USING_MOZFFVPX)
  if (aCodec == CodecType::H264) {
    return AV_CODEC_ID_H264;
  }
#  endif

  if (aCodec == CodecType::AV1) {
    return AV_CODEC_ID_AV1;
  }
#endif
  return AV_CODEC_ID_NONE;
}

uint8_t FFmpegVideoEncoder<LIBAV_VER>::SVCInfo::UpdateTemporalLayerId() {
  MOZ_ASSERT(!mTemporalLayerIds.IsEmpty());

  size_t currentIndex = mNextIndex % mTemporalLayerIds.Length();
  mNextIndex += 1;
  return static_cast<uint8_t>(mTemporalLayerIds[currentIndex]);
}

StaticMutex FFmpegVideoEncoder<LIBAV_VER>::sMutex;

FFmpegVideoEncoder<LIBAV_VER>::FFmpegVideoEncoder(
    const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
    const RefPtr<TaskQueue>& aTaskQueue, const EncoderConfig& aConfig)
    : mLib(aLib),
      mCodecID(aCodecID),
      mTaskQueue(aTaskQueue),
      mConfig(aConfig),
      mCodecName(EmptyCString()),
      mCodecContext(nullptr),
      mFrame(nullptr),
      mSVCInfo(Nothing()) {
  MOZ_ASSERT(mLib);
  MOZ_ASSERT(mTaskQueue);
#if LIBAVCODEC_VERSION_MAJOR < 58
  MOZ_CRASH("FFmpegVideoEncoder needs ffmpeg 58 at least.");
#endif
};

RefPtr<MediaDataEncoder::InitPromise> FFmpegVideoEncoder<LIBAV_VER>::Init() {
  FFMPEGV_LOG("Init");
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegVideoEncoder::ProcessInit);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegVideoEncoder<LIBAV_VER>::Encode(
    const MediaData* aSample) {
  MOZ_ASSERT(aSample != nullptr);

  FFMPEGV_LOG("Encode");
  return InvokeAsync(mTaskQueue, __func__,
                     [self = RefPtr<FFmpegVideoEncoder<LIBAV_VER>>(this),
                      sample = RefPtr<const MediaData>(aSample)]() {
                       return self->ProcessEncode(std::move(sample));
                     });
}

RefPtr<MediaDataEncoder::ReconfigurationPromise>
FFmpegVideoEncoder<LIBAV_VER>::Reconfigure(
    const RefPtr<const EncoderConfigurationChangeList>& aConfigurationChanges) {
  return InvokeAsync<const RefPtr<const EncoderConfigurationChangeList>>(
      mTaskQueue, this, __func__,
      &FFmpegVideoEncoder<LIBAV_VER>::ProcessReconfigure,
      aConfigurationChanges);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegVideoEncoder<LIBAV_VER>::Drain() {
  FFMPEGV_LOG("Drain");
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegVideoEncoder::ProcessDrain);
}

RefPtr<ShutdownPromise> FFmpegVideoEncoder<LIBAV_VER>::Shutdown() {
  FFMPEGV_LOG("Shutdown");
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegVideoEncoder::ProcessShutdown);
}

RefPtr<GenericPromise> FFmpegVideoEncoder<LIBAV_VER>::SetBitrate(
    uint32_t aBitrate) {
  FFMPEGV_LOG("SetBitrate");
  return GenericPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

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

RefPtr<MediaDataEncoder::InitPromise>
FFmpegVideoEncoder<LIBAV_VER>::ProcessInit() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessInit");
  MediaResult r = InitInternal();
  return NS_FAILED(r)
             ? InitPromise::CreateAndReject(r, __func__)
             : InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER>::ProcessEncode(RefPtr<const MediaData> aSample) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessEncode");

#if LIBAVCODEC_VERSION_MAJOR < 58
  // TODO(Bug 1868253): implement encode with avcodec_encode_video2().
  MOZ_CRASH("FFmpegVideoEncoder needs ffmpeg 58 at least.");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else
  RefPtr<const VideoData> sample(aSample->As<const VideoData>());
  MOZ_ASSERT(sample);

  return EncodeWithModernAPIs(sample);
#endif
}

RefPtr<MediaDataEncoder::ReconfigurationPromise>
FFmpegVideoEncoder<LIBAV_VER>::ProcessReconfigure(
    const RefPtr<const EncoderConfigurationChangeList> aConfigurationChanges) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessReconfigure");

  // Tracked in bug 1869583 -- for now this encoder always reports it cannot be
  // reconfigured on the fly
  return MediaDataEncoder::ReconfigurationPromise::CreateAndReject(
      NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER>::ProcessDrain() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessDrain");

#if LIBAVCODEC_VERSION_MAJOR < 58
  MOZ_CRASH("FFmpegVideoEncoder needs ffmpeg 58 at least.");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else
  return DrainWithModernAPIs();
#endif
}

RefPtr<ShutdownPromise> FFmpegVideoEncoder<LIBAV_VER>::ProcessShutdown() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessShutdown");

  ShutdownInternal();

  // Don't shut mTaskQueue down since it's owned by others.
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

MediaResult FFmpegVideoEncoder<LIBAV_VER>::InitInternal() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("InitInternal");

  if (mCodecID == AV_CODEC_ID_H264) {
    // H264Specific is required to get the format (avcc vs annexb).
    if (!mConfig.mCodecSpecific ||
        !mConfig.mCodecSpecific->is<H264Specific>()) {
      return MediaResult(
          NS_ERROR_DOM_MEDIA_FATAL_ERR,
          RESULT_DETAIL("Unable to get H264 necessary encoding info"));
    }
  }

  AVCodec* codec = FindEncoderWithPreference(mLib, mCodecID);
  if (!codec) {
    FFMPEGV_LOG("failed to find ffmpeg encoder for codec id %d", mCodecID);
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("Unable to find codec"));
  }
  FFMPEGV_LOG("find codec: %s", codec->name);
  mCodecName = codec->name;

  ForceEnablingFFmpegDebugLogs();

  MOZ_ASSERT(!mCodecContext);
  if (!(mCodecContext = mLib->avcodec_alloc_context3(codec))) {
    FFMPEGV_LOG("failed to allocate ffmpeg context for codec %s", codec->name);
    return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                       RESULT_DETAIL("Failed to initialize ffmpeg context"));
  }

  // Set up AVCodecContext.
  mCodecContext->pix_fmt = ffmpeg::FFMPEG_PIX_FMT_YUV420P;
  mCodecContext->bit_rate =
      static_cast<ffmpeg::FFmpegBitRate>(mConfig.mBitrate);
  mCodecContext->width = static_cast<int>(mConfig.mSize.width);
  mCodecContext->height = static_cast<int>(mConfig.mSize.height);
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
  // TODO (bug 1872871): Move the following extra settings to some helpers
  // instead.
  if (mConfig.mUsage == MediaDataEncoder::Usage::Realtime) {
    mLib->av_opt_set(mCodecContext->priv_data, "deadline", "realtime", 0);
    // Explicitly ask encoder do not keep in flight at any one time for
    // lookahead purposes.
    mLib->av_opt_set(mCodecContext->priv_data, "lag-in-frames", "0", 0);
  }
  // Apply SVC settings.
  if (Maybe<VPXSVCSetting> svc =
          GetVPXSVCSetting(mConfig.mScalabilityMode, mConfig.mBitrate)) {
    // For libvpx.
    if (mCodecName == "libvpx" || mCodecName == "libvpx-vp9") {
      // Show a warning if mScalabilityMode mismatches mNumTemporalLayers
      if (mConfig.mCodecSpecific) {
        if (mConfig.mCodecSpecific->is<VP8Specific>() ||
            mConfig.mCodecSpecific->is<VP9Specific>()) {
          const uint8_t numTemporalLayers =
              mConfig.mCodecSpecific->is<VP8Specific>()
                  ? mConfig.mCodecSpecific->as<VP8Specific>().mNumTemporalLayers
                  : mConfig.mCodecSpecific->as<VP9Specific>()
                        .mNumTemporalLayers;
          if (numTemporalLayers != svc->mNumberLayers) {
            FFMPEGV_LOG(
                "Force using %zu layers defined in scalability mode instead of "
                "the %u layers defined in VP8/9Specific",
                svc->mNumberLayers, numTemporalLayers);
          }
        }
      }

      // Set ts_layering_mode.
      nsPrintfCString parameters("ts_layering_mode=%u", svc->mLayeringMode);
      // Set ts_target_bitrate.
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

      // Set parameters into ts-parameters.
      mLib->av_opt_set(mCodecContext->priv_data, "ts-parameters",
                       parameters.get(), 0);

      // FFmpegVideoEncoder would be reset after Drain(), so mSVCInfo should be
      // reset() before emplace().
      mSVCInfo.reset();
      mSVCInfo.emplace(std::move(svc->mLayerIds));

      // TODO: layer settings should be changed dynamically when the frame's
      // color space changed.
    } else {
      FFMPEGV_LOG("SVC setting is not implemented for %s codec",
                  mCodecName.get());
    }
  }
  // Apply codec specific settings.
  nsAutoCString codecSpecificLog;
  if (mConfig.mCodecSpecific) {
    if (mConfig.mCodecSpecific->is<H264Specific>()) {
      // For libx264.
      if (mCodecName == "libx264") {
        codecSpecificLog.Append(", H264:");

        const H264Specific& specific =
            mConfig.mCodecSpecific->as<H264Specific>();

        // Set profile.
        Maybe<H264Setting> profile = GetH264Profile(specific.mProfile);
        if (!profile) {
          FFMPEGV_LOG("failed to get h264 profile");
          return MediaResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
                             RESULT_DETAIL("H264 profile is unknown"));
        }
        codecSpecificLog.Append(
            nsPrintfCString(" profile - %d", profile->mValue));
        mCodecContext->profile = profile->mValue;
        if (!profile->mString.IsEmpty()) {
          codecSpecificLog.AppendPrintf(" (%s)", profile->mString.get());
          mLib->av_opt_set(mCodecContext->priv_data, "profile",
                           profile->mString.get(), 0);
        }

        // Set level.
        Maybe<H264Setting> level = GetH264Level(specific.mLevel);
        if (!level) {
          FFMPEGV_LOG("failed to get h264 level");
          return MediaResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
                             RESULT_DETAIL("H264 level is unknown"));
        }
        codecSpecificLog.AppendPrintf(", level %d (%s)", level->mValue,
                                      level->mString.get());
        mCodecContext->level = level->mValue;
        MOZ_ASSERT(!level->mString.IsEmpty());
        mLib->av_opt_set(mCodecContext->priv_data, "level",
                         level->mString.get(), 0);

        // Set format: libx264's default format is annexb
        if (specific.mFormat == H264BitStreamFormat::AVC) {
          codecSpecificLog.Append(", AVCC");
          mLib->av_opt_set(mCodecContext->priv_data, "x264-params", "annexb=0",
                           0);
          // mCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER
          // if we don't want to append SPS/PPS data in all keyframe
          // (LIBAVCODEC_VERSION_MAJOR >= 57 only).
        } else {
          codecSpecificLog.Append(", AnnexB");
          // Set annexb explicitly even if it's default format.
          mLib->av_opt_set(mCodecContext->priv_data, "x264-params", "annexb=1",
                           0);
        }
      } else {
        FFMPEGV_LOG("H264 settings is not implemented for codec %s ",
                    mCodecName.get());
      }
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

  AVDictionary* options = nullptr;
  if (int ret = OpenCodecContext(codec, &options); ret < 0) {
    FFMPEGV_LOG("failed to open %s avcodec: %s", codec->name,
                MakeErrorString(mLib, ret).get());
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("Unable to open avcodec"));
  }
  mLib->av_dict_free(&options);

  FFMPEGV_LOG("%s has been initialized with format: %s, bitrate: %" PRIi64
              ", width: %d, height: %d, time_base: %d/%d%s",
              codec->name, ffmpeg::GetPixelFormatString(mCodecContext->pix_fmt),
              static_cast<int64_t>(mCodecContext->bit_rate),
              mCodecContext->width, mCodecContext->height,
              mCodecContext->time_base.num, mCodecContext->time_base.den,
              codecSpecificLog.IsEmpty() ? "" : codecSpecificLog.get());

  return MediaResult(NS_OK);
}

void FFmpegVideoEncoder<LIBAV_VER>::ShutdownInternal() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ShutdownInternal");

  DestroyFrame();

  if (mCodecContext) {
    CloseCodecContext();
    mLib->av_freep(&mCodecContext);
    mCodecContext = nullptr;
  }
}

int FFmpegVideoEncoder<LIBAV_VER>::OpenCodecContext(const AVCodec* aCodec,
                                                    AVDictionary** aOptions) {
  MOZ_ASSERT(mCodecContext);

  StaticMutexAutoLock mon(sMutex);
  return mLib->avcodec_open2(mCodecContext, aCodec, aOptions);
}

void FFmpegVideoEncoder<LIBAV_VER>::CloseCodecContext() {
  MOZ_ASSERT(mCodecContext);

  StaticMutexAutoLock mon(sMutex);
  mLib->avcodec_close(mCodecContext);
}

bool FFmpegVideoEncoder<LIBAV_VER>::PrepareFrame() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  // TODO: Merge the duplicate part with FFmpegDataDecoder's PrepareFrame.
#if LIBAVCODEC_VERSION_MAJOR >= 55
  if (mFrame) {
    mLib->av_frame_unref(mFrame);
  } else {
    mFrame = mLib->av_frame_alloc();
  }
#elif LIBAVCODEC_VERSION_MAJOR == 54
  if (mFrame) {
    mLib->avcodec_get_frame_defaults(mFrame);
  } else {
    mFrame = mLib->avcodec_alloc_frame();
  }
#else
  mLib->av_freep(&mFrame);
  mFrame = mLib->avcodec_alloc_frame();
#endif
  return !!mFrame;
}

void FFmpegVideoEncoder<LIBAV_VER>::DestroyFrame() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  if (mFrame) {
#if LIBAVCODEC_VERSION_MAJOR >= 55
    mLib->av_frame_unref(mFrame);
    mLib->av_frame_free(&mFrame);
#elif LIBAVCODEC_VERSION_MAJOR == 54
    mLib->avcodec_free_frame(&mFrame);
#else
    mLib->av_freep(&mFrame);
#endif
    mFrame = nullptr;
  }
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
RefPtr<MediaDataEncoder::EncodePromise> FFmpegVideoEncoder<
    LIBAV_VER>::EncodeWithModernAPIs(RefPtr<const VideoData> aSample) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(mCodecContext);
  MOZ_ASSERT(aSample);

  // Validate input.
  if (!aSample->mImage) {
    FFMPEGV_LOG("No image");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_ILLEGAL_INPUT,
                    RESULT_DETAIL("No image in sample")),
        __func__);
  } else if (aSample->mImage->GetSize().IsEmpty()) {
    FFMPEGV_LOG("image width or height is invalid");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_ILLEGAL_INPUT,
                    RESULT_DETAIL("Invalid image size")),
        __func__);
  }

  // Allocate AVFrame.
  if (!PrepareFrame()) {
    FFMPEGV_LOG("failed to allocate frame");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Unable to allocate frame")),
        __func__);
  }

  // Set AVFrame properties for its internal data allocation. For now, we always
  // convert into ffmpeg's buffer.
  mFrame->format = ffmpeg::FFMPEG_PIX_FMT_YUV420P;
  mFrame->width = static_cast<int>(aSample->mImage->GetSize().width);
  mFrame->height = static_cast<int>(aSample->mImage->GetSize().height);

  // Allocate AVFrame data.
  if (int ret = mLib->av_frame_get_buffer(mFrame, 0); ret < 0) {
    FFMPEGV_LOG("failed to allocate frame data: %s",
                MakeErrorString(mLib, ret).get());
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Unable to allocate frame data")),
        __func__);
  }

  // Make sure AVFrame is writable.
  if (int ret = mLib->av_frame_make_writable(mFrame); ret < 0) {
    FFMPEGV_LOG("failed to make frame writable: %s",
                MakeErrorString(mLib, ret).get());
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_NOT_AVAILABLE,
                    RESULT_DETAIL("Unable to make frame writable")),
        __func__);
  }

  nsresult rv = ConvertToI420(
      aSample->mImage, mFrame->data[0], mFrame->linesize[0], mFrame->data[1],
      mFrame->linesize[1], mFrame->data[2], mFrame->linesize[2]);
  if (NS_FAILED(rv)) {
    FFMPEGV_LOG("Conversion error!");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_ILLEGAL_INPUT,
                    RESULT_DETAIL("libyuv conversion error")),
        __func__);
  }

  // Scale the YUV input frame if needed -- the encoded frame will have the
  // dimensions configured at encoded initialization.
  if (mFrame->width != mConfig.mSize.Width() ||
      mFrame->height != mConfig.mSize.Height()) {
    if (!ScaleInputFrame()) {
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_OUT_OF_MEMORY,
                      RESULT_DETAIL("libyuv scaling error")),
          __func__);
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
  mFrame->pkt_duration = aSample->mDuration.ToMicroseconds();

  // Initialize AVPacket.
  AVPacket* pkt = mLib->av_packet_alloc();

  if (!pkt) {
    FFMPEGV_LOG("failed to allocate packet");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Unable to allocate packet")),
        __func__);
  }

  auto freePacket = MakeScopeExit([this, &pkt] { mLib->av_packet_free(&pkt); });

  // Send frame and receive packets.

  if (int ret = mLib->avcodec_send_frame(mCodecContext, mFrame); ret < 0) {
    // In theory, avcodec_send_frame could sent -EAGAIN to signal its internal
    // buffers is full. In practice this can't happen as we only feed one frame
    // at a time, and we immediately call avcodec_receive_packet right after.
    // TODO: Create a NS_ERROR_DOM_MEDIA_ENCODE_ERR in ErrorList.py?
    FFMPEGV_LOG("avcodec_send_frame error: %s",
                MakeErrorString(mLib, ret).get());
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("avcodec_send_frame error")),
        __func__);
  }

  EncodedData output;
  while (true) {
    int ret = mLib->avcodec_receive_packet(mCodecContext, pkt);
    if (ret == AVERROR(EAGAIN)) {
      // The encoder is asking for more inputs.
      FFMPEGV_LOG("encoder is asking for more input!");
      break;
    }

    if (ret < 0) {
      // AVERROR_EOF is returned when the encoder has been fully flushed, but it
      // shouldn't happen here.
      FFMPEGV_LOG("avcodec_receive_packet error: %s",
                  MakeErrorString(mLib, ret).get());
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      RESULT_DETAIL("avcodec_receive_packet error")),
          __func__);
    }

    RefPtr<MediaRawData> d = ToMediaRawData(pkt);
    mLib->av_packet_unref(pkt);
    if (!d) {
      FFMPEGV_LOG("failed to create a MediaRawData from the AVPacket");
      return EncodePromise::CreateAndReject(
          MediaResult(
              NS_ERROR_OUT_OF_MEMORY,
              RESULT_DETAIL("Unable to get MediaRawData from AVPacket")),
          __func__);
    }
    output.AppendElement(std::move(d));
  }

  FFMPEGV_LOG("get %zu encoded data", output.Length());
  return EncodePromise::CreateAndResolve(std::move(output), __func__);
}

RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER>::DrainWithModernAPIs() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(mCodecContext);

  // TODO: Create a Result<EncodedData, nsresult> EncodeWithModernAPIs(AVFrame
  // *aFrame) to merge the duplicate code below with EncodeWithModernAPIs above.

  // Initialize AVPacket.
  AVPacket* pkt = mLib->av_packet_alloc();
  if (!pkt) {
    FFMPEGV_LOG("failed to allocate packet");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Unable to allocate packet")),
        __func__);
  }
  auto freePacket = MakeScopeExit([this, &pkt] { mLib->av_packet_free(&pkt); });

  // Enter draining mode by sending NULL to the avcodec_send_frame(). Note that
  // this can leave the encoder in a permanent EOF state after draining. As a
  // result, the encoder is unable to continue encoding. A new
  // AVCodecContext/encoder creation is required if users need to encode after
  // draining.
  //
  // TODO: Use `avcodec_flush_buffers` to drain the pending packets if
  // AV_CODEC_CAP_ENCODER_FLUSH is set in mCodecContext->codec->capabilities.
  if (int ret = mLib->avcodec_send_frame(mCodecContext, nullptr); ret < 0) {
    if (ret == AVERROR_EOF) {
      // The encoder has been flushed. Drain can be called multiple time.
      FFMPEGV_LOG("encoder has been flushed!");
      return EncodePromise::CreateAndResolve(EncodedData(), __func__);
    }

    FFMPEGV_LOG("avcodec_send_frame error: %s",
                MakeErrorString(mLib, ret).get());
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("avcodec_send_frame error")),
        __func__);
  }

  EncodedData output;
  while (true) {
    int ret = mLib->avcodec_receive_packet(mCodecContext, pkt);
    if (ret == AVERROR_EOF) {
      FFMPEGV_LOG("encoder has no more output packet!");
      break;
    }

    if (ret < 0) {
      // avcodec_receive_packet should not result in a -EAGAIN once it's in
      // draining mode.
      FFMPEGV_LOG("avcodec_receive_packet error: %s",
                  MakeErrorString(mLib, ret).get());
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      RESULT_DETAIL("avcodec_receive_packet error")),
          __func__);
    }

    RefPtr<MediaRawData> d = ToMediaRawData(pkt);
    mLib->av_packet_unref(pkt);
    if (!d) {
      FFMPEGV_LOG("failed to create a MediaRawData from the AVPacket");
      return EncodePromise::CreateAndReject(
          MediaResult(
              NS_ERROR_OUT_OF_MEMORY,
              RESULT_DETAIL("Unable to get MediaRawData from AVPacket")),
          __func__);
    }
    output.AppendElement(std::move(d));
  }

  FFMPEGV_LOG("get %zu encoded data", output.Length());

  // TODO: Evaluate a better solution (Bug 1869466)
  // TODO: Only re-create AVCodecContext when avcodec_flush_buffers is
  // unavailable.
  ShutdownInternal();
  MediaResult r = InitInternal();
  return NS_FAILED(r)
             ? EncodePromise::CreateAndReject(r, __func__)
             : EncodePromise::CreateAndResolve(std::move(output), __func__);
}
#endif

RefPtr<MediaRawData> FFmpegVideoEncoder<LIBAV_VER>::ToMediaRawData(
    AVPacket* aPacket) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(aPacket);

  // TODO: Do we need to check AV_PKT_FLAG_CORRUPT?

  // Copy frame data from AVPacket.
  auto data = MakeRefPtr<MediaRawData>();
  UniquePtr<MediaRawDataWriter> writer(data->CreateWriter());
  if (!writer->Append(aPacket->data, static_cast<size_t>(aPacket->size))) {
    FFMPEGV_LOG("fail to allocate MediaRawData buffer");
    return nullptr;  // OOM
  }

  data->mKeyframe = (aPacket->flags & AV_PKT_FLAG_KEY) != 0;
  // TODO(bug 1869560): The unit of pts, dts, and duration is time_base, which
  // is recommended to be the reciprocal of the frame rate, but we set it to
  // microsecond for now.
  data->mTime = media::TimeUnit::FromMicroseconds(aPacket->pts);
#if LIBAVCODEC_VERSION_MAJOR >= 60
  data->mDuration = media::TimeUnit::FromMicroseconds(aPacket->duration);
#else
  int64_t duration;
  if (mDurationMap.Find(aPacket->pts, duration)) {
    data->mDuration = media::TimeUnit::FromMicroseconds(duration);
  } else {
    data->mDuration = media::TimeUnit::FromMicroseconds(aPacket->duration);
  }
#endif
  data->mTimecode = media::TimeUnit::FromMicroseconds(aPacket->dts);

  if (auto r = GetExtraData(aPacket); r.isOk()) {
    data->mExtraData = r.unwrap();
  }

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

}  // namespace mozilla
