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

void FFTConvolver::process(FFTBlock* fftKernel, const float* sourceP, float* destP)
{
    size_t halfSize = fftSize() / 2;

    // WEBAUDIO_BLOCK_SIZE must be an exact multiple of halfSize,
    // or halfSize is a multiple of WEBAUDIO_BLOCK_SIZE when halfSize > WEBAUDIO_BLOCK_SIZE.
    MOZ_ASSERT(!(halfSize % WEBAUDIO_BLOCK_SIZE &&
                 WEBAUDIO_BLOCK_SIZE % halfSize));

    size_t numberOfDivisions = halfSize <= WEBAUDIO_BLOCK_SIZE ?
        (WEBAUDIO_BLOCK_SIZE / halfSize) : 1;
    size_t divisionSize = numberOfDivisions == 1 ?
        WEBAUDIO_BLOCK_SIZE : halfSize;

    for (size_t i = 0; i < numberOfDivisions; ++i, sourceP += divisionSize, destP += divisionSize) {
        // Copy samples to input buffer (note contraint above!)
        float* inputP = m_inputBuffer.Elements();

        // Sanity check
        bool isCopyGood1 = sourceP && inputP && m_readWriteIndex + divisionSize <= m_inputBuffer.Length();
        MOZ_ASSERT(isCopyGood1);
        if (!isCopyGood1)
            return;

        memcpy(inputP + m_readWriteIndex, sourceP, sizeof(float) * divisionSize);

        float* outputP = m_outputBuffer.Elements();
        m_readWriteIndex += divisionSize;

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
            bool isCopyGood3 = m_outputBuffer.Length() == 2 * halfSize && m_lastOverlapBuffer.Length() == halfSize;
            MOZ_ASSERT(isCopyGood3);
            if (!isCopyGood3)
                return;

            memcpy(m_lastOverlapBuffer.Elements(), m_outputBuffer.Elements() + halfSize, sizeof(float) * halfSize);

            // Reset index back to start for next time
            m_readWriteIndex = 0;
        }

        // Sanity check
        bool isCopyGood2 = destP && outputP && m_readWriteIndex + divisionSize <= m_outputBuffer.Length();
        MOZ_ASSERT(isCopyGood2);
        if (!isCopyGood2)
            return;

        // Copy samples from output buffer
        memcpy(destP, outputP + m_readWriteIndex, sizeof(float) * divisionSize);
    }
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
