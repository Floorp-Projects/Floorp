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
#include "GMPUtils.h"
#include "GMPVideoHost.h"
#include "GMPVideoEncoderProxy.h"
#include "GMPCrashHelperHolder.h"

namespace mozilla {
namespace gmp {

class GMPContentParent;

class GMPVideoEncoderParent : public GMPVideoEncoderProxy,
                              public PGMPVideoEncoderParent,
                              public GMPSharedMemManager,
                              public GMPCrashHelperHolder
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPVideoEncoderParent)

  explicit GMPVideoEncoderParent(GMPContentParent *aPlugin);

  GMPVideoHostImpl& Host();
  void Shutdown();

  // GMPVideoEncoderProxy
  void Close() override;
  GMPErr InitEncode(const GMPVideoCodec& aCodecSettings,
                    const nsTArray<uint8_t>& aCodecSpecific,
                    GMPVideoEncoderCallbackProxy* aCallback,
                    int32_t aNumberOfCores,
                    uint32_t aMaxPayloadSize) override;
  GMPErr Encode(GMPUniquePtr<GMPVideoi420Frame> aInputFrame,
                const nsTArray<uint8_t>& aCodecSpecificInfo,
                const nsTArray<GMPVideoFrameType>& aFrameTypes) override;
  GMPErr SetChannelParameters(uint32_t aPacketLoss, uint32_t aRTT) override;
  GMPErr SetRates(uint32_t aNewBitRate, uint32_t aFrameRate) override;
  GMPErr SetPeriodicKeyFrames(bool aEnable) override;
  uint32_t GetPluginId() const override { return mPluginId; }

  // GMPSharedMemManager
  bool Alloc(size_t aSize, Shmem::SharedMemory::SharedMemoryType aType, Shmem* aMem) override
  {
#ifdef GMP_SAFE_SHMEM
    return AllocShmem(aSize, aType, aMem);
#else
    return AllocUnsafeShmem(aSize, aType, aMem);
#endif
  }
  void Dealloc(Shmem& aMem) override
  {
    DeallocShmem(aMem);
  }

private:
  virtual ~GMPVideoEncoderParent();

  // PGMPVideoEncoderParent
  void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvEncoded(const GMPVideoEncodedFrameData& aEncodedFrame,
                                      InfallibleTArray<uint8_t>&& aCodecSpecificInfo) override;
  mozilla::ipc::IPCResult RecvError(const GMPErr& aError) override;
  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvParentShmemForPool(Shmem&& aFrameBuffer) override;
  mozilla::ipc::IPCResult AnswerNeedShmem(const uint32_t& aEncodedBufferSize,
                                          Shmem* aMem) override;
  mozilla::ipc::IPCResult Recv__delete__() override;

  bool mIsOpen;
  bool mShuttingDown;
  bool mActorDestroyed;
  RefPtr<GMPContentParent> mPlugin;
  GMPVideoEncoderCallbackProxy* mCallback;
  GMPVideoHostImpl mVideoHost;
  nsCOMPtr<nsIThread> mEncodedThread;
  const uint32_t mPluginId;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoEncoderParent_h_
