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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef NSMSGBIFFMANAGER_H
#define NSMSGBIFFMANAGER_H

#include "msgCore.h"
#include "nsIMsgBiffManager.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsVoidArray.h"
#include "nsTime.h"
#include "nsCOMPtr.h"

typedef struct {
	nsCOMPtr<nsIMsgIncomingServer> server;
	nsTime nextBiffTime;
} nsBiffEntry;


class nsMsgBiffManager: public nsIMsgBiffManager, nsITimerCallback
{
public:
	nsMsgBiffManager(); 
	virtual ~nsMsgBiffManager();

	NS_DECL_ISUPPORTS
    NS_DECL_NSIMSGBIFFMANAGER

	//nsITimerCallback implementation
	virtual void Notify(nsITimer *timer);

protected:
	PRInt32 FindServer(nsIMsgIncomingServer *server);
	nsresult SetNextBiffTime(nsBiffEntry *biffEntry, nsTime startTime);
	nsresult SetupNextBiff();
	nsresult AddBiffEntry(nsBiffEntry *biffEntry);
	nsresult PerformBiff();

protected:
	nsITimer *mBiffTimer;
	nsVoidArray *mBiffArray;
};

NS_BEGIN_EXTERN_C

nsresult
NS_NewMsgBiffManager(const nsIID& iid, void **result);

NS_END_EXTERN_C

#endif

