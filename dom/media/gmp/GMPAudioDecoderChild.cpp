/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPAudioDecoderChild.h"
#include "GMPContentChild.h"
#include "GMPAudioHost.h"
#include "mozilla/Unused.h"
#include <stdio.h>

namespace mozilla {
namespace gmp {

GMPAudioDecoderChild::GMPAudioDecoderChild(GMPContentChild* aPlugin)
  : mPlugin(aPlugin)
  , mAudioDecoder(nullptr)
{
  MOZ_ASSERT(mPlugin);
}

GMPAudioDecoderChild::~GMPAudioDecoderChild()
{
}

void
GMPAudioDecoderChild::Init(GMPAudioDecoder* aDecoder)
{
  MOZ_ASSERT(aDecoder, "Cannot initialize Audio decoder child without a Audio decoder!");
  mAudioDecoder = aDecoder;
}

GMPAudioHostImpl&
GMPAudioDecoderChild::Host()
{
  return mAudioHost;
}

void
GMPAudioDecoderChild::Decoded(GMPAudioSamples* aDecodedSamples)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  if (!aDecodedSamples) {
    MOZ_CRASH("Not given decoded audio samples!");
  }

  GMPAudioDecodedSampleData samples;
  samples.mData().AppendElements((int16_t*)aDecodedSamples->Buffer(),
                                 aDecodedSamples->Size() / sizeof(int16_t));
  samples.mTimeStamp() = aDecodedSamples->TimeStamp();
  samples.mChannelCount() = aDecodedSamples->Channels();
  samples.mSamplesPerSecond() = aDecodedSamples->Rate();

  Unused << SendDecoded(samples);

  aDecodedSamples->Destroy();
}

void
GMPAudioDecoderChild::InputDataExhausted()
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  Unused << SendInputDataExhausted();
}

void
GMPAudioDecoderChild::DrainComplete()
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  Unused << SendDrainComplete();
}

void
GMPAudioDecoderChild::ResetComplete()
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  Unused << SendResetComplete();
}

void
GMPAudioDecoderChild::Error(GMPErr aError)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  Unused << SendError(aError);
}

mozilla::ipc::IPCResult
GMPAudioDecoderChild::RecvInitDecode(const GMPAudioCodecData& a)
{
  MOZ_ASSERT(mAudioDecoder);
  if (!mAudioDecoder) {
    return IPC_FAIL_NO_REASON(this);
  }

  GMPAudioCodec codec;
  codec.mCodecType = a.mCodecType();
  codec.mChannelCount = a.mChannelCount();
  codec.mBitsPerChannel = a.mBitsPerChannel();
  codec.mSamplesPerSecond = a.mSamplesPerSecond();
  codec.mExtraData = a.mExtraData().Elements();
  codec.mExtraDataLen = a.mExtraData().Length();

  // Ignore any return code. It is OK for this to fail without killing the process.
  mAudioDecoder->InitDecode(codec, this);

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPAudioDecoderChild::RecvDecode(const GMPAudioEncodedSampleData& aEncodedSamples)
{
  if (!mAudioDecoder) {
    return IPC_FAIL_NO_REASON(this);
  }

  GMPAudioSamples* samples = new GMPAudioSamplesImpl(aEncodedSamples);

  // Ignore any return code. It is OK for this to fail without killing the process.
  mAudioDecoder->Decode(samples);

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPAudioDecoderChild::RecvReset()
{
  if (!mAudioDecoder) {
    return IPC_FAIL_NO_REASON(this);
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mAudioDecoder->Reset();

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPAudioDecoderChild::RecvDrain()
{
  if (!mAudioDecoder) {
    return IPC_FAIL_NO_REASON(this);
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mAudioDecoder->Drain();

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPAudioDecoderChild::RecvDecodingComplete()
{
  if (mAudioDecoder) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mAudioDecoder->DecodingComplete();
    mAudioDecoder = nullptr;
  }

  mPlugin = nullptr;

  Unused << Send__delete__(this);

  return IPC_OK();
}

} // namespace gmp
} // namespace mozilla
