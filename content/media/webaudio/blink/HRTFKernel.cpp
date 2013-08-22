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

#include "HRTFKernel.h"
namespace WebCore {

// Takes the input audio channel |impulseP| as an input impulse response and calculates the average group delay.
// This represents the initial delay before the most energetic part of the impulse response.
// The sample-frame delay is removed from the |impulseP| impulse response, and this value  is returned.
// The |length| of the passed in |impulseP| must be must be a power of 2.
static float extractAverageGroupDelay(float* impulseP, size_t length)
{
    // Check for power-of-2.
    MOZ_ASSERT(length && (length & (length - 1)) == 0);

    FFTBlock estimationFrame(length);
    estimationFrame.PerformFFT(impulseP);

    float frameDelay = static_cast<float>(estimationFrame.ExtractAverageGroupDelay());
    estimationFrame.PerformInverseFFT(impulseP);

    return frameDelay;
}

HRTFKernel::HRTFKernel(float* impulseResponse, size_t length, float sampleRate)
    : m_frameDelay(0)
    , m_sampleRate(sampleRate)
{
    // Determine the leading delay (average group delay) for the response.
    m_frameDelay = extractAverageGroupDelay(impulseResponse, length);

    // The FFT size (with zero padding) needs to be twice the response length
    // in order to do proper convolution.
    unsigned fftSize = 2 * length;

    // Quick fade-out (apply window) at truncation point
    // because the impulse response has been truncated.
    unsigned numberOfFadeOutFrames = static_cast<unsigned>(sampleRate / 4410); // 10 sample-frames @44.1KHz sample-rate
    MOZ_ASSERT(numberOfFadeOutFrames < length);
    if (numberOfFadeOutFrames < length) {
        for (unsigned i = length - numberOfFadeOutFrames; i < length; ++i) {
            float x = 1.0f - static_cast<float>(i - (length - numberOfFadeOutFrames)) / numberOfFadeOutFrames;
            impulseResponse[i] *= x;
        }
    }

    m_fftFrame = new FFTBlock(fftSize);
    m_fftFrame->PerformPaddedFFT(impulseResponse, length);
}

// Interpolates two kernels with x: 0 -> 1 and returns the result.
nsReturnRef<HRTFKernel> HRTFKernel::createInterpolatedKernel(HRTFKernel* kernel1, HRTFKernel* kernel2, float x)
{
    MOZ_ASSERT(kernel1 && kernel2);
    if (!kernel1 || !kernel2)
        return nsReturnRef<HRTFKernel>();
 
    MOZ_ASSERT(x >= 0.0 && x < 1.0);
    x = mozilla::clamped(x, 0.0f, 1.0f);
    
    float sampleRate1 = kernel1->sampleRate();
    float sampleRate2 = kernel2->sampleRate();
    MOZ_ASSERT(sampleRate1 == sampleRate2);
    if (sampleRate1 != sampleRate2)
        return nsReturnRef<HRTFKernel>();
    
    float frameDelay = (1 - x) * kernel1->frameDelay() + x * kernel2->frameDelay();
    
    nsAutoPtr<FFTBlock> interpolatedFrame(
        FFTBlock::CreateInterpolatedBlock(*kernel1->fftFrame(), *kernel2->fftFrame(), x));
    return HRTFKernel::create(interpolatedFrame, frameDelay, sampleRate1);
}

} // namespace WebCore
