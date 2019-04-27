/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedOrient.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Move.h"
#include "mozilla/SMILValue.h"
#include "mozilla/dom/SVGMarkerElement.h"
#include "DOMSVGAnimatedAngle.h"
#include "DOMSVGAngle.h"
#include "nsContentUtils.h"
#include "nsTextFormatter.h"
#include "SVGAttrTearoffTable.h"
#include "SVGOrientSMILType.h"

using namespace mozilla::dom;
using namespace mozilla::dom::SVGAngle_Binding;
using namespace mozilla::dom::SVGMarkerElement_Binding;

namespace mozilla {

static const nsStaticAtom* const angleUnitMap[] = {
    nullptr, /* SVG_ANGLETYPE_UNKNOWN */
    nullptr, /* SVG_ANGLETYPE_UNSPECIFIED */
    nsGkAtoms::deg, nsGkAtoms::rad, nsGkAtoms::grad};

static SVGAttrTearoffTable<SVGAnimatedOrient, DOMSVGAnimatedEnumeration>
    sSVGAnimatedEnumTearoffTable;
static SVGAttrTearoffTable<SVGAnimatedOrient, DOMSVGAnimatedAngle>
    sSVGAnimatedAngleTearoffTable;
static SVGAttrTearoffTable<SVGAnimatedOrient, DOMSVGAngle>
    sBaseSVGAngleTearoffTable;
static SVGAttrTearoffTable<SVGAnimatedOrient, DOMSVGAngle>
    sAnimSVGAngleTearoffTable;

/* Helper functions */

//----------------------------------------------------------------------
// Helper class: AutoChangeOrientNotifier
// Stack-based helper class to pair calls to WillChangeOrient and
// DidChangeOrient.
class MOZ_RAII AutoChangeOrientNotifier {
 public:
  explicit AutoChangeOrientNotifier(
      SVGAnimatedOrient* aOrient, SVGElement* aSVGElement,
      bool aDoSetAttr = true MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mOrient(aOrient), mSVGElement(aSVGElement), mDoSetAttr(aDoSetAttr) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mOrient, "Expecting non-null orient");
    if (mSVGElement && mDoSetAttr) {
      mEmptyOrOldValue = mSVGElement->WillChangeOrient();
    }
  }

  ~AutoChangeOrientNotifier() {
    if (mSVGElement) {
      if (mDoSetAttr) {
        mSVGElement->DidChangeOrient(mEmptyOrOldValue);
      }
      if (mOrient->mIsAnimated) {
        mSVGElement->AnimationNeedsResample();
      }
    }
  }

 private:
  SVGAnimatedOrient* const mOrient;
  SVGElement* const mSVGElement;
  nsAttrValue mEmptyOrOldValue;
  bool mDoSetAttr;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

static bool IsValidAngleUnitType(uint16_t unit) {
  return unit > SVG_ANGLETYPE_UNKNOWN && unit <= SVG_ANGLETYPE_GRAD;
}

static void GetAngleUnitString(nsAString& unit, uint16_t unitType) {
  if (IsValidAngleUnitType(unitType)) {
    if (angleUnitMap[unitType]) {
      angleUnitMap[unitType]->ToString(unit);
    }
    return;
  }

  MOZ_ASSERT_UNREACHABLE("Unknown unit type");
}

static uint16_t GetAngleUnitTypeForString(const nsAString& unitStr) {
  if (unitStr.IsEmpty()) return SVG_ANGLETYPE_UNSPECIFIED;

  nsStaticAtom* unitAtom = NS_GetStaticAtom(unitStr);

  if (unitAtom) {
    for (uint32_t i = 0; i < ArrayLength(angleUnitMap); i++) {
      if (angleUnitMap[i] == unitAtom) {
        return i;
      }
    }
  }

  return SVG_ANGLETYPE_UNKNOWN;
}

static void GetAngleValueString(nsAString& aValueAsString, float aValue,
                                uint16_t aUnitType) {
  nsTextFormatter::ssprintf(aValueAsString, u"%g", (double)aValue);

  nsAutoString unitString;
  GetAngleUnitString(unitString, aUnitType);
  aValueAsString.Append(unitString);
}

/* static */
bool SVGAnimatedOrient::GetValueFromString(const nsAString& aString,
                                           float& aValue, uint16_t* aUnitType) {
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
  *aUnitType = GetAngleUnitTypeForString(units);
  return IsValidAngleUnitType(*aUnitType);
}

/* static */
float SVGAnimatedOrient::GetDegreesPerUnit(uint8_t aUnit) {
  switch (aUnit) {
    case SVG_ANGLETYPE_UNSPECIFIED:
    case SVG_ANGLETYPE_DEG:
      return 1;
    case SVG_ANGLETYPE_RAD:
      return static_cast<float>(180.0 / M_PI);
    case SVG_ANGLETYPE_GRAD:
      return 90.0f / 100.0f;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown unit type");
      return 0;
  }
}

void SVGAnimatedOrient::SetBaseValueInSpecifiedUnits(float aValue,
                                                     SVGElement* aSVGElement) {
  if (mBaseVal == aValue && mBaseType == SVG_MARKER_ORIENT_ANGLE) {
    return;
  }

  AutoChangeOrientNotifier notifier(this, aSVGElement);

  mBaseVal = aValue;
  mBaseType = SVG_MARKER_ORIENT_ANGLE;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
    mAnimType = mBaseType;
  }
}

nsresult SVGAnimatedOrient::ConvertToSpecifiedUnits(uint16_t unitType,
                                                    SVGElement* aSVGElement) {
  if (!IsValidAngleUnitType(unitType)) return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mBaseValUnit == uint8_t(unitType) &&
      mBaseType == SVG_MARKER_ORIENT_ANGLE) {
    return NS_OK;
  }

  float valueInUserUnits = mBaseVal * GetDegreesPerUnit(mBaseValUnit);
  SetBaseValue(valueInUserUnits, unitType, aSVGElement, true);

  return NS_OK;
}

nsresult SVGAnimatedOrient::NewValueSpecifiedUnits(uint16_t aUnitType,
                                                   float aValueInSpecifiedUnits,
                                                   SVGElement* aSVGElement) {
  NS_ENSURE_FINITE(aValueInSpecifiedUnits, NS_ERROR_ILLEGAL_VALUE);

  if (!IsValidAngleUnitType(aUnitType)) return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mBaseVal == aValueInSpecifiedUnits &&
      mBaseValUnit == uint8_t(aUnitType) &&
      mBaseType == SVG_MARKER_ORIENT_ANGLE)
    return NS_OK;

  AutoChangeOrientNotifier notifier(this, aSVGElement);

  mBaseVal = aValueInSpecifiedUnits;
  mBaseValUnit = uint8_t(aUnitType);
  mBaseType = SVG_MARKER_ORIENT_ANGLE;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
    mAnimValUnit = mBaseValUnit;
    mAnimType = mBaseType;
  }
  return NS_OK;
}

already_AddRefed<DOMSVGAngle> SVGAnimatedOrient::ToDOMBaseVal(
    SVGElement* aSVGElement) {
  RefPtr<DOMSVGAngle> domBaseVal = sBaseSVGAngleTearoffTable.GetTearoff(this);
  if (!domBaseVal) {
    domBaseVal = new DOMSVGAngle(this, aSVGElement, DOMSVGAngle::BaseValue);
    sBaseSVGAngleTearoffTable.AddTearoff(this, domBaseVal);
  }

  return domBaseVal.forget();
}

already_AddRefed<DOMSVGAngle> SVGAnimatedOrient::ToDOMAnimVal(
    SVGElement* aSVGElement) {
  RefPtr<DOMSVGAngle> domAnimVal = sAnimSVGAngleTearoffTable.GetTearoff(this);
  if (!domAnimVal) {
    domAnimVal = new DOMSVGAngle(this, aSVGElement, DOMSVGAngle::AnimValue);
    sAnimSVGAngleTearoffTable.AddTearoff(this, domAnimVal);
  }

  return domAnimVal.forget();
}

DOMSVGAngle::~DOMSVGAngle() {
  if (mType == BaseValue) {
    sBaseSVGAngleTearoffTable.RemoveTearoff(mVal);
  } else if (mType == AnimValue) {
    sAnimSVGAngleTearoffTable.RemoveTearoff(mVal);
  } else {
    delete mVal;
  }
}

/* Implementation */

nsresult SVGAnimatedOrient::SetBaseValueString(const nsAString& aValueAsString,
                                               SVGElement* aSVGElement,
                                               bool aDoSetAttr) {
  uint8_t type;
  float value;
  uint16_t unitType;
  bool valueChanged = false;

  if (aValueAsString.EqualsLiteral("auto")) {
    type = SVG_MARKER_ORIENT_AUTO;
    if (type == mBaseType) {
      return NS_OK;
    }
  } else if (aValueAsString.EqualsLiteral("auto-start-reverse")) {
    type = SVG_MARKER_ORIENT_AUTO_START_REVERSE;
    if (type == mBaseType) {
      return NS_OK;
    }
  } else {
    if (!GetValueFromString(aValueAsString, value, &unitType)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
    if (mBaseVal == value && mBaseValUnit == uint8_t(unitType) &&
        mBaseType == SVG_MARKER_ORIENT_ANGLE) {
      return NS_OK;
    }
    valueChanged = true;
  }

  AutoChangeOrientNotifier notifier(this, aSVGElement, aDoSetAttr);

  if (valueChanged) {
    mBaseVal = value;
    mBaseValUnit = uint8_t(unitType);
    mBaseType = SVG_MARKER_ORIENT_ANGLE;
  } else {
    mBaseVal = .0f;
    mBaseValUnit = SVG_ANGLETYPE_UNSPECIFIED;
    mBaseType = type;
  }

  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
    mAnimValUnit = mBaseValUnit;
    mAnimType = mBaseType;
  }

  return NS_OK;
}

void SVGAnimatedOrient::GetBaseValueString(nsAString& aValueAsString) const {
  switch (mBaseType) {
    case SVG_MARKER_ORIENT_AUTO:
      aValueAsString.AssignLiteral("auto");
      return;
    case SVG_MARKER_ORIENT_AUTO_START_REVERSE:
      aValueAsString.AssignLiteral("auto-start-reverse");
      return;
  }
  GetAngleValueString(aValueAsString, mBaseVal, mBaseValUnit);
}

void SVGAnimatedOrient::GetBaseAngleValueString(
    nsAString& aValueAsString) const {
  GetAngleValueString(aValueAsString, mBaseVal, mBaseValUnit);
}

void SVGAnimatedOrient::GetAnimAngleValueString(
    nsAString& aValueAsString) const {
  GetAngleValueString(aValueAsString, mAnimVal, mAnimValUnit);
}

void SVGAnimatedOrient::SetBaseValue(float aValue, uint8_t aUnit,
                                     SVGElement* aSVGElement, bool aDoSetAttr) {
  float valueInSpecifiedUnits = aValue / GetDegreesPerUnit(aUnit);
  if (aUnit == mBaseValUnit && mBaseVal == valueInSpecifiedUnits &&
      mBaseType == SVG_MARKER_ORIENT_ANGLE) {
    return;
  }

  AutoChangeOrientNotifier notifier(this, aSVGElement, aDoSetAttr);

  mBaseValUnit = aUnit;
  mBaseVal = valueInSpecifiedUnits;
  mBaseType = SVG_MARKER_ORIENT_ANGLE;
  if (!mIsAnimated) {
    mAnimValUnit = mBaseValUnit;
    mAnimVal = mBaseVal;
    mAnimType = mBaseType;
  }
}

nsresult SVGAnimatedOrient::SetBaseType(SVGEnumValue aValue,
                                        SVGElement* aSVGElement) {
  if (mBaseType == aValue) {
    return NS_OK;
  }
  if (aValue == SVG_MARKER_ORIENT_AUTO || aValue == SVG_MARKER_ORIENT_ANGLE ||
      aValue == SVG_MARKER_ORIENT_AUTO_START_REVERSE) {
    AutoChangeOrientNotifier notifier(this, aSVGElement);

    mBaseVal = .0f;
    mBaseValUnit = SVG_ANGLETYPE_UNSPECIFIED;
    mBaseType = aValue;
    if (!mIsAnimated) {
      mAnimVal = mBaseVal;
      mAnimValUnit = mBaseValUnit;
      mAnimType = mBaseType;
    }
    return NS_OK;
  }
  return NS_ERROR_DOM_TYPE_ERR;
}

void SVGAnimatedOrient::SetAnimValue(float aValue, uint8_t aUnit,
                                     SVGElement* aSVGElement) {
  if (mIsAnimated && mAnimVal == aValue && mAnimValUnit == aUnit &&
      mAnimType == SVG_MARKER_ORIENT_ANGLE) {
    return;
  }
  mAnimVal = aValue;
  mAnimValUnit = aUnit;
  mAnimType = SVG_MARKER_ORIENT_ANGLE;
  mIsAnimated = true;
  aSVGElement->DidAnimateOrient();
}

void SVGAnimatedOrient::SetAnimType(SVGEnumValue aValue,
                                    SVGElement* aSVGElement) {
  if (mIsAnimated && mAnimType == aValue) {
    return;
  }
  mAnimVal = .0f;
  mAnimValUnit = SVG_ANGLETYPE_UNSPECIFIED;
  mAnimType = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateOrient();
}

already_AddRefed<DOMSVGAnimatedAngle> SVGAnimatedOrient::ToDOMAnimatedAngle(
    SVGElement* aSVGElement) {
  RefPtr<DOMSVGAnimatedAngle> domAnimatedAngle =
      sSVGAnimatedAngleTearoffTable.GetTearoff(this);
  if (!domAnimatedAngle) {
    domAnimatedAngle = new DOMSVGAnimatedAngle(this, aSVGElement);
    sSVGAnimatedAngleTearoffTable.AddTearoff(this, domAnimatedAngle);
  }

  return domAnimatedAngle.forget();
}

already_AddRefed<DOMSVGAnimatedEnumeration>
SVGAnimatedOrient::ToDOMAnimatedEnum(SVGElement* aSVGElement) {
  RefPtr<DOMSVGAnimatedEnumeration> domAnimatedEnum =
      sSVGAnimatedEnumTearoffTable.GetTearoff(this);
  if (!domAnimatedEnum) {
    domAnimatedEnum = new DOMAnimatedEnum(this, aSVGElement);
    sSVGAnimatedEnumTearoffTable.AddTearoff(this, domAnimatedEnum);
  }

  return domAnimatedEnum.forget();
}

DOMSVGAnimatedAngle::~DOMSVGAnimatedAngle() {
  sSVGAnimatedAngleTearoffTable.RemoveTearoff(mVal);
}

SVGAnimatedOrient::DOMAnimatedEnum::~DOMAnimatedEnum() {
  sSVGAnimatedEnumTearoffTable.RemoveTearoff(mVal);
}

// we want to avoid exposing SVG_MARKER_ORIENT_AUTO_START_REVERSE to
// Web content
uint16_t SVGAnimatedOrient::DOMAnimatedEnum::Sanitize(uint16_t aValue) {
  return aValue == dom::SVG_MARKER_ORIENT_AUTO_START_REVERSE
             ? dom::SVGMarkerElement_Binding::SVG_MARKER_ORIENT_UNKNOWN
             : aValue;
}

UniquePtr<SMILAttr> SVGAnimatedOrient::ToSMILAttr(SVGElement* aSVGElement) {
  if (aSVGElement->NodeInfo()->Equals(nsGkAtoms::marker, kNameSpaceID_SVG)) {
    return MakeUnique<SMILOrient>(this, aSVGElement);
  }
  // SMILOrient would not be useful for general angle attributes (also,
  // "orient" is the only animatable <angle>-valued attribute in SVG 1.1).
  MOZ_ASSERT_UNREACHABLE("Trying to animate unknown angle attribute.");
  return nullptr;
}

nsresult SVGAnimatedOrient::SMILOrient::ValueFromString(
    const nsAString& aStr, const SVGAnimationElement* /*aSrcElement*/,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  SMILValue val(&SVGOrientSMILType::sSingleton);
  if (aStr.EqualsLiteral("auto")) {
    val.mU.mOrient.mOrientType = SVG_MARKER_ORIENT_AUTO;
    val.mU.mOrient.mAngle = .0f;
    val.mU.mOrient.mUnit = SVG_ANGLETYPE_UNSPECIFIED;
  } else if (aStr.EqualsLiteral("auto-start-reverse")) {
    val.mU.mOrient.mOrientType = SVG_MARKER_ORIENT_AUTO_START_REVERSE;
    val.mU.mOrient.mAngle = .0f;
    val.mU.mOrient.mUnit = SVG_ANGLETYPE_UNSPECIFIED;
  } else {
    float value;
    uint16_t unitType;
    if (!GetValueFromString(aStr, value, &unitType)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
    val.mU.mOrient.mAngle = value;
    val.mU.mOrient.mUnit = unitType;
    val.mU.mOrient.mOrientType = SVG_MARKER_ORIENT_ANGLE;
  }
  aValue = std::move(val);
  aPreventCachingOfSandwich = false;

  return NS_OK;
}

SMILValue SVGAnimatedOrient::SMILOrient::GetBaseValue() const {
  SMILValue val(&SVGOrientSMILType::sSingleton);
  val.mU.mOrient.mAngle = mOrient->GetBaseValInSpecifiedUnits();
  val.mU.mOrient.mUnit = mOrient->GetBaseValueUnit();
  val.mU.mOrient.mOrientType = mOrient->mBaseType;
  return val;
}

void SVGAnimatedOrient::SMILOrient::ClearAnimValue() {
  if (mOrient->mIsAnimated) {
    mOrient->mIsAnimated = false;
    mOrient->mAnimVal = mOrient->mBaseVal;
    mOrient->mAnimValUnit = mOrient->mBaseValUnit;
    mOrient->mAnimType = mOrient->mBaseType;
    mSVGElement->DidAnimateOrient();
  }
}

nsresult SVGAnimatedOrient::SMILOrient::SetAnimValue(const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == &SVGOrientSMILType::sSingleton,
               "Unexpected type to assign animated value");

  if (aValue.mType == &SVGOrientSMILType::sSingleton) {
    if (aValue.mU.mOrient.mOrientType == SVG_MARKER_ORIENT_AUTO ||
        aValue.mU.mOrient.mOrientType == SVG_MARKER_ORIENT_AUTO_START_REVERSE) {
      mOrient->SetAnimType(aValue.mU.mOrient.mOrientType, mSVGElement);
    } else {
      mOrient->SetAnimValue(aValue.mU.mOrient.mAngle, aValue.mU.mOrient.mUnit,
                            mSVGElement);
    }
  }
  return NS_OK;
}

}  // namespace mozilla
