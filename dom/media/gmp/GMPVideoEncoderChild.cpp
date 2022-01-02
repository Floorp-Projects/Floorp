/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncoderChild.h"
#include "GMPContentChild.h"
#include <stdio.h>
#include "mozilla/Unused.h"
#include "GMPVideoEncodedFrameImpl.h"
#include "GMPVideoi420FrameImpl.h"
#include "runnable_utils.h"

namespace mozilla::gmp {

GMPVideoEncoderChild::GMPVideoEncoderChild(GMPContentChild* aPlugin)
    : GMPSharedMemManager(aPlugin),
      mPlugin(aPlugin),
      mVideoEncoder(nullptr),
      mVideoHost(this),
      mNeedShmemIntrCount(0),
      mPendingEncodeComplete(false) {
  MOZ_ASSERT(mPlugin);
}

GMPVideoEncoderChild::~GMPVideoEncoderChild() {
  MOZ_ASSERT(!mNeedShmemIntrCount);
}

void GMPVideoEncoderChild::Init(GMPVideoEncoder* aEncoder) {
  MOZ_ASSERT(aEncoder,
             "Cannot initialize video encoder child without a video encoder!");
  mVideoEncoder = aEncoder;
}

GMPVideoHostImpl& GMPVideoEncoderChild::Host() { return mVideoHost; }

void GMPVideoEncoderChild::Encoded(GMPVideoEncodedFrame* aEncodedFrame,
                                   const uint8_t* aCodecSpecificInfo,
                                   uint32_t aCodecSpecificInfoLength) {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  auto ef = static_cast<GMPVideoEncodedFrameImpl*>(aEncodedFrame);

  GMPVideoEncodedFrameData frameData;
  ef->RelinquishFrameData(frameData);

  nsTArray<uint8_t> codecSpecific;
  codecSpecific.AppendElements(aCodecSpecificInfo, aCodecSpecificInfoLength);
  SendEncoded(frameData, codecSpecific);

  aEncodedFrame->Destroy();
}

void GMPVideoEncoderChild::Error(GMPErr aError) {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendError(aError);
}

mozilla::ipc::IPCResult GMPVideoEncoderChild::RecvInitEncode(
    const GMPVideoCodec& aCodecSettings, nsTArray<uint8_t>&& aCodecSpecific,
    const int32_t& aNumberOfCores, const uint32_t& aMaxPayloadSize) {
  if (!mVideoEncoder) {
    return IPC_FAIL_NO_REASON(this);
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoEncoder->InitEncode(aCodecSettings, aCodecSpecific.Elements(),
                            aCodecSpecific.Length(), this, aNumberOfCores,
                            aMaxPayloadSize);

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderChild::RecvEncode(
    const GMPVideoi420FrameData& aInputFrame,
    nsTArray<uint8_t>&& aCodecSpecificInfo,
    nsTArray<GMPVideoFrameType>&& aFrameTypes) {
  if (!mVideoEncoder) {
    return IPC_FAIL_NO_REASON(this);
  }

  auto f = new GMPVideoi420FrameImpl(aInputFrame, &mVideoHost);

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoEncoder->Encode(f, aCodecSpecificInfo.Elements(),
                        aCodecSpecificInfo.Length(), aFrameTypes.Elements(),
                        aFrameTypes.Length());

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderChild::RecvChildShmemForPool(
    Shmem&& aEncodedBuffer) {
  if (aEncodedBuffer.IsWritable()) {
    mVideoHost.SharedMemMgr()->MgrDeallocShmem(GMPSharedMem::kGMPEncodedData,
                                               aEncodedBuffer);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderChild::RecvSetChannelParameters(
    const uint32_t& aPacketLoss, const uint32_t& aRTT) {
  if (!mVideoEncoder) {
    return IPC_FAIL_NO_REASON(this);
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoEncoder->SetChannelParameters(aPacketLoss, aRTT);

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderChild::RecvSetRates(
    const uint32_t& aNewBitRate, const uint32_t& aFrameRate) {
  if (!mVideoEncoder) {
    return IPC_FAIL_NO_REASON(this);
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoEncoder->SetRates(aNewBitRate, aFrameRate);

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderChild::RecvSetPeriodicKeyFrames(
    const bool& aEnable) {
  if (!mVideoEncoder) {
    return IPC_FAIL_NO_REASON(this);
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoEncoder->SetPeriodicKeyFrames(aEnable);

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderChild::RecvEncodingComplete() {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  if (mNeedShmemIntrCount) {
    // There's a GMP blocked in Alloc() waiting for the CallNeedShem() to
    // return a frame they can use. Don't call the GMP's EncodingComplete()
    // now and don't delete the GMPVideoEncoderChild, defer processing the
    // EncodingComplete() until once the Alloc() finishes.
    mPendingEncodeComplete = true;
    return IPC_OK();
  }

  if (!mVideoEncoder) {
    Unused << Send__delete__(this);
    return IPC_FAIL_NO_REASON(this);
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoEncoder->EncodingComplete();

  mVideoHost.DoneWithAPI();

  mPlugin = nullptr;

  Unused << Send__delete__(this);

  return IPC_OK();
}

bool GMPVideoEncoderChild::Alloc(size_t aSize,
                                 Shmem::SharedMemory::SharedMemoryType aType,
                                 Shmem* aMem) {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  bool rv;
#ifndef SHMEM_ALLOC_IN_CHILD
  ++mNeedShmemIntrCount;
  rv = CallNeedShmem(aSize, aMem);
  --mNeedShmemIntrCount;
  if (mPendingEncodeComplete && mNeedShmemIntrCount == 0) {
    mPendingEncodeComplete = false;
    mPlugin->GMPMessageLoop()->PostTask(
        NewRunnableMethod("gmp::GMPVideoEncoderChild::RecvEncodingComplete",
                          this, &GMPVideoEncoderChild::RecvEncodingComplete));
  }
#else
#  ifdef GMP_SAFE_SHMEM
  rv = AllocShmem(aSize, aType, aMem);
#  else
  rv = AllocUnsafeShmem(aSize, aType, aMem);
#  endif
#endif
  return rv;
}

void GMPVideoEncoderChild::Dealloc(Shmem&& aMem) {
#ifndef SHMEM_ALLOC_IN_CHILD
  SendParentShmemForPool(std::move(aMem));
#else
  DeallocShmem(aMem);
#endif
}

}  // namespace mozilla::gmp
