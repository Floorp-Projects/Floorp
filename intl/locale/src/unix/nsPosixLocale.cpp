/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsILocale.h"
#include "nsPosixLocale.h"
#include "nsLocaleCID.h"
#include "prprf.h"
#include "nsFileSpec.h"

NS_DEFINE_IID(kIPosixLocaleIID, NS_IPOSIXLOCALE_IID);
NS_DEFINE_IID(kPosixLocaleCID, NS_POSIXLOCALE_CID);

/* nsPosixLocale ISupports */
NS_IMPL_ISUPPORTS(nsPosixLocale,kIPosixLocaleIID)

nsPosixLocale::nsPosixLocale(void)
{
  NS_INIT_REFCNT();
}

nsPosixLocale::~nsPosixLocale(void)
{

}

NS_IMETHODIMP 
nsPosixLocale::GetPlatformLocale(const nsString* locale,char* posixLocale, size_t length)
{
  char  country_code[3];
  char  lang_code[3];
  char  extra[65];
  char  posix_locale[128];
  nsAutoCString xp_locale(*locale);

  if ((const char *)xp_locale!=nsnull) {
    if (!ParseLocaleString((const char *)xp_locale,lang_code,country_code,extra,'-')) {
//      strncpy(posixLocale,"C",length);
      PL_strncpyz(posixLocale,(const char *)xp_locale,length);  // use xp locale if parse failed
      return NS_OK;
    }

    if (*country_code) {
      if (*extra) {
        PR_snprintf(posix_locale,128,"%s_%s.%s",lang_code,country_code,extra);
      }
      else {
        PR_snprintf(posix_locale,128,"%s_%s",lang_code,country_code);
      }
    }
    else {
      if (*extra) {
        PR_snprintf(posix_locale,128,"%s.%s",lang_code,extra);
      }
      else {
        PR_snprintf(posix_locale,128,"%s",lang_code);
      }
    }

    strncpy(posixLocale,posix_locale,length);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPosixLocale::GetXPLocale(const char* posixLocale, nsString* locale)
{
  char  country_code[3];
  char  lang_code[3];
  char  extra[65];
  char  posix_locale[128];

  if (posixLocale!=nsnull) {
    if (strcmp(posixLocale,"C")==0 || strcmp(posixLocale,"POSIX")==0) {
      locale->AssignWithConversion("en-US");
      return NS_OK;
    }
    if (!ParseLocaleString(posixLocale,lang_code,country_code,extra,'_')) {
//      * locale = "x-user-defined";
      locale->AssignWithConversion(posixLocale);  // use posix if parse failed
      return NS_OK;
    }

    if (*country_code) {
      if (*extra) {
        PR_snprintf(posix_locale,128,"%s-%s.%s",lang_code,country_code,extra);
      }
      else {
        PR_snprintf(posix_locale,128,"%s-%s",lang_code,country_code);
      }
    } 
    else {
      if (*extra) {
        PR_snprintf(posix_locale,128,"%s.%s",lang_code,extra);
      }
      else {
        PR_snprintf(posix_locale,128,"%s",lang_code);
      }
    }

    locale->AssignWithConversion(posix_locale);
    return NS_OK;

  }

    return NS_ERROR_FAILURE;

}

//
// returns PR_FALSE/PR_TRUE depending on if it was of the form LL-CC.Extra
PRBool
nsPosixLocale::ParseLocaleString(const char* locale_string, char* language, char* country, char* extra, char separator)
{
  PRUint32 len = PL_strlen(locale_string);

  *language = '\0';
  *country = '\0';
  *extra = '\0';

  if (2 == len) {
    language[0]=locale_string[0];
    language[1]=locale_string[1];
    language[2]='\0';
    country[0]='\0';
  } 
  else if (5 == len) {
    language[0]=locale_string[0];
    language[1]=locale_string[1];
    language[2]='\0';
    country[0]=locale_string[3];
    country[1]=locale_string[4];
    country[2]='\0';
  }
  else if (4 <= len && '.' == locale_string[2]) {
    PL_strcpy(extra, &locale_string[3]);
    language[0]=locale_string[0];
    language[1]=locale_string[1];
    language[2]='\0';
  }
  else if (7 <= len && '.' == locale_string[5]) {
    PL_strcpy(extra, &locale_string[6]);
    language[0]=locale_string[0];
    language[1]=locale_string[1];
    language[2]='\0';
    country[0]=locale_string[3];
    country[1]=locale_string[4];
    country[2]='\0';
  }
  else {
    return PR_FALSE;
  }

  return PR_TRUE;
}
