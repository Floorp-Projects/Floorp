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

#ifndef nsDocShellBase_h__
#define nsDocShellBase_h__

#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIContentViewer.h"
#include "nsIPref.h"

#include "nsIDocShell.h"
#include "nsIDocShellEdit.h"
#include "nsIDocShellFile.h"
#include "nsIGenericWindow.h"
#include "nsIScrollable.h"
#include "nsITextScroll.h"

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

class nsDocShellBase : public nsIDocShell, public nsIDocShellEdit, 
   public nsIDocShellFile, public nsIGenericWindow, public nsIScrollable, 
   public nsITextScroll
{
friend class nsDSURIContentListener;

public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIDOCSHELL
   NS_DECL_NSIDOCSHELLEDIT
   NS_DECL_NSIDOCSHELLFILE
   NS_DECL_NSIGENERICWINDOW
   NS_DECL_NSISCROLLABLE
   NS_DECL_NSITEXTSCROLL

protected:
   nsDocShellBase();
   virtual ~nsDocShellBase();

   nsresult GetChildOffset(nsIDOMNode* aChild, nsIDOMNode* aParent, 
      PRInt32* aOffset);
   nsresult GetRootScrollableView(nsIScrollableView** aOutScrollView);
   nsresult GetPresShell(nsIPresShell** aPresShell);
   nsresult EnsureContentListener();

   void SetCurrentURI(nsIURI* aUri);

protected:
   PRBool                     mCreated;
   nsString                   mName;
   nsDSURIContentListener*    mContentListener;
   nsDocShellInitInfo*        mBaseInitInfo;
   nsCOMPtr<nsIContentViewer> mContentViewer;
   nsCOMPtr<nsIWidget>        mParentWidget;
   nsCOMPtr<nsIPref>          mPrefs;
   nsCOMPtr<nsIURI>           mCurrentURI;
   nsCOMPtr<nsISupports>      mLoadCookie;

   /* Note this can not be nsCOMPtr as that that would cause an addref on the 
   parent thus a cycle.  A weak reference would work, but not required as the
   interface states a requirement to zero out the parent when the parent is
   releasing the interface.*/
   nsIDocShell*               mParent; 
                                       
};

#endif /* nsDocShellBase_h__ */
