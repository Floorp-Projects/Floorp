/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Conrad Carlen <conrad@ingress.com>
 */

// Local Includes
#include "CWebBrowserChrome.h"
#include "CBrowserWindow.h"

#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"

#include "UMacUnicode.h"

// Interfaces needed to be included

// CIDs

//*****************************************************************************
//***    CWebBrowserChrome: Object Management
//*****************************************************************************

CWebBrowserChrome::CWebBrowserChrome() : mBrowserWindow(nsnull)
{
	NS_INIT_REFCNT();
}

CWebBrowserChrome::~CWebBrowserChrome()
{
}

//*****************************************************************************
// CWebBrowserChrome::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(CWebBrowserChrome)
NS_IMPL_RELEASE(CWebBrowserChrome)

NS_INTERFACE_MAP_BEGIN(CWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
NS_INTERFACE_MAP_END

//*****************************************************************************
// CWebBrowserChrome::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   return QueryInterface(aIID, aInstancePtr);
}

//*****************************************************************************
// CWebBrowserChrome::nsIWebBrowserChrome
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::SetJSStatus(const PRUnichar* aStatus)
{
   NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   mBrowserWindow->SetStatus(aStatus);
  
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetJSDefaultStatus(const PRUnichar* aStatus)
{
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetOverLink(const PRUnichar* aLink)
{
   NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   mBrowserWindow->SetOverLink(aLink);
  
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CWebBrowserChrome::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CWebBrowserChrome::GetChromeMask(PRUint32* aChromeMask)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CWebBrowserChrome::SetChromeMask(PRUint32 aChromeMask)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}


NS_IMETHODIMP CWebBrowserChrome::GetNewBrowser(PRUint32 chromeMask, nsIWebBrowser **webBrowser)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}


NS_IMETHODIMP CWebBrowserChrome::FindNamedBrowserItem(const PRUnichar* aName,
                                                  	  nsIDocShellTreeItem ** aWebBrowser)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CWebBrowserChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}


NS_IMETHODIMP CWebBrowserChrome::ShowAsModal(void)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}


//*****************************************************************************
// CWebBrowserChrome::nsIDocumentLoaderObserver
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::OnStartDocumentLoad(nsIDocumentLoader *aLoader, nsIURI *aURL, const char *aCommand)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);
	return mBrowserWindow->BeginDocumentLoad(aLoader, aURL, aCommand);
}

NS_IMETHODIMP CWebBrowserChrome::OnEndDocumentLoad(nsIDocumentLoader *aLoader, nsIChannel *aChannel, PRUint32 aStatus)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);
	return mBrowserWindow->EndDocumentLoad(aLoader, aChannel, aStatus);
}

NS_IMETHODIMP CWebBrowserChrome::OnStartURLLoad(nsIDocumentLoader *aLoader, nsIChannel *channel)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWebBrowserChrome::OnProgressURLLoad(nsIDocumentLoader *aLoader, nsIChannel *aChannel, PRUint32 aProgress, PRUint32 aProgressMax)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWebBrowserChrome::OnStatusURLLoad(nsIDocumentLoader *loader, nsIChannel *channel, nsString & aMsg)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWebBrowserChrome::OnEndURLLoad(nsIDocumentLoader *aLoader, nsIChannel *aChannel, PRUint32 aStatus)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}


//*****************************************************************************
// CWebBrowserChrome::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::OnProgressChange(nsIChannel *channel, PRInt32 curSelfProgress, PRInt32 maxSelfProgress, PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::OnChildProgressChange(nsIChannel *channel, PRInt32 curSelfProgress, PRInt32 curTotalProgress)
{
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::OnStatusChange(nsIChannel *channel, PRInt32 progressStatusFlags)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);
	
	switch (progressStatusFlags)
	{
	   case nsIWebProgress::flag_net_start:
	      mBrowserWindow->OnStatusNetStart(channel);
	      break;
	   case nsIWebProgress::flag_net_stop:
	      mBrowserWindow->OnStatusNetStop(channel);
	      break;
	   case nsIWebProgress::flag_net_dns:
	      mBrowserWindow->OnStatusDNS(channel);
	      break;
	}

   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::OnChildStatusChange(nsIChannel *channel, PRInt32 progressStatusFlags)
{
   return NS_OK;
}


NS_IMETHODIMP CWebBrowserChrome::OnLocationChange(nsIURI *location)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

	nsXPIDLCString spec;
 
	if (location)
		location->GetSpec(getter_Copies(spec));

	nsAutoString tmp(spec);
	mBrowserWindow->SetLocation(tmp);

	return NS_OK;
}


//*****************************************************************************
// CWebBrowserChrome::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   // Ignore wigdet parents for now.  Don't think those are a vaild thing to call.
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, PR_FALSE), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::Create()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP CWebBrowserChrome::Destroy()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP CWebBrowserChrome::SetPosition(PRInt32 x, PRInt32 y)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_ARG_POINTER(x && y);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(cx && cy);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx,
   PRInt32* cy)
{
   
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::Repaint(PRBool aForce)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWebBrowserChrome::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWebBrowserChrome::GetVisibility(PRBool* aVisibility)
{
   NS_ENSURE_ARG_POINTER(aVisibility);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetVisibility(PRBool aVisibility)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetFocus()
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);
   NS_ENSURE_STATE(mBrowserWindow);

   Str255         aStr;
   nsAutoString   titleStr;
   
   mBrowserWindow->GetDescriptor(aStr);
   UMacUnicode::Str255ToString(aStr, titleStr);
   *aTitle = titleStr.ToNewUnicode();
   
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetTitle(const PRUnichar* aTitle)
{
   NS_ENSURE_STATE(mBrowserWindow);

	nsAutoString   titleStr(aTitle);
	Str255         aStr;
	
	UMacUnicode::StringToStr255(titleStr, aStr);
   mBrowserWindow->SetDescriptor(aStr);
   
   return NS_OK;
}

//*****************************************************************************
// CWebBrowserChrome: Helpers
//*****************************************************************************   

//*****************************************************************************
// CWebBrowserChrome: Accessors
//*****************************************************************************   

void CWebBrowserChrome::BrowserWindow(CBrowserWindow* aBrowserWindow)
{
   mBrowserWindow = aBrowserWindow;
}

CBrowserWindow* CWebBrowserChrome::BrowserWindow()
{
   return mBrowserWindow;
}

