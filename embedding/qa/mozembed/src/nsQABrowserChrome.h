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

#ifndef __nsQABrowserChrome__
#define __nsQABrowserChrome__

#include "nsCOMPtr.h"
#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsIWebBrowserChrome.h"
#include "nsIQABrowserChrome.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowser.h"
#include "nsWeakReference.h"
#include "nsIWeakReference.h"
#include "nsIQABrowserView.h"
#include "nsIQABrowserUIGlue.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserChromeFocus.h"

/*
#include "nsIContentViewer.h"
#include "nsIContentViewerFile.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserver.h"
#include "nsISHistoryListener.h"
#include "nsIContextMenuListener.h"
#include "nsITooltipListener.h"
*/

class nsQABrowserChrome   : public nsIWebBrowserChrome,
                            public nsIEmbeddingSiteWindow,
                            public nsIInterfaceRequestor,
                            public nsIQABrowserChrome,
                            public nsSupportsWeakReference,
                            public nsIWebProgressListener,
                            public nsIWebBrowserChromeFocus
{
public:
    nsQABrowserChrome();
    virtual ~nsQABrowserChrome();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIQABROWSERCHROME
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIWEBPROGRESSLISTENER

protected:

    void ContentFinishedLoading();

    nativeWindow mNativeWindow;
    PRUint32     mChromeFlags;
    PRBool       mContinueModalLoop;
    PRBool       mSizeSet;

    nsCOMPtr<nsIWebBrowser> mWebBrowser;
    nsCOMPtr<nsIQABrowserUIGlue> mBrowserUIGlue;    
    nsCOMPtr<nsIWebBrowserChrome> mDependentParent; // opener (for dependent windows only)
    
};

#endif /* __nsQABrowserChrome__ */
