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

//#include "nsIComponentManager.h"

#include "nsWebBrowser.h"

//*****************************************************************************
//***    nsWebBrowser: Object Management
//*****************************************************************************

nsWebBrowser::nsWebBrowser()
{
	NS_INIT_REFCNT();
}

nsWebBrowser::~nsWebBrowser()
{
}

NS_IMETHODIMP nsWebBrowser::Create(nsISupports* aOuter, const nsIID& aIID, 
	void** ppv)
{
	NS_ENSURE_ARG_POINTER(ppv);
	NS_ENSURE_NO_AGGREGATION(aOuter);

	nsWebBrowser* browser = new  nsWebBrowser();
	NS_ENSURE(browser, NS_ERROR_OUT_OF_MEMORY);

	NS_ADDREF(browser);
	nsresult rv = browser->QueryInterface(aIID, ppv);
	NS_RELEASE(browser);
	return rv;
}

//*****************************************************************************
// nsWebBrowser::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(nsWebBrowser, nsIWebBrowser)

//*****************************************************************************
// nsWebBrowser::nsIWebBrowser
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::AddListener(nsIWebBrowserListener* listener, 
   PRInt32* cookie)
{
   //XXX Implement
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::RemoveListener(nsIWebBrowserListener* listener,
   PRInt32 cookie)
{
   //XXX Implement
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetDocShell(nsIDocShell** docShell)
{
   //XXX Implement
   return NS_ERROR_FAILURE;
}
