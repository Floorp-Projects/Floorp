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

#include "nsHTMLDocShell.h"
#include "nsString.h"

//*****************************************************************************
//***    nsHTMLDocShell: Object Management
//*****************************************************************************

nsHTMLDocShell::nsHTMLDocShell() : nsDocShellBase()
{
}

nsHTMLDocShell::~nsHTMLDocShell()
{
}

NS_IMETHODIMP nsHTMLDocShell::Create(nsISupports* aOuter, const nsIID& aIID, 
	void** ppv)
{
	NS_ENSURE_ARG_POINTER(ppv);
	NS_ENSURE_NO_AGGREGATION(aOuter);

	nsHTMLDocShell* docShell = new  nsHTMLDocShell();
	NS_ENSURE(docShell, NS_ERROR_OUT_OF_MEMORY);

	NS_ADDREF(docShell);
	nsresult rv = docShell->QueryInterface(aIID, ppv);
	NS_RELEASE(docShell);  
	return rv;
}

//*****************************************************************************
// nsHTMLDocShell::nsISupports Overrides
//*****************************************************************************   

NS_IMPL_ISUPPORTS7(nsHTMLDocShell, nsIDocShell, nsIHTMLDocShell, 
   nsIDocShellEdit, nsIDocShellFile, nsIGenericWindow, nsIScrollable, 
   nsITextScroll)

//*****************************************************************************
// nsHTMLDocShell::nsIDocShell Overrides
//*****************************************************************************   

NS_IMETHODIMP nsHTMLDocShell::CanHandleContentType(const PRUnichar* contentType, 
   PRBool* canHandle)
{
   NS_ENSURE_ARG_POINTER(canHandle);

   nsAutoString aType(contentType);
                                         
   if(aType.EqualsIgnoreCase("text/html"))
      *canHandle = PR_TRUE;
   else   
      *canHandle = PR_FALSE;
   return NS_OK;
}

//*****************************************************************************
// nsHTMLDocShell::nsIHTMLDocShell
//*****************************************************************************   

NS_IMETHODIMP nsHTMLDocShell::ScrollToNode(nsIDOMNode* node)
{
   NS_ENSURE_ARG(node);
   //XXX First Check
    	/*
	Scrolls to a given DOM content node. 
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::GetAllowPlugins(PRBool* allowPlugins)
{
   NS_ENSURE_ARG_POINTER(allowPlugins);
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::SetAllowPlugins(PRBool allowPlugins)
{
   //XXX First Check
	/** if true, plugins are allowed within the doc shell.  default = true */
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::GetMarginWidth(PRInt32* marginWidth)
{
   NS_ENSURE_ARG_POINTER(marginWidth);
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::SetMarginWidth(PRInt32 marginWidth)
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::GetMarginHeight(PRInt32* marginHeight)
{
   NS_ENSURE_ARG_POINTER(marginHeight);
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::SetMarginHeight(PRInt32 marginHeight)
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::GetIsFrame(PRBool* isFrame)
{
   NS_ENSURE_ARG_POINTER(isFrame);
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::SetIsFrame(PRBool isFrame)
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::GetDefaultCharacterSet(PRUnichar** defaultCharacterSet)
{
   NS_ENSURE_ARG_POINTER(defaultCharacterSet);
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::SetDefaultCharacterSet(const PRUnichar* defaultCharacterSet)
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::GetForceCharacterSet(PRUnichar** forceCharacterSet)
{
   NS_ENSURE_ARG_POINTER(forceCharacterSet);
   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLDocShell::SetForceCharacterSet(const PRUnichar* forceCharacterSet)
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsHTMLDocShell::nsIDocShellEdit Overrides
//*****************************************************************************   

//*****************************************************************************
// nsHTMLDocShell::nsIDocShellFile Overrides
//*****************************************************************************   

//*****************************************************************************
// nsHTMLDocShell::nsIGenericWindow Overrides
//*****************************************************************************   

//*****************************************************************************
// nsHTMLDocShell::nsIScrollable Overrides
//*****************************************************************************   

//*****************************************************************************
// nsHTMLDocShell::nsITextScroll Overrides
//*****************************************************************************   
