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
  static const webrtc::PlaneType kPlanes[] =
                  {webrtc::kYPlane, webrtc::kUPlane, webrtc::kVPlane};
  if (aVideoFrame.IsZeroSize()) {
    return 0;
  }

  size_t sum = 0;
  for (auto plane : kPlanes) {
    sum += aVideoFrame.allocated_size(plane);
  }
  return sum;
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

  aDestProps.yAllocatedSize() = aVideoFrame.allocated_size(webrtc::kYPlane);
  aDestProps.uAllocatedSize() = aVideoFrame.allocated_size(webrtc::kYPlane);
  aDestProps.vAllocatedSize() = aVideoFrame.allocated_size(webrtc::kYPlane);

  aDestProps.width() = aVideoFrame.width();
  aDestProps.height() = aVideoFrame.height();

  aDestProps.yStride() = aVideoFrame.stride(webrtc::kYPlane);
  aDestProps.uStride() = aVideoFrame.stride(webrtc::kUPlane);
  aDestProps.vStride() = aVideoFrame.stride(webrtc::kVPlane);
}

void VideoFrameUtils::CopyVideoFrameBuffers(uint8_t* aDestBuffer,
                       const size_t aDestBufferSize,
                       const webrtc::VideoFrame& aFrame)
{
  static const webrtc::PlaneType planes[] = {webrtc::kYPlane, webrtc::kUPlane, webrtc::kVPlane};

  size_t aggregateSize = TotalRequiredBufferSize(aFrame);

  MOZ_ASSERT(aDestBufferSize >= aggregateSize);

  // If planes are ordered YUV and contiguous then do a single copy
  if ((aFrame.buffer(webrtc::kYPlane) != nullptr)
    // Check that the three planes are ordered
    && (aFrame.buffer(webrtc::kYPlane) < aFrame.buffer(webrtc::kUPlane))
    && (aFrame.buffer(webrtc::kUPlane) < aFrame.buffer(webrtc::kVPlane))
    //  Check that the last plane ends at firstPlane[totalsize]
    && (&aFrame.buffer(webrtc::kYPlane)[aggregateSize] == &aFrame.buffer(webrtc::kVPlane)[aFrame.allocated_size(webrtc::kVPlane)]))
  {
    memcpy(aDestBuffer,aFrame.buffer(webrtc::kYPlane),aggregateSize);
    return;
  }

  // Copy each plane
  size_t offset = 0;
  for (auto plane: planes) {
    memcpy(&aDestBuffer[offset], aFrame.buffer(plane), aFrame.allocated_size(plane));
    offset += aFrame.allocated_size(plane);
  }
}

void VideoFrameUtils::CopyVideoFrameBuffers(ShmemBuffer& aDestShmem,
                        const webrtc::VideoFrame& aVideoFrame)
{
  CopyVideoFrameBuffers(aDestShmem.Get().get<uint8_t>(), aDestShmem.Get().Size<uint8_t>(), aVideoFrame);
}

}
