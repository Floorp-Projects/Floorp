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
#include "nsIURL.h"
#include "nsINetlibURL.h" /* this should be temporary until Network N2 project lands */
#include "nsIUrlListener.h"
#include "nsIUrlListenerManager.h"
#include "nsCOMPtr.h"
#include "nsIMsgMailNewsUrl.h"

///////////////////////////////////////////////////////////////////////////////////
// Okay, I found that all of the mail and news url interfaces needed to support
// several common interfaces (in addition to those provided through nsIURL). 
// So I decided to group them all in this interface.
// This interface may grow or it may get smaller (if these things get pushed up
// into nsIURL). The unfortunate thing is that we'd also like to have all of our
// mail news protocols inherit implementations of this interface. But that is
// implementation inheritance across dlls. We could do it though....something else
// to add to the list =).
//
// mscott - I'm now adding a base url implementation for mailnews to unify
// some common code....
//////////////////////////////////////////////////////////////////////////////////

class NS_MSG_BASE nsMsgMailNewsUrl : public nsIMsgMailNewsUrl, public nsINetlibURL
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

	NS_IMETHOD SetErrorMessage (char * errorMessage);
	// caller must free using PR_FREE
	NS_IMETHOD GetErrorMessage (char ** errorMessage) const;

	// if you really want to know what the current state of the url is (running or not
	// running) you should look into becoming a urlListener...
	NS_IMETHOD SetUrlState(PRBool runningUrl, nsresult aStatusCode);
	NS_IMETHOD GetUrlState(PRBool *runningUrl);

	// nsIURL support
	// mscott: some of these we won't need to implement..as part of the netlib re-write we'll be removing them
	// from nsIURL and then we can remove them from here as well....
    NS_IMETHOD_(PRBool) Equals(const nsIURL *aURL) const;
    NS_IMETHOD GetSpec(const char* *result) const;
    NS_IMETHOD SetSpec(const char* spec);
    NS_IMETHOD GetProtocol(const char* *result) const;
    NS_IMETHOD SetProtocol(const char* protocol);
    NS_IMETHOD GetHost(const char* *result) const;
    NS_IMETHOD SetHost(const char* host);
    NS_IMETHOD GetHostPort(PRUint32 *result) const;
    NS_IMETHOD SetHostPort(PRUint32 port);
    NS_IMETHOD GetFile(const char* *result) const;
    NS_IMETHOD SetFile(const char* file);
    NS_IMETHOD GetRef(const char* *result) const;
    NS_IMETHOD SetRef(const char* ref);
    NS_IMETHOD GetSearch(const char* *result) const;
    NS_IMETHOD SetSearch(const char* search);
    NS_IMETHOD GetContainer(nsISupports* *result) const;
    NS_IMETHOD SetContainer(nsISupports* container);	
    NS_IMETHOD GetLoadAttribs(nsILoadAttribs* *result) const;	// make obsolete
    NS_IMETHOD SetLoadAttribs(nsILoadAttribs* loadAttribs);	// make obsolete
    NS_IMETHOD GetURLGroup(nsIURLGroup* *result) const;	// make obsolete
    NS_IMETHOD SetURLGroup(nsIURLGroup* group);	// make obsolete
    NS_IMETHOD SetPostHeader(const char* name, const char* value);	// make obsolete
    NS_IMETHOD SetPostData(nsIInputStream* input);	// make obsolete
    NS_IMETHOD GetContentLength(PRInt32 *len);
    NS_IMETHOD GetServerStatus(PRInt32 *status);  // make obsolete
    NS_IMETHOD ToString(PRUnichar* *aString) const;

    // from nsINetlibURL:

    NS_IMETHOD GetURLInfo(URL_Struct_ **aResult) const;
    NS_IMETHOD SetURLInfo(URL_Struct_ *URL_s);

protected:
	virtual ~nsMsgMailNewsUrl();
	virtual void ReconstructSpec(void) = 0;
	
	// protocol specific code to parse a url...
    virtual nsresult ParseUrl(const nsString& aSpec) = 0;

	char		*m_spec;
    char		*m_protocol;
    char		*m_host;
    char		*m_file;
    char		*m_ref;
    char		*m_search;
	char		*m_errorMessage;

	/* Here's our link to the netlib world.... */
    URL_Struct *m_URL_s;

	PRBool		m_runningUrl;
    
	PRInt32			m_port;
    nsISupports*    m_container;

	// manager of all of current url listeners....
	nsCOMPtr<nsIUrlListenerManager> m_urlListeners;
};


#endif /* nsMsgMailNewsUrl_h___ */
