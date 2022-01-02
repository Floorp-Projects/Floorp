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

#ifndef Reverb_h
#define Reverb_h

#include "ReverbConvolver.h"
#include "nsTArray.h"
#include "AudioBlock.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"

namespace WebCore {

using mozilla::UniquePtr;

// Multi-channel convolution reverb with channel matrixing - one or more
// ReverbConvolver objects are used internally.

class Reverb {
 public:
  enum { MaxFrameSize = 256 };

  // renderSliceSize is a rendering hint, so the FFTs can be optimized to not
  // all occur at the same time (very bad when rendering on a real-time thread).
  // aAllocation failure is to be checked by the caller. If false, internal
  // memory could not be allocated, and this Reverb instance is not to be
  // used.
  Reverb(const mozilla::AudioChunk& impulseResponseBuffer, size_t maxFFTSize,
         bool useBackgroundThreads, bool normalize, float sampleRate,
         bool* aAllocationFailure);

  void process(const mozilla::AudioBlock* sourceBus,
               mozilla::AudioBlock* destinationBus);

  size_t impulseResponseLength() const { return m_impulseResponseLength; }

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  bool initialize(const nsTArray<const float*>& impulseResponseBuffer,
                  size_t impulseResponseBufferLength, size_t maxFFTSize,
                  bool useBackgroundThreads);

  size_t m_impulseResponseLength;

  nsTArray<UniquePtr<ReverbConvolver> > m_convolvers;

  // For "True" stereo processing
  mozilla::AudioBlock m_tempBuffer;
};

}  // namespace WebCore

#endif  // Reverb_h
