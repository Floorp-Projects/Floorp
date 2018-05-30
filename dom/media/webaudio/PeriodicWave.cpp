/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PeriodicWave.h"
#include "AudioContext.h"
#include "mozilla/dom/PeriodicWaveBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PeriodicWave, mContext)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(PeriodicWave, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(PeriodicWave, Release)

PeriodicWave::PeriodicWave(AudioContext* aContext,
                           const float* aRealData,
                           const float* aImagData,
                           const uint32_t aLength,
                           const bool aDisableNormalization,
                           ErrorResult& aRv)
  : mContext(aContext)
  , mDisableNormalization(aDisableNormalization)
{
  MOZ_ASSERT(aContext);
  MOZ_ASSERT(aRealData || aImagData);

  // Caller should have checked this and thrown.
  MOZ_ASSERT(aLength > 0);
  mCoefficients.mDuration = aLength;

  // Copy coefficient data.
  // The SharedBuffer and two arrays share a single allocation.
  RefPtr<SharedBuffer> buffer =
    SharedBuffer::Create(sizeof(float) * aLength * 2, fallible);
  if (!buffer) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  auto data = static_cast<float*>(buffer->Data());
  mCoefficients.mBuffer = std::move(buffer);

  if (aRealData) {
    PodCopy(data, aRealData, aLength);
  } else {
    PodZero(data, aLength);
  }
  mCoefficients.mChannelData.AppendElement(data);

  data += aLength;
  if (aImagData) {
    PodCopy(data, aImagData, aLength);
  } else {
    PodZero(data, aLength);
  }
  mCoefficients.mChannelData.AppendElement(data);

  mCoefficients.mVolume = 1.0f;
  mCoefficients.mBufferFormat = AUDIO_FORMAT_FLOAT32;
}

/* static */ already_AddRefed<PeriodicWave>
PeriodicWave::Constructor(const GlobalObject& aGlobal,
                          AudioContext& aAudioContext,
                          const PeriodicWaveOptions& aOptions,
                          ErrorResult& aRv)
{
  if (!aOptions.mReal.WasPassed() && !aOptions.mImag.WasPassed()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  if (aOptions.mReal.WasPassed() && aOptions.mImag.WasPassed() &&
      aOptions.mReal.Value().Length() != aOptions.mImag.Value().Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  uint32_t length =
    aOptions.mReal.WasPassed() ? aOptions.mReal.Value().Length() : aOptions.mImag.Value().Length();
  if (length == 0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  const float* realData =
    aOptions.mReal.WasPassed() ? aOptions.mReal.Value().Elements() : nullptr;
  const float* imagData =
    aOptions.mImag.WasPassed() ? aOptions.mImag.Value().Elements() : nullptr;

  RefPtr<PeriodicWave> wave =
    new PeriodicWave(&aAudioContext, realData, imagData, length,
                     aOptions.mDisableNormalization, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return wave.forget();
}

size_t
PeriodicWave::SizeOfExcludingThisIfNotShared(MallocSizeOf aMallocSizeOf) const
{
  // Not owned:
  // - mContext
  size_t amount = 0;
  amount += mCoefficients.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

  return amount;
}

size_t
PeriodicWave::SizeOfIncludingThisIfNotShared(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThisIfNotShared(aMallocSizeOf);
}

JSObject*
PeriodicWave::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PeriodicWaveBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
