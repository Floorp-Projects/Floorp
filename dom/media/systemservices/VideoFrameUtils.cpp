/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoFrameUtils.h"
#include "webrtc/video_frame.h"
#include "mozilla/ShmemPool.h"

namespace mozilla {

size_t
VideoFrameUtils::TotalRequiredBufferSize(
                  const webrtc::VideoFrame& aVideoFrame)
{
  auto height = aVideoFrame.video_frame_buffer()->height();
  return height * aVideoFrame.video_frame_buffer()->StrideY() +
    ((height+1)/2) * aVideoFrame.video_frame_buffer()->StrideU() +
    ((height+1)/2) * aVideoFrame.video_frame_buffer()->StrideV();
}

void VideoFrameUtils::InitFrameBufferProperties(
                  const webrtc::VideoFrame& aVideoFrame,
                  camera::VideoFrameProperties& aDestProps)
{
  // The VideoFrameBuffer image data stored in the accompanying buffer
  // the buffer is at least this size of larger.
  aDestProps.bufferSize() = TotalRequiredBufferSize(aVideoFrame);

  aDestProps.timeStamp() = aVideoFrame.timestamp();
  aDestProps.ntpTimeMs() = aVideoFrame.ntp_time_ms();
  aDestProps.renderTimeMs() = aVideoFrame.render_time_ms();

  aDestProps.rotation() = aVideoFrame.rotation();

  auto height = aVideoFrame.video_frame_buffer()->height();
  aDestProps.yAllocatedSize() = height * aVideoFrame.video_frame_buffer()->StrideY();
  aDestProps.uAllocatedSize() = ((height+1)/2) * aVideoFrame.video_frame_buffer()->StrideU();
  aDestProps.vAllocatedSize() = ((height+1)/2) * aVideoFrame.video_frame_buffer()->StrideV();

  aDestProps.width() = aVideoFrame.video_frame_buffer()->width();
  aDestProps.height() = height;

  aDestProps.yStride() = aVideoFrame.video_frame_buffer()->StrideY();
  aDestProps.uStride() = aVideoFrame.video_frame_buffer()->StrideU();
  aDestProps.vStride() = aVideoFrame.video_frame_buffer()->StrideV();
}

void VideoFrameUtils::CopyVideoFrameBuffers(uint8_t* aDestBuffer,
                       const size_t aDestBufferSize,
                       const webrtc::VideoFrame& aFrame)
{
  size_t aggregateSize = TotalRequiredBufferSize(aFrame);

  MOZ_ASSERT(aDestBufferSize >= aggregateSize);

  // If planes are ordered YUV and contiguous then do a single copy
  if ((aFrame.video_frame_buffer()->DataY() != nullptr)
      // Check that the three planes are ordered
      && (aFrame.video_frame_buffer()->DataY() < aFrame.video_frame_buffer()->DataU())
      && (aFrame.video_frame_buffer()->DataU() < aFrame.video_frame_buffer()->DataV())
      //  Check that the last plane ends at firstPlane[totalsize]
      && (&aFrame.video_frame_buffer()->DataY()[aggregateSize] ==
          &aFrame.video_frame_buffer()->DataV()[((aFrame.video_frame_buffer()->height()+1)/2) *
                                                aFrame.video_frame_buffer()->StrideV()]))
  {
    memcpy(aDestBuffer, aFrame.video_frame_buffer()->DataY(), aggregateSize);
    return;
  }

  // Copy each plane
  size_t offset = 0;
  size_t size;
  auto height = aFrame.video_frame_buffer()->height();
  size = height * aFrame.video_frame_buffer()->StrideY();
  memcpy(&aDestBuffer[offset], aFrame.video_frame_buffer()->DataY(), size);
  offset += size;
  size = ((height+1)/2) * aFrame.video_frame_buffer()->StrideU();
  memcpy(&aDestBuffer[offset], aFrame.video_frame_buffer()->DataU(), size);
  offset += size;
  size = ((height+1)/2) * aFrame.video_frame_buffer()->StrideV();
  memcpy(&aDestBuffer[offset], aFrame.video_frame_buffer()->DataV(), size);
}

void VideoFrameUtils::CopyVideoFrameBuffers(ShmemBuffer& aDestShmem,
                        const webrtc::VideoFrame& aVideoFrame)
{
  CopyVideoFrameBuffers(aDestShmem.Get().get<uint8_t>(), aDestShmem.Get().Size<uint8_t>(), aVideoFrame);
}

}
