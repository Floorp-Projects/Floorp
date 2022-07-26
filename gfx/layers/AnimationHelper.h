/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AnimationHelper_h
#define mozilla_layers_AnimationHelper_h

#include "mozilla/dom/Nullable.h"
#include "mozilla/layers/AnimationStorageData.h"
#include "mozilla/layers/LayersMessages.h"     // for TransformData, etc
#include "mozilla/webrender/WebRenderTypes.h"  // for RenderRoot
#include "mozilla/TimeStamp.h"                 // for TimeStamp
#include "mozilla/TimingParams.h"
#include "mozilla/Types.h"  // for SideBits
#include "X11UndefineNone.h"
#include <unordered_map>

namespace mozilla::layers {
class Animation;
class APZSampler;
class CompositorAnimationStorage;
struct AnimatedValue;

typedef nsTArray<layers::Animation> AnimationArray;

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
      const APZSampler* aAPZSampler, const LayersId& aLayersId,
      const MutexAutoLock& aProofOfMapLock, TimeStamp aPreviousFrameTime,
      TimeStamp aCurrentFrameTime, const AnimatedValue* aPreviousValue,
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
  static AnimationStorageData ExtractAnimations(
      const LayersId& aLayersId, const AnimationArray& aAnimations);

  /**
   * Get a unique id to represent the compositor animation between child
   * and parent side. This id will be used as a key to store animation
   * data in the CompositorAnimationStorage per compositor.
   * Each layer on the content side calls this when it gets new animation
   * data.
   */
  static uint64_t GetNextCompositorAnimationsId();

  /**
   * Convert an array of animation values into a matrix given the corresponding
   * transform parameters. |aValue| must be a transform-like value
   * (e.g. transform, translate etc.).
   */
  static gfx::Matrix4x4 ServoAnimationValueToMatrix4x4(
      const nsTArray<RefPtr<RawServoAnimationValue>>& aValue,
      const TransformData& aTransformData, gfx::Path* aCachedMotionPath);

  /**
   * Returns true if |aPrerenderedRect| transformed by |aTransform| were
   * composited in |aClipRect| there appears area which wasn't pre-rendered
   * on the main-thread. I.e. checkerboarding.
   */
  static bool ShouldBeJank(const LayoutDeviceRect& aPrerenderedRect,
                           SideBits aOverflowedSides,
                           const gfx::Matrix4x4& aTransform,
                           const ParentLayerRect& aClipRect);
};

}  // namespace mozilla::layers

#endif  // mozilla_layers_AnimationHelper_h
