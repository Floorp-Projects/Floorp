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
 *   Adam Lock <adamlock@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Mozilla Includes
//#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIURIContentListener.h"
#include "nsCWebBrowser.h"
#include "nsCRT.h"

#include "GeckoContainer.h"



GeckoContainer::GeckoContainer(GeckoContainerUI *pUI, const char *aRole) :
    mUI(pUI),
    mNativeWindow(nsnull),
    mSizeSet(PR_FALSE),
    mIsChromeContainer(PR_FALSE),
    mIsURIContentListener(PR_TRUE)
{
    NS_ASSERTION(mUI, "No GeckoContainerUI! How do you expect this to work???");
    if (aRole)
        mRole = aRole;
}

GeckoContainer::~GeckoContainer()
{
    mUI->Destroyed();
}

nsresult GeckoContainer::CreateBrowser(PRInt32 aX, PRInt32 aY,
                                         PRInt32 aCX, PRInt32 aCY, nativeWindow aParent,
                                         nsIWebBrowser **aBrowser)
{
    NS_ENSURE_ARG_POINTER(aBrowser);
    *aBrowser = nsnull;

    nsresult rv;
    mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
    if (!mWebBrowser || NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    mWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome *, this));

    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(mWebBrowser);
    dsti->SetItemType(
        mIsChromeContainer ?
            nsIDocShellTreeItem::typeChromeWrapper :
            nsIDocShellTreeItem::typeContentWrapper);

    nsCOMPtr<nsIBaseWindow> browserBaseWindow = do_QueryInterface(mWebBrowser);

    mNativeWindow = aParent;

    if (!mNativeWindow)
        return NS_ERROR_FAILURE;

    browserBaseWindow->InitWindow( mNativeWindow,
                             nsnull, 
                             aX, aY, aCX, aCY);
    browserBaseWindow->Create();

    nsCOMPtr<nsIWebProgressListener> listener(NS_STATIC_CAST(nsIWebProgressListener*, this));
    nsCOMPtr<nsIWeakReference> thisListener(do_GetWeakReference(listener));
    (void)mWebBrowser->AddWebBrowserListener(thisListener, 
        NS_GET_IID(nsIWebProgressListener));

    if (mIsURIContentListener)
        mWebBrowser->SetParentURIContentListener(NS_STATIC_CAST(nsIURIContentListener *, this));


    // The window has been created. Now register for history notifications
//    mWebBrowser->AddWebBrowserListener(thisListener, NS_GET_IID(nsISHistoryListener));

    if (mWebBrowser)
    {
      *aBrowser = mWebBrowser;
      NS_ADDREF(*aBrowser);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
}


void GeckoContainer::ContentFinishedLoading()
{
  // if it was a chrome window and no one has already specified a size,
  // size to content
  if (mWebBrowser && !mSizeSet &&
     (mChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)) {
    nsCOMPtr<nsIDOMWindow> contentWin;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(contentWin));
    if (contentWin)
        contentWin->SizeToContent();
    mUI->ShowWindow(PR_TRUE);
  }
}

//*****************************************************************************
// GeckoContainer::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(GeckoContainer)
NS_IMPL_RELEASE(GeckoContainer)

NS_INTERFACE_MAP_BEGIN(GeckoContainer)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
   NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
   NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow2)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener) // optional
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIContextMenuListener2)
   NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
   NS_INTERFACE_MAP_ENTRY(nsIGeckoContainer)
NS_INTERFACE_MAP_END


//*****************************************************************************
// GeckoContainer::nsIGeckoContainer
//*****************************************************************************   

NS_IMETHODIMP GeckoContainer::GetRole(nsACString &aRole)
{
    aRole = mRole;
    return NS_OK;
}

NS_IMETHODIMP GeckoContainer::GetContainerUI(GeckoContainerUI **pUI)
{
    *pUI = mUI;
    return NS_OK;
}

//*****************************************************************************
// GeckoContainer::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP GeckoContainer::GetInterface(const nsIID &aIID, void** aInstancePtr)
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
// GeckoContainer::nsIWebBrowserChrome
//*****************************************************************************   

NS_IMETHODIMP GeckoContainer::SetStatus(PRUint32 aType, const PRUnichar* aStatus)
{
    mUI->UpdateStatusBarText(aStatus);
    return NS_OK;
}

NS_IMETHODIMP GeckoContainer::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
    NS_ENSURE_ARG_POINTER(aWebBrowser);
    *aWebBrowser = mWebBrowser;
    NS_IF_ADDREF(*aWebBrowser);
    return NS_OK;
}

NS_IMETHODIMP GeckoContainer::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
    mWebBrowser = aWebBrowser;
    return NS_OK;
}

NS_IMETHODIMP GeckoContainer::GetChromeFlags(PRUint32* aChromeMask)
{
    *aChromeMask = mChromeFlags;
    return NS_OK;
}

NS_IMETHODIMP GeckoContainer::SetChromeFlags(PRUint32 aChromeMask)
{
    mChromeFlags = aChromeMask;
    return NS_OK;
}

NS_IMETHODIMP GeckoContainer::DestroyBrowserWindow(void)
{
    mUI->Destroy();
    return NS_OK;
}


// IN: The desired browser client area dimensions.
NS_IMETHODIMP GeckoContainer::SizeBrowserTo(PRInt32 aWidth, PRInt32 aHeight)
{
  /* This isn't exactly correct: we're setting the whole window to
     the size requested for the browser. At time of writing, though,
     it's fine and useful for winEmbed's purposes. */
  mUI->SizeTo(aWidth, aHeight);
  mSizeSet = PR_TRUE;
  return NS_OK;
}


NS_IMETHODIMP GeckoContainer::ShowAsModal(void)
{
/*  if (mDependentParent)
    mUI->EnableChromeWindow(mDependentParent, PR_FALSE);

  mContinueModalLoop = PR_TRUE;
  mUI->RunEventLoop(mContinueModalLoop);

  if (mDependentParent)
    mUI->EnableChromeWindow(mDependentParent, PR_TRUE);
*/
  return NS_OK;
}

NS_IMETHODIMP GeckoContainer::IsWindowModal(PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GeckoContainer::ExitModalEventLoop(nsresult aStatus)
{
  mContinueModalLoop = PR_FALSE;
  return NS_OK;
}

//*****************************************************************************
// GeckoContainer::nsIWebBrowserChromeFocus
//*****************************************************************************

NS_IMETHODIMP GeckoContainer::FocusNextElement()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GeckoContainer::FocusPrevElement()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//*****************************************************************************
// GeckoContainer::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP GeckoContainer::OnProgressChange(nsIWebProgress *progress, nsIRequest *request,
                                                  PRInt32 curSelfProgress, PRInt32 maxSelfProgress,
                                                  PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
    mUI->UpdateProgress(curTotalProgress, maxTotalProgress);
    return NS_OK;
}

NS_IMETHODIMP GeckoContainer::OnStateChange(nsIWebProgress *progress, nsIRequest *request,
                                               PRUint32 progressStateFlags, nsresult status)
{
    if ((progressStateFlags & STATE_START) && (progressStateFlags & STATE_IS_DOCUMENT))
    {
        mUI->UpdateBusyState(PR_TRUE);
    }

    if ((progressStateFlags & STATE_STOP) && (progressStateFlags & STATE_IS_DOCUMENT))
    {
        mUI->UpdateBusyState(PR_FALSE);
        mUI->UpdateProgress(0, 100);
        mUI->UpdateStatusBarText(nsnull);
        ContentFinishedLoading();
    }

    return NS_OK;
}


NS_IMETHODIMP GeckoContainer::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI *location)
{
    PRBool isSubFrameLoad = PR_FALSE; // Is this a subframe load
    if (aWebProgress)
    {
        nsCOMPtr<nsIDOMWindow>  domWindow;
        nsCOMPtr<nsIDOMWindow>  topDomWindow;
        aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
        if (domWindow)
        {
            // Get root domWindow
            domWindow->GetTop(getter_AddRefs(topDomWindow));
        }
        if (domWindow != topDomWindow)
            isSubFrameLoad = PR_TRUE;
    }
    if (!isSubFrameLoad)
        mUI->UpdateCurrentURI();
    return NS_OK;
}

NS_IMETHODIMP 
GeckoContainer::OnStatusChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 nsresult aStatus,
                                 const PRUnichar* aMessage)
{
    mUI->UpdateStatusBarText(aMessage);
    return NS_OK;
}



NS_IMETHODIMP 
GeckoContainer::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRUint32 state)
{
    return NS_OK;
}


//*****************************************************************************
// GeckoContainer::nsIEmbeddingSiteWindow
//*****************************************************************************   

NS_IMETHODIMP GeckoContainer::SetDimensions(PRUint32 aFlags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GeckoContainer::GetDimensions(PRUint32 aFlags, PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
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
NS_IMETHODIMP GeckoContainer::SetFocus()
{
    mUI->SetFocus();
    return NS_OK;
}

/* attribute wstring title; */
NS_IMETHODIMP GeckoContainer::GetTitle(PRUnichar * *aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);

   *aTitle = nsnull;
   
   return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP GeckoContainer::SetTitle(const PRUnichar * aTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean visibility; */
NS_IMETHODIMP GeckoContainer::GetVisibility(PRBool * aVisibility)
{
    NS_ENSURE_ARG_POINTER(aVisibility);
    *aVisibility = PR_TRUE;
    return NS_OK;
}
NS_IMETHODIMP GeckoContainer::SetVisibility(PRBool aVisibility)
{
    return NS_OK;
}

/* attribute nativeSiteWindow siteWindow */
NS_IMETHODIMP GeckoContainer::GetSiteWindow(void * *aSiteWindow)
{
   NS_ENSURE_ARG_POINTER(aSiteWindow);

   *aSiteWindow = mNativeWindow;
   return NS_OK;
}

//*****************************************************************************
// GeckoContainer::nsIEmbeddingSiteWindow2
//*****************************************************************************   

NS_IMETHODIMP GeckoContainer::Blur()
{
   mUI->KillFocus();
   return NS_OK;
}

//*****************************************************************************
// GeckoContainer::nsIObserver
//*****************************************************************************   

NS_IMETHODIMP GeckoContainer::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
    nsresult rv = NS_OK;
    if (nsCRT::strcmp(aTopic, "profile-change-teardown") == 0)
    {
        // A profile change means death for this window
        mUI->Destroy();
    }
    return rv;
}

//*****************************************************************************
// GeckoContainer::nsIURIContentListener
//*****************************************************************************   

/* boolean onStartURIOpen (in nsIURI aURI); */
NS_IMETHODIMP GeckoContainer::OnStartURIOpen(nsIURI *aURI, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean doContent (in string aContentType, in boolean aIsContentPreferred, in nsIRequest aRequest, out nsIStreamListener aContentHandler); */
NS_IMETHODIMP GeckoContainer::DoContent(const char *aContentType, PRBool aIsContentPreferred, nsIRequest *aRequest, nsIStreamListener **aContentHandler, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isPreferred (in string aContentType, out string aDesiredContentType); */
NS_IMETHODIMP GeckoContainer::IsPreferred(const char *aContentType, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean canHandleContent (in string aContentType, in boolean aIsContentPreferred, out string aDesiredContentType); */
NS_IMETHODIMP GeckoContainer::CanHandleContent(const char *aContentType, PRBool aIsContentPreferred, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsISupports loadCookie; */
NS_IMETHODIMP GeckoContainer::GetLoadCookie(nsISupports * *aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP GeckoContainer::SetLoadCookie(nsISupports * aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIURIContentListener parentContentListener; */
NS_IMETHODIMP GeckoContainer::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP GeckoContainer::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//*****************************************************************************
// GeckoContainer::nsIContextMenuListener2
//*****************************************************************************   

/* void onShowContextMenu (in unsigned long aContextFlags, in nsIContextMenuInfo aUtils); */
NS_IMETHODIMP GeckoContainer::OnShowContextMenu(PRUint32 aContextFlags, nsIContextMenuInfo *aContextMenuInfo)
{
    mUI->ShowContextMenu(aContextFlags, aContextMenuInfo);
    return NS_OK;
}


//*****************************************************************************
// GeckoContainer::nsITooltipListener
//*****************************************************************************   

/* void OnShowTooltip (in long aXCoords, in long aYCoords, in wstring aTipText); */
NS_IMETHODIMP GeckoContainer::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
    mUI->ShowTooltip(aXCoords, aYCoords, aTipText);
    return NS_OK;
}

/* void OnHideTooltip (); */
NS_IMETHODIMP GeckoContainer::OnHideTooltip()
{
    mUI->HideTooltip();
    return NS_OK;
}
