/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PeriodicWave_h_
#define PeriodicWave_h_

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "AudioContext.h"
#include "AudioNodeEngine.h"

namespace mozilla::dom {

class AudioContext;
struct PeriodicWaveOptions;

class PeriodicWave final : public nsWrapperCache {
 public:
  PeriodicWave(AudioContext* aContext, const float* aRealData,
               const uint32_t aRealSize, const float* aImagData,
               const uint32_t aImagSize, const bool aDisableNormalization,
               ErrorResult& aRv);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PeriodicWave)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(PeriodicWave)

  static already_AddRefed<PeriodicWave> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const PeriodicWaveOptions& aOptions, ErrorResult& aRv);

  AudioContext* GetParentObject() const { return mContext; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  uint32_t DataLength() const { return mCoefficients.mDuration; }

  bool DisableNormalization() const { return mDisableNormalization; }

  const AudioChunk& GetThreadSharedBuffer() const { return mCoefficients; }

  size_t SizeOfExcludingThisIfNotShared(MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThisIfNotShared(MallocSizeOf aMallocSizeOf) const;

 private:
  ~PeriodicWave() = default;

  AudioChunk mCoefficients;
  RefPtr<AudioContext> mContext;
  bool mDisableNormalization;
};

}  // namespace mozilla::dom

#endif
