/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Christopher Blizzard
 */

#ifndef EmbedWindow_h
#define EmbedWindow_h

#include <nsString.h>
#include <nsIWebBrowserChrome.h>
#include <nsIWebBrowserChromeFocus.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsITooltipListener.h>
#include <nsISupports.h>
#include <nsIWebBrowser.h>
#include <nsIBaseWindow.h>
#include <nsIInterfaceRequestor.h>
#include <nsCOMPtr.h>
#include "nsString.h"

class NativeBrowserControl;
class nsIInputStream;
class nsIURI;
class nsIDocShellLoadInfo;
class nsIWeakReference;

#include "ns_util.h"

class EmbedWindow : public nsIWebBrowserChrome,
		    public nsIWebBrowserChromeFocus,
                    public nsIEmbeddingSiteWindow,
                    public nsITooltipListener,
		    public nsIInterfaceRequestor
{
    
public:
    
    EmbedWindow();
    virtual ~EmbedWindow();
    
    nsresult Init            (NativeBrowserControl *aOwner);
    nsresult CreateWindow_   (PRUint32 width, PRUint32 height);
    void     ReleaseChildren (void);

    nsresult SelectAll       ();
    nsresult GetSelection    (JNIEnv *env, jobject selection);

    nsresult LoadStream      (nsIInputStream *aStream, nsIURI * aURI,
                              const nsACString &aContentType,
                              const nsACString &aContentCharset,
                              nsIDocShellLoadInfo * aLoadInfo);

    nsresult AddWebBrowserListener(nsIWeakReference *aListener, 
                                   const nsIID & aIID);
    
    NS_DECL_ISUPPORTS
    
    NS_DECL_NSIWEBBROWSERCHROME
    
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    
    NS_DECL_NSITOOLTIPLISTENER
    
    NS_DECL_NSIINTERFACEREQUESTOR
    
    nsString                 mTitle;
    nsString                 mJSStatus;
    nsString                 mLinkMessage;
    
    nsCOMPtr<nsIBaseWindow>  mBaseWindow; // [OWNER]
    
private:
    
    NativeBrowserControl     *mOwner;
    nsCOMPtr<nsIWebBrowser>  mWebBrowser; // [OWNER]
    // PENDING(edburns): what about the tooltip window?
    PRBool                   mVisibility;
    PRBool                   mIsModal;

};

#endif // EmbedWindow_h
