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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsMsgDBFolder_h__
#define nsMsgDBFolder_h__

#include "msgCore.h"
#include "nsMsgFolder.h" 
#include "nsIDBFolderInfo.h"
#include "nsIMsgDatabase.h"
#include "nsIMessage.h"
#include "nsCOMPtr.h"
#include "nsIDBChangeListener.h"
#include "nsIUrlListener.h"

class nsIMsgFolderCacheElement;
 /* 
  * nsMsgDBFolder
  * class derived from nsMsgFolder for those folders that use an nsIMsgDatabase
  */ 

class NS_MSG_BASE nsMsgDBFolder: public nsMsgFolder, public nsIDBChangeListener, public nsIUrlListener
{
public: 
	nsMsgDBFolder(void);
	virtual ~nsMsgDBFolder(void);
  NS_DECL_NSIDBCHANGELISTENER
  
	NS_IMETHOD GetThreads(nsISimpleEnumerator** threadEnumerator);
	NS_IMETHOD GetThreadForMessage(nsIMessage *message, nsIMsgThread **thread);
	NS_IMETHOD HasMessage(nsIMessage *message, PRBool *hasMessage);
	NS_IMETHOD GetCharset(PRUnichar * *aCharset);
	NS_IMETHOD SetCharset(PRUnichar * aCharset);

  NS_IMETHOD GetMsgDatabase(nsIMsgDatabase** aMsgDatabase);

	NS_DECL_ISUPPORTS_INHERITED

	NS_DECL_NSIURLLISTENER

	NS_IMETHOD WriteToFolderCache(nsIMsgFolderCache *folderCache);
	NS_IMETHOD WriteToFolderCacheElem(nsIMsgFolderCacheElement *element);
	NS_IMETHOD ManyHeadersToDownload(PRBool *_retval);

	NS_IMETHOD MarkAllMessagesRead(void);

protected:
	virtual nsresult ReadDBFolderInfo(PRBool force);
	virtual nsresult GetDatabase() = 0;
	virtual nsresult SendFlagNotifications(nsISupports *item, PRUint32 oldFlags, PRUint32 newFlags);
	nsresult ReadFromFolderCache(nsIMsgFolderCacheElement *element);

protected:
	nsCOMPtr<nsIMsgDatabase> mDatabase;  
	nsString mCharset;


};

#endif
