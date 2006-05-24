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
#include <float.h>
#include "prlog.h"
#include "prprf.h"
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
                                             long *aResult, PRBool aOverFlowCheck)
{
  return !aNodeValue.IsEmpty() &&
         IsValidSchemaInteger(NS_ConvertUTF16toUTF8(aNodeValue).get(),
                              aResult, aOverFlowCheck);
}

// overloaded, for char* rather than nsAString
PRBool
nsSchemaValidatorUtils::IsValidSchemaInteger(const char* aString,
                                             long *aResult,
                                             PRBool aOverFlowCheck)
{
  PRBool isValid = PR_FALSE;

  if (*aString == 0)
    return PR_FALSE;

  char * pEnd;
  long intValue = strtol(aString, &pEnd, 10);

  if (aResult)
    *aResult = intValue;

  if (aOverFlowCheck) {
    isValid = (!((intValue == LONG_MAX || intValue == LONG_MIN) && errno == ERANGE))
              && *pEnd == '\0';
  } else {
    isValid = (*pEnd == '\0');
  }

  return isValid;
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
  PRBool isValid = PR_TRUE;

  if (*aString == 0)
    return PR_FALSE;

  char * pEnd;
  double value = PR_strtod(aString, &pEnd);

  // If end pointer hasn't moved, then the string wasn't a
  // true double (could be INF, -INF or NaN though)
  if (pEnd == aString) {
    nsCAutoString temp(aString);

    // doubles may be INF, -INF or NaN
    if (temp.EqualsLiteral("INF")) {
      value = DBL_MAX;
    } else if (temp.EqualsLiteral("-INF")) {
      value = - DBL_MAX;
    } else if (!temp.EqualsLiteral("NaN")) {
      isValid = PR_FALSE;
    }
  }
  if (aResult)
    *aResult = value;

  return isValid;
}

PRBool
nsSchemaValidatorUtils::ParseDateTime(const nsAString & aNodeValue,
                                      nsSchemaDateTime *aResult)
{
  PRBool isValid = PR_FALSE;

  nsAutoString datetimeString(aNodeValue);

  aResult->date.isNegative = (datetimeString.First() == '-');

  /*
    http://www.w3.org/TR/xmlschema-2/#dateTime
    (-)CCYY-MM-DDThh:mm:ss(.sss...)
      then either: Z
      or [+/-]hh:mm
  */

  // first handle the date part
  // search for 'T'

  LOG(("  Validating DateTime:"));

  int findString = datetimeString.FindChar('T');

  // if no T, invalid
  if (findString >= 0) {
    isValid = ParseSchemaDate(Substring(aNodeValue, 0, findString+1), &aResult->date);

    if (isValid) {
      isValid = ParseSchemaTime(
                  Substring(aNodeValue, findString + 1, aNodeValue.Length()),
                  &aResult->time);
    }
  }

  return isValid;
}

PRBool
nsSchemaValidatorUtils::ParseSchemaDate(const nsAString & aStrValue,
                                        nsSchemaDate *aDate)
{
  PRBool isValid = PR_FALSE;

  /*
    http://www.w3.org/TR/xmlschema-2/#date
    (-)CCYY-MM-DDT
  */

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
          if (buffLength < 4) {
            done = PR_TRUE;
          } else {
            year.Assign(Substring(buffStart, --start));
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

      case 1: {
        // month
        if (buffLength > 2) {
          done = PR_TRUE;
        } else if (currentChar == '-') {
          if (strcmp(month, "12") == 1 || buffLength < 2) {
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
          if ((start == end) && (buffLength == 2) && (strcmp(day, "31") < 1))
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
    char * pEnd;

    PRUint32 yearval = strtoul(NS_ConvertUTF16toUTF8(year).get(), &pEnd, 10);
    if (yearval == 0 || yearval == ULONG_MAX) {
      isValid = PR_FALSE;
    } else {
      PRUint8 monthval = strtol(month, &pEnd, 10);
      PRUint8 dayval = strtol(day, &pEnd, 10);

      // check for leap years
      PRUint8 maxDay = GetMaximumDayInMonthFor(yearval, monthval);
      if (maxDay >= dayval) {
        aDate->year = yearval;

        // month/day are validated in the parsing code above
        aDate->month = monthval;
        aDate->day = dayval;
      } else {
        isValid = PR_FALSE;
      }
    }
  }

  LOG(("     Date is %s", ((isValid) ? "Valid" : "Not Valid")));

  return isValid;
}

// parses a string as a schema time type and returns the parsed
// hour/minute/second/fraction seconds as well as if its a valid
// schema time type.
PRBool
nsSchemaValidatorUtils::ParseSchemaTime(const nsAString & aStrValue,
                                        nsSchemaTime* aTime)
{
  PRBool isValid = PR_FALSE;

  // time looks like this: HH:MM:SS(.[S]+)(+/-HH:MM)

  char hour[3] = "";
  char minute[3] = "";
  char second[3] = "";

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
          if (strcmp(hour, "24") == 1) {
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
          else
            hour[buffLength] = currentChar;
          buffLength++;
        }
        break;
      }

      case 1: {
        // minute
        if (buffLength > 2) {
          done = PR_TRUE;
        } else if (currentChar == ':') {
          // validate minute
          if (strcmp(minute, "59") == 1) {
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
            minute[buffLength] = currentChar;
          buffLength++;
        }
        break;
      }

      case 2: {
        // seconds
        if (buffLength > 2) {
          done = PR_TRUE;
        } else if (currentChar == 'Z') {
          // if its Z, has to be the last character
          if ((start == end) && (strcmp(second, "59") != 1)) {
            isValid = PR_TRUE;
          }
          done = PR_TRUE;
          tzSign = currentChar;
        } else if ((currentChar == '+') || (currentChar == '-')) {
          // timezone exists
          if (strcmp(second, "59") == 1) {
            done = PR_TRUE;
          } else {
            state = 4;
            buffLength = 0;
            buffStart = start;
            tzSign = currentChar;
          }
        } else if (currentChar == '.') {
          // fractional seconds exist
          if (strcmp(second, "59") == 1) {
            done = PR_TRUE;
          } else {
            state = 3;
            buffLength = 0;
            buffStart = start;
          }
        } else {
          // has to be a numerical character or else abort
          if ((currentChar > '9') || (currentChar < '0'))
            done = PR_TRUE;
          else
            second[buffLength] = currentChar;
          buffLength++;
        }

        break;
      }

      case 3: {
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

      case 4: {
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
    char * pEnd;

    PRUint32 usecval = strtoul(NS_ConvertUTF16toUTF8(usec).get(), &pEnd, 10);
    // be carefull, empty usec returns 0
    if (!usec.IsEmpty() && (usecval == 0 || usecval == ULONG_MAX)) {
      isValid = PR_FALSE;
    } else {
      aTime->hour = strtol(hour, &pEnd, 10);
      aTime->minute = strtol(minute, &pEnd, 10);
      aTime->second = strtol(second, &pEnd, 10);
      aTime->milisecond = usecval;

      if (tzSign == '+')
        aTime->tzIsNegative = PR_FALSE;
      else
        aTime->tzIsNegative = PR_TRUE;

      aTime->tzhour = strtol(timezoneHour, &pEnd, 10);
      aTime->tzminute = strtol(timezoneMinute, &pEnd, 10);
    }
  }

  LOG(("     Time is %s", ((isValid) ? "Valid" : "Not Valid")));

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
        if (buffLength > 2) {
          done = PR_TRUE;
        } else if (start == end) {
          if (buffLength == 1) {
            if ((currentChar > '9') || (currentChar < '0')) {
              done = PR_TRUE;
            } else {
              timezoneMinute[buffLength] = currentChar;

              timezoneMinute[2] = '\0';
              if (strcmp(timezoneMinute, "59") == 1) {
                done = PR_TRUE;
              } else {
                isValid = PR_TRUE;
              }
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
nsSchemaValidatorUtils::CompareDateTime(nsSchemaDateTime aDateTime1,
                                        nsSchemaDateTime aDateTime2)
{
  int result;

  nsSchemaDateTime dateTime1, dateTime2;
  AddTimeZoneToDateTime(aDateTime1, &dateTime1);
  AddTimeZoneToDateTime(aDateTime2, &dateTime2);

  if (!dateTime1.date.isNegative && dateTime2.date.isNegative) {
    // positive year is always bigger than negative year
    result = 1;
  } else if (dateTime1.date.isNegative && !dateTime2.date.isNegative) {
    result = -1;
  } else {
    result = CompareDate(dateTime1.date, dateTime2.date);

    if (result == 0)
      result = CompareTime(dateTime1.time, dateTime2.time);

    if (dateTime1.date.isNegative && dateTime2.date.isNegative) {
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
nsSchemaValidatorUtils::CompareDate(nsSchemaDate aDate1, nsSchemaDate aDate2)
{
  int result;

  if (aDate1.year < aDate2.year) {
    result = -1;
  } else if (aDate1.year > aDate2.year) {
    result = 1;
  } else {
    if (aDate1.month < aDate2.month) {
      result = -1;
    } else if (aDate1.month > aDate2.month) {
      result = 1;
    } else {
      if (aDate1.day < aDate2.day) {
        result = -1;
      } else if (aDate1.day > aDate2.day) {
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
nsSchemaValidatorUtils::CompareTime(nsSchemaTime aTime1, nsSchemaTime aTime2)
{
  int result;

  if (aTime1.hour < aTime2.hour) {
    result = -1;
  } else if (aTime1.hour > aTime2.hour) {
    result = 1;
  } else {
    if (aTime1.minute < aTime2.minute) {
      result = -1;
    } else if (aTime1.minute > aTime2.minute) {
      result = 1;
    } else {
      if (aTime1.second < aTime2.second) {
        result = -1;
      } else if (aTime1.second > aTime2.second) {
        result = 1;
      } else {
        if (aTime1.milisecond < aTime2.milisecond) {
          result = -1;
        } else if (aTime1.milisecond > aTime2.milisecond) {
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
nsSchemaValidatorUtils::AddTimeZoneToDateTime(nsSchemaDateTime aDateTime,
                                              nsSchemaDateTime* aDestDateTime)
{
  // With timezones, you subtract the timezone difference. So for example,
  // 2002-10-10T12:00:00+05:00 is 2002-10-10T07:00:00Z
  PRUint32 year = aDateTime.date.year;
  PRUint8 month = aDateTime.date.month;
  PRUint8 day = aDateTime.date.day;
  int hour = aDateTime.time.hour;
  int minute = aDateTime.time.minute;
  PRUint8 second = aDateTime.time.second;
  PRUint32 milisecond = aDateTime.time.milisecond;

  if (aDateTime.time.tzIsNegative) {
    hour = hour + aDateTime.time.tzhour;
    minute = minute + aDateTime.time.tzminute;
  } else {
    hour = hour - aDateTime.time.tzhour;
    minute = minute - aDateTime.time.tzminute;
  }

  div_t divresult;

  if (minute > 59) {
    divresult = div(minute, 60);
    hour += divresult.quot;
    minute = divresult.rem;
  } else if (minute < 0) {
    minute = 60 + minute;
    hour--;
  }

  // hour
  if (hour == 24 && (minute > 0 || second > 0)) {
    // can only be 24:0:0 - need to increment day
    day++;
    hour = 0;
  } else if (hour > 23) {
    divresult = div(hour, 24);
    day += divresult.quot;
    hour = divresult.rem;
  } else if (hour < 0) {
    hour = 24 + hour;
    day--;
  }

  // day
  if (day == 0) {
    // if day is 0, go back a month and make sure we handle month 0 (ie back a year).
    month--;

    if (month == 0) {
      month = 12;
      year--;
    }

    day = GetMaximumDayInMonthFor(month, year);
  } else {
    int maxDay = GetMaximumDayInMonthFor(month, year);
    while (day > maxDay) {
      day -= maxDay;
      month++;

      // since we are a valid datetime, month has to be 12 before the ++, so will
      // be 13
      if (month == 13) {
        month = 1;
        year++;
      }

      maxDay = GetMaximumDayInMonthFor(month, year);
    }
  }

  aDestDateTime->date.year = year;
  aDestDateTime->date.month = month;
  aDestDateTime->date.day = day;
  aDestDateTime->date.isNegative = aDateTime.date.isNegative;
  aDestDateTime->time.hour = hour;
  aDestDateTime->time.minute = minute;
  aDestDateTime->time.second = second;
  aDestDateTime->time.milisecond = milisecond;
  aDestDateTime->time.tzIsNegative = aDateTime.time.tzIsNegative;
}

void
nsSchemaValidatorUtils::GetMonthShorthand(PRUint8 aMonth, nsACString & aReturn)
{
  aReturn.AssignASCII(monthShortHand[aMonth - 1].shortHand);
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
            if (!IsValidSchemaInteger(parseBuffer, &temp, PR_TRUE))
              done = PR_TRUE;
            else
              year = temp;
            break;
          }

          case 1: {
            // months
            if (!IsValidSchemaInteger(parseBuffer, &temp, PR_TRUE))
              done = PR_TRUE;
            else
              month = temp;
            break;
          }

          case 2: {
            // days
            if (!IsValidSchemaInteger(parseBuffer, &temp, PR_TRUE))
              done = PR_TRUE;
            else
              day = temp;
            break;
          }

          case 3: {
            // hours
            if (!IsValidSchemaInteger(parseBuffer, &temp, PR_TRUE))
              done = PR_TRUE;
            else
              hour = temp;
            break;
          }

          case 4: {
            // minutes
            if (!IsValidSchemaInteger(parseBuffer, &temp, PR_TRUE))
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
              if (!IsValidSchemaInteger(parseBuffer, &temp, PR_TRUE))
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
nsSchemaValidatorUtils::GetMaximumDayInMonthFor(PRUint32 aYearValue, PRUint8 aMonthValue)
{
  PRUint8 maxDay = 28;
  PRUint8 month = ((aMonthValue - 1) % 12) + 1;
  PRUint32 year = aYearValue + ((aMonthValue - 1) / 12);

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

  nsSchemaDateTime dateTime, newDateTime1, newDateTime2;

  char* datetimeArray[] = { "1696-09-01T00:00:00Z", "1697-02-01T00:00:00Z",
                            "1903-03-01T00:00:00Z", "1903-07-01T00:00:00Z" };
  PRBool indeterminate = PR_FALSE;

  while (!indeterminate && (i < 4)) {
    ParseDateTime(NS_ConvertASCIItoUTF16(datetimeArray[i]), &dateTime);

    AddDurationToDatetime(dateTime, aDuration1, &newDateTime1);
    AddDurationToDatetime(dateTime, aDuration2, &newDateTime2);

    tmpcmp = CompareDateTime(newDateTime1, newDateTime2);

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
void
nsSchemaValidatorUtils::AddDurationToDatetime(nsSchemaDateTime aDatetime,
                                              nsISchemaDuration *aDuration,
                                              nsSchemaDateTime* aResultDateTime)
{
  // first handle months
  PRUint32 temp = 0;
  aDuration->GetMonths(&temp);
  temp += aDatetime.date.month;
  aResultDateTime->date.month = ((temp - 1) % 12) + 1;
  PRInt32 carry = (temp - 1) / 12;

  // years
  aDuration->GetYears(&temp);
  aResultDateTime->date.year = aDatetime.date.year + carry + temp;

  // reset the carry
  carry = 0;

  /* fraction seconds
   * XXX: Since the 4 datetimes we add durations to don't have fraction seconds
   *      we can just add the duration's fraction second (stored as an float),
   *      which will be < 1.0.
   */
  double dblValue;
  aDuration->GetFractionSeconds(&dblValue);
  aResultDateTime->time.milisecond = (int) dblValue * 1000000;

  // seconds
  aDuration->GetSeconds(&temp);
  temp += aDatetime.time.second + carry;
  aResultDateTime->time.second = temp % 60;
  carry = temp / 60;

  // minutes
  aDuration->GetMinutes(&temp);
  temp += aDatetime.time.minute + carry;
  aResultDateTime->time.minute = temp % 60;
  carry = temp / 60;

  // hours
  aDuration->GetHours(&temp);
  temp += aDatetime.time.hour + carry;
  aResultDateTime->time.hour = temp % 24;
  carry = temp / 24;

  // days
  int maxDay = GetMaximumDayInMonthFor(aResultDateTime->date.year,
                                       aResultDateTime->date.month);
  int tempDays = 0;
  if (aDatetime.date.day > maxDay)
    tempDays = maxDay;
  else if (aDatetime.date.day < 1)
    tempDays = 1;
  else
    tempDays = aDatetime.date.day;

  aDuration->GetDays(&temp);
  aResultDateTime->date.day = tempDays + carry + temp;

  PRBool done = PR_FALSE;
  while (!done) {
    maxDay = GetMaximumDayInMonthFor(aResultDateTime->date.year,
                                     aResultDateTime->date.month);
    if (aResultDateTime->date.day < 1) {
      aResultDateTime->date.day +=
        GetMaximumDayInMonthFor(aResultDateTime->date.year,
                                aResultDateTime->date.month - 1);
      carry = -1;
    } else if (aResultDateTime->date.day > maxDay) {
      aResultDateTime->date.day -= maxDay;
      carry = 1;
    } else {
      done = PR_TRUE;
    }

    if (!done) {
      temp = aResultDateTime->date.month + carry;
      aResultDateTime->date.month = ((temp - 1) % 12) + 1;
      aResultDateTime->date.year += (temp - 1) / 12;
    }
  }

  // copy over negative and tz data
  aResultDateTime->date.isNegative = aDatetime.date.isNegative;

  aResultDateTime->time.tzIsNegative = aDatetime.time.tzIsNegative;
  aResultDateTime->time.tzhour = aDatetime.time.tzhour;
  aResultDateTime->time.tzminute = aDatetime.time.tzminute;

  LOG(("\n  New datetime is %d-%d-%d %d:%d:%d\n", aResultDateTime->date.day,
    aResultDateTime->date.month, aResultDateTime->date.year,
    aResultDateTime->time.hour, aResultDateTime->time.minute,
    aResultDateTime->time.second));
}

// http://www.w3.org/TR/xmlschema-2/#normalizedString
PRBool
nsSchemaValidatorUtils::IsValidSchemaNormalizedString(const nsAString &aStrValue)
{
  PRBool isValid = PR_FALSE;
  nsAutoString string(aStrValue);

  // may not contain carriage return, line feed nor tab characters
  if (string.FindCharInSet("\t\r\n") == kNotFound)
    isValid = PR_TRUE;

  return isValid;
}

// http://www.w3.org/TR/xmlschema-2/#token
PRBool
nsSchemaValidatorUtils::IsValidSchemaToken(const nsAString &aStrValue)
{
  PRBool isValid = PR_FALSE;
  nsAutoString string(aStrValue);

  // may not contain carriage return, line feed, tab characters.  Also can
  // not contain leading/trailing whitespace and no internal sequences of
  // two or more spaces.
  if ((string.FindCharInSet("\t\r\n") == kNotFound) &&
      (string.Find(NS_LITERAL_STRING("  ")) == kNotFound) &&
      (string.First() != ' ') &&
      (string.Last() != ' '))
    isValid = PR_TRUE;

  return isValid;
}

// http://www.w3.org/TR/xmlschema-2/#language
PRBool
nsSchemaValidatorUtils::IsValidSchemaLanguage(const nsAString &aStrValue)
{
  PRBool isValid = PR_FALSE;

  // pattern is defined in spec
  nsAutoString pattern;
  pattern.AssignLiteral("[a-zA-Z]{1,8}(-[a-zA-Z0-9]{1,8})*");

  nsCOMPtr<nsISchemaValidatorRegexp> regexp = do_GetService(kREGEXP_CID);
  nsresult rv = regexp->RunRegexp(aStrValue, pattern, "g", &isValid);
  NS_ENSURE_SUCCESS(rv, rv);

  return isValid;
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
      LOG(("  Valid: Value matched enumeration #%d", i));
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

  PRUint32 length = aString.Length() - indexstart;

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

// Walks the inheritance tree until it finds a type that isn't a restriction
// type.  While it finds restriction types, it collects restriction facets and
// places them into the nsSchemaDerivedSimpleType.  Once a facet has been found,
// it makes sure that it won't be overwritten by the same facet defined in one
// of the inherited types.
nsresult
nsSchemaValidatorUtils::GetDerivedSimpleType(nsISchemaSimpleType *aSimpleType,
                                             nsSchemaDerivedSimpleType *aDerived)
{
  PRBool done = PR_FALSE, hasEnumerations = PR_FALSE;
  nsCOMPtr<nsISchemaSimpleType> simpleType(aSimpleType);
  PRUint16 simpleTypeValue;
  PRUint32 facetCount;

  nsAutoString enumeration;
  nsresult rv = NS_OK;

  while(simpleType && !done) {
    // get the type of the simpletype
    rv = simpleType->GetSimpleType(&simpleTypeValue);
    NS_ENSURE_SUCCESS(rv, rv);

    switch (simpleTypeValue) {
      case nsISchemaSimpleType::SIMPLE_TYPE_RESTRICTION: {
        // handle the facets

        nsCOMPtr<nsISchemaRestrictionType> restrictionType =
          do_QueryInterface(simpleType);

        nsCOMPtr<nsISchemaFacet> facet;
        PRUint32 facetCounter;
        PRUint16 facetType;

        // get the amount of restriction facet defined.
        rv = restrictionType->GetFacetCount(&facetCount);
        NS_ENSURE_SUCCESS(rv, rv);
        LOG(("    %d facet(s) defined.", facetCount));

        // if we had enumerations, we may not add new ones, since we are
        // being restricted. So if x restricts y, x defines the possible
        // enumerations and any enumerations on y are skipped
        hasEnumerations = (aDerived->enumerationList.Count() > 0);

        for (facetCounter = 0; facetCounter < facetCount; ++facetCounter) {
          rv = restrictionType->GetFacet(facetCounter, getter_AddRefs(facet));
          NS_ENSURE_SUCCESS(rv, rv);
          facet->GetFacetType(&facetType);

          switch (facetType) {
            case nsISchemaFacet::FACET_TYPE_LENGTH: {
              nsSchemaIntFacet *length = &aDerived->length;
              if (!length->isDefined) {
                length->isDefined = PR_TRUE;
                facet->GetLengthValue(&length->value);
                LOG(("  - Length Facet found (value is %d)",
                     length->value));
              }
              break;
            }

            case nsISchemaFacet::FACET_TYPE_MINLENGTH: {
              nsSchemaIntFacet *minLength = &aDerived->minLength;
              if (!minLength->isDefined) {
                minLength->isDefined = PR_TRUE;
                facet->GetLengthValue(&minLength->value);
                LOG(("  - Min Length Facet found (value is %d)",
                     minLength->value));
              }
              break;
            }

            case nsISchemaFacet::FACET_TYPE_MAXLENGTH: {
              nsSchemaIntFacet *maxLength = &aDerived->maxLength;
              if (!maxLength->isDefined) {
                maxLength->isDefined = PR_TRUE;
                facet->GetLengthValue(&maxLength->value);
                LOG(("  - Max Length Facet found (value is %d)",
                     maxLength->value));
              }
              break;
            }

            case nsISchemaFacet::FACET_TYPE_PATTERN: {
              nsSchemaStringFacet *pattern = &aDerived->pattern;
              if (!pattern->isDefined) {
                pattern->isDefined = PR_TRUE;
                facet->GetValue(pattern->value);
                LOG(("  - Pattern Facet found (value is %s)",
                      NS_ConvertUTF16toUTF8(pattern->value).get()));
              }
              break;
            }

            case nsISchemaFacet::FACET_TYPE_ENUMERATION: {
              if (!hasEnumerations) {
                facet->GetValue(enumeration);
                aDerived->enumerationList.AppendString(enumeration);
                LOG(("  - Enumeration found (%s)",
                  NS_ConvertUTF16toUTF8(enumeration).get()));
              }
              break;
            }

            case nsISchemaFacet::FACET_TYPE_WHITESPACE: {
              if (!aDerived->isWhitespaceDefined)
                facet->GetWhitespaceValue(&aDerived->whitespace);
              break;
            }

            case nsISchemaFacet::FACET_TYPE_MAXINCLUSIVE: {
              nsSchemaStringFacet *maxInclusive = &aDerived->maxInclusive;
              if (!maxInclusive->isDefined) {
                maxInclusive->isDefined = PR_TRUE;
                facet->GetValue(maxInclusive->value);
                LOG(("  - Max Inclusive Facet found (value is %s)",
                  NS_ConvertUTF16toUTF8(maxInclusive->value).get()));
              }
              break;
            }

            case nsISchemaFacet::FACET_TYPE_MININCLUSIVE: {
              nsSchemaStringFacet *minInclusive = &aDerived->minInclusive;
              if (!minInclusive->isDefined) {
                minInclusive->isDefined = PR_TRUE;
                facet->GetValue(minInclusive->value);
                LOG(("  - Min Inclusive Facet found (value is %s)",
                  NS_ConvertUTF16toUTF8(minInclusive->value).get()));
              }
              break;
            }

            case nsISchemaFacet::FACET_TYPE_MAXEXCLUSIVE: {
              nsSchemaStringFacet *maxExclusive = &aDerived->maxExclusive;
              if (!maxExclusive->isDefined) {
                maxExclusive->isDefined = PR_TRUE;
                facet->GetValue(aDerived->maxExclusive.value);
                LOG(("  - Max Exclusive Facet found (value is %s)",
                  NS_ConvertUTF16toUTF8(maxExclusive->value).get()));
              }
              break;
            }

            case nsISchemaFacet::FACET_TYPE_MINEXCLUSIVE: {
              nsSchemaStringFacet *minExclusive = &aDerived->minExclusive;
              if (!minExclusive->isDefined) {
                minExclusive->isDefined = PR_TRUE;
                facet->GetValue(minExclusive->value);
                LOG(("  - Min Exclusive Facet found (value is %s)",
                  NS_ConvertUTF16toUTF8(minExclusive->value).get()));
              }
              break;
            }

            case nsISchemaFacet::FACET_TYPE_TOTALDIGITS: {
              nsSchemaIntFacet *totalDigits = &aDerived->totalDigits;
              if (!totalDigits->isDefined) {
                totalDigits->isDefined = PR_TRUE;
                facet->GetDigitsValue(&totalDigits->value);
                LOG(("  - Totaldigits Facet found (value is %d)",
                     totalDigits->value));
              }
              break;
            }

            case nsISchemaFacet::FACET_TYPE_FRACTIONDIGITS: {
              nsSchemaIntFacet *fractionDigits = &aDerived->fractionDigits;
              if (!fractionDigits->isDefined) {
                fractionDigits->isDefined = PR_TRUE;
                facet->GetDigitsValue(&fractionDigits->value);
                LOG(("  - FractionDigits Facet found (value is %d)",
                     fractionDigits->value));
              }
              break;
            }
          }
        }

        // get base type
        nsresult rv = restrictionType->GetBaseType(getter_AddRefs(simpleType));
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }

      case nsISchemaSimpleType::SIMPLE_TYPE_BUILTIN: {
        // we are done
        aDerived->mBaseType = simpleType;
        done = PR_TRUE;
        break;
      }

      case nsISchemaSimpleType::SIMPLE_TYPE_LIST: {
        // set as base type
        aDerived->mBaseType = simpleType;
        done = PR_TRUE;
        break;
      }

      case nsISchemaSimpleType::SIMPLE_TYPE_UNION: {
        // set as base type
        aDerived->mBaseType = simpleType;
        done = PR_TRUE;
        break;
      }
    }
  }

  return rv;
}

// copies the data from aDerivedSrc to aDerivedDest
void
nsSchemaValidatorUtils::CopyDerivedSimpleType(nsSchemaDerivedSimpleType *aDerivedDest,
                                              nsSchemaDerivedSimpleType *aDerivedSrc)
{
  aDerivedDest->mBaseType = aDerivedSrc->mBaseType;

  aDerivedDest->length.value = aDerivedSrc->length.value;
  aDerivedDest->length.isDefined = aDerivedSrc->length.isDefined;
  aDerivedDest->minLength.value = aDerivedSrc->minLength.value;
  aDerivedDest->minLength.isDefined = aDerivedSrc->minLength.isDefined;
  aDerivedDest->maxLength.value = aDerivedSrc->maxLength.value;
  aDerivedDest->maxLength.isDefined = aDerivedSrc->maxLength.isDefined;

  aDerivedDest->pattern.value = aDerivedSrc->pattern.value;
  aDerivedDest->pattern.isDefined = aDerivedSrc->pattern.isDefined;

  aDerivedDest->isWhitespaceDefined = aDerivedSrc->isWhitespaceDefined;
  aDerivedDest->whitespace = aDerivedSrc->whitespace;

  aDerivedDest->maxInclusive.value = aDerivedSrc->maxInclusive.value;
  aDerivedDest->maxInclusive.isDefined = aDerivedSrc->maxInclusive.isDefined;
  aDerivedDest->minInclusive.value = aDerivedSrc->minInclusive.value;
  aDerivedDest->minInclusive.isDefined = aDerivedSrc->minInclusive.isDefined;
  aDerivedDest->maxExclusive.value = aDerivedSrc->maxExclusive.value;
  aDerivedDest->maxExclusive.isDefined = aDerivedSrc->maxExclusive.isDefined;
  aDerivedDest->minExclusive.value = aDerivedSrc->minExclusive.value;
  aDerivedDest->minExclusive.isDefined = aDerivedSrc->minExclusive.isDefined;

  aDerivedDest->totalDigits.value = aDerivedSrc->totalDigits.value;
  aDerivedDest->totalDigits.isDefined = aDerivedSrc->totalDigits.isDefined;
  aDerivedDest->fractionDigits.value = aDerivedSrc->fractionDigits.value;
  aDerivedDest->fractionDigits.isDefined = aDerivedSrc->fractionDigits.isDefined;

  aDerivedDest->enumerationList = aDerivedSrc->enumerationList;
}

// sets aResultNode to aNode, making sure it points to null or a dom element
void
nsSchemaValidatorUtils::SetToNullOrElement(nsIDOMNode *aNode,
                                           nsIDOMNode **aResultNode)
{
  nsCOMPtr<nsIDOMNode> currentNode(aNode), tmpNode;

  if (currentNode) {
    PRUint16 nodeType;
    currentNode->GetNodeType(&nodeType);

    // if not an element node, skip
    while (currentNode && nodeType != nsIDOMNode::ELEMENT_NODE) {
      currentNode->GetNextSibling(getter_AddRefs(tmpNode));
      currentNode = tmpNode;
      if (currentNode)
        currentNode->GetNodeType(&nodeType);
    }

    currentNode.swap(*aResultNode);

  }
}

