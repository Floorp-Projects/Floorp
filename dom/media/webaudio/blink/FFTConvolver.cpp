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

#include "FFTConvolver.h"
#include "mozilla/PodOperations.h"

using namespace mozilla;

namespace WebCore {

FFTConvolver::FFTConvolver(size_t fftSize, size_t renderPhase)
    : m_frame(fftSize)
    , m_readWriteIndex(renderPhase % (fftSize / 2))
{
    MOZ_ASSERT(fftSize >= 2 * WEBAUDIO_BLOCK_SIZE);
  m_inputBuffer.SetLength(fftSize);
  PodZero(m_inputBuffer.Elements(), fftSize);
  m_outputBuffer.SetLength(fftSize);
  PodZero(m_outputBuffer.Elements(), fftSize);
  m_lastOverlapBuffer.SetLength(fftSize / 2);
  PodZero(m_lastOverlapBuffer.Elements(), fftSize / 2);
}

size_t FFTConvolver::sizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
    size_t amount = 0;
    amount += m_frame.SizeOfExcludingThis(aMallocSizeOf);
    amount += m_inputBuffer.ShallowSizeOfExcludingThis(aMallocSizeOf);
    amount += m_outputBuffer.ShallowSizeOfExcludingThis(aMallocSizeOf);
    amount += m_lastOverlapBuffer.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
}

size_t FFTConvolver::sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + sizeOfExcludingThis(aMallocSizeOf);
}

const float* FFTConvolver::process(FFTBlock* fftKernel, const float* sourceP)
{
    size_t halfSize = fftSize() / 2;

    // WEBAUDIO_BLOCK_SIZE must be an exact multiple of halfSize,
    // halfSize must be a multiple of WEBAUDIO_BLOCK_SIZE
    // and > WEBAUDIO_BLOCK_SIZE.
    MOZ_ASSERT(halfSize % WEBAUDIO_BLOCK_SIZE == 0 &&
               WEBAUDIO_BLOCK_SIZE <= halfSize);

    // Copy samples to input buffer (note contraint above!)
    float* inputP = m_inputBuffer.Elements();

    MOZ_ASSERT(sourceP && inputP && m_readWriteIndex + WEBAUDIO_BLOCK_SIZE <= m_inputBuffer.Length());

    memcpy(inputP + m_readWriteIndex, sourceP, sizeof(float) * WEBAUDIO_BLOCK_SIZE);

    float* outputP = m_outputBuffer.Elements();
    m_readWriteIndex += WEBAUDIO_BLOCK_SIZE;

    // Check if it's time to perform the next FFT
    if (m_readWriteIndex == halfSize) {
        // The input buffer is now filled (get frequency-domain version)
        m_frame.PerformFFT(m_inputBuffer.Elements());
        m_frame.Multiply(*fftKernel);
        m_frame.GetInverseWithoutScaling(m_outputBuffer.Elements());

        // Overlap-add 1st half from previous time
        AudioBufferAddWithScale(m_lastOverlapBuffer.Elements(), 1.0f,
                                m_outputBuffer.Elements(), halfSize);

        // Finally, save 2nd half of result
        MOZ_ASSERT(m_outputBuffer.Length() == 2 * halfSize && m_lastOverlapBuffer.Length() == halfSize);

        memcpy(m_lastOverlapBuffer.Elements(), m_outputBuffer.Elements() + halfSize, sizeof(float) * halfSize);

        // Reset index back to start for next time
        m_readWriteIndex = 0;
    }

    return outputP + m_readWriteIndex;
}

void FFTConvolver::reset()
{
    PodZero(m_lastOverlapBuffer.Elements(), m_lastOverlapBuffer.Length());
    m_readWriteIndex = 0;
}

size_t FFTConvolver::latencyFrames() const
{
    return std::max<size_t>(fftSize()/2, WEBAUDIO_BLOCK_SIZE) -
        WEBAUDIO_BLOCK_SIZE;
}

} // namespace WebCore
