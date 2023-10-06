/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGLENGTH_H_
#define DOM_SVG_SVGLENGTH_H_

#include "nsDebug.h"
#include "nsMathUtils.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/dom/SVGAnimatedLength.h"
#include "mozilla/dom/SVGLengthBinding.h"

enum nsCSSUnit : uint32_t;

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
      : mValue(0.0f), mUnit(dom::SVGLength_Binding::SVG_LENGTHTYPE_UNKNOWN) {}

  SVGLength(float aValue, uint8_t aUnit) : mValue(aValue), mUnit(aUnit) {}

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
   * though. If SVGLengthListSMILType has to convert between unit types and the
   * unit conversion is undefined, it will end up passing in and setting
   * numeric_limits<float>::quiet_NaN(). The painting code has to be
   * able to handle NaN anyway, since conversion to user units may fail in
   * general.
   */
  float GetValueInCurrentUnits() const { return mValue; }

  uint8_t GetUnit() const { return mUnit; }

  void SetValueInCurrentUnits(float aValue) {
    NS_ASSERTION(std::isfinite(aValue), "Set invalid SVGLength");
    mValue = aValue;
  }

  void SetValueAndUnit(float aValue, uint8_t aUnit) {
    mValue = aValue;
    mUnit = aUnit;
  }

  /**
   * If it's not possible to convert this length's value to pixels, then
   * this method will return numeric_limits<float>::quiet_NaN().
   */

  float GetValueInPixels(const dom::SVGElement* aElement, uint8_t aAxis) const {
    return mValue * GetPixelsPerUnit(dom::SVGElementMetrics(aElement), aAxis);
  }

  /**
   * Get this length's value in the units specified.
   *
   * This method returns numeric_limits<float>::quiet_NaN() if it is not
   * possible to convert the value to the specified unit.
   */
  float GetValueInSpecifiedUnit(uint8_t aUnit, const dom::SVGElement* aElement,
                                uint8_t aAxis) const;

  bool IsPercentage() const { return IsPercentageUnit(mUnit); }

  float GetPixelsPerUnit(const dom::UserSpaceMetrics& aMetrics,
                         uint8_t aAxis) const {
    return GetPixelsPerUnit(aMetrics, mUnit, aAxis);
  }

  static bool IsValidUnitType(uint16_t aUnitType) {
    return aUnitType > dom::SVGLength_Binding::SVG_LENGTHTYPE_UNKNOWN &&
           aUnitType <= dom::SVGLength_Binding::SVG_LENGTHTYPE_PC;
  }

  static bool IsPercentageUnit(uint8_t aUnit) {
    return aUnit == dom::SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE;
  }

  static bool IsAbsoluteUnit(uint8_t aUnit);

  static bool IsFontRelativeUnit(uint8_t aUnit);

  static float GetAbsUnitsPerAbsUnit(uint8_t aUnits, uint8_t aPerUnit);

  static nsCSSUnit SpecifiedUnitTypeToCSSUnit(uint8_t aSpecifiedUnit);

  static void GetUnitString(nsAString& aUnit, uint16_t aUnitType);

  static uint16_t GetUnitTypeForString(const nsAString& aUnit);

  /**
   * Returns the number of pixels per given unit.
   */
  static float GetPixelsPerUnit(const dom::UserSpaceMetrics& aMetrics,
                                uint8_t aUnitType, uint8_t aAxis);

 private:
  float mValue;
  uint8_t mUnit;
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGLENGTH_H_
