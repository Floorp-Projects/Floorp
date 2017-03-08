/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AnimationHelper_h
#define mozilla_layers_AnimationHelper_h

#include "mozilla/ComputedTimingFunction.h" // for ComputedTimingFunction
#include "mozilla/layers/LayersMessages.h" // for TransformData, etc
#include "mozilla/TimeStamp.h"          // for TimeStamp


namespace mozilla {
class StyleAnimationValue;
namespace layers {
class Animation;

typedef InfallibleTArray<layers::Animation> AnimationArray;

struct AnimData {
  InfallibleTArray<mozilla::StyleAnimationValue> mStartValues;
  InfallibleTArray<mozilla::StyleAnimationValue> mEndValues;
  InfallibleTArray<Maybe<mozilla::ComputedTimingFunction>> mFunctions;
};

struct AnimationTransform {
  /*
   * This transform is calculated from sampleanimation in device pixel
   * and used by compositor.
   */
  gfx::Matrix4x4 mTransformInDevSpace;
  /*
   * This transform is calculated from frame and used by getOMTAStyle()
   * for OMTA testing.
   */
  gfx::Matrix4x4 mFrameTransform;
  TransformData mData;
};

struct AnimatedValue {
  enum {
    TRANSFORM,
    OPACITY,
    NONE
  } mType {NONE};

  union {
    AnimationTransform mTransform;
    float mOpacity;
  };

  AnimatedValue(gfx::Matrix4x4&& aTransformInDevSpace,
                gfx::Matrix4x4&& aFrameTransform,
                const TransformData& aData)
    : mType(AnimatedValue::TRANSFORM)
  {
    mTransform.mTransformInDevSpace = Move(aTransformInDevSpace);
    mTransform.mFrameTransform = Move(aFrameTransform);
    mTransform.mData = aData;
  }

  explicit AnimatedValue(const float& aValue)
    : mType(AnimatedValue::OPACITY)
    , mOpacity(aValue)
  {
  }

  ~AnimatedValue() {}

private:
  AnimatedValue() = delete;
};

// CompositorAnimationStorage stores the layer animations and animated value
// after sampling based on an unique id (CompositorAnimationsId)
class CompositorAnimationStorage final
{
  typedef nsClassHashtable<nsUint64HashKey, AnimatedValue> AnimatedValueTable;
  typedef nsClassHashtable<nsUint64HashKey, AnimationArray> AnimationsTable;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorAnimationStorage)
public:

  /**
   * Set the animation transform based on the unique id
   */
  void SetAnimatedValue(uint64_t aId,
                        gfx::Matrix4x4&& aTransformInDevSpace,
                        gfx::Matrix4x4&& aFrameTransform,
                        const TransformData& aData);

  /**
   * Set the animation opacity based on the unique id
   */
  void SetAnimatedValue(uint64_t aId, const float& aOpacity);

  /**
   * Return the animated value if a given id can map to its animated value
   */
  AnimatedValue* GetAnimatedValue(const uint64_t& aId) const;

  /**
   * Set the animations based on the unique id
   */
  void SetAnimations(uint64_t aId, const AnimationArray& aAnimations);

  /**
   * Return the animations if a given id can map to its animations
   */
  AnimationArray* GetAnimations(const uint64_t& aId) const;

  /**
   * Clear AnimatedValues and Animations data
   */
  void Clear();

private:
  ~CompositorAnimationStorage() { Clear(); };

private:
  AnimatedValueTable mAnimatedValues;
  AnimationsTable mAnimations;
};

class AnimationHelper
{
public:
  static bool
  SampleAnimationForEachNode(TimeStamp aPoint,
                             AnimationArray& aAnimations,
                             InfallibleTArray<AnimData>& aAnimationData,
                             StyleAnimationValue& aAnimationValue,
                             bool& aHasInEffectAnimations);

  static void
  SetAnimations(AnimationArray& aAnimations,
                InfallibleTArray<AnimData>& aAnimData,
                StyleAnimationValue& aBaseAnimationStyle);
  static uint64_t GetNextCompositorAnimationsId();
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_AnimationHelper_h
