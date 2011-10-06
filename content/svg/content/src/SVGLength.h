/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZILLA_SVGLENGTH_H__
#define MOZILLA_SVGLENGTH_H__

#include "nsIDOMSVGLength.h"
#include "nsIContent.h"
#include "nsAString.h"
#include "nsMathUtils.h"

class nsSVGElement;

namespace mozilla {

/**
 * This SVGLength class is currently used for SVGLength *list* attributes only.
 * The class that is currently used for <length> attributes is nsSVGLength2.
 *
 * The member mUnit should always be valid, but the member mValue may be
 * numeric_limits<float>::quiet_NaN() under one circumstances (see the comment
 * in SetValueAndUnit below). Even if mValue is valid, some methods may return
 * numeric_limits<float>::quiet_NaN() if they involve a unit conversion that
 * fails - see comments below.
 *
 * The DOM wrapper class for this class is DOMSVGLength.
 */
class SVGLength
{
public:

  SVGLength()
#ifdef DEBUG
    : mValue(0.0f)
    , mUnit(nsIDOMSVGLength::SVG_LENGTHTYPE_UNKNOWN) // caught by IsValid()
#endif
  {}

  SVGLength(float aValue, PRUint8 aUnit)
    : mValue(aValue)
    , mUnit(aUnit)
  {
    NS_ASSERTION(IsValid(), "Constructed an invalid length");
  }

  SVGLength(const SVGLength &aOther)
    : mValue(aOther.mValue)
    , mUnit(aOther.mUnit)
  {}

  SVGLength& operator=(const SVGLength &rhs) {
    mValue = rhs.mValue;
    mUnit = rhs.mUnit;
    return *this;
  }

  bool operator==(const SVGLength &rhs) const {
    return mValue == rhs.mValue && mUnit == rhs.mUnit;
  }

  void GetValueAsString(nsAString& aValue) const;

  /**
   * This method returns PR_TRUE, unless there was a parse failure, in which
   * case it returns PR_FALSE (and the length is left unchanged).
   */
  bool SetValueFromString(const nsAString& aValue);

  /**
   * This will usually return a valid, finite number. There is one exception
   * though - see the comment in SetValueAndUnit().
   */
  float GetValueInCurrentUnits() const {
    return mValue;
  }

  PRUint8 GetUnit() const {
    return mUnit;
  }

  void SetValueInCurrentUnits(float aValue) {
    mValue = aValue;
    NS_ASSERTION(IsValid(), "Set invalid SVGLength");
  }

  void SetValueAndUnit(float aValue, PRUint8 aUnit) {
    mValue = aValue;
    mUnit = aUnit;

    // IsValid() should always be true, with one exception: if
    // SVGLengthListSMILType has to convert between unit types and the unit
    // conversion is undefined, it will end up passing in and setting
    // numeric_limits<float>::quiet_NaN(). Because of that we only check the
    // unit here, and allow mValue to be invalid. The painting code has to be
    // able to handle NaN anyway, since conversion to user units may fail in
    // general.

    NS_ASSERTION(IsValidUnitType(mUnit), "Set invalid SVGLength");
  }

  /**
   * If it's not possible to convert this length's value to user units, then
   * this method will return numeric_limits<float>::quiet_NaN().
   */
  float GetValueInUserUnits(const nsSVGElement *aElement, PRUint8 aAxis) const {
    return mValue * GetUserUnitsPerUnit(aElement, aAxis);
  }

  /**
   * Sets this length's value, converting the supplied user unit value to this
   * lengths *current* unit (i.e. leaving the length's unit unchanged).
   *
   * This method returns PR_TRUE, unless the user unit value couldn't be
   * converted to this length's current unit, in which case it returns PR_FALSE
   * (and the length is left unchanged).
   */
  bool SetFromUserUnitValue(float aUserUnitValue,
                              nsSVGElement *aElement,
                              PRUint8 aAxis) {
    float uuPerUnit = GetUserUnitsPerUnit(aElement, aAxis);
    float value = aUserUnitValue / uuPerUnit;
    if (uuPerUnit > 0 && NS_finite(value)) {
      mValue = value;
      NS_ASSERTION(IsValid(), "Set invalid SVGLength");
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  /**
   * Get this length's value in the units specified.
   *
   * This method returns numeric_limits<float>::quiet_NaN() if it is not
   * possible to convert the value to the specified unit.
   */
  float GetValueInSpecifiedUnit(PRUint8 aUnit,
                                const nsSVGElement *aElement,
                                PRUint8 aAxis) const;

  /**
   * Convert this length's value to the unit specified.
   *
   * This method returns PR_TRUE, unless it isn't possible to convert the
   * length to the specified unit. In that case the length is left unchanged
   * and this method returns PR_FALSE.
   */
  bool ConvertToUnit(PRUint32 aUnit, nsSVGElement *aElement, PRUint8 aAxis) {
    float val = GetValueInSpecifiedUnit(aUnit, aElement, aAxis);
    if (NS_finite(val)) {
      mValue = val;
      mUnit = aUnit;
      NS_ASSERTION(IsValid(), "Set invalid SVGLength");
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  bool IsPercentage() const {
    return mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE;
  }

  static bool IsValidUnitType(PRUint16 unit) {
    return unit > nsIDOMSVGLength::SVG_LENGTHTYPE_UNKNOWN &&
           unit <= nsIDOMSVGLength::SVG_LENGTHTYPE_PC;
  }

private:

#ifdef DEBUG
  bool IsValid() const {
    return NS_finite(mValue) && IsValidUnitType(mUnit);
  }
#endif

  /**
   * Returns the number of user units per current unit.
   *
   * This method returns numeric_limits<float>::quiet_NaN() if the conversion
   * factor between the length's current unit and user units is undefined (see
   * the comments for GetUserUnitsPerInch and GetUserUnitsPerPercent).
   */
  float GetUserUnitsPerUnit(const nsSVGElement *aElement, PRUint8 aAxis) const;

  /**
   * The conversion factor between user units (CSS px) and CSS inches is
   * constant: 96 px per inch.
   */
  static float GetUserUnitsPerInch()
  {
    return 96.0;
  }

  /**
   * The conversion factor between user units and percentage units depends on
   * aElement being non-null, and on aElement having a viewport element
   * ancestor with only appropriate SVG elements between aElement and that
   * ancestor. If that's not the case, then the conversion factor is undefined.
   *
   * This function returns a non-negative value if the conversion factor is
   * defined, otherwise it returns numeric_limits<float>::quiet_NaN().
   */
  static float GetUserUnitsPerPercent(const nsSVGElement *aElement, PRUint8 aAxis);

  float mValue;
  PRUint8 mUnit;
};

} // namespace mozilla

#endif // MOZILLA_SVGLENGTH_H__
