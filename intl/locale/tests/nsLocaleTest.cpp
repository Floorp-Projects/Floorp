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
#include <stdlib.h>
#include "nsILocale.h"
#include "nsILocaleFactory.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"
#ifdef XP_PC
#include "nsIWin32Locale.h"
#include <windows.h>
#endif

NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
NS_DEFINE_IID(kILocaleFactoryIID, NS_ILOCALEFACTORY_IID);
NS_DEFINE_CID(kLocaleCID, NS_LOCALE_CID);
NS_DEFINE_IID(kILocaleIID, NS_ILOCALE_IID);
NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

#ifdef XP_PC
NS_DEFINE_CID(kWin32LocaleFactoryCID, NS_WIN32LOCALEFACTORY_CID);
NS_DEFINE_IID(kIWin32LocaleIID, NS_IWIN32LOCALE_IID);

#define USER_DEFINED_PRIMARYLANG	0x0200
#define USER_DEFINED_SUBLANGUAGE	0x20

#endif


char* localeCatagoryList[6] = { "NSILOCALE_TIME",
								"NSILOCALE_COLLATE",
								"NSILOCALE_CTYPE",
								"NSILOCALE_MONETARY",
								"NSILOCALE_MESSAGES",
								"NSILOCALE_NUMERIC"
};

void
factory_create_interface(void)
{
	nsresult			result;
	nsILocaleFactory*	localeFactory;
	nsIFactory*			genericFactory;

	result = nsComponentManager::CreateInstance(kLocaleFactoryCID,
									NULL,
									kILocaleFactoryIID,
									(void**)&localeFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");

	localeFactory->Release();

	result = nsComponentManager::CreateInstance(kLocaleFactoryCID,
									NULL,
									kIFactoryIID,
									(void**)&genericFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");

	genericFactory->Release();
}

void 
factory_test_isupports(void)
{
	nsresult			result;
	nsILocaleFactory*	localeFactory;
	nsISupports*		genericInterface1, *genericInterface2;
	nsIFactory*			genericFactory1, *genericFactory2;

	result = nsComponentManager::FindFactory(kLocaleFactoryCID,
										(nsIFactory**)&localeFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");

	//
	// test AddRef
	localeFactory->AddRef();

	//
	// test Release
	//
	localeFactory->Release();

	//
	// test generic interface
	//
	result = localeFactory->QueryInterface(kISupportsIID,(void**)&genericInterface1);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericInterface1!=NULL,"nsLocaleTest: factory_test_isupports failed.");

	result = localeFactory->QueryInterface(kISupportsIID,(void**)&genericInterface2);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericInterface2!=NULL,"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericInterface1==genericInterface2,"nsLocaleTest: factory_test_isupports failed.");

	genericInterface1->Release();
	genericInterface2->Release();

	//
	// test generic factory
	//
	result = localeFactory->QueryInterface(kIFactoryIID,(void**)&genericFactory1);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericFactory1!=NULL,"nsLocaleTest: factory_test_isupports failed.");

	result = localeFactory->QueryInterface(kIFactoryIID,(void**)&genericFactory2);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericFactory1!=NULL,"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericFactory1==genericFactory2,"nsLocaleTest: factory_test_isupports failed.");

	genericFactory1->Release();
	genericFactory2->Release();

	localeFactory->Release();
}

void
factory_new_locale(void)
{
	nsresult			result;
	nsILocaleFactory*	localeFactory;
	nsILocale*			locale;
	nsString*			localeName, *catagory, *value;
	int	i;
	nsString**			catagoryList, **valueList;

	result = nsComponentManager::FindFactory(kLocaleFactoryCID,
										(nsIFactory**)&localeFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");


	//
	// test NewLocale
	//
	localeName = new nsString("ja-JP");
	result = localeFactory->NewLocale(localeName,&locale);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_new_interface failed");
	NS_ASSERTION(locale!=NULL,"nsLocaleTest: factory_new_interface failed");

	for(i=0;i<6;i++)
	{
		catagory = new nsString(localeCatagoryList[i]);
		value = new nsString();

		result = locale->GetCatagory(catagory,value);
		NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_new_interface failed");
		NS_ASSERTION(value->Equals(*localeName)==PR_TRUE,"nsLocaleTest: factory_new_interface failed");
	
		delete catagory;
		delete value;
	}
	delete localeName;
	locale->Release();

	catagoryList = new nsString*[6];
	valueList = new nsString*[6];

	for(i=0;i<6;i++)
	{
		catagoryList[i] = new nsString(localeCatagoryList[i]);
		valueList[i] = new nsString("x-netscape");
	}

	result = localeFactory->NewLocale(catagoryList,valueList,6,&locale);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_new_interface failed");
	NS_ASSERTION(locale!=NULL,"nsLocaleTest: factory_new_interface failed");

	for(i=0;i<6;i++)
	{
		value = new nsString();
		result = locale->GetCatagory(catagoryList[i],value);
		NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_new_interface failed");
		NS_ASSERTION(value->Equals(*(valueList[i]))==PR_TRUE,"nsLocaleTest: factory_new_interface failed");

		delete value;
	}

	for(i=0;i<6;i++)
	{
		delete catagoryList[i];
		delete valueList[i];
	}

	delete [] catagoryList;
	delete [] valueList;

	locale->Release();

	localeFactory->Release();
}


void
factory_get_locale(void)
{
	nsresult			result;
	nsILocaleFactory*	localeFactory;
	nsILocale*			locale;
	nsString*			catagory;
	nsString*			value;

	result = nsComponentManager::FindFactory(kLocaleFactoryCID,
										(nsIFactory**)&localeFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");

	//
	// get the application locale
	//
	result = localeFactory->GetApplicationLocale(&locale);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_get_locale failed");
	NS_ASSERTION(locale!=NULL,"nsLocaleTest: factory_get_locale failed");

	//
	// test and make sure the locale is a valid Interface
	//
	locale->AddRef();

	catagory = new nsString("NSILOCALE_CTYPE");
	value = new nsString();

	result = locale->GetCatagory(catagory,value);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_get_locale failed");
	NS_ASSERTION(value->Length()>0,"nsLocaleTest: factory_get_locale failed");

	locale->Release();
	locale->Release();

	delete catagory;
	delete value;

	//
	// test GetSystemLocale
	//
	result = localeFactory->GetSystemLocale(&locale);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_get_locale failed");
	NS_ASSERTION(locale!=NULL,"nsLocaleTest: factory_get_locale failed");

	//
	// test and make sure the locale is a valid Interface
	//
	locale->AddRef();

	catagory = new nsString("NSILOCALE_CTYPE");
	value = new nsString();

	result = locale->GetCatagory(catagory,value);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_get_locale failed");
	NS_ASSERTION(value->Length()>0,"nsLocaleTest: factory_get_locale failed");

	locale->Release();
	locale->Release();

	delete catagory;
	delete value;

	localeFactory->Release();

}

#ifdef XP_PC

void
win32factory_create_interface(void)
{
	nsresult			result;
	nsIFactory*			factory;
	nsIWin32Locale*		win32Locale;

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIFactoryIID,
									(void**)&factory);
	NS_ASSERTION(factory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");

	factory->Release();

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&win32Locale);
	NS_ASSERTION(win32Locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");

	win32Locale->Release();
}

void
win32locale_test(void)
{
	nsresult			result;
	nsIWin32Locale*		win32Locale;
	nsString*			locale;
	LCID				loc_id;

	//
	// test with a simple locale
	//
	locale = new nsString("en-US");
	loc_id = 0;

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&win32Locale);
	NS_ASSERTION(win32Locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");

	delete locale;

	//
	// test with a not so simple locale
	//
	locale = new nsString("x-netscape");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(USER_DEFINED_PRIMARYLANG,USER_DEFINED_SUBLANGUAGE),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");

	delete locale;

	locale = new nsString("en");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");

	delete locale;
	win32Locale->Release();
}

void
win32locale_conversion_test(void)
{
	nsresult			result;
	nsIWin32Locale*		win32Locale;
	nsString*			locale;
	LCID				loc_id;

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&win32Locale);
	NS_ASSERTION(win32Locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");

	//
	// check english variants
	//
	locale = new nsString("en");	// generic english
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("en-US");	// US english
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("en-GB");	// UK english
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_UK),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("en-CA");	// Canadian english
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_CAN),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	//
	// japanese
	//
	locale = new nsString("ja");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_JAPANESE,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("ja-JP");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_JAPANESE,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	//
	// chinese Locales
	//
	locale = new nsString("zh");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_CHINESE,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("zh-CN");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_SIMPLIFIED),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("zh-TW");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_TRADITIONAL),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	//
	// german and variants
	//
	locale = new nsString("de");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_GERMAN,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("de-DE");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_GERMAN,SUBLANG_GERMAN),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("de-AT");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_GERMAN,SUBLANG_GERMAN_AUSTRIAN),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	//
	// french and it's variants
	//
	locale = new nsString("fr");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_FRENCH,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("fr-FR");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_FRENCH,SUBLANG_FRENCH),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("fr-CA");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_FRENCH,SUBLANG_FRENCH_CANADIAN),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	//
	// delete the XPCOM inteface
	//
	win32Locale->Release();
}

void
win32locale_reverse_conversion_test(void)
{
	nsresult			result;
	nsIWin32Locale*		win32Locale;
	nsString*			locale, *xp_locale;
	LCID				loc_id;

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&win32Locale);
	NS_ASSERTION(win32Locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");

	//
	// english and variants
	//
	win32Locale->Release();
}

	
#endif XP_PC

int
main(int argc, char** argv)
{

	//
	// what are we doing?
	//
	printf("Starting nsLocaleTest\n");
	printf("---------------------\n");
	printf("This test has completed successfully if no error messages are printed.\n");

	//
	// run the nsILocaleFactory tests (nsILocale gets tested in the prcoess)
	//
	factory_create_interface();
	factory_get_locale();
	factory_new_locale();

#ifdef XP_PC

	//
	// run the nsIWin32LocaleFactory tests
	//
	win32factory_create_interface();
	win32locale_test();
	win32locale_conversion_test();
	win32locale_reverse_conversion_test();

#endif


	//
	// we done
	//
	printf("---------------------\n");
	printf("Finished nsLocaleTest\n");

	return 0;
}
