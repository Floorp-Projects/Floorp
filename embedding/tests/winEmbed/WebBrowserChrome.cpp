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
    mUI = nsnull;
}

WebBrowserChrome::~WebBrowserChrome()
{
    if (mUI)
    {
        delete mUI;
    }
}

void WebBrowserChrome::SetUI(WebBrowserChromeUI *aUI)
{
    mUI = aUI;
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
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserSiteWindow)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener) // optional
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY(nsIPrompt)
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
   mUI->UpdateStatusBarText(this, aStatus);
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

NS_IMETHODIMP WebBrowserChrome::CreateBrowserWindow(PRUint32 chromeMask, PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, nsIWebBrowser **aWebBrowser)
{
   static int gCount = 0;

   NS_ENSURE_ARG_POINTER(aWebBrowser);
   *aWebBrowser = nsnull;

   if (++gCount > 1)
     return NS_OK;

    mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
    
	if (!mWebBrowser)
        return NS_ERROR_FAILURE;

    (void)mWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome*, this));

    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(mWebBrowser);
    dsti->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

    
    mBaseWindow = do_QueryInterface(mWebBrowser);
    mNativeWindow = mUI->CreateNativeWindow(NS_STATIC_CAST(nsIWebBrowserChrome*, this));

    if (!mNativeWindow)
        return NS_ERROR_FAILURE;

    mBaseWindow->InitWindow( mNativeWindow,
                             nsnull, 
                             aX, aY, aCX, aCY);
    mBaseWindow->Create();

    nsCOMPtr<nsIWebProgressListener> listener = NS_STATIC_CAST(nsIWebProgressListener*, this);
    nsCOMPtr<nsISupports> supports = do_QueryInterface(listener);
    (void)mWebBrowser->AddWebBrowserListener(supports, 
        NS_GET_IID(nsIWebProgressListener));
    
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

NS_IMETHODIMP WebBrowserChrome::IsWindowModal(PRBool *_retval)
{
  *_retval = PR_FALSE;
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
    mUI->UpdateProgress(this, curTotalProgress, maxTotalProgress);
    return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::OnStateChange(nsIWebProgress *progress, nsIRequest *request,
                                               PRInt32 progressStateFlags, PRUint32 status)
{
    if ((progressStateFlags & STATE_START) && (progressStateFlags & STATE_IS_DOCUMENT))
    {
        mUI->UpdateBusyState(this, PR_TRUE);
    }

    if ((progressStateFlags & STATE_STOP) && (progressStateFlags & STATE_IS_DOCUMENT))
    {
        mUI->UpdateBusyState(this, PR_FALSE);
        mUI->UpdateProgress(this, 0, 100);
        mUI->UpdateStatusBarText(this, nsnull);
    }

    return NS_OK;
}


NS_IMETHODIMP WebBrowserChrome::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI *location)
{
    mUI->UpdateCurrentURI(this);
    return NS_OK;
}

NS_IMETHODIMP 
WebBrowserChrome::OnStatusChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 nsresult aStatus,
                                 const PRUnichar* aMessage)
{
    mUI->UpdateStatusBarText(this, aMessage);
    return NS_OK;
}



NS_IMETHODIMP 
WebBrowserChrome::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


//*****************************************************************************
// WebBrowserChrome::nsIWebBrowserSiteWindow
//*****************************************************************************   

/* void destroy (); */
NS_IMETHODIMP WebBrowserChrome::Destroy()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

/* void setPosition (in long x, in long y); */
NS_IMETHODIMP WebBrowserChrome::SetPosition(PRInt32 x, PRInt32 y)
{
    return mBaseWindow->SetPosition(x, y);
}

/* void getPosition (out long x, out long y); */
NS_IMETHODIMP WebBrowserChrome::GetPosition(PRInt32 *x, PRInt32 *y)
{
    return mBaseWindow->GetPosition(x, y);
}

/* void setSize (in long cx, in long cy, in boolean fRepaint); */
NS_IMETHODIMP WebBrowserChrome::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
    return mBaseWindow->SetSize(cx, cy, fRepaint);
}

/* void getSize (out long cx, out long cy); */
NS_IMETHODIMP WebBrowserChrome::GetSize(PRInt32 *cx, PRInt32 *cy)
{
    return mBaseWindow->GetSize(cx, cy);
}

/* void setPositionAndSize (in long x, in long y, in long cx, in long cy, in boolean fRepaint); */
NS_IMETHODIMP WebBrowserChrome::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
    return mBaseWindow->SetPositionAndSize(x, y, cx, cy, fRepaint);
}

/* void getPositionAndSize (out long x, out long y, out long cx, out long cy); */
NS_IMETHODIMP WebBrowserChrome::GetPositionAndSize(PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
    return mBaseWindow->GetPositionAndSize(x, y, cx, cy);
}

/* void setFocus (); */
NS_IMETHODIMP WebBrowserChrome::SetFocus()
{
   return mBaseWindow->SetFocus();
}

/* attribute wstring title; */
NS_IMETHODIMP WebBrowserChrome::GetTitle(PRUnichar * *aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);

   *aTitle = nsnull;
   
   return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebBrowserChrome::SetTitle(const PRUnichar * aTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nativeSiteWindow siteWindow */
NS_IMETHODIMP WebBrowserChrome::GetSiteWindow(void * *aSiteWindow)
{
   NS_ENSURE_ARG_POINTER(aSiteWindow);

   *aSiteWindow = mNativeWindow;
   return NS_OK;
}


//*****************************************************************************
// WebBrowserChrome::nsIPrompt
//*****************************************************************************   

/* void alert (in wstring dialogTitle, in wstring text); */
NS_IMETHODIMP WebBrowserChrome::Alert(const PRUnichar *dialogTitle, const PRUnichar *text)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void alertCheck (in wstring dialogTitle, in wstring text, in wstring checkMsg, out boolean checkValue); */
NS_IMETHODIMP WebBrowserChrome::AlertCheck(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *checkMsg, PRBool *checkValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean confirm (in wstring dialogTitle, in wstring text); */
NS_IMETHODIMP WebBrowserChrome::Confirm(const PRUnichar *dialogTitle, const PRUnichar *text, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean confirmCheck (in wstring dialogTitle, in wstring text, in wstring checkMsg, out boolean checkValue); */
NS_IMETHODIMP WebBrowserChrome::ConfirmCheck(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean prompt (in wstring dialogTitle, in wstring text, in wstring passwordRealm, in PRUint32 savePassword, in wstring defaultText, out wstring result); */
NS_IMETHODIMP WebBrowserChrome::Prompt(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *passwordRealm, PRUint32 savePassword, const PRUnichar *defaultText, PRUnichar **result, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean promptUsernameAndPassword (in wstring dialogTitle, in wstring text, in wstring passwordRealm, in PRUint32 savePassword, out wstring user, out wstring pwd); */
NS_IMETHODIMP WebBrowserChrome::PromptUsernameAndPassword(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *passwordRealm, PRUint32 savePassword, PRUnichar **user, PRUnichar **pwd, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean promptPassword (in wstring dialogTitle, in wstring text, in wstring passwordRealm, in PRUint32 savePassword, out wstring pwd); */
NS_IMETHODIMP WebBrowserChrome::PromptPassword(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *passwordRealm, PRUint32 savePassword, PRUnichar **pwd, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean select (in wstring dialogTitle, in wstring text, in PRUint32 count, [array, size_is (count)] in wstring selectList, out long outSelection); */
NS_IMETHODIMP WebBrowserChrome::Select(const PRUnichar *dialogTitle, const PRUnichar *text, PRUint32 count, const PRUnichar **selectList, PRInt32 *outSelection, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void universalDialog (in wstring titleMessage, in wstring dialogTitle, in wstring text, in wstring checkboxMsg, in wstring button0Text, in wstring button1Text, in wstring button2Text, in wstring button3Text, in wstring editfield1Msg, in wstring editfield2Msg, inout wstring editfield1Value, inout wstring editfield2Value, in wstring iconURL, inout boolean checkboxState, in PRInt32 numberButtons, in PRInt32 numberEditfields, in PRInt32 editField1Password, out PRInt32 buttonPressed); */
NS_IMETHODIMP WebBrowserChrome::UniversalDialog(const PRUnichar *titleMessage, const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *checkboxMsg, const PRUnichar *button0Text, const PRUnichar *button1Text, const PRUnichar *button2Text, const PRUnichar *button3Text, const PRUnichar *editfield1Msg, const PRUnichar *editfield2Msg, PRUnichar **editfield1Value, PRUnichar **editfield2Value, const PRUnichar *iconURL, PRBool *checkboxState, PRInt32 numberButtons, PRInt32 numberEditfields, PRInt32 editField1Password, PRInt32 *buttonPressed)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
