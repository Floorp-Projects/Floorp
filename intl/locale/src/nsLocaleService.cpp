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
#include "nsLocaleFactory.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"

#include <ctype.h>

#if defined(XP_PC) && !defined(XP_OS2)
#include "nsIWin32Locale.h"
#endif
#ifdef XP_OS2
#include "unidef.h"
#include "nsIOS2Locale.h"
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
#include <locale.h>
#include <stdlib.h>
#include "nsIPosixLocale.h"
#endif
#ifdef XP_MAC
#include <script.h>
#include "nsIMacLocale.h"
#endif

//
// iids
//
static NS_DEFINE_IID(kILocaleDefinitionIID,NS_ILOCALEDEFINITION_IID);
static NS_DEFINE_IID(kILocaleServiceIID,NS_ILOCALESERVICE_IID);
static NS_DEFINE_IID(kILocaleIID,NS_ILOCALE_IID);
static NS_DEFINE_IID(kIFactoryIID,NS_IFACTORY_IID);
#if defined(XP_PC) && !defined(XP_OS2)
static NS_DEFINE_IID(kIWin32LocaleIID,NS_IWIN32LOCALE_IID);
#endif
#ifdef XP_OS2
static NS_DEFINE_IID(kIOS2LocaleIID,NS_IOS2LOCALE_IID);
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
static NS_DEFINE_IID(kIPosixLocaleIID,NS_IPOSIXLOCALE_IID);
#endif
#ifdef XP_MAC
static NS_DEFINE_IID(kIMacLocaleIID,NS_IMACLOCALE_IID);
#endif

//
// cids
//
#if defined(XP_PC) && !defined(XP_OS2)
static NS_DEFINE_CID(kWin32LocaleFactoryCID,NS_WIN32LOCALEFACTORY_CID);
#endif
#ifdef XP_OS2
static NS_DEFINE_CID(kOS2LocaleFactoryCID,NS_OS2LOCALEFACTORY_CID);
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
static NS_DEFINE_CID(kPosixLocaleFactoryCID,NS_POSIXLOCALEFACTORY_CID);
#endif
#ifdef XP_MAC
static NS_DEFINE_CID(kMacLocaleFactoryCID,NS_MACLOCALEFACTORY_CID);
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
#if defined(XP_PC) && !defined(XP_OS2)
	nsIWin32Locale*	win32Converter;
	nsString		xpLocale;
	nsresult result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
						NULL,kIWin32LocaleIID,(void**)&win32Converter);
	NS_ASSERTION(win32Converter!=NULL,"nsLocaleService: can't get win32 converter\n");
	if (NS_SUCCEEDED(result) && win32Converter!=nsnull) {
		
		//
		// get the system LCID
		//
		LCID win_lcid = GetSystemDefaultLCID();
		if (win_lcid==0) { win32Converter->Release(); return;}
		result = win32Converter->GetXPLocale(win_lcid,&xpLocale);
		if (NS_FAILED(result)) { win32Converter->Release(); return;}
		result = NewLocale(xpLocale.GetUnicode(), &mSystemLocale);
		if (NS_FAILED(result)) { win32Converter->Release(); return;}

		//
		// get the application LCID
		//
		win_lcid = GetUserDefaultLCID();
		if (win_lcid==0) { win32Converter->Release(); return;}
		result = win32Converter->GetXPLocale(win_lcid,&xpLocale);
		if (NS_FAILED(result)) { win32Converter->Release(); return;}
		result = NewLocale(xpLocale.GetUnicode(), &mApplicationLocale);
		if (NS_FAILED(result)) { win32Converter->Release(); return;}
	
		win32Converter->Release();
	}
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
    nsIPosixLocale* posixConverter;
    nsAutoString xpLocale;
    nsresult result = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID,
                           NULL,kIPosixLocaleIID,(void**)&posixConverter);
    if (NS_SUCCEEDED(result) && posixConverter!=nsnull) {
        nsAutoString category;
        nsLocale* resultLocale;
        int i;

        resultLocale = new nsLocale();
        if ( resultLocale == NULL ) { 
            posixConverter->Release(); 
            return; 
        }
        for( i = 0; i < LocaleListLength; i++ ) {
            char* lc_temp = nsCRT::strdup(setlocale(posix_locale_category[i],""));
            category.AssignWithConversion(LocaleList[i]);
            if (lc_temp != nsnull)
                result = posixConverter->GetXPLocale(lc_temp,&xpLocale);
            else {
                char* lang = getenv("LANG");
                if ( lang == nsnull ) {
                    nsCAutoString langcstr("en-US");
	            lang = nsCRT::strdup( langcstr.GetBuffer() );
                    result = posixConverter->GetXPLocale(lang,&xpLocale);
                    nsCRT::free(lang); 
	        }
                else
                    result = posixConverter->GetXPLocale(lang,&xpLocale); 
            }
            if (NS_FAILED(result)) {
                posixConverter->Release();
                nsCRT::free(lc_temp);
                return;
            }
            resultLocale->AddCategory(category.GetUnicode(),xpLocale.GetUnicode());
            nsCRT::free(lc_temp);
        }
        (void)resultLocale->QueryInterface(kILocaleIID,(void**)&mSystemLocale);
        (void)resultLocale->QueryInterface(kILocaleIID,(void**)&mApplicationLocale);
        posixConverter->Release();
    }  // if ( NS_SUCCEEDED )...
       
#endif // XP_UNIX || XP_BEOS
#if defined(XP_OS2)
    nsCOMPtr<nsIOS2Locale> os2Converter;
    nsAutoString xpLocale;
    nsresult result = nsComponentManager::CreateInstance(kOS2LocaleFactoryCID,
                           NULL,kIOS2LocaleIID,(void**)getter_AddRefs(os2Converter));
    if (NS_SUCCEEDED(result) && os2Converter!=nsnull) {
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
	            lang = nsCRT::strdup( langcstr.GetBuffer() );
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
            resultLocale->AddCategory(category.GetUnicode(),xpLocale.GetUnicode());
            UniFreeMem(lc_temp);
        }
        UniFreeLocaleObject(locale_object);
        (void)resultLocale->QueryInterface(kILocaleIID,(void**)&mSystemLocale);
        (void)resultLocale->QueryInterface(kILocaleIID,(void**)&mApplicationLocale);
    }  // if ( NS_SUCCEEDED )...

#endif

#ifdef XP_MAC
	long script = GetScriptManagerVariable(smSysScript);
	long lang = GetScriptVariable(smSystemScript,smScriptLang);
	long region = GetScriptManagerVariable(smRegionCode);
	nsCOMPtr<nsIMacLocale> macConverter;
	nsresult result = nsComponentManager::CreateInstance(kMacLocaleFactoryCID,
						NULL,kIMacLocaleIID,(void**)getter_AddRefs(macConverter));
	if (NS_SUCCEEDED(result) && macConverter!=nsnull) {
		nsString xpLocale;
		result = macConverter->GetXPLocale((short)script,(short)lang,(short)region,&xpLocale);
		if (NS_SUCCEEDED(result)) {
			result = NewLocale(xpLocale.GetUnicode(),&mSystemLocale);
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

NS_IMPL_THREADSAFE_ISUPPORTS(nsLocaleService, kILocaleServiceIID);

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
		result = resultLocale->AddCategory(category.GetUnicode(),aLocale);
		if (NS_FAILED(result)) { delete resultLocale; return result;}
	}

	return resultLocale->QueryInterface(kILocaleIID,(void**)_retval);
}


NS_IMETHODIMP
nsLocaleService::NewLocaleObject(nsILocaleDefinition *localeDefinition, nsILocale **_retval)
{
	if (!localeDefinition || !_retval) return NS_ERROR_INVALID_ARG;

	nsLocale* new_locale = new nsLocale(NS_STATIC_CAST(nsLocaleDefinition*,localeDefinition)->mLocaleDefinition);
	if (!new_locale) return NS_ERROR_OUT_OF_MEMORY;

	return new_locale->QueryInterface(kILocaleIID,(void**)_retval);
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

    /* cPtr = STRTOK(input,"," CPTR2); */
    cPtr = strtok(input,",");
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
      /* cPtr = STRTOK(NULL,"," CPTR2); */
      cPtr = strtok(NULL,",");
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

    /* cPtr = STRTOK(input,"," CPTR2); */
    cPtr = strtok(input,",");
    while (cPtr) {
      if (strlen(cPtr)<NSILOCALE_MAX_ACCEPT_LENGTH) {        /* ignore if too long */
        strcpy(acceptLanguageList[countLang++],cPtr);
        if (countLang>=NSILOCALE_MAX_ACCEPT_LENGTH) break; /* quit if too many */
      }
      /* cPtr = STRTOK(NULL,"," CPTR2); */
      cPtr = strtok(NULL,",");
    }
  }

  //
  // now create the locale 
  //
  result = NS_ERROR_FAILURE;
  if (countLang>0) {
	  PRUnichar* localeName = NS_ConvertToString(acceptLanguageList[0]).ToNewUnicode();
	  result = NewLocale(localeName,_retval);
	  delete localeName;
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
		result = system_locale->GetCategory(lc_messages.GetUnicode(),_retval);
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
  return (nsnull == *result) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}


//
// nsLocaleDefinition methods
//

NS_IMPL_ISUPPORTS(nsLocaleDefinition,kILocaleDefinitionIID)

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


nsLocaleServiceFactory::nsLocaleServiceFactory()
{
  NS_INIT_REFCNT();
}

nsLocaleServiceFactory::~nsLocaleServiceFactory()
{
}

NS_IMPL_ISUPPORTS(nsLocaleServiceFactory,kIFactoryIID)

NS_IMETHODIMP
nsLocaleServiceFactory::CreateInstance(nsISupports* aOuter,
  REFNSIID aIID, void** aResult)
{
	nsILocaleService*	locale_service;	
	NS_NewLocaleService(&locale_service);

	return locale_service->QueryInterface(aIID,aResult);

}

NS_IMETHODIMP
nsLocaleServiceFactory::LockFactory(PRBool aLock)
{

  return NS_OK;
}
