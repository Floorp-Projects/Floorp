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

#ifndef GMP_VIDEO_FRAME_ENCODED_h_
#define GMP_VIDEO_FRAME_ENCODED_h_

#include <stdint.h>
#include "gmp-decryption.h"
#include "gmp-video-frame.h"
#include "gmp-video-codec.h"

enum GMPVideoFrameType
{
  kGMPKeyFrame = 0,
  kGMPDeltaFrame = 1,
  kGMPGoldenFrame = 2,
  kGMPAltRefFrame = 3,
  kGMPSkipFrame = 4
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
class GMPVideoEncodedFrame : public GMPVideoFrame
{
public:
  // MAIN THREAD ONLY
  virtual GMPErr CreateEmptyFrame(uint32_t aSize) = 0;
  // MAIN THREAD ONLY
  virtual GMPErr CopyFrame(const GMPVideoEncodedFrame& aVideoFrame) = 0;
  virtual void     SetEncodedWidth(uint32_t aEncodedWidth) = 0;
  virtual uint32_t EncodedWidth() = 0;
  virtual void     SetEncodedHeight(uint32_t aEncodedHeight) = 0;
  virtual uint32_t EncodedHeight() = 0;
  // Microseconds
  virtual void     SetTimeStamp(uint64_t aTimeStamp) = 0;
  virtual uint64_t TimeStamp() = 0;
  // Set frame duration (microseconds)
  // NOTE: next-frame's Timestamp() != this-frame's TimeStamp()+Duration()
  // depending on rounding to avoid having to track roundoff errors
  // and dropped/missing frames(!) (which may leave a large gap)
  virtual void     SetDuration(uint64_t aDuration) = 0;
  virtual uint64_t Duration() const = 0;
  virtual void     SetFrameType(GMPVideoFrameType aFrameType) = 0;
  virtual GMPVideoFrameType FrameType() = 0;
  virtual void     SetAllocatedSize(uint32_t aNewSize) = 0;
  virtual uint32_t AllocatedSize() = 0;
  virtual void     SetSize(uint32_t aSize) = 0;
  virtual uint32_t Size() = 0;
  virtual void     SetCompleteFrame(bool aCompleteFrame) = 0;
  virtual bool     CompleteFrame() = 0;
  virtual const uint8_t* Buffer() const = 0;
  virtual uint8_t*       Buffer() = 0;
  virtual GMPBufferType  BufferType() const = 0;
  virtual void     SetBufferType(GMPBufferType aBufferType) = 0;

  // Get data describing how this frame is encrypted, or nullptr if the
  // frame is not encrypted.
  virtual const GMPEncryptedBufferData* GetDecryptionData() const = 0;
};

#endif // GMP_VIDEO_FRAME_ENCODED_h_
