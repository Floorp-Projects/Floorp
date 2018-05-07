/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationHelper.h"
#include "mozilla/ComputedTimingFunction.h" // for ComputedTimingFunction
#include "mozilla/dom/AnimationEffectBinding.h" // for dom::FillMode
#include "mozilla/dom/KeyframeEffectBinding.h" // for dom::IterationComposite
#include "mozilla/dom/KeyframeEffect.h" // for dom::KeyFrameEffectReadOnly
#include "mozilla/dom/Nullable.h" // for dom::Nullable
#include "mozilla/layers/CompositorThread.h" // for CompositorThreadHolder
#include "mozilla/layers/LayerAnimationUtils.h" // for TimingFunctionToComputedTimingFunction
#include "mozilla/ServoBindings.h" // for Servo_ComposeAnimationSegment, etc
#include "mozilla/StyleAnimationValue.h" // for StyleAnimationValue, etc
#include "nsDeviceContext.h"            // for AppUnitsPerCSSPixel
#include "nsDisplayList.h"              // for nsDisplayTransform, etc

namespace mozilla {
namespace layers {

void
CompositorAnimationStorage::Clear()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  mAnimatedValues.Clear();
  mAnimations.Clear();
}

void
CompositorAnimationStorage::ClearById(const uint64_t& aId)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  mAnimatedValues.Remove(aId);
  mAnimations.Remove(aId);
}

AnimatedValue*
CompositorAnimationStorage::GetAnimatedValue(const uint64_t& aId) const
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mAnimatedValues.Get(aId);
}

Maybe<float>
CompositorAnimationStorage::GetAnimationOpacity(const uint64_t& aId) const
{
  auto value = GetAnimatedValue(aId);
  if (!value || value->mType != AnimatedValue::OPACITY) {
    return Nothing();
  }

  return Some(value->mOpacity);
}

Maybe<gfx::Matrix4x4>
CompositorAnimationStorage::GetAnimationTransform(const uint64_t& aId) const
{
  auto value = GetAnimatedValue(aId);
  if (!value || value->mType != AnimatedValue::TRANSFORM) {
    return Nothing();
  }

  gfx::Matrix4x4 transform = value->mTransform.mFrameTransform;
  const TransformData& data = value->mTransform.mData;
  float scale = data.appUnitsPerDevPixel();
  gfx::Point3D transformOrigin = data.transformOrigin();

  // Undo the rebasing applied by
  // nsDisplayTransform::GetResultingTransformMatrixInternal
  transform.ChangeBasis(-transformOrigin);

  // Convert to CSS pixels (this undoes the operations performed by
  // nsStyleTransformMatrix::ProcessTranslatePart which is called from
  // nsDisplayTransform::GetResultingTransformMatrix)
  double devPerCss =
    double(scale) / double(nsDeviceContext::AppUnitsPerCSSPixel());
  transform._41 *= devPerCss;
  transform._42 *= devPerCss;
  transform._43 *= devPerCss;

  return Some(transform);
}

void
CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                             gfx::Matrix4x4&& aTransformInDevSpace,
                                             gfx::Matrix4x4&& aFrameTransform,
                                             const TransformData& aData)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  auto count = mAnimatedValues.Count();
  AnimatedValue* value = mAnimatedValues.LookupOrAdd(aId,
                                                     Move(aTransformInDevSpace),
                                                     Move(aFrameTransform),
                                                     aData);
  if (count == mAnimatedValues.Count()) {
    MOZ_ASSERT(value->mType == AnimatedValue::TRANSFORM);
    value->mTransform.mTransformInDevSpace = Move(aTransformInDevSpace);
    value->mTransform.mFrameTransform = Move(aFrameTransform);
    value->mTransform.mData = aData;
  }
}

void
CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                             gfx::Matrix4x4&& aTransformInDevSpace)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  const TransformData dontCare = {};
  SetAnimatedValue(aId,
                   Move(aTransformInDevSpace),
                   Move(gfx::Matrix4x4()),
                   dontCare);
}

void
CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                             const float& aOpacity)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  auto count = mAnimatedValues.Count();
  AnimatedValue* value = mAnimatedValues.LookupOrAdd(aId, aOpacity);
  if (count == mAnimatedValues.Count()) {
    MOZ_ASSERT(value->mType == AnimatedValue::OPACITY);
    value->mOpacity = aOpacity;
  }
}

AnimationArray*
CompositorAnimationStorage::GetAnimations(const uint64_t& aId) const
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  return mAnimations.Get(aId);
}

void
CompositorAnimationStorage::SetAnimations(uint64_t aId, const AnimationArray& aValue)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AnimationArray* value = new AnimationArray(aValue);
  mAnimations.Put(aId, value);
}


AnimationHelper::SampleResult
AnimationHelper::SampleAnimationForEachNode(
  TimeStamp aPreviousFrameTime,
  TimeStamp aCurrentFrameTime,
  AnimationArray& aAnimations,
  InfallibleTArray<AnimData>& aAnimationData,
  RefPtr<RawServoAnimationValue>& aAnimationValue,
  const AnimatedValue* aPreviousValue)
{
  MOZ_ASSERT(!aAnimations.IsEmpty(), "Should be called with animations");

  bool hasInEffectAnimations = false;
#ifdef DEBUG
  // In cases where this function returns a SampleResult::Skipped, we actually
  // do populate aAnimationValue in debug mode, so that we can MOZ_ASSERT at the
  // call site that the value that would have been computed matches the stored
  // value that we end up using. This flag is used to ensure we populate
  // aAnimationValue in this scenario.
  bool shouldBeSkipped = false;
#endif
  // Process in order, since later aAnimations override earlier ones.
  for (size_t i = 0, iEnd = aAnimations.Length(); i < iEnd; ++i) {
    Animation& animation = aAnimations[i];
    AnimData& animData = aAnimationData[i];

    MOZ_ASSERT((!animation.originTime().IsNull() &&
                animation.startTime().type() ==
                  MaybeTimeDuration::TTimeDuration) ||
               animation.isNotPlaying(),
               "If we are playing, we should have an origin time and a start"
               " time");

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
    if (!aPreviousValue &&
        !animation.isNotPlaying() &&
        !aPreviousFrameTime.IsNull()) {
      // This is the inverse of the calculation performed in
      // AnimationInfo::StartPendingAnimations to calculate the start time of
      // play-pending animations.
      // Note that we have to calculate (TimeStamp + TimeDuration) last to avoid
      // underflow in the middle of the calulation.
      const TimeStamp readyTime =
        animation.originTime() +
        (animation.startTime().get_TimeDuration() +
         animation.holdTime().MultDouble(1.0 / animation.playbackRate()));
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
      aPreviousFrameTime.IsNull() || hasFutureReadyTime
      ? aCurrentFrameTime
      : aPreviousFrameTime;

    // If the animation is not currently playing, e.g. paused or
    // finished, then use the hold time to stay at the same position.
    TimeDuration elapsedDuration =
      animation.isNotPlaying() ||
      animation.startTime().type() != MaybeTimeDuration::TTimeDuration
      ? animation.holdTime()
      : (timeStamp - animation.originTime() -
         animation.startTime().get_TimeDuration())
        .MultDouble(animation.playbackRate());

    ComputedTiming computedTiming =
      dom::AnimationEffect::GetComputedTimingAt(
        dom::Nullable<TimeDuration>(elapsedDuration), animData.mTiming,
        animation.playbackRate());

    if (computedTiming.mProgress.IsNull()) {
      continue;
    }

    dom::IterationCompositeOperation iterCompositeOperation =
        static_cast<dom::IterationCompositeOperation>(
          animation.iterationComposite());

    // Skip caluculation if the progress hasn't changed since the last
    // calculation.
    // Note that we don't skip calculate this animation if there is another
    // animation since the other animation might be 'accumulate' or 'add', or
    // might have a missing keyframe (i.e. this animation value will be used in
    // the missing keyframe).
    // FIXME Bug 1455476: We should do this optimizations for the case where
    // the layer has multiple animations.
    if (iEnd == 1 &&
        !dom::KeyframeEffect::HasComputedTimingChanged(
          computedTiming,
          iterCompositeOperation,
          animData.mProgressOnLastCompose,
          animData.mCurrentIterationOnLastCompose)) {
#ifdef DEBUG
      shouldBeSkipped = true;
#else
      return SampleResult::Skipped;
#endif
    }

    uint32_t segmentIndex = 0;
    size_t segmentSize = animation.segments().Length();
    AnimationSegment* segment = animation.segments().Elements();
    while (segment->endPortion() < computedTiming.mProgress.Value() &&
           segmentIndex < segmentSize - 1) {
      ++segment;
      ++segmentIndex;
    }

    double positionInSegment =
      (computedTiming.mProgress.Value() - segment->startPortion()) /
      (segment->endPortion() - segment->startPortion());

    double portion =
      ComputedTimingFunction::GetPortion(animData.mFunctions[segmentIndex],
                                         positionInSegment,
                                     computedTiming.mBeforeFlag);

    // Like above optimization, skip caluculation if the target segment isn't
    // changed and if the portion in the segment isn't changed.
    // This optimization is needed for CSS animations/transitions with step
    // timing functions (e.g. the throbber animation on tab or frame based
    // animations).
    // FIXME Bug 1455476: Like the above optimization, we should apply this
    // optimizations for multiple animation cases as well.
    if (iEnd == 1 &&
        animData.mSegmentIndexOnLastCompose == segmentIndex &&
        !animData.mPortionInSegmentOnLastCompose.IsNull() &&
        animData.mPortionInSegmentOnLastCompose.Value() == portion) {
#ifdef DEBUG
      shouldBeSkipped = true;
#else
      return SampleResult::Skipped;
#endif
    }

    AnimationPropertySegment animSegment;
    animSegment.mFromKey = 0.0;
    animSegment.mToKey = 1.0;
    animSegment.mFromValue =
      AnimationValue(animData.mStartValues[segmentIndex]);
    animSegment.mToValue =
      AnimationValue(animData.mEndValues[segmentIndex]);
    animSegment.mFromComposite =
      static_cast<dom::CompositeOperation>(segment->startComposite());
    animSegment.mToComposite =
      static_cast<dom::CompositeOperation>(segment->endComposite());

    // interpolate the property
    aAnimationValue =
      Servo_ComposeAnimationSegment(
        &animSegment,
        aAnimationValue,
        animData.mEndValues.LastElement(),
        iterCompositeOperation,
        portion,
        computedTiming.mCurrentIteration).Consume();

#ifdef DEBUG
    if (shouldBeSkipped) {
      return SampleResult::Skipped;
    }
#endif

    hasInEffectAnimations = true;
    animData.mProgressOnLastCompose = computedTiming.mProgress;
    animData.mCurrentIterationOnLastCompose = computedTiming.mCurrentIteration;
    animData.mSegmentIndexOnLastCompose = segmentIndex;
    animData.mPortionInSegmentOnLastCompose.SetValue(portion);
  }

#ifdef DEBUG
  // Sanity check that all of animation data are the same.
  const AnimationData& lastData = aAnimations.LastElement().data();
  for (const Animation& animation : aAnimations) {
    const AnimationData& data = animation.data();
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

struct BogusAnimation {};

static inline Result<Ok, BogusAnimation>
SetCSSAngle(const CSSAngle& aAngle, nsCSSValue& aValue)
{
  aValue.SetFloatValue(aAngle.value(), nsCSSUnit(aAngle.unit()));
  if (!aValue.IsAngularUnit()) {
    NS_ERROR("Bogus animation from IPC");
    return Err(BogusAnimation { });
  }
  return Ok();
}

static Result<nsCSSValueSharedList*, BogusAnimation>
CreateCSSValueList(const InfallibleTArray<TransformFunction>& aFunctions)
{
  nsAutoPtr<nsCSSValueList> result;
  nsCSSValueList** resultTail = getter_Transfers(result);
  for (uint32_t i = 0; i < aFunctions.Length(); i++) {
    RefPtr<nsCSSValue::Array> arr;
    switch (aFunctions[i].type()) {
      case TransformFunction::TRotationX:
      {
        const CSSAngle& angle = aFunctions[i].get_RotationX().angle();
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_rotatex,
                                                      resultTail);
        MOZ_TRY(SetCSSAngle(angle, arr->Item(1)));
        break;
      }
      case TransformFunction::TRotationY:
      {
        const CSSAngle& angle = aFunctions[i].get_RotationY().angle();
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_rotatey,
                                                      resultTail);
        MOZ_TRY(SetCSSAngle(angle, arr->Item(1)));
        break;
      }
      case TransformFunction::TRotationZ:
      {
        const CSSAngle& angle = aFunctions[i].get_RotationZ().angle();
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_rotatez,
                                                      resultTail);
        MOZ_TRY(SetCSSAngle(angle, arr->Item(1)));
        break;
      }
      case TransformFunction::TRotation:
      {
        const CSSAngle& angle = aFunctions[i].get_Rotation().angle();
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_rotate,
                                                      resultTail);
        MOZ_TRY(SetCSSAngle(angle, arr->Item(1)));
        break;
      }
      case TransformFunction::TRotation3D:
      {
        float x = aFunctions[i].get_Rotation3D().x();
        float y = aFunctions[i].get_Rotation3D().y();
        float z = aFunctions[i].get_Rotation3D().z();
        const CSSAngle& angle = aFunctions[i].get_Rotation3D().angle();
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_rotate3d,
                                                      resultTail);
        arr->Item(1).SetFloatValue(x, eCSSUnit_Number);
        arr->Item(2).SetFloatValue(y, eCSSUnit_Number);
        arr->Item(3).SetFloatValue(z, eCSSUnit_Number);
        MOZ_TRY(SetCSSAngle(angle, arr->Item(4)));
        break;
      }
      case TransformFunction::TScale:
      {
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_scale3d,
                                                      resultTail);
        arr->Item(1).SetFloatValue(aFunctions[i].get_Scale().x(), eCSSUnit_Number);
        arr->Item(2).SetFloatValue(aFunctions[i].get_Scale().y(), eCSSUnit_Number);
        arr->Item(3).SetFloatValue(aFunctions[i].get_Scale().z(), eCSSUnit_Number);
        break;
      }
      case TransformFunction::TTranslation:
      {
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_translate3d,
                                                      resultTail);
        arr->Item(1).SetFloatValue(aFunctions[i].get_Translation().x(), eCSSUnit_Pixel);
        arr->Item(2).SetFloatValue(aFunctions[i].get_Translation().y(), eCSSUnit_Pixel);
        arr->Item(3).SetFloatValue(aFunctions[i].get_Translation().z(), eCSSUnit_Pixel);
        break;
      }
      case TransformFunction::TSkewX:
      {
        const CSSAngle& x = aFunctions[i].get_SkewX().x();
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_skewx,
                                                      resultTail);
        MOZ_TRY(SetCSSAngle(x, arr->Item(1)));
        break;
      }
      case TransformFunction::TSkewY:
      {
        const CSSAngle& y = aFunctions[i].get_SkewY().y();
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_skewy,
                                                      resultTail);
        MOZ_TRY(SetCSSAngle(y, arr->Item(1)));
        break;
      }
      case TransformFunction::TSkew:
      {
        const CSSAngle& x = aFunctions[i].get_Skew().x();
        const CSSAngle& y = aFunctions[i].get_Skew().y();
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_skew,
                                                      resultTail);
        MOZ_TRY(SetCSSAngle(x, arr->Item(1)));
        MOZ_TRY(SetCSSAngle(y, arr->Item(2)));
        break;
      }
      case TransformFunction::TTransformMatrix:
      {
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_matrix3d,
                                                      resultTail);
        const gfx::Matrix4x4& matrix = aFunctions[i].get_TransformMatrix().value();
        arr->Item(1).SetFloatValue(matrix._11, eCSSUnit_Number);
        arr->Item(2).SetFloatValue(matrix._12, eCSSUnit_Number);
        arr->Item(3).SetFloatValue(matrix._13, eCSSUnit_Number);
        arr->Item(4).SetFloatValue(matrix._14, eCSSUnit_Number);
        arr->Item(5).SetFloatValue(matrix._21, eCSSUnit_Number);
        arr->Item(6).SetFloatValue(matrix._22, eCSSUnit_Number);
        arr->Item(7).SetFloatValue(matrix._23, eCSSUnit_Number);
        arr->Item(8).SetFloatValue(matrix._24, eCSSUnit_Number);
        arr->Item(9).SetFloatValue(matrix._31, eCSSUnit_Number);
        arr->Item(10).SetFloatValue(matrix._32, eCSSUnit_Number);
        arr->Item(11).SetFloatValue(matrix._33, eCSSUnit_Number);
        arr->Item(12).SetFloatValue(matrix._34, eCSSUnit_Number);
        arr->Item(13).SetFloatValue(matrix._41, eCSSUnit_Number);
        arr->Item(14).SetFloatValue(matrix._42, eCSSUnit_Number);
        arr->Item(15).SetFloatValue(matrix._43, eCSSUnit_Number);
        arr->Item(16).SetFloatValue(matrix._44, eCSSUnit_Number);
        break;
      }
      case TransformFunction::TPerspective:
      {
        float perspective = aFunctions[i].get_Perspective().value();
        arr = AnimationValue::AppendTransformFunction(eCSSKeyword_perspective,
                                                      resultTail);
        arr->Item(1).SetFloatValue(perspective, eCSSUnit_Pixel);
        break;
      }
      default:
        NS_ASSERTION(false, "All functions should be implemented?");
    }
  }
  if (aFunctions.Length() == 0) {
    result = new nsCSSValueList();
    result->mValue.SetNoneValue();
  }
  return new nsCSSValueSharedList(result.forget());
}

static already_AddRefed<RawServoAnimationValue>
ToAnimationValue(const Animatable& aAnimatable)
{
  RefPtr<RawServoAnimationValue> result;

  switch (aAnimatable.type()) {
    case Animatable::Tnull_t:
      break;
    case Animatable::TArrayOfTransformFunction: {
      const InfallibleTArray<TransformFunction>& transforms =
        aAnimatable.get_ArrayOfTransformFunction();
      auto listOrError = CreateCSSValueList(transforms);
      if (listOrError.isOk()) {
        RefPtr<nsCSSValueSharedList> list = listOrError.unwrap();
        MOZ_ASSERT(list, "Transform list should be non null");
        result = Servo_AnimationValue_Transform(*list).Consume();
      }
      break;
    }
    case Animatable::Tfloat:
      result = Servo_AnimationValue_Opacity(aAnimatable.get_float()).Consume();
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported type");
  }
  return result.forget();
}

void
AnimationHelper::SetAnimations(
  AnimationArray& aAnimations,
  InfallibleTArray<AnimData>& aAnimData,
  RefPtr<RawServoAnimationValue>& aBaseAnimationStyle)
{
  for (uint32_t i = 0; i < aAnimations.Length(); i++) {
    Animation& animation = aAnimations[i];
    // Adjust fill mode to fill forwards so that if the main thread is delayed
    // in clearing this animation we don't introduce flicker by jumping back to
    // the old underlying value
    switch (static_cast<dom::FillMode>(animation.fillMode())) {
      case dom::FillMode::None:
        animation.fillMode() = static_cast<uint8_t>(dom::FillMode::Forwards);
        break;
      case dom::FillMode::Backwards:
        animation.fillMode() = static_cast<uint8_t>(dom::FillMode::Both);
        break;
      default:
        break;
    }

    if (animation.baseStyle().type() != Animatable::Tnull_t) {
      aBaseAnimationStyle = ToAnimationValue(animation.baseStyle());
    }

    AnimData* data = aAnimData.AppendElement();

    data->mTiming = TimingParams {
      animation.duration(),
      animation.delay(),
      animation.endDelay(),
      animation.iterations(),
      animation.iterationStart(),
      static_cast<dom::PlaybackDirection>(animation.direction()),
      static_cast<dom::FillMode>(animation.fillMode()),
      Move(AnimationUtils::TimingFunctionToComputedTimingFunction(
           animation.easingFunction()))
    };
    InfallibleTArray<Maybe<ComputedTimingFunction>>& functions =
      data->mFunctions;
    InfallibleTArray<RefPtr<RawServoAnimationValue>>& startValues =
      data->mStartValues;
    InfallibleTArray<RefPtr<RawServoAnimationValue>>& endValues =
      data->mEndValues;

    const InfallibleTArray<AnimationSegment>& segments = animation.segments();
    for (const AnimationSegment& segment : segments) {
      startValues.AppendElement(ToAnimationValue(segment.startState()));
      endValues.AppendElement(ToAnimationValue(segment.endState()));

      TimingFunction tf = segment.sampleFn();
      Maybe<ComputedTimingFunction> ctf =
        AnimationUtils::TimingFunctionToComputedTimingFunction(tf);
      functions.AppendElement(ctf);
    }
  }
}

uint64_t
AnimationHelper::GetNextCompositorAnimationsId()
{
  static uint32_t sNextId = 0;
  ++sNextId;

  uint32_t procId = static_cast<uint32_t>(base::GetCurrentProcId());
  uint64_t nextId = procId;
  nextId = nextId << 32 | sNextId;
  return nextId;
}

bool
AnimationHelper::SampleAnimations(CompositorAnimationStorage* aStorage,
                                  TimeStamp aPreviousFrameTime,
                                  TimeStamp aCurrentFrameTime)
{
  MOZ_ASSERT(aStorage);
  bool isAnimating = false;

  // Do nothing if there are no compositor animations
  if (!aStorage->AnimationsCount()) {
    return isAnimating;
  }

  //Sample the animations in CompositorAnimationStorage
  for (auto iter = aStorage->ConstAnimationsTableIter();
       !iter.Done(); iter.Next()) {
    AnimationArray* animations = iter.UserData();
    if (animations->IsEmpty()) {
      continue;
    }

    isAnimating = true;
    RefPtr<RawServoAnimationValue> animationValue;
    InfallibleTArray<AnimData> animationData;
    AnimationHelper::SetAnimations(*animations,
                                   animationData,
                                   animationValue);
    AnimatedValue* previousValue = aStorage->GetAnimatedValue(iter.Key());
    AnimationHelper::SampleResult sampleResult =
      AnimationHelper::SampleAnimationForEachNode(aPreviousFrameTime,
                                                  aCurrentFrameTime,
                                                  *animations,
                                                  animationData,
                                                  animationValue,
                                                  previousValue);

    if (sampleResult != AnimationHelper::SampleResult::Sampled) {
      continue;
    }

    // Store the AnimatedValue
    Animation& animation = animations->LastElement();
    switch (animation.property()) {
      case eCSSProperty_opacity: {
        aStorage->SetAnimatedValue(
          iter.Key(),
          Servo_AnimationValue_GetOpacity(animationValue));
        break;
      }
      case eCSSProperty_transform: {
        RefPtr<nsCSSValueSharedList> list;
        Servo_AnimationValue_GetTransform(animationValue, &list);
        const TransformData& transformData = animation.data().get_TransformData();
        nsPoint origin = transformData.origin();
        // we expect all our transform data to arrive in device pixels
        gfx::Point3D transformOrigin = transformData.transformOrigin();
        nsDisplayTransform::FrameTransformProperties props(Move(list),
                                                           transformOrigin);

        gfx::Matrix4x4 transform =
          nsDisplayTransform::GetResultingTransformMatrix(props, origin,
                                                          transformData.appUnitsPerDevPixel(),
                                                          0, &transformData.bounds());
        gfx::Matrix4x4 frameTransform = transform;
        // If the parent has perspective transform, then the offset into reference
        // frame coordinates is already on this transform. If not, then we need to ask
        // for it to be added here.
        if (!transformData.hasPerspectiveParent()) {
           nsLayoutUtils::PostTranslate(transform, origin,
                                        transformData.appUnitsPerDevPixel(),
                                        true);
        }

        transform.PostScale(transformData.inheritedXScale(),
                            transformData.inheritedYScale(),
                            1);

        aStorage->SetAnimatedValue(iter.Key(),
                                   Move(transform), Move(frameTransform),
                                   transformData);
        break;
      }
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled animated property");
    }
  }

  return isAnimating;
}

} // namespace layers
} // namespace mozilla
