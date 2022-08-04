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
#include "VideoUtils.h"
#include "VPXDecoder.h"
#include "mozilla/layers/KnowsCompositor.h"
#if LIBAVCODEC_VERSION_MAJOR >= 57
#  include "mozilla/layers/TextureClient.h"
#endif
#if LIBAVCODEC_VERSION_MAJOR >= 58
#  include "mozilla/ProfilerMarkers.h"
#endif
#ifdef MOZ_WAYLAND_USE_VAAPI
#  include "H264.h"
#  include "mozilla/layers/DMABUFSurfaceImage.h"
#  include "mozilla/widget/DMABufLibWrapper.h"
#  include "FFmpegVideoFramePool.h"
#  include "va/va.h"
#endif

#if defined(MOZ_AV1) && defined(MOZ_WAYLAND) && \
    (defined(FFVPX_VERSION) || LIBAVCODEC_VERSION_MAJOR >= 59)
#  define FFMPEG_AV1_DECODE 1
#  include "AOMDecoder.h"
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
#  define AV_PIX_FMT_GBRP PIX_FMT_GBRP
#  define AV_PIX_FMT_NONE PIX_FMT_NONE
#endif
#if LIBAVCODEC_VERSION_MAJOR > 58
#  define AV_PIX_FMT_VAAPI_VLD AV_PIX_FMT_VAAPI
#endif
#include "mozilla/PodOperations.h"
#include "mozilla/StaticPrefs_media.h"
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
// Defines number of delayed frames until we switch back to SW decode.
#define HW_DECODE_LATE_FRAMES 15

#if LIBAVCODEC_VERSION_MAJOR >= 57 && LIBAVUTIL_VERSION_MAJOR >= 56
#  define CUSTOMIZED_BUFFER_ALLOCATION 1
#endif

typedef mozilla::layers::Image Image;
typedef mozilla::layers::PlanarYCbCrImage PlanarYCbCrImage;

namespace mozilla {

#ifdef MOZ_WAYLAND_USE_VAAPI
nsTArray<AVCodecID> FFmpegVideoDecoder<LIBAV_VER>::mAcceleratedFormats;
#endif

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
      case AV_PIX_FMT_GBRP:
        FFMPEG_LOG("Requesting pixel format GBRP.");
        return AV_PIX_FMT_GBRP;
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
        FFMPEG_LOG("Requesting pixel format VAAPI_VLD");
        return AV_PIX_FMT_VAAPI_VLD;
      default:
        break;
    }
  }

  NS_WARNING("FFmpeg does not share any supported pixel formats.");
  return AV_PIX_FMT_NONE;
}

AVCodec* FFmpegVideoDecoder<LIBAV_VER>::FindVAAPICodec() {
  AVCodec* decoder = FindHardwareAVCodec(mLib, mCodecID);
  if (!decoder) {
    FFMPEG_LOG("  We're missing hardware accelerated decoder");
    return nullptr;
  }
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

  FFMPEG_LOG("  HW Decoder does not support VAAPI device type");
  return nullptr;
}

template <int V>
class VAAPIDisplayHolder {};

template <>
class VAAPIDisplayHolder<LIBAV_VER>;

template <>
class VAAPIDisplayHolder<LIBAV_VER> {
 public:
  VAAPIDisplayHolder(FFmpegLibWrapper* aLib, VADisplay aDisplay)
      : mLib(aLib), mDisplay(aDisplay){};
  ~VAAPIDisplayHolder() { mLib->vaTerminate(mDisplay); }

 private:
  FFmpegLibWrapper* mLib;
  VADisplay mDisplay;
};

static void VAAPIDisplayReleaseCallback(struct AVHWDeviceContext* hwctx) {
  auto displayHolder =
      static_cast<VAAPIDisplayHolder<LIBAV_VER>*>(hwctx->user_opaque);
  delete displayHolder;
}

bool FFmpegVideoDecoder<LIBAV_VER>::CreateVAAPIDeviceContext() {
  mVAAPIDeviceContext = mLib->av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
  if (!mVAAPIDeviceContext) {
    FFMPEG_LOG("  av_hwdevice_ctx_alloc failed.");
    return false;
  }

  auto releaseVAAPIcontext =
      MakeScopeExit([&] { mLib->av_buffer_unref(&mVAAPIDeviceContext); });

  AVHWDeviceContext* hwctx = (AVHWDeviceContext*)mVAAPIDeviceContext->data;
  AVVAAPIDeviceContext* vactx = (AVVAAPIDeviceContext*)hwctx->hwctx;

  mDisplay = mLib->vaGetDisplayDRM(widget::GetDMABufDevice()->GetDRMFd());
  if (!mDisplay) {
    FFMPEG_LOG("  Can't get DRM VA-API display.");
    return false;
  }

  hwctx->user_opaque = new VAAPIDisplayHolder<LIBAV_VER>(mLib, mDisplay);
  hwctx->free = VAAPIDisplayReleaseCallback;

  int major, minor;
  int status = mLib->vaInitialize(mDisplay, &major, &minor);
  if (status != VA_STATUS_SUCCESS) {
    FFMPEG_LOG("  vaInitialize failed.");
    return false;
  }

  vactx->display = mDisplay;
  if (mLib->av_hwdevice_ctx_init(mVAAPIDeviceContext) < 0) {
    FFMPEG_LOG("  av_hwdevice_ctx_init failed.");
    return false;
  }

  mCodecContext->hw_device_ctx = mLib->av_buffer_ref(mVAAPIDeviceContext);
  releaseVAAPIcontext.release();
  return true;
}

MediaResult FFmpegVideoDecoder<LIBAV_VER>::InitVAAPIDecoder() {
  FFMPEG_LOG("Initialising VA-API FFmpeg decoder");

  StaticMutexAutoLock mon(sMutex);

  // mAcceleratedFormats is already configured so check supported
  // formats before we do anything.
  if (mAcceleratedFormats.Length()) {
    if (!IsFormatAccelerated(mCodecID)) {
      FFMPEG_LOG("  Format %s is not accelerated",
                 mLib->avcodec_get_name(mCodecID));
      return NS_ERROR_NOT_AVAILABLE;
    } else {
      FFMPEG_LOG("  Format %s is accelerated",
                 mLib->avcodec_get_name(mCodecID));
    }
  }

  if (!mLib->IsVAAPIAvailable()) {
    FFMPEG_LOG("  libva library or symbols are missing.");
    return NS_ERROR_NOT_AVAILABLE;
  }

  AVCodec* codec = FindVAAPICodec();
  if (!codec) {
    FFMPEG_LOG("  couldn't find ffmpeg VA-API decoder");
    return NS_ERROR_DOM_MEDIA_FATAL_ERR;
  }
  FFMPEG_LOG("  codec %s : %s", codec->name, codec->long_name);

  if (!(mCodecContext = mLib->avcodec_alloc_context3(codec))) {
    FFMPEG_LOG("  couldn't init VA-API ffmpeg context");
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mCodecContext->opaque = this;

  InitVAAPICodecContext();

  auto releaseVAAPIdecoder = MakeScopeExit([&] {
    if (mVAAPIDeviceContext) {
      mLib->av_buffer_unref(&mVAAPIDeviceContext);
    }
    if (mCodecContext) {
      mLib->av_freep(&mCodecContext);
    }
  });

  if (!CreateVAAPIDeviceContext()) {
    mLib->av_freep(&mCodecContext);
    FFMPEG_LOG("  Failed to create VA-API device context");
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
    FFMPEG_LOG("  Couldn't initialise VA-API decoder");
    return NS_ERROR_DOM_MEDIA_FATAL_ERR;
  }

  if (mAcceleratedFormats.IsEmpty()) {
    mAcceleratedFormats = GetAcceleratedFormats();
    if (!IsFormatAccelerated(mCodecID)) {
      FFMPEG_LOG("  Format %s is not accelerated",
                 mLib->avcodec_get_name(mCodecID));
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  FFMPEG_LOG("  VA-API FFmpeg init successful");
  releaseVAAPIdecoder.release();
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

#ifdef MOZ_WAYLAND_USE_VAAPI
void FFmpegVideoDecoder<LIBAV_VER>::InitHWDecodingPrefs() {
  if (!mEnableHardwareDecoding) {
    FFMPEG_LOG("VAAPI is disabled by parent decoder module.");
    return;
  }
  bool isHardwareWebRenderUsed = mImageAllocator &&
                                 (mImageAllocator->GetCompositorBackendType() ==
                                  layers::LayersBackend::LAYERS_WR) &&
                                 !mImageAllocator->UsingSoftwareWebRender();
  if (!isHardwareWebRenderUsed) {
    mEnableHardwareDecoding = false;
    FFMPEG_LOG("Hardware WebRender is off, VAAPI is disabled");
    return;
  }
  if (!XRE_IsRDDProcess()) {
    mEnableHardwareDecoding = false;
    FFMPEG_LOG("VA-API works in RDD process only");
  }
}
#endif

FFmpegVideoDecoder<LIBAV_VER>::FFmpegVideoDecoder(
    FFmpegLibWrapper* aLib, const VideoInfo& aConfig,
    KnowsCompositor* aAllocator, ImageContainer* aImageContainer,
    bool aLowLatency, bool aDisableHardwareDecoding)
    : FFmpegDataDecoder(aLib, GetCodecId(aConfig.mMimeType)),
#ifdef MOZ_WAYLAND_USE_VAAPI
      mVAAPIDeviceContext(nullptr),
      mEnableHardwareDecoding(!aDisableHardwareDecoding),
      mDisplay(nullptr),
#endif
      mImageAllocator(aAllocator),
      mImageContainer(aImageContainer),
      mInfo(aConfig),
      mDecodedFrames(0),
#if LIBAVCODEC_VERSION_MAJOR >= 58
      mDecodedFramesLate(0),
      mMissedDecodeInAverangeTime(0),
#endif
      mAverangeDecodeTime(0),
      mLowLatency(aLowLatency) {
  FFMPEG_LOG("FFmpegVideoDecoder::FFmpegVideoDecoder MIME %s Codec ID %d",
             aConfig.mMimeType.get(), mCodecID);
  // Use a new MediaByteBuffer as the object will be modified during
  // initialization.
  mExtraData = new MediaByteBuffer;
  mExtraData->AppendElements(*aConfig.mExtraData);
#ifdef MOZ_WAYLAND_USE_VAAPI
  InitHWDecodingPrefs();
#endif
}

FFmpegVideoDecoder<LIBAV_VER>::~FFmpegVideoDecoder() {
#ifdef CUSTOMIZED_BUFFER_ALLOCATION
  MOZ_DIAGNOSTIC_ASSERT(mAllocatedImages.IsEmpty(),
                        "Should release all shmem buffers before destroy!");
#endif
}

RefPtr<MediaDataDecoder::InitPromise> FFmpegVideoDecoder<LIBAV_VER>::Init() {
  MediaResult rv;

#ifdef MOZ_WAYLAND_USE_VAAPI
  if (mEnableHardwareDecoding) {
    rv = InitVAAPIDecoder();
    if (NS_SUCCEEDED(rv)) {
      return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
    }
    mEnableHardwareDecoding = false;
  }
#endif

  rv = InitDecoder();
  if (NS_SUCCEEDED(rv)) {
    return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
  }

  return InitPromise::CreateAndReject(rv, __func__);
}

#ifdef CUSTOMIZED_BUFFER_ALLOCATION
static int GetVideoBufferWrapper(struct AVCodecContext* aCodecContext,
                                 AVFrame* aFrame, int aFlags) {
  auto* decoder =
      static_cast<FFmpegVideoDecoder<LIBAV_VER>*>(aCodecContext->opaque);
  int rv = decoder->GetVideoBuffer(aCodecContext, aFrame, aFlags);
  return rv < 0 ? decoder->GetVideoBufferDefault(aCodecContext, aFrame, aFlags)
                : rv;
}

static void ReleaseVideoBufferWrapper(void* opaque, uint8_t* data) {
  if (opaque) {
    FFMPEG_LOGV("ReleaseVideoBufferWrapper: PlanarYCbCrImage=%p", opaque);
    RefPtr<ImageBufferWrapper> image = static_cast<ImageBufferWrapper*>(opaque);
    image->ReleaseBuffer();
  }
}

static gfx::YUVColorSpace TransferAVColorSpaceToYUVColorSpace(
    AVColorSpace aSpace) {
  switch (aSpace) {
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
      return gfx::YUVColorSpace::BT2020;
    case AVCOL_SPC_BT709:
      return gfx::YUVColorSpace::BT709;
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_BT470BG:
      return gfx::YUVColorSpace::BT601;
    default:
      return gfx::YUVColorSpace::Default;
  }
}

static bool IsColorFormatSupportedForUsingCustomizedBuffer(
    const AVPixelFormat& aFormat) {
#  if XP_WIN
  // Currently the web render doesn't support uploading R16 surface, so we can't
  // use the shmem texture for 10 bit+ videos which would be uploaded by the
  // web render. See Bug 1751498.
  return aFormat == AV_PIX_FMT_YUV420P || aFormat == AV_PIX_FMT_YUVJ420P ||
         aFormat == AV_PIX_FMT_YUV444P;
#  else
  // For now, we only support for YUV420P, YUVJ420P and YUV444 which are the
  // only non-HW accelerated format supported by FFmpeg's H264 and VP9 decoder.
  return aFormat == AV_PIX_FMT_YUV420P || aFormat == AV_PIX_FMT_YUVJ420P ||
         aFormat == AV_PIX_FMT_YUV420P10LE ||
         aFormat == AV_PIX_FMT_YUV420P12LE || aFormat == AV_PIX_FMT_YUV444P ||
         aFormat == AV_PIX_FMT_YUV444P10LE || aFormat == AV_PIX_FMT_YUV444P12LE;
#  endif
}

static gfx::ColorDepth GetColorDepth(const AVPixelFormat& aFormat) {
  switch (aFormat) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUV444P:
      return gfx::ColorDepth::COLOR_8;
    case AV_PIX_FMT_YUV420P10LE:
    case AV_PIX_FMT_YUV422P10LE:
    case AV_PIX_FMT_YUV444P10LE:
      return gfx::ColorDepth::COLOR_10;
    case AV_PIX_FMT_YUV420P12LE:
    case AV_PIX_FMT_YUV422P12LE:
    case AV_PIX_FMT_YUV444P12LE:
      return gfx::ColorDepth::COLOR_12;
    default:
      MOZ_ASSERT_UNREACHABLE("Not supported format?");
      return gfx::ColorDepth::COLOR_8;
  }
}

static bool IsYUV420Sampling(const AVPixelFormat& aFormat) {
  return aFormat == AV_PIX_FMT_YUV420P || aFormat == AV_PIX_FMT_YUVJ420P ||
         aFormat == AV_PIX_FMT_YUV420P10LE || aFormat == AV_PIX_FMT_YUV420P12LE;
}

layers::TextureClient*
FFmpegVideoDecoder<LIBAV_VER>::AllocateTextureClientForImage(
    struct AVCodecContext* aCodecContext, PlanarYCbCrImage* aImage) {
  MOZ_ASSERT(
      IsColorFormatSupportedForUsingCustomizedBuffer(aCodecContext->pix_fmt));

  // FFmpeg will store images with color depth > 8 bits in 16 bits with extra
  // padding.
  const int32_t bytesPerChannel =
      GetColorDepth(aCodecContext->pix_fmt) == gfx::ColorDepth::COLOR_8 ? 1 : 2;

  // If adjusted Ysize is larger than the actual image size (coded_width *
  // coded_height), that means ffmpeg decoder needs extra padding on both width
  // and height. If that happens, the planes will need to be cropped later in
  // order to avoid visible incorrect border on the right and bottom of the
  // actual image.
  //
  // Here are examples of various sizes video in YUV420P format, the width and
  // height would need to be adjusted in order to align padding.
  //
  // Eg1. video (1920*1080)
  // plane Y
  // width 1920 height 1080 -> adjusted-width 1920 adjusted-height 1088
  // plane Cb/Cr
  // width 960  height  540 -> adjusted-width 1024 adjusted-height 544
  //
  // Eg2. video (2560*1440)
  // plane Y
  // width 2560 height 1440 -> adjusted-width 2560 adjusted-height 1440
  // plane Cb/Cr
  // width 1280 height  720 -> adjusted-width 1280 adjusted-height 736
  layers::PlanarYCbCrData data;
  const auto yDims =
      gfx::IntSize{aCodecContext->coded_width, aCodecContext->coded_height};
  auto paddedYSize = yDims;
  mLib->avcodec_align_dimensions(aCodecContext, &paddedYSize.width,
                                 &paddedYSize.height);
  data.mYStride = paddedYSize.Width() * bytesPerChannel;

  MOZ_ASSERT(
      IsColorFormatSupportedForUsingCustomizedBuffer(aCodecContext->pix_fmt));
  auto uvDims = yDims;
  if (IsYUV420Sampling(aCodecContext->pix_fmt)) {
    uvDims.width = (uvDims.width + 1) / 2;
    uvDims.height = (uvDims.height + 1) / 2;
    data.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;
  }
  auto paddedCbCrSize = uvDims;
  mLib->avcodec_align_dimensions(aCodecContext, &paddedCbCrSize.width,
                                 &paddedCbCrSize.height);
  data.mCbCrStride = paddedCbCrSize.Width() * bytesPerChannel;

  // Setting other attributes
  data.mPictureRect = gfx::IntRect(
      mInfo.ScaledImageRect(aCodecContext->width, aCodecContext->height)
          .TopLeft(),
      gfx::IntSize(aCodecContext->width, aCodecContext->height));
  data.mStereoMode = mInfo.mStereoMode;
  if (aCodecContext->colorspace != AVCOL_SPC_UNSPECIFIED) {
    data.mYUVColorSpace =
        TransferAVColorSpaceToYUVColorSpace(aCodecContext->colorspace);
  } else {
    data.mYUVColorSpace = mInfo.mColorSpace
                              ? *mInfo.mColorSpace
                              : DefaultColorSpace(data.mPictureRect.Size());
  }
  data.mColorDepth = GetColorDepth(aCodecContext->pix_fmt);
  data.mColorRange = aCodecContext->color_range == AVCOL_RANGE_JPEG
                         ? gfx::ColorRange::FULL
                         : gfx::ColorRange::LIMITED;

  FFMPEG_LOGV(
      "Created plane data, YSize=(%d, %d), CbCrSize=(%d, %d), "
      "CroppedYSize=(%d, %d), CroppedCbCrSize=(%d, %d), ColorDepth=%hhu",
      paddedYSize.Width(), paddedYSize.Height(), paddedCbCrSize.Width(),
      paddedCbCrSize.Height(), data.YPictureSize().Width(),
      data.YPictureSize().Height(), data.CbCrPictureSize().Width(),
      data.CbCrPictureSize().Height(), static_cast<uint8_t>(data.mColorDepth));

  // Allocate a shmem buffer for image.
  if (!aImage->CreateEmptyBuffer(data, paddedYSize, paddedCbCrSize)) {
    return nullptr;
  }
  return aImage->GetTextureClient(mImageAllocator);
}

int FFmpegVideoDecoder<LIBAV_VER>::GetVideoBuffer(
    struct AVCodecContext* aCodecContext, AVFrame* aFrame, int aFlags) {
  FFMPEG_LOGV("GetVideoBuffer: aCodecContext=%p aFrame=%p", aCodecContext,
              aFrame);
  if (!StaticPrefs::media_ffmpeg_customized_buffer_allocation()) {
    return AVERROR(EINVAL);
  }

  if (mIsUsingShmemBufferForDecode && !*mIsUsingShmemBufferForDecode) {
    return AVERROR(EINVAL);
  }

  // Codec doesn't support custom allocator.
  if (!(aCodecContext->codec->capabilities & AV_CODEC_CAP_DR1)) {
    return AVERROR(EINVAL);
  }

  // Pre-allocation is only for sw decoding. During decoding, ffmpeg decoder
  // will need to reference decoded frames, if those frames are on shmem buffer,
  // then it would cause a need to read CPU data from GPU, which is slow.
  if (IsHardwareAccelerated()) {
    return AVERROR(EINVAL);
  }

  if (!IsColorFormatSupportedForUsingCustomizedBuffer(aCodecContext->pix_fmt)) {
    FFMPEG_LOG("Not support color format %d", aCodecContext->pix_fmt);
    return AVERROR(EINVAL);
  }

  if (aCodecContext->lowres != 0) {
    FFMPEG_LOG("Not support low resolution decoding");
    return AVERROR(EINVAL);
  }

  const gfx::IntSize size(aCodecContext->width, aCodecContext->height);
  int rv = mLib->av_image_check_size(size.Width(), size.Height(), 0, nullptr);
  if (rv < 0) {
    FFMPEG_LOG("Invalid image size");
    return rv;
  }

  CheckedInt32 dataSize = mLib->av_image_get_buffer_size(
      aCodecContext->pix_fmt, aCodecContext->coded_width,
      aCodecContext->coded_height, 16);
  if (!dataSize.isValid()) {
    FFMPEG_LOG("Data size overflow!");
    return AVERROR(EINVAL);
  }

  if (!mImageContainer) {
    FFMPEG_LOG("No Image container!");
    return AVERROR(EINVAL);
  }

  RefPtr<PlanarYCbCrImage> image = mImageContainer->CreatePlanarYCbCrImage();
  if (!image) {
    FFMPEG_LOG("Failed to create YCbCr image");
    return AVERROR(EINVAL);
  }

  RefPtr<layers::TextureClient> texture =
      AllocateTextureClientForImage(aCodecContext, image);
  if (!texture) {
    FFMPEG_LOG("Failed to allocate a texture client");
    return AVERROR(EINVAL);
  }

  if (!texture->Lock(layers::OpenMode::OPEN_WRITE)) {
    FFMPEG_LOG("Failed to lock the texture");
    return AVERROR(EINVAL);
  }
  auto autoUnlock = MakeScopeExit([&] { texture->Unlock(); });

  layers::MappedYCbCrTextureData mapped;
  if (!texture->BorrowMappedYCbCrData(mapped)) {
    FFMPEG_LOG("Failed to borrow mapped data for the texture");
    return AVERROR(EINVAL);
  }

  aFrame->data[0] = mapped.y.data;
  aFrame->data[1] = mapped.cb.data;
  aFrame->data[2] = mapped.cr.data;

  aFrame->linesize[0] = mapped.y.stride;
  aFrame->linesize[1] = mapped.cb.stride;
  aFrame->linesize[2] = mapped.cr.stride;

  aFrame->width = aCodecContext->coded_width;
  aFrame->height = aCodecContext->coded_height;
  aFrame->format = aCodecContext->pix_fmt;
  aFrame->extended_data = aFrame->data;
  aFrame->reordered_opaque = aCodecContext->reordered_opaque;
  MOZ_ASSERT(aFrame->data[0] && aFrame->data[1] && aFrame->data[2]);

  // This will hold a reference to image, and the reference would be dropped
  // when ffmpeg tells us that the buffer is no longer needed.
  auto imageWrapper = MakeRefPtr<ImageBufferWrapper>(image.get(), this);
  aFrame->buf[0] =
      mLib->av_buffer_create(aFrame->data[0], dataSize.value(),
                             ReleaseVideoBufferWrapper, imageWrapper.get(), 0);
  if (!aFrame->buf[0]) {
    FFMPEG_LOG("Failed to allocate buffer");
    return AVERROR(EINVAL);
  }

  FFMPEG_LOG("Created av buffer, buf=%p, data=%p, image=%p, sz=%d",
             aFrame->buf[0], aFrame->data[0], imageWrapper.get(),
             dataSize.value());
  mAllocatedImages.Insert(imageWrapper.get());
  mIsUsingShmemBufferForDecode = Some(true);
  return 0;
}
#endif

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
#ifdef CUSTOMIZED_BUFFER_ALLOCATION
  FFMPEG_LOG("Set get_buffer2 for customized buffer allocation");
  mCodecContext->get_buffer2 = GetVideoBufferWrapper;
  mCodecContext->opaque = this;
#  if FF_API_THREAD_SAFE_CALLBACKS
  mCodecContext->thread_safe_callbacks = 1;
#  endif
#endif
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
  if (mLowLatency) {
    mCodecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
  }
}
#endif

static int64_t GetFramePts(AVFrame* aFrame) {
#if LIBAVCODEC_VERSION_MAJOR > 57
  return aFrame->pts;
#else
  return aFrame->pkt_pts;
#endif
}

void FFmpegVideoDecoder<LIBAV_VER>::UpdateDecodeTimes(TimeStamp aDecodeStart) {
  mDecodedFrames++;
  float decodeTime = (TimeStamp::Now() - aDecodeStart).ToMilliseconds();
  mAverangeDecodeTime =
      (mAverangeDecodeTime * (mDecodedFrames - 1) + decodeTime) /
      mDecodedFrames;
  FFMPEG_LOG(
      "Frame decode finished, time %.2f ms averange decode time %.2f ms "
      "decoded %d frames\n",
      decodeTime, mAverangeDecodeTime, mDecodedFrames);
#if LIBAVCODEC_VERSION_MAJOR >= 58
  if (mFrame->pkt_duration > 0) {
    // Switch frame duration to ms
    float frameDuration = mFrame->pkt_duration / 1000.0f;
    if (frameDuration < decodeTime) {
      PROFILER_MARKER_TEXT("FFmpegVideoDecoder::DoDecode", MEDIA_PLAYBACK, {},
                           "frame decode takes too long");
      mDecodedFramesLate++;
      if (frameDuration < mAverangeDecodeTime) {
        mMissedDecodeInAverangeTime++;
      }
      FFMPEG_LOG(
          "  slow decode: failed to decode in time, frame duration %.2f ms, "
          "decode time %.2f\n",
          frameDuration, decodeTime);
      FFMPEG_LOG("  frames: all decoded %d late decoded %d over averange %d\n",
                 mDecodedFrames, mDecodedFramesLate,
                 mMissedDecodeInAverangeTime);
    }
  }
#endif
}

MediaResult FFmpegVideoDecoder<LIBAV_VER>::DoDecode(
    MediaRawData* aSample, uint8_t* aData, int aSize, bool* aGotFrame,
    MediaDataDecoder::DecodedData& aResults) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  AVPacket packet;
  mLib->av_init_packet(&packet);

  TimeStamp decodeStart = TimeStamp::Now();

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
    char errStr[AV_ERROR_MAX_STRING_SIZE];
    mLib->av_strerror(res, errStr, AV_ERROR_MAX_STRING_SIZE);
    FFMPEG_LOG("avcodec_send_packet error: %s", errStr);
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("avcodec_send_packet error: %s", errStr));
  }
  if (aGotFrame) {
    *aGotFrame = false;
  }
  do {
    if (!PrepareFrame()) {
      NS_WARNING("FFmpeg decoder failed to allocate frame.");
      return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
    }

#  ifdef MOZ_WAYLAND_USE_VAAPI
    // Create VideoFramePool in case we need it.
    if (!mVideoFramePool && mEnableHardwareDecoding) {
      mVideoFramePool = MakeUnique<VideoFramePool<LIBAV_VER>>();
    }

    // Release unused VA-API surfaces before avcodec_receive_frame() as
    // ffmpeg recycles VASurface for HW decoding.
    if (mVideoFramePool) {
      mVideoFramePool->ReleaseUnusedVAAPIFrames();
    }
#  endif

    res = mLib->avcodec_receive_frame(mCodecContext, mFrame);
    if (res == int(AVERROR_EOF)) {
      FFMPEG_LOG("  End of stream.");
      return NS_ERROR_DOM_MEDIA_END_OF_STREAM;
    }
    if (res == AVERROR(EAGAIN)) {
      return NS_OK;
    }
    if (res < 0) {
      char errStr[AV_ERROR_MAX_STRING_SIZE];
      mLib->av_strerror(res, errStr, AV_ERROR_MAX_STRING_SIZE);
      FFMPEG_LOG("  avcodec_receive_frame error: %s", errStr);
      return MediaResult(
          NS_ERROR_DOM_MEDIA_DECODE_ERR,
          RESULT_DETAIL("avcodec_receive_frame error: %s", errStr));
    }

    UpdateDecodeTimes(decodeStart);
    decodeStart = TimeStamp::Now();

    MediaResult rv;
#  ifdef MOZ_WAYLAND_USE_VAAPI
    if (IsHardwareAccelerated()) {
      if (mMissedDecodeInAverangeTime > HW_DECODE_LATE_FRAMES) {
        PROFILER_MARKER_TEXT("FFmpegVideoDecoder::DoDecode", MEDIA_PLAYBACK, {},
                             "Fallback to SW decode");
        FFMPEG_LOG("  HW decoding is slow, switch back to SW decode");
        return MediaResult(
            NS_ERROR_DOM_MEDIA_DECODE_ERR,
            RESULT_DETAIL("HW decoding is slow, switch back to SW decode"));
      }
      rv = CreateImageVAAPI(mFrame->pkt_pos, GetFramePts(mFrame),
                            mFrame->pkt_duration, aResults);
      // If VA-API playback failed, just quit. Decoder is going to be restarted
      // without VA-API.
      if (NS_FAILED(rv)) {
        // Explicitly remove dmabuf surface pool as it's configured
        // for VA-API support.
        mVideoFramePool = nullptr;
        return rv;
      }
    } else
#  endif
    {
      rv = CreateImage(mFrame->pkt_pos, GetFramePts(mFrame),
                       mFrame->pkt_duration, aResults);
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
    NS_WARNING("FFmpeg decoder failed to allocate frame.");
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
      "opaque(%" PRId64 ") pts(%" PRId64 ") pkt_dts(%" PRId64 "))",
      bytesConsumed, decoded, packet.pts, packet.dts, mFrame->pts,
      mFrame->reordered_opaque, mFrame->pts, mFrame->pkt_dts);

  if (bytesConsumed < 0) {
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("FFmpeg video error: %d", bytesConsumed));
  }

  if (!decoded) {
    if (aGotFrame) {
      *aGotFrame = false;
    }
    return NS_OK;
  }

  UpdateDecodeTimes(decodeStart);

  // If we've decoded a frame then we need to output it
  int64_t pts =
      mPtsContext.GuessCorrectPts(GetFramePts(mFrame), mFrame->pkt_dts);
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

gfx::YUVColorSpace FFmpegVideoDecoder<LIBAV_VER>::GetFrameColorSpace() const {
  AVColorSpace colorSpace = AVCOL_SPC_UNSPECIFIED;
#if LIBAVCODEC_VERSION_MAJOR > 58
  colorSpace = mFrame->colorspace;
#else
  if (mLib->av_frame_get_colorspace) {
    colorSpace = (AVColorSpace)mLib->av_frame_get_colorspace(mFrame);
  }
#endif
  switch (colorSpace) {
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
    case AVCOL_SPC_RGB:
      return gfx::YUVColorSpace::Identity;
    default:
      return DefaultColorSpace({mFrame->width, mFrame->height});
  }
}

gfx::ColorRange FFmpegVideoDecoder<LIBAV_VER>::GetFrameColorRange() const {
  AVColorRange range = AVCOL_RANGE_UNSPECIFIED;
#if LIBAVCODEC_VERSION_MAJOR > 58
  range = mFrame->color_range;
#else
  if (mLib->av_frame_get_color_range) {
    range = (AVColorRange)mLib->av_frame_get_color_range(mFrame);
  }
#endif
  return range == AVCOL_RANGE_JPEG ? gfx::ColorRange::FULL
                                   : gfx::ColorRange::LIMITED;
}

MediaResult FFmpegVideoDecoder<LIBAV_VER>::CreateImage(
    int64_t aOffset, int64_t aPts, int64_t aDuration,
    MediaDataDecoder::DecodedData& aResults) const {
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

  b.mPlanes[0].mSkip = 0;
  b.mPlanes[1].mSkip = 0;
  b.mPlanes[2].mSkip = 0;

  b.mPlanes[0].mWidth = mFrame->width;
  b.mPlanes[0].mHeight = mFrame->height;
  if (mCodecContext->pix_fmt == AV_PIX_FMT_YUV444P ||
      mCodecContext->pix_fmt == AV_PIX_FMT_YUV444P10LE ||
      mCodecContext->pix_fmt == AV_PIX_FMT_GBRP
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
    b.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH;
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
    b.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;
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
  b.mColorRange = GetFrameColorRange();

  RefPtr<VideoData> v;
#ifdef CUSTOMIZED_BUFFER_ALLOCATION
  bool requiresCopy = false;
#  ifdef XP_MACOSX
  // Bug 1765388: macOS needs to generate a MacIOSurfaceImage in order to
  // properly display HDR video. The later call to ::CreateAndCopyData does
  // that. If this shared memory buffer path also generated a
  // MacIOSurfaceImage, then we could use it for HDR.
  requiresCopy = (b.mColorDepth != gfx::ColorDepth::COLOR_8);
#  endif
  if (mIsUsingShmemBufferForDecode && *mIsUsingShmemBufferForDecode &&
      !requiresCopy) {
    RefPtr<ImageBufferWrapper> wrapper = static_cast<ImageBufferWrapper*>(
        mLib->av_buffer_get_opaque(mFrame->buf[0]));
    MOZ_ASSERT(wrapper);
    FFMPEG_LOGV("Create a video data from a shmem image=%p", wrapper.get());
    v = VideoData::CreateFromImage(
        mInfo.mDisplay, aOffset, TimeUnit::FromMicroseconds(aPts),
        TimeUnit::FromMicroseconds(aDuration), wrapper->AsImage(),
        !!mFrame->key_frame, TimeUnit::FromMicroseconds(-1));
  }
#endif
  if (!v) {
    v = VideoData::CreateAndCopyData(
        mInfo, mImageContainer, aOffset, TimeUnit::FromMicroseconds(aPts),
        TimeUnit::FromMicroseconds(aDuration), b, !!mFrame->key_frame,
        TimeUnit::FromMicroseconds(-1),
        mInfo.ScaledImageRect(mFrame->width, mFrame->height), mImageAllocator);
  }

  if (!v) {
    return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                       RESULT_DETAIL("image allocation error"));
  }
  aResults.AppendElement(std::move(v));
  return NS_OK;
}

#ifdef MOZ_WAYLAND_USE_VAAPI
bool FFmpegVideoDecoder<LIBAV_VER>::GetVAAPISurfaceDescriptor(
    VADRMPRIMESurfaceDescriptor* aVaDesc) {
  VASurfaceID surface_id = (VASurfaceID)(uintptr_t)mFrame->data[3];
  VAStatus vas = mLib->vaExportSurfaceHandle(
      mDisplay, surface_id, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
      VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS, aVaDesc);
  if (vas != VA_STATUS_SUCCESS) {
    return false;
  }
  vas = mLib->vaSyncSurface(mDisplay, surface_id);
  if (vas != VA_STATUS_SUCCESS) {
    NS_WARNING("vaSyncSurface() failed.");
  }
  return true;
}

MediaResult FFmpegVideoDecoder<LIBAV_VER>::CreateImageVAAPI(
    int64_t aOffset, int64_t aPts, int64_t aDuration,
    MediaDataDecoder::DecodedData& aResults) {
  FFMPEG_LOG("VA-API Got one frame output with pts=%" PRId64 " dts=%" PRId64
             " duration=%" PRId64 " opaque=%" PRId64,
             aPts, mFrame->pkt_dts, aDuration, mCodecContext->reordered_opaque);

  VADRMPRIMESurfaceDescriptor vaDesc;
  if (!GetVAAPISurfaceDescriptor(&vaDesc)) {
    return MediaResult(
        NS_ERROR_DOM_MEDIA_DECODE_ERR,
        RESULT_DETAIL("Unable to get frame by vaExportSurfaceHandle()"));
  }

  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  auto surface = mVideoFramePool->GetVideoFrameSurface(
      vaDesc, mFrame->width, mFrame->height, mCodecContext, mFrame, mLib);
  if (!surface) {
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("VAAPI dmabuf allocation error"));
  }
  surface->SetYUVColorSpace(GetFrameColorSpace());
  surface->SetColorRange(GetFrameColorRange());

  RefPtr<VideoData> vp = VideoData::CreateFromImage(
      mInfo.mDisplay, aOffset, TimeUnit::FromMicroseconds(aPts),
      TimeUnit::FromMicroseconds(aDuration), surface->GetAsImage(),
      !!mFrame->key_frame, TimeUnit::FromMicroseconds(-1));

  if (!vp) {
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("VAAPI image allocation error"));
  }

  aResults.AppendElement(std::move(vp));
  return NS_OK;
}
#endif

RefPtr<MediaDataDecoder::FlushPromise>
FFmpegVideoDecoder<LIBAV_VER>::ProcessFlush() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
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

#if defined(FFMPEG_AV1_DECODE)
  if (AOMDecoder::IsAV1(aMimeType)) {
    return AV_CODEC_ID_AV1;
  }
#endif

  return AV_CODEC_ID_NONE;
}

void FFmpegVideoDecoder<LIBAV_VER>::ProcessShutdown() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
#ifdef MOZ_WAYLAND_USE_VAAPI
  mVideoFramePool = nullptr;
  if (IsHardwareAccelerated()) {
    mLib->av_buffer_unref(&mVAAPIDeviceContext);
  }
#endif
  FFmpegDataDecoder<LIBAV_VER>::ProcessShutdown();
}

bool FFmpegVideoDecoder<LIBAV_VER>::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
#ifdef MOZ_WAYLAND_USE_VAAPI
  return !!mVAAPIDeviceContext;
#else
  return false;
#endif
}

#ifdef MOZ_WAYLAND_USE_VAAPI
bool FFmpegVideoDecoder<LIBAV_VER>::IsFormatAccelerated(
    AVCodecID aCodecID) const {
  for (const auto& format : mAcceleratedFormats) {
    if (format == aCodecID) {
      return true;
    }
  }
  return false;
}

// See ffmpeg / vaapi_decode.c how CodecID is mapped to VAProfile.
static const struct {
  enum AVCodecID codec_id;
  VAProfile va_profile;
  char name[100];
} vaapi_profile_map[] = {
#  define MAP(c, v, n) \
    { AV_CODEC_ID_##c, VAProfile##v, n }
    MAP(H264, H264ConstrainedBaseline, "H264ConstrainedBaseline"),
    MAP(H264, H264Main, "H264Main"),
    MAP(H264, H264High, "H264High"),
    MAP(VP8, VP8Version0_3, "VP8Version0_3"),
    MAP(VP9, VP9Profile0, "VP9Profile0"),
    MAP(VP9, VP9Profile2, "VP9Profile2"),
    MAP(AV1, AV1Profile0, "AV1Profile0"),
    MAP(AV1, AV1Profile1, "AV1Profile1"),
#  undef MAP
};

static AVCodecID VAProfileToCodecID(VAProfile aVAProfile) {
  for (const auto& profile : vaapi_profile_map) {
    if (profile.va_profile == aVAProfile) {
      return profile.codec_id;
    }
  }
  return AV_CODEC_ID_NONE;
}

static const char* VAProfileName(VAProfile aVAProfile) {
  for (const auto& profile : vaapi_profile_map) {
    if (profile.va_profile == aVAProfile) {
      return profile.name;
    }
  }
  return nullptr;
}

// This code is adopted from mpv project va-api routine
// determine_working_formats()
void FFmpegVideoDecoder<LIBAV_VER>::AddAcceleratedFormats(
    nsTArray<AVCodecID>& aCodecList, AVCodecID aCodecID,
    AVVAAPIHWConfig* hwconfig) {
  AVHWFramesConstraints* fc =
      mLib->av_hwdevice_get_hwframe_constraints(mVAAPIDeviceContext, hwconfig);
  if (!fc) {
    FFMPEG_LOG("    failed to retrieve libavutil frame constraints");
    return;
  }
  auto autoRelease =
      MakeScopeExit([&] { mLib->av_hwframe_constraints_free(&fc); });

  bool foundSupportedFormat = false;
  for (int n = 0;
       fc->valid_sw_formats && fc->valid_sw_formats[n] != AV_PIX_FMT_NONE;
       n++) {
#  ifdef MOZ_LOGGING
    char formatDesc[1000];
    FFMPEG_LOG("    codec %s format %s", mLib->avcodec_get_name(aCodecID),
               mLib->av_get_pix_fmt_string(formatDesc, sizeof(formatDesc),
                                           fc->valid_sw_formats[n]));
#  endif
    if (fc->valid_sw_formats[n] == AV_PIX_FMT_NV12 ||
        fc->valid_sw_formats[n] == AV_PIX_FMT_YUV420P) {
      foundSupportedFormat = true;
#  ifndef MOZ_LOGGING
      break;
#  endif
    }
  }

  if (!foundSupportedFormat) {
    FFMPEG_LOG("    %s target pixel format is not supported!",
               mLib->avcodec_get_name(aCodecID));
    return;
  }

  if (!aCodecList.Contains(aCodecID)) {
    aCodecList.AppendElement(aCodecID);
  }
}

nsTArray<AVCodecID> FFmpegVideoDecoder<LIBAV_VER>::GetAcceleratedFormats() {
  FFMPEG_LOG("FFmpegVideoDecoder::GetAcceleratedFormats()");

  VAProfile* profiles = nullptr;
  VAEntrypoint* entryPoints = nullptr;

  nsTArray<AVCodecID> supportedHWCodecs(AV_CODEC_ID_NONE);
#  ifdef MOZ_LOGGING
  auto printCodecs = MakeScopeExit([&] {
    FFMPEG_LOG("  Supported accelerated formats:");
    for (unsigned i = 0; i < supportedHWCodecs.Length(); i++) {
      FFMPEG_LOG("      %s", mLib->avcodec_get_name(supportedHWCodecs[i]));
    }
  });
#  endif

  AVVAAPIHWConfig* hwconfig =
      mLib->av_hwdevice_hwconfig_alloc(mVAAPIDeviceContext);
  if (!hwconfig) {
    FFMPEG_LOG("  failed to get AVVAAPIHWConfig");
    return supportedHWCodecs;
  }
  auto autoRelease = MakeScopeExit([&] {
    delete[] profiles;
    delete[] entryPoints;
    mLib->av_freep(&hwconfig);
  });

  int maxProfiles = vaMaxNumProfiles(mDisplay);
  int maxEntryPoints = vaMaxNumEntrypoints(mDisplay);
  if (MOZ_UNLIKELY(maxProfiles <= 0 || maxEntryPoints <= 0)) {
    return supportedHWCodecs;
  }

  profiles = new VAProfile[maxProfiles];
  int numProfiles = 0;
  VAStatus status = vaQueryConfigProfiles(mDisplay, profiles, &numProfiles);
  if (status != VA_STATUS_SUCCESS) {
    FFMPEG_LOG("  vaQueryConfigProfiles() failed %s", vaErrorStr(status));
    return supportedHWCodecs;
  }
  numProfiles = MIN(numProfiles, maxProfiles);

  entryPoints = new VAEntrypoint[maxEntryPoints];
  for (int p = 0; p < numProfiles; p++) {
    VAProfile profile = profiles[p];

    AVCodecID codecID = VAProfileToCodecID(profile);
    if (codecID == AV_CODEC_ID_NONE) {
      continue;
    }

    int numEntryPoints = 0;
    status = vaQueryConfigEntrypoints(mDisplay, profile, entryPoints,
                                      &numEntryPoints);
    if (status != VA_STATUS_SUCCESS) {
      FFMPEG_LOG("  vaQueryConfigEntrypoints() failed: '%s' for profile %d",
                 vaErrorStr(status), (int)profile);
      continue;
    }
    numEntryPoints = MIN(numEntryPoints, maxEntryPoints);

    FFMPEG_LOG("  Profile %s:", VAProfileName(profile));
    for (int e = 0; e < numEntryPoints; e++) {
      VAConfigID config = VA_INVALID_ID;
      status = vaCreateConfig(mDisplay, profile, entryPoints[e], nullptr, 0,
                              &config);
      if (status != VA_STATUS_SUCCESS) {
        FFMPEG_LOG("  vaCreateConfig() failed: '%s' for profile %d",
                   vaErrorStr(status), (int)profile);
        continue;
      }
      hwconfig->config_id = config;
      AddAcceleratedFormats(supportedHWCodecs, codecID, hwconfig);
      vaDestroyConfig(mDisplay, config);
    }
  }

  return supportedHWCodecs;
}

#endif

}  // namespace mozilla
