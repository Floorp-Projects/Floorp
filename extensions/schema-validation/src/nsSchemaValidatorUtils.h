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

#include "nsSchemaValidator.h"

class nsSchemaValidatorUtils
{
public:
  static PRBool IsValidSchemaInteger(const nsAString & aNodeValue, long *aResult);
  static PRBool IsValidSchemaInteger(char* aString, long *aResult);

  static PRBool ParseSchemaDate(const char * strValue,
    char *rv_year, char *rv_month, char *rv_day);
  static PRBool ParseSchemaTime(const char * strValue,
    char *rv_hour, char *rv_minute, char *rv_second, char *rv_fraction_second);
  static PRBool ParseSchemaTimeZone(const char * strValue,
    char *rv_tzhour, char *rv_tzminute);

  static void GetMonthShorthand(char* aMonth, nsACString & aReturn);

  static int CompareExplodedDateTime(PRExplodedTime aDateTime1, PRBool aDateTime1IsNegative,
    PRExplodedTime aDateTime2, PRBool aDateTime2IsNegative);
  static int CompareExplodedDate(PRExplodedTime aDateTime1, PRExplodedTime aDateTime2);
  static int CompareExplodedTime(PRExplodedTime aDateTime1, PRExplodedTime aDateTime2);

  static PRBool IsStringANumber(char* aString);

  static int CompareGYearMonth(Schema_GYearMonth aYearMonth1, Schema_GYearMonth aYearMonth2);
  static int CompareGMonthDay(Schema_GMonthDay aMonthDay1, Schema_GMonthDay aMonthDay2);

  static void RemoveLeadingZeros(nsAString & aString);

private:
  nsSchemaValidatorUtils();
  ~nsSchemaValidatorUtils();

protected:

};

