/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "SVGLength.h"
#include "nsSVGElement.h"
#include "nsSVGSVGElement.h"
#include "nsString.h"
#include "nsSVGUtils.h"
#include "nsTextFormatter.h"
#include "prdtoa.h"
#include "nsMathUtils.h"
#include <limits>

namespace mozilla {

// Declare some helpers defined below:
static void GetUnitString(nsAString& unit, PRUint16 unitType);
static PRUint16 GetUnitTypeForString(const char* unitStr);

void
SVGLength::GetValueAsString(nsAString &aValue) const
{
  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
                            NS_LITERAL_STRING("%g").get(),
                            (double)mValue);
  aValue.Assign(buf);

  nsAutoString unitString;
  GetUnitString(unitString, mUnit);
  aValue.Append(unitString);
}

bool
SVGLength::SetValueFromString(const nsAString &aValue)
{
  float tmpValue;
  PRUint16 tmpUnit;

  NS_ConvertUTF16toUTF8 value(aValue);
  const char *str = value.get();

  while (*str != '\0' && IsSVGWhitespace(*str)) {
    ++str;
  }
  char *unit;
  tmpValue = float(PR_strtod(str, &unit));
  if (unit != str && NS_finite(tmpValue)) {
    char *theRest = unit;
    if (*unit != '\0' && !IsSVGWhitespace(*unit)) {
      while (*theRest != '\0' && !IsSVGWhitespace(*theRest)) {
        ++theRest;
      }
      nsCAutoString unitStr(unit, theRest - unit);
      tmpUnit = GetUnitTypeForString(unitStr.get());
      if (tmpUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_UNKNOWN) {
        // nsSVGUtils::ReportToConsole
        return false;
      }
    } else {
      tmpUnit = nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER;
    }
    while (*theRest && IsSVGWhitespace(*theRest)) {
      ++theRest;
    }
    if (!*theRest) {
      mValue = tmpValue;
      mUnit = tmpUnit;
      return true;
    }
  }
  return false;
}

inline static bool
IsAbsoluteUnit(PRUint8 aUnit)
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
inline static float GetAbsUnitsPerAbsUnit(PRUint8 aUnits, PRUint8 aPerUnit)
{
  NS_ABORT_IF_FALSE(IsAbsoluteUnit(aUnits), "Not a CSS absolute unit");
  NS_ABORT_IF_FALSE(IsAbsoluteUnit(aPerUnit), "Not a CSS absolute unit");

  float CSSAbsoluteUnitConversionFactors[5][5] = { // columns: cm, mm, in, pt, pc
    // cm per...:
    { 1.0, 0.1, 2.54, 0.035277777777777778, 0.42333333333333333 },
    // mm per...:
    { 10.0, 1.0, 25.4, 0.35277777777777778, 4.2333333333333333 },
    // in per...:
    { 0.39370078740157481, 0.039370078740157481, 1.0, 0.013888888888888889, 0.16666666666666667 },
    // pt per...:
    { 28.346456692913386, 2.8346456692913386, 72.0, 1.0, 12.0 },
    // pc per...:
    { 2.3622047244094489, 0.23622047244094489, 6.0, 0.083333333333333333, 1.0 }
  };

  // First absolute unit is SVG_LENGTHTYPE_CM = 6
  return CSSAbsoluteUnitConversionFactors[aUnits - 6][aPerUnit - 6];
}

float
SVGLength::GetValueInSpecifiedUnit(PRUint8 aUnit,
                                   const nsSVGElement *aElement,
                                   PRUint8 aAxis) const
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
               !NS_finite(userUnitsPerCurrentUnit),
               "bad userUnitsPerCurrentUnit");
  NS_ASSERTION(userUnitsPerNewUnit >= 0 ||
               !NS_finite(userUnitsPerNewUnit),
               "bad userUnitsPerNewUnit");

  float value = mValue * userUnitsPerCurrentUnit / userUnitsPerNewUnit;

  // userUnitsPerCurrentUnit could be infinity, or userUnitsPerNewUnit could
  // be zero.
  if (NS_finite(value)) {
    return value;
  }
  return std::numeric_limits<float>::quiet_NaN();
}

#define INCHES_PER_MM_FLOAT float(0.0393700787)
#define INCHES_PER_CM_FLOAT float(0.393700787)

float
SVGLength::GetUserUnitsPerUnit(const nsSVGElement *aElement, PRUint8 aAxis) const
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
      return nsSVGUtils::GetFontSize(const_cast<nsSVGElement*>(aElement));
    case nsIDOMSVGLength::SVG_LENGTHTYPE_EXS:
      return nsSVGUtils::GetFontXHeight(const_cast<nsSVGElement*>(aElement));
    default:
      NS_NOTREACHED("Unknown unit type");
      return std::numeric_limits<float>::quiet_NaN();
  }
}

/* static */ float
SVGLength::GetUserUnitsPerPercent(const nsSVGElement *aElement, PRUint8 aAxis)
{
  if (aElement) {
    nsSVGSVGElement *viewportElement = const_cast<nsSVGElement*>(aElement)->GetCtx();
    if (viewportElement) {
      return NS_MAX(viewportElement->GetLength(aAxis) / 100.0f, 0.0f);
    }
  }
  return std::numeric_limits<float>::quiet_NaN();
}

// Helpers:

// These items must be at the same index as the nsIDOMSVGLength constants!
static nsIAtom** const unitMap[] =
{
  nsnull, /* SVG_LENGTHTYPE_UNKNOWN */
  nsnull, /* SVG_LENGTHTYPE_NUMBER */
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
GetUnitString(nsAString& unit, PRUint16 unitType)
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

static PRUint16
GetUnitTypeForString(const char* unitStr)
{
  if (!unitStr || *unitStr == '\0')
    return nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER;

  nsCOMPtr<nsIAtom> unitAtom = do_GetAtom(unitStr);

  for (PRUint32 i = 1 ; i < ArrayLength(unitMap) ; i++) {
    if (unitMap[i] && *unitMap[i] == unitAtom) {
      return i;
    }
  }
  return nsIDOMSVGLength::SVG_LENGTHTYPE_UNKNOWN;
}

} // namespace mozilla
