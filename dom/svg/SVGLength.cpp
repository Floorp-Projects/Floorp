/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "SVGLength.h"
#include "nsSVGElement.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "nsTextFormatter.h"
#include "SVGContentUtils.h"
#include <limits>
#include <algorithm>

namespace mozilla {

// Declare some helpers defined below:
static void GetUnitString(nsAString& unit, uint16_t unitType);
static uint16_t GetUnitTypeForString(const nsAString& unitStr);

void
SVGLength::GetValueAsString(nsAString &aValue) const
{
  char16_t buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(char16_t),
                            MOZ_UTF16("%g"),
                            (double)mValue);
  aValue.Assign(buf);

  nsAutoString unitString;
  GetUnitString(unitString, mUnit);
  aValue.Append(unitString);
}

bool
SVGLength::SetValueFromString(const nsAString &aString)
{
  RangedPtr<const char16_t> iter =
    SVGContentUtils::GetStartRangedPtr(aString);
  const RangedPtr<const char16_t> end =
    SVGContentUtils::GetEndRangedPtr(aString);

  float value;

  if (!SVGContentUtils::ParseNumber(iter, end, value)) {
    return false;
  }

  const nsAString& units = Substring(iter.get(), end.get());
  uint16_t unitType = GetUnitTypeForString(units);
  if (!IsValidUnitType(unitType)) {
    return false;
  }
  mValue = value;
  mUnit = uint8_t(unitType);
  return true;
}

inline static bool
IsAbsoluteUnit(uint8_t aUnit)
{
  return aUnit >= nsIDOMSVGLength::SVG_LENGTHTYPE_CM &&
         aUnit <= nsIDOMSVGLength::SVG_LENGTHTYPE_PC;
}

/**
 * Helper to convert between different CSS absolute units without the need for
 * an element, which provides more flexibility at the DOM level (and without
 * the need for an intermediary conversion to user units, which avoids
 * unnecessary overhead and rounding error).
 *
 * Example usage: to find out how many centimeters there are per inch:
 *
 *   GetAbsUnitsPerAbsUnit(nsIDOMSVGLength::SVG_LENGTHTYPE_CM,
 *                         nsIDOMSVGLength::SVG_LENGTHTYPE_IN)
 */
inline static float GetAbsUnitsPerAbsUnit(uint8_t aUnits, uint8_t aPerUnit)
{
  NS_ABORT_IF_FALSE(IsAbsoluteUnit(aUnits), "Not a CSS absolute unit");
  NS_ABORT_IF_FALSE(IsAbsoluteUnit(aPerUnit), "Not a CSS absolute unit");

  float CSSAbsoluteUnitConversionFactors[5][5] = { // columns: cm, mm, in, pt, pc
    // cm per...:
    { 1.0f, 0.1f, 2.54f, 0.035277777777777778f, 0.42333333333333333f },
    // mm per...:
    { 10.0f, 1.0f, 25.4f, 0.35277777777777778f, 4.2333333333333333f },
    // in per...:
    { 0.39370078740157481f, 0.039370078740157481f, 1.0f, 0.013888888888888889f, 0.16666666666666667f },
    // pt per...:
    { 28.346456692913386f, 2.8346456692913386f, 72.0f, 1.0f, 12.0f },
    // pc per...:
    { 2.3622047244094489f, 0.23622047244094489f, 6.0f, 0.083333333333333333f, 1.0f }
  };

  // First absolute unit is SVG_LENGTHTYPE_CM = 6
  return CSSAbsoluteUnitConversionFactors[aUnits - 6][aPerUnit - 6];
}

float
SVGLength::GetValueInSpecifiedUnit(uint8_t aUnit,
                                   const nsSVGElement *aElement,
                                   uint8_t aAxis) const
{
  if (aUnit == mUnit) {
    return mValue;
  }
  if ((aUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER &&
       mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_PX) ||
      (aUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_PX &&
       mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER)) {
    return mValue;
  }
  if (IsAbsoluteUnit(aUnit) && IsAbsoluteUnit(mUnit)) {
    return mValue * GetAbsUnitsPerAbsUnit(aUnit, mUnit);
  }

  // Otherwise we do a two step convertion via user units. This can only
  // succeed if aElement is non-null (although that's not sufficent to
  // guarantee success).

  float userUnitsPerCurrentUnit = GetUserUnitsPerUnit(aElement, aAxis);
  float userUnitsPerNewUnit =
    SVGLength(0.0f, aUnit).GetUserUnitsPerUnit(aElement, aAxis);

  NS_ASSERTION(userUnitsPerCurrentUnit >= 0 ||
               !IsFinite(userUnitsPerCurrentUnit),
               "bad userUnitsPerCurrentUnit");
  NS_ASSERTION(userUnitsPerNewUnit >= 0 ||
               !IsFinite(userUnitsPerNewUnit),
               "bad userUnitsPerNewUnit");

  float value = mValue * userUnitsPerCurrentUnit / userUnitsPerNewUnit;

  // userUnitsPerCurrentUnit could be infinity, or userUnitsPerNewUnit could
  // be zero.
  if (IsFinite(value)) {
    return value;
  }
  return std::numeric_limits<float>::quiet_NaN();
}

#define INCHES_PER_MM_FLOAT float(0.0393700787)
#define INCHES_PER_CM_FLOAT float(0.393700787)

float
SVGLength::GetUserUnitsPerUnit(const nsSVGElement *aElement, uint8_t aAxis) const
{
  switch (mUnit) {
    case nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER:
    case nsIDOMSVGLength::SVG_LENGTHTYPE_PX:
      return 1.0f;
    case nsIDOMSVGLength::SVG_LENGTHTYPE_MM:
      return INCHES_PER_MM_FLOAT * GetUserUnitsPerInch();
    case nsIDOMSVGLength::SVG_LENGTHTYPE_CM:
      return INCHES_PER_CM_FLOAT * GetUserUnitsPerInch();
    case nsIDOMSVGLength::SVG_LENGTHTYPE_IN:
      return GetUserUnitsPerInch();
    case nsIDOMSVGLength::SVG_LENGTHTYPE_PT:
      return (1.0f/POINTS_PER_INCH_FLOAT) * GetUserUnitsPerInch();
    case nsIDOMSVGLength::SVG_LENGTHTYPE_PC:
      return (12.0f/POINTS_PER_INCH_FLOAT) * GetUserUnitsPerInch();
    case nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE:
      return GetUserUnitsPerPercent(aElement, aAxis);
    case nsIDOMSVGLength::SVG_LENGTHTYPE_EMS:
      return SVGContentUtils::GetFontSize(const_cast<nsSVGElement*>(aElement));
    case nsIDOMSVGLength::SVG_LENGTHTYPE_EXS:
      return SVGContentUtils::GetFontXHeight(const_cast<nsSVGElement*>(aElement));
    default:
      NS_NOTREACHED("Unknown unit type");
      return std::numeric_limits<float>::quiet_NaN();
  }
}

/* static */ float
SVGLength::GetUserUnitsPerPercent(const nsSVGElement *aElement, uint8_t aAxis)
{
  if (aElement) {
    dom::SVGSVGElement *viewportElement = aElement->GetCtx();
    if (viewportElement) {
      return std::max(viewportElement->GetLength(aAxis) / 100.0f, 0.0f);
    }
  }
  return std::numeric_limits<float>::quiet_NaN();
}

// Helpers:

// These items must be at the same index as the nsIDOMSVGLength constants!
static nsIAtom** const unitMap[] =
{
  nullptr, /* SVG_LENGTHTYPE_UNKNOWN */
  nullptr, /* SVG_LENGTHTYPE_NUMBER */
  &nsGkAtoms::percentage,
  &nsGkAtoms::em,
  &nsGkAtoms::ex,
  &nsGkAtoms::px,
  &nsGkAtoms::cm,
  &nsGkAtoms::mm,
  &nsGkAtoms::in,
  &nsGkAtoms::pt,
  &nsGkAtoms::pc
};

static void
GetUnitString(nsAString& unit, uint16_t unitType)
{
  if (SVGLength::IsValidUnitType(unitType)) {
    if (unitMap[unitType]) {
      (*unitMap[unitType])->ToString(unit);
    }
    return;
  }
  NS_NOTREACHED("Unknown unit type"); // Someone's using an SVGLength with an invalid unit?
  return;
}

static uint16_t
GetUnitTypeForString(const nsAString& unitStr)
{
  if (unitStr.IsEmpty())
    return nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER;

  nsIAtom* unitAtom = NS_GetStaticAtom(unitStr);

  if (unitAtom) {
    for (uint32_t i = 1 ; i < ArrayLength(unitMap) ; i++) {
      if (unitMap[i] && *unitMap[i] == unitAtom) {
        return i;
      }
    }
  }
  return nsIDOMSVGLength::SVG_LENGTHTYPE_UNKNOWN;
}

} // namespace mozilla
