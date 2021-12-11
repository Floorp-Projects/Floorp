/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ANIMATIONINFO_H
#define GFX_ANIMATIONINFO_H

#include "nsCSSPropertyIDSet.h"
#include "nsDisplayItemTypes.h"
#include "mozilla/Array.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/FunctionRef.h"
#include "mozilla/layers/AnimationStorageData.h"
#include "mozilla/layers/LayersMessages.h"  // for TransformData

struct RawServoAnimationValue;
class nsIContent;
class nsIFrame;

namespace mozilla {

class nsDisplayItem;
class nsDisplayListBuilder;
class EffectSet;
struct AnimationProperty;

namespace dom {
class Animation;
}  // namespace dom

namespace gfx {
class Path;
}  // namespace gfx

namespace layers {

class Animation;
class CompositorAnimations;
class Layer;
class WebRenderLayerManager;
struct CompositorAnimationData;
struct PropertyAnimationGroup;

class AnimationInfo final {
  typedef nsTArray<Animation> AnimationArray;

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
      const LayersId& aLayersId,
      const CompositorAnimations& aCompositorAnimations);
  bool StartPendingAnimations(const TimeStamp& aReadyTime);
  void TransferMutatedFlagToLayer(Layer* aLayer);

  uint64_t GetCompositorAnimationsId() { return mCompositorAnimationsId; }
  // Note: We don't set mAnimations on the compositor thread, so this will
  // always return an empty array on the compositor thread.
  AnimationArray& GetAnimations() { return mAnimations; }
  nsTArray<PropertyAnimationGroup>& GetPropertyAnimationGroups() {
    return mStorageData.mAnimation;
  }
  const Maybe<TransformData>& GetTransformData() const {
    return mStorageData.mTransformData;
  }
  const LayersId& GetLayersId() const { return mStorageData.mLayersId; }
  bool ApplyPendingUpdatesForThisTransaction();
  bool HasTransformAnimation() const;

  gfx::Path* CachedMotionPath() { return mStorageData.mCachedMotionPath; }

  // In case of continuation, |aFrame| must be the first or the last
  // continuation frame, otherwise this function might return Nothing().
  static Maybe<uint64_t> GetGenerationFromFrame(
      nsIFrame* aFrame, DisplayItemType aDisplayItemKey);

  using CompositorAnimatableDisplayItemTypes =
      Array<DisplayItemType,
            nsCSSPropertyIDSet::CompositorAnimatableDisplayItemCount()>;
  using AnimationGenerationCallback = FunctionRef<bool(
      const Maybe<uint64_t>& aGeneration, DisplayItemType aDisplayItemType)>;
  // Enumerates animation generations on |aFrame| for the given display item
  // types and calls |aCallback| with the animation generation.
  //
  // The enumeration stops if |aCallback| returns false.
  static void EnumerateGenerationOnFrame(
      const nsIFrame* aFrame, const nsIContent* aContent,
      const CompositorAnimatableDisplayItemTypes& aDisplayItemTypes,
      AnimationGenerationCallback);

  void AddAnimationsForDisplayItem(
      nsIFrame* aFrame, nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem,
      DisplayItemType aType, WebRenderLayerManager* aLayerManager,
      const Maybe<LayoutDevicePoint>& aPosition = Nothing());

 private:
  enum class Send {
    NextTransaction,
    Immediate,
  };
  void AddAnimationForProperty(nsIFrame* aFrame,
                               const AnimationProperty& aProperty,
                               dom::Animation* aAnimation,
                               const Maybe<TransformData>& aTransformData,
                               Send aSendFlag);

  bool AddAnimationsForProperty(
      nsIFrame* aFrame, const EffectSet* aEffects,
      const nsTArray<RefPtr<dom::Animation>>& aCompositorAnimations,
      const Maybe<TransformData>& aTransformData, nsCSSPropertyID aProperty,
      Send aSendFlag, WebRenderLayerManager* aLayerManager);

  void AddNonAnimatingTransformLikePropertiesStyles(
      const nsCSSPropertyIDSet& aNonAnimatingProperties, nsIFrame* aFrame,
      Send aSendFlag);

 protected:
  // mAnimations (and mPendingAnimations) are only set on the main thread.
  //
  // Once the animations are received on the compositor thread/process we
  // use AnimationHelper::ExtractAnimations to transform the rather compact
  // representation of animation data we transfer into something we can more
  // readily use for sampling and then store it in mPropertyAnimationGroups
  // (below) or CompositorAnimationStorage.mAnimations for WebRender.
  AnimationArray mAnimations;
  UniquePtr<AnimationArray> mPendingAnimations;

  uint64_t mCompositorAnimationsId;
  // The extracted data produced by AnimationHelper::ExtractAnimations().
  AnimationStorageData mStorageData;
  // If this layer is used for OMTA, then this counter is used to ensure we
  // stay in sync with the animation manager
  Maybe<uint64_t> mAnimationGeneration;
  bool mMutated;
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_ANIMATIONINFO_H
