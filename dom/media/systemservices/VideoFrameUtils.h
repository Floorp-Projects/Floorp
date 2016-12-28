/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_VideoFrameUtil_h
#define mozilla_VideoFrameUtil_h

#include "mozilla/camera/PCameras.h"

namespace webrtc {
  class VideoFrame;
}

namespace mozilla
{
  class ShmemBuffer;

// Util methods for working with webrtc::VideoFrame(s) and
// the IPC classes that are used to deliver their contents to the
// MediaEnginge

class VideoFrameUtils {
public:

  // Returns the total number of bytes necessary to copy a VideoFrame's buffer
  // across all planes.
  static size_t TotalRequiredBufferSize(const webrtc::VideoFrame & frame);

  // Initializes a camera::VideoFrameProperties from a VideoFrameBuffer
  static void InitFrameBufferProperties(const webrtc::VideoFrame& aVideoFrame,
                camera::VideoFrameProperties & aDestProperties);

  // Copies the buffers out of a VideoFrameBuffer into a buffer.
  // Attempts to make as few memcopies as possible.
  static void CopyVideoFrameBuffers(uint8_t * aDestBuffer,
                         const size_t aDestBufferSize,
                         const webrtc::VideoFrame & aVideoFrame);

  // Copies the buffers in a VideoFrameBuffer into a Shmem
  // returns the eno from the underlying memcpy.
  static void CopyVideoFrameBuffers(ShmemBuffer & aDestShmem,
                         const webrtc::VideoFrame & aVideoFrame);


};

} /* namespace mozilla */

#endif
