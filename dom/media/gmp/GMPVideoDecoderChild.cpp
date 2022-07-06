/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoderChild.h"
#include "GMPVideoi420FrameImpl.h"
#include "GMPContentChild.h"
#include <stdio.h>
#include "mozilla/Unused.h"
#include "GMPVideoEncodedFrameImpl.h"
#include "runnable_utils.h"

namespace mozilla::gmp {

GMPVideoDecoderChild::GMPVideoDecoderChild(GMPContentChild* aPlugin)
    : GMPSharedMemManager(aPlugin),
      mPlugin(aPlugin),
      mVideoDecoder(nullptr),
      mVideoHost(this),
      mNeedShmemIntrCount(0),
      mPendingDecodeComplete(false) {
  MOZ_ASSERT(mPlugin);
}

GMPVideoDecoderChild::~GMPVideoDecoderChild() {
  MOZ_ASSERT(!mNeedShmemIntrCount);
}

void GMPVideoDecoderChild::Init(GMPVideoDecoder* aDecoder) {
  MOZ_ASSERT(aDecoder,
             "Cannot initialize video decoder child without a video decoder!");
  mVideoDecoder = aDecoder;
}

GMPVideoHostImpl& GMPVideoDecoderChild::Host() { return mVideoHost; }

void GMPVideoDecoderChild::Decoded(GMPVideoi420Frame* aDecodedFrame) {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  if (!aDecodedFrame) {
    MOZ_CRASH("Not given a decoded frame!");
  }

  auto df = static_cast<GMPVideoi420FrameImpl*>(aDecodedFrame);

  GMPVideoi420FrameData frameData;
  df->InitFrameData(frameData);
  SendDecoded(frameData);

  aDecodedFrame->Destroy();
}

void GMPVideoDecoderChild::ReceivedDecodedReferenceFrame(
    const uint64_t aPictureId) {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendReceivedDecodedReferenceFrame(aPictureId);
}

void GMPVideoDecoderChild::ReceivedDecodedFrame(const uint64_t aPictureId) {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendReceivedDecodedFrame(aPictureId);
}

void GMPVideoDecoderChild::InputDataExhausted() {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendInputDataExhausted();
}

void GMPVideoDecoderChild::DrainComplete() {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendDrainComplete();
}

void GMPVideoDecoderChild::ResetComplete() {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendResetComplete();
}

void GMPVideoDecoderChild::Error(GMPErr aError) {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendError(aError);
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvInitDecode(
    const GMPVideoCodec& aCodecSettings, nsTArray<uint8_t>&& aCodecSpecific,
    const int32_t& aCoreCount) {
  if (!mVideoDecoder) {
    return IPC_FAIL(this, "!mVideoDecoder");
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoDecoder->InitDecode(aCodecSettings, aCodecSpecific.Elements(),
                            aCodecSpecific.Length(), this, aCoreCount);
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvDecode(
    const GMPVideoEncodedFrameData& aInputFrame, const bool& aMissingFrames,
    nsTArray<uint8_t>&& aCodecSpecificInfo, const int64_t& aRenderTimeMs) {
  if (!mVideoDecoder) {
    return IPC_FAIL(this, "!mVideoDecoder");
  }

  auto f = new GMPVideoEncodedFrameImpl(aInputFrame, &mVideoHost);

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoDecoder->Decode(f, aMissingFrames, aCodecSpecificInfo.Elements(),
                        aCodecSpecificInfo.Length(), aRenderTimeMs);

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvChildShmemForPool(
    Shmem&& aFrameBuffer) {
  if (aFrameBuffer.IsWritable()) {
    mVideoHost.SharedMemMgr()->MgrDeallocShmem(GMPSharedMem::kGMPFrameData,
                                               aFrameBuffer);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvReset() {
  if (!mVideoDecoder) {
    return IPC_FAIL(this, "!mVideoDecoder");
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoDecoder->Reset();

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvDrain() {
  if (!mVideoDecoder) {
    return IPC_FAIL(this, "!mVideoDecoder");
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoDecoder->Drain();

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvDecodingComplete() {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  if (mNeedShmemIntrCount) {
    // There's a GMP blocked in Alloc() waiting for the CallNeedShem() to
    // return a frame they can use. Don't call the GMP's DecodingComplete()
    // now and don't delete the GMPVideoDecoderChild, defer processing the
    // DecodingComplete() until once the Alloc() finishes.
    mPendingDecodeComplete = true;
    return IPC_OK();
  }
  if (mVideoDecoder) {
    // Ignore any return code. It is OK for this to fail without killing the
    // process.
    mVideoDecoder->DecodingComplete();
    mVideoDecoder = nullptr;
  }

  mVideoHost.DoneWithAPI();

  mPlugin = nullptr;

  Unused << Send__delete__(this);

  return IPC_OK();
}

bool GMPVideoDecoderChild::Alloc(size_t aSize, Shmem* aMem) {
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  bool rv;
#ifndef SHMEM_ALLOC_IN_CHILD
  ++mNeedShmemIntrCount;
  rv = SendNeedShmem(aSize, aMem);
  --mNeedShmemIntrCount;
  if (mPendingDecodeComplete && mNeedShmemIntrCount == 0) {
    mPendingDecodeComplete = false;
    mPlugin->GMPMessageLoop()->PostTask(
        NewRunnableMethod("gmp::GMPVideoDecoderChild::RecvDecodingComplete",
                          this, &GMPVideoDecoderChild::RecvDecodingComplete));
  }
#else
  rv = AllocShmem(aSize, aType, aMem);
#endif
  return rv;
}

void GMPVideoDecoderChild::Dealloc(Shmem&& aMem) {
#ifndef SHMEM_ALLOC_IN_CHILD
  SendParentShmemForPool(std::move(aMem));
#else
  DeallocShmem(aMem);
#endif
}

}  // namespace mozilla::gmp
