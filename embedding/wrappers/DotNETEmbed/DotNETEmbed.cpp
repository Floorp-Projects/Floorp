/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <vcclr.h>
#include "nsCOMPtr.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIBaseWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWindowWatcher.h"
#include "nsIInputStream.h"
#include "nsEmbedAPI.h"

#include "DotNETEmbed.h"
#include "umWebChrome.h"

using namespace Mozilla::Embedding;

// Silly class for holding on to a static UTF8Encoding so that we
// don't need to create a new one in every call to CopyString()...
public __gc class UTF8EncodingHolder
{
public:
  static Text::UTF8Encoding *sUTF8Encoding = new Text::UTF8Encoding();
};

String *
Mozilla::Embedding::CopyString(const nsAFlatCString& aStr)
{
  return new String(aStr.get(), 0, aStr.Length(),
                    UTF8EncodingHolder::sUTF8Encoding);
}

// In stead of copying String's to nsAString's we could write a class
// that wraps a String object and exposes it's underlying
// buffer. Doable, if we pin the String object, and so on, but this'll
// do for now.
nsAFlatString&
Mozilla::Embedding::CopyString(String *aSrc, nsAFlatString& aDest)
{
  const wchar_t __pin * strbuf = PtrToStringChars(aSrc);

  aDest.Assign(strbuf, aSrc->Length);

  return aDest;
}

nsAFlatCString&
Mozilla::Embedding::CopyString(String *aSrc, nsAFlatCString& aDest)
{
  const wchar_t __pin * strbuf = PtrToStringChars(aSrc);

  CopyUTF16toUTF8(nsDependentString(strbuf, aSrc->Length), aDest);

  return aDest;
}

void
Mozilla::Embedding::ThrowIfFailed(nsresult rv)
{
  if (NS_FAILED(rv)) {
    // XXX: Throw some useful exception here!

    throw "rv is an error code!";
  }
}

#pragma unmanaged

#define NS_WEBBROWSER_CONTRACTID "@mozilla.org/embedding/browser/nsWebBrowser;1"
//*****************************************************************************
// WebBrowserChrome::nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(WebBrowserChrome)
NS_IMPL_RELEASE(WebBrowserChrome)

NS_INTERFACE_MAP_BEGIN(WebBrowserChrome)
 NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
 NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
 NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
NS_INTERFACE_MAP_END


WebBrowserChrome::WebBrowserChrome()
{
  mNativeWindow = nsnull;
}

WebBrowserChrome::~WebBrowserChrome()
{
}

/* attribute nativeSiteWindow siteWindow */
NS_IMETHODIMP
WebBrowserChrome::GetSiteWindow(void * *aSiteWindow)
{
  *aSiteWindow = mNativeWindow;
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::GetTitle(PRUnichar * *aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  *aTitle = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
WebBrowserChrome::SetTitle(const PRUnichar * aTitle)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean visibility; */
NS_IMETHODIMP
WebBrowserChrome::GetVisibility(PRBool * aVisibility)
{
  NS_ENSURE_ARG_POINTER(aVisibility);
  *aVisibility = PR_TRUE;
  return NS_OK;
}


NS_IMETHODIMP
WebBrowserChrome::SetVisibility(PRBool aVisibility)
{
  return NS_OK;
}

/* void setFocus (); */
NS_IMETHODIMP
WebBrowserChrome::SetFocus()
{
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::SetDimensions(PRUint32 aFlags, PRInt32 x, PRInt32 y,
                                PRInt32 cx, PRInt32 cy)
{
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::GetDimensions(PRUint32 aFlags, PRInt32 *x, PRInt32 *y,
                                PRInt32 *cx, PRInt32 *cy)
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

  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::ExitModalEventLoop(nsresult aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::ShowAsModal(void)
{
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::IsWindowModal(PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
WebBrowserChrome::SetStatus(PRUint32 aType, const PRUnichar* aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
  NS_ENSURE_ARG_POINTER(aWebBrowser);
  *aWebBrowser = mWebBrowser;
  NS_IF_ADDREF(*aWebBrowser);
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
  mWebBrowser = aWebBrowser;
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::GetChromeFlags(PRUint32* aChromeMask)
{
  *aChromeMask = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::SetChromeFlags(PRUint32 aChromeMask)
{
  mChromeFlags = aChromeMask;
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome::DestroyBrowserWindow(void)
{
  return NS_OK;
}


NS_IMETHODIMP
WebBrowserChrome::SizeBrowserTo(PRInt32 aWidth, PRInt32 aHeight)
{
  ::MoveWindow((HWND)mNativeWindow, 0, 0, aWidth, aHeight, TRUE);
  return NS_OK;
}

nsresult
WebBrowserChrome::CreateBrowser(HWND hWnd, PRInt32 aX, PRInt32 aY, PRInt32 aCX,
                                PRInt32 aCY, nsIWebBrowser **aBrowser)
{
  NS_ENSURE_ARG_POINTER(aBrowser);
  *aBrowser = nsnull;

  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);

  if (!mWebBrowser)
    return NS_ERROR_FAILURE;

  mWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome*, this));

  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(mWebBrowser);
  dsti->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

  nsCOMPtr<nsIBaseWindow> browserBaseWindow = do_QueryInterface(mWebBrowser);

  mNativeWindow = hWnd;

  if (!mNativeWindow)
    return NS_ERROR_FAILURE;

  browserBaseWindow->InitWindow(mNativeWindow, nsnull, aX, aY, aCX, aCY);
  browserBaseWindow->Create();

  if (mWebBrowser)
  {
    *aBrowser = mWebBrowser;
    NS_ADDREF(*aBrowser);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

#pragma managed


Gecko::Gecko()
  : mChrome(nsnull)
{
}

void
Gecko::TermEmbedding()
{
  if (!sIsInitialized) {
    return;
  }

  sIsInitialized = false;

  nsresult rv = NS_TermEmbedding();
  ThrowIfFailed(rv);
}

void
Gecko::OpenURL(String *aUrl)
{
  if (!sIsInitialized) {
    nsresult rv = NS_InitEmbedding(nsnull, nsnull);
    ThrowIfFailed(rv);

    sIsInitialized = true;
  }

  const wchar_t __pin * url = PtrToStringChars(aUrl);
  nsresult rv;

  HWND hWnd = (HWND)Handle.ToPointer();

  if (!mChrome) {
    CreateBrowserWindow(nsIWebBrowserChrome::CHROME_ALL, nsnull);
  }

  // Start loading a page
  nsCOMPtr<nsIWebBrowser> newBrowser;
  mChrome->GetWebBrowser(getter_AddRefs(newBrowser));
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(newBrowser));

  rv = webNav->LoadURI(url, nsIWebNavigation::LOAD_FLAGS_NONE, nsnull, nsnull,
                       nsnull);

  ThrowIfFailed(rv);
}

void
Gecko::OnResize(EventArgs *e)
{
  if (mChrome) {
    nsCOMPtr<nsIEmbeddingSiteWindow> embeddingSite =
      do_QueryInterface(mChrome);

    RECT rect;
    GetClientRect((HWND)Handle.ToPointer(), &rect);

    // Make sure the browser is visible and sized
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mChrome->GetWebBrowser(getter_AddRefs(webBrowser));
    nsCOMPtr<nsIBaseWindow> webBrowserAsWin = do_QueryInterface(webBrowser);
    if (webBrowserAsWin) {
      webBrowserAsWin->SetPositionAndSize(rect.left,
                                          rect.top,
                                          rect.right - rect.left,
                                          rect.bottom - rect.top,
                                          PR_TRUE);
      webBrowserAsWin->SetVisibility(PR_TRUE);
    }
  }

  UserControl::OnResize(e);
}

void
Gecko::CreateBrowserWindow(PRUint32 aChromeFlags, nsIWebBrowserChrome *aParent)
{
  WebBrowserChrome * chrome = new WebBrowserChrome();
  if (!chrome) {
    throw new OutOfMemoryException();
  }

  mChrome = chrome;
  NS_ADDREF(mChrome);

  // XXX: Is this really needed?
  // now an extra addref; the window owns itself (to be released by
  // WebBrowserChromeUI::Destroy)
  NS_ADDREF(mChrome);

  chrome->SetChromeFlags(aChromeFlags);

  HWND hWnd = (HWND)Handle.ToPointer();

  // Insert the browser
  nsCOMPtr<nsIWebBrowser> newBrowser;
  chrome->CreateBrowser(hWnd, -1, -1, -1, -1, getter_AddRefs(newBrowser));
  if (!newBrowser) {
    ThrowIfFailed(NS_ERROR_FAILURE);
  }

  // Place it where we want it.
  OnResize(0);

  // if opened as chrome, it'll be made visible after the chrome has loaded.
  // otherwise, go ahead and show it now.
  if (!(aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME))
    ::ShowWindow(hWnd, SW_RESTORE);
}

