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

class nsImapLogProxy : public nsIImapLog
{
public:
	nsImapLogProxy(nsIImapLog* aImapLog, 
								 PLEventQueue* aEventQ,
								 PRThread* aThread);
	virtual ~nsImapLogProxy();

	NS_DECL_ISUPPORTS

	NS_IMETHOD HandleImapLogData(const char* aLogData);
	nsIImapLog* m_realImapLog;

private:
	PLEventQueue* m_eventQueue;
	PRThread* m_thread;
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
	nsImapLogProxyEvent(nsImapLogProxy* aProxy, const char* aLogData);
	virtual ~nsImapLogProxyEvent();

	NS_IMETHOD HandleEvent();
	char *m_logData;
	nsImapLogProxy *m_proxy;
};

nsIImapLog* ns_NewImapLogProxy(nsIImapLog* aImapLog, 
															 PLEventQueue* aEventQ,
															 PRThread* aThread);
#endif
