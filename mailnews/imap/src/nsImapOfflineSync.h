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

#ifndef _nsImapOfflineSync_H_
#define _nsImapOfflineSync_H_


#include "nsIMsgDatabase.h"
#include "nsIUrlListener.h"
#include "nsIMsgOfflineImapOperation.h"
#include "nsIMsgWindow.h"
#include "nsIMsgFolder.h"
#include "nsIFileSpec.h"

class nsImapOfflineSync : public nsIUrlListener, public nsIMsgCopyServiceListener {
public:												// set to one folder to playback one folder only
	nsImapOfflineSync(nsIMsgWindow *window, nsIUrlListener *listener, nsIMsgFolder *singleFolderOnly = nsnull);
	virtual ~nsImapOfflineSync();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLLISTENER
  NS_DECL_NSIMSGCOPYSERVICELISTENER
	virtual nsresult		ProcessNextOperation(); // this kicks off playback
	
	PRInt32		GetCurrentUIDValidity();
	void		SetCurrentUIDValidity(PRInt32 uidvalidity) { mCurrentUIDValidity = uidvalidity; }
	
	void		SetPseudoOffline(PRBool pseudoOffline) {m_pseudoOffline = pseudoOffline;}
	PRBool		ProcessingStaleFolderUpdate() { return m_singleFolderToUpdate != nsnull; }

	PRBool		CreateOfflineFolder(nsIMsgFolder *folder);
  void      SetWindow(nsIMsgWindow *window);
protected:
	PRBool		CreateOfflineFolders();
  nsresult  AdvanceToNextServer();
	nsresult  AdvanceToNextFolder();
	void		AdvanceToFirstIMAPFolder();
	void 		DeleteAllOfflineOpsForCurrentDB();
	
	void		ProcessFlagOperation(nsIMsgOfflineImapOperation *currentOp);
	void		ProcessMoveOperation(nsIMsgOfflineImapOperation *currentOp);
	void		ProcessCopyOperation(nsIMsgOfflineImapOperation *currentOp);
	void		ProcessEmptyTrash(nsIMsgOfflineImapOperation *currentOp);
	void		ProcessAppendMsgOperation(nsIMsgOfflineImapOperation *currentOp,
										  nsOfflineImapOperationType opType);
	
	nsCOMPtr <nsIMsgFolder>	m_currentFolder;
	nsCOMPtr <nsIMsgFolder> m_singleFolderToUpdate;
  nsCOMPtr <nsIMsgWindow> m_window;
  nsCOMPtr <nsISupportsArray> m_allServers;
  nsCOMPtr <nsISupportsArray> m_allFolders;
  nsCOMPtr <nsIMsgIncomingServer> m_currentServer;
  nsCOMPtr <nsIEnumerator> m_serverEnumerator;
  nsCOMPtr <nsIFileSpec> m_curTempFile;

	nsMsgKeyArray				m_CurrentKeys;
	PRUint32					m_KeyIndex;
	nsCOMPtr <nsIMsgDatabase>				m_currentDB;
  nsCOMPtr <nsIUrlListener> m_listener;
	PRInt32				mCurrentUIDValidity;
	PRInt32				mCurrentPlaybackOpType;	// kFlagsChanged -> kMsgCopy -> kMsgMoved
	PRBool				m_mailboxupdatesStarted;
  PRBool        m_mailboxupdatesFinished;
	PRBool				m_pseudoOffline;		// for queueing online events in offline db
	PRBool				m_createdOfflineFolders;

};

class nsImapOfflineDownloader : public nsImapOfflineSync
{
public:
  nsImapOfflineDownloader(nsIMsgWindow *window, nsIUrlListener *listener);
  virtual ~nsImapOfflineDownloader();
	virtual nsresult		ProcessNextOperation(); // this kicks off download
};

#endif
