/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsCOMPtr.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"
#include "nsReadableUtils.h"

#include <ctype.h>

#if defined(XP_WIN)
#  include "nsIWin32Locale.h"
#elif defined(XP_OS2)
#  include "unidef.h"
#  include "nsIOS2Locale.h"
#elif defined(XP_UNIX) || defined(XP_BEOS)
#  include <locale.h>
#  include <stdlib.h>
#  include "nsIPosixLocale.h"
#elif defined(XP_MAC)
#  include <script.h>
#  include "nsIMacLocale.h"
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

#if defined(XP_UNIX) || defined(XP_BEOS) || defined(XP_OS2)
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

	nsILocale*					mSystemLocale;
	nsILocale*					mApplicationLocale;

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
	NS_IMETHOD SetLocaleCategory(const PRUnichar *category, const PRUnichar *value);

protected:

	nsLocaleDefinition();
	virtual ~nsLocaleDefinition();

	nsLocale*	mLocaleDefinition;
};




//
// nsLocaleService methods
//
nsLocaleService::nsLocaleService(void)
:	mSystemLocale(nsnull), mApplicationLocale(nsnull)
{
	NS_INIT_REFCNT();
#if defined(XP_WIN)
    nsresult result;
	nsCOMPtr<nsIWin32Locale> win32Converter
        = do_CreateInstance(NS_WIN32LOCALE_CONTRACTID, &result);

	nsString		xpLocale;
	NS_ASSERTION(win32Converter!=NULL,"nsLocaleService: can't get win32 converter\n");
	if (NS_SUCCEEDED(result) && win32Converter) {
		
		//
		// get the system LCID
		//
		LCID win_lcid = GetSystemDefaultLCID();
		if (win_lcid==0) { return;}
		result = win32Converter->GetXPLocale(win_lcid,&xpLocale);
		if (NS_FAILED(result)) { return;}
		result = NewLocale(xpLocale.get(), &mSystemLocale);
		if (NS_FAILED(result)) { return;}

		//
		// get the application LCID
		//
		win_lcid = GetUserDefaultLCID();
		if (win_lcid==0) { return;}
		result = win32Converter->GetXPLocale(win_lcid,&xpLocale);
		if (NS_FAILED(result)) { return;}
		result = NewLocale(xpLocale.get(), &mApplicationLocale);
		if (NS_FAILED(result)) { return;}
	}
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
    nsresult result;
    nsCOMPtr<nsIPosixLocale> posixConverter =
        do_CreateInstance(NS_POSIXLOCALE_CONTRACTID, &result);

    nsAutoString xpLocale, platformLocale;
    if (NS_SUCCEEDED(result) && posixConverter) {
        nsAutoString category, category_platform;
        nsLocale* resultLocale;
        int i;

        resultLocale = new nsLocale();
        if ( resultLocale == NULL ) { 
            return; 
        }
        for( i = 0; i < LocaleListLength; i++ ) {
            char* lc_temp = nsCRT::strdup(setlocale(posix_locale_category[i],""));
            category.AssignWithConversion(LocaleList[i]);
            category_platform.AssignWithConversion(LocaleList[i]);
            category_platform.Append(NS_LITERAL_STRING("##PLATFORM"));
            if (lc_temp != nsnull) {
                result = posixConverter->GetXPLocale(lc_temp,&xpLocale);
                platformLocale.AssignWithConversion(lc_temp);
            } else {
                char* lang = getenv("LANG");
                if ( lang == nsnull ) {
                    nsCAutoString langcstr("en-US");
                    platformLocale.AssignWithConversion("en_US");
	            lang = langcstr.ToNewCString();
                    result = posixConverter->GetXPLocale(lang,&xpLocale);
                    nsCRT::free(lang); 
	        }
                else {
                    result = posixConverter->GetXPLocale(lang,&xpLocale); 
                    platformLocale.AssignWithConversion(lang);
                }
            }
            if (NS_FAILED(result)) {
                nsCRT::free(lc_temp);
                return;
            }
            resultLocale->AddCategory(category.get(),xpLocale.get());
            resultLocale->AddCategory(category_platform.get(),platformLocale.get());
            nsCRT::free(lc_temp);
        }
        (void)resultLocale->QueryInterface(NS_GET_IID(nsILocale),(void**)&mSystemLocale);
        (void)resultLocale->QueryInterface(NS_GET_IID(nsILocale),(void**)&mApplicationLocale);
    }  // if ( NS_SUCCEEDED )...
       
#endif // XP_UNIX || XP_BEOS
#if defined(XP_OS2)
    nsresult result;
    nsCOMPtr<nsIOS2Locale> os2Converter
        = do_CreateInstance(NS_OS2LOCALE_CONTRACTID, &result);
    nsAutoString xpLocale;
    if (NS_SUCCEEDED(result) && os2Converter) {
        nsAutoString category;
        nsLocale* resultLocale;
        int i;

        resultLocale = new nsLocale();
        if ( resultLocale == NULL ) { 
            return; 
        }

        LocaleObject locale_object = NULL;
        UniCreateLocaleObject(UNI_UCS_STRING_POINTER,
                              (UniChar *)L"", &locale_object);

        char* lc_temp;
        for( i = 0; i < LocaleListLength; i++ ) {
            lc_temp = nsnull;
            UniQueryLocaleObject(locale_object,
                                 posix_locale_category[i],
                                 UNI_MBS_STRING_POINTER,
                                 (void **)&lc_temp);
            category.AssignWithConversion(LocaleList[i]);
            if (lc_temp != nsnull)
                result = os2Converter->GetXPLocale(lc_temp,&xpLocale);
            else {
                char* lang = getenv("LANG");
                if ( lang == nsnull ) {
                    nsCAutoString langcstr("en-US");
	            lang = langcstr.ToNewCString();
                    result = os2Converter->GetXPLocale(lang,&xpLocale);
                    nsCRT::free(lang); 
	        }
                else
                    result = os2Converter->GetXPLocale(lang,&xpLocale); 
            }
            if (NS_FAILED(result)) {
                nsCRT::free(lc_temp);
                return;
            }
            resultLocale->AddCategory(category.get(),xpLocale.get());
            UniFreeMem(lc_temp);
        }
        UniFreeLocaleObject(locale_object);
        (void)resultLocale->QueryInterface(NS_GET_IID(nsILocale),(void**)&mSystemLocale);
        (void)resultLocale->QueryInterface(NS_GET_IID(nsILocale),(void**)&mApplicationLocale);
    }  // if ( NS_SUCCEEDED )...

#endif

#ifdef XP_MAC
	long script = GetScriptManagerVariable(smSysScript);
	long lang = GetScriptVariable(smSystemScript,smScriptLang);
	long region = GetScriptManagerVariable(smRegionCode);
    nsresult result;
	nsCOMPtr<nsIMacLocale> macConverter
        = do_CreateInstance(NS_MACLOCALE_CONTRACTID, &result);
	if (NS_SUCCEEDED(result) && macConverter) {
		nsString xpLocale;
		result = macConverter->GetXPLocale((short)script,(short)lang,(short)region,&xpLocale);
		if (NS_SUCCEEDED(result)) {
			result = NewLocale(xpLocale.get(),&mSystemLocale);
			if (NS_SUCCEEDED(result)) {
				mApplicationLocale = mSystemLocale;
				mApplicationLocale->AddRef();
			}
		}
	}
#endif
}

nsLocaleService::~nsLocaleService(void)
{
	if (mSystemLocale) mSystemLocale->Release();
	if (mApplicationLocale) mApplicationLocale->Release();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLocaleService, nsILocaleService);

NS_IMETHODIMP
nsLocaleService::NewLocale(const PRUnichar *aLocale, nsILocale **_retval)
{
	int		i;
	nsresult result;

	*_retval = (nsILocale*)nsnull;
	
	nsLocale* resultLocale = new nsLocale();
	if (!resultLocale) return NS_ERROR_OUT_OF_MEMORY;

	for(i=0;i<LocaleListLength;i++) {
		nsString category; category.AssignWithConversion(LocaleList[i]);
		result = resultLocale->AddCategory(category.get(),aLocale);
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
		mSystemLocale->AddRef();
		*_retval = mSystemLocale;
		return NS_OK;
	}

	*_retval = (nsILocale*)nsnull;
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocaleService::GetApplicationLocale(nsILocale **_retval)
{
	if (mApplicationLocale) {
		mApplicationLocale->AddRef();
		*_retval = mApplicationLocale;
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
        sscanf(cPtr1,";q=%f",&qvalue[countLang]);
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
      strcpy(acceptLanguageList[i],ptrLanguage[i]);
    }

  } else {
    /* simple case: no quality values */

    cPtr = nsCRT::strtok(input,",",&cPtr2);
    while (cPtr) {
      if (strlen(cPtr)<NSILOCALE_MAX_ACCEPT_LENGTH) {        /* ignore if too long */
        strcpy(acceptLanguageList[countLang++],cPtr);
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
	  result = NewLocale(NS_ConvertASCIItoUCS2(acceptLanguageList[0]).get(),_retval);
  }

  //
  // clean up
  //
  delete[] input;
  return result;
}


nsresult
nsLocaleService::GetLocaleComponentForUserAgent(PRUnichar **_retval)
{
	nsCOMPtr<nsILocale>		system_locale;
	nsresult				result;

	result = GetSystemLocale(getter_AddRefs(system_locale));
	if (NS_SUCCEEDED(result))
	{
		nsString	lc_messages; lc_messages.AssignWithConversion(NSILOCALE_MESSAGE);
		result = system_locale->GetCategory(lc_messages.get(),_retval);
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
	NS_INIT_REFCNT();

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
nsLocaleDefinition::SetLocaleCategory(const PRUnichar *category, const PRUnichar *value)
{
	if (mLocaleDefinition)
		return mLocaleDefinition->AddCategory(category,value);
	
	return NS_ERROR_FAILURE;
}
