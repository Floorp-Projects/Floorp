/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
*/

#ifndef nsSecureBrowserUIImpl_h_
#define nsSecureBrowserUIImpl_h_

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsIObserver.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsIStringBundle.h"
#include "nsISecureBrowserUI.h"
#include "nsIDocShell.h"
#include "nsIPref.h"
#include "nsIWebProgressListener.h"
#include "nsIFormSubmitObserver.h"
#include "nsIURI.h"
#include "nsISecurityEventSink.h"

#define NS_SECURE_BROWSER_DOCOBSERVER_CLASSNAME "Mozilla Secure Browser Doc Observer"

#define NS_SECURE_BROWSER_DOCOBSERVER_CID \
{0x97c06c30, 0xa145, 0x11d3, \
{0x8c, 0x7c, 0x00, 0x60, 0x97, 0x92, 0x27, 0x8c}}

#define NS_SECURE_BROWSER_DOCOBSERVER_CONTRACTID "@mozilla.org/secure_browser_docobserver;1"


class nsSecureBrowserUIImpl : public nsSecureBrowserUI, 
                              public nsIWebProgressListener, 
                              public nsIFormSubmitObserver,
                              public nsIObserver
{
public:

	nsSecureBrowserUIImpl();
	virtual ~nsSecureBrowserUIImpl();
    
    static NS_METHOD Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  	NS_DECL_ISUPPORTS    
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSSECUREBROWSERUI


    // nsIObserver
    NS_DECL_NSIOBSERVER
    NS_IMETHOD Notify(nsIContent* formNode, nsIDOMWindowInternal* window, nsIURI *actionURL);

protected:

	nsCOMPtr<nsIDOMWindowInternal>              mWindow;
    nsCOMPtr<nsIDOMElement>             mSecurityButton;
	nsCOMPtr<nsIDocumentLoaderObserver> mOldWebShellObserver;
    nsCOMPtr<nsIPref>                   mPref;
    nsCOMPtr<nsIStringBundle>           mStringBundle;
    
    nsCOMPtr<nsIURI>                    mCurrentURI;

	PRBool					mIsSecureDocument;  // is https loaded
	PRBool					mIsDocumentBroken;  // 
	PRBool					mMixContentAlertShown;
    
    PRBool                  mInitByLocationChange;

    char*                   mLastPSMStatus;
    

    void GetBundleString(const nsString& name, nsString &outString);
    
    nsresult CheckProtocolContextSwitch( nsISecurityEventSink* sink, nsIRequest* request, nsIURI* newURI, nsIURI* oldURI);
    nsresult CheckMixedContext( nsISecurityEventSink* sink, nsIRequest* request, nsIURI* nextURI);
    nsresult CheckPost(nsIURI *actionURL, PRBool *okayToPost);
    nsresult IsURLHTTPS(nsIURI* aURL, PRBool *value);
    nsresult IsURLfromPSM(nsIURI* aURL, PRBool *value);
    nsresult SetBrokenLockIcon(nsISecurityEventSink* sink,  nsIRequest* request, PRBool removeValue = PR_FALSE);
};


#endif /* nsSecureBrowserUIImpl_h_ */
