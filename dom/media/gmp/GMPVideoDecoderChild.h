/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoDecoderChild_h_
#define GMPVideoDecoderChild_h_

#include "nsString.h"
#include "mozilla/gmp/PGMPVideoDecoderChild.h"
#include "gmp-video-decode.h"
#include "GMPSharedMemManager.h"
#include "GMPVideoHost.h"
#include "mozilla/gmp/GMPTypes.h"

namespace mozilla {
namespace gmp {

class GMPContentChild;

class GMPVideoDecoderChild : public PGMPVideoDecoderChild,
                             public GMPVideoDecoderCallback,
                             public GMPSharedMemManager
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPVideoDecoderChild);

  explicit GMPVideoDecoderChild(GMPContentChild* aPlugin);

  void Init(GMPVideoDecoder* aDecoder);
  GMPVideoHostImpl& Host();

  // GMPVideoDecoderCallback
  virtual void Decoded(GMPVideoi420Frame* decodedFrame) override;
  virtual void ReceivedDecodedReferenceFrame(const uint64_t pictureId) override;
  virtual void ReceivedDecodedFrame(const uint64_t pictureId) override;
  virtual void InputDataExhausted() override;
  virtual void DrainComplete() override;
  virtual void ResetComplete() override;
  virtual void Error(GMPErr aError) override;

  // GMPSharedMemManager
  virtual bool Alloc(size_t aSize, Shmem::SharedMemory::SharedMemoryType aType, Shmem* aMem) override;
  virtual void Dealloc(Shmem& aMem) override;

private:
  virtual ~GMPVideoDecoderChild();

  // PGMPVideoDecoderChild
  virtual bool RecvInitDecode(const GMPVideoCodec& aCodecSettings,
                              InfallibleTArray<uint8_t>&& aCodecSpecific,
                              const int32_t& aCoreCount) override;
  virtual bool RecvDecode(const GMPVideoEncodedFrameData& aInputFrame,
                          const bool& aMissingFrames,
                          InfallibleTArray<uint8_t>&& aCodecSpecificInfo,
                          const int64_t& aRenderTimeMs) override;
  virtual bool RecvChildShmemForPool(Shmem&& aFrameBuffer) override;
  virtual bool RecvReset() override;
  virtual bool RecvDrain() override;
  virtual bool RecvDecodingComplete() override;

  GMPContentChild* mPlugin;
  GMPVideoDecoder* mVideoDecoder;
  GMPVideoHostImpl mVideoHost;

  // Non-zero when a GMP is blocked spinning the IPC message loop while
  // waiting on an NeedShmem to complete.
  int mNeedShmemIntrCount;
  bool mPendingDecodeComplete;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoDecoderChild_h_
