/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SMIL_SMILPARSERUTILS_H_
#define DOM_SMIL_SMILPARSERUTILS_H_

#include "nsTArray.h"
#include "nsStringFwd.h"
#include "SMILTimeValue.h"

namespace mozilla {

class SMILAttr;
class SMILKeySpline;
class SMILRepeatCount;
class SMILTimeValueSpecParams;
class SMILValue;

namespace dom {
class SVGAnimationElement;
}  // namespace dom

/**
 * Common parsing utilities for the SMIL module. There is little re-use here; it
 * simply serves to simplify other classes by moving parsing outside and to aid
 * unit testing.
 */
class SMILParserUtils {
 public:
  // Abstract helper-class for assisting in parsing |values| attribute
  class MOZ_STACK_CLASS GenericValueParser {
   public:
    virtual bool Parse(const nsAString& aValueStr) = 0;
  };

  static const nsDependentSubstring TrimWhitespace(const nsAString& aString);

  static bool ParseKeySplines(const nsAString& aSpec,
                              FallibleTArray<SMILKeySpline>& aKeySplines);

  // Used for parsing the |keyTimes| and |keyPoints| attributes.
  static bool ParseSemicolonDelimitedProgressList(
      const nsAString& aSpec, bool aNonDecreasing,
      FallibleTArray<double>& aArray);

  static bool ParseValues(const nsAString& aSpec,
                          const mozilla::dom::SVGAnimationElement* aSrcElement,
                          const SMILAttr& aAttribute,
                          FallibleTArray<SMILValue>& aValuesArray,
                          bool& aPreventCachingOfSandwich);

  // Generic method that will run some code on each sub-section of an animation
  // element's "values" list.
  static bool ParseValuesGeneric(const nsAString& aSpec,
                                 GenericValueParser& aParser);

  static bool ParseRepeatCount(const nsAString& aSpec,
                               SMILRepeatCount& aResult);

  static bool ParseTimeValueSpecParams(const nsAString& aSpec,
                                       SMILTimeValueSpecParams& aResult);

  /*
   * Parses a clock value as defined in the SMIL Animation specification.
   * If parsing succeeds the returned value will be a non-negative, definite
   * time value i.e. IsDefinite will return true.
   *
   * @param aSpec    The string containing a clock value, e.g. "10s"
   * @param aResult  The parsed result. [OUT]
   * @return true if parsing succeeded, otherwise false.
   */
  static bool ParseClockValue(const nsAString& aSpec,
                              SMILTimeValue::Rounding aRounding,
                              SMILTimeValue* aResult);

  /*
   * This method checks whether the given string looks like a negative number.
   * Specifically, it checks whether the string looks matches the pattern
   * "[whitespace]*-[numeral].*" If the string matches this pattern, this
   * method returns the index of the first character after the '-' sign
   * (i.e. the index of the absolute value).  If not, this method returns -1.
   */
  static int32_t CheckForNegativeNumber(const nsAString& aStr);
};

}  // namespace mozilla

#endif  // DOM_SMIL_SMILPARSERUTILS_H_
