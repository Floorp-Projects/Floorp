/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegVideoFramePool_h__
#define __FFmpegVideoFramePool_h__

#include "FFmpegLibWrapper.h"
#include "FFmpegLibs.h"
#include "FFmpegLog.h"

#include "mozilla/layers/DMABUFSurfaceImage.h"
#include "mozilla/widget/DMABufLibWrapper.h"
#include "mozilla/widget/DMABufSurface.h"

namespace mozilla {

// VideoFrameSurface holds a reference to GPU data with a video frame.
//
// Actual GPU pixel data are stored at DMABufSurface and
// DMABufSurface is passed to gecko GL rendering pipeline via.
// DMABUFSurfaceImage.
//
// VideoFrameSurface can optionally hold VA-API ffmpeg related data to keep
// GPU data locked untill we need them.
//
// VideoFrameSurface is used for both HW accelerated video decoding
// (VA-API) and ffmpeg SW decoding.
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
template <int V>
class VideoFrameSurface {};
template <>
class VideoFrameSurface<LIBAV_VER>;

template <int V>
class VideoFramePool {};
template <>
class VideoFramePool<LIBAV_VER>;

template <>
class VideoFrameSurface<LIBAV_VER> {
  friend class VideoFramePool<LIBAV_VER>;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoFrameSurface)

  explicit VideoFrameSurface(DMABufSurface* aSurface);

  void SetYUVColorSpace(mozilla::gfx::YUVColorSpace aColorSpace) {
    mSurface->GetAsDMABufSurfaceYUV()->SetYUVColorSpace(aColorSpace);
  }
  void SetColorRange(mozilla::gfx::ColorRange aColorRange) {
    mSurface->GetAsDMABufSurfaceYUV()->SetColorRange(aColorRange);
  }

  RefPtr<DMABufSurfaceYUV> GetDMABufSurface() {
    return mSurface->GetAsDMABufSurfaceYUV();
  };

  RefPtr<layers::Image> GetAsImage();

  // Don't allow VideoFrameSurface plain copy as it leads to
  // unexpected DMABufSurface/HW buffer releases and we don't want to
  // deep copy them.
  VideoFrameSurface(const VideoFrameSurface&) = delete;
  const VideoFrameSurface& operator=(VideoFrameSurface const&) = delete;

 protected:
  // Lock VAAPI related data
  void LockVAAPIData(AVCodecContext* aAVCodecContext, AVFrame* aAVFrame,
                     FFmpegLibWrapper* aLib);
  // Release VAAPI related data, DMABufSurface can be reused
  // for another frame.
  void ReleaseVAAPIData(bool aForFrameRecycle = true);

  // Check if DMABufSurface is used by any gecko rendering process
  // (WebRender or GL compositor) or by DMABUFSurfaceImage/VideoData.
  bool IsUsed() const { return mSurface->IsGlobalRefSet(); }

  // Surface points to dmabuf memmory owned by ffmpeg.
  bool IsFFMPEGSurface() const { return !!mLib; }

  void MarkAsUsed(VASurfaceID aFFMPEGSurfaceID) {
    mFFMPEGSurfaceID = Some(aFFMPEGSurfaceID);
  }

 private:
  virtual ~VideoFrameSurface();

  const RefPtr<DMABufSurface> mSurface;
  const FFmpegLibWrapper* mLib;
  AVBufferRef* mAVHWFrameContext;
  AVBufferRef* mHWAVBuffer;
  Maybe<VASurfaceID> mFFMPEGSurfaceID;
};

// VideoFramePool class is thread-safe.
template <>
class VideoFramePool<LIBAV_VER> {
 public:
  explicit VideoFramePool(int aFFMPEGPoolSize);
  ~VideoFramePool();

  RefPtr<VideoFrameSurface<LIBAV_VER>> GetVideoFrameSurface(
      VADRMPRIMESurfaceDescriptor& aVaDesc, int aWidth, int aHeight,
      AVCodecContext* aAVCodecContext, AVFrame* aAVFrame,
      FFmpegLibWrapper* aLib);
  RefPtr<VideoFrameSurface<LIBAV_VER>> GetVideoFrameSurface(
      AVDRMFrameDescriptor& aDesc, int aWidth, int aHeight,
      AVCodecContext* aAVCodecContext, AVFrame* aAVFrame,
      FFmpegLibWrapper* aLib);

  void ReleaseUnusedVAAPIFrames();

 private:
  RefPtr<VideoFrameSurface<LIBAV_VER>> GetFreeVideoFrameSurface();
  bool ShouldCopySurface();
  void CheckNewFFMPEGSurface(VASurfaceID aNewSurfaceID);

 private:
  // Protect mDMABufSurfaces pool access
  Mutex mSurfaceLock MOZ_UNANNOTATED;
  nsTArray<RefPtr<VideoFrameSurface<LIBAV_VER>>> mDMABufSurfaces;
  // Number of dmabuf surfaces allocated by ffmpeg for decoded video frames.
  // Can be adjusted by extra_hw_frames at InitVAAPICodecContext().
  int mFFMPEGPoolSize;
  // We may fail to create texture over DMABuf memory due to driver bugs so
  // check that before we export first DMABuf video frame.
  Maybe<bool> mTextureCreationWorks;
  // We may fail to copy DMABuf memory on NVIDIA drivers.
  bool mTextureCopyWorks = true;
};

}  // namespace mozilla

#endif  // __FFmpegVideoFramePool_h__
