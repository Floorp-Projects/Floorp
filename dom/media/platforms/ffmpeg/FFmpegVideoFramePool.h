/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegVideoFramePool_h__
#define __FFmpegVideoFramePool_h__

#include "FFmpegVideoDecoder.h"
#include "FFmpegLibWrapper.h"

#include "mozilla/layers/DMABUFSurfaceImage.h"
#include "mozilla/widget/DMABufLibWrapper.h"
#include "mozilla/widget/DMABufSurface.h"

namespace mozilla {

class VideoFrameSurface {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoFrameSurface)

  VideoFrameSurface(){};

  virtual void LockVAAPIData(AVCodecContext* aAVCodecContext, AVFrame* aAVFrame,
                             FFmpegLibWrapper* aLib){};
  virtual void ReleaseVAAPIData(bool aForFrameRecycle = true){};
  virtual bool IsUsed() const = 0;

  virtual void SetYUVColorSpace(mozilla::gfx::YUVColorSpace aColorSpace) = 0;

  virtual RefPtr<DMABufSurfaceYUV> GetDMABufSurface() { return nullptr; };

  virtual RefPtr<layers::Image> GetAsImage() = 0;

  // Don't allow VideoFrameSurface plain copy as it leads to
  // unexpected DMABufSurface/HW buffer releases and we don't want to
  // deep copy them.
  VideoFrameSurface(const VideoFrameSurface&) = delete;
  const VideoFrameSurface& operator=(VideoFrameSurface const&) = delete;

 protected:
  virtual ~VideoFrameSurface(){};
};

// VideoFrameSurfaceDMABuf is YUV dmabuf surface used for SW video decoding.
// Stores decoded video data in GPU memory.
class VideoFrameSurfaceDMABuf : public VideoFrameSurface {
 public:
  explicit VideoFrameSurfaceDMABuf(DMABufSurface* aSurface);

  // Check if DMABufSurface is used by any gecko rendering process
  // (WebRender or GL compositor) or by DMABUFSurfaceImage/VideoData.
  bool IsUsed() const { return mSurface->IsGlobalRefSet(); }

  void SetYUVColorSpace(mozilla::gfx::YUVColorSpace aColorSpace) {
    mSurface->GetAsDMABufSurfaceYUV()->SetYUVColorSpace(aColorSpace);
  }

  RefPtr<DMABufSurfaceYUV> GetDMABufSurface() {
    return mSurface->GetAsDMABufSurfaceYUV();
  };

  RefPtr<layers::Image> GetAsImage();

 protected:
  const RefPtr<DMABufSurface> mSurface;

 protected:
  ~VideoFrameSurfaceDMABuf(){};
};

// VideoFrameSurfaceVAAPI holds a reference to GPU data with a video frame.
//
// Actual GPU pixel data are stored at DMABufSurface and
// DMABufSurface is passed to gecko GL rendering pipeline via.
// DMABUFSurfaceImage.
//
// VideoFrameSurfaceVAAPI can optionally hold VA-API ffmpeg related data to keep
// GPU data locked untill we need them.
//
// VideoFrameSurfaceVAAPI is used for both HW accelerated video decoding
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

class VideoFrameSurfaceVAAPI : public VideoFrameSurfaceDMABuf {
 public:
  explicit VideoFrameSurfaceVAAPI(DMABufSurface* aSurface);

  // Lock VAAPI related data
  void LockVAAPIData(AVCodecContext* aAVCodecContext, AVFrame* aAVFrame,
                     FFmpegLibWrapper* aLib);

  // Release VAAPI related data, DMABufSurface can be reused
  // for another frame.
  void ReleaseVAAPIData(bool aForFrameRecycle);

 private:
  ~VideoFrameSurfaceVAAPI();

  const FFmpegLibWrapper* mLib;
  AVBufferRef* mAVHWFramesContext;
  AVBufferRef* mHWAVBuffer;
};

class VideoFramePool final {
 public:
  explicit VideoFramePool(bool aUseVAAPI);
  ~VideoFramePool();

  RefPtr<VideoFrameSurface> GetVideoFrameSurface(
      VADRMPRIMESurfaceDescriptor& aVaDesc);
  RefPtr<VideoFrameSurface> GetVideoFrameSurface(AVPixelFormat aPixelFormat,
                                                 AVFrame* aFrame);
  void ReleaseUnusedVAAPIFrames();

 private:
  RefPtr<VideoFrameSurface> GetFreeVideoFrameSurface();

 private:
  const bool mUseVAAPI;
  nsTArray<RefPtr<VideoFrameSurface>> mDMABufSurfaces;
};

}  // namespace mozilla

#endif  // __FFmpegVideoFramePool_h__
