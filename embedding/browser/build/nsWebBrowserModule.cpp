/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsIDocShell.h"
#include "nsWebBrowser.h"
#include "nsWebBrowserSetup.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kWebBrowserCID, NS_WEBBROWSER_CID);
static NS_DEFINE_CID(kWebBrowserSetupCID, NS_WEBBROWSER_SETUP_CID);

//*****************************************************************************
//*** Library Exports
//*****************************************************************************

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
	NS_ENSURE_ARG_POINTER(aFactory);
   nsresult rv;

   nsIGenericFactory* fact;

	if(aClass.Equals(kWebBrowserCID))
		rv = NS_NewGenericFactory(&fact, nsWebBrowser::Create);
	else if(aClass.Equals(kWebBrowserSetupCID))
		rv = NS_NewGenericFactory(&fact, nsWebBrowserSetup::Create);
   else 
		rv = NS_NOINTERFACE;

	if(NS_SUCCEEDED(rv))
		*aFactory = fact;
	return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
	nsresult rv;
	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
	NS_ENSURE_SUCCESS(rv, rv);

	rv = compMgr->RegisterComponent(kWebBrowserCID,  
											"nsWebBrowser",
											NS_WEBBROWSER_PROGID,
											aPath, PR_TRUE, PR_TRUE);
	rv = compMgr->RegisterComponent(kWebBrowserSetupCID,  
											"nsWebBrowserSetup",
											NS_WEBBROWSER_SETUP_PROGID,
											aPath, PR_TRUE, PR_TRUE);
	NS_ENSURE_SUCCESS(rv, rv);

	return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
	nsresult rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
	NS_ENSURE_SUCCESS(rv, rv);
	rv = compMgr->UnregisterComponent(kWebBrowserCID, aPath);
	rv = compMgr->UnregisterComponent(kWebBrowserSetupCID, aPath);
	NS_ENSURE_SUCCESS(rv, rv);

	return rv;
}
