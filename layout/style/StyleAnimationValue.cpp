/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utilities for animation of computed style values */

#include "mozilla/StyleAnimationValue.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "mozilla/ComputedStyle.h"
#include "nsComputedDOMStyle.h"
#include "nsContentUtils.h"
#include "nsCSSPseudoElements.h"
#include "mozilla/dom/Element.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"
#include "mozilla/ServoBindings.h" // RawServoDeclarationBlock
#include "mozilla/ServoCSSParser.h"
#include "gfxMatrix.h"
#include "gfxQuaternion.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "gfx2DGlue.h"
#include "mozilla/ComputedStyleInlines.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::gfx;
using nsStyleTransformMatrix::Decompose2DMatrix;
using nsStyleTransformMatrix::Decompose3DMatrix;
using nsStyleTransformMatrix::ShearType;


static already_AddRefed<nsCSSValue::Array>
AppendFunction(nsCSSKeyword aTransformFunction)
{
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



// AnimationValue Implementation

bool
AnimationValue::operator==(const AnimationValue& aOther) const
{
  if (mServo && aOther.mServo) {
    return Servo_AnimationValue_DeepEqual(mServo, aOther.mServo);
  }
  if (!mServo && !aOther.mServo) {
    return true;
  }
  return false;
}

bool
AnimationValue::operator!=(const AnimationValue& aOther) const
{
  return !operator==(aOther);
}

float
AnimationValue::GetOpacity() const
{
  MOZ_ASSERT(mServo);
  if (mServo) {
    return Servo_AnimationValue_GetOpacity(mServo);
  }
  MOZ_CRASH("old style system disabled");
}

already_AddRefed<const nsCSSValueSharedList>
AnimationValue::GetTransformList() const
{
  MOZ_ASSERT(mServo);

  RefPtr<nsCSSValueSharedList> transform;
  if (mServo) {
    Servo_AnimationValue_GetTransform(mServo, &transform);
  } else {
    MOZ_CRASH("old style system disabled");
  }
  return transform.forget();
}

Size
AnimationValue::GetScaleValue(const nsIFrame* aFrame) const
{
  MOZ_ASSERT(mServo);

  if (mServo) {
    RefPtr<nsCSSValueSharedList> list;
    Servo_AnimationValue_GetTransform(mServo, &list);
    return nsStyleTransformMatrix::GetScaleValue(list, aFrame);
  }
  MOZ_CRASH("old style system disabled");
}

void
AnimationValue::SerializeSpecifiedValue(nsCSSPropertyID aProperty,
                                        nsAString& aString) const
{
  MOZ_ASSERT(mServo);

  if (mServo) {
    Servo_AnimationValue_Serialize(mServo, aProperty, &aString);
    return;
  }

  MOZ_CRASH("old style system disabled");
}

bool
AnimationValue::IsInterpolableWith(nsCSSPropertyID aProperty,
                                   const AnimationValue& aToValue) const
{
  if (IsNull() || aToValue.IsNull()) {
    return false;
  }

  MOZ_ASSERT(mServo);
  MOZ_ASSERT(aToValue.mServo);

  if (mServo) {
    return Servo_AnimationValues_IsInterpolable(mServo, aToValue.mServo);
  }

  MOZ_CRASH("old style system disabled");
}

double
AnimationValue::ComputeDistance(nsCSSPropertyID aProperty,
                                const AnimationValue& aOther,
                                ComputedStyle* aComputedStyle) const
{
  if (IsNull() || aOther.IsNull()) {
    return 0.0;
  }

  MOZ_ASSERT(mServo);
  MOZ_ASSERT(aOther.mServo);

  double distance= 0.0;
  if (mServo) {
    distance = Servo_AnimationValues_ComputeDistance(mServo, aOther.mServo);
    return distance < 0.0
           ? 0.0
           : distance;
  }

  MOZ_CRASH("old style system disabled");
}

/* static */ AnimationValue
AnimationValue::FromString(nsCSSPropertyID aProperty,
                           const nsAString& aValue,
                           Element* aElement)
{
  MOZ_ASSERT(aElement);

  AnimationValue result;

  nsCOMPtr<nsIDocument> doc = aElement->GetComposedDoc();
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

  RefPtr<RawServoDeclarationBlock> declarations =
    ServoCSSParser::ParseProperty(aProperty, aValue,
                                  ServoCSSParser::GetParsingEnvironment(doc));

  if (!declarations) {
    return result;
  }

  result.mServo = shell->StyleSet()->
    ComputeAnimationValue(aElement, declarations, computedStyle);
  return result;
}

/* static */ AnimationValue
AnimationValue::Opacity(float aOpacity)
{
  AnimationValue result;
  result.mServo = Servo_AnimationValue_Opacity(aOpacity).Consume();
  return result;
}

/* static */ AnimationValue
AnimationValue::Transform(nsCSSValueSharedList& aList)
{
  AnimationValue result;
  result.mServo = Servo_AnimationValue_Transform(aList).Consume();
  return result;
}

/* static */ already_AddRefed<nsCSSValue::Array>
AnimationValue::AppendTransformFunction(nsCSSKeyword aTransformFunction,
                                        nsCSSValueList**& aListTail)
{
  RefPtr<nsCSSValue::Array> arr = AppendFunction(aTransformFunction);
  nsCSSValueList *item = new nsCSSValueList;
  item->mValue.SetArrayValue(arr, eCSSUnit_Function);

  *aListTail = item;
  aListTail = &item->mNext;

  return arr.forget();
}
