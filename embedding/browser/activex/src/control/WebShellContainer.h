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
 * Contributor(s): 
 */
#ifndef WEBSHELLCONTAINER_H
#define WEBSHELLCONTAINER_H

// This is the class that handles the XPCOM side of things, callback
// interfaces into the web shell and so forth.

class CWebShellContainer :
		public nsIBaseWindow,
		public nsIWebBrowserChrome,
		public nsIWebShellContainer,
		public nsIStreamObserver,
		public nsIDocumentLoaderObserver,
		public nsIDocShellTreeOwner
{
public:
	CWebShellContainer(CMozillaBrowser *pOwner);

protected:
	virtual ~CWebShellContainer();

// Protected members
protected:
	nsString m_sTitle;
	
	CMozillaBrowser *m_pOwner;
	CDWebBrowserEvents1 *m_pEvents1;
	CDWebBrowserEvents2 *m_pEvents2;

public:

	NS_DECL_ISUPPORTS
	NS_DECL_NSIBASEWINDOW
	NS_DECL_NSIWEBBROWSERCHROME
	NS_DECL_NSIDOCSHELLTREEOWNER

	// nsIWebShellContainer
	NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason);
	NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL);
	NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
	NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsresult aStatus);
	NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
						PRBool aVisible,
						nsIWebShell *&aNewWebShell);
	NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken);
	NS_IMETHOD ContentShellAdded(nsIWebShell* aWebShell, nsIContent* frameNode);
	NS_IMETHOD CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent, 
						 PRInt32 aXPos, PRInt32 aYPos, 
						 const nsString& aPopupType, const nsString& anAnchorAlignment,
						 const nsString& aPopupAlignment,
                         nsIDOMWindow* aWindow, nsIDOMWindow** outPopup);

	// nsIStreamObserver
    NS_IMETHOD OnStartRequest(nsIChannel* aChannel, nsISupports* aContext);
    NS_IMETHOD OnStopRequest(nsIChannel* aChannel, nsISupports* aContext, nsresult aStatus, const PRUnichar* aMsg);

	// nsIDocumentLoaderObserver
   NS_DECL_NSIDOCUMENTLOADEROBSERVER
};

#endif

