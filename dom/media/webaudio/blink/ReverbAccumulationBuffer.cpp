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

#include "ReverbAccumulationBuffer.h"
#include "AudioNodeEngine.h"
#include "mozilla/PodOperations.h"
#include <algorithm>

using namespace mozilla;

namespace WebCore {

ReverbAccumulationBuffer::ReverbAccumulationBuffer()
    : m_readIndex(0), m_readTimeFrame(0) {}

bool ReverbAccumulationBuffer::allocate(size_t length) {
  if (!m_buffer.SetLength(length, fallible)) {
    return false;
  }
  PodZero(m_buffer.Elements(), length);
  return true;
}

void ReverbAccumulationBuffer::readAndClear(float* destination,
                                            size_t numberOfFrames) {
  size_t bufferLength = m_buffer.Length();
  bool isCopySafe =
      m_readIndex <= bufferLength && numberOfFrames <= bufferLength;

  MOZ_ASSERT(isCopySafe);
  if (!isCopySafe) return;

  size_t framesAvailable = bufferLength - m_readIndex;
  size_t numberOfFrames1 = std::min(numberOfFrames, framesAvailable);
  size_t numberOfFrames2 = numberOfFrames - numberOfFrames1;

  float* source = m_buffer.Elements();
  memcpy(destination, source + m_readIndex, sizeof(float) * numberOfFrames1);
  memset(source + m_readIndex, 0, sizeof(float) * numberOfFrames1);

  // Handle wrap-around if necessary
  if (numberOfFrames2 > 0) {
    memcpy(destination + numberOfFrames1, source,
           sizeof(float) * numberOfFrames2);
    memset(source, 0, sizeof(float) * numberOfFrames2);
  }

  m_readIndex = (m_readIndex + numberOfFrames) % bufferLength;
  m_readTimeFrame += numberOfFrames;
}

void ReverbAccumulationBuffer::accumulate(const float* source,
                                          size_t numberOfFrames,
                                          size_t* readIndex,
                                          size_t delayFrames) {
  size_t bufferLength = m_buffer.Length();

  size_t writeIndex = (*readIndex + delayFrames) % bufferLength;

  // Update caller's readIndex
  *readIndex = (*readIndex + numberOfFrames) % bufferLength;

  size_t framesAvailable = bufferLength - writeIndex;
  size_t numberOfFrames1 = std::min(numberOfFrames, framesAvailable);
  size_t numberOfFrames2 = numberOfFrames - numberOfFrames1;

  float* destination = m_buffer.Elements();

  bool isSafe = writeIndex <= bufferLength &&
                numberOfFrames1 + writeIndex <= bufferLength &&
                numberOfFrames2 <= bufferLength;
  MOZ_ASSERT(isSafe);
  if (!isSafe) return;

  AudioBufferAddWithScale(source, 1.0f, destination + writeIndex,
                          numberOfFrames1);
  if (numberOfFrames2 > 0) {
    AudioBufferAddWithScale(source + numberOfFrames1, 1.0f, destination,
                            numberOfFrames2);
  }
}

void ReverbAccumulationBuffer::reset() {
  PodZero(m_buffer.Elements(), m_buffer.Length());
  m_readIndex = 0;
  m_readTimeFrame = 0;
}

}  // namespace WebCore
