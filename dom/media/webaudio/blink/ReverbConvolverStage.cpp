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

#include "ReverbConvolverStage.h"

#include "ReverbAccumulationBuffer.h"
#include "ReverbConvolver.h"
#include "ReverbInputBuffer.h"
#include "mozilla/PodOperations.h"

using namespace mozilla;

namespace WebCore {

ReverbConvolverStage::ReverbConvolverStage(const float* impulseResponse, size_t,
                                           size_t reverbTotalLatency,
                                           size_t stageOffset,
                                           size_t stageLength,
                                           size_t fftSize, size_t renderPhase,
                                           ReverbAccumulationBuffer* accumulationBuffer)
    : m_accumulationBuffer(accumulationBuffer)
    , m_accumulationReadIndex(0)
    , m_inputReadIndex(0)
{
    MOZ_ASSERT(impulseResponse);
    MOZ_ASSERT(accumulationBuffer);

    m_fftKernel = new FFTBlock(fftSize);
    m_fftKernel->PadAndMakeScaledDFT(impulseResponse + stageOffset, stageLength);
    m_fftConvolver = new FFTConvolver(fftSize, renderPhase);

    // The convolution stage at offset stageOffset needs to have a corresponding delay to cancel out the offset.
    size_t totalDelay = stageOffset + reverbTotalLatency;

    // But, the FFT convolution itself incurs latency, so subtract this out...
    size_t fftLatency = m_fftConvolver->latencyFrames();
    MOZ_ASSERT(totalDelay >= fftLatency);
    totalDelay -= fftLatency;

    m_postDelayLength = totalDelay;
}

size_t ReverbConvolverStage::sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
    size_t amount = aMallocSizeOf(this);

    if (m_fftKernel) {
        amount += m_fftKernel->SizeOfIncludingThis(aMallocSizeOf);
    }

    if (m_fftConvolver) {
        amount += m_fftConvolver->sizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
}

void ReverbConvolverStage::processInBackground(ReverbConvolver* convolver)
{
    ReverbInputBuffer* inputBuffer = convolver->inputBuffer();
    float* source = inputBuffer->directReadFrom(&m_inputReadIndex,
                                                WEBAUDIO_BLOCK_SIZE);
    process(source);
}

void ReverbConvolverStage::process(const float* source)
{
    MOZ_ASSERT(source);
    if (!source)
        return;

    // Now, run the convolution (into the delay buffer).
    // An expensive FFT will happen every fftSize / 2 frames.
    const float* output = m_fftConvolver->process(m_fftKernel, source);

    // Now accumulate into reverb's accumulation buffer.
    m_accumulationBuffer->accumulate(output, WEBAUDIO_BLOCK_SIZE,
                                     &m_accumulationReadIndex,
                                     m_postDelayLength);
}

} // namespace WebCore
