/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsILocalFile.h"
#include "nsIEventQueueService.h"

static nsIServiceManager *sServiceManager = nsnull;
static PRBool sRegistryInitializedFlag = PR_FALSE;
#ifdef HACK_AROUND_NONREENTRANT_INITXPCOM
static PRBool sXPCOMInitializedFlag = PR_FALSE;
#endif

extern "C" void NS_SetupRegistry();

NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

nsresult NS_InitEmbedding(const char *aPath)
{
	// Initialise XPCOM
#ifdef HACK_AROUND_NONREENTRANT_INITXPCOM
	// Can't call NS_InitXPCom more than once or things go boom!
	if (!sXPCOMInitializedFlag)
#endif
	{
		// Create an object to represent the path
		nsILocalFile *pBinDirPath = nsnull;
		if (aPath && strlen(aPath) > 0)
		{
			NS_NewLocalFile(aPath, &pBinDirPath);
		}

		// Initialise XPCOM
		if (pBinDirPath)
		{
			NS_InitXPCOM(&sServiceManager, pBinDirPath);
			NS_RELEASE(pBinDirPath);
		}
		else
		{
			NS_InitXPCOM(&sServiceManager, nsnull);
		}

#ifdef HACK_AROUND_NONREENTRANT_INITXPCOM
		sXPCOMInitializedFlag = PR_TRUE;
#endif
	}

    nsresult rv;

	// Register components
	if (!sRegistryInitializedFlag)
	{
		NS_SetupRegistry();
 	    
        rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
	    if (NS_OK != rv)
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

	nsIEventQueueService* eventQService = NULL;
	rv = nsServiceManager::GetService(kEventQueueServiceCID,
								NS_GET_IID(nsIEventQueueService),
								(nsISupports **)&eventQService);
	if (NS_SUCCEEDED(rv))
	{
		rv = eventQService->CreateThreadEventQueue();
		nsServiceManager::ReleaseService(kEventQueueServiceCID, eventQService);
	}

    return NS_OK;
}

nsresult NS_TermEmbedding()
{
	// Destroy the event queue
	nsIEventQueueService* eventQService = NULL;
	nsresult rv = nsServiceManager::GetService(kEventQueueServiceCID,
								NS_GET_IID(nsIEventQueueService),
								(nsISupports **)&eventQService);
	if (NS_SUCCEEDED(rv))
	{
		rv = eventQService->DestroyThreadEventQueue();
		nsServiceManager::ReleaseService(kEventQueueServiceCID, eventQService);
	}

	// Terminate XPCOM & cleanup
#ifndef HACK_AROUND_NONREENTRANT_INITXPCOM
	NS_ShutdownXPCOM(sServiceManager);
#endif
	sServiceManager = nsnull;

	return NS_OK;
}
