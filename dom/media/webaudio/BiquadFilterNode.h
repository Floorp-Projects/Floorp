/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BiquadFilterNode_h_
#define BiquadFilterNode_h_

#include "AudioNode.h"
#include "AudioParam.h"
#include "mozilla/dom/BiquadFilterNodeBinding.h"

namespace mozilla {
namespace dom {

class AudioContext;
struct BiquadFilterOptions;

class BiquadFilterNode final : public AudioNode {
 public:
  static already_AddRefed<BiquadFilterNode> Create(
      AudioContext& aAudioContext, const BiquadFilterOptions& aOptions,
      ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BiquadFilterNode, AudioNode)

  static already_AddRefed<BiquadFilterNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const BiquadFilterOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  BiquadFilterType Type() const { return mType; }
  void SetType(BiquadFilterType aType);

  AudioParam* Frequency() const { return mFrequency; }

  AudioParam* Detune() const { return mDetune; }

  AudioParam* Q() const { return mQ; }

  AudioParam* Gain() const { return mGain; }

  void GetFrequencyResponse(const Float32Array& aFrequencyHz,
                            const Float32Array& aMagResponse,
                            const Float32Array& aPhaseResponse,
                            ErrorResult& aRv);

  const char* NodeType() const override { return "BiquadFilterNode"; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

 private:
  explicit BiquadFilterNode(AudioContext* aContext);
  ~BiquadFilterNode() = default;

  BiquadFilterType mType;
  RefPtr<AudioParam> mFrequency;
  RefPtr<AudioParam> mDetune;
  RefPtr<AudioParam> mQ;
  RefPtr<AudioParam> mGain;
};

}  // namespace dom
}  // namespace mozilla

#endif
