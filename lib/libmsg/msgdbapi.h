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

#ifndef MSGDBAPI_H
#define MSGDBAPI_H

#include "msgdbtyp.h"

MsgERR MSG_InitDB();
MsgERR MSG_ShutdownDB();

// gets an open db handle 
MsgERR MSG_OpenDB(const char *dbFileName, XP_Bool create, MSG_DBHandle *dbHandle, MSG_DBFolderInfoHandle *handle) ;
MsgERR MSG_CloseDB(MSG_DBHandle dbHandle); 

// compress means try to recover unused space - we use this very sparingly, at the moment 
MsgERR MSG_CommitDB(MSG_DBHandle dbHandle, XP_Bool compress); 

// iteration and enumeration interfaces
MsgERR MSG_DBHandle_ListAllKeys(MSG_DBHandle dbHandle, MessageKey **resultKeys, int32 *numKeys);

MsgERR MSG_DBHandle_CreateHdrListIterator(MSG_DBHandle dbHandle, XP_Bool forward, MSG_IteratorHandle *iterator, MSG_HeaderHandle *);
MsgERR MSG_IteratorHandle_GetNextHeader(MSG_IteratorHandle iterator, MSG_DBHandle, MSG_HeaderHandle *);

MsgERR MSG_DBHandle_CreateThreadListIterator(MSG_DBHandle dbHandle, XP_Bool forward, MSG_IteratorHandle *iterator, MSG_ThreadHandle *);
MsgERR MSG_IteratorHandle_GetNextThread(MSG_IteratorHandle iterator, MSG_DBHandle, MSG_ThreadHandle *);
void	MSG_IteratorHandle_DestroyIterator(MSG_IteratorHandle iterator);

// routines to manage mail and news db folder infos.
MSG_MailDBFolderInfoHandle MSG_CreateMailDBFolderInfo();
MSG_NewsDBFolderInfoHandle MSG_CreateNewsDBFolderInfo();

void MSG_DBFolderInfoHandle_RemoveReference(MSG_DBFolderInfoHandle handle);

MsgERR MSG_AddDBFolderInfo(MSG_DBHandle dbHandle, MSG_DBFolderInfoHandle handle);
MsgERR MSG_AddMailDBFolderInfo(MSG_DBHandle dbHandle, MSG_MailDBFolderInfoHandle handle); 
MsgERR MSG_AddNewsDBFolderInfo(MSG_DBHandle dbHandle, MSG_NewsDBFolderInfoHandle handle); 
MsgERR MSG_GetMailDBFolderInfo(MSG_DBHandle dbHandle, MSG_MailDBFolderInfoHandle *handle);
MsgERR MSG_GetNewsDBFolderInfo(MSG_DBHandle dbHandle, MSG_NewsDBFolderInfoHandle *handle);

void MSG_DBFolderInfo_SetFolderInfo(MSG_DBFolderInfoHandle folderInfoHandle, MSG_DBFolderInfoExchange *exchangeInfo, MSG_DBHandle dbHandle);
void MSG_DBFolderInfo_GetFolderInfo(MSG_DBFolderInfoHandle folderInfoHandle, MSG_DBFolderInfoExchange *exchangeInfo);

 void   MSG_DBFolderInfo_SetHighWater(MSG_DBFolderInfoHandle folderInfoHandle, MessageKey highWater, Bool force = 
FALSE) ;
 MessageKey  MSG_DBFolderInfo_GetHighWater(MSG_DBFolderInfoHandle folderInfoHandle);
 void    MSG_DBFolderInfo_SetExpiredMark(MSG_DBFolderInfoHandle folderInfoHandle, MessageKey expiredKey);
 int     MSG_DBFolderInfo_GetDiskVersion(MSG_DBFolderInfoHandle folderInfoHandle) ;
 XP_Bool    MSG_DBFolderInfo_AddLaterKey(MSG_DBFolderInfoHandle folderInfoHandle, MessageKey key, time_t until);
 int32    MSG_DBFolderInfo_GetNumLatered(MSG_DBFolderInfoHandle folderInfoHandle);
 MessageKey   MSG_DBFolderInfo_GetLateredAt(MSG_DBFolderInfoHandle folderInfoHandle, int32 laterIndex, time_t *pUntil);
 void    MSG_DBFolderInfo_RemoveLateredAt(MSG_DBFolderInfoHandle folderInfoHandle, int32 laterIndex);

 void MSG_DBFolderInfo_SetNewArtsSet(MSG_DBFolderInfoHandle folderInfoHandle, const char *newArtsSet, MSG_DBHandle dbHandle);
 void  MSG_DBFolderInfo_GetNewArtsSet(MSG_DBFolderInfoHandle folderInfoHandle, char **newArtsSet);

 void  MSG_DBFolderInfo_SetMailboxName(MSG_DBFolderInfoHandle folderInfoHandle, const char *newBoxName);
 void  MSG_DBFolderInfo_GetMailboxName(MSG_DBFolderInfoHandle folderInfoHandle, char **boxName);

void    MSG_DBFolderInfo_SetViewType(MSG_DBFolderInfoHandle folderInfoHandle, int32 viewType);
 int32    MSG_DBFolderInfo_GetViewType(MSG_DBFolderInfoHandle folderInfoHandle);
 void    MSG_DBFolderInfo_SetSortInfo(MSG_DBFolderInfoHandle folderInfoHandle, SortType, SortOrder);
 void    MSG_DBFolderInfo_GetSortInfo(MSG_DBFolderInfoHandle folderInfoHandle, SortType *, SortOrder *);
 int32    MSG_DBFolderInfo_ChangeNumNewMessages(MSG_DBFolderInfoHandle folderInfoHandle, int32 delta);
 int32    MSG_DBFolderInfo_ChangeNumMessages(MSG_DBFolderInfoHandle folderInfoHandle, int32 delta);
 int32    MSG_DBFolderInfo_ChangeNumVisibleMessages(MSG_DBFolderInfoHandle folderInfoHandle, int32 delta);
 int32    MSG_DBFolderInfo_GetNumNewMessages(MSG_DBFolderInfoHandle folderInfoHandle);
 int32    MSG_DBFolderInfo_GetNumMessages(MSG_DBFolderInfoHandle folderInfoHandle);
 int32    MSG_DBFolderInfo_GetNumVisibleMessages(MSG_DBFolderInfoHandle folderInfoHandle);
 int32    MSG_DBFolderInfo_GetFlags(MSG_DBFolderInfoHandle folderInfoHandle);
 void    MSG_DBFolderInfo_SetFlags(MSG_DBFolderInfoHandle folderInfoHandle, int32 flags);
 void    MSG_DBFolderInfo_OrFlags(MSG_DBFolderInfoHandle folderInfoHandle, int32 flags);
 void    MSG_DBFolderInfo_AndFlags(MSG_DBFolderInfoHandle folderInfoHandle, int32 flags);
 XP_Bool    MSG_DBFolderInfo_TestFlag(MSG_DBFolderInfoHandle folderInfoHandle, int32 flags);
 int16    MSG_DBFolderInfo_GetCSID(MSG_DBFolderInfoHandle folderInfoHandle);
 void    MSG_DBFolderInfo_SetCSID(MSG_DBFolderInfoHandle folderInfoHandle, int16 csid) ;
 int16    MSG_DBFolderInfo_GetIMAPHierarchySeparator(MSG_DBFolderInfoHandle folderInfoHandle);
 void    MSG_DBFolderInfo_SetIMAPHierarchySeparator(MSG_DBFolderInfoHandle folderInfoHandle, int16 hierarchySeparator); 
 int32    MSG_DBFolderInfo_GetImapTotalPendingMessages(MSG_DBFolderInfoHandle folderInfoHandle) ;
 void    MSG_DBFolderInfo_ChangeImapTotalPendingMessages(MSG_DBFolderInfoHandle folderInfoHandle, int32 delta);
 int32    GetImapUnreadPendingMessages(MSG_DBFolderInfoHandle folderInfoHandle);
 void    MSG_DBFolderInfo_ChangeImapUnreadPendingMessages(MSG_DBFolderInfoHandle folderInfoHandle, int32 delta) ;
 
 int32    MSG_DBFolderInfo_GetImapUidValidity(MSG_DBFolderInfoHandle folderInfoHandle) ;
 void    MSG_DBFolderInfo_SetImapUidValidity(MSG_DBFolderInfoHandle folderInfoHandle, int32 uidValidity) ;

 MessageKey   MSG_DBFolderInfo_GetLastMessageLoaded(MSG_DBFolderInfoHandle folderInfoHandle);
 void    MSG_DBFolderInfo_SetLastMessageLoaded(MSG_DBFolderInfoHandle folderInfoHandle, MessageKey lastLoaded);

void    MSG_DBFolderInfo_SetCachedPassword(MSG_DBFolderInfoHandle folderInfoHandle,  const char *password, MSG_DBHandle db);
void    MSG_DBFolderInfo_GetCachedPassword(MSG_DBFolderInfoHandle folderInfoHandle,  char **password, MSG_DBHandle db);

void	MSG_NewsDBFolderInfoHandle_SetKnownArts(MSG_NewsDBFolderInfoHandle handle, const char *knownArts);
void	MSG_NewsDBFolderInfoHandle_GetKnownArts(MSG_NewsDBFolderInfoHandle handle, char **knownArts);

// apis to add, remove, and retrieve various message headers from a DB.
MSG_HeaderHandle MSG_DBHandle_GetHandleForKey(MSG_DBHandle handle, MessageKey messageKey); 
MSG_HeaderHandle MSG_DBHandle_GetHandleForMessageID(MSG_DBHandle handle, const char *msgID);
MSG_ThreadHandle MSG_DBHandle_GetThreadHandleForMsgHdrSubject(MSG_DBHandle handle, MSG_HeaderHandle);
MessageKey		MSG_DBHandle_GetHighwaterMark(MSG_DBHandle);
// message header apis
void MSG_HeaderHandle_AddReference(MSG_HeaderHandle handle);
void MSG_HeaderHandle_RemoveReference(MSG_HeaderHandle handle);

MSG_HeaderHandle GetNewMailHeaderHandle();
MSG_HeaderHandle GetNewNewsHeaderHandle();

void MSG_DBHandle_RemoveHeader(MSG_DBHandle, MSG_HeaderHandle);
MsgERR MSG_DBHandle_AddHeader(MSG_DBHandle, MSG_HeaderHandle);

MsgERR MSG_DBHandle_ExpireRange(MSG_DBHandle dbHandle, MessageKey startRange, MessageKey endRange);

/* header data setters and getters */
void MSG_HeaderHandle_GetHeaderInfo(MSG_HeaderHandle, MSG_DBHeaderExchange *);

MessageKey MSG_HeaderHandle_GetMessageKey(MSG_HeaderHandle);
void MSG_HeaderHandle_SetMessageKey(MSG_HeaderHandle headerHandle, MessageKey key);
uint32 MSG_HeaderHandle_GetMessageSize(MSG_HeaderHandle headerHandle);
void MSG_HeaderHandle_SetMessageSize(MSG_HeaderHandle, uint32 msgSize);
void MSG_HeaderHandle_SetDate(MSG_HeaderHandle, time_t date);
time_t MSG_HeaderHandle_GetDate(MSG_HeaderHandle);
void MSG_HeaderHandle_SetLevel(MSG_HeaderHandle, char level);
char MSG_HeaderHandle_GetLevel(MSG_HeaderHandle);
MessageKey MSG_HeaderHandle_GetThreadID(MSG_HeaderHandle);
void MSG_HeaderHandle_SetThreadID(MSG_HeaderHandle, MessageKey threadID);
uint32 MSG_HeaderHandle_GetByteLength(MSG_HeaderHandle);
uint32 MSG_HeaderHandle_GetLineCount(MSG_HeaderHandle);
void MSG_HeaderHandle_SetByteLength(MSG_HeaderHandle, uint32 byteLength);
void MSG_HeaderHandle_SetStatusOffset(MSG_HeaderHandle, uint16 headerOffset);
uint16 MSG_HeaderHandle_GetStatusOffset(MSG_HeaderHandle);

void MSG_HeaderHandle_CopyFromMsgHdr(MSG_HeaderHandle destHdrHandle, MSG_HeaderHandle srcHdrHandle, MSG_DBHandle srcDBHandle, MSG_DBHandle destDBHandle);
void MSG_HeaderHandle_CopyFromMailMsgHdr(MSG_HeaderHandle destHdrHandle, MSG_HeaderHandle srcHdrHandle, MSG_DBHandle srcDBHandle, MSG_DBHandle destDBHandle);
void MSG_HeaderHandle_CopyFromNewsMsgHdr(MSG_HeaderHandle destHdrHandle, MSG_HeaderHandle srcHdrHandle, MSG_DBHandle srcDBHandle, MSG_DBHandle destDBHandle);
void MSG_HeaderHandle_CopyToMessageHdr(MSG_HeaderHandle handle, MessageHdrStruct *msgHdr, MSG_DBHandle db);
void MSG_HeaderHandle_CopyToShortMessageHdr(MSG_HeaderHandle handle, MSG_MessageLine *msgHdr, MSG_DBHandle db);
XP_Bool MSG_HeaderHandle_SetSubject(MSG_HeaderHandle handle, const char * subject, MSG_DBHandle db);
XP_Bool MSG_HeaderHandle_SetAuthor(MSG_HeaderHandle handle, const char * author, MSG_DBHandle db);
XP_Bool MSG_HeaderHandle_SetMessageId(MSG_HeaderHandle handle, const char * messageId, MSG_DBHandle db);
XP_Bool MSG_HeaderHandle_SetReferences(MSG_HeaderHandle handle, const char * references, MSG_DBHandle db);

XP_Bool MSG_HeaderHandle_GetSubject(MSG_HeaderHandle handle, char **subject, XP_Bool withRE, MSG_DBHandle db);
XP_Bool MSG_HeaderHandle_GetAuthor(MSG_HeaderHandle handle, char **author, MSG_DBHandle db);
XP_Bool MSG_HeaderHandle_GetMessageId(MSG_HeaderHandle handle, char **messageId, MSG_DBHandle db);
int32 MSG_HeaderHandle_GetNumReferences(MSG_HeaderHandle handle);

void MSG_HeaderHandle_SetPriority(MSG_HeaderHandle handle, MSG_PRIORITY priority);
MSG_PRIORITY MSG_HeaderHandle_GetPriority(MSG_HeaderHandle handle);
int32 MSG_HeaderHandle_GetNumRecipients(MSG_HeaderHandle handle);
int32 MSG_HeaderHandle_GetNumCCRecipients(MSG_HeaderHandle handle);
void MSG_HeaderHandle_GenerateAddressList(MSG_HeaderHandle hdrHandle, MSG_DBHandle dbHandle, char **addressList);
void MSG_HeaderHandle_GenerateCCAddressList(MSG_HeaderHandle hdrHandle, MSG_DBHandle dbHandle, char **addressList);
void MSG_HeaderHandle_GetFullRecipient(MSG_HeaderHandle hdrHandle, MSG_DBHandle dbHandle, int whichRecipient, char **recipient);
void MSG_HeaderHandle_GetFullCCRecipient(MSG_HeaderHandle hdrHandle, MSG_DBHandle dbHandle, int whichRecipient, char **recipient);

uint32 MSG_HeaderHandle_OrFlags(MSG_HeaderHandle hdrHandle, uint32 flags);
uint32 MSG_HeaderHandle_SetFlags(MSG_HeaderHandle hdrHandle, uint32 flags);
uint32 MSG_HeaderHandle_AndFlags(MSG_HeaderHandle hdrHandle, uint32 flags);
uint32 MSG_HeaderHandle_GetFlags(MSG_HeaderHandle hdrHandle);
uint32 MSG_HeaderHandle_SetFlags(MSG_HeaderHandle hdrHandle, uint32 flags);

// recipients are either comma separated list of rfc 822 addresses, or a newsgroup.
void MSG_HeaderHandle_SetRecipients(MSG_HeaderHandle hdrHandle, const char *recipients, MSG_DBHandle db, XP_Bool rfc822AddressList);
void MSG_HeaderHandle_SetCCRecipients(MSG_HeaderHandle hdrHandle, const char *ccList, MSG_DBHandle db);

// threading interfaces - these are a bit ugly in order to make threading as efficient as possible.

// returns the thread header handle for the reference with the passed index in the passed header, NULL if it doesn't exist.
MSG_ThreadHandle MSG_HeaderHandle_GetThreadForReference(MSG_HeaderHandle hdrHandle, int32 refIndex, MSG_DBHandle dbHandle);
void MSG_ThreadHandle_AddChild(MSG_ThreadHandle threadHandle, MSG_HeaderHandle headerHandle, MSG_DBHandle dbHandle, XP_Bool threadInThread);
XP_Bool MSG_ThreadHandle_GetSubject(MSG_ThreadHandle handle, char **subject, MSG_DBHandle db);
MessageKey MSG_ThreadHandle_GetChildAt(MSG_ThreadHandle handle, uint16 index);
MSG_HeaderHandle MSG_ThreadHandle_GetChildHdrAt(MSG_ThreadHandle handle, uint16 index);
MSG_HeaderHandle MSG_ThreadHandle_GetChildForKey(MSG_ThreadHandle handle, MessageKey key);
void MSG_ThreadHandle_MarkChildRead(MSG_ThreadHandle thread, XP_Bool bRead, MSG_DBHandle db);
void MSG_ThreadHandle_RemoveChildByKey(MSG_ThreadHandle, MessageKey key, MSG_DBHandle db);
uint16 MSG_ThreadHandle_GetNumChildren(MSG_ThreadHandle);
MSG_ThreadHandle MSG_DBHandle_AddThreadFromMsgHandle(MSG_DBHandle, MSG_HeaderHandle);

void MSG_DBHandle_RemoveThread(MSG_DBHandle, MSG_ThreadHandle);

// thread header apis
void MSG_ThreadHandle_AddReference(MSG_ThreadHandle handle);
void MSG_ThreadHandle_RemoveReference(MSG_ThreadHandle handle);

void MSG_ThreadHandle_GetThreadInfo(MSG_ThreadHandle, MSG_DBThreadExchange *);
MSG_ThreadHandle MSG_DBHandle_GetThreadHeaderForThreadID(MSG_DBHandle, MessageKey threadID);
// offline operation api's
void MSG_DBHandle_AddOfflineOperation(MSG_DBHandle dbHandle, MSG_OfflineIMAPOperationHandle offlineOpHandle);  
void MSG_DBHandle_RemoveOfflineOperation(MSG_DBHandle dbHandle, MSG_OfflineIMAPOperationHandle offlineOpHandle);  
MSG_OfflineIMAPOperationHandle MSG_DBHandle_GetOfflineOp(MSG_DBHandle dbHandle, MessageKey messageKey);
MSG_OfflineIMAPOperationHandle GetOfflineIMAPOperation();
void MSG_OfflineIMAPOperationHandle_RemoveReference(MSG_OfflineIMAPOperationHandle opHandle);

void MSG_OfflineIMAPOperationHandle_SetMessageKey(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle, MessageKey key);
MessageKey MSG_OfflineIMAPOperationHandle_GetMessageKey(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle);
MessageKey MSG_OfflineIMAPOperationHandle_GetSourceMessageKey(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle);

void MSG_OfflineIMAPOperationHandle_SetAppendMsgOperation(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle,
														  const char *destinationBox, int32 opType);
void MSG_OfflineIMAPOperationHandle_ClearAppendMsgOperation(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle,
														  int32 opType);
void MSG_OfflineIMAPOperationHandle_SetMessageMoveOperation(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle,
														  const char *destinationBox);
void MSG_OfflineIMAPOperationHandle_ClearMoveOperation(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle);
void MSG_OfflineIMAPOperationHandle_AddMessageCopyOperation(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle,
														  const char *destinationBox);
void MSG_OfflineIMAPOperationHandle_ClearFirstCopyOperation(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle);
MsgERR MSG_OfflineIMAPOperationHandle_SetSourceMailbox(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle, const char *sourceMailbox);
XP_Bool MSG_OfflineIMAPOperationHandle_GetIndexedCopyDestination(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle, uint32 index, char **boxName);

XP_Bool MSG_OfflineIMAPOperationHandle_GetMoveDestination(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle, char **boxName);
uint32 MSG_OfflineIMAPOperationHandle_GetNumberOfCopyOps(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle);

imapMessageFlagsType MSG_OfflineIMAPOperationHandle_GetNewMessageFlags(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle); 
uint32 MSG_OfflineIMAPOperationHandle_GetOperationFlags(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle);
void MSG_OfflineIMAPOperationHandle_SetDeleteAllMsgs(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle);
void MSG_OfflineIMAPOperationHandle_ClearDeleteAllMsgs(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle);
void MSG_OfflineIMAPOperationHandle_SetInitialImapFlags(MSG_OfflineIMAPOperationHandle opHandle, imapMessageFlagsType flags, MSG_DBHandle dbHandle);
void MSG_OfflineIMAPOperationHandle_SetImapFlagOperation(MSG_OfflineIMAPOperationHandle opHandle, imapMessageFlagsType flags, MSG_DBHandle dbHandle);
void MSG_OfflineIMAPOperationHandle_ClearImapFlagOperation(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle);
MsgERR MSG_OfflineIMAPOperationHandle_SetSourceMailbox(MSG_OfflineIMAPOperationHandle opHandle, MSG_DBHandle dbHandle, const char *mailbox, MessageKey key);
MsgERR MSG_DBHandle_ListAllOflineOperationKeys(MSG_DBHandle dbHandle, MessageKey **resultKeys, int32 *numKeys);
MsgERR MSG_DBHandle_ListAllOfflineDeletedMessageKeys(MSG_DBHandle, MessageKey **deletedKeys, int32 *numKeys);

// offline message body apis.
int32 MSG_HeaderHandle_AddToOfflineMessage(MSG_HeaderHandle hdrHandle, const char *block, int32 length, MSG_DBHandle dbHandle);
int32 MSG_HeaderHandle_ReadFromOfflineMessage(MSG_HeaderHandle hdrHandle, char *block, int32 length, int32 offset, MSG_DBHandle dbHandle);
void MSG_HeaderHandle_PurgeOfflineMessage(MSG_HeaderHandle hdrHandle, MSG_DBHandle dbHandle);
int32 MSG_HeaderHandle_WriteOfflineMessageBody(MSG_HeaderHandle hdrHandle, MSG_DBHandle dbHandle, XP_File	destinationFile);
int32 MSG_HeaderHandle_GetOfflineMessageLength(MSG_HeaderHandle hdrHandle, MSG_DBHandle dbHandle);

// Offline Message Document api's

MSG_OfflineMsgDocumentHandle MSG_OfflineMsgDocumentHandle_Create(MSG_DBHandle dbHandle, MSG_HeaderHandle hdrHandle);
void						  MSG_OfflineMsgDocumentHandle_Destroy(MSG_OfflineMsgDocumentHandle);
void						  MSG_OfflineMsgDocumentHandle_Complete(MSG_OfflineMsgDocumentHandle);
void						  MSG_OfflineMsgDocumentHandle_SetMsgHeaderHandle(MSG_OfflineMsgDocumentHandle, MSG_HeaderHandle hdrHandle, MSG_DBHandle dbHandle);
int32						  MSG_OfflineMsgDocumentHandle_AddToOfflineMessage(MSG_OfflineMsgDocumentHandle, const char *block, int32 length);
#endif
