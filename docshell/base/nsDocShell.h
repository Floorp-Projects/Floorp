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

#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderObserver.h"

#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIInterfaceRequestor.h"

#include "nsDSURIContentListener.h"


class nsDocShellInitInfo
{
public:
   //nsIGenericWindow Stuff
   PRInt32        x;
   PRInt32        y;
   PRInt32        cx;
   PRInt32        cy;
   PRBool         visible;
};

class nsDocShell : public nsIDocShell,
                   public nsIDocShellTreeItem, 
                   public nsIDocShellTreeNode,
                   public nsIBaseWindow, 
                   public nsIScrollable, 
                   public nsITextScroll, 
                   public nsIContentViewerContainer,
                   public nsIInterfaceRequestor,
                   public nsIScriptGlobalObjectOwner
{
friend class nsDSURIContentListener;

public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIDOCSHELL
   NS_DECL_NSIDOCSHELLTREEITEM
   NS_DECL_NSIDOCSHELLTREENODE
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

   NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* aLoader,
                                      nsIChannel* channel,
                                      const char *aContentType,
                                      const char *aCommand);


   static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void** ppv);

protected:
   nsDocShell();
   virtual ~nsDocShell();

   nsDocShellInitInfo* InitInfo();
   NS_IMETHOD GetChildOffset(nsIDOMNode* aChild, nsIDOMNode* aParent, 
      PRInt32* aOffset);
   NS_IMETHOD GetRootScrollableView(nsIScrollableView** aOutScrollView);
   NS_IMETHOD EnsureContentListener();
   NS_IMETHOD EnsureScriptEnvironment();

   void SetCurrentURI(nsIURI* aUri);
   NS_IMETHOD CreateContentViewer(const char* aContentType, nsURILoadCommand aCommand,
      nsIChannel* aOpenedChannel, nsIStreamListener** aContentHandler);
   NS_IMETHOD NewContentViewerObj(const char* aContentType, nsURILoadCommand aCommand,
      nsIChannel* aOpenedChannel, nsIStreamListener** aContentHandler);

   NS_IMETHOD FireStartDocumentLoad(nsIDocumentLoader* aLoader,
                                    nsIURI* aURL,
                                    const char* aCommand);

   NS_IMETHOD FireEndDocumentLoad(nsIDocumentLoader* aLoader,
                                  nsIChannel* aChannel,
                                  nsresult aStatus,
                                  nsIDocumentLoaderObserver * aObserver);

   NS_IMETHOD InsertDocumentInDocTree();
   NS_IMETHOD DestroyChildren();

   NS_IMETHOD GetPrimaryFrameFor(nsIContent* content, nsIFrame** frame);

protected:
   PRBool                     mCreated;
   nsString                   mName;
   nsVoidArray                mChildren;
   nsDSURIContentListener*    mContentListener;
   nsDocShellInitInfo*        mInitInfo;
   nsCOMPtr<nsIContentViewer> mContentViewer;
   nsCOMPtr<nsIDocumentLoader>mDocLoader;
   nsCOMPtr<nsIDocumentLoaderObserver> mDocLoaderObserver;
   nsCOMPtr<nsIWidget>        mParentWidget;
   nsCOMPtr<nsIPref>          mPrefs;
   nsCOMPtr<nsIURI>           mCurrentURI;
   nsCOMPtr<nsIScriptGlobalObject> mScriptGlobal;
   nsCOMPtr<nsIScriptContext> mScriptContext;
   nsCOMPtr<nsISupports>      mLoadCookie;
   PRInt32                    mMarginWidth;
   PRInt32                    mMarginHeight;
   PRInt32                    mItemType;

   /* Note this can not be nsCOMPtr as that that would cause an addref on the 
   parent thus a cycle.  A weak reference would work, but not required as the
   interface states a requirement to zero out the parent when the parent is
   releasing the interface.*/
   nsIDocShellTreeItem*       mParent;  // Weak Reference
   nsIDocShellTreeOwner*      mTreeOwner; // Weak Reference
};

#endif /* nsDocShell_h__ */
