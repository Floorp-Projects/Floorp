/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utilities for animation of computed style values */

#include "mozilla/StyleAnimationValue.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
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

// TODO(emilio): Remove angle unit in a followup, should always be degrees.
static inline StyleAngle GetCSSAngle(const layers::CSSAngle& aAngle) {
  if (aAngle.unit() != eCSSUnit_Degree) {
    NS_ERROR("Bogus animation from IPC");
    return StyleAngle{0.0};
  }
  return StyleAngle{aAngle.value()};
}

static StyleTransformOperation OperationFromLayers(
    const layers::TransformFunction& aFunction) {
  switch (aFunction.type()) {
    case layers::TransformFunction::TRotationX: {
      const layers::CSSAngle& angle = aFunction.get_RotationX().angle();
      return StyleTransformOperation::RotateX(GetCSSAngle(angle));
    }
    case layers::TransformFunction::TRotationY: {
      const layers::CSSAngle& angle = aFunction.get_RotationY().angle();
      return StyleTransformOperation::RotateY(GetCSSAngle(angle));
    }
    case layers::TransformFunction::TRotationZ: {
      const layers::CSSAngle& angle = aFunction.get_RotationZ().angle();
      return StyleTransformOperation::RotateZ(GetCSSAngle(angle));
    }
    case layers::TransformFunction::TRotation: {
      const layers::CSSAngle& angle = aFunction.get_Rotation().angle();
      return StyleTransformOperation::Rotate(GetCSSAngle(angle));
    }
    case layers::TransformFunction::TRotation3D: {
      float x = aFunction.get_Rotation3D().x();
      float y = aFunction.get_Rotation3D().y();
      float z = aFunction.get_Rotation3D().z();
      const layers::CSSAngle& angle = aFunction.get_Rotation3D().angle();
      return StyleTransformOperation::Rotate3D(x, y, z, GetCSSAngle(angle));
    }
    case layers::TransformFunction::TScale: {
      float x = aFunction.get_Scale().x();
      float y = aFunction.get_Scale().y();
      float z = aFunction.get_Scale().z();
      return StyleTransformOperation::Scale3D(x, y, z);
    }
    case layers::TransformFunction::TTranslation: {
      float x = aFunction.get_Translation().x();
      float y = aFunction.get_Translation().y();
      float z = aFunction.get_Translation().z();
      return StyleTransformOperation::Translate3D(
          LengthPercentage::FromPixels(x), LengthPercentage::FromPixels(y),
          Length{z});
    }
    case layers::TransformFunction::TSkewX: {
      const layers::CSSAngle& x = aFunction.get_SkewX().x();
      return StyleTransformOperation::SkewX(GetCSSAngle(x));
    }
    case layers::TransformFunction::TSkewY: {
      const layers::CSSAngle& y = aFunction.get_SkewY().y();
      return StyleTransformOperation::SkewY(GetCSSAngle(y));
    }
    case layers::TransformFunction::TSkew: {
      const layers::CSSAngle& x = aFunction.get_Skew().x();
      const layers::CSSAngle& y = aFunction.get_Skew().y();
      return StyleTransformOperation::Skew(GetCSSAngle(x), GetCSSAngle(y));
    }
    case layers::TransformFunction::TTransformMatrix: {
      const gfx::Matrix4x4& matrix = aFunction.get_TransformMatrix().value();
      return StyleTransformOperation::Matrix3D({
          matrix._11,
          matrix._12,
          matrix._13,
          matrix._14,
          matrix._21,
          matrix._22,
          matrix._23,
          matrix._24,
          matrix._31,
          matrix._32,
          matrix._33,
          matrix._34,
          matrix._41,
          matrix._42,
          matrix._43,
          matrix._44,
      });
    }
    case layers::TransformFunction::TPerspective: {
      float perspective = aFunction.get_Perspective().value();
      return StyleTransformOperation::Perspective(Length{perspective});
    }
    default:
      MOZ_ASSERT_UNREACHABLE("All functions should be implemented?");
      return StyleTransformOperation::TranslateX(LengthPercentage::Zero());
  }
}

static nsTArray<StyleTransformOperation> CreateTransformList(
    const nsTArray<layers::TransformFunction>& aFunctions) {
  nsTArray<StyleTransformOperation> result;
  result.SetCapacity(aFunctions.Length());
  for (const layers::TransformFunction& function : aFunctions) {
    result.AppendElement(OperationFromLayers(function));
  }
  return result;
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

const StyleTranslate& AnimationValue::GetTranslateProperty() const {
  MOZ_ASSERT(mServo);
  return *Servo_AnimationValue_GetTranslate(mServo);
}

const StyleRotate& AnimationValue::GetRotateProperty() const {
  MOZ_ASSERT(mServo);
  return *Servo_AnimationValue_GetRotate(mServo);
}

const StyleScale& AnimationValue::GetScaleProperty() const {
  MOZ_ASSERT(mServo);
  return *Servo_AnimationValue_GetScale(mServo);
}

const StyleTransform& AnimationValue::GetTransformProperty() const {
  MOZ_ASSERT(mServo);
  return *Servo_AnimationValue_GetTransform(mServo);
}

Size AnimationValue::GetScaleValue(const nsIFrame* aFrame) const {
  using namespace nsStyleTransformMatrix;

  const StyleTranslate* translate = nullptr;
  const StyleRotate* rotate = nullptr;
  const StyleScale* scale = nullptr;
  const StyleTransform* transform = nullptr;

  switch (Servo_AnimationValue_GetPropertyId(mServo)) {
    case eCSSProperty_scale:
      scale = &GetScaleProperty();
      break;
    case eCSSProperty_translate:
      translate = &GetTranslateProperty();
      break;
    case eCSSProperty_rotate:
      rotate = &GetRotateProperty();
      break;
    case eCSSProperty_transform:
      transform = &GetTransformProperty();
      break;
    default:
      MOZ_ASSERT_UNREACHABLE(
          "Should only need to check in transform properties");
      return Size(1.0, 1.0);
  }

  TransformReferenceBox refBox(aFrame);
  Matrix4x4 t =
      ReadTransforms(translate ? *translate : StyleTranslate::None(),
                     rotate ? *rotate : StyleRotate::None(),
                     scale ? *scale : StyleScale::None(), Nothing(),
                     transform ? *transform : StyleTransform(), refBox,
                     aFrame->PresContext()->AppUnitsPerDevPixel());
  Matrix transform2d;
  bool canDraw2D = t.CanDraw2D(&transform2d);
  if (!canDraw2D) {
    return Size();
  }
  return transform2d.ScaleFactors(true);
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

  RefPtr<PresShell> presShell = doc->GetPresShell();
  if (!presShell) {
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

  result.mServo = presShell->StyleSet()->ComputeAnimationValue(
      aElement, declarations, computedStyle);
  return result;
}

StyleRotate RotateFromLayers(const layers::Rotate& aRotate) {
  switch (aRotate.type()) {
    case layers::Rotate::Tnull_t:
      return StyleRotate::None();
    case layers::Rotate::TRotation: {
      const layers::CSSAngle& angle = aRotate.get_Rotation().angle();
      return StyleRotate::Rotate(GetCSSAngle(angle));
    }
    case layers::Rotate::TRotation3D: {
      float x = aRotate.get_Rotation3D().x();
      float y = aRotate.get_Rotation3D().y();
      float z = aRotate.get_Rotation3D().z();
      const layers::CSSAngle& angle = aRotate.get_Rotation3D().angle();
      return StyleRotate::Rotate3D(x, y, z, GetCSSAngle(angle));
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown rotate value?");
      return StyleRotate::None();
  }
}

/* static */
already_AddRefed<RawServoAnimationValue> AnimationValue::FromAnimatable(
    nsCSSPropertyID aProperty, const layers::Animatable& aAnimatable) {
  switch (aAnimatable.type()) {
    case layers::Animatable::Tnull_t:
      break;
    case layers::Animatable::TArrayOfTransformFunction: {
      nsTArray<StyleTransformOperation> ops =
          CreateTransformList(aAnimatable.get_ArrayOfTransformFunction());
      ;
      return Servo_AnimationValue_Transform(ops.Elements(), ops.Length())
          .Consume();
    }
    case layers::Animatable::Tfloat:
      return Servo_AnimationValue_Opacity(aAnimatable.get_float()).Consume();
    case layers::Animatable::Tnscolor:
      return Servo_AnimationValue_Color(aProperty, aAnimatable.get_nscolor())
          .Consume();
    case layers::Animatable::TRotate: {
      auto rotate = RotateFromLayers(aAnimatable.get_Rotate());
      return Servo_AnimationValue_Rotate(&rotate).Consume();
    }
    case layers::Animatable::TScale: {
      const layers::Scale& s = aAnimatable.get_Scale();
      auto scale = StyleScale::Scale3D(s.x(), s.y(), s.z());
      return Servo_AnimationValue_Scale(&scale).Consume();
    }
    case layers::Animatable::TTranslation: {
      const layers::Translation& t = aAnimatable.get_Translation();
      auto translate = StyleTranslate::Translate3D(
          LengthPercentage::FromPixels(t.x()),
          LengthPercentage::FromPixels(t.y()), Length{t.z()});
      return Servo_AnimationValue_Translate(&translate).Consume();
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported type");
  }
  return nullptr;
}
