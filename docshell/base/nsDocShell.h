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
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef nsDocShell_h__
#define nsDocShell_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIParser.h"  // for nsCharSetSource
#include "nsIPresShell.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIContentViewer.h"
#include "nsIPref.h"
#include "nsVoidArray.h"

#include "nsCDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIContentViewerContainer.h"
#include "nsIDeviceContext.h"

#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderObserver.h"

#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIInterfaceRequestor.h"

#include "nsPoint.h" // mCurrent/mDefaultScrollbarPreferences


// Local Includes
#include "nsDSURIContentListener.h"
#include "nsDSWebProgressListener.h"

// Interfaces Needed
#include "nsISupportsArray.h"
#include "nsISHistory.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"


class nsDocShellInitInfo
{
public:
   //nsIGenericWindow Stuff
   PRInt32        x;
   PRInt32        y;
   PRInt32        cx;
   PRInt32        cy;
};

class nsDocShell : public nsIDocShell,
                   public nsIDocShellTreeItem, 
                   public nsIDocShellTreeNode,
                   public nsIWebNavigation,
                   public nsIWebProgress,
                   public nsIBaseWindow, 
                   public nsIScrollable, 
                   public nsITextScroll, 
                   public nsIContentViewerContainer,
                   public nsIInterfaceRequestor,
                   public nsIScriptGlobalObjectOwner
{
friend class nsDSURIContentListener;
friend class nsDSWebProgressListener;

public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIDOCSHELL
   NS_DECL_NSIDOCSHELLTREEITEM
   NS_DECL_NSIDOCSHELLTREENODE
   NS_DECL_NSIWEBNAVIGATION
   NS_DECL_NSIWEBPROGRESS
   NS_DECL_NSIBASEWINDOW
   NS_DECL_NSISCROLLABLE
   NS_DECL_NSITEXTSCROLL
   NS_DECL_NSIINTERFACEREQUESTOR
   NS_DECL_NSISCRIPTGLOBALOBJECTOWNER

   // XXX: move to a macro
   // nsIContentViewerContainer
   NS_IMETHOD Embed(nsIContentViewer* aDocViewer, 
                   const char* aCommand,
                   nsISupports* aExtraInfo);


   static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void** ppv);

protected:
   // Object Management
   nsDocShell();
   virtual ~nsDocShell();
   NS_IMETHOD DestroyChildren();

   // Content Viewer Management
   NS_IMETHOD EnsureContentViewer();
   NS_IMETHOD EnsureDeviceContext();
   NS_IMETHOD CreateAboutBlankContentViewer();
   NS_IMETHOD CreateContentViewer(const char* aContentType, 
      nsIChannel* aOpenedChannel, nsIStreamListener** aContentHandler);
   NS_IMETHOD NewContentViewerObj(const char* aContentType, 
      nsIChannel* aOpenedChannel, nsILoadGroup* aLoadGroup, 
      nsIStreamListener** aContentHandler, nsIContentViewer** aViewer);
   NS_IMETHOD SetupNewViewer(nsIContentViewer* aNewViewer);

   // Site Loading
   typedef enum
   {
      loadNormal,          // Normal Load
      loadNormalReplace,   // Normal Load but replaces current history slot
      loadHistory,         // Load from history
      loadReloadNormal,    // Reload
      loadReloadBypassCache,
      loadReloadBypassProxy,
      loadRelaodBypassProxyAndCache,
      loadLink
   } loadType;

   NS_IMETHOD InternalLoad(nsIURI* aURI, nsIURI* aReferrerURI,
      const char* aWindowTarget=nsnull, nsIInputStream* aPostData=nsnull, 
      loadType aLoadType=loadNormal);
   NS_IMETHOD CreateFixupURI(const PRUnichar* aStringURI, nsIURI** aURI);
   NS_IMETHOD FileURIFixup(const PRUnichar* aStringURI, nsIURI** aURI);
   NS_IMETHOD ConvertFileToStringURI(nsString& aIn, nsString& aOut);
   NS_IMETHOD ConvertStringURIToFileCharset(nsString& aIn, nsCString& aOut);
   NS_IMETHOD KeywordURIFixup(const PRUnichar* aStringURI, nsIURI** aURI);
   NS_IMETHOD DoURILoad(nsIURI* aURI, nsIURI* aReferrer, 
      nsURILoadCommand aLoadCmd, const char* aWindowTarget, 
      nsIInputStream* aPostData);
   NS_IMETHOD StopCurrentLoads();
   NS_IMETHOD ScrollIfAnchor(nsIURI* aURI, PRBool* aWasAnchor);
   NS_IMETHOD OnLoadingSite(nsIChannel* aChannel);
   virtual void SetCurrentURI(nsIURI* aURI);
   virtual void SetReferrerURI(nsIURI* aURI);

   // Session History
   NS_IMETHOD ShouldAddToSessionHistory(nsIURI* aURI, PRBool* aShouldAdd);
   NS_IMETHOD ShouldPersistInSessionHistory(nsIURI* aURI, PRBool* aShouldPersist);
   NS_IMETHOD AddToSessionHistory(nsIURI* aURI);
   NS_IMETHOD UpdateCurrentSessionHistory();
   NS_IMETHOD LoadHistoryEntry(nsISHEntry* aEntry);

   // Global History
   NS_IMETHOD ShouldAddToGlobalHistory(nsIURI* aURI, PRBool* aShouldAdd);
   NS_IMETHOD AddToGlobalHistory(nsIURI* aURI);
   NS_IMETHOD UpdateCurrentGlobalHistory();

   // WebProgressListener Management
   NS_IMETHOD EnsureWebProgressListener();

   NS_IMETHOD FireOnProgressChange(nsIChannel* aChannel, 
      PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, 
      PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress);
   NS_IMETHOD FireOnChildProgressChange(nsIChannel* aChannel,
      PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress);
   NS_IMETHOD FireOnStatusChange(nsIChannel* aChannel, PRInt32 aProgressStatusFlags);
   NS_IMETHOD FireOnChildStatusChange(nsIChannel* aChannel, 
      PRInt32 aProgressStatusFlags);
   NS_IMETHOD FireOnLocationChange(nsIURI* aURI);

   // Helper Routines
   nsDocShellInitInfo* InitInfo();
   NS_IMETHOD GetChildOffset(nsIDOMNode* aChild, nsIDOMNode* aParent, 
      PRInt32* aOffset);
   NS_IMETHOD GetRootScrollableView(nsIScrollableView** aOutScrollView);
   NS_IMETHOD EnsureContentListener();
   NS_IMETHOD EnsureScriptEnvironment();


   NS_IMETHOD FireStartDocumentLoad(nsIDocumentLoader* aLoader,
                                    nsIURI* aURL,
                                    const char* aCommand);

   NS_IMETHOD FireEndDocumentLoad(nsIDocumentLoader* aLoader,
                                  nsIChannel* aChannel,
                                  nsresult aStatus);

   NS_IMETHOD InsertDocumentInDocTree();

   PRBool IsFrame();

protected:
   nsString                   mName;
   nsString                   mTitle;
   nsVoidArray                mChildren;
   nsDSURIContentListener*    mContentListener;
   nsDSWebProgressListener*   mWebProgressListener;
   nsDocShellInitInfo*        mInitInfo;
   nsCOMPtr<nsIContentViewer> mContentViewer;
   nsCOMPtr<nsIDeviceContext> mDeviceContext;
   nsCOMPtr<nsIDocumentLoader>mDocLoader;
   nsCOMPtr<nsIDocumentLoaderObserver> mDocLoaderObserver;
   nsCOMPtr<nsIWidget>        mParentWidget;
   nsCOMPtr<nsIPref>          mPrefs;
   nsCOMPtr<nsIURI>           mCurrentURI;
   nsCOMPtr<nsIURI>           mReferrerURI;
   nsCOMPtr<nsIScriptGlobalObject> mScriptGlobal;
   nsCOMPtr<nsIScriptContext> mScriptContext;
   nsCOMPtr<nsISHistory>      mSessionHistory;
   nsCOMPtr<nsISupportsArray> mWebProgressListenerList;
   nsCOMPtr<nsISupports>      mLoadCookie; // the load cookie associated with the window context.
   PRInt32                    mMarginWidth;
   PRInt32                    mMarginHeight;
   PRInt32                    mItemType;
   nsPoint                    mCurrentScrollbarPref; // this document only
   nsPoint                    mDefaultScrollbarPref; // persistent across doc loads
   loadType                   mLoadType;
   PRBool                     mInitialPageLoad;
   PRBool                     mAllowPlugins;
   PRInt32                    mViewMode;
   // this flag is for bug #21358. a docshell may load many urls
   // which don't result in new documents being created (i.e. a new content viewer)
   // we want to make sure we don't call a on load event more than once for a given
   // content viewer. 
   PRBool                     mEODForCurrentDocument; 

   /* WEAK REFERENCES BELOW HERE.
   Note these are intentionally not addrefd.  Doing so will create a cycle.
   For that reasons don't use nsCOMPtr.*/
   nsIDocShellTreeItem*       mParent;  // Weak Reference
   nsIDocShellTreeOwner*      mTreeOwner; // Weak Reference
   nsIWebProgressListener*    mOwnerProgressListener; // Weak Reference
   nsIChromeEventHandler*     mChromeEventHandler; //Weak Reference
};

#endif /* nsDocShell_h__ */
