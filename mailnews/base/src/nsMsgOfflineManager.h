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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMsgOfflineManager_h__
#define nsMsgOfflineManager_h__

#include "nscore.h"
#include "nsIMsgOfflineManager.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIUrlListener.h"
#include "nsIMsgWindow.h"
#include "nsIMsgSendLaterListener.h"
#include "nsIStringBundle.h"

class nsMsgOfflineManager
	: public nsIMsgOfflineManager,
      public nsIObserver,
      public nsSupportsWeakReference,
      public nsIMsgSendLaterListener,
    public nsIUrlListener
{
public:

  nsMsgOfflineManager();
  virtual ~nsMsgOfflineManager();
  
  NS_DECL_ISUPPORTS
 
  /* nsIMsgOfflineManager methods */
  
  NS_DECL_NSIMSGOFFLINEMANAGER
  NS_DECL_NSIOBSERVER  
  NS_DECL_NSIURLLISTENER
  NS_DECL_NSIMSGSENDLATERLISTENER

  typedef enum 
  {
    eStarting = 0,
	  eSynchronizingOfflineImapChanges = 1,
    eDownloadingNews = 2,
    eDownloadingMail = 3,
	  eSendingUnsent = 4,
    eDone = 5,
    eNoState = 6  // we're not doing anything
  } offlineManagerState;

  typedef enum 
  {
    eGoingOnline = 0,
    eDownloadingForOffline = 1,
    eNoOp = 2 // no operation in progress
  } offlineManagerOperation;

private:
  nsresult AdvanceToNextState(nsresult exitStatus);
  nsresult SynchronizeOfflineImapChanges();
  nsresult StopRunning(nsresult exitStatus);
  nsresult SendUnsentMessages();
  nsresult DownloadOfflineNewsgroups();
  nsresult DownloadMail();

  nsresult SetOnlineState(PRBool online);
  nsresult ShowStatus(const char *statusMsgName);

  PRBool m_inProgress;
  PRBool m_sendUnsentMessages;
  PRBool m_downloadNews;
  PRBool m_downloadMail;
  PRBool m_playbackOfflineImapOps;
  PRBool m_goOfflineWhenDone;
  offlineManagerState m_curState;
  offlineManagerOperation m_curOperation;
  nsCOMPtr <nsIMsgWindow> m_window;
  nsCOMPtr <nsIMsgStatusFeedback> m_statusFeedback;
  nsCOMPtr<nsIStringBundle>   mStringBundle;
  nsCOMPtr<nsISupports> mOfflineImapSync;

};

#endif
