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
/*   msgnet.h --- prototypes for API's between libmsg and libnet.
 */

#ifndef _MSGNET_H_
#define _MSGNET_H_
#include "msgcom.h"
#include "dirprefs.h"

XP_BEGIN_PROTOS

/* so libnet can mark expired articles as read. */
extern int MSG_MarkMessageKeyRead (MSG_Pane *pane, MessageKey key, const char *xref);

/* record imap message flags in the db of the current pane (message or thread panes) */
extern void MSG_RecordImapMessageFlags(MSG_Pane* pane, 
									   MessageKey msgKey, 
									   imapMessageFlagsType flags);

/* notify libmsg of deleted messages */
extern void MSG_ImapMsgsDeleted(MSG_Pane *urlPane,
								const char *onlineMailboxName,
								const char *hostName,
								XP_Bool deleteAllMsgs, 
                    			const char *doomedKeyString);

/* called to setup state before a copy download */
extern void MSG_StartImapMessageToOfflineFolderDownload(MWContext* context);

/* notify libmsg that inbox filtering is complete */
extern void MSG_ImapInboxFilteringComplete(MSG_Pane *urlPane);

/* notify libmsg that the online/offline synch is complete */              				
extern void MSG_ImapOnOffLineSynchComplete(MSG_Pane *urlPane);

/* notify libmsg that an imap folder load was interrupted */
extern void MSG_InterruptImapFolderLoad(MSG_Pane *urlPane, const char *hostName, const char *onlineFolderPath);

/* find a reference or NULL to the specified imap folder */
extern MSG_FolderInfo* MSG_FindImapFolder(MSG_Pane *urlPane, const char *hostName, const char *onlineFolderPath);

/* Used to kill connections after removing them from the cache so biff wont hang around them */
extern void MSG_IMAP_KillConnection(TNavigatorImapConnection *imapConnection);

/* If there is a cached connection, for this folder, uncache it and return it */
extern TNavigatorImapConnection* MSG_UnCacheImapConnection(MSG_Master* master, const char *host, const char *folderName);

/* Cache this connection and return TRUE if there is not one there already, else false */
extern XP_Bool MSG_TryToCacheImapConnection(MSG_Master* master, const char *host, const char *folderName, TNavigatorImapConnection *connection);

extern void MSG_SetIMAPMessageUID (MessageKey key,
								   void *state);


extern const char *MSG_GetMessageIdFromState (void *state);
extern XP_Bool MSG_IsSaveDraftDeliveryState (void *state);

extern void MSG_SetUserAuthenticated (MSG_Master* master, XP_Bool bAuthenticated);

extern XP_Bool MSG_IsUserAuthenticated (MSG_Master* master);

extern void MSG_SetMailAccountURL(MSG_Master* master, const char *urlString);
extern void MSG_SetHostMailAccountURL(MSG_Master* master, const char *hostName, const char *urlString);
extern void MSG_SetHostManageListsURL(MSG_Master* master, const char *hostName, const char *urlString);
extern void MSG_SetHostManageFiltersURL(MSG_Master* master, const char *hostName, const char *urlString);

extern const char *MSG_GetMailAccountURL(MSG_Master* master);

extern void MSG_SetNewsgroupUsername(MSG_Pane* pane, const char *username);
extern const char *MSG_GetNewsgroupUsername(MSG_Pane *pane);

extern void MSG_SetNewsgroupPassword(MSG_Pane* pane, const char *password);
extern const char *MSG_GetNewsgroupPassword(MSG_Pane *pane);

extern const char* MSG_GetPopHost(MSG_Prefs* prefs);

extern XP_Bool MSG_GetUseSSLForIMAP4(MSG_Pane *pane);
extern int32 MSG_GetIMAPSSLPort(MSG_Pane *pane);

/* called from libnet to clean up state from mailbox:copymessages */
extern void MSG_MessageCopyIsCompleted (struct MessageCopyInfo**);

/* called from libnet to determine if the current copy is finished */
extern XP_Bool MSG_IsMessageCopyFinished(struct MessageCopyInfo*);

/* called from mailbox and url libnet modules */
extern
int MSG_BeginCopyingMessages(MWContext *context);

extern
int MSG_FinishCopyingMessages(MWContext *context);

extern const char *MSG_GetIMAPHostUsername(MSG_Master *master, const char *hostName);
extern const char *MSG_GetIMAPHostPassword(MSG_Master *master, const char *hostName);
extern void MSG_SetIMAPHostPassword(MSG_Master *master, const char *hostName, const char *password);
extern int MSG_GetIMAPHostIsUsingSubscription(MSG_Master *master, const char *hostName, XP_Bool *usingSubscription);
extern XP_Bool MSG_GetIMAPHostDeleteIsMoveToTrash(MSG_Master *master, const char *hostName);
extern MSG_FolderInfo *MSG_GetTrashFolderForHost(MSG_IMAPHost *host);
extern int IMAP_AddIMAPHost(const char *hostName, XP_Bool usingSubscription, XP_Bool overrideNamespaces,
							const char *personalNamespacePrefix, const char *publicNamespacePrefixes, 
							const char *otherUsersNamespacePrefixes, XP_Bool haveAdminUrl);
extern XP_Bool MSG_GetIMAPHostIsSecure(MSG_Master *master, const char *hostName);

typedef enum
{	MSG_NotRunning		= 0x00000000
,	MSG_RunningOnline	= 0x00000001
,	MSG_RunningOffline	= 0x00000002
} MSG_RunningState;

extern MSG_FolderInfo *MSG_SetFolderRunningIMAPUrl(MSG_Pane *urlPane, const char *hostName, const char *onlineFolderPath, MSG_RunningState runningState);
extern void MSG_IMAPUrlFinished(MSG_FolderInfo *folder, URL_Struct *URL_s);

/* ===========================================================================
   								OFFLINE IMAP
   ===========================================================================
 */

extern uint32 MSG_GetImapMessageFlags(MSG_Pane *urlPane, 
										const char *hostName,
										const char *onlineBoxName, 
										uint32 key);

extern void MSG_StartOfflineImapRetrieval(MSG_Pane *urlPane, 
										const char *hostName,
											const char *onlineBoxName, 
											uint32 key,
											void **offLineRetrievalData);
											
extern uint32 MSG_GetOfflineMessageSize(void *offLineRetrievalData);

extern int MSG_ProcessOfflineImap(void *offLineRetrievalData, char *socketBuffer, uint32 read_size);

extern int MSG_InterruptOfflineImap(void *offLineRetrievalData);

extern void MSG_GetNextURL(MSG_Pane *pane);

/* Returns the original pane that a progress pane was associated with.  If
   the given pane is not a progresspane, returns NULL. */
extern MSG_Pane* MSG_GetParentPane(MSG_Pane* progresspane);

/* do an imap biff of the imap inbox */
extern void MSG_ImapBiff(MWContext* context, MSG_Prefs* prefs);
extern MWContext *MSG_GetBiffContext();

extern void MSG_SetFolderAdminURL(MSG_Master *master, const char *hostName, const char*mailboxName, const char *url);

/* The NNTP module of netlib calls these to feed XOVER data to the message
   library, in response to a news:group.name URL having been opened.
   If MSG_FinishXOVER() returns a message ID, that message will be loaded
   next (used for selecting the first unread message in a group after
   listing that group.)

   The "out" arguments are (if non-NULL) a file descriptor to write the XOVER
   line to, followed by a "\n".  This is used by the XOVER-caching code.
 */
extern int MSG_InitXOVER (MSG_Pane* pane,
						  MSG_NewsHost* host,
						  const char* group_name,
						  uint32 first_msg, uint32 last_msg,
						  uint32 oldest_msg, uint32 youngest_msg,
						  void **data);
extern int MSG_ProcessXOVER (MSG_Pane* pane, char *line, void **data);
extern int MSG_ProcessNonXOVER (MSG_Pane* pane, char *line, void **data);
extern int MSG_FinishXOVER (MSG_Pane* pane, void **data, int status);

/* In case of XOVER failed due to the authentication process, we need to
   do some clean up. So that we could have a fresh start once we pass the
   authentication check.
*/
extern int MSG_ResetXOVER (MSG_Pane* pane, void **data);


/* These calls are used by libnet to determine which articles it ought to
   get in a big newsgroup. */

extern int
MSG_GetRangeOfArtsToDownload(MSG_Pane* pane, void** data, MSG_NewsHost* host,
							 const char* group_name,
							 int32 first_possible, /* Oldest article available
													  from newsserver*/
							 int32 last_possible, /* Newest article available
													 from newsserver*/
							 int32 maxextra,
							 int32* first,
							 int32* last);


extern int
MSG_AddToKnownArticles(MSG_Pane *pane, MSG_NewsHost* host,
					   const char* groupname, int32 first, int32 last);

extern int MSG_InitAddArticleKeyToGroup(MSG_Pane *pane, MSG_NewsHost* host,
									 const char* groupName,  void **parseState);

extern int	MSG_AddArticleKeyToGroup(void *parse_state, int32 first);

extern int	MSG_FinishAddArticleKeyToGroup(MSG_Pane *pane, void **parse_state);

/* After displaying a list of newsgroups, we need the NNTP module to go and
   run "GROUP" commands for the ones for which we don't know the unread
   article count.  This function returns a count of how many groups we think
   we're going to need this for (so we can display progress in a reasonable
   way).
 */
extern int32 MSG_GetNewsRCCount(MSG_Pane* pane, MSG_NewsHost* host);

/* Gets the name of the next group that we want to get article counts for.
   The caller must free the given name using XP_FREE().
   MSG_DisplaySubscribedGroup() should get called with this group before
   this call happens again. */

extern char* MSG_GetNewsRCGroup(MSG_Pane* pane, MSG_NewsHost* host);



/* In response to a "news://host/" URL; this is called once for each group
   that was returned by MSG_GetNewsRCGroup(), after the NNTP GROUP command has
   been run.  It's also called whenever we actually visit the group (the user
   clicks on the newsgroup line), in case the data has changed since the
   initial passthrough.  The "nowvisiting" parameter is TRUE in the latter
   case, FALSE otherwise. */
extern int MSG_DisplaySubscribedGroup(MSG_Pane* pane,
									  MSG_NewsHost* host,
									  const char *group,
									  int32 oldest_message,
									  int32 youngest_message,
									  int32 total_messages,
									  XP_Bool nowvisiting);

/* In response to an NNTP GROUP command, the server said the group doesn't exist */
extern int MSG_GroupNotFound(MSG_Pane* pane,
							 MSG_NewsHost* host,
							 const char *group,
							 XP_Bool opening);

/* In response to a "news://host/?newgroups" URL, to ask the server for a 
   list of recently-added newsgroups.  Similar to MSG_DisplaySubscribedGroup,
   except that in this case, the group is not already in the list. */
extern int MSG_DisplayNewNewsGroup (MWContext *context,
									MSG_NewsHost* host, const char *group_name,
									int32 oldest_message,
									int32 youngest_message);


/* News servers work better if you ask for message numbers instead of IDs.
   So, the NNTP module asks us what the group and number of an ID is with
   this.  If we don't know, we return 0 for both.   If the pane is not a 
   thead or message pane, this routine will fail.
 */
extern void MSG_NewsGroupAndNumberOfID (MSG_Pane *pane,
										const char *message_id,
										const char **group_return,
										uint32 *message_number_return);

/*	This routine is used by netlib to see if we have this article off-line 
	It might be combined with the above routine, but I'm not sure if this
	is the way we're ultimately going to do this.
*/
extern XP_Bool MSG_IsOfflineArticle		(MSG_Pane *pane,
										const char *message_id,
										const char **group_return,
										uint32 *message_number_return);

extern int MSG_StartOfflineRetrieval(MSG_Pane *pane,
										const char *group,
										uint32 message_number,
										void **offlineState);

extern int MSG_ProcessOfflineNews(void *offlineState, char *outputBuffer, int outputBufSize);

extern int MSG_InterruptOfflineNews(void *offlineState);

/* libnet callbacks for Dredd NNTP extensions */

extern void MSG_SupportsNewsExtensions (MSG_NewsHost *host, XP_Bool supports);
extern void MSG_AddNewsExtension (MSG_NewsHost *host, const char *ext);
extern XP_Bool MSG_QueryNewsExtension (MSG_NewsHost *host, const char *ext);
extern XP_Bool MSG_NeedsNewsExtension (MSG_NewsHost *host, const char *ext);

extern void MSG_AddSearchableGroup (MSG_NewsHost *host, const char *group);
extern void MSG_AddSearchableHeader (MSG_NewsHost *host, const char *header);
extern int MSG_AddProfileGroup (MSG_Pane *pane,
								MSG_NewsHost* host,
								const char *groupName);

extern int MSG_AddPrettyName(MSG_NewsHost* host,
							 const char *groupName, const char *prettyName);

extern int 	MSG_SetXActiveFlags(MSG_Pane *pane, char *groupName, 
								int32 firstPossibleArt, 
								int32 lastPossibleArt, 
								char *flags);

extern int MSG_AddSubscribedGroup (MSG_Pane *pane, const char *groupUrl);

extern void MSG_AddPropertyForGet (MSG_NewsHost *host, const char *property,
								   const char *value);

/* libnet calls this if it got an error 441 back from the newsserver.  That
   error almost certainly means that the newsserver already has a message
   with the same message id.  If this routine returns TRUE, then we were
   pretty much expecting that error code, because we know we tried twice to
   post the same message, and we can just ignore it. */
extern XP_Bool MSG_IsDuplicatePost(MSG_Pane* comppane);


/* libnet uses this on an error condition to tell libmsg to generate a new
   message-id for the given composition. */
extern void MSG_ClearCompositionMessageID(MSG_Pane* comppane);


/* libnet uses this to determine the message-id for the given composition (so
   it can test if this message was already posted.) */
extern const char* MSG_GetCompositionMessageID(MSG_Pane* comppane);

/* The "news:" and "mailbox:" protocol handlers call this when a message is
   displayed, so that we can use the contents of the headers when composing
   replies.
 */
extern void 
MSG_ActivateReplyOptions(MSG_Pane* messagepane, MimeHeaders *headers);

/* Tell the subscribe pane about a new newsgroup we noticed. */

extern int
MSG_AddNewNewsGroup(MSG_Pane* pane, MSG_NewsHost* host,
					const char* groupname, int32 oldest, int32 youngest,
					const char *flag, XP_Bool bXactiveFlags);

/* tell the host info database that we're going to need to get the extra info
   for this new newsgroup
*/
extern int		MSG_SetGroupNeedsExtraInfo(MSG_NewsHost *host,
					const char* groupname, XP_Bool needsExtra);

/* returns the name of the first group which needs extra info */
extern char *MSG_GetFirstGroupNeedingExtraInfo(MSG_NewsHost *host);

/* Find out from libmsg when we last checked for new newsgroups (so we know
   what date to give the "newgroups" command.) */

extern time_t
MSG_NewsgroupsLastUpdatedTime(MSG_NewsHost* host);

					

/* The "mailbox:" protocol module uses these routines to invoke the mailbox
   parser in libmsg.
 */
extern int MSG_BeginOpenFolderSock (MSG_Pane* pane,
									const char *folder_name,
									const char *message_id, int32 msgnum,
									void **folder_ptr);
extern int MSG_FinishOpenFolderSock (MSG_Pane* pane,
									 const char *folder_name,
									 const char *message_id, int32 msgnum,
									 void **folder_ptr);
extern void MSG_CloseFolderSock (MSG_Pane* pane, const char *folder_name,
								 const char *message_id, int32 msgnum,
								 void *folder_ptr);
extern int MSG_OpenMessageSock (MSG_Pane* messagepane, const char *folder_name,
								const char *msg_id, int32 msgnum,
								void *folder_ptr, void **message_ptr,
								int32 *content_length);
extern int MSG_ReadMessageSock (MSG_Pane* messagepane, const char *folder_name,
								void *message_ptr, const char *message_id,
								int32 msgnum, char *buffer, int32 buffer_size);
extern void MSG_CloseMessageSock (MSG_Pane* messagepane,
								  const char *folder_name,
								  const char *message_id, int32 msgnum,
								  void *message_ptr);
extern void MSG_PrepareToIncUIDL(MSG_Pane* messagepane, URL_Struct* url,
								 const char* uidl);

/* This is how "mailbox:?empty-trash" works
 */
extern int MSG_BeginEmptyTrash(MSG_Pane* folderpane, URL_Struct* url,
							   void** closure);
extern int MSG_FinishEmptyTrash(MSG_Pane* folderpane, URL_Struct* url,
								void* closure);
extern int MSG_CloseEmptyTrashSock(MSG_Pane* folderpane, URL_Struct* url,
								   void* closure);

/* This is how "mailbox:?compress-folder" and
   "mailbox:/foo/baz/nsmail/inbox?compress-folder" works. */

extern int MSG_BeginCompressFolder(MSG_Pane* pane, URL_Struct* url,
								   const char* foldername, void** closure);
extern int MSG_FinishCompressFolder(MSG_Pane* pane, URL_Struct* url,
									const char* foldername, void* closure);
extern int MSG_CloseCompressFolderSock(MSG_Pane* pane, URL_Struct* url,
									   void* closure);
/* This is how "mailbox:?deliver-queued" works
 */
extern int MSG_BeginDeliverQueued(MSG_Pane* pane, URL_Struct* url,
								  void** closure);
extern int MSG_FinishDeliverQueued(MSG_Pane* pane, URL_Struct* url,
								   void* closure);
extern int MSG_CloseDeliverQueuedSock(MSG_Pane* pane, URL_Struct* url,
									  void* closure);

/* This is how "mailbox:?background" works */
extern int MSG_ProcessBackground(URL_Struct* urlstruct);

/* libnet --> libmsg glue for newsgroup searching */
extern void MSG_AddNewsXpatHit (MWContext *context, uint32 artNum);
extern void MSG_AddNewsSearchHit (MWContext *context, const char *resultLine);

/* libnet --> libmsg glue for imap mail folder searching */
extern void MSG_AddImapSearchHit (MWContext *context, const char *resultLine);
/* The POP3 protocol module uses these routines to hand us new messages.
 */
extern XP_Bool MSG_BeginMailDelivery (MSG_Pane* folderpane);
extern void MSG_AbortMailDelivery (MSG_Pane* folderpane);
extern void MSG_EndMailDelivery (MSG_Pane* folderpane);
extern void *MSG_IncorporateBegin (MSG_Pane* folderpane,
								   FO_Present_Types format_out,
								   char *pop3_uidl,
								   URL_Struct *url,
								   uint32 flags);
extern int MSG_IncorporateWrite (MSG_Pane* folderpane, void *closure,
								 const char *block, int32 length);
extern int MSG_IncorporateComplete(MSG_Pane* folderpane, void *closure);
extern int MSG_IncorporateAbort (MSG_Pane* folderpane, void *closure,
								 int status);
extern void MSG_ClearSenderAuthedFlag(MSG_Pane* folderpane, void *closure);




/* This is how the netlib registers the converters relevant to MIME message
   display and composition.
 */
void MSG_RegisterConverters (void);

extern void
MSG_StartMessageDelivery (MSG_Pane *pane,
						  void      *fe_data,
						  MSG_CompositionFields *fields,
						  XP_Bool digest_p,
						  XP_Bool dont_deliver_p,
						  const char *attachment1_type,
						  const char *attachment1_body,
						  uint32 attachment1_body_length,
						  const struct MSG_AttachmentData *attachments,
						  void *mimeRelatedPart, /* only used in compose pane */
						  void (*message_delivery_done_callback)
						       (MWContext *context, 
								void *fe_data,
								int status,
								const char *error_message));

/* When a message which has the `partial' bit set, meaning we only downloaded
   the first 20 lines because it was huge, this function will be called to 
   return some HTML to tack onto the end of the message to explain that it
   is truncated, and provide a clickable link which will download the whole
   message.
 */
extern char *MSG_GeneratePartialMessageBlurb (MSG_Pane* messagepane,
											  URL_Struct *url, void *closure,
											  MimeHeaders *headers);


extern int MSG_GetUrlQueueSize (const char *url, MWContext *context);

extern XP_Bool MSG_RequestForReturnReceipt(MSG_Pane* pane);
extern XP_Bool MSG_SendingMDNInProgress(MSG_Pane* pane);

extern uint32 MSG_GetIMAPMessageSizeFromDB(MSG_Pane *masterPane, const char *hostName, char *folderName, char *id, XP_Bool idIsUid);

extern void MSG_RefreshFoldersForUpdatedIMAPHosts(MWContext *context);

extern XP_Bool MSG_MailCheck(MWContext *context, MSG_Prefs *prefs);

extern void MSG_Pop3MailCheck(MWContext *context);

extern int NET_parse_news_url (const char *url,
					char **host_and_portP,
					XP_Bool *securepP,
					char **groupP,
					char **message_idP,
					char **command_specific_dataP);

extern char *MSG_GetArbitraryHeadersForHost(MSG_Master *master, const char *hostName);

/* Directory Server Replication
 */
extern XP_Bool NET_ReplicateDirectory(MSG_Pane *pane, DIR_Server *server);

XP_END_PROTOS


#endif
