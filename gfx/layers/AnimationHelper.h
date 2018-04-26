/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AnimationHelper_h
#define mozilla_layers_AnimationHelper_h

#include "mozilla/dom/Nullable.h"
#include "mozilla/ComputedTimingFunction.h" // for ComputedTimingFunction
#include "mozilla/layers/LayersMessages.h" // for TransformData, etc
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/TimingParams.h"
#include "X11UndefineNone.h"

namespace mozilla {
struct AnimationValue;
namespace layers {
class Animation;

typedef InfallibleTArray<layers::Animation> AnimationArray;

struct AnimData {
  InfallibleTArray<RefPtr<RawServoAnimationValue>> mStartValues;
  InfallibleTArray<RefPtr<RawServoAnimationValue>> mEndValues;
  InfallibleTArray<Maybe<mozilla::ComputedTimingFunction>> mFunctions;
  TimingParams mTiming;
  // These two variables correspond to the variables of the same name in
  // KeyframeEffectReadOnly and are used for the same purpose: to skip composing
  // animations whose progress has not changed.
  dom::Nullable<double> mProgressOnLastCompose;
  uint64_t mCurrentIterationOnLastCompose = 0;
  // These two variables are used for a similar optimization above but are
  // applied to the timing function in each keyframe.
  uint32_t mSegmentIndexOnLastCompose = 0;
  dom::Nullable<double> mPortionInSegmentOnLastCompose;
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

// CompositorAnimationStorage stores the animations and animated values
// keyed by a CompositorAnimationsId. The "animations" are a representation of
// an entire animation over time, while the "animated values" are values sampled
// from the animations at a particular point in time.
//
// There is one CompositorAnimationStorage per CompositorBridgeParent (i.e.
// one per browser window), and the CompositorAnimationsId key is unique within
// a particular CompositorAnimationStorage instance.
//
// Each layer which has animations gets a CompositorAnimationsId key, and reuses
// that key during its lifetime. Likewise, in layers-free webrender, a display
// item that is animated (e.g. nsDisplayTransform) gets a CompositorAnimationsId
// key and reuses that key (it persists the key via the frame user-data
// mechanism).
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
  ~CompositorAnimationStorage() { };

private:
  AnimatedValueTable mAnimatedValues;
  AnimationsTable mAnimations;
};

/**
 * This utility class allows reusing code between the webrender and
 * non-webrender compositor-side implementations. It provides
 * utility functions for sampling animations at particular timestamps.
 */
class AnimationHelper
{
public:

  enum class SampleResult {
    None,
    Skipped,
    Sampled
  };

  /**
   * Sample animations based on a given time stamp for a element(layer) with
   * its animation data.
   *
   * Returns SampleResult::None if none of the animations are producing a result
   * (e.g. they are in the delay phase with no backwards fill),
   * SampleResult::Skipped if the animation output did not change since the last
   * call of this function,
   * SampleResult::Sampled if the animation output was updated.
   */
  static SampleResult
  SampleAnimationForEachNode(TimeStamp aTime,
                             AnimationArray& aAnimations,
                             InfallibleTArray<AnimData>& aAnimationData,
                             RefPtr<RawServoAnimationValue>& aAnimationValue);
  /**
   * Populates AnimData stuctures into |aAnimData| and |aBaseAnimationStyle|
   * based on |aAnimations|.
   */
  static void
  SetAnimations(AnimationArray& aAnimations,
                InfallibleTArray<AnimData>& aAnimData,
                RefPtr<RawServoAnimationValue>& aBaseAnimationStyle);

  /**
   * Get a unique id to represent the compositor animation between child
   * and parent side. This id will be used as a key to store animation
   * data in the CompositorAnimationStorage per compositor.
   * Each layer on the content side calls this when it gets new animation
   * data.
   */
  static uint64_t GetNextCompositorAnimationsId();

  /**
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
