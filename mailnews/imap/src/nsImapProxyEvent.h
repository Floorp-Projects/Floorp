/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsImapProxyEvent_h__
#define nsImapProxyEvent_h__

#include "plevent.h"
#include "prthread.h"
#include "nsISupports.h"
#include "nsIURL.h"
#include "nsIImapMailFolderSink.h"
#include "nsIImapMessageSink.h"
#include "nsIImapExtensionSink.h"
#include "nsIImapMiscellaneousSink.h"
#include "nsIImapIncomingServer.h"
#include "nsImapCore.h"
#include "nsIImapUrl.h"
#include "nsIImapMailFolderSink.h"


#include "nsCOMPtr.h"
class nsImapProxyBase
{
public:
    nsImapProxyBase(nsIImapProtocol* aProtocol,
                    nsIEventQueue* aEventQ,
                    PRThread* aThread);
    virtual ~nsImapProxyBase();

    nsIEventQueue* m_eventQueue;
    PRThread* m_thread;
    nsIImapProtocol* m_protocol;
};

class nsImapExtensionSinkProxy : public nsIImapExtensionSink, 
                             public nsImapProxyBase
{
public:
    nsImapExtensionSinkProxy(nsIImapExtensionSink* aImapExtensionSink,
                         nsIImapProtocol* aProtocol,
                         nsIEventQueue* aEventQ,
                         PRThread* aThread);
    virtual ~nsImapExtensionSinkProxy();

    NS_DECL_ISUPPORTS
  
    NS_IMETHOD ClearFolderRights(nsIImapProtocol* aProtocol,
                                 nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD AddFolderRights(nsIImapProtocol* aProtocol,
                             nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD RefreshFolderRights(nsIImapProtocol* aProtocol,
                                   nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
                                         nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD SetCopyResponseUid(nsIImapProtocol* aProtocol,
                                  nsMsgKeyArray* aKeyArray,
                                  const char* msgIdString,
                                  nsIImapUrl * aUrl);
    NS_IMETHOD SetAppendMsgUid(nsIImapProtocol* aProtocol,
                               nsMsgKey aKey,
                               nsIImapUrl * aUrl);
    NS_IMETHOD GetMessageId(nsIImapProtocol* aProtocol,
                            nsCString* messageId,
                            nsIImapUrl * aUrl);
    
    nsIImapExtensionSink* m_realImapExtensionSink;
};

class nsImapMiscellaneousSinkProxy : public nsIImapMiscellaneousSink, 
                                 public nsImapProxyBase
{
public:
    nsImapMiscellaneousSinkProxy (nsIImapMiscellaneousSink* aImapMiscellaneousSink,
                              nsIImapProtocol* aProtocol,
                              nsIEventQueue* aEventQ,
                              PRThread* aThread);
    ~nsImapMiscellaneousSinkProxy ();

    NS_DECL_ISUPPORTS
	
    NS_IMETHOD GetArbitraryHeaders(nsIImapProtocol* aProtocol,
                                   GenericInfo* aInfo);
    NS_IMETHOD GetShouldDownloadArbitraryHeaders(nsIImapProtocol* aProtocol,
                                                 GenericInfo* aInfo);
    NS_IMETHOD HeaderFetchCompleted(nsIImapProtocol* aProtocol);
    NS_IMETHOD UpdateSecurityStatus(nsIImapProtocol* aProtocol);
    // ****
    NS_IMETHOD SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
                                     nsMsgBiffState biffState);
    NS_IMETHOD GetStoredUIDValidity(nsIImapProtocol* aProtocol,
                                    uid_validity_info* aInfo);
    NS_IMETHOD LiteSelectUIDValidity(nsIImapProtocol* aProtocol,
                                     PRUint32 uidValidity);
	  NS_IMETHOD ProgressStatus(nsIImapProtocol* aProtocol,
                              PRUint32 statusMsgId, const char *extraInfo);
    NS_IMETHOD PercentProgress(nsIImapProtocol* aProtocol,
                               ProgressInfo* aInfo);
    NS_IMETHOD TunnelOutStream(nsIImapProtocol* aProtocol,
														 msg_line_info* aInfo);
    NS_IMETHOD ProcessTunnel(nsIImapProtocol* aProtocol,
                             TunnelInfo *aInfo);
    NS_IMETHOD CopyNextStreamMessage(nsIImapProtocol* aProtocl,
                                     nsIImapUrl * aUrl);

    NS_IMETHOD SetUrlState(nsIImapProtocol* aProtocol,
                           nsIMsgMailNewsUrl* aUrl,
                           PRBool isRunning,
                           nsresult statusCode);

    nsIImapMiscellaneousSink* m_realImapMiscellaneousSink;
};

/* ******* Imap Base Event struct ******** */
struct nsImapEvent : public PLEvent
{
  nsImapEvent();
	virtual ~nsImapEvent();
	virtual void InitEvent();

	NS_IMETHOD HandleEvent() = 0;
	void PostEvent(nsIEventQueue* aEventQ);
  virtual void SetNotifyCompletion(PRBool notifyCompletion);
  
	static void PR_CALLBACK imap_event_handler(PLEvent* aEvent);
	static void PR_CALLBACK imap_event_destructor(PLEvent *aEvent);
  PRBool m_notifyCompletion;
};

struct nsImapExtensionSinkProxyEvent : nsImapEvent
{
    nsImapExtensionSinkProxyEvent(nsImapExtensionSinkProxy* aProxy);
    virtual ~nsImapExtensionSinkProxyEvent();
    nsImapExtensionSinkProxy* m_proxy;
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

struct SetCopyResponseUidProxyEvent : nsImapExtensionSinkProxyEvent
{
  SetCopyResponseUidProxyEvent(nsImapExtensionSinkProxy* aProxy,
                               nsMsgKeyArray* aKeyArray, 
                               const char* msgIdString,
                               nsIImapUrl * aUr);
  virtual ~SetCopyResponseUidProxyEvent();
  NS_IMETHOD HandleEvent();
  nsMsgKeyArray m_copyKeyArray;
  nsCString m_msgIdString;
  nsCOMPtr<nsIImapUrl> m_Url;
};

struct SetAppendMsgUidProxyEvent : nsImapExtensionSinkProxyEvent
{
    SetAppendMsgUidProxyEvent(nsImapExtensionSinkProxy* aProxy,
                              nsMsgKey aKey, nsIImapUrl * aUrl);
    virtual ~SetAppendMsgUidProxyEvent();
    NS_IMETHOD HandleEvent();
    nsMsgKey m_key;
    nsCOMPtr<nsIImapUrl> m_Url;
};

struct GetMessageIdProxyEvent : nsImapExtensionSinkProxyEvent
{
    GetMessageIdProxyEvent(nsImapExtensionSinkProxy* aProxy,
                           nsCString* messageId, nsIImapUrl * aUrl);
    virtual ~GetMessageIdProxyEvent();
    NS_IMETHOD HandleEvent();
    nsCString* m_messageId;
    nsCOMPtr<nsIImapUrl> m_Url;
};

struct nsImapMiscellaneousSinkProxyEvent : public nsImapEvent
{
    nsImapMiscellaneousSinkProxyEvent(nsImapMiscellaneousSinkProxy* aProxy);
    virtual ~nsImapMiscellaneousSinkProxyEvent();
    nsImapMiscellaneousSinkProxy* m_proxy;
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


struct ProgressStatusProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    ProgressStatusProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                            PRUint32 statusMsgId, const char *extraInfo);
    virtual ~ProgressStatusProxyEvent();
    NS_IMETHOD HandleEvent();
    PRUint32 m_statusMsgId;
	char	*m_extraInfo;
};

struct PercentProgressProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    PercentProgressProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                              ProgressInfo* aInfo);
    virtual ~PercentProgressProxyEvent();
    NS_IMETHOD HandleEvent();
    ProgressInfo m_progressInfo;
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

struct CopyNextStreamMessageProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    CopyNextStreamMessageProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                                    nsIImapUrl * aUrl);
    virtual ~CopyNextStreamMessageProxyEvent();
    NS_IMETHOD HandleEvent();
    nsCOMPtr<nsIImapUrl> m_Url;
};

struct SetUrlStateProxyEvent : public nsImapMiscellaneousSinkProxyEvent
{
    SetUrlStateProxyEvent(nsImapMiscellaneousSinkProxy* aProxy,
                          nsIMsgMailNewsUrl* aUrl, PRBool isRunning, 
                          nsresult statusCode);
    virtual ~SetUrlStateProxyEvent();
    NS_IMETHOD HandleEvent();
    nsCOMPtr<nsIMsgMailNewsUrl> m_url;
    PRBool m_isRunning;
    nsresult m_status;
};

#endif
