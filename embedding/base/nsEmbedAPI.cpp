/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s):
 */

#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIChromeRegistry.h"

#include "nsIStringBundle.h"

#include "nsEmbedAPI.h"

static nsIServiceManager *sServiceManager = nsnull;
static PRBool             sRegistryInitializedFlag = PR_FALSE;
static PRUint32           sInitCounter = 0;

#define HACK_AROUND_THREADING_ISSUES
//#define HACK_AROUND_NONREENTRANT_INITXPCOM

#ifdef HACK_AROUND_NONREENTRANT_INITXPCOM
// XXX hack class to clean up XPCOM when this module is unloaded
class XPCOMCleanupHack
{
public:
    PRBool mCleanOnExit;

    XPCOMCleanupHack() : mCleanOnExit(PR_FALSE) {}
    ~XPCOMCleanupHack()
    {
        if (mCleanOnExit)
        {
            if (sInitCounter > 0)
            {
                sInitCounter = 1;
                NS_TermEmbedding();
            }
            // XXX Global destructors and NS_ShutdownXPCOM don't seem to mix
//          NS_ShutdownXPCOM(sServiceManager);
        }
    }
};
static PRBool sXPCOMInitializedFlag = PR_FALSE;
static XPCOMCleanupHack sXPCOMCleanupHack;
#endif

extern "C" void NS_SetupRegistry();

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);


nsresult NS_InitEmbedding(const char *aPath)
{
    // Create an object to represent the path
    nsILocalFile *localFile = nsnull;
    if (aPath && strlen(aPath) > 0)
    {
        NS_NewLocalFile(aPath, PR_FALSE, &localFile);
    }

    nsresult rv = NS_InitEmbedding(localFile);
    NS_IF_RELEASE(localFile);

    return rv;
}


nsresult NS_InitEmbedding(nsILocalFile *aPath)
{
    // Reentrant calls to this method do nothing except increment a counter
    sInitCounter++;
    if (sInitCounter > 1)
        return NS_OK;

    // Initialise XPCOM
#ifdef HACK_AROUND_NONREENTRANT_INITXPCOM
    // Can't call NS_InitXPCom more than once or things go boom!
    if (!sXPCOMInitializedFlag)
#endif
    {
        // Initialise XPCOM
        if (aPath)
            NS_InitXPCOM(&sServiceManager, aPath);
        else
            NS_InitXPCOM(&sServiceManager, nsnull);

#ifdef HACK_AROUND_NONREENTRANT_INITXPCOM
        sXPCOMInitializedFlag = PR_TRUE;
        sXPCOMCleanupHack.mCleanOnExit = PR_TRUE;
#endif
    }

    nsresult rv;

    // Register components
    if (!sRegistryInitializedFlag)
    {
        // XXX hack method
        NS_SetupRegistry();
        
        rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                              NULL /* default */);
        if (NS_FAILED(rv))
        {
            NS_ASSERTION(PR_FALSE, "Could not AutoRegister");
            return rv;
        }

        sRegistryInitializedFlag = PR_TRUE;
    }

    // Create the Event Queue for the UI thread...
    //
    // If an event queue already exists for the thread, then 
    // CreateThreadEventQueue(...) will fail...
    NS_WITH_SERVICE(nsIEventQueueService, eventQService,
                    kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv))
      return rv;

    eventQService->CreateThreadEventQueue();

#ifdef HACK_AROUND_THREADING_ISSUES
    // XXX force certain objects to be created on the main thread
    NS_WITH_SERVICE(nsIStringBundleService, sBundleService,
                    kStringBundleServiceCID, &rv);
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIStringBundle> stringBundle;
        char*  propertyURL = "chrome://necko/locale/necko.properties";
        nsILocale *locale = nsnull;
        rv = sBundleService->CreateBundle(propertyURL, locale,
                                          getter_AddRefs(stringBundle));
    }
#endif

    // Init the chrome registry.
    nsCOMPtr <nsIChromeRegistry> chromeReg = 
        do_GetService("component://netscape/chrome/chrome-registry");
    NS_ASSERTION(chromeReg, "chrome check couldn't get the chrome registry");

    if (!chromeReg)
        return NS_ERROR_FAILURE;

    // Ignore the return value here.  If chrome is already initialized
    // this call will return an error even though nothing is wrong.
    chromeReg->CheckForNewChrome();

    return NS_OK;

}

nsresult NS_TermEmbedding()
{
    // Reentrant calls to this method do nothing except decrement a counter
    if (sInitCounter > 1)
    {
        sInitCounter--;
        return NS_OK;
    }
    sInitCounter = 0;

    nsresult rv;
    // Destroy the event queue
    NS_WITH_SERVICE(nsIEventQueueService, eventQService,
                    kEventQueueServiceCID, &rv);
    if (NS_SUCCEEDED(rv))
        rv = eventQService->DestroyThreadEventQueue();

    NS_RELEASE(sServiceManager);

    // Terminate XPCOM & cleanup
#ifndef HACK_AROUND_NONREENTRANT_INITXPCOM
    NS_ShutdownXPCOM(sServiceManager);
#endif

    return NS_OK;
}
