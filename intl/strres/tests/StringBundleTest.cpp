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

#include "nsIStringBundle.h"
#include "nsIEventQueueService.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsRepository.h"
#include "nsXPComCIID.h"

#define TEST_URL "resource:/res/strres.properties"

#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL "xpcom32.dll"

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kIStringBundleFactoryIID, NS_ISTRINGBUNDLEFACTORY_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kStringBundleFactoryCID, NS_STRINGBUNDLEFACTORY_CID);

int
main(int argc, char *argv[])
{
  nsresult ret;

  nsIStringBundleFactory* factory = nsnull;
  ret = nsRepository::FindFactory(kStringBundleFactoryCID,
    (nsIFactory**) &factory);
  if (NS_FAILED(ret)) {
    printf("cannot create factory\n");
    return 1;
  }

  nsRepository::RegisterFactory(kEventQueueServiceCID, XPCOM_DLL,
    PR_FALSE, PR_FALSE);
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

  nsRepository::RegisterFactory(kNetServiceCID, NETLIB_DLL, PR_FALSE, PR_FALSE);
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

  nsAutoString locale("en-US");

  nsIStringBundle* bundle = nsnull;
  ret = factory->CreateBundle(url, locale, &bundle);
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

  return 0;
}
