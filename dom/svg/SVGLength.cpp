/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGLength.h"

#include "mozilla/dom/SVGElement.h"
#include "nsCSSValue.h"
#include "nsTextFormatter.h"
#include "SVGContentUtils.h"
#include <limits>
#include <algorithm>

using namespace mozilla::dom;
using namespace mozilla::dom::SVGLength_Binding;

namespace mozilla {

// These types are numbered so that different length categories are in
// contiguous ranges - See `SVGLength::Is[..]Unit()`.
const unsigned short SVG_LENGTHTYPE_Q = 11;
const unsigned short SVG_LENGTHTYPE_CH = 12;
const unsigned short SVG_LENGTHTYPE_REM = 13;
const unsigned short SVG_LENGTHTYPE_IC = 14;
const unsigned short SVG_LENGTHTYPE_CAP = 15;
const unsigned short SVG_LENGTHTYPE_LH = 16;
const unsigned short SVG_LENGTHTYPE_RLH = 17;
const unsigned short SVG_LENGTHTYPE_VW = 18;
const unsigned short SVG_LENGTHTYPE_VH = 19;
const unsigned short SVG_LENGTHTYPE_VMIN = 20;
const unsigned short SVG_LENGTHTYPE_VMAX = 21;

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

/*static*/
bool SVGLength::IsAbsoluteUnit(uint8_t aUnit) {
  return aUnit == SVG_LENGTHTYPE_NUMBER ||
         (aUnit >= SVG_LENGTHTYPE_PX && aUnit <= SVG_LENGTHTYPE_Q);
}

/*static*/
bool SVGLength::IsFontRelativeUnit(uint8_t aUnit) {
  return aUnit == SVG_LENGTHTYPE_EMS || aUnit == SVG_LENGTHTYPE_EXS ||
         (aUnit >= SVG_LENGTHTYPE_CH && aUnit <= SVG_LENGTHTYPE_RLH);
}

/**
 * Helper to convert between different CSS absolute units without the need for
 * an element, which provides more flexibility at the DOM level (and without
 * the need for an intermediary conversion to user units, which avoids
 * unnecessary overhead and rounding error).
 *
 * Example usage: to find out how many centimeters there are per inch:
 *
 *   GetAbsUnitsPerAbsUnit(SVG_LENGTHTYPE_CM, SVG_LENGTHTYPE_IN)
 */
/*static*/
float SVGLength::GetAbsUnitsPerAbsUnit(uint8_t aUnits, uint8_t aPerUnit) {
  MOZ_ASSERT(SVGLength::IsAbsoluteUnit(aUnits), "Not a CSS absolute unit");
  MOZ_ASSERT(SVGLength::IsAbsoluteUnit(aPerUnit), "Not a CSS absolute unit");

  static const float CSSAbsoluteUnitConversionFactors[7][7] = {
      // columns: px, cm, mm, in, pt, pc, q
      // px per...:
      {1.0f, 37.7952755906f, 3.779528f, 96.0f, 1.33333333333333333f, 16.0f,
       0.94488188988f},
      // cm per...:
      {0.02645833333f, 1.0f, 0.1f, 2.54f, 0.035277777777777778f,
       0.42333333333333333f, 0.025f},
      // mm per...:
      {0.26458333333f, 10.0f, 1.0f, 25.4f, 0.35277777777777778f,
       4.2333333333333333f, 0.25f},
      // in per...:
      {0.01041666666f, 0.39370078740157481f, 0.039370078740157481f, 1.0f,
       0.013888888888888889f, 0.16666666666666667f, 0.02204860853f},
      // pt per...:
      {0.75f, 28.346456692913386f, 2.8346456692913386f, 72.0f, 1.0f, 12.0f,
       0.70866141732f},
      // pc per...:
      {0.0625f, 2.3622047244094489f, 0.23622047244094489f, 6.0f,
       0.083333333333333333f, 1.0f, 16.9333333333f},
      // q per...:
      {1.0583333332f, 40.0f, 4.0f, 45.354336f, 1.41111111111f, 16.9333333333f,
       1.0f}};

  auto ToIndex = [](uint8_t aUnit) {
    return aUnit == SVG_LENGTHTYPE_NUMBER ? 0 : aUnit - 5;
  };

  return CSSAbsoluteUnitConversionFactors[ToIndex(aUnits)][ToIndex(aPerUnit)];
}

float SVGLength::GetValueInSpecifiedUnit(uint8_t aUnit,
                                         const SVGElement* aElement,
                                         uint8_t aAxis) const {
  if (aUnit == mUnit) {
    return mValue;
  }
  if ((aUnit == SVG_LENGTHTYPE_NUMBER && mUnit == SVG_LENGTHTYPE_PX) ||
      (aUnit == SVG_LENGTHTYPE_PX && mUnit == SVG_LENGTHTYPE_NUMBER)) {
    return mValue;
  }
  if (IsAbsoluteUnit(aUnit) && IsAbsoluteUnit(mUnit)) {
    return mValue * GetAbsUnitsPerAbsUnit(aUnit, mUnit);
  }

  // Otherwise we do a two step conversion via user units. This can only
  // succeed if aElement is non-null (although that's not sufficient to
  // guarantee success).

  SVGElementMetrics userSpaceMetrics(aElement);

  float userUnitsPerCurrentUnit = GetPixelsPerUnit(userSpaceMetrics, aAxis);
  float userUnitsPerNewUnit =
      SVGLength(0.0f, aUnit).GetPixelsPerUnit(userSpaceMetrics, aAxis);

  float value = mValue * userUnitsPerCurrentUnit / userUnitsPerNewUnit;

  // userUnitsPerCurrentUnit could be infinity, or userUnitsPerNewUnit could
  // be zero.
  if (std::isfinite(value)) {
    return value;
  }
  return std::numeric_limits<float>::quiet_NaN();
}

// Helpers:

enum class ZoomType { Self, SelfFromRoot, None };

/*static*/
float SVGLength::GetPixelsPerUnit(const UserSpaceMetrics& aMetrics,
                                  uint8_t aUnitType, uint8_t aAxis,
                                  bool aApplyZoom) {
  auto zoomType = ZoomType::Self;
  float value = [&]() -> float {
    switch (aUnitType) {
      case SVG_LENGTHTYPE_NUMBER:
      case SVG_LENGTHTYPE_PX:
        return 1.0f;
      case SVG_LENGTHTYPE_PERCENTAGE:
        zoomType = ZoomType::None;
        return aMetrics.GetAxisLength(aAxis) / 100.0f;
      case SVG_LENGTHTYPE_EMS:
        zoomType = ZoomType::None;
        return aMetrics.GetEmLength(UserSpaceMetrics::Type::This);
      case SVG_LENGTHTYPE_EXS:
        zoomType = ZoomType::None;
        return aMetrics.GetExLength(UserSpaceMetrics::Type::This);
      case SVG_LENGTHTYPE_CH:
        zoomType = ZoomType::None;
        return aMetrics.GetChSize(UserSpaceMetrics::Type::This);
      case SVG_LENGTHTYPE_REM:
        zoomType = ZoomType::SelfFromRoot;
        return aMetrics.GetEmLength(UserSpaceMetrics::Type::Root);
      case SVG_LENGTHTYPE_IC:
        zoomType = ZoomType::None;
        return aMetrics.GetIcWidth(UserSpaceMetrics::Type::This);
      case SVG_LENGTHTYPE_CAP:
        zoomType = ZoomType::None;
        return aMetrics.GetCapHeight(UserSpaceMetrics::Type::This);
      case SVG_LENGTHTYPE_VW:
        return aMetrics.GetCSSViewportSize().width / 100.f;
      case SVG_LENGTHTYPE_VH:
        return aMetrics.GetCSSViewportSize().height / 100.f;
      case SVG_LENGTHTYPE_VMIN: {
        auto sz = aMetrics.GetCSSViewportSize();
        return std::min(sz.width, sz.height) / 100.f;
      }
      case SVG_LENGTHTYPE_VMAX: {
        auto sz = aMetrics.GetCSSViewportSize();
        return std::max(sz.width, sz.height) / 100.f;
      }
      case SVG_LENGTHTYPE_LH:
        zoomType = ZoomType::None;
        return aMetrics.GetLineHeight(UserSpaceMetrics::Type::This);
      case SVG_LENGTHTYPE_RLH:
        zoomType = ZoomType::SelfFromRoot;
        return aMetrics.GetLineHeight(UserSpaceMetrics::Type::Root);
      default:
        MOZ_ASSERT(IsAbsoluteUnit(aUnitType));
        return GetAbsUnitsPerAbsUnit(SVG_LENGTHTYPE_PX, aUnitType);
    }
  }();
  if (aApplyZoom) {
    switch (zoomType) {
      case ZoomType::None:
        break;
      case ZoomType::Self:
        value *= aMetrics.GetZoom();
        break;
      case ZoomType::SelfFromRoot:
        value *= aMetrics.GetZoom() / aMetrics.GetRootZoom();
        break;
    }
  }
  return value;
}

/* static */
nsCSSUnit SVGLength::SpecifiedUnitTypeToCSSUnit(uint8_t aSpecifiedUnit) {
  switch (aSpecifiedUnit) {
    case SVG_LENGTHTYPE_NUMBER:
    case SVG_LENGTHTYPE_PX:
      return nsCSSUnit::eCSSUnit_Pixel;

    case SVG_LENGTHTYPE_MM:
      return nsCSSUnit::eCSSUnit_Millimeter;

    case SVG_LENGTHTYPE_CM:
      return nsCSSUnit::eCSSUnit_Centimeter;

    case SVG_LENGTHTYPE_IN:
      return nsCSSUnit::eCSSUnit_Inch;

    case SVG_LENGTHTYPE_PT:
      return nsCSSUnit::eCSSUnit_Point;

    case SVG_LENGTHTYPE_PC:
      return nsCSSUnit::eCSSUnit_Pica;

    case SVG_LENGTHTYPE_PERCENTAGE:
      return nsCSSUnit::eCSSUnit_Percent;

    case SVG_LENGTHTYPE_EMS:
      return nsCSSUnit::eCSSUnit_EM;

    case SVG_LENGTHTYPE_EXS:
      return nsCSSUnit::eCSSUnit_XHeight;

    case SVG_LENGTHTYPE_Q:
      return nsCSSUnit::eCSSUnit_Quarter;

    case SVG_LENGTHTYPE_CH:
      return nsCSSUnit::eCSSUnit_Char;

    case SVG_LENGTHTYPE_REM:
      return nsCSSUnit::eCSSUnit_RootEM;

    case SVG_LENGTHTYPE_IC:
      return nsCSSUnit::eCSSUnit_Ideographic;

    case SVG_LENGTHTYPE_CAP:
      return nsCSSUnit::eCSSUnit_CapHeight;

    case SVG_LENGTHTYPE_VW:
      return nsCSSUnit::eCSSUnit_VW;

    case SVG_LENGTHTYPE_VH:
      return nsCSSUnit::eCSSUnit_VH;

    case SVG_LENGTHTYPE_VMIN:
      return nsCSSUnit::eCSSUnit_VMin;

    case SVG_LENGTHTYPE_VMAX:
      return nsCSSUnit::eCSSUnit_VMax;

    case SVG_LENGTHTYPE_LH:
      return nsCSSUnit::eCSSUnit_LineHeight;

    case SVG_LENGTHTYPE_RLH:
      return nsCSSUnit::eCSSUnit_RootLineHeight;

    default:
      MOZ_ASSERT_UNREACHABLE("Unknown unit type");
      return nsCSSUnit::eCSSUnit_Pixel;
  }
}

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
    case SVG_LENGTHTYPE_Q:
      aUnit.AssignLiteral("q");
      return;
    case SVG_LENGTHTYPE_CH:
      aUnit.AssignLiteral("ch");
      return;
    case SVG_LENGTHTYPE_REM:
      aUnit.AssignLiteral("rem");
      return;
    case SVG_LENGTHTYPE_IC:
      aUnit.AssignLiteral("ic");
      return;
    case SVG_LENGTHTYPE_CAP:
      aUnit.AssignLiteral("cap");
      return;
    case SVG_LENGTHTYPE_VW:
      aUnit.AssignLiteral("vw");
      return;
    case SVG_LENGTHTYPE_VH:
      aUnit.AssignLiteral("vh");
      return;
    case SVG_LENGTHTYPE_VMIN:
      aUnit.AssignLiteral("vmin");
      return;
    case SVG_LENGTHTYPE_VMAX:
      aUnit.AssignLiteral("vmax");
      return;
    case SVG_LENGTHTYPE_LH:
      aUnit.AssignLiteral("lh");
      return;
    case SVG_LENGTHTYPE_RLH:
      aUnit.AssignLiteral("rlh");
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
  if (aUnit.LowerCaseEqualsLiteral("q")) {
    return SVG_LENGTHTYPE_Q;
  }
  if (aUnit.LowerCaseEqualsLiteral("ch")) {
    return SVG_LENGTHTYPE_CH;
  }
  if (aUnit.LowerCaseEqualsLiteral("rem")) {
    return SVG_LENGTHTYPE_REM;
  }
  if (aUnit.LowerCaseEqualsLiteral("ic")) {
    return SVG_LENGTHTYPE_IC;
  }
  if (aUnit.LowerCaseEqualsLiteral("cap")) {
    return SVG_LENGTHTYPE_CAP;
  }
  if (aUnit.LowerCaseEqualsLiteral("vw")) {
    return SVG_LENGTHTYPE_VW;
  }
  if (aUnit.LowerCaseEqualsLiteral("vh")) {
    return SVG_LENGTHTYPE_VH;
  }
  if (aUnit.LowerCaseEqualsLiteral("vmin")) {
    return SVG_LENGTHTYPE_VMIN;
  }
  if (aUnit.LowerCaseEqualsLiteral("vmax")) {
    return SVG_LENGTHTYPE_VMAX;
  }
  if (aUnit.LowerCaseEqualsLiteral("lh")) {
    return SVG_LENGTHTYPE_LH;
  }
  if (aUnit.LowerCaseEqualsLiteral("rlh")) {
    return SVG_LENGTHTYPE_RLH;
  }
  return SVG_LENGTHTYPE_UNKNOWN;
}

}  // namespace mozilla
