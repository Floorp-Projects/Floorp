/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef nsWebBrowser_h__
#define nsWebBrowser_h__

// Local Includes
#include "nsDocShellTreeOwner.h"
#include "nsWBURIContentListener.h"

// Core Includes
#include "nsCOMPtr.h"

// Interfaces needed
#include "nsCWebBrowser.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInterfaceRequestor.h"
#include "nsIScrollable.h"
#include "nsISHistory.h"
#include "nsITextScroll.h"
#include "nsIWidget.h"
#include "nsIWebProgress.h"

#include "nsIWebBrowser.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowserSetup.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserFind.h"

#include "nsVoidArray.h"
#include "nsWeakPtr.h"

class nsWebBrowserFindImpl;

class nsWebBrowserInitInfo
{
public:
   //nsIBaseWindow Stuff
   PRInt32                 x;
   PRInt32                 y;
   PRInt32                 cx;
   PRInt32                 cy;
   PRBool                  visible;
   nsCOMPtr<nsISHistory>   sessionHistory;
   nsString                name;
};

class nsWebBrowserListenerState
{
public:
    PRBool Equals(nsISupports *aListener, const nsIID& aID) {
        nsCOMPtr<nsISupports> listener = do_QueryReferent(mWeakPtr);
        if (listener.get() == aListener && mID.Equals(aID)) return PR_TRUE;
        return PR_FALSE;
    };

    nsWeakPtr mWeakPtr;
    nsIID mID;
};

class nsWebBrowser : public nsIWebBrowser,
                     public nsIWebNavigation,
                     public nsIWebBrowserSetup,
                     public nsIDocShellTreeItem,
                     public nsIBaseWindow,
                     public nsIScrollable, 
                     public nsITextScroll, 
                     public nsIInterfaceRequestor,
                     public nsIWebBrowserPersist,
                     public nsIWebBrowserFocus,
                     public nsIWebBrowserFind
{
friend class nsDocShellTreeOwner;
friend class nsWBURIContentListener;
public:
    nsWebBrowser();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIBASEWINDOW
    NS_DECL_NSIDOCSHELLTREEITEM
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSISCROLLABLE   
    NS_DECL_NSITEXTSCROLL
    NS_DECL_NSIWEBBROWSER
    NS_DECL_NSIWEBNAVIGATION
    NS_DECL_NSIWEBBROWSERSETUP
    NS_DECL_NSIWEBBROWSERPERSIST
    NS_DECL_NSIWEBBROWSERFOCUS
    NS_DECL_NSIWEBBROWSERFIND
    
protected:
    virtual ~nsWebBrowser();
    NS_IMETHOD InternalDestroy();

    NS_IMETHOD SetDocShell(nsIDocShell* aDocShell);
    NS_IMETHOD EnsureDocShellTreeOwner();
    NS_IMETHOD EnsureContentListener();
    NS_IMETHOD GetPrimaryContentWindow(nsIDOMWindowInternal **aDomWindow);
    NS_IMETHOD BindListener(nsISupports *aListener, const nsIID& aIID);
    NS_IMETHOD UnBindListener(nsISupports *aListener, const nsIID& aIID);
    NS_IMETHOD EnsureFindImpl();

    static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);

protected:
   nsDocShellTreeOwner*       mDocShellTreeOwner;
   nsWBURIContentListener*    mContentListener;
   nsCOMPtr<nsIDocShell>      mDocShell;
   nsCOMPtr<nsIInterfaceRequestor> mDocShellAsReq;
   nsCOMPtr<nsIBaseWindow>    mDocShellAsWin;
   nsCOMPtr<nsIDocShellTreeItem> mDocShellAsItem;
   nsCOMPtr<nsIWebNavigation> mDocShellAsNav;
   nsCOMPtr<nsIScrollable>    mDocShellAsScrollable;
   nsCOMPtr<nsITextScroll>    mDocShellAsTextScroll;
   nsCOMPtr<nsIWidget>        mInternalWidget;
   nsWebBrowserInitInfo*      mInitInfo;
   PRUint32                   mContentType;
   nativeWindow               mParentNativeWindow;
   nsIWebBrowserPersistProgress *mProgressListener;
   nsCOMPtr<nsIWebProgress>   mWebProgress;
   nsWebBrowserFindImpl*      mFindImpl;

   //Weak Reference interfaces...
   nsIWidget*                 mParentWidget;
   nsIDocShellTreeItem*       mParent; 
   nsVoidArray *              mListenerArray;
          
};

#endif /* nsWebBrowser_h__ */


