/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#define NS_IMPL_IDS

#include "nsIProperties.h"
#include "nsIStringBundle.h"
#include "nsIEventQueueService.h"
#include "nsILocale.h"
#include <iostream.h>

#ifndef NECKO
#include "nsINetService.h"
#else
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#endif

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
//
#include "nsFileLocations.h"
#include "nsIFileLocator.h"
#include "nsIFileSpec.h"

#define TEST_URL "resource:/res/strres.properties"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define RAPTORBASE_DLL "raptorbase.dll"
#define XPCOM_DLL "xpcom32.dll"
#else /* else XP_PC */
#ifdef XP_MAC
#define NETLIB_DLL "NETLIB_DLL"
#define RAPTORBASE_DLL "base.shlb"
#define XPCOM_DLL "XPCOM_DLL"
#else /* else XP_MAC */
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define RAPTORBASE_DLL "libraptorbase"MOZ_DLL_SUFFIX
#define XPCOM_DLL "libxpcom"MOZ_DLL_SUFFIX
#endif /* XP_MAC */
#endif /* XP_PC */

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

#ifndef NECKO
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
#else
static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO

static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// let add some locale stuff
//
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#include "nsILocale.h"
#include "nsILocaleFactory.h"
#include "nsLocaleCID.h"

NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
NS_DEFINE_IID(kILocaleFactoryIID, NS_ILOCALEFACTORY_IID);
//
//
//
nsILocale*
get_applocale(void)
{
	nsresult			result;
	nsILocaleFactory*	localeFactory;
	nsILocale*			locale;
	nsString*			catagory;
	nsString*			value;
  const PRUnichar *lc_name_unichar;

	result = nsComponentManager::FindFactory(kLocaleFactoryCID,
										(nsIFactory**)&localeFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_create_interface failed");

	//
	// test GetApplicationLocale
	//
	result = localeFactory->GetApplicationLocale(&locale);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_get_locale failed");
	NS_ASSERTION(locale!=NULL,"nsLocaleTest: factory_get_locale failed");

	//
	// test and make sure the locale is a valid Interface
	//
	locale->AddRef();

	catagory = new nsString("NSILOCALE_MESSAGES");
	value = new nsString();

	result = locale->GetCategory(catagory->GetUnicode(),&lc_name_unichar);
	value->SetString(lc_name_unichar);
	NS_ASSERTION(result==NS_OK,"nsLocaleTest: factory_get_locale failed");
	NS_ASSERTION(value->Length()>0,"nsLocaleTest: factory_get_locale failed");

	locale->Release();
	delete catagory;
	delete value;

	localeFactory->Release();
    return locale;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// end of locale stuff
//
////////////////////////////////////////////////////////////////////////////////////////////////////
nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, 
                                                 NULL /* default */);

	// startup netlib:	
	nsComponentManager::RegisterComponent(kEventQueueServiceCID, 
                                        NULL, NULL, 
                                        XPCOM_DLL, 
                                        PR_FALSE, PR_FALSE);
#ifndef NECKO
  nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
#else
  nsComponentManager::RegisterComponent(kIOServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
#endif // NECKO

  // Create the Event Queue for this thread...
  nsIEventQueueService* pEventQService;
  
  pEventQService = nsnull;
  nsresult result = nsServiceManager::GetService(kEventQueueServiceCID,
                                                 kIEventQueueServiceIID,
                                                 (nsISupports **)&pEventQService);
  if (NS_SUCCEEDED(result)) {
    // XXX: What if this fails?
    result = pEventQService->CreateThreadEventQueue();
  }
  
  nsComponentManager::RegisterComponent(kPersistentPropertiesCID, 
                                        NULL,
                                        NULL, 
                                        RAPTORBASE_DLL, 
                                        PR_FALSE, 
                                        PR_FALSE);
  return rv;
}

int
main(int argc, char *argv[])
{
  nsresult ret;

  NS_AutoregisterComponents();

  nsIStringBundleService* service = nsnull;
  ret = nsServiceManager::GetService(kStringBundleServiceCID,
                                     kIStringBundleServiceIID, (nsISupports**) &service);
  if (NS_FAILED(ret)) {
    printf("cannot create service\n");
    return 1;
  }

  nsILocale* locale = get_applocale();

  nsIStringBundle* bundle = nsnull;

  ret = service->CreateBundle(TEST_URL, locale, &bundle);

  /* free it
  */
  locale->Release();

  if (NS_FAILED(ret)) {
    printf("cannot create instance\n");
    return 1;
  }

  nsAutoString v("");
  PRUnichar *ptrv = nsnull;
  char *value = nsnull;

  // 123
  ret = bundle->GetStringFromID(123, &ptrv);
  if (NS_FAILED(ret)) {
    printf("cannot get string from ID 123, ret=%d\n", ret);
    return 1;
  }
  v = ptrv;
  value = v.ToNewCString();
  cout << "123=\"" << value << "\"" << endl;

  // file
  nsString strfile("file");
  const PRUnichar *ptrFile = strfile.GetUnicode();
  ret = bundle->GetStringFromName(ptrFile, &ptrv);
  if (NS_FAILED(ret)) {
    printf("cannot get string from name\n");
    return 1;
  }
  v = ptrv;
  value = v.ToNewCString();
  cout << "file=\"" << value << "\"" << endl;

 nsIBidirectionalEnumerator* propEnum = nsnull;
  ret = bundle->GetEnumeration(&propEnum);
  if (NS_FAILED(ret)) {
	  printf("cannot get enumeration\n");
	  return 1;
  }
  ret = propEnum->First();
  if (NS_FAILED(ret))
  {
	printf("enumerator is empty\n");
	return 1;
  }

  cout << endl << "Key" << "\t" << "Value" << endl;
  cout <<		  "---" << "\t" << "-----" << endl;
  while (NS_SUCCEEDED(ret))
  {
	  nsIPropertyElement* propElem = nsnull;
	  ret = propEnum->CurrentItem((nsISupports**)&propElem);
	  if (NS_FAILED(ret)) {
		printf("failed to get current item\n");
		return 1;
	  }
	  nsString* key = nsnull;
	  nsString* val = nsnull;
	  ret = propElem->GetKey(&key);
	  if (NS_FAILED(ret)) {
		  printf("failed to get current element's key\n");
		  return 1;
	  }
	  ret = propElem->GetValue(&val);
	  if (NS_FAILED(ret)) {
		  printf("failed to get current element's value\n");
		  return 1;
	  }
	  char* keyCStr = key->ToNewCString();
	  char* valCStr = val->ToNewCString();
	  if (keyCStr && valCStr) 
		cout << keyCStr << "\t" << valCStr << endl;
	  delete[] keyCStr;
	  delete[] valCStr;
      delete key;
      delete val;
	  ret = propEnum->Next();
  }

  return 0;
}
