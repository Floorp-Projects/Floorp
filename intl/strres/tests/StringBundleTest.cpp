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
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

#define TEST_URL "resource:/res/strres.properties"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#endif
#endif

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);

int
main(int argc, char *argv[])
{
  nsresult ret;

  ret = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                         "components");
  if (NS_FAILED(ret)) {
    printf("auto-registration failed\n");
    return 1;
  }
  nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL,
    PR_FALSE, PR_FALSE);

  nsIStringBundleService* service = nsnull;
  ret = nsServiceManager::GetService(kStringBundleServiceCID,
    kIStringBundleServiceIID, (nsISupports**) &service);
  if (NS_FAILED(ret)) {
    printf("cannot create service\n");
    return 1;
  }

  nsIEventQueueService* pEventQueueService = nsnull;
  ret = nsServiceManager::GetService(kEventQueueServiceCID,
    kIEventQueueServiceIID, (nsISupports**) &pEventQueueService);
  if (NS_FAILED(ret)) {
    printf("cannot get event queue service\n");
    return 1;
  }
  ret = pEventQueueService->CreateThreadEventQueue();
  if (NS_FAILED(ret)) {
    printf("CreateThreadEventQueue failed\n");
    return 1;
  }

  nsINetService* pNetService = nsnull;
  ret = nsServiceManager::GetService(kNetServiceCID, kINetServiceIID,
    (nsISupports**) &pNetService);
  if (NS_FAILED(ret)) {
    printf("cannot get net service\n");
    return 1;
  }
  nsIURL *url = nsnull;
  ret = pNetService->CreateURL(&url, nsString(TEST_URL), nsnull, nsnull,
    nsnull);
  if (NS_FAILED(ret)) {
    printf("cannot create URL\n");
    return 1;
  }

  nsILocale* locale = nsnull;

  nsIStringBundle* bundle = nsnull;
  ret = service->CreateBundle(url, locale, &bundle);
  if (NS_FAILED(ret)) {
    printf("cannot create instance\n");
    return 1;
  }

  nsAutoString v("");
  ret = bundle->GetStringFromName(nsString("file"), v);
  if (NS_FAILED(ret)) {
    printf("cannot get string from name\n");
    return 1;
  }
  char *value = v.ToNewCString();
  cout << "file=\"" << value << "\"" << endl;

  ret = bundle->GetStringFromID(123, v);
  if (NS_FAILED(ret)) {
    printf("cannot get string from ID\n");
    return 1;
  }
  value = v.ToNewCString();
  cout << "123=\"" << value << "\"" << endl;

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
