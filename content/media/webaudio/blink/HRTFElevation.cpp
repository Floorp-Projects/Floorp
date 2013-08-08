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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "core/platform/audio/HRTFElevation.h"

#include "speex/speex_resampler.h"
#include "mozilla/PodOperations.h"
#include "AudioSampleFormat.h"
#include <math.h>
#include <algorithm>
#include "core/platform/PlatformMemoryInstrumentation.h"
#include "core/platform/audio/AudioBus.h"
#include "core/platform/audio/HRTFPanner.h"
#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/OwnPtr.h>

#include "IRC_Composite_C_R0195-incl.cpp"

using namespace std;
using namespace mozilla;
 
namespace WebCore {

const int elevationSpacing = irc_composite_c_r0195_elevation_interval;
const int firstElevation = irc_composite_c_r0195_first_elevation;
const int numberOfElevations = MOZ_ARRAY_LENGTH(irc_composite_c_r0195);

const unsigned HRTFElevation::NumberOfTotalAzimuths = 360 / 15 * 8;

const int rawSampleRate = irc_composite_c_r0195_sample_rate;

// Number of frames in an individual impulse response.
const size_t ResponseFrameSize = 256;

size_t HRTFElevation::fftSizeForSampleRate(float sampleRate)
{
    // The HRTF impulse responses (loaded as audio resources) are 512 sample-frames @44.1KHz.
    // Currently, we truncate the impulse responses to half this size, but an FFT-size of twice impulse response size is needed (for convolution).
    // So for sample rates around 44.1KHz an FFT size of 512 is good. We double the FFT-size only for sample rates at least double this.
    ASSERT(sampleRate >= 44100 && sampleRate <= 96000.0);
    return (sampleRate < 88200.0) ? 512 : 1024;
}

bool HRTFElevation::calculateKernelForAzimuthElevation(int azimuth, int elevation, SpeexResamplerState* resampler, float sampleRate,
                                                       RefPtr<HRTFKernel>& kernelL)
{
    int elevationIndex = (elevation - firstElevation) / elevationSpacing;
    MOZ_ASSERT(elevationIndex >= 0 && elevationIndex <= numberOfElevations);

    int numberOfAzimuths = irc_composite_c_r0195[elevationIndex].count;
    int azimuthSpacing = 360 / numberOfAzimuths;
    MOZ_ASSERT(numberOfAzimuths * azimuthSpacing == 360);

    int azimuthIndex = azimuth / azimuthSpacing;
    MOZ_ASSERT(azimuthIndex * azimuthSpacing == azimuth);

    const int16_t (&impulse_response_data)[ResponseFrameSize] =
        irc_composite_c_r0195[elevationIndex].azimuths[azimuthIndex];
    float floatResponse[ResponseFrameSize];
    ConvertAudioSamples(impulse_response_data, floatResponse,
                        ResponseFrameSize);

    // Note that depending on the fftSize returned by the panner, we may be truncating the impulse response.
    const size_t responseLength = fftSizeForSampleRate(sampleRate) / 2;

    float* response;
    nsAutoTArray<float, 2 * ResponseFrameSize> resampled;
    if (sampleRate == rawSampleRate) {
        response = floatResponse;
        MOZ_ASSERT(responseLength == ResponseFrameSize);
    } else {
        resampled.SetLength(responseLength);
        response = resampled.Elements();
        speex_resampler_skip_zeros(resampler);

        // Feed the input buffer into the resampler.
        spx_uint32_t in_len = ResponseFrameSize;
        spx_uint32_t out_len = resampled.Length();
        speex_resampler_process_float(resampler, 0, floatResponse, &in_len,
                                      response, &out_len);

        if (out_len < resampled.Length()) {
            // The input should have all been processed.
            MOZ_ASSERT(in_len == ResponseFrameSize);
            // Feed in zeros get the data remaining in the resampler.
            spx_uint32_t out_index = out_len;
            in_len = speex_resampler_get_input_latency(resampler);
            nsAutoTArray<float, 256> zeros;
            zeros.SetLength(in_len);
            PodZero(zeros.Elements(), in_len);
            out_len = resampled.Length() - out_index;
            speex_resampler_process_float(resampler, 0,
                                          zeros.Elements(), &in_len,
                                          response + out_index, &out_len);
            out_index += out_len;
            // There may be some uninitialized samples remaining for low
            // sample rates.
            PodZero(response + out_index, resampled.Length() - out_index);
        }

        speex_resampler_reset_mem(resampler);
    }

    kernelL = HRTFKernel::create(response, responseLength, sampleRate);
    
    return true;
}

// The range of elevations for the IRCAM impulse responses varies depending on azimuth, but the minimum elevation appears to always be -45.
//
// Here's how it goes:
static int maxElevations[] = {
        //  Azimuth
        //
    90, // 0  
    45, // 15 
    60, // 30 
    45, // 45 
    75, // 60 
    45, // 75 
    60, // 90 
    45, // 105 
    75, // 120 
    45, // 135 
    60, // 150 
    45, // 165 
    75, // 180 
    45, // 195 
    60, // 210 
    45, // 225 
    75, // 240 
    45, // 255 
    60, // 270 
    45, // 285 
    75, // 300 
    45, // 315 
    60, // 330 
    45 //  345 
};

PassOwnPtr<HRTFElevation> HRTFElevation::createForSubject(const String& subjectName, int elevation, float sampleRate)
{
    if (elevation < firstElevation ||
        elevation > firstElevation + numberOfElevations * elevationSpacing ||
        (elevation / elevationSpacing) * elevationSpacing != elevation)
        return nullptr;
        
    // Spacing, in degrees, between every azimuth loaded from resource.
    // Some elevations do not have data for all these intervals.
    // See maxElevations.
    static const unsigned AzimuthSpacing = 15;
    static const unsigned NumberOfRawAzimuths = 360 / AzimuthSpacing;
    static_assert(AzimuthSpacing * NumberOfRawAzimuths == 360,
                  "Not a multiple");
    static const unsigned InterpolationFactor =
        NumberOfTotalAzimuths / NumberOfRawAzimuths;
    static_assert(NumberOfTotalAzimuths ==
                  NumberOfRawAzimuths * InterpolationFactor, "Not a multiple");

    OwnPtr<HRTFKernelList> kernelListL = adoptPtr(new HRTFKernelList(NumberOfTotalAzimuths));

    SpeexResamplerState* resampler = sampleRate == rawSampleRate ? nullptr :
        speex_resampler_init(1, rawSampleRate, sampleRate,
                             SPEEX_RESAMPLER_QUALITY_DEFAULT, nullptr);

    // Load convolution kernels from HRTF files.
    int interpolatedIndex = 0;
    for (unsigned rawIndex = 0; rawIndex < NumberOfRawAzimuths; ++rawIndex) {
        // Don't let elevation exceed maximum for this azimuth.
        int maxElevation = maxElevations[rawIndex];
        int actualElevation = min(elevation, maxElevation);

        bool success = calculateKernelForAzimuthElevation(rawIndex * AzimuthSpacing, actualElevation, resampler, sampleRate, kernelListL->at(interpolatedIndex));
        if (!success)
            return nullptr;
            
        interpolatedIndex += InterpolationFactor;
    }

    if (resampler)
        speex_resampler_destroy(resampler);

    // Now go back and interpolate intermediate azimuth values.
    for (unsigned i = 0; i < NumberOfTotalAzimuths; i += InterpolationFactor) {
        int j = (i + InterpolationFactor) % NumberOfTotalAzimuths;

        // Create the interpolated convolution kernels and delays.
        for (unsigned jj = 1; jj < InterpolationFactor; ++jj) {
            float x = float(jj) / float(InterpolationFactor); // interpolate from 0 -> 1

            (*kernelListL)[i + jj] = HRTFKernel::createInterpolatedKernel(kernelListL->at(i).get(), kernelListL->at(j).get(), x);
        }
    }
    
    OwnPtr<HRTFElevation> hrtfElevation = adoptPtr(new HRTFElevation(kernelListL.release(), elevation, sampleRate));
    return hrtfElevation.release();
}

PassOwnPtr<HRTFElevation> HRTFElevation::createByInterpolatingSlices(HRTFElevation* hrtfElevation1, HRTFElevation* hrtfElevation2, float x, float sampleRate)
{
    ASSERT(hrtfElevation1 && hrtfElevation2);
    if (!hrtfElevation1 || !hrtfElevation2)
        return nullptr;
        
    ASSERT(x >= 0.0 && x < 1.0);
    
    OwnPtr<HRTFKernelList> kernelListL = adoptPtr(new HRTFKernelList(NumberOfTotalAzimuths));

    HRTFKernelList* kernelListL1 = hrtfElevation1->kernelListL();
    HRTFKernelList* kernelListL2 = hrtfElevation2->kernelListL();
    
    // Interpolate kernels of corresponding azimuths of the two elevations.
    for (unsigned i = 0; i < NumberOfTotalAzimuths; ++i) {
        (*kernelListL)[i] = HRTFKernel::createInterpolatedKernel(kernelListL1->at(i).get(), kernelListL2->at(i).get(), x);
    }

    // Interpolate elevation angle.
    double angle = (1.0 - x) * hrtfElevation1->elevationAngle() + x * hrtfElevation2->elevationAngle();
    
    OwnPtr<HRTFElevation> hrtfElevation = adoptPtr(new HRTFElevation(kernelListL.release(), static_cast<int>(angle), sampleRate));
    return hrtfElevation.release();  
}

void HRTFElevation::getKernelsFromAzimuth(double azimuthBlend, unsigned azimuthIndex, HRTFKernel* &kernelL, HRTFKernel* &kernelR, double& frameDelayL, double& frameDelayR)
{
    bool checkAzimuthBlend = azimuthBlend >= 0.0 && azimuthBlend < 1.0;
    ASSERT(checkAzimuthBlend);
    if (!checkAzimuthBlend)
        azimuthBlend = 0.0;
    
    unsigned numKernels = m_kernelListL->size();

    bool isIndexGood = azimuthIndex < numKernels;
    ASSERT(isIndexGood);
    if (!isIndexGood) {
        kernelL = 0;
        kernelR = 0;
        return;
    }
    
    // Return the left and right kernels,
    // using symmetry to produce the right kernel.
    kernelL = m_kernelListL->at(azimuthIndex).get();
    int azimuthIndexR = (numKernels - azimuthIndex) % numKernels;
    kernelR = m_kernelListL->at(azimuthIndexR).get();

    frameDelayL = kernelL->frameDelay();
    frameDelayR = kernelR->frameDelay();

    int azimuthIndex2L = (azimuthIndex + 1) % numKernels;
    double frameDelay2L = m_kernelListL->at(azimuthIndex2L)->frameDelay();
    int azimuthIndex2R = (numKernels - azimuthIndex2L) % numKernels;
    double frameDelay2R = m_kernelListL->at(azimuthIndex2R)->frameDelay();

    // Linearly interpolate delays.
    frameDelayL = (1.0 - azimuthBlend) * frameDelayL + azimuthBlend * frameDelay2L;
    frameDelayR = (1.0 - azimuthBlend) * frameDelayR + azimuthBlend * frameDelay2R;
}

void HRTFElevation::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, PlatformMemoryTypes::AudioSharedData);
    info.addMember(m_kernelListL, "kernelListL");
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
