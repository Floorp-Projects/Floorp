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

const mozilla::StyleOffsetPath& AnimationValue::GetOffsetPathProperty() const {
  MOZ_ASSERT(mServo);
  return *Servo_AnimationValue_GetOffsetPath(mServo);
}

const mozilla::LengthPercentage& AnimationValue::GetOffsetDistanceProperty()
    const {
  MOZ_ASSERT(mServo);
  return *Servo_AnimationValue_GetOffsetDistance(mServo);
}

const mozilla::StyleOffsetRotate& AnimationValue::GetOffsetRotateProperty()
    const {
  MOZ_ASSERT(mServo);
  return *Servo_AnimationValue_GetOffsetRotate(mServo);
}

const mozilla::StylePositionOrAuto& AnimationValue::GetOffsetAnchorProperty()
    const {
  MOZ_ASSERT(mServo);
  return *Servo_AnimationValue_GetOffsetAnchor(mServo);
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
                                             const RawServoStyleSet* aRawSet,
                                             nsAString& aString) const {
  MOZ_ASSERT(mServo);
  Servo_AnimationValue_Serialize(mServo, aProperty, aRawSet, &aString);
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

/* static */
already_AddRefed<RawServoAnimationValue> AnimationValue::FromAnimatable(
    nsCSSPropertyID aProperty, const layers::Animatable& aAnimatable) {
  switch (aAnimatable.type()) {
    case layers::Animatable::Tnull_t:
      break;
    case layers::Animatable::TStyleTransform: {
      const StyleTransform& transform = aAnimatable.get_StyleTransform();
      MOZ_ASSERT(!transform.HasPercent(),
                 "Received transform operations should have been resolved.");
      return Servo_AnimationValue_Transform(&transform).Consume();
    }
    case layers::Animatable::Tfloat:
      return Servo_AnimationValue_Opacity(aAnimatable.get_float()).Consume();
    case layers::Animatable::Tnscolor:
      return Servo_AnimationValue_Color(aProperty, aAnimatable.get_nscolor())
          .Consume();
    case layers::Animatable::TStyleRotate:
      return Servo_AnimationValue_Rotate(&aAnimatable.get_StyleRotate())
          .Consume();
    case layers::Animatable::TStyleScale:
      return Servo_AnimationValue_Scale(&aAnimatable.get_StyleScale())
          .Consume();
    case layers::Animatable::TStyleTranslate:
      MOZ_ASSERT(
          aAnimatable.get_StyleTranslate().IsNone() ||
              (!aAnimatable.get_StyleTranslate()
                    .AsTranslate()
                    ._0.HasPercent() &&
               !aAnimatable.get_StyleTranslate().AsTranslate()._1.HasPercent()),
          "Should have been resolved already");
      return Servo_AnimationValue_Translate(&aAnimatable.get_StyleTranslate())
          .Consume();
    case layers::Animatable::TStyleOffsetPath:
      return Servo_AnimationValue_OffsetPath(&aAnimatable.get_StyleOffsetPath())
          .Consume();
    case layers::Animatable::TLengthPercentage:
      return Servo_AnimationValue_OffsetDistance(
                 &aAnimatable.get_LengthPercentage())
          .Consume();
    case layers::Animatable::TStyleOffsetRotate:
      return Servo_AnimationValue_OffsetRotate(
                 &aAnimatable.get_StyleOffsetRotate())
          .Consume();
    case layers::Animatable::TStylePositionOrAuto:
      return Servo_AnimationValue_OffsetAnchor(
                 &aAnimatable.get_StylePositionOrAuto())
          .Consume();
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported type");
  }
  return nullptr;
}
