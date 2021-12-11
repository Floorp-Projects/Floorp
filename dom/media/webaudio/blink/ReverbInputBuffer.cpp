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

#include "ReverbInputBuffer.h"
#include "mozilla/PodOperations.h"

using namespace mozilla;

namespace WebCore {

ReverbInputBuffer::ReverbInputBuffer(size_t length) : m_writeIndex(0) {
  m_buffer.SetLength(length);
  PodZero(m_buffer.Elements(), length);
}

void ReverbInputBuffer::write(const float* sourceP, size_t numberOfFrames) {
  // m_writeIndex is atomic and checked by other threads, so only touch
  // it at the start and end.
  size_t bufferLength = m_buffer.Length();
  size_t index = m_writeIndex;
  size_t newIndex = index + numberOfFrames;

  MOZ_RELEASE_ASSERT(newIndex <= bufferLength);

  memcpy(m_buffer.Elements() + index, sourceP, sizeof(float) * numberOfFrames);

  if (newIndex >= bufferLength) {
    newIndex = 0;
  }
  m_writeIndex = newIndex;
}

float* ReverbInputBuffer::directReadFrom(int* readIndex,
                                         size_t numberOfFrames) {
  size_t bufferLength = m_buffer.Length();
  bool isPointerGood = readIndex && *readIndex >= 0 &&
                       *readIndex + numberOfFrames <= bufferLength;
  MOZ_ASSERT(isPointerGood);
  if (!isPointerGood) {
    // Should never happen in practice but return pointer to start of buffer
    // (avoid crash)
    if (readIndex) *readIndex = 0;
    return m_buffer.Elements();
  }

  float* sourceP = m_buffer.Elements();
  float* p = sourceP + *readIndex;

  // Update readIndex
  *readIndex = (*readIndex + numberOfFrames) % bufferLength;

  return p;
}

void ReverbInputBuffer::reset() {
  PodZero(m_buffer.Elements(), m_buffer.Length());
  m_writeIndex = 0;
}

}  // namespace WebCore
