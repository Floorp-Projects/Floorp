/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef __imap__
#define __imap__

#include "structs.h"
#include "msgcom.h"


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

#define kImapMsgSupportUserFlag 0x8000		/* This seems to be the most cost effective way of
											 * piggying back the server support user flag
											 * info.
											 */

typedef enum {
	kPersonalNamespace = 0,
	kOtherUsersNamespace,
	kPublicNamespace,
	kUnknownNamespace
} EIMAPNamespaceType;

typedef enum {
	kCapabilityUndefined = 0x00000000,
	kCapabilityDefined = 0x00000001,
	kHasAuthLoginCapability = 0x00000002,
	kHasXNetscapeCapability = 0x00000004,
	kHasXSenderCapability = 0x00000008,
	kIMAP4Capability = 0x00000010,		/* RFC1734 */
	kIMAP4rev1Capability = 0x00000020,	/* RFC2060 */
	kIMAP4other = 0x00000040,			/* future rev?? */
	kNoHierarchyRename = 0x00000080,			/* no hierarchy rename */
	kACLCapability = 0x00000100,	/* ACL extension */
	kNamespaceCapability = 0x00000200,	/* IMAP4 Namespace Extension */
	kMailboxDataCapability = 0x00000400,	/* MAILBOXDATA SMTP posting extension */
	kXServerInfoCapability = 0x00000800	/* XSERVERINFO extension for admin urls */
} eIMAPCapabilityFlag;

typedef int32 imap_uid;

#ifdef XP_CPLUSPLUS
class TImapFlagAndUidState;
#else
typedef struct TImapFlagAndUidState TImapFlagAndUidState;
#endif

/* forward declaration */
typedef void ImapActiveEntry;

/* url used to signify that filtering is complete so
   we can tell the fe that the inbox thread pane is 
   loaded */
#define kImapFilteringCompleteURL "Mailbox://?filteringcomplete"

/* url used to signify that online/offline synch is complete */
#define kImapOnOffSynchCompleteURL "Mailbox://?onoffsynchcomplete"

/* if a url creator does not know the hierarchySeparator, use this */
#define kOnlineHierarchySeparatorUnknown ' '

struct mailbox_spec {
	int32 		  	folder_UIDVALIDITY;
	int32			number_of_messages;
	int32 		  	number_of_unseen_messages;
	int32			number_of_recent_messages;
	
	uint32			box_flags;
	
	char          *allocatedPathName;
	char			hierarchySeparator;
	const char		*hostName;
	
	TNavigatorImapConnection *connection;
	TImapFlagAndUidState     *flagState;
	
	XP_Bool			folderSelected;
	XP_Bool			discoveredFromLsub;

	const char		*smtpPostAddress;
	XP_Bool			onlineVerified;
};

typedef struct mailbox_spec mailbox_spec;


enum EIMAPSubscriptionUpgradeState {
	kEverythingDone,
	kBringUpSubscribeUI
};

enum ImapOnlineCopyState {
	kInProgress,
	kSuccessfulCopy,
	kFailedCopy,
	kSuccessfulDelete,
	kFailedDelete,
	kReadyForAppendData,
	kFailedAppend,
	kInterruptedState
};

struct folder_rename_struct {
	char		*fHostName;
	char 		*fOldName;
	char		*fNewName;
};


typedef struct folder_rename_struct folder_rename_struct;


/* this file defines the syntax of the imap4 url's and offers functions
   that create url strings.  If the functions do not offer enough 
   functionality then let kevin know before you starting creating strings
   from scratch. */
#include "xp_mcom.h"

XP_BEGIN_PROTOS

/* need mailbox status urls to get the number of message and the
  number of unread messages */

/* Selecting a mailbox */
/* imap4://HOST?select?MAILBOXPATH */
char *CreateImapMailboxSelectUrl(const char *imapHost, 
								 const char *mailbox,
								 char  hierarchySeparator,
								 const char *undoDeleteIdentifierList);

/* lite select, used to verify UIDVALIDITY while going on/offline */
char *CreateImapMailboxLITESelectUrl(const char *imapHost, 
								 	const char *mailbox,
								 	char  hierarchySeparator);

/* expunge, used in traditional imap delete model */
char *CreateImapMailboxExpungeUrl(const char *imapHost, 
								 	const char *mailbox,
								 	char  hierarchySeparator);

/* Creating a mailbox */
/* imap4://HOST?create?MAILBOXPATH */
char *CreateImapMailboxCreateUrl(const char *imapHost, const char *mailbox, char  hierarchySeparator);

/* discover the children of this mailbox */
char *CreateImapChildDiscoveryUrl(const char *imapHost, const char *mailbox, char  hierarchySeparator);

/* discover the n-th level children of this mailbox */
char *CreateImapLevelChildDiscoveryUrl(const char *imapHost, const char *mailbox, char  hierarchySeparator, int n);

/* discover the mailboxes of this account  */
char *CreateImapAllMailboxDiscoveryUrl(const char *imapHost);

/* discover the mailboxes of this account, and the subscribed mailboxes  */
char *CreateImapAllAndSubscribedMailboxDiscoveryUrl(const char *imapHost);

/* deleting a mailbox */
/* imap4://HOST?delete?MAILBOXPATH */
char *CreateImapMailboxDeleteUrl(const char *imapHost, const char *mailbox, char  hierarchySeparator);

/* renaming a mailbox */
/* imap4://HOST?rename?OLDNAME?NEWNAME */
char *CreateImapMailboxRenameLeafUrl(const char *imapHost, 
								     const char *oldBoxPathName,
								     char  hierarchySeparator,
								     const char *newBoxLeafName);

/* renaming a mailbox, moving hierarchy */
/* imap4://HOST?movefolderhierarchy?OLDNAME?NEWNAME */
/* oldBoxPathName is the old name of the child folder */
/* destinationBoxPathName is the name of the new parent */
char *CreateImapMailboxMoveFolderHierarchyUrl(const char *imapHost, 
								              const char *oldBoxPathName,
								              char  oldHierarchySeparator,
								              const char *destinationBoxPathName,
								              char  newHierarchySeparator);

/* listing available mailboxes */
/* imap4://HOST?list */
char *CreateImapListUrl(const char *imapHost,
						const char *mailbox,
						const char hierarchySeparator);

/* biff */
char *CreateImapBiffUrl(const char *imapHost,
						const char *mailbox,
						char  hierarchySeparator,
						uint32 uidHighWater);



/* fetching RFC822 messages */
/* imap4://HOST?fetch?<UID/SEQUENCE>?MAILBOXPATH?x */
/*   'x' is the message UID or sequence number list */
/* will set the 'SEEN' flag */
char *CreateImapMessageFetchUrl(const char *imapHost,
								const char *mailbox,
								char  hierarchySeparator,
								const char *messageIdentifierList,
								XP_Bool messageIdsAreUID);
								
								
/* fetching the headers of RFC822 messages */
/* imap4://HOST?header?<UID/SEQUENCE>?MAILBOXPATH?x */
/*   'x' is the message UID or sequence number list */
/* will not affect the 'SEEN' flag */
char *CreateImapMessageHeaderUrl(const char *imapHost,
								 const char *mailbox,
								 char  hierarchySeparator,
								 const char *messageIdentifierList,
								 XP_Bool messageIdsAreUID);
								 

/* search an online mailbox */
/* imap4://HOST?search?<UID/SEQUENCE>?MAILBOXPATH?SEARCHSTRING */
char *CreateImapSearchUrl(const char *imapHost,
						  const char *mailbox,
						  char  hierarchySeparator,
						  const char *searchString,
						  XP_Bool messageIdsAreUID);

/* delete messages */
/* imap4://HOST?deletemsg?<UID/SEQUENCE>?MAILBOXPATH?x */
/*   'x' is the message UID or sequence number list */
char *CreateImapDeleteMessageUrl(const char *imapHost,
								 const char *mailbox,
								 char  hierarchySeparator,
								 const char *messageIds,
								 XP_Bool idsAreUids);
								 
/* delete all messages */
/* imap4://HOST?deleteallmsgs?MAILBOXPATH */
char *CreateImapDeleteAllMessagesUrl(const char *imapHost,
								     const char *mailbox,
									 char  hierarchySeparator);

/* store +flags url */
/* imap4://HOST?store+flags?<UID/SEQUENCE>?MAILBOXPATH?x?f */
/*   'x' is the message UID or sequence number list */
/*   'f' is the byte of flags */
char *CreateImapAddMessageFlagsUrl(const char *imapHost,
								   const char *mailbox,
								   char  hierarchySeparator,
								   const char *messageIds,
								   imapMessageFlagsType flags,
								   XP_Bool idsAreUids);
/* store -flags url */
/* imap4://HOST?store-flags?<UID/SEQUENCE>?MAILBOXPATH?x?f */
/*   'x' is the message UID or sequence number list */
/*   'f' is the byte of flags */
char *CreateImapSubtractMessageFlagsUrl(const char *imapHost,
								        const char *mailbox,
								   		char  hierarchySeparator,
								        const char *messageIds,
								        imapMessageFlagsType flags,
								        XP_Bool idsAreUids);

/* set flags url, make the flags match */
char *CreateImapSetMessageFlagsUrl(const char *imapHost,
								        const char *mailbox,
								   		char  hierarchySeparator,
								        const char *messageIds,
								        imapMessageFlagsType flags,
								        XP_Bool idsAreUids);


/* copy messages from one online box to another */
/* imap4://HOST?onlineCopy?<UID/SEQUENCE>?
         SOURCEMAILBOXPATH?x?DESTINATIONMAILBOXPATH */
/*   'x' is the message UID or sequence number list */
char *CreateImapOnlineCopyUrl(const char *imapHost,
							  const char *sourceMailbox,
							  char  sourceHierarchySeparator,
							  const char *messageIds,
							  const char *destinationMailbox,
							  char  destinationHierarchySeparator,
							  XP_Bool idsAreUids,
							  XP_Bool isMove);	/* cause delete after copy */

/* copy a message from an online box to an offline box */
/* imap4://HOST?ontooffCopy?SOURCEMAILBOXPATH?number=x?
         DESTINATIONMAILBOXPATH */
/*   'x' is the message sequence number */
char *CreateImapOnToOfflineCopyUrl( const char *imapHost,
							  		const char *sourceMailbox,
							  		char  sourceHierarchySeparator,
							  		const char *messageIds,
							  		const char *destinationMailbox,
							  		XP_Bool idsAreUids,
							  		XP_Bool isMove);	/* cause delete after copy */

/* copy messages from an offline box to an online box */
/* imap4://HOST?offtoonCopy?DESTINATIONMAILBOXPATH */
/*   the number of messages and their sizes are negotiated later */
char *CreateImapOffToOnlineCopyUrl(const char *imapHost,
							       const char *destinationMailbox,
							  	   char  destinationHierarchySeparator);

/* Subscribe to a mailbox on the given IMAP host */
char *CreateIMAPSubscribeMailboxURL(const char *imapHost, const char *mailboxName);

/* Unsubscribe from a mailbox on the given IMAP host */
char *CreateIMAPUnsubscribeMailboxURL(const char *imapHost, const char *mailboxName);

/* get mail account rul */
/* imap4://HOST?NETSCAPE */
char *CreateImapManageMailAccountUrl(const char *imapHost);

/* append message from file */
/* imap4://HOST?appendmsgfromfile?MSGFILEPATH?DESTINATIONMAILBOXPATH */
char *CreateImapAppendMessageFromFileUrl(const char *imapHost,
										 const char *destinationMailboxPath,
										 const char hierarchySeparator,
										 XP_Bool isDraft);

/* refresh the ACL for a folder */
char *CreateIMAPRefreshACLForFolderURL(const char *imapHost, const char *mailbox);

/* refresh the ACL for all folders on given host*/
char *CreateIMAPRefreshACLForAllFoldersURL(const char *imapHost);

/* Run the auto-upgrade to IMAP Subscription */
char *CreateIMAPUpgradeToSubscriptionURL(const char *imapHost, XP_Bool subscribeToAll);

/* do a status command for the folder */
char *CreateIMAPStatusFolderURL(const char *imapHost, const char *mailboxName, char  hierarchySeparator);

/* refresh the imap folder urls for the folder */
char *CreateIMAPRefreshFolderURLs(const char *imapHost, const char *mailboxName);

/* create a URL to force an all-parts reload of the fetch URL given in url. */
char *IMAP_CreateReloadAllPartsUrl(const char *url);

/* Explicitly LIST a given mailbox, and refresh its flags in the folder list */
char *CreateIMAPListFolderURL(const char *imapHost, const char *mailboxName);

NET_StreamClass *CreateIMAPDownloadMessageStream(ImapActiveEntry *ce, uint32 size,
												 const char *content_type, XP_Bool content_modified);

void UpdateIMAPMailboxInfo(mailbox_spec *adoptedBoxSpec, MWContext *currentContext);
void UpdateIMAPMailboxStatus(mailbox_spec *adoptedBoxSpec, MWContext *currentContext);

#define kUidUnknown -1
int32 GetUIDValidityForSpecifiedImapFolder(const char *hostName, const char *canonicalimapName, MWContext *currentContext);

enum EMailboxDiscoverStatus {
    	eContinue,
		eContinueNew,
    	eListMyChildren,
    	eNewServerDirectory,
    	eCancelled };

enum EMailboxDiscoverStatus DiscoverIMAPMailbox(mailbox_spec *adoptedBoxSpec, MSG_Master *master,
												MWContext *currentContext, XP_Bool broadcastDiscovery);

void ReportSuccessOfOnlineCopy(MWContext *currentContext, enum ImapOnlineCopyState copyState);
void ReportSuccessOfOnlineDelete(MWContext *currentContext, const char *hostName, const char *mailboxName);
void ReportFailureOfOnlineCreate(MWContext *currentContext, const char *mailboxName);
void ReportSuccessOfOnlineRename(MWContext *currentContext, folder_rename_struct *names);
void ReportMailboxDiscoveryDone(MWContext *currentContext, URL_Struct *URL_s);
void ReportSuccessOfChildMailboxDiscovery(MWContext *currentContext);
void NotifyHeaderFetchCompleted(MWContext *currentContext, TNavigatorImapConnection *imapConnection);

void ReportLiteSelectUIDVALIDITY(MSG_Pane *receivingPane, uint32 UIDVALIDITY);

typedef void (UploadCompleteFunctionPointer)(void*);
void BeginMessageUpload(MWContext *currentContext, 
						PRFileDesc *ioSocket,
						UploadCompleteFunctionPointer *completeFunction, 
						void *completionFunctionArgument);

void IMAP_DoNotDownLoadAnyMessageHeadersForMailboxSelect(TNavigatorImapConnection *connection);
void IMAP_DownLoadMessagesForMailboxSelect(TNavigatorImapConnection *connection,
									       uint32 *messageKeys,	/* uint32* is adopted */
									       uint32 numberOfKeys);
									       
void IMAP_DownLoadMessageBodieForMailboxSelect(TNavigatorImapConnection *connection,
									           uint32 *messageKeys,	/* uint32* is adopted */
									           uint32 numberOfKeys);

void IMAP_BodyIdMonitor(TNavigatorImapConnection *connection, XP_Bool enter);

char *IMAP_GetCurrentConnectionUrl(TNavigatorImapConnection *connection);

void IMAP_UploadAppendMessageSize(TNavigatorImapConnection *connection, uint32 msgSize, imapMessageFlagsType flags);
void IMAP_ResetAnyCachedConnectionInfo();

XP_Bool IMAP_CheckNewMail(TNavigatorImapConnection *connection);
XP_Bool IMAP_NewMailDetected(TNavigatorImapConnection *connection);

TImapFlagAndUidState *IMAP_CreateFlagState(int numberOfMessages);
void				  IMAP_DeleteFlagState(TImapFlagAndUidState *state);
int 				  IMAP_GetFlagStateNumberOfMessages(TImapFlagAndUidState *state);
	
imap_uid 		     IMAP_GetUidOfMessage(int zeroBasedIndex, TImapFlagAndUidState *state);
imapMessageFlagsType IMAP_GetMessageFlags(int zeroBasedIndex, TImapFlagAndUidState *state);
imapMessageFlagsType IMAP_GetMessageFlagsFromUID(imap_uid uid, XP_Bool *foundIt, TImapFlagAndUidState *state);

void                 IMAP_TerminateConnection (TNavigatorImapConnection *connection);

char				*IMAP_CreateOnlineSourceFolderNameFromUrl(const char *url);

void				 IMAP_FreeBoxSpec(mailbox_spec *victim);

const char			*IMAP_GetPassword();
void				 IMAP_SetPassword(const char *password);
void				IMAP_SetPasswordForHost(const char *host, const char *password);
/* called once only by MSG_InitMsgLib */
void 				IMAP_StartupImap();

/* called once only by MSG_ShutdownMsgLib */
void 				IMAP_ShutdownImap();

/* used to prevent recursive listing of mailboxes during discovery */
int64 IMAP_GetTimeStampOfNonPipelinedList();

/* returns TRUE if either we have a password or we were preAuth'd by SSL certs */
XP_Bool IMAP_HaveWeBeenAuthenticated();

/* used by libmsg when creating an imap message display stream */
int IMAP_InitializeImapFeData (ImapActiveEntry * ce);
MSG_Pane *IMAP_GetActiveEntryPane(ImapActiveEntry * ce);
NET_StreamClass *IMAP_CreateDisplayStream(ImapActiveEntry * ce, XP_Bool clearCacheBit, uint32 size, const char *content_type);

/* used by libmsg when a new message is loaded to interrupt the load of the previous message */
void				IMAP_PseudoInterruptFetch(MWContext *context);

void IMAP_URLFinished(URL_Struct *URL_s);

XP_Bool IMAP_HostHasACLCapability(const char *hostName);

/**** IMAP Host stuff - used for communication between MSG_IMAPHost (in libmsg) and TImapHostInfo (in libnet) ****/

/* obsolete? */
/*void	IMAP_SetNamespacesFromPrefs(const char *hostName, char *personalDir, char *publicDir, char *otherUsersDir);*/

/* Sets the preference of whether or not we should always explicitly LIST the INBOX for given host */
void	IMAP_SetShouldAlwaysListInboxForHost(const char *hostName, XP_Bool shouldList);

/* Gets the number of namespaces in use for a given host */
int		IMAP_GetNumberOfNamespacesForHost(const char *hostName);

/* Sets the currently-used default personal namespace for a given host.  Used for updating from libnet when
   we get a NAMESPACE response. */
void MSG_SetNamespacePrefixes(MSG_Master *master, const char *hostName, EIMAPNamespaceType type, const char *prefix);


/* Check to see if we need upgrade to IMAP subscription */
extern XP_Bool	MSG_ShouldUpgradeToIMAPSubscription(MSG_Master *mast);
extern void		MSG_ReportSuccessOfUpgradeToIMAPSubscription(MWContext *context, enum EIMAPSubscriptionUpgradeState *state);

/* Adds a set of ACL rights for the given folder on the given host for the given user.  If userName is NULL, it means
   the currently authenticated user (i.e. my rights). */
extern void MSG_AddFolderRightsForUser(MSG_Master *master, const char *hostName, const char*mailboxName, const char *userName, const char *rights);

/* Clears all ACL rights for the given folder on the given host for all users. */
extern void MSG_ClearFolderRightsForFolder(MSG_Master *master, const char *hostName, const char *mailboxName);

/* Refreshes the icon / flags for a given folder, based on new ACL rights */
extern void MSG_RefreshFolderRightsViewForFolder(MSG_Master *master, const char *hostName, const char *mailboxName);

extern XP_Bool MSG_GetFolderNeedsSubscribing(MSG_FolderInfo *folder);

/* Returns TRUE if this folder needs an auto-refresh of the ACL (on a folder open, for example) */
extern XP_Bool MSG_GetFolderNeedsACLListed(MSG_FolderInfo *folder);

/* Returns TRUE if this folder has NEVER (ever) had an ACL retrieved for it */
extern XP_Bool MSG_IsFolderACLInitialized(MSG_Master *master, const char *folderName, const char *hostName);

extern char *IMAP_SerializeNamespaces(char **prefixes, int len);
extern int IMAP_UnserializeNamespaces(const char *str, char **prefixes, int len);

extern XP_Bool IMAP_SetHostIsUsingSubscription(const char *hostname, XP_Bool using_subscription);

/* Returns the runtime capabilities of the given host, or 0 if the host doesn't exist or if 
   they are uninitialized so far */
extern uint32	IMAP_GetCapabilityForHost(const char *hostName);

/* Causes a libmsg MSG_IMAPHost to refresh its capabilities based on new runtime info */
extern void MSG_CommitCapabilityForHost(const char *hostName, MSG_Master *master);

/* Returns TRUE if the given folder is \Noselect.  Returns FALSE if it's not or if we don't
   know about it. */
extern XP_Bool MSG_IsFolderNoSelect(MSG_Master *master, const char *folderName, const char *hostName);

XP_END_PROTOS

#endif
