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

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserChromeFocus.h"

#include "nsIBaseWindow.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgressListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebBrowser.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsISHistoryListener.h"
#include "nsIContextMenuListener.h"
#include "nsITooltipListener.h"

class WebBrowserChromeUI
{
public:
    static nativeWindow CreateNativeWindow(nsIWebBrowserChrome* chrome);
    static void Destroy(nsIWebBrowserChrome* chrome);
    static void Destroyed(nsIWebBrowserChrome* chrome);
    static void SetFocus(nsIWebBrowserChrome *chrome);
    static void UpdateStatusBarText(nsIWebBrowserChrome *aChrome, const PRUnichar* aStatusText);
    static void UpdateCurrentURI(nsIWebBrowserChrome *aChrome);
    static void UpdateBusyState(nsIWebBrowserChrome *aChrome, PRBool aBusy);
    static void UpdateProgress(nsIWebBrowserChrome *aChrome, PRInt32 aCurrent, PRInt32 aMax);
    static void GetResourceStringById(PRInt32 aID, char ** aReturn);
    static void ShowContextMenu(nsIWebBrowserChrome *aChrome, PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode);
    static void ShowTooltip(nsIWebBrowserChrome *aChrome, PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText);
    static void HideTooltip(nsIWebBrowserChrome *aChrome);
    static void ShowWindow(nsIWebBrowserChrome *aChrome, PRBool aShow);
    static void SizeTo(nsIWebBrowserChrome *aChrome, PRInt32 aWidth, PRInt32 aHeight);
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

    nsresult CreateBrowser(PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY,
                           nsIWebBrowser **aBrowser);

    void     SetParent(nsIWebBrowserChrome *aParent)
               { mDependentParent = aParent; }
   
protected:
    nsresult SendHistoryStatusMessage(nsIURI * aURI, char * operation, PRInt32 info1=0, PRUint32 info2=0);

    void ContentFinishedLoading();

    nativeWindow mNativeWindow;
    PRUint32     mChromeFlags;
    PRBool       mContinueModalLoop;
    PRBool       mSizeSet;

    nsCOMPtr<nsIWebBrowser> mWebBrowser;
    nsCOMPtr<nsIWebBrowserChrome> mDependentParent; // opener (for dependent windows only)
};

#endif /* __WebBrowserChrome__ */
