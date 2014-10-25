/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoEncoderParent_h_
#define GMPVideoEncoderParent_h_

#include "mozilla/RefPtr.h"
#include "gmp-video-encode.h"
#include "mozilla/gmp/PGMPVideoEncoderParent.h"
#include "GMPMessageUtils.h"
#include "GMPSharedMemManager.h"
#include "GMPVideoHost.h"
#include "GMPVideoEncoderProxy.h"

namespace mozilla {
namespace gmp {

class GMPParent;

class GMPVideoEncoderParent : public GMPVideoEncoderProxy,
                              public PGMPVideoEncoderParent,
                              public GMPSharedMemManager
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPVideoEncoderParent)

  explicit GMPVideoEncoderParent(GMPParent *aPlugin);

  GMPVideoHostImpl& Host();
  void Shutdown();

  // GMPVideoEncoderProxy
  virtual void Close() MOZ_OVERRIDE;
  virtual GMPErr InitEncode(const GMPVideoCodec& aCodecSettings,
                            const nsTArray<uint8_t>& aCodecSpecific,
                            GMPVideoEncoderCallbackProxy* aCallback,
                            int32_t aNumberOfCores,
                            uint32_t aMaxPayloadSize) MOZ_OVERRIDE;
  virtual GMPErr Encode(UniquePtr<GMPVideoi420Frame> aInputFrame,
                        const nsTArray<uint8_t>& aCodecSpecificInfo,
                        const nsTArray<GMPVideoFrameType>& aFrameTypes) MOZ_OVERRIDE;
  virtual GMPErr SetChannelParameters(uint32_t aPacketLoss, uint32_t aRTT) MOZ_OVERRIDE;
  virtual GMPErr SetRates(uint32_t aNewBitRate, uint32_t aFrameRate) MOZ_OVERRIDE;
  virtual GMPErr SetPeriodicKeyFrames(bool aEnable) MOZ_OVERRIDE;
  virtual const uint64_t ParentID() MOZ_OVERRIDE { return reinterpret_cast<uint64_t>(mPlugin.get()); }

  // GMPSharedMemManager
  virtual bool Alloc(size_t aSize, Shmem::SharedMemory::SharedMemoryType aType, Shmem* aMem) MOZ_OVERRIDE
  {
#ifdef GMP_SAFE_SHMEM
    return AllocShmem(aSize, aType, aMem);
#else
    return AllocUnsafeShmem(aSize, aType, aMem);
#endif
  }
  virtual void Dealloc(Shmem& aMem) MOZ_OVERRIDE
  {
    DeallocShmem(aMem);
  }

private:
  virtual ~GMPVideoEncoderParent();

  // PGMPVideoEncoderParent
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual bool RecvEncoded(const GMPVideoEncodedFrameData& aEncodedFrame,
                           const nsTArray<uint8_t>& aCodecSpecificInfo) MOZ_OVERRIDE;
  virtual bool RecvError(const GMPErr& aError) MOZ_OVERRIDE;
  virtual bool RecvParentShmemForPool(Shmem& aFrameBuffer) MOZ_OVERRIDE;
  virtual bool AnswerNeedShmem(const uint32_t& aEncodedBufferSize,
                               Shmem* aMem) MOZ_OVERRIDE;
  virtual bool Recv__delete__() MOZ_OVERRIDE;

  bool mIsOpen;
  bool mShuttingDown;
  nsRefPtr<GMPParent> mPlugin;
  GMPVideoEncoderCallbackProxy* mCallback;
  GMPVideoHostImpl mVideoHost;
  nsCOMPtr<nsIThread> mEncodedThread;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoEncoderParent_h_
