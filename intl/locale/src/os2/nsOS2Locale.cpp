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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 *
 * 07/05/2000       IBM Corp.      renamed file to nsOS2Locale.cpp from ns LocaleOS2.cpp
 *                                 added implementation for GetPlatformLocale, and GetXPlatformLocale using Unix model
 *                                 created ParseLocaleString method
 */

#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsILocale.h"
#include "nsOS2Locale.h"
#include "nsLocaleCID.h"
#include "prprf.h"
#include "nsReadableUtils.h"
#include <unidef.h>

extern "C" {
#include <callconv.h>
int APIENTRY UniQueryLocaleValue ( const LocaleObject locale_object,
                                   LocaleItem item,
                                  int *info_item);
}

/* nsOS2Locale ISupports */
NS_IMPL_ISUPPORTS1(nsOS2Locale,nsIOS2Locale)

nsOS2Locale::nsOS2Locale(void)
{
}

nsOS2Locale::~nsOS2Locale(void)
{

}

/* Workaround for GCC problem */
#ifndef LOCI_sName
#define LOCI_sName ((LocaleItem)100)
#endif

NS_IMETHODIMP 
nsOS2Locale::GetPlatformLocale(const nsAString& locale, PULONG os2Codepage)
{
  LocaleObject locObj = NULL;
  int codePage;
  nsAutoString tempLocale(locale);
  tempLocale.ReplaceChar('-', '_');

 
  int  ret = UniCreateLocaleObject(UNI_UCS_STRING_POINTER, (UniChar *)PromiseFlatString(tempLocale).get(), &locObj);
  if (ret != ULS_SUCCESS)
    UniCreateLocaleObject(UNI_UCS_STRING_POINTER, (UniChar *)L"C", &locObj);

  ret = UniQueryLocaleValue(locObj, LOCI_iCodepage, &codePage);
  if (ret != ULS_SUCCESS)
    return NS_ERROR_FAILURE;

  if (codePage == 437) {
    *os2Codepage = 850;
  } else {
    *os2Codepage = codePage;
  }
  UniFreeLocaleObject(locObj);

  return NS_OK;
}

NS_IMETHODIMP
nsOS2Locale::GetXPLocale(const char* os2Locale, nsAString& locale)
{
  char  country_code[MAX_COUNTRY_CODE_LEN];
  char  lang_code[MAX_LANGUAGE_CODE_LEN];
  char  extra[MAX_EXTRA_LEN];
  char  os2_locale[MAX_LOCALE_LEN];

  if (os2Locale!=nsnull) {
    if (strcmp(os2Locale,"C")==0 || strcmp(os2Locale,"OS2")==0) {
      locale.AssignLiteral("en-US");
      return NS_OK;
    }
    if (!ParseLocaleString(os2Locale,lang_code,country_code,extra,'_')) {
//      * locale = "x-user-defined";
      // use os2 if parse failed
      CopyASCIItoUTF16(nsDependentCString(os2Locale), locale);  
      return NS_OK;
    }

    if (*country_code) {
      if (*extra) {
        PR_snprintf(os2_locale,MAX_LOCALE_LEN,"%s-%s.%s",lang_code,country_code,extra);
      }
      else {
        PR_snprintf(os2_locale,MAX_LOCALE_LEN,"%s-%s",lang_code,country_code);
      }
    } 
    else {
      if (*extra) {
        PR_snprintf(os2_locale,MAX_LOCALE_LEN,"%s.%s",lang_code,extra);
      }
      else {
        PR_snprintf(os2_locale,MAX_LOCALE_LEN,"%s",lang_code);
      }
    }

    CopyASCIItoUTF16(nsDependentCString(os2_locale), locale);  
    return NS_OK;

  }

  return NS_ERROR_FAILURE;
}

// copied from nsPosixLocale::ParseLocaleString:
// returns PR_FALSE/PR_TRUE depending on if it was of the form LL-CC.Extra
// or possibly ll_CC_Extra (depending on the separator, which happens on OS/2
PRBool
nsOS2Locale::ParseLocaleString(const char* locale_string, char* language, char* country, char* extra, char separator)
{
  const char *src = locale_string;
  char modifier[MAX_EXTRA_LEN+1];
  char *dest;
  int dest_space, len;

  *language = '\0';
  *country = '\0';
  *extra = '\0';
  if (strlen(locale_string) < 2) {
    return(PR_FALSE);
  }

  //
  // parse the language part
  //
  dest = language;
  dest_space = MAX_LANGUAGE_CODE_LEN;
  while ((*src) && (isalpha(*src)) && (dest_space--)) {
    *dest++ = tolower(*src++);
  }
  *dest = '\0';
  len = dest - language;
  if ((len != 2) && (len != 3)) {
    NS_ASSERTION((len == 2) || (len == 3), "language code too short");
    NS_ASSERTION(len < 3, "reminder: verify we can handle 3+ character language code in all parts of the system; eg: language packs");
    *language = '\0';
    return(PR_FALSE);
  }

  // check if all done
  if (*src == '\0') {
    return(PR_TRUE);
  }

  if ((*src != '_') && (*src != '-') && (*src != '.') && (*src != '@')) {
    NS_ASSERTION(isalpha(*src), "language code too long");
    NS_ASSERTION(!isalpha(*src), "unexpected language/country separator");
    *language = '\0';
    return(PR_FALSE);
  }

  //
  // parse the country part
  //
  if ((*src == '_') || (*src == '-')) { 
    src++;
    dest = country;
    dest_space = MAX_COUNTRY_CODE_LEN;
    while ((*src) && (isalpha(*src)) && (dest_space--)) {
      *dest++ = toupper(*src++);
    }
    *dest = '\0';
    len = dest - country;
    if (len != 2) {
      NS_ASSERTION(len == 2, "unexpected country code length");
      *language = '\0';
      *country = '\0';
      return(PR_FALSE);
    }
  }

  // check if all done
  if (*src == '\0') {
    return(PR_TRUE);
  }

  if ((*src != '.') && (*src != '@') && (*src != separator)) {
    NS_ASSERTION(isalpha(*src), "country code too long");
    NS_ASSERTION(!isalpha(*src), "unexpected country/extra separator");
    *language = '\0';
    *country = '\0';
    return(PR_FALSE);
  }

  //
  // handle the extra part
  //
  if (*src == '.') { 
    src++;  // move past the extra part separator
    dest = extra;
    dest_space = MAX_EXTRA_LEN;
    while ((*src) && (*src != '@') && (dest_space--)) {
      *dest++ = *src++;
    }
    *dest = '\0';
    len = dest - extra;
    if (len < 1) {
      NS_ASSERTION(len > 0, "found country/extra separator but no extra code");
      *language = '\0';
      *country = '\0';
      *extra = '\0';
      return(PR_FALSE);
    }
  }

  // check if all done
  if (*src == '\0') {
    return(PR_TRUE);
  }

  //
  // handle the modifier part
  //
  if ((*src == '@') || (*src == separator)) { 
    src++;  // move past the modifier separator
    NS_ASSERTION(strcmp("euro",src) == 0, "found non euro modifier");
    dest = modifier;
    dest_space = MAX_EXTRA_LEN;
    while ((*src) && (dest_space--)) {
      *dest++ = *src++;
    }
    *dest = '\0';
    len = dest - modifier;
    if (len < 1) {
      NS_ASSERTION(len > 0, "found modifier separator but no modifier code");
      *language = '\0';
      *country = '\0';
      *extra = '\0';
      *modifier = '\0';
      return(PR_FALSE);
    }
  }

  // check if all done
  if (*src == '\0') {
    return(PR_TRUE);
  }

  NS_ASSERTION(*src == '\0', "extra/modifier code too long");
  *language = '\0';
  *country = '\0';
  *extra = '\0';

  return(PR_FALSE);
}
