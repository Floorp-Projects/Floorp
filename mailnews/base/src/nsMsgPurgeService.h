/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Navin Gupta <naving@netscape.com> (Original Author)
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NSMSGPURGESERVICE_H
#define NSMSGPURGESERVICE_H

#include "msgCore.h"
#include "nsIMsgPurgeService.h"
#include "nsIMsgSearchSession.h"
#include "nsITimer.h"
#include "nsVoidArray.h"
#include "nsTime.h"
#include "nsCOMPtr.h"
#include "nsIMsgSearchNotify.h"
#include "nsIMsgFolder.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"

class nsMsgPurgeService
	: public nsIMsgPurgeService,
		public nsIMsgSearchNotify
{
public:
	nsMsgPurgeService(); 
	virtual ~nsMsgPurgeService();

	NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGPURGESERVICE
	NS_DECL_NSIMSGSEARCHNOTIFY

	nsresult PerformPurge();

protected:
  PRInt32 FindServer(nsIMsgIncomingServer *server);
  nsresult SetupNextPurge();
  nsresult PurgeSurver(nsIMsgIncomingServer *server);
  nsresult SearchFolderToPurge(nsIMsgFolder *folder, PRInt32 purgeInterval);

protected:
  nsCOMPtr<nsITimer> mPurgeTimer;
  nsCOMPtr<nsIMsgSearchSession> mSearchSession;
  nsCOMPtr<nsIMsgFolder> mSearchFolder;
  nsCOMPtr<nsISupportsArray> mHdrsToDelete;
  nsVoidArray mPurgeArray;
  PRBool mHaveShutdown;

private:
  PRInt32 mMinDelayBetweenPurges;  // in minutes, how long must pass between two consecutive purges on the same junk folder?
  PRInt32 mPurgeTimerInterval;  // in minutes, how often to check if we need to purge one of the junk folders?
};



#endif

