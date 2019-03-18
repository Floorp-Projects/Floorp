/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationHelper.h"
#include "mozilla/ComputedTimingFunction.h"      // for ComputedTimingFunction
#include "mozilla/dom/AnimationEffectBinding.h"  // for dom::FillMode
#include "mozilla/dom/KeyframeEffectBinding.h"   // for dom::IterationComposite
#include "mozilla/dom/KeyframeEffect.h"       // for dom::KeyFrameEffectReadOnly
#include "mozilla/dom/Nullable.h"             // for dom::Nullable
#include "mozilla/layers/CompositorThread.h"  // for CompositorThreadHolder
#include "mozilla/layers/LayerAnimationUtils.h"  // for TimingFunctionToComputedTimingFunction
#include "mozilla/ServoBindings.h"  // for Servo_ComposeAnimationSegment, etc
#include "mozilla/StyleAnimationValue.h"  // for StyleAnimationValue, etc
#include "nsDeviceContext.h"              // for AppUnitsPerCSSPixel
#include "nsDisplayList.h"                // for nsDisplayTransform, etc

namespace mozilla {
namespace layers {

void CompositorAnimationStorage::Clear() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  mAnimatedValues.Clear();
  mAnimations.Clear();
}

void CompositorAnimationStorage::ClearById(const uint64_t& aId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  mAnimatedValues.Remove(aId);
  mAnimations.Remove(aId);
}

AnimatedValue* CompositorAnimationStorage::GetAnimatedValue(
    const uint64_t& aId) const {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mAnimatedValues.Get(aId);
}

OMTAValue CompositorAnimationStorage::GetOMTAValue(const uint64_t& aId) const {
  OMTAValue omtaValue = mozilla::null_t();
  auto animatedValue = GetAnimatedValue(aId);
  if (!animatedValue) {
    return omtaValue;
  }

  switch (animatedValue->mType) {
    case AnimatedValue::COLOR:
      omtaValue = animatedValue->mColor;
      break;
    case AnimatedValue::OPACITY:
      omtaValue = animatedValue->mOpacity;
      break;
    case AnimatedValue::TRANSFORM: {
      gfx::Matrix4x4 transform = animatedValue->mTransform.mFrameTransform;
      const TransformData& data = animatedValue->mTransform.mData;
      float scale = data.appUnitsPerDevPixel();
      gfx::Point3D transformOrigin = data.transformOrigin();

      // Undo the rebasing applied by
      // nsDisplayTransform::GetResultingTransformMatrixInternal
      transform.ChangeBasis(-transformOrigin);

      // Convert to CSS pixels (this undoes the operations performed by
      // nsStyleTransformMatrix::ProcessTranslatePart which is called from
      // nsDisplayTransform::GetResultingTransformMatrix)
      double devPerCss = double(scale) / double(AppUnitsPerCSSPixel());
      transform._41 *= devPerCss;
      transform._42 *= devPerCss;
      transform._43 *= devPerCss;
      omtaValue = transform;
      break;
    }
    case AnimatedValue::NONE:
      break;
  }

  return omtaValue;
}

void CompositorAnimationStorage::SetAnimatedValue(
    uint64_t aId, gfx::Matrix4x4&& aTransformInDevSpace,
    gfx::Matrix4x4&& aFrameTransform, const TransformData& aData) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  auto count = mAnimatedValues.Count();
  AnimatedValue* value = mAnimatedValues.LookupOrAdd(
      aId, std::move(aTransformInDevSpace), std::move(aFrameTransform), aData);
  if (count == mAnimatedValues.Count()) {
    MOZ_ASSERT(value->mType == AnimatedValue::TRANSFORM);
    value->mTransform.mTransformInDevSpace = std::move(aTransformInDevSpace);
    value->mTransform.mFrameTransform = std::move(aFrameTransform);
    value->mTransform.mData = aData;
  }
}

void CompositorAnimationStorage::SetAnimatedValue(
    uint64_t aId, gfx::Matrix4x4&& aTransformInDevSpace) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  const TransformData dontCare = {};
  SetAnimatedValue(aId, std::move(aTransformInDevSpace), gfx::Matrix4x4(),
                   dontCare);
}

void CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                                  nscolor aColor) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  auto count = mAnimatedValues.Count();
  AnimatedValue* value = mAnimatedValues.LookupOrAdd(aId, aColor);
  if (count == mAnimatedValues.Count()) {
    MOZ_ASSERT(value->mType == AnimatedValue::COLOR);
    value->mColor = aColor;
  }
}

void CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                                  const float& aOpacity) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  auto count = mAnimatedValues.Count();
  AnimatedValue* value = mAnimatedValues.LookupOrAdd(aId, aOpacity);
  if (count == mAnimatedValues.Count()) {
    MOZ_ASSERT(value->mType == AnimatedValue::OPACITY);
    value->mOpacity = aOpacity;
  }
}

nsTArray<PropertyAnimationGroup>* CompositorAnimationStorage::GetAnimations(
    const uint64_t& aId) const {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mAnimations.Get(aId);
}

void CompositorAnimationStorage::SetAnimations(uint64_t aId,
                                               const AnimationArray& aValue) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mAnimations.Put(aId, new nsTArray<PropertyAnimationGroup>(
                           AnimationHelper::ExtractAnimations(aValue)));
}

AnimationHelper::SampleResult AnimationHelper::SampleAnimationForEachNode(
    TimeStamp aPreviousFrameTime, TimeStamp aCurrentFrameTime,
    nsTArray<PropertyAnimationGroup>& aPropertyAnimationGroups,
    RefPtr<RawServoAnimationValue>& aAnimationValue,
    const AnimatedValue* aPreviousValue) {
  MOZ_ASSERT(!aPropertyAnimationGroups.IsEmpty(),
             "Should be called with animations");

  // FIXME: Will extend this list to multiple properties. For now, its length is
  // always 1.
  MOZ_ASSERT(aPropertyAnimationGroups.Length() == 1);
  PropertyAnimationGroup& propertyAnimationGroup = aPropertyAnimationGroups[0];

  bool hasInEffectAnimations = false;
#ifdef DEBUG
  // In cases where this function returns a SampleResult::Skipped, we actually
  // do populate aAnimationValue in debug mode, so that we can MOZ_ASSERT at the
  // call site that the value that would have been computed matches the stored
  // value that we end up using. This flag is used to ensure we populate
  // aAnimationValue in this scenario.
  bool shouldBeSkipped = false;
#endif
  bool isSingleAnimation = propertyAnimationGroup.mAnimations.Length() == 1;
  // Initialize by using base style.
  aAnimationValue = propertyAnimationGroup.mBaseStyle;
  // Process in order, since later animations override earlier ones.
  for (PropertyAnimation& animation : propertyAnimationGroup.mAnimations) {
    MOZ_ASSERT(
        (!animation.mOriginTime.IsNull() &&
         animation.mStartTime.type() == MaybeTimeDuration::TTimeDuration) ||
            animation.mIsNotPlaying,
        "If we are playing, we should have an origin time and a start time");

    // Determine if the animation was play-pending and used a ready time later
    // than the previous frame time.
    //
    // To determine this, _all_ of the following consitions need to hold:
    //
    // * There was no previous animation value (i.e. this is the first frame for
    //   the animation since it was sent to the compositor), and
    // * The animation is playing, and
    // * There is a previous frame time, and
    // * The ready time of the animation is ahead of the previous frame time.
    //
    bool hasFutureReadyTime = false;
    if (!aPreviousValue && !animation.mIsNotPlaying &&
        !aPreviousFrameTime.IsNull()) {
      // This is the inverse of the calculation performed in
      // AnimationInfo::StartPendingAnimations to calculate the start time of
      // play-pending animations.
      // Note that we have to calculate (TimeStamp + TimeDuration) last to avoid
      // underflow in the middle of the calulation.
      const TimeStamp readyTime =
          animation.mOriginTime +
          (animation.mStartTime.get_TimeDuration() +
           animation.mHoldTime.MultDouble(1.0 / animation.mPlaybackRate));
      hasFutureReadyTime =
          !readyTime.IsNull() && readyTime > aPreviousFrameTime;
    }
    // Use the previous vsync time to make main thread animations and compositor
    // more closely aligned.
    //
    // On the first frame where we have animations the previous timestamp will
    // not be set so we simply use the current timestamp.  As a result we will
    // end up painting the first frame twice.  That doesn't appear to be
    // noticeable, however.
    //
    // Likewise, if the animation is play-pending, it may have a ready time that
    // is *after* |aPreviousFrameTime| (but *before* |aCurrentFrameTime|).
    // To avoid flicker we need to use |aCurrentFrameTime| to avoid temporarily
    // jumping backwards into the range prior to when the animation starts.
    const TimeStamp& timeStamp =
        aPreviousFrameTime.IsNull() || hasFutureReadyTime ? aCurrentFrameTime
                                                          : aPreviousFrameTime;

    // If the animation is not currently playing, e.g. paused or
    // finished, then use the hold time to stay at the same position.
    TimeDuration elapsedDuration =
        animation.mIsNotPlaying ||
                animation.mStartTime.type() != MaybeTimeDuration::TTimeDuration
            ? animation.mHoldTime
            : (timeStamp - animation.mOriginTime -
               animation.mStartTime.get_TimeDuration())
                  .MultDouble(animation.mPlaybackRate);

    ComputedTiming computedTiming = dom::AnimationEffect::GetComputedTimingAt(
        dom::Nullable<TimeDuration>(elapsedDuration), animation.mTiming,
        animation.mPlaybackRate);

    if (computedTiming.mProgress.IsNull()) {
      continue;
    }

    dom::IterationCompositeOperation iterCompositeOperation =
        animation.mIterationComposite;

    // Skip caluculation if the progress hasn't changed since the last
    // calculation.
    // Note that we don't skip calculate this animation if there is another
    // animation since the other animation might be 'accumulate' or 'add', or
    // might have a missing keyframe (i.e. this animation value will be used in
    // the missing keyframe).
    // FIXME Bug 1455476: We should do this optimizations for the case where
    // the layer has multiple animations.
    if (isSingleAnimation && !dom::KeyframeEffect::HasComputedTimingChanged(
                                 computedTiming, iterCompositeOperation,
                                 animation.mProgressOnLastCompose,
                                 animation.mCurrentIterationOnLastCompose)) {
#ifdef DEBUG
      shouldBeSkipped = true;
#else
      return SampleResult::Skipped;
#endif
    }

    uint32_t segmentIndex = 0;
    size_t segmentSize = animation.mSegments.Length();
    PropertyAnimation::SegmentData* segment = animation.mSegments.Elements();
    while (segment->mEndPortion < computedTiming.mProgress.Value() &&
           segmentIndex < segmentSize - 1) {
      ++segment;
      ++segmentIndex;
    }

    double positionInSegment =
        (computedTiming.mProgress.Value() - segment->mStartPortion) /
        (segment->mEndPortion - segment->mStartPortion);

    double portion = ComputedTimingFunction::GetPortion(
        segment->mFunction, positionInSegment, computedTiming.mBeforeFlag);

    // Like above optimization, skip caluculation if the target segment isn't
    // changed and if the portion in the segment isn't changed.
    // This optimization is needed for CSS animations/transitions with step
    // timing functions (e.g. the throbber animation on tab or frame based
    // animations).
    // FIXME Bug 1455476: Like the above optimization, we should apply this
    // optimizations for multiple animation cases as well.
    if (isSingleAnimation &&
        animation.mSegmentIndexOnLastCompose == segmentIndex &&
        !animation.mPortionInSegmentOnLastCompose.IsNull() &&
        animation.mPortionInSegmentOnLastCompose.Value() == portion) {
#ifdef DEBUG
      shouldBeSkipped = true;
#else
      return SampleResult::Skipped;
#endif
    }

    AnimationPropertySegment animSegment;
    animSegment.mFromKey = 0.0;
    animSegment.mToKey = 1.0;
    animSegment.mFromValue = AnimationValue(segment->mStartValue);
    animSegment.mToValue = AnimationValue(segment->mEndValue);
    animSegment.mFromComposite = segment->mStartComposite;
    animSegment.mToComposite = segment->mEndComposite;

    // interpolate the property
    aAnimationValue =
        Servo_ComposeAnimationSegment(
            &animSegment, aAnimationValue,
            animation.mSegments.LastElement().mEndValue, iterCompositeOperation,
            portion, computedTiming.mCurrentIteration)
            .Consume();

#ifdef DEBUG
    if (shouldBeSkipped) {
      return SampleResult::Skipped;
    }
#endif

    hasInEffectAnimations = true;
    animation.mProgressOnLastCompose = computedTiming.mProgress;
    animation.mCurrentIterationOnLastCompose = computedTiming.mCurrentIteration;
    animation.mSegmentIndexOnLastCompose = segmentIndex;
    animation.mPortionInSegmentOnLastCompose.SetValue(portion);
  }

#ifdef DEBUG
  // Sanity check that all of animation data are the same.
  const AnimationData& lastData =
      aPropertyAnimationGroups.LastElement().mAnimationData;
  for (const PropertyAnimationGroup& group : aPropertyAnimationGroups) {
    const AnimationData& data = group.mAnimationData;
    MOZ_ASSERT(data.type() == lastData.type(),
               "The type of AnimationData should be the same");
    if (data.type() == AnimationData::Tnull_t) {
      continue;
    }

    MOZ_ASSERT(data.type() == AnimationData::TTransformData);
    const TransformData& transformData = data.get_TransformData();
    const TransformData& lastTransformData = lastData.get_TransformData();
    MOZ_ASSERT(transformData.origin() == lastTransformData.origin() &&
                   transformData.transformOrigin() ==
                       lastTransformData.transformOrigin() &&
                   transformData.bounds() == lastTransformData.bounds() &&
                   transformData.appUnitsPerDevPixel() ==
                       lastTransformData.appUnitsPerDevPixel(),
               "All of members of TransformData should be the same");
  }
#endif

  return hasInEffectAnimations ? SampleResult::Sampled : SampleResult::None;
}

static dom::FillMode GetAdjustedFillMode(const Animation& aAnimation) {
  // Adjust fill mode so that if the main thread is delayed in clearing
  // this animation we don't introduce flicker by jumping back to the old
  // underlying value.
  auto fillMode = static_cast<dom::FillMode>(aAnimation.fillMode());
  float playbackRate = aAnimation.playbackRate();
  switch (fillMode) {
    case dom::FillMode::None:
      if (playbackRate > 0) {
        fillMode = dom::FillMode::Forwards;
      } else if (playbackRate < 0) {
        fillMode = dom::FillMode::Backwards;
      }
      break;
    case dom::FillMode::Backwards:
      if (playbackRate > 0) {
        fillMode = dom::FillMode::Both;
      }
      break;
    case dom::FillMode::Forwards:
      if (playbackRate < 0) {
        fillMode = dom::FillMode::Both;
      }
      break;
    default:
      break;
  }
  return fillMode;
}

nsTArray<PropertyAnimationGroup> AnimationHelper::ExtractAnimations(
    const AnimationArray& aAnimations) {
  nsTArray<PropertyAnimationGroup> propertyAnimationGroupArray;
  // FIXME: We only have one entry for now. In the next patch, we will extend
  // this to multiple properties.
  auto* propertyAnimationGroup = propertyAnimationGroupArray.AppendElement();
  if (!aAnimations.IsEmpty()) {
    propertyAnimationGroup->mProperty = aAnimations.LastElement().property();
    propertyAnimationGroup->mAnimationData = aAnimations.LastElement().data();
  }

  for (const Animation& animation : aAnimations) {
    if (animation.baseStyle().type() != Animatable::Tnull_t) {
      propertyAnimationGroup->mBaseStyle = AnimationValue::FromAnimatable(
          animation.property(), animation.baseStyle());
    }

    PropertyAnimation* propertyAnimation =
        propertyAnimationGroup->mAnimations.AppendElement();

    propertyAnimation->mOriginTime = animation.originTime();
    propertyAnimation->mStartTime = animation.startTime();
    propertyAnimation->mHoldTime = animation.holdTime();
    propertyAnimation->mPlaybackRate = animation.playbackRate();
    propertyAnimation->mIterationComposite =
        static_cast<dom::IterationCompositeOperation>(
            animation.iterationComposite());
    propertyAnimation->mIsNotPlaying = animation.isNotPlaying();
    propertyAnimation->mTiming =
        TimingParams{animation.duration(),
                     animation.delay(),
                     animation.endDelay(),
                     animation.iterations(),
                     animation.iterationStart(),
                     static_cast<dom::PlaybackDirection>(animation.direction()),
                     GetAdjustedFillMode(animation),
                     AnimationUtils::TimingFunctionToComputedTimingFunction(
                         animation.easingFunction())};

    nsTArray<PropertyAnimation::SegmentData>& segmentData =
        propertyAnimation->mSegments;
    for (const AnimationSegment& segment : animation.segments()) {
      segmentData.AppendElement(PropertyAnimation::SegmentData{
          AnimationValue::FromAnimatable(animation.property(),
                                         segment.startState()),
          AnimationValue::FromAnimatable(animation.property(),
                                         segment.endState()),
          AnimationUtils::TimingFunctionToComputedTimingFunction(
              segment.sampleFn()),
          segment.startPortion(), segment.endPortion(),
          static_cast<dom::CompositeOperation>(segment.startComposite()),
          static_cast<dom::CompositeOperation>(segment.endComposite())});
    }
  }

  if (propertyAnimationGroup->IsEmpty()) {
    propertyAnimationGroupArray.Clear();
  }
  return propertyAnimationGroupArray;
}

uint64_t AnimationHelper::GetNextCompositorAnimationsId() {
  static uint32_t sNextId = 0;
  ++sNextId;

  uint32_t procId = static_cast<uint32_t>(base::GetCurrentProcId());
  uint64_t nextId = procId;
  nextId = nextId << 32 | sNextId;
  return nextId;
}

bool AnimationHelper::SampleAnimations(CompositorAnimationStorage* aStorage,
                                       TimeStamp aPreviousFrameTime,
                                       TimeStamp aCurrentFrameTime) {
  MOZ_ASSERT(aStorage);
  bool isAnimating = false;

  // Do nothing if there are no compositor animations
  if (!aStorage->AnimationsCount()) {
    return isAnimating;
  }

  // Sample the animations in CompositorAnimationStorage
  for (auto iter = aStorage->ConstAnimationsTableIter(); !iter.Done();
       iter.Next()) {
    auto& propertyAnimationGroups = *iter.UserData();
    if (propertyAnimationGroups.IsEmpty()) {
      continue;
    }

    isAnimating = true;
    RefPtr<RawServoAnimationValue> animationValue;
    AnimatedValue* previousValue = aStorage->GetAnimatedValue(iter.Key());
    AnimationHelper::SampleResult sampleResult =
        AnimationHelper::SampleAnimationForEachNode(
            aPreviousFrameTime, aCurrentFrameTime, propertyAnimationGroups,
            animationValue, previousValue);

    if (sampleResult != AnimationHelper::SampleResult::Sampled) {
      continue;
    }

    const PropertyAnimationGroup& lastPropertyAnimationGroup =
        propertyAnimationGroups.LastElement();

    // Store the AnimatedValue
    switch (lastPropertyAnimationGroup.mProperty) {
      case eCSSProperty_opacity: {
        aStorage->SetAnimatedValue(
            iter.Key(), Servo_AnimationValue_GetOpacity(animationValue));
        break;
      }
      case eCSSProperty_transform: {
        const TransformData& transformData =
            lastPropertyAnimationGroup.mAnimationData.get_TransformData();

        gfx::Matrix4x4 transform =
            ServoAnimationValueToMatrix4x4(animationValue, transformData);
        gfx::Matrix4x4 frameTransform = transform;
        // If the parent has perspective transform, then the offset into
        // reference frame coordinates is already on this transform. If not,
        // then we need to ask for it to be added here.
        if (!transformData.hasPerspectiveParent()) {
          nsLayoutUtils::PostTranslate(transform, transformData.origin(),
                                       transformData.appUnitsPerDevPixel(),
                                       true);
        }

        transform.PostScale(transformData.inheritedXScale(),
                            transformData.inheritedYScale(), 1);

        aStorage->SetAnimatedValue(iter.Key(), std::move(transform),
                                   std::move(frameTransform), transformData);
        break;
      }
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled animated property");
    }
  }

  return isAnimating;
}

gfx::Matrix4x4 AnimationHelper::ServoAnimationValueToMatrix4x4(
    const RefPtr<RawServoAnimationValue>& aValue,
    const TransformData& aTransformData) {
  // FIXME: Bug 1457033: We should convert servo's animation value to matrix
  // directly without nsCSSValueSharedList.
  RefPtr<nsCSSValueSharedList> list;
  Servo_AnimationValue_GetTransform(aValue, &list);
  // We expect all our transform data to arrive in device pixels
  gfx::Point3D transformOrigin = aTransformData.transformOrigin();
  nsDisplayTransform::FrameTransformProperties props(std::move(list),
                                                     transformOrigin);

  return nsDisplayTransform::GetResultingTransformMatrix(
      props, aTransformData.origin(), aTransformData.appUnitsPerDevPixel(), 0,
      &aTransformData.bounds());
}

}  // namespace layers
}  // namespace mozilla
