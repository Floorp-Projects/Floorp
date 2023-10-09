/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDriftCorrection.h"

#include "AudioResampler.h"
#include "DriftController.h"

namespace mozilla {

static constexpr uint32_t kMinBufferMs = 5;

AudioDriftCorrection::AudioDriftCorrection(
    uint32_t aSourceRate, uint32_t aTargetRate, uint32_t aBufferMs,
    const PrincipalHandle& aPrincipalHandle)
    : mDesiredBuffering(std::max(kMinBufferMs, aBufferMs) * aSourceRate / 1000),
      mTargetRate(aTargetRate),
      mDriftController(MakeUnique<DriftController>(aSourceRate, aTargetRate,
                                                   mDesiredBuffering)),
      mResampler(MakeUnique<AudioResampler>(
          aSourceRate, aTargetRate, mDesiredBuffering, aPrincipalHandle)) {}

AudioDriftCorrection::~AudioDriftCorrection() = default;

AudioSegment AudioDriftCorrection::RequestFrames(const AudioSegment& aInput,
                                                 uint32_t aOutputFrames) {
  uint32_t inputFrames = aInput.GetDuration();
  // Very important to go first since the Dynamic will get the sample format
  // from the chunk.
  if (aInput.GetDuration()) {
    // Always go through the resampler because the clock might shift later.
    mResampler->AppendInput(aInput);
  }
  bool hasUnderrun = false;
  AudioSegment output = mResampler->Resample(aOutputFrames, &hasUnderrun);
  mDriftController->UpdateClock(inputFrames, aOutputFrames,
                                mResampler->InputReadableFrames(),
                                mResampler->InputCapacityFrames());
  // Update resampler's rate if there is a new correction.
  mResampler->UpdateOutRate(mDriftController->GetCorrectedTargetRate());
  if (hasUnderrun) {
    NS_WARNING("Drift-correction: Underrun");
  }
  return output;
}

uint32_t AudioDriftCorrection::CurrentBuffering() const {
  return mResampler->InputReadableFrames();
}

uint32_t AudioDriftCorrection::BufferSize() const {
  return mResampler->InputCapacityFrames();
}

uint32_t AudioDriftCorrection::NumCorrectionChanges() const {
  return mDriftController->NumCorrectionChanges();
}
}  // namespace mozilla
