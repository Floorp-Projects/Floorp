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
#include "nsMsgMailNewsUrl.h"
#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

class nsMailboxUrl : public nsIMailboxUrl, public nsMsgMailNewsUrl, public nsIMsgUriUrl
{
public:
	// nsIURI over-ride...
	NS_IMETHOD SetSpec(char * aSpec);

	// from nsIMailboxUrl:
	NS_IMETHOD GetMessageHeader(nsIMsgDBHdr ** aMsgHdr);
	NS_IMETHOD SetMailboxParser(nsIStreamListener * aConsumer);
	NS_IMETHOD GetMailboxParser(nsIStreamListener ** aConsumer);
	NS_IMETHOD SetMailboxCopyHandler(nsIStreamListener *  aConsumer);
	NS_IMETHOD GetMailboxCopyHandler(nsIStreamListener ** aConsumer);
	
	NS_IMETHOD GetFilePath(nsFileSpec ** aFilePath);
	NS_IMETHOD GetMessageKey(nsMsgKey* aMessageKey);
    NS_IMETHOD GetMessageSize(PRUint32 *aMessageSize);
	NS_IMETHOD SetMessageSize(PRUint32 aMessageSize);
	NS_IMPL_CLASS_GETSET(MailboxAction, nsMailboxAction, m_mailboxAction);

	// used by save message to disk....
	NS_IMETHOD SetMessageFile(nsIFileSpec * aFileSpec);
	NS_IMETHOD GetMessageFile(nsIFileSpec ** aFileSpec);

	// from nsIMsgUriUrl
	NS_IMETHOD GetURI(char ** aURI); 

    // nsMailboxUrl
    nsMailboxUrl();
	virtual ~nsMailboxUrl();

    NS_DECL_ISUPPORTS_INHERITED


protected:
	// protocol specific code to parse a url...
    virtual nsresult ParseUrl(const nsString& aSpec);
	virtual const char * GetUserName() { return nsnull;}

	// mailboxurl specific state
	nsCOMPtr<nsIStreamListener> m_mailboxParser;
	nsCOMPtr<nsIStreamListener> m_mailboxCopyHandler;

	nsMailboxAction m_mailboxAction; // the action this url represents...parse mailbox, display messages, etc.
	nsFileSpec	*m_filePath; 
	char		*m_messageID;
	PRUint32	m_messageSize;
	nsMsgKey	m_messageKey;
	nsXPIDLCString m_file;

	// used by save message to disk
	nsCOMPtr<nsIFileSpec> m_messageFileSpec;
	nsresult ParseSearchPart();
};

#endif // nsMailboxUrl_h__
