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

namespace mozilla {
namespace gmp {

class GMPParent;

class GMPVideoDecoderParent MOZ_FINAL : public GMPVideoDecoder
                                      , public PGMPVideoDecoderParent
                                      , public GMPSharedMemManager
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPVideoDecoderParent)

  GMPVideoDecoderParent(GMPParent *aPlugin);

  GMPVideoHostImpl& Host();

  // GMPSharedMemManager
  virtual bool MgrAllocShmem(size_t aSize,
                             ipc::Shmem::SharedMemory::SharedMemoryType aType,
                             ipc::Shmem* aMem) MOZ_OVERRIDE;
  virtual bool MgrDeallocShmem(Shmem& aMem) MOZ_OVERRIDE;

  // GMPVideoDecoder
  virtual GMPVideoErr InitDecode(const GMPVideoCodec& aCodecSettings,
                                 GMPDecoderCallback* aCallback,
                                 int32_t aCoreCount) MOZ_OVERRIDE;
  virtual GMPVideoErr Decode(GMPVideoEncodedFrame* aInputFrame,
                             bool aMissingFrames,
                             const GMPCodecSpecificInfo& aCodecSpecificInfo,
                             int64_t aRenderTimeMs = -1) MOZ_OVERRIDE;
  virtual GMPVideoErr Reset() MOZ_OVERRIDE;
  virtual GMPVideoErr Drain() MOZ_OVERRIDE;
  virtual void DecodingComplete() MOZ_OVERRIDE;

private:
  ~GMPVideoDecoderParent();

  // PGMPVideoDecoderParent
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual bool RecvDecoded(const GMPVideoi420FrameData& aDecodedFrame) MOZ_OVERRIDE;
  virtual bool RecvReceivedDecodedReferenceFrame(const uint64_t& aPictureId) MOZ_OVERRIDE;
  virtual bool RecvReceivedDecodedFrame(const uint64_t& aPictureId) MOZ_OVERRIDE;
  virtual bool RecvInputDataExhausted() MOZ_OVERRIDE;
  virtual bool Recv__delete__() MOZ_OVERRIDE;

  bool mCanSendMessages;
  GMPParent* mPlugin;
  GMPDecoderCallback* mCallback;
  GMPVideoHostImpl mVideoHost;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoDecoderParent_h_
