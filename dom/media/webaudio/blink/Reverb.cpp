/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Reverb.h"
#include "ReverbConvolverStage.h"

#include <math.h>
#include "ReverbConvolver.h"
#include "mozilla/FloatingPoint.h"

using namespace mozilla;

namespace WebCore {

// Empirical gain calibration tested across many impulse responses to ensure
// perceived volume is same as dry (unprocessed) signal
const float GainCalibration = 0.00125f;
const float GainCalibrationSampleRate = 44100;

// A minimum power value to when normalizing a silent (or very quiet) impulse
// response
const float MinPower = 0.000125f;

static float calculateNormalizationScale(const nsTArray<const float*>& response,
                                         size_t aLength, float sampleRate) {
  // Normalize by RMS power
  size_t numberOfChannels = response.Length();

  float power = 0;

  for (size_t i = 0; i < numberOfChannels; ++i) {
    float channelPower = AudioBufferSumOfSquares(response[i], aLength);
    power += channelPower;
  }

  power = sqrt(power / (numberOfChannels * aLength));

  // Protect against accidental overload
  if (!IsFinite(power) || IsNaN(power) || power < MinPower) power = MinPower;

  float scale = 1 / power;

  scale *= GainCalibration;  // calibrate to make perceived volume same as
                             // unprocessed

  // Scale depends on sample-rate.
  if (sampleRate) scale *= GainCalibrationSampleRate / sampleRate;

  // True-stereo compensation
  if (numberOfChannels == 4) scale *= 0.5f;

  return scale;
}

Reverb::Reverb(const AudioChunk& impulseResponse, size_t maxFFTSize,
               bool useBackgroundThreads, bool normalize, float sampleRate,
               bool* aAllocationFailure) {
  MOZ_ASSERT(aAllocationFailure);
  size_t impulseResponseBufferLength = impulseResponse.mDuration;
  float scale = impulseResponse.mVolume;

  CopyableAutoTArray<const float*, 4> irChannels(
      impulseResponse.ChannelData<float>());
  AutoTArray<float, 1024> tempBuf;

  if (normalize) {
    scale = calculateNormalizationScale(irChannels, impulseResponseBufferLength,
                                        sampleRate);
  }

  if (scale != 1.0f) {
    bool rv = tempBuf.SetLength(
        irChannels.Length() * impulseResponseBufferLength, mozilla::fallible);
    *aAllocationFailure = !rv;
    if (*aAllocationFailure) {
      return;
    }

    for (uint32_t i = 0; i < irChannels.Length(); ++i) {
      float* buf = &tempBuf[i * impulseResponseBufferLength];
      AudioBufferCopyWithScale(irChannels[i], scale, buf,
                               impulseResponseBufferLength);
      irChannels[i] = buf;
    }
  }

  *aAllocationFailure = !initialize(irChannels, impulseResponseBufferLength,
                                    maxFFTSize, useBackgroundThreads);
}

size_t Reverb::sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t amount = aMallocSizeOf(this);
  amount += m_convolvers.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (size_t i = 0; i < m_convolvers.Length(); i++) {
    if (m_convolvers[i]) {
      amount += m_convolvers[i]->sizeOfIncludingThis(aMallocSizeOf);
    }
  }

  amount += m_tempBuffer.SizeOfExcludingThis(aMallocSizeOf, false);
  return amount;
}

bool Reverb::initialize(const nsTArray<const float*>& impulseResponseBuffer,
                        size_t impulseResponseBufferLength, size_t maxFFTSize,
                        bool useBackgroundThreads) {
  m_impulseResponseLength = impulseResponseBufferLength;

  // The reverb can handle a mono impulse response and still do stereo
  // processing
  size_t numResponseChannels = impulseResponseBuffer.Length();
  MOZ_ASSERT(numResponseChannels > 0);
  // The number of convolvers required is at least the number of audio
  // channels.  Even if there is initially only one audio channel, another
  // may be added later, and so a second convolver is created now while the
  // impulse response is available.
  size_t numConvolvers = std::max<size_t>(numResponseChannels, 2);
  m_convolvers.SetCapacity(numConvolvers);

  int convolverRenderPhase = 0;
  for (size_t i = 0; i < numConvolvers; ++i) {
    size_t channelIndex = i < numResponseChannels ? i : 0;
    const float* channel = impulseResponseBuffer[channelIndex];
    size_t length = impulseResponseBufferLength;

    bool allocationFailure;
    UniquePtr<ReverbConvolver> convolver(
        new ReverbConvolver(channel, length, maxFFTSize, convolverRenderPhase,
                            useBackgroundThreads, &allocationFailure));
    if (allocationFailure) {
      return false;
    }
    m_convolvers.AppendElement(std::move(convolver));

    convolverRenderPhase += WEBAUDIO_BLOCK_SIZE;
  }

  // For "True" stereo processing we allocate a temporary buffer to avoid
  // repeatedly allocating it in the process() method. It can be bad to allocate
  // memory in a real-time thread.
  if (numResponseChannels == 4) {
    m_tempBuffer.AllocateChannels(2);
    WriteZeroesToAudioBlock(&m_tempBuffer, 0, WEBAUDIO_BLOCK_SIZE);
  }
  return true;
}

void Reverb::process(const AudioBlock* sourceBus, AudioBlock* destinationBus) {
  // Do a fairly comprehensive sanity check.
  // If these conditions are satisfied, all of the source and destination
  // pointers will be valid for the various matrixing cases.
  bool isSafeToProcess =
      sourceBus && destinationBus && sourceBus->ChannelCount() > 0 &&
      destinationBus->mChannelData.Length() > 0 &&
      WEBAUDIO_BLOCK_SIZE <= MaxFrameSize &&
      WEBAUDIO_BLOCK_SIZE <= size_t(sourceBus->GetDuration()) &&
      WEBAUDIO_BLOCK_SIZE <= size_t(destinationBus->GetDuration());

  MOZ_ASSERT(isSafeToProcess);
  if (!isSafeToProcess) return;

  // For now only handle mono or stereo output
  MOZ_ASSERT(destinationBus->ChannelCount() <= 2);

  float* destinationChannelL =
      static_cast<float*>(const_cast<void*>(destinationBus->mChannelData[0]));
  const float* sourceBusL =
      static_cast<const float*>(sourceBus->mChannelData[0]);

  // Handle input -> output matrixing...
  size_t numInputChannels = sourceBus->ChannelCount();
  size_t numOutputChannels = destinationBus->ChannelCount();
  size_t numReverbChannels = m_convolvers.Length();

  if (numInputChannels == 2 && numReverbChannels == 2 &&
      numOutputChannels == 2) {
    // 2 -> 2 -> 2
    const float* sourceBusR =
        static_cast<const float*>(sourceBus->mChannelData[1]);
    float* destinationChannelR =
        static_cast<float*>(const_cast<void*>(destinationBus->mChannelData[1]));
    m_convolvers[0]->process(sourceBusL, destinationChannelL);
    m_convolvers[1]->process(sourceBusR, destinationChannelR);
  } else if (numInputChannels == 1 && numOutputChannels == 2 &&
             numReverbChannels == 2) {
    // 1 -> 2 -> 2
    for (int i = 0; i < 2; ++i) {
      float* destinationChannel = static_cast<float*>(
          const_cast<void*>(destinationBus->mChannelData[i]));
      m_convolvers[i]->process(sourceBusL, destinationChannel);
    }
  } else if (numInputChannels == 1 && numOutputChannels == 1) {
    // 1 -> 1 -> 1 (Only one of the convolvers is used.)
    m_convolvers[0]->process(sourceBusL, destinationChannelL);
  } else if (numInputChannels == 2 && numReverbChannels == 4 &&
             numOutputChannels == 2) {
    // 2 -> 4 -> 2 ("True" stereo)
    const float* sourceBusR =
        static_cast<const float*>(sourceBus->mChannelData[1]);
    float* destinationChannelR =
        static_cast<float*>(const_cast<void*>(destinationBus->mChannelData[1]));

    float* tempChannelL =
        static_cast<float*>(const_cast<void*>(m_tempBuffer.mChannelData[0]));
    float* tempChannelR =
        static_cast<float*>(const_cast<void*>(m_tempBuffer.mChannelData[1]));

    // Process left virtual source
    m_convolvers[0]->process(sourceBusL, destinationChannelL);
    m_convolvers[1]->process(sourceBusL, destinationChannelR);

    // Process right virtual source
    m_convolvers[2]->process(sourceBusR, tempChannelL);
    m_convolvers[3]->process(sourceBusR, tempChannelR);

    AudioBufferAddWithScale(tempChannelL, 1.0f, destinationChannelL,
                            sourceBus->GetDuration());
    AudioBufferAddWithScale(tempChannelR, 1.0f, destinationChannelR,
                            sourceBus->GetDuration());
  } else if (numInputChannels == 1 && numReverbChannels == 4 &&
             numOutputChannels == 2) {
    // 1 -> 4 -> 2 (Processing mono with "True" stereo impulse response)
    // This is an inefficient use of a four-channel impulse response, but we
    // should handle the case.
    float* destinationChannelR =
        static_cast<float*>(const_cast<void*>(destinationBus->mChannelData[1]));

    float* tempChannelL =
        static_cast<float*>(const_cast<void*>(m_tempBuffer.mChannelData[0]));
    float* tempChannelR =
        static_cast<float*>(const_cast<void*>(m_tempBuffer.mChannelData[1]));

    // Process left virtual source
    m_convolvers[0]->process(sourceBusL, destinationChannelL);
    m_convolvers[1]->process(sourceBusL, destinationChannelR);

    // Process right virtual source
    m_convolvers[2]->process(sourceBusL, tempChannelL);
    m_convolvers[3]->process(sourceBusL, tempChannelR);

    AudioBufferAddWithScale(tempChannelL, 1.0f, destinationChannelL,
                            sourceBus->GetDuration());
    AudioBufferAddWithScale(tempChannelR, 1.0f, destinationChannelR,
                            sourceBus->GetDuration());
  } else {
    MOZ_ASSERT_UNREACHABLE("Unexpected Reverb configuration");
    destinationBus->SetNull(destinationBus->GetDuration());
  }
}

}  // namespace WebCore
