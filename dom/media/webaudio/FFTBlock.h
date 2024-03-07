/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FFTBlock_h_
#define FFTBlock_h_

#include "AlignedTArray.h"
#include "AudioNodeEngine.h"
#include "FFVPXRuntimeLinker.h"
#include "ffvpx/tx.h"

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
    if (!sFFTFuncs.init) {
      FFVPXRuntimeLinker::GetFFTFuncs(&sFFTFuncs);
    }
  }
  explicit FFTBlock(uint32_t aFFTSize, float aInverseScaling = 1.0f)
      : mInverseScaling(aInverseScaling) {
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

    mFn(mTxCtx, mOutputBuffer.Elements()->f, const_cast<float*>(aData),
        2 * sizeof(float));
#ifdef DEBUG
    mInversePerformed = false;
#endif
  }
  // Inverse-transform internal frequency data and store the resulting
  // FFTSize() points in |aDataOut|.  If frequency data has not already been
  // scaled, then the output will need scaling by 1/FFTSize().
  void GetInverse(float* aDataOut) {
    if (!EnsureIFFT()) {
      std::fill_n(aDataOut, mFFTSize, 0.0f);
      return;
    };
    // When performing an inverse transform, tx overwrites the input. This
    // asserts that forward / inverse transforms are interleaved to avoid having
    // to keep the input around.
    MOZ_ASSERT(!mInversePerformed);
    mIFn(mITxCtx, aDataOut, mOutputBuffer.Elements()->f, 2 * sizeof(float));
#ifdef DEBUG
    mInversePerformed = true;
#endif
  }

  void Multiply(const FFTBlock& aFrame) {
    MOZ_ASSERT(!mInversePerformed);

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
    AudioBufferCopyWithScale(aData, 1.0f / AssertedCast<float>(FFTSize()),
                             paddedData.Elements(), dataSize);
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
  float RealData(uint32_t aIndex) const {
    MOZ_ASSERT(!mInversePerformed);
    return mOutputBuffer[aIndex].r;
  }
  float& RealData(uint32_t aIndex) {
    MOZ_ASSERT(!mInversePerformed);
    return mOutputBuffer[aIndex].r;
  }
  float ImagData(uint32_t aIndex) const {
    MOZ_ASSERT(!mInversePerformed);
    return mOutputBuffer[aIndex].i;
  }
  float& ImagData(uint32_t aIndex) {
    MOZ_ASSERT(!mInversePerformed);
    return mOutputBuffer[aIndex].i;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t amount = 0;

    // malloc_usable_size can't be used here because the pointer isn't
    // necessarily from malloc. This value has been manually checked.
    if (mTxCtx) {
      amount += 711;
    }
    if (mTxCtx) {
      amount += 711;
    }

    amount += mOutputBuffer.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  FFTBlock(const FFTBlock& other) = delete;
  void operator=(const FFTBlock& other) = delete;

 private:
  bool EnsureFFT() {
    if (!mTxCtx) {
      if (!sFFTFuncs.init) {
        return false;
      }
      // Forward transform is always unscaled for our purpose.
      float scale = 1.0f;
      int rv = sFFTFuncs.init(&mTxCtx, &mFn, AV_TX_FLOAT_RDFT, 0 /* forward */,
                              AssertedCast<int>(mFFTSize), &scale, 0);
      MOZ_ASSERT(!rv, "av_tx_init: invalid parameters (forward)");
      return !rv;
    }
    return true;
  }
  bool EnsureIFFT() {
    if (!mITxCtx) {
      if (!sFFTFuncs.init) {
        return false;
      }
      int rv =
          sFFTFuncs.init(&mITxCtx, &mIFn, AV_TX_FLOAT_RDFT, 1 /* inverse */,
                         AssertedCast<int>(mFFTSize), &mInverseScaling, 0);
      MOZ_ASSERT(!rv, "av_tx_init: invalid parameters (inverse)");
      return !rv;
    }
    return true;
  }
  void Clear() {
    if (mTxCtx) {
      sFFTFuncs.uninit(&mTxCtx);
      mTxCtx = nullptr;
      mFn = nullptr;
    }
    if (mITxCtx) {
      sFFTFuncs.uninit(&mITxCtx);
      mITxCtx = nullptr;
      mIFn = nullptr;
    }
  }
  void AddConstantGroupDelay(double sampleFrameDelay);
  void InterpolateFrequencyComponents(const FFTBlock& block0,
                                      const FFTBlock& block1, double interp);
  static FFmpegFFTFuncs sFFTFuncs;
  // Context and function pointer for forward transform
  AVTXContext* mTxCtx{};
  av_tx_fn mFn{};
  // Context and function pointer for inverse transform
  AVTXContext* mITxCtx{};
  av_tx_fn mIFn{};
  AlignedTArray<ComplexU> mOutputBuffer;
  uint32_t mFFTSize{};
  // A scaling that is performed when doing an inverse transform. The forward
  // transform is always unscaled.
  float mInverseScaling;
#ifdef DEBUG
  bool mInversePerformed = false;
#endif
};

}  // namespace mozilla

#endif
