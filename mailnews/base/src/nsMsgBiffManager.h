/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

