/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegVideoDecoder_h__
#define __FFmpegVideoDecoder_h__

#include "ImageContainer.h"
#include "FFmpegDataDecoder.h"
#include "FFmpegLibWrapper.h"
#include "SimpleMap.h"
#include "mozilla/ScopeExit.h"
#include "nsTHashSet.h"
#if LIBAVCODEC_VERSION_MAJOR >= 57 && LIBAVUTIL_VERSION_MAJOR >= 56
#  include "mozilla/layers/TextureClient.h"
#endif

struct _VADRMPRIMESurfaceDescriptor;
typedef struct _VADRMPRIMESurfaceDescriptor VADRMPRIMESurfaceDescriptor;

namespace mozilla {

class ImageBufferWrapper;
class VideoFramePool;

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

  ~FFmpegVideoDecoder();

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

#if LIBAVCODEC_VERSION_MAJOR >= 57 && LIBAVUTIL_VERSION_MAJOR >= 56
  int GetVideoBuffer(struct AVCodecContext* aCodecContext, AVFrame* aFrame,
                     int aFlags);
  int GetVideoBufferDefault(struct AVCodecContext* aCodecContext,
                            AVFrame* aFrame, int aFlags) {
    mIsUsingShmemBufferForDecode = Some(false);
    return mLib->avcodec_default_get_buffer2(aCodecContext, aFrame, aFlags);
  }
  void ReleaseAllocatedImage(ImageBufferWrapper* aImage) {
    mAllocatedImages.Remove(aImage);
  }
#endif

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

  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  bool IsHardwareAccelerated() const {
    nsAutoCString dummy;
    return IsHardwareAccelerated(dummy);
  }

#if LIBAVCODEC_VERSION_MAJOR >= 57 && LIBAVUTIL_VERSION_MAJOR >= 56
  layers::TextureClient* AllocateTextueClientForImage(
      struct AVCodecContext* aCodecContext, layers::PlanarYCbCrImage* aImage);

  layers::PlanarYCbCrData CreateEmptyPlanarYCbCrData(
      struct AVCodecContext* aCodecContext, const VideoInfo& aInfo);

  gfx::IntSize GetAlignmentVideoFrameSize(struct AVCodecContext* aCodecContext,
                                          int32_t aWidth,
                                          int32_t aHeight) const;
#endif

#ifdef MOZ_WAYLAND_USE_VAAPI
  void InitHWDecodingPrefs();
  MediaResult InitVAAPIDecoder();
  bool CreateVAAPIDeviceContext();
  void InitVAAPICodecContext();
  AVCodec* FindVAAPICodec();
  bool GetVAAPISurfaceDescriptor(VADRMPRIMESurfaceDescriptor* aVaDesc);
  void AddAcceleratedFormats(nsTArray<AVCodecID>& aCodecList,
                             AVCodecID aCodecID, AVVAAPIHWConfig* hwconfig);
  nsTArray<AVCodecID> GetAcceleratedFormats();
  bool IsFormatAccelerated(AVCodecID aCodecID) const;

  MediaResult CreateImageVAAPI(int64_t aOffset, int64_t aPts, int64_t aDuration,
                               MediaDataDecoder::DecodedData& aResults);
  MediaResult CreateImageDMABuf(int64_t aOffset, int64_t aPts,
                                int64_t aDuration,
                                MediaDataDecoder::DecodedData& aResults);
#endif

#ifdef MOZ_WAYLAND_USE_VAAPI
  AVBufferRef* mVAAPIDeviceContext;
  bool mEnableHardwareDecoding;
  VADisplay mDisplay;
  bool mUseDMABufSurfaces;
  UniquePtr<VideoFramePool> mVideoFramePool;
  static nsTArray<AVCodecID> mAcceleratedFormats;
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

  // True if we're allocating shmem for ffmpeg decode buffer.
  Maybe<Atomic<bool>> mIsUsingShmemBufferForDecode;

#if LIBAVCODEC_VERSION_MAJOR >= 57 && LIBAVUTIL_VERSION_MAJOR >= 56
  // These images are buffers for ffmpeg in order to store decoded data when
  // using custom allocator for decoding. We want to explictly track all images
  // we allocate to ensure that we won't leak any of them.
  nsTHashSet<RefPtr<ImageBufferWrapper>> mAllocatedImages;
#endif
};

#if LIBAVCODEC_VERSION_MAJOR >= 57 && LIBAVUTIL_VERSION_MAJOR >= 56
class ImageBufferWrapper final {
 public:
  typedef mozilla::layers::Image Image;
  typedef mozilla::layers::PlanarYCbCrImage PlanarYCbCrImage;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageBufferWrapper)

  ImageBufferWrapper(Image* aImage, void* aDecoder)
      : mImage(aImage), mDecoder(aDecoder) {
    MOZ_ASSERT(aImage);
    MOZ_ASSERT(mDecoder);
  }

  PlanarYCbCrImage* AsPlanarYCbCrImage() {
    return mImage->AsPlanarYCbCrImage();
  }

  void ReleaseBuffer() {
    auto clear = MakeScopeExit([&]() {
      auto* decoder = static_cast<FFmpegVideoDecoder<LIBAV_VER>*>(mDecoder);
      decoder->ReleaseAllocatedImage(this);
    });
    if (!mImage) {
      return;
    }
    PlanarYCbCrImage* image = mImage->AsPlanarYCbCrImage();
    RefPtr<layers::TextureClient> texture = image->GetTextureClient(nullptr);
    // Usually the decoded video buffer would be locked when it is allocated,
    // and gets unlocked when we create the video data via `DoDecode`. However,
    // sometime the buffer won't be used for the decoded data (maybe just as
    // an internal temporary usage?) so we will need to unlock the texture here
    // before sending it back to recycle.
    if (!texture) {
      NS_WARNING("Failed to get the texture client during release!");
    } else if (texture->IsLocked()) {
      texture->Unlock();
    }
  }

 private:
  ~ImageBufferWrapper() = default;
  const RefPtr<Image> mImage;
  void* const MOZ_NON_OWNING_REF mDecoder;
};
#endif

}  // namespace mozilla

#endif  // __FFmpegVideoDecoder_h__
