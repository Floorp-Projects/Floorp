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

#include "MailNewsTypes.h"
#include "nsString2.h"

class nsIMAPNamespace;
class nsImapProtocol;
class nsImapFlagAndUidState;

/* imap message flags */
typedef PRUint16 imapMessageFlagsType;

/* used for communication between libmsg and libnet */
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
	kXServerInfoCapability = 0x00000800,     /* XSERVERINFO extension for admin urls */
	kHasAuthPlainCapability = 0x00001000, /* new form of auth plain base64
											login */
	kUidplusCapability = 0x00002000,	/* RFC 2359 UIDPLUS extension */
	kLiteralPlusCapability = 0x00004000 /* RFC 2088 LITERAL+ extension */
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
	const char *GetMailboxName() { return m_mailboxName.GetBuffer(); }
	char	GetDelimiter() { return m_delimiter; }

protected:
	PRBool m_childrenListed;
	nsString2 m_mailboxName;
	char m_delimiter;
};


struct mailbox_spec {
	PRInt32 		  	folder_UIDVALIDITY;
	PRInt32			number_of_messages;
	PRInt32 		  	number_of_unseen_messages;
	PRInt32			number_of_recent_messages;
	
	PRUint32			box_flags;
	
	char          *allocatedPathName;
	char			hierarchySeparator;
	const char		*hostName;
	
	nsImapProtocol *connection;	// do we need this? It seems evil.
	nsImapFlagAndUidState     *flagState;
	
	PRBool			folderSelected;
	PRBool			discoveredFromLsub;

	const char		*smtpPostAddress;
	PRBool			onlineVerified;

	nsIMAPNamespace	*namespaceForFolder;
	PRBool			folderIsNamespace;
};

typedef struct mailbox_spec mailbox_spec;


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
	struct mailbox_spec *boxSpec;
} StreamInfo;

typedef struct _ProgressInfo {
	char *message;
	int percent;
} ProgressInfo;

typedef struct _FolderQueryInfo {
	char *name, *hostName;
	PRBool rv;
} FolderQueryInfo;

typedef struct _StatusMessageInfo {
	PRUint32 msgID;
	char * extraInfo;
} StatusMessageInfo;

typedef struct _UploadMessageInfo {
	PRUint32 newMsgID;
	// XP_File	fileId; *** need to be changed
	char *dataBuffer;
	PRInt32 bytesRemain;
} UploadMessageInfo;

typedef struct _utf_name_struct {
	PRBool toUtf7Imap;
	unsigned char *sourceString;
	unsigned char *convertedString;
} utf_name_struct;


typedef struct _msg_line_info {
    char   *adoptedMessageLine;
    PRUint32 uidOfMessage;
} msg_line_info;


typedef struct _FlagsKeyStruct {
    imapMessageFlagsType flags;
    nsMsgKey key;
} FlagsKeyStruct;

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
	kInProgress,
	kSuccessfulCopy,
	kFailedCopy,
	kSuccessfulDelete,
	kFailedDelete,
	kReadyForAppendData,
	kFailedAppend,
	kInterruptedState
} ImapOnlineCopyState;

typedef struct _folder_rename_struct {
	char		*fHostName;
	char 		*fOldName;
	char		*fNewName;
} folder_rename_struct;

typedef enum {
    	eContinue,
		eContinueNew,
    	eListMyChildren,
    	eNewServerDirectory,
    	eCancelled 
} EMailboxDiscoverStatus;

typedef struct _MessageSizeInfo
{
	char *id;
	char *folderName;
	PRBool idIsUid;
	PRUint32 size;
} MessageSizeInfo;


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
