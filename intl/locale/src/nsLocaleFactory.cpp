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

 
#include "nsILocaleFactory.h"
#include "nsLocaleFactory.h"
#include "nsILocale.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"
#include <ctype.h>  // for isalpha, tolower & isspace

#if defined(XP_WIN)
#include <windows.h>
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)  || defined(XP_OS2)
#include <locale.h>
#include <stdlib.h>
#endif

#define NSILOCALE_MAX_ACCEPT_LANGUAGE	16
#define NSILOCALE_MAX_ACCEPT_LENGTH		18

NS_DEFINE_IID(kILocaleFactoryIID, NS_ILOCALEFACTORY_IID);
NS_DEFINE_IID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);

#if defined(XP_WIN)
NS_DEFINE_CID(kWin32LocaleFactoryCID, NS_WIN32LOCALEFACTORY_CID);
NS_DEFINE_IID(kIWin32LocaleIID, NS_IWIN32LOCALE_IID);
#endif

#ifdef XP_OS2
NS_DEFINE_CID(kOS2LocaleFactoryCID, NS_OS2LOCALEFACTORY_CID);
NS_DEFINE_IID(kIOS2LocaleIID, NS_IOS2LOCALE_IID);
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
NS_DEFINE_CID(kPosixLocaleFactoryCID,NS_POSIXLOCALEFACTORY_CID);
NS_DEFINE_IID(kIPosixLocaleIID, NS_IPOSIXLOCALE_IID);
#endif

NS_IMPL_ISUPPORTS2(nsLocaleFactory,nsIFactory, nsILocaleFactory)

#define LOCALE_CATEGORY_LISTLEN 6
char* localeCategoryList[LOCALE_CATEGORY_LISTLEN] = 
{
								"NSILOCALE_COLLATE",
								"NSILOCALE_CTYPE",
								"NSILOCALE_MONETARY",
								"NSILOCALE_NUMERIC",
                "NSILOCALE_TIME",
                "NSILOCALE_MESSAGES"
};

#if defined(XP_UNIX) || defined(XP_BEOS)

//
// of systems w/o LC_MESSAGE, we grab LC_CTYPE for the value of LC_MESSAGE
// i want NSILOCALE_MESSAGES to always exist in the XP version, and if it doesn't
// exist on a platform LC_CTYPE is probably the cloest approximation
//

int posix_locale_category[LOCALE_CATEGORY_LISTLEN] =
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

nsLocaleFactory::nsLocaleFactory(void)
:	fSystemLocale(NULL),
	fApplicationLocale(NULL)
{
  NS_ASSERTION(0, "obsoleted, use nsILocaleService instead.\n");

  int	i;
  nsresult result;
  NS_INIT_REFCNT();

  fCategoryList = new nsString*[LOCALE_CATEGORY_LISTLEN];

  for(i=0;i< LOCALE_CATEGORY_LISTLEN;i++) {
    nsString* temp = new nsString;
    temp->AssignWithConversion(localeCategoryList[i]);
	  fCategoryList[i] = temp;
	}

#if defined(XP_WIN)
   fWin32LocaleInterface = nsnull;
   result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&fWin32LocaleInterface);
	NS_ASSERTION(fWin32LocaleInterface!=NULL,"nsLocaleFactory: factory_create_interface failed.\n");
#elif defined(XP_UNIX) || defined(XP_BEOS)
  fPosixLocaleInterface = nsnull;
  result = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID,
                                              NULL,
                                              kIPosixLocaleIID,
                                              (void**)&fPosixLocaleInterface);
	NS_ASSERTION(fPosixLocaleInterface!=NULL,"nsLocaleFactory: factory_create_interface failed.\n");
#elif defined(XP_OS2)
  fOS2LocaleInterface = nsnull;
  result = nsComponentManager::CreateInstance(kOS2LocaleFactoryCID,
                                              NULL,
                                              kIOS2LocaleIID,
                                              (void**)&fOS2LocaleInterface);
	NS_ASSERTION(fOS2LocaleInterface!=NULL,"nsLocaleFactory: factory_create_interface failed.\n");
#endif

}

nsLocaleFactory::~nsLocaleFactory(void)
{
	int i;

	for(i=0;i< LOCALE_CATEGORY_LISTLEN;i++)
		delete fCategoryList[i];

	delete []fCategoryList;

	if (fSystemLocale)
		fSystemLocale->Release();
	if (fApplicationLocale)
		fApplicationLocale->Release();

#if defined(XP_WIN)
	if (fWin32LocaleInterface)
		fWin32LocaleInterface->Release();
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
  if (fPosixLocaleInterface)
    fPosixLocaleInterface->Release();
#endif
#ifdef XP_OS2
  if (fOS2LocaleInterface)
   fOS2LocaleInterface->Release();
#endif

}

NS_IMETHODIMP
nsLocaleFactory::CreateInstance(nsISupports* aOuter, REFNSIID aIID,
		void** aResult)
{
	nsresult	result;

	result = this->GetApplicationLocale((nsILocale**)aResult);

	return result;
}

NS_IMETHODIMP
nsLocaleFactory::LockFactory(PRBool	aBool)
{
	return NS_OK;
}

NS_IMETHODIMP
nsLocaleFactory::NewLocale(nsString** categoryList,nsString**  
      valueList, PRUint8 count, nsILocale** locale)
{
	nsLocale*	newLocale;

	newLocale = new nsLocale(categoryList,valueList,count);
	NS_ASSERTION(newLocale!=NULL,"nsLocaleFactory: failed to create nsLocale instance");
	newLocale->AddRef();

	*locale = (nsILocale*)newLocale;

	return NS_OK;
}

NS_IMETHODIMP
nsLocaleFactory::NewLocale(const nsString* localeName, nsILocale** locale)
{
	int	i;
	nsString**	valueList;
	nsLocale*	newLocale;

	valueList = new nsString*[LOCALE_CATEGORY_LISTLEN];
	for(i=0;i< LOCALE_CATEGORY_LISTLEN;i++)
		valueList[i] = new nsString(*localeName);

	newLocale = new nsLocale(fCategoryList,valueList,LOCALE_CATEGORY_LISTLEN);
	NS_ASSERTION(newLocale!=NULL,"nsLocaleFactory: failed to create nsLocale");
	newLocale->AddRef();

	//
	// delete temporary strings
	//
	for(i=0;i<LOCALE_CATEGORY_LISTLEN;i++)
		delete valueList[i];
	delete [] valueList;

	*locale = (nsILocale*)newLocale;

	return NS_OK;
}

NS_IMETHODIMP
nsLocaleFactory::GetSystemLocale(nsILocale** systemLocale)
{
	nsresult	result;

	if (fSystemLocale!=NULL)
	{
		fSystemLocale->AddRef();
		*systemLocale = fSystemLocale;
		return NS_OK;
	}
	
	//
	// for Windows
	//
#if defined(XP_WIN)
	LCID				sysLCID;
  nsString*   systemLocaleName;
	
	sysLCID = GetSystemDefaultLCID();
	if (sysLCID==0) {
		*systemLocale = (nsILocale*)nsnull;
		return NS_ERROR_FAILURE;
	}
	
	if (fWin32LocaleInterface==NULL) {
		*systemLocale = (nsILocale*)nsnull;
		return NS_ERROR_FAILURE;
	}

	systemLocaleName = new nsString();
	result = fWin32LocaleInterface->GetXPLocale(sysLCID,systemLocaleName);
	if (NS_FAILED(result)) {
		delete systemLocaleName;
		*systemLocale = (nsILocale*)nsnull;
		return result;
	}
	result = this->NewLocale(systemLocaleName,&fSystemLocale);
	if (NS_FAILED(result)) {
		delete systemLocaleName;
		*systemLocale=(nsILocale*)nsnull;
		fSystemLocale=(nsILocale*)nsnull;
		return result;
	}

	*systemLocale = fSystemLocale;
	fSystemLocale->AddRef();
	delete systemLocaleName;
	return result;
	
#elif defined(XP_UNIX) || defined(XP_BEOS)
 
  char* lc_all = setlocale(LC_ALL,"");
  char* lc_temp;
  char* tempvalue;
  char* lang = getenv("LANG");
  nsString* lc_values[LOCALE_CATEGORY_LISTLEN];
  int i;

  NS_ASSERTION(fPosixLocaleInterface!=nsnull,"nsLocale::GetSystemLocale failed");

  if (lc_all!=nsnull) {

    //
    // create an XP_Locale using the value in lc_all
    //
    lc_values[0] = new nsString();
    fPosixLocaleInterface->GetXPLocale(lc_all,lc_values[0]);
    tempvalue = lc_values[0]->ToNewCString();
    result = NewLocale(lc_values[0],&fSystemLocale);
    if (NS_FAILED(result)) {
      delete lc_values[0];
      *systemLocale=(nsILocale*)nsnull;
      fSystemLocale=(nsILocale*)nsnull;
      return result;
    }
    
    fSystemLocale->AddRef();
    *systemLocale = fSystemLocale;
    return result;
  }

  //
  // check to see if lang is null, if it is we can default to en
  //  and use the same logic
  if (lang==NULL) {
    lang = "en";
  }

  for(i=0;i<LOCALE_CATEGORY_LISTLEN;i++) {
    lc_temp = setlocale(posix_locale_category[i],"");
    if (lc_temp==NULL) lc_temp = lang;
    lc_values[i] = new nsString();
    fPosixLocaleInterface->GetXPLocale(lc_temp,lc_values[i]);
  }

  result = NewLocale(fCategoryList,(nsString**)lc_values,
                     LOCALE_CATEGORY_LISTLEN,&fSystemLocale);
  if (NS_FAILED(result)) {
    for(i=0;i<LOCALE_CATEGORY_LISTLEN;i++) {
      delete lc_values[i];
    }
    *systemLocale = (nsILocale*)nsnull;
    fSystemLocale = (nsILocale*)nsnull;
    return result;
  }

  for(i=0;i<LOCALE_CATEGORY_LISTLEN;i++) {
    delete lc_values[i];
  }
  *systemLocale = fSystemLocale;
  fSystemLocale->AddRef();
  return NS_OK;

#else
  nsString* systemLocaleName;

	systemLocaleName = new nsString; systemLocaleName->AssignWithConversion("en-US");
	result = this->NewLocale(systemLocaleName,&fSystemLocale);
	if (NS_FAILED(result)) {
		delete systemLocaleName;
		*systemLocale=(nsILocale*)nsnull;
		fSystemLocale=(nsILocale*)nsnull;
		return result;
	}

	*systemLocale = fSystemLocale;
	fSystemLocale->AddRef();
	delete systemLocaleName;
	return result;

#endif

}

NS_IMETHODIMP
nsLocaleFactory::GetApplicationLocale(nsILocale** applicationLocale)
{

	nsresult	result;

	if (fApplicationLocale!=NULL)
	{
		fApplicationLocale->AddRef();
		*applicationLocale = fApplicationLocale;
		return NS_OK;
	}
	
	//
	// for now
	//
	//
	// for Windows
	//
#if defined(XP_WIN)
	LCID				appLCID;
  nsString*   applicationLocaleName;
	
	appLCID = GetUserDefaultLCID();
	if (appLCID==0) {
		*applicationLocale = (nsILocale*)nsnull;
		return NS_ERROR_FAILURE;
	}
	
	if (fWin32LocaleInterface==NULL) {
		*applicationLocale = (nsILocale*)nsnull;
		return NS_ERROR_FAILURE;
	}

	applicationLocaleName = new nsString();
	result = fWin32LocaleInterface->GetXPLocale(appLCID,applicationLocaleName);
	if (NS_FAILED(result)) {
		delete applicationLocaleName;
		*applicationLocale = (nsILocale*)nsnull;
		return result;
	}
	result = this->NewLocale(applicationLocaleName,&fApplicationLocale);
	if (NS_FAILED(result)) {
		delete applicationLocaleName;
		*applicationLocale=(nsILocale*)nsnull;
		fApplicationLocale=(nsILocale*)nsnull;
		return result;
	}

	*applicationLocale = fApplicationLocale;
	fApplicationLocale->AddRef();
	delete applicationLocaleName;
	return result;

#elif defined(XP_UNIX) || defined(XP_BEOS)

  result = GetSystemLocale(&fApplicationLocale);
  *applicationLocale = fApplicationLocale;
  fApplicationLocale->AddRef();
  return result;

#else
  nsString* applicationLocaleName;

	applicationLocaleName = new nsString; applicationLocaleName->AssignWithConversion("en-US");
	result = this->NewLocale(applicationLocaleName,&fApplicationLocale);
	if (NS_FAILED(result)) {
		delete applicationLocaleName;
		*applicationLocale=(nsILocale*)nsnull;
		fApplicationLocale=(nsILocale*)nsnull;
		return result;
	}

	*applicationLocale = fApplicationLocale;
	fApplicationLocale->AddRef();
	delete applicationLocaleName;
	return result;

#endif

}

NS_IMETHODIMP
nsLocaleFactory::GetLocaleFromAcceptLanguage(const char* acceptLanguage, nsILocale** acceptLocale)
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
  nsString*	localeName;

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
	  localeName = new nsString; localeName->AssignWithConversion(acceptLanguageList[0]);
	  result = NewLocale(localeName,acceptLocale);
	  delete localeName;
  }

  //
  // clean up
  //
  delete[] input;
  return result;

}

#if defined(XP_WIN)
// XXX I had to add this because I changed the link order on Windows
void notCalled()
{
  nsCID cid;
  (void)NSGetFactory(nsnull, cid, nsnull, nsnull, nsnull);
}
#endif
