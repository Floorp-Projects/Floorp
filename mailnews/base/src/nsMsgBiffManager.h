/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsIIncomingServerListener.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

typedef struct {
	nsCOMPtr<nsIMsgIncomingServer> server;
	nsTime nextBiffTime;
} nsBiffEntry;


class nsMsgBiffManager
	: public nsIMsgBiffManager,
		public nsIIncomingServerListener,
		public nsIObserver,
		public nsSupportsWeakReference
{
public:
	nsMsgBiffManager(); 
	virtual ~nsMsgBiffManager();

	NS_DECL_ISUPPORTS
    NS_DECL_NSIMSGBIFFMANAGER
	NS_DECL_NSIINCOMINGSERVERLISTENER
	NS_DECL_NSIOBSERVER

	nsresult Init();
	nsresult PerformBiff();

protected:
	PRInt32 FindServer(nsIMsgIncomingServer *server);
	nsresult SetNextBiffTime(nsBiffEntry *biffEntry, nsTime startTime);
	nsresult SetupNextBiff();
	nsresult AddBiffEntry(nsBiffEntry *biffEntry);

protected:
	nsCOMPtr<nsITimer> mBiffTimer;
	nsVoidArray *mBiffArray;
	PRBool mHaveShutdown;
};



#endif

