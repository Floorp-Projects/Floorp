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

#ifndef _nsMsgSendLater_H_
#define _nsMsgSendLater_H_

#include "nsIMsgSendLater.h"

#include "nsIMsgIdentity.h"
#include "nsIEnumerator.h"
#include "nsISupportsArray.h"
#include "nsIMsgFolder.h"
#include "nsIFileSpec.h"
#include "nsFileStream.h"
#include "nsIMsgSendListener.h"
#include "nsIMsgSendLaterListener.h"
#include "nsMsgDeliveryListener.h"
#include "nsIMsgSendLater.h"
#include "nsIMsgWindow.h"

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
class nsMsgSendLater;
class nsMsgDeliveryListener;

class SendOperationListener : public nsIMsgSendListener
{
public:
  SendOperationListener(void);
  virtual ~SendOperationListener(void);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIMsgSendListener interface
  NS_DECL_NSIMSGSENDLISTENER
    
  NS_IMETHOD SetSendLaterObject(nsMsgSendLater *obj);
private:
  nsMsgSendLater    *mSendLater;
};

class nsMsgSendLater: public nsIMsgSendLater
{
public:
	nsMsgSendLater();
	virtual     ~nsMsgSendLater();

	NS_DECL_ISUPPORTS

  NS_DECL_NSIMSGSENDLATER
  // For nsIStreamListener interface...
  NS_DECL_NSISTREAMLISTENER

  // For nsIRequestObserver interface...
  NS_DECL_NSIREQUESTOBSERVER

  // Methods needed for implementing interface...
  nsresult                  StartNextMailFileSend();
  nsresult                  CompleteMailFileSend();

  nsresult                  DeleteCurrentMessage();

  // Necessary for creating a valid list of recipients
  nsresult                  BuildHeaders();
  nsresult                  DeliverQueuedLine(char *line, PRInt32 length);
  nsresult                  RebufferLeftovers(char *startBuf,  PRUint32 aLen);
  nsresult                  BuildNewBuffer(const char* aBuf, PRUint32 aCount, PRUint32 *totalBufSize);

  // methods for listener array processing...
  nsresult  NotifyListenersOnStartSending(PRUint32 aTotalMessageCount);
  nsresult  NotifyListenersOnProgress(PRUint32 aCurrentMessage, PRUint32 aTotalMessage);
  nsresult  NotifyListenersOnStatus(const PRUnichar *aMsg);
  nsresult  NotifyListenersOnStopSending(nsresult aStatus, const PRUnichar *aMsg, 
                                           PRUint32 aTotalTried, PRUint32 aSuccessful);

  // counters and things for enumeration 
  PRUint32                  mTotalSentSuccessfully;
  PRUint32                  mTotalSendCount;
  nsCOMPtr<nsISupportsArray> mMessagesToSend;
  nsCOMPtr<nsIEnumerator> mEnumerator;
  nsIMsgIdentity            *mIdentity;
  nsCOMPtr<nsIMsgFolder>    mMessageFolder;
  nsCOMPtr<nsIMsgWindow>    m_window;
 
  // Private Information
private:
  NS_IMETHOD                DealWithTheIdentityMojo(nsIMsgIdentity *identity,
                                                    PRBool         aSearchHeadersOnly);

  nsIMsgSendLaterListener   **mListenerArray;
  PRInt32                   mListenerArrayCount;

  nsMsgSendUnsentMessagesCallback  mCompleteCallback;

  nsCOMPtr<nsIMsgDBHdr>      mMessage;

  //
  // File output stuff...
  //
  nsFileSpec                *mTempFileSpec;
  nsIFileSpec               *mTempIFileSpec;
  nsOutputFileStream        *mOutFile;

  void                      *mTagData;

  // For building headers and stream parsing...
  char                      *m_to;
  char                      *m_bcc;
  char                      *m_fcc;
  char                      *m_newsgroups;
  char                      *m_newshost;
  char                      *m_headers;
  PRInt32                   m_flags;
  PRInt32                   m_headersFP;
  PRBool                    m_inhead;
  PRInt32                   m_headersPosition;
  PRInt32                   m_bytesRead;
  PRInt32                   m_position;
  PRInt32                   m_flagsPosition;
  PRInt32                   m_headersSize;
  char                      *mLeftoverBuffer;
  PRBool                    mRequestReturnReceipt;
  char                      *mIdentityKey;
};


#endif /* _nsMsgSendLater_H_ */
