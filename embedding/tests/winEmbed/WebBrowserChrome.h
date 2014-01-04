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

#ifndef __WebBrowserChrome__
#define __WebBrowserChrome__

// OS headers

#include <windows.h>

// FROZEN APIs

#include "nsStringAPI.h"

#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserChromeFocus.h"

#include "nsIContextMenuListener.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsISHistoryListener.h"
#include "nsITooltipListener.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowser.h"

// GLUE APIs (not frozen, but safe because we're statically linking them)
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsWeakReference.h"

class WebBrowserChromeUI
{
public:
    static HWND CreateNativeWindow(nsIWebBrowserChrome* chrome);
    static void Destroy(nsIWebBrowserChrome* chrome);
    static void Destroyed(nsIWebBrowserChrome* chrome);
    static void SetFocus(nsIWebBrowserChrome *chrome);
    static void UpdateStatusBarText(nsIWebBrowserChrome *aChrome, const char16_t* aStatusText);
    static void UpdateCurrentURI(nsIWebBrowserChrome *aChrome);
    static void UpdateBusyState(nsIWebBrowserChrome *aChrome, bool aBusy);
    static void UpdateProgress(nsIWebBrowserChrome *aChrome, int32_t aCurrent, int32_t aMax);
    static void GetResourceStringById(int32_t aID, char ** aReturn);
    static void ShowContextMenu(nsIWebBrowserChrome *aChrome, uint32_t aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode);
    static void ShowTooltip(nsIWebBrowserChrome *aChrome, int32_t aXCoords, int32_t aYCoords, const char16_t *aTipText);
    static void HideTooltip(nsIWebBrowserChrome *aChrome);
    static void ShowWindow(nsIWebBrowserChrome *aChrome, bool aShow);
    static void SizeTo(nsIWebBrowserChrome *aChrome, int32_t aWidth, int32_t aHeight);
};

class WebBrowserChrome   : public nsIWebBrowserChrome,
                           public nsIWebBrowserChromeFocus,
                           public nsIWebProgressListener,
                           public nsIEmbeddingSiteWindow,
                           public nsIInterfaceRequestor,
                           public nsISHistoryListener,
                           public nsIObserver,
                           public nsIContextMenuListener,
                           public nsITooltipListener,
                           public nsSupportsWeakReference

{
public:
    WebBrowserChrome();
    virtual ~WebBrowserChrome();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSISHISTORYLISTENER
    NS_DECL_NSIOBSERVER
    NS_DECL_NSICONTEXTMENULISTENER
    NS_DECL_NSITOOLTIPLISTENER

    nsresult CreateBrowser(int32_t aX, int32_t aY, int32_t aCX, int32_t aCY,
                           nsIWebBrowser **aBrowser);

    void     SetParent(nsIWebBrowserChrome *aParent)
               { mDependentParent = aParent; }
   
protected:
    nsresult SendHistoryStatusMessage(nsIURI * aURI, char * operation, int32_t info1=0, uint32_t info2=0);

    void ContentFinishedLoading();

    HWND         mNativeWindow;
    uint32_t     mChromeFlags;
    bool         mContinueModalLoop;
    bool         mSizeSet;

    nsCOMPtr<nsIWebBrowser> mWebBrowser;
    nsCOMPtr<nsIWebBrowserChrome> mDependentParent; // opener (for dependent windows only)
};

#endif /* __WebBrowserChrome__ */
