/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AnimationStorageData_h
#define mozilla_layers_AnimationStorageData_h

#include "mozilla/dom/Nullable.h"
#include "mozilla/ServoStyleConsts.h"       // for ComputedTimingFunction
#include "mozilla/layers/LayersMessages.h"  // for TransformData, etc
#include "mozilla/layers/LayersTypes.h"     // for LayersId
#include "mozilla/TimeStamp.h"              // for TimeStamp
#include "mozilla/TimingParams.h"
#include "X11UndefineNone.h"

namespace mozilla {

namespace dom {
enum class CompositeOperation : uint8_t;
enum class IterationCompositeOperation : uint8_t;
};  // namespace dom

namespace layers {

struct PropertyAnimation {
  struct SegmentData {
    RefPtr<RawServoAnimationValue> mStartValue;
    RefPtr<RawServoAnimationValue> mEndValue;
    Maybe<mozilla::StyleComputedTimingFunction> mFunction;
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

  // The information for scroll-linked animations.
  Maybe<ScrollTimelineOptions> mScrollTimelineOptions;

  void ResetLastCompositionValues() {
    mCurrentIterationOnLastCompose = 0;
    mSegmentIndexOnLastCompose = 0;
    mProgressOnLastCompose.SetNull();
    mPortionInSegmentOnLastCompose.SetNull();
  }
};

struct PropertyAnimationGroup {
  nsCSSPropertyID mProperty;

  nsTArray<PropertyAnimation> mAnimations;
  RefPtr<RawServoAnimationValue> mBaseStyle;

  bool IsEmpty() const { return mAnimations.IsEmpty(); }
  void Clear() {
    mAnimations.Clear();
    mBaseStyle = nullptr;
  }
  void ResetLastCompositionValues() {
    for (PropertyAnimation& animation : mAnimations) {
      animation.ResetLastCompositionValues();
    }
  }
};

struct AnimationStorageData {
  // Each entry in the array represents an animation list for one property.  For
  // transform-like properties (e.g. transform, rotate etc.), there may be
  // multiple entries depending on how many transform-like properties we have.
  nsTArray<PropertyAnimationGroup> mAnimation;
  Maybe<TransformData> mTransformData;
  // For motion path. We cached the gfx path for optimization.
  RefPtr<gfx::Path> mCachedMotionPath;
  // This is used to communicate with the main-thread. E.g. to tell the fact
  // that this animation needs to be pre-rendered again on the main-thread, etc.
  LayersId mLayersId;

  AnimationStorageData() = default;
  AnimationStorageData(AnimationStorageData&& aOther) = default;
  AnimationStorageData& operator=(AnimationStorageData&& aOther) = default;

  // Avoid any copy because mAnimation could be a large array.
  AnimationStorageData(const AnimationStorageData& aOther) = delete;
  AnimationStorageData& operator=(const AnimationStorageData& aOther) = delete;

  bool IsEmpty() const { return mAnimation.IsEmpty(); }
  void Clear() {
    mAnimation.Clear();
    mTransformData.reset();
    mCachedMotionPath = nullptr;
  }
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_AnimationStorageData_h
