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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsMsgCopy.h"
#include "nsIPref.h"
#include "nsMsgSendLater.h"
#include "nsIEnumerator.h"
#include "nsIFileSpec.h"
#include "nsISmtpService.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsMsgDeliveryListener.h"
#include "nsIMsgIncomingServer.h"
#include "nsICopyMessageListener.h"
#include "nsIMsgMessageService.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"
#include "nsMsgCompUtils.h"
#include "nsMsgUtils.h"
#include "nsMsgFolderFlags.h"
#include "nsIMessage.h"
#include "nsIRDFResource.h"
#include "nsISupportsArray.h"
#include "nsMailHeaders.h"
#include "nsMsgPrompts.h"
#include "nsIMsgSendListener.h"
#include "nsIMsgSendLaterListener.h"
#include "nsMsgCopy.h"
#include "nsMsgComposeStringBundle.h"
#include "nsXPIDLString.h"
#include "nsIPrompt.h"
#include "nsIURI.h"
#include "nsISmtpUrl.h"
#include "nsIChannel.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_IID(kIMsgSendLater, NS_IMSGSENDLATER_IID);
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_CID(kMsgSendCID, NS_MSGSEND_CID); 
static NS_DEFINE_CID(kISupportsArrayCID, NS_SUPPORTSARRAY_CID);

//
// This function will be used by the factory to generate the 
// nsMsgComposeAndSend Object....
//
nsresult NS_NewMsgSendLater(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgSendLater *pSendLater = new nsMsgSendLater();
		if (pSendLater)
			return pSendLater->QueryInterface(kIMsgSendLater, aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

NS_IMPL_ISUPPORTS2(nsMsgSendLater, nsIMsgSendLater, nsIStreamListener)

nsMsgSendLater::nsMsgSendLater()
{
  mIdentity = nsnull;  
  mTempIFileSpec = nsnull;
  mTempFileSpec = nsnull;
  mOutFile = nsnull;
  mEnumerator = nsnull;
  mTotalSentSuccessfully = 0;
  mTotalSendCount = 0;
  mMessageFolder = null_nsCOMPtr();
  mMessage = nsnull;
  mLeftoverBuffer = nsnull;

  mSendListener = nsnull;
  mListenerArray = nsnull;
  mListenerArrayCount = 0;

  m_to = nsnull;
  m_bcc = nsnull;
  m_fcc = nsnull;
  m_newsgroups = nsnull;
  m_newshost = nsnull;
  m_headers = nsnull;
  m_flags = 0;
  m_headersFP = 0;
  m_inhead = PR_TRUE;
  m_headersPosition = 0;

  m_bytesRead = 0;
  m_position = 0;
  m_flagsPosition = 0;
  m_headersSize = 0;

  mIdentityKey = nsnull;

  mRequestReturnReceipt = PR_FALSE;
  NS_INIT_REFCNT();
}

nsMsgSendLater::~nsMsgSendLater()
{
  NS_IF_RELEASE(mEnumerator);
  NS_IF_RELEASE(mTempIFileSpec);
  PR_FREEIF(m_to);
  PR_FREEIF(m_fcc);
  PR_FREEIF(m_bcc);
  PR_FREEIF(m_newsgroups);
  PR_FREEIF(m_newshost);
  PR_FREEIF(m_headers);
  PR_FREEIF(mLeftoverBuffer);
  PR_FREEIF(mIdentityKey);

  NS_IF_RELEASE(mSendListener);
  NS_IF_RELEASE(mIdentity);
}

// Stream is done...drive on!
nsresult
nsMsgSendLater::OnStopRequest(nsIChannel *channel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg)
{
  nsresult    rv;

  // First, this shouldn't happen, but if
  // it does, flush the buffer and move on.
  if (mLeftoverBuffer)
  {
    DeliverQueuedLine(mLeftoverBuffer, PL_strlen(mLeftoverBuffer));
  }

  if (mOutFile)
    mOutFile->close();

  // See if we succeeded on reading the message from the message store?
  //
  if (NS_SUCCEEDED(status))
  {
    // Now, so some analysis on the identity for this particular message!
    DealWithTheIdentityMojo(nsnull, PR_TRUE);

    // Message is done...send it!
    rv = CompleteMailFileSend();

#ifdef NS_DEBUG
    printf("nsMsgSendLater: Success on getting message...\n");
#endif
    
    // If the send operation failed..try the next one...
    if (NS_FAILED(rv))
    {
      rv = StartNextMailFileSend();
      if (NS_FAILED(rv))
        NotifyListenersOnStopSending(rv, nsnull, mTotalSendCount, mTotalSentSuccessfully);
    }
  }
  else
  {
    // extract the prompt object to use for the alert from the url....
    nsCOMPtr<nsIURI> uri; 
    nsCOMPtr<nsIPrompt> promptObject;
    if (channel)
    {
      channel->GetURI(getter_AddRefs(uri));
      nsCOMPtr<nsISmtpUrl> smtpUrl (do_QueryInterface(uri));
      if (smtpUrl)
        smtpUrl->GetPrompt(getter_AddRefs(promptObject));
    } 
    nsMsgDisplayMessageByID(promptObject, NS_ERROR_QUEUED_DELIVERY_FAILED);
    
    // Getting the data failed, but we will still keep trying to send the rest...
    rv = StartNextMailFileSend();
    if (NS_FAILED(rv))
      NotifyListenersOnStopSending(rv, nsnull, mTotalSendCount, mTotalSentSuccessfully);
  }

  return rv;
}

char *
FindEOL(char *inBuf, char *buf_end)
{
  char *buf = inBuf;
  char *findLoc = nsnull;

  while (buf <= buf_end)
    if (*buf == 0) 
      return buf;
    else if ( (*buf == LF) || (*buf == CR) )
    {
      findLoc = buf;
      break;
    }
    else
      ++buf;

  if (!findLoc)
    return nsnull;
  else if ((findLoc + 1) > buf_end)
    return buf;

  if ( (*findLoc == LF && *(findLoc+1) == CR) || 
       (*findLoc == CR && *(findLoc+1) == LF))
    findLoc++; // possibly a pair.       
  return findLoc;
}

nsresult
nsMsgSendLater::RebufferLeftovers(char *startBuf, PRUint32 aLen)
{
  PR_FREEIF(mLeftoverBuffer);
  mLeftoverBuffer = (char *)PR_Malloc(aLen + 1);
  if (!mLeftoverBuffer)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCRT::memcpy(mLeftoverBuffer, startBuf, aLen);
  mLeftoverBuffer[aLen] = '\0';
  return NS_OK;
}

nsresult
nsMsgSendLater::BuildNewBuffer(const char* aBuf, PRUint32 aCount, PRUint32 *totalBufSize)
{
  // Only build a buffer when there are leftovers...
  if (!mLeftoverBuffer)
    return NS_ERROR_FAILURE;

  PRInt32 leftoverSize = PL_strlen(mLeftoverBuffer);
  mLeftoverBuffer = (char *)PR_Realloc(mLeftoverBuffer, aCount + leftoverSize);
  if (!mLeftoverBuffer)
    return NS_ERROR_FAILURE;

  nsCRT::memcpy(mLeftoverBuffer + leftoverSize, aBuf, aCount);
  *totalBufSize = aCount + leftoverSize;
  return NS_OK;
}

// Got data?
nsresult
nsMsgSendLater::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  // This is a little bit tricky since we have to chop random 
  // buffers into lines and deliver the lines...plus keeping the
  // leftovers for next time...some fun, eh?
  //
  nsresult    rv = NS_OK;
  char        *startBuf; 
  char        *endBuf;
  char        *lineEnd;
  char        *newbuf = nsnull;
  PRUint32    size;  

  PRUint32    aCount = count;
  char        *aBuf = (char *)PR_Malloc(aCount + 1);

  inStr->Read(aBuf, count, &aCount);

  // First, create a new work buffer that will 
  if (NS_FAILED(BuildNewBuffer(aBuf, aCount, &size))) // no leftovers...
  {
    startBuf = (char *)aBuf;
    endBuf = (char *)(aBuf + aCount - 1);
  }
  else  // yum, leftovers...new buffer created...sitting in mLeftoverBuffer
  {
    newbuf = mLeftoverBuffer;
    startBuf = newbuf; 
    endBuf = startBuf + size - 1;
    mLeftoverBuffer = nsnull; // null out this 
  }

  while (startBuf < endBuf)
  {
    lineEnd = FindEOL(startBuf, endBuf);
    if (!lineEnd)
    {
      rv = RebufferLeftovers(startBuf, (endBuf - startBuf) + 1);           
      break;
    }

    rv = DeliverQueuedLine(startBuf, (lineEnd - startBuf) + 1);
    if (NS_FAILED(rv))
      break;

    startBuf = lineEnd+1;
  }

  if (newbuf)
    PR_FREEIF(newbuf);

  PR_FREEIF(aBuf);
  return rv;
}

nsresult
nsMsgSendLater::OnStartRequest(nsIChannel *channel, nsISupports *ctxt)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS(SendOperationListener, NS_GET_IID(nsIMsgSendListener));

SendOperationListener::SendOperationListener(void) 
{ 
  mSendLater = nsnull;
  NS_INIT_REFCNT(); 
}

SendOperationListener::~SendOperationListener(void) 
{
}

nsresult
SendOperationListener::SetSendLaterObject(nsMsgSendLater *obj)
{
  mSendLater = obj;
  return NS_OK;
}

nsresult
SendOperationListener::OnStartSending(const char *aMsgID, PRUint32 aMsgSize)
{
#ifdef NS_DEBUG
  printf("SendOperationListener::OnStartSending()\n");
#endif
  return NS_OK;
}
  
nsresult
SendOperationListener::OnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax)
{
#ifdef NS_DEBUG
  printf("SendOperationListener::OnProgress()\n");
#endif
  return NS_OK;
}

nsresult
SendOperationListener::OnStatus(const char *aMsgID, const PRUnichar *aMsg)
{
#ifdef NS_DEBUG
  printf("SendOperationListener::OnStatus()\n");
#endif

  return NS_OK;
}
  
nsresult
SendOperationListener::OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                                     nsIFileSpec *returnFileSpec)
{
  nsresult                    rv = NS_OK;

  if (mSendLater)
  {
    if (NS_SUCCEEDED(aStatus))
    {
#ifdef NS_DEBUG
      printf("nsMsgSendLater: Success on the message send operation!\n");
#endif

      PRBool    deleteMsgs = PR_TRUE;

      //
      // Now delete the message from the outbox folder.
      //
      NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
      if (NS_SUCCEEDED(rv) && prefs)
    		prefs->GetBoolPref("mail.really_delete_draft", &deleteMsgs);

      if (deleteMsgs)
      {
        mSendLater->DeleteCurrentMessage();
        //
        // Since we are deleting entries from the enumeration set, we need to
        // refresh the enumerator
        //
        NS_IF_RELEASE(mSendLater->mEnumerator);
        mSendLater->mEnumerator = nsnull;

        nsresult ret = mSendLater->mMessageFolder->GetMessages(nsnull, &(mSendLater->mEnumerator));
	      if (NS_FAILED(ret) || (!(mSendLater->mEnumerator)))
          mSendLater->mEnumerator = nsnull;   // just to be sure!

      }

      ++(mSendLater->mTotalSentSuccessfully);
    }
    else
    {
      // shame we can't get access to a prompt interface from here...=(
      nsMsgDisplayMessageByID(nsnull, NS_ERROR_SEND_FAILED);
    }

    // Regardless, we will still keep trying to send the rest...
    rv = mSendLater->StartNextMailFileSend();
    if (NS_FAILED(rv))
      mSendLater->NotifyListenersOnStopSending(rv, nsnull, mSendLater->mTotalSendCount, 
                                               mSendLater->mTotalSentSuccessfully);
    NS_RELEASE(mSendLater);
  }

  return rv;
}

nsIMsgSendListener **
CreateListenerArray(nsIMsgSendListener *listener, PRUint32 *aListeners)
{
  if (!listener)
    return nsnull;

  nsIMsgSendListener **tArray = (nsIMsgSendListener **)PR_Malloc(sizeof(nsIMsgSendListener *) * 2);
  if (!tArray)
    return nsnull;
  nsCRT::memset(tArray, 0, sizeof(nsIMsgSendListener *) * 2);
  tArray[0] = listener;
  *aListeners = 2;
  return tArray;
}

nsresult
nsMsgSendLater::CompleteMailFileSend()
{
nsresult                    rv;
nsXPIDLCString                    recips;
nsXPIDLCString                    ccList;
PRBool                      created;
nsCOMPtr<nsIMsgCompFields>  compFields = nsnull;
nsCOMPtr<nsIMsgSend>        pMsgSend = nsnull;

  // If for some reason the tmp file didn't get created, we've failed here
  mTempIFileSpec->Exists(&created);
  if (!created)
    return NS_ERROR_FAILURE;

  // Get the recipients...
  if (NS_FAILED(mMessage->GetRecipients(getter_Copies(recips))))
    return NS_ERROR_UNEXPECTED;
  else
  	mMessage->GetCcList(getter_Copies(ccList));

  // Get the composition fields interface
  nsresult res = nsComponentManager::CreateInstance(kMsgCompFieldsCID, NULL, NS_GET_IID(nsIMsgCompFields), 
                                                    (void **) getter_AddRefs(compFields)); 
  if (NS_FAILED(res) || !compFields)
  {
    return NS_ERROR_FACTORY_NOT_LOADED;
  }

  // Get the message send interface
  rv = nsComponentManager::CreateInstance(kMsgSendCID, NULL, NS_GET_IID(nsIMsgSend), 
                                          (void **) getter_AddRefs(pMsgSend)); 
  if (NS_FAILED(res) || !pMsgSend)
  {
    return NS_ERROR_FACTORY_NOT_LOADED;
  }

  // Since we have already parsed all of the headers, we are simply going to
  // set the composition fields and move on.
  //
  nsXPIDLCString author;
  mMessage->GetAuthor(getter_Copies(author));

  nsMsgCompFields * fields = (nsMsgCompFields *)compFields.get();

  nsString authorStr; authorStr.AssignWithConversion(author);
  fields->SetFrom(authorStr.ToNewUnicode());

  if (m_to)
  	fields->SetTo(m_to);

  if (m_bcc)
  	fields->SetBcc(m_bcc);

  if (m_fcc)
  	fields->SetFcc(m_fcc);

  if (m_newsgroups)
    fields->SetNewsgroups(m_newsgroups);

  // If we have this, we found a HEADER_X_MOZILLA_NEWSHOST which means
  // that we saved what the user typed into the "Newsgroup" line in this
  // header
  if (m_newshost)
    fields->SetNewsgroups(m_newshost);

  if (mRequestReturnReceipt)
    fields->SetReturnReceipt(PR_TRUE);

  // Create the listener for the send operation...
  mSendListener = new SendOperationListener();
  if (!mSendListener)
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_ADDREF(mSendListener);
  // set this object for use on completion...
  mSendListener->SetSendLaterObject(this);
  PRUint32 listeners;
  nsIMsgSendListener **tArray = CreateListenerArray(mSendListener, &listeners);
  if (!tArray)
  {
    NS_RELEASE(mSendListener);
    mSendListener = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(this);  
  rv = pMsgSend->SendMessageFile(mIdentity,
                                 compFields, // nsIMsgCompFields *fields,
                                 mTempIFileSpec, // nsIFileSpec *sendFileSpec,
                                 PR_TRUE, // PRBool deleteSendFileOnCompletion,
                                 PR_FALSE, // PRBool digest_p,
                                 nsIMsgSend::nsMsgDeliverNow, // nsMsgDeliverMode mode,
                                 nsnull, // nsIMessage *msgToReplace, 
                                 tArray, listeners); 
  NS_RELEASE(mSendListener);
  mSendListener = nsnull;
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

nsresult
nsMsgSendLater::StartNextMailFileSend()
{
  nsFileSpec    fileSpec;
  nsresult      rv;
  nsXPIDLCString  aMessageURI;

  PRBool hasMore = PR_FALSE;

  if ( (!mEnumerator) || !NS_SUCCEEDED(mEnumerator->HasMoreElements(&hasMore)) || !hasMore )
  {
    // Call any listeners on this operation and then exit cleanly
#ifdef NS_DEBUG
    printf("nsMsgSendLater: Finished \"Send Later\" operation.\n");
#endif
    NotifyListenersOnStopSending(NS_OK, nsnull, mTotalSendCount, mTotalSentSuccessfully);
    return NS_OK;
  }

  nsCOMPtr<nsISupports>   currentItem;

  rv = mEnumerator->GetNext(getter_AddRefs(currentItem));
  if (NS_FAILED(rv))
  {
    return rv;
  }

  mMessage = do_QueryInterface(currentItem); 
  if(!mMessage)
  {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIRDFResource>  myRDFNode ;
  myRDFNode = do_QueryInterface(mMessage, &rv);
  if(NS_FAILED(rv) || (!myRDFNode))
  {
    return NS_ERROR_NOT_AVAILABLE;
  }

  myRDFNode->GetValue(getter_Copies(aMessageURI));

#ifdef NS_DEBUG
  nsXPIDLCString      subject;
  mMessage->GetSubject(getter_Copies(subject));
  printf("Sending message: [%s]\n", (const char*)subject);
#endif

  mTempFileSpec = nsMsgCreateTempFileSpec("nsqmail.tmp"); 
	if (!mTempFileSpec)
    return NS_ERROR_FAILURE;

  NS_NewFileSpecWithSpec(*mTempFileSpec, &mTempIFileSpec);
	if (!mTempIFileSpec)
    return NS_ERROR_FAILURE;

  nsIMsgMessageService * messageService = nsnull;
	rv = GetMessageServiceFromURI(aMessageURI, &messageService);
  if (NS_FAILED(rv) && !messageService)
    return NS_ERROR_FACTORY_NOT_LOADED;

  ++mTotalSendCount;

  // Setup what we need to parse the data stream correctly
  m_inhead = PR_TRUE;
  m_headersFP = 0;
  m_headersPosition = 0;
  m_bytesRead = 0;
  m_position = 0;
  m_flagsPosition = 0;
  m_headersSize = 0;
  PR_FREEIF(mLeftoverBuffer);

  //
  // Now, get our stream listener interface and plug it into the DisplayMessage
  // operation
  //
  NS_ADDREF(this);

  nsCOMPtr<nsIStreamListener> convertedListener = do_QueryInterface(this);
  if (convertedListener)
  {
    // Now, just plug the two together and get the hell out of the way!
    rv = messageService->DisplayMessage(aMessageURI, convertedListener, nsnull, nsnull, nsnull, nsnull);
  }
  else
    rv = NS_ERROR_FAILURE;

  ReleaseMessageServiceFromURI(aMessageURI, messageService);
  Release();

	if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

nsresult 
nsMsgSendLater::GetUnsentMessagesFolder(nsIMsgIdentity *userIdentity, nsIMsgFolder **folder)
{
  char        *uri = nsnull;
  nsresult    rv;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_FAILED(rv) || !prefs) 
    return nsnull;

  rv = prefs->CopyCharPref("mail.default_sendlater_uri", &uri);
  if (NS_FAILED(rv) || !uri) 
    uri = PR_smprintf("%s", ANY_SERVER);

  if (!uri)
    return NS_ERROR_FAILURE;

  rv = LocateMessageFolder(userIdentity, nsIMsgSend::nsMsgQueueForLater, uri, folder);
  PR_FREEIF(uri);
  return rv;
}

//
// To really finalize this capability, we need to have the ability to get
// the message from the mail store in a stream for processing. The flow 
// would be something like this:
//
//      foreach (message in Outbox folder)
//         get stream of Nth message
//         if (not done with headers)
//            Tack on to current buffer of headers
//         when done with headers
//            BuildHeaders()
//            Write Headers to Temp File
//         after done with headers
//            write rest of message body to temp file
//
//          when done with the message
//            do send operation
//
//          when send is complete
//            Copy from Outbox to FCC folder
//            Delete from Outbox folder
//
//
nsresult 
nsMsgSendLater::SendUnsentMessages(nsIMsgIdentity                   *identity,
                                   nsIMsgSendLaterListener          **listenerArray)
{
  nsresult rv;

  DealWithTheIdentityMojo(identity, PR_FALSE);

  // Set the listener array 
  if (listenerArray)
    SetListenerArray(listenerArray);

  rv = GetUnsentMessagesFolder(mIdentity, getter_AddRefs(mMessageFolder));
  if (NS_FAILED(rv) || !mMessageFolder)
  {
    NS_IF_RELEASE(mIdentity);
    mIdentity = nsnull;
    return NS_ERROR_FAILURE;
  }

  // ### fix me - need an nsIMsgWindow here
  nsresult ret = mMessageFolder->GetMessages(nsnull, &mEnumerator);
	if (NS_FAILED(ret) || (!mEnumerator))
  {
    NS_IF_RELEASE(mIdentity);
    mIdentity = nsnull;
    return NS_ERROR_FAILURE;
  }

	return StartNextMailFileSend();
}

nsresult
nsMsgSendLater::DeleteCurrentMessage()
{
  nsCOMPtr<nsISupportsArray>  msgArray;

  // Get the composition fields interface
  nsresult res = nsComponentManager::CreateInstance(kISupportsArrayCID, NULL, NS_GET_IID(nsISupportsArray), 
                                                    (void **) getter_AddRefs(msgArray)); 
  if (NS_FAILED(res) || !msgArray)
  {
    return NS_ERROR_FACTORY_NOT_LOADED;
  }

  nsCOMPtr<nsISupports> msgSupport = do_QueryInterface(mMessage, &res);
  msgArray->InsertElementAt(msgSupport, 0);
  res = mMessageFolder->DeleteMessages(msgArray, nsnull, PR_TRUE, PR_FALSE);
  if (NS_FAILED(res))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

//
// This function parses the headers, and also deletes from the header block
// any headers which should not be delivered in mail, regardless of whether
// they were present in the queue file.  Such headers include: BCC, FCC,
// Sender, X-Mozilla-Status, X-Mozilla-News-Host, and Content-Length.
// (Content-Length is for the disk file only, and must not be allowed to
// escape onto the network, since it depends on the local linebreak
// representation.  Arguably, we could allow Lines to escape, but it's not
// required by NNTP.)
//
#define UNHEX(C) \
  ((C >= '0' && C <= '9') ? C - '0' : \
  ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
        ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))
nsresult
nsMsgSendLater::BuildHeaders()
{
	char *buf = m_headers;
	char *buf_end = buf + m_headersFP;

	PR_FREEIF(m_to);
	PR_FREEIF(m_bcc);
	PR_FREEIF(m_newsgroups);
	PR_FREEIF(m_newshost);
	m_flags = 0;

	while (buf < buf_end)
	{
		PRBool prune_p = PR_FALSE;
		PRBool  do_flags_p = PR_FALSE;
		PRBool  do_return_receipt_p = PR_FALSE;
		char *colon = PL_strchr(buf, ':');
		char *end;
		char *value = 0;
		char **header = 0;
		char *header_start = buf;

		if (! colon)
			break;

		end = colon;
		while (end > buf && (*end == ' ' || *end == '\t'))
			end--;

		switch (buf [0])
		{
		case 'B': case 'b':
		  if (!PL_strncasecmp ("BCC", buf, end - buf))
			{
			  header = &m_bcc;
			  prune_p = PR_TRUE;
			}
		  break;
		case 'C': case 'c':
		  if (!PL_strncasecmp ("CC", buf, end - buf))
			header = &m_to;
		  else if (!PL_strncasecmp (HEADER_CONTENT_LENGTH, buf, end - buf))
			prune_p = PR_TRUE;
		  break;
		case 'F': case 'f':
		  if (!PL_strncasecmp ("FCC", buf, end - buf))
			{
			  header = &m_fcc;
			  prune_p = PR_TRUE;
			}
		  break;
		case 'L': case 'l':
		  if (!PL_strncasecmp ("Lines", buf, end - buf))
			prune_p = PR_TRUE;
		  break;
		case 'N': case 'n':
		  if (!PL_strncasecmp ("Newsgroups", buf, end - buf))
  			header = &m_newsgroups;
		  break;
		case 'S': case 's':
		  if (!PL_strncasecmp ("Sender", buf, end - buf))
			prune_p = PR_TRUE;
		  break;
		case 'T': case 't':
		  if (!PL_strncasecmp ("To", buf, end - buf))
			header = &m_to;
		  break;
		case 'X': case 'x':
      {
        PRInt32 headLen = PL_strlen(HEADER_X_MOZILLA_STATUS2);
        if (headLen == end - buf &&
          !PL_strncasecmp(HEADER_X_MOZILLA_STATUS2, buf, end - buf))
          prune_p = PR_TRUE;
        else if (headLen == end - buf &&
          !PL_strncasecmp(HEADER_X_MOZILLA_STATUS, buf, end - buf))
          prune_p = do_flags_p = PR_TRUE;
        else if (!PL_strncasecmp(HEADER_X_MOZILLA_DRAFT_INFO, buf, end - buf))
          prune_p = do_return_receipt_p = PR_TRUE;
        else if (!PL_strncasecmp(HEADER_X_MOZILLA_NEWSHOST, buf, end - buf))
        {
          prune_p = PR_TRUE;
          header = &m_newshost;
        }
        else if (!PL_strncasecmp(HEADER_X_MOZILLA_IDENTITY_KEY, buf, end - buf))
        {
          prune_p = PR_TRUE;
          header = &mIdentityKey;
        }
        break;
      }
		}

	  buf = colon + 1;
	  while (*buf == ' ' || *buf == '\t')
		buf++;

	  value = buf;

SEARCH_NEWLINE:
	  while (*buf != 0 && *buf != CR && *buf != LF)
		  buf++;

	  if (buf+1 >= buf_end)
		  ;
	  // If "\r\n " or "\r\n\t" is next, that doesn't terminate the header.
	  else if (buf+2 < buf_end &&
			   (buf[0] == CR  && buf[1] == LF) &&
			   (buf[2] == ' ' || buf[2] == '\t'))
		{
		  buf += 3;
		  goto SEARCH_NEWLINE;
		}
	  // If "\r " or "\r\t" or "\n " or "\n\t" is next, that doesn't terminate
		// the header either. 
	  else if ((buf[0] == CR  || buf[0] == LF) &&
			   (buf[1] == ' ' || buf[1] == '\t'))
		{
		  buf += 2;
		  goto SEARCH_NEWLINE;
		}

	  if (header)
		{
		  int L = buf - value;
		  if (*header)
			{
			  char *newh = (char*) PR_Realloc ((*header),
											   PL_strlen(*header) + L + 10);
			  if (!newh) return NS_ERROR_OUT_OF_MEMORY;
			  *header = newh;
			  newh = (*header) + PL_strlen (*header);
			  *newh++ = ',';
			  *newh++ = ' ';
        nsCRT::memcpy(newh, value, L);
			  newh [L] = 0;
			}
		  else
			{
			  *header = (char *) PR_Malloc(L+1);
			  if (!*header) return NS_ERROR_OUT_OF_MEMORY;
        nsCRT::memcpy((*header), value, L);
			  (*header)[L] = 0;
			}
		}
	  else if (do_flags_p)
		{
		  int i;
		  char *s = value;
		  PR_ASSERT(*s != ' ' && *s != '\t');
		  m_flags = 0;
		  for (i=0 ; i<4 ; i++) {
			m_flags = (m_flags << 4) | UNHEX(*s);
			s++;
		  }
		}
	  else if (do_return_receipt_p)
		{
		  int L = buf - value;
		  char *draftInfo = (char*) PR_Malloc(L+1);
		  char *receipt = NULL;
		  if (!draftInfo) return NS_ERROR_OUT_OF_MEMORY;
      nsCRT::memcpy(draftInfo, value, L);
		  *(draftInfo+L)=0;
		  receipt = PL_strstr(draftInfo, "receipt=");
		  if (receipt) 
			{
			  char *s = receipt+8;
			  int requestForReturnReceipt = 0;
			  PR_sscanf(s, "%d", &requestForReturnReceipt);
			  
        if ((requestForReturnReceipt == 2 || requestForReturnReceipt == 3))
          mRequestReturnReceipt = PR_TRUE;
			}
		  PR_FREEIF(draftInfo);
		}

	  if (*buf == CR || *buf == LF)
		{
		  if (*buf == CR && buf[1] == LF)
			buf++;
		  buf++;
		}

	  if (prune_p)
		{
		  char *to = header_start;
		  char *from = buf;
		  while (from < buf_end)
			*to++ = *from++;
		  buf = header_start;
		  buf_end = to;
		  m_headersFP = buf_end - m_headers;
		}
	}

  m_headers[m_headersFP++] = CR;
  m_headers[m_headersFP++] = LF;

  // Now we have parsed out all of the headers we need and we 
  // can proceed.
  return NS_OK;
}

int
DoGrowBuffer(PRInt32 desired_size, PRInt32 element_size, PRInt32 quantum,
    				char **buffer, PRInt32 *size)
{
  if (*size <= desired_size)
  {
    char *new_buf;
    PRInt32 increment = desired_size - *size;
    if (increment < quantum) // always grow by a minimum of N bytes 
      increment = quantum;
    
    new_buf = (*buffer
                ? (char *) PR_Realloc (*buffer, (*size + increment)
                * (element_size / sizeof(char)))
                : (char *) PR_Malloc ((*size + increment)
                * (element_size / sizeof(char))));
    if (! new_buf)
      return NS_ERROR_OUT_OF_MEMORY;
    *buffer = new_buf;
    *size += increment;
  }
  return 0;
}

#define do_grow_headers(desired_size) \
  (((desired_size) >= m_headersSize) ? \
   DoGrowBuffer ((desired_size), sizeof(char), 1024, \
				   &m_headers, &m_headersSize) \
   : 0)

nsresult
nsMsgSendLater::DeliverQueuedLine(char *line, PRInt32 length)
{
  PRInt32 flength = length;
  
  m_bytesRead += length;
  
// convert existing newline to CRLF 
// Don't need this because the calling routine is taking care of it.
//  if (length > 0 && (line[length-1] == CR || 
//     (line[length-1] == LF && (length < 2 || line[length-2] != CR))))
//  {
//    line[length-1] = CR;
//    line[length++] = LF;
//  }
//
  //
  // We are going to check if we are looking at a "From - " line. If so, 
  // then just eat it and return NS_OK
  //
  if (!PL_strncasecmp(line, "From - ", 7))
    return NS_OK;

  if (m_inhead)
  {
    if (m_headersPosition == 0)
    {
		  // This line is the first line in a header block.
      // Remember its position.
      m_headersPosition = m_position;
      
      // Also, since we're now processing the headers, clear out the
      // slots which we will parse data into, so that the values that
      // were used the last time around do not persist.
      
      // We must do that here, and not in the previous clause of this
      // `else' (the "I've just seen a `From ' line clause") because
      // that clause happens before delivery of the previous message is
      // complete, whereas this clause happens after the previous msg
      // has been delivered.  If we did this up there, then only the
      // last message in the folder would ever be able to be both
      // mailed and posted (or fcc'ed.)
      PR_FREEIF(m_to);
      PR_FREEIF(m_bcc);
      PR_FREEIF(m_newsgroups);
      PR_FREEIF(m_newshost);
      PR_FREEIF(m_fcc);
    }
    
    if (line[0] == CR || line[0] == LF || line[0] == 0)
    {
		  // End of headers.  Now parse them; open the temp file;
      // and write the appropriate subset of the headers out. 
      m_inhead = PR_FALSE;

			mOutFile = new nsOutputFileStream(*mTempFileSpec, PR_WRONLY | PR_CREATE_FILE, 00600);
      if ( (!mOutFile) || (!mOutFile->is_open()) )
        return NS_MSG_ERROR_WRITING_FILE;

      nsresult status = BuildHeaders();
      if (NS_FAILED(status))
        return status;

      if (mOutFile->write(m_headers, m_headersFP) != m_headersFP)
        return NS_MSG_ERROR_WRITING_FILE;
    }
    else
    {
		  // Otherwise, this line belongs to a header.  So append it to the
      // header data.
      
      if (!PL_strncasecmp (line, HEADER_X_MOZILLA_STATUS, PL_strlen(HEADER_X_MOZILLA_STATUS)))
        // Notice the position of the flags.
        m_flagsPosition = m_position;
      else if (m_headersFP == 0)
        m_flagsPosition = 0;
      
      nsresult status = do_grow_headers (length + m_headersFP + 10);
      if (NS_FAILED(status)) 
        return status;
      
      nsCRT::memcpy(m_headers + m_headersFP, line, length);
      m_headersFP += length;
    }
  }
  else
  {
    // This is a body line.  Write it to the file.
    PR_ASSERT(mOutFile);
    if (mOutFile)
    {
      PRInt32 wrote = mOutFile->write(line, length);
      if (wrote < (PRInt32) length) 
        return NS_MSG_ERROR_WRITING_FILE;
    }
  }
  
  m_position += flength;
  return NS_OK;
}

nsresult
nsMsgSendLater::SetListenerArray(nsIMsgSendLaterListener **aListenerArray)
{
  nsIMsgSendLaterListener **ptr = aListenerArray;
  if ( (!aListenerArray) || (!*aListenerArray) )
    return NS_OK;

  // First, count the listeners passed in...
  mListenerArrayCount = 0;
  while (*ptr != nsnull)
  {
    mListenerArrayCount++;
    ++ptr;
  }

  // now allocate an array to hold the number of entries.
  mListenerArray = (nsIMsgSendLaterListener **) PR_Malloc(sizeof(nsIMsgSendLaterListener *) * mListenerArrayCount);
  if (!mListenerArray)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCRT::memset(mListenerArray, 0, (sizeof(nsIMsgSendLaterListener *) * mListenerArrayCount));
  
  // Now assign the listeners...
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
  {
    mListenerArray[i] = aListenerArray[i];
    NS_ADDREF(mListenerArray[i]);
  }

  return NS_OK;
}

nsresult
nsMsgSendLater::AddListener(nsIMsgSendLaterListener *aListener)
{
  if ( (mListenerArrayCount > 0) || mListenerArray )
  {
    ++mListenerArrayCount;
    mListenerArray = (nsIMsgSendLaterListener **) 
                  PR_Realloc(*mListenerArray, sizeof(nsIMsgSendLaterListener *) * mListenerArrayCount);
    if (!mListenerArray)
      return NS_ERROR_OUT_OF_MEMORY;
    else
    {
      mListenerArray[mListenerArrayCount - 1] = aListener;
      return NS_OK;
    }
  }
  else
  {
    mListenerArrayCount = 1;
    mListenerArray = (nsIMsgSendLaterListener **) PR_Malloc(sizeof(nsIMsgSendLaterListener *) * mListenerArrayCount);
    if (!mListenerArray)
      return NS_ERROR_OUT_OF_MEMORY;

    nsCRT::memset(mListenerArray, 0, (sizeof(nsIMsgSendLaterListener *) * mListenerArrayCount));
  
    mListenerArray[0] = aListener;
    NS_ADDREF(mListenerArray[0]);
    return NS_OK;
  }
}

nsresult
nsMsgSendLater::RemoveListener(nsIMsgSendLaterListener *aListener)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] == aListener)
    {
      NS_RELEASE(mListenerArray[i]);
      mListenerArray[i] = nsnull;
      return NS_OK;
    }

  return NS_ERROR_INVALID_ARG;
}

nsresult
nsMsgSendLater::DeleteListeners()
{
  if ( (mListenerArray) && (*mListenerArray) )
  {
    PRInt32 i;
    for (i=0; i<mListenerArrayCount; i++)
    {
      NS_RELEASE(mListenerArray[i]);
    }
    
    PR_FREEIF(mListenerArray);
  }

  mListenerArrayCount = 0;
  return NS_OK;
}

nsresult
nsMsgSendLater::NotifyListenersOnStartSending(PRUint32 aTotalMessageCount)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStartSending(aTotalMessageCount);

  return NS_OK;
}

nsresult
nsMsgSendLater::NotifyListenersOnProgress(PRUint32 aCurrentMessage, PRUint32 aTotalMessage)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnProgress(aCurrentMessage, aTotalMessage);

  return NS_OK;
}

nsresult
nsMsgSendLater::NotifyListenersOnStatus(const PRUnichar *aMsg)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStatus(aMsg);

  return NS_OK;
}

nsresult
nsMsgSendLater::NotifyListenersOnStopSending(nsresult aStatus, const PRUnichar *aMsg, 
                                             PRUint32 aTotalTried, PRUint32 aSuccessful)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStopSending(aStatus, aMsg, aTotalTried, aSuccessful);

  return NS_OK;
}

nsresult
nsMsgSendLater::DealWithTheIdentityMojo(nsIMsgIdentity  *identity, 
                                        PRBool          aSearchHeadersOnly)
{
  //
  // Ok, here's the deal. This is the part where we need to determine the identity
  // to use for this particular email message since we should observer that setting.
  // If this comes up snake-eyes, then we should fall back to a default identity, BUT
  // if aSearchHeadersOnly is TRUE, then we should ONLY look for the header identity
  //
  nsIMsgIdentity  *tIdentity = nsnull;
  nsresult        rv;

  if (mIdentityKey)
  {
    // get the account manager
    NS_WITH_SERVICE(nsIMsgAccountManager, accountManager, NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
      return rv;

    nsCOMPtr<nsIMsgAccount>     defAcc = nsnull;
    if (NS_SUCCEEDED(accountManager->GetDefaultAccount( getter_AddRefs(defAcc) )) && defAcc)
    {
      nsCOMPtr<nsISupportsArray> identities;
      if (NS_SUCCEEDED(defAcc->GetIdentities(getter_AddRefs(identities))))
      {
        nsIMsgIdentity    *lookupIdentity = nsnull;
        PRUint32          count = 0;
        char              *tName = nsnull;

        identities->Count(&count);
        for (PRUint32 i=0; i < count; i++)
        {
          rv = identities->QueryElementAt(0, NS_GET_IID(nsIMsgIdentity),
                                    (void **)&lookupIdentity);
          if (NS_FAILED(rv))
            continue;

          lookupIdentity->GetKey(&tName);
          if (!nsCRT::strcasecmp(mIdentityKey, tName))
          {
            PR_FREEIF(tName);
            NS_IF_RELEASE(mIdentity);
            mIdentity = lookupIdentity;
            NS_IF_ADDREF(mIdentity);
            return NS_OK;
          }
          else
          {
            PR_FREEIF(tName);
          }
        }
      }
    }
  }

  // At this point, if we are only looking for something in the email headers, 
  // we should just bail
  //
  if (aSearchHeadersOnly)
    return NS_OK;

  // Ok, the sucker is still null, assign the identity arg
  // to the temp variable.
  if ( (!tIdentity) && (!identity) )
  {
    // get the account manager
    NS_WITH_SERVICE(nsIMsgAccountManager, accountManager, NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
      return rv;

    nsCOMPtr<nsIMsgAccount>     defAcc = nsnull;
    if (NS_SUCCEEDED(accountManager->GetDefaultAccount( getter_AddRefs(defAcc) )) && defAcc)
    {
      if (NS_FAILED(defAcc->GetDefaultIdentity(&tIdentity)))
        return NS_ERROR_INVALID_ARG;
    }
  }
  else
  {
    tIdentity = identity;
  }

  if (!tIdentity)
    return NS_ERROR_INVALID_ARG;

  // Now setup and addref the identity
  NS_IF_RELEASE(mIdentity);
  mIdentity = tIdentity;
  NS_IF_ADDREF(mIdentity);

  return NS_OK;
}

