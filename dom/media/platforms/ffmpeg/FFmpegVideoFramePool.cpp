/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoFramePool.h"
#include "PlatformDecoderModule.h"
#include "FFmpegLog.h"
#include "mozilla/widget/DMABufLibWrapper.h"
#include "libavutil/pixfmt.h"

#ifdef MOZ_LOGGING
#  undef DMABUF_LOG
extern mozilla::LazyLogModule gDmabufLog;
#  define DMABUF_LOG(str, ...) \
    MOZ_LOG(gDmabufLog, mozilla::LogLevel::Debug, (str, ##__VA_ARGS__))
#else
#  define DMABUF_LOG(args)
#endif /* MOZ_LOGGING */

// Start copying surfaces when free ffmpeg surface count is below 1/4 of all
// available surfaces.
#define SURFACE_COPY_TRESHOLD (1.0f / 4.0f);

namespace mozilla {

RefPtr<layers::Image> VideoFrameSurface<LIBAV_VER>::GetAsImage() {
  return new layers::DMABUFSurfaceImage(mSurface);
}

VideoFrameSurface<LIBAV_VER>::VideoFrameSurface(DMABufSurface* aSurface)
    : mSurface(aSurface),
      mLib(nullptr),
      mAVHWFrameContext(nullptr),
      mHWAVBuffer(nullptr) {
  // Create global refcount object to track mSurface usage over
  // gects rendering engine. We can't release it until it's used
  // by GL compositor / WebRender.
  MOZ_ASSERT(mSurface);
  MOZ_RELEASE_ASSERT(mSurface->GetAsDMABufSurfaceYUV());
  mSurface->GlobalRefCountCreate();
  DMABUF_LOG("VideoFrameSurface: creating surface UID %d", mSurface->GetUID());
}

void VideoFrameSurface<LIBAV_VER>::LockVAAPIData(
    AVCodecContext* aAVCodecContext, AVFrame* aAVFrame,
    FFmpegLibWrapper* aLib) {
  MOZ_DIAGNOSTIC_ASSERT(aAVCodecContext->hw_frames_ctx);
  mLib = aLib;
  mAVHWFrameContext = aLib->av_buffer_ref(aAVCodecContext->hw_frames_ctx);
  mHWAVBuffer = aLib->av_buffer_ref(aAVFrame->buf[0]);
  DMABUF_LOG(
      "VideoFrameSurface: VAAPI locking dmabuf surface UID %d "
      "mAVHWFrameContext %p mHWAVBuffer %p",
      mSurface->GetUID(), mAVHWFrameContext, mHWAVBuffer);
}

void VideoFrameSurface<LIBAV_VER>::ReleaseVAAPIData(bool aForFrameRecycle) {
  DMABUF_LOG(
      "VideoFrameSurface: VAAPI releasing dmabuf surface UID %d "
      "aForFrameRecycle %d mLib %p mAVHWFrameContext %p mHWAVBuffer %p",
      mSurface->GetUID(), aForFrameRecycle, mLib, mAVHWFrameContext,
      mHWAVBuffer);

  // It's possible to unref GPU data while IsUsed() is still set.
  // It can happens when VideoFramePool is deleted while decoder shutdown
  // but related dmabuf surfaces are still used in another process.
  // In such case we don't care as the dmabuf surface will not be
  // recycled for another frame and stays here untill last fd of it
  // is closed.
  if (mLib) {
    mLib->av_buffer_unref(&mHWAVBuffer);
    mLib->av_buffer_unref(&mAVHWFrameContext);
    mLib = nullptr;
  }

  // If we want to recycle the frame, make sure it's not used
  // by gecko rendering pipeline.
  if (aForFrameRecycle) {
    MOZ_DIAGNOSTIC_ASSERT(!IsUsed());
    mSurface->ReleaseSurface();
  }
}

VideoFrameSurface<LIBAV_VER>::~VideoFrameSurface() {
  DMABUF_LOG("VideoFrameSurface: deleting dmabuf surface UID %d",
             mSurface->GetUID());
  // We're about to quit, no need to recycle the frames.
  ReleaseVAAPIData(/* aForFrameRecycle */ false);
}

VideoFramePool<LIBAV_VER>::VideoFramePool(int aFFMPEGPoolSize)
    : mSurfaceLock("VideoFramePoolSurfaceLock"),
      mFFMPEGPoolSize(aFFMPEGPoolSize) {
  DMABUF_LOG("VideoFramePool::VideoFramePool() pool size %d", mFFMPEGPoolSize);
}

VideoFramePool<LIBAV_VER>::~VideoFramePool() {
  MutexAutoLock lock(mSurfaceLock);
  mDMABufSurfaces.Clear();
}

void VideoFramePool<LIBAV_VER>::ReleaseUnusedVAAPIFrames() {
  MutexAutoLock lock(mSurfaceLock);
  for (const auto& surface : mDMABufSurfaces) {
    if (!surface->IsUsed()) {
      surface->ReleaseVAAPIData();
    }
  }
}

RefPtr<VideoFrameSurface<LIBAV_VER>>
VideoFramePool<LIBAV_VER>::GetFreeVideoFrameSurface() {
  for (auto& surface : mDMABufSurfaces) {
    if (surface->IsUsed()) {
      continue;
    }
    surface->ReleaseVAAPIData();
    return surface;
  }
  return nullptr;
}

bool VideoFramePool<LIBAV_VER>::ShouldCopySurface() {
  // Number of used HW surfaces.
  int surfacesUsed = 0;
  int surfacesUsedFFmpeg = 0;
  for (const auto& surface : mDMABufSurfaces) {
    if (surface->IsUsed()) {
      surfacesUsed++;
      if (surface->IsFFMPEGSurface()) {
        surfacesUsedFFmpeg++;
      }
    }
  }
  float freeRatio = 1.0f - (surfacesUsedFFmpeg / (float)mFFMPEGPoolSize);
  DMABUF_LOG(
      "Surface pool size %d used copied %d used ffmpeg %d (max %d) free ratio "
      "%f",
      (int)mDMABufSurfaces.Length(), surfacesUsed - surfacesUsedFFmpeg,
      surfacesUsedFFmpeg, mFFMPEGPoolSize, freeRatio);
  return freeRatio < SURFACE_COPY_TRESHOLD;
}

RefPtr<VideoFrameSurface<LIBAV_VER>>
VideoFramePool<LIBAV_VER>::GetVideoFrameSurface(
    VADRMPRIMESurfaceDescriptor& aVaDesc, int aWidth, int aHeight,
    AVCodecContext* aAVCodecContext, AVFrame* aAVFrame,
    FFmpegLibWrapper* aLib) {
  if (aVaDesc.fourcc != VA_FOURCC_NV12 && aVaDesc.fourcc != VA_FOURCC_YV12 &&
      aVaDesc.fourcc != VA_FOURCC_P010) {
    DMABUF_LOG("Unsupported VA-API surface format %d", aVaDesc.fourcc);
    return nullptr;
  }

  MutexAutoLock lock(mSurfaceLock);
  RefPtr<DMABufSurfaceYUV> surface;
  RefPtr<VideoFrameSurface<LIBAV_VER>> videoSurface =
      GetFreeVideoFrameSurface();
  if (!videoSurface) {
    surface = new DMABufSurfaceYUV();
    videoSurface = new VideoFrameSurface<LIBAV_VER>(surface);
    mDMABufSurfaces.AppendElement(videoSurface);
    DMABUF_LOG("Created new VA-API DMABufSurface UID %d", surface->GetUID());
  } else {
    surface = videoSurface->GetDMABufSurface();
    DMABUF_LOG("Reusing VA-API DMABufSurface UID %d", surface->GetUID());
  }

  bool copySurface = ShouldCopySurface();
  if (!surface->UpdateYUVData(aVaDesc, aWidth, aHeight, copySurface)) {
    return nullptr;
  }

  if (MOZ_UNLIKELY(!mTextureCreationWorks)) {
    mTextureCreationWorks = Some(surface->VerifyTextureCreation());
    if (!*mTextureCreationWorks) {
      DMABUF_LOG("  failed to create texture over DMABuf memory!");
      return nullptr;
    }
  }

  if (!copySurface) {
    videoSurface->LockVAAPIData(aAVCodecContext, aAVFrame, aLib);
  }
  videoSurface->MarkAsUsed();
  return videoSurface;
}

}  // namespace mozilla
