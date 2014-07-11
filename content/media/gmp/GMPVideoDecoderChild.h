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

class GMPChild;

class GMPVideoDecoderChild : public PGMPVideoDecoderChild,
                             public GMPVideoDecoderCallback,
                             public GMPSharedMemManager
{
public:
  GMPVideoDecoderChild(GMPChild* aPlugin);
  virtual ~GMPVideoDecoderChild();

  void Init(GMPVideoDecoder* aDecoder);
  GMPVideoHostImpl& Host();

  // GMPVideoDecoderCallback
  virtual void Decoded(GMPVideoi420Frame* decodedFrame) MOZ_OVERRIDE;
  virtual void ReceivedDecodedReferenceFrame(const uint64_t pictureId) MOZ_OVERRIDE;
  virtual void ReceivedDecodedFrame(const uint64_t pictureId) MOZ_OVERRIDE;
  virtual void InputDataExhausted() MOZ_OVERRIDE;
  virtual void DrainComplete() MOZ_OVERRIDE;
  virtual void ResetComplete() MOZ_OVERRIDE;

  // GMPSharedMemManager
  virtual void CheckThread();
  virtual bool Alloc(size_t aSize, Shmem::SharedMemory::SharedMemoryType aType, Shmem* aMem)
  {
#ifndef SHMEM_ALLOC_IN_CHILD
    return CallNeedShmem(aSize, aMem);
#else
#ifdef GMP_SAFE_SHMEM
    return AllocShmem(aSize, aType, aMem);
#else
    return AllocUnsafeShmem(aSize, aType, aMem);
#endif
#endif
  }
  virtual void Dealloc(Shmem& aMem)
  {
#ifndef SHMEM_ALLOC_IN_CHILD
    SendParentShmemForPool(aMem);
#else
    DeallocShmem(aMem);
#endif
  }

private:
  // PGMPVideoDecoderChild
  virtual bool RecvInitDecode(const GMPVideoCodec& aCodecSettings,
                              const nsTArray<uint8_t>& aCodecSpecific,
                              const int32_t& aCoreCount) MOZ_OVERRIDE;
  virtual bool RecvDecode(const GMPVideoEncodedFrameData& aInputFrame,
                          const bool& aMissingFrames,
                          const nsTArray<uint8_t>& aCodecSpecificInfo,
                          const int64_t& aRenderTimeMs) MOZ_OVERRIDE;
  virtual bool RecvChildShmemForPool(Shmem& aFrameBuffer) MOZ_OVERRIDE;
  virtual bool RecvReset() MOZ_OVERRIDE;
  virtual bool RecvDrain() MOZ_OVERRIDE;
  virtual bool RecvDecodingComplete() MOZ_OVERRIDE;

  GMPChild* mPlugin;
  GMPVideoDecoder* mVideoDecoder;
  GMPVideoHostImpl mVideoHost;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoDecoderChild_h_
