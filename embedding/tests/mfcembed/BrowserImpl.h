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

class CBrowserImpl : public nsIInterfaceRequestor,
					 public nsIWebBrowserChrome,
                     public nsIWebBrowserChromeFocus,
					 public nsIEmbeddingSiteWindow,
					 public nsIWebProgressListener,
					 public nsIContextMenuListener,
					 public nsSupportsWeakReference
{
public:
    CBrowserImpl();
    ~CBrowserImpl();
    NS_METHOD Init(PBROWSERFRAMEGLUE pBrowserFrameGlue,
                   nsIWebBrowser* aWebBrowser);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSICONTEXTMENULISTENER

protected:

    PBROWSERFRAMEGLUE  m_pBrowserFrameGlue;

    nsCOMPtr<nsIWebBrowser> mWebBrowser;
};

#endif //_BROWSERIMPL_H
