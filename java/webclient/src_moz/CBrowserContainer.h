/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s): Ashutosh Kulkarni <ashuk@eng.sun.com>
 *                 Ed Burns <edburns@acm.org>
 *
 */

#ifndef BROWSERCONTAINER_H
#define BROWSERCONTAINER_H

#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebProgressListener.h"
#include "nsIWebShell.h"  // We still have to implement nsIWebShellContainer
                          // in order to receveive some DocumentLoaderObserver
                          // events.  edburns
#include "nsIStreamObserver.h"
#include "nsIURIContentListener.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIPrompt.h"
#include "nsCWebBrowser.h"

#include "wcIBrowserContainer.h"

#include "ns_util.h"

class nsIURI;

// This is the class that handles the XPCOM side of things, callback
// interfaces into the web shell and so forth.

class CBrowserContainer :
		public nsIBaseWindow,
		public nsIWebBrowserChrome,
		public nsIWebProgressListener,
		public nsIWebShellContainer,
		public nsIStreamObserver,
		public nsIURIContentListener,
		public nsIDocumentLoaderObserver,
		public nsIDocShellTreeOwner,
		public nsIInterfaceRequestor,
		public nsIPrompt,
		public nsIDOMMouseListener,
    public wcIBrowserContainer
{

public:


public:
  CBrowserContainer(nsIWebBrowser *pOwner, JNIEnv *yourJNIEnv, WebShellInitContext *yourInitContext);

  CBrowserContainer();

public:
  virtual ~CBrowserContainer();

// Protected members
protected:
  nsIWebBrowser *m_pOwner;
  JNIEnv *mJNIEnv;
  WebShellInitContext *mInitContext;
  jobject mDocTarget;
  jobject mMouseTarget;
  nsCOMPtr<nsIDOMEventTarget> mDomEventTarget;

//
// The following arguments are used in the takeActionOnNode method.
//
    
/**

 * 0 is the leaf depth.  That's why we call it the inverse depth.

 */

    PRInt32 inverseDepth;

/**

 * The properties table, created during a mouseEvent handler

 */

    jobject properties;

/**

 * the nsIDOMEvent in the current event

 */

    nsCOMPtr<nsIDOMEvent> currentDOMEvent;

//
// End of ivars used in takeActionOnNode
//


public:
  	NS_DECL_ISUPPORTS
	NS_DECL_NSIBASEWINDOW
	NS_DECL_NSIWEBBROWSERCHROME
	NS_DECL_NSIDOCSHELLTREEOWNER
	NS_DECL_NSIURICONTENTLISTENER
	NS_DECL_NSISTREAMOBSERVER
	NS_DECL_NSIDOCUMENTLOADEROBSERVER
	NS_DECL_NSIINTERFACEREQUESTOR
	NS_DECL_NSIWEBPROGRESSLISTENER

	NS_DECL_WCIBROWSERCONTAINER

	// "Services" accessed through nsIInterfaceRequestor
	NS_DECL_NSIPROMPT

	// nsIDOMMouseListener
  
	virtual nsresult HandleEvent(nsIDOMEvent* aEvent);
	virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent);

  // nsIWebShellContainer
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell,
                         const PRUnichar* aURL,
                         nsLoadType aReason);
  
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell,
                          const PRUnichar* aURL);
  
  
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell,
                        const PRUnichar* aURL,
                        nsresult aStatus);

protected:
//
// Local methods
//
jobject JNICALL getPropertiesFromEvent(nsIDOMEvent *aMouseEvent);

void JNICALL addMouseEventDataToProperties(nsIDOMEvent *aMouseEvent);

static  nsresult JNICALL takeActionOnNode(nsCOMPtr<nsIDOMNode> curNode, 
                                          void *yourObject);

};

#endif

