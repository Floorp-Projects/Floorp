/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chak Nanga <chak@netscape.com> 
 */

#ifndef _BROWSERIMPL_H
#define _BROWSERIMPL_H

#include "IBrowserFrameGlue.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIContextMenuListener.h"
#include "nsIDOMNode.h"


class CBrowserImpl : 
	 public nsIInterfaceRequestor,
	 public nsIWebBrowserChrome,
     public nsIWebBrowserChromeFocus,
	 public nsIEmbeddingSiteWindow,
	 public nsIWebProgressListener,
	 public nsIContextMenuListener,
	 public nsSupportsWeakReference,
	 public nsISHistoryListener,		// de: added this in 5/11   					 
	 public nsIStreamListener,			// de: added this in 6/29
	 public nsITooltipListener   		// de: added this in 7/25
//	 public nsITooltipTextProvider		// de: added this in 7/25
{
public:
    CBrowserImpl();
    ~CBrowserImpl();
    NS_METHOD Init(PBROWSERFRAMEGLUE pBrowserFrameGlue,
                   nsIWebBrowser* aWebBrowser);
// de: macros that declare methods for the ifaces implemented
	// (instead of .h declarations)
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSICONTEXTMENULISTENER
	NS_DECL_NSISHISTORYLISTENER		 // de: added this in 5/11
	NS_DECL_NSISTREAMLISTENER  		 // de: added this in 6/29
	NS_DECL_NSIREQUESTOBSERVER		 // de: added this in 6/29
	NS_DECL_NSITOOLTIPLISTENER		 // de: added this in 7/25
	//NS_DECL_NSITOOLTIPTEXTPROVIDER   // de: added this in 7/25
protected:

    PBROWSERFRAMEGLUE  m_pBrowserFrameGlue;

    nsCOMPtr<nsIWebBrowser> mWebBrowser;
};

#endif //_BROWSERIMPL_H
