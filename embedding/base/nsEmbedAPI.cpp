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
#include "nsIAppStartupNotifier.h"
#include "nsIStringBundle.h"

#include "nsIDirectoryService.h"

#include "nsEmbedAPI.h"
#include "nsLiteralString.h"

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


nsresult NS_InitEmbedding(nsILocalFile *mozBinDirectory,
                          nsIDirectoryServiceProvider *appFileLocProvider)
{
    nsresult rv;

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
        NS_InitXPCOM2(&sServiceManager, mozBinDirectory, appFileLocProvider);
                
#ifdef HACK_AROUND_NONREENTRANT_INITXPCOM
        sXPCOMInitializedFlag = PR_TRUE;
        sXPCOMCleanupHack.mCleanOnExit = PR_TRUE;
#endif
    }

    // Create the Event Queue for the UI thread...
    //
    // If an event queue already exists for the thread, then 
    // CreateThreadEventQueue(...) will fail...
    // CreateThread0ueue(...) will fail...
    nsCOMPtr<nsIEventQueueService> eventQService(
                do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;

    eventQService->CreateThreadEventQueue();

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

	nsCOMPtr<nsIObserver> mStartupNotifier = do_CreateInstance(NS_APPSTARTUPNOTIFIER_CONTRACTID, &rv);
	if(NS_FAILED(rv))
		return rv;
	mStartupNotifier->Observe(nsnull, APPSTARTUP_TOPIC, nsnull);

#ifdef HACK_AROUND_THREADING_ISSUES
    // XXX force certain objects to be created on the main thread
    nsCOMPtr<nsIStringBundleService> sBundleService;
    sBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIStringBundle> stringBundle;
        char*  propertyURL = "chrome://necko/locale/necko.properties";
        rv = sBundleService->CreateBundle(propertyURL,
                                          getter_AddRefs(stringBundle));
    }
#endif

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

    NS_IF_RELEASE(sServiceManager);

    // Terminate XPCOM & cleanup
#ifndef HACK_AROUND_NONREENTRANT_INITXPCOM
    NS_ShutdownXPCOM(sServiceManager);
#endif

    return NS_OK;
}
