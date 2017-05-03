/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationHelper.h"
#include "mozilla/ComputedTimingFunction.h" // for ComputedTimingFunction
#include "mozilla/dom/AnimationEffectReadOnlyBinding.h" // for dom::FillMode
#include "mozilla/dom/KeyframeEffectBinding.h" // for dom::IterationComposite
#include "mozilla/dom/KeyframeEffectReadOnly.h" // for dom::KeyFrameEffectReadOnly
#include "mozilla/layers/CompositorThread.h" // for CompositorThreadHolder
#include "mozilla/layers/LayerAnimationUtils.h" // for TimingFunctionToComputedTimingFunction
#include "mozilla/StyleAnimationValue.h" // for StyleAnimationValue, etc
#include "nsDisplayList.h"              // for nsDisplayTransform, etc

namespace mozilla {
namespace layers {

struct StyleAnimationValueCompositePair {
  StyleAnimationValue mValue;
  dom::CompositeOperation mComposite;
};

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

void
CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                             gfx::Matrix4x4&& aTransformInDevSpace,
                                             gfx::Matrix4x4&& aFrameTransform,
                                             const TransformData& aData)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AnimatedValue* value = new AnimatedValue(Move(aTransformInDevSpace), Move(aFrameTransform), aData);
  mAnimatedValues.Put(aId, value);
}

void
CompositorAnimationStorage::SetAnimatedValue(uint64_t aId,
                                             const float& aOpacity)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AnimatedValue* value = new AnimatedValue(aOpacity);
  mAnimatedValues.Put(aId, value);
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

static StyleAnimationValue
SampleValue(float aPortion, const layers::Animation& aAnimation,
            const StyleAnimationValueCompositePair& aStart,
            const StyleAnimationValueCompositePair& aEnd,
            const StyleAnimationValue& aLastValue,
            uint64_t aCurrentIteration,
            const StyleAnimationValue& aUnderlyingValue)
{
  NS_ASSERTION(aStart.mValue.IsNull() || aEnd.mValue.IsNull() ||
               aStart.mValue.GetUnit() == aEnd.mValue.GetUnit(),
               "Must have same unit");

  StyleAnimationValue startValue =
    dom::KeyframeEffectReadOnly::CompositeValue(aAnimation.property(),
                                                aStart.mValue,
                                                aUnderlyingValue,
                                                aStart.mComposite);
  StyleAnimationValue endValue =
    dom::KeyframeEffectReadOnly::CompositeValue(aAnimation.property(),
                                                aEnd.mValue,
                                                aUnderlyingValue,
                                                aEnd.mComposite);

  // Iteration composition for accumulate
  if (static_cast<dom::IterationCompositeOperation>
        (aAnimation.iterationComposite()) ==
          dom::IterationCompositeOperation::Accumulate &&
      aCurrentIteration > 0) {
    // FIXME: Bug 1293492: Add a utility function to calculate both of
    // below StyleAnimationValues.
    startValue =
      StyleAnimationValue::Accumulate(aAnimation.property(),
                                      aLastValue.IsNull()
                                        ? aUnderlyingValue
                                        : aLastValue,
                                      Move(startValue),
                                      aCurrentIteration);
    endValue =
      StyleAnimationValue::Accumulate(aAnimation.property(),
                                      aLastValue.IsNull()
                                        ? aUnderlyingValue
                                        : aLastValue,
                                      Move(endValue),
                                      aCurrentIteration);
  }

  StyleAnimationValue interpolatedValue;
  // This should never fail because we only pass transform and opacity values
  // to the compositor and they should never fail to interpolate.
  DebugOnly<bool> uncomputeResult =
    StyleAnimationValue::Interpolate(aAnimation.property(),
                                     startValue, endValue,
                                     aPortion, interpolatedValue);
  MOZ_ASSERT(uncomputeResult, "could not uncompute value");
  return interpolatedValue;
}

bool
AnimationHelper::SampleAnimationForEachNode(TimeStamp aTime,
                           AnimationArray& aAnimations,
                           InfallibleTArray<AnimData>& aAnimationData,
                           StyleAnimationValue& aAnimationValue,
                           bool& aHasInEffectAnimations)
{
  bool activeAnimations = false;

  if (aAnimations.IsEmpty()) {
    return activeAnimations;
  }

  // Process in order, since later aAnimations override earlier ones.
  for (size_t i = 0, iEnd = aAnimations.Length(); i < iEnd; ++i) {
    Animation& animation = aAnimations[i];
    AnimData& animData = aAnimationData[i];

    activeAnimations = true;

    MOZ_ASSERT((!animation.originTime().IsNull() &&
                animation.startTime().type() != MaybeTimeDuration::Tnull_t) ||
               animation.isNotPlaying(),
               "If we are playing, we should have an origin time and a start"
               " time");
    // If the animation is not currently playing , e.g. paused or
    // finished, then use the hold time to stay at the same position.
    TimeDuration elapsedDuration = animation.isNotPlaying()
      ? animation.holdTime()
      : (aTime - animation.originTime() -
         animation.startTime().get_TimeDuration())
        .MultDouble(animation.playbackRate());
    TimingParams timing;
    timing.mDuration.emplace(animation.duration());
    timing.mDelay = animation.delay();
    timing.mEndDelay = animation.endDelay();
    timing.mIterations = animation.iterations();
    timing.mIterationStart = animation.iterationStart();
    timing.mDirection =
      static_cast<dom::PlaybackDirection>(animation.direction());
    timing.mFill = static_cast<dom::FillMode>(animation.fillMode());
    timing.mFunction =
      AnimationUtils::TimingFunctionToComputedTimingFunction(
        animation.easingFunction());

    ComputedTiming computedTiming =
      dom::AnimationEffectReadOnly::GetComputedTimingAt(
        Nullable<TimeDuration>(elapsedDuration), timing,
        animation.playbackRate());

    if (computedTiming.mProgress.IsNull()) {
      continue;
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

    StyleAnimationValueCompositePair from {
      animData.mStartValues[segmentIndex],
      static_cast<dom::CompositeOperation>(segment->startComposite())
    };
    StyleAnimationValueCompositePair to {
      animData.mEndValues[segmentIndex],
      static_cast<dom::CompositeOperation>(segment->endComposite())
    };
    // interpolate the property
    aAnimationValue = SampleValue(portion,
                                 animation,
                                 from, to,
                                 animData.mEndValues.LastElement(),
                                 computedTiming.mCurrentIteration,
                                 aAnimationValue);
    aHasInEffectAnimations = true;
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
  return activeAnimations;
}

static inline void
SetCSSAngle(const CSSAngle& aAngle, nsCSSValue& aValue)
{
  aValue.SetFloatValue(aAngle.value(), nsCSSUnit(aAngle.unit()));
}

static nsCSSValueSharedList*
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
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotatex,
                                                           resultTail);
        SetCSSAngle(angle, arr->Item(1));
        break;
      }
      case TransformFunction::TRotationY:
      {
        const CSSAngle& angle = aFunctions[i].get_RotationY().angle();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotatey,
                                                           resultTail);
        SetCSSAngle(angle, arr->Item(1));
        break;
      }
      case TransformFunction::TRotationZ:
      {
        const CSSAngle& angle = aFunctions[i].get_RotationZ().angle();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotatez,
                                                           resultTail);
        SetCSSAngle(angle, arr->Item(1));
        break;
      }
      case TransformFunction::TRotation:
      {
        const CSSAngle& angle = aFunctions[i].get_Rotation().angle();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotate,
                                                           resultTail);
        SetCSSAngle(angle, arr->Item(1));
        break;
      }
      case TransformFunction::TRotation3D:
      {
        float x = aFunctions[i].get_Rotation3D().x();
        float y = aFunctions[i].get_Rotation3D().y();
        float z = aFunctions[i].get_Rotation3D().z();
        const CSSAngle& angle = aFunctions[i].get_Rotation3D().angle();
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotate3d,
                                                       resultTail);
        arr->Item(1).SetFloatValue(x, eCSSUnit_Number);
        arr->Item(2).SetFloatValue(y, eCSSUnit_Number);
        arr->Item(3).SetFloatValue(z, eCSSUnit_Number);
        SetCSSAngle(angle, arr->Item(4));
        break;
      }
      case TransformFunction::TScale:
      {
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_scale3d,
                                                       resultTail);
        arr->Item(1).SetFloatValue(aFunctions[i].get_Scale().x(), eCSSUnit_Number);
        arr->Item(2).SetFloatValue(aFunctions[i].get_Scale().y(), eCSSUnit_Number);
        arr->Item(3).SetFloatValue(aFunctions[i].get_Scale().z(), eCSSUnit_Number);
        break;
      }
      case TransformFunction::TTranslation:
      {
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_translate3d,
                                                       resultTail);
        arr->Item(1).SetFloatValue(aFunctions[i].get_Translation().x(), eCSSUnit_Pixel);
        arr->Item(2).SetFloatValue(aFunctions[i].get_Translation().y(), eCSSUnit_Pixel);
        arr->Item(3).SetFloatValue(aFunctions[i].get_Translation().z(), eCSSUnit_Pixel);
        break;
      }
      case TransformFunction::TSkewX:
      {
        const CSSAngle& x = aFunctions[i].get_SkewX().x();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_skewx,
                                                           resultTail);
        SetCSSAngle(x, arr->Item(1));
        break;
      }
      case TransformFunction::TSkewY:
      {
        const CSSAngle& y = aFunctions[i].get_SkewY().y();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_skewy,
                                                           resultTail);
        SetCSSAngle(y, arr->Item(1));
        break;
      }
      case TransformFunction::TSkew:
      {
        const CSSAngle& x = aFunctions[i].get_Skew().x();
        const CSSAngle& y = aFunctions[i].get_Skew().y();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_skew,
                                                           resultTail);
        SetCSSAngle(x, arr->Item(1));
        SetCSSAngle(y, arr->Item(2));
        break;
      }
      case TransformFunction::TTransformMatrix:
      {
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_matrix3d,
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
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_perspective,
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

static StyleAnimationValue
ToStyleAnimationValue(const Animatable& aAnimatable)
{
  StyleAnimationValue result;

  switch (aAnimatable.type()) {
    case Animatable::Tnull_t:
      break;
    case Animatable::TArrayOfTransformFunction: {
      const InfallibleTArray<TransformFunction>& transforms =
        aAnimatable.get_ArrayOfTransformFunction();
      result.SetTransformValue(CreateCSSValueList(transforms));
      break;
    }
    case Animatable::Tfloat:
      result.SetFloatValue(aAnimatable.get_float());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported type");
  }

  return result;
}

void
AnimationHelper::SetAnimations(AnimationArray& aAnimations,
                               InfallibleTArray<AnimData>& aAnimData,
                               StyleAnimationValue& aBaseAnimationStyle)
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
      aBaseAnimationStyle = ToStyleAnimationValue(animation.baseStyle());
    }

    AnimData* data = aAnimData.AppendElement();
    InfallibleTArray<Maybe<ComputedTimingFunction>>& functions =
      data->mFunctions;
    const InfallibleTArray<AnimationSegment>& segments = animation.segments();
    for (uint32_t j = 0; j < segments.Length(); j++) {
      TimingFunction tf = segments.ElementAt(j).sampleFn();

      Maybe<ComputedTimingFunction> ctf =
        AnimationUtils::TimingFunctionToComputedTimingFunction(tf);
      functions.AppendElement(ctf);
    }

    // Precompute the StyleAnimationValues that we need if this is a transform
    // animation.
    InfallibleTArray<StyleAnimationValue>& startValues = data->mStartValues;
    InfallibleTArray<StyleAnimationValue>& endValues = data->mEndValues;
    for (const AnimationSegment& segment : segments) {
      startValues.AppendElement(ToStyleAnimationValue(segment.startState()));
      endValues.AppendElement(ToStyleAnimationValue(segment.endState()));
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

void
AnimationHelper::SampleAnimations(CompositorAnimationStorage* aStorage,
                                  TimeStamp aTime)
{
  MOZ_ASSERT(aStorage);

  // Do nothing if there are no compositor animations
  if (!aStorage->AnimationsCount()) {
    return;
  }

  //Sample the animations in CompositorAnimationStorage
  for (auto iter = aStorage->ConstAnimationsTableIter();
       !iter.Done(); iter.Next()) {
    bool hasInEffectAnimations = false;
    AnimationArray* animations = iter.UserData();
    StyleAnimationValue animationValue;
    InfallibleTArray<AnimData> animationData;
    AnimationHelper::SetAnimations(*animations,
                                   animationData,
                                   animationValue);
    AnimationHelper::SampleAnimationForEachNode(aTime,
                                                *animations,
                                                animationData,
                                                animationValue,
                                                hasInEffectAnimations);

    if (!hasInEffectAnimations) {
      continue;
    }

    // Store the AnimatedValue
    Animation& animation = animations->LastElement();
    switch (animation.property()) {
      case eCSSProperty_opacity: {
        aStorage->SetAnimatedValue(iter.Key(),
                                   animationValue.GetFloatValue());
        break;
      }
      case eCSSProperty_transform: {
        nsCSSValueSharedList* list = animationValue.GetCSSValueSharedListValue();
        const TransformData& transformData = animation.data().get_TransformData();
        nsPoint origin = transformData.origin();
        // we expect all our transform data to arrive in device pixels
        gfx::Point3D transformOrigin = transformData.transformOrigin();
        nsDisplayTransform::FrameTransformProperties props(list,
                                                           transformOrigin);

        gfx::Matrix4x4 transform =
          nsDisplayTransform::GetResultingTransformMatrix(props, origin,
                                                          transformData.appUnitsPerDevPixel(),
                                                          0, &transformData.bounds());
        gfx::Matrix4x4 frameTransform = transform;

        //TODO how do we support this without layer information
        // If our parent layer is a perspective layer, then the offset into reference
        // frame coordinates is already on that layer. If not, then we need to ask
        // for it to be added here.
        // if (!aLayer->GetParent() ||
        //     !aLayer->GetParent()->GetTransformIsPerspective()) {
        //   nsLayoutUtils::PostTranslate(transform, origin,
        //                                transformData.appUnitsPerDevPixel(),
        //                                true);
        // }

        // if (ContainerLayer* c = aLayer->AsContainerLayer()) {
        //   transform.PostScale(c->GetInheritedXScale(), c->GetInheritedYScale(), 1);
        // }

        aStorage->SetAnimatedValue(iter.Key(),
                                   Move(transform), Move(frameTransform),
                                   transformData);
        break;
      }
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled animated property");
    }
  }
}

} // namespace layers
} // namespace mozilla
