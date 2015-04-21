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
  virtual void Close() override;
  virtual nsresult InitDecode(const GMPVideoCodec& aCodecSettings,
                              const nsTArray<uint8_t>& aCodecSpecific,
                              GMPVideoDecoderCallbackProxy* aCallback,
                              int32_t aCoreCount) override;
  virtual nsresult Decode(GMPUniquePtr<GMPVideoEncodedFrame> aInputFrame,
                          bool aMissingFrames,
                          const nsTArray<uint8_t>& aCodecSpecificInfo,
                          int64_t aRenderTimeMs = -1) override;
  virtual nsresult Reset() override;
  virtual nsresult Drain() override;
  virtual const uint64_t ParentID() override { return reinterpret_cast<uint64_t>(mPlugin.get()); }
  virtual const nsCString& GetDisplayName() const override;

  // GMPSharedMemManager
  virtual bool Alloc(size_t aSize, Shmem::SharedMemory::SharedMemoryType aType, Shmem* aMem) override
  {
#ifdef GMP_SAFE_SHMEM
    return AllocShmem(aSize, aType, aMem);
#else
    return AllocUnsafeShmem(aSize, aType, aMem);
#endif
  }
  virtual void Dealloc(Shmem& aMem) override
  {
    DeallocShmem(aMem);
  }

private:
  ~GMPVideoDecoderParent();

  // PGMPVideoDecoderParent
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual bool RecvDecoded(const GMPVideoi420FrameData& aDecodedFrame) override;
  virtual bool RecvReceivedDecodedReferenceFrame(const uint64_t& aPictureId) override;
  virtual bool RecvReceivedDecodedFrame(const uint64_t& aPictureId) override;
  virtual bool RecvInputDataExhausted() override;
  virtual bool RecvDrainComplete() override;
  virtual bool RecvResetComplete() override;
  virtual bool RecvError(const GMPErr& aError) override;
  virtual bool RecvShutdown() override;
  virtual bool RecvParentShmemForPool(Shmem&& aEncodedBuffer) override;
  virtual bool AnswerNeedShmem(const uint32_t& aFrameBufferSize,
                               Shmem* aMem) override;
  virtual bool Recv__delete__() override;

  bool mIsOpen;
  bool mShuttingDown;
  bool mActorDestroyed;
  nsRefPtr<GMPContentParent> mPlugin;
  GMPVideoDecoderCallbackProxy* mCallback;
  GMPVideoHostImpl mVideoHost;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoDecoderParent_h_
