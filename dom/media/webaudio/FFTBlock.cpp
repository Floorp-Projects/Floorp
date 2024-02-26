/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cindent: */
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

#include "FFTBlock.h"

#include <complex>

namespace mozilla {

typedef std::complex<double> Complex;

#ifdef MOZ_LIBAV_FFT
FFmpegRDFTFuncs FFTBlock::sRDFTFuncs;
#endif

static double fdlibm_cabs(const Complex& z) {
  return fdlibm_hypot(real(z), imag(z));
}

static double fdlibm_carg(const Complex& z) {
  return fdlibm_atan2(imag(z), real(z));
}

FFTBlock* FFTBlock::CreateInterpolatedBlock(const FFTBlock& block0,
                                            const FFTBlock& block1,
                                            double interp) {
  FFTBlock* newBlock = new FFTBlock(block0.FFTSize());

  newBlock->InterpolateFrequencyComponents(block0, block1, interp);

  // In the time-domain, the 2nd half of the response must be zero, to avoid
  // circular convolution aliasing...
  int fftSize = newBlock->FFTSize();
  AlignedTArray<float> buffer(fftSize);
  newBlock->GetInverseWithoutScaling(buffer.Elements());
  AudioBufferInPlaceScale(buffer.Elements(), 1.0f / fftSize, fftSize / 2);
  PodZero(buffer.Elements() + fftSize / 2, fftSize / 2);

  // Put back into frequency domain.
  newBlock->PerformFFT(buffer.Elements());

  return newBlock;
}

void FFTBlock::InterpolateFrequencyComponents(const FFTBlock& block0,
                                              const FFTBlock& block1,
                                              double interp) {
  // FIXME : with some work, this method could be optimized

  ComplexU* dft = mOutputBuffer.Elements();

  const ComplexU* dft1 = block0.mOutputBuffer.Elements();
  const ComplexU* dft2 = block1.mOutputBuffer.Elements();

  MOZ_ASSERT(mFFTSize == block0.FFTSize());
  MOZ_ASSERT(mFFTSize == block1.FFTSize());
  double s1base = (1.0 - interp);
  double s2base = interp;

  double phaseAccum = 0.0;
  double lastPhase1 = 0.0;
  double lastPhase2 = 0.0;

  int n = mFFTSize / 2;

  dft[0].r = static_cast<float>(s1base * dft1[0].r + s2base * dft2[0].r);
  dft[n].r = static_cast<float>(s1base * dft1[n].r + s2base * dft2[n].r);

  for (int i = 1; i < n; ++i) {
    Complex c1(dft1[i].r, dft1[i].i);
    Complex c2(dft2[i].r, dft2[i].i);

    double mag1 = fdlibm_cabs(c1);
    double mag2 = fdlibm_cabs(c2);

    // Interpolate magnitudes in decibels
    double mag1db = 20.0 * fdlibm_log10(mag1);
    double mag2db = 20.0 * fdlibm_log10(mag2);

    double s1 = s1base;
    double s2 = s2base;

    double magdbdiff = mag1db - mag2db;

    // Empirical tweak to retain higher-frequency zeroes
    double threshold = (i > 16) ? 5.0 : 2.0;

    if (magdbdiff < -threshold && mag1db < 0.0) {
      s1 = fdlibm_pow(s1, 0.75);
      s2 = 1.0 - s1;
    } else if (magdbdiff > threshold && mag2db < 0.0) {
      s2 = fdlibm_pow(s2, 0.75);
      s1 = 1.0 - s2;
    }

    // Average magnitude by decibels instead of linearly
    double magdb = s1 * mag1db + s2 * mag2db;
    double mag = fdlibm_pow(10.0, 0.05 * magdb);

    // Now, deal with phase
    double phase1 = fdlibm_carg(c1);
    double phase2 = fdlibm_carg(c2);

    double deltaPhase1 = phase1 - lastPhase1;
    double deltaPhase2 = phase2 - lastPhase2;
    lastPhase1 = phase1;
    lastPhase2 = phase2;

    // Unwrap phase deltas
    if (deltaPhase1 > M_PI) deltaPhase1 -= 2.0 * M_PI;
    if (deltaPhase1 < -M_PI) deltaPhase1 += 2.0 * M_PI;
    if (deltaPhase2 > M_PI) deltaPhase2 -= 2.0 * M_PI;
    if (deltaPhase2 < -M_PI) deltaPhase2 += 2.0 * M_PI;

    // Blend group-delays
    double deltaPhaseBlend;

    if (deltaPhase1 - deltaPhase2 > M_PI)
      deltaPhaseBlend = s1 * deltaPhase1 + s2 * (2.0 * M_PI + deltaPhase2);
    else if (deltaPhase2 - deltaPhase1 > M_PI)
      deltaPhaseBlend = s1 * (2.0 * M_PI + deltaPhase1) + s2 * deltaPhase2;
    else
      deltaPhaseBlend = s1 * deltaPhase1 + s2 * deltaPhase2;

    phaseAccum += deltaPhaseBlend;

    // Unwrap
    if (phaseAccum > M_PI) phaseAccum -= 2.0 * M_PI;
    if (phaseAccum < -M_PI) phaseAccum += 2.0 * M_PI;

    dft[i].r = static_cast<float>(mag * fdlibm_cos(phaseAccum));
    dft[i].i = static_cast<float>(mag * fdlibm_sin(phaseAccum));
  }
}

double FFTBlock::ExtractAverageGroupDelay() {
  ComplexU* dft = mOutputBuffer.Elements();

  double aveSum = 0.0;
  double weightSum = 0.0;
  double lastPhase = 0.0;

  int halfSize = FFTSize() / 2;

  const double kSamplePhaseDelay = (2.0 * M_PI) / double(FFTSize());

  // Remove DC offset
  dft[0].r = 0.0f;

  // Calculate weighted average group delay
  for (int i = 1; i < halfSize; i++) {
    Complex c(dft[i].r, dft[i].i);
    double mag = fdlibm_cabs(c);
    double phase = fdlibm_carg(c);

    double deltaPhase = phase - lastPhase;
    lastPhase = phase;

    // Unwrap
    if (deltaPhase < -M_PI) deltaPhase += 2.0 * M_PI;
    if (deltaPhase > M_PI) deltaPhase -= 2.0 * M_PI;

    aveSum += mag * deltaPhase;
    weightSum += mag;
  }

  // Note how we invert the phase delta wrt frequency since this is how group
  // delay is defined
  double ave = aveSum / weightSum;
  double aveSampleDelay = -ave / kSamplePhaseDelay;

  // Leave 20 sample headroom (for leading edge of impulse)
  aveSampleDelay -= 20.0;
  if (aveSampleDelay <= 0.0) return 0.0;

  // Remove average group delay (minus 20 samples for headroom)
  AddConstantGroupDelay(-aveSampleDelay);

  return aveSampleDelay;
}

void FFTBlock::AddConstantGroupDelay(double sampleFrameDelay) {
  int halfSize = FFTSize() / 2;

  ComplexU* dft = mOutputBuffer.Elements();

  const double kSamplePhaseDelay = (2.0 * M_PI) / double(FFTSize());

  double phaseAdj = -sampleFrameDelay * kSamplePhaseDelay;

  // Add constant group delay
  for (int i = 1; i < halfSize; i++) {
    Complex c(dft[i].r, dft[i].i);
    double mag = fdlibm_cabs(c);
    double phase = fdlibm_carg(c);

    phase += i * phaseAdj;

    dft[i].r = static_cast<float>(mag * fdlibm_cos(phase));
    dft[i].i = static_cast<float>(mag * fdlibm_sin(phase));
  }
}

}  // namespace mozilla
