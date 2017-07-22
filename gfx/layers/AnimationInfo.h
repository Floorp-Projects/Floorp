/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ANIMATIONINFO_H
#define GFX_ANIMATIONINFO_H

#include "mozilla/StyleAnimationValue.h"

namespace mozilla {
namespace layers {

class Animation;
class CompositorAnimations;
class Layer;
class LayerManager;
struct AnimData;

class AnimationInfo
{
  typedef InfallibleTArray<Animation> AnimationArray;
public:
  explicit AnimationInfo(LayerManager* aManager);
  virtual ~AnimationInfo();

  // Ensure that this AnimationInfo has a valid (non-zero) animations id. This value is
  // unique across layers.
  void EnsureAnimationsId();

  // Call AddAnimation to add a new animation to this layer from layout code.
  // Caller must fill in all the properties of the returned animation.
  // A later animation overrides an earlier one.
  Animation* AddAnimation();

  // These are a parallel to AddAnimation and clearAnimations, except
  // they add pending animations that apply only when the next
  // transaction is begun.  (See also
  // SetBaseTransformForNextTransaction.)
  Animation* AddAnimationForNextTransaction();

  void SetAnimationGeneration(uint64_t aCount) { mAnimationGeneration = aCount; }
  uint64_t GetAnimationGeneration() { return mAnimationGeneration; }

  // ClearAnimations clears animations on this layer.
  void ClearAnimations();
  void ClearAnimationsForNextTransaction();
  void SetCompositorAnimations(const CompositorAnimations& aCompositorAnimations);
  bool StartPendingAnimations(const TimeStamp& aReadyTime);
  void TransferMutatedFlagToLayer(Layer* aLayer);

  uint64_t GetCompositorAnimationsId() { return mCompositorAnimationsId; }
  StyleAnimationValue GetBaseAnimationStyle() const { return mBaseAnimationStyle; }
  InfallibleTArray<AnimData>& GetAnimationData() { return mAnimationData; }
  AnimationArray& GetAnimations() { return mAnimations; }
  bool ApplyPendingUpdatesForThisTransaction();
  bool HasOpacityAnimation() const;
  bool HasTransformAnimation() const;

protected:
  LayerManager* mManager;
  AnimationArray mAnimations;
  uint64_t mCompositorAnimationsId;
  nsAutoPtr<AnimationArray> mPendingAnimations;
  InfallibleTArray<AnimData> mAnimationData;
  // If this layer is used for OMTA, then this counter is used to ensure we
  // stay in sync with the animation manager
  uint64_t mAnimationGeneration;
  StyleAnimationValue mBaseAnimationStyle;
  bool mMutated;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_ANIMATIONINFO_H
