/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/********************************************************************************************************
 
   Interface for representing Local Mail folders.
 
*********************************************************************************************************/

#ifndef nsMsgLocalMailFolder_h__
#define nsMsgLocalMailFolder_h__

#include "nsMsgDBFolder.h" /* include the interface we are going to support */
#include "nsFileSpec.h"
#include "nsICopyMessageListener.h"
#include "nsFileStream.h"
#include "nsIFileStreams.h"
#include "nsIPop3IncomingServer.h"  // need this for an interface ID
#include "nsMsgTxn.h"
#include "nsIMsgMessageService.h"
#include "nsIMsgParseMailMsgState.h"
#include "nsITransactionManager.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgStringService.h"
#include "nsIMsgFilterPlugin.h"
#define COPY_BUFFER_SIZE 16384

struct nsLocalMailCopyState
{
  nsLocalMailCopyState();
  virtual ~nsLocalMailCopyState();
  
  nsOutputFileStream * m_fileStream;
  nsCOMPtr<nsISupports> m_srcSupport;
  nsCOMPtr<nsISupportsArray> m_messages;
  nsCOMPtr<nsMsgTxn> m_undoMsgTxn;
  nsCOMPtr<nsIMsgDBHdr> m_message; // current copy message
  nsCOMPtr<nsIMsgParseMailMsgState> m_parseMsgState;
  nsCOMPtr<nsIMsgCopyServiceListener> m_listener;
  nsCOMPtr<nsIMsgWindow> m_msgWindow;

  // for displaying status;
  nsCOMPtr <nsIMsgStatusFeedback> m_statusFeedback;
  nsCOMPtr <nsIStringBundle> m_stringBundle;
  PRInt64 m_lastProgressTime;

  nsMsgKey m_curDstKey;
  PRUint32 m_curCopyIndex;
  nsCOMPtr <nsIMsgMessageService> m_messageService;
  PRUint32 m_totalMsgCount;
  char *m_dataBuffer;
  PRUint32 m_dataBufferSize;
  PRUint32 m_leftOver;
  PRPackedBool m_isMove;
  PRPackedBool m_isFolder;   // isFolder move/copy
  PRPackedBool m_dummyEnvelopeNeeded;
  PRPackedBool m_copyingMultipleMessages;
  PRPackedBool m_fromLineSeen;
  PRPackedBool m_allowUndo;
  PRPackedBool m_writeFailed;
  PRPackedBool m_notifyFolderLoaded;
};

struct nsLocalFolderScanState
{
  nsLocalFolderScanState();
  ~nsLocalFolderScanState();

  nsFileSpec *m_fileSpec;
  nsCOMPtr<nsILocalFile> m_localFile;
  nsCOMPtr<nsIFileInputStream> m_fileStream;
  nsCOMPtr<nsIInputStream> m_inputStream;
  nsCOMPtr<nsISeekableStream> m_seekableStream;
  nsCOMPtr<nsILineInputStream> m_fileLineStream;
  nsCString m_header;
  nsCString m_accountKey;
  const char *m_uidl;	// memory is owned by m_header
};

class nsMsgLocalMailFolder : public nsMsgDBFolder,
                             public nsIMsgLocalMailFolder,
                             public nsICopyMessageListener,
                             public nsIJunkMailClassificationListener
{
public:
  nsMsgLocalMailFolder(void);
  virtual ~nsMsgLocalMailFolder(void);
  NS_DECL_NSICOPYMESSAGELISTENER
  NS_DECL_NSIMSGLOCALMAILFOLDER
  NS_DECL_NSIJUNKMAILCLASSIFICATIONLISTENER
  NS_DECL_ISUPPORTS_INHERITED
#if 0
  static nsresult GetRoot(nsIMsgFolder* *result);
#endif
  // nsIRDFResource methods:
  NS_IMETHOD Init(const char *aURI);
  
  // nsICollection methods:
  NS_IMETHOD Enumerate(nsIEnumerator* *result);

  // nsIUrlListener methods
  NS_IMETHOD OnStartRunningUrl(nsIURI * aUrl);
  NS_IMETHOD OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode);

  // nsIMsgFolder methods:
  NS_IMETHOD GetSubFolders(nsIEnumerator* *result);

  NS_IMETHOD GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result);
  NS_IMETHOD UpdateFolder(nsIMsgWindow *aWindow);

  NS_IMETHOD CreateSubfolder(const PRUnichar *folderName ,nsIMsgWindow *msgWindow);
  NS_IMETHOD AddSubfolder(const nsAString& folderName, nsIMsgFolder** newFolder);

  NS_IMETHOD Compact(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow);
  NS_IMETHOD CompactAll(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow, nsISupportsArray *aFolderArray, PRBool aCompactOfflineAlso, nsISupportsArray *aOfflineFolderArray);
  NS_IMETHOD EmptyTrash(nsIMsgWindow *msgWindow, nsIUrlListener *aListener);
  NS_IMETHOD Delete ();
  NS_IMETHOD DeleteSubFolders(nsISupportsArray *folders, nsIMsgWindow *msgWindow);
  NS_IMETHOD CreateStorageIfMissing(nsIUrlListener* urlListener);
  NS_IMETHOD Rename (const PRUnichar *aNewName, nsIMsgWindow *msgWindow);
  NS_IMETHOD RenameSubFolders (nsIMsgWindow *msgWindow, nsIMsgFolder *oldFolder);

  NS_IMETHOD GetPrettyName(PRUnichar** prettyName);	// Override of the base, for top-level mail folder
  NS_IMETHOD SetPrettyName(const PRUnichar *aName);

  NS_IMETHOD GetFolderURL(char **url);

  NS_IMETHOD UpdateSummaryTotals(PRBool force) ;

  NS_IMETHOD  GetManyHeadersToDownload(PRBool *retval);

  NS_IMETHOD GetDeletable (PRBool *deletable); 
  NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);
  NS_IMETHOD GetSizeOnDisk(PRUint32* size);

  NS_IMETHOD  GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db);

  NS_IMETHOD DeleteMessages(nsISupportsArray *messages, 
                      nsIMsgWindow *msgWindow, PRBool
                      deleteStorage, PRBool isMove,
                      nsIMsgCopyServiceListener* listener, PRBool allowUndo);
  NS_IMETHOD CopyMessages(nsIMsgFolder *srcFolder, nsISupportsArray* messages,
                          PRBool isMove, nsIMsgWindow *msgWindow,
                          nsIMsgCopyServiceListener* listener, PRBool isFolder, PRBool allowUndo);
  NS_IMETHOD CopyFolder(nsIMsgFolder *srcFolder, PRBool isMoveFolder, nsIMsgWindow *msgWindow,
                          nsIMsgCopyServiceListener* listener);
  NS_IMETHOD CopyFileMessage(nsIFileSpec* fileSpec, nsIMsgDBHdr* msgToReplace,
                             PRBool isDraftOrTemplate, 
                             nsIMsgWindow *msgWindow,
                             nsIMsgCopyServiceListener* listener);
  NS_IMETHOD GetNewMessages(nsIMsgWindow *aWindow, nsIUrlListener *aListener);
  NS_IMETHOD NotifyCompactCompleted();
  NS_IMETHOD Shutdown(PRBool shutdownChildren);

  NS_IMETHOD WriteToFolderCacheElem(nsIMsgFolderCacheElement *element);
  NS_IMETHOD ReadFromFolderCacheElem(nsIMsgFolderCacheElement *element);

  NS_IMETHOD GetName(PRUnichar **aName);

  // Used when headers_only is TRUE
  NS_IMETHOD DownloadMessagesForOffline(nsISupportsArray *aMessages, nsIMsgWindow *aWindow);


protected:
  nsresult CopyFolderAcrossServer(nsIMsgFolder *srcFolder, nsIMsgWindow *msgWindow,nsIMsgCopyServiceListener* listener);

  nsresult CreateSubFolders(nsFileSpec &path);
  nsresult GetDatabase(nsIMsgWindow *aMsgWindow);
  nsresult GetTrashFolder(nsIMsgFolder** trashFolder);
  nsresult WriteStartOfNewMessage();
  nsresult IsChildOfTrash(PRBool *result);
  nsresult RecursiveSetDeleteIsMoveTrash(PRBool bVal);
  nsresult ConfirmFolderDeletion(nsIMsgWindow *aMsgWindow, PRBool *aResult);

  nsresult DeleteMessage(nsISupports *message, nsIMsgWindow *msgWindow,
                   PRBool deleteStorage, PRBool commit);

  // copy message helper
  nsresult DisplayMoveCopyStatusMsg();
  nsresult SortMessagesBasedOnKey(nsISupportsArray *messages, nsMsgKeyArray *aKeyArray, nsIMsgFolder *srcFolder);

  nsresult CopyMessageTo(nsISupports *message, nsIMsgFolder *dstFolder,
                         nsIMsgWindow *msgWindow, PRBool isMove);

  // copy multiple messages at a time from this folder
  nsresult CopyMessagesTo(nsISupportsArray *messages, nsIMsgWindow *aMsgWindow,
                                       nsIMsgFolder *dstFolder,
                                       PRBool isMove);
  virtual const char* GetIncomingServerType();
  nsresult InitCopyState(nsISupports* aSupport, nsISupportsArray* messages,
                         PRBool isMove, nsIMsgCopyServiceListener* listener, nsIMsgWindow *msgWindow, PRBool isMoveFolder, PRBool allowUndo);
  void CopyPropertiesToMsgHdr(nsIMsgDBHdr *destHdr, nsIMsgDBHdr *srcHdr);
  virtual nsresult CreateBaseMessageURI(const char *aURI);
  virtual nsresult SpamFilterClassifyMessage(const char *aURI, nsIMsgWindow *aMsgWindow, nsIJunkMailPlugin *aJunkMailPlugin);
  virtual nsresult SpamFilterClassifyMessages(const char **aURIArray, PRUint32 aURICount, nsIMsgWindow *aMsgWindow, nsIJunkMailPlugin *aJunkMailPlugin);
protected:
  nsLocalMailCopyState *mCopyState; //We only allow one of these at a time
  const char *mType;
  PRPackedBool mHaveReadNameFromDB;
  PRPackedBool mInitialized;
  PRPackedBool mCheckForNewMessagesAfterParsing;
  PRPackedBool m_parsingFolder;
  nsCOMPtr<nsIMsgStringService> mMsgStringService;
  PRInt32 mNumFilterClassifyRequests;
  nsMsgKeyArray mSpamKeysToMove;
  nsCString mSpamFolderURI;
  nsresult setSubfolderFlag(const PRUnichar *aFolderName, PRUint32 flags);


  // state variables for DownloadMessagesForOffline

  // Do we notify the owning window of Delete's before or after
  // Adding the new msg?
#define DOWNLOAD_NOTIFY_FIRST 1
#define DOWNLOAD_NOTIFY_LAST  2

#ifndef DOWNLOAD_NOTIFY_STYLE
#define DOWNLOAD_NOTIFY_STYLE	DOWNLOAD_NOTIFY_FIRST
#endif

  nsCOMPtr<nsISupportsArray> mDownloadMessages;
  nsCOMPtr<nsIMsgWindow> mDownloadWindow;
  nsMsgKey mDownloadSelectKey;
  PRUint32 mDownloadState;
#define DOWNLOAD_STATE_NONE	0
#define DOWNLOAD_STATE_INITED	1
#define DOWNLOAD_STATE_GOTMSG	2
#define DOWNLOAD_STATE_DIDSEL	3

#if DOWNLOAD_NOTIFY_STYLE == DOWNLOAD_NOTIFY_LAST
  nsMsgKey mDownloadOldKey;
  nsMsgKey mDownloadOldParent;
  PRUint32 mDownloadOldFlags;
#endif
};

#endif // nsMsgLocalMailFolder_h__
