/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsImapCore_H_
#define _nsImapCore_H_

#include "MailNewsTypes.h"
#include "nsString.h"
#include "nsIMailboxSpec.h"
#include "nsIImapFlagAndUidState.h"

class nsIMAPNamespace;
class nsImapProtocol;
class nsImapFlagAndUidState;

/* imap message flags */
typedef PRUint16 imapMessageFlagsType;

/* used for communication between imap thread and event sinks */
#define kNoFlags     0x00	/* RFC flags */
#define kMarked      0x01
#define kUnmarked    0x02
#define kNoinferiors 0x04
#define kNoselect    0x08
#define kImapTrash   0x10	/* Navigator flag */
#define kJustExpunged 0x20	/* This update is a post expunge url update. */
#define kPersonalMailbox	0x40	/* this mailbox is in the personal namespace */
#define kPublicMailbox		0x80	/* this mailbox is in the public namespace */
#define kOtherUsersMailbox	0x100	/* this mailbox is in the other users' namespace */
#define kNameSpace			0x200	/* this mailbox IS a namespace */
#define kNewlyCreatedFolder     0x400 /* this folder was just created */

/* flags for individual messages */
/* currently the ui only offers \Seen and \Flagged */
#define kNoImapMsgFlag			0x0000
#define kImapMsgSeenFlag		0x0001
#define kImapMsgAnsweredFlag	0x0002
#define kImapMsgFlaggedFlag		0x0004
#define kImapMsgDeletedFlag		0x0008
#define kImapMsgDraftFlag		0x0010
#define kImapMsgRecentFlag		0x0020
#define	kImapMsgForwardedFlag	0x0040		/* Not always supported, check mailbox folder */
#define kImapMsgMDNSentFlag		0x0080		/* Not always supported. check mailbox folder */

#define kImapMsgSupportMDNSentFlag 0x2000
#define kImapMsgSupportForwardedFlag 0x4000
#define kImapMsgSupportUserFlag 0x8000		/* This seems to be the most cost effective way of
											 * piggying back the server support user flag
											 * info.
											 */

/* if a url creator does not know the hierarchySeparator, use this */
#define kOnlineHierarchySeparatorUnknown '^'
#define kOnlineHierarchySeparatorNil '|'

#define IMAP_URL_TOKEN_SEPARATOR ">"
#define kUidUnknown -1

// this has to do with Mime Parts on Demand. It used to live in net.h
// I'm not sure where this will live, but here is OK temporarily
typedef enum {
	IMAP_CONTENT_NOT_MODIFIED = 0,
	IMAP_CONTENT_MODIFIED_VIEW_INLINE,
	IMAP_CONTENT_MODIFIED_VIEW_AS_LINKS,
	IMAP_CONTENT_FORCE_CONTENT_NOT_MODIFIED
} IMAP_ContentModifiedType;



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
	kACLCapability = 0x00000100,          /* ACL extension */
	kNamespaceCapability = 0x00000200,    /* IMAP4 Namespace Extension */
	kMailboxDataCapability = 0x00000400,  /* MAILBOXDATA SMTP posting extension */
	kXServerInfoCapability = 0x00000800,  /* XSERVERINFO extension for admin urls */
	kHasAuthPlainCapability = 0x00001000, /* new form of auth plain base64 login */
	kUidplusCapability = 0x00002000,	   /* RFC 2359 UIDPLUS extension */
	kLiteralPlusCapability = 0x00004000, /* RFC 2088 LITERAL+ extension */
	kAOLImapCapability = 0x00008000,     /* aol imap extensions */
  kHasLanguageCapability = 0x00010000, /* language extensions */
  kHasCRAMCapability     = 0x00020000  /* CRAM auth extension */
} eIMAPCapabilityFlag;

// this used to be part of the connection object class - maybe we should move it into 
// something similar
typedef enum {
    kEveryThingRFC822,
    kEveryThingRFC822Peek,
    kHeadersRFC822andUid,
    kUid,
    kFlags,
	  kRFC822Size,
	  kRFC822HeadersOnly,
	  kMIMEPart,
    kMIMEHeader
} nsIMAPeFetchFields;
    
// This class is currently only used for the one-time upgrade
// process from a LIST view to the subscribe model.
// It basically contains the name of a mailbox and whether or not
// its children have been listed.
class nsIMAPMailboxInfo
{
public:
	nsIMAPMailboxInfo(const char *name, char delimiter);
	virtual ~nsIMAPMailboxInfo();
	void SetChildrenListed(PRBool childrenListed) { m_childrenListed = childrenListed; }
	PRBool GetChildrenListed() { return m_childrenListed; }
	const char *GetMailboxName() { return m_mailboxName.get(); }
	char	GetDelimiter() { return m_delimiter; }

protected:
	PRBool m_childrenListed;
	nsCString m_mailboxName;
	char m_delimiter;
};

class nsImapMailboxSpec : public nsIMailboxSpec
{
public:
	NS_DECL_ISUPPORTS

	nsImapMailboxSpec();
	
	virtual ~nsImapMailboxSpec();
	NS_DECL_NSIMAILBOXSPEC

  nsImapMailboxSpec& operator=(const nsImapMailboxSpec& aCopy);
	PRInt32 		  	folder_UIDVALIDITY;
	PRInt32			number_of_messages;
	PRInt32 		  	number_of_unseen_messages;
	PRInt32			number_of_recent_messages;
	
	PRUint32			box_flags;
	
	char          *allocatedPathName;
	PRUnichar		*unicharPathName;
	char			hierarchySeparator;
	char			*hostName;
	
	nsImapProtocol *connection;	// do we need this? It seems evil.
	nsCOMPtr <nsIImapFlagAndUidState>     flagState;
	
	PRBool			folderSelected;
	PRBool			discoveredFromLsub;

	PRBool			onlineVerified;

	nsIMAPNamespace *namespaceForFolder;
};

const int kImapServerDisconnected = 1;
const int kImapOutOfMemory = 2;
const int kImapDownloadingMessage = 3;

typedef struct _GenericInfo {
	char *c, *hostName;
	PRBool rv;
} GenericInfo;

typedef struct _StreamInfo {
	PRUint32	size;
	char	*content_type;
	nsImapMailboxSpec *boxSpec;
} StreamInfo;

typedef struct _ProgressInfo {
	PRUnichar *message;
  PRInt32 currentProgress;
  PRInt32 maxProgress;
} ProgressInfo;

typedef struct _StatusMessageInfo {
	PRUint32 msgID;
	char * extraInfo;
} StatusMessageInfo;

typedef struct _utf_name_struct {
	PRBool toUtf7Imap;
	unsigned char *sourceString;
	unsigned char *convertedString;
} utf_name_struct;


typedef struct _msg_line_info {
    char   *adoptedMessageLine;
    PRUint32 uidOfMessage;
} msg_line_info;


typedef struct _TunnelInfo {
	PRInt32 maxSize;
	// NETSOCK		ioSocket; *** not yet ****
	// char**		inputSocketBuffer;
    // PRInt32*		inputBufferSize;
	PRBool		fromHeaderSeenAlready;
	const char	*xSenderInfo;
	PRUint32		uidOfMessage;
} TunnelInfo;

typedef struct _delete_message_struct {
	char *onlineFolderName;
	PRBool		deleteAllMsgs;
	char *msgIdString;
} delete_message_struct;

typedef enum {
	kEverythingDone,
	kBringUpSubscribeUI
} EIMAPSubscriptionUpgradeState;

typedef enum {
    	eContinue,
		eContinueNew,
    	eListMyChildren,
    	eNewServerDirectory,
    	eCancelled 
} EMailboxDiscoverStatus;



// This class is only used for passing data
// between the IMAP and mozilla threadns 
class nsIMAPACLRightsInfo
{ 
public:
	char *hostName, *mailboxName, *userName, *rights;
};

typedef struct _uid_validity_info {
	char *canonical_boxname;
	const char *hostName;
	int32 returnValidity;
} uid_validity_info;


#define kDownLoadCacheSize 1536

class TLineDownloadCache {
public:
    TLineDownloadCache();
    virtual ~TLineDownloadCache();

    PRUint32  CurrentUID();
    PRUint32  SpaceAvailable();
    PRBool CacheEmpty();
    
    msg_line_info *GetCurrentLineInfo();
    
    void ResetCache();
    void CacheLine(const char *line, PRUint32 uid);
private:
    char   fLineCache[kDownLoadCacheSize];
    PRUint32 fBytesUsed;
    
    msg_line_info *fLineInfo;
    
};

#endif
