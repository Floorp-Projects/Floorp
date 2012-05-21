/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWebBrowser_h__
#define nsWebBrowser_h__

// Local Includes
#include "nsDocShellTreeOwner.h"

// Core Includes
#include "nsCOMPtr.h"

// Interfaces needed
#include "nsCWebBrowser.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScrollable.h"
#include "nsISHistory.h"
#include "nsITextScroll.h"
#include "nsIWidget.h"
#include "nsIWebProgress.h"
#include "nsISecureBrowserUI.h"
#include "nsIWebBrowser.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowserSetup.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserStream.h"
#include "nsIWindowWatcher.h"
#include "nsIPrintSettings.h"
#include "nsEmbedStream.h"

#include "nsTArray.h"
#include "nsWeakPtr.h"

class nsIContentViewerFile;

class nsWebBrowserInitInfo
{
public:
   //nsIBaseWindow Stuff
   PRInt32                 x;
   PRInt32                 y;
   PRInt32                 cx;
   PRInt32                 cy;
   bool                    visible;
   nsCOMPtr<nsISHistory>   sessionHistory;
   nsString                name;
};

class nsWebBrowserListenerState
{
public:
    bool Equals(nsIWeakReference *aListener, const nsIID& aID) {
        if (mWeakPtr.get() == aListener && mID.Equals(aID)) return true;
        return false;
    }

    nsWeakPtr mWeakPtr;
    nsIID mID;
};

//  {F1EAC761-87E9-11d3-AF80-00A024FFC08C} - 
#define NS_WEBBROWSER_CID \
{0xf1eac761, 0x87e9, 0x11d3, { 0xaf, 0x80, 0x00, 0xa0, 0x24, 0xff, 0xc0, 0x8c }}


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
                     public nsIWebProgressListener,
                     public nsIWebBrowserStream,
                     public nsSupportsWeakReference
{
friend class nsDocShellTreeOwner;
public:
    nsWebBrowser();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIBASEWINDOW
    NS_DECL_NSIDOCSHELLTREEITEM
    NS_DECL_NSIDOCSHELLTREENODE
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSISCROLLABLE   
    NS_DECL_NSITEXTSCROLL
    NS_DECL_NSIWEBBROWSER
    NS_DECL_NSIWEBNAVIGATION
    NS_DECL_NSIWEBBROWSERSETUP
    NS_DECL_NSIWEBBROWSERPERSIST
    NS_DECL_NSICANCELABLE
    NS_DECL_NSIWEBBROWSERFOCUS
    NS_DECL_NSIWEBBROWSERSTREAM
    NS_DECL_NSIWEBPROGRESSLISTENER

protected:
    virtual ~nsWebBrowser();
    NS_IMETHOD InternalDestroy();

    // XXXbz why are these NS_IMETHOD?  They're not interface methods!
    NS_IMETHOD SetDocShell(nsIDocShell* aDocShell);
    NS_IMETHOD EnsureDocShellTreeOwner();
    NS_IMETHOD GetPrimaryContentWindow(nsIDOMWindow **aDomWindow);
    NS_IMETHOD BindListener(nsISupports *aListener, const nsIID& aIID);
    NS_IMETHOD UnBindListener(nsISupports *aListener, const nsIID& aIID);
    NS_IMETHOD EnableGlobalHistory(bool aEnable);

    static nsEventStatus HandleEvent(nsGUIEvent *aEvent);

protected:
   nsDocShellTreeOwner*       mDocShellTreeOwner;
   nsCOMPtr<nsIDocShell>      mDocShell;
   nsCOMPtr<nsIInterfaceRequestor> mDocShellAsReq;
   nsCOMPtr<nsIBaseWindow>    mDocShellAsWin;
   nsCOMPtr<nsIDocShellTreeItem> mDocShellAsItem;
   nsCOMPtr<nsIWebNavigation> mDocShellAsNav;
   nsCOMPtr<nsIScrollable>    mDocShellAsScrollable;
   nsCOMPtr<nsITextScroll>    mDocShellAsTextScroll;
   nsCOMPtr<nsIWidget>        mInternalWidget;
   nsCOMPtr<nsIWindowWatcher> mWWatch;
   nsWebBrowserInitInfo*      mInitInfo;
   PRUint32                   mContentType;
   bool                       mActivating;
   bool                       mShouldEnableHistory;
   bool                       mIsActive;
   nativeWindow               mParentNativeWindow;
   nsIWebProgressListener    *mProgressListener;
   nsCOMPtr<nsIWebProgress>      mWebProgress;

   nsCOMPtr<nsIPrintSettings> mPrintSettings;

   // cached background color
   nscolor                       mBackgroundColor;

   // persistence object
   nsCOMPtr<nsIWebBrowserPersist> mPersist;
   PRUint32                       mPersistCurrentState;
   PRUint32                       mPersistResult;
   PRUint32                       mPersistFlags;

   // stream
   nsEmbedStream                 *mStream;
   nsCOMPtr<nsISupports>          mStreamGuard;

   //Weak Reference interfaces...
   nsIWidget*                            mParentWidget;
   nsTArray<nsWebBrowserListenerState*>* mListenerArray;
};

#endif /* nsWebBrowser_h__ */


