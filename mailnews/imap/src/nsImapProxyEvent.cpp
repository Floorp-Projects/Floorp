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
#include "nsImapProxyEvent.h"
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

nsImapLogProxy::nsImapLogProxy(nsIImapLog* aImapLog, 
															 PLEventQueue* aEventQ,
															 PRThread* aThread)
{
		NS_PRECONDITION(aImapLog && aEventQ, 
										"nsImapLogProxy: invalid aImapLog or aEventQ");
		NS_INIT_REFCNT();

		m_realImapLog = aImapLog;
		NS_ADDREF(m_realImapLog);
		
		m_eventQueue = aEventQ;
		m_thread = aThread;
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
				nsImapLogProxyEvent *ev = new nsImapLogProxyEvent(this, aLogData);
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
		}
		return res;
}

nsIImapLog*
ns_NewImapLogProxy(nsIImapLog* aImapLog,
									 PLEventQueue* aEventQ,
									 PRThread* aThread)
{
		return new nsImapLogProxy(aImapLog, aEventQ, aThread);
}

nsImapLogProxyEvent::nsImapLogProxyEvent(nsImapLogProxy* aProxy,
																				 const char* aLogData)
{
		NS_PRECONDITION(aProxy && aLogData, 
										"nsImapLogProxyEvent: invalid aProxy or aLogData");
		m_logData = PL_strdup(aLogData);
		m_proxy = aProxy;
		NS_ADDREF(m_proxy);
}

nsImapLogProxyEvent::~nsImapLogProxyEvent()
{
		PR_Free(m_logData);
		NS_RELEASE(m_proxy);
}

NS_IMETHODIMP
nsImapLogProxyEvent::HandleEvent()
{
		return m_proxy->m_realImapLog->HandleImapLogData(m_logData);
}

