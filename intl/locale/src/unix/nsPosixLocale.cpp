/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsILocale.h"
#include "nsPosixLocale.h"
#include "nsLocaleCID.h"
#include "prprf.h"

NS_DEFINE_IID(kIPosixLocaleIID, NS_IPOSIXLOCALE_IID);
NS_DEFINE_IID(kPosixLocaleCID, NS_POSIXLOCALE_CID);

/* nsPosixLocale ISupports */
NS_IMPL_ISUPPORTS(nsPosixLocale,kIPosixLocaleIID)

nsPosixLocale::nsPosixLocale(void)
{
#ifdef DEBUG_tague
  fprintf(stderr,"nsLocale: creating nsIPosixLocale implementation\n");
#endif
}

nsPosixLocale::~nsPosixLocale(void)
{

}

NS_IMETHODIMP 
nsPosixLocale::GetPlatformLocale(const nsString* locale,char* posixLocale, size_t length)
{
  char* xp_locale;
  char  country_code[3];
  char  lang_code[3];
  char  region_code[3];
  char  posix_locale[9];

#ifdef DEBUG_tague
  fprintf(stderr,"nsLocale: nsPosixLocale::GetPlatformLocale\n");
#endif
  xp_locale = locale->ToNewCString();
  if (xp_locale!=nsnull) {
    if (!ParseLocaleString(xp_locale,lang_code,country_code,region_code,'-')) {
      strncpy(posixLocale,"C",length);
      return NS_OK;
    }

    if (country_code[0]==0) {
      PR_snprintf(posix_locale,9,"%s%c",lang_code,0);
    } else {
      PR_snprintf(posix_locale,9,"%s_%s%c",lang_code,country_code,0);
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
  char  region_code[3];
  char  posix_locale[9];

  if (posixLocale!=nsnull) {
    if (strcmp(posixLocale,"C")==0 || strcmp(posixLocale,"POSIX")==0) {
      *locale = "x-user-defined";
      return NS_OK;
    }
    if (!ParseLocaleString(posixLocale,lang_code,country_code,region_code,'_')) {
      * locale = "x-user-defined";
      return NS_OK;
    }

    if (country_code[0]==0) {
      PR_snprintf(posix_locale,9,"%s%c",lang_code,0);
    } else {
      PR_snprintf(posix_locale,9,"%s-%s%c",lang_code,country_code,0);
    }

    *locale = posix_locale;
    return NS_OK;

  }

    return NS_ERROR_FAILURE;

}

//
// returns PR_FALSE/PR_TRUE depending on if it was of the form LL-CC-RR
PRBool
nsPosixLocale::ParseLocaleString(const char* locale_string, char* language, char* country, char* region, char separator)
{
	size_t		len;

	len = strlen(locale_string);
	if (len==0 || (len!=2 && len!=5 && len!=8))
		return PR_FALSE;
	
	if (len==2) {
		language[0]=locale_string[0];
		language[1]=locale_string[1];
		language[2]=0;
		country[0]=0;
		region[0]=0;
	} else if (len==5) {
		language[0]=locale_string[0];
		language[1]=locale_string[1];
		language[2]=0;
		country[0]=locale_string[3];
		country[1]=locale_string[4];
		country[2]=0;
		region[0]=0;
		if (locale_string[2]!=separator) return PR_FALSE;
	} else if (len==8) {
		language[0]=locale_string[0];
		language[1]=locale_string[1];
		language[2]=0;
		country[0]=locale_string[3];
		country[1]=locale_string[4];
		country[2]=0;
		region[0]=locale_string[6];
		region[1]=locale_string[7];
		region[2]=0;
		if (locale_string[2]!=separator || locale_string[5]!=separator) return PR_FALSE;
	} else {
		return PR_FALSE;
	}

	return PR_TRUE;
}
