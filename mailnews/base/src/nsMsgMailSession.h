/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsMsgMailSession_h___
#define nsMsgMailSession_h___

#include "nsIMsgMailSession.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIMsgStatusFeedback.h"

///////////////////////////////////////////////////////////////////////////////////
// The mail session is a replacement for the old 4.x MSG_Master object. It contains
// mail session generic information such as the user's current mail identity, ....
// I'm starting this off as an empty interface and as people feel they need to
// add more information to it, they can. I think this is a better approach than 
// trying to port over the old MSG_Master in its entirety as that had a lot of 
// cruft in it....
//////////////////////////////////////////////////////////////////////////////////

#include "nsIMsgAccountManager.h"

class nsIMsgFolderCache;

class nsMsgMailSession : public nsIMsgMailSession
{
public:
	nsMsgMailSession();
	virtual ~nsMsgMailSession();

	NS_DECL_ISUPPORTS
	
	// nsIMsgMailSession support
	NS_IMETHOD GetCurrentIdentity(nsIMsgIdentity ** aIdentity);
    NS_IMETHOD GetCurrentServer(nsIMsgIncomingServer **aServer);
    NS_IMETHOD GetAccountManager(nsIMsgAccountManager* *aAM);

	NS_IMETHOD AddFolderListener(nsIFolderListener *listener);
	NS_IMETHOD RemoveFolderListener(nsIFolderListener *listener);
	NS_IMETHOD NotifyFolderItemPropertyChanged(nsISupports *item, const char *property, const char* oldValue, const char* newValue);
	NS_IMETHOD NotifyFolderItemPropertyFlagChanged(nsISupports *item, const char *property, PRUint32 oldValue,
												   PRUint32 newValue);
	NS_IMETHOD NotifyFolderItemAdded(nsIFolder *folder, nsISupports *item);
	NS_IMETHOD NotifyFolderItemDeleted(nsIFolder *folder, nsISupports *item);

	NS_IMETHOD GetFolderCache(nsIMsgFolderCache **aFolderCache);

	NS_IMETHOD SetTemporaryMsgStatusFeedback(nsIMsgStatusFeedback *aMsgStatusFeedback);
	NS_IMETHOD GetTemporaryMsgStatusFeedback(nsIMsgStatusFeedback **aMsgStatusFeedback);
	nsresult Init();
protected:
  nsIMsgAccountManager *m_accountManager;
  nsIMsgFolderCache		*m_msgFolderCache;
	nsVoidArray *mListeners; 
	// stick this here temporarily
	nsCOMPtr <nsIMsgStatusFeedback> m_temporaryMsgStatusFeedback;

};

NS_BEGIN_EXTERN_C

nsresult
NS_NewMsgMailSession(const nsIID& iid, void **result);

NS_END_EXTERN_C

#endif /* nsMsgMailSession_h__ */
