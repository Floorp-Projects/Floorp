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

#include <windows.h> // for cheesy nsIPrompt implementation

// Local Includes

#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInterfaceRequestor.h"
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsCWebBrowser.h"
#include "nsWidgetsCID.h"
#include "nsXPIDLString.h"
#include "resource.h"
 

#include "winEmbed.h"
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
   NS_INTERFACE_MAP_ENTRY(nsISHistoryListener)
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
  *aChromeMask = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::SetChromeFlags(PRUint32 aChromeMask)
{
  if (mUI) {
    NS_ERROR("cheesy implementation can't change chrome flags on the fly");
    return NS_ERROR_FAILURE;
  }
  mChromeFlags = aChromeMask;
  return NS_OK;
}

nsresult WebBrowserChrome::CreateBrowser(PRInt32 aX, PRInt32 aY,
                                         PRInt32 aCX, PRInt32 aCY,
                                         nsIWebBrowser **aBrowser)
{
    NS_ENSURE_ARG_POINTER(aBrowser);
    *aBrowser = nsnull;

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

    // The window has been created. Now register for history notifications
    mWebBrowser->AddWebBrowserListener(supports, NS_GET_IID(nsISHistoryListener));

    if (mWebBrowser) {
      *aBrowser = mWebBrowser;
      NS_ADDREF(*aBrowser);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP WebBrowserChrome::CreateBrowserWindow(PRUint32 aChromeFlags,
                                  PRInt32 aX, PRInt32 aY,
                                  PRInt32 aCX, PRInt32 aCY,
                                  nsIWebBrowser **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  WebBrowserChrome *newChrome;
  WebBrowserChrome *parent = aChromeFlags & nsIWebBrowserChrome::CHROME_DEPENDENT ? this : nsnull;
  nsresult          rv;

  rv = ::CreateBrowserWindow(aChromeFlags, parent, &newChrome);

  if (NS_SUCCEEDED(rv))
    newChrome->GetWebBrowser(_retval);
  return rv;
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
// WebBrowserChrome::nsISHistoryListener
//*****************************************************************************   

NS_IMETHODIMP
WebBrowserChrome::OnHistoryNewEntry(nsIURI * aNewURI)
{
  return SendHistoryStatusMessage(aNewURI, "add");
}

NS_IMETHODIMP
WebBrowserChrome::OnHistoryGoBack(nsIURI * aBackURI, PRBool * aContinue)
{
  // For now, let the operation continue
  *aContinue = PR_TRUE;
  return SendHistoryStatusMessage(aBackURI, "back");
}


NS_IMETHODIMP
WebBrowserChrome::OnHistoryGoForward(nsIURI * aForwardURI, PRBool * aContinue)
{
  // For now, let the operation continue
  *aContinue = PR_TRUE;
  return SendHistoryStatusMessage(aForwardURI, "forward");
}


NS_IMETHODIMP
WebBrowserChrome::OnHistoryGotoIndex(PRInt32 aIndex, nsIURI * aGotoURI, PRBool * aContinue)
{
  // For now, let the operation continue
  *aContinue = PR_TRUE;
  return SendHistoryStatusMessage(aGotoURI, "goto", aIndex);
}

NS_IMETHODIMP
WebBrowserChrome::OnHistoryReload(nsIURI * aURI, PRUint32 aReloadFlags, PRBool * aContinue)
{
  // For now, let the operation continue
  *aContinue = PR_TRUE;
  return SendHistoryStatusMessage(aURI, "reload", 0 /* no info to pass here */, aReloadFlags);
}

NS_IMETHODIMP
WebBrowserChrome::OnHistoryPurge(PRInt32 aNumEntries, PRBool *aContinue)
{
  // For now let the operation continue
  *aContinue = PR_FALSE;
  return SendHistoryStatusMessage(nsnull, "purge", aNumEntries);
}

nsresult
WebBrowserChrome::SendHistoryStatusMessage(nsIURI * aURI, char * operation, PRInt32 info1, PRUint32 aReloadFlags)
{
  nsXPIDLCString uriCStr;
  if (aURI)
    aURI->GetSpec(getter_Copies(uriCStr));

  nsString uriAStr;

  if(!(nsCRT::strcmp(operation, "back"))) {
    // Going back. XXX Get string from a resource file
    uriAStr.Append(NS_ConvertASCIItoUCS2("Going back to url:"));
    uriAStr.Append(NS_ConvertASCIItoUCS2(uriCStr));
  }
  else if (!(nsCRT::strcmp(operation, "forward"))) {
    // Going forward. XXX Get string from a resource file
    uriAStr.Append(NS_ConvertASCIItoUCS2("Going forward to url:"));
    uriAStr.Append(NS_ConvertASCIItoUCS2(uriCStr));
  }
  else if (!(nsCRT::strcmp(operation, "reload"))) {
    // Reloading. XXX Get string from a resource file
    if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY && 
        aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE)
    {
      uriAStr.Append(NS_ConvertASCIItoUCS2("Reloading url,(bypassing proxy and cache) :"));
    }
    else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY)
    {
      uriAStr.Append(NS_ConvertASCIItoUCS2("Reloading url, (bypassing proxy):"));
    }
    else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE)
    {
      uriAStr.Append(NS_ConvertASCIItoUCS2("Reloading url, (bypassing cache):"));
    }
    else
    {
      uriAStr.Append(NS_ConvertASCIItoUCS2("Reloading url, (normal):"));
    }
    uriAStr.Append(NS_ConvertASCIItoUCS2(uriCStr));
  }
  else if (!(nsCRT::strcmp(operation, "add"))) {
    // Adding new entry. XXX Get string from a resource file
    uriAStr.Append(NS_ConvertASCIItoUCS2(uriCStr));
    uriAStr.Append(NS_ConvertASCIItoUCS2(" added to session History"));
  }
  else if (!(nsCRT::strcmp(operation, "goto"))) {
    // Goto. XXX Get string from a resource file
    uriAStr.Append(NS_ConvertASCIItoUCS2("Going to HistoryIndex:"));
    uriAStr.AppendInt(info1);
    uriAStr.Append(NS_ConvertASCIItoUCS2(" Url:"));
    uriAStr.Append(NS_ConvertASCIItoUCS2(uriCStr));
  }
  else if (!(nsCRT::strcmp(operation, "purge"))) {
    // Purging old entries
    uriAStr.AppendInt(info1);
    uriAStr.Append(NS_ConvertASCIItoUCS2(" purged from Session History"));
  }

  PRUnichar * uriStr = nsnull;
  uriStr = uriAStr.ToNewUnicode();
  if (mUI)
    mUI->UpdateStatusBarText(this, uriStr);
  nsCRT::free(uriStr);

  return NS_OK;


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
/* Simple, cheesy, partial implementation of nsIPrompt.
A real app would want better. */

NS_IMETHODIMP WebBrowserChrome::Alert(const PRUnichar* dialogTitle, const PRUnichar *text)
{
  nsAutoString stext(text);
  nsAutoString stitle(dialogTitle);
  char *ctext = stext.ToNewCString();
  char *ctitle = stitle.ToNewCString();
  ::MessageBox((HWND)mNativeWindow, ctext, ctitle, MB_OK | MB_ICONEXCLAMATION);
  nsMemory::Free(ctitle);
  nsMemory::Free(ctext);
  return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::AlertCheck(const PRUnichar* dialogTitle, const PRUnichar *text, const PRUnichar *checkMsg, PRBool *checkValue)
{
  nsAutoString stext(text);
  nsAutoString stitle(dialogTitle);
  char *ctext = stext.ToNewCString();
  char *ctitle = stitle.ToNewCString();
  ::MessageBox((HWND)mNativeWindow, ctext, ctitle, MB_OK | MB_ICONEXCLAMATION);
  *checkValue = PR_FALSE; // yeah, well, it's not a real implementation
  delete ctitle;
  delete ctext;
  return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::Confirm(const PRUnichar* dialogTitle, const PRUnichar *text, PRBool *_retval)
{
  nsAutoString stext(text);
  nsAutoString stitle(dialogTitle);
  char *ctext = stext.ToNewCString();
  char *ctitle = stitle.ToNewCString();
  int answer = ::MessageBox((HWND)mNativeWindow, ctext, ctitle, MB_YESNO | MB_ICONQUESTION);
  delete ctitle;
  delete ctext;
  *_retval = answer == IDYES ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::ConfirmCheck(const PRUnichar* dialogTitle, const PRUnichar *text, const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)
{
  nsAutoString stext(text);
  nsAutoString stitle(dialogTitle);
  char *ctext = stext.ToNewCString();
  char *ctitle = stitle.ToNewCString();
  int answer = ::MessageBox((HWND)mNativeWindow, ctext, ctitle, MB_YESNO | MB_ICONQUESTION);
  delete ctitle;
  delete ctext;
  *_retval = answer == IDYES ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::Prompt(const PRUnichar* dialogTitle, const PRUnichar *text, const PRUnichar* passwordRealm, PRUint32 savePassword, const PRUnichar *defaultText, PRUnichar **result, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::PromptUsernameAndPassword(const PRUnichar* dialogTitle, const PRUnichar *text, const PRUnichar* passwordRealm, PRUint32 savePassword, PRUnichar **user, PRUnichar **pwd, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::PromptPassword(const PRUnichar* dialogTitle, const PRUnichar *text, const PRUnichar* passwordRealm, PRUint32 savePassword, PRUnichar **pwd, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::Select(const PRUnichar *inDialogTitle, const PRUnichar *inMsg, PRUint32 inCount, const PRUnichar **inList, PRInt32 *outSelection, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::UniversalDialog(const PRUnichar *titleMessage, const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *checkboxMsg, const PRUnichar *button0Text, const PRUnichar *button1Text, const PRUnichar *button2Text, const PRUnichar *button3Text, const PRUnichar *editfield1Msg, const PRUnichar *editfield2Msg, PRUnichar **editfield1Value, PRUnichar **editfield2Value, const PRUnichar *iconURL, PRBool *checkboxState, PRInt32 numberButtons, PRInt32 numberEditfields, PRInt32 editField1Password, PRInt32 *buttonPressed)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
