/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright (c) 2014, Mozilla Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GMPVideoEncodedFrameImpl_h_
#define GMPVideoEncodedFrameImpl_h_

#include "gmp-video-errors.h"
#include "gmp-video-frame.h"
#include "gmp-video-frame-encoded.h"
#include "mozilla/ipc/Shmem.h"

namespace mozilla {
namespace gmp {

class GMPVideoHostImpl;
class GMPVideoEncodedFrameData;

class GMPVideoEncodedFrameImpl: public GMPVideoEncodedFrame
{
  friend struct IPC::ParamTraits<mozilla::gmp::GMPVideoEncodedFrameImpl>;
public:
  GMPVideoEncodedFrameImpl(GMPVideoHostImpl* aHost);
  GMPVideoEncodedFrameImpl(const GMPVideoEncodedFrameData& aFrameData, GMPVideoHostImpl* aHost);
  virtual ~GMPVideoEncodedFrameImpl();

  // This is called during a normal destroy sequence, which is
  // when a consumer is finished or during XPCOM shutdown.
  void DoneWithAPI();
  // Does not attempt to release Shmem, as the Shmem has already been released.
  void ActorDestroyed();

  bool RelinquishFrameData(GMPVideoEncodedFrameData& aFrameData);

  // GMPVideoFrame
  virtual GMPVideoFrameFormat GetFrameFormat() MOZ_OVERRIDE;
  virtual void Destroy() MOZ_OVERRIDE;

  // GMPVideoEncodedFrame
  virtual GMPVideoErr CreateEmptyFrame(uint32_t aSize) MOZ_OVERRIDE;
  virtual GMPVideoErr CopyFrame(const GMPVideoEncodedFrame& aFrame) MOZ_OVERRIDE;
  virtual void     SetEncodedWidth(uint32_t aEncodedWidth) MOZ_OVERRIDE;
  virtual uint32_t EncodedWidth() MOZ_OVERRIDE;
  virtual void     SetEncodedHeight(uint32_t aEncodedHeight) MOZ_OVERRIDE;
  virtual uint32_t EncodedHeight() MOZ_OVERRIDE;
  virtual void     SetTimeStamp(uint32_t aTimeStamp) MOZ_OVERRIDE;
  virtual uint32_t TimeStamp() MOZ_OVERRIDE;
  virtual void     SetCaptureTime(int64_t aCaptureTime) MOZ_OVERRIDE;
  virtual int64_t  CaptureTime() MOZ_OVERRIDE;
  virtual void     SetFrameType(GMPVideoFrameType aFrameType) MOZ_OVERRIDE;
  virtual GMPVideoFrameType FrameType() MOZ_OVERRIDE;
  virtual void     SetAllocatedSize(uint32_t aNewSize) MOZ_OVERRIDE;
  virtual uint32_t AllocatedSize() MOZ_OVERRIDE;
  virtual void     SetSize(uint32_t aSize) MOZ_OVERRIDE;
  virtual uint32_t Size() MOZ_OVERRIDE;
  virtual void     SetCompleteFrame(bool aCompleteFrame) MOZ_OVERRIDE;
  virtual bool     CompleteFrame() MOZ_OVERRIDE;
  virtual const uint8_t* Buffer() const MOZ_OVERRIDE;
  virtual uint8_t* Buffer() MOZ_OVERRIDE;

private:
  void DestroyBuffer();

  uint32_t mEncodedWidth;
  uint32_t mEncodedHeight;
  uint32_t mTimeStamp;
  int64_t  mCaptureTime_ms;
  GMPVideoFrameType mFrameType;
  uint32_t mSize;
  bool     mCompleteFrame;
  GMPVideoHostImpl* mHost;
  ipc::Shmem mBuffer;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoEncodedFrameImpl_h_
