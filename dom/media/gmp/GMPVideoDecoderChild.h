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
  void Decoded(GMPVideoi420Frame* decodedFrame) override;
  void ReceivedDecodedReferenceFrame(const uint64_t pictureId) override;
  void ReceivedDecodedFrame(const uint64_t pictureId) override;
  void InputDataExhausted() override;
  void DrainComplete() override;
  void ResetComplete() override;
  void Error(GMPErr aError) override;

  // GMPSharedMemManager
  bool Alloc(size_t aSize, Shmem::SharedMemory::SharedMemoryType aType, Shmem* aMem) override;
  void Dealloc(Shmem& aMem) override;

private:
  virtual ~GMPVideoDecoderChild();

  // PGMPVideoDecoderChild
  mozilla::ipc::IPCResult RecvInitDecode(const GMPVideoCodec& aCodecSettings,
                                         InfallibleTArray<uint8_t>&& aCodecSpecific,
                                         const int32_t& aCoreCount) override;
  mozilla::ipc::IPCResult RecvDecode(const GMPVideoEncodedFrameData& aInputFrame,
                                     const bool& aMissingFrames,
                                     InfallibleTArray<uint8_t>&& aCodecSpecificInfo,
                                     const int64_t& aRenderTimeMs) override;
  mozilla::ipc::IPCResult RecvChildShmemForPool(Shmem&& aFrameBuffer) override;
  mozilla::ipc::IPCResult RecvReset() override;
  mozilla::ipc::IPCResult RecvDrain() override;
  mozilla::ipc::IPCResult RecvDecodingComplete() override;

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
