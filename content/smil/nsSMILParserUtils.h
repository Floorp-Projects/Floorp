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
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef NS_SMILPARSERUTILS_H_
#define NS_SMILPARSERUTILS_H_

#include "nscore.h"
#include "nsTArray.h"
#include "nsString.h"

class nsISMILAttr;
class nsISMILAnimationElement;
class nsSMILTimeValue;
class nsSMILValue;
class nsSMILRepeatCount;
class nsSMILTimeValueSpecParams;

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
                              const nsISMILAnimationElement* aSrcElement,
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
  static const PRInt8 kClockValueAllowSign       = 1;
  // Used with ParseClockValue. Allow "indefinite" in a clock value
  static const PRInt8 kClockValueAllowIndefinite = 2;

  /*
   * This method can actually parse more than a clock value as defined in the
   * SMIL Animation specification. It can also parse:
   *  - the + or - before an offset
   *  - the special value "indefinite"
   *  - the special value "media"
   *
   * Because the value "media" cannot be represented as part of an
   * nsSMILTimeValue and has different meanings depending on where it is used,
   * it is passed out as a separate parameter (which can be set to nsnull if the
   * media attribute is not allowed).
   *
   * @param aSpec    The string containing a clock value, e.g. "10s"
   * @param aResult  The parsed result. May be NULL (e.g. if this method is
   *                 being called just to test if aSpec is a valid clock value).
   *                 [OUT]
   * @param aFlags   A combination of the kClockValue* bit flags OR'ed together
   *                 to define what additional syntax is allowed.
   * @param aIsMedia Optional out parameter which, if not null, will be set to
   *                 PR_TRUE if the value is the string "media", PR_FALSE
   *                 otherwise. If it is null, the string "media" is not
   *                 allowed.
   *
   * @return NS_OK if aSpec was successfully parsed as a valid clock value
   * (according to aFlags), an error code otherwise.
   */
  static nsresult ParseClockValue(const nsAString& aSpec,
                                  nsSMILTimeValue* aResult,
                                  PRUint32 aFlags = 0,
                                  bool* aIsMedia = nsnull);

  /*
   * This method checks whether the given string looks like a negative number.
   * Specifically, it checks whether the string looks matches the pattern
   * "[whitespace]*-[numeral].*" If the string matches this pattern, this
   * method returns the index of the first character after the '-' sign
   * (i.e. the index of the absolute value).  If not, this method returns -1.
   */
  static PRInt32 CheckForNegativeNumber(const nsAString& aStr);
};

#endif // NS_SMILPARSERUTILS_H_
