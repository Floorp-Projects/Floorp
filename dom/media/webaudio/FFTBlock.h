/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FFTBlock_h_
#define FFTBlock_h_

#include "AlignedTArray.h"
#include "AudioNodeEngine.h"
#include "FFmpegRDFTTypes.h"
#include "FFVPXRuntimeLinker.h"

namespace mozilla {

// This class defines an FFT block, loosely modeled after Blink's FFTFrame
// class to make sharing code with Blink easy.
class FFTBlock final {
  union ComplexU {
    float f[2];
    struct {
      float r;
      float i;
    };
  };

 public:
  static void MainThreadInit() {
    FFVPXRuntimeLinker::Init();
    if (!sRDFTFuncs.init) {
      FFVPXRuntimeLinker::GetRDFTFuncs(&sRDFTFuncs);
    }
  }

  explicit FFTBlock(uint32_t aFFTSize) : mAvRDFT(nullptr), mAvIRDFT(nullptr) {
    MOZ_COUNT_CTOR(FFTBlock);
    SetFFTSize(aFFTSize);
  }
  ~FFTBlock() {
    MOZ_COUNT_DTOR(FFTBlock);
    Clear();
  }

  // Return a new FFTBlock with frequency components interpolated between
  // |block0| and |block1| with |interp| between 0.0 and 1.0.
  static FFTBlock* CreateInterpolatedBlock(const FFTBlock& block0,
                                           const FFTBlock& block1,
                                           double interp);

  // Transform FFTSize() points of aData and store the result internally.
  void PerformFFT(const float* aData) {
    if (!EnsureFFT()) {
      return;
    }

    PodCopy(mOutputBuffer.Elements()->f, aData, mFFTSize);
    sRDFTFuncs.calc(mAvRDFT, mOutputBuffer.Elements()->f);
    // Recover packed Nyquist.
    mOutputBuffer[mFFTSize / 2].r = mOutputBuffer[0].i;
    mOutputBuffer[0].i = 0.0f;
  }
  // Inverse-transform internal data and store the resulting FFTSize()
  // points in aDataOut.
  void GetInverse(float* aDataOut) {
    GetInverseWithoutScaling(aDataOut);
    AudioBufferInPlaceScale(aDataOut, 1.0f / mFFTSize, mFFTSize);
  }

  // Inverse-transform internal frequency data and store the resulting
  // FFTSize() points in |aDataOut|.  If frequency data has not already been
  // scaled, then the output will need scaling by 1/FFTSize().
  void GetInverseWithoutScaling(float* aDataOut) {
    if (!EnsureIFFT()) {
      std::fill_n(aDataOut, mFFTSize, 0.0f);
      return;
    };

    // Even though this function doesn't scale, the libav forward transform
    // gives a value that needs scaling by 2 in order for things to turn out
    // similar to how we expect from kissfft/openmax.
    AudioBufferCopyWithScale(mOutputBuffer.Elements()->f, 2.0f, aDataOut,
                             mFFTSize);
    aDataOut[1] = 2.0f * mOutputBuffer[mFFTSize / 2].r;  // Packed Nyquist
    sRDFTFuncs.calc(mAvIRDFT, aDataOut);
  }

  void Multiply(const FFTBlock& aFrame) {
    uint32_t halfSize = mFFTSize / 2;
    // DFTs are not packed.
    MOZ_ASSERT(mOutputBuffer[0].i == 0);
    MOZ_ASSERT(aFrame.mOutputBuffer[0].i == 0);

    BufferComplexMultiply(mOutputBuffer.Elements()->f,
                          aFrame.mOutputBuffer.Elements()->f,
                          mOutputBuffer.Elements()->f, halfSize);
    mOutputBuffer[halfSize].r *= aFrame.mOutputBuffer[halfSize].r;
    // This would have been set to NaN if either real component was NaN.
    mOutputBuffer[0].i = 0.0f;
  }

  // Perform a forward FFT on |aData|, assuming zeros after dataSize samples,
  // and pre-scale the generated internal frequency domain coefficients so
  // that GetInverseWithoutScaling() can be used to transform to the time
  // domain.  This is useful for convolution kernels.
  void PadAndMakeScaledDFT(const float* aData, size_t dataSize) {
    MOZ_ASSERT(dataSize <= FFTSize());
    AlignedTArray<float> paddedData;
    paddedData.SetLength(FFTSize());
    AudioBufferCopyWithScale(aData, 1.0f / FFTSize(), paddedData.Elements(),
                             dataSize);
    PodZero(paddedData.Elements() + dataSize, mFFTSize - dataSize);
    PerformFFT(paddedData.Elements());
  }

  // aSize must be a power of 2
  void SetFFTSize(uint32_t aSize) {
    MOZ_ASSERT(CountPopulation32(aSize) == 1);
    mFFTSize = aSize;
    mOutputBuffer.SetLength(aSize / 2 + 1);
    PodZero(mOutputBuffer.Elements(), aSize / 2 + 1);
    Clear();
  }

  // Return the average group delay and removes this from the frequency data.
  double ExtractAverageGroupDelay();

  uint32_t FFTSize() const { return mFFTSize; }
  float RealData(uint32_t aIndex) const { return mOutputBuffer[aIndex].r; }
  float& RealData(uint32_t aIndex) { return mOutputBuffer[aIndex].r; }
  float ImagData(uint32_t aIndex) const { return mOutputBuffer[aIndex].i; }
  float& ImagData(uint32_t aIndex) { return mOutputBuffer[aIndex].i; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t amount = 0;

    auto ComputedSizeOfContextIfSet = [this](void* aContext) -> size_t {
      if (!aContext) {
        return 0;
      }
      // RDFTContext is only forward declared in public headers, but this is
      // an estimate based on a value of 231 seen requested from
      // _aligned_alloc on Win64.  Don't use malloc_usable_size() because the
      // context pointer is not necessarily from malloc.
      size_t amount = 232;
      // Add size of allocations performed in ff_fft_init().
      // The maximum FFT size used is 32768 = 2^15 and so revtab32 is not
      // allocated.
      MOZ_ASSERT(mFFTSize <= 32768);
      amount += mFFTSize * (sizeof(uint16_t) + 2 * sizeof(float));

      return amount;
    };

    amount += ComputedSizeOfContextIfSet(mAvRDFT);
    amount += ComputedSizeOfContextIfSet(mAvIRDFT);
    amount += mOutputBuffer.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  FFTBlock(const FFTBlock& other) = delete;
  void operator=(const FFTBlock& other) = delete;

  bool EnsureFFT() {
    if (!mAvRDFT) {
      if (!sRDFTFuncs.init) {
        return false;
      }

      mAvRDFT = sRDFTFuncs.init(FloorLog2(mFFTSize), DFT_R2C);
    }
    return true;
  }

  bool EnsureIFFT() {
    if (!mAvIRDFT) {
      if (!sRDFTFuncs.init) {
        return false;
      }

      mAvIRDFT = sRDFTFuncs.init(FloorLog2(mFFTSize), IDFT_C2R);
    }
    return true;
  }

  void Clear() {
    if (mAvRDFT) {
      sRDFTFuncs.end(mAvRDFT);
      mAvRDFT = nullptr;
    }
    if (mAvIRDFT) {
      sRDFTFuncs.end(mAvIRDFT);
      mAvIRDFT = nullptr;
    }
  }
  void AddConstantGroupDelay(double sampleFrameDelay);
  void InterpolateFrequencyComponents(const FFTBlock& block0,
                                      const FFTBlock& block1, double interp);
  static FFmpegRDFTFuncs sRDFTFuncs;
  RDFTContext* mAvRDFT;
  RDFTContext* mAvIRDFT;
  AlignedTArray<ComplexU> mOutputBuffer;
  uint32_t mFFTSize{};
};

}  // namespace mozilla

#endif
