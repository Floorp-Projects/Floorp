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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
/*
  This program simply goes through the network service to grab a URL
  and write it to standard out...

  The program takes a single parameter: url to fetch
 */
// as does this
#define NS_IMPL_IDS
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIIOService.h"
#include "nsCOMPtr.h"
#include "nsIURL.h"
#include "nsIEventQueueService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIGenericFactory.h"
#include "nsIStreamListener.h"
#include "nsFileStream.h"
#include "nsFileSpec.h"
#include "nsMimeTypes.h"
#include "nsIPref.h"
#include "prprf.h"
#include "nsIMemory.h" // for the CID
#include "nsURLFetcher.h"

#include "nsIIOService.h"
#include "nsIChannel.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#ifdef WIN32
#include "windows.h"
#endif

////////////////////////////////////////////////////////////////////////////////////
// THIS IS THE STUFF TO GET THE TEST HARNESS OFF THE GROUND
////////////////////////////////////////////////////////////////////////////////////
#if defined(XP_PC)
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define MIME_DLL  "mime.dll"
#define PREF_DLL  "xppref32.dll"
#define UNICHAR_DLL  "uconv.dll"
#elif defined(XP_UNIX) || defined(XP_BEOS)
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define MIME_DLL  "libmime.dll"MOZ_DLL_SUFFIX
#define PREF_DLL  "libxppref32"MOZ_DLL_SUFFIX
#define UNICHAR_DLL  "libunicharutil"MOZ_DLL_SUFFIX
#elif defined(XP_MAC)
#define NETLIB_DLL "NETLIB_DLL"
#define XPCOM_DLL  "XPCOM_DLL"
#define MIME_DLL   "MIME_DLL"
#define PREF_DLL  "XPPREF32_DLL"
#define UNICHAR_DLL  "UNICHARUTIL_DLL"
#endif

// prefs
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

// xpcom
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kGenericFactoryCID,    NS_GENERICFACTORY_CID);

// netlib definitions....

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kICharsetConverterManagerIID, NS_ICHARSETCONVERTERMANAGER_IID);

nsICharsetConverterManager *ccMan = nsnull;

static nsresult
SetupRegistry(void)
{
  // i18n
  nsComponentManager::RegisterComponent(kCharsetConverterManagerCID, NULL, NULL, UNICHAR_DLL,  PR_FALSE, PR_FALSE);
  nsresult res = nsServiceManager::GetService(kCharsetConverterManagerCID, NS_GET_IID(nsICharsetConverterManager), (nsISupports **)&ccMan);
  if (NS_FAILED(res)) 
  {
    printf("ERROR at GetService() code=0x%x.\n",res);
    return NS_ERROR_FAILURE;
  }

  // netlib
  
  // xpcom
  static NS_DEFINE_CID(kMemoryCID,  NS_MEMORY_CID);
  static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
  nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kEventQueueCID,        NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kGenericFactoryCID,    NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kMemoryCID,            NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);

  // prefs
  nsComponentManager::RegisterComponent(kPrefCID,              NULL, NULL, PREF_DLL,  PR_FALSE, PR_FALSE);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// END OF STUFF TO GET THE TEST HARNESS OFF THE GROUND
////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////
// THIS IS THE CLASS THAT WOULD BE IMPLEMENTED BY THE CONSUMER OF THE HTML OUPUT
// FROM LIBMIME. THIS EXAMPLE SIMPLY WRITES THE OUTPUT TO STDOUT()
////////////////////////////////////////////////////////////////////////////////////
class ConsoleOutputStreamImpl : public nsIOutputStream
{
public:
    ConsoleOutputStreamImpl(void) { NS_INIT_REFCNT(); }
    virtual ~ConsoleOutputStreamImpl(void) {}

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIBaseStream interface
    NS_IMETHOD Close(void) 
    {
      return NS_OK;
    }

    // nsIOutputStream interface
    NS_IMETHOD Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount) {
        PR_Write(PR_GetSpecialFD(PR_StandardOutput), aBuf, aCount);
        *aWriteCount = aCount;
        return NS_OK;
    }

    NS_IMETHOD Flush(void) {
        PR_Sync(PR_GetSpecialFD(PR_StandardOutput));
        return NS_OK;
    }
};
NS_IMPL_ISUPPORTS(ConsoleOutputStreamImpl, NS_GET_IID(nsIOutputStream));
////////////////////////////////////////////////////////////////////////////////////
// END OF CONSUMER STREAM
////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////

// Utility to create a nsIURI object...
nsresult 
nsMsgNewURL(nsIURI** aInstancePtrResult, const char * aSpec)
{  
  nsresult rv = NS_OK;
  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  NS_WITH_SERVICE(nsIIOService, pNetService, kIOServiceCID, &rv); 
  if (NS_SUCCEEDED(rv) && pNetService)
	rv = pNetService->NewURI(aSpec, nsnull, aInstancePtrResult);
  return rv;
}

void
FlushEvents(void)
{
#ifdef WIN32
    MSG msg;
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
#endif
}

nsresult 
MyCallback(nsIURI* aURL, nsresult aStatus, PRInt32 totalSize, const PRUnichar* aMsg, void *tagData)
{
  printf("Done with URL Request. Exit Code = [%d] - Received [%d] bytes\n", aStatus, totalSize);
  return NS_OK;
}

int
main(int argc, char** argv)
{
  nsresult  rv;
  nsIURI    *aURL;
  
  // Do some sanity checking...
  if (argc < 3) 
  {
    fprintf(stderr, "usage: %s <url> <output_file>\n", argv[0]);
    return 1;
  }
  
  // Setup the registry...
  SetupRegistry();

  // Get an event queue and get it started...
  NS_WITH_SERVICE(nsIEventQueueService, theEventQueueService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) 
    return rv;
  rv = theEventQueueService->CreateThreadEventQueue();
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create thread event queue");
  if (NS_FAILED(rv)) 
    return rv;

  // Create an nsIURI object needed for the URL operation IO...
  if (NS_FAILED(nsMsgNewURL(&aURL, argv[1])))
  {
    printf("Unable to create URL\n");
    return NS_ERROR_FAILURE;
  }

  // Create a fetcher for the URL attachment...
  nsURLFetcher *sFetcher = new nsURLFetcher();
  if (!sFetcher)
  {
    printf("Failed to create an attachment stream listener...\n");
    return rv;
  }

  nsFileSpec  fSpec(argv[2]);
  nsOutputFileStream dstFile(fSpec, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE );
  if (!dstFile.is_open() )
  {
    printf("Failed to open the output file!\n");
    return rv;
  }

  rv = sFetcher->FireURLRequest(aURL, &dstFile, MyCallback, sFetcher);
  if (NS_FAILED(rv)) 
  {
    printf("Failed to fire the URL request\n");
    return rv;
  }

  PRBool running = PR_TRUE; 
  while (running)
  {
    sFetcher->StillRunning(&running);
    FlushEvents();
  }

  NS_RELEASE(sFetcher);

  // Cleanup stuff necessary...
  NS_RELEASE(ccMan);
  return NS_OK;
}

