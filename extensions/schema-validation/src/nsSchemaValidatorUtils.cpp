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

#include <stdlib.h>
#include "prprf.h"
#include "prtime.h"

PRBool nsSchemaValidatorUtils::IsValidSchemaInteger(const nsAString & aNodeValue, 
                                                    long *aResult){
  PRBool isValid = PR_FALSE;
  NS_ConvertUTF16toUTF8 temp(aNodeValue);
  char * pEnd;
  long intValue = strtol(temp.get(), &pEnd, 10);

  if (*pEnd == '\0')
    isValid = PR_TRUE;

  if (aResult)
    *aResult = intValue;

  return isValid;
}

// overloaded, for char* rather than nsAString
PRBool nsSchemaValidatorUtils::IsValidSchemaInteger(char* aString, long *aResult){
  PRBool isValid = PR_FALSE;
  char * pEnd;
  long intValue = strtol(aString, &pEnd, 10);

  if (*pEnd == '\0')
    isValid = PR_TRUE;

  if (aResult)
    *aResult = intValue;

  return isValid;
}

PRBool nsSchemaValidatorUtils::ParseSchemaDate(const char * strValue,
  char *rv_year, char *rv_month, char *rv_day)
{
  PRBool isValid = PR_FALSE;
  PRBool isNegativeYear = PR_FALSE;
  
  int run = 0;
  int length = strlen(strValue);
  PRBool doneParsing = PR_FALSE;

  /*
    0 - year
    1 - Month MM
    2 - Day (DD)
  */
  int parseState = 0;

  char parseBuffer[80] = "";
  int bufferRun = 0;
  char year[80] = "";
  char month[3] = "";
  char day[3] = "";

  PRTime dateTime;

  char fulldate[86] = "";

  // overflow check
  if (strlen(strValue) >= sizeof(fulldate))
    return PR_FALSE;

  if (strValue[0] == '-') {
    isNegativeYear = PR_TRUE;
    run = 1;
    /* nspr can't handle negative years it seems.*/
 }
  /*
    http://www.w3.org/TR/xmlschema-2/#date
    (-)CCYY-MM-DDT
  */
#ifdef DEBUG
   printf("\n    Validating Date part");
#endif

  while ( (run <= length) && (!doneParsing) ) {
    switch (parseState) {
      case 0: {
        // year
        if (strValue[run] == '-') {
          // finished
          parseState++;
          if (strlen(parseBuffer) < 4)
            doneParsing = PR_TRUE;

          strcpy(year, parseBuffer);
          bufferRun = 0;
        } else {
          parseBuffer[bufferRun] = strValue[run];
          bufferRun++;
        }

        break;
      }

      case 1: {
        // month
        if (strValue[run] == '-') {
          // finished
          parseState++;
       
          if (strlen(month) != 2)
            doneParsing = PR_TRUE;

          bufferRun = 0;
        } else {
          if (bufferRun > 1)
            doneParsing = PR_TRUE;
          else {
            month[bufferRun] = strValue[run];          
            bufferRun++;
          }
        }

        break;
      }

      case 2: {
        // day
        if (strValue[run] == 'T') {
          // finished
          parseState++;

          if (strlen(day) == 2)
            isValid = PR_TRUE;

          doneParsing = PR_TRUE;
        } else {
          if (bufferRun > 1)
            doneParsing = PR_TRUE;
          else {
            day[bufferRun] = strValue[run];
            bufferRun++;
          }
        }

        break;
      }
    }

    run++;
  }

  if (isValid) {
    nsCAutoString monthShorthand;
    nsSchemaValidatorUtils::GetMonthShorthand(month, monthShorthand);

    // 22-AUG-1993
    sprintf(fulldate, "%s-%s-%s",day, monthShorthand.get(), year);

#ifdef DEBUG
    printf("\n      Parsed date is %s", fulldate);
#endif
    PRStatus status = PR_ParseTimeString(fulldate, 
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
    strcpy(rv_day, day);
    strcpy(rv_month, month);
    strcpy(rv_year, year);
  }

#ifdef DEBUG
  printf("\n      Date is %s \n", ((isValid) ? "Valid" : "Not Valid"));
#endif

  return isValid;
}

// parses a string as a schema time type and returns the parsed
// hour/minute/second/fraction seconds as well as if its a valid
// schema time type.
PRBool nsSchemaValidatorUtils::ParseSchemaTime(const char * strValue,
  char *rv_hour, char *rv_minute, char *rv_second, char *rv_fraction_second)
{
  PRBool isValid = PR_FALSE;
  
  int run = 0;
  int length = strlen(strValue);
  PRBool doneParsing = PR_FALSE;

  int parseState = 0;
  int bufferRun = 0;
  char hour[3] = "";
  char minute[3] = "";
  char second[3] = "";
  // there can be any amount of fraction seconds
  char fraction_seconds[80] = "";
  char timeZoneSign[2] = "";
  char timezoneHour[3] = "";
  char timezoneMinute[3] = "";

  PRTime dateTime;

  char fulldate[100] = "";

  // overflow check
  if (strlen(strValue) >= sizeof(fulldate))
    return PR_FALSE;

  /*
    http://www.w3.org/TR/xmlschema-2/#time
    (-)CCYY-MM-DDT
  */

#ifdef DEBUG
  printf("\n    Validating Time part");
#endif

  while ( (run <= length) && (!doneParsing) ) {
    switch (parseState) {

     case 0: {
        // hour
        if (strValue[run] == ':') {
          // finished
          parseState++;

          if ( (strlen(hour) != 2) || (strcmp(hour, "24") == 1))
            doneParsing = PR_TRUE;

          bufferRun = 0;
        } else {
          if (bufferRun > 1)
            doneParsing = PR_TRUE;
          else {
            hour[bufferRun] = strValue[run];          
            bufferRun++;
          }
        }

        break;
      }

     case 1: {
        // minutes
        if (strValue[run] == ':') {
          // finished
          parseState++;

          if ((strlen(minute) != 2) || (strcmp(minute, "59") == 1))
            doneParsing = PR_TRUE;

          bufferRun = 0;
        } else {
          if ((bufferRun > 1) || (strValue[run] > '9') || (strValue[run] < '0'))
            doneParsing = PR_TRUE;
          else {
            minute[bufferRun] = strValue[run];          
            bufferRun++;
          }
        }

        break;
      }

     case 2: {
        // seconds
        if ((strValue[run] == 'Z') || (strValue[run] == '-') || (strValue[run] == '+')) {
          // finished
          parseState = 4;

          if (strlen(second) != 2)
            doneParsing = PR_TRUE;
          else if (strValue[run] == 'Z') {
            if (strValue[run+1] == '\0')
              isValid = PR_TRUE;
            doneParsing = PR_TRUE;
          }

          timeZoneSign[0] = strValue[run];

          bufferRun = 0;
        } else if (strValue[run] == '.') {
          // fractional seconds
          parseState = 3;

          if (strlen(second) != 2)
            doneParsing = PR_TRUE;

          bufferRun = 0;

        } else {
          if (bufferRun > 1)
            doneParsing = PR_TRUE;
          else {
            second[bufferRun] = strValue[run];          
            bufferRun++;
          }
        }

        break;
      }

      case 3: {
        // fraction seconds
        if ((strValue[run] == 'Z') || (strValue[run] == '-') || (strValue[run] == '+')) {
          // finished
          parseState = 4;

          if (!IsStringANumber(fraction_seconds)) {
            doneParsing = PR_TRUE;
          } else if (strValue[run] == 'Z') {
            if (strValue[run+1] == '\0')
              isValid = PR_TRUE;

            doneParsing = PR_TRUE;
          }

          timeZoneSign[0] = strValue[run];

          bufferRun = 0;
        } else { 
          if (bufferRun >= sizeof(fraction_seconds)) {
            // more fraction seconds than the variable can store
            // so stop parseing
            doneParsing = PR_TRUE;
          } else {
            fraction_seconds[bufferRun] = strValue[run];
            bufferRun++;
          }
        }

        break;
      }

      case 4: {
        // validate timezone
        char timezone[6] = "";
        int tzLength = strlen(strValue) - run;

        // timezone is hh:mm
        if (tzLength == 5) {
          strncat(timezone, &strValue[run], tzLength);
          timezone[sizeof(timezone)-1] = '\0';
          isValid = ParseSchemaTimeZone(timezone, timezoneHour, timezoneMinute);
        }
        doneParsing = PR_TRUE;      
        
        break;
      }
    }
    run++;
  }

  if (isValid) {
    // 10:59:12.82
    // use a dummy year to make nspr do the work for us

    sprintf(fulldate, "1-1-90 %s:%s:%s%s",
      hour,
      minute,
      second,
      timeZoneSign);

    if (timeZoneSign[0] != 'Z') {
      strcat(fulldate, timezoneHour);
      strcat(fulldate, ":");
      strcat(fulldate, timezoneMinute);
    }
    fulldate[sizeof(fulldate)-1] = '\0';

#ifdef DEBUG
    printf("\n     new time is %s", fulldate);
#endif

    PRStatus foo = PR_ParseTimeString(fulldate, 
                                      PR_TRUE, &dateTime); 
    if (foo == -1) {
      isValid = PR_FALSE;
    } else {

      PRExplodedTime explodedDateTime;
      PR_ExplodeTime(dateTime, PR_GMTParameters, &explodedDateTime);

      sprintf(rv_hour, "%d", explodedDateTime.tm_hour);
      sprintf(rv_minute, "%d", explodedDateTime.tm_min);
      strcpy(rv_second, second);
      strcpy(rv_fraction_second, fraction_seconds);
    }
  }

#ifdef DEBUG
  printf("\n     Time is %s \n", ((isValid) ? "Valid" : "Not Valid"));
#endif

  return isValid;
}

PRBool nsSchemaValidatorUtils::ParseSchemaTimeZone(const char * strValue,
  char *rv_tzhour, char *rv_tzminute)
{
  PRBool isValid = PR_FALSE;
  
  int run = 0;
  int bufferRun = 0;
  int length = strlen(strValue);

  PRBool doneParsing = PR_FALSE;

  int parseState = 0;
  char timezoneHour[3] = "";
  char timezoneMinute[3] = "";

  char fulldate[100] = "";

  // overflow check
  if (strlen(fulldate) >= strlen(strValue))
    return PR_FALSE;

#ifdef DEBUG
  printf("\n    Validating TimeZone");
#endif

  while ( (run <= length) && (!doneParsing) ) {
    switch (parseState) {

      case 0: {
        // timezone hh
        if (strValue[run] == ':') {
          parseState = 1;

          if ((strlen(timezoneHour) != 2) || (strcmp(timezoneHour, "24") == 1))
            doneParsing = PR_TRUE;

          bufferRun = 0;
        } else {
          if ((bufferRun > 1) || (strValue[run] > '9') || (strValue[run] < '0')){
            doneParsing = PR_TRUE;
          } else {
            timezoneHour[bufferRun] = strValue[run];          
            bufferRun++;
          }
        }

        break;
      }

      case 1: {
        // timezone mm
        if (strValue[run] == '\0') {
          if ((strlen(timezoneMinute) == 2) || (strcmp(timezoneMinute, "59") == 1))
            isValid = PR_TRUE;
        } else {
          if ((bufferRun > 1) || (strValue[run] > '9') || (strValue[run] < '0'))
            doneParsing = PR_TRUE;
          else {
            timezoneMinute[bufferRun] = strValue[run];          
            bufferRun++;
          }
        }

        break;
      }
    }
    run++;
  }

  if (isValid) {
    timezoneHour[2] = '\0';
    timezoneMinute[2] = '\0';
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
int nsSchemaValidatorUtils::CompareExplodedDateTime(PRExplodedTime aDateTime1, 
  PRBool aDateTime1IsNegative, PRExplodedTime aDateTime2, PRBool aDateTime2IsNegative) 
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
int nsSchemaValidatorUtils::CompareExplodedDate(PRExplodedTime aDateTime1, 
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
int nsSchemaValidatorUtils::CompareExplodedTime(PRExplodedTime aDateTime1, 
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

void nsSchemaValidatorUtils::GetMonthShorthand(char* aMonth, nsACString & aReturn) 
{
   if (strcmp(aMonth, "01") == 0)
    aReturn.AssignLiteral("Jan");
  else if (strcmp(aMonth, "02") == 0)
    aReturn.AssignLiteral("Feb");
  else if (strcmp(aMonth, "03") == 0)
     aReturn.AssignLiteral("Mar");
  else if (strcmp(aMonth, "04") == 0)
    aReturn.AssignLiteral("Apr");
  else if (strcmp(aMonth, "05") == 0)
    aReturn.AssignLiteral("May");
  else if (strcmp(aMonth, "06") == 0)
    aReturn.AssignLiteral("Jun");
  else if (strcmp(aMonth, "07") == 0)
    aReturn.AssignLiteral("Jul");
  else if (strcmp(aMonth, "08") == 0)
    aReturn.AssignLiteral("Aug");
  else if (strcmp(aMonth, "09") == 0)
    aReturn.AssignLiteral("Sep");
  else if (strcmp(aMonth, "10") == 0)
    aReturn.AssignLiteral("Oct");
  else if (strcmp(aMonth, "11") == 0)
    aReturn.AssignLiteral("Nov");
  else if (strcmp(aMonth, "12") == 0)
    aReturn.AssignLiteral("Dec");
}

PRBool nsSchemaValidatorUtils::IsStringANumber(char* aChar)
{
  char * pEnd;
  long intValue = strtol(aChar, &pEnd, 10);

  return (*pEnd == '\0');
}


/*
 -1 - aYearMonth1 < aYearMonth2
  0 - equal
  1 - aYearMonth1 > aYearMonth2
*/
int nsSchemaValidatorUtils::CompareGYearMonth(Schema_GYearMonth aYearMonth1, 
  Schema_GYearMonth aYearMonth2)
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
int nsSchemaValidatorUtils::CompareGMonthDay(Schema_GMonthDay aMonthDay1,
  Schema_GMonthDay aMonthDay2)
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

void nsSchemaValidatorUtils::RemoveLeadingZeros(nsAString & aString)
{
  nsAString::const_iterator start, end;
  aString.BeginReading(start);
  aString.EndReading(end);

  PRBool done = PR_FALSE;
  PRUint32 count = 0;

  while ((start != end) && !done)
  {
    if (*start++ == '0') {
      ++count;
    } else {
      done = PR_TRUE;
    }
  }

  // finally, remove the leading zeros
  aString.Cut(0, count);
}

