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
#include "mozilla/LayerAnimationInfo.h"  // for GetCSSPropertiesFor()
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
  mAnimationRenderRoots.Clear();
}

void CompositorAnimationStorage::ClearById(const uint64_t& aId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  mAnimatedValues.Remove(aId);
  mAnimations.Remove(aId);
  mAnimationRenderRoots.Remove(aId);
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

  animatedValue->Value().match(
      [&](const AnimationTransform& aTransform) {
        gfx::Matrix4x4 transform = aTransform.mFrameTransform;
        const TransformData& data = aTransform.mData;
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
      },
      [&](const float& aOpacity) { omtaValue = aOpacity; },
      [&](const nscolor& aColor) { omtaValue = aColor; });
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
    MOZ_ASSERT(value->Is<AnimationTransform>());
    *value = AnimatedValue(std::move(aTransformInDevSpace),
                           std::move(aFrameTransform), aData);
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
    MOZ_ASSERT(value->Is<nscolor>());
    *value = AnimatedValue(aColor);
  }
}

void CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                                  const float& aOpacity) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  auto count = mAnimatedValues.Count();
  AnimatedValue* value = mAnimatedValues.LookupOrAdd(aId, aOpacity);
  if (count == mAnimatedValues.Count()) {
    MOZ_ASSERT(value->Is<float>());
    *value = AnimatedValue(aOpacity);
  }
}

nsTArray<PropertyAnimationGroup>* CompositorAnimationStorage::GetAnimations(
    const uint64_t& aId) const {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mAnimations.Get(aId);
}

void CompositorAnimationStorage::SetAnimations(uint64_t aId,
                                               const AnimationArray& aValue,
                                               wr::RenderRoot aRenderRoot) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mAnimations.Put(aId, new nsTArray<PropertyAnimationGroup>(
                           AnimationHelper::ExtractAnimations(aValue)));
  mAnimationRenderRoots.Put(aId, aRenderRoot);
}

enum class CanSkipCompose {
  IfPossible,
  No,
};
static AnimationHelper::SampleResult SampleAnimationForProperty(
    TimeStamp aPreviousFrameTime, TimeStamp aCurrentFrameTime,
    const AnimatedValue* aPreviousValue, CanSkipCompose aCanSkipCompose,
    nsTArray<PropertyAnimation>& aPropertyAnimations,
    RefPtr<RawServoAnimationValue>& aAnimationValue) {
  MOZ_ASSERT(!aPropertyAnimations.IsEmpty(), "Should have animations");
  bool hasInEffectAnimations = false;
#ifdef DEBUG
  // In cases where this function returns a SampleResult::Skipped, we actually
  // do populate aAnimationValue in debug mode, so that we can MOZ_ASSERT at the
  // call site that the value that would have been computed matches the stored
  // value that we end up using. This flag is used to ensure we populate
  // aAnimationValue in this scenario.
  bool shouldBeSkipped = false;
#endif
  // Process in order, since later animations override earlier ones.
  for (PropertyAnimation& animation : aPropertyAnimations) {
    MOZ_ASSERT(
        (!animation.mOriginTime.IsNull() && animation.mStartTime.isSome()) ||
            animation.mIsNotPlaying,
        "If we are playing, we should have an origin time and a start time");

    // Determine if the animation was play-pending and used a ready time later
    // than the previous frame time.
    //
    // To determine this, _all_ of the following conditions need to hold:
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
          (animation.mStartTime.ref() +
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
        animation.mIsNotPlaying || animation.mStartTime.isNothing()
            ? animation.mHoldTime
            : (timeStamp - animation.mOriginTime - animation.mStartTime.ref())
                  .MultDouble(animation.mPlaybackRate);

    ComputedTiming computedTiming = dom::AnimationEffect::GetComputedTimingAt(
        dom::Nullable<TimeDuration>(elapsedDuration), animation.mTiming,
        animation.mPlaybackRate);

    if (computedTiming.mProgress.IsNull()) {
      continue;
    }

    dom::IterationCompositeOperation iterCompositeOperation =
        animation.mIterationComposite;

    // Skip calculation if the progress hasn't changed since the last
    // calculation.
    // Note that we don't skip calculate this animation if there is another
    // animation since the other animation might be 'accumulate' or 'add', or
    // might have a missing keyframe (i.e. this animation value will be used in
    // the missing keyframe).
    // FIXME Bug 1455476: We should do this optimizations for the case where
    // the layer has multiple animations and multiple properties.
    if (aCanSkipCompose == CanSkipCompose::IfPossible &&
        !dom::KeyframeEffect::HasComputedTimingChanged(
            computedTiming, iterCompositeOperation,
            animation.mProgressOnLastCompose,
            animation.mCurrentIterationOnLastCompose)) {
#ifdef DEBUG
      shouldBeSkipped = true;
#else
      return AnimationHelper::SampleResult::Skipped;
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

    // Like above optimization, skip calculation if the target segment isn't
    // changed and if the portion in the segment isn't changed.
    // This optimization is needed for CSS animations/transitions with step
    // timing functions (e.g. the throbber animation on tabs or frame based
    // animations).
    // FIXME Bug 1455476: Like the above optimization, we should apply this
    // optimizations for multiple animation cases and multiple properties as
    // well.
    if (aCanSkipCompose == CanSkipCompose::IfPossible &&
        animation.mSegmentIndexOnLastCompose == segmentIndex &&
        !animation.mPortionInSegmentOnLastCompose.IsNull() &&
        animation.mPortionInSegmentOnLastCompose.Value() == portion) {
#ifdef DEBUG
      shouldBeSkipped = true;
#else
      return AnimationHelper::SampleResult::Skipped;
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
      return AnimationHelper::SampleResult::Skipped;
    }
#endif

    hasInEffectAnimations = true;
    animation.mProgressOnLastCompose = computedTiming.mProgress;
    animation.mCurrentIterationOnLastCompose = computedTiming.mCurrentIteration;
    animation.mSegmentIndexOnLastCompose = segmentIndex;
    animation.mPortionInSegmentOnLastCompose.SetValue(portion);
  }

  return hasInEffectAnimations ? AnimationHelper::SampleResult::Sampled
                               : AnimationHelper::SampleResult::None;
}

AnimationHelper::SampleResult AnimationHelper::SampleAnimationForEachNode(
    TimeStamp aPreviousFrameTime, TimeStamp aCurrentFrameTime,
    const AnimatedValue* aPreviousValue,
    nsTArray<PropertyAnimationGroup>& aPropertyAnimationGroups,
    nsTArray<RefPtr<RawServoAnimationValue>>& aAnimationValues /* out */) {
  MOZ_ASSERT(!aPropertyAnimationGroups.IsEmpty(),
             "Should be called with animation data");
  MOZ_ASSERT(aAnimationValues.IsEmpty(),
             "Should be called with empty aAnimationValues");

  for (PropertyAnimationGroup& group : aPropertyAnimationGroups) {
    // Initialize animation value with base style.
    RefPtr<RawServoAnimationValue> currValue = group.mBaseStyle;

    CanSkipCompose canSkipCompose = aPropertyAnimationGroups.Length() == 1 &&
                                            group.mAnimations.Length() == 1
                                        ? CanSkipCompose::IfPossible
                                        : CanSkipCompose::No;
    SampleResult result = SampleAnimationForProperty(
        aPreviousFrameTime, aCurrentFrameTime, aPreviousValue, canSkipCompose,
        group.mAnimations, currValue);

    // FIXME: Bug 1455476: Do optimization for multiple properties. For now,
    // the result is skipped only if the property count == 1.
    if (result == SampleResult::Skipped) {
#ifdef DEBUG
      aAnimationValues.AppendElement(std::move(currValue));
#endif
      return SampleResult::Skipped;
    }

    if (result != SampleResult::Sampled) {
      continue;
    }

    // Insert the interpolation result into the output array.
    MOZ_ASSERT(currValue);
    aAnimationValues.AppendElement(std::move(currValue));
  }

#ifdef DEBUG
  // Sanity check that all of animation data are the same.
  const Maybe<TransformData>& lastData =
      aPropertyAnimationGroups.LastElement().mAnimationData;
  for (const PropertyAnimationGroup& group : aPropertyAnimationGroups) {
    const Maybe<TransformData>& data = group.mAnimationData;
    MOZ_ASSERT(data.isSome() == lastData.isSome(),
               "The type of AnimationData should be the same");
    if (data.isNothing()) {
      continue;
    }

    MOZ_ASSERT(data.isSome());
    const TransformData& transformData = data.ref();
    const TransformData& lastTransformData = lastData.ref();
    MOZ_ASSERT(transformData.origin() == lastTransformData.origin() &&
                   transformData.transformOrigin() ==
                       lastTransformData.transformOrigin() &&
                   transformData.bounds() == lastTransformData.bounds() &&
                   transformData.appUnitsPerDevPixel() ==
                       lastTransformData.appUnitsPerDevPixel(),
               "All of members of TransformData should be the same");
  }
#endif

  return aAnimationValues.IsEmpty() ? SampleResult::None
                                    : SampleResult::Sampled;
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

  nsCSSPropertyID prevID = eCSSProperty_UNKNOWN;
  PropertyAnimationGroup* currData = nullptr;
  DebugOnly<const layers::Animatable*> currBaseStyle = nullptr;

  for (const Animation& animation : aAnimations) {
    // Animations with same property are grouped together, so we can just
    // check if the current property is the same as the previous one for
    // knowing this is a new group.
    if (prevID != animation.property()) {
      // Got a different group, we should create a different array.
      currData = propertyAnimationGroupArray.AppendElement();
      currData->mProperty = animation.property();
      currData->mAnimationData = animation.data();
      prevID = animation.property();

      // Reset the debug pointer.
      currBaseStyle = nullptr;
    }

    MOZ_ASSERT(currData);
    if (animation.baseStyle().type() != Animatable::Tnull_t) {
      MOZ_ASSERT(!currBaseStyle || *currBaseStyle == animation.baseStyle(),
                 "Should be the same base style");

      currData->mBaseStyle = AnimationValue::FromAnimatable(
          animation.property(), animation.baseStyle());
      currBaseStyle = &animation.baseStyle();
    }

    PropertyAnimation* propertyAnimation =
        currData->mAnimations.AppendElement();

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

#ifdef DEBUG
  // Sanity check that the grouped animation data is correct by looking at the
  // property set.
  if (!propertyAnimationGroupArray.IsEmpty()) {
    nsCSSPropertyIDSet seenProperties;
    for (const auto& group : propertyAnimationGroupArray) {
      nsCSSPropertyID id = group.mProperty;

      MOZ_ASSERT(!seenProperties.HasProperty(id), "Should be a new property");
      seenProperties.AddProperty(id);
    }

    MOZ_ASSERT(
        seenProperties.IsSubsetOf(LayerAnimationInfo::GetCSSPropertiesFor(
            DisplayItemType::TYPE_TRANSFORM)) ||
            seenProperties.IsSubsetOf(LayerAnimationInfo::GetCSSPropertiesFor(
                DisplayItemType::TYPE_OPACITY)) ||
            seenProperties.IsSubsetOf(LayerAnimationInfo::GetCSSPropertiesFor(
                DisplayItemType::TYPE_BACKGROUND_COLOR)),
        "The property set of output should be the subset of transform-like "
        "properties, opacity, or background_color.");
  }
#endif

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
    nsTArray<RefPtr<RawServoAnimationValue>> animationValues;
    AnimatedValue* previousValue = aStorage->GetAnimatedValue(iter.Key());
    AnimationHelper::SampleResult sampleResult =
        AnimationHelper::SampleAnimationForEachNode(
            aPreviousFrameTime, aCurrentFrameTime, previousValue,
            propertyAnimationGroups, animationValues);

    if (sampleResult != AnimationHelper::SampleResult::Sampled) {
      continue;
    }

    const PropertyAnimationGroup& lastPropertyAnimationGroup =
        propertyAnimationGroups.LastElement();

    // Store the AnimatedValue
    switch (lastPropertyAnimationGroup.mProperty) {
      case eCSSProperty_opacity: {
        MOZ_ASSERT(animationValues.Length() == 1);
        aStorage->SetAnimatedValue(
            iter.Key(), Servo_AnimationValue_GetOpacity(animationValues[0]));
        break;
      }
      case eCSSProperty_rotate:
      case eCSSProperty_scale:
      case eCSSProperty_translate:
      case eCSSProperty_transform: {
        const TransformData& transformData =
            lastPropertyAnimationGroup.mAnimationData.ref();

        gfx::Matrix4x4 transform =
            ServoAnimationValueToMatrix4x4(animationValues, transformData);
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
    const nsTArray<RefPtr<RawServoAnimationValue>>& aValues,
    const TransformData& aTransformData) {
  // This is a bit silly just to avoid the transform list copy from the
  // animation transform list.
  auto noneTranslate = StyleTranslate::None();
  auto noneRotate = StyleRotate::None();
  auto noneScale = StyleScale::None();
  const StyleTransform noneTransform;

  const StyleTranslate* translate = nullptr;
  const StyleRotate* rotate = nullptr;
  const StyleScale* scale = nullptr;
  const StyleTransform* transform = nullptr;

  // TODO: Bug 1429305: Support compositor animations for motion-path.
  for (const auto& value : aValues) {
    MOZ_ASSERT(value);
    nsCSSPropertyID id = Servo_AnimationValue_GetPropertyId(value);
    switch (id) {
      case eCSSProperty_transform:
        MOZ_ASSERT(!transform);
        transform = Servo_AnimationValue_GetTransform(value);
        break;
      case eCSSProperty_translate:
        MOZ_ASSERT(!translate);
        translate = Servo_AnimationValue_GetTranslate(value);
        break;
      case eCSSProperty_rotate:
        MOZ_ASSERT(!rotate);
        rotate = Servo_AnimationValue_GetRotate(value);
        break;
      case eCSSProperty_scale:
        MOZ_ASSERT(!scale);
        scale = Servo_AnimationValue_GetScale(value);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unsupported transform-like property");
    }
  }
  // We expect all our transform data to arrive in device pixels
  gfx::Point3D transformOrigin = aTransformData.transformOrigin();
  nsDisplayTransform::FrameTransformProperties props(
      translate ? *translate : noneTranslate, rotate ? *rotate : noneRotate,
      scale ? *scale : noneScale, transform ? *transform : noneTransform,
      transformOrigin);

  return nsDisplayTransform::GetResultingTransformMatrix(
      props, aTransformData.origin(), aTransformData.appUnitsPerDevPixel(), 0,
      &aTransformData.bounds());
}

}  // namespace layers
}  // namespace mozilla
