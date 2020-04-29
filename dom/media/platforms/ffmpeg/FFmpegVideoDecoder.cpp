/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoDecoder.h"
#include "FFmpegLog.h"
#include "ImageContainer.h"
#include "MP4Decoder.h"
#include "MediaInfo.h"
#include "VPXDecoder.h"
#include "mozilla/layers/KnowsCompositor.h"
#ifdef MOZ_WAYLAND_USE_VAAPI
#  include "gfxPlatformGtk.h"
#  include "mozilla/layers/WaylandDMABUFSurfaceImage.h"
#  include "H264.h"
#endif

#include "libavutil/pixfmt.h"
#if LIBAVCODEC_VERSION_MAJOR < 54
#  define AVPixelFormat PixelFormat
#  define AV_PIX_FMT_YUV420P PIX_FMT_YUV420P
#  define AV_PIX_FMT_YUVJ420P PIX_FMT_YUVJ420P
#  define AV_PIX_FMT_YUV420P10LE PIX_FMT_YUV420P10LE
#  define AV_PIX_FMT_YUV422P PIX_FMT_YUV422P
#  define AV_PIX_FMT_YUV422P10LE PIX_FMT_YUV422P10LE
#  define AV_PIX_FMT_YUV444P PIX_FMT_YUV444P
#  define AV_PIX_FMT_YUV444P10LE PIX_FMT_YUV444P10LE
#  define AV_PIX_FMT_NONE PIX_FMT_NONE
#endif
#include "mozilla/PodOperations.h"
#include "mozilla/TaskQueue.h"
#include "nsThreadUtils.h"
#include "prsystem.h"

// Forward declare from va.h
#ifdef MOZ_WAYLAND_USE_VAAPI
typedef int VAStatus;
#  define VA_EXPORT_SURFACE_READ_ONLY 0x0001
#  define VA_EXPORT_SURFACE_SEPARATE_LAYERS 0x0004
#  define VA_STATUS_SUCCESS 0x00000000
#endif

// Use some extra HW frames for potential rendering lags.
#define EXTRA_HW_FRAMES 6

typedef mozilla::layers::Image Image;
typedef mozilla::layers::PlanarYCbCrImage PlanarYCbCrImage;

namespace mozilla {

using media::TimeUnit;

/**
 * FFmpeg calls back to this function with a list of pixel formats it supports.
 * We choose a pixel format that we support and return it.
 * For now, we just look for YUV420P, YUVJ420P and YUV444 as those are the only
 * only non-HW accelerated format supported by FFmpeg's H264 and VP9 decoder.
 */
static AVPixelFormat ChoosePixelFormat(AVCodecContext* aCodecContext,
                                       const AVPixelFormat* aFormats) {
  FFMPEG_LOG("Choosing FFmpeg pixel format for video decoding.");
  for (; *aFormats > -1; aFormats++) {
    switch (*aFormats) {
      case AV_PIX_FMT_YUV420P:
        FFMPEG_LOG("Requesting pixel format YUV420P.");
        return AV_PIX_FMT_YUV420P;
      case AV_PIX_FMT_YUVJ420P:
        FFMPEG_LOG("Requesting pixel format YUVJ420P.");
        return AV_PIX_FMT_YUVJ420P;
      case AV_PIX_FMT_YUV420P10LE:
        FFMPEG_LOG("Requesting pixel format YUV420P10LE.");
        return AV_PIX_FMT_YUV420P10LE;
      case AV_PIX_FMT_YUV422P:
        FFMPEG_LOG("Requesting pixel format YUV422P.");
        return AV_PIX_FMT_YUV422P;
      case AV_PIX_FMT_YUV422P10LE:
        FFMPEG_LOG("Requesting pixel format YUV422P10LE.");
        return AV_PIX_FMT_YUV422P10LE;
      case AV_PIX_FMT_YUV444P:
        FFMPEG_LOG("Requesting pixel format YUV444P.");
        return AV_PIX_FMT_YUV444P;
      case AV_PIX_FMT_YUV444P10LE:
        FFMPEG_LOG("Requesting pixel format YUV444P10LE.");
        return AV_PIX_FMT_YUV444P10LE;
#if LIBAVCODEC_VERSION_MAJOR >= 57
      case AV_PIX_FMT_YUV420P12LE:
        FFMPEG_LOG("Requesting pixel format YUV420P12LE.");
        return AV_PIX_FMT_YUV420P12LE;
      case AV_PIX_FMT_YUV422P12LE:
        FFMPEG_LOG("Requesting pixel format YUV422P12LE.");
        return AV_PIX_FMT_YUV422P12LE;
      case AV_PIX_FMT_YUV444P12LE:
        FFMPEG_LOG("Requesting pixel format YUV444P12LE.");
        return AV_PIX_FMT_YUV444P12LE;
#endif
      default:
        break;
    }
  }

  NS_WARNING("FFmpeg does not share any supported pixel formats.");
  return AV_PIX_FMT_NONE;
}

#ifdef MOZ_WAYLAND_USE_VAAPI
static AVPixelFormat ChooseVAAPIPixelFormat(AVCodecContext* aCodecContext,
                                            const AVPixelFormat* aFormats) {
  FFMPEG_LOG("Choosing FFmpeg pixel format for VA-API video decoding.");
  for (; *aFormats > -1; aFormats++) {
    switch (*aFormats) {
      case AV_PIX_FMT_VAAPI_VLD:
        FFMPEG_LOG("Requesting pixel format VAAPI_VLD\n");
        return AV_PIX_FMT_VAAPI_VLD;
      default:
        break;
    }
  }

  NS_WARNING("FFmpeg does not share any supported pixel formats.");
  return AV_PIX_FMT_NONE;
}

VAAPIFrameHolder::VAAPIFrameHolder(FFmpegLibWrapper* aLib,
                                   AVBufferRef* aVAAPIDeviceContext,
                                   AVBufferRef* aAVHWFramesContext,
                                   AVBufferRef* aHWFrame)
    : mLib(aLib),
      mVAAPIDeviceContext(mLib->av_buffer_ref(aVAAPIDeviceContext)),
      mAVHWFramesContext(mLib->av_buffer_ref(aAVHWFramesContext)),
      mHWFrame(mLib->av_buffer_ref(aHWFrame)){};

VAAPIFrameHolder::~VAAPIFrameHolder() {
  mLib->av_buffer_unref(&mHWFrame);
  mLib->av_buffer_unref(&mAVHWFramesContext);
  mLib->av_buffer_unref(&mVAAPIDeviceContext);
}

AVCodec* FFmpegVideoDecoder<LIBAV_VER>::FindVAAPICodec() {
  AVCodec* decoder = mLib->avcodec_find_decoder(mCodecID);
  for (int i = 0;; i++) {
    const AVCodecHWConfig* config = mLib->avcodec_get_hw_config(decoder, i);
    if (!config) {
      break;
    }
    if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
        config->device_type == AV_HWDEVICE_TYPE_VAAPI) {
      return decoder;
    }
  }

  FFMPEG_LOG("Decoder does not support VAAPI device type");
  return nullptr;
}

class VAAPIDisplayHolder {
 public:
  VAAPIDisplayHolder(FFmpegLibWrapper* aLib, VADisplay aDisplay)
      : mLib(aLib), mDisplay(aDisplay){};
  ~VAAPIDisplayHolder() { mLib->vaTerminate(mDisplay); }

 private:
  FFmpegLibWrapper* mLib;
  VADisplay mDisplay;
};

static void VAAPIDisplayReleaseCallback(struct AVHWDeviceContext* hwctx) {
  auto displayHolder = static_cast<VAAPIDisplayHolder*>(hwctx->user_opaque);
  delete displayHolder;
}

bool FFmpegVideoDecoder<LIBAV_VER>::CreateVAAPIDeviceContext() {
  mVAAPIDeviceContext = mLib->av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
  if (!mVAAPIDeviceContext) {
    return false;
  }
  AVHWDeviceContext* hwctx = (AVHWDeviceContext*)mVAAPIDeviceContext->data;
  AVVAAPIDeviceContext* vactx = (AVVAAPIDeviceContext*)hwctx->hwctx;

  wl_display* display = widget::WaylandDisplayGetWLDisplay();
  if (!display) {
    FFMPEG_LOG("Can't get default wayland display.");
    return false;
  }
  mDisplay = mLib->vaGetDisplayWl(display);

  hwctx->user_opaque = new VAAPIDisplayHolder(mLib, mDisplay);
  hwctx->free = VAAPIDisplayReleaseCallback;

  int major, minor;
  int status = mLib->vaInitialize(mDisplay, &major, &minor);
  if (status != VA_STATUS_SUCCESS) {
    return false;
  }

  vactx->display = mDisplay;

  if (mLib->av_hwdevice_ctx_init(mVAAPIDeviceContext) < 0) {
    return false;
  }

  mCodecContext->hw_device_ctx = mLib->av_buffer_ref(mVAAPIDeviceContext);
  return true;
}

MediaResult FFmpegVideoDecoder<LIBAV_VER>::InitVAAPIDecoder() {
  FFMPEG_LOG("Initialising VA-API FFmpeg decoder");

  if (!mLib->IsVAAPIAvailable()) {
    FFMPEG_LOG("libva library or symbols are missing.");
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto layersBackend = mImageAllocator
                           ? mImageAllocator->GetCompositorBackendType()
                           : layers::LayersBackend::LAYERS_BASIC;
  if (layersBackend != layers::LayersBackend::LAYERS_WR) {
    FFMPEG_LOG("VA-API works with WebRender only!");
    return NS_ERROR_NOT_AVAILABLE;
  }

  AVCodec* codec = FindVAAPICodec();
  if (!codec) {
    FFMPEG_LOG("Couldn't find ffmpeg VA-API decoder");
    return NS_ERROR_DOM_MEDIA_FATAL_ERR;
  }

  StaticMutexAutoLock mon(sMonitor);

  if (!(mCodecContext = mLib->avcodec_alloc_context3(codec))) {
    FFMPEG_LOG("Couldn't init VA-API ffmpeg context");
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mCodecContext->opaque = this;

  InitVAAPICodecContext();

  if (!CreateVAAPIDeviceContext()) {
    mLib->av_freep(&mCodecContext);
    FFMPEG_LOG("Failed to create VA-API device context");
    return NS_ERROR_DOM_MEDIA_FATAL_ERR;
  }

  MediaResult ret = AllocateExtraData();
  if (NS_FAILED(ret)) {
    mLib->av_buffer_unref(&mVAAPIDeviceContext);
    mLib->av_freep(&mCodecContext);
    return ret;
  }

  if (mLib->avcodec_open2(mCodecContext, codec, nullptr) < 0) {
    mLib->av_buffer_unref(&mVAAPIDeviceContext);
    mLib->av_freep(&mCodecContext);
    FFMPEG_LOG("Couldn't initialise VA-API decoder");
    return NS_ERROR_DOM_MEDIA_FATAL_ERR;
  }

  FFMPEG_LOG("VA-API FFmpeg init successful");
  return NS_OK;
}

#endif

FFmpegVideoDecoder<LIBAV_VER>::PtsCorrectionContext::PtsCorrectionContext()
    : mNumFaultyPts(0),
      mNumFaultyDts(0),
      mLastPts(INT64_MIN),
      mLastDts(INT64_MIN) {}

int64_t FFmpegVideoDecoder<LIBAV_VER>::PtsCorrectionContext::GuessCorrectPts(
    int64_t aPts, int64_t aDts) {
  int64_t pts = AV_NOPTS_VALUE;

  if (aDts != int64_t(AV_NOPTS_VALUE)) {
    mNumFaultyDts += aDts <= mLastDts;
    mLastDts = aDts;
  }
  if (aPts != int64_t(AV_NOPTS_VALUE)) {
    mNumFaultyPts += aPts <= mLastPts;
    mLastPts = aPts;
  }
  if ((mNumFaultyPts <= mNumFaultyDts || aDts == int64_t(AV_NOPTS_VALUE)) &&
      aPts != int64_t(AV_NOPTS_VALUE)) {
    pts = aPts;
  } else {
    pts = aDts;
  }
  return pts;
}

void FFmpegVideoDecoder<LIBAV_VER>::PtsCorrectionContext::Reset() {
  mNumFaultyPts = 0;
  mNumFaultyDts = 0;
  mLastPts = INT64_MIN;
  mLastDts = INT64_MIN;
}

FFmpegVideoDecoder<LIBAV_VER>::FFmpegVideoDecoder(
    FFmpegLibWrapper* aLib, TaskQueue* aTaskQueue, const VideoInfo& aConfig,
    KnowsCompositor* aAllocator, ImageContainer* aImageContainer,
    bool aLowLatency, bool aDisableHardwareDecoding)
    : FFmpegDataDecoder(aLib, aTaskQueue, GetCodecId(aConfig.mMimeType)),
#ifdef MOZ_WAYLAND_USE_VAAPI
      mVAAPIDeviceContext(nullptr),
      mDisableHardwareDecoding(aDisableHardwareDecoding),
      mDisplay(nullptr),
#endif
      mImageAllocator(aAllocator),
      mImageContainer(aImageContainer),
      mInfo(aConfig),
      mLowLatency(aLowLatency) {
  // Use a new MediaByteBuffer as the object will be modified during
  // initialization.
  mExtraData = new MediaByteBuffer;
  mExtraData->AppendElements(*aConfig.mExtraData);
}

RefPtr<MediaDataDecoder::InitPromise> FFmpegVideoDecoder<LIBAV_VER>::Init() {
  MediaResult rv;

#ifdef MOZ_WAYLAND_USE_VAAPI
  if (!mDisableHardwareDecoding) {
    rv = InitVAAPIDecoder();
    if (NS_SUCCEEDED(rv)) {
      return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
    }
  }
#endif

  rv = InitDecoder();
  if (NS_SUCCEEDED(rv)) {
    return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
  }

  return InitPromise::CreateAndReject(rv, __func__);
}

void FFmpegVideoDecoder<LIBAV_VER>::InitCodecContext() {
  mCodecContext->width = mInfo.mImage.width;
  mCodecContext->height = mInfo.mImage.height;

  // We use the same logic as libvpx in determining the number of threads to use
  // so that we end up behaving in the same fashion when using ffmpeg as
  // we would otherwise cause various crashes (see bug 1236167)
  int decode_threads = 1;
  if (mInfo.mDisplay.width >= 2048) {
    decode_threads = 8;
  } else if (mInfo.mDisplay.width >= 1024) {
    decode_threads = 4;
  } else if (mInfo.mDisplay.width >= 320) {
    decode_threads = 2;
  }

  if (mLowLatency) {
    mCodecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
    // ffvp9 and ffvp8 at this stage do not support slice threading, but it may
    // help with the h264 decoder if there's ever one.
    mCodecContext->thread_type = FF_THREAD_SLICE;
  } else {
    decode_threads = std::min(decode_threads, PR_GetNumberOfProcessors() - 1);
    decode_threads = std::max(decode_threads, 1);
    mCodecContext->thread_count = decode_threads;
    if (decode_threads > 1) {
      mCodecContext->thread_type = FF_THREAD_SLICE | FF_THREAD_FRAME;
    }
  }

  // FFmpeg will call back to this to negotiate a video pixel format.
  mCodecContext->get_format = ChoosePixelFormat;
}

#ifdef MOZ_WAYLAND_USE_VAAPI
void FFmpegVideoDecoder<LIBAV_VER>::InitVAAPICodecContext() {
  mCodecContext->width = mInfo.mImage.width;
  mCodecContext->height = mInfo.mImage.height;
  mCodecContext->thread_count = 1;
  mCodecContext->get_format = ChooseVAAPIPixelFormat;
  if (mCodecID == AV_CODEC_ID_H264) {
    mCodecContext->extra_hw_frames =
        H264::ComputeMaxRefFrames(mInfo.mExtraData);
  } else {
    mCodecContext->extra_hw_frames = EXTRA_HW_FRAMES;
  }
}
#endif

MediaResult FFmpegVideoDecoder<LIBAV_VER>::DoDecode(
    MediaRawData* aSample, uint8_t* aData, int aSize, bool* aGotFrame,
    MediaDataDecoder::DecodedData& aResults) {
  AVPacket packet;
  mLib->av_init_packet(&packet);

  packet.data = aData;
  packet.size = aSize;
  packet.dts = aSample->mTimecode.ToMicroseconds();
  packet.pts = aSample->mTime.ToMicroseconds();
  packet.flags = aSample->mKeyframe ? AV_PKT_FLAG_KEY : 0;
  packet.pos = aSample->mOffset;

#if LIBAVCODEC_VERSION_MAJOR >= 58
  packet.duration = aSample->mDuration.ToMicroseconds();
  int res = mLib->avcodec_send_packet(mCodecContext, &packet);
  if (res < 0) {
    // In theory, avcodec_send_packet could sent -EAGAIN should its internal
    // buffers be full. In practice this can't happen as we only feed one frame
    // at a time, and we immediately call avcodec_receive_frame right after.
    FFMPEG_LOG("avcodec_send_packet error: %d", res);
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("avcodec_send_packet error: %d", res));
  }

  if (aGotFrame) {
    *aGotFrame = false;
  }
  do {
    if (!PrepareFrame()) {
      NS_WARNING("FFmpeg h264 decoder failed to allocate frame.");
      return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    res = mLib->avcodec_receive_frame(mCodecContext, mFrame);
    if (res == int(AVERROR_EOF)) {
      return NS_ERROR_DOM_MEDIA_END_OF_STREAM;
    }
    if (res == AVERROR(EAGAIN)) {
      return NS_OK;
    }
    if (res < 0) {
      FFMPEG_LOG("avcodec_receive_frame error: %d", res);
      return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("avcodec_receive_frame error: %d", res));
    }

    MediaResult rv;
#  ifdef MOZ_WAYLAND_USE_VAAPI
    if (mVAAPIDeviceContext) {
      MOZ_ASSERT(mFrame->format == AV_PIX_FMT_VAAPI_VLD);
      rv = CreateImageVAAPI(mFrame->pkt_pos, mFrame->pkt_pts,
                            mFrame->pkt_duration, aResults);
    } else
#  endif
    {
      rv = CreateImage(mFrame->pkt_pos, mFrame->pkt_pts, mFrame->pkt_duration,
                       aResults);
    }
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (aGotFrame) {
      *aGotFrame = true;
    }
  } while (true);
#else
  // LibAV provides no API to retrieve the decoded sample's duration.
  // (FFmpeg >= 1.0 provides av_frame_get_pkt_duration)
  // As such we instead use a map using the dts as key that we will retrieve
  // later.
  // The map will have a typical size of 16 entry.
  mDurationMap.Insert(aSample->mTimecode.ToMicroseconds(),
                      aSample->mDuration.ToMicroseconds());

  if (!PrepareFrame()) {
    NS_WARNING("FFmpeg h264 decoder failed to allocate frame.");
    return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  // Required with old version of FFmpeg/LibAV
  mFrame->reordered_opaque = AV_NOPTS_VALUE;

  int decoded;
  int bytesConsumed =
      mLib->avcodec_decode_video2(mCodecContext, mFrame, &decoded, &packet);

  FFMPEG_LOG(
      "DoDecodeFrame:decode_video: rv=%d decoded=%d "
      "(Input: pts(%" PRId64 ") dts(%" PRId64 ") Output: pts(%" PRId64
      ") "
      "opaque(%" PRId64 ") pkt_pts(%" PRId64 ") pkt_dts(%" PRId64 "))",
      bytesConsumed, decoded, packet.pts, packet.dts, mFrame->pts,
      mFrame->reordered_opaque, mFrame->pkt_pts, mFrame->pkt_dts);

  if (bytesConsumed < 0) {
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("FFmpeg video error:%d", bytesConsumed));
  }

  if (!decoded) {
    if (aGotFrame) {
      *aGotFrame = false;
    }
    return NS_OK;
  }

  // If we've decoded a frame then we need to output it
  int64_t pts = mPtsContext.GuessCorrectPts(mFrame->pkt_pts, mFrame->pkt_dts);
  // Retrieve duration from dts.
  // We use the first entry found matching this dts (this is done to
  // handle damaged file with multiple frames with the same dts)

  int64_t duration;
  if (!mDurationMap.Find(mFrame->pkt_dts, duration)) {
    NS_WARNING("Unable to retrieve duration from map");
    duration = aSample->mDuration.ToMicroseconds();
    // dts are probably incorrectly reported ; so clear the map as we're
    // unlikely to find them in the future anyway. This also guards
    // against the map becoming extremely big.
    mDurationMap.Clear();
  }

  MediaResult rv = CreateImage(aSample->mOffset, pts, duration, aResults);
  if (NS_SUCCEEDED(rv) && aGotFrame) {
    *aGotFrame = true;
  }
  return rv;
#endif
}

gfx::YUVColorSpace FFmpegVideoDecoder<LIBAV_VER>::GetFrameColorSpace() {
  if (mLib->av_frame_get_colorspace) {
    switch (mLib->av_frame_get_colorspace(mFrame)) {
#if LIBAVCODEC_VERSION_MAJOR >= 55
      case AVCOL_SPC_BT2020_NCL:
      case AVCOL_SPC_BT2020_CL:
        return gfx::YUVColorSpace::BT2020;
#endif
      case AVCOL_SPC_BT709:
        return gfx::YUVColorSpace::BT709;
      case AVCOL_SPC_SMPTE170M:
      case AVCOL_SPC_BT470BG:
        return gfx::YUVColorSpace::BT601;
      default:
        break;
    }
  }
  return DefaultColorSpace({mFrame->width, mFrame->height});
}

MediaResult FFmpegVideoDecoder<LIBAV_VER>::CreateImage(
    int64_t aOffset, int64_t aPts, int64_t aDuration,
    MediaDataDecoder::DecodedData& aResults) {
  FFMPEG_LOG("Got one frame output with pts=%" PRId64 " dts=%" PRId64
             " duration=%" PRId64 " opaque=%" PRId64,
             aPts, mFrame->pkt_dts, aDuration, mCodecContext->reordered_opaque);

  VideoData::YCbCrBuffer b;
  b.mPlanes[0].mData = mFrame->data[0];
  b.mPlanes[1].mData = mFrame->data[1];
  b.mPlanes[2].mData = mFrame->data[2];

  b.mPlanes[0].mStride = mFrame->linesize[0];
  b.mPlanes[1].mStride = mFrame->linesize[1];
  b.mPlanes[2].mStride = mFrame->linesize[2];

  b.mPlanes[0].mOffset = b.mPlanes[0].mSkip = 0;
  b.mPlanes[1].mOffset = b.mPlanes[1].mSkip = 0;
  b.mPlanes[2].mOffset = b.mPlanes[2].mSkip = 0;

  b.mPlanes[0].mWidth = mFrame->width;
  b.mPlanes[0].mHeight = mFrame->height;
  if (mCodecContext->pix_fmt == AV_PIX_FMT_YUV444P ||
      mCodecContext->pix_fmt == AV_PIX_FMT_YUV444P10LE
#if LIBAVCODEC_VERSION_MAJOR >= 57
      || mCodecContext->pix_fmt == AV_PIX_FMT_YUV444P12LE
#endif
  ) {
    b.mPlanes[1].mWidth = b.mPlanes[2].mWidth = mFrame->width;
    b.mPlanes[1].mHeight = b.mPlanes[2].mHeight = mFrame->height;
    if (mCodecContext->pix_fmt == AV_PIX_FMT_YUV444P10LE) {
      b.mColorDepth = gfx::ColorDepth::COLOR_10;
    }
#if LIBAVCODEC_VERSION_MAJOR >= 57
    else if (mCodecContext->pix_fmt == AV_PIX_FMT_YUV444P12LE) {
      b.mColorDepth = gfx::ColorDepth::COLOR_12;
    }
#endif
  } else if (mCodecContext->pix_fmt == AV_PIX_FMT_YUV422P ||
             mCodecContext->pix_fmt == AV_PIX_FMT_YUV422P10LE
#if LIBAVCODEC_VERSION_MAJOR >= 57
             || mCodecContext->pix_fmt == AV_PIX_FMT_YUV422P12LE
#endif
  ) {
    b.mPlanes[1].mWidth = b.mPlanes[2].mWidth = (mFrame->width + 1) >> 1;
    b.mPlanes[1].mHeight = b.mPlanes[2].mHeight = mFrame->height;
    if (mCodecContext->pix_fmt == AV_PIX_FMT_YUV422P10LE) {
      b.mColorDepth = gfx::ColorDepth::COLOR_10;
    }
#if LIBAVCODEC_VERSION_MAJOR >= 57
    else if (mCodecContext->pix_fmt == AV_PIX_FMT_YUV422P12LE) {
      b.mColorDepth = gfx::ColorDepth::COLOR_12;
    }
#endif
  } else {
    b.mPlanes[1].mWidth = b.mPlanes[2].mWidth = (mFrame->width + 1) >> 1;
    b.mPlanes[1].mHeight = b.mPlanes[2].mHeight = (mFrame->height + 1) >> 1;
    if (mCodecContext->pix_fmt == AV_PIX_FMT_YUV420P10LE) {
      b.mColorDepth = gfx::ColorDepth::COLOR_10;
    }
#if LIBAVCODEC_VERSION_MAJOR >= 57
    else if (mCodecContext->pix_fmt == AV_PIX_FMT_YUV420P12LE) {
      b.mColorDepth = gfx::ColorDepth::COLOR_12;
    }
#endif
  }
  b.mYUVColorSpace = GetFrameColorSpace();

  if (mLib->av_frame_get_color_range) {
    auto range = mLib->av_frame_get_color_range(mFrame);
    b.mColorRange = range == AVCOL_RANGE_JPEG ? gfx::ColorRange::FULL
                                              : gfx::ColorRange::LIMITED;
  }

  RefPtr<VideoData> v = VideoData::CreateAndCopyData(
      mInfo, mImageContainer, aOffset, TimeUnit::FromMicroseconds(aPts),
      TimeUnit::FromMicroseconds(aDuration), b, !!mFrame->key_frame,
      TimeUnit::FromMicroseconds(-1),
      mInfo.ScaledImageRect(mFrame->width, mFrame->height), mImageAllocator);

  if (!v) {
    return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                       RESULT_DETAIL("image allocation error"));
  }
  aResults.AppendElement(std::move(v));
  return NS_OK;
}

#ifdef MOZ_WAYLAND_USE_VAAPI
static void VAAPIFrameReleaseCallback(VAAPIFrameHolder* aVAAPIFrameHolder) {
  auto frameHolder = static_cast<VAAPIFrameHolder*>(aVAAPIFrameHolder);
  delete frameHolder;
}

MediaResult FFmpegVideoDecoder<LIBAV_VER>::CreateImageVAAPI(
    int64_t aOffset, int64_t aPts, int64_t aDuration,
    MediaDataDecoder::DecodedData& aResults) {
  FFMPEG_LOG("Got one VAAPI frame output with pts=%" PRId64 " dts=%" PRId64
             " duration=%" PRId64 " opaque=%" PRId64,
             aPts, mFrame->pkt_dts, aDuration, mCodecContext->reordered_opaque);

  VADRMPRIMESurfaceDescriptor va_desc;
  VASurfaceID surface_id = (VASurfaceID)(uintptr_t)mFrame->data[3];
  VAStatus vas = mLib->vaExportSurfaceHandle(
      mDisplay, surface_id, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
      VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
      &va_desc);
  if (vas != VA_STATUS_SUCCESS) {
    return MediaResult(
        NS_ERROR_OUT_OF_MEMORY,
        RESULT_DETAIL("Unable to get frame by vaExportSurfaceHandle()"));
  }
  vas = mLib->vaSyncSurface(mDisplay, surface_id);
  if (vas != VA_STATUS_SUCCESS) {
    NS_WARNING("vaSyncSurface() failed.");
  }

  va_desc.width = mFrame->width;
  va_desc.height = mFrame->height;

  RefPtr<WaylandDMABufSurfaceNV12> surface =
      WaylandDMABufSurfaceNV12::CreateNV12Surface(va_desc);
  if (!surface) {
    return MediaResult(
        NS_ERROR_OUT_OF_MEMORY,
        RESULT_DETAIL("Unable to allocate WaylandDMABufSurfaceNV12."));
  }

  surface->SetYUVColorSpace(GetFrameColorSpace());

  // mFrame->buf[0] is a reference to H264 VASurface for this mFrame.
  // We need create WaylandDMABUFSurfaceImage on top of it,
  // create EGLImage/Texture on top of it and render it by GL.

  // FFmpeg tends to reuse the particual VASurface for another frame
  // even when the mFrame is not released. To keep VASurface as is
  // we explicitly reference it and keep until WaylandDMABUFSurfaceImage
  // is live.
  RefPtr<layers::Image> im = new layers::WaylandDMABUFSurfaceImage(
      surface, VAAPIFrameReleaseCallback,
      new VAAPIFrameHolder(mLib, mVAAPIDeviceContext,
                           mCodecContext->hw_frames_ctx, mFrame->buf[0]));

  RefPtr<VideoData> vp = VideoData::CreateFromImage(
      mInfo.mDisplay, aOffset, TimeUnit::FromMicroseconds(aPts),
      TimeUnit::FromMicroseconds(aDuration), im, !!mFrame->key_frame,
      TimeUnit::FromMicroseconds(-1));

  if (!vp) {
    return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                       RESULT_DETAIL("image allocation error"));
  }

  aResults.AppendElement(std::move(vp));
  return NS_OK;
}
#endif

RefPtr<MediaDataDecoder::FlushPromise>
FFmpegVideoDecoder<LIBAV_VER>::ProcessFlush() {
  mPtsContext.Reset();
  mDurationMap.Clear();
  return FFmpegDataDecoder::ProcessFlush();
}

AVCodecID FFmpegVideoDecoder<LIBAV_VER>::GetCodecId(
    const nsACString& aMimeType) {
  if (MP4Decoder::IsH264(aMimeType)) {
    return AV_CODEC_ID_H264;
  }

  if (aMimeType.EqualsLiteral("video/x-vnd.on2.vp6")) {
    return AV_CODEC_ID_VP6F;
  }

#if LIBAVCODEC_VERSION_MAJOR >= 54
  if (VPXDecoder::IsVP8(aMimeType)) {
    return AV_CODEC_ID_VP8;
  }
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 55
  if (VPXDecoder::IsVP9(aMimeType)) {
    return AV_CODEC_ID_VP9;
  }
#endif

  return AV_CODEC_ID_NONE;
}

void FFmpegVideoDecoder<LIBAV_VER>::ProcessShutdown() {
#ifdef MOZ_WAYLAND_USE_VAAPI
  if (mVAAPIDeviceContext) {
    mLib->av_buffer_unref(&mVAAPIDeviceContext);
  }
#endif
  FFmpegDataDecoder<LIBAV_VER>::ProcessShutdown();
}

#ifdef MOZ_WAYLAND_USE_VAAPI
bool FFmpegVideoDecoder<LIBAV_VER>::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
  return !!mVAAPIDeviceContext;
}
#endif

}  // namespace mozilla
