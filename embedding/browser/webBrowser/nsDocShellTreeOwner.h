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

#ifndef nsDocShellTreeOwner_h__
#define nsDocShellTreeOwner_h__

// Helper Classes
#include "nsCOMPtr.h"
#include "nsString.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebProgressListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMDocument.h"

#include "nsCommandHandler.h"

class nsWebBrowser;

// {6D10C180-6888-11d4-952B-0020183BF181}
#define NS_ICDOCSHELLTREEOWNER_IID \
{ 0x6d10c180, 0x6888, 0x11d4, { 0x95, 0x2b, 0x0, 0x20, 0x18, 0x3b, 0xf1, 0x81 } }

/*
 * This is a fake 'hidden' interface that nsDocShellTreeOwner implements.
 * Classes such as nsCommandHandler can QI for this interface to be
 * sure that they're dealing with a valid nsDocShellTreeOwner and not some
 * other object that implements nsIDocShellTreeOwner.
 */
class nsICDocShellTreeOwner : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICDOCSHELLTREEOWNER_IID)
};


class nsDocShellTreeOwner : public nsIDocShellTreeOwner,
                            public nsIBaseWindow,
                            public nsIInterfaceRequestor,
                            public nsIWebProgressListener,
                            public nsIDOMMouseListener,
                            public nsICDocShellTreeOwner
{
friend class nsWebBrowser;
friend class nsCommandHandler;

public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIBASEWINDOW
    NS_DECL_NSIDOCSHELLTREEOWNER
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWEBPROGRESSLISTENER

	// nsIDOMMouseListener
	virtual nsresult HandleEvent(nsIDOMEvent* aEvent);
	virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent);

protected:
    nsDocShellTreeOwner();
    virtual ~nsDocShellTreeOwner();

    void WebBrowser(nsWebBrowser* aWebBrowser);
    nsWebBrowser* WebBrowser();
    NS_IMETHOD SetTreeOwner(nsIDocShellTreeOwner* aTreeOwner);
    NS_IMETHOD SetWebBrowserChrome(nsIWebBrowserChrome* aWebBrowserChrome);

    nsCOMPtr<nsIDOMDocument> mLastDOMDocument;
    NS_IMETHOD AddMouseListener();
    NS_IMETHOD RemoveMouseListener();

protected:

   // Weak References
   nsWebBrowser*           mWebBrowser;
   nsIDocShellTreeOwner*   mTreeOwner;
   nsIDocShellTreeItem*    mPrimaryContentShell; 

   nsIWebBrowserChrome*    mWebBrowserChrome;
   nsIWebProgressListener* mOwnerProgressListener;
   nsIBaseWindow*          mOwnerWin;
   nsIInterfaceRequestor*  mOwnerRequestor;
   PRBool                  mMouseListenerActive;
};

#endif /* nsDocShellTreeOwner_h__ */












