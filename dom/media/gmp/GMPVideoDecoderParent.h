/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoDecoderParent_h_
#define GMPVideoDecoderParent_h_

#include "mozilla/RefPtr.h"
#include "gmp-video-decode.h"
#include "mozilla/gmp/PGMPVideoDecoderParent.h"
#include "GMPMessageUtils.h"
#include "GMPSharedMemManager.h"
#include "GMPUtils.h"
#include "GMPVideoHost.h"
#include "GMPVideoDecoderProxy.h"
#include "VideoUtils.h"

namespace mozilla {
namespace gmp {

class GMPContentParent;

class GMPVideoDecoderParent final : public PGMPVideoDecoderParent
                                  , public GMPVideoDecoderProxy
                                  , public GMPSharedMemManager
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPVideoDecoderParent)

  explicit GMPVideoDecoderParent(GMPContentParent *aPlugin);

  GMPVideoHostImpl& Host();
  nsresult Shutdown();

  // GMPVideoDecoder
  void Close() override;
  nsresult InitDecode(const GMPVideoCodec& aCodecSettings,
                      const nsTArray<uint8_t>& aCodecSpecific,
                      GMPVideoDecoderCallbackProxy* aCallback,
                      int32_t aCoreCount) override;
  nsresult Decode(GMPUniquePtr<GMPVideoEncodedFrame> aInputFrame,
                  bool aMissingFrames,
                  const nsTArray<uint8_t>& aCodecSpecificInfo,
                  int64_t aRenderTimeMs = -1) override;
  nsresult Reset() override;
  nsresult Drain() override;
  uint32_t GetPluginId() const override { return mPluginId; }
  const nsCString& GetDisplayName() const override;

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
  ~GMPVideoDecoderParent();

  // PGMPVideoDecoderParent
  void ActorDestroy(ActorDestroyReason aWhy) override;
  bool RecvDecoded(const GMPVideoi420FrameData& aDecodedFrame) override;
  bool RecvReceivedDecodedReferenceFrame(const uint64_t& aPictureId) override;
  bool RecvReceivedDecodedFrame(const uint64_t& aPictureId) override;
  bool RecvInputDataExhausted() override;
  bool RecvDrainComplete() override;
  bool RecvResetComplete() override;
  bool RecvError(const GMPErr& aError) override;
  bool RecvShutdown() override;
  bool RecvParentShmemForPool(Shmem&& aEncodedBuffer) override;
  bool AnswerNeedShmem(const uint32_t& aFrameBufferSize,
                       Shmem* aMem) override;
  bool Recv__delete__() override;

  void UnblockResetAndDrain();
  void CancelResetCompleteTimeout();

  bool mIsOpen;
  bool mShuttingDown;
  bool mActorDestroyed;
  bool mIsAwaitingResetComplete;
  bool mIsAwaitingDrainComplete;
  RefPtr<GMPContentParent> mPlugin;
  GMPVideoDecoderCallbackProxy* mCallback;
  GMPVideoHostImpl mVideoHost;
  const uint32_t mPluginId;
  int32_t mFrameCount;
  RefPtr<SimpleTimer> mResetCompleteTimeout;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoDecoderParent_h_
