/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsMsgMailNewsUrl_h___
#define nsMsgMailNewsUrl_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsIUrlListener.h"
#include "nsIUrlListenerManager.h"
#include "nsIMsgStatusFeedback.h"
#include "nsCOMPtr.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIURL.h"

///////////////////////////////////////////////////////////////////////////////////
// Okay, I found that all of the mail and news url interfaces needed to support
// several common interfaces (in addition to those provided through nsIURI). 
// So I decided to group them all in this implementation so we don't have to
// duplicate the code.
//
//////////////////////////////////////////////////////////////////////////////////

class NS_MSG_BASE nsMsgMailNewsUrl : public nsIMsgMailNewsUrl
{
public:
	nsMsgMailNewsUrl();

	NS_DECL_ISUPPORTS

	// nsIMsgMailNewsUrl support

	///////////////////////////////////////////////////////////////////////////////
	// The idea is to allow the "application" (the part of the code which wants to 
	// run a url in order to perform some action) to register itself as a listener
	// on url. As a url listener, the app will be informed when the url begins to run
	// and when the url is finished. 
	////////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD RegisterListener (nsIUrlListener * aUrlListener);
	NS_IMETHOD UnRegisterListener (nsIUrlListener * aUrlListener);

	/////////////////////////////////////////////////////////////////////////////// 
	// Getters and Setters for the nsMsgMailNewsUrl specific info....
	///////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD SetErrorMessage (const char * errorMessage);
	// caller must free using PR_FREE
	NS_IMETHOD GetErrorMessage (char ** errorMessage);

	NS_IMETHOD SetStatusFeedback(nsIMsgStatusFeedback *aMsgFeedback);
	NS_IMETHOD GetStatusFeedback(nsIMsgStatusFeedback **aMsgFeedback);

	// This is just a stub implementation. It is the responsibility of derived
	// url classes to over-ride this method.
	NS_IMETHOD GetServer(nsIMsgIncomingServer ** aIncomingServer);

	// if you really want to know what the current state of the url is (running or not
	// running) you should look into becoming a urlListener...
	NS_IMETHOD SetUrlState(PRBool runningUrl, nsresult aStatusCode);
	NS_IMETHOD GetUrlState(PRBool *runningUrl);

	//////////////////////////////////////////////////////////////////////////////////
	// nsIURI support
    NS_DECL_NSIURI

	//////////////////////////////////////////////////////////////////////////////////
	// nsIURL support
    NS_DECL_NSIURL

protected:
	virtual ~nsMsgMailNewsUrl();

	// a helper function I needed from derived urls...
	virtual const char * GetUserName() = 0;

	nsCOMPtr<nsIURL> m_baseURL;
	nsCOMPtr<nsIMsgStatusFeedback> m_statusFeedback;
	char		*m_errorMessage;
	PRBool		m_runningUrl;

	// manager of all of current url listeners....
	nsCOMPtr<nsIUrlListenerManager> m_urlListeners;
};


#endif /* nsMsgMailNewsUrl_h___ */
