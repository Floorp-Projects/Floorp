/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncoderChild.h"
#include "GMPChild.h"
#include <stdio.h>
#include "mozilla/unused.h"
#include "GMPVideoEncodedFrameImpl.h"
#include "GMPVideoi420FrameImpl.h"

namespace mozilla {
namespace gmp {

GMPVideoEncoderChild::GMPVideoEncoderChild(GMPChild* aPlugin)
: mPlugin(aPlugin),
  mVideoEncoder(nullptr),
  mVideoHost(MOZ_THIS_IN_INITIALIZER_LIST())
{
  MOZ_ASSERT(mPlugin);
}

GMPVideoEncoderChild::~GMPVideoEncoderChild()
{
}

void
GMPVideoEncoderChild::Init(GMPVideoEncoder* aEncoder)
{
  MOZ_ASSERT(aEncoder, "Cannot initialize video encoder child without a video encoder!");
  mVideoEncoder = aEncoder;
}

GMPVideoHostImpl&
GMPVideoEncoderChild::Host()
{
  return mVideoHost;
}

void
GMPVideoEncoderChild::Encoded(GMPVideoEncodedFrame* aEncodedFrame,
                              const GMPCodecSpecificInfo& aCodecSpecificInfo)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  auto ef = static_cast<GMPVideoEncodedFrameImpl*>(aEncodedFrame);

  GMPVideoEncodedFrameData frameData;
  ef->RelinquishFrameData(frameData);

  SendEncoded(frameData, aCodecSpecificInfo);

  aEncodedFrame->Destroy();
}

bool
GMPVideoEncoderChild::MgrAllocShmem(size_t aSize,
                                    ipc::Shmem::SharedMemory::SharedMemoryType aType,
                                    ipc::Shmem* aMem)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  return AllocShmem(aSize, aType, aMem);
}

bool
GMPVideoEncoderChild::MgrDeallocShmem(Shmem& aMem)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  return DeallocShmem(aMem);
}

bool
GMPVideoEncoderChild::RecvInitEncode(const GMPVideoCodec& aCodecSettings,
                                     const int32_t& aNumberOfCores,
                                     const uint32_t& aMaxPayloadSize)
{
  if (!mVideoEncoder) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mVideoEncoder->InitEncode(aCodecSettings, this, aNumberOfCores, aMaxPayloadSize);

  return true;
}

bool
GMPVideoEncoderChild::RecvEncode(const GMPVideoi420FrameData& aInputFrame,
                                 const GMPCodecSpecificInfo& aCodecSpecificInfo,
                                 const InfallibleTArray<int>& aFrameTypes)
{
  if (!mVideoEncoder) {
    return false;
  }

  auto f = new GMPVideoi420FrameImpl(aInputFrame, &mVideoHost);

  std::vector<GMPVideoFrameType> frameTypes(aFrameTypes.Length());
  for (uint32_t i = 0; i < aFrameTypes.Length(); i++) {
    frameTypes[i] = static_cast<GMPVideoFrameType>(aFrameTypes[i]);
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mVideoEncoder->Encode(f, aCodecSpecificInfo, frameTypes);

  return true;
}

bool
GMPVideoEncoderChild::RecvSetChannelParameters(const uint32_t& aPacketLoss,
                                               const uint32_t& aRTT)
{
  if (!mVideoEncoder) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mVideoEncoder->SetChannelParameters(aPacketLoss, aRTT);

  return true;
}

bool
GMPVideoEncoderChild::RecvSetRates(const uint32_t& aNewBitRate,
                                   const uint32_t& aFrameRate)
{
  if (!mVideoEncoder) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mVideoEncoder->SetRates(aNewBitRate, aFrameRate);

  return true;
}

bool
GMPVideoEncoderChild::RecvSetPeriodicKeyFrames(const bool& aEnable)
{
  if (!mVideoEncoder) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mVideoEncoder->SetPeriodicKeyFrames(aEnable);

  return true;
}

bool
GMPVideoEncoderChild::RecvEncodingComplete()
{
  if (!mVideoEncoder) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mVideoEncoder->EncodingComplete();

  mVideoHost.DoneWithAPI();

  mPlugin = nullptr;

  unused << Send__delete__(this);

  return true;
}

} // namespace gmp
} // namespace mozilla
