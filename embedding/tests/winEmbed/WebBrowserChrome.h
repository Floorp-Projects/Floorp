/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __WebBrowserChrome__
#define __WebBrowserChrome__

#include "nsCOMPtr.h"
#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsIWebBrowserChrome.h"

#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerFile.h"
#include "nsIBaseWindow.h"
#include "nsIWebBrowserSiteWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgressListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIPrompt.h"
#include "nsIWebBrowser.h"
#include "nsVoidArray.h"
#include "nsWeakReference.h"
#include "nsISHistoryListener.h"

class WebBrowserChromeUI
{
public:
    virtual nativeWindow CreateNativeWindow(nsIWebBrowserChrome* chrome) = 0;
    virtual void UpdateStatusBarText(nsIWebBrowserChrome *aChrome, const PRUnichar* aStatusText) = 0;
    virtual void UpdateCurrentURI(nsIWebBrowserChrome *aChrome) = 0;
    virtual void UpdateBusyState(nsIWebBrowserChrome *aChrome, PRBool aBusy) = 0;
    virtual void UpdateProgress(nsIWebBrowserChrome *aChrome, PRInt32 aCurrent, PRInt32 aMax) = 0;
    virtual void GetResourceStringById(PRInt32 aID, char ** aReturn) =0;
};

#define NS_DECL_WEBBROWSERCHROMEUI \
  public: \
    virtual nativeWindow CreateNativeWindow(nsIWebBrowserChrome* chrome); \
    virtual void UpdateStatusBarText(nsIWebBrowserChrome *aChrome, const PRUnichar* aStatusText); \
    virtual void UpdateCurrentURI(nsIWebBrowserChrome *aChrome); \
    virtual void UpdateBusyState(nsIWebBrowserChrome *aChrome, PRBool aBusy); \
    virtual void UpdateProgress(nsIWebBrowserChrome *aChrome, PRInt32 aCurrent, PRInt32 aMax); \
    virtual void GetResourceStringById(PRInt32 aID, char ** aReturn);

class WebBrowserChrome   : public nsIWebBrowserChrome,
                           public nsIWebProgressListener,
                           public nsIWebBrowserSiteWindow,
                           public nsIPrompt,
                           public nsIInterfaceRequestor,
                           public nsISHistoryListener,
                           public nsSupportsWeakReference
{
public:
    WebBrowserChrome();
    virtual ~WebBrowserChrome();

    void SetUI(WebBrowserChromeUI *aUI);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIWEBBROWSERSITEWINDOW
    NS_DECL_NSIPROMPT
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSISHISTORYLISTENER

    nsresult CreateBrowser(PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY,
                           nsIWebBrowser **aBrowser);
   
protected:
   nsresult SendHistoryStatusMessage(nsIURI * aURI, char * operation, PRInt32 info1=0, PRUint32 info2=0);

   nativeWindow mNativeWindow;
   PRUint32     mChromeFlags;
   
   WebBrowserChromeUI *mUI;
   nsCOMPtr<nsIWebBrowser> mWebBrowser;
   nsCOMPtr<nsIBaseWindow> mBaseWindow;
   nsCOMPtr<nsIWebBrowserChrome> mTopWindow;
    
   static nsVoidArray sBrowserList;
};

#endif /* __WebBrowserChrome__ */
