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

 
#include "nsILocaleFactory.h"
#include "nsLocaleFactory.h"
#include "nsILocale.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"
#ifdef XP_PC
#include <windows.h>
#endif
#ifdef XP_UNIX
#include <locale.h>
#include <stdlib.h>
#endif


NS_DEFINE_IID(kILocaleFactoryIID, NS_ILOCALEFACTORY_IID);
NS_DEFINE_IID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);

#ifdef XP_PC
NS_DEFINE_CID(kWin32LocaleFactoryCID, NS_WIN32LOCALEFACTORY_CID);
NS_DEFINE_IID(kIWin32LocaleIID, NS_IWIN32LOCALE_IID);
#endif

#ifdef XP_UNIX
NS_DEFINE_CID(kPosixLocaleFactoryCID,NS_POSIXLOCALEFACTORY_CID);
NS_DEFINE_IID(kIPosixLocaleIID, NS_IPOSIXLOCALE_IID);
#endif

NS_IMPL_ISUPPORTS(nsLocaleFactory,kILocaleFactoryIID)

#define LOCALE_CATAGORY_LISTLEN 6
char* localeCatagoryList[LOCALE_CATAGORY_LISTLEN] = 
{
								"NSILOCALE_COLLATE",
								"NSILOCALE_CTYPE",
								"NSILOCALE_MONETARY",
								"NSILOCALE_NUMERIC",
                "NSILOCALE_TIME",
                "NSILOCALE_MESSAGES"
};

#ifdef XP_UNIX

//
// of systems w/o LC_MESSAGE, we grab LC_CTYPE for the value of LC_MESSAGE
// i want NSILOCALE_MESSAGES to always exist in the XP version, and if it doesn't
// exist on a platform LC_CTYPE is probably the cloest approximation
//

int posix_locale_catagory[LOCALE_CATAGORY_LISTLEN] =
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
  int	i;
  nsresult result;
  NS_INIT_REFCNT();

  fCatagoryList = new nsString*[LOCALE_CATAGORY_LISTLEN];

  for(i=0;i< LOCALE_CATAGORY_LISTLEN;i++)
	fCatagoryList[i] = new nsString(localeCatagoryList[i]);

#ifdef XP_PC
   fWin32LocaleInterface = nsnull;
   result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&fWin32LocaleInterface);
	NS_ASSERTION(fWin32LocaleInterface!=NULL,"nsLocaleFactory: factory_create_interface failed.\n");
#elif defined(XP_UNIX)
  fPosixLocaleInterface = nsnull;
  result = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID,
                                              NULL,
                                              kIPosixLocaleIID,
                                              (void**)&fPosixLocaleInterface);
	NS_ASSERTION(fPosixLocaleInterface!=NULL,"nsLocaleFactory: factory_create_interface failed.\n");
#endif

}

nsLocaleFactory::~nsLocaleFactory(void)
{
	int i;

	for(i=0;i< LOCALE_CATAGORY_LISTLEN;i++)
		delete fCatagoryList[i];

	delete []fCatagoryList;

	if (fSystemLocale)
		fSystemLocale->Release();
	if (fApplicationLocale)
		fApplicationLocale->Release();

#ifdef XP_PC
	if (fWin32LocaleInterface)
		fWin32LocaleInterface->Release();
#endif
#ifdef XP_UNIX
  if (fPosixLocaleInterface)
    fPosixLocaleInterface->Release();
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
nsLocaleFactory::NewLocale(nsString** catagoryList,nsString**  
      valueList, PRUint8 count, nsILocale** locale)
{
	nsLocale*	newLocale;

	newLocale = new nsLocale(catagoryList,valueList,count);
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

	valueList = new nsString*[LOCALE_CATAGORY_LISTLEN];
	for(i=0;i< LOCALE_CATAGORY_LISTLEN;i++)
		valueList[i] = new nsString(*localeName);

	newLocale = new nsLocale(fCatagoryList,valueList,LOCALE_CATAGORY_LISTLEN);
	NS_ASSERTION(newLocale!=NULL,"nsLocaleFactory: failed to create nsLocale");
	newLocale->AddRef();

	//
	// delete temporary strings
	//
	for(i=0;i<LOCALE_CATAGORY_LISTLEN;i++)
		delete valueList[i];
	delete [] valueList;

	*locale = (nsILocale*)newLocale;

	return NS_OK;
}

NS_IMETHODIMP
nsLocaleFactory::GetSystemLocale(nsILocale** systemLocale)
{
	nsString*	systemLocaleName;
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
#ifdef XP_PC
	LCID				sysLCID;
	
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
	if (result!=NS_OK) {
		delete systemLocaleName;
		*systemLocale = (nsILocale*)nsnull;
		return result;
	}
	result = this->NewLocale(systemLocaleName,&fSystemLocale);
	if (result!=NS_OK) {
		delete systemLocaleName;
		*systemLocale=(nsILocale*)nsnull;
		fSystemLocale=(nsILocale*)nsnull;
		return result;
	}

	*systemLocale = fSystemLocale;
	fSystemLocale->AddRef();
	delete systemLocaleName;
	return result;
	
#elif defined(XP_UNIX)
 
  char* lc_all = setlocale(LC_ALL,NULL);
  char* lc_temp;
  char* lang = getenv("LANG");
  nsString* lc_values[LOCALE_CATAGORY_LISTLEN];
  int i;

  NS_ASSERTION(fPosixLocaleInterface!=nsnull,"nsLocale::GetSystemLocale failed");

  if (lc_all!=nsnull) {

    //
    // create an XP_Locale using the value in lc_all
    //
    lc_values[0] = new nsString();
    fPosixLocaleInterface->GetXPLocale(lc_all,lc_values[0]);
    char* tempvalue = lc_values[0]->ToNewCString();
    result = NewLocale(lc_values[0],&fSystemLocale);
    if (result!=NS_OK) {
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

  for(i=0;i<LOCALE_CATAGORY_LISTLEN;i++) {
    lc_temp = setlocale(posix_locale_catagory[i],"");
    if (lc_temp==NULL) lc_temp = lang;
    lc_values[i] = new nsString();
    fPosixLocaleInterface->GetXPLocale(lc_temp,lc_values[i]);
  }

  result = NewLocale(fCatagoryList,(nsString**)lc_values,
                     LOCALE_CATAGORY_LISTLEN,&fSystemLocale);
  if (result!=NS_OK) {
    for(i=0;i<LOCALE_CATAGORY_LISTLEN;i++) {
      delete lc_values[i];
    }
    *systemLocale = (nsILocale*)nsnull;
    fSystemLocale = (nsILocale*)nsnull;
    return result;
  }

  for(i=0;i<LOCALE_CATAGORY_LISTLEN;i++) {
    delete lc_values[i];
  }
  *systemLocale = fSystemLocale;
  fSystemLocale->AddRef();
  return NS_OK;

#else
	systemLocaleName = new nsString("en-US");
	result = this->NewLocale(systemLocaleName,&fSystemLocale);
	if (result!=NS_OK) {
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
	nsString*	applicationLocaleName;
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
#ifdef XP_PC
	LCID				appLCID;
	nsIWin32Locale*		iWin32Locale;
	
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
	if (result!=NS_OK) {
		delete applicationLocaleName;
		*applicationLocale = (nsILocale*)nsnull;
		return result;
	}
	result = this->NewLocale(applicationLocaleName,&fApplicationLocale);
	if (result!=NS_OK) {
		delete applicationLocaleName;
		*applicationLocale=(nsILocale*)nsnull;
		fApplicationLocale=(nsILocale*)nsnull;
		return result;
	}

	*applicationLocale = fApplicationLocale;
	fApplicationLocale->AddRef();
	delete applicationLocaleName;
	return result;

#elif defined(XP_UNIX)

  result = GetSystemLocale(&fApplicationLocale);
  *applicationLocale = fApplicationLocale;
  fApplicationLocale->AddRef();
  return result;

#else
	applicationLocaleName = new nsString("en-US");
	result = this->NewLocale(applicationLocaleName,&fApplicationLocale);
	if (result!=NS_OK) {
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

