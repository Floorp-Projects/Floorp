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

#include "nsXPComCIID.h"
#include "nsIEventQueueService.h"
#include "nsINetService.h"
#include "nsIProperties.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#ifdef XP_PC
#include "plevent.h"
#endif

#define TEST_URL "resource:/res/test.PersistentProperties"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define RAPTORBASE_DLL "raptorbase.dll"
#define XPCOM_DLL "xpcom32.dll"
#else
#ifdef XP_MAC
#define NETLIB_DLL "NETLIB_DLL"
#define RAPTORBASE_DLL "base.shlb"
#define XPCOM_DLL "XPCOM_DLL"
#else
#define NETLIB_DLL "libnetlib.so"
#define RAPTORBASE_DLL "libraptorbase.so"
#define XPCOM_DLL "libxpcom.so"
#endif
#endif
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kIPersistentPropertiesIID, NS_IPERSISTENTPROPERTIES_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);


#ifdef XP_MAC  // have not build this on PC and UNIX yet so make it #ifdef XP_MAC
extern "C" void NS_SetupRegistry();
#endif

int
main(int argc, char *argv[])
{
  nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL,
    PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL,
    PR_FALSE, PR_FALSE);
#ifdef XP_MAC    // have not build this on PC and UNIX yet so make it #ifdef XP_MAC
  NS_SetupRegistry(); 
#endif
  
  nsresult ret;
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
  nsIInputStream *in = nsnull;
  ret = pNetService->OpenBlockingStream(url, nsnull, &in);
  if (NS_FAILED(ret)) {
    printf("cannot open stream\n");
    return 1;
  }
  nsIPersistentProperties *props = nsnull;
  ret = nsComponentManager::CreateInstance(kPersistentPropertiesCID, NULL,
    kIPersistentPropertiesIID, (void**) &props);
  if (NS_FAILED(ret)) {
    printf("create nsIPersistentProperties failed\n");
    return 1;
  }
  props->Load(in);
  int i = 1;
  while (1) {
    char name[16];
    sprintf(name, "%d", i);
    nsAutoString v("");
    props->GetProperty(name, v);
    if (!v.Length()) {
      break;
    }
    char *value = v.ToNewCString();
    cout << "\"" << i << "\"=\"" << value << "\"" << endl;
    delete[] value;
    i++;
  }

  return 0;
}
