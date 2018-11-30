/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ANIMATIONINFO_H
#define GFX_ANIMATIONINFO_H

#include "nsAutoPtr.h"
#include "nsCSSPropertyIDSet.h"
#include "nsDisplayItemTypes.h"
#include "mozilla/Array.h"

struct RawServoAnimationValue;
class nsIContent;
class nsIFrame;

namespace mozilla {
namespace layers {

class Animation;
class CompositorAnimations;
class Layer;
class LayerManager;
struct AnimData;

class AnimationInfo final {
  typedef InfallibleTArray<Animation> AnimationArray;

 public:
  AnimationInfo();
  ~AnimationInfo();

  // Ensure that this AnimationInfo has a valid (non-zero) animations id. This
  // value is unique across layers.
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

  void SetAnimationGeneration(uint64_t aCount) {
    mAnimationGeneration = Some(aCount);
  }
  Maybe<uint64_t> GetAnimationGeneration() const {
    return mAnimationGeneration;
  }

  // ClearAnimations clears animations on this layer.
  void ClearAnimations();
  void ClearAnimationsForNextTransaction();
  void SetCompositorAnimations(
      const CompositorAnimations& aCompositorAnimations);
  bool StartPendingAnimations(const TimeStamp& aReadyTime);
  void TransferMutatedFlagToLayer(Layer* aLayer);

  uint64_t GetCompositorAnimationsId() { return mCompositorAnimationsId; }
  RawServoAnimationValue* GetBaseAnimationStyle() const {
    return mBaseAnimationStyle;
  }
  InfallibleTArray<AnimData>& GetAnimationData() { return mAnimationData; }
  AnimationArray& GetAnimations() { return mAnimations; }
  bool ApplyPendingUpdatesForThisTransaction();
  bool HasTransformAnimation() const;

  // In case of continuation, |aFrame| must be the first or the last
  // continuation frame, otherwise this function might return Nothing().
  static Maybe<uint64_t> GetGenerationFromFrame(
      nsIFrame* aFrame, DisplayItemType aDisplayItemKey);

  using CompositorAnimatableDisplayItemTypes =
      Array<DisplayItemType, nsCSSPropertyIDSet::CompositorAnimatableCount()>;
  using AnimationGenerationCallback = std::function<bool(
      const Maybe<uint64_t>& aGeneration, DisplayItemType aDisplayItemType)>;
  // Enumerates animation generations on |aFrame| for the given display item
  // types and calls |aCallback| with the animation generation.
  //
  // The enumeration stops if |aCallback| returns false.
  static void EnumerateGenerationOnFrame(
      const nsIFrame* aFrame, const nsIContent* aContent,
      const CompositorAnimatableDisplayItemTypes& aDisplayItemTypes,
      const AnimationGenerationCallback& aCallback);

 protected:
  AnimationArray mAnimations;
  uint64_t mCompositorAnimationsId;
  nsAutoPtr<AnimationArray> mPendingAnimations;
  InfallibleTArray<AnimData> mAnimationData;
  // If this layer is used for OMTA, then this counter is used to ensure we
  // stay in sync with the animation manager
  Maybe<uint64_t> mAnimationGeneration;
  RefPtr<RawServoAnimationValue> mBaseAnimationStyle;
  bool mMutated;
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_ANIMATIONINFO_H
