/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

  NS_IMETHOD SetMsgComposeAndSendObject(nsIMsgSend *obj);
  
  nsCOMPtr<nsISupports> mCopyObject;
  PRBool                          mCopyInProgress;

private:
  nsCOMPtr<nsIMsgSend>       mComposeAndSend;
};

//
// This is a class that deals with processing remote attachments. It implements
// an nsIStreamListener interface to deal with incoming data
//
class nsMsgCopy : public nsIUrlListener
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IMSGCOPY_IID; return iid; }

  nsMsgCopy();
  virtual ~nsMsgCopy();

  // nsISupports interface
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLLISTENER


  //////////////////////////////////////////////////////////////////////
  // Object methods...
  //////////////////////////////////////////////////////////////////////
  //
  nsresult              StartCopyOperation(nsIMsgIdentity       *aUserIdentity,
                                           nsIFileSpec          *aFileSpec, 
                                           nsMsgDeliverMode     aMode,
                                           nsIMsgSend           *aMsgSendObj,
                                           const char           *aSavePref,
                                           nsIMsgDBHdr          *aMsgToReplace);

  nsresult              DoCopy(nsIFileSpec *aDiskFile, nsIMsgFolder *dstFolder,
                               nsIMsgDBHdr *aMsgToReplace, PRBool aIsDraft,
                               nsIMsgWindow *msgWindow,
                               nsIMsgSend   *aMsgSendObj);

  nsresult	GetUnsentMessagesFolder(nsIMsgIdentity *userIdentity, nsIMsgFolder **msgFolder, PRBool *waitForUrl);
  nsresult	GetDraftsFolder(nsIMsgIdentity *userIdentity, nsIMsgFolder **msgFolder, PRBool *waitForUrl);
  nsresult	GetTemplatesFolder(nsIMsgIdentity *userIdentity, nsIMsgFolder **msgFolder, PRBool *waitForUrl);
  nsresult	GetSentFolder(nsIMsgIdentity *userIdentity,  nsIMsgFolder **msgFolder, PRBool *waitForUrl);
  nsresult   CreateIfMissing(nsIMsgFolder **folder, PRBool *waitForUrl);

  
  //
  // Vars for implementation...
  //
  nsIFileSpec                     *mFileSpec;     // the file we are sending...
  nsMsgDeliverMode                mMode;
  nsCOMPtr<CopyListener>          mCopyListener;
  nsCOMPtr<nsIMsgFolder>          mDstFolder;
  nsCOMPtr<nsIMsgDBHdr>           mMsgToReplace;
  PRBool                          mIsDraft;
  nsCOMPtr<nsIMsgSend>            mMsgSendObj;
  char                            *mSavePref;
};

// Useful function for the back end...
nsresult	LocateMessageFolder(nsIMsgIdentity   *userIdentity, 
                                       nsMsgDeliverMode aFolderType,
                                       const char       *aSaveURI,
				       nsIMsgFolder **msgFolder);

nsresult	MessageFolderIsLocal(nsIMsgIdentity   *userIdentity, 
                                       nsMsgDeliverMode aFolderType,
                                       const char       *aSaveURI,
				       PRBool		*aResult);

#endif /* _nsMsgCopy_H_ */
