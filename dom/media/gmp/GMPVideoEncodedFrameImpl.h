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

#include "gmp-errors.h"
#include "gmp-video-frame.h"
#include "gmp-video-frame-encoded.h"
#include "gmp-decryption.h"
#include "mozilla/ipc/Shmem.h"

namespace mozilla {
class CryptoSample;

namespace gmp {

class GMPVideoHostImpl;
class GMPVideoEncodedFrameData;
class GMPEncryptedBufferDataImpl;

class GMPVideoEncodedFrameImpl: public GMPVideoEncodedFrame
{
  friend struct IPC::ParamTraits<mozilla::gmp::GMPVideoEncodedFrameImpl>;
public:
  explicit GMPVideoEncodedFrameImpl(GMPVideoHostImpl* aHost);
  GMPVideoEncodedFrameImpl(const GMPVideoEncodedFrameData& aFrameData, GMPVideoHostImpl* aHost);
  virtual ~GMPVideoEncodedFrameImpl();

  void InitCrypto(const CryptoSample& aCrypto);

  // This is called during a normal destroy sequence, which is
  // when a consumer is finished or during XPCOM shutdown.
  void DoneWithAPI();
  // Does not attempt to release Shmem, as the Shmem has already been released.
  void ActorDestroyed();

  bool RelinquishFrameData(GMPVideoEncodedFrameData& aFrameData);

  // GMPVideoFrame
  GMPVideoFrameFormat GetFrameFormat() override;
  void Destroy() override;

  // GMPVideoEncodedFrame
  GMPErr   CreateEmptyFrame(uint32_t aSize) override;
  GMPErr   CopyFrame(const GMPVideoEncodedFrame& aFrame) override;
  void     SetEncodedWidth(uint32_t aEncodedWidth) override;
  uint32_t EncodedWidth() override;
  void     SetEncodedHeight(uint32_t aEncodedHeight) override;
  uint32_t EncodedHeight() override;
  // Microseconds
  void     SetTimeStamp(uint64_t aTimeStamp) override;
  uint64_t TimeStamp() override;
  // Set frame duration (microseconds)
  // NOTE: next-frame's Timestamp() != this-frame's TimeStamp()+Duration()
  // depending on rounding to avoid having to track roundoff errors
  // and dropped/missing frames(!) (which may leave a large gap)
  void     SetDuration(uint64_t aDuration) override;
  uint64_t Duration() const override;
  void     SetFrameType(GMPVideoFrameType aFrameType) override;
  GMPVideoFrameType FrameType() override;
  void     SetAllocatedSize(uint32_t aNewSize) override;
  uint32_t AllocatedSize() override;
  void     SetSize(uint32_t aSize) override;
  uint32_t Size() override;
  void     SetCompleteFrame(bool aCompleteFrame) override;
  bool     CompleteFrame() override;
  const uint8_t* Buffer() const override;
  uint8_t* Buffer() override;
  GMPBufferType BufferType() const override;
  void     SetBufferType(GMPBufferType aBufferType) override;
  const    GMPEncryptedBufferMetadata* GetDecryptionData() const override;

private:
  void DestroyBuffer();

  uint32_t mEncodedWidth;
  uint32_t mEncodedHeight;
  uint64_t mTimeStamp;
  uint64_t mDuration;
  GMPVideoFrameType mFrameType;
  uint32_t mSize;
  bool     mCompleteFrame;
  GMPVideoHostImpl* mHost;
  ipc::Shmem mBuffer;
  GMPBufferType mBufferType;
  nsAutoPtr<GMPEncryptedBufferDataImpl> mCrypto;
};

} // namespace gmp

} // namespace mozilla

#endif // GMPVideoEncodedFrameImpl_h_
