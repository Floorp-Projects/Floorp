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
#include "GMPCrashHelperHolder.h"

namespace mozilla {
namespace gmp {

class GMPContentParent;

class GMPVideoDecoderParent final : public PGMPVideoDecoderParent,
                                    public GMPVideoDecoderProxy,
                                    public GMPSharedMemManager,
                                    public GMPCrashHelperHolder {
  friend class PGMPVideoDecoderParent;

 public:
  // Mark AddRef and Release as `final`, as they overload pure virtual
  // implementations in PGMPVideoDecoderParent.
  NS_INLINE_DECL_REFCOUNTING(GMPVideoDecoderParent, final)

  explicit GMPVideoDecoderParent(GMPContentParent* aPlugin);

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
  bool Alloc(size_t aSize, Shmem::SharedMemory::SharedMemoryType aType,
             Shmem* aMem) override {
#ifdef GMP_SAFE_SHMEM
    return AllocShmem(aSize, aType, aMem);
#else
    return AllocUnsafeShmem(aSize, aType, aMem);
#endif
  }
  void Dealloc(Shmem&& aMem) override { DeallocShmem(aMem); }

 private:
  ~GMPVideoDecoderParent();

  // PGMPVideoDecoderParent
  void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvDecoded(
      const GMPVideoi420FrameData& aDecodedFrame) override;
  mozilla::ipc::IPCResult RecvReceivedDecodedReferenceFrame(
      const uint64_t& aPictureId) override;
  mozilla::ipc::IPCResult RecvReceivedDecodedFrame(
      const uint64_t& aPictureId) override;
  mozilla::ipc::IPCResult RecvInputDataExhausted() override;
  mozilla::ipc::IPCResult RecvDrainComplete() override;
  mozilla::ipc::IPCResult RecvResetComplete() override;
  mozilla::ipc::IPCResult RecvError(const GMPErr& aError) override;
  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvParentShmemForPool(
      Shmem&& aEncodedBuffer) override;
  mozilla::ipc::IPCResult AnswerNeedShmem(const uint32_t& aFrameBufferSize,
                                          Shmem* aMem) override;
  mozilla::ipc::IPCResult Recv__delete__() override;

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

}  // namespace gmp
}  // namespace mozilla

#endif  // GMPVideoDecoderParent_h_
