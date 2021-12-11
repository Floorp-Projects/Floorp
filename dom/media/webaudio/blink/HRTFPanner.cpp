/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "HRTFPanner.h"
#include "HRTFDatabaseLoader.h"

#include "FFTConvolver.h"
#include "HRTFDatabase.h"
#include "AudioBlock.h"

using namespace mozilla;
using dom::ChannelInterpretation;

namespace WebCore {

// The value of 2 milliseconds is larger than the largest delay which exists in
// any HRTFKernel from the default HRTFDatabase (0.0136 seconds). We ASSERT the
// delay values used in process() with this value.
const float MaxDelayTimeSeconds = 0.002f;

const int UninitializedAzimuth = -1;

HRTFPanner::HRTFPanner(float sampleRate,
                       already_AddRefed<HRTFDatabaseLoader> databaseLoader)
    : m_databaseLoader(databaseLoader),
      m_sampleRate(sampleRate),
      m_crossfadeSelection(CrossfadeSelection1),
      m_azimuthIndex1(UninitializedAzimuth),
      m_azimuthIndex2(UninitializedAzimuth)
      // m_elevation1 and m_elevation2 are initialized in pan()
      ,
      m_crossfadeX(0),
      m_crossfadeIncr(0),
      m_convolverL1(HRTFElevation::fftSizeForSampleRate(sampleRate)),
      m_convolverR1(m_convolverL1.fftSize()),
      m_convolverL2(m_convolverL1.fftSize()),
      m_convolverR2(m_convolverL1.fftSize()),
      m_delayLine(MaxDelayTimeSeconds * sampleRate) {
  MOZ_ASSERT(m_databaseLoader);
  MOZ_COUNT_CTOR(HRTFPanner);
}

HRTFPanner::~HRTFPanner() { MOZ_COUNT_DTOR(HRTFPanner); }

size_t HRTFPanner::sizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t amount = aMallocSizeOf(this);

  // NB: m_databaseLoader can be shared, so it is not measured here
  amount += m_convolverL1.sizeOfExcludingThis(aMallocSizeOf);
  amount += m_convolverR1.sizeOfExcludingThis(aMallocSizeOf);
  amount += m_convolverL2.sizeOfExcludingThis(aMallocSizeOf);
  amount += m_convolverR2.sizeOfExcludingThis(aMallocSizeOf);
  amount += m_delayLine.SizeOfExcludingThis(aMallocSizeOf);

  return amount;
}

void HRTFPanner::reset() {
  m_azimuthIndex1 = UninitializedAzimuth;
  m_azimuthIndex2 = UninitializedAzimuth;
  // m_elevation1 and m_elevation2 are initialized in pan()
  m_crossfadeSelection = CrossfadeSelection1;
  m_crossfadeX = 0.0f;
  m_crossfadeIncr = 0.0f;
  m_convolverL1.reset();
  m_convolverR1.reset();
  m_convolverL2.reset();
  m_convolverR2.reset();
  m_delayLine.Reset();
}

int HRTFPanner::calculateDesiredAzimuthIndexAndBlend(double azimuth,
                                                     double& azimuthBlend) {
  // Convert the azimuth angle from the range -180 -> +180 into the range 0 ->
  // 360. The azimuth index may then be calculated from this positive value.
  if (azimuth < 0) azimuth += 360.0;

  int numberOfAzimuths = HRTFDatabase::numberOfAzimuths();
  const double angleBetweenAzimuths = 360.0 / numberOfAzimuths;

  // Calculate the azimuth index and the blend (0 -> 1) for interpolation.
  double desiredAzimuthIndexFloat = azimuth / angleBetweenAzimuths;
  int desiredAzimuthIndex = static_cast<int>(desiredAzimuthIndexFloat);
  azimuthBlend =
      desiredAzimuthIndexFloat - static_cast<double>(desiredAzimuthIndex);

  // We don't immediately start using this azimuth index, but instead approach
  // this index from the last index we rendered at. This minimizes the clicks
  // and graininess for moving sources which occur otherwise.
  desiredAzimuthIndex = std::max(0, desiredAzimuthIndex);
  desiredAzimuthIndex = std::min(numberOfAzimuths - 1, desiredAzimuthIndex);
  return desiredAzimuthIndex;
}

void HRTFPanner::pan(double desiredAzimuth, double elevation,
                     const AudioBlock* inputBus, AudioBlock* outputBus) {
#ifdef DEBUG
  unsigned numInputChannels = inputBus->IsNull() ? 0 : inputBus->ChannelCount();

  MOZ_ASSERT(numInputChannels <= 2);
  MOZ_ASSERT(inputBus->GetDuration() == WEBAUDIO_BLOCK_SIZE);
#endif

  bool isOutputGood = outputBus && outputBus->ChannelCount() == 2 &&
                      outputBus->GetDuration() == WEBAUDIO_BLOCK_SIZE;
  MOZ_ASSERT(isOutputGood);

  if (!isOutputGood) {
    if (outputBus) outputBus->SetNull(outputBus->GetDuration());
    return;
  }

  HRTFDatabase* database = m_databaseLoader->database();
  if (!database) {  // not yet loaded
    outputBus->SetNull(outputBus->GetDuration());
    return;
  }

  // IRCAM HRTF azimuths values from the loaded database is reversed from the
  // panner's notion of azimuth.
  double azimuth = -desiredAzimuth;

  bool isAzimuthGood = azimuth >= -180.0 && azimuth <= 180.0;
  MOZ_ASSERT(isAzimuthGood);
  if (!isAzimuthGood) {
    outputBus->SetNull(outputBus->GetDuration());
    return;
  }

  // Normally, we'll just be dealing with mono sources.
  // If we have a stereo input, implement stereo panning with left source
  // processed by left HRTF, and right source by right HRTF.

  // Get destination pointers.
  float* destinationL =
      static_cast<float*>(const_cast<void*>(outputBus->mChannelData[0]));
  float* destinationR =
      static_cast<float*>(const_cast<void*>(outputBus->mChannelData[1]));

  double azimuthBlend;
  int desiredAzimuthIndex =
      calculateDesiredAzimuthIndexAndBlend(azimuth, azimuthBlend);

  // Initially snap azimuth and elevation values to first values encountered.
  if (m_azimuthIndex1 == UninitializedAzimuth) {
    m_azimuthIndex1 = desiredAzimuthIndex;
    m_elevation1 = elevation;
  }
  if (m_azimuthIndex2 == UninitializedAzimuth) {
    m_azimuthIndex2 = desiredAzimuthIndex;
    m_elevation2 = elevation;
  }

  // Cross-fade / transition over a period of around 45 milliseconds.
  // This is an empirical value tuned to be a reasonable trade-off between
  // smoothness and speed.
  const double fadeFrames = sampleRate() <= 48000 ? 2048 : 4096;

  // Check for azimuth and elevation changes, initiating a cross-fade if needed.
  if (!m_crossfadeX && m_crossfadeSelection == CrossfadeSelection1) {
    if (desiredAzimuthIndex != m_azimuthIndex1 || elevation != m_elevation1) {
      // Cross-fade from 1 -> 2
      m_crossfadeIncr = 1 / fadeFrames;
      m_azimuthIndex2 = desiredAzimuthIndex;
      m_elevation2 = elevation;
    }
  }
  if (m_crossfadeX == 1 && m_crossfadeSelection == CrossfadeSelection2) {
    if (desiredAzimuthIndex != m_azimuthIndex2 || elevation != m_elevation2) {
      // Cross-fade from 2 -> 1
      m_crossfadeIncr = -1 / fadeFrames;
      m_azimuthIndex1 = desiredAzimuthIndex;
      m_elevation1 = elevation;
    }
  }

  // Get the HRTFKernels and interpolated delays.
  HRTFKernel* kernelL1;
  HRTFKernel* kernelR1;
  HRTFKernel* kernelL2;
  HRTFKernel* kernelR2;
  double frameDelayL1;
  double frameDelayR1;
  double frameDelayL2;
  double frameDelayR2;
  database->getKernelsFromAzimuthElevation(azimuthBlend, m_azimuthIndex1,
                                           m_elevation1, kernelL1, kernelR1,
                                           frameDelayL1, frameDelayR1);
  database->getKernelsFromAzimuthElevation(azimuthBlend, m_azimuthIndex2,
                                           m_elevation2, kernelL2, kernelR2,
                                           frameDelayL2, frameDelayR2);

  bool areKernelsGood = kernelL1 && kernelR1 && kernelL2 && kernelR2;
  MOZ_ASSERT(areKernelsGood);
  if (!areKernelsGood) {
    outputBus->SetNull(outputBus->GetDuration());
    return;
  }

  MOZ_ASSERT(frameDelayL1 / sampleRate() < MaxDelayTimeSeconds &&
             frameDelayR1 / sampleRate() < MaxDelayTimeSeconds);
  MOZ_ASSERT(frameDelayL2 / sampleRate() < MaxDelayTimeSeconds &&
             frameDelayR2 / sampleRate() < MaxDelayTimeSeconds);

  // Crossfade inter-aural delays based on transitions.
  float frameDelaysL[WEBAUDIO_BLOCK_SIZE];
  float frameDelaysR[WEBAUDIO_BLOCK_SIZE];
  {
    float x = m_crossfadeX;
    float incr = m_crossfadeIncr;
    for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      frameDelaysL[i] = (1 - x) * frameDelayL1 + x * frameDelayL2;
      frameDelaysR[i] = (1 - x) * frameDelayR1 + x * frameDelayR2;
      x += incr;
    }
  }

  // First run through delay lines for inter-aural time difference.
  m_delayLine.Write(*inputBus);
  // "Speakers" means a mono input is read into both outputs (with possibly
  // different delays).
  m_delayLine.ReadChannel(frameDelaysL, outputBus, 0,
                          ChannelInterpretation::Speakers);
  m_delayLine.ReadChannel(frameDelaysR, outputBus, 1,
                          ChannelInterpretation::Speakers);
  m_delayLine.NextBlock();

  bool needsCrossfading = m_crossfadeIncr;

  const float* convolutionDestinationL1;
  const float* convolutionDestinationR1;
  const float* convolutionDestinationL2;
  const float* convolutionDestinationR2;

  // Now do the convolutions.
  // Note that we avoid doing convolutions on both sets of convolvers if we're
  // not currently cross-fading.

  if (m_crossfadeSelection == CrossfadeSelection1 || needsCrossfading) {
    convolutionDestinationL1 =
        m_convolverL1.process(kernelL1->fftFrame(), destinationL);
    convolutionDestinationR1 =
        m_convolverR1.process(kernelR1->fftFrame(), destinationR);
  }

  if (m_crossfadeSelection == CrossfadeSelection2 || needsCrossfading) {
    convolutionDestinationL2 =
        m_convolverL2.process(kernelL2->fftFrame(), destinationL);
    convolutionDestinationR2 =
        m_convolverR2.process(kernelR2->fftFrame(), destinationR);
  }

  if (needsCrossfading) {
    // Apply linear cross-fade.
    float x = m_crossfadeX;
    float incr = m_crossfadeIncr;
    for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      destinationL[i] = (1 - x) * convolutionDestinationL1[i] +
                        x * convolutionDestinationL2[i];
      destinationR[i] = (1 - x) * convolutionDestinationR1[i] +
                        x * convolutionDestinationR2[i];
      x += incr;
    }
    // Update cross-fade value from local.
    m_crossfadeX = x;

    if (m_crossfadeIncr > 0 && fabs(m_crossfadeX - 1) < m_crossfadeIncr) {
      // We've fully made the crossfade transition from 1 -> 2.
      m_crossfadeSelection = CrossfadeSelection2;
      m_crossfadeX = 1;
      m_crossfadeIncr = 0;
    } else if (m_crossfadeIncr < 0 && fabs(m_crossfadeX) < -m_crossfadeIncr) {
      // We've fully made the crossfade transition from 2 -> 1.
      m_crossfadeSelection = CrossfadeSelection1;
      m_crossfadeX = 0;
      m_crossfadeIncr = 0;
    }
  } else {
    const float* sourceL;
    const float* sourceR;
    if (m_crossfadeSelection == CrossfadeSelection1) {
      sourceL = convolutionDestinationL1;
      sourceR = convolutionDestinationR1;
    } else {
      sourceL = convolutionDestinationL2;
      sourceR = convolutionDestinationR2;
    }
    PodCopy(destinationL, sourceL, WEBAUDIO_BLOCK_SIZE);
    PodCopy(destinationR, sourceR, WEBAUDIO_BLOCK_SIZE);
  }
}

int HRTFPanner::maxTailFrames() const {
  // Although the ideal tail time would be the length of the impulse
  // response, there is additional tail time from the approximations in the
  // implementation.  Because HRTFPanner is implemented with a DelayKernel
  // and a FFTConvolver, the tailTime of the HRTFPanner is the sum of the
  // tailTime of the DelayKernel and the tailTime of the FFTConvolver.  The
  // FFTs of the convolver are fftSize(), half of which is latency, but this
  // is aligned with blocks and so is reduced by the one block which is
  // processed immediately.
  return m_delayLine.MaxDelayTicks() + m_convolverL1.fftSize() / 2 +
         m_convolverL1.latencyFrames();
}

}  // namespace WebCore
