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
#include "nsIImapMailFolderSink.h"
#include "nsIImapMessageSink.h"
#include "nsIImapExtensionSink.h"
#include "nsIImapMiscellaneousSink.h"
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

// ***** class implementation of nsImapMailFolderSinkProxy *****

nsImapMailFolderSinkProxy::nsImapMailFolderSinkProxy(
    nsIImapMailFolderSink* aImapMailFolderSink,
    nsIImapProtocol* aProtocol,
    PLEventQueue* aEventQ,
    PRThread* aThread) : nsImapProxyBase(aProtocol, aEventQ, aThread)
{
    NS_ASSERTION (aImapMailFolderSink, 
                  "nsImapMailFolderSinkProxy: invalid aImapMailFolderSink");
    NS_INIT_REFCNT ();
    m_realImapMailFolderSink = aImapMailFolderSink;
    NS_IF_ADDREF (m_realImapMailFolderSink);
}

nsImapMailFolderSinkProxy::~nsImapMailFolderSinkProxy()
{
    NS_IF_RELEASE (m_realImapMailFolderSink);
}

static NS_DEFINE_IID(kIImapMailFolderSinkIID, NS_IIMAPMAILFOLDERSINK_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsImapMailFolderSinkProxy, kIImapMailFolderSinkIID);

NS_IMETHODIMP
nsImapMailFolderSinkProxy::PossibleImapMailbox(nsIImapProtocol* aProtocol,
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
        res = m_realImapMailFolderSink->PossibleImapMailbox(aProtocol,
                                                        aSpec);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::MailboxDiscoveryDone(nsIImapProtocol* aProtocol)
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
        res = m_realImapMailFolderSink->MailboxDiscoveryDone(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::UpdateImapMailboxInfo(nsIImapProtocol* aProtocol,
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
        res = m_realImapMailFolderSink->UpdateImapMailboxInfo(aProtocol,
                                                        aSpec);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::UpdateImapMailboxStatus(nsIImapProtocol* aProtocol,
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
        res = m_realImapMailFolderSink->UpdateImapMailboxStatus(aProtocol,
                                                        aSpec);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::ChildDiscoverySucceeded(nsIImapProtocol* aProtocol)
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
        res = m_realImapMailFolderSink->ChildDiscoverySucceeded(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::OnlineFolderDelete(nsIImapProtocol* aProtocol,
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
        res = m_realImapMailFolderSink->OnlineFolderDelete(aProtocol,
                                                       folderName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::OnlineFolderCreateFailed(nsIImapProtocol* aProtocol,
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
        res = m_realImapMailFolderSink->OnlineFolderCreateFailed(aProtocol,
                                                             folderName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::OnlineFolderRename(nsIImapProtocol* aProtocol,
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
        res = m_realImapMailFolderSink->OnlineFolderRename(aProtocol,
                                                       aStruct);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::SubscribeUpgradeFinished(nsIImapProtocol* aProtocol,
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
        res = m_realImapMailFolderSink->SubscribeUpgradeFinished(aProtocol,
                                                             aState);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::PromptUserForSubscribeUpdatePath(
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
        res = m_realImapMailFolderSink->PromptUserForSubscribeUpdatePath
                                                         (aProtocol, aBool);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::FolderIsNoSelect(nsIImapProtocol* aProtocol,
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
        res = m_realImapMailFolderSink->FolderIsNoSelect(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::SetupHeaderParseStream(nsIImapProtocol* aProtocol,
                                        StreamInfo* aStreamInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aStreamInfo, "Oops... null aStreamInfo");
    if(!aStreamInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        SetupHeaderParseStreamProxyEvent *ev =
            new SetupHeaderParseStreamProxyEvent(this, aStreamInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailFolderSink->SetupHeaderParseStream(aProtocol, aStreamInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::ParseAdoptedHeaderLine(nsIImapProtocol* aProtocol,
                                        msg_line_info* aMsgLineInfo)
{
    nsresult res = NS_OK;
    NS_PRECONDITION (aMsgLineInfo, "Oops... null aMsgLineInfo");
    if(!aMsgLineInfo)
        return NS_ERROR_NULL_POINTER;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        ParseAdoptedHeaderLineProxyEvent *ev =
            new ParseAdoptedHeaderLineProxyEvent(this, aMsgLineInfo);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailFolderSink->ParseAdoptedHeaderLine(aProtocol, aMsgLineInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::NormalEndHeaderParseStream(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
        NormalEndHeaderParseStreamProxyEvent *ev =
            new NormalEndHeaderParseStreamProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailFolderSink->NormalEndHeaderParseStream(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMailFolderSinkProxy::AbortHeaderParseStream(nsIImapProtocol* aProtocol)
{
    nsresult res = NS_OK;
    NS_ASSERTION (m_protocol == aProtocol, "Ooh ooh, wrong protocol");

    if (PR_GetCurrentThread() == m_thread)
    {
       AbortHeaderParseStreamProxyEvent *ev =
            new AbortHeaderParseStreamProxyEvent(this);
        if(nsnull == ev)
            res = NS_ERROR_OUT_OF_MEMORY;
        else
            ev->PostEvent(m_eventQueue);
    }
    else
    {
        res = m_realImapMailFolderSink->AbortHeaderParseStream(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}


nsImapMessageSinkProxy::nsImapMessageSinkProxy(nsIImapMessageSink* aImapMessageSink,
                                       nsIImapProtocol* aProtocol,
                                       PLEventQueue* aEventQ,
                                       PRThread* aThread) :
    nsImapProxyBase(aProtocol, aEventQ, aThread)
{
    NS_ASSERTION (aImapMessageSink, 
                  "nsImapMessageSinkProxy: invalid aImapMessageSink");
    NS_INIT_REFCNT ();
    m_realImapMessageSink = aImapMessageSink;
    NS_IF_ADDREF (m_realImapMessageSink);
}

nsImapMessageSinkProxy::~nsImapMessageSinkProxy()
{
    NS_IF_ADDREF (m_realImapMessageSink);
}

static NS_DEFINE_IID(kIImapMessageSinkIID, NS_IIMAPMESSAGESINK_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsImapMessageSinkProxy, kIImapMessageSinkIID);

NS_IMETHODIMP
nsImapMessageSinkProxy::SetupMsgWriteStream(nsIImapProtocol* aProtocol,
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
        res = m_realImapMessageSink->SetupMsgWriteStream(aProtocol, aStreamInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageSinkProxy::ParseAdoptedMsgLine(nsIImapProtocol* aProtocol,
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
        res = m_realImapMessageSink->ParseAdoptedMsgLine(aProtocol, aMsgLineInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageSinkProxy::NormalEndMsgWriteStream(nsIImapProtocol* aProtocol)
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
        res = m_realImapMessageSink->NormalEndMsgWriteStream(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageSinkProxy::AbortMsgWriteStream(nsIImapProtocol* aProtocol)
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
        res = m_realImapMessageSink->AbortMsgWriteStream(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageSinkProxy::OnlineCopyReport(nsIImapProtocol* aProtocol,
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
        res = m_realImapMessageSink->OnlineCopyReport(aProtocol, aCopyState);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageSinkProxy::BeginMessageUpload(nsIImapProtocol* aProtocol)
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
        res = m_realImapMessageSink->BeginMessageUpload(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageSinkProxy::UploadMessageFile(nsIImapProtocol* aProtocol,
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
        res = m_realImapMessageSink->UploadMessageFile(aProtocol, aMsgInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageSinkProxy::NotifyMessageFlags(nsIImapProtocol* aProtocol,
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
        res = m_realImapMessageSink->NotifyMessageFlags(aProtocol, aKeyStruct);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageSinkProxy::NotifyMessageDeleted(nsIImapProtocol* aProtocol,
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
        res = m_realImapMessageSink->NotifyMessageDeleted(aProtocol, aStruct);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMessageSinkProxy::GetMessageSizeFromDB(nsIImapProtocol* aProtocol,
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
        res = m_realImapMessageSink->GetMessageSizeFromDB(aProtocol, sizeInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

nsImapExtensionSinkProxy::nsImapExtensionSinkProxy(nsIImapExtensionSink* aImapExtensionSink,
                                           nsIImapProtocol* aProtocol,
                                           PLEventQueue* aEventQ,
                                           PRThread* aThread) :
    nsImapProxyBase(aProtocol, aEventQ, aThread)
{
    NS_ASSERTION (aImapExtensionSink, 
                  "nsImapExtensionSinkProxy: invalid aImapExtensionSink");
    NS_INIT_REFCNT ();
    m_realImapExtensionSink = aImapExtensionSink;
    NS_IF_ADDREF (m_realImapExtensionSink);
}

nsImapExtensionSinkProxy::~nsImapExtensionSinkProxy()
{
    NS_IF_ADDREF (m_realImapExtensionSink);
}

static NS_DEFINE_IID(kIImapExtensionSinkIID, NS_IIMAPEXTENSIONSINK_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsImapExtensionSinkProxy, kIImapExtensionSinkIID);

NS_IMETHODIMP
nsImapExtensionSinkProxy::SetUserAuthenticated(nsIImapProtocol* aProtocol,
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
        res = m_realImapExtensionSink->SetUserAuthenticated(aProtocol, aBool);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionSinkProxy::SetMailServerUrls(nsIImapProtocol* aProtocol,
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
        res = m_realImapExtensionSink->SetMailServerUrls(aProtocol, hostName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionSinkProxy::SetMailAccountUrl(nsIImapProtocol* aProtocol,
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
        res = m_realImapExtensionSink->SetMailAccountUrl(aProtocol, hostName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionSinkProxy::ClearFolderRights(nsIImapProtocol* aProtocol,
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
        res = m_realImapExtensionSink->ClearFolderRights(aProtocol, aclRights);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionSinkProxy::AddFolderRights(nsIImapProtocol* aProtocol,
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
        res = m_realImapExtensionSink->AddFolderRights(aProtocol, aclRights);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionSinkProxy::RefreshFolderRights(nsIImapProtocol* aProtocol,
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
        res = m_realImapExtensionSink->RefreshFolderRights(aProtocol, aclRights);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionSinkProxy::FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
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
        res = m_realImapExtensionSink->FolderNeedsACLInitialized(aProtocol,
                                                             aclRights);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapExtensionSinkProxy::SetFolderAdminURL(nsIImapProtocol* aProtocol,
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
        res = m_realImapExtensionSink->SetFolderAdminURL(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

nsImapMiscellaneousSinkProxy::nsImapMiscellaneousSinkProxy(
    nsIImapMiscellaneousSink* aImapMiscellaneousSink, 
    nsIImapProtocol* aProtocol,
    PLEventQueue* aEventQ,
    PRThread* aThread) : nsImapProxyBase(aProtocol, aEventQ, aThread)
{
    NS_ASSERTION (aImapMiscellaneousSink, 
                  "nsImapMiscellaneousSinkProxy: invalid aImapMiscellaneousSink");
    NS_INIT_REFCNT ();
    m_realImapMiscellaneousSink = aImapMiscellaneousSink;
    NS_IF_ADDREF (m_realImapMiscellaneousSink);
}

nsImapMiscellaneousSinkProxy::~nsImapMiscellaneousSinkProxy()
{
    NS_IF_ADDREF (m_realImapMiscellaneousSink);
}

static NS_DEFINE_IID(kIImapMiscellaneousSinkIID, NS_IIMAPMISCELLANEOUSSINK_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsImapMiscellaneousSinkProxy, kIImapMiscellaneousSinkIID);

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::AddSearchResult(nsIImapProtocol* aProtocol, 
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
        res = m_realImapMiscellaneousSink->AddSearchResult(aProtocol, searchHitLine);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::GetArbitraryHeaders(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->GetArbitraryHeaders(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::GetShouldDownloadArbitraryHeaders(
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
            m_realImapMiscellaneousSink->GetShouldDownloadArbitraryHeaders(aProtocol,
                                                                   aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::GetShowAttachmentsInline(
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
            m_realImapMiscellaneousSink->GetShowAttachmentsInline(aProtocol,
                                                              aBool);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::HeaderFetchCompleted(nsIImapProtocol* aProtocol)
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
        res = m_realImapMiscellaneousSink->HeaderFetchCompleted(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::UpdateSecurityStatus(nsIImapProtocol* aProtocol)
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
        res = m_realImapMiscellaneousSink->UpdateSecurityStatus(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::FinishImapConnection(nsIImapProtocol* aProtocol)
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
        res = m_realImapMiscellaneousSink->FinishImapConnection(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::SetImapHostPassword(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->SetImapHostPassword(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::GetPasswordForUser(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->GetPasswordForUser(aProtocol, userName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->SetBiffStateAndUpdate(aProtocol, biffState);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::GetStoredUIDValidity(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->GetStoredUIDValidity(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::LiteSelectUIDValidity(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->LiteSelectUIDValidity(aProtocol, uidValidity);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::FEAlert(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->FEAlert(aProtocol, aString);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::FEAlertFromServer(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->FEAlertFromServer(aProtocol, aString);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::ProgressStatus(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->ProgressStatus(aProtocol, statusMsg);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::PercentProgress(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->PercentProgress(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::PastPasswordCheck(nsIImapProtocol* aProtocol)
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
        res = m_realImapMiscellaneousSink->PastPasswordCheck(aProtocol);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::CommitNamespaces(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->CommitNamespaces(aProtocol, hostName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::CommitCapabilityForHost(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->CommitCapabilityForHost(aProtocol, hostName);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::TunnelOutStream(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->TunnelOutStream(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

NS_IMETHODIMP
nsImapMiscellaneousSinkProxy::ProcessTunnel(nsIImapProtocol* aProtocol,
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
        res = m_realImapMiscellaneousSink->ProcessTunnel(aProtocol, aInfo);
        aProtocol->NotifyFEEventCompletion();
    }
    return res;
}

nsImapMailFolderSinkProxyEvent::nsImapMailFolderSinkProxyEvent(nsImapMailFolderSinkProxy*
                                                       aProxy)
{
    NS_ASSERTION (aProxy, "fatal null proxy object");
    m_proxy = aProxy;
    NS_IF_ADDREF (m_proxy);
}

nsImapMailFolderSinkProxyEvent::~nsImapMailFolderSinkProxyEvent()
{
    NS_IF_RELEASE (m_proxy);
}

PossibleImapMailboxProxyEvent::PossibleImapMailboxProxyEvent(
    nsImapMailFolderSinkProxy* aProxy, mailbox_spec* aSpec) :
    nsImapMailFolderSinkProxyEvent(aProxy)
{
    NS_ASSERTION (aSpec, "PossibleImapMailboxProxyEvent: null aSpec");
    if (aSpec)
    {
        m_mailboxSpec = *aSpec;
        if (aSpec->allocatedPathName)
            m_mailboxSpec.allocatedPathName =
                PL_strdup(aSpec->allocatedPathName); 
#if 0 // mscott - we appear to be creating a new name space on top of the existing one..
      // i don't really understand what is going on here. but aSpec is pointing to the same
	  // object as m_mailboxSpec. so m_mailboxSpec.namespacesforfolder =new (aspec->namespacesforfolder)
	  // isn't going to fly...
        if (aSpec->namespaceForFolder)
            m_mailboxSpec.namespaceForFolder = 
                new nsIMAPNamespace(aSpec->namespaceForFolder->GetType(),
                                    aSpec->namespaceForFolder->GetPrefix(),
                                    aSpec->namespaceForFolder->GetDelimiter(),
                                    aSpec->namespaceForFolder->GetIsNamespaceFromPrefs());
#endif
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
// mscott - again, i didn't actually copy the namespace over see my comment in the function above...so we 
// shouldn't delete the name space as this is the original!
//    if (m_mailboxSpec.namespaceForFolder)
//        delete m_mailboxSpec.namespaceForFolder;
}

NS_IMETHODIMP
PossibleImapMailboxProxyEvent::HandleEvent()
{
    nsresult res =
        m_proxy->m_realImapMailFolderSink->PossibleImapMailbox(
            m_proxy->m_protocol, &m_mailboxSpec);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

MailboxDiscoveryDoneProxyEvent::MailboxDiscoveryDoneProxyEvent(
    nsImapMailFolderSinkProxy* aProxy) :
    nsImapMailFolderSinkProxyEvent(aProxy)
{
}

MailboxDiscoveryDoneProxyEvent::~MailboxDiscoveryDoneProxyEvent()
{
}

NS_IMETHODIMP
MailboxDiscoveryDoneProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailFolderSink->MailboxDiscoveryDone(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

UpdateImapMailboxInfoProxyEvent::UpdateImapMailboxInfoProxyEvent(
    nsImapMailFolderSinkProxy* aProxy, mailbox_spec* aSpec) :
    nsImapMailFolderSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMailFolderSink->UpdateImapMailboxInfo(
        m_proxy->m_protocol, &m_mailboxSpec);
//    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

UpdateImapMailboxStatusProxyEvent::UpdateImapMailboxStatusProxyEvent(
    nsImapMailFolderSinkProxy* aProxy, mailbox_spec* aSpec) :
    nsImapMailFolderSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMailFolderSink->UpdateImapMailboxStatus(
        m_proxy->m_protocol, &m_mailboxSpec);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ChildDiscoverySucceededProxyEvent::ChildDiscoverySucceededProxyEvent(
    nsImapMailFolderSinkProxy* aProxy) :
    nsImapMailFolderSinkProxyEvent(aProxy)
{
}

ChildDiscoverySucceededProxyEvent::~ChildDiscoverySucceededProxyEvent()
{
}

NS_IMETHODIMP
ChildDiscoverySucceededProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailFolderSink->ChildDiscoverySucceeded(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

OnlineFolderDeleteProxyEvent::OnlineFolderDeleteProxyEvent(
    nsImapMailFolderSinkProxy* aProxy, const char* folderName) :
    nsImapMailFolderSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMailFolderSink->OnlineFolderDelete(
        m_proxy->m_protocol, m_folderName); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

OnlineFolderCreateFailedProxyEvent::OnlineFolderCreateFailedProxyEvent(
    nsImapMailFolderSinkProxy* aProxy, const char* folderName) :
    nsImapMailFolderSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMailFolderSink->OnlineFolderCreateFailed(
        m_proxy->m_protocol, m_folderName);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

OnlineFolderRenameProxyEvent::OnlineFolderRenameProxyEvent(
    nsImapMailFolderSinkProxy* aProxy, folder_rename_struct* aStruct) :
    nsImapMailFolderSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMailFolderSink->OnlineFolderRename(
        m_proxy->m_protocol, &m_folderRenameStruct);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

SubscribeUpgradeFinishedProxyEvent::SubscribeUpgradeFinishedProxyEvent(
    nsImapMailFolderSinkProxy* aProxy, EIMAPSubscriptionUpgradeState* aState) :
    nsImapMailFolderSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMailFolderSink->SubscribeUpgradeFinished(
        m_proxy->m_protocol, &m_state);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

PromptUserForSubscribeUpdatePathProxyEvent::PromptUserForSubscribeUpdatePathProxyEvent(
    nsImapMailFolderSinkProxy* aProxy, PRBool* aBool) :
    nsImapMailFolderSinkProxyEvent(aProxy)
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
        m_proxy->m_realImapMailFolderSink->PromptUserForSubscribeUpdatePath(
            m_proxy->m_protocol, &m_bool); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

FolderIsNoSelectProxyEvent::FolderIsNoSelectProxyEvent(
    nsImapMailFolderSinkProxy* aProxy, FolderQueryInfo* aInfo) :
    nsImapMailFolderSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMailFolderSink->FolderIsNoSelect(
        m_proxy->m_protocol, &m_folderQueryInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}


///

SetupHeaderParseStreamProxyEvent::SetupHeaderParseStreamProxyEvent(
    nsImapMailFolderSinkProxy* aImapFolderProxy,
    StreamInfo* aStreamInfo) :
    nsImapMailFolderSinkProxyEvent(aImapFolderProxy)
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

SetupHeaderParseStreamProxyEvent::~SetupHeaderParseStreamProxyEvent()
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
SetupHeaderParseStreamProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailFolderSink->SetupHeaderParseStream(
        m_proxy->m_protocol, &m_streamInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ParseAdoptedHeaderLineProxyEvent::ParseAdoptedHeaderLineProxyEvent(
    nsImapMailFolderSinkProxy* aImapFolderProxy,
    msg_line_info* aMsgLineInfo) :
    nsImapMailFolderSinkProxyEvent(aImapFolderProxy)
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

ParseAdoptedHeaderLineProxyEvent::~ParseAdoptedHeaderLineProxyEvent()
{
    if (m_msgLineInfo.adoptedMessageLine)
        PL_strfree(m_msgLineInfo.adoptedMessageLine);
}

NS_IMETHODIMP
ParseAdoptedHeaderLineProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailFolderSink->ParseAdoptedHeaderLine(
        m_proxy->m_protocol, &m_msgLineInfo);
//    m_proxy->m_protocol->NotifyFEEventCompletion();
	// imap thread is NOT waiting for FEEvent completion, so don't send it.
    return res;
}
    
NormalEndHeaderParseStreamProxyEvent::NormalEndHeaderParseStreamProxyEvent(
    nsImapMailFolderSinkProxy* aImapFolderProxy) :
    nsImapMailFolderSinkProxyEvent(aImapFolderProxy)
{
}

NormalEndHeaderParseStreamProxyEvent::~NormalEndHeaderParseStreamProxyEvent()
{
}

NS_IMETHODIMP
NormalEndHeaderParseStreamProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailFolderSink->NormalEndHeaderParseStream(
        m_proxy->m_protocol);
	// IMAP thread is NOT waiting for FEEvent Completion.
//    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}
    
AbortHeaderParseStreamProxyEvent::AbortHeaderParseStreamProxyEvent(
    nsImapMailFolderSinkProxy* aImapFolderProxy) :
    nsImapMailFolderSinkProxyEvent(aImapFolderProxy)
{
}

AbortHeaderParseStreamProxyEvent::~AbortHeaderParseStreamProxyEvent()
{
}

NS_IMETHODIMP
AbortHeaderParseStreamProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMailFolderSink->AbortHeaderParseStream(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

////
nsImapMessageSinkProxyEvent::nsImapMessageSinkProxyEvent(nsImapMessageSinkProxy* aProxy)
{
    NS_ASSERTION (aProxy, "fatal null proxy object");
    m_proxy = aProxy;
    NS_IF_ADDREF (m_proxy);
}

nsImapMessageSinkProxyEvent::~nsImapMessageSinkProxyEvent()
{
    NS_IF_RELEASE (m_proxy);
}

SetupMsgWriteStreamProxyEvent::SetupMsgWriteStreamProxyEvent(
    nsImapMessageSinkProxy* aImapMessageSinkProxy,
    StreamInfo* aStreamInfo) :
    nsImapMessageSinkProxyEvent(aImapMessageSinkProxy)
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
    nsresult res = m_proxy->m_realImapMessageSink->SetupMsgWriteStream(
        m_proxy->m_protocol, &m_streamInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ParseAdoptedMsgLineProxyEvent::ParseAdoptedMsgLineProxyEvent(
    nsImapMessageSinkProxy* aImapMessageSinkProxy,
    msg_line_info* aMsgLineInfo) :
    nsImapMessageSinkProxyEvent(aImapMessageSinkProxy)
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
    nsresult res = m_proxy->m_realImapMessageSink->ParseAdoptedMsgLine(
        m_proxy->m_protocol, &m_msgLineInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}
    
NormalEndMsgWriteStreamProxyEvent::NormalEndMsgWriteStreamProxyEvent(
    nsImapMessageSinkProxy* aImapMessageSinkProxy) :
    nsImapMessageSinkProxyEvent(aImapMessageSinkProxy)
{
}

NormalEndMsgWriteStreamProxyEvent::~NormalEndMsgWriteStreamProxyEvent()
{
}

NS_IMETHODIMP
NormalEndMsgWriteStreamProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessageSink->NormalEndMsgWriteStream(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}
    
AbortMsgWriteStreamProxyEvent::AbortMsgWriteStreamProxyEvent(
    nsImapMessageSinkProxy* aImapMessageSinkProxy) :
    nsImapMessageSinkProxyEvent(aImapMessageSinkProxy)
{
}

AbortMsgWriteStreamProxyEvent::~AbortMsgWriteStreamProxyEvent()
{
}

NS_IMETHODIMP
AbortMsgWriteStreamProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessageSink->AbortMsgWriteStream(
        m_proxy->m_protocol);
	// IMAP thread is NOT waiting for FEEvent Completion.
//    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

OnlineCopyReportProxyEvent::OnlineCopyReportProxyEvent(
    nsImapMessageSinkProxy* aImapMessageSinkProxy,
    ImapOnlineCopyState* aCopyState) :
    nsImapMessageSinkProxyEvent(aImapMessageSinkProxy)
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
    nsresult res = m_proxy->m_realImapMessageSink->OnlineCopyReport(
        m_proxy->m_protocol, &m_copyState); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

BeginMessageUploadProxyEvent::BeginMessageUploadProxyEvent(
    nsImapMessageSinkProxy* aImapMessageSinkProxy) :
    nsImapMessageSinkProxyEvent(aImapMessageSinkProxy)
{
}

BeginMessageUploadProxyEvent::~BeginMessageUploadProxyEvent()
{
}

NS_IMETHODIMP
BeginMessageUploadProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMessageSink->BeginMessageUpload(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

UploadMessageFileProxyEvent::UploadMessageFileProxyEvent(
    nsImapMessageSinkProxy* aImapMessageSinkProxy,
    UploadMessageInfo* aMsgInfo) :
    nsImapMessageSinkProxyEvent(aImapMessageSinkProxy)
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
    nsresult res = m_proxy->m_realImapMessageSink->UploadMessageFile(
        m_proxy->m_protocol, &m_msgInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

NotifyMessageFlagsProxyEvent::NotifyMessageFlagsProxyEvent(
    nsImapMessageSinkProxy* aImapMessageSinkProxy,
    FlagsKeyStruct* aKeyStruct) :
    nsImapMessageSinkProxyEvent(aImapMessageSinkProxy)
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
    nsresult res = m_proxy->m_realImapMessageSink->NotifyMessageFlags(
        m_proxy->m_protocol, &m_keyStruct); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

NotifyMessageDeletedProxyEvent::NotifyMessageDeletedProxyEvent(
    nsImapMessageSinkProxy* aImapMessageSinkProxy,
    delete_message_struct* aStruct) :
    nsImapMessageSinkProxyEvent(aImapMessageSinkProxy)
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
    nsresult res = m_proxy->m_realImapMessageSink->NotifyMessageDeleted(
        m_proxy->m_protocol, &m_deleteMessageStruct); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetMessageSizeFromDBProxyEvent::GetMessageSizeFromDBProxyEvent(
    nsImapMessageSinkProxy* aImapMessageSinkProxy,
    MessageSizeInfo* sizeInfo) :
    nsImapMessageSinkProxyEvent(aImapMessageSinkProxy)
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
    nsresult res = m_proxy->m_realImapMessageSink->GetMessageSizeFromDB(
        m_proxy->m_protocol, m_sizeInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

nsImapExtensionSinkProxyEvent::nsImapExtensionSinkProxyEvent(
    nsImapExtensionSinkProxy* aProxy)
{
    NS_ASSERTION (aProxy, "fatal a null imap extension proxy");
    m_proxy = aProxy;
    NS_IF_ADDREF (m_proxy);
}

nsImapExtensionSinkProxyEvent::~nsImapExtensionSinkProxyEvent()
{
    NS_IF_RELEASE (m_proxy);
}

SetUserAuthenticatedProxyEvent::SetUserAuthenticatedProxyEvent(
    nsImapExtensionSinkProxy* aProxy, PRBool aBool) :
    nsImapExtensionSinkProxyEvent(aProxy)
{
    m_bool = aBool;
}

SetUserAuthenticatedProxyEvent::~SetUserAuthenticatedProxyEvent()
{
}

NS_IMETHODIMP
SetUserAuthenticatedProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapExtensionSink->SetUserAuthenticated(
        m_proxy->m_protocol, m_bool); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

SetMailServerUrlsProxyEvent::SetMailServerUrlsProxyEvent(
    nsImapExtensionSinkProxy* aProxy, const char* hostName) :
    nsImapExtensionSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapExtensionSink->SetMailServerUrls(
        m_proxy->m_protocol, m_hostName); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

SetMailAccountUrlProxyEvent::SetMailAccountUrlProxyEvent(
    nsImapExtensionSinkProxy* aProxy, const char* hostName) :
    nsImapExtensionSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapExtensionSink->SetMailAccountUrl(
        m_proxy->m_protocol, m_hostName); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ClearFolderRightsProxyEvent::ClearFolderRightsProxyEvent(
    nsImapExtensionSinkProxy* aProxy, nsIMAPACLRightsInfo* aclRights) :
    nsImapExtensionSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapExtensionSink->ClearFolderRights(
        m_proxy->m_protocol, &m_aclRightsInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

AddFolderRightsProxyEvent::AddFolderRightsProxyEvent(
    nsImapExtensionSinkProxy* aProxy, nsIMAPACLRightsInfo* aclRights) :
    nsImapExtensionSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapExtensionSink->AddFolderRights(
        m_proxy->m_protocol, &m_aclRightsInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

RefreshFolderRightsProxyEvent::RefreshFolderRightsProxyEvent(
    nsImapExtensionSinkProxy* aProxy, nsIMAPACLRightsInfo* aclRights) :
    nsImapExtensionSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapExtensionSink->RefreshFolderRights(
        m_proxy->m_protocol, &m_aclRightsInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

FolderNeedsACLInitializedProxyEvent::FolderNeedsACLInitializedProxyEvent(
    nsImapExtensionSinkProxy* aProxy, nsIMAPACLRightsInfo* aclRights) :
    nsImapExtensionSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapExtensionSink->FolderNeedsACLInitialized(
        m_proxy->m_protocol, &m_aclRightsInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

SetFolderAdminURLProxyEvent::SetFolderAdminURLProxyEvent(
    nsImapExtensionSinkProxy* aProxy, FolderQueryInfo* aInfo) :
    nsImapExtensionSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapExtensionSink->SetFolderAdminURL(
        m_proxy->m_protocol, &m_folderQueryInfo); 
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

nsImapMiscellaneousSinkProxyEvent::nsImapMiscellaneousSinkProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy)
{
    NS_ASSERTION (aProxy, "fatal: a null imap miscellaneous proxy");
    m_proxy = aProxy;
    NS_IF_ADDREF (m_proxy);
}

nsImapMiscellaneousSinkProxyEvent::~nsImapMiscellaneousSinkProxyEvent()
{
    NS_IF_RELEASE (m_proxy);
}

AddSearchResultProxyEvent::AddSearchResultProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, const char* searchHitLine) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->AddSearchResult(
        m_proxy->m_protocol, m_searchHitLine);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetArbitraryHeadersProxyEvent::GetArbitraryHeadersProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, GenericInfo* aInfo) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->GetArbitraryHeaders(
        m_proxy->m_protocol, m_info);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetShouldDownloadArbitraryHeadersProxyEvent::GetShouldDownloadArbitraryHeadersProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, GenericInfo* aInfo) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
        m_proxy->m_realImapMiscellaneousSink->GetShouldDownloadArbitraryHeaders(
            m_proxy->m_protocol, m_info);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetShowAttachmentsInlineProxyEvent::GetShowAttachmentsInlineProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, PRBool* aBool) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->GetShowAttachmentsInline(
        m_proxy->m_protocol, m_bool);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

HeaderFetchCompletedProxyEvent::HeaderFetchCompletedProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
{
}

HeaderFetchCompletedProxyEvent::~HeaderFetchCompletedProxyEvent()
{
}

NS_IMETHODIMP
HeaderFetchCompletedProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneousSink->HeaderFetchCompleted(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

FinishImapConnectionProxyEvent::FinishImapConnectionProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
{
}

FinishImapConnectionProxyEvent::~FinishImapConnectionProxyEvent()
{
}

NS_IMETHODIMP
FinishImapConnectionProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneousSink->FinishImapConnection(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

UpdateSecurityStatusProxyEvent::UpdateSecurityStatusProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
{
}

UpdateSecurityStatusProxyEvent::~UpdateSecurityStatusProxyEvent()
{
}

NS_IMETHODIMP
UpdateSecurityStatusProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneousSink->UpdateSecurityStatus(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

SetImapHostPasswordProxyEvent::SetImapHostPasswordProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, GenericInfo* aInfo) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->SetImapHostPassword(
        m_proxy->m_protocol, &m_info);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetPasswordForUserProxyEvent::GetPasswordForUserProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, const char* userName) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->GetPasswordForUser(
        m_proxy->m_protocol, m_userName);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}


SetBiffStateAndUpdateProxyEvent::SetBiffStateAndUpdateProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, nsMsgBiffState biffState) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
{
    m_biffState = biffState;
}

SetBiffStateAndUpdateProxyEvent::~SetBiffStateAndUpdateProxyEvent()
{
}

NS_IMETHODIMP
SetBiffStateAndUpdateProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneousSink->SetBiffStateAndUpdate(
        m_proxy->m_protocol, m_biffState);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

GetStoredUIDValidityProxyEvent::GetStoredUIDValidityProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, uid_validity_info* aInfo) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->GetStoredUIDValidity(
        m_proxy->m_protocol, &m_uidValidityInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

LiteSelectUIDValidityProxyEvent::LiteSelectUIDValidityProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, PRUint32 uidValidity) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
{
    m_uidValidity = uidValidity;
}

LiteSelectUIDValidityProxyEvent::~LiteSelectUIDValidityProxyEvent()
{
}

NS_IMETHODIMP
LiteSelectUIDValidityProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneousSink->LiteSelectUIDValidity(
        m_proxy->m_protocol, m_uidValidity);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

FEAlertProxyEvent::FEAlertProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, const char* alertString) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->FEAlert(
        m_proxy->m_protocol, m_alertString);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

FEAlertFromServerProxyEvent::FEAlertFromServerProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, const char* alertString) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->FEAlertFromServer(
        m_proxy->m_protocol, m_alertString);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ProgressStatusProxyEvent::ProgressStatusProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, const char* statusMsg) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->ProgressStatus(
        m_proxy->m_protocol, m_statusMsg);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

PercentProgressProxyEvent::PercentProgressProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, ProgressInfo* aInfo) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->PercentProgress(
        m_proxy->m_protocol, &m_progressInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

PastPasswordCheckProxyEvent::PastPasswordCheckProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
{
}

PastPasswordCheckProxyEvent::~PastPasswordCheckProxyEvent()
{
}

NS_IMETHODIMP
PastPasswordCheckProxyEvent::HandleEvent()
{
    nsresult res = m_proxy->m_realImapMiscellaneousSink->PastPasswordCheck(
        m_proxy->m_protocol);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

CommitNamespacesProxyEvent::CommitNamespacesProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, const char* hostName) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->CommitNamespaces(
        m_proxy->m_protocol, m_hostName);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

CommitCapabilityForHostProxyEvent::CommitCapabilityForHostProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, const char* hostName) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->CommitCapabilityForHost(
        m_proxy->m_protocol, m_hostName);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

TunnelOutStreamProxyEvent::TunnelOutStreamProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, msg_line_info* aInfo) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->TunnelOutStream(
        m_proxy->m_protocol, &m_msgLineInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}

ProcessTunnelProxyEvent::ProcessTunnelProxyEvent(
    nsImapMiscellaneousSinkProxy* aProxy, TunnelInfo *aInfo) :
    nsImapMiscellaneousSinkProxyEvent(aProxy)
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
    nsresult res = m_proxy->m_realImapMiscellaneousSink->ProcessTunnel(
        m_proxy->m_protocol, &m_tunnelInfo);
    m_proxy->m_protocol->NotifyFEEventCompletion();
    return res;
}
