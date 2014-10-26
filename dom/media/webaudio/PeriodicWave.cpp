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
                           ErrorResult& aRv)
  : mContext(aContext)
{
  MOZ_ASSERT(aContext);

  // Caller should have checked this and thrown.
  MOZ_ASSERT(aLength > 0);
  MOZ_ASSERT(aLength <= 4096);
  mLength = aLength;

  // Copy coefficient data. The two arrays share an allocation.
  mCoefficients = new ThreadSharedFloatArrayBufferList(2);
  float* buffer = static_cast<float*>(malloc(aLength*sizeof(float)*2));
  if (buffer == nullptr) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  PodCopy(buffer, aRealData, aLength);
  mCoefficients->SetData(0, buffer, free, buffer);
  PodCopy(buffer+aLength, aImagData, aLength);
  mCoefficients->SetData(1, nullptr, free, buffer+aLength);
}

size_t
PeriodicWave::SizeOfExcludingThisIfNotShared(MallocSizeOf aMallocSizeOf) const
{
  // Not owned:
  // - mContext
  size_t amount = 0;
  if (!mCoefficients->IsShared()) {
    amount += mCoefficients->SizeOfIncludingThis(aMallocSizeOf);
  }

  return amount;
}

size_t
PeriodicWave::SizeOfIncludingThisIfNotShared(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThisIfNotShared(aMallocSizeOf);
}

JSObject*
PeriodicWave::WrapObject(JSContext* aCx)
{
  return PeriodicWaveBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla

