/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DynamicsCompressorNode_h_
#define DynamicsCompressorNode_h_

#include "AudioNode.h"
#include "AudioParam.h"

namespace mozilla {
namespace dom {

class AudioContext;
struct DynamicsCompressorOptions;

class DynamicsCompressorNode final : public AudioNode {
 public:
  static already_AddRefed<DynamicsCompressorNode> Create(
      AudioContext& aAudioContext, const DynamicsCompressorOptions& aOptions,
      ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DynamicsCompressorNode, AudioNode)

  static already_AddRefed<DynamicsCompressorNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const DynamicsCompressorOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  AudioParam* Threshold() const { return mThreshold; }

  AudioParam* Knee() const { return mKnee; }

  AudioParam* Ratio() const { return mRatio; }

  AudioParam* Attack() const { return mAttack; }

  // Called GetRelease to prevent clashing with the nsISupports::Release name
  AudioParam* GetRelease() const { return mRelease; }

  float Reduction() const { return mReduction; }

  const char* NodeType() const override { return "DynamicsCompressorNode"; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  void SetReduction(float aReduction) {
    MOZ_ASSERT(NS_IsMainThread());
    mReduction = aReduction;
  }

  void SetChannelCountModeValue(ChannelCountMode aMode,
                                ErrorResult& aRv) override {
    if (aMode == ChannelCountMode::Max) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCountModeValue(aMode, aRv);
  }

  void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) override {
    if (aChannelCount > 2) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCount(aChannelCount, aRv);
  }

 private:
  explicit DynamicsCompressorNode(AudioContext* aContext);
  ~DynamicsCompressorNode() = default;

  RefPtr<AudioParam> mThreshold;
  RefPtr<AudioParam> mKnee;
  RefPtr<AudioParam> mRatio;
  float mReduction;
  RefPtr<AudioParam> mAttack;
  RefPtr<AudioParam> mRelease;
};

}  // namespace dom
}  // namespace mozilla

#endif
