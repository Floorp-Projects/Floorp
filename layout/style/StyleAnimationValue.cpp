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

static StylePathCommand CommandFromLayers(const layers::PathCommand& aCommand) {
  switch (aCommand.type()) {
    case layers::PathCommand::TMoveTo:
      return StylePathCommand::MoveTo(
          StyleCoordPair(aCommand.get_MoveTo().point()), StyleIsAbsolute::Yes);
    case layers::PathCommand::TLineTo:
      return StylePathCommand::LineTo(
          StyleCoordPair(aCommand.get_LineTo().point()), StyleIsAbsolute::Yes);
    case layers::PathCommand::THorizontalLineTo:
      return StylePathCommand::HorizontalLineTo(
          aCommand.get_HorizontalLineTo().x(), StyleIsAbsolute::Yes);
    case layers::PathCommand::TVerticalLineTo:
      return StylePathCommand::VerticalLineTo(aCommand.get_VerticalLineTo().y(),
                                              StyleIsAbsolute::Yes);
    case layers::PathCommand::TCurveTo:
      return StylePathCommand::CurveTo(
          StyleCoordPair(aCommand.get_CurveTo().control1()),
          StyleCoordPair(aCommand.get_CurveTo().control2()),
          StyleCoordPair(aCommand.get_CurveTo().point()), StyleIsAbsolute::Yes);
    case layers::PathCommand::TSmoothCurveTo:
      return StylePathCommand::SmoothCurveTo(
          StyleCoordPair(aCommand.get_SmoothCurveTo().control2()),
          StyleCoordPair(aCommand.get_SmoothCurveTo().point()),
          StyleIsAbsolute::Yes);
    case layers::PathCommand::TQuadBezierCurveTo:
      return StylePathCommand::QuadBezierCurveTo(
          StyleCoordPair(aCommand.get_QuadBezierCurveTo().control1()),
          StyleCoordPair(aCommand.get_QuadBezierCurveTo().point()),
          StyleIsAbsolute::Yes);
    case layers::PathCommand::TSmoothQuadBezierCurveTo:
      return StylePathCommand::SmoothQuadBezierCurveTo(
          StyleCoordPair(aCommand.get_SmoothQuadBezierCurveTo().point()),
          StyleIsAbsolute::Yes);
    case layers::PathCommand::TEllipticalArc: {
      const layers::EllipticalArc& arc = aCommand.get_EllipticalArc();
      return StylePathCommand::EllipticalArc(
          arc.rx(), arc.ry(), arc.angle(), StyleArcFlag{arc.largeArcFlag()},
          StyleArcFlag{arc.sweepFlag()}, StyleCoordPair(arc.point()),
          StyleIsAbsolute::Yes);
    }
    case layers::PathCommand::Tnull_t:
      return StylePathCommand::ClosePath();
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported path command");
  }
  return StylePathCommand::Unknown();
}

static nsTArray<StylePathCommand> CreatePathCommandList(
    const nsTArray<layers::PathCommand>& aCommands) {
  nsTArray<StylePathCommand> result(aCommands.Length());
  for (const layers::PathCommand& command : aCommands) {
    result.AppendElement(CommandFromLayers(command));
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
    case layers::Animatable::TArrayOfTransformFunction: {
      nsTArray<StyleTransformOperation> ops =
          CreateTransformList(aAnimatable.get_ArrayOfTransformFunction());
      return Servo_AnimationValue_Transform(ops.Elements(), ops.Length())
          .Consume();
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
    case layers::Animatable::TOffsetPath: {
      const layers::OffsetPath& p = aAnimatable.get_OffsetPath();
      switch (p.type()) {
        case layers::OffsetPath::TArrayOfPathCommand: {
          nsTArray<StylePathCommand> commands =
              CreatePathCommandList(p.get_ArrayOfPathCommand());
          return Servo_AnimationValue_SVGPath(commands.Elements(),
                                              commands.Length())
              .Consume();
        }
        case layers::OffsetPath::TRayFunction:
          return Servo_AnimationValue_RayFunction(&p.get_RayFunction())
              .Consume();
        case layers::OffsetPath::Tnull_t:
          return Servo_AnimationValue_NoneOffsetPath().Consume();
        default:
          MOZ_ASSERT_UNREACHABLE("Unsupported path");
      }
      return nullptr;
    }
    case layers::Animatable::TLengthPercentage:
      return Servo_AnimationValue_OffsetDistance(
                 &aAnimatable.get_LengthPercentage())
          .Consume();
    case layers::Animatable::TOffsetRotate: {
      const layers::OffsetRotate& r = aAnimatable.get_OffsetRotate();
      auto rotate = StyleOffsetRotate{r.isAuto(), GetCSSAngle(r.angle())};
      return Servo_AnimationValue_OffsetRotate(&rotate).Consume();
    }
    case layers::Animatable::TOffsetAnchor: {
      const layers::OffsetAnchor& a = aAnimatable.get_OffsetAnchor();
      auto anchor = a.type() == layers::OffsetAnchor::Type::Tnull_t
                        ? StylePositionOrAuto::Auto()
                        : StylePositionOrAuto::Position(
                              StylePosition{a.get_AnchorPosition().horizontal(),
                                            a.get_AnchorPosition().vertical()});
      return Servo_AnimationValue_OffsetAnchor(&anchor).Consume();
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported type");
  }
  return nullptr;
}
