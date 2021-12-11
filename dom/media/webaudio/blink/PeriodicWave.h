/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef PeriodicWave_h
#define PeriodicWave_h

#include "mozilla/dom/OscillatorNodeBinding.h"
#include "mozilla/UniquePtr.h"
#include <nsTArray.h>
#include "AlignedTArray.h"
#include "mozilla/MemoryReporting.h"

namespace WebCore {

typedef AlignedTArray<float> AlignedAudioFloatArray;
typedef nsTArray<float> AudioFloatArray;

using mozilla::UniquePtr;

class PeriodicWave {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebCore::PeriodicWave);

  static already_AddRefed<PeriodicWave> createSine(float sampleRate);
  static already_AddRefed<PeriodicWave> createSquare(float sampleRate);
  static already_AddRefed<PeriodicWave> createSawtooth(float sampleRate);
  static already_AddRefed<PeriodicWave> createTriangle(float sampleRate);

  // Creates an arbitrary periodic wave given the frequency components
  // (Fourier coefficients).
  static already_AddRefed<PeriodicWave> create(float sampleRate,
                                               const float* real,
                                               const float* imag,
                                               size_t numberOfComponents,
                                               bool disableNormalization);

  // Returns pointers to the lower and higher wave data for the pitch range
  // containing the given fundamental frequency. These two tables are in
  // adjacent "pitch" ranges where the higher table will have the maximum
  // number of partials which won't alias when played back at this
  // fundamental frequency. The lower wave is the next range containing fewer
  // partials than the higher wave. Interpolation between these two tables
  // can be made according to tableInterpolationFactor. Where values
  // from 0 -> 1 interpolate between lower -> higher.
  void waveDataForFundamentalFrequency(float, float*& lowerWaveData,
                                       float*& higherWaveData,
                                       float& tableInterpolationFactor);

  // Returns the scalar multiplier to the oscillator frequency to calculate
  // wave buffer phase increment.
  float rateScale() const { return m_rateScale; }

  unsigned periodicWaveSize() const { return m_periodicWaveSize; }
  float sampleRate() const { return m_sampleRate; }

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  explicit PeriodicWave(float sampleRate, size_t numberOfComponents,
                        bool disableNormalization);
  ~PeriodicWave() = default;

  void generateBasicWaveform(mozilla::dom::OscillatorType);

  float m_sampleRate;
  unsigned m_periodicWaveSize;
  unsigned m_numberOfRanges;
  float m_centsPerRange;
  unsigned m_numberOfComponents;
  UniquePtr<AudioFloatArray> m_realComponents;
  UniquePtr<AudioFloatArray> m_imagComponents;

  // The lowest frequency (in Hertz) where playback will include all of the
  // partials.  Playing back lower than this frequency will gradually lose
  // more high-frequency information.
  // This frequency is quite low (~10Hz @ // 44.1KHz)
  float m_lowestFundamentalFrequency;

  float m_rateScale;

  unsigned numberOfRanges() const { return m_numberOfRanges; }

  // Maximum possible number of partials (before culling).
  unsigned maxNumberOfPartials() const;

  unsigned numberOfPartialsForRange(unsigned rangeIndex) const;

  // Creates table for specified index based on fundamental frequency.
  void createBandLimitedTables(float fundamentalFrequency, unsigned rangeIndex);
  unsigned m_maxPartialsInBandLimitedTable;
  float m_normalizationScale;
  bool m_disableNormalization;
  nsTArray<UniquePtr<AlignedAudioFloatArray> > m_bandLimitedTables;
};

}  // namespace WebCore

#endif  // PeriodicWave_h
