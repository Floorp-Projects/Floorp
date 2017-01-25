/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationHelper.h"
#include "mozilla/ComputedTimingFunction.h" // for ComputedTimingFunction
#include "mozilla/dom/AnimationEffectReadOnlyBinding.h" // for FillMode
#include "mozilla/layers/LayerAnimationUtils.h"  // for TimingFunctionToComputedTimingFunction
#include "mozilla/layers/LayersMessages.h"  // for TransformFunction, etc
#include "mozilla/StyleAnimationValue.h" // for StyleAnimationValue, etc

namespace mozilla {
namespace layers {

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

} // namespace layers
} // namespace mozilla
