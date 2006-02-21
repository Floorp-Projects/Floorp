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

#ifndef __nsSchemaValidatorUtils_h__
#define __nsSchemaValidatorUtils_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsISchema.h"
#include "nsCOMArray.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsISchemaDuration.h"
#include "nsISchemaValidatorRegexp.h"

struct nsSchemaGDay {
  PRUint32 day;            // day represented (1-31)
  PRBool tz_negative;      // is timezone negative
  PRUint32 tz_hour;        // timezone - hour (0-23) - null if not specified
  PRUint32 tz_minute;      // timezone - minute (0-59) - null if not specified
} ;

struct nsSchemaGMonth {
  PRUint32 month;          // month represented (1-12)
  PRBool tz_negative;      // is timezone negative
  PRUint32 tz_hour;        // timezone - hour (0-23) - null if not specified
  PRUint32 tz_minute;      // timezone - minute (0-59) - null if not specified
} ;

struct nsSchemaGYear {
  long year;               // year
  PRBool tz_negative;      // is timezone negative
  PRUint32 tz_hour;        // timezone - hour (0-23) - null if not specified
  PRUint32 tz_minute;      // timezone - minute (0-59) - null if not specified
} ;

struct nsSchemaGYearMonth {
  nsSchemaGYear gYear;
  nsSchemaGMonth gMonth;
} ;

struct nsSchemaGMonthDay {
  nsSchemaGMonth gMonth;
  nsSchemaGDay gDay;
} ;

struct nsMonthShortHand {
  const char *number;
  const char *shortHand;
};

const nsMonthShortHand monthShortHand[] = {
  { "01", "Jan" },
  { "02", "Feb" },
  { "03", "Mar" },
  { "04", "Apr" },
  { "05", "May" },
  { "06", "Jun" },
  { "07", "Jul" },
  { "08", "Aug" },
  { "09", "Sep" },
  { "10", "Oct" },
  { "11", "Nov" },
  { "12", "Dec" }
};

#define kREGEXP_CID "@mozilla.org/xmlextras/schemas/schemavalidatorregexp;1"

class nsSchemaStringFacet
{
public:
  PRBool isDefined;
  nsString value;
  nsSchemaStringFacet() {
    isDefined = PR_FALSE;
  }
};

class nsSchemaIntFacet
{
public:
  PRBool isDefined;
  PRUint32 value;
  nsSchemaIntFacet() {
    isDefined = PR_FALSE;
    value = 0;
  }
};

struct nsSchemaDerivedSimpleType {
  nsISchemaSimpleType* mBaseType;

  nsSchemaIntFacet length;
  nsSchemaIntFacet minLength;
  nsSchemaIntFacet maxLength;

  nsSchemaStringFacet pattern;

  PRBool isWhitespaceDefined;
  unsigned short whitespace;

  nsSchemaStringFacet maxInclusive;
  nsSchemaStringFacet minInclusive;
  nsSchemaStringFacet maxExclusive;
  nsSchemaStringFacet minExclusive;

  nsSchemaIntFacet totalDigits;
  nsSchemaIntFacet fractionDigits;

  nsStringArray enumerationList;
};

class nsSchemaValidatorUtils
{
public:
  static PRBool IsValidSchemaInteger(const nsAString & aNodeValue, long *aResult);
  static PRBool IsValidSchemaInteger(const char* aString, long *aResult);

  static PRBool IsValidSchemaDouble(const nsAString & aNodeValue, double *aResult);
  static PRBool IsValidSchemaDouble(const char* aString, double *aResult);

  static PRBool ParseSchemaDate(const nsAString & aStrValue, char *rv_year,
                                char *rv_month, char *rv_day);
  static PRBool ParseSchemaTime(const nsAString & aStrValue, char *rv_hour,
                                char *rv_minute, char *rv_second,
                                char *rv_fraction_second);

  static PRBool ParseSchemaTimeZone(const nsAString & aStrValue, 
                                    char *rv_tzhour, char *rv_tzminute);

  static void GetMonthShorthand(char* aMonth, nsACString & aReturn);

  static PRBool GetPRTimeFromDateTime(const nsAString & aNodeValue, PRTime *aResult);

  static int CompareExplodedDateTime(PRExplodedTime aDateTime1,
                                     PRBool aDateTime1IsNegative,
                                     PRExplodedTime aDateTime2,
                                     PRBool aDateTime2IsNegative);
  static int CompareExplodedDate(PRExplodedTime aDateTime1, PRExplodedTime aDateTime2);
  static int CompareExplodedTime(PRExplodedTime aDateTime1, PRExplodedTime aDateTime2);

  static int CompareGYearMonth(nsSchemaGYearMonth aYearMonth1, nsSchemaGYearMonth aYearMonth2);
  static int CompareGMonthDay(nsSchemaGMonthDay aMonthDay1, nsSchemaGMonthDay aMonthDay2);

  static PRBool ParseSchemaDuration(const char * aStrValue,
                                    nsISchemaDuration **aDuration);
  static PRBool ParseSchemaDuration(const nsAString & aStrValue,
                                    nsISchemaDuration **aDuration);

  static int CompareStrings(const nsAString & aString1, const nsAString & aString2);

  static int GetMaximumDayInMonthFor(int aYearValue, int aMonthValue);
  static int CompareDurations(nsISchemaDuration *aDuration1,
                              nsISchemaDuration *aDuration2);
  static PRExplodedTime AddDurationToDatetime(PRExplodedTime aDatetime,
                                              nsISchemaDuration *aDuration);

  static PRBool IsValidSchemaNormalizedString(const nsAString & aStrValue);
  static PRBool IsValidSchemaToken(const nsAString & aStrValue);
  static PRBool IsValidSchemaLanguage(const nsAString & aStrValue);

  static PRBool HandleEnumeration(const nsAString &aStrValue,
                                  const nsStringArray &aEnumerationList);

  static void RemoveLeadingZeros(nsAString & aString);
  static void RemoveTrailingZeros(nsAString & aString);

  static nsresult GetDerivedSimpleType(nsISchemaSimpleType *aSimpleType,
                                       nsSchemaDerivedSimpleType *aDerived);
  static void CopyDerivedSimpleType(nsSchemaDerivedSimpleType *aDerivedDest,
                                    nsSchemaDerivedSimpleType *aDerivedSrc);
private:
  nsSchemaValidatorUtils();
  ~nsSchemaValidatorUtils();

protected:
};

#endif // __nsSchemaValidatorUtils_h__
