/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

// Local includes
#include "resource.h"
#include "winEmbed.h"
#include "WebBrowserChrome.h"

// OS headers
#include <stdio.h>

// Frozen APIs

#include "nsStringAPI.h"
#include "nsIComponentManager.h"
#include "nsIDOMWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIRequest.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"
#include "nsCWebBrowser.h"
#include "nsIProfileChangeStatus.h"

// Glue APIs (not frozen, but safe to use because they are statically linked)
#include "nsComponentManagerUtils.h"

// NON-FROZEN APIS!
#include "nsIWebNavigation.h"

WebBrowserChrome::WebBrowserChrome()
{
    mNativeWindow = nsnull;
    mSizeSet = false;
}

WebBrowserChrome::~WebBrowserChrome()
{
    WebBrowserChromeUI::Destroyed(this);
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

    (void)mWebBrowser->SetContainerWindow(static_cast<nsIWebBrowserChrome*>(this));

    nsCOMPtr<nsIBaseWindow> browserBaseWindow = do_QueryInterface(mWebBrowser);

    mNativeWindow = WebBrowserChromeUI::CreateNativeWindow(static_cast<nsIWebBrowserChrome*>(this));

    if (!mNativeWindow)
        return NS_ERROR_FAILURE;

    browserBaseWindow->InitWindow( mNativeWindow,
                             nsnull, 
                             aX, aY, aCX, aCY);
    browserBaseWindow->Create();

    nsCOMPtr<nsIWebProgressListener> listener(static_cast<nsIWebProgressListener*>(this));
    nsCOMPtr<nsIWeakReference> thisListener(do_GetWeakReference(listener));
    (void)mWebBrowser->AddWebBrowserListener(thisListener, 
        NS_GET_IID(nsIWebProgressListener));

    // The window has been created. Now register for history notifications
    mWebBrowser->AddWebBrowserListener(thisListener, NS_GET_IID(nsISHistoryListener));

    if (mWebBrowser)
    {
      *aBrowser = mWebBrowser;
      NS_ADDREF(*aBrowser);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
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
   NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener) // optional
   NS_INTERFACE_MAP_ENTRY(nsISHistoryListener)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
   NS_INTERFACE_MAP_ENTRY(nsIContextMenuListener)
   NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
NS_INTERFACE_MAP_END

//*****************************************************************************
// WebBrowserChrome::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP WebBrowserChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);

    *aInstancePtr = 0;
    if (aIID.Equals(NS_GET_IID(nsIDOMWindow)))
    {
        if (mWebBrowser)
        {
            return mWebBrowser->GetContentDOMWindow((nsIDOMWindow **) aInstancePtr);
        }
        return NS_ERROR_NOT_INITIALIZED;
    }
    return QueryInterface(aIID, aInstancePtr);
}

//*****************************************************************************
// WebBrowserChrome::nsIWebBrowserChrome
//*****************************************************************************   

NS_IMETHODIMP WebBrowserChrome::SetStatus(PRUint32 aType, const PRUnichar* aStatus)
{
    WebBrowserChromeUI::UpdateStatusBarText(this, aStatus);
    return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
    NS_ENSURE_ARG_POINTER(aWebBrowser);
    *aWebBrowser = mWebBrowser;
    NS_IF_ADDREF(*aWebBrowser);
    return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
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
    mChromeFlags = aChromeMask;
    return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::DestroyBrowserWindow(void)
{
    WebBrowserChromeUI::Destroy(this);
    return NS_OK;
}


// IN: The desired browser client area dimensions.
NS_IMETHODIMP WebBrowserChrome::SizeBrowserTo(PRInt32 aWidth, PRInt32 aHeight)
{
  /* This isn't exactly correct: we're setting the whole window to
     the size requested for the browser. At time of writing, though,
     it's fine and useful for winEmbed's purposes. */
  WebBrowserChromeUI::SizeTo(this, aWidth, aHeight);
  mSizeSet = true;
  return NS_OK;
}


NS_IMETHODIMP WebBrowserChrome::ShowAsModal(void)
{
  if (mDependentParent)
    AppCallbacks::EnableChromeWindow(mDependentParent, false);

  mContinueModalLoop = true;
  AppCallbacks::RunEventLoop(mContinueModalLoop);

  if (mDependentParent)
    AppCallbacks::EnableChromeWindow(mDependentParent, true);

  return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::IsWindowModal(bool *_retval)
{
    *_retval = false;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::ExitModalEventLoop(nsresult aStatus)
{
  mContinueModalLoop = false;
  return NS_OK;
}

//*****************************************************************************
// WebBrowserChrome::nsIWebBrowserChromeFocus
//*****************************************************************************

NS_IMETHODIMP WebBrowserChrome::FocusNextElement()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::FocusPrevElement()
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
    WebBrowserChromeUI::UpdateProgress(this, curTotalProgress, maxTotalProgress);
    return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::OnStateChange(nsIWebProgress *progress, nsIRequest *request,
                                               PRUint32 progressStateFlags, nsresult status)
{
    if ((progressStateFlags & STATE_START) && (progressStateFlags & STATE_IS_DOCUMENT))
    {
        WebBrowserChromeUI::UpdateBusyState(this, true);
    }

    if ((progressStateFlags & STATE_STOP) && (progressStateFlags & STATE_IS_DOCUMENT))
    {
        WebBrowserChromeUI::UpdateBusyState(this, false);
        WebBrowserChromeUI::UpdateProgress(this, 0, 100);
        WebBrowserChromeUI::UpdateStatusBarText(this, nsnull);
        ContentFinishedLoading();
    }

    return NS_OK;
}


NS_IMETHODIMP WebBrowserChrome::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI *location,
                                                 PRUint32 aFlags)
{
  bool isSubFrameLoad = false; // Is this a subframe load
  if (aWebProgress) {
    nsCOMPtr<nsIDOMWindow>  domWindow;
    nsCOMPtr<nsIDOMWindow>  topDomWindow;
    aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
    if (domWindow) { // Get root domWindow
      domWindow->GetTop(getter_AddRefs(topDomWindow));
    }
    if (domWindow != topDomWindow)
      isSubFrameLoad = true;
  }
  if (!isSubFrameLoad)
    WebBrowserChromeUI::UpdateCurrentURI(this);
  return NS_OK;
}

NS_IMETHODIMP 
WebBrowserChrome::OnStatusChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 nsresult aStatus,
                                 const PRUnichar* aMessage)
{
    WebBrowserChromeUI::UpdateStatusBarText(this, aMessage);
    return NS_OK;
}



NS_IMETHODIMP 
WebBrowserChrome::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRUint32 state)
{
    return NS_OK;
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
WebBrowserChrome::OnHistoryGoBack(nsIURI * aBackURI, bool * aContinue)
{
    // For now, let the operation continue
    *aContinue = true;
    return SendHistoryStatusMessage(aBackURI, "back");
}


NS_IMETHODIMP
WebBrowserChrome::OnHistoryGoForward(nsIURI * aForwardURI, bool * aContinue)
{
    // For now, let the operation continue
    *aContinue = true;
    return SendHistoryStatusMessage(aForwardURI, "forward");
}


NS_IMETHODIMP
WebBrowserChrome::OnHistoryGotoIndex(PRInt32 aIndex, nsIURI * aGotoURI, bool * aContinue)
{
    // For now, let the operation continue
    *aContinue = true;
    return SendHistoryStatusMessage(aGotoURI, "goto", aIndex);
}

NS_IMETHODIMP
WebBrowserChrome::OnHistoryReload(nsIURI * aURI, PRUint32 aReloadFlags, bool * aContinue)
{
    // For now, let the operation continue
    *aContinue = true;
    return SendHistoryStatusMessage(aURI, "reload", 0 /* no info to pass here */, aReloadFlags);
}

NS_IMETHODIMP
WebBrowserChrome::OnHistoryPurge(PRInt32 aNumEntries, bool *aContinue)
{
    // For now let the operation continue
    *aContinue = false;
    return SendHistoryStatusMessage(nsnull, "purge", aNumEntries);
}

static void
AppendIntToCString(PRInt32 info1, nsCString& aResult)
{
  char intstr[10];
  _snprintf(intstr, sizeof(intstr) - 1, "%i", info1);
  intstr[sizeof(intstr) - 1] = '\0';
  aResult.Append(intstr);
}

nsresult
WebBrowserChrome::SendHistoryStatusMessage(nsIURI * aURI, char * operation, PRInt32 info1, PRUint32 aReloadFlags)
{
    nsCString uriSpec;
    if (aURI)
    {
        aURI->GetSpec(uriSpec);
    }

    nsCString status;

    if(!(strcmp(operation, "back")))
    {
        status.Assign("Going back to url: ");
        status.Append(uriSpec);
    }
    else if (!(strcmp(operation, "forward")))
    {
        // Going forward. XXX Get string from a resource file
        status.Assign("Going forward to url: ");
        status.Append(uriSpec);
    }
    else if (!(strcmp(operation, "reload")))
    {
        // Reloading. XXX Get string from a resource file
        if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY && 
            aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE)
        {
            status.Assign("Reloading url, (bypassing proxy and cache): ");
        }
        else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY)
        {
            status.Assign("Reloading url, (bypassing proxy): ");
        }
        else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE)
        {
            status.Assign("Reloading url, (bypassing cache): ");
        }
        else
        {
            status.Assign("Reloading url, (normal): ");
        }
        status.Append(uriSpec);
    }
    else if (!(strcmp(operation, "add")))
    {
        status.Assign(uriSpec);
        status.Append(" added to session History");
    }
    else if (!(strcmp(operation, "goto")))
    {
        status.Assign("Going to HistoryIndex: ");

	AppendIntToCString(info1, status);

        status.Append(" Url: ");
        status.Append(uriSpec);
    }
    else if (!(strcmp(operation, "purge")))
    {
        AppendIntToCString(info1, status);
        status.Append(" purged from Session History");
    }

    nsString wstatus;
    NS_CStringToUTF16(status, NS_CSTRING_ENCODING_UTF8, wstatus);
    WebBrowserChromeUI::UpdateStatusBarText(this, wstatus.get());

    return NS_OK;
}

void WebBrowserChrome::ContentFinishedLoading()
{
  // if it was a chrome window and no one has already specified a size,
  // size to content
  if (mWebBrowser && !mSizeSet &&
     (mChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)) {
    nsCOMPtr<nsIDOMWindow> contentWin;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(contentWin));
    if (contentWin)
        contentWin->SizeToContent();
    WebBrowserChromeUI::ShowWindow(this, true);
  }
}


//*****************************************************************************
// WebBrowserChrome::nsIEmbeddingSiteWindow
//*****************************************************************************   

NS_IMETHODIMP WebBrowserChrome::SetDimensions(PRUint32 aFlags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebBrowserChrome::GetDimensions(PRUint32 aFlags, PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION)
    {
        *x = 0;
        *y = 0;
    }
    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER ||
        aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)
    {
        *cx = 0;
        *cy = 0;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setFocus (); */
NS_IMETHODIMP WebBrowserChrome::SetFocus()
{
    WebBrowserChromeUI::SetFocus(this);
    return NS_OK;
}

/* void blur (); */
NS_IMETHODIMP WebBrowserChrome::Blur()
{
    return NS_ERROR_NOT_IMPLEMENTED;
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

/* attribute boolean visibility; */
NS_IMETHODIMP WebBrowserChrome::GetVisibility(bool * aVisibility)
{
    NS_ENSURE_ARG_POINTER(aVisibility);
    *aVisibility = true;
    return NS_OK;
}
NS_IMETHODIMP WebBrowserChrome::SetVisibility(bool aVisibility)
{
    return NS_OK;
}

/* attribute nativeSiteWindow siteWindow */
NS_IMETHODIMP WebBrowserChrome::GetSiteWindow(void * *aSiteWindow)
{
   NS_ENSURE_ARG_POINTER(aSiteWindow);

   *aSiteWindow = mNativeWindow;
   return NS_OK;
}


//*****************************************************************************
// WebBrowserChrome::nsIObserver
//*****************************************************************************   

NS_IMETHODIMP WebBrowserChrome::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
    nsresult rv = NS_OK;
    if (strcmp(aTopic, "profile-change-teardown") == 0)
    {
        // A profile change means death for this window
        WebBrowserChromeUI::Destroy(this);
    }
    return rv;
}

//*****************************************************************************
// WebBrowserChrome::nsIContextMenuListener
//*****************************************************************************   

/* void OnShowContextMenu (in unsigned long aContextFlags, in nsIDOMEvent aEvent, in nsIDOMNode aNode); */
NS_IMETHODIMP WebBrowserChrome::OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
    WebBrowserChromeUI::ShowContextMenu(this, aContextFlags, aEvent, aNode);
    return NS_OK;
}

//*****************************************************************************
// WebBrowserChrome::nsITooltipListener
//*****************************************************************************   

/* void OnShowTooltip (in long aXCoords, in long aYCoords, in wstring aTipText); */
NS_IMETHODIMP WebBrowserChrome::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
    WebBrowserChromeUI::ShowTooltip(this, aXCoords, aYCoords, aTipText);
    return NS_OK;
}

/* void OnHideTooltip (); */
NS_IMETHODIMP WebBrowserChrome::OnHideTooltip()
{
    WebBrowserChromeUI::HideTooltip(this);
    return NS_OK;
}
