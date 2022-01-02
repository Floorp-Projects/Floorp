/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationHelper.h"
#include "base/process_util.h"
#include "gfx2DGlue.h"                       // for ThebesRect
#include "gfxLineSegment.h"                  // for gfxLineSegment
#include "gfxPoint.h"                        // for gfxPoint
#include "gfxQuad.h"                         // for gfxQuad
#include "gfxRect.h"                         // for gfxRect
#include "gfxUtils.h"                        // for gfxUtils::TransformToQuad
#include "mozilla/ComputedTimingFunction.h"  // for ComputedTimingFunction
#include "mozilla/dom/AnimationEffectBinding.h"  // for dom::FillMode
#include "mozilla/dom/KeyframeEffectBinding.h"   // for dom::IterationComposite
#include "mozilla/dom/KeyframeEffect.h"       // for dom::KeyFrameEffectReadOnly
#include "mozilla/dom/Nullable.h"             // for dom::Nullable
#include "mozilla/layers/CompositorThread.h"  // for CompositorThreadHolder
#include "mozilla/layers/CompositorAnimationStorage.h"  // for CompositorAnimationStorage
#include "mozilla/layers/LayerAnimationUtils.h"  // for TimingFunctionToComputedTimingFunction
#include "mozilla/LayerAnimationInfo.h"  // for GetCSSPropertiesFor()
#include "mozilla/MotionPathUtils.h"     // for ResolveMotionPath()
#include "mozilla/ServoBindings.h"  // for Servo_ComposeAnimationSegment, etc
#include "mozilla/StyleAnimationValue.h"  // for StyleAnimationValue, etc
#include "nsDeviceContext.h"              // for AppUnitsPerCSSPixel
#include "nsDisplayList.h"                // for nsDisplayTransform, etc

namespace mozilla {
namespace layers {

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

  nsTArray<RefPtr<RawServoAnimationValue>> nonAnimatingValues;
  for (PropertyAnimationGroup& group : aPropertyAnimationGroups) {
    // Initialize animation value with base style.
    RefPtr<RawServoAnimationValue> currValue = group.mBaseStyle;

    CanSkipCompose canSkipCompose =
        aPreviousValue && aPropertyAnimationGroups.Length() == 1 &&
                group.mAnimations.Length() == 1
            ? CanSkipCompose::IfPossible
            : CanSkipCompose::No;

    MOZ_ASSERT(
        !group.mAnimations.IsEmpty() ||
            nsCSSPropertyIDSet::TransformLikeProperties().HasProperty(
                group.mProperty),
        "Only transform-like properties can have empty PropertyAnimation list");

    // For properties which are not animating (i.e. their values are always the
    // same), we store them in a different array, and then merge them into the
    // final result (a.k.a. aAnimationValues) because we shouldn't take them
    // into account for SampleResult. (In other words, these properties
    // shouldn't affect the optimization.)
    if (group.mAnimations.IsEmpty()) {
      nonAnimatingValues.AppendElement(std::move(currValue));
      continue;
    }

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

  SampleResult rv =
      aAnimationValues.IsEmpty() ? SampleResult::None : SampleResult::Sampled;
  if (rv == SampleResult::Sampled) {
    aAnimationValues.AppendElements(std::move(nonAnimatingValues));
  }
  return rv;
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

#ifdef DEBUG
static bool HasTransformLikeAnimations(const AnimationArray& aAnimations) {
  nsCSSPropertyIDSet transformSet =
      nsCSSPropertyIDSet::TransformLikeProperties();

  for (const Animation& animation : aAnimations) {
    if (animation.isNotAnimating()) {
      continue;
    }

    if (transformSet.HasProperty(animation.property())) {
      return true;
    }
  }

  return false;
}
#endif

AnimationStorageData AnimationHelper::ExtractAnimations(
    const LayersId& aLayersId, const AnimationArray& aAnimations) {
  AnimationStorageData storageData;
  storageData.mLayersId = aLayersId;

  nsCSSPropertyID prevID = eCSSProperty_UNKNOWN;
  PropertyAnimationGroup* currData = nullptr;
  DebugOnly<const layers::Animatable*> currBaseStyle = nullptr;

  for (const Animation& animation : aAnimations) {
    // Animations with same property are grouped together, so we can just
    // check if the current property is the same as the previous one for
    // knowing this is a new group.
    if (prevID != animation.property()) {
      // Got a different group, we should create a different array.
      currData = storageData.mAnimation.AppendElement();
      currData->mProperty = animation.property();
      if (animation.transformData()) {
        MOZ_ASSERT(!storageData.mTransformData,
                   "Only one entry has TransformData");
        storageData.mTransformData = animation.transformData();
      }

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

    // If this layers::Animation sets isNotAnimating to true, it only has
    // base style and doesn't have any animation information, so we can skip
    // the rest steps. (And so its PropertyAnimationGroup::mAnimation will be
    // an empty array.)
    if (animation.isNotAnimating()) {
      MOZ_ASSERT(nsCSSPropertyIDSet::TransformLikeProperties().HasProperty(
                     animation.property()),
                 "Only transform-like properties could set this true");

      if (animation.property() == eCSSProperty_offset_path) {
        MOZ_ASSERT(currData->mBaseStyle,
                   "Fixed offset-path should have base style");
        MOZ_ASSERT(HasTransformLikeAnimations(aAnimations));

        AnimationValue value{currData->mBaseStyle};
        const StyleOffsetPath& offsetPath = value.GetOffsetPathProperty();
        if (offsetPath.IsPath()) {
          MOZ_ASSERT(!storageData.mCachedMotionPath,
                     "Only one offset-path: path() is set");

          RefPtr<gfx::PathBuilder> builder =
              MotionPathUtils::GetCompositorPathBuilder();
          storageData.mCachedMotionPath =
              MotionPathUtils::BuildPath(offsetPath.AsPath(), builder);
        }
      }

      continue;
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
  if (!storageData.mAnimation.IsEmpty()) {
    nsCSSPropertyIDSet seenProperties;
    for (const auto& group : storageData.mAnimation) {
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

    if (seenProperties.IsSubsetOf(LayerAnimationInfo::GetCSSPropertiesFor(
            DisplayItemType::TYPE_TRANSFORM))) {
      MOZ_ASSERT(storageData.mTransformData, "Should have TransformData");
    }

    if (seenProperties.HasProperty(eCSSProperty_offset_path)) {
      MOZ_ASSERT(storageData.mTransformData, "Should have TransformData");
      MOZ_ASSERT(storageData.mTransformData->motionPathData(),
                 "Should have MotionPathData");
    }
  }
#endif

  return storageData;
}

uint64_t AnimationHelper::GetNextCompositorAnimationsId() {
  static uint32_t sNextId = 0;
  ++sNextId;

  uint32_t procId = static_cast<uint32_t>(base::GetCurrentProcId());
  uint64_t nextId = procId;
  nextId = nextId << 32 | sNextId;
  return nextId;
}

gfx::Matrix4x4 AnimationHelper::ServoAnimationValueToMatrix4x4(
    const nsTArray<RefPtr<RawServoAnimationValue>>& aValues,
    const TransformData& aTransformData, gfx::Path* aCachedMotionPath) {
  using nsStyleTransformMatrix::TransformReferenceBox;

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
  const StyleOffsetPath* path = nullptr;
  const StyleLengthPercentage* distance = nullptr;
  const StyleOffsetRotate* offsetRotate = nullptr;
  const StylePositionOrAuto* anchor = nullptr;

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
      case eCSSProperty_offset_path:
        MOZ_ASSERT(!path);
        path = Servo_AnimationValue_GetOffsetPath(value);
        break;
      case eCSSProperty_offset_distance:
        MOZ_ASSERT(!distance);
        distance = Servo_AnimationValue_GetOffsetDistance(value);
        break;
      case eCSSProperty_offset_rotate:
        MOZ_ASSERT(!offsetRotate);
        offsetRotate = Servo_AnimationValue_GetOffsetRotate(value);
        break;
      case eCSSProperty_offset_anchor:
        MOZ_ASSERT(!anchor);
        anchor = Servo_AnimationValue_GetOffsetAnchor(value);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unsupported transform-like property");
    }
  }

  TransformReferenceBox refBox(nullptr, aTransformData.bounds());
  Maybe<ResolvedMotionPathData> motion = MotionPathUtils::ResolveMotionPath(
      path, distance, offsetRotate, anchor, aTransformData.motionPathData(),
      refBox, aCachedMotionPath);

  // We expect all our transform data to arrive in device pixels
  gfx::Point3D transformOrigin = aTransformData.transformOrigin();
  nsDisplayTransform::FrameTransformProperties props(
      translate ? *translate : noneTranslate, rotate ? *rotate : noneRotate,
      scale ? *scale : noneScale, transform ? *transform : noneTransform,
      motion, transformOrigin);

  return nsDisplayTransform::GetResultingTransformMatrix(
      props, refBox, aTransformData.appUnitsPerDevPixel());
}

static uint8_t CollectOverflowedSideLines(const gfxQuad& aPrerenderedQuad,
                                          SideBits aOverflowSides,
                                          gfxLineSegment sideLines[4]) {
  uint8_t count = 0;

  if (aOverflowSides & SideBits::eTop) {
    sideLines[count] = gfxLineSegment(aPrerenderedQuad.mPoints[0],
                                      aPrerenderedQuad.mPoints[1]);
    count++;
  }
  if (aOverflowSides & SideBits::eRight) {
    sideLines[count] = gfxLineSegment(aPrerenderedQuad.mPoints[1],
                                      aPrerenderedQuad.mPoints[2]);
    count++;
  }
  if (aOverflowSides & SideBits::eBottom) {
    sideLines[count] = gfxLineSegment(aPrerenderedQuad.mPoints[2],
                                      aPrerenderedQuad.mPoints[3]);
    count++;
  }
  if (aOverflowSides & SideBits::eLeft) {
    sideLines[count] = gfxLineSegment(aPrerenderedQuad.mPoints[3],
                                      aPrerenderedQuad.mPoints[0]);
    count++;
  }

  return count;
}

enum RegionBits : uint8_t {
  Inside = 0,
  Left = (1 << 0),
  Right = (1 << 1),
  Bottom = (1 << 2),
  Top = (1 << 3),
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(RegionBits);

static RegionBits GetRegionBitsForPoint(double aX, double aY,
                                        const gfxRect& aClip) {
  RegionBits result = RegionBits::Inside;
  if (aX < aClip.X()) {
    result |= RegionBits::Left;
  } else if (aX > aClip.XMost()) {
    result |= RegionBits::Right;
  }

  if (aY < aClip.Y()) {
    result |= RegionBits::Bottom;
  } else if (aY > aClip.YMost()) {
    result |= RegionBits::Top;
  }
  return result;
};

// https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
static bool LineSegmentIntersectsClip(double aX0, double aY0, double aX1,
                                      double aY1, const gfxRect& aClip) {
  RegionBits b0 = GetRegionBitsForPoint(aX0, aY0, aClip);
  RegionBits b1 = GetRegionBitsForPoint(aX1, aY1, aClip);

  while (true) {
    if (!(b0 | b1)) {
      // Completely inside.
      return true;
    }

    if (b0 & b1) {
      // Completely outside.
      return false;
    }

    double x, y;
    // Choose an outside point.
    RegionBits outsidePointBits = b1 > b0 ? b1 : b0;
    if (outsidePointBits & RegionBits::Top) {
      x = aX0 + (aX1 - aX0) * (aClip.YMost() - aY0) / (aY1 - aY0);
      y = aClip.YMost();
    } else if (outsidePointBits & RegionBits::Bottom) {
      x = aX0 + (aX1 - aX0) * (aClip.Y() - aY0) / (aY1 - aY0);
      y = aClip.Y();
    } else if (outsidePointBits & RegionBits::Right) {
      y = aY0 + (aY1 - aY0) * (aClip.XMost() - aX0) / (aX1 - aX0);
      x = aClip.XMost();
    } else if (outsidePointBits & RegionBits::Left) {
      y = aY0 + (aY1 - aY0) * (aClip.X() - aX0) / (aX1 - aX0);
      x = aClip.X();
    }

    if (outsidePointBits == b0) {
      aX0 = x;
      aY0 = y;
      b0 = GetRegionBitsForPoint(aX0, aY0, aClip);
    } else {
      aX1 = x;
      aY1 = y;
      b1 = GetRegionBitsForPoint(aX1, aY1, aClip);
    }
  }
  MOZ_ASSERT_UNREACHABLE();
  return false;
}

// static
bool AnimationHelper::ShouldBeJank(const LayoutDeviceRect& aPrerenderedRect,
                                   SideBits aOverflowSides,
                                   const gfx::Matrix4x4& aTransform,
                                   const ParentLayerRect& aClipRect) {
  if (aClipRect.IsEmpty()) {
    return false;
  }

  gfxQuad prerenderedQuad = gfxUtils::TransformToQuad(
      ThebesRect(aPrerenderedRect.ToUnknownRect()), aTransform);

  gfxLineSegment sideLines[4];
  uint8_t overflowSideCount =
      CollectOverflowedSideLines(prerenderedQuad, aOverflowSides, sideLines);

  gfxRect clipRect = ThebesRect(aClipRect.ToUnknownRect());
  for (uint8_t j = 0; j < overflowSideCount; j++) {
    if (LineSegmentIntersectsClip(sideLines[j].mStart.x, sideLines[j].mStart.y,
                                  sideLines[j].mEnd.x, sideLines[j].mEnd.y,
                                  clipRect)) {
      return true;
    }
  }

  // With step timing functions there are cases the transform jumps to a
  // position where the partial pre-render area is totally outside of the clip
  // rect without any intersection of the partial pre-render area and the clip
  // rect happened in previous compositions but there remains visible area of
  // the entire transformed area.
  //
  // So now all four points of the transformed partial pre-render rect are
  // outside of the clip rect, if all these four points are in either side of
  // the clip rect, we consider it's jank so that on the main-thread we will
  // either a) rebuild the up-to-date display item if there remains visible area
  // or b) no longer rebuild the display item if it's totally outside of the
  // clip rect.
  //
  // Note that RegionBits::Left and Right are mutually exclusive,
  // RegionBits::Top and Bottom are also mutually exclusive, so if there remains
  // any bits, it means all four points are in the same side.
  return GetRegionBitsForPoint(prerenderedQuad.mPoints[0].x,
                               prerenderedQuad.mPoints[0].y, clipRect) &
         GetRegionBitsForPoint(prerenderedQuad.mPoints[1].x,
                               prerenderedQuad.mPoints[1].y, clipRect) &
         GetRegionBitsForPoint(prerenderedQuad.mPoints[2].x,
                               prerenderedQuad.mPoints[2].y, clipRect) &
         GetRegionBitsForPoint(prerenderedQuad.mPoints[3].x,
                               prerenderedQuad.mPoints[3].y, clipRect);
}

}  // namespace layers
}  // namespace mozilla
