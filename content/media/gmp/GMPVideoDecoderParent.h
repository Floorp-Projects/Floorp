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
#include "GMPVideoHost.h"
#include "GMPVideoDecoderProxy.h"

namespace mozilla {
namespace gmp {

class GMPParent;

class GMPVideoDecoderParent MOZ_FINAL : public PGMPVideoDecoderParent
                                      , public GMPVideoDecoderProxy
                                      , public GMPSharedMemManager
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPVideoDecoderParent)

  explicit GMPVideoDecoderParent(GMPParent *aPlugin);

  GMPVideoHostImpl& Host();
  nsresult Shutdown();

  // GMPVideoDecoder
  virtual void Close() MOZ_OVERRIDE;
  virtual nsresult InitDecode(const GMPVideoCodec& aCodecSettings,
                              const nsTArray<uint8_t>& aCodecSpecific,
                              GMPVideoDecoderCallbackProxy* aCallback,
                              int32_t aCoreCount) MOZ_OVERRIDE;
  virtual nsresult Decode(UniquePtr<GMPVideoEncodedFrame> aInputFrame,
                          bool aMissingFrames,
                          const nsTArray<uint8_t>& aCodecSpecificInfo,
                          int64_t aRenderTimeMs = -1) MOZ_OVERRIDE;
  virtual nsresult Reset() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
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
  ~GMPVideoDecoderParent();

  // PGMPVideoDecoderParent
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual bool RecvDecoded(const GMPVideoi420FrameData& aDecodedFrame) MOZ_OVERRIDE;
  virtual bool RecvReceivedDecodedReferenceFrame(const uint64_t& aPictureId) MOZ_OVERRIDE;
  virtual bool RecvReceivedDecodedFrame(const uint64_t& aPictureId) MOZ_OVERRIDE;
  virtual bool RecvInputDataExhausted() MOZ_OVERRIDE;
  virtual bool RecvDrainComplete() MOZ_OVERRIDE;
  virtual bool RecvResetComplete() MOZ_OVERRIDE;
  virtual bool RecvError(const GMPErr& aError) MOZ_OVERRIDE;
  virtual bool RecvParentShmemForPool(Shmem& aEncodedBuffer) MOZ_OVERRIDE;
  virtual bool AnswerNeedShmem(const uint32_t& aFrameBufferSize,
                               Shmem* aMem) MOZ_OVERRIDE;
  virtual bool Recv__delete__() MOZ_OVERRIDE;

  bool mIsOpen;
  nsRefPtr<GMPParent> mPlugin;
  GMPVideoDecoderCallbackProxy* mCallback;
  GMPVideoHostImpl mVideoHost;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoDecoderParent_h_
