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

#include "PeriodicWave.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "mozilla/FFTBlock.h"

const unsigned MinPeriodicWaveSize = 4096; // This must be a power of two.
const unsigned MaxPeriodicWaveSize = 8192; // This must be a power of two.
const float CentsPerRange = 1200 / 3; // 1/3 Octave.

using namespace mozilla;
using mozilla::dom::OscillatorType;

namespace WebCore {

already_AddRefed<PeriodicWave>
PeriodicWave::create(float sampleRate,
                     const float* real,
                     const float* imag,
                     size_t numberOfComponents,
                     bool disableNormalization)
{
    bool isGood = real && imag && numberOfComponents > 0;
    MOZ_ASSERT(isGood);
    if (isGood) {
        RefPtr<PeriodicWave> periodicWave =
            new PeriodicWave(sampleRate, numberOfComponents,
                             disableNormalization);

        // Limit the number of components used to those for frequencies below the
        // Nyquist of the fixed length inverse FFT.
        size_t halfSize = periodicWave->m_periodicWaveSize / 2;
        numberOfComponents = std::min(numberOfComponents, halfSize);
        periodicWave->m_numberOfComponents = numberOfComponents;
        periodicWave->m_realComponents = new AudioFloatArray(numberOfComponents);
        periodicWave->m_imagComponents = new AudioFloatArray(numberOfComponents);
        memcpy(periodicWave->m_realComponents->Elements(), real,
               numberOfComponents * sizeof(float));
        memcpy(periodicWave->m_imagComponents->Elements(), imag,
               numberOfComponents * sizeof(float));

        return periodicWave.forget();
    }
    return nullptr;
}

already_AddRefed<PeriodicWave>
PeriodicWave::createSine(float sampleRate)
{
    RefPtr<PeriodicWave> periodicWave =
        new PeriodicWave(sampleRate, MinPeriodicWaveSize, false);
    periodicWave->generateBasicWaveform(OscillatorType::Sine);
    return periodicWave.forget();
}

already_AddRefed<PeriodicWave>
PeriodicWave::createSquare(float sampleRate)
{
    RefPtr<PeriodicWave> periodicWave =
        new PeriodicWave(sampleRate, MinPeriodicWaveSize, false);
    periodicWave->generateBasicWaveform(OscillatorType::Square);
    return periodicWave.forget();
}

already_AddRefed<PeriodicWave>
PeriodicWave::createSawtooth(float sampleRate)
{
    RefPtr<PeriodicWave> periodicWave =
        new PeriodicWave(sampleRate, MinPeriodicWaveSize, false);
    periodicWave->generateBasicWaveform(OscillatorType::Sawtooth);
    return periodicWave.forget();
}

already_AddRefed<PeriodicWave>
PeriodicWave::createTriangle(float sampleRate)
{
    RefPtr<PeriodicWave> periodicWave =
        new PeriodicWave(sampleRate, MinPeriodicWaveSize, false);
    periodicWave->generateBasicWaveform(OscillatorType::Triangle);
    return periodicWave.forget();
}

PeriodicWave::PeriodicWave(float sampleRate, size_t numberOfComponents, bool disableNormalization)
    : m_sampleRate(sampleRate)
    , m_centsPerRange(CentsPerRange)
    , m_maxPartialsInBandLimitedTable(0)
    , m_normalizationScale(1.0f)
    , m_disableNormalization(disableNormalization)
{
    float nyquist = 0.5 * m_sampleRate;

    if (numberOfComponents <= MinPeriodicWaveSize) {
        m_periodicWaveSize = MinPeriodicWaveSize;
    } else {
        unsigned npow2 = powf(2.0f, floorf(logf(numberOfComponents - 1.0)/logf(2.0f) + 1.0f));
        m_periodicWaveSize = std::min(MaxPeriodicWaveSize, npow2);
    }

    m_numberOfRanges = (unsigned)(3.0f*logf(m_periodicWaveSize)/logf(2.0f));
    m_bandLimitedTables.SetLength(m_numberOfRanges);
    m_lowestFundamentalFrequency = nyquist / maxNumberOfPartials();
    m_rateScale = m_periodicWaveSize / m_sampleRate;
}

size_t PeriodicWave::sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
    size_t amount = aMallocSizeOf(this);

    amount += m_bandLimitedTables.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < m_bandLimitedTables.Length(); i++) {
        if (m_bandLimitedTables[i]) {
            amount += m_bandLimitedTables[i]->ShallowSizeOfIncludingThis(aMallocSizeOf);
        }
    }

    return amount;
}

void PeriodicWave::waveDataForFundamentalFrequency(float fundamentalFrequency, float* &lowerWaveData, float* &higherWaveData, float& tableInterpolationFactor)
{

    // Negative frequencies are allowed, in which case we alias
    // to the positive frequency.
    fundamentalFrequency = fabsf(fundamentalFrequency);

    // We only need to rebuild to the tables if the new fundamental
    // frequency is low enough to allow for more partials below the
    // Nyquist frequency.
    unsigned numberOfPartials = numberOfPartialsForRange(0);
    float nyquist = 0.5 * m_sampleRate;
    if (fundamentalFrequency != 0.0) {
        numberOfPartials = std::min(numberOfPartials, (unsigned)(nyquist / fundamentalFrequency));
    }
    if (numberOfPartials > m_maxPartialsInBandLimitedTable) {
        for (unsigned rangeIndex = 0; rangeIndex < m_numberOfRanges; ++rangeIndex) {
            m_bandLimitedTables[rangeIndex] = 0;
        }

        // We need to create the first table to determine the normalization
        // constant.
        createBandLimitedTables(fundamentalFrequency, 0);
        m_maxPartialsInBandLimitedTable = numberOfPartials;
    }

    // Calculate the pitch range.
    float ratio = fundamentalFrequency > 0 ? fundamentalFrequency / m_lowestFundamentalFrequency : 0.5;
    float centsAboveLowestFrequency = logf(ratio)/logf(2.0f) * 1200;

    // Add one to round-up to the next range just in time to truncate
    // partials before aliasing occurs.
    float pitchRange = 1 + centsAboveLowestFrequency / m_centsPerRange;

    pitchRange = std::max(pitchRange, 0.0f);
    pitchRange = std::min(pitchRange, static_cast<float>(m_numberOfRanges - 1));

    // The words "lower" and "higher" refer to the table data having
    // the lower and higher numbers of partials. It's a little confusing
    // since the range index gets larger the more partials we cull out.
    // So the lower table data will have a larger range index.
    unsigned rangeIndex1 = static_cast<unsigned>(pitchRange);
    unsigned rangeIndex2 = rangeIndex1 < m_numberOfRanges - 1 ? rangeIndex1 + 1 : rangeIndex1;

    if (!m_bandLimitedTables[rangeIndex1].get())
        createBandLimitedTables(fundamentalFrequency, rangeIndex1);

    if (!m_bandLimitedTables[rangeIndex2].get())
        createBandLimitedTables(fundamentalFrequency, rangeIndex2);

    lowerWaveData = m_bandLimitedTables[rangeIndex2]->Elements();
    higherWaveData = m_bandLimitedTables[rangeIndex1]->Elements();

    // Ranges from 0 -> 1 to interpolate between lower -> higher.
    tableInterpolationFactor = rangeIndex2 - pitchRange;
}

unsigned PeriodicWave::maxNumberOfPartials() const
{
    return m_periodicWaveSize / 2;
}

unsigned PeriodicWave::numberOfPartialsForRange(unsigned rangeIndex) const
{
    // Number of cents below nyquist where we cull partials.
    float centsToCull = rangeIndex * m_centsPerRange;

    // A value from 0 -> 1 representing what fraction of the partials to keep.
    float cullingScale = pow(2, -centsToCull / 1200);

    // The very top range will have all the partials culled.
    unsigned numberOfPartials = cullingScale * maxNumberOfPartials();

    return numberOfPartials;
}

// Convert into time-domain wave buffers.
// One table is created for each range for non-aliasing playback
// at different playback rates. Thus, higher ranges have more
// high-frequency partials culled out.
void PeriodicWave::createBandLimitedTables(float fundamentalFrequency,
                                           unsigned rangeIndex)
{
    unsigned fftSize = m_periodicWaveSize;
    unsigned i;

    const float *realData = m_realComponents->Elements();
    const float *imagData = m_imagComponents->Elements();

    // This FFTBlock is used to cull partials (represented by frequency bins).
    FFTBlock frame(fftSize);

    // Find the starting bin where we should start culling the aliasing
    // partials for this pitch range.  We need to clear out the highest
    // frequencies to band-limit the waveform.
    unsigned numberOfPartials = numberOfPartialsForRange(rangeIndex);
    // Also limit to the number of components that are provided.
    numberOfPartials = std::min(numberOfPartials, m_numberOfComponents - 1);

    // Limit number of partials to those below Nyquist frequency
    float nyquist = 0.5 * m_sampleRate;
    if (fundamentalFrequency != 0.0) {
        numberOfPartials = std::min(numberOfPartials,
                                    (unsigned)(nyquist / fundamentalFrequency));
    }

    // Copy from loaded frequency data and generate complex conjugate
    // because of the way the inverse FFT is defined.
    // The coefficients of higher partials remain zero, as initialized in
    // the FFTBlock constructor.
    for (i = 0; i < numberOfPartials + 1; ++i) {
        frame.RealData(i) = realData[i];
        frame.ImagData(i) = -imagData[i];
    }

    // Clear any DC-offset.
    frame.RealData(0) = 0;
    // Clear value which has no effect.
    frame.ImagData(0) = 0;

    // Create the band-limited table.
    AlignedAudioFloatArray* table = new AlignedAudioFloatArray(m_periodicWaveSize);
    m_bandLimitedTables[rangeIndex] = table;

    // Apply an inverse FFT to generate the time-domain table data.
    float* data = m_bandLimitedTables[rangeIndex]->Elements();
    frame.GetInverseWithoutScaling(data);

    // For the first range (which has the highest power), calculate
    // its peak value then compute normalization scale.
    if (m_disableNormalization) {
      // See Bug 1424906, results need to be scaled by 0.5 even
      // when normalization is disabled.
      m_normalizationScale = 0.5;
    } else if (!rangeIndex) {
        float maxValue;
        maxValue = AudioBufferPeakValue(data, m_periodicWaveSize);

        if (maxValue)
            m_normalizationScale = 1.0f / maxValue;
    }

    // Apply normalization scale.
    AudioBufferInPlaceScale(data, m_normalizationScale, m_periodicWaveSize);
}

void PeriodicWave::generateBasicWaveform(OscillatorType shape)
{
    const float piFloat = float(M_PI);
    unsigned fftSize = periodicWaveSize();
    unsigned halfSize = fftSize / 2;

    m_numberOfComponents = halfSize;
    m_realComponents = new AudioFloatArray(halfSize);
    m_imagComponents = new AudioFloatArray(halfSize);
    float* realP = m_realComponents->Elements();
    float* imagP = m_imagComponents->Elements();

    // Clear DC and imag value which is ignored.
    realP[0] = 0;
    imagP[0] = 0;

    for (unsigned n = 1; n < halfSize; ++n) {
        float omega = 2 * piFloat * n;
        float invOmega = 1 / omega;

        // Fourier coefficients according to standard definition.
        float a; // Coefficient for cos().
        float b; // Coefficient for sin().

        // Calculate Fourier coefficients depending on the shape.
        // Note that the overall scaling (magnitude) of the waveforms
        // is normalized in createBandLimitedTables().
        switch (shape) {
        case OscillatorType::Sine:
            // Standard sine wave function.
            a = 0;
            b = (n == 1) ? 1 : 0;
            break;
        case OscillatorType::Square:
            // Square-shaped waveform with the first half its maximum value
            // and the second half its minimum value.
            a = 0;
            b = invOmega * ((n & 1) ? 2 : 0);
            break;
        case OscillatorType::Sawtooth:
            // Sawtooth-shaped waveform with the first half ramping from
            // zero to maximum and the second half from minimum to zero.
            a = 0;
            b = -invOmega * cos(0.5 * omega);
            break;
        case OscillatorType::Triangle:
            // Triangle-shaped waveform going from its maximum value to
            // its minimum value then back to the maximum value.
            a = 0;
            if (n & 1) {
              b = 2 * (2 / (n * piFloat) * 2 / (n * piFloat)) * ((((n - 1) >> 1) & 1) ? -1 : 1);
            } else {
              b = 0;
            }
            break;
        default:
            NS_NOTREACHED("invalid oscillator type");
            a = 0;
            b = 0;
            break;
        }

        realP[n] = a;
        imagP[n] = b;
    }
}

} // namespace WebCore
