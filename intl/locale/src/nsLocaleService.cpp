/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsCOMPtr.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsLocaleFactory.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"

#include <ctype.h>

#ifdef XP_PC
#include "nsIWin32Locale.h"
#endif
#if defined(XP_UNIX) || defined(BEOS)
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
#ifdef XP_PC
static NS_DEFINE_IID(kIWin32LocaleIID,NS_IWIN32LOCALE_IID);
#endif
#ifdef XP_UNIX
static NS_DEFINE_IID(kIPosixLocaleIID,NS_IPOSIXLOCALE_IID);
#endif
#ifdef XP_MAC
static NS_DEFINE_IID(kIMacLocaleIID,NS_IMACLOCALE_IID);
#endif

//
// cids
//
#ifdef XP_PC
static NS_DEFINE_CID(kWin32LocaleFactoryCID,NS_WIN32LOCALEFACTORY_CID);
#endif
#ifdef XP_UNIX
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

#ifdef XP_UNIX
static int posix_locale_category[LocaleListLength] =
{
  LC_TIME,
  LC_COLLATE,
  LC_CTYPE,
  LC_MONETARY,
  LC_NUMERIC,
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
	NS_IMETHOD NewLocale(const PRUnichar *aLocale, nsILocale **_retval);
	NS_IMETHOD NewLocaleObject(nsILocaleDefinition *localeDefinition, nsILocale **_retval);
	NS_IMETHOD GetSystemLocale(nsILocale **_retval);
	NS_IMETHOD GetApplicationLocale(nsILocale **_retval);
	NS_IMETHOD GetLocaleFromAcceptLanguage(const char *acceptLanguage, nsILocale **_retval);

	//
	// implementation methods
	//
	static nsresult GetLocaleService(nsILocaleService** localeService);

protected:

	nsLocaleService(void);
	virtual ~nsLocaleService(void);

	nsresult SetSystemLocale(void);
	nsresult SetApplicationLocale(void);

	nsILocale*					mSystemLocale;
	nsILocale*					mApplicationLocale;
	static nsILocaleService*	gLocaleService;

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


nsILocaleService* nsLocaleService::gLocaleService = NS_STATIC_CAST(nsILocaleService*,nsnull);


//
// nsLocaleService methods
//
nsLocaleService::nsLocaleService(void)
:	mSystemLocale(nsnull), mApplicationLocale(nsnull)
{
	NS_INIT_REFCNT();
#ifdef XP_PC
	nsIWin32Locale*	win32Converter;
	nsString		xpLocale;
	nsresult result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
						NULL,kIWin32LocaleIID,(void**)&win32Converter);
	NS_ASSERTION(win32Converter!=NULL,"nsLocaleService: can't get win32 converter\n");
	if (result==NS_OK && win32Converter!=nsnull) {
		
		//
		// get the system LCID
		//
		LCID win_lcid = GetSystemDefaultLCID();
		if (win_lcid==0) { win32Converter->Release(); return;}
		result = win32Converter->GetXPLocale(win_lcid,&xpLocale);
		if (result!=NS_OK) { win32Converter->Release(); return;}
		result = NewLocale(xpLocale.ToNewUnicode(),&mSystemLocale);
		if (result!=NS_OK) { win32Converter->Release(); return;}

		//
		// get the application LCID
		//
		win_lcid = GetSystemDefaultLCID();
		if (win_lcid==0) { win32Converter->Release(); return;}
		result = win32Converter->GetXPLocale(win_lcid,&xpLocale);
		if (result!=NS_OK) { win32Converter->Release(); return;}
		result = NewLocale(xpLocale.ToNewUnicode(),&mApplicationLocale);
		if (result!=NS_OK) { win32Converter->Release(); return;}
	
		win32Converter->Release();
	}
#endif
#ifdef XP_UNIX
    nsIPosixLocale* posixConverter;
    nsString xpLocale;
    nsresult result = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID,
                           NULL,kIPosixLocaleIID,(void**)&posixConverter);
    if (result==NS_OK && posixConverter!=nsnull) {
        char* lc_all = setlocale(LC_ALL,NULL);
        char* lang = getenv("LANG");

        if (lc_all!=nsnull) {
            result = posixConverter->GetXPLocale(lc_all,&xpLocale);
            if (result!=NS_OK) { posixConverter->Release(); return; }
            result = NewLocale(xpLocale.ToNewUnicode(),&mSystemLocale);
            if (result!=NS_OK) { posixConverter->Release(); return; }
            mApplicationLocale=mSystemLocale;
            mApplicationLocale->AddRef();
            posixConverter->Release();
        } else {
            if (lang==nsnull) {
                xpLocale = "en-US";
                result = NewLocale(xpLocale.ToNewUnicode(),&mSystemLocale);
                if (result!=NS_OK) { posixConverter->Release(); return; }
                mApplicationLocale = mSystemLocale;
                mApplicationLocale->AddRef();
                posixConverter->Release();
            } else {
                int i;
                nsString category;
                nsLocale* resultLocale = new nsLocale();
                for(i=0;i<LocaleListLength;i++) {
                    char* lc_temp = setlocale(posix_locale_category[i],"");
                    category = LocaleList[i];
                    if (lc_temp==nsnull) xpLocale = "en-US";
                    else xpLocale = lc_temp;
                    resultLocale->AddCategory(category.ToNewUnicode(),xpLocale.ToNewUnicode());
                }
                (void)resultLocale->QueryInterface(kILocaleIID,(void**)&mSystemLocale);
                (void)resultLocale->QueryInterface(kILocaleIID,(void**)&mApplicationLocale);
                posixConverter->Release();
            }
        }
    }
#endif // XP_PC
#ifdef XP_MAC
	long script = GetScriptManagerVariable(smSysScript);
	long region = GetScriptManagerVariable(smRegionCode);
	nsIMacLocale*	macConverter;
	nsresult result = nsComponentManager::CreateInstance(kMacLocaleFactoryCID,
						NULL,kIMacLocaleIID,(void**)&macConverter);
	if (result==NS_OK && macConverter!=nsnull) {
		nsString xpLocale;
		result = macConverter->GetXPLocale((short)script,(short)region,&xpLocale);
		if (result!=NS_OK) { macConverter->Release(); return; }
		result = NewLocale(xpLocale.ToNewUnicode(),&mSystemLocale);
		if (result!=NS_OK) { macConverter->Release(); return; }
		mApplicationLocale = mSystemLocale;
		mApplicationLocale->AddRef();
		macConverter->Release();
	}
#endif	
                              
}

nsLocaleService::~nsLocaleService(void)
{
	gLocaleService = NS_STATIC_CAST(nsILocaleService*,nsnull);
	if (mSystemLocale) mSystemLocale->Release();
	if (mApplicationLocale) mApplicationLocale->Release();
}

NS_IMETHODIMP_(nsrefcnt)
nsLocaleService::AddRef(void)
{
    return 2;
}

NS_IMETHODIMP_(nsrefcnt)
nsLocaleService::Release(void)
{
    return 1;
}

NS_IMPL_QUERY_INTERFACE(nsLocaleService, kILocaleServiceIID);

NS_IMETHODIMP
nsLocaleService::NewLocale(const PRUnichar *aLocale, nsILocale **_retval)
{
	int		i;
	nsresult result;

	*_retval = (nsILocale*)nsnull;
	
	nsLocale* resultLocale = new nsLocale();
	if (!resultLocale) return NS_ERROR_OUT_OF_MEMORY;

	for(i=0;i<LocaleListLength;i++) {
		nsString category = LocaleList[i];
		result = resultLocale->AddCategory(category.ToNewUnicode(),aLocale);
		if (result!=NS_OK) { delete resultLocale; return result;}
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
    else if (isspace(*cPtr1));                             /* ignore any space */
    else if (*cPtr1=='-')      *cPtr2++ = '_';             /* "-" -> "_"       */
    else if (*cPtr1=='*');                                 /* ignore "*"       */
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
      if ((cPtr1 = strchr(cPtr,';'))) {
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
	  PRUnichar* localeName = nsString(acceptLanguageList[0]).ToNewUnicode();
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
nsLocaleService::GetLocaleService(nsILocaleService** localeService)
{
	if (!gLocaleService)
	{
		nsLocaleService* locale_service = new nsLocaleService();
		if (!locale_service)
		{
			*localeService = NS_STATIC_CAST(nsILocaleService*,locale_service);
			return NS_ERROR_OUT_OF_MEMORY;
		} 

		gLocaleService = NS_STATIC_CAST(nsILocaleService*,locale_service);

	}

	*localeService = gLocaleService;
	return NS_OK;
}

nsresult
NS_NewLocaleService(nsILocaleService** result)
{
	return nsLocaleService::GetLocaleService(result);
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
