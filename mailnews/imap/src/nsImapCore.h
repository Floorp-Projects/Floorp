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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsImapCore_H_
#define _nsImapCore_H_

class nsIMAPNamespace;

// I think this should really go in an imap.h equivalent file
typedef enum {
	kPersonalNamespace = 0,
	kOtherUsersNamespace,
	kPublicNamespace,
	kDefaultNamespace,
	kUnknownNamespace
} EIMAPNamespaceType;


typedef enum {
	kCapabilityUndefined = 0x00000000,
	kCapabilityDefined = 0x00000001,
	kHasAuthLoginCapability = 0x00000002,
	kHasXNetscapeCapability = 0x00000004,
	kHasXSenderCapability = 0x00000008,
	kIMAP4Capability = 0x00000010,          /* RFC1734 */
	kIMAP4rev1Capability = 0x00000020,      /* RFC2060 */
	kIMAP4other = 0x00000040,                       /* future rev?? */
	kNoHierarchyRename = 0x00000080,                        /* no hierarchy rename */
	kACLCapability = 0x00000100,    /* ACL extension */
	kNamespaceCapability = 0x00000200,      /* IMAP4 Namespace Extension */
	kMailboxDataCapability = 0x00000400,    /* MAILBOXDATA SMTP posting extension */
	kXServerInfoCapability = 0x00000800     /* XSERVERINFO extension for admin urls */
} eIMAPCapabilityFlag;

class nsIMAPNamespace
{

public:
	nsIMAPNamespace(EIMAPNamespaceType type, const char *prefix, char delimiter, PRBool from_prefs);

	~nsIMAPNamespace();

	EIMAPNamespaceType	GetType() { return m_namespaceType; }
	const char *		GetPrefix() { return m_prefix; }
	char				GetDelimiter() { return m_delimiter; }
	void				SetDelimiter(char delimiter);
	PRBool				GetIsDelimiterFilledIn() { return m_delimiterFilledIn; }
	PRBool				GetIsNamespaceFromPrefs() { return m_fromPrefs; }

	// returns -1 if this box is not part of this namespace,
	// or the length of the prefix if it is part of this namespace
	int					MailboxMatchesNamespace(const char *boxname);

protected:
	EIMAPNamespaceType m_namespaceType;
	char	*m_prefix;
	char	m_delimiter;
	PRBool	m_fromPrefs;
	PRBool m_delimiterFilledIn;

};

// this used to be part of the connection object class - maybe we should move it into 
// something similar
enum nsIMAPeFetchFields {
    kEveryThingRFC822,
    kEveryThingRFC822Peek,
    kHeadersRFC822andUid,
    kUid,
    kFlags,
	kRFC822Size,
	kRFC822HeadersOnly,
	kMIMEPart,
    kMIMEHeader
};
    
// This class is currently only used for the one-time upgrade
// process from a LIST view to the subscribe model.
// It basically contains the name of a mailbox and whether or not
// its children have been listed.
class nsIMAPMailboxInfo
{
public:
	nsIMAPMailboxInfo(const char *name);
	virtual ~nsIMAPMailboxInfo();
	void SetChildrenListed(PRBool childrenListed) { m_childrenListed = childrenListed; }
	PRBool GetChildrenListed() { return m_childrenListed; }
	const char *GetMailboxName() { return m_mailboxName; }

protected:
	PRBool m_childrenListed;
	char *m_mailboxName;
};



#endif
