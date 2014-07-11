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

namespace mozilla {
namespace gmp {

class GMPChild;

class GMPVideoDecoderChild : public PGMPVideoDecoderChild,
                             public GMPDecoderCallback,
                             public GMPSharedMemManager
{
public:
  GMPVideoDecoderChild(GMPChild* aPlugin);
  virtual ~GMPVideoDecoderChild();

  void Init(GMPVideoDecoder* aDecoder);
  GMPVideoHostImpl& Host();

  // GMPDecoderCallback
  virtual void Decoded(GMPVideoi420Frame* decodedFrame) MOZ_OVERRIDE;
  virtual void ReceivedDecodedReferenceFrame(const uint64_t pictureId) MOZ_OVERRIDE;
  virtual void ReceivedDecodedFrame(const uint64_t pictureId) MOZ_OVERRIDE;
  virtual void InputDataExhausted() MOZ_OVERRIDE;

  // GMPSharedMemManager
  virtual bool MgrAllocShmem(size_t aSize,
                             ipc::Shmem::SharedMemory::SharedMemoryType aType,
                             ipc::Shmem* aMem) MOZ_OVERRIDE;
  virtual bool MgrDeallocShmem(Shmem& aMem) MOZ_OVERRIDE;

private:
  // PGMPVideoDecoderChild
  virtual bool RecvInitDecode(const GMPVideoCodec& codecSettings,
                              const int32_t& coreCount) MOZ_OVERRIDE;
  virtual bool RecvDecode(const GMPVideoEncodedFrameData& inputFrame,
                          const bool& missingFrames,
                          const GMPCodecSpecificInfo& codecSpecificInfo,
                          const int64_t& renderTimeMs) MOZ_OVERRIDE;
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
