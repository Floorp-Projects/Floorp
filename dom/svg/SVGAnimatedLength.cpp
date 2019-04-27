/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedLength.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/SMILValue.h"
#include "mozilla/dom/SVGViewportElement.h"
#include "DOMSVGAnimatedLength.h"
#include "DOMSVGLength.h"
#include "LayoutLogging.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "nsTextFormatter.h"
#include "SMILFloatType.h"
#include "SVGAttrTearoffTable.h"
#include "nsSVGIntegrationUtils.h"

using namespace mozilla::dom;

namespace mozilla {

static const nsStaticAtom* const unitMap[] = {
    nullptr, /* SVG_LENGTHTYPE_UNKNOWN */
    nullptr, /* SVG_LENGTHTYPE_NUMBER */
    nsGkAtoms::percentage,
    nsGkAtoms::em,
    nsGkAtoms::ex,
    nsGkAtoms::px,
    nsGkAtoms::cm,
    nsGkAtoms::mm,
    nsGkAtoms::in,
    nsGkAtoms::pt,
    nsGkAtoms::pc};

static SVGAttrTearoffTable<SVGAnimatedLength, DOMSVGAnimatedLength>
    sSVGAnimatedLengthTearoffTable;

/* Helper functions */

static bool IsValidUnitType(uint16_t unit) {
  return unit > SVGLength_Binding::SVG_LENGTHTYPE_UNKNOWN &&
         unit <= SVGLength_Binding::SVG_LENGTHTYPE_PC;
}

static void GetUnitString(nsAString& unit, uint16_t unitType) {
  if (IsValidUnitType(unitType)) {
    if (unitMap[unitType]) {
      unitMap[unitType]->ToString(unit);
    }
    return;
  }

  MOZ_ASSERT_UNREACHABLE("Unknown unit type");
}

static uint16_t GetUnitTypeForString(const nsAString& unitStr) {
  if (unitStr.IsEmpty()) return SVGLength_Binding::SVG_LENGTHTYPE_NUMBER;

  nsAtom* unitAtom = NS_GetStaticAtom(unitStr);
  if (unitAtom) {
    for (uint32_t i = 0; i < ArrayLength(unitMap); i++) {
      if (unitMap[i] == unitAtom) {
        return i;
      }
    }
  }

  return SVGLength_Binding::SVG_LENGTHTYPE_UNKNOWN;
}

static void GetValueString(nsAString& aValueAsString, float aValue,
                           uint16_t aUnitType) {
  nsTextFormatter::ssprintf(aValueAsString, u"%g", (double)aValue);

  nsAutoString unitString;
  GetUnitString(unitString, aUnitType);
  aValueAsString.Append(unitString);
}

static bool GetValueFromString(const nsAString& aString, float& aValue,
                               uint16_t* aUnitType) {
  bool success;
  auto token = SVGContentUtils::GetAndEnsureOneToken(aString, success);

  if (!success) {
    return false;
  }

  RangedPtr<const char16_t> iter = SVGContentUtils::GetStartRangedPtr(token);
  const RangedPtr<const char16_t> end = SVGContentUtils::GetEndRangedPtr(token);

  if (!SVGContentUtils::ParseNumber(iter, end, aValue)) {
    return false;
  }
  const nsAString& units = Substring(iter.get(), end.get());
  *aUnitType = GetUnitTypeForString(units);
  return IsValidUnitType(*aUnitType);
}

static float FixAxisLength(float aLength) {
  if (aLength == 0.0f) {
    LAYOUT_WARNING("zero axis length");
    return 1e-20f;
  }
  return aLength;
}

SVGElementMetrics::SVGElementMetrics(SVGElement* aSVGElement,
                                     SVGViewportElement* aCtx)
    : mSVGElement(aSVGElement), mCtx(aCtx) {}

float SVGElementMetrics::GetEmLength() const {
  return SVGContentUtils::GetFontSize(mSVGElement);
}

float SVGElementMetrics::GetExLength() const {
  return SVGContentUtils::GetFontXHeight(mSVGElement);
}

float SVGElementMetrics::GetAxisLength(uint8_t aCtxType) const {
  if (!EnsureCtx()) {
    return 1;
  }

  return FixAxisLength(mCtx->GetLength(aCtxType));
}

bool SVGElementMetrics::EnsureCtx() const {
  if (!mCtx && mSVGElement) {
    mCtx = mSVGElement->GetCtx();
    if (!mCtx && mSVGElement->IsSVGElement(nsGkAtoms::svg)) {
      // mSVGElement must be the outer svg element
      mCtx = static_cast<SVGViewportElement*>(mSVGElement);
    }
  }
  return mCtx != nullptr;
}

NonSVGFrameUserSpaceMetrics::NonSVGFrameUserSpaceMetrics(nsIFrame* aFrame)
    : mFrame(aFrame) {}

float NonSVGFrameUserSpaceMetrics::GetEmLength() const {
  return SVGContentUtils::GetFontSize(mFrame);
}

float NonSVGFrameUserSpaceMetrics::GetExLength() const {
  return SVGContentUtils::GetFontXHeight(mFrame);
}

gfx::Size NonSVGFrameUserSpaceMetrics::GetSize() const {
  return nsSVGIntegrationUtils::GetSVGCoordContextForNonSVGFrame(mFrame);
}

float UserSpaceMetricsWithSize::GetAxisLength(uint8_t aCtxType) const {
  gfx::Size size = GetSize();
  float length;
  switch (aCtxType) {
    case SVGContentUtils::X:
      length = size.width;
      break;
    case SVGContentUtils::Y:
      length = size.height;
      break;
    case SVGContentUtils::XY:
      length =
          SVGContentUtils::ComputeNormalizedHypotenuse(size.width, size.height);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown axis type");
      length = 1;
      break;
  }
  return FixAxisLength(length);
}

float SVGAnimatedLength::GetPixelsPerUnit(SVGElement* aSVGElement,
                                          uint8_t aUnitType) const {
  return GetPixelsPerUnit(SVGElementMetrics(aSVGElement), aUnitType);
}

float SVGAnimatedLength::GetPixelsPerUnit(SVGViewportElement* aCtx,
                                          uint8_t aUnitType) const {
  return GetPixelsPerUnit(SVGElementMetrics(aCtx, aCtx), aUnitType);
}

float SVGAnimatedLength::GetPixelsPerUnit(nsIFrame* aFrame,
                                          uint8_t aUnitType) const {
  nsIContent* content = aFrame->GetContent();
  if (content->IsSVGElement()) {
    return GetPixelsPerUnit(
        SVGElementMetrics(static_cast<SVGElement*>(content)), aUnitType);
  }
  return GetPixelsPerUnit(NonSVGFrameUserSpaceMetrics(aFrame), aUnitType);
}

// See https://www.w3.org/TR/css-values-3/#absolute-lengths
static const float DPI = 96.0f;

float SVGAnimatedLength::GetPixelsPerUnit(const UserSpaceMetrics& aMetrics,
                                          uint8_t aUnitType) const {
  switch (aUnitType) {
    case SVGLength_Binding::SVG_LENGTHTYPE_NUMBER:
    case SVGLength_Binding::SVG_LENGTHTYPE_PX:
      return 1;
    case SVGLength_Binding::SVG_LENGTHTYPE_MM:
      return DPI / MM_PER_INCH_FLOAT;
    case SVGLength_Binding::SVG_LENGTHTYPE_CM:
      return 10.0f * DPI / MM_PER_INCH_FLOAT;
    case SVGLength_Binding::SVG_LENGTHTYPE_IN:
      return DPI;
    case SVGLength_Binding::SVG_LENGTHTYPE_PT:
      return DPI / POINTS_PER_INCH_FLOAT;
    case SVGLength_Binding::SVG_LENGTHTYPE_PC:
      return 12.0f * DPI / POINTS_PER_INCH_FLOAT;
    case SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE:
      return aMetrics.GetAxisLength(mCtxType) / 100.0f;
    case SVGLength_Binding::SVG_LENGTHTYPE_EMS:
      return aMetrics.GetEmLength();
    case SVGLength_Binding::SVG_LENGTHTYPE_EXS:
      return aMetrics.GetExLength();
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown unit type");
      return 0;
  }
}

void SVGAnimatedLength::SetBaseValueInSpecifiedUnits(float aValue,
                                                     SVGElement* aSVGElement,
                                                     bool aDoSetAttr) {
  if (mIsBaseSet && mBaseVal == aValue) {
    return;
  }

  nsAttrValue emptyOrOldValue;
  if (aDoSetAttr) {
    emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);
  }
  mBaseVal = aValue;
  mIsBaseSet = true;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  } else {
    aSVGElement->AnimationNeedsResample();
  }
  if (aDoSetAttr) {
    aSVGElement->DidChangeLength(mAttrEnum, emptyOrOldValue);
  }
}

nsresult SVGAnimatedLength::ConvertToSpecifiedUnits(uint16_t unitType,
                                                    SVGElement* aSVGElement) {
  if (!IsValidUnitType(unitType)) return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mIsBaseSet && mSpecifiedUnitType == uint8_t(unitType)) return NS_OK;

  float pixelsPerUnit = GetPixelsPerUnit(aSVGElement, unitType);
  if (pixelsPerUnit == 0.0f) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  float valueInUserUnits =
      mBaseVal * GetPixelsPerUnit(aSVGElement, mSpecifiedUnitType);
  float valueInSpecifiedUnits = valueInUserUnits / pixelsPerUnit;

  if (!IsFinite(valueInSpecifiedUnits)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // Even though we're not changing the visual effect this length will have
  // on the document, we still need to send out notifications in case we have
  // mutation listeners, since the actual string value of the attribute will
  // change.
  nsAttrValue emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);

  mSpecifiedUnitType = uint8_t(unitType);
  // Setting aDoSetAttr to false here will ensure we don't call
  // Will/DidChangeAngle a second time (and dispatch duplicate notifications).
  SetBaseValueInSpecifiedUnits(valueInSpecifiedUnits, aSVGElement, false);

  aSVGElement->DidChangeLength(mAttrEnum, emptyOrOldValue);

  return NS_OK;
}

nsresult SVGAnimatedLength::NewValueSpecifiedUnits(uint16_t aUnitType,
                                                   float aValueInSpecifiedUnits,
                                                   SVGElement* aSVGElement) {
  NS_ENSURE_FINITE(aValueInSpecifiedUnits, NS_ERROR_ILLEGAL_VALUE);

  if (!IsValidUnitType(aUnitType)) return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mIsBaseSet && mBaseVal == aValueInSpecifiedUnits &&
      mSpecifiedUnitType == uint8_t(aUnitType)) {
    return NS_OK;
  }

  nsAttrValue emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);
  mBaseVal = aValueInSpecifiedUnits;
  mIsBaseSet = true;
  mSpecifiedUnitType = uint8_t(aUnitType);
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  } else {
    aSVGElement->AnimationNeedsResample();
  }
  aSVGElement->DidChangeLength(mAttrEnum, emptyOrOldValue);
  return NS_OK;
}

already_AddRefed<DOMSVGLength> SVGAnimatedLength::ToDOMBaseVal(
    SVGElement* aSVGElement) {
  return DOMSVGLength::GetTearOff(this, aSVGElement, false);
}

already_AddRefed<DOMSVGLength> SVGAnimatedLength::ToDOMAnimVal(
    SVGElement* aSVGElement) {
  return DOMSVGLength::GetTearOff(this, aSVGElement, true);
}

/* Implementation */

nsresult SVGAnimatedLength::SetBaseValueString(const nsAString& aValueAsString,
                                               SVGElement* aSVGElement,
                                               bool aDoSetAttr) {
  float value;
  uint16_t unitType;

  if (!GetValueFromString(aValueAsString, value, &unitType)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if (mIsBaseSet && mBaseVal == float(value) &&
      mSpecifiedUnitType == uint8_t(unitType)) {
    return NS_OK;
  }

  nsAttrValue emptyOrOldValue;
  if (aDoSetAttr) {
    emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);
  }
  mBaseVal = value;
  mIsBaseSet = true;
  mSpecifiedUnitType = uint8_t(unitType);
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  } else {
    aSVGElement->AnimationNeedsResample();
  }

  if (aDoSetAttr) {
    aSVGElement->DidChangeLength(mAttrEnum, emptyOrOldValue);
  }
  return NS_OK;
}

void SVGAnimatedLength::GetBaseValueString(nsAString& aValueAsString) const {
  GetValueString(aValueAsString, mBaseVal, mSpecifiedUnitType);
}

void SVGAnimatedLength::GetAnimValueString(nsAString& aValueAsString) const {
  GetValueString(aValueAsString, mAnimVal, mSpecifiedUnitType);
}

nsresult SVGAnimatedLength::SetBaseValue(float aValue, SVGElement* aSVGElement,
                                         bool aDoSetAttr) {
  float pixelsPerUnit = GetPixelsPerUnit(aSVGElement, mSpecifiedUnitType);
  if (pixelsPerUnit == 0.0f) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  float valueInSpecifiedUnits = aValue / pixelsPerUnit;
  if (!IsFinite(valueInSpecifiedUnits)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  SetBaseValueInSpecifiedUnits(valueInSpecifiedUnits, aSVGElement, aDoSetAttr);
  return NS_OK;
}

void SVGAnimatedLength::SetAnimValueInSpecifiedUnits(float aValue,
                                                     SVGElement* aSVGElement) {
  if (mAnimVal == aValue && mIsAnimated) {
    return;
  }
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateLength(mAttrEnum);
}

nsresult SVGAnimatedLength::SetAnimValue(float aValue,
                                         SVGElement* aSVGElement) {
  float valueInSpecifiedUnits =
      aValue / GetPixelsPerUnit(aSVGElement, mSpecifiedUnitType);

  if (IsFinite(valueInSpecifiedUnits)) {
    SetAnimValueInSpecifiedUnits(valueInSpecifiedUnits, aSVGElement);
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

already_AddRefed<DOMSVGAnimatedLength> SVGAnimatedLength::ToDOMAnimatedLength(
    SVGElement* aSVGElement) {
  RefPtr<DOMSVGAnimatedLength> svgAnimatedLength =
      sSVGAnimatedLengthTearoffTable.GetTearoff(this);
  if (!svgAnimatedLength) {
    svgAnimatedLength = new DOMSVGAnimatedLength(this, aSVGElement);
    sSVGAnimatedLengthTearoffTable.AddTearoff(this, svgAnimatedLength);
  }

  return svgAnimatedLength.forget();
}

DOMSVGAnimatedLength::~DOMSVGAnimatedLength() {
  sSVGAnimatedLengthTearoffTable.RemoveTearoff(mVal);
}

UniquePtr<SMILAttr> SVGAnimatedLength::ToSMILAttr(SVGElement* aSVGElement) {
  return MakeUnique<SMILLength>(this, aSVGElement);
}

nsresult SVGAnimatedLength::SMILLength::ValueFromString(
    const nsAString& aStr, const SVGAnimationElement* /*aSrcElement*/,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  float value;
  uint16_t unitType;

  if (!GetValueFromString(aStr, value, &unitType)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  SMILValue val(SMILFloatType::Singleton());
  val.mU.mDouble = value * mVal->GetPixelsPerUnit(mSVGElement, unitType);
  aValue = val;
  aPreventCachingOfSandwich =
      (unitType == SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE ||
       unitType == SVGLength_Binding::SVG_LENGTHTYPE_EMS ||
       unitType == SVGLength_Binding::SVG_LENGTHTYPE_EXS);

  return NS_OK;
}

SMILValue SVGAnimatedLength::SMILLength::GetBaseValue() const {
  SMILValue val(SMILFloatType::Singleton());
  val.mU.mDouble = mVal->GetBaseValue(mSVGElement);
  return val;
}

void SVGAnimatedLength::SMILLength::ClearAnimValue() {
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimateLength(mVal->mAttrEnum);
  }
}

nsresult SVGAnimatedLength::SMILLength::SetAnimValue(const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == SMILFloatType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SMILFloatType::Singleton()) {
    return mVal->SetAnimValue(float(aValue.mU.mDouble), mSVGElement);
  }
  return NS_OK;
}

}  // namespace mozilla
