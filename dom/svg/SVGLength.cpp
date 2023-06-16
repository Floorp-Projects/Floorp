/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGLength.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "nsTextFormatter.h"
#include "SVGContentUtils.h"
#include <limits>
#include <algorithm>

using namespace mozilla::dom;
using namespace mozilla::dom::SVGLength_Binding;

namespace mozilla {

void SVGLength::GetValueAsString(nsAString& aValue) const {
  nsTextFormatter::ssprintf(aValue, u"%g", (double)mValue);

  nsAutoString unitString;
  GetUnitString(unitString, mUnit);
  aValue.Append(unitString);
}

bool SVGLength::SetValueFromString(const nsAString& aString) {
  bool success;
  auto token = SVGContentUtils::GetAndEnsureOneToken(aString, success);

  if (!success) {
    return false;
  }

  RangedPtr<const char16_t> iter = SVGContentUtils::GetStartRangedPtr(token);
  const RangedPtr<const char16_t> end = SVGContentUtils::GetEndRangedPtr(token);

  float value;

  if (!SVGContentUtils::ParseNumber(iter, end, value)) {
    return false;
  }

  const nsAString& units = Substring(iter.get(), end.get());
  uint16_t unitType = GetUnitTypeForString(units);
  if (unitType == SVG_LENGTHTYPE_UNKNOWN) {
    return false;
  }
  mValue = value;
  mUnit = uint8_t(unitType);
  return true;
}

inline static bool IsAbsoluteUnit(uint8_t aUnit) {
  return aUnit >= SVGLength_Binding::SVG_LENGTHTYPE_CM &&
         aUnit <= SVGLength_Binding::SVG_LENGTHTYPE_PC;
}

/**
 * Helper to convert between different CSS absolute units without the need for
 * an element, which provides more flexibility at the DOM level (and without
 * the need for an intermediary conversion to user units, which avoids
 * unnecessary overhead and rounding error).
 *
 * Example usage: to find out how many centimeters there are per inch:
 *
 *   GetAbsUnitsPerAbsUnit(SVGLength_Binding::SVG_LENGTHTYPE_CM,
 *                         SVGLength_Binding::SVG_LENGTHTYPE_IN)
 */
inline static float GetAbsUnitsPerAbsUnit(uint8_t aUnits, uint8_t aPerUnit) {
  MOZ_ASSERT(IsAbsoluteUnit(aUnits), "Not a CSS absolute unit");
  MOZ_ASSERT(IsAbsoluteUnit(aPerUnit), "Not a CSS absolute unit");

  float CSSAbsoluteUnitConversionFactors[5][5] = {
      // columns: cm, mm, in, pt, pc
      // cm per...:
      {1.0f, 0.1f, 2.54f, 0.035277777777777778f, 0.42333333333333333f},
      // mm per...:
      {10.0f, 1.0f, 25.4f, 0.35277777777777778f, 4.2333333333333333f},
      // in per...:
      {0.39370078740157481f, 0.039370078740157481f, 1.0f, 0.013888888888888889f,
       0.16666666666666667f},
      // pt per...:
      {28.346456692913386f, 2.8346456692913386f, 72.0f, 1.0f, 12.0f},
      // pc per...:
      {2.3622047244094489f, 0.23622047244094489f, 6.0f, 0.083333333333333333f,
       1.0f}};

  // First absolute unit is SVG_LENGTHTYPE_CM = 6
  return CSSAbsoluteUnitConversionFactors[aUnits - 6][aPerUnit - 6];
}

float SVGLength::GetValueInSpecifiedUnit(uint8_t aUnit,
                                         const SVGElement* aElement,
                                         uint8_t aAxis) const {
  if (aUnit == mUnit) {
    return mValue;
  }
  if ((aUnit == SVGLength_Binding::SVG_LENGTHTYPE_NUMBER &&
       mUnit == SVGLength_Binding::SVG_LENGTHTYPE_PX) ||
      (aUnit == SVGLength_Binding::SVG_LENGTHTYPE_PX &&
       mUnit == SVGLength_Binding::SVG_LENGTHTYPE_NUMBER)) {
    return mValue;
  }
  if (IsAbsoluteUnit(aUnit) && IsAbsoluteUnit(mUnit)) {
    return mValue * GetAbsUnitsPerAbsUnit(aUnit, mUnit);
  }

  // Otherwise we do a two step conversion via user units. This can only
  // succeed if aElement is non-null (although that's not sufficient to
  // guarantee success).

  float userUnitsPerCurrentUnit = GetUserUnitsPerUnit(aElement, aAxis);
  float userUnitsPerNewUnit =
      SVGLength(0.0f, aUnit).GetUserUnitsPerUnit(aElement, aAxis);

  NS_ASSERTION(
      userUnitsPerCurrentUnit >= 0 || !std::isfinite(userUnitsPerCurrentUnit),
      "bad userUnitsPerCurrentUnit");
  NS_ASSERTION(userUnitsPerNewUnit >= 0 || !std::isfinite(userUnitsPerNewUnit),
               "bad userUnitsPerNewUnit");

  float value = mValue * userUnitsPerCurrentUnit / userUnitsPerNewUnit;

  // userUnitsPerCurrentUnit could be infinity, or userUnitsPerNewUnit could
  // be zero.
  if (std::isfinite(value)) {
    return value;
  }
  return std::numeric_limits<float>::quiet_NaN();
}

#define INCHES_PER_MM_FLOAT float(0.0393700787)
#define INCHES_PER_CM_FLOAT float(0.393700787)

float SVGLength::GetUserUnitsPerUnit(const SVGElement* aElement,
                                     uint8_t aAxis) const {
  switch (mUnit) {
    case SVGLength_Binding::SVG_LENGTHTYPE_NUMBER:
    case SVGLength_Binding::SVG_LENGTHTYPE_PX:
      return 1.0f;
    case SVGLength_Binding::SVG_LENGTHTYPE_MM:
      return INCHES_PER_MM_FLOAT * GetUserUnitsPerInch();
    case SVGLength_Binding::SVG_LENGTHTYPE_CM:
      return INCHES_PER_CM_FLOAT * GetUserUnitsPerInch();
    case SVGLength_Binding::SVG_LENGTHTYPE_IN:
      return GetUserUnitsPerInch();
    case SVGLength_Binding::SVG_LENGTHTYPE_PT:
      return (1.0f / POINTS_PER_INCH_FLOAT) * GetUserUnitsPerInch();
    case SVGLength_Binding::SVG_LENGTHTYPE_PC:
      return (12.0f / POINTS_PER_INCH_FLOAT) * GetUserUnitsPerInch();
    case SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE:
      return GetUserUnitsPerPercent(aElement, aAxis);
    case SVGLength_Binding::SVG_LENGTHTYPE_EMS:
      return SVGContentUtils::GetFontSize(const_cast<SVGElement*>(aElement));
    case SVGLength_Binding::SVG_LENGTHTYPE_EXS:
      return SVGContentUtils::GetFontXHeight(const_cast<SVGElement*>(aElement));
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown unit type");
      return std::numeric_limits<float>::quiet_NaN();
  }
}

/* static */
float SVGLength::GetUserUnitsPerPercent(const SVGElement* aElement,
                                        uint8_t aAxis) {
  if (aElement) {
    dom::SVGViewportElement* viewportElement = aElement->GetCtx();
    if (viewportElement) {
      return std::max(viewportElement->GetLength(aAxis) / 100.0f, 0.0f);
    }
  }
  return std::numeric_limits<float>::quiet_NaN();
}

// Helpers:

/* static */
void SVGLength::GetUnitString(nsAString& aUnit, uint16_t aUnitType) {
  switch (aUnitType) {
    case SVG_LENGTHTYPE_NUMBER:
      aUnit.Truncate();
      return;
    case SVG_LENGTHTYPE_PERCENTAGE:
      aUnit.AssignLiteral("%");
      return;
    case SVG_LENGTHTYPE_EMS:
      aUnit.AssignLiteral("em");
      return;
    case SVG_LENGTHTYPE_EXS:
      aUnit.AssignLiteral("ex");
      return;
    case SVG_LENGTHTYPE_PX:
      aUnit.AssignLiteral("px");
      return;
    case SVG_LENGTHTYPE_CM:
      aUnit.AssignLiteral("cm");
      return;
    case SVG_LENGTHTYPE_MM:
      aUnit.AssignLiteral("mm");
      return;
    case SVG_LENGTHTYPE_IN:
      aUnit.AssignLiteral("in");
      return;
    case SVG_LENGTHTYPE_PT:
      aUnit.AssignLiteral("pt");
      return;
    case SVG_LENGTHTYPE_PC:
      aUnit.AssignLiteral("pc");
      return;
  }
  MOZ_ASSERT_UNREACHABLE(
      "Unknown unit type! Someone's using an SVGLength "
      "with an invalid unit?");
}

/* static */
uint16_t SVGLength::GetUnitTypeForString(const nsAString& aUnit) {
  if (aUnit.IsEmpty()) {
    return SVG_LENGTHTYPE_NUMBER;
  }
  if (aUnit.EqualsLiteral("%")) {
    return SVG_LENGTHTYPE_PERCENTAGE;
  }
  if (aUnit.LowerCaseEqualsLiteral("em")) {
    return SVG_LENGTHTYPE_EMS;
  }
  if (aUnit.LowerCaseEqualsLiteral("ex")) {
    return SVG_LENGTHTYPE_EXS;
  }
  if (aUnit.LowerCaseEqualsLiteral("px")) {
    return SVG_LENGTHTYPE_PX;
  }
  if (aUnit.LowerCaseEqualsLiteral("cm")) {
    return SVG_LENGTHTYPE_CM;
  }
  if (aUnit.LowerCaseEqualsLiteral("mm")) {
    return SVG_LENGTHTYPE_MM;
  }
  if (aUnit.LowerCaseEqualsLiteral("in")) {
    return SVG_LENGTHTYPE_IN;
  }
  if (aUnit.LowerCaseEqualsLiteral("pt")) {
    return SVG_LENGTHTYPE_PT;
  }
  if (aUnit.LowerCaseEqualsLiteral("pc")) {
    return SVG_LENGTHTYPE_PC;
  }
  return SVG_LENGTHTYPE_UNKNOWN;
}

}  // namespace mozilla
