/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 * Copyright (c) 2014, Mozilla
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 ** Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 ** Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in
 *  the documentation and/or other materials provided with the
 *  distribution.
 *
 ** Neither the name of Google nor the names of its contributors may
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GMP_VIDEO_FRAME_I420_h_
#define GMP_VIDEO_FRAME_I420_h_

#include "gmp-errors.h"
#include "gmp-video-frame.h"
#include "gmp-video-plane.h"

#include <stdint.h>

enum GMPPlaneType {
  kGMPYPlane = 0,
  kGMPUPlane = 1,
  kGMPVPlane = 2,
  kGMPNumOfPlanes = 3
};

// The implementation backing this interface uses shared memory for the
// buffer(s). This means it can only be used by the "owning" process.
// At first the process which created the object owns it. When the object
// is passed to an interface the creator loses ownership and must Destroy()
// the object. Further attempts to use it may fail due to not being able to
// access the underlying buffer(s).
//
// Methods that create or destroy shared memory must be called on the main
// thread. They are marked below.
class GMPVideoi420Frame : public GMPVideoFrame {
public:
  // MAIN THREAD ONLY
  // CreateEmptyFrame: Sets frame dimensions and allocates buffers based
  // on set dimensions - height and plane stride.
  // If required size is bigger than the allocated one, new buffers of adequate
  // size will be allocated.
  virtual GMPErr CreateEmptyFrame(int32_t aWidth, int32_t aHeight,
                                  int32_t aStride_y, int32_t aStride_u, int32_t aStride_v) = 0;

  // MAIN THREAD ONLY
  // CreateFrame: Sets the frame's members and buffers. If required size is
  // bigger than allocated one, new buffers of adequate size will be allocated.
  virtual GMPErr CreateFrame(int32_t aSize_y, const uint8_t* aBuffer_y,
                             int32_t aSize_u, const uint8_t* aBuffer_u,
                             int32_t aSize_v, const uint8_t* aBuffer_v,
                             int32_t aWidth, int32_t aHeight,
                             int32_t aStride_y, int32_t aStride_u, int32_t aStride_v) = 0;

  // MAIN THREAD ONLY
  // Copy frame: If required size is bigger than allocated one, new buffers of
  // adequate size will be allocated.
  virtual GMPErr CopyFrame(const GMPVideoi420Frame& aVideoFrame) = 0;

  // Swap Frame.
  virtual void SwapFrame(GMPVideoi420Frame* aVideoFrame) = 0;

  // Get pointer to buffer per plane.
  virtual uint8_t* Buffer(GMPPlaneType aType) = 0;

  // Overloading with const.
  virtual const uint8_t* Buffer(GMPPlaneType aType) const = 0;

  // Get allocated size per plane.
  virtual int32_t AllocatedSize(GMPPlaneType aType) const = 0;

  // Get allocated stride per plane.
  virtual int32_t Stride(GMPPlaneType aType) const = 0;

  // Set frame width.
  virtual GMPErr SetWidth(int32_t aWidth) = 0;

  // Set frame height.
  virtual GMPErr SetHeight(int32_t aHeight) = 0;

  // Get frame width.
  virtual int32_t Width() const = 0;

  // Get frame height.
  virtual int32_t Height() const = 0;

  // Set frame timestamp (microseconds)
  virtual void SetTimestamp(uint64_t aTimestamp) = 0;

  // Get frame timestamp (microseconds)
  virtual uint64_t Timestamp() const = 0;

  // Set frame duration (microseconds)
  // NOTE: next-frame's Timestamp() != this-frame's TimeStamp()+Duration()
  // depending on rounding to avoid having to track roundoff errors
  // and dropped/missing frames(!) (which may leave a large gap)
  virtual void SetDuration(uint64_t aDuration) = 0;

  // Get frame duration (microseconds)
  virtual uint64_t Duration() const = 0;

  // Return true if underlying plane buffers are of zero size, false if not.
  virtual bool IsZeroSize() const = 0;

  // Reset underlying plane buffers sizes to 0. This function doesn't clear memory.
  virtual void ResetSize() = 0;
};

#endif // GMP_VIDEO_FRAME_I420_h_
