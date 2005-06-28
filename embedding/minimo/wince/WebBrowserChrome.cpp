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

// Mozilla Includes
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIRequest.h"
#include "nsCWebBrowser.h"
#include "nsIProfileChangeStatus.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIContent.h"
#include "nsIWidget.h"
#include "WebBrowserChrome.h"
#include "nsIWebBrowserSetup.h"

WebBrowserChrome::WebBrowserChrome()
{
    mNativeWindow = nsnull;
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


     // Configure what the web browser can and cannot do
     nsCOMPtr<nsIWebBrowserSetup> webBrowserAsSetup(do_QueryInterface(mWebBrowser));
 
     webBrowserAsSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS, PR_TRUE);
     webBrowserAsSetup->SetProperty(nsIWebBrowserSetup::SETUP_IS_CHROME_WRAPPER, PR_TRUE);


    (void)mWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome*, this));

    nsCOMPtr<nsIBaseWindow> browserBaseWindow = do_QueryInterface(mWebBrowser);
    mNativeWindow = WebBrowserChromeUI::CreateNativeWindow(NS_STATIC_CAST(nsIWebBrowserChrome*, this));

    if (!mNativeWindow)
      return NS_ERROR_FAILURE;

    browserBaseWindow->InitWindow( mNativeWindow, nsnull, aX, aY, aCX, aCY);
    browserBaseWindow->Create();

    nsCOMPtr<nsIWidget> mozWidget;
    browserBaseWindow->GetMainWidget(getter_AddRefs(mozWidget));
    mNativeMozWindow = mozWidget->GetNativeData(NS_NATIVE_WINDOW);

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
  SetWindowPos((HWND)mNativeWindow, NULL, 0, 0, aWidth, aHeight, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
  return NS_OK;
}


NS_IMETHODIMP WebBrowserChrome::ShowAsModal(void)
{
  mContinueModalLoop = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::IsWindowModal(PRBool *_retval)
{
    *_retval = mContinueModalLoop;
    return NS_OK;
}

NS_IMETHODIMP WebBrowserChrome::ExitModalEventLoop(nsresult aStatus)
{
  mContinueModalLoop = PR_FALSE;
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
// WebBrowserChrome::nsIEmbeddingSiteWindow
//*****************************************************************************   

NS_IMETHODIMP WebBrowserChrome::SetDimensions(PRUint32 aFlags, PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY)
{
  nsCOMPtr<nsIBaseWindow> browserBaseWindow = do_QueryInterface(mWebBrowser);

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION && (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER | nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))) 
  {
    MoveWindow((HWND)mNativeWindow, aX, aY, aCX, aCY, PR_TRUE);
    return NS_OK;
  }
  else if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) 
  {
    RECT rect;
    GetWindowRect((HWND)mNativeWindow, &rect);
    MoveWindow((HWND)mNativeWindow, aX, aY, rect.right-rect.left, rect.bottom-rect.top, TRUE);
    return NS_OK;
  }
  else if (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
                     nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)) {
    RECT rect;
    GetWindowRect((HWND)mNativeWindow, &rect);
    MoveWindow((HWND)mNativeWindow, rect.left, rect.top, aCX, aCY, TRUE);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP WebBrowserChrome::GetDimensions(PRUint32 aFlags, PRInt32 *aX, PRInt32 *aY, PRInt32 *aCX, PRInt32 *aCY)
{
  nsCOMPtr<nsIBaseWindow> browserBaseWindow = do_QueryInterface(mWebBrowser);

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION &&
      (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
                 nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))) {
    return browserBaseWindow->GetPositionAndSize(aX, aY, aCX, aCY);
  }
  else if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    return browserBaseWindow->GetPosition(aX, aY);
  }
  else if (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
                     nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)) {
    return browserBaseWindow->GetSize(aCX, aCY);
  }
  return NS_ERROR_INVALID_ARG;
}

/* void setFocus (); */
NS_IMETHODIMP WebBrowserChrome::SetFocus()
{
    ::SetFocus((HWND)mNativeWindow);
    return NS_OK;
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
NS_IMETHODIMP WebBrowserChrome::GetVisibility(PRBool * aVisibility)
{
    NS_ENSURE_ARG_POINTER(aVisibility);
    *aVisibility = PR_TRUE;
    return NS_OK;
}
NS_IMETHODIMP WebBrowserChrome::SetVisibility(PRBool aVisibility)
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
