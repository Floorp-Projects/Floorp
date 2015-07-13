/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILPARSERUTILS_H_
#define NS_SMILPARSERUTILS_H_

#include "nsTArray.h"
#include "nsStringFwd.h"

class nsISMILAttr;
class nsSMILKeySpline;
class nsSMILTimeValue;
class nsSMILValue;
class nsSMILRepeatCount;
class nsSMILTimeValueSpecParams;

namespace mozilla {
namespace dom {
class SVGAnimationElement;
} // namespace dom
} // namespace mozilla

/**
 * Common parsing utilities for the SMIL module. There is little re-use here; it
 * simply serves to simplify other classes by moving parsing outside and to aid
 * unit testing.
 */
class nsSMILParserUtils
{
public:
  // Abstract helper-class for assisting in parsing |values| attribute
  class MOZ_STACK_CLASS GenericValueParser {
  public:
    virtual bool Parse(const nsAString& aValueStr) = 0;
  };

  static const nsDependentSubstring TrimWhitespace(const nsAString& aString);

  static bool ParseKeySplines(const nsAString& aSpec,
                              FallibleTArray<nsSMILKeySpline>& aKeySplines);

  // Used for parsing the |keyTimes| and |keyPoints| attributes.
  static bool ParseSemicolonDelimitedProgressList(const nsAString& aSpec,
                                                  bool aNonDecreasing,
                                                  FallibleTArray<double>& aArray);

  static bool ParseValues(const nsAString& aSpec,
                          const mozilla::dom::SVGAnimationElement* aSrcElement,
                          const nsISMILAttr& aAttribute,
                          FallibleTArray<nsSMILValue>& aValuesArray,
                          bool& aPreventCachingOfSandwich);

  // Generic method that will run some code on each sub-section of an animation
  // element's "values" list.
  static bool ParseValuesGeneric(const nsAString& aSpec,
                                 GenericValueParser& aParser);

  static bool ParseRepeatCount(const nsAString& aSpec,
                               nsSMILRepeatCount& aResult);

  static bool ParseTimeValueSpecParams(const nsAString& aSpec,
                                       nsSMILTimeValueSpecParams& aResult);

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
                              nsSMILTimeValue* aResult);

  /*
   * This method checks whether the given string looks like a negative number.
   * Specifically, it checks whether the string looks matches the pattern
   * "[whitespace]*-[numeral].*" If the string matches this pattern, this
   * method returns the index of the first character after the '-' sign
   * (i.e. the index of the absolute value).  If not, this method returns -1.
   */
  static int32_t CheckForNegativeNumber(const nsAString& aStr);
};

#endif // NS_SMILPARSERUTILS_H_
