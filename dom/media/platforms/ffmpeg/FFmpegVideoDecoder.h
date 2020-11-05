/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegVideoDecoder_h__
#define __FFmpegVideoDecoder_h__

#include "FFmpegDataDecoder.h"
#include "FFmpegLibWrapper.h"
#include "SimpleMap.h"
#ifdef MOZ_WAYLAND_USE_VAAPI
#  include "mozilla/LinkedList.h"
#  include "mozilla/widget/DMABufSurface.h"
#endif

namespace mozilla {

#ifdef MOZ_WAYLAND_USE_VAAPI
// DMABufSurfaceWrapper holds a reference to GPU data with a video frame.
//
// Actual GPU pixel data are stored at DMABufSurface and
// DMABufSurface is passed to gecko GL rendering pipeline via.
// DMABUFSurfaceImage.
//
// DMABufSurfaceWrapper can optionally hold VA-API ffmpeg related data to keep
// GPU data locked untill we need them.
//
// DMABufSurfaceWrapper is used for both HW accelerated video decoding (VA-API)
// and ffmpeg SW decoding.
//
// VA-API scenario
//
// When VA-API decoding is running, ffmpeg allocates AVHWFramesContext - a pool
// of "hardware" frames. Every "hardware" frame (VASurface) is backed
// by actual piece of GPU memory which holds the decoded image data.
//
// The VASurface is wrapped by DMABufSurface and transferred to
// rendering queue by DMABUFSurfaceImage, where TextureClient is
// created and VASurface is used as a texture there.
//
// As there's a limited number of VASurfaces, ffmpeg reuses them to decode
// next frames ASAP even if they are still attached to DMABufSurface
// and used as a texture in our rendering engine.
//
// Unfortunately there isn't any obvious way how to mark particular VASurface
// as used. The best we can do is to hold a reference to particular AVBuffer
// from decoded AVFrame and AVHWFramesContext which owns the AVBuffer.
//
// FFmpeg SW decoding scenario
//
// When SW ffmpeg decoding is running, DMABufSurfaceWrapper contains only
// a DMABufSurface reference and VA-API related members are null.
// We own the DMABufSurface underlying GPU data and we use it for
// repeated rendering of video frames.
//
class DMABufSurfaceWrapper final {
 public:
  DMABufSurfaceWrapper(DMABufSurface* aSurface, FFmpegLibWrapper* aLib);
  ~DMABufSurfaceWrapper();

  // Lock VAAPI related data
  void LockVAAPIData(AVCodecContext* aAVCodecContext, AVFrame* aAVFrame);

  // Release VAAPI related data, DMABufSurface can be reused
  // for another frame.
  void ReleaseVAAPIData();

  // Check if DMABufSurface is used by any gecko rendering process
  // (WebRender or GL compositor) or by DMABUFSurfaceImage/VideoData.
  bool IsUsed() const { return mSurface->IsGlobalRefSet(); }

  RefPtr<DMABufSurfaceYUV> GetDMABufSurface() const {
    return mSurface->GetAsDMABufSurfaceYUV();
  }

  // Don't allow DMABufSurfaceWrapper plain copy as it leads to
  // enexpected DMABufSurface/HW buffer releases and we don't want to
  // deep copy them.
  DMABufSurfaceWrapper(const DMABufSurfaceWrapper&) = delete;
  const DMABufSurfaceWrapper& operator=(DMABufSurfaceWrapper const&) = delete;

 private:
  const RefPtr<DMABufSurface> mSurface;
  const FFmpegLibWrapper* mLib;
  AVBufferRef* mAVHWFramesContext;
  AVBufferRef* mHWAVBuffer;
};
#endif

template <int V>
class FFmpegVideoDecoder : public FFmpegDataDecoder<V> {};

template <>
class FFmpegVideoDecoder<LIBAV_VER>;
DDLoggedTypeNameAndBase(FFmpegVideoDecoder<LIBAV_VER>,
                        FFmpegDataDecoder<LIBAV_VER>);

template <>
class FFmpegVideoDecoder<LIBAV_VER>
    : public FFmpegDataDecoder<LIBAV_VER>,
      public DecoderDoctorLifeLogger<FFmpegVideoDecoder<LIBAV_VER>> {
  typedef mozilla::layers::Image Image;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::KnowsCompositor KnowsCompositor;
  typedef SimpleMap<int64_t> DurationMap;

 public:
  FFmpegVideoDecoder(FFmpegLibWrapper* aLib, const VideoInfo& aConfig,
                     KnowsCompositor* aAllocator,
                     ImageContainer* aImageContainer, bool aLowLatency,
                     bool aDisableHardwareDecoding);

  RefPtr<InitPromise> Init() override;
  void InitCodecContext() override;
  nsCString GetDescriptionName() const override {
#ifdef USING_MOZFFVPX
    return "ffvpx video decoder"_ns;
#else
    return "ffmpeg video decoder"_ns;
#endif
  }
  ConversionRequired NeedsConversion() const override {
    return ConversionRequired::kNeedAVCC;
  }

  static AVCodecID GetCodecId(const nsACString& aMimeType);

 private:
  RefPtr<FlushPromise> ProcessFlush() override;
  void ProcessShutdown() override;
  MediaResult DoDecode(MediaRawData* aSample, uint8_t* aData, int aSize,
                       bool* aGotFrame, DecodedData& aResults) override;
  void OutputDelayedFrames();
  bool NeedParser() const override {
    return
#if LIBAVCODEC_VERSION_MAJOR >= 58
        false;
#else
#  if LIBAVCODEC_VERSION_MAJOR >= 55
        mCodecID == AV_CODEC_ID_VP9 ||
#  endif
        mCodecID == AV_CODEC_ID_VP8;
#endif
  }
  gfx::YUVColorSpace GetFrameColorSpace() const;

  MediaResult CreateImage(int64_t aOffset, int64_t aPts, int64_t aDuration,
                          MediaDataDecoder::DecodedData& aResults) const;

#ifdef MOZ_WAYLAND_USE_VAAPI
  MediaResult InitVAAPIDecoder();
  bool CreateVAAPIDeviceContext();
  void InitVAAPICodecContext();
  AVCodec* FindVAAPICodec();
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  bool GetVAAPISurfaceDescriptor(VADRMPRIMESurfaceDescriptor& aVaDesc);

  MediaResult CreateImageDMABuf(int64_t aOffset, int64_t aPts,
                                int64_t aDuration,
                                MediaDataDecoder::DecodedData& aResults);

  void ReleaseUnusedVAAPIFrames();
  DMABufSurfaceWrapper* GetUnusedDMABufSurfaceWrapper();
  void ReleaseDMABufSurfaces();
#endif

  /**
   * This method allocates a buffer for FFmpeg's decoder, wrapped in an Image.
   * Currently it only supports Planar YUV420, which appears to be the only
   * non-hardware accelerated image format that FFmpeg's H264 decoder is
   * capable of outputting.
   */
  int AllocateYUV420PVideoBuffer(AVCodecContext* aCodecContext,
                                 AVFrame* aFrame);

#ifdef MOZ_WAYLAND_USE_VAAPI
  AVBufferRef* mVAAPIDeviceContext;
  const bool mDisableHardwareDecoding;
  VADisplay mDisplay;
  bool mUseDMABufSurfaces;
  nsTArray<DMABufSurfaceWrapper> mDMABufSurfaces;
#endif
  RefPtr<KnowsCompositor> mImageAllocator;
  RefPtr<ImageContainer> mImageContainer;
  VideoInfo mInfo;

  class PtsCorrectionContext {
   public:
    PtsCorrectionContext();
    int64_t GuessCorrectPts(int64_t aPts, int64_t aDts);
    void Reset();
    int64_t LastDts() const { return mLastDts; }

   private:
    int64_t mNumFaultyPts;  /// Number of incorrect PTS values so far
    int64_t mNumFaultyDts;  /// Number of incorrect DTS values so far
    int64_t mLastPts;       /// PTS of the last frame
    int64_t mLastDts;       /// DTS of the last frame
  };

  PtsCorrectionContext mPtsContext;

  DurationMap mDurationMap;
  const bool mLowLatency;
};

}  // namespace mozilla

#endif  // __FFmpegVideoDecoder_h__
