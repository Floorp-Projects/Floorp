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
 * Contributor(s): Radha Kulkarni (radha@netscape.com)
 *
 * ***** END LICENSE BLOCK ***** */

// Mozilla Includes
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsCWebBrowser.h"
#include "nsWidgetsCID.h"
#include "nsIProfileChangeStatus.h"
#include "nsCRT.h"
#include "nsWeakReference.h"

// Local includes
#include "resource.h"
#include "mozEmbed.h"
#include "nsQABrowserChrome.h"


nsQABrowserChrome::nsQABrowserChrome()
{
    mNativeWindow = nsnull;
    mSizeSet = PR_FALSE;
}

nsQABrowserChrome::~nsQABrowserChrome()
{
  if (mBrowserUIGlue)
    mBrowserUIGlue->Destroyed(this);
}


//*****************************************************************************
// nsQABrowserChrome::nsISupports
//*****************************************************************************   


NS_IMPL_ISUPPORTS7(nsQABrowserChrome, 
                   nsIQABrowserChrome, 
                   nsIWebBrowserChrome,
                   nsIInterfaceRequestor,
                   nsIEmbeddingSiteWindow,
                   nsIWebProgressListener,
                   nsIWebBrowserChromeFocus,
                   nsISupportsWeakReference);



//*****************************************************************************
// nsQABrowserChrome::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsQABrowserChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
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
// nsQABrowserChrome::nsIQABrowserChrome
//*****************************************************************************   

NS_IMETHODIMP
nsQABrowserChrome::InitQAChrome(nsIQABrowserUIGlue * aUIGlue,  nativeWindow aNativeWnd)
{
  mBrowserUIGlue = aUIGlue;
  mNativeWindow = aNativeWnd;
  
  return NS_OK;
}

//*****************************************************************************
// nsQABrowserChrome::nsIQABrowserChrome
//*****************************************************************************   

NS_IMETHODIMP nsQABrowserChrome::SetStatus(PRUint32 aType, const PRUnichar* aStatus)
{
  if (mBrowserUIGlue)
    return mBrowserUIGlue->UpdateStatusBarText(this, aStatus);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsQABrowserChrome::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
    NS_ENSURE_ARG_POINTER(aWebBrowser);
    *aWebBrowser = mWebBrowser;
    NS_IF_ADDREF(*aWebBrowser);
    return NS_OK;
}

NS_IMETHODIMP nsQABrowserChrome::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
    mWebBrowser = aWebBrowser;
    return NS_OK;
}

NS_IMETHODIMP nsQABrowserChrome::GetChromeFlags(PRUint32* aChromeMask)
{
  if (aChromeMask)
    *aChromeMask = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP nsQABrowserChrome::SetChromeFlags(PRUint32 aChromeMask)
{
    mChromeFlags = aChromeMask;
    return NS_OK;
}

NS_IMETHODIMP nsQABrowserChrome::DestroyBrowserWindow(void)
{
  if (mBrowserUIGlue)
    return mBrowserUIGlue->Destroy(this);
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsQABrowserChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
  /* This isn't exactly correct: we're setting the whole window to
     the size requested for the browser. At time of writing, though,
     it's fine and useful for mozEmbed's purposes. 
  */
  mSizeSet = PR_TRUE;
  if (mBrowserUIGlue)
    return mBrowserUIGlue->SizeTo(this, aCX, aCY);
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsQABrowserChrome::ShowAsModal(void)
{
/*
  if (mDependentParent)
    AppCallbacks::EnableChromeWindow(mDependentParent, PR_FALSE);

  mContinueModalLoop = PR_TRUE;
  AppCallbacks::RunEventLoop(mContinueModalLoop);

  if (mDependentParent)
    AppCallbacks::EnableChromeWindow(mDependentParent, PR_TRUE);
*/
  return NS_OK;
}

NS_IMETHODIMP nsQABrowserChrome::IsWindowModal(PRBool *aReturn)
{
  if (aReturn)
    *aReturn = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsQABrowserChrome::ExitModalEventLoop(nsresult aStatus)
{
  mContinueModalLoop = PR_FALSE;
  return NS_OK;
}

//*****************************************************************************
// nsQABrowserChrome::nsIEmbeddingSiteWindow
//*****************************************************************************   

NS_IMETHODIMP nsQABrowserChrome::SetDimensions(PRUint32 aFlags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsQABrowserChrome::GetDimensions(PRUint32 aFlags, PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
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
NS_IMETHODIMP nsQABrowserChrome::SetFocus()
{
  if (mBrowserUIGlue)
    return mBrowserUIGlue->SetFocus(this);
  return NS_ERROR_FAILURE;
}

/* attribute wstring title; */
NS_IMETHODIMP nsQABrowserChrome::GetTitle(PRUnichar * *aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);
   *aTitle = nsnull;   
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsQABrowserChrome::SetTitle(const PRUnichar * aTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean visibility; */
NS_IMETHODIMP nsQABrowserChrome::GetVisibility(PRBool * aVisibility)
{
    NS_ENSURE_ARG_POINTER(aVisibility);
    *aVisibility = PR_TRUE;
    return NS_OK;
}
NS_IMETHODIMP nsQABrowserChrome::SetVisibility(PRBool aVisibility)
{
    return NS_OK;
}

/* attribute nativeSiteWindow siteWindow */
NS_IMETHODIMP nsQABrowserChrome::GetSiteWindow(void * *aSiteWindow)
{
   NS_ENSURE_ARG_POINTER(aSiteWindow);
   *aSiteWindow = mNativeWindow;
   return NS_OK;
}


//*****************************************************************************
// nsQABrowserChrome::nsIWebProgressListener
//*****************************************************************************   
NS_IMETHODIMP
nsQABrowserChrome::OnProgressChange(nsIWebProgress *progress, nsIRequest *request,
                                    PRInt32 curSelfProgress, PRInt32 maxSelfProgress,
                                    PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
  if (mBrowserUIGlue)
    return mBrowserUIGlue->UpdateProgress(this, curTotalProgress, maxTotalProgress);
  return NS_OK;
}

NS_IMETHODIMP
nsQABrowserChrome::OnStateChange(nsIWebProgress *progress, nsIRequest *request,
                                               PRUint32 progressStateFlags, nsresult status)
{
  if (!mBrowserUIGlue)
    return NS_ERROR_FAILURE;

  if ((progressStateFlags & STATE_START) && (progressStateFlags & STATE_IS_DOCUMENT))
  {
    mBrowserUIGlue->UpdateBusyState(this, PR_TRUE);
  }

  if ((progressStateFlags & STATE_STOP) && (progressStateFlags & STATE_IS_DOCUMENT))
  {
    mBrowserUIGlue->UpdateBusyState(this, PR_FALSE);
    mBrowserUIGlue->UpdateProgress(this, 0, 100);
    mBrowserUIGlue->UpdateStatusBarText(this, nsnull);
    ContentFinishedLoading();
  }

  return NS_OK;
}


NS_IMETHODIMP
nsQABrowserChrome::OnLocationChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsIURI *location)
{
  if (!mBrowserUIGlue)
    return NS_ERROR_FAILURE;

  PRBool isSubFrameLoad = PR_FALSE; // Is this a subframe load
  if (aWebProgress) {
    nsCOMPtr<nsIDOMWindow>  domWindow;
    nsCOMPtr<nsIDOMWindow>  topDomWindow;
    aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
    if (domWindow) { // Get root domWindow
      domWindow->GetTop(getter_AddRefs(topDomWindow));
    }
    if (domWindow != topDomWindow)
      isSubFrameLoad = PR_TRUE;
  }
  if (!isSubFrameLoad)
    mBrowserUIGlue->UpdateCurrentURI(this);
  return NS_OK;
}

NS_IMETHODIMP 
nsQABrowserChrome::OnStatusChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 nsresult aStatus,
                                 const PRUnichar* aMessage)
{
  if (mBrowserUIGlue)
    return mBrowserUIGlue->UpdateStatusBarText(this, aMessage);
  return NS_OK;
}



NS_IMETHODIMP 
nsQABrowserChrome::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRUint32 state)
{
    return NS_OK;
}
//*****************************************************************************
// nsQABrowserChrome::nsInsQABrowserChromeFocus
//*****************************************************************************   

NS_IMETHODIMP
nsQABrowserChrome::FocusNextElement()
{
    return NS_OK;
}

NS_IMETHODIMP
nsQABrowserChrome::FocusPrevElement()
{
    return NS_OK;
}



void
nsQABrowserChrome::ContentFinishedLoading()
{
  // if it was a chrome window and no one has already specified a size,
  // size to content
  if (mWebBrowser && !mSizeSet && mBrowserUIGlue &&
     (mChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)) {
    nsCOMPtr<nsIDOMWindow> contentWin;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(contentWin));
    if (contentWin)
        contentWin->SizeToContent();
    mBrowserUIGlue->ShowWindow(this, PR_TRUE);
  }
}













