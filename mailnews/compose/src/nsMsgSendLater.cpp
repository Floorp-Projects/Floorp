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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsMsgSendLater.h"
#include "nsCOMPtr.h"
#include "nsMsgCopy.h"
#include "nsIPref.h"
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
#include "prlog.h"
#include "nsMsgSimulateError.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_CID(kMsgSendCID, NS_MSGSEND_CID); 
static NS_DEFINE_CID(kISupportsArrayCID, NS_SUPPORTSARRAY_CID);

NS_IMPL_ISUPPORTS2(nsMsgSendLater, nsIMsgSendLater, nsIStreamListener)

nsMsgSendLater::nsMsgSendLater()
{
  mIdentity = nsnull;  
  mTempIFileSpec = nsnull;
  mTempFileSpec = nsnull;
  mOutFile = nsnull;
  mTotalSentSuccessfully = 0;
  mTotalSendCount = 0;
  mMessageFolder = nsnull;
  mMessage = nsnull;
  mLeftoverBuffer = nsnull;

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

  NS_NewISupportsArray(getter_AddRefs(mMessagesToSend));

  NS_INIT_REFCNT();
}

nsMsgSendLater::~nsMsgSendLater()
{
  NS_IF_RELEASE(mTempIFileSpec);
  PR_FREEIF(m_to);
  PR_FREEIF(m_fcc);
  PR_FREEIF(m_bcc);
  PR_FREEIF(m_newsgroups);
  PR_FREEIF(m_newshost);
  PR_FREEIF(m_headers);
  PR_FREEIF(mLeftoverBuffer);
  PR_FREEIF(mIdentityKey);

  NS_IF_RELEASE(mIdentity);
}

// Stream is done...drive on!
NS_IMETHODIMP
nsMsgSendLater::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
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
  SET_SIMULATED_ERROR(SIMULATED_SEND_ERROR_13, status, NS_ERROR_FAILURE);
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
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    if(!channel) return NS_ERROR_FAILURE;

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
    else if ( (*buf == nsCRT::LF) || (*buf == nsCRT::CR) )
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

  if ( (*findLoc == nsCRT::LF && *(findLoc+1) == nsCRT::CR) || 
       (*findLoc == nsCRT::CR && *(findLoc+1) == nsCRT::LF))
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
NS_IMETHODIMP
nsMsgSendLater::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
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

  while (startBuf <= endBuf)
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

NS_IMETHODIMP
nsMsgSendLater::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS2(SendOperationListener, nsIMsgSendListener,
                   nsIMsgCopyServiceListener)

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
SendOperationListener::OnGetDraftFolderURI(const char *aFolderURI)
{
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
      nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
      if (NS_SUCCEEDED(rv) && prefs)
        prefs->GetBoolPref("mail.really_delete_draft", &deleteMsgs);

      if (deleteMsgs)
      {
        mSendLater->DeleteCurrentMessage();
      }

      ++(mSendLater->mTotalSentSuccessfully);
    }
  }

  return rv;
}

// nsIMsgCopyServiceListener

nsresult
SendOperationListener::OnStartCopy(void)
{
  return NS_OK;
}

nsresult
SendOperationListener::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

nsresult
SendOperationListener::SetMessageKey(PRUint32 aKey)
{
  NS_NOTREACHED("SendOperationListener::SetMessageKey()");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
SendOperationListener::GetMessageId(nsCString * aMessageId)
{
  NS_NOTREACHED("SendOperationListener::GetMessageId()\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
SendOperationListener::OnStopCopy(nsresult aStatus)
{
  if (mSendLater) {
    // Regardless of the success of the copy we will still keep trying
    // to send the rest...
    nsresult rv;
    rv = mSendLater->StartNextMailFileSend();
    if (NS_FAILED(rv))
      mSendLater->NotifyListenersOnStopSending(rv, nsnull,
                                               mSendLater->mTotalSendCount, 
                                               mSendLater->mTotalSentSuccessfully);
    NS_RELEASE(mSendLater);
  }

  return NS_OK;
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

  fields->SetFrom(author.get());

  if (m_to)
    fields->SetTo(m_to);

  if (m_bcc)
    fields->SetBcc(m_bcc);

  if (m_fcc)
    fields->SetFcc(m_fcc);

  if (m_newsgroups)
    fields->SetNewsgroups(m_newsgroups);

#if 0
  // needs cleanup.  SetNewspostUrl()?
  if (m_newshost)
    fields->SetNewshost(m_newshost);
#endif

  if (mRequestReturnReceipt)
    fields->SetReturnReceipt(PR_TRUE);

  // Create the listener for the send operation...
  SendOperationListener * sendListener = new SendOperationListener();
  if (!sendListener)
    return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(sendListener);
  // set this object for use on completion...
  sendListener->SetSendLaterObject(this);

  NS_ADDREF(this);  //TODO: We should remove this!!!
  rv = pMsgSend->SendMessageFile(mIdentity,
                                 compFields, // nsIMsgCompFields *fields,
                                 mTempIFileSpec, // nsIFileSpec *sendFileSpec,
                                 PR_TRUE, // PRBool deleteSendFileOnCompletion,
                                 PR_FALSE, // PRBool digest_p,
                                 nsIMsgSend::nsMsgSendUnsent, // nsMsgDeliverMode mode,
                                 nsnull, // nsIMsgDBHdr *msgToReplace, 
                                 sendListener,
                                 nsnull); 
  NS_IF_RELEASE(sendListener);
  return rv;
}

nsresult
nsMsgSendLater::StartNextMailFileSend()
{
  nsFileSpec    fileSpec;
  nsresult      rv = NS_OK;;
  nsXPIDLCString  aMessageURI;

  PRBool hasMore = PR_FALSE;

  if ( (!mEnumerator) || (mEnumerator->IsDone() == NS_OK) )
  {
    // Call any listeners on this operation and then exit cleanly
#ifdef NS_DEBUG
    printf("nsMsgSendLater: Finished \"Send Later\" operation.\n");
#endif

    mMessagesToSend->Clear(); // clear out our array
    NotifyListenersOnStopSending(NS_OK, nsnull, mTotalSendCount, mTotalSentSuccessfully);
    return NS_OK;
  }

  nsCOMPtr<nsISupports>   currentItem;
  mEnumerator->CurrentItem(getter_AddRefs(currentItem));
  // advance to the next item for the next pass.
  mEnumerator->Next();

  mMessage = do_QueryInterface(currentItem); 
  if(!mMessage)
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIMsgDBHdr>  myRDFNode ;
  myRDFNode = do_QueryInterface(mMessage, &rv);
  if(NS_FAILED(rv) || (!myRDFNode))
    return NS_ERROR_NOT_AVAILABLE;

  mMessageFolder->GetUriForMsg(mMessage, getter_Copies(aMessageURI));

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

  nsCOMPtr <nsIMsgMessageService> messageService;
	rv = GetMessageServiceFromURI(aMessageURI, getter_AddRefs(messageService));
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

  Release();

  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

NS_IMETHODIMP 
nsMsgSendLater::GetUnsentMessagesFolder(nsIMsgIdentity *userIdentity, nsIMsgFolder **folder)
{
  nsresult    rv;
  char        *uri = GetFolderURIFromUserPrefs(nsIMsgSend::nsMsgQueueForLater, userIdentity);

  if (!uri)
    return NS_ERROR_OUT_OF_MEMORY;

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
NS_IMETHODIMP 
nsMsgSendLater::SendUnsentMessages(nsIMsgIdentity *identity)
{
  nsresult rv = NS_OK;

  DealWithTheIdentityMojo(identity, PR_FALSE);

  rv = GetUnsentMessagesFolder(mIdentity, getter_AddRefs(mMessageFolder));
  if (NS_FAILED(rv) || !mMessageFolder)
  {
    NS_IF_RELEASE(mIdentity);
    mIdentity = nsnull;
    return NS_ERROR_FAILURE;
  }

  // ### fix me - if we need to reparse the folder, this will be asynchronous
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult ret = mMessageFolder->GetMessages(m_window, getter_AddRefs(enumerator));
  if (NS_FAILED(ret) || (!enumerator))
  {
    NS_IF_RELEASE(mIdentity);
    mIdentity = nsnull;
    return NS_ERROR_FAILURE;
  }

  // copy all the elements in the enumerator into our isupports array....

  nsCOMPtr<nsISupports>   currentItem;
  PRBool hasMoreElements = PR_FALSE;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreElements)) && hasMoreElements)
  {
    rv = enumerator->GetNext(getter_AddRefs(currentItem));
    if (NS_SUCCEEDED(rv) && currentItem)
      mMessagesToSend->AppendElement(currentItem);
  }

  // now get an enumerator for our array
  mMessagesToSend->Enumerate(getter_AddRefs(mEnumerator));

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
  res = mMessageFolder->DeleteMessages(msgArray, nsnull, PR_TRUE, PR_FALSE, nsnull, PR_FALSE /*allowUndo*/);
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
        else if (PL_strlen(HEADER_X_MOZILLA_STATUS) == end - buf &&
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
    while (*buf != 0 && *buf != nsCRT::CR && *buf != nsCRT::LF)
      buf++;

    if (buf+1 >= buf_end)
      ;
    // If "\r\n " or "\r\n\t" is next, that doesn't terminate the header.
    else if (buf+2 < buf_end &&
         (buf[0] == nsCRT::CR  && buf[1] == nsCRT::LF) &&
         (buf[2] == ' ' || buf[2] == '\t'))
    {
      buf += 3;
      goto SEARCH_NEWLINE;
    }
    // If "\r " or "\r\t" or "\n " or "\n\t" is next, that doesn't terminate
    // the header either. 
    else if ((buf[0] == nsCRT::CR  || buf[0] == nsCRT::LF) &&
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

    if (*buf == nsCRT::CR || *buf == nsCRT::LF)
    {
      if (*buf == nsCRT::CR && buf[1] == nsCRT::LF)
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

  m_headers[m_headersFP++] = nsCRT::CR;
  m_headers[m_headersFP++] = nsCRT::LF;

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
//  if (length > 0 && (line[length-1] == nsCRT::CR || 
//     (line[length-1] == nsCRT::LF && (length < 2 || line[length-2] != nsCRT::CR))))
//  {
//    line[length-1] = nsCRT::CR;
//    line[length++] = nsCRT::LF;
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
    
    if (line[0] == nsCRT::CR || line[0] == nsCRT::LF || line[0] == 0)
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

NS_IMETHODIMP
nsMsgSendLater::SetMsgWindow(nsIMsgWindow *aMsgWindow)
{
  m_window = aMsgWindow;
  return NS_OK;
}
NS_IMETHODIMP
nsMsgSendLater::GetMsgWindow(nsIMsgWindow **aMsgWindow)
{
  NS_ENSURE_ARG(aMsgWindow);
  *aMsgWindow = m_window;
  NS_IF_ADDREF(*aMsgWindow);
  return NS_OK;
}

NS_IMETHODIMP
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

NS_IMETHODIMP
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
    nsCOMPtr<nsIMsgAccountManager> accountManager = 
             do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
      return rv;

    nsCOMPtr<nsIMsgAccount>     defAcc = nsnull;
    if (NS_SUCCEEDED(accountManager->GetDefaultAccount( getter_AddRefs(defAcc) )) && defAcc)
    {
      nsCOMPtr<nsISupportsArray> identities;
      if (NS_SUCCEEDED(defAcc->GetIdentities(getter_AddRefs(identities))))
      {
        nsCOMPtr<nsIMsgIdentity>  lookupIdentity;
        PRUint32          count = 0;
        char              *tName = nsnull;

        identities->Count(&count);
        for (PRUint32 i=0; i < count; i++)
        {
          rv = identities->QueryElementAt(0, NS_GET_IID(nsIMsgIdentity),
                                    getter_AddRefs(lookupIdentity));
          if (NS_FAILED(rv))
            continue;

          lookupIdentity->GetKey(&tName);
          if (!nsCRT::strcasecmp(mIdentityKey, tName))
          {
            PR_FREEIF(tName);
            NS_IF_RELEASE(mIdentity);
            mIdentity = lookupIdentity.get();
            NS_IF_ADDREF(mIdentity);
            return NS_OK;
          }
          else
            PR_FREEIF(tName);
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
    nsCOMPtr<nsIMsgAccountManager> accountManager = 
             do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
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

