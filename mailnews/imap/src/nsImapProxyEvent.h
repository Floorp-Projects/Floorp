/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsImapProxyEvent_h__
#define nsImapProxyEvent_h__

#include "plevent.h"
#include "prthread.h"
#include "nsISupports.h"
#include "nsIURL.h"
#include "nsIImapLog.h"
#include "nsIImapMailFolderSink.h"
#include "nsIImapMessageSink.h"
#include "nsIImapExtensionSink.h"
#include "nsIImapMiscellaneousSink.h"

class nsImapProxyBase
{
public:
    nsImapProxyBase(nsIImapProtocol* aProtocol,
                    PLEventQueue* aEventQ,
                    PRThread* aThread);
    virtual ~nsImapProxyBase();

    PLEventQueue* m_eventQueue;
    PRThread* m_thread;
    nsIImapProtocol* m_protocol;
};

class nsImapLogProxy : public nsIImapLog, 
                       public nsImapProxyBase
{
public:
	nsImapLogProxy(nsIImapLog* aImapLog,
                 nsIImapProtocol* aProtocol,
                 PLEventQueue* aEventQ,
                 PRThread* aThread);
	virtual ~nsImapLogProxy();

	NS_DECL_ISUPPORTS

	NS_IMETHOD HandleImapLogData(const char* aLogData);

	nsIImapLog* m_realImapLog;
};

class nsImapMailFolderSinkProxy : public nsIImapMailFolderSink, 
                              public nsImapProxyBase
{
public:
    nsImapMailFolderSinkProxy(nsIImapMailFolderSink* aImapMailFolderSink,
                          nsIImapProtocol* aProtocol,
                          PLEventQueue* aEventQ,
                          PRThread* aThread);
    virtual ~nsImapMailFolderSinkProxy();
    
    NS_DECL_ISUPPORTS

    // Tell mail master about a discovered imap mailbox
    NS_IMETHOD PossibleImapMailbox(nsIImapProtocol* aProtocol,
                                   mailbox_spec* aSpec);
    NS_IMETHOD MailboxDiscoveryDone(nsIImapProtocol* aProtocol);
    // Tell mail master about the newly selected mailbox
    NS_IMETHOD UpdateImapMailboxInfo(nsIImapProtocol* aProtocol,
                                     mailbox_spec* aSpec);
    NS_IMETHOD UpdateImapMailboxStatus(nsIImapProtocol* aProtocol,
                                       mailbox_spec* aSpec);
    NS_IMETHOD ChildDiscoverySucceeded(nsIImapProtocol* aProtocol);
    NS_IMETHOD OnlineFolderDelete(nsIImapProtocol* aProtocol,
                                  const char* folderName);
    NS_IMETHOD OnlineFolderCreateFailed(nsIImapProtocol* aProtocol,
                                        const char* folderName);
    NS_IMETHOD OnlineFolderRename(nsIImapProtocol* aProtocol,
                                  folder_rename_struct* aStruct);
    NS_IMETHOD SubscribeUpgradeFinished(nsIImapProtocol* aProtocol,
                        EIMAPSubscriptionUpgradeState* aState);
    NS_IMETHOD PromptUserForSubscribeUpdatePath(nsIImapProtocol* aProtocol,
                                                PRBool* aBool);
    NS_IMETHOD FolderIsNoSelect(nsIImapProtocol* aProtocol,
                                FolderQueryInfo* aInfo);

    NS_IMETHOD SetupHeaderParseStream(nsIImapProtocol* aProtocol,
                                   StreamInfo* aStreamInfo);

    NS_IMETHOD ParseAdoptedHeaderLine(nsIImapProtocol* aProtocol,
                                   msg_line_info* aMsgLineInfo);
    
    NS_IMETHOD NormalEndHeaderParseStream(nsIImapProtocol* aProtocol);
    
    NS_IMETHOD AbortHeaderParseStream(nsIImapProtocol* aProtocol);
    
    nsIImapMailFolderSink* m_realImapMailFolderSink;
};

class nsImapMessageSinkProxy : public nsIImapMessageSink, 
                           public nsImapProxyBase
{
public:
    nsImapMessageSinkProxy(nsIImapMessageSink* aImapMessageSink,
                       nsIImapProtocol* aProtocol,
                       PLEventQueue* aEventQ,
                       PRThread* aThread);
    virtual ~nsImapMessageSinkProxy();

    NS_DECL_ISUPPORTS 

    // set up messge download output stream
    // FE calls nsIImapProtocol::SetMessageDownloadOutputStream 
    NS_IMETHOD SetupMsgWriteStream(nsIImapProtocol* aProtocol,
                                   StreamInfo* aStreamInfo);

    NS_IMETHOD ParseAdoptedMsgLine(nsIImapProtocol* aProtocol,
                                   msg_line_info* aMsgLineInfo);
    
    NS_IMETHOD NormalEndMsgWriteStream(nsIImapProtocol* aProtocol);
    
    NS_IMETHOD AbortMsgWriteStream(nsIImapProtocol* aProtocol);
    
    // message move/copy related methods
    NS_IMETHOD OnlineCopyReport(nsIImapProtocol* aProtocol,
                                ImapOnlineCopyState* aCopyState);
    NS_IMETHOD BeginMessageUpload(nsIImapProtocol* aProtocol);
    NS_IMETHOD UploadMessageFile(nsIImapProtocol* aProtocol,
                                 UploadMessageInfo* aMsgInfo);

    // message flags operation
    NS_IMETHOD NotifyMessageFlags(nsIImapProtocol* aProtocol,
                                  FlagsKeyStruct* aKeyStruct);

    NS_IMETHOD NotifyMessageDeleted(nsIImapProtocol* aProtocol,
                                    delete_message_struct* aStruct);
    NS_IMETHOD GetMessageSizeFromDB(nsIImapProtocol* aProtocol,
                                    MessageSizeInfo* sizeInfo);

    nsIImapMessageSink* m_realImapMessageSink;
};

class nsImapExtensionSinkProxy : public nsIImapExtensionSink, 
                             public nsImapProxyBase
{
public:
    nsImapExtensionSinkProxy(nsIImapExtensionSink* aImapExtensionSink,
                         nsIImapProtocol* aProtocol,
                         PLEventQueue* aEventQ,
                         PRThread* aThread);
    virtual ~nsImapExtensionSinkProxy();

    NS_DECL_ISUPPORTS
  
    NS_IMETHOD SetUserAuthenticated(nsIImapProtocol* aProtocol,
                                    PRBool aBool);
    NS_IMETHOD SetMailServerUrls(nsIImapProtocol* aProtocol,
                                 const char* hostName);
    NS_IMETHOD SetMailAccountUrl(nsIImapProtocol* aProtocol,
                                 const char* hostName);
    NS_IMETHOD ClearFolderRights(nsIImapProtocol* aProtocol,
                                 nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD AddFolderRights(nsIImapProtocol* aProtocol,
                             nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD RefreshFolderRights(nsIImapProtocol* aProtocol,
                                   nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
                                         nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD SetFolderAdminURL(nsIImapProtocol* aProtocol,
                                 FolderQueryInfo* aInfo);
    
    nsIImapExtensionSink* m_realImapExtensionSink;
};

class nsImapMiscellaneousSinkProxy : public nsIImapMiscellaneousSink, 
                                 public nsImapProxyBase
{
public:
    nsImapMiscellaneousSinkProxy (nsIImapMiscellaneousSink* aImapMiscellaneousSink,
                              nsIImapProtocol* aProtocol,
                              PLEventQueue* aEventQ,
                              PRThread* aThread);
    ~nsImapMiscellaneousSinkProxy ();

    NS_DECL_ISUPPORTS
	
    NS_IMETHOD AddSearchResult(nsIImapProtocol* aProtocol, 
														 const char* searchHitLine);
    NS_IMETHOD GetArbitraryHeaders(nsIImapProtocol* aProtocol,
                                   GenericInfo* aInfo);
    NS_IMETHOD GetShouldDownloadArbitraryHeaders(nsIImapProtocol* aProtocol,
                                                 GenericInfo* aInfo);
    NS_IMETHOD GetShowAttachmentsInline(nsIImapProtocol* aProtocol,
                                        PRBool* aBool);
    NS_IMETHOD HeaderFetchCompleted(nsIImapProtocol* aProtocol);
    NS_IMETHOD UpdateSecurityStatus(nsIImapProtocol* aProtocol);
    // ****
    NS_IMETHOD FinishImapConnection(nsIImapProtocol* aProtocol);
    NS_IMETHOD SetImapHostPassword(nsIImapProtocol* aProtocol,
                                   GenericInfo* aInfo);
    NS_IMETHOD GetPasswordForUser(nsIImapProtocol* aProtocol,
                                  const char* userName);
    NS_IMETHOD SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
                                     nsMsgBiffState biffState);
    NS_IMETHOD GetStoredUIDValidity(nsIImapProtocol* aProtocol,
                                    uid_validity_info* aInfo);
    NS_IMETHOD LiteSelectUIDValidity(nsIImapProtocol* aProtocol,
                                     PRUint32 uidValidity);
    NS_IMETHOD FEAlert(nsIImapProtocol* aProtocol,
                       const char* aString);
    NS_IMETHOD FEAlertFromServer(nsIImapProtocol* aProtocol,
                                 const char* aString);
    NS_IMETHOD ProgressStatus(nsIImapProtocol* aProtocol,
                              const char* statusMsg);
    NS_IMETHOD PercentProgress(nsIImapProtocol* aProtocol,
                               ProgressInfo* aInfo);
    NS_IMETHOD PastPasswordCheck(nsIImapProtocol* aProtocol);
    NS_IMETHOD CommitNamespaces(nsIImapProtocol* aProtocol,
                                const char* hostName);
    NS_IMETHOD CommitCapabilityForHost(nsIImapProtocol* aProtocol,
                                       const char* hostName);
    NS_IMETHOD TunnelOutStream(nsIImapProtocol* aProtocol,
														 msg_line_info* aInfo);
    NS_IMETHOD ProcessTunnel(nsIImapProtocol* aProtocol,
                             TunnelInfo *aInfo);

    nsIImapMiscellaneousSink* m_realImapMiscellaneousSink;
};

/* ******* Imap Base Event struct ******** */
struct nsImapEvent : public PLEvent
{
	virtual ~nsImapEvent();
	virtual void InitEvent();

	NS_IMETHOD HandleEvent() = 0;
	void PostEvent(PLEventQueue* aEventQ);

	static void PR_CALLBACK imap_event_handler(PLEvent* aEvent);
	static void PR_CALLBACK imap_event_destructor(PLEvent *aEvent);
};

struct nsImapLogProxyEvent : public nsImapEvent
{
	nsImapLogProxyEvent(nsImapLogProxy* aProxy, 
                      const char* aLogData);
	virtual ~nsImapLogProxyEvent();

	NS_IMETHOD HandleEvent();
	char *m_logData;
	nsImapLogProxy *m_proxy;
};

struct nsImapMailFolderSinkProxyEvent : public nsImapEvent
{
    nsImapMailFolderSinkProxyEvent(nsImapMailFolderSinkProxy* aProxy);
    virtual ~nsImapMailFolderSinkProxyEvent();
    nsImapMailFolderSinkProxy* m_proxy;
};

struct PossibleImapMailboxProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    PossibleImapMailboxProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                                  mailbox_spec* aSpec);
    virtual ~PossibleImapMailboxProxyEvent();
    NS_IMETHOD HandleEvent();

    mailbox_spec m_mailboxSpec;
};

struct MailboxDiscoveryDoneProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    MailboxDiscoveryDoneProxyEvent(nsImapMailFolderSinkProxy* aProxy);
    virtual ~MailboxDiscoveryDoneProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct UpdateImapMailboxInfoProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    UpdateImapMailboxInfoProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                                    mailbox_spec* aSpec);
    virtual ~UpdateImapMailboxInfoProxyEvent();
    NS_IMETHOD HandleEvent();
    mailbox_spec m_mailboxSpec;
};

struct UpdateImapMailboxStatusProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    UpdateImapMailboxStatusProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                                      mailbox_spec* aSpec);
    virtual ~UpdateImapMailboxStatusProxyEvent();
    NS_IMETHOD HandleEvent();
    mailbox_spec m_mailboxSpec;
};

struct ChildDiscoverySucceededProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    ChildDiscoverySucceededProxyEvent(nsImapMailFolderSinkProxy* aProxy);
    virtual ~ChildDiscoverySucceededProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct OnlineFolderDeleteProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    OnlineFolderDeleteProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                                 const char* folderName);
    virtual ~OnlineFolderDeleteProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_folderName;
};

struct OnlineFolderCreateFailedProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    OnlineFolderCreateFailedProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                                       const char* folderName);
    virtual ~OnlineFolderCreateFailedProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_folderName;
};

struct OnlineFolderRenameProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    OnlineFolderRenameProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                                 folder_rename_struct* aStruct);
    virtual ~OnlineFolderRenameProxyEvent();
    NS_IMETHOD HandleEvent();
    folder_rename_struct m_folderRenameStruct;
};

struct SubscribeUpgradeFinishedProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    SubscribeUpgradeFinishedProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                                       EIMAPSubscriptionUpgradeState* aState);
    virtual ~SubscribeUpgradeFinishedProxyEvent();
    NS_IMETHOD HandleEvent();
    EIMAPSubscriptionUpgradeState m_state;
};

struct PromptUserForSubscribeUpdatePathProxyEvent : 
    public nsImapMailFolderSinkProxyEvent 
{
    PromptUserForSubscribeUpdatePathProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                                               PRBool* aBool);
    virtual ~PromptUserForSubscribeUpdatePathProxyEvent();
    NS_IMETHOD HandleEvent();
    PRBool m_bool;
};

struct FolderIsNoSelectProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    FolderIsNoSelectProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                               FolderQueryInfo* aInfo);
    virtual ~FolderIsNoSelectProxyEvent();
    NS_IMETHOD HandleEvent();
    FolderQueryInfo m_folderQueryInfo;
};

struct SetupHeaderParseStreamProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    SetupHeaderParseStreamProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                               StreamInfo* aStreamInfo);
    virtual ~SetupHeaderParseStreamProxyEvent();
    NS_IMETHOD HandleEvent();
    StreamInfo m_streamInfo;
};

struct NormalEndHeaderParseStreamProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    NormalEndHeaderParseStreamProxyEvent(nsImapMailFolderSinkProxy* aProxyo);
    virtual ~NormalEndHeaderParseStreamProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct ParseAdoptedHeaderLineProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    ParseAdoptedHeaderLineProxyEvent(nsImapMailFolderSinkProxy* aProxy,
                               msg_line_info* aMsgLineInfo);
    virtual ~ParseAdoptedHeaderLineProxyEvent();
    NS_IMETHOD HandleEvent();
    msg_line_info m_msgLineInfo;
};

struct AbortHeaderParseStreamProxyEvent : public nsImapMailFolderSinkProxyEvent
{
    AbortHeaderParseStreamProxyEvent(nsImapMailFolderSinkProxy* aProxy);
    virtual ~AbortHeaderParseStreamProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct nsImapMessageSinkProxyEvent : nsImapEvent
{
    nsImapMessageSinkProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy);
    virtual ~nsImapMessageSinkProxyEvent();
    nsImapMessageSinkProxy* m_proxy;
};

struct SetupMsgWriteStreamProxyEvent : nsImapMessageSinkProxyEvent
{
    SetupMsgWriteStreamProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy,
                                  StreamInfo* aStreamInfo);
    virtual ~SetupMsgWriteStreamProxyEvent();
    NS_IMETHOD HandleEvent();
    StreamInfo m_streamInfo;
};

struct ParseAdoptedMsgLineProxyEvent : nsImapMessageSinkProxyEvent
{
    ParseAdoptedMsgLineProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy,
                                  msg_line_info* aMsgLineInfo);
    virtual ~ParseAdoptedMsgLineProxyEvent();
    NS_IMETHOD HandleEvent();
    msg_line_info m_msgLineInfo;
};

struct NormalEndMsgWriteStreamProxyEvent : nsImapMessageSinkProxyEvent
{
    NormalEndMsgWriteStreamProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy);
    virtual ~NormalEndMsgWriteStreamProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct AbortMsgWriteStreamProxyEvent : nsImapMessageSinkProxyEvent
{
    AbortMsgWriteStreamProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy);
    virtual ~AbortMsgWriteStreamProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct OnlineCopyReportProxyEvent : nsImapMessageSinkProxyEvent
{
    OnlineCopyReportProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy,
                               ImapOnlineCopyState* aCopyState);
    virtual ~OnlineCopyReportProxyEvent();
    NS_IMETHOD HandleEvent();
    ImapOnlineCopyState m_copyState;
};

struct BeginMessageUploadProxyEvent : nsImapMessageSinkProxyEvent
{
    BeginMessageUploadProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy);
    virtual ~BeginMessageUploadProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct UploadMessageFileProxyEvent : nsImapMessageSinkProxyEvent
{
    UploadMessageFileProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy,
                                UploadMessageInfo* aMsgInfo);
    virtual ~UploadMessageFileProxyEvent();
    NS_IMETHOD HandleEvent();
    UploadMessageInfo m_msgInfo;
};

struct NotifyMessageFlagsProxyEvent : nsImapMessageSinkProxyEvent
{
    NotifyMessageFlagsProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy,
                                 FlagsKeyStruct* aKeyStruct);
    virtual ~NotifyMessageFlagsProxyEvent();
    NS_IMETHOD HandleEvent();
    FlagsKeyStruct m_keyStruct;
};

struct NotifyMessageDeletedProxyEvent : nsImapMessageSinkProxyEvent
{
    NotifyMessageDeletedProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy,
                                   delete_message_struct* aStruct);
    virtual ~NotifyMessageDeletedProxyEvent();
    NS_IMETHOD HandleEvent();
    delete_message_struct m_deleteMessageStruct;
};

struct GetMessageSizeFromDBProxyEvent : nsImapMessageSinkProxyEvent
{
    GetMessageSizeFromDBProxyEvent(nsImapMessageSinkProxy* aImapMessageSinkProxy,
                                   MessageSizeInfo* sizeInfo);
    virtual ~GetMessageSizeFromDBProxyEvent();
    NS_IMETHOD HandleEvent();
    MessageSizeInfo *m_sizeInfo; // pass in handle we don't own it
};

struct nsImapExtensionSinkProxyEvent : nsImapEvent
{
    nsImapExtensionSinkProxyEvent(nsImapExtensionSinkProxy* aProxy);
    virtual ~nsImapExtensionSinkProxyEvent();
    nsImapExtensionSinkProxy* m_proxy;
};

struct SetUserAuthenticatedProxyEvent : nsImapExtensionSinkProxyEvent
{
    SetUserAuthenticatedProxyEvent(nsImapExtensionSinkProxy* aProxy,
                                   PRBool aBool);
    virtual ~SetUserAuthenticatedProxyEvent();
    NS_IMETHOD HandleEvent();
    PRBool m_bool;
};

struct SetMailServerUrlsProxyEvent : nsImapExtensionSinkProxyEvent
{
    SetMailServerUrlsProxyEvent(nsImapExtensionSinkProxy* aProxy,
                                const char* hostName);
    virtual ~SetMailServerUrlsProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_hostName;
};

struct SetMailAccountUrlProxyEvent : nsImapExtensionSinkProxyEvent
{
    SetMailAccountUrlProxyEvent(nsImapExtensionSinkProxy* aProxy,
                                const char* hostName);
    virtual ~SetMailAccountUrlProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_hostName;
};

struct ClearFolderRightsProxyEvent : nsImapExtensionSinkProxyEvent
{
    ClearFolderRightsProxyEvent(nsImapExtensionSinkProxy* aProxy,
                                nsIMAPACLRightsInfo* aclRights);
    virtual ~ClearFolderRightsProxyEvent();
    NS_IMETHOD HandleEvent();
    nsIMAPACLRightsInfo m_aclRightsInfo;
};

struct AddFolderRightsProxyEvent : nsImapExtensionSinkProxyEvent
{
    AddFolderRightsProxyEvent(nsImapExtensionSinkProxy* aProxy,
                                nsIMAPACLRightsInfo* aclRights);
    virtual ~AddFolderRightsProxyEvent();
    NS_IMETHOD HandleEvent();
    nsIMAPACLRightsInfo m_aclRightsInfo;
};

struct RefreshFolderRightsProxyEvent : nsImapExtensionSinkProxyEvent
{
    RefreshFolderRightsProxyEvent(nsImapExtensionSinkProxy* aProxy,
                                nsIMAPACLRightsInfo* aclRights);
    virtual ~RefreshFolderRightsProxyEvent();
    NS_IMETHOD HandleEvent();
    nsIMAPACLRightsInfo m_aclRightsInfo;
};

struct FolderNeedsACLInitializedProxyEvent : nsImapExtensionSinkProxyEvent
{
    FolderNeedsACLInitializedProxyEvent(nsImapExtensionSinkProxy* aProxy,
                                nsIMAPACLRightsInfo* aclRights);
    virtual ~FolderNeedsACLInitializedProxyEvent();
    NS_IMETHOD HandleEvent();
    nsIMAPACLRightsInfo m_aclRightsInfo;
};

struct SetFolderAdminURLProxyEvent : nsImapExtensionSinkProxyEvent
{
    SetFolderAdminURLProxyEvent(nsImapExtensionSinkProxy* aProxy,
                                FolderQueryInfo* aInfo);
    virtual ~SetFolderAdminURLProxyEvent();
    NS_IMETHOD HandleEvent();
    FolderQueryInfo m_folderQueryInfo;
};

struct nsImapMiscellaneousSinkProxyEvent : public nsImapEvent
{
    nsImapMiscellaneousSinkProxyEvent(nsImapMiscellaneousSinkProxy* aProxy);
    virtual ~nsImapMiscellaneousSinkProxyEvent();
    nsImapMiscellaneousSinkProxy* m_proxy;
};

struct AddSearchResultProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    AddSearchResultProxyEvent(nsImapMiscellaneousSinkProxy* aProxy, 
                              const char* searchHitLine);
    virtual ~AddSearchResultProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_searchHitLine;
};

struct GetArbitraryHeadersProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    GetArbitraryHeadersProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                                  GenericInfo* aInfo);
    virtual ~GetArbitraryHeadersProxyEvent();
    NS_IMETHOD HandleEvent();
    GenericInfo *m_info;        // pass in handle we don't own it
};

struct GetShouldDownloadArbitraryHeadersProxyEvent : 
    public nsImapMiscellaneousSinkProxyEvent
{
    GetShouldDownloadArbitraryHeadersProxyEvent(
        nsImapMiscellaneousSinkProxy* aProxy, GenericInfo* aInfo);
    virtual ~GetShouldDownloadArbitraryHeadersProxyEvent();
    NS_IMETHOD HandleEvent();
    GenericInfo *m_info;        // pass in handle we don't own it
};

struct GetShowAttachmentsInlineProxyEvent : 
    public nsImapMiscellaneousSinkProxyEvent
{
    GetShowAttachmentsInlineProxyEvent(
        nsImapMiscellaneousSinkProxy* aProxy, PRBool* aBool);
    virtual ~GetShowAttachmentsInlineProxyEvent();
    NS_IMETHOD HandleEvent();
    PRBool *m_bool;        // pass in handle we don't own it
};

struct HeaderFetchCompletedProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    HeaderFetchCompletedProxyEvent(nsImapMiscellaneousSinkProxy* aProxy);
    virtual ~HeaderFetchCompletedProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct UpdateSecurityStatusProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    UpdateSecurityStatusProxyEvent(nsImapMiscellaneousSinkProxy* aProxy);
    virtual ~UpdateSecurityStatusProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct FinishImapConnectionProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    FinishImapConnectionProxyEvent(nsImapMiscellaneousSinkProxy* aProxy);
    virtual ~FinishImapConnectionProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct SetImapHostPasswordProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    SetImapHostPasswordProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                                  GenericInfo* aInfo);
    virtual ~SetImapHostPasswordProxyEvent();
    NS_IMETHOD HandleEvent();
    GenericInfo m_info;
};

struct GetPasswordForUserProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    GetPasswordForUserProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                                 const char* userName);
    virtual ~GetPasswordForUserProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_userName;
};

struct SetBiffStateAndUpdateProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    SetBiffStateAndUpdateProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                                    nsMsgBiffState biffState);
    virtual ~SetBiffStateAndUpdateProxyEvent();
    NS_IMETHOD HandleEvent();
    nsMsgBiffState m_biffState;
};

struct GetStoredUIDValidityProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    GetStoredUIDValidityProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                                   uid_validity_info* aInfo);
    virtual ~GetStoredUIDValidityProxyEvent();
    NS_IMETHOD HandleEvent();
    uid_validity_info m_uidValidityInfo;
};

struct LiteSelectUIDValidityProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    LiteSelectUIDValidityProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                                    PRUint32 uidValidity);
    virtual ~LiteSelectUIDValidityProxyEvent();
    NS_IMETHOD HandleEvent();
    PRUint32 m_uidValidity;
};

struct FEAlertProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    FEAlertProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                      const char* aString);
    virtual ~FEAlertProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_alertString;
};

struct FEAlertFromServerProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    FEAlertFromServerProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                                const char* aString);
    virtual ~FEAlertFromServerProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_alertString;
};

struct ProgressStatusProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    ProgressStatusProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                             const char* statusMsg);
    virtual ~ProgressStatusProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_statusMsg;
};

struct PercentProgressProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    PercentProgressProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                              ProgressInfo* aInfo);
    virtual ~PercentProgressProxyEvent();
    NS_IMETHOD HandleEvent();
    ProgressInfo m_progressInfo;
};

struct PastPasswordCheckProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    PastPasswordCheckProxyEvent(nsImapMiscellaneousSinkProxy* aProxy);
    virtual ~PastPasswordCheckProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct CommitNamespacesProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    CommitNamespacesProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                               const char* hostName);
    virtual ~CommitNamespacesProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_hostName;
};

struct CommitCapabilityForHostProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    CommitCapabilityForHostProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                                      const char* hostName);
    virtual ~CommitCapabilityForHostProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_hostName;
};

struct TunnelOutStreamProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    TunnelOutStreamProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                              msg_line_info* aInfo);
    virtual ~TunnelOutStreamProxyEvent();
    NS_IMETHOD HandleEvent();
    msg_line_info m_msgLineInfo;
};

struct ProcessTunnelProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    ProcessTunnelProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                            TunnelInfo *aInfo);
    virtual ~ProcessTunnelProxyEvent();
    NS_IMETHOD HandleEvent();
    TunnelInfo m_tunnelInfo;
};

#endif
