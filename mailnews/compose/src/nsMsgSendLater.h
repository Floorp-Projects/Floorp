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

#ifndef _nsMsgSendLater_H_
#define _nsMsgSendLater_H_

#include "nsIMsgIdentity.h"
#include "nsIMsgSendLater.h"
#include "nsIEnumerator.h"
#include "nsIMsgFolder.h"
#include "nsIMessage.h"
#include "nsIFileSpec.h"
#include "nsFileStream.h"
#include "nsIMsgSendListener.h"
#include "nsIMsgSendLaterListener.h"
#include "nsMsgDeliveryListener.h"
#include "nsIMsgSendLater.h"

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

  /* void OnStartSending (in string aMsgID, in PRUint32 aMsgSize); */
  NS_IMETHOD OnStartSending(const char *aMsgID, PRUint32 aMsgSize);
  
  /* void OnProgress (in string aMsgID, in PRUint32 aProgress, in PRUint32 aProgressMax); */
  NS_IMETHOD OnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax);
  
  /* void OnStatus (in string aMsgID, in wstring aMsg); */
  NS_IMETHOD OnStatus(const char *aMsgID, const PRUnichar *aMsg);
  
  /* void OnStopSending (in string aMsgID, in nsresult aStatus, in wstring aMsg, in nsIFileSpec returnFileSpec); */
  NS_IMETHOD OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                           nsIFileSpec *returnFileSpec);
  
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

  //  nsIBaseStream interface
  NS_DECL_NSIBASESTREAM

  // nsIOutputStream interface
  NS_DECL_NSIOUTPUTSTREAM

  // nsIMsgSendLater support
  NS_IMETHOD                SendUnsentMessages(nsIMsgIdentity *identity,
                                               nsIMsgSendLaterListener          **listenerArray);

  // Methods needed for implementing interface...
  nsIMsgFolder              *GetUnsentMessagesFolder(nsIMsgIdentity *userIdentity);
  nsresult                  StartNextMailFileSend();
  nsresult                  CompleteMailFileSend();

  nsresult                  DeleteCurrentMessage();

  // Necessary for creating a valid list of recipients
  nsresult                  BuildHeaders();
  nsresult                  DeliverQueuedLine(char *line, PRInt32 length);
  nsresult                  RebufferLeftovers(char *startBuf,  PRUint32 aLen);
  nsresult                  BuildNewBuffer(const char* aBuf, PRUint32 aCount, PRUint32 *totalBufSize);

  // RICHIE
  // This will go away when we get a stream from netlib...
  //
  nsresult                  DriveFakeStream(nsIOutputStream *stream);

  // methods for listener array processing...
  NS_IMETHOD  SetListenerArray(nsIMsgSendLaterListener **aListener);
  NS_IMETHOD  AddListener(nsIMsgSendLaterListener *aListener);
  NS_IMETHOD  RemoveListener(nsIMsgSendLaterListener *aListener);
  NS_IMETHOD  DeleteListeners();
  NS_IMETHOD  NotifyListenersOnStartSending(PRUint32 aTotalMessageCount);
  NS_IMETHOD  NotifyListenersOnProgress(PRUint32 aCurrentMessage, PRUint32 aTotalMessage);
  NS_IMETHOD  NotifyListenersOnStatus(const PRUnichar *aMsg);
  NS_IMETHOD  NotifyListenersOnStopSending(nsresult aStatus, const PRUnichar *aMsg, 
                                           PRUint32 aTotalTried, PRUint32 aSuccessful);

  // counters and things for enumeration 
  PRUint32                  mTotalSentSuccessfully;
  PRUint32                  mTotalSendCount;
  nsIEnumerator             *mEnumerator;
  nsIMsgIdentity            *mIdentity;
  nsCOMPtr<nsIMsgFolder>    mMessageFolder;
  PRBool                    mFirstTime;
 
  // Private Information
private:
  nsIMsgSendLaterListener   **mListenerArray;
  PRInt32                   mListenerArrayCount;

  nsMsgSendUnsentMessagesCallback  mCompleteCallback;
  SendOperationListener     *mSendListener;

  nsCOMPtr<nsIMessage>      mMessage;

  // RICHIE 
  // Theses are here for the hack temp file we need
  // to do...this will go away!!!
  nsFileSpec                *mHackTempFileSpec;
  nsIFileSpec               *mHackTempIFileSpec;

  //
  // File output stuff...
  //
  nsFileSpec                *mTempFileSpec;
  nsIFileSpec               *mTempIFileSpec;
  nsOutputFileStream        *mOutFile;

  void                      *mTagData;

  nsMsgDeliveryListener     *mSaveListener;

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
};


#endif /* _nsMsgSendLater_H_ */
