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
#include "nsIImapMailfolder.h"
#include "nsIImapMessage.h"
#include "nsIImapExtension.h"
#include "nsIImapMiscellaneous.h"

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

class nsImapMailfolderProxy : public nsIImapMailfolder, 
                              public nsImapProxyBase
{
public:
    nsImapMailfolderProxy(nsIImapMailfolder* aImapMailfolder,
                          nsIImapProtocol* aProtocol,
                          PLEventQueue* aEventQ,
                          PRThread* aThread);
    virtual ~nsImapMailfolderProxy();
    
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

    nsIImapMailfolder* m_realImapMailfolder;
};

class nsImapMessageProxy : public nsIImapMessage, 
                           public nsImapProxyBase
{
public:
    nsImapMessageProxy(nsIImapMessage* aImapMessage,
                       nsIImapProtocol* aProtocol,
                       PLEventQueue* aEventQ,
                       PRThread* aThread);
    virtual ~nsImapMessageProxy();

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

    nsIImapMessage* m_realImapMessage;
};

class nsImapExtensionProxy : public nsIImapExtension, 
                             public nsImapProxyBase
{
public:
    nsImapExtensionProxy(nsIImapExtension* aImapExtension,
                         nsIImapProtocol* aProtocol,
                         PLEventQueue* aEventQ,
                         PRThread* aThread);
    virtual ~nsImapExtensionProxy();

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
    
    nsIImapExtension* m_realImapExtension;
};

class nsImapMiscellaneousProxy : public nsIImapMiscellaneous, 
                                 public nsImapProxyBase
{
public:
    nsImapMiscellaneousProxy (nsIImapMiscellaneous* aImapMiscellaneous,
                              nsIImapProtocol* aProtocol,
                              PLEventQueue* aEventQ,
                              PRThread* aThread);
    ~nsImapMiscellaneousProxy ();

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

    nsIImapMiscellaneous* m_realImapMiscellaneous;
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

struct nsImapMailfolderProxyEvent : public nsImapEvent
{
    nsImapMailfolderProxyEvent(nsImapMailfolderProxy* aProxy);
    virtual ~nsImapMailfolderProxyEvent();
    nsImapMailfolderProxy* m_proxy;
};

struct PossibleImapMailboxProxyEvent : public nsImapMailfolderProxyEvent
{
    PossibleImapMailboxProxyEvent(nsImapMailfolderProxy* aProxy,
                                  mailbox_spec* aSpec);
    virtual ~PossibleImapMailboxProxyEvent();
    NS_IMETHOD HandleEvent();

    mailbox_spec m_mailboxSpec;
};

struct MailboxDiscoveryDoneProxyEvent : public nsImapMailfolderProxyEvent
{
    MailboxDiscoveryDoneProxyEvent(nsImapMailfolderProxy* aProxy);
    virtual ~MailboxDiscoveryDoneProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct UpdateImapMailboxInfoProxyEvent : public nsImapMailfolderProxyEvent
{
    UpdateImapMailboxInfoProxyEvent(nsImapMailfolderProxy* aProxy,
                                    mailbox_spec* aSpec);
    virtual ~UpdateImapMailboxInfoProxyEvent();
    NS_IMETHOD HandleEvent();
    mailbox_spec m_mailboxSpec;
};

struct UpdateImapMailboxStatusProxyEvent : public nsImapMailfolderProxyEvent
{
    UpdateImapMailboxStatusProxyEvent(nsImapMailfolderProxy* aProxy,
                                      mailbox_spec* aSpec);
    virtual ~UpdateImapMailboxStatusProxyEvent();
    NS_IMETHOD HandleEvent();
    mailbox_spec m_mailboxSpec;
};

struct ChildDiscoverySucceededProxyEvent : public nsImapMailfolderProxyEvent
{
    ChildDiscoverySucceededProxyEvent(nsImapMailfolderProxy* aProxy);
    virtual ~ChildDiscoverySucceededProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct OnlineFolderDeleteProxyEvent : public nsImapMailfolderProxyEvent
{
    OnlineFolderDeleteProxyEvent(nsImapMailfolderProxy* aProxy,
                                 const char* folderName);
    virtual ~OnlineFolderDeleteProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_folderName;
};

struct OnlineFolderCreateFailedProxyEvent : public nsImapMailfolderProxyEvent
{
    OnlineFolderCreateFailedProxyEvent(nsImapMailfolderProxy* aProxy,
                                       const char* folderName);
    virtual ~OnlineFolderCreateFailedProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_folderName;
};

struct OnlineFolderRenameProxyEvent : public nsImapMailfolderProxyEvent
{
    OnlineFolderRenameProxyEvent(nsImapMailfolderProxy* aProxy,
                                 folder_rename_struct* aStruct);
    virtual ~OnlineFolderRenameProxyEvent();
    NS_IMETHOD HandleEvent();
    folder_rename_struct m_folderRenameStruct;
};

struct SubscribeUpgradeFinishedProxyEvent : public nsImapMailfolderProxyEvent
{
    SubscribeUpgradeFinishedProxyEvent(nsImapMailfolderProxy* aProxy,
                                       EIMAPSubscriptionUpgradeState* aState);
    virtual ~SubscribeUpgradeFinishedProxyEvent();
    NS_IMETHOD HandleEvent();
    EIMAPSubscriptionUpgradeState m_state;
};

struct PromptUserForSubscribeUpdatePathProxyEvent : 
    public nsImapMailfolderProxyEvent 
{
    PromptUserForSubscribeUpdatePathProxyEvent(nsImapMailfolderProxy* aProxy,
                                               PRBool* aBool);
    virtual ~PromptUserForSubscribeUpdatePathProxyEvent();
    NS_IMETHOD HandleEvent();
    PRBool m_bool;
};

struct FolderIsNoSelectProxyEvent : public nsImapMailfolderProxyEvent
{
    FolderIsNoSelectProxyEvent(nsImapMailfolderProxy* aProxy,
                               FolderQueryInfo* aInfo);
    virtual ~FolderIsNoSelectProxyEvent();
    NS_IMETHOD HandleEvent();
    FolderQueryInfo m_folderQueryInfo;
};

struct nsImapMessageProxyEvent : nsImapEvent
{
    nsImapMessageProxyEvent(nsImapMessageProxy* aImapMessageProxy);
    virtual ~nsImapMessageProxyEvent();
    nsImapMessageProxy* m_proxy;
};

struct SetupMsgWriteStreamProxyEvent : nsImapMessageProxyEvent
{
    SetupMsgWriteStreamProxyEvent(nsImapMessageProxy* aImapMessageProxy,
                                  StreamInfo* aStreamInfo);
    virtual ~SetupMsgWriteStreamProxyEvent();
    NS_IMETHOD HandleEvent();
    StreamInfo m_streamInfo;
};

struct ParseAdoptedMsgLineProxyEvent : nsImapMessageProxyEvent
{
    ParseAdoptedMsgLineProxyEvent(nsImapMessageProxy* aImapMessageProxy,
                                  msg_line_info* aMsgLineInfo);
    virtual ~ParseAdoptedMsgLineProxyEvent();
    NS_IMETHOD HandleEvent();
    msg_line_info m_msgLineInfo;
};

struct NormalEndMsgWriteStreamProxyEvent : nsImapMessageProxyEvent
{
    NormalEndMsgWriteStreamProxyEvent(nsImapMessageProxy* aImapMessageProxy);
    virtual ~NormalEndMsgWriteStreamProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct AbortMsgWriteStreamProxyEvent : nsImapMessageProxyEvent
{
    AbortMsgWriteStreamProxyEvent(nsImapMessageProxy* aImapMessageProxy);
    virtual ~AbortMsgWriteStreamProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct OnlineCopyReportProxyEvent : nsImapMessageProxyEvent
{
    OnlineCopyReportProxyEvent(nsImapMessageProxy* aImapMessageProxy,
                               ImapOnlineCopyState* aCopyState);
    virtual ~OnlineCopyReportProxyEvent();
    NS_IMETHOD HandleEvent();
    ImapOnlineCopyState m_copyState;
};

struct BeginMessageUploadProxyEvent : nsImapMessageProxyEvent
{
    BeginMessageUploadProxyEvent(nsImapMessageProxy* aImapMessageProxy);
    virtual ~BeginMessageUploadProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct UploadMessageFileProxyEvent : nsImapMessageProxyEvent
{
    UploadMessageFileProxyEvent(nsImapMessageProxy* aImapMessageProxy,
                                UploadMessageInfo* aMsgInfo);
    virtual ~UploadMessageFileProxyEvent();
    NS_IMETHOD HandleEvent();
    UploadMessageInfo m_msgInfo;
};

struct NotifyMessageFlagsProxyEvent : nsImapMessageProxyEvent
{
    NotifyMessageFlagsProxyEvent(nsImapMessageProxy* aImapMessageProxy,
                                 FlagsKeyStruct* aKeyStruct);
    virtual ~NotifyMessageFlagsProxyEvent();
    NS_IMETHOD HandleEvent();
    FlagsKeyStruct m_keyStruct;
};

struct NotifyMessageDeletedProxyEvent : nsImapMessageProxyEvent
{
    NotifyMessageDeletedProxyEvent(nsImapMessageProxy* aImapMessageProxy,
                                   delete_message_struct* aStruct);
    virtual ~NotifyMessageDeletedProxyEvent();
    NS_IMETHOD HandleEvent();
    delete_message_struct m_deleteMessageStruct;
};

struct GetMessageSizeFromDBProxyEvent : nsImapMessageProxyEvent
{
    GetMessageSizeFromDBProxyEvent(nsImapMessageProxy* aImapMessageProxy,
                                   MessageSizeInfo* sizeInfo);
    virtual ~GetMessageSizeFromDBProxyEvent();
    NS_IMETHOD HandleEvent();
    MessageSizeInfo *m_sizeInfo; // pass in handle we don't own it
};

struct nsImapExtensionProxyEvent : nsImapEvent
{
    nsImapExtensionProxyEvent(nsImapExtensionProxy* aProxy);
    virtual ~nsImapExtensionProxyEvent();
    nsImapExtensionProxy* m_proxy;
};

struct SetUserAuthenticatedProxyEvent : nsImapExtensionProxyEvent
{
    SetUserAuthenticatedProxyEvent(nsImapExtensionProxy* aProxy,
                                   PRBool aBool);
    virtual ~SetUserAuthenticatedProxyEvent();
    NS_IMETHOD HandleEvent();
    PRBool m_bool;
};

struct SetMailServerUrlsProxyEvent : nsImapExtensionProxyEvent
{
    SetMailServerUrlsProxyEvent(nsImapExtensionProxy* aProxy,
                                const char* hostName);
    virtual ~SetMailServerUrlsProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_hostName;
};

struct SetMailAccountUrlProxyEvent : nsImapExtensionProxyEvent
{
    SetMailAccountUrlProxyEvent(nsImapExtensionProxy* aProxy,
                                const char* hostName);
    virtual ~SetMailAccountUrlProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_hostName;
};

struct ClearFolderRightsProxyEvent : nsImapExtensionProxyEvent
{
    ClearFolderRightsProxyEvent(nsImapExtensionProxy* aProxy,
                                nsIMAPACLRightsInfo* aclRights);
    virtual ~ClearFolderRightsProxyEvent();
    NS_IMETHOD HandleEvent();
    nsIMAPACLRightsInfo m_aclRightsInfo;
};

struct AddFolderRightsProxyEvent : nsImapExtensionProxyEvent
{
    AddFolderRightsProxyEvent(nsImapExtensionProxy* aProxy,
                                nsIMAPACLRightsInfo* aclRights);
    virtual ~AddFolderRightsProxyEvent();
    NS_IMETHOD HandleEvent();
    nsIMAPACLRightsInfo m_aclRightsInfo;
};

struct RefreshFolderRightsProxyEvent : nsImapExtensionProxyEvent
{
    RefreshFolderRightsProxyEvent(nsImapExtensionProxy* aProxy,
                                nsIMAPACLRightsInfo* aclRights);
    virtual ~RefreshFolderRightsProxyEvent();
    NS_IMETHOD HandleEvent();
    nsIMAPACLRightsInfo m_aclRightsInfo;
};

struct FolderNeedsACLInitializedProxyEvent : nsImapExtensionProxyEvent
{
    FolderNeedsACLInitializedProxyEvent(nsImapExtensionProxy* aProxy,
                                nsIMAPACLRightsInfo* aclRights);
    virtual ~FolderNeedsACLInitializedProxyEvent();
    NS_IMETHOD HandleEvent();
    nsIMAPACLRightsInfo m_aclRightsInfo;
};

struct SetFolderAdminURLProxyEvent : nsImapExtensionProxyEvent
{
    SetFolderAdminURLProxyEvent(nsImapExtensionProxy* aProxy,
                                FolderQueryInfo* aInfo);
    virtual ~SetFolderAdminURLProxyEvent();
    NS_IMETHOD HandleEvent();
    FolderQueryInfo m_folderQueryInfo;
};

struct nsImapMiscellaneousProxyEvent : public nsImapEvent
{
    nsImapMiscellaneousProxyEvent(nsImapMiscellaneousProxy* aProxy);
    virtual ~nsImapMiscellaneousProxyEvent();
    nsImapMiscellaneousProxy* m_proxy;
};

struct AddSearchResultProxyEvent : public nsImapMiscellaneousProxyEvent
{
    AddSearchResultProxyEvent(nsImapMiscellaneousProxy* aProxy, 
                              const char* searchHitLine);
    virtual ~AddSearchResultProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_searchHitLine;
};

struct GetArbitraryHeadersProxyEvent : public nsImapMiscellaneousProxyEvent
{
    GetArbitraryHeadersProxyEvent(nsImapMiscellaneousProxy* aProxy,
                                  GenericInfo* aInfo);
    virtual ~GetArbitraryHeadersProxyEvent();
    NS_IMETHOD HandleEvent();
    GenericInfo *m_info;        // pass in handle we don't own it
};

struct GetShouldDownloadArbitraryHeadersProxyEvent : 
    public nsImapMiscellaneousProxyEvent
{
    GetShouldDownloadArbitraryHeadersProxyEvent(
        nsImapMiscellaneousProxy* aProxy, GenericInfo* aInfo);
    virtual ~GetShouldDownloadArbitraryHeadersProxyEvent();
    NS_IMETHOD HandleEvent();
    GenericInfo *m_info;        // pass in handle we don't own it
};

struct GetShowAttachmentsInlineProxyEvent : 
    public nsImapMiscellaneousProxyEvent
{
    GetShowAttachmentsInlineProxyEvent(
        nsImapMiscellaneousProxy* aProxy, PRBool* aBool);
    virtual ~GetShowAttachmentsInlineProxyEvent();
    NS_IMETHOD HandleEvent();
    PRBool *m_bool;        // pass in handle we don't own it
};

struct HeaderFetchCompletedProxyEvent : public nsImapMiscellaneousProxyEvent
{
    HeaderFetchCompletedProxyEvent(nsImapMiscellaneousProxy* aProxy);
    virtual ~HeaderFetchCompletedProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct UpdateSecurityStatusProxyEvent : public nsImapMiscellaneousProxyEvent
{
    UpdateSecurityStatusProxyEvent(nsImapMiscellaneousProxy* aProxy);
    virtual ~UpdateSecurityStatusProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct FinishImapConnectionProxyEvent : public nsImapMiscellaneousProxyEvent
{
    FinishImapConnectionProxyEvent(nsImapMiscellaneousProxy* aProxy);
    virtual ~FinishImapConnectionProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct SetImapHostPasswordProxyEvent : public nsImapMiscellaneousProxyEvent
{
    SetImapHostPasswordProxyEvent(nsImapMiscellaneousProxy* aProxy,
                                  GenericInfo* aInfo);
    virtual ~SetImapHostPasswordProxyEvent();
    NS_IMETHOD HandleEvent();
    GenericInfo m_info;
};

struct GetPasswordForUserProxyEvent : public nsImapMiscellaneousProxyEvent
{
    GetPasswordForUserProxyEvent(nsImapMiscellaneousProxy* aProxy,
                                 const char* userName);
    virtual ~GetPasswordForUserProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_userName;
};

struct SetBiffStateAndUpdateProxyEvent : public nsImapMiscellaneousProxyEvent
{
    SetBiffStateAndUpdateProxyEvent(nsImapMiscellaneousProxy* aProxy,
                                    nsMsgBiffState biffState);
    virtual ~SetBiffStateAndUpdateProxyEvent();
    NS_IMETHOD HandleEvent();
    nsMsgBiffState m_biffState;
};

struct GetStoredUIDValidityProxyEvent : public nsImapMiscellaneousProxyEvent
{
    GetStoredUIDValidityProxyEvent(nsImapMiscellaneousProxy* aProxy,
                                   uid_validity_info* aInfo);
    virtual ~GetStoredUIDValidityProxyEvent();
    NS_IMETHOD HandleEvent();
    uid_validity_info m_uidValidityInfo;
};

struct LiteSelectUIDValidityProxyEvent : public nsImapMiscellaneousProxyEvent
{
    LiteSelectUIDValidityProxyEvent(nsImapMiscellaneousProxy* aProxy,
                                    PRUint32 uidValidity);
    virtual ~LiteSelectUIDValidityProxyEvent();
    NS_IMETHOD HandleEvent();
    PRUint32 m_uidValidity;
};

struct FEAlertProxyEvent : public nsImapMiscellaneousProxyEvent
{
    FEAlertProxyEvent(nsImapMiscellaneousProxy* aProxy,
                      const char* aString);
    virtual ~FEAlertProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_alertString;
};

struct FEAlertFromServerProxyEvent : public nsImapMiscellaneousProxyEvent
{
    FEAlertFromServerProxyEvent(nsImapMiscellaneousProxy* aProxy,
                                const char* aString);
    virtual ~FEAlertFromServerProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_alertString;
};

struct ProgressStatusProxyEvent : public nsImapMiscellaneousProxyEvent
{
    ProgressStatusProxyEvent(nsImapMiscellaneousProxy* aProxy,
                             const char* statusMsg);
    virtual ~ProgressStatusProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_statusMsg;
};

struct PercentProgressProxyEvent : public nsImapMiscellaneousProxyEvent
{
    PercentProgressProxyEvent(nsImapMiscellaneousProxy* aProxy,
                              ProgressInfo* aInfo);
    virtual ~PercentProgressProxyEvent();
    NS_IMETHOD HandleEvent();
    ProgressInfo m_progressInfo;
};

struct PastPasswordCheckProxyEvent : public nsImapMiscellaneousProxyEvent
{
    PastPasswordCheckProxyEvent(nsImapMiscellaneousProxy* aProxy);
    virtual ~PastPasswordCheckProxyEvent();
    NS_IMETHOD HandleEvent();
};

struct CommitNamespacesProxyEvent : public nsImapMiscellaneousProxyEvent
{
    CommitNamespacesProxyEvent(nsImapMiscellaneousProxy* aProxy,
                               const char* hostName);
    virtual ~CommitNamespacesProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_hostName;
};

struct CommitCapabilityForHostProxyEvent : public nsImapMiscellaneousProxyEvent
{
    CommitCapabilityForHostProxyEvent(nsImapMiscellaneousProxy* aProxy,
                                      const char* hostName);
    virtual ~CommitCapabilityForHostProxyEvent();
    NS_IMETHOD HandleEvent();
    char* m_hostName;
};

struct TunnelOutStreamProxyEvent : public nsImapMiscellaneousProxyEvent
{
    TunnelOutStreamProxyEvent(nsImapMiscellaneousProxy* aProxy,
                              msg_line_info* aInfo);
    virtual ~TunnelOutStreamProxyEvent();
    NS_IMETHOD HandleEvent();
    msg_line_info m_msgLineInfo;
};

struct ProcessTunnelProxyEvent : public nsImapMiscellaneousProxyEvent
{
    ProcessTunnelProxyEvent(nsImapMiscellaneousProxy* aProxy,
                            TunnelInfo *aInfo);
    virtual ~ProcessTunnelProxyEvent();
    NS_IMETHOD HandleEvent();
    TunnelInfo m_tunnelInfo;
};

#endif
