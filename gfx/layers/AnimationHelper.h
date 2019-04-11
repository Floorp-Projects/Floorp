/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AnimationHelper_h
#define mozilla_layers_AnimationHelper_h

#include "mozilla/dom/Nullable.h"
#include "mozilla/ComputedTimingFunction.h"    // for ComputedTimingFunction
#include "mozilla/layers/LayersMessages.h"     // for TransformData, etc
#include "mozilla/webrender/WebRenderTypes.h"  // for RenderRoot
#include "mozilla/TimeStamp.h"                 // for TimeStamp
#include "mozilla/TimingParams.h"
#include "mozilla/Variant.h"
#include "X11UndefineNone.h"

namespace mozilla {
struct AnimationValue;

namespace dom {
enum class CompositeOperation : uint8_t;
enum class IterationCompositeOperation : uint8_t;
};  // namespace dom

namespace layers {
class Animation;

typedef nsTArray<layers::Animation> AnimationArray;

struct PropertyAnimation {
  struct SegmentData {
    RefPtr<RawServoAnimationValue> mStartValue;
    RefPtr<RawServoAnimationValue> mEndValue;
    Maybe<mozilla::ComputedTimingFunction> mFunction;
    float mStartPortion;
    float mEndPortion;
    dom::CompositeOperation mStartComposite;
    dom::CompositeOperation mEndComposite;
  };
  nsTArray<SegmentData> mSegments;
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

  TimeStamp mOriginTime;
  Maybe<TimeDuration> mStartTime;
  TimeDuration mHoldTime;
  float mPlaybackRate;
  dom::IterationCompositeOperation mIterationComposite;
  bool mIsNotPlaying;
};

struct PropertyAnimationGroup {
  nsCSSPropertyID mProperty;
  Maybe<TransformData> mAnimationData;

  nsTArray<PropertyAnimation> mAnimations;
  RefPtr<RawServoAnimationValue> mBaseStyle;

  bool IsEmpty() const { return mAnimations.IsEmpty(); }
  void Clear() {
    mAnimations.Clear();
    mBaseStyle = nullptr;
  }
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

struct AnimatedValue final {
  typedef Variant<AnimationTransform, float, nscolor> AnimatedValueType;

  const AnimatedValueType& Value() const { return mValue; }
  const AnimationTransform& Transform() const {
    return mValue.as<AnimationTransform>();
  }
  const float& Opacity() const { return mValue.as<float>(); }
  const nscolor& Color() const { return mValue.as<nscolor>(); }
  template <typename T>
  bool Is() const {
    return mValue.is<T>();
  }

  AnimatedValue(gfx::Matrix4x4&& aTransformInDevSpace,
                gfx::Matrix4x4&& aFrameTransform, const TransformData& aData)
      : mValue(
            AsVariant(AnimationTransform{std::move(aTransformInDevSpace),
                                         std::move(aFrameTransform), aData})) {}

  explicit AnimatedValue(const float& aValue) : mValue(AsVariant(aValue)) {}

  explicit AnimatedValue(nscolor aValue) : mValue(AsVariant(aValue)) {}

 private:
  AnimatedValueType mValue;
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
class CompositorAnimationStorage final {
  typedef nsClassHashtable<nsUint64HashKey, AnimatedValue> AnimatedValueTable;
  typedef nsClassHashtable<nsUint64HashKey, nsTArray<PropertyAnimationGroup>>
      AnimationsTable;
  typedef nsDataHashtable<nsUint64HashKey, wr::RenderRoot>
      AnimationsRenderRootsTable;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorAnimationStorage)
 public:
  /**
   * Set the animation transform based on the unique id and also
   * set up |aFrameTransform| and |aData| for OMTA testing
   */
  void SetAnimatedValue(uint64_t aId, gfx::Matrix4x4&& aTransformInDevSpace,
                        gfx::Matrix4x4&& aFrameTransform,
                        const TransformData& aData);

  /**
   * Set the animation transform in device pixel based on the unique id
   */
  void SetAnimatedValue(uint64_t aId, gfx::Matrix4x4&& aTransformInDevSpace);

  /**
   * Set the animation opacity based on the unique id
   */
  void SetAnimatedValue(uint64_t aId, const float& aOpacity);

  /**
   * Set the animation color based on the unique id
   */
  void SetAnimatedValue(uint64_t aId, nscolor aColor);

  /**
   * Return the animated value if a given id can map to its animated value
   */
  AnimatedValue* GetAnimatedValue(const uint64_t& aId) const;

  OMTAValue GetOMTAValue(const uint64_t& aId) const;

  /**
   * Return the iterator of animated value table
   */
  AnimatedValueTable::Iterator ConstAnimatedValueTableIter() const {
    return mAnimatedValues.ConstIter();
  }

  uint32_t AnimatedValueCount() const { return mAnimatedValues.Count(); }

  /**
   * Set the animations based on the unique id
   */
  void SetAnimations(uint64_t aId, const AnimationArray& aAnimations,
                     wr::RenderRoot aRenderRoot);

  /**
   * Return the animations if a given id can map to its animations
   */
  nsTArray<PropertyAnimationGroup>* GetAnimations(const uint64_t& aId) const;

  /**
   * Return the iterator of animations table
   */
  AnimationsTable::Iterator ConstAnimationsTableIter() const {
    return mAnimations.ConstIter();
  }

  uint32_t AnimationsCount() const { return mAnimations.Count(); }

  wr::RenderRoot AnimationRenderRoot(const uint64_t& aId) const {
    return mAnimationRenderRoots.Get(aId);
  }

  /**
   * Clear AnimatedValues and Animations data
   */
  void Clear();
  void ClearById(const uint64_t& aId);

 private:
  ~CompositorAnimationStorage(){};

 private:
  AnimatedValueTable mAnimatedValues;
  AnimationsTable mAnimations;
  AnimationsRenderRootsTable mAnimationRenderRoots;
};

/**
 * This utility class allows reusing code between the webrender and
 * non-webrender compositor-side implementations. It provides
 * utility functions for sampling animations at particular timestamps.
 */
class AnimationHelper {
 public:
  enum class SampleResult { None, Skipped, Sampled };

  /**
   * Sample animations based on a given time stamp for a element(layer) with
   * its animation data.
   * Generally |aPreviousFrameTime| is used for the sampling if it's
   * supplied to make the animation more in sync with other animations on the
   * main-thread.  But in the case where the animation just started at the time
   * when the animation was sent to the compositor, |aCurrentFrameTime| is used
   * for sampling instead to avoid flicker.
   *
   * Returns SampleResult::None if none of the animations are producing a result
   * (e.g. they are in the delay phase with no backwards fill),
   * SampleResult::Skipped if the animation output did not change since the last
   * call of this function,
   * SampleResult::Sampled if the animation output was updated.
   *
   * Using the same example from ExtractAnimations (below):
   *
   * Input |aPropertyAnimationGroups| (ignoring the base animation style):
   *
   * [
   *   Group A: [ { rotate, Animation A }, { rotate, Animation B } ],
   *   Group B: [ { scale, Animation B } ],
   *   Group C: [ { transform, Animation A }, { transform, Animation B } ],
   * ]
   *
   * For each property group, this function interpolates each animation in turn,
   * using the result of interpolating one animation as input for the next such
   * that it reduces each property group to a single output value:
   *
   * [
   *   { rotate, RawServoAnimationValue },
   *   { scale, RawServoAnimationValue },
   *   { transform, RawServoAnimationValue },
   * ]
   *
   * For transform animations, the caller (SampleAnimations) will combine the
   * result of the various transform properties into a final matrix.
   */
  static SampleResult SampleAnimationForEachNode(
      TimeStamp aPreviousFrameTime, TimeStamp aCurrentFrameTime,
      const AnimatedValue* aPreviousValue,
      nsTArray<PropertyAnimationGroup>& aPropertyAnimationGroups,
      nsTArray<RefPtr<RawServoAnimationValue>>& aAnimationValues);

  /**
   * Extract organized animation data by property into an array of
   * PropertyAnimationGroup objects.
   *
   * For example, suppose we have the following animations:
   *
   *   Animation A: [ transform, rotate ]
   *   Animation B: [ rotate, scale ]
   *   Animation C: [ transform ]
   *   Animation D: [ opacity ]
   *
   * When we go to send transform-like properties to the compositor, we
   * sort them as follows:
   *
   *   [
   *     { rotate: Animation A (rotate segments only) },
   *     { rotate: Animation B ( " " ) },
   *     { scale: Animation B (scale segments only) },
   *     { transform: Animation A (transform segments only) },
   *     { transform: Animation C ( " " ) },
   *   ]
   *
   * In this function, we group these animations together by property producing
   * output such as the following:
   *
   *   [
   *     [ { rotate, Animation A }, { rotate, Animation B } ],
   *     [ { scale, Animation B } ],
   *     [ { transform, Animation A }, { transform, Animation B } ],
   *   ]
   *
   * In the process of grouping these animations, we also convert their values
   * from the rather compact representation we use for transferring across the
   * IPC boundary into something we can readily use for sampling.
   */
  static nsTArray<PropertyAnimationGroup> ExtractAnimations(
      const AnimationArray& aAnimations);

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
   *
   * Returns true if there is any animation.
   * Note that even if there are only in-delay phase animations (i.e. not
   * visually effective), this function returns true to ensure we composite
   * again on the next tick.
   *
   * Note: This is called only by WebRender.
   */
  static bool SampleAnimations(CompositorAnimationStorage* aStorage,
                               TimeStamp aPreviousFrameTime,
                               TimeStamp aCurrentFrameTime);

  /**
   * Convert an array of animation values into a matrix given the corresponding
   * transform parameters. |aValue| must be a transform-like value
   * (e.g. transform, translate etc.).
   */
  static gfx::Matrix4x4 ServoAnimationValueToMatrix4x4(
      const nsTArray<RefPtr<RawServoAnimationValue>>& aValue,
      const TransformData& aTransformData);
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_AnimationHelper_h
