/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

class NS_MSG_BASE nsMsgDBFolder:  public nsMsgFolder,
                                        public nsIDBChangeListener,
                                        public nsIUrlListener
{
public: 
	nsMsgDBFolder(void);
	virtual ~nsMsgDBFolder(void);
  NS_DECL_NSIDBCHANGELISTENER
  
	NS_IMETHOD  StartFolderLoading(void);
	NS_IMETHOD  EndFolderLoading(void);
	NS_IMETHOD GetThreads(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator** threadEnumerator);
	NS_IMETHOD GetThreadForMessage(nsIMessage *message, nsIMsgThread **thread);
	NS_IMETHOD HasMessage(nsIMessage *message, PRBool *hasMessage);
	NS_IMETHOD GetCharset(PRUnichar * *aCharset);
	NS_IMETHOD SetCharset(const PRUnichar * aCharset);
	//NS_IMETHOD HasNewMessages(PRBool *hasNewMessages);
	NS_IMETHOD GetFirstNewMessage(nsIMessage **firstNewMessage);
	NS_IMETHOD ClearNewMessages();
  NS_IMETHOD GetFlags(PRUint32 *aFlags);
  NS_IMETHOD GetExpungedBytes(PRUint32 *count);

  NS_IMETHOD GetMsgDatabase(nsIMsgWindow *aMsgWindow,
                            nsIMsgDatabase** aMsgDatabase);

	NS_DECL_ISUPPORTS_INHERITED

	NS_DECL_NSIURLLISTENER

	NS_IMETHOD WriteToFolderCache(nsIMsgFolderCache *folderCache, PRBool deep);
	NS_IMETHOD WriteToFolderCacheElem(nsIMsgFolderCacheElement *element);
	NS_IMETHOD ReadFromFolderCacheElem(nsIMsgFolderCacheElement *element);
	NS_IMETHOD ManyHeadersToDownload(PRBool *_retval);

	NS_IMETHOD MarkAllMessagesRead(void);
    NS_IMETHOD MarkThreadRead(nsIMsgThread *thread);
  NS_IMETHOD SetFlag(PRUint32 flag);


	NS_IMETHOD Shutdown(PRBool shutdownChildren);
  NS_IMETHOD ForceDBClosed();
	NS_IMETHOD GetHasNewMessages(PRBool *hasNewMessages);
	NS_IMETHOD SetHasNewMessages(PRBool hasNewMessages);
  NS_IMETHOD GetGettingNewMessages(PRBool *gettingNewMessages);
  NS_IMETHOD SetGettingNewMessages(PRBool gettingNewMessages);

protected:
	virtual nsresult ReadDBFolderInfo(PRBool force);
  virtual nsresult FlushToFolderCache();
	virtual nsresult GetDatabase(nsIMsgWindow *aMsgWindow) = 0;
	virtual nsresult SendFlagNotifications(nsISupports *item, PRUint32 oldFlags, PRUint32 newFlags);
	nsresult CheckWithNewMessagesStatus(PRBool messageAdded);
	nsresult OnKeyAddedOrDeleted(nsMsgKey aKeyChanged, nsMsgKey  aParentKey , PRInt32 aFlags, 
                        nsIDBChangeListener * aInstigator, PRBool added, PRBool doFlat, PRBool doThread);
	nsresult CreatePlatformLeafNameForDisk(const char *userLeafName, nsFileSpec &baseDir, char **resultName);

	nsresult GetFolderCacheKey(nsIFileSpec **aFileSpec);
	nsresult GetFolderCacheElemFromFileSpec(nsIFileSpec *fileSpec, nsIMsgFolderCacheElement **cacheElement);
protected:
	nsCOMPtr<nsIMsgDatabase> mDatabase;
	nsString mCharset;
	PRBool mAddListener;
	PRBool mNewMessages;
  PRBool mGettingNewMessages;

  static nsIAtom* mFolderLoadedAtom;
  static nsIAtom* mDeleteOrMoveMsgCompletedAtom;

private:
  static nsrefcnt mInstanceCount;
};

#endif
