/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGLENGTH_H__
#define MOZILLA_SVGLENGTH_H__

#include "nsDebug.h"
#include "nsMathUtils.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/dom/SVGLengthBinding.h"

namespace mozilla {

namespace dom {
class SVGElement;
}

/**
 * This SVGLength class is currently used for SVGLength *list* attributes only.
 * The class that is currently used for <length> attributes is
 * SVGAnimatedLength.
 *
 * The member mUnit should always be valid, but the member mValue may be
 * numeric_limits<float>::quiet_NaN() under one circumstances (see the comment
 * in SetValueAndUnit below). Even if mValue is valid, some methods may return
 * numeric_limits<float>::quiet_NaN() if they involve a unit conversion that
 * fails - see comments below.
 *
 * The DOM wrapper class for this class is DOMSVGLength.
 */
class SVGLength {
 public:
  SVGLength()
      : mValue(0.0f),
        mUnit(dom::SVGLength_Binding::SVG_LENGTHTYPE_UNKNOWN)  // caught by
                                                               // IsValid()
  {}

  SVGLength(float aValue, uint8_t aUnit) : mValue(aValue), mUnit(aUnit) {
    NS_ASSERTION(IsValid(), "Constructed an invalid length");
  }

  bool operator==(const SVGLength& rhs) const {
    return mValue == rhs.mValue && mUnit == rhs.mUnit;
  }

  void GetValueAsString(nsAString& aValue) const;

  /**
   * This method returns true, unless there was a parse failure, in which
   * case it returns false (and the length is left unchanged).
   */
  bool SetValueFromString(const nsAString& aString);

  /**
   * This will usually return a valid, finite number. There is one exception
   * though - see the comment in SetValueAndUnit().
   */
  float GetValueInCurrentUnits() const { return mValue; }

  uint8_t GetUnit() const { return mUnit; }

  void SetValueInCurrentUnits(float aValue) {
    mValue = aValue;
    NS_ASSERTION(IsValid(), "Set invalid SVGLength");
  }

  void SetValueAndUnit(float aValue, uint8_t aUnit) {
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
  float GetValueInUserUnits(const dom::SVGElement* aElement,
                            uint8_t aAxis) const {
    return mValue * GetUserUnitsPerUnit(aElement, aAxis);
  }

  /**
   * Get this length's value in the units specified.
   *
   * This method returns numeric_limits<float>::quiet_NaN() if it is not
   * possible to convert the value to the specified unit.
   */
  float GetValueInSpecifiedUnit(uint8_t aUnit, const dom::SVGElement* aElement,
                                uint8_t aAxis) const;

  bool IsPercentage() const {
    return mUnit == dom::SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE;
  }

  static bool IsValidUnitType(uint16_t unit) {
    return unit > dom::SVGLength_Binding::SVG_LENGTHTYPE_UNKNOWN &&
           unit <= dom::SVGLength_Binding::SVG_LENGTHTYPE_PC;
  }

  /**
   * Returns the number of user units per current unit.
   *
   * This method returns numeric_limits<float>::quiet_NaN() if the conversion
   * factor between the length's current unit and user units is undefined (see
   * the comments for GetUserUnitsPerInch and GetUserUnitsPerPercent).
   */
  float GetUserUnitsPerUnit(const dom::SVGElement* aElement,
                            uint8_t aAxis) const;

 private:
#ifdef DEBUG
  bool IsValid() const { return IsFinite(mValue) && IsValidUnitType(mUnit); }
#endif

  /**
   * The conversion factor between user units (CSS px) and CSS inches is
   * constant: 96 px per inch.
   */
  static float GetUserUnitsPerInch() { return 96.0; }

  /**
   * The conversion factor between user units and percentage units depends on
   * aElement being non-null, and on aElement having a viewport element
   * ancestor with only appropriate SVG elements between aElement and that
   * ancestor. If that's not the case, then the conversion factor is undefined.
   *
   * This function returns a non-negative value if the conversion factor is
   * defined, otherwise it returns numeric_limits<float>::quiet_NaN().
   */
  static float GetUserUnitsPerPercent(const dom::SVGElement* aElement,
                                      uint8_t aAxis);

  float mValue;
  uint8_t mUnit;
};

}  // namespace mozilla

#endif  // MOZILLA_SVGLENGTH_H__
