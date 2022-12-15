/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PeriodicWave.h"
#include "AudioContext.h"
#include "mozilla/dom/PeriodicWaveBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PeriodicWave, mContext)

PeriodicWave::PeriodicWave(AudioContext* aContext, const float* aRealData,
                           const uint32_t aRealSize, const float* aImagData,
                           const uint32_t aImagSize,
                           const bool aDisableNormalization, ErrorResult& aRv)
    : mContext(aContext), mDisableNormalization(aDisableNormalization) {
  if (aRealData && aImagData && aRealSize != aImagSize) {
    aRv.ThrowIndexSizeError("\"real\" and \"imag\" are different in length");
    return;
  }

  uint32_t length = 0;
  if (aRealData) {
    length = aRealSize;
  } else if (aImagData) {
    length = aImagSize;
  } else {
    // If nothing has been passed, this PeriodicWave will be a sine wave: 2
    // elements for each array, the second imaginary component set to 1.0.
    length = 2;
  }

  if (length < 2) {
    aRv.ThrowIndexSizeError(
        "\"real\" and \"imag\" must have a length of "
        "at least 2");
    return;
  }

  MOZ_ASSERT(aContext);
  MOZ_ASSERT((aRealData || aImagData) || length == 2);

  // Caller should have checked this and thrown.
  MOZ_ASSERT(length >= 2);
  mCoefficients.mDuration = length;

  // Copy coefficient data.
  // The SharedBuffer and two arrays share a single allocation.
  CheckedInt<size_t> bufferSize(sizeof(float));
  bufferSize *= length;
  bufferSize *= 2;
  RefPtr<SharedBuffer> buffer = SharedBuffer::Create(bufferSize, fallible);
  if (!buffer) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  auto data = static_cast<float*>(buffer->Data());
  mCoefficients.mBuffer = std::move(buffer);

  if (!aRealData && !aImagData) {
    PodZero(data, length);
    mCoefficients.mChannelData.AppendElement(data);
    data += length;
    data[0] = 0.0f;
    data[1] = 1.0f;
    mCoefficients.mChannelData.AppendElement(data);
  } else {
    if (aRealData) {
      PodCopy(data, aRealData, length);
    } else {
      PodZero(data, length);
    }
    mCoefficients.mChannelData.AppendElement(data);

    data += length;
    if (aImagData) {
      PodCopy(data, aImagData, length);
    } else {
      PodZero(data, length);
    }
    mCoefficients.mChannelData.AppendElement(data);
  }
  mCoefficients.mVolume = 1.0f;
  mCoefficients.mBufferFormat = AUDIO_FORMAT_FLOAT32;
}

/* static */
already_AddRefed<PeriodicWave> PeriodicWave::Constructor(
    const GlobalObject& aGlobal, AudioContext& aAudioContext,
    const PeriodicWaveOptions& aOptions, ErrorResult& aRv) {
  const float* realData;
  const float* imagData;
  uint32_t realSize;
  uint32_t imagSize;

  if (aOptions.mReal.WasPassed()) {
    realData = aOptions.mReal.Value().Elements();
    realSize = aOptions.mReal.Value().Length();
  } else {
    realData = nullptr;
    realSize = 0;
  }

  if (aOptions.mImag.WasPassed()) {
    imagData = aOptions.mImag.Value().Elements();
    imagSize = aOptions.mImag.Value().Length();
  } else {
    imagData = nullptr;
    imagSize = 0;
  }

  RefPtr<PeriodicWave> wave =
      new PeriodicWave(&aAudioContext, realData, realSize, imagData, imagSize,
                       aOptions.mDisableNormalization, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return wave.forget();
}

size_t PeriodicWave::SizeOfExcludingThisIfNotShared(
    MallocSizeOf aMallocSizeOf) const {
  // Not owned:
  // - mContext
  size_t amount = 0;
  amount += mCoefficients.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

  return amount;
}

size_t PeriodicWave::SizeOfIncludingThisIfNotShared(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThisIfNotShared(aMallocSizeOf);
}

JSObject* PeriodicWave::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return PeriodicWave_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
