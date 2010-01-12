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

#include "nsSMILParserUtils.h"
#include "nsISMILAttr.h"
#include "nsSMILValue.h"
#include "nsSMILTimeValue.h"
#include "nsSMILTypes.h"
#include "nsSMILRepeatCount.h"
#include "nsString.h"
#include "prdtoa.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "prlong.h"

//------------------------------------------------------------------------------
// Helper functions and Constants

namespace {

const PRUint32 MSEC_PER_SEC  = 1000;
const PRUint32 MSEC_PER_MIN  = 1000 * 60;
const PRUint32 MSEC_PER_HOUR = 1000 * 60 * 60;

// NS_IS_SPACE relies on isspace which may return true for \xB and \xC but
// SMILANIM does not consider these characters to be whitespace.
inline PRBool
IsSpace(const PRUnichar c)
{
  return (c == 0x9 || c == 0xA || c == 0xD || c == 0x20);
}

template<class T>
inline void
SkipWsp(T& aStart, T aEnd)
{
  while (aStart != aEnd && IsSpace(*aStart)) {
    ++aStart;
  }
}

double
GetFloat(const char*& aStart, const char* aEnd, nsresult* aErrorCode)
{
  char *floatEnd;
  double value = PR_strtod(aStart, &floatEnd);

  nsresult rv;

  if (floatEnd == aStart || floatEnd > aEnd) {
    rv = NS_ERROR_FAILURE;
  } else {
    aStart = floatEnd;
    rv = NS_OK;
  }

  if (aErrorCode) {
    *aErrorCode = rv;
  }

  return value;
}

inline PRBool
ConsumeSubstring(const char*& aStart, const char* aEnd, const char* aSubstring)
{
  size_t substrLen = PL_strlen(aSubstring);

  if (static_cast<size_t>(aEnd - aStart) < substrLen)
    return PR_FALSE;

  PRBool result = PR_FALSE;

  if (PL_strstr(aStart, aSubstring) == aStart) {
    aStart += substrLen;
    result = PR_TRUE;
  }

  return result;
}

PRBool
ParseClockComponent(const char*& aStart,
                    const char* aEnd,
                    double& aResult,
                    PRBool& aIsReal,
                    PRBool& aCouldBeMin,
                    PRBool& aCouldBeSec)
{
  nsresult rv;
  const char* begin = aStart;
  double value = GetFloat(aStart, aEnd, &rv);

  // Check a number was found
  if (NS_FAILED(rv))
    return PR_FALSE;

  // Check that it's not expressed in exponential form
  size_t len = aStart - begin;
  PRBool isExp = (PL_strnpbrk(begin, "eE", len) != nsnull);
  if (isExp)
    return PR_FALSE;

  // Don't allow real numbers of the form "23."
  if (*(aStart - 1) == '.')
    return PR_FALSE;

  // Number looks good
  aResult = value;

  // Set some flags so we can check this number is valid once we know
  // whether it's an hour, minute string etc.
  aIsReal = (PL_strnchr(begin, '.', len) != nsnull);
  aCouldBeMin = (value < 60.0 && (len == 2));
  aCouldBeSec = (value < 60.0 ||
      (value == 60.0 && begin[0] == '5')); // Take care of rounding error
  aCouldBeSec &= (len >= 2 &&
      (begin[2] == '\0' || begin[2] == '.' || IsSpace(begin[2])));

  return PR_TRUE;
}

PRBool
ParseMetricMultiplicand(const char*& aStart,
                        const char* aEnd,
                        PRInt32& multiplicand)
{
  PRBool result = PR_FALSE;

  size_t len = aEnd - aStart;
  const char* cur = aStart;

  if (len) {
    switch (*cur++)
    {
      case 'h':
        multiplicand = MSEC_PER_HOUR;
        result = PR_TRUE;
        break;
      case 'm':
        if (len >= 2) {
          if (*cur == 's') {
            ++cur;
            multiplicand = 1;
            result = PR_TRUE;
          } else if (len >= 3 && *cur++ == 'i' && *cur++ == 'n') {
            multiplicand = MSEC_PER_MIN;
            result = PR_TRUE;
          }
        }
        break;
      case 's':
        multiplicand = MSEC_PER_SEC;
        result = PR_TRUE;
        break;
    }
  }

  if (result) {
    aStart = cur;
  }

  return result;
}

} // end anonymous namespace block

//------------------------------------------------------------------------------
// Implementation

nsresult
nsSMILParserUtils::ParseKeySplines(const nsAString& aSpec,
                                   nsTArray<double>& aSplineArray)
{
  nsresult rv = NS_OK;

  NS_ConvertUTF16toUTF8 spec(aSpec);
  const char* start = spec.BeginReading();
  const char* end = spec.EndReading();

  SkipWsp(start, end);

  int i = 0;

  while (start != end)
  {
    double value = GetFloat(start, end, &rv);
    if (NS_FAILED(rv))
      break;

    if (value > 1.0 || value < 0.0) {
      rv = NS_ERROR_FAILURE;
      break;
    }

    if (!aSplineArray.AppendElement(value)) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }

    ++i;

    SkipWsp(start, end);
    if (start == end)
      break;

    if (i % 4) {
      if (*start == ',') {
        ++start;
      }
    } else {
      if (*start != ';') {
        rv = NS_ERROR_FAILURE;
        break;
      }
      ++start;
    }

    SkipWsp(start, end);
  }

  if (i % 4) {
    rv = NS_ERROR_FAILURE; // wrong number of points
  }

  return rv;
}

nsresult
nsSMILParserUtils::ParseKeyTimes(const nsAString& aSpec,
                                 nsTArray<double>& aTimeArray)
{
  nsresult rv = NS_OK;

  NS_ConvertUTF16toUTF8 spec(aSpec);
  const char* start = spec.BeginReading();
  const char* end = spec.EndReading();

  SkipWsp(start, end);

  double previousValue = -1.0;

  while (start != end) {
    double value = GetFloat(start, end, &rv);
    if (NS_FAILED(rv))
      break;

    if (value > 1.0 || value < 0.0 || value < previousValue) {
      rv = NS_ERROR_FAILURE;
      break;
    }

    if (!aTimeArray.AppendElement(value)) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }
    previousValue = value;

    SkipWsp(start, end);
    if (start == end)
      break;

    if (*start++ != ';') {
      rv = NS_ERROR_FAILURE;
      break;
    }

    SkipWsp(start, end);
  }

  return rv;
}

nsresult
nsSMILParserUtils::ParseValues(const nsAString& aSpec,
                               const nsISMILAnimationElement* aSrcElement,
                               const nsISMILAttr& aAttribute,
                               nsTArray<nsSMILValue>& aValuesArray)
{
  nsresult rv = NS_ERROR_FAILURE;

  const PRUnichar* start = aSpec.BeginReading();
  const PRUnichar* end = aSpec.EndReading();
  const PRUnichar* substrEnd = nsnull;
  const PRUnichar* next = nsnull;

  while (start != end) {
    rv = NS_ERROR_FAILURE;

    SkipWsp(start, end);

    if (start == end || *start == ';')
      break;

    substrEnd = start;

    while (substrEnd != end && *substrEnd != ';') {
      ++substrEnd;
    }

    next = substrEnd;
    if (*substrEnd == ';') {
      ++next;
      if (next == end)
        break;
    }

    while (substrEnd != start && NS_IS_SPACE(*(substrEnd-1)))
      --substrEnd;

    nsSMILValue newValue;
    rv = aAttribute.ValueFromString(Substring(start, substrEnd),
                                    aSrcElement, newValue);
    if (NS_FAILED(rv))
      break;

    if (!aValuesArray.AppendElement(newValue)) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }

    rv = NS_OK;
    start = next;
  }

  return rv;
}

nsresult
nsSMILParserUtils::ParseRepeatCount(const nsAString& aSpec,
                                    nsSMILRepeatCount& aResult)
{
  nsresult rv = NS_OK;

  NS_ConvertUTF16toUTF8 spec(aSpec);
  const char* start = spec.BeginReading();
  const char* end = spec.EndReading();

  SkipWsp(start, end);

  if (start != end)
  {
    if (ConsumeSubstring(start, end, "indefinite")) {
      aResult.SetIndefinite();
    } else {
      double value = GetFloat(start, end, &rv);

      if (NS_SUCCEEDED(rv))
      {
        /* Repeat counts must be > 0 */
        if (value <= 0.0) {
          rv = NS_ERROR_FAILURE;
        } else {
          aResult = value;
        }
      }
    }

    /* Check for trailing junk */
    SkipWsp(start, end);
    if (start != end) {
      rv = NS_ERROR_FAILURE;
    }
  } else {
    /* Empty spec */
    rv = NS_ERROR_FAILURE;
  }

  if (NS_FAILED(rv)) {
    aResult.Unset();
  }

  return rv;
}

nsresult
nsSMILParserUtils::ParseClockValue(const nsAString& aSpec,
                                   nsSMILTimeValue* aResult,
                                   PRUint32 aFlags,   // = 0
                                   PRBool* aIsMedia)  // = nsnull
{
  nsSMILTime offset = 0L;
  double component = 0.0;

  PRInt8 sign = 0;
  PRUint8 colonCount = 0;

  PRBool started = PR_FALSE;
  PRBool isValid = PR_TRUE;

  PRInt32 metricMultiplicand = MSEC_PER_SEC;

  PRBool numIsReal = PR_FALSE;
  PRBool prevNumCouldBeMin = PR_FALSE;
  PRBool numCouldBeMin = PR_FALSE;
  PRBool numCouldBeSec = PR_FALSE;
  PRBool isIndefinite = PR_FALSE;

  if (aIsMedia) {
    *aIsMedia = PR_FALSE;
  }

  NS_ConvertUTF16toUTF8 spec(aSpec);
  const char* start = spec.BeginReading();
  const char* end = spec.EndReading();

  while (start != end) {
    if (IsSpace(*start)) {
      if (started) {
        ++start;
        break;
      }
      // else, we haven't started yet, ignore initial whitespace
      ++start;

    } else if ((aFlags & kClockValueAllowSign)
               && (*start == '+' || *start == '-')) {
      if (sign != 0) {
        // sign has already been set
        isValid = PR_FALSE;
        break;
      }

      if (started) {
        // sign appears in the middle of the string
        isValid = PR_FALSE;
        break;
      }

      sign = (*start == '+') ? 1 : -1;
      ++start;
    // The NS_IS_DIGIT etc. macros are not locale-specific
    } else if (NS_IS_DIGIT(*start)) {
      prevNumCouldBeMin = numCouldBeMin;

      if (!ParseClockComponent(start, end, component, numIsReal, numCouldBeMin,
                               numCouldBeSec)) {
        isValid = PR_FALSE;
        break;
      }

      started = PR_TRUE;
    } else if (*start == ':') {
      ++colonCount;

      // Neither minutes nor hours can be reals
      if (numIsReal) {
        isValid = PR_FALSE;
        break;
      }

      // Clock value can't start with a ':'
      if (!started) {
        isValid = PR_FALSE;
        break;
      }

      // Can't have more than two colons
      if (colonCount > 2) {
        isValid = PR_FALSE;
        break;
      }

      // Multiply the offset by 60 and add the last accumulated component
      offset = offset * 60 + PRInt64(component);

      component = 0.0;
      ++start;
    } else if (NS_IS_ALPHA(*start)) {
      if (colonCount > 0) {
        isValid = PR_FALSE;
        break;
      }

      if ((aFlags & kClockValueAllowIndefinite)
          && ConsumeSubstring(start, end, "indefinite")) {
        // We set a separate flag because we don't know what the state of the
        // passed in time value is and we shouldn't change it in the case of a
        // bad input string (so we can't initialise it to 0ms for example).
        isIndefinite = PR_TRUE;
        if (aResult) {
          aResult->SetIndefinite();
        }
      } else if (aIsMedia && ConsumeSubstring(start, end, "media")) {
        *aIsMedia = PR_TRUE;
      } else if (!ParseMetricMultiplicand(start, end, metricMultiplicand)) {
        isValid = PR_FALSE;
        break;
      }

      // Nothing must come after the string except whitespace
      break;
    } else {
      isValid = PR_FALSE;
      break;
    }
  }

  if (!started) {
    isValid = PR_FALSE;
  }

  // Process remainder of string (if any) to ensure it is only trailing
  // whitespace (embedded whitespace is not allowed)
  SkipWsp(start, end);
  if (start != end) {
    isValid = PR_FALSE;
  }

  // No more processing required if the value was "indefinite" or "media".
  if (isIndefinite || (aIsMedia && *aIsMedia))
    return NS_OK;

  // If there is more than one colon then the previous component must be a
  // correctly formatted minute (i.e. two digits between 00 and 59) and the
  // latest component must be a correctly formatted second (i.e. two digits
  // before the .)
  if (colonCount > 0 && (!prevNumCouldBeMin || !numCouldBeSec)) {
    isValid = PR_FALSE;
  }

  if (isValid) {
    // Tack on the last component
    if (colonCount > 0) {
      offset = offset * 60 * 1000;
      component *= 1000;
      // rounding
      component = (component >= 0) ? component + 0.5 : component - 0.5;
      offset += PRInt64(component);
    } else {
      component *= metricMultiplicand;
      // rounding
      component = (component >= 0) ? component + 0.5 : component - 0.5;
      offset = PRInt64(component);
    }

    if (aResult) {
      nsSMILTime millis = offset;

      if (sign == -1) {
        millis = -offset;
      }

      aResult->SetMillis(millis);
    }
  }

  return (isValid) ? NS_OK : NS_ERROR_FAILURE;
}

PRInt32
nsSMILParserUtils::CheckForNegativeNumber(const nsAString& aStr)
{
  PRInt32 absValLocation = -1;

  nsAString::const_iterator start, end;
  aStr.BeginReading(start);
  aStr.EndReading(end);

  // Skip initial whitespace
  SkipWsp(start, end);

  // Check for dash
  if (start != end && *start == '-') {
    ++start;
    // Check for numeric character
    if (start != end && NS_IS_DIGIT(*start)) {
      absValLocation = start.get() - start.start();
    }
  }
  return absValLocation;
}
