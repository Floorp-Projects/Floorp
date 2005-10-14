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
 * The Original Code is Mozilla Schema Validation.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Doron Rosenberg <doronr@us.ibm.com> (original author)
 *   Laurent Jouanneau <laurent@xulfr.org>
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

// string includes
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsUnicharUtils.h"

#include "nsISchema.h"
#include "nsSchemaValidator.h"
#include "nsSchemaValidatorUtils.h"
#include "nsISchemaValidatorRegexp.h"
#include "nsSchemaDuration.h"

#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include "prlog.h"
#include "prprf.h"
#include "prtime.h"
#include "prdtoa.h"

#ifdef PR_LOGGING
PRLogModuleInfo *gSchemaValidationUtilsLog = PR_NewLogModule("schemaValidation");

#define LOG(x) PR_LOG(gSchemaValidationUtilsLog, PR_LOG_DEBUG, x)
#define LOG_ENABLED() PR_LOG_TEST(gSchemaValidationUtilsLog, PR_LOG_DEBUG)
#else
#define LOG(x)
#endif

PRBool
nsSchemaValidatorUtils::IsValidSchemaInteger(const nsAString & aNodeValue,
                                             long *aResult)
{
  return !aNodeValue.IsEmpty() &&
         IsValidSchemaInteger(NS_ConvertUTF16toUTF8(aNodeValue).get(), aResult);
}

// overloaded, for char* rather than nsAString
PRBool
nsSchemaValidatorUtils::IsValidSchemaInteger(const char* aString, long *aResult)
{
  if (*aString == 0)
    return PR_FALSE;

  char * pEnd;
  long intValue = strtol(aString, &pEnd, 10);

  if (aResult)
    *aResult = intValue;

  return (!((intValue == LONG_MAX || intValue == LONG_MIN) && errno == ERANGE))
         && *pEnd == '\0';
}

PRBool
nsSchemaValidatorUtils::IsValidSchemaDouble(const nsAString & aNodeValue,
                                            double *aResult)
{
  return !aNodeValue.IsEmpty() &&
         IsValidSchemaDouble(NS_ConvertUTF16toUTF8(aNodeValue).get(), aResult);
}

// overloaded, for char* rather than nsAString
PRBool
nsSchemaValidatorUtils::IsValidSchemaDouble(const char* aString,
                                            double *aResult)
{
  if (*aString == 0)
    return PR_FALSE;

  char * pEnd;
  double value = PR_strtod(aString, &pEnd);

  if (aResult)
    *aResult = value;

  return (pEnd != aString);
}

PRBool
nsSchemaValidatorUtils::GetPRTimeFromDateTime(const nsAString & aNodeValue,
                                              PRTime *aResult)
{
  PRBool isValid = PR_FALSE;
  PRBool isNegativeYear = PR_FALSE;

  int run = 0;
  PRBool doneParsing = PR_FALSE;

  char year[80] = "";
  char month[3] = "";
  char day[3] = "";
  char hour[3] = "";
  char minute[3] = "";
  char second[3] = "";
  char fraction_seconds[80] = "";
  PRTime dateTime;

  char fulldate[100] = "";
  nsAutoString datetimeString(aNodeValue);

  if (datetimeString.First() == '-') {
    isNegativeYear = PR_TRUE;
    run = 1;
  }

  /*
    http://www.w3.org/TR/xmlschema-2/#dateTime
    (-)CCYY-MM-DDThh:mm:ss(.sss...)
      then either: Z
      or [+/-]hh:mm
  */

  // first handle the date part
  // search for 'T'

  LOG(("\n  Validating DateTime:"));

  int findString = datetimeString.FindChar('T');

  if (findString >= 0) {
    isValid = ParseSchemaDate(Substring(aNodeValue, 0, findString+1), year,
                              month, day);

    if (isValid) {
      isValid = ParseSchemaTime(
                  Substring(aNodeValue, findString + 1, aNodeValue.Length()),
                  hour, minute, second, fraction_seconds);
    }
  } else {
    // no T, invalid
    doneParsing = PR_TRUE;
  }

  if (isValid && aResult) {
    nsCAutoString monthShorthand;
    GetMonthShorthand(month, monthShorthand);

    // 22-AUG-1993 10:59:12.82
    sprintf(fulldate, "%s-%s-%s %s:%s:%s.%s",
      day,
      monthShorthand.get(),
      year,
      hour,
      minute,
      second,
      fraction_seconds);

    LOG(("\n    new date is %s", fulldate));
    PRStatus status = PR_ParseTimeString(fulldate, PR_TRUE, &dateTime);

    if (status == -1)
      isValid = PR_FALSE;
    else
      *aResult = dateTime;
  }

  return isValid;
}

PRBool
nsSchemaValidatorUtils::ParseSchemaDate(const nsAString & aStrValue,
                                        char *rv_year, char *rv_month,
                                        char *rv_day)
{
  PRBool isValid = PR_FALSE;

  /*
    http://www.w3.org/TR/xmlschema-2/#date
    (-)CCYY-MM-DDT
  */

  PRTime dateTime;

  nsAString::const_iterator start, end, buffStart;
  aStrValue.BeginReading(start);
  aStrValue.BeginReading(buffStart);
  aStrValue.EndReading(end);
  PRUint32 state = 0;
  PRUint32 buffLength = 0;
  PRBool done = PR_FALSE;
  PRUnichar currentChar;

  nsAutoString year;
  char month[3] = "";
  char day[3] = "";

  // if year is negative, skip it
  if (aStrValue.First() == '-') {
    start++;
    buffStart = start;
  }

  while ((start != end) && !done) {
    currentChar = *start++;
    switch (state) {
      case 0: {
        // year
        if (currentChar == '-') {
          year.Assign(Substring(buffStart, --start));
          state = 1;
          buffLength = 0;
          buffStart = ++start;
        } else {
          // has to be a numerical character or else abort
          if ((currentChar > '9') || (currentChar < '0'))
            done = PR_TRUE;
          buffLength++;
        }
        break;
      }

      case 1: {
        // month
        if (buffLength > 2) {
          done = PR_TRUE;
        } else if (currentChar == '-') {
          if (strcmp(month, "12") == 1) {
            done = PR_TRUE;
          } else {
            state = 2;
            buffLength = 0;
            buffStart = start;
          }
        } else {
          // has to be a numerical character or else abort
          if ((currentChar > '9') || (currentChar < '0'))
            done = PR_TRUE;
          else
            month[buffLength] = currentChar;
          buffLength++;
        }
        break;
      }

      case 2: {
        // day
        if (buffLength > 2) {
          done = PR_TRUE;
        } else if (currentChar == 'T') {
          if ((start == end) && (strcmp(day, "31") < 1))
            isValid = PR_TRUE;
          done = PR_TRUE;
        } else {
          // has to be a numerical character or else abort
          if ((currentChar > '9') || (currentChar < '0'))
            done = PR_TRUE;
          else {
            day[buffLength] = currentChar;
          }
          buffLength++;
        }
        break;
      }

    }
  }

  if (isValid) {
    nsCAutoString monthShorthand;
    nsSchemaValidatorUtils::GetMonthShorthand(month, monthShorthand);

    // 22-AUG-1993
    nsAutoString fullDate;
    fullDate.AppendLiteral(day);
    fullDate.AppendLiteral("-");
    AppendASCIItoUTF16(monthShorthand, fullDate);
    fullDate.AppendLiteral("-");
    fullDate.Append(year);

    LOG(("\n      Parsed date is %s", NS_ConvertUTF16toUTF8(fullDate).get()));

    PRStatus status = PR_ParseTimeString(NS_ConvertUTF16toUTF8(fullDate).get(),
                                         PR_TRUE, &dateTime);
    if (status == -1) {
      isValid = PR_FALSE;
    } else {
      // PRStatus will be 0 for feb 30th, but the returned day will be
      // different
      char * pEnd;
      long val = strtol(day, &pEnd, 10);

      PRExplodedTime explodedDateTime;
      PR_ExplodeTime(dateTime, PR_GMTParameters, &explodedDateTime);

      if (val != explodedDateTime.tm_mday) {
        isValid = PR_FALSE;
      }
    }

    if (isValid) {
      strcpy(rv_day, day);
      strcpy(rv_month, month);
      strcpy(rv_year, NS_ConvertUTF16toUTF8(year).get());
    }
  }

  LOG(("\n      Date is %s \n", ((isValid) ? "Valid" : "Not Valid")));

  return isValid;
}


// parses a string as a schema time type and returns the parsed
// hour/minute/second/fraction seconds as well as if its a valid
// schema time type.
PRBool
nsSchemaValidatorUtils::ParseSchemaTime(const nsAString & aStrValue,
                                        char *rv_hour, char *rv_minute,
                                        char *rv_second, char *rv_fraction_second)
{
  PRBool isValid = PR_FALSE;

  // time looks like this: HH:MM:SS(.[S]+)(+/-HH:MM)

  char timezoneHour[3] = "";
  char timezoneMinute[3] = "";
  // we store the fraction seconds because PR_ExplodeTime seems to skip them.
  nsAutoString usec;

  nsAString::const_iterator start, end, buffStart;
  aStrValue.BeginReading(start);
  aStrValue.BeginReading(buffStart);
  aStrValue.EndReading(end);
  PRUint32 state = 0;
  PRUint32 buffLength = 0;
  PRBool done = PR_FALSE;
  PRUnichar currentChar;
  PRUnichar tzSign = PRUnichar(' ');

  while ((start != end) && !done) {
    currentChar = *start++;

    switch (state) {
      case 0: {
        // hour
        if (buffLength > 2) {
          done = PR_TRUE;
        } else if (currentChar == ':') {
          // validate hour
          if (strcmp(NS_ConvertUTF16toUTF8(Substring(buffStart, --start)).get(), "24") == 1) {
            done = PR_TRUE;
          } else {
            state = 1;
            buffLength = 0;
            buffStart = ++start;
          }
        } else {
          // has to be a numerical character or else abort
          if ((currentChar > '9') || (currentChar < '0'))
            done = PR_TRUE;
          buffLength++;
        }
        break;
      }

      case 1 : {
        // minute
        if (buffLength > 2) {
          done = PR_TRUE;
        } else if (currentChar == ':') {
          // validate minute
          if (strcmp(NS_ConvertUTF16toUTF8(Substring(buffStart, --start)).get(), "59") == 1) {
            done = PR_TRUE;
          } else {
            state = 2;
            buffLength = 0;
            buffStart = ++start;
          }
        } else {
          // has to be a numerical character or else abort
          if ((currentChar > '9') || (currentChar < '0'))
            done = PR_TRUE;
          buffLength++;
        }
        break;
      }

      case 2 : {
        // seconds
        if (buffLength > 2) {
          done = PR_TRUE;
        } else if (currentChar == 'Z') {
          // if its Z, has to be the last character
          if ((start == end) &&
            (strcmp(NS_ConvertUTF16toUTF8(Substring(buffStart, --start)).get(), "59") != 1)) {

            isValid = PR_TRUE;
            //sprintf(rv_second, "%s", NS_ConvertUTF16toUTF8(Substring(buffStart, start)).get());
          }
          done = PR_TRUE;
          tzSign = currentChar;
        } else if ((currentChar == '+') || (currentChar == '-')) {
          // timezone exists
          if (strcmp(NS_ConvertUTF16toUTF8(Substring(buffStart, --start)).get(), "59") == 1) {
            done = PR_TRUE;
          } else {
            state = 4;
            buffLength = 0;
            buffStart = ++start;
            tzSign = currentChar;
          }
        } else if (currentChar == '.') {
          // fractional seconds exist
          if (strcmp(NS_ConvertUTF16toUTF8(Substring(buffStart, --start)).get(), "59") == 1) {
            done = PR_TRUE;
          } else {
            state = 3;
            buffLength = 0;
            buffStart = ++start;
          }
        } else {
          // has to be a numerical character or else abort
          if ((currentChar > '9') || (currentChar < '0'))
            done = PR_TRUE;
          buffLength++;
        }

        break;
      }

      case 3 : {
        // fractional seconds

        if (currentChar == 'Z') {
          // if its Z, has to be the last character
          if (start == end)
            isValid = PR_TRUE;
          else
            done = PR_TRUE;
          tzSign = currentChar;
          usec.Assign(Substring(buffStart.get(), start.get()-1));
        } else if ((currentChar == '+') || (currentChar == '-')) {
          // timezone exists
          usec.Assign(Substring(buffStart.get(), start.get()-1));
          state = 4;
          buffLength = 0;
          buffStart = start;
          tzSign = currentChar;
        } else {
          // has to be a numerical character or else abort
          if ((currentChar > '9') || (currentChar < '0'))
            done = PR_TRUE;
          buffLength++;
        }
        break;
      }

      case 4 : {
        // timezone hh:mm
       if (buffStart.size_forward() == 5)
         isValid = ParseSchemaTimeZone(Substring(buffStart, end), timezoneHour,
                                       timezoneMinute);

       done = PR_TRUE;
       break;
      }
    }
  }

  if (isValid) {
    // 10:59:12.82
    // use a dummy year to make nspr do the work for us
    PRTime dateTime;
    nsAutoString fullDate;
    fullDate.AppendLiteral("1-1-90 ");
    fullDate.Append(aStrValue);

    LOG(("\n     new time is %s", NS_ConvertUTF16toUTF8(fullDate).get()));

    PRStatus status = PR_ParseTimeString(NS_ConvertUTF16toUTF8(fullDate).get(),
                                         PR_TRUE, &dateTime);
    if (status == -1) {
      isValid = PR_FALSE;
    } else {
      PRExplodedTime explodedDateTime;
      PR_ExplodeTime(dateTime, PR_GMTParameters, &explodedDateTime);

      sprintf(rv_hour, "%d", explodedDateTime.tm_hour);
      sprintf(rv_minute, "%d", explodedDateTime.tm_min);
      sprintf(rv_second, "%d", explodedDateTime.tm_sec);
      sprintf(rv_fraction_second, "%s", NS_ConvertUTF16toUTF8(usec).get());
    }
  }

  LOG(("\n     Time is %s \n", ((isValid) ? "Valid" : "Not Valid")));

  return isValid;
}

PRBool
nsSchemaValidatorUtils::ParseSchemaTimeZone(const nsAString & aStrValue,
                                            char *rv_tzhour, char *rv_tzminute)
{
  PRBool isValid = PR_FALSE;
  char timezoneHour[3] = "";
  char timezoneMinute[3] = "";

  nsAString::const_iterator start, end, buffStart;
  aStrValue.BeginReading(start);
  aStrValue.BeginReading(buffStart);
  aStrValue.EndReading(end);
  PRUint32 state = 0;
  PRUint32 buffLength = 0;
  PRBool done = PR_FALSE;
  PRUnichar currentChar;

  LOG(("\n    Validating TimeZone"));

  while ((start != end) && !done) {
    currentChar = *start++;

    switch (state) {
      case 0: {
        // hour
        if (buffLength > 2) {
          done = PR_TRUE;
        } else if (currentChar == ':') {
          timezoneHour[2] = '\0';
          if (strcmp(timezoneHour, "24") == 1) {
            done = PR_TRUE;
          } else {
            state = 1;
            buffLength = 0;
            buffStart = start;
          }
        } else {
          // has to be a numerical character or else abort
          if ((currentChar > '9') || (currentChar < '0'))
            done = PR_TRUE;
          else {
            timezoneHour[buffLength] = currentChar;
          }
          buffLength++;
        }
        break;
      }

      case 1: {
        // minute
        if (buffLength > 1) {
          done = PR_TRUE;
        } else if (start == end) {
          if (buffLength == 1) {
            timezoneMinute[2] = '\0';
            if (strcmp(timezoneMinute, "59") == 1) {
              done = PR_TRUE;
            } else {
              isValid = PR_TRUE;
            }
          } else {
            done = PR_FALSE;
          }
        } else {
          // has to be a numerical character or else abort
          if ((currentChar > '9') || (currentChar < '0')) {
            done = PR_TRUE;
          } else {
            timezoneMinute[buffLength] = currentChar;
          }
          buffLength++;
        }
        break;
      }

    }
  }

  if (isValid) {
    strncpy(rv_tzhour, timezoneHour, 3);
    strncpy(rv_tzminute, timezoneMinute, 3);
  }

  return isValid;
}

/*
 -1 - aDateTime1 < aDateTime2
  0 - equal
  1 - aDateTime1 > aDateTime2
*/
int
nsSchemaValidatorUtils::CompareExplodedDateTime(PRExplodedTime aDateTime1,
                                                PRBool aDateTime1IsNegative,
                                                PRExplodedTime aDateTime2,
                                                PRBool aDateTime2IsNegative)
{
  int result;

  if (!aDateTime1IsNegative && aDateTime2IsNegative) {
    // positive year is always bigger than negative year
    result = 1;
  } else if (aDateTime1IsNegative && !aDateTime2IsNegative) {
    result = -1;
  } else {
    result = CompareExplodedDate(aDateTime1, aDateTime2);

    if (result == 0)
      result = CompareExplodedTime(aDateTime1, aDateTime2);

    if (aDateTime1IsNegative && aDateTime2IsNegative) {
      // -20 is smaller than -21
      if (result == -1)
        result = 1;
      else if (result == 1)
        result = -1;
    }
  }
  return result;
}

/*
 -1 - aDateTime1 < aDateTime2
  0 - equal
  1 - aDateTime1 > aDateTime2
*/
int
nsSchemaValidatorUtils::CompareExplodedDate(PRExplodedTime aDateTime1,
                                            PRExplodedTime aDateTime2)
{
  int result;
  if (aDateTime1.tm_year < aDateTime2.tm_year) {
    result = -1;
  } else if (aDateTime1.tm_year > aDateTime2.tm_year) {
    result = 1;
  } else {
    if (aDateTime1.tm_month < aDateTime2.tm_month) {
      result = -1;
    } else if (aDateTime1.tm_month > aDateTime2.tm_month) {
      result = 1;
    } else {
      if (aDateTime1.tm_mday < aDateTime2.tm_mday) {
        result = -1;
      } else if (aDateTime1.tm_mday > aDateTime2.tm_mday) {
        result = 1;
      } else {
        result = 0;
      }
    }
  }

  return result;
}

/*
 -1 - aDateTime1 < aDateTime2
  0 - equal
  1 - aDateTime1 > aDateTime2
*/
int
nsSchemaValidatorUtils::CompareExplodedTime(PRExplodedTime aDateTime1,
                                            PRExplodedTime aDateTime2)
{
  int result;

  if (aDateTime1.tm_hour < aDateTime2.tm_hour) {
    result = -1;
  } else if (aDateTime1.tm_hour > aDateTime2.tm_hour) {
    result = 1;
  } else {
    if (aDateTime1.tm_min < aDateTime2.tm_min) {
      result = -1;
    } else if (aDateTime1.tm_min > aDateTime2.tm_min) {
      result = 1;
    } else {
      if (aDateTime1.tm_sec < aDateTime2.tm_sec) {
        result = -1;
      } else if (aDateTime1.tm_sec > aDateTime2.tm_sec) {
        result = 1;
      } else {
        if (aDateTime1.tm_usec < aDateTime2.tm_usec) {
          result = -1;
        } else if (aDateTime1.tm_usec > aDateTime2.tm_usec) {
          result = 1;
        } else {
          result = 0;
        }
      }
    }
  }

  return result;
}

void
nsSchemaValidatorUtils::GetMonthShorthand(char* aMonth, nsACString & aReturn)
{
  PRInt32 i, length = NS_ARRAY_LENGTH(monthShortHand);
  for (i = 0; i < length; ++i) {
    if (strcmp(aMonth, monthShortHand[i].number) == 0) {
      aReturn.AssignASCII(monthShortHand[i].shortHand);
    }
  }
}

/*
 -1 - aYearMonth1 < aYearMonth2
  0 - equal
  1 - aYearMonth1 > aYearMonth2
*/
int
nsSchemaValidatorUtils::CompareGYearMonth(nsSchemaGYearMonth aYearMonth1,
                                          nsSchemaGYearMonth aYearMonth2)
{
  int rv;

  if (aYearMonth1.gYear.year > aYearMonth2.gYear.year) {
    rv = 1;
  } else if (aYearMonth1.gYear.year < aYearMonth2.gYear.year) {
    rv = -1;
  } else {
    // both have the same year
    if (aYearMonth1.gMonth.month > aYearMonth2.gMonth.month)
      rv = 1;
    else if (aYearMonth1.gMonth.month < aYearMonth2.gMonth.month)
      rv = -1;
    else
      rv = 0;
  }

  return rv;
}

/*
 -1 - aMonthDay1 < aMonthDay2
  0 - equal
  1 - aMonthDay1 > aMonthDay2
*/
int
nsSchemaValidatorUtils::CompareGMonthDay(nsSchemaGMonthDay aMonthDay1,
                                         nsSchemaGMonthDay aMonthDay2)
{
  int rv;

  if (aMonthDay1.gMonth.month > aMonthDay2.gMonth.month) {
    rv = 1;
  } else if (aMonthDay1.gMonth.month < aMonthDay2.gMonth.month) {
    rv = -1;
  } else {
    // both have the same year
    if (aMonthDay1.gDay.day > aMonthDay2.gDay.day)
      rv = 1;
    else if (aMonthDay1.gDay.day < aMonthDay2.gDay.day)
      rv = -1;                
    else
      rv = 0;
  }

  return rv;
}

PRBool
nsSchemaValidatorUtils::ParseSchemaDuration(const nsAString & aStrValue,
                                            nsISchemaDuration **aDuration)
{
  PRBool isValid = PR_FALSE;

  nsAString::const_iterator start, end, buffStart;
  aStrValue.BeginReading(start);
  aStrValue.BeginReading(buffStart);
  aStrValue.EndReading(end);
  PRUint32 state = 0;
  PRUint32 buffLength = 0;
  PRBool done = PR_FALSE;
  PRUnichar currentChar;

  PRBool isNegative = PR_FALSE;

  // make sure leading P is present.  Take negative durations into consideration.
  if (*start == '-') {
    start.advance(1);
    if (*start != 'P') {
      return PR_FALSE;
    } else {
      isNegative = PR_TRUE;
      buffStart = ++start;
    }
  } else { 
    if (*start != 'P')
      return PR_FALSE;
    else
      ++start;
  }

  nsAutoString parseBuffer;
  PRBool timeSeparatorFound = PR_FALSE;

  // designators may not repeat, so keep track of those we find.
  PRBool yearFound = PR_FALSE;
  PRBool monthFound = PR_FALSE;
  PRBool dayFound = PR_FALSE;
  PRBool hourFound = PR_FALSE;
  PRBool minuteFound = PR_FALSE;
  PRBool secondFound = PR_FALSE;
  PRBool fractionSecondFound = PR_FALSE;

  PRUint32 year = 0;
  PRUint32 month = 0;
  PRUint32 day = 0;
  PRUint32 hour = 0;
  PRUint32 minute = 0;
  PRUint32 second = 0;
  double fractionSecond = 0;

  /* durations look like this:
       (-)PnYnMnDTnHnMn(.n)S
     - P is required, plus sign is invalid
     - order is important, so day after year is invalid (PnDnY)
     - Y,M,D,H,M,S are called designators
     - designators are not allowed without a number before them
     - T is the date/time seperator and is only allowed given a time part
  */

  while ((start != end) && !done) {
    currentChar = *start++;
    // not a number - so it has to be a type designator (YMDTHMS)
    // it can also be |.| for fractional seconds
    if ((currentChar > '9') || (currentChar < '0')) {
      // first check if the buffer is bigger than what long can store
      // which is 11 digits, as we convert to long
      if ((parseBuffer.Length() == 10) &&
          (CompareStrings(parseBuffer, NS_LITERAL_STRING("2147483647")) == 1)) {
        done = PR_TRUE;
      } else if (currentChar == 'Y') {
        if (yearFound || monthFound || dayFound || timeSeparatorFound) {
          done = PR_TRUE;
        } else {
          state = 0;
          yearFound = PR_TRUE;
        }
      } else if (currentChar == 'M') {
        // M is used twice - Months and Minutes
        if (!timeSeparatorFound)
          if (monthFound || dayFound || timeSeparatorFound) {
            done = PR_TRUE;
          } else {
            state = 1;
            monthFound = PR_TRUE;
          }
        else {
          if (!timeSeparatorFound) {
            done = PR_TRUE;
          } else {
            if (minuteFound || secondFound) {
              done = PR_TRUE;
            } else {
              state = 4;
              minuteFound = PR_TRUE;
            }
          }
        }
      } else if (currentChar == 'D') {
        if (dayFound || timeSeparatorFound) {
          done = PR_TRUE;
        } else {
          state = 2;
          dayFound = PR_TRUE;
        }
      } else if (currentChar == 'T') {
        // can't have the time seperator more than once
        if (timeSeparatorFound)
          done = PR_TRUE;
        else
          timeSeparatorFound = PR_TRUE;
      } else  if (currentChar == 'H') {
        if (!timeSeparatorFound || hourFound || secondFound ) {
          done = PR_TRUE;
        } else {
          state = 3;
          hourFound = PR_TRUE;
        }
      } else if (currentChar == 'S') {
        if (!timeSeparatorFound)
          done = PR_TRUE;
        else
          if (secondFound) {
            done = PR_TRUE;
          } else {
            state = 5;
            secondFound = PR_TRUE;
          }
      } else if (currentChar == '.') {
        // fractional seconds
        if (fractionSecondFound) {
          done = PR_TRUE;
        } else {
          parseBuffer.Append(currentChar);
          buffLength++;
          fractionSecondFound = PR_TRUE;
        }
      } else {
        done = PR_TRUE;
      }

      // if its a designator and no buffer, invalid per spec.  Rule doesn't apply
      // if T or '.' (fractional seconds), as we need to parse on for those.
      // so P200YM is invalid as there is no number before M
      if ((currentChar != 'T') && (currentChar != '.') && (parseBuffer.Length() == 0)) {
        done = PR_TRUE;
      } else if ((currentChar == 'T') && (start == end)) {
        // if 'T' is found but no time data after it, invalid
        done = PR_TRUE;
      }

      if (!done && (currentChar != 'T') && (currentChar != '.')) {
        long temp;
        switch (state) {
          case 0: {
            // years
            if (!IsValidSchemaInteger(parseBuffer, &temp))
              done = PR_TRUE;
            else
              year = temp;
            break;
          }

          case 1: {
            // months
            if (!IsValidSchemaInteger(parseBuffer, &temp))
              done = PR_TRUE;
            else
              month = temp;
            break;
          }

          case 2: {
            // days
            if (!IsValidSchemaInteger(parseBuffer, &temp))
              done = PR_TRUE;
            else
              day = temp;
            break;
          }

          case 3: {
            // hours
            if (!IsValidSchemaInteger(parseBuffer, &temp))
              done = PR_TRUE;
            else
              hour = temp;
            break;
          }

          case 4: {
            // minutes
            if (!IsValidSchemaInteger(parseBuffer, &temp))
              done = PR_TRUE;
            else
              minute = temp;
            break;
          }

          case 5: {
            // seconds - we have to handle optional fraction seconds as well
            if (fractionSecondFound) {
              double temp2, intpart;

              if (!IsValidSchemaDouble(parseBuffer, &temp2)) {
                done = PR_TRUE;
              } else {
                fractionSecond = modf(temp2, &intpart);
                second = NS_STATIC_CAST(PRUint32, intpart);
              }
            } else {
              if (!IsValidSchemaInteger(parseBuffer, &temp))
                done = PR_TRUE;
              else
                second = temp;
            }
            break;
          }
        }
      }

      // clear buffer unless we are at fraction seconds, since we want to parse
      // the seconds and fraction seconds into the same buffer.
      if (!fractionSecondFound) {
        parseBuffer.AssignLiteral("");
        buffLength = 0;
      }
    } else {
      if (buffLength > 11) {
        done = PR_TRUE;
      } else {
        parseBuffer.Append(currentChar);
        buffLength++;
      }
    }
  }

  if ((start == end) && (!done)) {
    isValid = PR_TRUE;
  }

  if (isValid) {
    nsISchemaDuration* duration = new nsSchemaDuration(year, month, day, hour,
                                                       minute, second,
                                                       fractionSecond,
                                                       isNegative);

    *aDuration = duration;
    NS_IF_ADDREF(*aDuration);
  }

  return isValid;
}

/* compares 2 strings that contain integers.
   Schema Integers have no limit, thus converting the strings
   into numbers won't work.

 -1 - aString1 < aString2
  0 - equal
  1 - aString1 > aString2

 */
int
nsSchemaValidatorUtils::CompareStrings(const nsAString & aString1,
                                       const nsAString & aString2)
{
  int rv;

  PRBool isNegative1 = (aString1.First() == PRUnichar('-'));
  PRBool isNegative2 = (aString2.First() == PRUnichar('-'));

  if (isNegative1 && !isNegative2) {
    // negative is always smaller than positive
    return -1;
  } else if (!isNegative1 && isNegative2) {
    // positive is always bigger than negative
    return 1;
  }

  nsAString::const_iterator start1, start2, end1, end2;
  aString1.BeginReading(start1);
  aString1.EndReading(end1);
  aString2.BeginReading(start2);
  aString2.EndReading(end2);

  // skip negative sign
  if (isNegative1)
    start1++;

  if (isNegative2)
    start2++;

  // jump over leading zeros
  PRBool done = PR_FALSE;
  while ((start1 != end1) && !done) {
    if (*start1 != '0')
      done = PR_TRUE;
    else
      ++start1;
  }

  done = PR_FALSE;
  while ((start2 != end2) && !done) {
    if (*start2 != '0')
      done = PR_TRUE;
    else
      ++start2;
  }

  nsAutoString compareString1, compareString2;
  compareString1.Assign(Substring(start1, end1));
  compareString2.Assign(Substring(start2, end2));

  // after removing leading 0s, check if they are the same

  if (compareString1.Equals(compareString2)) {
    return 0;
  }

  if (compareString1.Length() > compareString2.Length())
    rv = 1;
  else if (compareString1.Length() < compareString2.Length())
    rv = -1;
  else
    rv = strcmp(NS_ConvertUTF16toUTF8(compareString1).get(),
                NS_ConvertUTF16toUTF8(compareString2).get());

  // 3>2, but -2>-3
  if (isNegative1 && isNegative2) {
    if (rv == 1)
      rv = -1;
    else
      rv = 1;
  }
  return rv;
}

// For xsd:duration support, the the maximum day for a month/year combo as
// defined in http://w3.org/TR/xmlschema-2/#adding-durations-to-dateTimes.
int
nsSchemaValidatorUtils::GetMaximumDayInMonthFor(int aYearValue, int aMonthValue)
{
  int maxDay = 28;
  int month = ((aMonthValue - 1) % 12) + 1;
  int year = aYearValue + ((aMonthValue - 1) / 12);

  /*
    Return Value      Condition
    31                month is either 1, 3, 5, 7, 8, 10, 12
    30                month is either 4, 6, 9, 11
    29                month is 2 AND either (year % 400 == 0) OR (year % 100 == 0)
                                            AND (year % 4 == 0)
    28                Otherwise
  */

  if ((month == 1) || (month == 3) || (month == 5) || (month == 7) ||
      (month == 8) || (month == 10) || (month == 12))
    maxDay = 31;
  else if ((month == 4) || (month == 6) || (month == 9) || (month == 11))
    maxDay = 30;
  else if ((month == 2) && ((year % 400 == 0) || (year % 100 == 0) && (year % 4 == 0)))
    maxDay = 29;

  return maxDay;
}

/*
 * compares 2 durations using the algorithm defined in
 * http://w3.org/TR/xmlschema-2/#adding-durations-to-dateTimes by adding them to
 * the 4 datetimes specified in http://w3.org/TR/xmlschema-2/#duration-order.
 * If not all 4 result in x<y, we return indeterminate per
 * http://www.w3.org/TR/xmlschema-2/#facet-comparison-for-durations.
 *
 *  0 - aDuration1 < aDuration2
 *  1 - indeterminate
 *
 */
int
nsSchemaValidatorUtils::CompareDurations(nsISchemaDuration *aDuration1,
                                         nsISchemaDuration *aDuration2)
{
  int cmp = 0, tmpcmp, i = 0;

  PRTime foo;
  PRExplodedTime explodedTime, newTime1, newTime2;

  char* datetimeArray[] = { "1696-09-01T00:00:00Z", "1697-02-01T00:00:00Z",
                            "1903-03-01T00:00:00Z", "1903-07-01T00:00:00Z" };
  PRBool indeterminate = PR_FALSE;

  while (!indeterminate && (i < 4)) {
    GetPRTimeFromDateTime(NS_ConvertASCIItoUTF16(datetimeArray[i]), &foo);
    PR_ExplodeTime(foo, PR_GMTParameters, &explodedTime);

    newTime1 = AddDurationToDatetime(explodedTime, aDuration1);
    newTime2 = AddDurationToDatetime(explodedTime, aDuration2);

    tmpcmp = CompareExplodedDateTime(newTime1, PR_FALSE, newTime2, PR_FALSE);

    if (i > 0) {
      if (tmpcmp != cmp || tmpcmp > -1) {
        indeterminate = PR_TRUE;
      }
    }

    cmp = tmpcmp;
    ++i;
  }

  return indeterminate ? 1 : 0;
}

/* 
 * This method implements the algorithm described at:
 *    http://w3.org/TR/xmlschema-2/#adding-durations-to-dateTimes
 */
PRExplodedTime
nsSchemaValidatorUtils::AddDurationToDatetime(PRExplodedTime aDatetime,
                                              nsISchemaDuration *aDuration)
{
  PRExplodedTime resultDatetime;
  // first handle months
  PRUint32 temp;
  aDuration->GetMonths(&temp);
  temp += aDatetime.tm_month + 1;
  resultDatetime.tm_month = ((temp - 1) % 12) + 1;
  PRInt32 carry = (temp - 1) / 12;

  // years
  aDuration->GetYears(&temp);
  resultDatetime.tm_year = aDatetime.tm_year + carry + temp;

  /* fraction seconds
   * XXX: Since the 4 datetimes we add durations to don't have fraction seconds
   *      we can just add the duration's fraction second (stored as an float),
   *      which will be < 1.0.
   */
  double dblValue;
  aDuration->GetFractionSeconds(&dblValue);
  resultDatetime.tm_usec = (int) dblValue * 1000000;

  // seconds
  aDuration->GetSeconds(&temp);
  temp += aDatetime.tm_sec + carry;
  resultDatetime.tm_sec = temp % 60;
  carry = temp / 60;

  // minutes
  aDuration->GetMinutes(&temp);
  temp += aDatetime.tm_min + carry;
  resultDatetime.tm_min = temp % 60;
  carry = temp / 60;

  // hours
  aDuration->GetHours(&temp);
  temp += aDatetime.tm_hour + carry;
  resultDatetime.tm_hour = temp % 24;
  carry = temp / 24;

  // days
  int maxDay = GetMaximumDayInMonthFor(resultDatetime.tm_year,
                                       resultDatetime.tm_month);
  int tempDays = 0;
  if (aDatetime.tm_mday > maxDay)
    tempDays = maxDay;
  else if (aDatetime.tm_mday < 1)
    tempDays = 1;
  else
    tempDays = aDatetime.tm_mday;

  aDuration->GetDays(&temp);
  resultDatetime.tm_mday = tempDays + carry + temp;

  PRBool done = PR_FALSE;
  while (!done) {
    maxDay = GetMaximumDayInMonthFor(resultDatetime.tm_year,
                                         resultDatetime.tm_month);
    if (resultDatetime.tm_mday < 1) {
      resultDatetime.tm_mday +=
        GetMaximumDayInMonthFor(resultDatetime.tm_year,
                                resultDatetime.tm_month - 1);
      carry = -1;
    } else if (resultDatetime.tm_mday > maxDay) {
      resultDatetime.tm_mday -= maxDay;
      carry = 1;
    } else {
      done = PR_TRUE;
    }

    if (!done) {
      temp = resultDatetime.tm_month + carry;
      resultDatetime.tm_month = ((temp - 1) % 12) + 1;
      resultDatetime.tm_year += (temp - 1) / 12;
    }
  }

  LOG(("\n  New datetime is %d-%d-%d %d:%d:%d\n", resultDatetime.tm_mday,
    resultDatetime.tm_month, resultDatetime.tm_year, resultDatetime.tm_hour,
    resultDatetime.tm_min, resultDatetime.tm_sec));

  return resultDatetime;
}

PRBool
nsSchemaValidatorUtils::HandleEnumeration(const nsAString &aStrValue,
                                          const nsStringArray &aEnumerationList)
{
  PRBool isValid = PR_FALSE;

  // check enumeration
  PRInt32 count = aEnumerationList.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    if (aEnumerationList[i]->Equals(aStrValue)) {
      isValid = PR_TRUE;
      break;
    }
  }

  if (!isValid) {
    LOG(("  Not valid: Value doesn't match any of the enumerations"));
  }

  return isValid;
}

void
nsSchemaValidatorUtils::RemoveLeadingZeros(nsAString & aString)
{
  nsAString::const_iterator start, end;
  aString.BeginReading(start);
  aString.EndReading(end);

  PRBool done = PR_FALSE;
  PRUint32 count = 0, indexstart = 0;

  if (*start == '+' || *start == '-') {
    start++;
    indexstart = 1;
  }

  while ((start != end) && !done)
  {
    if (*start++ == '0') {
      ++count;
    } else {
      done = PR_TRUE;
    }
  }

  PRUint32 length = aString.Length();

  // if the entire string is composed of zeros, set it to one zero
  if (length == count) {
    aString.AssignLiteral("0");
  } else {
    // finally, remove the leading zeros
    aString.Cut(indexstart, count);
  }
}

void
nsSchemaValidatorUtils::RemoveTrailingZeros(nsAString & aString)
{
  nsAString::const_iterator start, end;
  aString.BeginReading(start);
  aString.EndReading(end);

  PRUint32 length = aString.Length();

  PRBool done = PR_FALSE;
  PRUint32 count = 0;
  if(start != end)
      end --;

  while ((start != end) && !done)
  {
    if (*end-- == '0') {
      ++count;
    } else {
      done = PR_TRUE;
    }
  }

  // finally, remove the trailing zeros
  aString.Cut(length - count, count);
}

