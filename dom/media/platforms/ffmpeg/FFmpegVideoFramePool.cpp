/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoFramePool.h"
#include "FFmpegLog.h"
#include "mozilla/widget/DMABufLibWrapper.h"
#include "libavutil/pixfmt.h"

namespace mozilla {

RefPtr<layers::Image> VideoFrameSurfaceDMABuf::GetAsImage() {
  return new layers::DMABUFSurfaceImage(mSurface);
}

VideoFrameSurfaceDMABuf::VideoFrameSurfaceDMABuf(DMABufSurface* aSurface)
    : mSurface(aSurface) {
  // Create global refcount object to track mSurface usage over
  // gects rendering engine. We can't release it until it's used
  // by GL compositor / WebRender.
  MOZ_ASSERT(mSurface);
  MOZ_RELEASE_ASSERT(mSurface->GetAsDMABufSurfaceYUV());
  mSurface->GlobalRefCountCreate();
  FFMPEG_LOG("VideoFrameSurfaceDMABuf: creating surface UID = %d",
             mSurface->GetUID());
}

VideoFrameSurfaceVAAPI::VideoFrameSurfaceVAAPI(DMABufSurface* aSurface)
    : VideoFrameSurfaceDMABuf(aSurface) {}

void VideoFrameSurfaceVAAPI::LockVAAPIData(AVCodecContext* aAVCodecContext,
                                           AVFrame* aAVFrame,
                                           FFmpegLibWrapper* aLib) {
  FFMPEG_LOG("VideoFrameSurfaceVAAPI: VAAPI locking dmabuf surface UID = %d",
             mSurface->GetUID());
  mLib = aLib;
  mAVHWFramesContext = aLib->av_buffer_ref(aAVCodecContext->hw_frames_ctx);
  mHWAVBuffer = aLib->av_buffer_ref(aAVFrame->buf[0]);
}

void VideoFrameSurfaceVAAPI::ReleaseVAAPIData(bool aForFrameRecycle) {
  FFMPEG_LOG("VideoFrameSurfaceVAAPI: VAAPI releasing dmabuf surface UID = %d",
             mSurface->GetUID());

  // It's possible to unref GPU data while IsUsed() is still set.
  // It can happens when VideoFramePool is deleted while decoder shutdown
  // but related dmabuf surfaces are still used in another process.
  // In such case we don't care as the dmabuf surface will not be
  // recycled for another frame and stays here untill last fd of it
  // is closed.
  mLib->av_buffer_unref(&mHWAVBuffer);
  mLib->av_buffer_unref(&mAVHWFramesContext);

  if (aForFrameRecycle) {
    // If we want to recycle the frame, make sure it's not used
    // by gecko rendering pipeline.
    MOZ_DIAGNOSTIC_ASSERT(!IsUsed());
    mSurface->ReleaseSurface();
  }
}

VideoFrameSurfaceVAAPI::~VideoFrameSurfaceVAAPI() {
  FFMPEG_LOG("VideoFrameSurfaceVAAPI: deleting dmabuf surface UID = %d",
             mSurface->GetUID());
  // We're about to quit, no need to recycle the frames.
  ReleaseVAAPIData(/* aForFrameRecycle */ false);
}

VideoFramePool::VideoFramePool(bool aUseVAAPI) : mUseVAAPI(aUseVAAPI) {}

VideoFramePool::~VideoFramePool() { mDMABufSurfaces.Clear(); }

void VideoFramePool::ReleaseUnusedVAAPIFrames() {
  if (!mUseVAAPI) {
    return;
  }
  for (const auto& surface : mDMABufSurfaces) {
    if (!surface->IsUsed()) {
      surface->ReleaseVAAPIData();
    }
  }
}

RefPtr<VideoFrameSurface> VideoFramePool::GetFreeVideoFrameSurface() {
  int len = mDMABufSurfaces.Length();
  for (int i = 0; i < len; i++) {
    if (!mDMABufSurfaces[i]->IsUsed()) {
      return mDMABufSurfaces[i];
    }
  }
  return nullptr;
}

RefPtr<VideoFrameSurface> VideoFramePool::GetVideoFrameSurface(
    VADRMPRIMESurfaceDescriptor& aVaDesc) {
  // VADRMPRIMESurfaceDescriptor can be used with VA-API only.
  MOZ_ASSERT(mUseVAAPI);

  auto videoSurface = GetFreeVideoFrameSurface();
  if (!videoSurface) {
    RefPtr<DMABufSurfaceYUV> surface =
        DMABufSurfaceYUV::CreateYUVSurface(aVaDesc);
    if (!surface) {
      return nullptr;
    }
    FFMPEG_LOG("Created new VA-API DMABufSurface UID = %d", surface->GetUID());
    videoSurface = new VideoFrameSurfaceVAAPI(surface);
    mDMABufSurfaces.AppendElement(videoSurface);
    return videoSurface;
  }

  // Release VAAPI surface data before we reuse it.
  videoSurface->ReleaseVAAPIData();

  RefPtr<DMABufSurfaceYUV> surface = videoSurface->GetDMABufSurface();
  if (!surface->UpdateYUVData(aVaDesc)) {
    return nullptr;
  }
  FFMPEG_LOG("Reusing VA-API DMABufSurface UID = %d", surface->GetUID());
  return videoSurface;
}

RefPtr<VideoFrameSurface> VideoFramePool::GetVideoFrameSurface(
    AVPixelFormat aPixelFormat, AVFrame* aFrame) {
  // We should not use SW surfaces when VA-API is enabled.
  MOZ_ASSERT(!mUseVAAPI);
  MOZ_ASSERT(aFrame);

  // With SW decode we support only YUV420P format with DMABuf surfaces.
  if (aPixelFormat != AV_PIX_FMT_YUV420P) {
    return nullptr;
  }

  auto videoSurface = GetFreeVideoFrameSurface();
  if (!videoSurface) {
    RefPtr<DMABufSurfaceYUV> surface = DMABufSurfaceYUV::CreateYUVSurface(
        aFrame->width, aFrame->height, (void**)aFrame->data, aFrame->linesize);
    if (!surface) {
      return nullptr;
    }
    FFMPEG_LOG("Created new SW DMABufSurface UID = %d", surface->GetUID());
    videoSurface = new VideoFrameSurfaceDMABuf(surface);
    mDMABufSurfaces.AppendElement(videoSurface);
    return videoSurface;
  }

  RefPtr<DMABufSurfaceYUV> surface = videoSurface->GetDMABufSurface();
  if (!surface->UpdateYUVData((void**)aFrame->data, aFrame->linesize)) {
    return nullptr;
  }
  FFMPEG_LOG("Reusing SW DMABufSurface UID = %d", surface->GetUID());
  return videoSurface;
}

}  // namespace mozilla
