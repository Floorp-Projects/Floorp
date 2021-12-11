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

#include "HRTFDatabase.h"

#include "HRTFElevation.h"

namespace WebCore {

const int HRTFDatabase::MinElevation = -45;
const int HRTFDatabase::MaxElevation = 90;
const unsigned HRTFDatabase::RawElevationAngleSpacing = 15;
const unsigned HRTFDatabase::NumberOfRawElevations =
    10;  // -45 -> +90 (each 15 degrees)
const unsigned HRTFDatabase::InterpolationFactor = 1;
const unsigned HRTFDatabase::NumberOfTotalElevations =
    NumberOfRawElevations * InterpolationFactor;

nsReturnRef<HRTFDatabase> HRTFDatabase::create(float sampleRate) {
  return nsReturnRef<HRTFDatabase>(new HRTFDatabase(sampleRate));
}

HRTFDatabase::HRTFDatabase(float sampleRate) : m_sampleRate(sampleRate) {
  m_elevations.SetLength(NumberOfTotalElevations);

  unsigned elevationIndex = 0;
  for (int elevation = MinElevation; elevation <= MaxElevation;
       elevation += RawElevationAngleSpacing) {
    nsAutoRef<HRTFElevation> hrtfElevation(
        HRTFElevation::createBuiltin(elevation, sampleRate));
    MOZ_ASSERT(hrtfElevation.get());
    if (!hrtfElevation.get()) return;

    m_elevations[elevationIndex] = hrtfElevation.out();
    elevationIndex += InterpolationFactor;
  }

  // Now, go back and interpolate elevations.
  if (InterpolationFactor > 1) {
    for (unsigned i = 0; i < NumberOfTotalElevations;
         i += InterpolationFactor) {
      unsigned j = (i + InterpolationFactor);
      if (j >= NumberOfTotalElevations)
        j = i;  // for last elevation interpolate with itself

      // Create the interpolated convolution kernels and delays.
      for (unsigned jj = 1; jj < InterpolationFactor; ++jj) {
        float x =
            static_cast<float>(jj) / static_cast<float>(InterpolationFactor);
        m_elevations[i + jj] = HRTFElevation::createByInterpolatingSlices(
            m_elevations[i].get(), m_elevations[j].get(), x, sampleRate);
        MOZ_ASSERT(m_elevations[i + jj].get());
      }
    }
  }
}

size_t HRTFDatabase::sizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t amount = aMallocSizeOf(this);
  amount += m_elevations.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (size_t i = 0; i < m_elevations.Length(); i++) {
    amount += m_elevations[i]->sizeOfIncludingThis(aMallocSizeOf);
  }

  return amount;
}

void HRTFDatabase::getKernelsFromAzimuthElevation(
    double azimuthBlend, unsigned azimuthIndex, double elevationAngle,
    HRTFKernel*& kernelL, HRTFKernel*& kernelR, double& frameDelayL,
    double& frameDelayR) {
  unsigned elevationIndex = indexFromElevationAngle(elevationAngle);
  MOZ_ASSERT(elevationIndex < m_elevations.Length() &&
             m_elevations.Length() > 0);

  if (!m_elevations.Length()) {
    kernelL = 0;
    kernelR = 0;
    return;
  }

  if (elevationIndex > m_elevations.Length() - 1)
    elevationIndex = m_elevations.Length() - 1;

  HRTFElevation* hrtfElevation = m_elevations[elevationIndex].get();
  MOZ_ASSERT(hrtfElevation);
  if (!hrtfElevation) {
    kernelL = 0;
    kernelR = 0;
    return;
  }

  hrtfElevation->getKernelsFromAzimuth(azimuthBlend, azimuthIndex, kernelL,
                                       kernelR, frameDelayL, frameDelayR);
}

unsigned HRTFDatabase::indexFromElevationAngle(double elevationAngle) {
  // Clamp to allowed range.
  elevationAngle =
      mozilla::clamped(elevationAngle, static_cast<double>(MinElevation),
                       static_cast<double>(MaxElevation));

  unsigned elevationIndex =
      static_cast<int>(InterpolationFactor * (elevationAngle - MinElevation) /
                       RawElevationAngleSpacing);
  return elevationIndex;
}

}  // namespace WebCore
