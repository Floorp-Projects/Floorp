/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "ZeroPole.h"

#include "DenormalDisabler.h"

#include <cmath>
#include <float.h>

namespace WebCore {

void ZeroPole::process(const float* source, float* destination,
                       int framesToProcess) {
  float zero = m_zero;
  float pole = m_pole;

  // Gain compensation to make 0dB @ 0Hz
  const float k1 = 1 / (1 - zero);
  const float k2 = 1 - pole;

  // Member variables to locals.
  float lastX = m_lastX;
  float lastY = m_lastY;

  for (int i = 0; i < framesToProcess; ++i) {
    float input = source[i];

    // Zero
    float output1 = k1 * (input - zero * lastX);
    lastX = input;

    // Pole
    float output2 = k2 * output1 + pole * lastY;
    lastY = output2;

    destination[i] = output2;
  }

// Locals to member variables. Flush denormals here so we don't
// slow down the inner loop above.
#ifndef HAVE_DENORMAL
  if (lastX == 0.0f && lastY != 0.0f && fabsf(lastY) < FLT_MIN) {
    // Flush future values to zero (until there is new input).
    lastY = 0.0;

    // Flush calculated values.
    for (int i = framesToProcess; i-- && fabsf(destination[i]) < FLT_MIN;) {
      destination[i] = 0.0f;
    }
  }
#endif

  m_lastX = lastX;
  m_lastY = lastY;
}

}  // namespace WebCore
