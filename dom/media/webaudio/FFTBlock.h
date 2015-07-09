/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FFTBlock_h_
#define FFTBlock_h_

#ifdef BUILD_ARM_NEON
#include <cmath>
#include "mozilla/arm.h"
#include "dl/sp/api/omxSP.h"
#endif

#include "AlignedTArray.h"
#include "AudioNodeEngine.h"
#include "kiss_fft/kiss_fftr.h"

namespace mozilla {

// This class defines an FFT block, loosely modeled after Blink's FFTFrame
// class to make sharing code with Blink easy.
// Currently it's implemented on top of KissFFT on all platforms.
class FFTBlock final
{
  union ComplexU {
    kiss_fft_cpx c;
    float f[2];
    struct {
      float r;
      float i;
    };
  };

public:
  explicit FFTBlock(uint32_t aFFTSize)
    : mKissFFT(nullptr)
    , mKissIFFT(nullptr)
#ifdef BUILD_ARM_NEON
    , mOmxFFT(nullptr)
    , mOmxIFFT(nullptr)
#endif
  {
    MOZ_COUNT_CTOR(FFTBlock);
    SetFFTSize(aFFTSize);
  }
  ~FFTBlock()
  {
    MOZ_COUNT_DTOR(FFTBlock);
    Clear();
  }

  // Return a new FFTBlock with frequency components interpolated between
  // |block0| and |block1| with |interp| between 0.0 and 1.0.
  static FFTBlock*
  CreateInterpolatedBlock(const FFTBlock& block0,
                          const FFTBlock& block1, double interp);

  // Transform FFTSize() points of aData and store the result internally.
  void PerformFFT(const float* aData)
  {
    EnsureFFT();
#ifdef BUILD_ARM_NEON
    if (mozilla::supports_neon()) {
      omxSP_FFTFwd_RToCCS_F32_Sfs(aData, mOutputBuffer.Elements()->f, mOmxFFT);
    } else
#endif
    {
      kiss_fftr(mKissFFT, aData, &(mOutputBuffer.Elements()->c));
    }
  }
  // Inverse-transform internal data and store the resulting FFTSize()
  // points in aDataOut.
  void GetInverse(float* aDataOut)
  {
    GetInverseWithoutScaling(aDataOut);
    AudioBufferInPlaceScale(aDataOut, 1.0f / mFFTSize, mFFTSize);
  }
  // Inverse-transform internal frequency data and store the resulting
  // FFTSize() points in |aDataOut|.  If frequency data has not already been
  // scaled, then the output will need scaling by 1/FFTSize().
  void GetInverseWithoutScaling(float* aDataOut)
  {
    EnsureIFFT();
#ifdef BUILD_ARM_NEON
    if (mozilla::supports_neon()) {
      omxSP_FFTInv_CCSToR_F32_Sfs(mOutputBuffer.Elements()->f, aDataOut, mOmxIFFT);
      // There is no function that computes de inverse FFT without scaling, so
      // we have to scale back up here. Bug 1158741.
      AudioBufferInPlaceScale(aDataOut, mFFTSize, mFFTSize);
    } else
#endif
    {
      kiss_fftri(mKissIFFT, &(mOutputBuffer.Elements()->c), aDataOut);
    }
  }
  // Inverse-transform the FFTSize()/2+1 points of data in each
  // of aRealDataIn and aImagDataIn and store the resulting
  // FFTSize() points in aRealDataOut.
  void PerformInverseFFT(float* aRealDataIn,
                         float *aImagDataIn,
                         float *aRealDataOut)
  {
    EnsureIFFT();
    const uint32_t inputSize = mFFTSize / 2 + 1;
    AlignedTArray<ComplexU> inputBuffer(inputSize);
    for (uint32_t i = 0; i < inputSize; ++i) {
      inputBuffer[i].r = aRealDataIn[i];
      inputBuffer[i].i = aImagDataIn[i];
    }
#ifdef BUILD_ARM_NEON
    if (mozilla::supports_neon()) {
      omxSP_FFTInv_CCSToR_F32_Sfs(inputBuffer.Elements()->f,
                                  aRealDataOut, mOmxIFFT);
    } else
#endif
    {
      kiss_fftri(mKissIFFT, &(inputBuffer.Elements()->c), aRealDataOut);
      for (uint32_t i = 0; i < mFFTSize; ++i) {
        aRealDataOut[i] /= mFFTSize;
      }
    }
  }

  void Multiply(const FFTBlock& aFrame)
  {
    BufferComplexMultiply(mOutputBuffer.Elements()->f,
                          aFrame.mOutputBuffer.Elements()->f,
                          mOutputBuffer.Elements()->f,
                          mFFTSize / 2 + 1);
  }

  // Perform a forward FFT on |aData|, assuming zeros after dataSize samples,
  // and pre-scale the generated internal frequency domain coefficients so
  // that GetInverseWithoutScaling() can be used to transform to the time
  // domain.  This is useful for convolution kernels.
  void PadAndMakeScaledDFT(const float* aData, size_t dataSize)
  {
    MOZ_ASSERT(dataSize <= FFTSize());
    AlignedTArray<float> paddedData;
    paddedData.SetLength(FFTSize());
    AudioBufferCopyWithScale(aData, 1.0f / FFTSize(),
                             paddedData.Elements(), dataSize);
    PodZero(paddedData.Elements() + dataSize, mFFTSize - dataSize);
    PerformFFT(paddedData.Elements());
  }

  void SetFFTSize(uint32_t aSize)
  {
    mFFTSize = aSize;
    mOutputBuffer.SetLength(aSize / 2 + 1);
    PodZero(mOutputBuffer.Elements(), aSize / 2 + 1);
    Clear();
  }

  // Return the average group delay and removes this from the frequency data.
  double ExtractAverageGroupDelay();

  uint32_t FFTSize() const
  {
    return mFFTSize;
  }
  float RealData(uint32_t aIndex) const
  {
    return mOutputBuffer[aIndex].r;
  }
  float ImagData(uint32_t aIndex) const
  {
    return mOutputBuffer[aIndex].i;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t amount = 0;
    amount += aMallocSizeOf(mKissFFT);
    amount += aMallocSizeOf(mKissIFFT);
    amount += mOutputBuffer.SizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  FFTBlock(const FFTBlock& other) = delete;
  void operator=(const FFTBlock& other) = delete;

  void EnsureFFT()
  {
#ifdef BUILD_ARM_NEON
    if (mozilla::supports_neon()) {
      if (!mOmxFFT) {
        mOmxFFT = createOmxFFT(mFFTSize);
      }
    } else
#endif
    {
      if (!mKissFFT) {
        mKissFFT = kiss_fftr_alloc(mFFTSize, 0, nullptr, nullptr);
      }
    }
  }
  void EnsureIFFT()
  {
#ifdef BUILD_ARM_NEON
    if (mozilla::supports_neon()) {
      if (!mOmxIFFT) {
        mOmxIFFT = createOmxFFT(mFFTSize);
      }
    } else
#endif
    {
      if (!mKissIFFT) {
        mKissIFFT = kiss_fftr_alloc(mFFTSize, 1, nullptr, nullptr);
      }
    }
  }

#ifdef BUILD_ARM_NEON
  static OMXFFTSpec_R_F32* createOmxFFT(uint32_t aFFTSize)
  {
    MOZ_ASSERT((aFFTSize & (aFFTSize-1)) == 0);
    OMX_INT bufSize;
    OMX_INT order = log((double)aFFTSize)/M_LN2;
    MOZ_ASSERT(aFFTSize>>order == 1);
    OMXResult status = omxSP_FFTGetBufSize_R_F32(order, &bufSize);
    if (status == OMX_Sts_NoErr) {
      OMXFFTSpec_R_F32* context = static_cast<OMXFFTSpec_R_F32*>(malloc(bufSize));
      if (omxSP_FFTInit_R_F32(context, order) != OMX_Sts_NoErr) {
        return nullptr;
      }
      return context;
    }
    return nullptr;
  }
#endif

  void Clear()
  {
#ifdef BUILD_ARM_NEON
    free(mOmxFFT);
    free(mOmxIFFT);
    mOmxFFT = mOmxIFFT = nullptr;
#endif
    free(mKissFFT);
    free(mKissIFFT);
    mKissFFT = mKissIFFT = nullptr;
  }
  void AddConstantGroupDelay(double sampleFrameDelay);
  void InterpolateFrequencyComponents(const FFTBlock& block0,
                                      const FFTBlock& block1, double interp);

  kiss_fftr_cfg mKissFFT;
  kiss_fftr_cfg mKissIFFT;
#ifdef BUILD_ARM_NEON
  OMXFFTSpec_R_F32* mOmxFFT;
  OMXFFTSpec_R_F32* mOmxIFFT;
#endif
  AlignedTArray<ComplexU> mOutputBuffer;
  uint32_t mFFTSize;
};
}

#endif

