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
#include "nsCOMPtr.h"
#include "nsIDBChangeListener.h"
#include "nsIUrlListener.h"
#include "nsIMsgHdr.h"
#include "nsIOutputStream.h"
#include "nsITransport.h"
#include "nsIMsgStringService.h"
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
  
  NS_IMETHOD StartFolderLoading(void);
  NS_IMETHOD EndFolderLoading(void);
  NS_IMETHOD GetCharset(PRUnichar * *aCharset);
  NS_IMETHOD SetCharset(const PRUnichar * aCharset);
  NS_IMETHOD GetCharsetOverride(PRBool *aCharsetOverride);
  NS_IMETHOD SetCharsetOverride(PRBool aCharsetOverride);
  NS_IMETHOD GetFirstNewMessage(nsIMsgDBHdr **firstNewMessage);
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

  NS_IMETHOD AddMessageDispositionState(nsIMsgDBHdr *aMessage, nsMsgDispositionState aDispositionFlag);
  NS_IMETHOD MarkAllMessagesRead(void);
  NS_IMETHOD MarkThreadRead(nsIMsgThread *thread);
  NS_IMETHOD SetFlag(PRUint32 flag);


  NS_IMETHOD Shutdown(PRBool shutdownChildren);
  NS_IMETHOD ForceDBClosed();
  NS_IMETHOD GetHasNewMessages(PRBool *hasNewMessages);
  NS_IMETHOD SetHasNewMessages(PRBool hasNewMessages);
  NS_IMETHOD GetGettingNewMessages(PRBool *gettingNewMessages);
  NS_IMETHOD SetGettingNewMessages(PRBool gettingNewMessages);

  NS_IMETHOD GetSupportsOffline(PRBool *aSupportsOffline);
  NS_IMETHOD ShouldStoreMsgOffline(nsMsgKey msgKey, PRBool *result);
  NS_IMETHOD GetOfflineFileTransport(nsMsgKey msgKey, PRUint32 *offset, PRUint32 *size, nsITransport **_retval);
  NS_IMETHOD HasMsgOffline(nsMsgKey msgKey, PRBool *result);
  NS_IMETHOD DownloadMessagesForOffline(nsISupportsArray *messages, nsIMsgWindow *msgWindow);
  NS_IMETHOD DownloadAllForOffline(nsIUrlListener *listener, nsIMsgWindow *msgWindow);
  NS_IMETHOD GetRetentionSettings(nsIMsgRetentionSettings **settings);
  NS_IMETHOD SetRetentionSettings(nsIMsgRetentionSettings *settings);
  NS_IMETHOD GetDownloadSettings(nsIMsgDownloadSettings **settings);
  NS_IMETHOD SetDownloadSettings(nsIMsgDownloadSettings *settings);
  NS_IMETHOD CompactAllOfflineStores(nsIMsgWindow *msgWindow, nsISupportsArray *aOfflineFolderArray);
  NS_IMETHOD GetOfflineStoreOutputStream(nsIOutputStream **outputStream);
  NS_IMETHOD GetOfflineStoreInputStream(nsIInputStream **outputStream);
  NS_IMETHOD IsCommandEnabled(const char *command, PRBool *result);
  NS_IMETHOD MatchOrChangeFilterDestination(nsIMsgFolder *oldFolder, PRBool caseInsensitive, PRBool *changed);

protected:
  virtual nsresult ReadDBFolderInfo(PRBool force);
  virtual nsresult FlushToFolderCache();
  virtual nsresult GetDatabase(nsIMsgWindow *aMsgWindow) = 0;
  virtual nsresult SendFlagNotifications(nsISupports *item, PRUint32 oldFlags, PRUint32 newFlags);
  nsresult CheckWithNewMessagesStatus(PRBool messageAdded);
  nsresult OnKeyAddedOrDeleted(nsMsgKey aKeyChanged, nsMsgKey  aParentKey , PRInt32 aFlags, 
                  nsIDBChangeListener * aInstigator, PRBool added, PRBool doFlat, PRBool doThread);
  nsresult CreateFileSpecForDB(const char *userLeafName, nsFileSpec &baseDir, nsIFileSpec **dbFileSpec);

  nsresult GetFolderCacheKey(nsIFileSpec **aFileSpec);
  nsresult GetFolderCacheElemFromFileSpec(nsIFileSpec *fileSpec, nsIMsgFolderCacheElement **cacheElement);
  nsresult NotifyStoreClosedAllHeaders();

  // offline support methods.
  nsresult StartNewOfflineMessage();
  nsresult WriteStartOfNewLocalMessage();
  nsresult EndNewOfflineMessage();
  nsresult CompactOfflineStore(nsIMsgWindow *inWindow);
  nsresult AutoCompact(nsIMsgWindow *aWindow);
  // this is a helper routine that ignores whether MSG_FLAG_OFFLINE is set for the folder
  nsresult MsgFitsDownloadCriteria(nsMsgKey msgKey, PRBool *result);
  nsresult GetPromptPurgeThreshold(PRBool *aPrompt);
  nsresult GetPurgeThreshold(PRInt32 *aThreshold);
protected:
  nsCOMPtr<nsIMsgDatabase> mDatabase;
  nsString mCharset;
  PRBool mCharsetOverride;
  PRBool mAddListener;
  PRBool mNewMessages;
  PRBool mGettingNewMessages;

  nsCOMPtr <nsIMsgDBHdr> m_offlineHeader;
  PRInt32 m_numOfflineMsgLines;
	// this is currently used when we do a save as of an imap or news message..
  nsCOMPtr<nsIOutputStream> m_tempMessageStream;

  nsCOMPtr <nsIMsgRetentionSettings> m_retentionSettings;
  nsCOMPtr <nsIMsgDownloadSettings> m_downloadSettings;
  static nsIAtom* mFolderLoadedAtom;
  static nsIAtom* mDeleteOrMoveMsgCompletedAtom;
  static nsIAtom* mDeleteOrMoveMsgFailedAtom;
  static nsrefcnt mInstanceCount;
};

#endif
