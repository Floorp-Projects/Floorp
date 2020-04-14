/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VPXDecoder.h"
#include "BitReader.h"
#include "TimeUnits.h"
#include "gfx2DGlue.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Unused.h"
#include "ImageContainer.h"
#include "nsError.h"
#include "prsystem.h"

#include <algorithm>

#undef LOG
#define LOG(arg, ...)                                                  \
  DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, "::%s: " arg, __func__, \
            ##__VA_ARGS__)

namespace mozilla {

using namespace gfx;
using namespace layers;

static VPXDecoder::Codec MimeTypeToCodec(const nsACString& aMimeType) {
  if (aMimeType.EqualsLiteral("video/vp8")) {
    return VPXDecoder::Codec::VP8;
  } else if (aMimeType.EqualsLiteral("video/vp9")) {
    return VPXDecoder::Codec::VP9;
  }
  return VPXDecoder::Codec::Unknown;
}

static nsresult InitContext(vpx_codec_ctx_t* aCtx, const VideoInfo& aInfo,
                            const VPXDecoder::Codec aCodec, bool aLowLatency) {
  int decode_threads = 2;

  vpx_codec_iface_t* dx = nullptr;
  if (aCodec == VPXDecoder::Codec::VP8) {
    dx = vpx_codec_vp8_dx();
  } else if (aCodec == VPXDecoder::Codec::VP9) {
    dx = vpx_codec_vp9_dx();
    if (aInfo.mDisplay.width >= 2048) {
      decode_threads = 8;
    } else if (aInfo.mDisplay.width >= 1024) {
      decode_threads = 4;
    }
  }
  decode_threads = std::min(decode_threads, PR_GetNumberOfProcessors());

  vpx_codec_dec_cfg_t config;
  config.threads = aLowLatency ? 1 : decode_threads;
  config.w = config.h = 0;  // set after decode

  if (!dx || vpx_codec_dec_init(aCtx, dx, &config, 0)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

VPXDecoder::VPXDecoder(const CreateDecoderParams& aParams)
    : mImageContainer(aParams.mImageContainer),
      mImageAllocator(aParams.mKnowsCompositor),
      mTaskQueue(aParams.mTaskQueue),
      mInfo(aParams.VideoConfig()),
      mCodec(MimeTypeToCodec(aParams.VideoConfig().mMimeType)),
      mLowLatency(
          aParams.mOptions.contains(CreateDecoderParams::Option::LowLatency)) {
  MOZ_COUNT_CTOR(VPXDecoder);
  PodZero(&mVPX);
  PodZero(&mVPXAlpha);
}

VPXDecoder::~VPXDecoder() { MOZ_COUNT_DTOR(VPXDecoder); }

RefPtr<ShutdownPromise> VPXDecoder::Shutdown() {
  RefPtr<VPXDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    vpx_codec_destroy(&self->mVPX);
    vpx_codec_destroy(&self->mVPXAlpha);
    return ShutdownPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<MediaDataDecoder::InitPromise> VPXDecoder::Init() {
  if (NS_FAILED(InitContext(&mVPX, mInfo, mCodec, mLowLatency))) {
    return VPXDecoder::InitPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }
  if (mInfo.HasAlpha()) {
    if (NS_FAILED(InitContext(&mVPXAlpha, mInfo, mCodec, mLowLatency))) {
      return VPXDecoder::InitPromise::CreateAndReject(
          NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    }
  }
  return VPXDecoder::InitPromise::CreateAndResolve(TrackInfo::kVideoTrack,
                                                   __func__);
}

RefPtr<MediaDataDecoder::FlushPromise> VPXDecoder::Flush() {
  return InvokeAsync(mTaskQueue, __func__, []() {
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<MediaDataDecoder::DecodePromise> VPXDecoder::ProcessDecode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  if (vpx_codec_err_t r = vpx_codec_decode(&mVPX, aSample->Data(),
                                           aSample->Size(), nullptr, 0)) {
    LOG("VPX Decode error: %s", vpx_codec_err_to_string(r));
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("VPX error: %s", vpx_codec_err_to_string(r))),
        __func__);
  }

  vpx_codec_iter_t iter = nullptr;
  vpx_image_t* img;
  vpx_image_t* img_alpha = nullptr;
  bool alpha_decoded = false;
  DecodedData results;

  while ((img = vpx_codec_get_frame(&mVPX, &iter))) {
    NS_ASSERTION(img->fmt == VPX_IMG_FMT_I420 || img->fmt == VPX_IMG_FMT_I444,
                 "WebM image format not I420 or I444");
    NS_ASSERTION(!alpha_decoded,
                 "Multiple frames per packet that contains alpha");

    if (aSample->AlphaSize() > 0) {
      if (!alpha_decoded) {
        MediaResult rv = DecodeAlpha(&img_alpha, aSample);
        if (NS_FAILED(rv)) {
          return DecodePromise::CreateAndReject(rv, __func__);
        }
        alpha_decoded = true;
      }
    }
    // Chroma shifts are rounded down as per the decoding examples in the SDK
    VideoData::YCbCrBuffer b;
    b.mPlanes[0].mData = img->planes[0];
    b.mPlanes[0].mStride = img->stride[0];
    b.mPlanes[0].mHeight = img->d_h;
    b.mPlanes[0].mWidth = img->d_w;
    b.mPlanes[0].mOffset = b.mPlanes[0].mSkip = 0;

    b.mPlanes[1].mData = img->planes[1];
    b.mPlanes[1].mStride = img->stride[1];
    b.mPlanes[1].mOffset = b.mPlanes[1].mSkip = 0;

    b.mPlanes[2].mData = img->planes[2];
    b.mPlanes[2].mStride = img->stride[2];
    b.mPlanes[2].mOffset = b.mPlanes[2].mSkip = 0;

    if (img->fmt == VPX_IMG_FMT_I420) {
      b.mPlanes[1].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[1].mWidth = (img->d_w + 1) >> img->x_chroma_shift;

      b.mPlanes[2].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[2].mWidth = (img->d_w + 1) >> img->x_chroma_shift;
    } else if (img->fmt == VPX_IMG_FMT_I444) {
      b.mPlanes[1].mHeight = img->d_h;
      b.mPlanes[1].mWidth = img->d_w;

      b.mPlanes[2].mHeight = img->d_h;
      b.mPlanes[2].mWidth = img->d_w;
    } else {
      LOG("VPX Unknown image format");
      return DecodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                      RESULT_DETAIL("VPX Unknown image format")),
          __func__);
    }
    b.mYUVColorSpace = [&]() {
      switch (img->cs) {
        case VPX_CS_BT_601:
        case VPX_CS_SMPTE_170:
        case VPX_CS_SMPTE_240:
          return gfx::YUVColorSpace::BT601;
        case VPX_CS_BT_709:
          return gfx::YUVColorSpace::BT709;
        case VPX_CS_BT_2020:
          return gfx::YUVColorSpace::BT2020;
        default:
          return DefaultColorSpace({img->d_w, img->d_h});
      }
    }();
    b.mColorRange = img->range == VPX_CR_FULL_RANGE ? gfx::ColorRange::FULL
                                                    : gfx::ColorRange::LIMITED;

    RefPtr<VideoData> v;
    if (!img_alpha) {
      v = VideoData::CreateAndCopyData(
          mInfo, mImageContainer, aSample->mOffset, aSample->mTime,
          aSample->mDuration, b, aSample->mKeyframe, aSample->mTimecode,
          mInfo.ScaledImageRect(img->d_w, img->d_h), mImageAllocator);
    } else {
      VideoData::YCbCrBuffer::Plane alpha_plane;
      alpha_plane.mData = img_alpha->planes[0];
      alpha_plane.mStride = img_alpha->stride[0];
      alpha_plane.mHeight = img_alpha->d_h;
      alpha_plane.mWidth = img_alpha->d_w;
      alpha_plane.mOffset = alpha_plane.mSkip = 0;
      v = VideoData::CreateAndCopyData(
          mInfo, mImageContainer, aSample->mOffset, aSample->mTime,
          aSample->mDuration, b, alpha_plane, aSample->mKeyframe,
          aSample->mTimecode, mInfo.ScaledImageRect(img->d_w, img->d_h));
    }

    if (!v) {
      LOG("Image allocation error source %ux%u display %ux%u picture %ux%u",
          img->d_w, img->d_h, mInfo.mDisplay.width, mInfo.mDisplay.height,
          mInfo.mImage.width, mInfo.mImage.height);
      return DecodePromise::CreateAndReject(
          MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
    }
    results.AppendElement(std::move(v));
  }
  return DecodePromise::CreateAndResolve(std::move(results), __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> VPXDecoder::Decode(
    MediaRawData* aSample) {
  return InvokeAsync<MediaRawData*>(mTaskQueue, this, __func__,
                                    &VPXDecoder::ProcessDecode, aSample);
}

RefPtr<MediaDataDecoder::DecodePromise> VPXDecoder::Drain() {
  return InvokeAsync(mTaskQueue, __func__, [] {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  });
}

MediaResult VPXDecoder::DecodeAlpha(vpx_image_t** aImgAlpha,
                                    const MediaRawData* aSample) {
  vpx_codec_err_t r = vpx_codec_decode(&mVPXAlpha, aSample->AlphaData(),
                                       aSample->AlphaSize(), nullptr, 0);
  if (r) {
    LOG("VPX decode alpha error: %s", vpx_codec_err_to_string(r));
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("VPX decode alpha error: %s",
                                     vpx_codec_err_to_string(r)));
  }

  vpx_codec_iter_t iter = nullptr;

  *aImgAlpha = vpx_codec_get_frame(&mVPXAlpha, &iter);
  NS_ASSERTION((*aImgAlpha)->fmt == VPX_IMG_FMT_I420 ||
                   (*aImgAlpha)->fmt == VPX_IMG_FMT_I444,
               "WebM image format not I420 or I444");

  return NS_OK;
}

/* static */
bool VPXDecoder::IsVPX(const nsACString& aMimeType, uint8_t aCodecMask) {
  return ((aCodecMask & VPXDecoder::VP8) &&
          aMimeType.EqualsLiteral("video/vp8")) ||
         ((aCodecMask & VPXDecoder::VP9) &&
          aMimeType.EqualsLiteral("video/vp9"));
}

/* static */
bool VPXDecoder::IsVP8(const nsACString& aMimeType) {
  return IsVPX(aMimeType, VPXDecoder::VP8);
}

/* static */
bool VPXDecoder::IsVP9(const nsACString& aMimeType) {
  return IsVPX(aMimeType, VPXDecoder::VP9);
}

/* static */
bool VPXDecoder::IsKeyframe(Span<const uint8_t> aBuffer, Codec aCodec) {
  VPXStreamInfo info;
  return GetStreamInfo(aBuffer, info, aCodec) && info.mKeyFrame;
}

/* static */
gfx::IntSize VPXDecoder::GetFrameSize(Span<const uint8_t> aBuffer,
                                      Codec aCodec) {
  VPXStreamInfo info;
  if (!GetStreamInfo(aBuffer, info, aCodec)) {
    return gfx::IntSize();
  }
  return info.mImage;
}

/* static */
gfx::IntSize VPXDecoder::GetDisplaySize(Span<const uint8_t> aBuffer,
                                        Codec aCodec) {
  VPXStreamInfo info;
  if (!GetStreamInfo(aBuffer, info, aCodec)) {
    return gfx::IntSize();
  }
  return info.mDisplay;
}

/* static */
int VPXDecoder::GetVP9Profile(Span<const uint8_t> aBuffer) {
  VPXStreamInfo info;
  if (!GetStreamInfo(aBuffer, info, Codec::VP9)) {
    return -1;
  }
  return info.mProfile;
}

/* static */
bool VPXDecoder::GetStreamInfo(Span<const uint8_t> aBuffer,
                               VPXDecoder::VPXStreamInfo& aInfo, Codec aCodec) {
  if (aBuffer.IsEmpty()) {
    // Can't be good.
    return false;
  }

  aInfo = VPXStreamInfo();

  if (aCodec == Codec::VP8) {
    aInfo.mKeyFrame = (aBuffer[0] & 1) ==
                      0;  // frame type (0 for key frames, 1 for interframes)
    if (!aInfo.mKeyFrame) {
      // We can't retrieve the required information from interframes.
      return true;
    }
    if (aBuffer.Length() < 10) {
      return false;
    }
    uint8_t version = (aBuffer[0] >> 1) & 0x7;
    if (version > 3) {
      return false;
    }
    uint8_t start_code_byte_0 = aBuffer[3];
    uint8_t start_code_byte_1 = aBuffer[4];
    uint8_t start_code_byte_2 = aBuffer[5];
    if (start_code_byte_0 != 0x9d || start_code_byte_1 != 0x01 ||
        start_code_byte_2 != 0x2a) {
      return false;
    }
    uint16_t width = (aBuffer[6] | aBuffer[7] << 8) & 0x3fff;
    uint16_t height = (aBuffer[8] | aBuffer[9] << 8) & 0x3fff;

    // aspect ratio isn't found in the VP8 frame header.
    aInfo.mImage = aInfo.mDisplay = gfx::IntSize(width, height);
    return true;
  }

  BitReader br(aBuffer.Elements(), aBuffer.Length() * 8);
  uint32_t frameMarker = br.ReadBits(2);  // frame_marker
  if (frameMarker != 2) {
    // That's not a valid vp9 header.
    return false;
  }
  uint32_t profile = br.ReadBits(1);  // profile_low_bit
  profile |= br.ReadBits(1) << 1;     // profile_high_bit
  if (profile == 3) {
    profile += br.ReadBits(1);  // reserved_zero
    if (profile > 3) {
      // reserved_zero wasn't zero.
      return false;
    }
  }

  aInfo.mProfile = profile;

  bool show_existing_frame = br.ReadBits(1);
  if (show_existing_frame) {
    if (profile == 3 && aBuffer.Length() < 2) {
      return false;
    }
    Unused << br.ReadBits(3);  // frame_to_show_map_idx
    return true;
  }

  if (aBuffer.Length() < 10) {
    // Header too small;
    return false;
  }

  aInfo.mKeyFrame = !br.ReadBits(1);
  bool show_frame = br.ReadBits(1);
  bool error_resilient_mode = br.ReadBits(1);

  auto frame_sync_code = [&]() -> bool {
    uint8_t frame_sync_byte_1 = br.ReadBits(8);
    uint8_t frame_sync_byte_2 = br.ReadBits(8);
    uint8_t frame_sync_byte_3 = br.ReadBits(8);
    return frame_sync_byte_1 == 0x49 && frame_sync_byte_2 == 0x83 &&
           frame_sync_byte_3 == 0x42;
  };

  auto color_config = [&]() -> bool {
    aInfo.mBitDepth = 8;
    if (profile >= 2) {
      bool ten_or_twelve_bit = br.ReadBits(1);
      aInfo.mBitDepth = ten_or_twelve_bit ? 12 : 10;
    }
    aInfo.mColorSpace = br.ReadBits(3);
    if (aInfo.mColorSpace != 7 /* CS_RGB */) {
      aInfo.mFullRange = br.ReadBits(1);
      if (profile == 1 || profile == 3) {
        aInfo.mSubSampling_x = br.ReadBits(1);
        aInfo.mSubSampling_y = br.ReadBits(1);
        if (br.ReadBits(1)) {  // reserved_zero
          return false;
        };
      } else {
        aInfo.mSubSampling_x = true;
        aInfo.mSubSampling_y = true;
      }
    } else {
      aInfo.mFullRange = true;
      if (profile == 1 || profile == 3) {
        aInfo.mSubSampling_x = false;
        aInfo.mSubSampling_y = false;
        if (br.ReadBits(1)) {  // reserved_zero
          return false;
        };
      } else {
        // sRGB color space is only available with VP9 profile 1.
        return false;
      }
    }
    return true;
  };

  auto frame_size = [&]() {
    int32_t width = static_cast<int32_t>(br.ReadBits(16)) + 1;
    int32_t height = static_cast<int32_t>(br.ReadBits(16)) + 1;
    aInfo.mImage = gfx::IntSize(width, height);
  };

  auto render_size = [&]() {
    bool render_and_frame_size_different = br.ReadBits(1);
    if (render_and_frame_size_different) {
      int32_t width = static_cast<int32_t>(br.ReadBits(16)) + 1;
      int32_t height = static_cast<int32_t>(br.ReadBits(16)) + 1;
      aInfo.mDisplay = gfx::IntSize(width, height);
    } else {
      aInfo.mDisplay = aInfo.mImage;
    }
  };

  if (aInfo.mKeyFrame) {
    if (!frame_sync_code()) {
      return false;
    }
    if (!color_config()) {
      return false;
    }
    frame_size();
    render_size();
  } else {
    bool intra_only = show_frame ? false : br.ReadBit();
    if (!error_resilient_mode) {
      Unused << br.ReadBits(2);  // reset_frame_context
    }
    if (intra_only) {
      if (!frame_sync_code()) {
        return false;
      }
      if (profile > 0) {
        if (!color_config()) {
          return false;
        }
      } else {
        aInfo.mColorSpace = 1;  // CS_BT_601
        aInfo.mSubSampling_x = true;
        aInfo.mSubSampling_y = true;
        aInfo.mBitDepth = 8;
      }
      Unused << br.ReadBits(8);  // refresh_frame_flags
      frame_size();
      render_size();
    }
  }
  return true;
}

}  // namespace mozilla
#undef LOG
