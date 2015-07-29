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

// Empirical gain calibration tested across many impulse responses to ensure perceived volume is same as dry (unprocessed) signal
const float GainCalibration = -58;
const float GainCalibrationSampleRate = 44100;

// A minimum power value to when normalizing a silent (or very quiet) impulse response
const float MinPower = 0.000125f;

static float calculateNormalizationScale(ThreadSharedFloatArrayBufferList* response, size_t aLength, float sampleRate)
{
    // Normalize by RMS power
    size_t numberOfChannels = response->GetChannels();

    float power = 0;

    for (size_t i = 0; i < numberOfChannels; ++i) {
        float channelPower = AudioBufferSumOfSquares(static_cast<const float*>(response->GetData(i)), aLength);
        power += channelPower;
    }

    power = sqrt(power / (numberOfChannels * aLength));

    // Protect against accidental overload
    if (!IsFinite(power) || IsNaN(power) || power < MinPower)
        power = MinPower;

    float scale = 1 / power;

    scale *= powf(10, GainCalibration * 0.05f); // calibrate to make perceived volume same as unprocessed

    // Scale depends on sample-rate.
    if (sampleRate)
        scale *= GainCalibrationSampleRate / sampleRate;

    // True-stereo compensation
    if (response->GetChannels() == 4)
        scale *= 0.5f;

    return scale;
}

Reverb::Reverb(ThreadSharedFloatArrayBufferList* impulseResponse, size_t impulseResponseBufferLength, size_t renderSliceSize, size_t maxFFTSize, size_t numberOfChannels, bool useBackgroundThreads, bool normalize, float sampleRate)
{
    float scale = 1;

    nsAutoTArray<const float*,4> irChannels;
    for (size_t i = 0; i < impulseResponse->GetChannels(); ++i) {
        irChannels.AppendElement(impulseResponse->GetData(i));
    }
    nsAutoTArray<float,1024> tempBuf;

    if (normalize) {
        scale = calculateNormalizationScale(impulseResponse, impulseResponseBufferLength, sampleRate);

        if (scale) {
            tempBuf.SetLength(irChannels.Length()*impulseResponseBufferLength);
            for (uint32_t i = 0; i < irChannels.Length(); ++i) {
                float* buf = &tempBuf[i*impulseResponseBufferLength];
                AudioBufferCopyWithScale(irChannels[i], scale, buf,
                                         impulseResponseBufferLength);
                irChannels[i] = buf;
            }
        }
    }

    initialize(irChannels, impulseResponseBufferLength, renderSliceSize,
               maxFFTSize, numberOfChannels, useBackgroundThreads);
}

size_t Reverb::sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
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


void Reverb::initialize(const nsTArray<const float*>& impulseResponseBuffer,
                        size_t impulseResponseBufferLength, size_t renderSliceSize,
                        size_t maxFFTSize, size_t numberOfChannels, bool useBackgroundThreads)
{
    m_impulseResponseLength = impulseResponseBufferLength;

    // The reverb can handle a mono impulse response and still do stereo processing
    size_t numResponseChannels = impulseResponseBuffer.Length();
    m_convolvers.SetCapacity(numberOfChannels);

    int convolverRenderPhase = 0;
    for (size_t i = 0; i < numResponseChannels; ++i) {
        const float* channel = impulseResponseBuffer[i];
        size_t length = impulseResponseBufferLength;

        nsAutoPtr<ReverbConvolver> convolver(new ReverbConvolver(channel, length, renderSliceSize, maxFFTSize, convolverRenderPhase, useBackgroundThreads));
        m_convolvers.AppendElement(convolver.forget());

        convolverRenderPhase += renderSliceSize;
    }

    // For "True" stereo processing we allocate a temporary buffer to avoid repeatedly allocating it in the process() method.
    // It can be bad to allocate memory in a real-time thread.
    if (numResponseChannels == 4) {
        AllocateAudioBlock(2, &m_tempBuffer);
        WriteZeroesToAudioBlock(&m_tempBuffer, 0, WEBAUDIO_BLOCK_SIZE);
    }
}

void Reverb::process(const AudioChunk* sourceBus, AudioChunk* destinationBus, size_t framesToProcess)
{
    // Do a fairly comprehensive sanity check.
    // If these conditions are satisfied, all of the source and destination pointers will be valid for the various matrixing cases.
    bool isSafeToProcess = sourceBus && destinationBus && sourceBus->mChannelData.Length() > 0 && destinationBus->mChannelData.Length() > 0
        && framesToProcess <= MaxFrameSize && framesToProcess <= size_t(sourceBus->mDuration) && framesToProcess <= size_t(destinationBus->mDuration);

    MOZ_ASSERT(isSafeToProcess);
    if (!isSafeToProcess)
        return;

    // For now only handle mono or stereo output
    MOZ_ASSERT(destinationBus->mChannelData.Length() <= 2);

    float* destinationChannelL = static_cast<float*>(const_cast<void*>(destinationBus->mChannelData[0]));
    const float* sourceBusL = static_cast<const float*>(sourceBus->mChannelData[0]);

    // Handle input -> output matrixing...
    size_t numInputChannels = sourceBus->mChannelData.Length();
    size_t numOutputChannels = destinationBus->mChannelData.Length();
    size_t numReverbChannels = m_convolvers.Length();

    if (numInputChannels == 2 && numReverbChannels == 2 && numOutputChannels == 2) {
        // 2 -> 2 -> 2
        const float* sourceBusR = static_cast<const float*>(sourceBus->mChannelData[1]);
        float* destinationChannelR = static_cast<float*>(const_cast<void*>(destinationBus->mChannelData[1]));
        m_convolvers[0]->process(sourceBusL, sourceBus->mDuration, destinationChannelL, destinationBus->mDuration, framesToProcess);
        m_convolvers[1]->process(sourceBusR, sourceBus->mDuration, destinationChannelR, destinationBus->mDuration, framesToProcess);
    } else  if (numInputChannels == 1 && numOutputChannels == 2 && numReverbChannels == 2) {
        // 1 -> 2 -> 2
        for (int i = 0; i < 2; ++i) {
            float* destinationChannel = static_cast<float*>(const_cast<void*>(destinationBus->mChannelData[i]));
            m_convolvers[i]->process(sourceBusL, sourceBus->mDuration, destinationChannel, destinationBus->mDuration, framesToProcess);
        }
    } else if (numInputChannels == 1 && numReverbChannels == 1 && numOutputChannels == 2) {
        // 1 -> 1 -> 2
        m_convolvers[0]->process(sourceBusL, sourceBus->mDuration, destinationChannelL, destinationBus->mDuration, framesToProcess);

        // simply copy L -> R
        float* destinationChannelR = static_cast<float*>(const_cast<void*>(destinationBus->mChannelData[1]));
        bool isCopySafe = destinationChannelL && destinationChannelR && size_t(destinationBus->mDuration) >= framesToProcess;
        MOZ_ASSERT(isCopySafe);
        if (!isCopySafe)
            return;
        PodCopy(destinationChannelR, destinationChannelL, framesToProcess);
    } else if (numInputChannels == 1 && numReverbChannels == 1 && numOutputChannels == 1) {
        // 1 -> 1 -> 1
        m_convolvers[0]->process(sourceBusL, sourceBus->mDuration, destinationChannelL, destinationBus->mDuration, framesToProcess);
    } else if (numInputChannels == 2 && numReverbChannels == 4 && numOutputChannels == 2) {
        // 2 -> 4 -> 2 ("True" stereo)
        const float* sourceBusR = static_cast<const float*>(sourceBus->mChannelData[1]);
        float* destinationChannelR = static_cast<float*>(const_cast<void*>(destinationBus->mChannelData[1]));

        float* tempChannelL = static_cast<float*>(const_cast<void*>(m_tempBuffer.mChannelData[0]));
        float* tempChannelR = static_cast<float*>(const_cast<void*>(m_tempBuffer.mChannelData[1]));

        // Process left virtual source
        m_convolvers[0]->process(sourceBusL, sourceBus->mDuration, destinationChannelL, destinationBus->mDuration, framesToProcess);
        m_convolvers[1]->process(sourceBusL, sourceBus->mDuration, destinationChannelR, destinationBus->mDuration, framesToProcess);

        // Process right virtual source
        m_convolvers[2]->process(sourceBusR, sourceBus->mDuration, tempChannelL, m_tempBuffer.mDuration, framesToProcess);
        m_convolvers[3]->process(sourceBusR, sourceBus->mDuration, tempChannelR, m_tempBuffer.mDuration, framesToProcess);

        AudioBufferAddWithScale(tempChannelL, 1.0f, destinationChannelL, sourceBus->mDuration);
        AudioBufferAddWithScale(tempChannelR, 1.0f, destinationChannelR, sourceBus->mDuration);
    } else if (numInputChannels == 1 && numReverbChannels == 4 && numOutputChannels == 2) {
        // 1 -> 4 -> 2 (Processing mono with "True" stereo impulse response)
        // This is an inefficient use of a four-channel impulse response, but we should handle the case.
        float* destinationChannelR = static_cast<float*>(const_cast<void*>(destinationBus->mChannelData[1]));

        float* tempChannelL = static_cast<float*>(const_cast<void*>(m_tempBuffer.mChannelData[0]));
        float* tempChannelR = static_cast<float*>(const_cast<void*>(m_tempBuffer.mChannelData[1]));

        // Process left virtual source
        m_convolvers[0]->process(sourceBusL, sourceBus->mDuration, destinationChannelL, destinationBus->mDuration, framesToProcess);
        m_convolvers[1]->process(sourceBusL, sourceBus->mDuration, destinationChannelR, destinationBus->mDuration, framesToProcess);

        // Process right virtual source
        m_convolvers[2]->process(sourceBusL, sourceBus->mDuration, tempChannelL, m_tempBuffer.mDuration, framesToProcess);
        m_convolvers[3]->process(sourceBusL, sourceBus->mDuration, tempChannelR, m_tempBuffer.mDuration, framesToProcess);

        AudioBufferAddWithScale(tempChannelL, 1.0f, destinationChannelL, sourceBus->mDuration);
        AudioBufferAddWithScale(tempChannelR, 1.0f, destinationChannelR, sourceBus->mDuration);
    } else {
        // Handle gracefully any unexpected / unsupported matrixing
        // FIXME: add code for 5.1 support...
        destinationBus->SetNull(destinationBus->mDuration);
    }
}

void Reverb::reset()
{
    for (size_t i = 0; i < m_convolvers.Length(); ++i)
        m_convolvers[i]->reset();
}

size_t Reverb::latencyFrames() const
{
    return !m_convolvers.IsEmpty() ? m_convolvers[0]->latencyFrames() : 0;
}

} // namespace WebCore
