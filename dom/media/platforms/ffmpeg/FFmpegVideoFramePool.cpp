/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoFramePool.h"
#include "FFmpegLog.h"
#include "mozilla/widget/DMABufLibWrapper.h"
#include "libavutil/pixfmt.h"

#undef FFMPEG_LOG
#define FFMPEG_LOG(str, ...) \
  MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (str, ##__VA_ARGS__))

namespace mozilla {

RefPtr<layers::Image> VideoFrameSurface::GetAsImage() {
  return new layers::DMABUFSurfaceImage(mSurface);
}

VideoFrameSurface::VideoFrameSurface(DMABufSurface* aSurface)
    : mSurface(aSurface),
      mLib(nullptr),
      mAVHWFramesContext(nullptr),
      mHWAVBuffer(nullptr) {
  // Create global refcount object to track mSurface usage over
  // gects rendering engine. We can't release it until it's used
  // by GL compositor / WebRender.
  MOZ_ASSERT(mSurface);
  MOZ_RELEASE_ASSERT(mSurface->GetAsDMABufSurfaceYUV());
  mSurface->GlobalRefCountCreate();
  FFMPEG_LOG("VideoFrameSurface: creating surface UID = %d",
             mSurface->GetUID());
}

void VideoFrameSurface::LockVAAPIData(AVCodecContext* aAVCodecContext,
                                      AVFrame* aAVFrame,
                                      FFmpegLibWrapper* aLib) {
  FFMPEG_LOG("VideoFrameSurface: VAAPI locking dmabuf surface UID = %d",
             mSurface->GetUID());
  mLib = aLib;
  mAVHWFramesContext = aLib->av_buffer_ref(aAVCodecContext->hw_frames_ctx);
  mHWAVBuffer = aLib->av_buffer_ref(aAVFrame->buf[0]);
}

void VideoFrameSurface::ReleaseVAAPIData(bool aForFrameRecycle) {
  FFMPEG_LOG("VideoFrameSurface: VAAPI releasing dmabuf surface UID = %d",
             mSurface->GetUID());

  // It's possible to unref GPU data while IsUsed() is still set.
  // It can happens when VideoFramePool is deleted while decoder shutdown
  // but related dmabuf surfaces are still used in another process.
  // In such case we don't care as the dmabuf surface will not be
  // recycled for another frame and stays here untill last fd of it
  // is closed.
  if (mLib) {
    mLib->av_buffer_unref(&mHWAVBuffer);
    mLib->av_buffer_unref(&mAVHWFramesContext);
  }

  // If we want to recycle the frame, make sure it's not used
  // by gecko rendering pipeline.
  if (aForFrameRecycle) {
    MOZ_DIAGNOSTIC_ASSERT(!IsUsed());
    mSurface->ReleaseSurface();
  }
}

VideoFrameSurface::~VideoFrameSurface() {
  FFMPEG_LOG("VideoFrameSurface: deleting dmabuf surface UID = %d",
             mSurface->GetUID());
  // We're about to quit, no need to recycle the frames.
  ReleaseVAAPIData(/* aForFrameRecycle */ false);
}

VideoFramePool::VideoFramePool() : mSurfaceLock("VideoFramePoolSurfaceLock") {}

VideoFramePool::~VideoFramePool() {
  MutexAutoLock lock(mSurfaceLock);
  mDMABufSurfaces.Clear();
}

void VideoFramePool::ReleaseUnusedVAAPIFrames() {
  MutexAutoLock lock(mSurfaceLock);
  for (const auto& surface : mDMABufSurfaces) {
    if (!surface->IsUsed()) {
      surface->ReleaseVAAPIData();
    }
  }
}

RefPtr<VideoFrameSurface> VideoFramePool::GetFreeVideoFrameSurface() {
  for (auto& surface : mDMABufSurfaces) {
    if (surface->IsUsed()) {
      continue;
    }
    surface->ReleaseVAAPIData();
    return surface;
  }
  return nullptr;
}

RefPtr<VideoFrameSurface> VideoFramePool::GetVideoFrameSurface(
    VADRMPRIMESurfaceDescriptor& aVaDesc, AVCodecContext* aAVCodecContext,
    AVFrame* aAVFrame, FFmpegLibWrapper* aLib) {
  if (aVaDesc.fourcc != VA_FOURCC_NV12 && aVaDesc.fourcc != VA_FOURCC_YV12 &&
      aVaDesc.fourcc != VA_FOURCC_P010) {
    FFMPEG_LOG("Unsupported VA-API surface format %d", aVaDesc.fourcc);
    return nullptr;
  }

  MutexAutoLock lock(mSurfaceLock);
  RefPtr<VideoFrameSurface> videoSurface = GetFreeVideoFrameSurface();
  if (!videoSurface) {
    RefPtr<DMABufSurfaceYUV> surface =
        DMABufSurfaceYUV::CreateYUVSurface(aVaDesc);
    if (!surface) {
      return nullptr;
    }
    FFMPEG_LOG("Created new VA-API DMABufSurface UID = %d", surface->GetUID());
    RefPtr<VideoFrameSurface> surf = new VideoFrameSurface(surface);
    if (!mTextureCreationWorks) {
      mTextureCreationWorks = Some(surface->VerifyTextureCreation());
    }
    if (!*mTextureCreationWorks) {
      FFMPEG_LOG("  failed to create texture over DMABuf memory!");
      return nullptr;
    }
    videoSurface = surf;
    mDMABufSurfaces.AppendElement(std::move(surf));
  } else {
    RefPtr<DMABufSurfaceYUV> surface = videoSurface->GetDMABufSurface();
    if (!surface->UpdateYUVData(aVaDesc)) {
      return nullptr;
    }
    FFMPEG_LOG("Reusing VA-API DMABufSurface UID = %d", surface->GetUID());
  }
  videoSurface->LockVAAPIData(aAVCodecContext, aAVFrame, aLib);
  videoSurface->MarkAsUsed();
  return videoSurface;
}

}  // namespace mozilla
