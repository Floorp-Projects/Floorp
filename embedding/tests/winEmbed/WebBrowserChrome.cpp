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
 */

// Local Includes

#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"
#include "nsIDocShellTreeItem.h"
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsCWebBrowser.h"
#include "nsWidgetsCID.h"

#include "WebBrowserChrome.h"

nsVoidArray WebBrowserChrome::sBrowserList;

WebBrowserChrome::WebBrowserChrome()
{
	NS_INIT_REFCNT();
    mNativeWindow = nsnull;
}

WebBrowserChrome::~WebBrowserChrome()
{
}

//*****************************************************************************
// WebBrowserChrome::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(WebBrowserChrome)
NS_IMPL_RELEASE(WebBrowserChrome)

NS_INTERFACE_MAP_BEGIN(WebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)  //optional
//   NS_INTERFACE_MAP_ENTRY(nsIPrompt)
NS_INTERFACE_MAP_END

//*****************************************************************************
// WebBrowserChrome::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP WebBrowserChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   return QueryInterface(aIID, aInstancePtr);
}

//*****************************************************************************
// WebBrowserChrome::nsIWebBrowserChrome
//*****************************************************************************   

NS_IMETHODIMP WebBrowserChrome::SetStatus(PRUint32 aType, const PRUnichar* aStatus)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
   NS_ENSURE_ARG_POINTER(aWebBrowser);
   NS_ENSURE_TRUE(mWebBrowser, NS_ERROR_NOT_INITIALIZED);
   *aWebBrowser = mWebBrowser;
   NS_IF_ADDREF(*aWebBrowser);

   return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
   NS_ENSURE_ARG(aWebBrowser);   // Passing nsnull is NOT OK
   NS_ENSURE_TRUE(mWebBrowser, NS_ERROR_NOT_INITIALIZED);
   NS_ERROR("Who be calling me");
   mWebBrowser = aWebBrowser;
   return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::GetChromeFlags(PRUint32* aChromeMask)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP WebBrowserChrome::SetChromeFlags(PRUint32 aChromeMask)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}


// in winEmbed.cpp
extern nativeWindow CreateNativeWindow(nsIWebBrowserChrome* chrome);

NS_IMETHODIMP WebBrowserChrome::CreateBrowserWindow(PRUint32 chromeMask, PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, nsIWebBrowser **aWebBrowser)
{
   NS_ENSURE_ARG_POINTER(aWebBrowser);
   *aWebBrowser = nsnull;

    mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
    
	if (!mWebBrowser)
        return NS_ERROR_FAILURE;

    mWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome*, this));

    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(mWebBrowser);
    dsti->SetItemType(nsIDocShellTreeItem::typeChromeWrapper);

    
    mBaseWindow = do_QueryInterface(mWebBrowser);
    mNativeWindow = CreateNativeWindow(NS_STATIC_CAST(nsIWebBrowserChrome*, this));

    if (!mNativeWindow)
        return NS_ERROR_FAILURE;

    mBaseWindow->InitWindow( mNativeWindow,
                             nsnull, 
                             0, 0, 450, 450);
    mBaseWindow->Create();
    
    NS_IF_ADDREF(*aWebBrowser = mWebBrowser);
    return NS_OK;
}


NS_IMETHODIMP WebBrowserChrome::FindNamedBrowserItem(const PRUnichar* aName,
                                                  	  nsIDocShellTreeItem ** aBrowserItem)
{
    NS_ENSURE_ARG(aName);
    NS_ENSURE_ARG_POINTER(aBrowserItem);
    *aBrowserItem = nsnull;

    if (!mWebBrowser)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mWebBrowser));
    NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

    return docShellAsItem->FindItemWithName(aName, NS_STATIC_CAST(nsIWebBrowserChrome*, this), aBrowserItem);
}

NS_IMETHODIMP WebBrowserChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP WebBrowserChrome::ShowAsModal(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::ExitModalEventLoop(nsresult aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
WebBrowserChrome::SetPersistence(PRBool aPersistX, PRBool aPersistY,
                                  PRBool aPersistCX, PRBool aPersistCY,
                                  PRBool aPersistSizeMode)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome::GetPersistence(PRBool* aPersistX, PRBool* aPersistY,
                                  PRBool* aPersistCX, PRBool* aPersistCY,
                                  PRBool* aPersistSizeMode)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//*****************************************************************************
// WebBrowserChrome::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP WebBrowserChrome::OnProgressChange(nsIWebProgress *progress, nsIRequest *request,
                                                  PRInt32 curSelfProgress, PRInt32 maxSelfProgress,
                                                  PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::OnStateChange(nsIWebProgress *progress, nsIRequest *request,
                                               PRInt32 progressStateFlags, PRUint32 status)
{

    if ((progressStateFlags & STATE_STOP) && (progressStateFlags & STATE_IS_REQUEST))
    {
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP WebBrowserChrome::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
WebBrowserChrome::OnStatusChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 nsresult aStatus,
                                 const PRUnichar* aMessage)
{
    return NS_OK;
}

//*****************************************************************************
// WebBrowserChrome::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP WebBrowserChrome::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::Create()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP WebBrowserChrome::Destroy()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP WebBrowserChrome::SetPosition(PRInt32 x, PRInt32 y)
{
    return mBaseWindow->SetPosition(x, y);
}

NS_IMETHODIMP WebBrowserChrome::GetPosition(PRInt32* x, PRInt32* y)
{
    return mBaseWindow->GetPosition(x, y);
}

NS_IMETHODIMP WebBrowserChrome::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
    return mBaseWindow->SetSize(cx, cy, fRepaint);
}

NS_IMETHODIMP WebBrowserChrome::GetSize(PRInt32* cx, PRInt32* cy)
{
    return mBaseWindow->GetSize(cx, cy);
}

NS_IMETHODIMP WebBrowserChrome::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
    return mBaseWindow->SetPositionAndSize(x, y, cx, cy, fRepaint);
}

NS_IMETHODIMP WebBrowserChrome::GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx, PRInt32* cy)
{
    return mBaseWindow->GetPositionAndSize(x, y, cx, cy);
}

NS_IMETHODIMP WebBrowserChrome::Repaint(PRBool aForce)
{
   return mBaseWindow->Repaint(aForce);
}

NS_IMETHODIMP WebBrowserChrome::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);

   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   *aParentNativeWindow = mNativeWindow;
   return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   mNativeWindow = aParentNativeWindow;
   return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::GetVisibility(PRBool* aVisibility)
{
   return mBaseWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP WebBrowserChrome::SetVisibility(PRBool aVisibility)
{   
   return mBaseWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP WebBrowserChrome::GetMainWidget(nsIWidget** aMainWidget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::SetFocus()
{
   return mBaseWindow->SetFocus();
}

NS_IMETHODIMP WebBrowserChrome::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   return mBaseWindow->FocusAvailable(aCurrentFocus, aTookFocus);
}

NS_IMETHODIMP WebBrowserChrome::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);

   *aTitle = nsnull;
   
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::SetTitle(const PRUnichar* aTitle)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}
