/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef _nsMsgFolderCompactor_h
#define _nsMsgFolderCompactor_h

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIMsgFolder.h"	 
#include "nsIStreamListener.h"
#include "nsIMsgFolderCompactor.h"

class nsIMsgMessageService;

class nsFolderCompactState : public nsIMsgFolderCompactor, public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIMSGFOLDERCOMPACTOR

  nsFolderCompactState(void);
  virtual ~nsFolderCompactState(void);
//  virtual nsresult Init(nsIMsgFolder *folder, const char *baseMsgUri, nsIMsgDatabase *db,
//                           nsIFileSpec *pathSpec);
  // virtual nsresult FinishCompact();
  virtual nsresult InitDB(nsIMsgDatabase *db);

  nsresult GetMessage(nsIMessage **message);
  nsresult BuildMessageURI(const char *baseURI, PRUint32 key, nsCString& uri);  
  char *m_baseMessageUri; // base message uri
  nsCString m_messageUri; // current message uri being copy
  nsCOMPtr<nsIMsgFolder> m_folder; // current folder being compact
  nsCOMPtr<nsIMsgDatabase> m_db; // new database for the compact folder
  nsFileSpec m_fileSpec; // new mailbox for the compact folder
  nsOutputFileStream *m_fileStream; // output file stream for writing
  nsMsgKeyArray m_keyArray; // all message keys need to be copied over
  PRInt32 m_size; // size of the message key array
  PRInt32 m_curIndex; // index of the current copied message key in key array
  nsMsgKey m_startOfNewMsg; // offset in mailbox of new message
  char m_dataBuffer[4096 + 1]; // temp data buffer for copying message
  nsresult m_status; // the status of the copying operation
  nsIMsgMessageService* m_messageService; // message service for copying
                                          // message 
};

class nsOfflineStoreCompactState : public nsFolderCompactState
{
public:

  nsOfflineStoreCompactState(void);
  virtual ~nsOfflineStoreCompactState(void);
  virtual nsresult InitDB(nsIMsgDatabase *db);

  NS_IMETHOD OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                    nsresult status, const PRUnichar *errorMsg);

  NS_IMETHODIMP FinishCompact();
};



#endif
