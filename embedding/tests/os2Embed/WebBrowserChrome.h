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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __WebBrowserChrome__
#define __WebBrowserChrome__

#include "nsCOMPtr.h"
#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserChromeFocus.h"

#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerFile.h"
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
