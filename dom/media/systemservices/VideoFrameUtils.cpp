/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoFrameUtils.h"
#include "api/video/video_frame.h"
#include "mozilla/ShmemPool.h"

namespace mozilla {

uint32_t VideoFrameUtils::TotalRequiredBufferSize(
    const webrtc::VideoFrame& aVideoFrame) {
  auto i420 = aVideoFrame.video_frame_buffer()->ToI420();
  auto height = i420->height();
  size_t size = height * i420->StrideY() +
                ((height + 1) / 2) * i420->StrideU() +
                ((height + 1) / 2) * i420->StrideV();
  MOZ_RELEASE_ASSERT(size < std::numeric_limits<uint32_t>::max());
  return static_cast<uint32_t>(size);
}

void VideoFrameUtils::InitFrameBufferProperties(
    const webrtc::VideoFrame& aVideoFrame,
    camera::VideoFrameProperties& aDestProps) {
  // The VideoFrameBuffer image data stored in the accompanying buffer
  // the buffer is at least this size of larger.
  aDestProps.bufferSize() = TotalRequiredBufferSize(aVideoFrame);

  aDestProps.timeStamp() = aVideoFrame.timestamp();
  aDestProps.ntpTimeMs() = aVideoFrame.ntp_time_ms();
  aDestProps.renderTimeMs() = aVideoFrame.render_time_ms();

  aDestProps.rotation() = aVideoFrame.rotation();

  auto i420 = aVideoFrame.video_frame_buffer()->ToI420();
  auto height = i420->height();
  aDestProps.yAllocatedSize() = height * i420->StrideY();
  aDestProps.uAllocatedSize() = ((height + 1) / 2) * i420->StrideU();
  aDestProps.vAllocatedSize() = ((height + 1) / 2) * i420->StrideV();

  aDestProps.width() = i420->width();
  aDestProps.height() = height;

  aDestProps.yStride() = i420->StrideY();
  aDestProps.uStride() = i420->StrideU();
  aDestProps.vStride() = i420->StrideV();
}

void VideoFrameUtils::CopyVideoFrameBuffers(uint8_t* aDestBuffer,
                                            const size_t aDestBufferSize,
                                            const webrtc::VideoFrame& aFrame) {
  size_t aggregateSize = TotalRequiredBufferSize(aFrame);

  MOZ_ASSERT(aDestBufferSize >= aggregateSize);
  auto i420 = aFrame.video_frame_buffer()->ToI420();

  // If planes are ordered YUV and contiguous then do a single copy
  if ((i420->DataY() != nullptr) &&
      // Check that the three planes are ordered
      (i420->DataY() < i420->DataU()) && (i420->DataU() < i420->DataV()) &&
      //  Check that the last plane ends at firstPlane[totalsize]
      (&i420->DataY()[aggregateSize] ==
       &i420->DataV()[((i420->height() + 1) / 2) * i420->StrideV()])) {
    memcpy(aDestBuffer, i420->DataY(), aggregateSize);
    return;
  }

  // Copy each plane
  size_t offset = 0;
  size_t size;
  auto height = i420->height();
  size = height * i420->StrideY();
  memcpy(&aDestBuffer[offset], i420->DataY(), size);
  offset += size;
  size = ((height + 1) / 2) * i420->StrideU();
  memcpy(&aDestBuffer[offset], i420->DataU(), size);
  offset += size;
  size = ((height + 1) / 2) * i420->StrideV();
  memcpy(&aDestBuffer[offset], i420->DataV(), size);
}

void VideoFrameUtils::CopyVideoFrameBuffers(
    ShmemBuffer& aDestShmem, const webrtc::VideoFrame& aVideoFrame) {
  CopyVideoFrameBuffers(aDestShmem.Get().get<uint8_t>(),
                        aDestShmem.Get().Size<uint8_t>(), aVideoFrame);
}

}  // namespace mozilla
