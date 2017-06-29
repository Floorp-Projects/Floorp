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
   * Set the animation transform based on the unique id and also
   * set up |aFrameTransform| and |aData| for OMTA testing
   */
  void SetAnimatedValue(uint64_t aId,
                        gfx::Matrix4x4&& aTransformInDevSpace,
                        gfx::Matrix4x4&& aFrameTransform,
                        const TransformData& aData);

  /**
   * Set the animation transform in device pixel based on the unique id
   */
  void SetAnimatedValue(uint64_t aId,
                        gfx::Matrix4x4&& aTransformInDevSpace);

  /**
   * Set the animation opacity based on the unique id
   */
  void SetAnimatedValue(uint64_t aId, const float& aOpacity);

  /**
   * Return the animated value if a given id can map to its animated value
   */
  AnimatedValue* GetAnimatedValue(const uint64_t& aId) const;

  /**
   * Like GetAnimatedValue(), but ensures the value is an opacity and returns
   * the float value if possible, or Nothing() otherwise.
   */
  Maybe<float> GetAnimationOpacity(const uint64_t& aId) const;

  /**
   * Like GetAnimatedValue(), but ensures the value is a transform and returns
   * the transform matrix if possible, or Nothing() otherwise. It also does
   * some post-processing on the transform matrix as well. See the comments
   * inside the function for details.
   */
  Maybe<gfx::Matrix4x4> GetAnimationTransform(const uint64_t& aId) const;

  /**
   * Return the iterator of animated value table
   */
  AnimatedValueTable::Iterator ConstAnimatedValueTableIter() const
  {
    return mAnimatedValues.ConstIter();
  }

  uint32_t AnimatedValueCount() const
  {
    return mAnimatedValues.Count();
  }

  /**
   * Set the animations based on the unique id
   */
  void SetAnimations(uint64_t aId, const AnimationArray& aAnimations);

  /**
   * Return the animations if a given id can map to its animations
   */
  AnimationArray* GetAnimations(const uint64_t& aId) const;

  /**
   * Return the iterator of animations table
   */
  AnimationsTable::Iterator ConstAnimationsTableIter() const
  {
    return mAnimations.ConstIter();
  }

  uint32_t AnimationsCount() const
  {
    return mAnimations.Count();
  }

  /**
   * Clear AnimatedValues and Animations data
   */
  void Clear();

  void ClearById(const uint64_t& aId);
private:
  ~CompositorAnimationStorage() { Clear(); };

private:
  AnimatedValueTable mAnimatedValues;
  AnimationsTable mAnimations;
};

class AnimationHelper
{
public:


  /*
   * TODO Bug 1356509 Once we decouple the compositor animations and layers
   * in parent side, the API will be called inside SampleAnimations.
   * Before this, we expose this API for AsyncCompositionManager.
   *
   * Sample animations based on a given time stamp for a element(layer) with
   * its animation data.
   * Returns true if there exists compositor animation, and stores corresponding
   * animated value in |aAnimationValue|.
   */
  static bool
  SampleAnimationForEachNode(TimeStamp aTime,
                             AnimationArray& aAnimations,
                             InfallibleTArray<AnimData>& aAnimationData,
                             StyleAnimationValue& aAnimationValue,
                             bool& aHasInEffectAnimations);
  /*
   * TODO Bug 1356509 Once we decouple the compositor animations and layers
   * in parent side, the API will be called inside SampleAnimations.
   * Before this, we expose this API for AsyncCompositionManager.
   *
   * Populates AnimData stuctures into |aAnimData| based on |aAnimations|
   */
  static void
  SetAnimations(AnimationArray& aAnimations,
                InfallibleTArray<AnimData>& aAnimData,
                StyleAnimationValue& aBaseAnimationStyle);

  /*
   * Get a unique id to represent the compositor animation between child
   * and parent side. This id will be used as a key to store animation
   * data in the CompositorAnimationStorage per compositor.
   * Each layer on the content side calls this when it gets new animation
   * data.
   */
  static uint64_t GetNextCompositorAnimationsId();

  /*
   * Sample animation based a given time stamp |aTime| and the animation
   * data inside CompositorAnimationStorage |aStorage|. The animated values
   * after sampling will be stored in CompositorAnimationStorage as well.
   */
  static void
  SampleAnimations(CompositorAnimationStorage* aStorage,
                   TimeStamp aTime);
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_AnimationHelper_h
