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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsMsgCopy_H_
#define _nsMsgCopy_H_

#include "nscore.h"
#include "nsIFileSpec.h"
#include "nsMsgSend.h"
#include "nsIMsgFolder.h"
#include "nsITransactionManager.h"
#include "nsIMsgCopyServiceListener.h"
#include "nsIMsgCopyService.h"

// {0874C3B5-317D-11d3-8EFB-00A024A7D144}
#define NS_IMSGCOPY_IID           \
{ 0x874c3b5, 0x317d, 0x11d3,      \
{ 0x8e, 0xfb, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } };

// Forward declarations...
class   nsMsgCopy;

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the copy operation. We have to create this class 
// to listen for message copy completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
class CopyListener : public nsIMsgCopyServiceListener
{
public:
  CopyListener(void);
  virtual ~CopyListener(void);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  NS_IMETHOD OnStartCopy();
  
  NS_IMETHOD OnProgress(PRUint32 aProgress, PRUint32 aProgressMax);

  NS_IMETHOD SetMessageKey(PRUint32 aMessageKey);
  
  NS_IMETHOD GetMessageId(nsCString* aMessageId);
  
  NS_IMETHOD OnStopCopy(nsresult aStatus);

  NS_IMETHOD SetMsgComposeAndSendObject(nsMsgComposeAndSend *obj);

private:
  nsCOMPtr<nsMsgComposeAndSend>       mComposeAndSend;
};

//
// This is a class that deals with processing remote attachments. It implements
// an nsIStreamListener interface to deal with incoming data
//
class nsMsgCopy : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IMSGCOPY_IID; return iid; }

  nsMsgCopy();
  virtual ~nsMsgCopy();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  //////////////////////////////////////////////////////////////////////
  // Object methods...
  //////////////////////////////////////////////////////////////////////
  //
  nsresult              StartCopyOperation(nsIMsgIdentity       *aUserIdentity,
                                           nsIFileSpec          *aFileSpec, 
                                           nsMsgDeliverMode     aMode,
                                           nsMsgComposeAndSend  *aMsgSendObj,
                                           const char           *aSavePref,
                                           nsIMessage           *aMsgToReplace);

  nsresult              DoCopy(nsIFileSpec *aDiskFile, nsIMsgFolder *dstFolder,
                               nsIMessage *aMsgToReplace, PRBool aIsDraft,
                               nsITransactionManager *txnMgr,
                               nsMsgComposeAndSend   *aMsgSendObj);

  nsIMsgFolder          *GetUnsentMessagesFolder(nsIMsgIdentity *userIdentity);
  nsIMsgFolder          *GetDraftsFolder(nsIMsgIdentity *userIdentity);
  nsIMsgFolder          *GetTemplatesFolder(nsIMsgIdentity *userIdentity);
  nsIMsgFolder          *GetSentFolder(nsIMsgIdentity *userIdentity);

  
  //
  // Vars for implementation...
  //
  nsIFileSpec                     *mFileSpec;     // the file we are sending...
  nsMsgDeliverMode                mMode;
  nsCOMPtr<CopyListener>          mCopyListener;
  char                            *mSavePref;
};

// Useful function for the back end...
nsIMsgFolder      *LocateMessageFolder(nsIMsgIdentity   *userIdentity, 
                                       nsMsgDeliverMode aFolderType,
                                       const char       *aSaveURI);

PRBool            MessageFolderIsLocal(nsIMsgIdentity   *userIdentity, 
                                       nsMsgDeliverMode aFolderType,
                                       const char       *aSaveURI);

#endif /* _nsMsgCopy_H_ */
