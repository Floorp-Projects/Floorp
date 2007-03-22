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
