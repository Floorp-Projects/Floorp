/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "prprf.h"
#include "nsAutoBuffer.h"

#include <ctype.h>

#if defined(XP_WIN)
#  include "nsIWin32Locale.h"
#elif defined(XP_OS2)
#  include "unidef.h"
#  include "nsIOS2Locale.h"
#elif defined(XP_MAC) || defined(XP_MACOSX)
#  include <Script.h>
#  include "nsIMacLocale.h"
#elif defined(XP_UNIX) || defined(XP_BEOS)
#  include <locale.h>
#  include <stdlib.h>
#  include "nsIPosixLocale.h"
#endif

//
// implementation constants
const int LocaleListLength = 6;
const char* LocaleList[LocaleListLength] = 
{
	NSILOCALE_COLLATE,
	NSILOCALE_CTYPE,
	NSILOCALE_MONETARY,
	NSILOCALE_NUMERIC,
	NSILOCALE_TIME,
	NSILOCALE_MESSAGE
};

#define NSILOCALE_MAX_ACCEPT_LANGUAGE	16
#define NSILOCALE_MAX_ACCEPT_LENGTH		18

#if (defined(XP_UNIX) && !defined(XP_MACOSX)) || defined(XP_BEOS) || defined(XP_OS2)
static int posix_locale_category[LocaleListLength] =
{
  LC_COLLATE,
  LC_CTYPE,
  LC_MONETARY,
  LC_NUMERIC,
  LC_TIME,
#ifdef HAVE_I18N_LC_MESSAGES
  LC_MESSAGES
#else
  LC_CTYPE
#endif
};
#endif

#ifdef XP_MACOSX
#if !defined(__COREFOUNDATION_CFLOCALE__)
typedef void* CFLocaleRef;
#endif
typedef CFLocaleRef (*fpCFLocaleCopyCurrent_type) (void);
typedef CFStringRef (*fpCFLocaleGetIdentifier_type) (CFLocaleRef);
#endif

//
// nsILocaleService implementation
//
class nsLocaleService: public nsILocaleService {

public:
	
	//
	// nsISupports
	//
	NS_DECL_ISUPPORTS

	//
	// nsILocaleService
	//
    NS_DECL_NSILOCALESERVICE


	nsLocaleService(void);
	virtual ~nsLocaleService(void);

protected:

	nsresult SetSystemLocale(void);
	nsresult SetApplicationLocale(void);

	nsCOMPtr<nsILocale>				mSystemLocale;
	nsCOMPtr<nsILocale>				mApplicationLocale;

};

//
// nsILocaleDefinition implementation
//
class nsLocaleDefinition: public nsILocaleDefinition {
	friend class nsLocaleService;

public:

	//
	// nsISupports
	//
	NS_DECL_ISUPPORTS

	//
	// nsILocaleDefintion
	//
	NS_IMETHOD SetLocaleCategory(const nsAString &category, const nsAString &value);

protected:

	nsLocaleDefinition();
	virtual ~nsLocaleDefinition();

	nsLocale*	mLocaleDefinition;
};




//
// nsLocaleService methods
//
nsLocaleService::nsLocaleService(void) 
    : mSystemLocale(0), mApplicationLocale(0)
{
#if defined(XP_WIN)
    nsCOMPtr<nsIWin32Locale> win32Converter = do_CreateInstance(NS_WIN32LOCALE_CONTRACTID);

    NS_ASSERTION(win32Converter, "nsLocaleService: can't get win32 converter\n");

    nsAutoString        xpLocale;
    if (win32Converter) {
        
        nsresult result;
        //
        // get the system LCID
        //
        LCID win_lcid = GetSystemDefaultLCID();
        if (win_lcid==0) { return;}
            result = win32Converter->GetXPLocale(win_lcid, xpLocale);
        if (NS_FAILED(result)) { return;}
            result = NewLocale(xpLocale, getter_AddRefs(mSystemLocale));
        if (NS_FAILED(result)) { return;}

        //
        // get the application LCID
        //
        win_lcid = GetUserDefaultLCID();
        if (win_lcid==0) { return;}
            result = win32Converter->GetXPLocale(win_lcid, xpLocale);
        if (NS_FAILED(result)) { return;}
            result = NewLocale(xpLocale, getter_AddRefs(mApplicationLocale));
        if (NS_FAILED(result)) { return;}
    }
#endif
#if (defined(XP_UNIX) && !defined(XP_MACOSX)) || defined(XP_BEOS)
    nsCOMPtr<nsIPosixLocale> posixConverter = do_CreateInstance(NS_POSIXLOCALE_CONTRACTID);

    nsAutoString xpLocale;
    if (posixConverter) {
        nsAutoString category;
        nsLocale* resultLocale;
        int i;

        resultLocale = new nsLocale();
        if ( resultLocale == NULL ) { 
            return; 
        }
        for( i = 0; i < LocaleListLength; i++ ) {
            nsresult result;
            char* lc_temp = setlocale(posix_locale_category[i], "");
            category.AssignWithConversion(LocaleList[i]);
            if (lc_temp != nsnull) {
                result = posixConverter->GetXPLocale(lc_temp, xpLocale);
            } else {
                char* lang = getenv("LANG");
                if ( lang == nsnull )
                    result = posixConverter->GetXPLocale("en-US", xpLocale);
                else
                    result = posixConverter->GetXPLocale(lang, xpLocale); 
            }
            if (NS_FAILED(result)) {
                return;
            }
            resultLocale->AddCategory(category, xpLocale);
        }
        mSystemLocale = do_QueryInterface(resultLocale);
        mApplicationLocale = do_QueryInterface(resultLocale);
    }  // if ( NS_SUCCEEDED )...
       
#endif // XP_UNIX || XP_BEOS
#if defined(XP_OS2)
    nsCOMPtr<nsIOS2Locale> os2Converter = do_CreateInstance(NS_OS2LOCALE_CONTRACTID);
    nsAutoString xpLocale;
    if (os2Converter) {
        nsAutoString category;
        nsLocale* resultLocale;
        int i;

        resultLocale = new nsLocale();
        if ( resultLocale == NULL ) { 
            return; 
        }

        LocaleObject locale_object = NULL;
        int result = UniCreateLocaleObject(UNI_UCS_STRING_POINTER,
                                           (UniChar *)L"", &locale_object);
        if (result != ULS_SUCCESS) {
            int result = UniCreateLocaleObject(UNI_UCS_STRING_POINTER,
                                               (UniChar *)L"en_US", &locale_object);
        }
        char* lc_temp;
        for( i = 0; i < LocaleListLength; i++ ) {
            lc_temp = nsnull;
            UniQueryLocaleObject(locale_object,
                                 posix_locale_category[i],
                                 UNI_MBS_STRING_POINTER,
                                 (void **)&lc_temp);
            category.AssignWithConversion(LocaleList[i]);
            nsresult result;
            if (lc_temp != nsnull)
                result = os2Converter->GetXPLocale(lc_temp, xpLocale);
            else {
                char* lang = getenv("LANG");
                if ( lang == nsnull ) {
                    result = os2Converter->GetXPLocale("en-US", xpLocale);
                }
                else
                    result = os2Converter->GetXPLocale(lang, xpLocale); 
            }
            if (NS_FAILED(result)) {
                UniFreeMem(lc_temp);
                UniFreeLocaleObject(locale_object);
                return;
            }
            resultLocale->AddCategory(category, xpLocale);
            UniFreeMem(lc_temp);
        }
        UniFreeLocaleObject(locale_object);
        mSystemLocale = do_QueryInterface(resultLocale);
        mApplicationLocale = do_QueryInterface(resultLocale);
    }  // if ( NS_SUCCEEDED )...

#endif

#if defined(XP_MAC) || defined(XP_MACOSX)
    // On MacOSX, the recommended way to get the user's current locale is to use
    // the CFLocale APIs.  However, these are only available on 10.3 and later.
    // So for the older systems, we have to keep using the Script Manager APIs.

    static PRBool checked = PR_FALSE;
    static fpCFLocaleCopyCurrent_type fpCFLocaleCopyCurrent = NULL;
    static fpCFLocaleGetIdentifier_type fpCFLocaleGetIdentifier = NULL;

    if (!checked)
    {
        CFBundleRef bundle =
            ::CFBundleGetBundleWithIdentifier(CFSTR("com.apple.Carbon"));
        if (bundle)
        {
            // We dynamically load these two functions and only use them if
            // they are available (OS 10.3+).
            fpCFLocaleCopyCurrent = (fpCFLocaleCopyCurrent_type)
                ::CFBundleGetFunctionPointerForName(bundle,
                                                    CFSTR("CFLocaleCopyCurrent"));
            fpCFLocaleGetIdentifier = (fpCFLocaleGetIdentifier_type)
                ::CFBundleGetFunctionPointerForName(bundle,
                                                    CFSTR("CFLocaleGetIdentifier"));
        }
        checked = PR_TRUE;
    }

    if (fpCFLocaleCopyCurrent)
    {
        // Get string representation of user's current locale
        CFLocaleRef userLocaleRef = fpCFLocaleCopyCurrent();
        CFStringRef userLocaleStr = fpCFLocaleGetIdentifier(userLocaleRef);
        ::CFRetain(userLocaleStr);

        nsAutoBuffer<UniChar, 32> buffer;
        int size = ::CFStringGetLength(userLocaleStr);
        if (buffer.EnsureElemCapacity(size))
        {
            CFRange range = ::CFRangeMake(0, size);
            ::CFStringGetCharacters(userLocaleStr, range, buffer.get());
            buffer.get()[size] = 0;

            // Convert the locale string to the format that Mozilla expects
            nsAutoString xpLocale(buffer.get());
            xpLocale.ReplaceChar('_', '-');

            nsresult rv = NewLocale(xpLocale, getter_AddRefs(mSystemLocale));
            if (NS_SUCCEEDED(rv)) {
              mApplicationLocale = mSystemLocale;
            }
        }

        ::CFRelease(userLocaleStr);
        ::CFRelease(userLocaleRef);

        NS_ASSERTION(mApplicationLocale, "Failed to create locale objects");
    }
    else
    {
        // Legacy MacOSX locale code
        long script = GetScriptManagerVariable(smSysScript);
        long lang = GetScriptVariable(smSystemScript,smScriptLang);
        long region = GetScriptManagerVariable(smRegionCode);
        nsCOMPtr<nsIMacLocale> macConverter = do_CreateInstance(NS_MACLOCALE_CONTRACTID);
        if (macConverter) {
            nsresult result;
            nsAutoString xpLocale;
            result = macConverter->GetXPLocale((short)script,(short)lang,(short)region, xpLocale);
            if (NS_SUCCEEDED(result)) {
                result = NewLocale(xpLocale, getter_AddRefs(mSystemLocale));
                if (NS_SUCCEEDED(result)) {
                    mApplicationLocale = mSystemLocale;
                }
            }
        }
    }
#endif
}

nsLocaleService::~nsLocaleService(void)
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLocaleService, nsILocaleService)

NS_IMETHODIMP
nsLocaleService::NewLocale(const nsAString &aLocale, nsILocale **_retval)
{
	int		i;
	nsresult result;

	*_retval = (nsILocale*)nsnull;
	
	nsLocale* resultLocale = new nsLocale();
	if (!resultLocale) return NS_ERROR_OUT_OF_MEMORY;

	for(i=0;i<LocaleListLength;i++) {
		nsString category; category.AssignWithConversion(LocaleList[i]);
		result = resultLocale->AddCategory(category, aLocale);
		if (NS_FAILED(result)) { delete resultLocale; return result;}
	}

	return resultLocale->QueryInterface(NS_GET_IID(nsILocale),(void**)_retval);
}


NS_IMETHODIMP
nsLocaleService::NewLocaleObject(nsILocaleDefinition *localeDefinition, nsILocale **_retval)
{
	if (!localeDefinition || !_retval) return NS_ERROR_INVALID_ARG;

	nsLocale* new_locale = new nsLocale(NS_STATIC_CAST(nsLocaleDefinition*,localeDefinition)->mLocaleDefinition);
	if (!new_locale) return NS_ERROR_OUT_OF_MEMORY;

	return new_locale->QueryInterface(NS_GET_IID(nsILocale),(void**)_retval);
}


NS_IMETHODIMP
nsLocaleService::GetSystemLocale(nsILocale **_retval)
{
	if (mSystemLocale) {
		NS_ADDREF(*_retval = mSystemLocale);
		return NS_OK;
	}

	*_retval = (nsILocale*)nsnull;
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocaleService::GetApplicationLocale(nsILocale **_retval)
{
	if (mApplicationLocale) {
		NS_ADDREF(*_retval = mApplicationLocale);
		return NS_OK;
	}

	*_retval=(nsILocale*)nsnull;
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocaleService::GetLocaleFromAcceptLanguage(const char *acceptLanguage, nsILocale **_retval)
{
  char* input;
  char* cPtr;
  char* cPtr1;
  char* cPtr2;
  int i;
  int j;
  int countLang = 0;
  char	acceptLanguageList[NSILOCALE_MAX_ACCEPT_LANGUAGE][NSILOCALE_MAX_ACCEPT_LENGTH];
  nsresult	result;

  input = new char[strlen(acceptLanguage)+1];
  NS_ASSERTION(input!=nsnull,"nsLocaleFactory::GetLocaleFromAcceptLanguage: memory allocation failed.");
  if (input == (char*)NULL){ return NS_ERROR_OUT_OF_MEMORY; }

  strcpy(input, acceptLanguage);
  cPtr1 = input-1;
  cPtr2 = input;

  /* put in standard form */
  while (*(++cPtr1)) {
    if      (isalpha(*cPtr1))  *cPtr2++ = tolower(*cPtr1); /* force lower case */
    else if (isspace(*cPtr1))  ;                           /* ignore any space */
    else if (*cPtr1=='-')      *cPtr2++ = '_';             /* "-" -> "_"       */
    else if (*cPtr1=='*')      ;                           /* ignore "*"       */
    else                       *cPtr2++ = *cPtr1;          /* else unchanged   */
  }
  *cPtr2 = '\0';

  countLang = 0;

  if (strchr(input,';')) {
    /* deal with the quality values */

    float qvalue[NSILOCALE_MAX_ACCEPT_LANGUAGE];
    float qSwap;
    float bias = 0.0f;
    char* ptrLanguage[NSILOCALE_MAX_ACCEPT_LANGUAGE];
    char* ptrSwap;

    cPtr = nsCRT::strtok(input,",",&cPtr2);
    while (cPtr) {
      qvalue[countLang] = 1.0f;
      /* add extra parens to get rid of warning */
      if ((cPtr1 = strchr(cPtr,';')) != nsnull) {
        PR_sscanf(cPtr1,";q=%f",&qvalue[countLang]);
        *cPtr1 = '\0';
      }
      if (strlen(cPtr)<NSILOCALE_MAX_ACCEPT_LANGUAGE) {     /* ignore if too long */
        qvalue[countLang] -= (bias += 0.0001f); /* to insure original order */
        ptrLanguage[countLang++] = cPtr;
        if (countLang>=NSILOCALE_MAX_ACCEPT_LANGUAGE) break; /* quit if too many */
      }
      cPtr = nsCRT::strtok(cPtr2,",",&cPtr2);
    }

    /* sort according to decending qvalue */
    /* not a very good algorithm, but count is not likely large */
    for ( i=0 ; i<countLang-1 ; i++ ) {
      for ( j=i+1 ; j<countLang ; j++ ) {
        if (qvalue[i]<qvalue[j]) {
          qSwap     = qvalue[i];
          qvalue[i] = qvalue[j];
          qvalue[j] = qSwap;
          ptrSwap        = ptrLanguage[i];
          ptrLanguage[i] = ptrLanguage[j];
          ptrLanguage[j] = ptrSwap;
        }
      }
    }
    for ( i=0 ; i<countLang ; i++ ) {
      PL_strncpyz(acceptLanguageList[i],ptrLanguage[i],NSILOCALE_MAX_ACCEPT_LENGTH);
    }

  } else {
    /* simple case: no quality values */

    cPtr = nsCRT::strtok(input,",",&cPtr2);
    while (cPtr) {
      if (strlen(cPtr)<NSILOCALE_MAX_ACCEPT_LENGTH) {        /* ignore if too long */
        PL_strncpyz(acceptLanguageList[countLang++],cPtr,NSILOCALE_MAX_ACCEPT_LENGTH);
        if (countLang>=NSILOCALE_MAX_ACCEPT_LENGTH) break; /* quit if too many */
      }
      cPtr = nsCRT::strtok(cPtr2,",",&cPtr2);
    }
  }

  //
  // now create the locale 
  //
  result = NS_ERROR_FAILURE;
  if (countLang>0) {
	  result = NewLocale(NS_ConvertASCIItoUTF16(acceptLanguageList[0]), _retval);
  }

  //
  // clean up
  //
  delete[] input;
  return result;
}


nsresult
nsLocaleService::GetLocaleComponentForUserAgent(nsAString& retval)
{
    nsCOMPtr<nsILocale>     system_locale;
    nsresult                result;

    result = GetSystemLocale(getter_AddRefs(system_locale));
    if (NS_SUCCEEDED(result))
    {
        result = system_locale->
                 GetCategory(NS_LITERAL_STRING(NSILOCALE_MESSAGE), retval);
        return result;
    }

    return result;
}



nsresult
NS_NewLocaleService(nsILocaleService** result)
{
  if(!result)
    return NS_ERROR_NULL_POINTER;
  *result = new nsLocaleService();
  if (! *result)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result);
  return NS_OK;
}


//
// nsLocaleDefinition methods
//

NS_IMPL_ISUPPORTS1(nsLocaleDefinition,nsILocaleDefinition)

nsLocaleDefinition::nsLocaleDefinition(void)
{
	mLocaleDefinition = new nsLocale;
	if (mLocaleDefinition)
		mLocaleDefinition->AddRef();
}

nsLocaleDefinition::~nsLocaleDefinition(void)
{
	if (mLocaleDefinition)
		mLocaleDefinition->Release();
}

NS_IMETHODIMP
nsLocaleDefinition::SetLocaleCategory(const nsAString &category, const nsAString &value)
{
	if (mLocaleDefinition)
		return mLocaleDefinition->AddCategory(category,value);
	
	return NS_ERROR_FAILURE;
}
