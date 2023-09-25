/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "RLBoxSoundTouch.h"

using namespace rlbox;
using namespace mozilla;
using namespace soundtouch;

RLBoxSoundTouch::RLBoxSoundTouch() {
#ifdef MOZ_WASM_SANDBOXING_SOUNDTOUCH
  mSandbox.create_sandbox(false /* infallible */);
#else
  mSandbox.create_sandbox();
#endif
  mTimeStretcher = mSandbox.invoke_sandbox_function(createSoundTouchObj);

  // Allocate buffer in sandbox to receive samples.
  mSampleBuffer = mSandbox.malloc_in_sandbox<AudioDataValue>(mSampleBufferSize);
  MOZ_RELEASE_ASSERT(mSampleBuffer);
}

RLBoxSoundTouch::~RLBoxSoundTouch() {
  mSandbox.free_in_sandbox(mSampleBuffer);
  mSandbox.invoke_sandbox_function(destroySoundTouchObj, mTimeStretcher);
  mTimeStretcher = nullptr;
  mSandbox.destroy_sandbox();
}

void RLBoxSoundTouch::setSampleRate(uint aRate) {
  mSandbox.invoke_sandbox_function(SetSampleRate, mTimeStretcher, aRate);
}

void RLBoxSoundTouch::setChannels(uint aChannels) {
  mChannels = aChannels;
  mSandbox.invoke_sandbox_function(SetChannels, mTimeStretcher, aChannels);
}

void RLBoxSoundTouch::setPitch(double aPitch) {
  mSandbox.invoke_sandbox_function(SetPitch, mTimeStretcher, aPitch);
}

void RLBoxSoundTouch::setSetting(int aSettingId, int aValue) {
  mSandbox.invoke_sandbox_function(SetSetting, mTimeStretcher, aSettingId,
                                   aValue);
}

void RLBoxSoundTouch::setTempo(double aTempo) {
  mSandbox.invoke_sandbox_function(SetTempo, mTimeStretcher, aTempo);
}

void RLBoxSoundTouch::setRate(double aRate) {
  mSandbox.invoke_sandbox_function(SetRate, mTimeStretcher, aRate);
}

uint RLBoxSoundTouch::numChannels() {
  auto numChannels = mChannels;
  mSandbox.invoke_sandbox_function(NumChannels, mTimeStretcher)
      .copy_and_verify([numChannels](auto ch) {
        MOZ_RELEASE_ASSERT(ch == numChannels, "Number of channels changed");
      });
  return mChannels;
}

tainted_soundtouch<uint> RLBoxSoundTouch::numSamples() {
  return mSandbox.invoke_sandbox_function(NumSamples, mTimeStretcher);
}

tainted_soundtouch<uint> RLBoxSoundTouch::numUnprocessedSamples() {
  return mSandbox.invoke_sandbox_function(NumUnprocessedSamples,
                                          mTimeStretcher);
}

void RLBoxSoundTouch::putSamples(const AudioDataValue* aSamples,
                                 uint aNumSamples) {
  const CheckedInt<size_t> nrTotalSamples = numChannels() * aNumSamples;
  MOZ_RELEASE_ASSERT(nrTotalSamples.isValid(), "Input buffer size overflow");

  bool copied = false;
  auto t_aSamples = copy_memory_or_grant_access(
      mSandbox, aSamples, nrTotalSamples.value(), false, copied);

  mSandbox.invoke_sandbox_function(PutSamples, mTimeStretcher, t_aSamples,
                                   aNumSamples);

  if (copied) {
    mSandbox.free_in_sandbox(t_aSamples);
  }
}

uint RLBoxSoundTouch::receiveSamples(AudioDataValue* aOutput,
                                     uint aMaxSamples) {
  // Check number of channels.
  const CheckedInt<uint> channels = numChannels();
  // Sanity check max samples.
  const CheckedInt<uint> maxElements = channels * aMaxSamples;
  MOZ_RELEASE_ASSERT(maxElements.isValid(), "Max number of elements overflow");

  // Check mSampleBuffer size, and resize if required.
  if (mSampleBufferSize < maxElements.value()) {
    resizeSampleBuffer(maxElements.value());
  }

  auto numWrittenSamples =
      mSandbox
          .invoke_sandbox_function(ReceiveSamples, mTimeStretcher,
                                   mSampleBuffer, aMaxSamples)
          .copy_and_verify([aMaxSamples](uint written) {
            MOZ_RELEASE_ASSERT(written <= aMaxSamples,
                               "Number of samples exceeds max samples");
            return written;
          });

  // Copy received elements from sandbox.
  if (numWrittenSamples > 0) {
    const CheckedInt<uint> numCopyElements = channels * numWrittenSamples;

    MOZ_RELEASE_ASSERT(numCopyElements.isValid() &&
                           numCopyElements.value() <= maxElements.value(),
                       "Bad number of written elements");

    // Get pointer to mSampleBuffer. RLBox ensures that we have at least
    // numCopyElements elements.  We are just copying data values out. These
    // values are untrusted but should only be interpreted as sound samples --
    // so should, at worst, just sound weird.
    auto* mSampleBuffer_ptr = mSampleBuffer.unverified_safe_pointer_because(
        numCopyElements.value(),
        "Pointer to buffer is within sandbox and has at least numCopyElements "
        "elements.");
    PodCopy(aOutput, mSampleBuffer_ptr, numCopyElements.value());
  }

  return numWrittenSamples;
}

void RLBoxSoundTouch::flush() {
  return mSandbox.invoke_sandbox_function(Flush, mTimeStretcher);
}

void RLBoxSoundTouch::resizeSampleBuffer(uint aNewSize) {
  mSandbox.free_in_sandbox(mSampleBuffer);
  mSampleBufferSize = aNewSize;
  mSampleBuffer = mSandbox.malloc_in_sandbox<AudioDataValue>(aNewSize);
  MOZ_RELEASE_ASSERT(mSampleBuffer);
}
