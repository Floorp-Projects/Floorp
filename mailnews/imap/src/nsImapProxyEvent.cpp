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

#include "nscore.h"
#include "msgCore.h"		// precompiled header
#include "nspr.h"
#include "nsIImapMailfolder.h"
#include "nsIImapMessage.h"
#include "nsIImapExtension.h"
#include "nsIImapMiscellaneous.h"
#include "nsImapProxyEvent.h"
#include "nsIMAPNamespace.h"
#include "nsCOMPtr.h"

#include <windows.h>						// for InterlockedIncrement

nsImapEvent::~nsImapEvent()
{
}

void
nsImapEvent::InitEvent()
{
		PL_InitEvent(this, nsnull,
								 (PLHandleEventProc) imap_event_handler,
								 (PLDestroyEventProc) imap_event_destructor);
}

void
nsImapEvent::PostEvent(PLEventQueue* aEventQ)
{
		NS_PRECONDITION(nsnull != aEventQ, "PostEvent: aEventQ is null");

		InitEvent();
		PL_PostEvent(aEventQ, this);
}

void PR_CALLBACK
nsImapEvent::imap_event_handler(PLEvent *aEvent)
{
		nsImapEvent* ev = (nsImapEvent*) aEvent;
		ev->HandleEvent();
}

void PR_CALLBACK
nsImapEvent::imap_event_destructor(PLEvent *aEvent)
{
		nsImapEvent* ev = (nsImapEvent*) aEvent;
		delete ev;
}

nsImapProxyBase::nsImapProxyBase(nsIImapProtocol* aProtocol,
                                 PLEventQueue* aEventQ,
                                 PRThread* aThread)
{
    NS_ASSERTION (aProtocol && aEventQ && aThread,
                  "nsImapProxy: invalid aProtocol, aEventQ, or aThread");

    m_protocol = aProtocol;
    NS_IF_ADDREF(m_protocol);
		
		m_eventQueue = aEventQ;
		m_thread = aThread;
}

nsImapProxyBase::~nsImapProxyBase()
{
    NS_IF_RELEASE (m_protocol);
}

nsImapLogProxy::nsImapLogProxy(nsIImapLog* aImapLog, 
                               nsIImapProtocol* aProtocol,
															 PLEventQueue* aEventQ,
															 PRThread* aThread) :
    nsImapProxyBase(aProtocol, aEventQ, aThread)
{
		NS_ASSERTION(aImapLog, "nsImapLogProxy: invalid aImapLog");
		NS_INIT_REFCNT();

		m_realImapLog = aImapLog;
		NS_ADDREF(m_realImapLog);
}

nsImapLogProxy::~nsImapLogProxy()
{
		NS_IF_RELEASE(m_realImapLog);
}

/*
 * Implementation of thread save nsISupports methods ....
 */
static NS_DEFINE_IID(kIImapLogIID, NS_IIMAPLOG_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsImapLogProxy, kIImapLogIID);

NS_IMETHODIMP nsImapLogProxy::HandleImapLogData(const char *aLogData)
{
		NS_PRECONDITION(aLogData, "HandleImapLogData: invalid log data");
		nsresult res = NS_OK;
		
		if(PR_GetCurrentThread() == m_thread)
		{
				nsImapLogProxyEvent *ev = 
            new nsImapLogProxyEvent(this, aLogData);
				if (nsnull == ev)
				{
						res = NS_ERROR_OUT_OF_MEMORY;
				}
				else
				{
						ev->PostEvent(m_eventQueue);
				}
		}
		else
		{
				res = m_realImapLog->HandleImapLogData(aLogData);
        // notify the protocol instance that the fe event has been completed.
        // *** important ***
        m_protocol->NotifyFEEventCompletion();
		}
		return res;
}

nsImapLogProxyEvent::nsImapLogProxyEvent(nsImapLogProxy* aProxy,
																				 const char* aLogData)
{
		NS_ASSERTION (aProxy && aLogData, 
										"nsImapLogProxyEvent: invalid aProxy or aLogData");
		m_logData = PL_strdup(aLogData);
		m_proxy = aProxy;
		NS_ADDREF(m_proxy);
}

nsImapLogProxyEvent::~nsImapLogProxyEvent()
{
		PR_Free(m_logData);
		NS_IF_RELEASE(m_proxy);
}

NS_IMETHODIMP
nsImapLogProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapLog->HandleImapLogData(m_logData);
    m_proxy->m_protocol->NotifyFEEventCompletion();
		return res;
}

// ***** class implementation of nsImapMailfolderProxy *****

nsImapMailfolderProxy::nsImapMailfolderProxy(
    nsIImapMailfolder* aImapMailfolder,
    nsIImapProtocol* aProtocol,
    PLEventQueue* aEventQ,
    PRThread* aThread) : nsImapProxyBase(aProtocol, aEventQ, aThread)
{
    NS_ASSERTION (aImapMailfolder, 
                  "nsImapMailfolderProxy: invalid aImapMailfolder");
    NS_INIT_REFCNT ();
    m_realImapMailfolder = aImapMailfolder;
    NS_IF_ADDREF (m_realImapMailfolder);
}

nsImapMailfolderProxy::~nsImapMailfolderProxy()
{
    NS_IF_RELEASE (m_realImapMailfolder);
}

static NS_DEFINE_IID(kIImapMailfolderIID, NS_IIMAPMAILFOLDER_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsImapMailfolderProxy, kIImapMailfolderIID);

NS_IMETHODIMP
nsImapMailfolderProxy::PossibleImapMailbox(nsIImapProtocol* aProtocol,
                                           mailbox_spec* aSpec)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aSpec, "Oops... null mailbox_spec");
    if(!aSpec)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        PossibleImapMailboxProxyEvent *ev =
            new PossibleImapMailboxProxyEvent(this, aSpec);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->PossibleImapMailbox(aProtocol,
                                                        aSpec);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailfolderProxy::MailboxDiscoveryDone(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        MailboxDiscoveryDoneProxyEvent *ev =
            new MailboxDiscoveryDoneProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->MailboxDiscoveryDone(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailfolderProxy::UpdateImapMailboxInfo(nsIImapProtocol* aProtocol,
                                             mailbox_spec* aSpec)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aSpec, "Oops... null mailbox_spec");
    if(!aSpec)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        UpdateImapMailboxInfoProxyEvent *ev =
            new UpdateImapMailboxInfoProxyEvent(this, aSpec);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->UpdateImapMailboxInfo(aProtocol,
                                                        aSpec);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailfolderProxy::UpdateImapMailboxStatus(nsIImapProtocol* aProtocol,
                                               mailbox_spec* aSpec)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aSpec, "Oops... null mailbox_spec");
    if(!aSpec)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        UpdateImapMailboxStatusProxyEvent *ev =
            new UpdateImapMailboxStatusProxyEvent(this, aSpec);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->UpdateImapMailboxStatus(aProtocol,
                                                        aSpec);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailfolderProxy::ChildDiscoverySucceeded(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        ChildDiscoverySucceededProxyEvent *ev =
            new ChildDiscoverySucceededProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->ChildDiscoverySucceeded(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailfolderProxy::OnlineFolderDelete(nsIImapProtocol* aProtocol,
                                          const char* folderName)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (folderName, "Oops... null folderName");
    if(!folderName)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        OnlineFolderDeleteProxyEvent *ev =
            new OnlineFolderDeleteProxyEvent(this, folderName);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->OnlineFolderDelete(aProtocol,
                                                       folderName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailfolderProxy::OnlineFolderCreateFailed(nsIImapProtocol* aProtocol,
                                                const char* folderName)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (folderName, "Oops... null folderName");
    if(!folderName)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        OnlineFolderCreateFailedProxyEvent *ev =
            new OnlineFolderCreateFailedProxyEvent(this, folderName);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->OnlineFolderCreateFailed(aProtocol,
                                                             folderName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailfolderProxy::OnlineFolderRename(nsIImapProtocol* aProtocol,
                                          folder_rename_struct* aStruct)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aStruct, "Oops... null aStruct");
    if(!aStruct)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        OnlineFolderRenameProxyEvent *ev =
            new OnlineFolderRenameProxyEvent(this, aStruct);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->OnlineFolderRename(aProtocol,
                                                       aStruct);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailfolderProxy::SubscribeUpgradeFinished(nsIImapProtocol* aProtocol,
                                     EIMAPSubscriptionUpgradeState* aState)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aState, "Oops... null aState");
    if(!aState)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        SubscribeUpgradeFinishedProxyEvent *ev =
            new SubscribeUpgradeFinishedProxyEvent(this, aState);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->SubscribeUpgradeFinished(aProtocol,
                                                             aState);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailfolderProxy::PromptUserForSubscribeUpdatePath(
    nsIImapProtocol* aProtocol, PRBool* aBool)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aBool, "Oops... null aBool");
    if(!aBool)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        PromptUserForSubscribeUpdatePathProxyEvent *ev =
            new PromptUserForSubscribeUpdatePathProxyEvent(this, aBool);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->PromptUserForSubscribeUpdatePath
                                                         (aProtocol, aBool);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailfolderProxy::FolderIsNoSelect(nsIImapProtocol* aProtocol,
                                        FolderQueryInfo* aInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aInfo, "Oops... null aInfo");
    if(!aInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        FolderIsNoSelectProxyEvent *ev =
            new FolderIsNoSelectProxyEvent(this, aInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailfolder->FolderIsNoSelect(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

nsImapMessageProxy::nsImapMessageProxy(nsIImapMessage* aImapMessage,
                                       nsIImapProtocol* aProtocol,
                                       PLEventQueue* aEventQ,
                                       PRThread* aThread) :
    nsImapProxyBase(aProtocol, aEventQ, aThread)
{
    NS_ASSERTION (aImapMessage, 
                  "nsImapMessageProxy: invalid aImapMessage");
    NS_INIT_REFCNT ();
    m_realImapMessage = aImapMessage;
    NS_IF_ADDREF (m_realImapMessage);
}

nsImapMessageProxy::~nsImapMessageProxy()
{
    NS_IF_ADDREF (m_realImapMessage);
}

static NS_DEFINE_IID(kIImapMessageIID, NS_IIMAPMESSAGE_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsImapMessageProxy, kIImapMessageIID);

NS_IMETHODIMP
nsImapMessageProxy::SetupMsgWriteStream(nsIImapProtocol* aProtocol,
                                        StreamInfo* aStreamInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aStreamInfo, "Oops... null aStreamInfo");
    if(!aStreamInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        SetupMsgWriteStreamProxyEvent *ev =
            new SetupMsgWriteStreamProxyEvent(this, aStreamInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMessage->SetupMsgWriteStream(aProtocol, aStreamInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageProxy::ParseAdoptedMsgLine(nsIImapProtocol* aProtocol,
                                        msg_line_info* aMsgLineInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aMsgLineInfo, "Oops... null aMsgLineInfo");
    if(!aMsgLineInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        ParseAdoptedMsgLineProxyEvent *ev =
            new ParseAdoptedMsgLineProxyEvent(this, aMsgLineInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMessage->ParseAdoptedMsgLine(aProtocol, aMsgLineInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageProxy::NormalEndMsgWriteStream(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        NormalEndMsgWriteStreamProxyEvent *ev =
            new NormalEndMsgWriteStreamProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMessage->NormalEndMsgWriteStream(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageProxy::AbortMsgWriteStream(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        AbortMsgWriteStreamProxyEvent *ev =
            new AbortMsgWriteStreamProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMessage->AbortMsgWriteStream(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageProxy::OnlineCopyReport(nsIImapProtocol* aProtocol,
                                     ImapOnlineCopyState* aCopyState)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aCopyState, "Oops... null aCopyState");
    if(!aCopyState)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        OnlineCopyReportProxyEvent *ev =
            new OnlineCopyReportProxyEvent(this, aCopyState);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMessage->OnlineCopyReport(aProtocol, aCopyState);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageProxy::BeginMessageUpload(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        BeginMessageUploadProxyEvent *ev =
            new BeginMessageUploadProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMessage->BeginMessageUpload(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageProxy::UploadMessageFile(nsIImapProtocol* aProtocol,
                                      UploadMessageInfo* aMsgInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aMsgInfo, "Oops... null aMsgInfo");
    if(!aMsgInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        UploadMessageFileProxyEvent *ev =
            new UploadMessageFileProxyEvent(this, aMsgInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMessage->UploadMessageFile(aProtocol, aMsgInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageProxy::NotifyMessageFlags(nsIImapProtocol* aProtocol,
                                       FlagsKeyStruct* aKeyStruct)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aKeyStruct, "Oops... null aKeyStruct");
    if(!aKeyStruct)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        NotifyMessageFlagsProxyEvent *ev =
            new NotifyMessageFlagsProxyEvent(this, aKeyStruct);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMessage->NotifyMessageFlags(aProtocol, aKeyStruct);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageProxy::NotifyMessageDeleted(nsIImapProtocol* aProtocol,
                                         delete_message_struct* aStruct)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aStruct, "Oops... null aStruct");
    if(!aStruct)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        NotifyMessageDeletedProxyEvent *ev =
            new NotifyMessageDeletedProxyEvent(this, aStruct);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMessage->NotifyMessageDeleted(aProtocol, aStruct);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageProxy::GetMessageSizeFromDB(nsIImapProtocol* aProtocol,
                                         MessageSizeInfo* sizeInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (sizeInfo, "Oops... null sizeInfo");
    if(!sizeInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        GetMessageSizeFromDBProxyEvent *ev =
            new GetMessageSizeFromDBProxyEvent(this, sizeInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMessage->GetMessageSizeFromDB(aProtocol, sizeInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

nsImapExtensionProxy::nsImapExtensionProxy(nsIImapExtension* aImapExtension,
                                           nsIImapProtocol* aProtocol,
                                           PLEventQueue* aEventQ,
                                           PRThread* aThread) :
    nsImapProxyBase(aProtocol, aEventQ, aThread)
{
    NS_ASSERTION (aImapExtension, 
                  "nsImapExtensionProxy: invalid aImapExtension");
    NS_INIT_REFCNT ();
    m_realImapExtension = aImapExtension;
    NS_IF_ADDREF (m_realImapExtension);
}

nsImapExtensionProxy::~nsImapExtensionProxy()
{
    NS_IF_ADDREF (m_realImapExtension);
}

static NS_DEFINE_IID(kIImapExtensionIID, NS_IIMAPEXTENSION_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsImapExtensionProxy, kIImapExtensionIID);

NS_IMETHODIMP
nsImapExtensionProxy::SetUserAuthenticated(nsIImapProtocol* aProtocol,
                                           PRBool aBool)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        SetUserAuthenticatedProxyEvent *ev =
            new SetUserAuthenticatedProxyEvent(this, aBool);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapExtension->SetUserAuthenticated(aProtocol, aBool);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionProxy::SetMailServerUrls(nsIImapProtocol* aProtocol,
                                        const char* hostName)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (hostName, "Oops... null hostName");
    if(!hostName)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        SetMailServerUrlsProxyEvent *ev =
            new SetMailServerUrlsProxyEvent(this, hostName);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapExtension->SetMailServerUrls(aProtocol, hostName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionProxy::SetMailAccountUrl(nsIImapProtocol* aProtocol,
                                        const char* hostName)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (hostName, "Oops... null hostName");
    if(!hostName)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        SetMailAccountUrlProxyEvent *ev =
            new SetMailAccountUrlProxyEvent(this, hostName);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapExtension->SetMailAccountUrl(aProtocol, hostName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionProxy::ClearFolderRights(nsIImapProtocol* aProtocol,
                                        nsIMAPACLRightsInfo* aclRights)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aclRights, "Oops... null aclRights");
    if(!aclRights)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        ClearFolderRightsProxyEvent *ev =
            new ClearFolderRightsProxyEvent(this, aclRights);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapExtension->ClearFolderRights(aProtocol, aclRights);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionProxy::AddFolderRights(nsIImapProtocol* aProtocol,
                                      nsIMAPACLRightsInfo* aclRights)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aclRights, "Oops... null aclRights");
    if(!aclRights)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        AddFolderRightsProxyEvent *ev =
            new AddFolderRightsProxyEvent(this, aclRights);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapExtension->AddFolderRights(aProtocol, aclRights);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionProxy::RefreshFolderRights(nsIImapProtocol* aProtocol,
                                          nsIMAPACLRightsInfo* aclRights)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aclRights, "Oops... null aclRights");
    if(!aclRights)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        RefreshFolderRightsProxyEvent *ev =
            new RefreshFolderRightsProxyEvent(this, aclRights);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapExtension->RefreshFolderRights(aProtocol, aclRights);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionProxy::FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
                                                nsIMAPACLRightsInfo* aclRights)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aclRights, "Oops... null aclRights");
    if(!aclRights)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        FolderNeedsACLInitializedProxyEvent *ev =
            new FolderNeedsACLInitializedProxyEvent(this, aclRights);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapExtension->FolderNeedsACLInitialized(aProtocol,
                                                             aclRights);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionProxy::SetFolderAdminURL(nsIImapProtocol* aProtocol,
                                        FolderQueryInfo* aInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aInfo, "Oops... null aInfo");
    if(!aInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        SetFolderAdminURLProxyEvent *ev =
            new SetFolderAdminURLProxyEvent(this, aInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapExtension->SetFolderAdminURL(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

nsImapMiscellaneousProxy::nsImapMiscellaneousProxy(
    nsIImapMiscellaneous* aImapMiscellaneous, 
    nsIImapProtocol* aProtocol,
    PLEventQueue* aEventQ,
    PRThread* aThread) : nsImapProxyBase(aProtocol, aEventQ, aThread)
{
    NS_ASSERTION (aImapMiscellaneous, 
                  "nsImapMiscellaneousProxy: invalid aImapMiscellaneous");
    NS_INIT_REFCNT ();
    m_realImapMiscellaneous = aImapMiscellaneous;
    NS_IF_ADDREF (m_realImapMiscellaneous);
}

nsImapMiscellaneousProxy::~nsImapMiscellaneousProxy()
{
    NS_IF_ADDREF (m_realImapMiscellaneous);
}

static NS_DEFINE_IID(kIImapMiscellaneousIID, NS_IIMAPMISCELLANEOUS_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsImapMiscellaneousProxy, kIImapMiscellaneousIID);

NS_IMETHODIMP
nsImapMiscellaneousProxy::AddSearchResult(nsIImapProtocol* aProtocol, 
                                          const char* searchHitLine)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (searchHitLine, "Oops... null searchHitLine");
    if(!searchHitLine)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        AddSearchResultProxyEvent *ev =
            new AddSearchResultProxyEvent(this, searchHitLine);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->AddSearchResult(aProtocol, searchHitLine);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::GetArbitraryHeaders(nsIImapProtocol* aProtocol,
                                              GenericInfo* aInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aInfo, "Oops... null aInfo");
    if(!aInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        GetArbitraryHeadersProxyEvent *ev =
            new GetArbitraryHeadersProxyEvent(this, aInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->GetArbitraryHeaders(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::GetShouldDownloadArbitraryHeaders(
    nsIImapProtocol* aProtocol, GenericInfo* aInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aInfo, "Oops... null aInfo");
    if(!aInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        GetShouldDownloadArbitraryHeadersProxyEvent *ev =
            new GetShouldDownloadArbitraryHeadersProxyEvent(this, aInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res =
            m_realImapMiscellaneous->GetShouldDownloadArbitraryHeaders(aProtocol,
                                                                   aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::GetShowAttachmentsInline(
    nsIImapProtocol* aProtocol, PRBool* aBool)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aBool, "Oops... null aBool");
    if(!aBool)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        GetShowAttachmentsInlineProxyEvent *ev =
            new GetShowAttachmentsInlineProxyEvent(this, aBool);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res =
            m_realImapMiscellaneous->GetShowAttachmentsInline(aProtocol,
                                                              aBool);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::HeaderFetchCompleted(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        HeaderFetchCompletedProxyEvent *ev =
            new HeaderFetchCompletedProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->HeaderFetchCompleted(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::UpdateSecurityStatus(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        UpdateSecurityStatusProxyEvent *ev =
            new UpdateSecurityStatusProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->UpdateSecurityStatus(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::FinishImapConnection(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        FinishImapConnectionProxyEvent *ev =
            new FinishImapConnectionProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->FinishImapConnection(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::SetImapHostPassword(nsIImapProtocol* aProtocol,
                                              GenericInfo* aInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aInfo, "Oops... null aInfo");
    if(!aInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        SetImapHostPasswordProxyEvent *ev =
            new SetImapHostPasswordProxyEvent(this, aInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->SetImapHostPassword(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::GetPasswordForUser(nsIImapProtocol* aProtocol,
                                             const char* userName)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (userName, "Oops... null userName");
    if(!userName)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        GetPasswordForUserProxyEvent *ev =
            new GetPasswordForUserProxyEvent(this, userName);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->GetPasswordForUser(aProtocol, userName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
                                                nsMsgBiffState biffState)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        SetBiffStateAndUpdateProxyEvent *ev =
            new SetBiffStateAndUpdateProxyEvent(this, biffState);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->SetBiffStateAndUpdate(aProtocol, biffState);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::GetStoredUIDValidity(nsIImapProtocol* aProtocol,
                                               uid_validity_info* aInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aInfo, "Oops... null aInfo");
    if(!aInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        GetStoredUIDValidityProxyEvent *ev =
            new GetStoredUIDValidityProxyEvent(this, aInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->GetStoredUIDValidity(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::LiteSelectUIDValidity(nsIImapProtocol* aProtocol,
                                                PRUint32 uidValidity)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        LiteSelectUIDValidityProxyEvent *ev =
            new LiteSelectUIDValidityProxyEvent(this, uidValidity);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->LiteSelectUIDValidity(aProtocol, uidValidity);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::FEAlert(nsIImapProtocol* aProtocol,
                                  const char* aString)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aString, "Oops... null aString");
    if(!aString)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        FEAlertProxyEvent *ev =
            new FEAlertProxyEvent(this, aString);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->FEAlert(aProtocol, aString);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::FEAlertFromServer(nsIImapProtocol* aProtocol,
                                            const char* aString)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aString, "Oops... null aString");
    if(!aString)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        FEAlertFromServerProxyEvent *ev =
            new FEAlertFromServerProxyEvent(this, aString);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->FEAlertFromServer(aProtocol, aString);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::ProgressStatus(nsIImapProtocol* aProtocol,
                                         const char* statusMsg)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (statusMsg, "Oops... null statusMsg");
    if(!statusMsg)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        ProgressStatusProxyEvent *ev =
            new ProgressStatusProxyEvent(this, statusMsg);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->ProgressStatus(aProtocol, statusMsg);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::PercentProgress(nsIImapProtocol* aProtocol,
                                          ProgressInfo* aInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aInfo, "Oops... null aInfo");
    if(!aInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        PercentProgressProxyEvent *ev =
            new PercentProgressProxyEvent(this, aInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->PercentProgress(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::PastPasswordCheck(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        PastPasswordCheckProxyEvent *ev =
            new PastPasswordCheckProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->PastPasswordCheck(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::CommitNamespaces(nsIImapProtocol* aProtocol,
                                           const char* hostName)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (hostName, "Oops... null hostName");
    if(!hostName)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        CommitNamespacesProxyEvent *ev =
            new CommitNamespacesProxyEvent(this, hostName);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->CommitNamespaces(aProtocol, hostName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::CommitCapabilityForHost(nsIImapProtocol* aProtocol,
                                                  const char* hostName)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (hostName, "Oops... null hostName");
    if(!hostName)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        CommitCapabilityForHostProxyEvent *ev =
            new CommitCapabilityForHostProxyEvent(this, hostName);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->CommitCapabilityForHost(aProtocol, hostName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::TunnelOutStream(nsIImapProtocol* aProtocol,
                                          msg_line_info* aInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aInfo, "Oops... null aInfo");
    if(!aInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        TunnelOutStreamProxyEvent *ev =
            new TunnelOutStreamProxyEvent(this, aInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->TunnelOutStream(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousProxy::ProcessTunnel(nsIImapProtocol* aProtocol,
                                        TunnelInfo *aInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aInfo, "Oops... null aInfo");
    if(!aInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        ProcessTunnelProxyEvent *ev =
            new ProcessTunnelProxyEvent(this, aInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMiscellaneous->ProcessTunnel(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

nsImapMailfolderProxyEvent::nsImapMailfolderProxyEvent(nsImapMailfolderProxy*
                                                       aProxy)
{
    NS_ASSERTION (aProxy, "fatal null proxy object");
    m_proxy = aProxy;
    NS_IF_ADDREF (m_proxy);
}

nsImapMailfolderProxyEvent::~nsImapMailfolderProxyEvent()
{
    NS_IF_RELEASE (m_proxy);
}

PossibleImapMailboxProxyEvent::PossibleImapMailboxProxyEvent(
    nsImapMailfolderProxy* aProxy, mailbox_spec* aSpec) :
    nsImapMailfolderProxyEvent(aProxy)
{
    NS_ASSERTION (aSpec, "PossibleImapMailboxProxyEvent: null aSpec");
    if (aSpec)
    {
        m_mailboxSpec = *aSpec;
        if (aSpec->allocatedPathName)
            m_mailboxSpec.allocatedPathName =
                PL_strdup(aSpec->allocatedPathName); 
        if (aSpec->namespaceForFolder)
            m_mailboxSpec.namespaceForFolder = 
                new nsIMAPNamespace(aSpec->namespaceForFolder->GetType(),
                                    aSpec->namespaceForFolder->GetPrefix(),
                                    aSpec->namespaceForFolder->GetDelimiter(),
                                    aSpec->namespaceForFolder->GetIsNamespaceFromPrefs());
    }
    else
    {
        memset(&m_mailboxSpec, 0, sizeof(mailbox_spec));
    }
}

PossibleImapMailboxProxyEvent::~PossibleImapMailboxProxyEvent()
{
    if (m_mailboxSpec.allocatedPathName)
        PL_strfree(m_mailboxSpec.allocatedPathName);
    if (m_mailboxSpec.namespaceForFolder)
        delete m_mailboxSpec.namespaceForFolder;
}

NS_IMETHODIMP
PossibleImapMailboxProxyEvent::HandleEvent()
{
    nsresult res =
        m_proxy->m_realImapMailfolder->PossibleImapMailbox(
            m_proxy->m_protocol, &m_mailboxSpec);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

MailboxDiscoveryDoneProxyEvent::MailboxDiscoveryDoneProxyEvent(
    nsImapMailfolderProxy* aProxy) :
    nsImapMailfolderProxyEvent(aProxy)
{
}

MailboxDiscoveryDoneProxyEvent::~MailboxDiscoveryDoneProxyEvent()
{
}

NS_IMETHODIMP
MailboxDiscoveryDoneProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailfolder->MailboxDiscoveryDone(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

UpdateImapMailboxInfoProxyEvent::UpdateImapMailboxInfoProxyEvent(
    nsImapMailfolderProxy* aProxy, mailbox_spec* aSpec) :
    nsImapMailfolderProxyEvent(aProxy)
{
    NS_ASSERTION (aSpec, "PossibleImapMailboxProxyEvent: null aSpec");
    if (aSpec)
    {
        m_mailboxSpec = *aSpec;
        if (aSpec->allocatedPathName)
            m_mailboxSpec.allocatedPathName =
                PL_strdup(aSpec->allocatedPathName); 
        if (aSpec->namespaceForFolder)
            m_mailboxSpec.namespaceForFolder = 
                new nsIMAPNamespace(aSpec->namespaceForFolder->GetType(),
                                    aSpec->namespaceForFolder->GetPrefix(),
                                    aSpec->namespaceForFolder->GetDelimiter(),
                                    aSpec->namespaceForFolder->GetIsNamespaceFromPrefs());
    }
    else
    {
        memset(&m_mailboxSpec, 0, sizeof(mailbox_spec));
    }
}

UpdateImapMailboxInfoProxyEvent::~UpdateImapMailboxInfoProxyEvent()
{
    if (m_mailboxSpec.allocatedPathName)
        PL_strfree(m_mailboxSpec.allocatedPathName);
    if (m_mailboxSpec.namespaceForFolder)
        delete m_mailboxSpec.namespaceForFolder;
}

NS_IMETHODIMP
UpdateImapMailboxInfoProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailfolder->UpdateImapMailboxInfo(
        m_proxy->m_protocol, &m_mailboxSpec);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

UpdateImapMailboxStatusProxyEvent::UpdateImapMailboxStatusProxyEvent(
    nsImapMailfolderProxy* aProxy, mailbox_spec* aSpec) :
    nsImapMailfolderProxyEvent(aProxy)
{
    NS_ASSERTION (aSpec, "PossibleImapMailboxProxyEvent: null aSpec");
    if (aSpec)
    {
        m_mailboxSpec = *aSpec;
        if (aSpec->allocatedPathName)
            m_mailboxSpec.allocatedPathName =
                PL_strdup(aSpec->allocatedPathName);
        if (aSpec->namespaceForFolder)
            m_mailboxSpec.namespaceForFolder = 
                new nsIMAPNamespace(aSpec->namespaceForFolder->GetType(),
                                    aSpec->namespaceForFolder->GetPrefix(),
                                    aSpec->namespaceForFolder->GetDelimiter(),
                                    aSpec->namespaceForFolder->GetIsNamespaceFromPrefs());
    }
    else
    {
        memset(&m_mailboxSpec, 0, sizeof(mailbox_spec));
    }
}

UpdateImapMailboxStatusProxyEvent::~UpdateImapMailboxStatusProxyEvent()
{
    if (m_mailboxSpec.allocatedPathName)
        PL_strfree(m_mailboxSpec.allocatedPathName);
    if (m_mailboxSpec.namespaceForFolder)
        delete m_mailboxSpec.namespaceForFolder;
}

NS_IMETHODIMP
UpdateImapMailboxStatusProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailfolder->UpdateImapMailboxStatus(
        m_proxy->m_protocol, &m_mailboxSpec);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ChildDiscoverySucceededProxyEvent::ChildDiscoverySucceededProxyEvent(
    nsImapMailfolderProxy* aProxy) :
    nsImapMailfolderProxyEvent(aProxy)
{
}

ChildDiscoverySucceededProxyEvent::~ChildDiscoverySucceededProxyEvent()
{
}

NS_IMETHODIMP
ChildDiscoverySucceededProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailfolder->ChildDiscoverySucceeded(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

OnlineFolderDeleteProxyEvent::OnlineFolderDeleteProxyEvent(
    nsImapMailfolderProxy* aProxy, const char* folderName) :
    nsImapMailfolderProxyEvent(aProxy)
{
    NS_ASSERTION (folderName, "Oops, null folderName");
    if (folderName)
        m_folderName = PL_strdup(folderName);
    else
        m_folderName = nsnull;
}

OnlineFolderDeleteProxyEvent::~OnlineFolderDeleteProxyEvent()
{
    if (m_folderName)
        PL_strfree(m_folderName);
}

NS_IMETHODIMP
OnlineFolderDeleteProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailfolder->OnlineFolderDelete(
        m_proxy->m_protocol, m_folderName); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

OnlineFolderCreateFailedProxyEvent::OnlineFolderCreateFailedProxyEvent(
    nsImapMailfolderProxy* aProxy, const char* folderName) :
    nsImapMailfolderProxyEvent(aProxy)
{
    NS_ASSERTION (folderName, "Oops, null folderName");
    if (folderName)
        m_folderName = PL_strdup(folderName);
    else
        m_folderName = nsnull;
}

OnlineFolderCreateFailedProxyEvent::~OnlineFolderCreateFailedProxyEvent()
{
    if (m_folderName)
        PL_strfree(m_folderName);
}

NS_IMETHODIMP
OnlineFolderCreateFailedProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailfolder->OnlineFolderCreateFailed(
        m_proxy->m_protocol, m_folderName);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

OnlineFolderRenameProxyEvent::OnlineFolderRenameProxyEvent(
    nsImapMailfolderProxy* aProxy, folder_rename_struct* aStruct) :
    nsImapMailfolderProxyEvent(aProxy)
{
    NS_ASSERTION (aStruct, "Oops... null folder_rename_struct");
    if (aStruct)
    {
        m_folderRenameStruct.fHostName = PL_strdup(aStruct->fHostName);
        m_folderRenameStruct.fOldName = PL_strdup(aStruct->fOldName);
        m_folderRenameStruct.fNewName = PL_strdup(aStruct->fNewName);
    }
    else
    {
        m_folderRenameStruct.fHostName = nsnull;
        m_folderRenameStruct.fOldName = nsnull;
        m_folderRenameStruct.fNewName = nsnull;
    }
}

OnlineFolderRenameProxyEvent::~OnlineFolderRenameProxyEvent()
{
    if (m_folderRenameStruct.fHostName)
        PL_strfree(m_folderRenameStruct.fHostName);
    if (m_folderRenameStruct.fOldName)
        PL_strfree( m_folderRenameStruct.fOldName);
    if (m_folderRenameStruct.fNewName)
        PL_strfree(m_folderRenameStruct.fNewName);
}

NS_IMETHODIMP
OnlineFolderRenameProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailfolder->OnlineFolderRename(
        m_proxy->m_protocol, &m_folderRenameStruct);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

SubscribeUpgradeFinishedProxyEvent::SubscribeUpgradeFinishedProxyEvent(
    nsImapMailfolderProxy* aProxy, EIMAPSubscriptionUpgradeState* aState) :
    nsImapMailfolderProxyEvent(aProxy)
{
    NS_ASSERTION (aState, "Oops... null aState");
    if (aState)
        m_state = *aState;
    else
        m_state = kEverythingDone;
}

SubscribeUpgradeFinishedProxyEvent::~SubscribeUpgradeFinishedProxyEvent()
{
}

NS_IMETHODIMP
SubscribeUpgradeFinishedProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailfolder->SubscribeUpgradeFinished(
        m_proxy->m_protocol, &m_state);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

PromptUserForSubscribeUpdatePathProxyEvent::PromptUserForSubscribeUpdatePathProxyEvent(
    nsImapMailfolderProxy* aProxy, PRBool* aBool) :
    nsImapMailfolderProxyEvent(aProxy)
{
    NS_ASSERTION (aBool, "Oops... null aBool");
    if (aBool)
        m_bool = *aBool;
    else 
        m_bool = PR_FALSE;
}

PromptUserForSubscribeUpdatePathProxyEvent::~PromptUserForSubscribeUpdatePathProxyEvent()
{
}

NS_IMETHODIMP
PromptUserForSubscribeUpdatePathProxyEvent::HandleEvent()
{
    nsresult res =
        m_proxy->m_realImapMailfolder->PromptUserForSubscribeUpdatePath(
            m_proxy->m_protocol, &m_bool); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

FolderIsNoSelectProxyEvent::FolderIsNoSelectProxyEvent(
    nsImapMailfolderProxy* aProxy, FolderQueryInfo* aInfo) :
    nsImapMailfolderProxyEvent(aProxy)
{
    NS_ASSERTION (aInfo, "Ooops... null folder query info");
    if (aInfo)
    {
        m_folderQueryInfo.name = PL_strdup(aInfo->name);
        m_folderQueryInfo.hostName = PL_strdup(aInfo->hostName);
        m_folderQueryInfo.rv = aInfo->rv;
    }
    else
    {
        m_folderQueryInfo.name = nsnull;
        m_folderQueryInfo.hostName = nsnull;
        m_folderQueryInfo.rv = PR_FALSE;
    }
}

FolderIsNoSelectProxyEvent::~FolderIsNoSelectProxyEvent()
{
    if (m_folderQueryInfo.name)
        PL_strfree(m_folderQueryInfo.name);
    if (m_folderQueryInfo.hostName)
        PL_strfree(m_folderQueryInfo.hostName);
}

NS_IMETHODIMP
FolderIsNoSelectProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailfolder->FolderIsNoSelect(
        m_proxy->m_protocol, &m_folderQueryInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

nsImapMessageProxyEvent::nsImapMessageProxyEvent(nsImapMessageProxy* aProxy)
{
    NS_ASSERTION (aProxy, "fatal null proxy object");
    m_proxy = aProxy;
    NS_IF_ADDREF (m_proxy);
}

nsImapMessageProxyEvent::~nsImapMessageProxyEvent()
{
    NS_IF_RELEASE (m_proxy);
}

SetupMsgWriteStreamProxyEvent::SetupMsgWriteStreamProxyEvent(
    nsImapMessageProxy* aImapMessageProxy,
    StreamInfo* aStreamInfo) :
    nsImapMessageProxyEvent(aImapMessageProxy)
{
    NS_ASSERTION (aStreamInfo, "Oops... null stream info");
    if (aStreamInfo)
    {
        m_streamInfo.size = aStreamInfo->size;
        m_streamInfo.content_type = PL_strdup(aStreamInfo->content_type);
        if (aStreamInfo->boxSpec)
        {
            m_streamInfo.boxSpec = (mailbox_spec*)
                PR_CALLOC(sizeof(mailbox_spec));
            *m_streamInfo.boxSpec = *aStreamInfo->boxSpec;
            if (aStreamInfo->boxSpec->allocatedPathName)
                m_streamInfo.boxSpec->allocatedPathName =
                    PL_strdup(aStreamInfo->boxSpec->allocatedPathName); 
            if (aStreamInfo->boxSpec->namespaceForFolder)
                m_streamInfo.boxSpec->namespaceForFolder = 
                    new nsIMAPNamespace(
                        aStreamInfo->boxSpec->namespaceForFolder->GetType(),
                        aStreamInfo->boxSpec->namespaceForFolder->GetPrefix(),
                        aStreamInfo->boxSpec->namespaceForFolder->GetDelimiter(),
                        aStreamInfo->boxSpec->namespaceForFolder->GetIsNamespaceFromPrefs());
        }
        else
        {
            m_streamInfo.boxSpec = nsnull;
        }
    }
    else
    {
        m_streamInfo.size = 0;
        m_streamInfo.content_type = nsnull;
        m_streamInfo.boxSpec = nsnull;
    }
}

SetupMsgWriteStreamProxyEvent::~SetupMsgWriteStreamProxyEvent()
{
    if (m_streamInfo.content_type)
        PL_strfree(m_streamInfo.content_type);
    if (m_streamInfo.boxSpec)
    {
        if (m_streamInfo.boxSpec->allocatedPathName)
            PL_strfree(m_streamInfo.boxSpec->allocatedPathName);
        if (m_streamInfo.boxSpec->namespaceForFolder)
            delete m_streamInfo.boxSpec->namespaceForFolder;
        delete m_streamInfo.boxSpec;
    }
}

NS_IMETHODIMP
SetupMsgWriteStreamProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessage->SetupMsgWriteStream(
        m_proxy->m_protocol, &m_streamInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ParseAdoptedMsgLineProxyEvent::ParseAdoptedMsgLineProxyEvent(
    nsImapMessageProxy* aImapMessageProxy,
    msg_line_info* aMsgLineInfo) :
    nsImapMessageProxyEvent(aImapMessageProxy)
{
    NS_ASSERTION (aMsgLineInfo, "Oops... null msg_line_info");
    if (aMsgLineInfo)
    {
        m_msgLineInfo.adoptedMessageLine =
            PL_strdup(aMsgLineInfo->adoptedMessageLine);
        m_msgLineInfo.uidOfMessage = aMsgLineInfo->uidOfMessage;
    }
    else
    {
        m_msgLineInfo.adoptedMessageLine = nsnull;
        m_msgLineInfo.uidOfMessage = 0xffffffff;
    }
}

ParseAdoptedMsgLineProxyEvent::~ParseAdoptedMsgLineProxyEvent()
{
    if (m_msgLineInfo.adoptedMessageLine)
        PL_strfree(m_msgLineInfo.adoptedMessageLine);
}

NS_IMETHODIMP
ParseAdoptedMsgLineProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessage->ParseAdoptedMsgLine(
        m_proxy->m_protocol, &m_msgLineInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}
    
NormalEndMsgWriteStreamProxyEvent::NormalEndMsgWriteStreamProxyEvent(
    nsImapMessageProxy* aImapMessageProxy) :
    nsImapMessageProxyEvent(aImapMessageProxy)
{
}

NormalEndMsgWriteStreamProxyEvent::~NormalEndMsgWriteStreamProxyEvent()
{
}

NS_IMETHODIMP
NormalEndMsgWriteStreamProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessage->NormalEndMsgWriteStream(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}
    
AbortMsgWriteStreamProxyEvent::AbortMsgWriteStreamProxyEvent(
    nsImapMessageProxy* aImapMessageProxy) :
    nsImapMessageProxyEvent(aImapMessageProxy)
{
}

AbortMsgWriteStreamProxyEvent::~AbortMsgWriteStreamProxyEvent()
{
}

NS_IMETHODIMP
AbortMsgWriteStreamProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessage->AbortMsgWriteStream(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

OnlineCopyReportProxyEvent::OnlineCopyReportProxyEvent(
    nsImapMessageProxy* aImapMessageProxy,
    ImapOnlineCopyState* aCopyState) :
    nsImapMessageProxyEvent(aImapMessageProxy)
{
    NS_ASSERTION (aCopyState, "Oops... a null copy state");
    if (aCopyState)
    {
        m_copyState = *aCopyState;
    }
    else
    {
        m_copyState = kFailedCopy;
    }
}

OnlineCopyReportProxyEvent::~OnlineCopyReportProxyEvent()
{
}

NS_IMETHODIMP
OnlineCopyReportProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessage->OnlineCopyReport(
        m_proxy->m_protocol, &m_copyState); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

BeginMessageUploadProxyEvent::BeginMessageUploadProxyEvent(
    nsImapMessageProxy* aImapMessageProxy) :
    nsImapMessageProxyEvent(aImapMessageProxy)
{
}

BeginMessageUploadProxyEvent::~BeginMessageUploadProxyEvent()
{
}

NS_IMETHODIMP
BeginMessageUploadProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessage->BeginMessageUpload(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

UploadMessageFileProxyEvent::UploadMessageFileProxyEvent(
    nsImapMessageProxy* aImapMessageProxy,
    UploadMessageInfo* aMsgInfo) :
    nsImapMessageProxyEvent(aImapMessageProxy)
{
    NS_ASSERTION (aMsgInfo, "Oops... a null upload message info");
    if (aMsgInfo)
    {
        m_msgInfo = *aMsgInfo;
        m_msgInfo.dataBuffer = PL_strdup(aMsgInfo->dataBuffer);
    }
    else
    {
        m_msgInfo.newMsgID = 0xffffffff;
        m_msgInfo.bytesRemain = 0;
        m_msgInfo.dataBuffer = nsnull;
    }
}

UploadMessageFileProxyEvent::~UploadMessageFileProxyEvent()
{
    if (m_msgInfo.dataBuffer)
        PL_strfree(m_msgInfo.dataBuffer);
}

NS_IMETHODIMP
UploadMessageFileProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessage->UploadMessageFile(
        m_proxy->m_protocol, &m_msgInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

NotifyMessageFlagsProxyEvent::NotifyMessageFlagsProxyEvent(
    nsImapMessageProxy* aImapMessageProxy,
    FlagsKeyStruct* aKeyStruct) :
    nsImapMessageProxyEvent(aImapMessageProxy)
{
    NS_ASSERTION (aKeyStruct, "Oops... a null flags key struct");
    if (aKeyStruct)
    {
        m_keyStruct = *aKeyStruct;
    }
    else
    {
        m_keyStruct.flags = kNoFlags;
        m_keyStruct.key = 0xffffffff;
    }
}

NotifyMessageFlagsProxyEvent::~NotifyMessageFlagsProxyEvent()
{
}

NS_IMETHODIMP
NotifyMessageFlagsProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessage->NotifyMessageFlags(
        m_proxy->m_protocol, &m_keyStruct); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

NotifyMessageDeletedProxyEvent::NotifyMessageDeletedProxyEvent(
    nsImapMessageProxy* aImapMessageProxy,
    delete_message_struct* aStruct) :
    nsImapMessageProxyEvent(aImapMessageProxy)
{
    NS_ASSERTION (aStruct, "Oops... a null delete message struct");
    if (aStruct)
    {
        m_deleteMessageStruct.onlineFolderName = 
            PL_strdup(aStruct->onlineFolderName);
        m_deleteMessageStruct.deleteAllMsgs = aStruct->deleteAllMsgs;
        m_deleteMessageStruct.msgIdString = 
            PL_strdup(aStruct->msgIdString);
    }
    else
    {
        m_deleteMessageStruct.onlineFolderName = nsnull;
        m_deleteMessageStruct.deleteAllMsgs = PR_FALSE;
        m_deleteMessageStruct.msgIdString = nsnull;
    }
}

NotifyMessageDeletedProxyEvent::~NotifyMessageDeletedProxyEvent()
{
    if (m_deleteMessageStruct.onlineFolderName)
        PL_strfree(m_deleteMessageStruct.onlineFolderName);
    if (m_deleteMessageStruct.msgIdString)
        PL_strfree(m_deleteMessageStruct.msgIdString);
}

NS_IMETHODIMP
NotifyMessageDeletedProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessage->NotifyMessageDeleted(
        m_proxy->m_protocol, &m_deleteMessageStruct); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetMessageSizeFromDBProxyEvent::GetMessageSizeFromDBProxyEvent(
    nsImapMessageProxy* aImapMessageProxy,
    MessageSizeInfo* sizeInfo) :
    nsImapMessageProxyEvent(aImapMessageProxy)
{
    NS_ASSERTION (sizeInfo, "Oops... a null message size info");
    m_sizeInfo = sizeInfo;
}

GetMessageSizeFromDBProxyEvent::~GetMessageSizeFromDBProxyEvent()
{
}

NS_IMETHODIMP
GetMessageSizeFromDBProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessage->GetMessageSizeFromDB(
        m_proxy->m_protocol, m_sizeInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

nsImapExtensionProxyEvent::nsImapExtensionProxyEvent(
    nsImapExtensionProxy* aProxy)
{
    NS_ASSERTION (aProxy, "fatal a null imap extension proxy");
    m_proxy = aProxy;
    NS_IF_ADDREF (m_proxy);
}

nsImapExtensionProxyEvent::~nsImapExtensionProxyEvent()
{
    NS_IF_RELEASE (m_proxy);
}

SetUserAuthenticatedProxyEvent::SetUserAuthenticatedProxyEvent(
    nsImapExtensionProxy* aProxy, PRBool aBool) :
    nsImapExtensionProxyEvent(aProxy)
{
    m_bool = aBool;
}

SetUserAuthenticatedProxyEvent::~SetUserAuthenticatedProxyEvent()
{
}

NS_IMETHODIMP
SetUserAuthenticatedProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapExtension->SetUserAuthenticated(
        m_proxy->m_protocol, m_bool); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

SetMailServerUrlsProxyEvent::SetMailServerUrlsProxyEvent(
    nsImapExtensionProxy* aProxy, const char* hostName) :
    nsImapExtensionProxyEvent(aProxy)
{
    NS_ASSERTION (hostName, "Oops... a null host name");
    if (hostName)
        m_hostName = PL_strdup (hostName);
    else
        m_hostName = nsnull;
}

SetMailServerUrlsProxyEvent::~SetMailServerUrlsProxyEvent()
{
    if (m_hostName)
        PL_strfree(m_hostName);
}

NS_IMETHODIMP
SetMailServerUrlsProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapExtension->SetMailServerUrls(
        m_proxy->m_protocol, m_hostName); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

SetMailAccountUrlProxyEvent::SetMailAccountUrlProxyEvent(
    nsImapExtensionProxy* aProxy, const char* hostName) :
    nsImapExtensionProxyEvent(aProxy)
{
    NS_ASSERTION (hostName, "Oops... a null host name");
    if (hostName)
        m_hostName = PL_strdup (hostName);
    else
        m_hostName = nsnull;
}

SetMailAccountUrlProxyEvent::~SetMailAccountUrlProxyEvent()
{
    if (m_hostName)
        PL_strfree(m_hostName);
}

NS_IMETHODIMP
SetMailAccountUrlProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapExtension->SetMailAccountUrl(
        m_proxy->m_protocol, m_hostName); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ClearFolderRightsProxyEvent::ClearFolderRightsProxyEvent(
    nsImapExtensionProxy* aProxy, nsIMAPACLRightsInfo* aclRights) :
    nsImapExtensionProxyEvent(aProxy)
{
    NS_ASSERTION (aclRights, "Oops... a null acl rights info");
    if (aclRights)
    {
        m_aclRightsInfo.hostName = PL_strdup(aclRights->hostName);
        m_aclRightsInfo.mailboxName = PL_strdup(aclRights->mailboxName);
        m_aclRightsInfo.userName = PL_strdup(aclRights->userName);
        m_aclRightsInfo.rights = PL_strdup(aclRights->rights);
    }
    else
    {
        m_aclRightsInfo.hostName = nsnull;
        m_aclRightsInfo.mailboxName = nsnull;
        m_aclRightsInfo.userName = nsnull;
        m_aclRightsInfo.rights = nsnull;
    }
}

ClearFolderRightsProxyEvent::~ClearFolderRightsProxyEvent()
{
    if (m_aclRightsInfo.hostName)
        PL_strfree(m_aclRightsInfo.hostName);
    if (m_aclRightsInfo.mailboxName)
        PL_strfree(m_aclRightsInfo.mailboxName);
    if (m_aclRightsInfo.userName)
        PL_strfree(m_aclRightsInfo.userName);
    if (m_aclRightsInfo.rights)
        PL_strfree(m_aclRightsInfo.rights);
}

NS_IMETHODIMP
ClearFolderRightsProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapExtension->ClearFolderRights(
        m_proxy->m_protocol, &m_aclRightsInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

AddFolderRightsProxyEvent::AddFolderRightsProxyEvent(
    nsImapExtensionProxy* aProxy, nsIMAPACLRightsInfo* aclRights) :
    nsImapExtensionProxyEvent(aProxy)
{
    NS_ASSERTION (aclRights, "Oops... a null acl rights info");
    if (aclRights)
    {
        m_aclRightsInfo.hostName = PL_strdup(aclRights->hostName);
        m_aclRightsInfo.mailboxName = PL_strdup(aclRights->mailboxName);
        m_aclRightsInfo.userName = PL_strdup(aclRights->userName);
        m_aclRightsInfo.rights = PL_strdup(aclRights->rights);
    }
    else
    {
        m_aclRightsInfo.hostName = nsnull;
        m_aclRightsInfo.mailboxName = nsnull;
        m_aclRightsInfo.userName = nsnull;
        m_aclRightsInfo.rights = nsnull;
    }
}

AddFolderRightsProxyEvent::~AddFolderRightsProxyEvent()
{
    if (m_aclRightsInfo.hostName)
        PL_strfree(m_aclRightsInfo.hostName);
    if (m_aclRightsInfo.mailboxName)
        PL_strfree(m_aclRightsInfo.mailboxName);
    if (m_aclRightsInfo.userName)
        PL_strfree(m_aclRightsInfo.userName);
    if (m_aclRightsInfo.rights)
        PL_strfree(m_aclRightsInfo.rights);
}

NS_IMETHODIMP
AddFolderRightsProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapExtension->AddFolderRights(
        m_proxy->m_protocol, &m_aclRightsInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

RefreshFolderRightsProxyEvent::RefreshFolderRightsProxyEvent(
    nsImapExtensionProxy* aProxy, nsIMAPACLRightsInfo* aclRights) :
    nsImapExtensionProxyEvent(aProxy)
{
    NS_ASSERTION (aclRights, "Oops... a null acl rights info");
    if (aclRights)
    {
        m_aclRightsInfo.hostName = PL_strdup(aclRights->hostName);
        m_aclRightsInfo.mailboxName = PL_strdup(aclRights->mailboxName);
        m_aclRightsInfo.userName = PL_strdup(aclRights->userName);
        m_aclRightsInfo.rights = PL_strdup(aclRights->rights);
    }
    else
    {
        m_aclRightsInfo.hostName = nsnull;
        m_aclRightsInfo.mailboxName = nsnull;
        m_aclRightsInfo.userName = nsnull;
        m_aclRightsInfo.rights = nsnull;
    }
}

RefreshFolderRightsProxyEvent::~RefreshFolderRightsProxyEvent()
{
    if (m_aclRightsInfo.hostName)
        PL_strfree(m_aclRightsInfo.hostName);
    if (m_aclRightsInfo.mailboxName)
        PL_strfree(m_aclRightsInfo.mailboxName);
    if (m_aclRightsInfo.userName)
        PL_strfree(m_aclRightsInfo.userName);
    if (m_aclRightsInfo.rights)
        PL_strfree(m_aclRightsInfo.rights);
}

NS_IMETHODIMP
RefreshFolderRightsProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapExtension->RefreshFolderRights(
        m_proxy->m_protocol, &m_aclRightsInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

FolderNeedsACLInitializedProxyEvent::FolderNeedsACLInitializedProxyEvent(
    nsImapExtensionProxy* aProxy, nsIMAPACLRightsInfo* aclRights) :
    nsImapExtensionProxyEvent(aProxy)
{
    NS_ASSERTION (aclRights, "Oops... a null acl rights info");
    if (aclRights)
    {
        m_aclRightsInfo.hostName = PL_strdup(aclRights->hostName);
        m_aclRightsInfo.mailboxName = PL_strdup(aclRights->mailboxName);
        m_aclRightsInfo.userName = PL_strdup(aclRights->userName);
        m_aclRightsInfo.rights = PL_strdup(aclRights->rights);
    }
    else
    {
        m_aclRightsInfo.hostName = nsnull;
        m_aclRightsInfo.mailboxName = nsnull;
        m_aclRightsInfo.userName = nsnull;
        m_aclRightsInfo.rights = nsnull;
    }
}

FolderNeedsACLInitializedProxyEvent::~FolderNeedsACLInitializedProxyEvent()
{
    if (m_aclRightsInfo.hostName)
        PL_strfree(m_aclRightsInfo.hostName);
    if (m_aclRightsInfo.mailboxName)
        PL_strfree(m_aclRightsInfo.mailboxName);
    if (m_aclRightsInfo.userName)
        PL_strfree(m_aclRightsInfo.userName);
    if (m_aclRightsInfo.rights)
        PL_strfree(m_aclRightsInfo.rights);
}

NS_IMETHODIMP
FolderNeedsACLInitializedProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapExtension->FolderNeedsACLInitialized(
        m_proxy->m_protocol, &m_aclRightsInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

SetFolderAdminURLProxyEvent::SetFolderAdminURLProxyEvent(
    nsImapExtensionProxy* aProxy, FolderQueryInfo* aInfo) :
    nsImapExtensionProxyEvent(aProxy)
{
    NS_ASSERTION (aInfo, "Oops... a null folder query info");
    if (aInfo)
    {
        m_folderQueryInfo.name = PL_strdup(aInfo->name);
        m_folderQueryInfo.hostName = PL_strdup(aInfo->hostName);
        m_folderQueryInfo.rv = aInfo->rv;
    }
    else
    {
        memset(&m_folderQueryInfo, 0, sizeof(FolderQueryInfo));
    }
}

SetFolderAdminURLProxyEvent::~SetFolderAdminURLProxyEvent()
{
    if (m_folderQueryInfo.name)
        PL_strfree(m_folderQueryInfo.name);
    if (m_folderQueryInfo.hostName)
        PL_strfree(m_folderQueryInfo.hostName);
}

NS_IMETHODIMP
SetFolderAdminURLProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapExtension->SetFolderAdminURL(
        m_proxy->m_protocol, &m_folderQueryInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

nsImapMiscellaneousProxyEvent::nsImapMiscellaneousProxyEvent(
    nsImapMiscellaneousProxy* aProxy)
{
    NS_ASSERTION (aProxy, "fatal: a null imap miscellaneous proxy");
    m_proxy = aProxy;
    NS_IF_ADDREF (m_proxy);
}

nsImapMiscellaneousProxyEvent::~nsImapMiscellaneousProxyEvent()
{
    NS_IF_RELEASE (m_proxy);
}

AddSearchResultProxyEvent::AddSearchResultProxyEvent(
    nsImapMiscellaneousProxy* aProxy, const char* searchHitLine) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (searchHitLine, "Oops... a null search hit line");
    if (searchHitLine)
        m_searchHitLine = PL_strdup(searchHitLine);
    else
        m_searchHitLine = nsnull;
}

AddSearchResultProxyEvent::~AddSearchResultProxyEvent()
{
    if (m_searchHitLine)
        PL_strfree(m_searchHitLine);
}

NS_IMETHODIMP
AddSearchResultProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->AddSearchResult(
        m_proxy->m_protocol, m_searchHitLine);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetArbitraryHeadersProxyEvent::GetArbitraryHeadersProxyEvent(
    nsImapMiscellaneousProxy* aProxy, GenericInfo* aInfo) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (aInfo, "Oops... a null info");
    m_info = aInfo;
}

GetArbitraryHeadersProxyEvent::~GetArbitraryHeadersProxyEvent()
{
}

NS_IMETHODIMP
GetArbitraryHeadersProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->GetArbitraryHeaders(
        m_proxy->m_protocol, m_info);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetShouldDownloadArbitraryHeadersProxyEvent::GetShouldDownloadArbitraryHeadersProxyEvent(
    nsImapMiscellaneousProxy* aProxy, GenericInfo* aInfo) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (aInfo, "Oops... a null info");
    m_info = aInfo;
}

GetShouldDownloadArbitraryHeadersProxyEvent::~GetShouldDownloadArbitraryHeadersProxyEvent()
{
}

NS_IMETHODIMP
GetShouldDownloadArbitraryHeadersProxyEvent::HandleEvent()
{
    nsresult res = 
        m_proxy->m_realImapMiscellaneous->GetShouldDownloadArbitraryHeaders(
            m_proxy->m_protocol, m_info);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetShowAttachmentsInlineProxyEvent::GetShowAttachmentsInlineProxyEvent(
    nsImapMiscellaneousProxy* aProxy, PRBool* aBool) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (aBool, "Oops... a null bool pointer");
    m_bool = aBool;
}

GetShowAttachmentsInlineProxyEvent::~GetShowAttachmentsInlineProxyEvent()
{
}

NS_IMETHODIMP
GetShowAttachmentsInlineProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->GetShowAttachmentsInline(
        m_proxy->m_protocol, m_bool);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

HeaderFetchCompletedProxyEvent::HeaderFetchCompletedProxyEvent(
    nsImapMiscellaneousProxy* aProxy) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
}

HeaderFetchCompletedProxyEvent::~HeaderFetchCompletedProxyEvent()
{
}

NS_IMETHODIMP
HeaderFetchCompletedProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->HeaderFetchCompleted(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

FinishImapConnectionProxyEvent::FinishImapConnectionProxyEvent(
    nsImapMiscellaneousProxy* aProxy) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
}

FinishImapConnectionProxyEvent::~FinishImapConnectionProxyEvent()
{
}

NS_IMETHODIMP
FinishImapConnectionProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->FinishImapConnection(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

UpdateSecurityStatusProxyEvent::UpdateSecurityStatusProxyEvent(
    nsImapMiscellaneousProxy* aProxy) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
}

UpdateSecurityStatusProxyEvent::~UpdateSecurityStatusProxyEvent()
{
}

NS_IMETHODIMP
UpdateSecurityStatusProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->UpdateSecurityStatus(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

SetImapHostPasswordProxyEvent::SetImapHostPasswordProxyEvent(
    nsImapMiscellaneousProxy* aProxy, GenericInfo* aInfo) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (aInfo, "Oops... a null info");
    if (aInfo)
    {
        m_info.c = PL_strdup(aInfo->c);
        m_info.hostName = PL_strdup(aInfo->hostName);
        m_info.rv = aInfo->rv;
    }
    else
    {
        memset(&m_info, 0, sizeof(GenericInfo));
    }
}

SetImapHostPasswordProxyEvent::~SetImapHostPasswordProxyEvent()
{
    if (m_info.c)
        PL_strfree(m_info.c);
    if (m_info.hostName)
        PL_strfree(m_info.hostName);
}

NS_IMETHODIMP
SetImapHostPasswordProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->SetImapHostPassword(
        m_proxy->m_protocol, &m_info);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetPasswordForUserProxyEvent::GetPasswordForUserProxyEvent(
    nsImapMiscellaneousProxy* aProxy, const char* userName) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (userName, "Oops... a null username");
    if (userName)
        m_userName = PL_strdup(userName);
    else
        m_userName = nsnull;
}

GetPasswordForUserProxyEvent::~GetPasswordForUserProxyEvent()
{
    if (m_userName)
        PL_strfree(m_userName);
}

NS_IMETHODIMP
GetPasswordForUserProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->GetPasswordForUser(
        m_proxy->m_protocol, m_userName);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}


SetBiffStateAndUpdateProxyEvent::SetBiffStateAndUpdateProxyEvent(
    nsImapMiscellaneousProxy* aProxy, nsMsgBiffState biffState) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    m_biffState = biffState;
}

SetBiffStateAndUpdateProxyEvent::~SetBiffStateAndUpdateProxyEvent()
{
}

NS_IMETHODIMP
SetBiffStateAndUpdateProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->SetBiffStateAndUpdate(
        m_proxy->m_protocol, m_biffState);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetStoredUIDValidityProxyEvent::GetStoredUIDValidityProxyEvent(
    nsImapMiscellaneousProxy* aProxy, uid_validity_info* aInfo) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (aInfo, "Oops... a null uid validity info");
    if (aInfo)
    {
        m_uidValidityInfo.canonical_boxname = 
            PL_strdup(aInfo->canonical_boxname);
        m_uidValidityInfo.hostName = aInfo->hostName;
        m_uidValidityInfo.returnValidity = aInfo->returnValidity;
    }
    else
    {
        m_uidValidityInfo.canonical_boxname = nsnull;
        m_uidValidityInfo.hostName = nsnull;
        m_uidValidityInfo.returnValidity = 0;
    }
}

GetStoredUIDValidityProxyEvent::~GetStoredUIDValidityProxyEvent()
{
    if (m_uidValidityInfo.canonical_boxname)
        PL_strfree(m_uidValidityInfo.canonical_boxname);
}

NS_IMETHODIMP
GetStoredUIDValidityProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->GetStoredUIDValidity(
        m_proxy->m_protocol, &m_uidValidityInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

LiteSelectUIDValidityProxyEvent::LiteSelectUIDValidityProxyEvent(
    nsImapMiscellaneousProxy* aProxy, PRUint32 uidValidity) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    m_uidValidity = uidValidity;
}

LiteSelectUIDValidityProxyEvent::~LiteSelectUIDValidityProxyEvent()
{
}

NS_IMETHODIMP
LiteSelectUIDValidityProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->LiteSelectUIDValidity(
        m_proxy->m_protocol, m_uidValidity);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

FEAlertProxyEvent::FEAlertProxyEvent(
    nsImapMiscellaneousProxy* aProxy, const char* alertString) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (alertString, "Oops... a null alertString");
    if (alertString)
        m_alertString = PL_strdup(alertString);
    else
        m_alertString = nsnull;
}

FEAlertProxyEvent::~FEAlertProxyEvent()
{
    if (m_alertString)
        PL_strfree(m_alertString);
}

NS_IMETHODIMP
FEAlertProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->FEAlert(
        m_proxy->m_protocol, m_alertString);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

FEAlertFromServerProxyEvent::FEAlertFromServerProxyEvent(
    nsImapMiscellaneousProxy* aProxy, const char* alertString) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (alertString, "Oops... a null alertString");
    if (alertString)
        m_alertString = PL_strdup(alertString);
    else
        m_alertString = nsnull;
}

FEAlertFromServerProxyEvent::~FEAlertFromServerProxyEvent()
{
    if (m_alertString)
        PL_strfree(m_alertString);
}

NS_IMETHODIMP
FEAlertFromServerProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->FEAlertFromServer(
        m_proxy->m_protocol, m_alertString);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ProgressStatusProxyEvent::ProgressStatusProxyEvent(
    nsImapMiscellaneousProxy* aProxy, const char* statusMsg) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (statusMsg, "Oops... a null statusMsg");
    if (statusMsg)
        m_statusMsg = PL_strdup(statusMsg);
    else
        m_statusMsg = nsnull;
}

ProgressStatusProxyEvent::~ProgressStatusProxyEvent()
{
    if (m_statusMsg)
        PL_strfree(m_statusMsg);
}

NS_IMETHODIMP
ProgressStatusProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->ProgressStatus(
        m_proxy->m_protocol, m_statusMsg);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

PercentProgressProxyEvent::PercentProgressProxyEvent(
    nsImapMiscellaneousProxy* aProxy, ProgressInfo* aInfo) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (aInfo, "Oops... a null progress info");
    if (aInfo)
    {
        m_progressInfo.message = PL_strdup(aInfo->message);
        m_progressInfo.percent = aInfo->percent;
    }
    else
    {
        m_progressInfo.message = nsnull;
        m_progressInfo.percent = 0;
    }
}

PercentProgressProxyEvent::~PercentProgressProxyEvent()
{
    if (m_progressInfo.message)
        PL_strfree(m_progressInfo.message);
}

NS_IMETHODIMP
PercentProgressProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->PercentProgress(
        m_proxy->m_protocol, &m_progressInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

PastPasswordCheckProxyEvent::PastPasswordCheckProxyEvent(
    nsImapMiscellaneousProxy* aProxy) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
}

PastPasswordCheckProxyEvent::~PastPasswordCheckProxyEvent()
{
}

NS_IMETHODIMP
PastPasswordCheckProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->PastPasswordCheck(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

CommitNamespacesProxyEvent::CommitNamespacesProxyEvent(
    nsImapMiscellaneousProxy* aProxy, const char* hostName) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (hostName, "Oops... a null host name");
    if (hostName)
        m_hostName = PL_strdup(hostName);
    else
        m_hostName = nsnull;
}

CommitNamespacesProxyEvent::~CommitNamespacesProxyEvent()
{
    if (m_hostName)
        PL_strfree(m_hostName);
}

NS_IMETHODIMP
CommitNamespacesProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->CommitNamespaces(
        m_proxy->m_protocol, m_hostName);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

CommitCapabilityForHostProxyEvent::CommitCapabilityForHostProxyEvent(
    nsImapMiscellaneousProxy* aProxy, const char* hostName) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (hostName, "Oops... a null host name");
    if (hostName)
        m_hostName = PL_strdup(hostName);
    else
        m_hostName = nsnull;
}

CommitCapabilityForHostProxyEvent::~CommitCapabilityForHostProxyEvent()
{
    if (m_hostName)
        PL_strfree(m_hostName);
}

NS_IMETHODIMP
CommitCapabilityForHostProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->CommitCapabilityForHost(
        m_proxy->m_protocol, m_hostName);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

TunnelOutStreamProxyEvent::TunnelOutStreamProxyEvent(
    nsImapMiscellaneousProxy* aProxy, msg_line_info* aInfo) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (aInfo, "Oops... a null message line info");
    if (aInfo)
    {
        m_msgLineInfo.adoptedMessageLine = 
            PL_strdup(aInfo->adoptedMessageLine);
        m_msgLineInfo.uidOfMessage = aInfo->uidOfMessage;
    }
    else
    {
        m_msgLineInfo.adoptedMessageLine = nsnull;
        m_msgLineInfo.uidOfMessage = 0xffffffff;
    }
}

TunnelOutStreamProxyEvent::~TunnelOutStreamProxyEvent()
{
    if (m_msgLineInfo.adoptedMessageLine)
        PL_strfree(m_msgLineInfo.adoptedMessageLine);
}

NS_IMETHODIMP
TunnelOutStreamProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->TunnelOutStream(
        m_proxy->m_protocol, &m_msgLineInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ProcessTunnelProxyEvent::ProcessTunnelProxyEvent(
    nsImapMiscellaneousProxy* aProxy, TunnelInfo *aInfo) :
    nsImapMiscellaneousProxyEvent(aProxy)
{
    NS_ASSERTION (aInfo, "Oops... a null tunnel info");
    if (aInfo)
    {
        m_tunnelInfo = *aInfo;
    }
    else
    {
        memset(&m_tunnelInfo, 0, sizeof(TunnelInfo));
    }
}

ProcessTunnelProxyEvent::~ProcessTunnelProxyEvent()
{
}

NS_IMETHODIMP
ProcessTunnelProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneous->ProcessTunnel(
        m_proxy->m_protocol, &m_tunnelInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}
