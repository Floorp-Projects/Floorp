/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILPARSERUTILS_H_
#define NS_SMILPARSERUTILS_H_

#include "nscore.h"
#include "nsTArray.h"
#include "nsString.h"

class nsISMILAttr;
class nsSMILTimeValue;
class nsSMILValue;
class nsSMILRepeatCount;
class nsSMILTimeValueSpecParams;

namespace mozilla {
namespace dom {
class SVGAnimationElement;
}
}

/**
 * Common parsing utilities for the SMIL module. There is little re-use here; it
 * simply serves to simplify other classes by moving parsing outside and to aid
 * unit testing.
 */
class nsSMILParserUtils
{
public:
  // Abstract helper-class for assisting in parsing |values| attribute
  class GenericValueParser {
  public:
    virtual nsresult Parse(const nsAString& aValueStr) = 0;
  };

  static nsresult ParseKeySplines(const nsAString& aSpec,
                                  nsTArray<double>& aSplineArray);

  // Used for parsing the |keyTimes| and |keyPoints| attributes.
  static nsresult ParseSemicolonDelimitedProgressList(const nsAString& aSpec,
                                                      bool aNonDecreasing,
                                                      nsTArray<double>& aArray);

  static nsresult ParseValues(const nsAString& aSpec,
                              const mozilla::dom::SVGAnimationElement* aSrcElement,
                              const nsISMILAttr& aAttribute,
                              nsTArray<nsSMILValue>& aValuesArray,
                              bool& aPreventCachingOfSandwich);

  // Generic method that will run some code on each sub-section of an animation
  // element's "values" list.
  static nsresult ParseValuesGeneric(const nsAString& aSpec,
                                     GenericValueParser& aParser);

  static nsresult ParseRepeatCount(const nsAString& aSpec,
                                   nsSMILRepeatCount& aResult);

  static nsresult ParseTimeValueSpecParams(const nsAString& aSpec,
                                           nsSMILTimeValueSpecParams& aResult);


  // Used with ParseClockValue. Allow + or - before a clock value.
  static const int8_t kClockValueAllowSign       = 1;
  // Used with ParseClockValue. Allow "indefinite" in a clock value
  static const int8_t kClockValueAllowIndefinite = 2;

  /*
   * This method can actually parse more than a clock value as defined in the
   * SMIL Animation specification. It can also parse:
   *  - the + or - before an offset
   *  - the special value "indefinite"
   *  - the special value "media"
   *
   * Because the value "media" cannot be represented as part of an
   * nsSMILTimeValue and has different meanings depending on where it is used,
   * it is passed out as a separate parameter (which can be set to nullptr if the
   * media attribute is not allowed).
   *
   * @param aSpec    The string containing a clock value, e.g. "10s"
   * @param aResult  The parsed result. May be nullptr (e.g. if this method is
   *                 being called just to test if aSpec is a valid clock value).
   *                 [OUT]
   * @param aFlags   A combination of the kClockValue* bit flags OR'ed together
   *                 to define what additional syntax is allowed.
   * @param aIsMedia Optional out parameter which, if not null, will be set to
   *                 true if the value is the string "media", false
   *                 otherwise. If it is null, the string "media" is not
   *                 allowed.
   *
   * @return NS_OK if aSpec was successfully parsed as a valid clock value
   * (according to aFlags), an error code otherwise.
   */
  static nsresult ParseClockValue(const nsAString& aSpec,
                                  nsSMILTimeValue* aResult,
                                  uint32_t aFlags = 0,
                                  bool* aIsMedia = nullptr);

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
