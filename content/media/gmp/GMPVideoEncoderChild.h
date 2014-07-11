/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoEncoderChild_h_
#define GMPVideoEncoderChild_h_

#include "nsString.h"
#include "mozilla/gmp/PGMPVideoEncoderChild.h"
#include "gmp-video-encode.h"
#include "GMPSharedMemManager.h"
#include "GMPVideoHost.h"

namespace mozilla {
namespace gmp {

class GMPChild;

class GMPVideoEncoderChild : public PGMPVideoEncoderChild,
                             public GMPVideoEncoderCallback,
                             public GMPSharedMemManager
{
public:
  GMPVideoEncoderChild(GMPChild* aPlugin);
  virtual ~GMPVideoEncoderChild();

  void Init(GMPVideoEncoder* aEncoder);
  GMPVideoHostImpl& Host();

  // GMPVideoEncoderCallback
  virtual void Encoded(GMPVideoEncodedFrame* aEncodedFrame,
                       const uint8_t* aCodecSpecificInfo,
                       uint32_t aCodecSpecificInfoLength) MOZ_OVERRIDE;

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
  // PGMPVideoEncoderChild
  virtual bool RecvInitEncode(const GMPVideoCodec& aCodecSettings,
                              const nsTArray<uint8_t>& aCodecSpecific,
                              const int32_t& aNumberOfCores,
                              const uint32_t& aMaxPayloadSize) MOZ_OVERRIDE;
  virtual bool RecvEncode(const GMPVideoi420FrameData& aInputFrame,
                          const nsTArray<uint8_t>& aCodecSpecificInfo,
                          const nsTArray<GMPVideoFrameType>& aFrameTypes) MOZ_OVERRIDE;
  virtual bool RecvChildShmemForPool(Shmem& aEncodedBuffer) MOZ_OVERRIDE;
  virtual bool RecvSetChannelParameters(const uint32_t& aPacketLoss,
                                        const uint32_t& aRTT) MOZ_OVERRIDE;
  virtual bool RecvSetRates(const uint32_t& aNewBitRate,
                            const uint32_t& aFrameRate) MOZ_OVERRIDE;
  virtual bool RecvSetPeriodicKeyFrames(const bool& aEnable) MOZ_OVERRIDE;
  virtual bool RecvEncodingComplete() MOZ_OVERRIDE;

  GMPChild* mPlugin;
  GMPVideoEncoder* mVideoEncoder;
  GMPVideoHostImpl mVideoHost;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoEncoderChild_h_
