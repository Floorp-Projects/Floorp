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

#ifndef __WebBrowserChrome__
#define __WebBrowserChrome__

#include "nsCOMPtr.h"
#include "nsIGenericFactory.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserChromeFocus.h"

#include "nsIContentViewer.h"
#include "nsIContentViewerFile.h"
#include "nsIBaseWindow.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgressListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebBrowser.h"
#include "nsIURIContentListener.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIContextMenuListener.h"
#include "nsIContextMenuListener2.h"
#include "nsITooltipListener.h"

class GeckoContainerUI;

// Define a custom nsIGeckoContainer interface. It means that if we are passed
// some random nsIWebBrowserChrome object we can tell if its one of ours by
// QIing to see if it implements nsIGeckoContainer

#define NS_IGECKOCONTAINER_IID \
    { 0xbf47a2ec, 0x9be0, 0x4f18, { 0x9a, 0xf0, 0x8e, 0x1c, 0x89, 0x2a, 0xa3, 0x1d } }

class NS_NO_VTABLE nsIGeckoContainer : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IGECKOCONTAINER_IID)

    NS_IMETHOD GetRole(nsACString &aRole) = 0;
    NS_IMETHOD GetContainerUI(GeckoContainerUI **pUI) = 0;
};

#define NS_DECL_NSIGECKOCONTAINER \
    NS_IMETHOD GetRole(nsACString &aRole); \
    NS_IMETHOD GetContainerUI(GeckoContainerUI **pUI);

class GeckoContainer   :
    public nsIWebBrowserChrome,
    public nsIWebBrowserChromeFocus,
    public nsIWebProgressListener,
    public nsIEmbeddingSiteWindow2,
    public nsIInterfaceRequestor,
    public nsIObserver,
    public nsIContextMenuListener2,
    public nsITooltipListener,
    public nsIURIContentListener,
    public nsIGeckoContainer,
    public nsSupportsWeakReference
{
public:
    // Supply a container UI callback and optionally a role, e.g. "browser"
    // which can be used to modify the behaviour depending on the value
    // it contains.
    GeckoContainer(GeckoContainerUI *pUI, const char *aRole = NULL);
    virtual ~GeckoContainer();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIEMBEDDINGSITEWINDOW2
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIOBSERVER
    NS_DECL_NSICONTEXTMENULISTENER2
    NS_DECL_NSITOOLTIPLISTENER
    NS_DECL_NSIURICONTENTLISTENER
    NS_DECL_NSIGECKOCONTAINER

    nsresult CreateBrowser(PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, nativeWindow aParent,
                           nsIWebBrowser **aBrowser);

    void     SetParent(nsIWebBrowserChrome *aParent)
               { mDependentParent = aParent; }
   
protected:
    nsresult SendHistoryStatusMessage(nsIURI * aURI, char * operation, PRInt32 info1=0, PRUint32 info2=0);

    void ContentFinishedLoading();

    GeckoContainerUI *mUI;
    nativeWindow mNativeWindow;
    PRUint32     mChromeFlags;
    PRBool       mContinueModalLoop;
    PRBool       mSizeSet;
    PRBool       mIsChromeContainer;
    PRBool       mIsURIContentListener;

    nsCString    mRole;
    nsCOMPtr<nsIWebBrowser> mWebBrowser;
    nsCOMPtr<nsIWebBrowserChrome> mDependentParent; // opener (for dependent windows only)
};


class GeckoContainerUI
{
protected:
    PRBool mBusy;

public:
    GeckoContainerUI() :
        mBusy(PR_FALSE)
    {
    }
    // Called by the window creator when a new browser window is requested
    virtual nsresult CreateBrowserWindow(PRUint32 aChromeFlags,
         nsIWebBrowserChrome *aParent, nsIWebBrowserChrome **aNewWindow);
    // Called when the content wants to be destroyed (e.g. a window.close() happened)
    virtual void Destroy();
    // Called when the Gecko has been torn down, allowing dangling references to be released
    virtual void Destroyed();
    // Called when the content wants focus
    virtual void SetFocus();
    // Called when the content wants focus (e.g. onblur handler)
    virtual void KillFocus();
    // Called when the status bar text needs to be updated (e.g. progress notifications)
    virtual void UpdateStatusBarText(const PRUnichar* aStatusText);
    // Called when the current URI has changed. Allows UI to update address bar
    virtual void UpdateCurrentURI();
    // Called when the browser busy state changes. Allows UI to display an hourglass 
    virtual void UpdateBusyState(PRBool aBusy);
    // Called when total progress changes
    virtual void UpdateProgress(PRInt32 aCurrent, PRInt32 aMax);
    virtual void GetResourceStringById(PRInt32 aID, char ** aReturn);
    // Called when a context menu event has been detected on the page
    virtual void ShowContextMenu(PRUint32 aContextFlags, nsIContextMenuInfo *aContextMenuInfo);
    // Called when a tooltip should be shown
    virtual void ShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText);
    // Called when a tooltip should be hidden
    virtual void HideTooltip();
    // Called when the window should be hidden or shown
    virtual void ShowWindow(PRBool aShow);
    // Called when the browser area should be resized
    virtual void SizeTo(PRInt32 aWidth, PRInt32 aHeight);
    virtual void EnableChromeWindow(PRBool aEnabled);
    virtual PRUint32 RunEventLoop(PRBool &aRunCondition);
};

#endif /* __WebBrowserChrome__ */
