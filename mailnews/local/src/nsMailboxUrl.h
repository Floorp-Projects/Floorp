/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsMailboxUrl_h__
#define nsMailboxUrl_h__

#include "nsIMailboxUrl.h"
#include "nsIUrlListenerManager.h"
#include "nsINetlibURL.h" /* this should be temporary until Network N2 project lands */
#include "nsFileSpec.h"

class nsMailboxUrl : public nsIMailboxUrl, public nsINetlibURL, nsIMsgUriUrl
{
public:
    // from nsIURL:

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
	
	// from nsIMailboxUrl:
	NS_IMETHOD SetMailboxParser(nsIStreamListener * aConsumer);
	NS_IMETHOD GetMailboxParser(nsIStreamListener ** aConsumer);
	NS_IMETHOD SetMailboxCopyHandler(nsIStreamListener *  aConsumer);
	NS_IMETHOD GetMailboxCopyHandler(nsIStreamListener ** aConsumer);
	
	NS_IMETHOD SetFilePath(const nsFileSpec& aFilePath);
	NS_IMETHOD GetFilePath(const nsFileSpec ** aFilePath);
	NS_IMETHOD GetMessageKey(nsMsgKey& aMessageKey);
	NS_IMETHOD SetMessageSize(PRUint32 aMessageSize);
	NS_IMPL_CLASS_GETSET(MailboxAction, nsMailboxAction, m_mailboxAction);

	// from nsIMsgMailNewsUrl:
	NS_IMETHOD SetUrlState(PRBool aRunningUrl, nsresult aExitCode);
	NS_IMETHOD GetUrlState(PRBool * aRunningUrl);

	NS_IMETHOD SetErrorMessage (char * errorMessage);
	// caller must free using PR_FREE
	NS_IMETHOD GetErrorMessage (char ** errorMessage) const;
	NS_IMETHOD RegisterListener (nsIUrlListener * aUrlListener);
	NS_IMETHOD UnRegisterListener (nsIUrlListener * aUrlListener);

	// from nsIMsgUriUrl
	NS_IMETHOD GetURI(char ** aURI); 

    // nsMailboxUrl
    nsMailboxUrl(nsISupports* aContainer, nsIURLGroup* aGroup);
	virtual ~nsMailboxUrl();

    NS_DECL_ISUPPORTS

	// protocol specific code to parse a url...
    nsresult ParseURL(const nsString& aSpec, const nsIURL* aURL = nsnull);

protected:

	// mailboxurl specific state
	nsIStreamListener *m_mailboxParser;
	nsIStreamListener *m_mailboxCopyHandler;

	// manager of all of current url listeners....
	nsIUrlListenerManager * m_urlListeners;

    /* Here's our link to the old netlib world.... */
    URL_Struct *m_URL_s;

	char		*m_spec;
    char		*m_protocol;
    char		*m_host;
    char		*m_file;
    char		*m_ref;
	char		*m_search;
	char		*m_errorMessage;

	PRBool		m_runningUrl;
    
    nsISupports	*m_container;

	nsMailboxAction m_mailboxAction; // the action this url represents...parse mailbox, display messages, etc.
	nsFileSpec	*m_filePath; 
	char		*m_messageID;
	PRUint32	m_messageSize;
	nsMsgKey	m_messageKey;

	void ReconstructSpec(void);
	nsresult ParseSearchPart();
};

#endif // nsMailboxUrl_h__
