/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utilities for animation of computed style values */

#include "mozilla/StyleAnimationValue.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "mozilla/ComputedStyle.h"
#include "nsComputedDOMStyle.h"
#include "nsCSSPseudoElements.h"
#include "mozilla/dom/Element.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"
#include "mozilla/ServoBindings.h"  // RawServoDeclarationBlock
#include "mozilla/ServoCSSParser.h"
#include "gfxMatrix.h"
#include "gfxQuaternion.h"
#include "mozilla/dom/Document.h"
#include "nsIFrame.h"
#include "gfx2DGlue.h"
#include "mozilla/ComputedStyleInlines.h"
#include "mozilla/layers/LayersMessages.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using nsStyleTransformMatrix::Decompose2DMatrix;
using nsStyleTransformMatrix::Decompose3DMatrix;
using nsStyleTransformMatrix::ShearType;

static already_AddRefed<nsCSSValue::Array> AppendFunction(
    nsCSSKeyword aTransformFunction) {
  uint32_t nargs;
  switch (aTransformFunction) {
    case eCSSKeyword_matrix3d:
      nargs = 16;
      break;
    case eCSSKeyword_matrix:
      nargs = 6;
      break;
    case eCSSKeyword_rotate3d:
      nargs = 4;
      break;
    case eCSSKeyword_interpolatematrix:
    case eCSSKeyword_accumulatematrix:
    case eCSSKeyword_translate3d:
    case eCSSKeyword_scale3d:
      nargs = 3;
      break;
    case eCSSKeyword_translate:
    case eCSSKeyword_skew:
    case eCSSKeyword_scale:
      nargs = 2;
      break;
    default:
      NS_ERROR("must be a transform function");
      MOZ_FALLTHROUGH;
    case eCSSKeyword_translatex:
    case eCSSKeyword_translatey:
    case eCSSKeyword_translatez:
    case eCSSKeyword_scalex:
    case eCSSKeyword_scaley:
    case eCSSKeyword_scalez:
    case eCSSKeyword_skewx:
    case eCSSKeyword_skewy:
    case eCSSKeyword_rotate:
    case eCSSKeyword_rotatex:
    case eCSSKeyword_rotatey:
    case eCSSKeyword_rotatez:
    case eCSSKeyword_perspective:
      nargs = 1;
      break;
  }

  RefPtr<nsCSSValue::Array> arr = nsCSSValue::Array::Create(nargs + 1);
  arr->Item(0).SetIntValue(aTransformFunction, eCSSUnit_Enumerated);

  return arr.forget();
}

static already_AddRefed<nsCSSValue::Array> AppendTransformFunction(
    nsCSSKeyword aTransformFunction, nsCSSValueList**& aListTail) {
  RefPtr<nsCSSValue::Array> arr = AppendFunction(aTransformFunction);
  nsCSSValueList* item = new nsCSSValueList;
  item->mValue.SetArrayValue(arr, eCSSUnit_Function);

  *aListTail = item;
  aListTail = &item->mNext;

  return arr.forget();
}

struct BogusAnimation {};

static inline Result<Ok, BogusAnimation> SetCSSAngle(
    const layers::CSSAngle& aAngle, nsCSSValue& aValue) {
  aValue.SetFloatValue(aAngle.value(), nsCSSUnit(aAngle.unit()));
  if (!aValue.IsAngularUnit()) {
    NS_ERROR("Bogus animation from IPC");
    return Err(BogusAnimation{});
  }
  return Ok();
}

static Result<nsCSSValueSharedList*, BogusAnimation> CreateCSSValueList(
    const InfallibleTArray<layers::TransformFunction>& aFunctions) {
  nsAutoPtr<nsCSSValueList> result;
  nsCSSValueList** resultTail = getter_Transfers(result);
  for (const layers::TransformFunction& function : aFunctions) {
    RefPtr<nsCSSValue::Array> arr;
    switch (function.type()) {
      case layers::TransformFunction::TRotationX: {
        const layers::CSSAngle& angle = function.get_RotationX().angle();
        arr = AppendTransformFunction(eCSSKeyword_rotatex, resultTail);
        MOZ_TRY(SetCSSAngle(angle, arr->Item(1)));
        break;
      }
      case layers::TransformFunction::TRotationY: {
        const layers::CSSAngle& angle = function.get_RotationY().angle();
        arr = AppendTransformFunction(eCSSKeyword_rotatey, resultTail);
        MOZ_TRY(SetCSSAngle(angle, arr->Item(1)));
        break;
      }
      case layers::TransformFunction::TRotationZ: {
        const layers::CSSAngle& angle = function.get_RotationZ().angle();
        arr = AppendTransformFunction(eCSSKeyword_rotatez, resultTail);
        MOZ_TRY(SetCSSAngle(angle, arr->Item(1)));
        break;
      }
      case layers::TransformFunction::TRotation: {
        const layers::CSSAngle& angle = function.get_Rotation().angle();
        arr = AppendTransformFunction(eCSSKeyword_rotate, resultTail);
        MOZ_TRY(SetCSSAngle(angle, arr->Item(1)));
        break;
      }
      case layers::TransformFunction::TRotation3D: {
        float x = function.get_Rotation3D().x();
        float y = function.get_Rotation3D().y();
        float z = function.get_Rotation3D().z();
        const layers::CSSAngle& angle = function.get_Rotation3D().angle();
        arr = AppendTransformFunction(eCSSKeyword_rotate3d, resultTail);
        arr->Item(1).SetFloatValue(x, eCSSUnit_Number);
        arr->Item(2).SetFloatValue(y, eCSSUnit_Number);
        arr->Item(3).SetFloatValue(z, eCSSUnit_Number);
        MOZ_TRY(SetCSSAngle(angle, arr->Item(4)));
        break;
      }
      case layers::TransformFunction::TScale: {
        arr = AppendTransformFunction(eCSSKeyword_scale3d, resultTail);
        arr->Item(1).SetFloatValue(function.get_Scale().x(), eCSSUnit_Number);
        arr->Item(2).SetFloatValue(function.get_Scale().y(), eCSSUnit_Number);
        arr->Item(3).SetFloatValue(function.get_Scale().z(), eCSSUnit_Number);
        break;
      }
      case layers::TransformFunction::TTranslation: {
        arr = AppendTransformFunction(eCSSKeyword_translate3d, resultTail);
        arr->Item(1).SetFloatValue(function.get_Translation().x(),
                                   eCSSUnit_Pixel);
        arr->Item(2).SetFloatValue(function.get_Translation().y(),
                                   eCSSUnit_Pixel);
        arr->Item(3).SetFloatValue(function.get_Translation().z(),
                                   eCSSUnit_Pixel);
        break;
      }
      case layers::TransformFunction::TSkewX: {
        const layers::CSSAngle& x = function.get_SkewX().x();
        arr = AppendTransformFunction(eCSSKeyword_skewx, resultTail);
        MOZ_TRY(SetCSSAngle(x, arr->Item(1)));
        break;
      }
      case layers::TransformFunction::TSkewY: {
        const layers::CSSAngle& y = function.get_SkewY().y();
        arr = AppendTransformFunction(eCSSKeyword_skewy, resultTail);
        MOZ_TRY(SetCSSAngle(y, arr->Item(1)));
        break;
      }
      case layers::TransformFunction::TSkew: {
        const layers::CSSAngle& x = function.get_Skew().x();
        const layers::CSSAngle& y = function.get_Skew().y();
        arr = AppendTransformFunction(eCSSKeyword_skew, resultTail);
        MOZ_TRY(SetCSSAngle(x, arr->Item(1)));
        MOZ_TRY(SetCSSAngle(y, arr->Item(2)));
        break;
      }
      case layers::TransformFunction::TTransformMatrix: {
        arr = AppendTransformFunction(eCSSKeyword_matrix3d, resultTail);
        const gfx::Matrix4x4& matrix = function.get_TransformMatrix().value();
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
      case layers::TransformFunction::TPerspective: {
        float perspective = function.get_Perspective().value();
        arr = AppendTransformFunction(eCSSKeyword_perspective, resultTail);
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

// AnimationValue Implementation

bool AnimationValue::operator==(const AnimationValue& aOther) const {
  if (mServo && aOther.mServo) {
    return Servo_AnimationValue_DeepEqual(mServo, aOther.mServo);
  }
  if (!mServo && !aOther.mServo) {
    return true;
  }
  return false;
}

bool AnimationValue::operator!=(const AnimationValue& aOther) const {
  return !operator==(aOther);
}

float AnimationValue::GetOpacity() const {
  MOZ_ASSERT(mServo);
  return Servo_AnimationValue_GetOpacity(mServo);
}

nscolor AnimationValue::GetColor(nscolor aForegroundColor) const {
  MOZ_ASSERT(mServo);
  return Servo_AnimationValue_GetColor(mServo, aForegroundColor);
}

already_AddRefed<const nsCSSValueSharedList> AnimationValue::GetTransformList()
    const {
  MOZ_ASSERT(mServo);
  RefPtr<nsCSSValueSharedList> transform;
  Servo_AnimationValue_GetTransform(mServo, &transform);
  return transform.forget();
}

Size AnimationValue::GetScaleValue(const nsIFrame* aFrame) const {
  RefPtr<const nsCSSValueSharedList> list = GetTransformList();
  return nsStyleTransformMatrix::GetScaleValue(list, aFrame);
}

void AnimationValue::SerializeSpecifiedValue(nsCSSPropertyID aProperty,
                                             nsAString& aString) const {
  MOZ_ASSERT(mServo);
  Servo_AnimationValue_Serialize(mServo, aProperty, &aString);
}

bool AnimationValue::IsInterpolableWith(nsCSSPropertyID aProperty,
                                        const AnimationValue& aToValue) const {
  if (IsNull() || aToValue.IsNull()) {
    return false;
  }

  MOZ_ASSERT(mServo);
  MOZ_ASSERT(aToValue.mServo);
  return Servo_AnimationValues_IsInterpolable(mServo, aToValue.mServo);
}

double AnimationValue::ComputeDistance(nsCSSPropertyID aProperty,
                                       const AnimationValue& aOther,
                                       ComputedStyle* aComputedStyle) const {
  if (IsNull() || aOther.IsNull()) {
    return 0.0;
  }

  MOZ_ASSERT(mServo);
  MOZ_ASSERT(aOther.mServo);

  double distance =
      Servo_AnimationValues_ComputeDistance(mServo, aOther.mServo);
  return distance < 0.0 ? 0.0 : distance;
}

/* static */
AnimationValue AnimationValue::FromString(nsCSSPropertyID aProperty,
                                          const nsAString& aValue,
                                          Element* aElement) {
  MOZ_ASSERT(aElement);

  AnimationValue result;

  nsCOMPtr<Document> doc = aElement->GetComposedDoc();
  if (!doc) {
    return result;
  }

  nsCOMPtr<nsIPresShell> shell = doc->GetShell();
  if (!shell) {
    return result;
  }

  // GetComputedStyle() flushes style, so we shouldn't assume that any
  // non-owning references we have are still valid.
  RefPtr<ComputedStyle> computedStyle =
      nsComputedDOMStyle::GetComputedStyle(aElement, nullptr);
  MOZ_ASSERT(computedStyle);

  RefPtr<RawServoDeclarationBlock> declarations = ServoCSSParser::ParseProperty(
      aProperty, aValue, ServoCSSParser::GetParsingEnvironment(doc));

  if (!declarations) {
    return result;
  }

  result.mServo = shell->StyleSet()->ComputeAnimationValue(
      aElement, declarations, computedStyle);
  return result;
}

/* static */
already_AddRefed<RawServoAnimationValue> AnimationValue::FromAnimatable(
    nsCSSPropertyID aProperty, const layers::Animatable& aAnimatable) {
  RefPtr<RawServoAnimationValue> result;

  switch (aAnimatable.type()) {
    case layers::Animatable::Tnull_t:
      break;
    case layers::Animatable::TArrayOfTransformFunction: {
      const InfallibleTArray<layers::TransformFunction>& transforms =
          aAnimatable.get_ArrayOfTransformFunction();
      auto listOrError = CreateCSSValueList(transforms);
      if (listOrError.isOk()) {
        RefPtr<nsCSSValueSharedList> list = listOrError.unwrap();
        MOZ_ASSERT(list, "Transform list should be non null");
        result = Servo_AnimationValue_Transform(eCSSProperty_transform, *list)
                     .Consume();
      }
      break;
    }
    case layers::Animatable::Tfloat:
      result = Servo_AnimationValue_Opacity(aAnimatable.get_float()).Consume();
      break;
    case layers::Animatable::Tnscolor:
      result = Servo_AnimationValue_Color(aProperty, aAnimatable.get_nscolor())
                   .Consume();
      break;
    case layers::Animatable::TRotate: {
      RefPtr<nsCSSValueSharedList> list = new nsCSSValueSharedList;
      list->mHead = new nsCSSValueList;

      const layers::Rotate& r = aAnimatable.get_Rotate();
      if (r.type() == layers::Rotate::Tnull_t) {
        list->mHead->mValue.SetNoneValue();
      } else {
        RefPtr<nsCSSValue::Array> arr;
        if (r.type() == layers::Rotate::TRotation) {
          const layers::CSSAngle& angle = r.get_Rotation().angle();
          arr = AppendFunction(eCSSKeyword_rotate);
          auto rv = SetCSSAngle(angle, arr->Item(1));
          if (rv.isErr()) {
            arr->Item(1).SetFloatValue(0.0, eCSSUnit_Degree);
          }
        } else {
          MOZ_ASSERT(r.type() == layers::Rotate::TRotation3D,
                     "Should be rotate3D");
          float x = r.get_Rotation3D().x();
          float y = r.get_Rotation3D().y();
          float z = r.get_Rotation3D().z();
          const layers::CSSAngle& angle = r.get_Rotation3D().angle();
          arr = AppendFunction(eCSSKeyword_rotate3d);
          arr->Item(1).SetFloatValue(x, eCSSUnit_Number);
          arr->Item(2).SetFloatValue(y, eCSSUnit_Number);
          arr->Item(3).SetFloatValue(z, eCSSUnit_Number);
          auto rv = SetCSSAngle(angle, arr->Item(4));
          if (rv.isErr()) {
            arr->Item(4).SetFloatValue(0.0, eCSSUnit_Degree);
          }
        }
        list->mHead->mValue.SetArrayValue(arr, eCSSUnit_Function);
      }
      result =
          Servo_AnimationValue_Transform(eCSSProperty_rotate, *list).Consume();
      break;
    }
    case layers::Animatable::TScale: {
      const layers::Scale& scale = aAnimatable.get_Scale();
      RefPtr<nsCSSValue::Array> arr = AppendFunction(eCSSKeyword_scale3d);
      arr->Item(1).SetFloatValue(scale.x(), eCSSUnit_Number);
      arr->Item(2).SetFloatValue(scale.y(), eCSSUnit_Number);
      arr->Item(3).SetFloatValue(scale.z(), eCSSUnit_Number);

      RefPtr<nsCSSValueSharedList> list = new nsCSSValueSharedList;
      list->mHead = new nsCSSValueList;
      list->mHead->mValue.SetArrayValue(arr, eCSSUnit_Function);
      result =
          Servo_AnimationValue_Transform(eCSSProperty_scale, *list).Consume();
      break;
    }
    case layers::Animatable::TTranslation: {
      const layers::Translation& translate = aAnimatable.get_Translation();
      RefPtr<nsCSSValue::Array> arr = AppendFunction(eCSSKeyword_translate3d);
      arr->Item(1).SetFloatValue(translate.x(), eCSSUnit_Pixel);
      arr->Item(2).SetFloatValue(translate.y(), eCSSUnit_Pixel);
      arr->Item(3).SetFloatValue(translate.z(), eCSSUnit_Pixel);

      RefPtr<nsCSSValueSharedList> list = new nsCSSValueSharedList;
      list->mHead = new nsCSSValueList;
      list->mHead->mValue.SetArrayValue(arr, eCSSUnit_Function);
      result = Servo_AnimationValue_Transform(eCSSProperty_translate, *list)
                   .Consume();
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported type");
  }
  return result.forget();
}
