/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "msgCore.h"

#include "nsMailboxProtocol.h"
#include "nscore.h"
#include "nsIOutputStream.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgHdr.h"
#include "nsMsgLineBuffer.h"
#include "nsMsgDBCID.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsICopyMsgStreamListener.h"
#include "nsMsgMessageFlags.h"
#include "prtime.h"
#include "prlog.h"
#include "prerror.h"
#include "prprf.h"
#include "nspr.h"

#include "nsFileStream.h"
PRLogModuleInfo *MAILBOX;
#include "nsIFileStreams.h"
#include "nsIStreamTransportService.h"
#include "nsIStreamConverterService.h"
#include "nsIIOService.h"
#include "nsXPIDLString.h"
#include "nsNetUtil.h"
#include "nsIMsgWindow.h"
#include "nsIMimeHeaders.h"

#include "nsIMsgMdnGenerator.h"

/* the output_buffer_size must be larger than the largest possible line
 * 2000 seems good for news
 *
 * jwz: I increased this to 4k since it must be big enough to hold the
 * entire button-bar HTML, and with the new "mailto" format, that can
 * contain arbitrarily long header fields like "references".
 *
 * fortezza: proxy auth is huge, buffer increased to 8k (sigh).
 */
#define OUTPUT_BUFFER_SIZE (4096*2)

nsMailboxProtocol::nsMailboxProtocol(nsIURI * aURI)
    : nsMsgProtocol(aURI)
{
  m_lineStreamBuffer =nsnull;

  // initialize the pr log if it hasn't been initialiezed already
  if (!MAILBOX)
    MAILBOX = PR_NewLogModule("MAILBOX");
}

nsMailboxProtocol::~nsMailboxProtocol()
{
  // free our local state 
  delete m_lineStreamBuffer;
}

NS_IMETHODIMP nsMailboxProtocol::GetContentLength(PRInt32 * aContentLength)
{
  *aContentLength = -1;
  if (m_mailboxAction == nsIMailboxUrl::ActionParseMailbox)
  {
    // our file transport knows the entire length of the berkley mail folder
    // so get it from there.
    if (!m_request)
      return NS_OK;

    nsCOMPtr<nsIChannel> info = do_QueryInterface(m_request);
    if (info) info->GetContentLength(aContentLength);
    return NS_OK;

  }
  else if (m_runningUrl)
  {
    PRUint32 msgSize = 0;
    m_runningUrl->GetMessageSize(&msgSize);
    *aContentLength = (PRInt32) msgSize;
  }

  return NS_OK;
}

nsresult nsMailboxProtocol::OpenMultipleMsgTransport(PRUint32 offset, PRInt32 size)
{
  nsresult rv;

  NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);
  nsCOMPtr<nsIStreamTransportService> serv =
      do_GetService(kStreamTransportServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = serv->CreateInputTransport(m_multipleMsgMoveCopyStream, offset, size, PR_FALSE, getter_AddRefs(m_transport));

  return rv;
}

nsresult nsMailboxProtocol::OpenFileSocketForReuse(nsIURI * aURL, PRUint32 aStartPosition, PRInt32 aReadCount)
{
  NS_ENSURE_ARG_POINTER(aURL);

  nsresult rv = NS_OK;
  m_readCount = aReadCount;

  nsCOMPtr <nsIFile> file;

  rv = GetFileFromURL(aURL, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);
    
  nsCOMPtr<nsIFileInputStream> fileStream = do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  m_multipleMsgMoveCopyStream = do_QueryInterface(fileStream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  fileStream->Init(file,  PR_RDONLY, 0664, PR_FALSE);  //just have to read the messages

  rv = OpenMultipleMsgTransport(aStartPosition, aReadCount);

  m_socketIsOpen = PR_FALSE;
  return rv;
}


nsresult nsMailboxProtocol::Initialize(nsIURI * aURL)
{
  NS_PRECONDITION(aURL, "invalid URL passed into MAILBOX Protocol");
  nsresult rv = NS_OK;
  if (aURL)
  {
    rv = aURL->QueryInterface(NS_GET_IID(nsIMailboxUrl), (void **) getter_AddRefs(m_runningUrl));
    if (NS_SUCCEEDED(rv) && m_runningUrl)
    {
      nsCOMPtr <nsIMsgWindow> window;
      rv = m_runningUrl->GetMailboxAction(&m_mailboxAction); 
      // clear stopped flag on msg window, because we care.
      nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningUrl);
      if (mailnewsUrl)
      {
        mailnewsUrl->GetMsgWindow(getter_AddRefs(window));
        if (window)
          window->SetStopped(PR_FALSE);
      }
      if (m_mailboxAction == nsIMailboxUrl::ActionParseMailbox)
        rv = OpenFileSocket(aURL, 0, -1 /* read in all the bytes in the file */);
      else
      {
        // we need to specify a byte range to read in so we read in JUST the message we want.
        rv = SetupMessageExtraction();
        if (NS_FAILED(rv)) return rv;
        nsMsgKey aMsgKey;
        PRUint32 aMsgSize = 0;
        rv = m_runningUrl->GetMessageKey(&aMsgKey);
        NS_ASSERTION(NS_SUCCEEDED(rv), "oops....i messed something up");
        rv = m_runningUrl->GetMessageSize(&aMsgSize);
        NS_ASSERTION(NS_SUCCEEDED(rv), "oops....i messed something up");
        if (RunningMultipleMsgUrl())
        {
          rv = OpenFileSocketForReuse(aURL, (PRUint32) aMsgKey, aMsgSize);
          // if we're running multiple msg url, we clear the event sink because the multiple
          // msg urls will handle setting the progress.
          mProgressEventSink = nsnull;
        }
        else
          rv = OpenFileSocket(aURL, (PRUint32) aMsgKey, aMsgSize);
        NS_ASSERTION(NS_SUCCEEDED(rv), "oops....i messed something up");
      }
    }
  }
  
#if defined(XP_MAC)
  m_lineStreamBuffer = new nsMsgLineStreamBuffer(OUTPUT_BUFFER_SIZE, PR_TRUE, PR_TRUE, '\r');
#else
  m_lineStreamBuffer = new nsMsgLineStreamBuffer(OUTPUT_BUFFER_SIZE, PR_TRUE);
#endif
  
  m_nextState = MAILBOX_READ_FOLDER;
  m_initialState = MAILBOX_READ_FOLDER;
  mCurrentProgress = 0;
  
  NS_NewFileSpecWithSpec(m_tempMsgFileSpec, getter_AddRefs(m_tempMessageFile));
  return rv;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// we suppport the nsIStreamListener interface 
////////////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsMailboxProtocol::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  // extract the appropriate event sinks from the url and initialize them in our protocol data
  // the URL should be queried for a nsINewsURL. If it doesn't support a news URL interface then
  // we have an error.
  if (m_nextState == MAILBOX_READ_FOLDER && m_mailboxParser)
  {
    // we need to inform our mailbox parser that it's time to start...
    m_mailboxParser->OnStartRequest(request, ctxt);
  }
  return nsMsgProtocol::OnStartRequest(request, ctxt);
}

PRBool nsMailboxProtocol::RunningMultipleMsgUrl()
{
  if (m_mailboxAction == nsIMailboxUrl::ActionCopyMessage || m_mailboxAction == nsIMailboxUrl::ActionMoveMessage)
  {
    PRUint32 numMoveCopyMsgs;
    nsresult rv = m_runningUrl->GetNumMoveCopyMsgs(&numMoveCopyMsgs);
    if (NS_SUCCEEDED(rv) && numMoveCopyMsgs > 1)
      return PR_TRUE;
  }
  return PR_FALSE;
}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsMailboxProtocol::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult aStatus)
{
  nsresult rv;
  if (m_nextState == MAILBOX_READ_FOLDER && m_mailboxParser)
  {
    // we need to inform our mailbox parser that there is no more incoming data...
    m_mailboxParser->OnStopRequest(request, ctxt, aStatus);
  }
  else if (m_nextState == MAILBOX_READ_MESSAGE) 
  {
    DoneReadingMessage();
  }
  // I'm not getting cancel status - maybe the load group still has the status.
  PRBool stopped = PR_FALSE;
  if (m_runningUrl)
  {
    nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningUrl);
    if (mailnewsUrl)
    {
      nsCOMPtr <nsIMsgWindow> window;
      mailnewsUrl->GetMsgWindow(getter_AddRefs(window));
      if (window)
        window->GetStopped(&stopped);
    }
    
    if (!stopped && NS_SUCCEEDED(aStatus) && (m_mailboxAction == nsIMailboxUrl::ActionCopyMessage || m_mailboxAction == nsIMailboxUrl::ActionMoveMessage))
    {
      PRUint32 numMoveCopyMsgs;
      PRUint32 curMoveCopyMsgIndex;
      rv = m_runningUrl->GetNumMoveCopyMsgs(&numMoveCopyMsgs);
      if (NS_SUCCEEDED(rv) && numMoveCopyMsgs > 0)
      {
        m_runningUrl->GetCurMoveCopyMsgIndex(&curMoveCopyMsgIndex);
        if (++curMoveCopyMsgIndex < numMoveCopyMsgs)
        {
          if (!mSuppressListenerNotifications && m_channelListener)
          {
       	    nsCOMPtr<nsICopyMessageStreamListener> listener = do_QueryInterface(m_channelListener, &rv);
            if (listener)
            {
              listener->EndCopy(ctxt, aStatus);
              listener->StartMessage(); // start next message.
            }
          }
          m_runningUrl->SetCurMoveCopyMsgIndex(curMoveCopyMsgIndex);
          nsCOMPtr <nsIMsgDBHdr> nextMsg;
          rv = m_runningUrl->GetMoveCopyMsgHdrForIndex(curMoveCopyMsgIndex, getter_AddRefs(nextMsg));
          if (NS_SUCCEEDED(rv) && nextMsg)
          {
            PRUint32 msgSize = 0;
            nsMsgKey msgKey;
            nsCOMPtr <nsIMsgFolder> msgFolder;
            nextMsg->GetFolder(getter_AddRefs(msgFolder));
            if (msgFolder)
            {
              nsXPIDLCString uri;
              msgFolder->GetUriForMsg(nextMsg, getter_Copies(uri));
              nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(m_runningUrl);
              if (msgUrl)
              {
                msgUrl->SetOriginalSpec(uri);
                msgUrl->SetUri(uri);
                
                nextMsg->GetMessageKey(&msgKey);
                nextMsg->GetMessageSize(&msgSize);
                // now we have to seek to the right position in the file and
                // basically re-initialize the transport with the correct message size.
                // then, we have to make sure the url keeps running somehow.
                nsCOMPtr<nsISupports> urlSupports = do_QueryInterface(m_runningUrl);
                //
                // put us in a state where we are always notified of incoming data
                //
                
                m_transport = 0; // open new stream transport
                m_inputStream = 0;
                m_outputStream = 0;
                
                rv = OpenMultipleMsgTransport(msgKey, msgSize);
                if (NS_SUCCEEDED(rv))
                {
                  if (!m_inputStream)
                    rv = m_transport->OpenInputStream(0, 0, 0, getter_AddRefs(m_inputStream));
                  
                  if (NS_SUCCEEDED(rv))
                  {
                    nsCOMPtr<nsIInputStreamPump> pump;
                    rv = NS_NewInputStreamPump(getter_AddRefs(pump), m_inputStream);
                    if (NS_SUCCEEDED(rv)) {
                      rv = pump->AsyncRead(this, urlSupports);
                      if (NS_SUCCEEDED(rv))
                        m_request = pump;
                    }
                  }
                }
                
                NS_ASSERTION(NS_SUCCEEDED(rv), "AsyncRead failed");
                if (m_loadGroup)
                  m_loadGroup->RemoveRequest(NS_STATIC_CAST(nsIRequest *, this), nsnull, aStatus);
                m_socketIsOpen = PR_TRUE; // mark the channel as open
                return aStatus;
              }
            }
          }
        }
        else
        {
        }
      }
    }
  }
  // and we want to mark ourselves for deletion or some how inform our protocol manager that we are 
  // available for another url if there is one.
  
  // mscott --> maybe we should set our state to done because we don't run multiple urls in a mailbox
  // protocol connection....
  m_nextState = MAILBOX_DONE;
  
  // the following is for smoke test purposes. QA is looking at this "Mailbox Done" string which
  // is printed out to the console and determining if the mail app loaded up correctly...obviously
  // this solution is not very good so we should look at something better, but don't remove this
  // line before talking to me (mscott) and mailnews QA....
  
  PR_LOG(MAILBOX, PR_LOG_ALWAYS, ("Mailbox Done\n"));
  
  // when on stop binding is called, we as the protocol are done...let's close down the connection
  // releasing all of our interfaces. It's important to remember that this on stop binding call
  // is coming from netlib so they are never going to ping us again with on data available. This means
  // we'll never be going through the Process loop...
  
  if (m_multipleMsgMoveCopyStream)
  {
    m_multipleMsgMoveCopyStream->Close();
    m_multipleMsgMoveCopyStream = nsnull;
  }
  nsMsgProtocol::OnStopRequest(request, ctxt, aStatus);
  return CloseSocket(); 
}

/////////////////////////////////////////////////////////////////////////////////////////////
// End of nsIStreamListenerSupport
//////////////////////////////////////////////////////////////////////////////////////////////

PRInt32 nsMailboxProtocol::DoneReadingMessage()
{
  nsresult rv = NS_OK;
  // and close the article file if it was open....
  
  if (m_mailboxAction == nsIMailboxUrl::ActionSaveMessageToDisk && m_tempMessageFile)
    rv = m_tempMessageFile->CloseStream();
  
  return rv;
}

PRInt32 nsMailboxProtocol::SetupMessageExtraction()
{
	// Determine the number of bytes we are going to need to read out of the 
	// mailbox url....
	nsCOMPtr<nsIMsgDBHdr> msgHdr;
	nsresult rv = NS_OK;
	
  NS_ASSERTION(m_runningUrl, "Not running a url");
  if (m_runningUrl)
  {
	  rv = m_runningUrl->GetMessageHeader(getter_AddRefs(msgHdr));
	  if (NS_SUCCEEDED(rv) && msgHdr)
	  {
		  PRUint32 messageSize = 0;
		  msgHdr->GetMessageSize(&messageSize);
		  m_runningUrl->SetMessageSize(messageSize);
	  }
    else
      NS_ASSERTION(PR_FALSE, "couldn't get message header");
  }
	return rv;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Begin protocol state machine functions...
//////////////////////////////////////////////////////////////////////////////////////////////

nsresult nsMailboxProtocol::LoadUrl(nsIURI * aURL, nsISupports * aConsumer)
{
  nsresult rv = NS_OK;
  // if we were already initialized with a consumer, use it...
  nsCOMPtr<nsIStreamListener> consumer = do_QueryInterface(aConsumer);
  if (consumer)
    m_channelListener = consumer;
  
  if (aURL)
  {
    m_runningUrl = do_QueryInterface(aURL);
    if (m_runningUrl)
    {
      // find out from the url what action we are supposed to perform...
      rv = m_runningUrl->GetMailboxAction(&m_mailboxAction);
      
      PRBool convertData = PR_FALSE;

      if (m_mailboxAction == nsIMailboxUrl::ActionFetchMessage)
      {
        nsCOMPtr<nsIMsgMailNewsUrl> msgUrl = do_QueryInterface(m_runningUrl, &rv);
        NS_ENSURE_SUCCESS(rv,rv);

        nsCAutoString queryStr;
        rv = msgUrl->GetQuery(queryStr);
        NS_ENSURE_SUCCESS(rv,rv);

        // check if this is a filter plugin requesting the message.
        // in that case, set up a text converter
        convertData = (queryStr.Find("header=filter") != kNotFound);
      }
      else if (m_mailboxAction == nsIMailboxUrl::ActionFetchPart)
      {
        // when fetching a part, we need to insert a converter into the listener chain order to
        // force just the part out of the message. Our channel listener is the consumer we'll
        // pass in to AsyncConvertData.
        convertData = PR_TRUE;
        consumer = m_channelListener;
      }
      if (convertData)
      {
          nsCOMPtr<nsIStreamConverterService> streamConverter = do_GetService("@mozilla.org/streamConverters;1", &rv);
          NS_ENSURE_SUCCESS(rv, rv);
          nsCOMPtr <nsIStreamListener> conversionListener;
          nsCOMPtr<nsIChannel> channel;
          QueryInterface(NS_GET_IID(nsIChannel), getter_AddRefs(channel));

          rv = streamConverter->AsyncConvertData("message/rfc822",
                                                 "*/*",
                                                 consumer, channel, getter_AddRefs(m_channelListener));
      }
      
      if (NS_SUCCEEDED(rv))
      {
        switch (m_mailboxAction)
        {
        case nsIMailboxUrl::ActionParseMailbox:
          // extract the mailbox parser..
          rv = m_runningUrl->GetMailboxParser(getter_AddRefs(m_mailboxParser));
          m_nextState = MAILBOX_READ_FOLDER;
          break;
        case nsIMailboxUrl::ActionSaveMessageToDisk:
          // ohhh, display message already writes a msg to disk (as part of a hack)
          // so we can piggy back off of that!! We just need to change m_tempMessageFile
          // to be the name of our save message to disk file. Since save message to disk
          // urls are run without a docshell to display the msg into, we won't be trying
          // to display the message after we write it to disk...
          {
            nsCOMPtr<nsIMsgMessageUrl> msgUri = do_QueryInterface(m_runningUrl);
            msgUri->GetMessageFile(getter_AddRefs(m_tempMessageFile));
            m_tempMessageFile->OpenStreamForWriting();
          }
        case nsIMailboxUrl::ActionCopyMessage:
        case nsIMailboxUrl::ActionMoveMessage:
        case nsIMailboxUrl::ActionFetchMessage:
          if (m_mailboxAction == nsIMailboxUrl::ActionSaveMessageToDisk) 
          {
            nsCOMPtr<nsIMsgMessageUrl> messageUrl = do_QueryInterface(aURL, &rv);
            if (NS_SUCCEEDED(rv))
            {
              PRBool addDummyEnvelope = PR_FALSE;
              messageUrl->GetAddDummyEnvelope(&addDummyEnvelope);
              if (addDummyEnvelope)
                SetFlag(MAILBOX_MSG_PARSE_FIRST_LINE);
              else
                ClearFlag(MAILBOX_MSG_PARSE_FIRST_LINE);
            }
          }
          else
          {
            ClearFlag(MAILBOX_MSG_PARSE_FIRST_LINE);
          }
          
          m_nextState = MAILBOX_READ_MESSAGE;
          break;
        case nsIMailboxUrl::ActionFetchPart:
            m_nextState = MAILBOX_READ_MESSAGE;
            break;
        default:
          break;
        }
      }
      
      rv = nsMsgProtocol::LoadUrl(aURL, m_channelListener);
      
    } // if we received an MAILBOX url...
  } // if we received a url!
  
  return rv;
}

PRInt32 nsMailboxProtocol::ReadFolderResponse(nsIInputStream * inputStream, PRUint32 sourceOffset, PRUint32 length)
{
  // okay we are doing a folder read in 8K chunks of a mail folder....
  // this is almost too easy....we can just forward the data in this stream on to our
  // folder parser object!!!
  
  nsresult rv = NS_OK;
  mCurrentProgress += length;
  
  if (m_mailboxParser)
  {
    nsCOMPtr <nsIURI> url = do_QueryInterface(m_runningUrl);
    rv = m_mailboxParser->OnDataAvailable(nsnull, url, inputStream, sourceOffset, length); // let the parser deal with it...
  }
  if (NS_FAILED(rv))
  {
    m_nextState = MAILBOX_ERROR_DONE; // drop out of the loop....
    return -1;
  }
  
  // now wait for the next 8K chunk to come in.....
  SetFlag(MAILBOX_PAUSE_FOR_READ);
  
  // leave our state alone so when the next chunk of the mailbox comes in we jump to this state
  // and repeat....how does this process end? Well when the file is done being read in, core net lib
  // will issue an ::OnStopRequest to us...we'll use that as our sign to drop out of this state and to
  // close the protocol instance...
  
  return 0; 
}

PRInt32 nsMailboxProtocol::ReadMessageResponse(nsIInputStream * inputStream, PRUint32 sourceOffset, PRUint32 length)
{
  char *line = nsnull;
  PRUint32 status = 0;
  nsresult rv = NS_OK;
  mCurrentProgress += length;
  
  // if we are doing a move or a copy, forward the data onto the copy handler...
  // if we want to display the message then parse the incoming data...
  
  if (m_channelListener)
  {
    // just forward the data we read in to the listener...
    rv = m_channelListener->OnDataAvailable(this, m_channelContext, inputStream, sourceOffset, length);
  }
  else
  {
    PRBool pauseForMoreData = PR_FALSE;
    PRBool canonicalLineEnding = PR_FALSE;
    nsCOMPtr<nsIMsgMessageUrl> msgurl = do_QueryInterface(m_runningUrl);
    
    if (msgurl)
      msgurl->GetCanonicalLineEnding(&canonicalLineEnding);
    do
    {
      char *saveLine;
      saveLine = line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);
      
      if (!line || (line[0] == '.' && line[1] == 0))
      {
        // we reached the end of the message!
        ClearFlag(MAILBOX_PAUSE_FOR_READ);
      } // otherwise process the line
      else
      {
        if (line[0] == '.')
          line++; // skip over the '.'
        
        /* When we're sending this line to a converter (ie,
        it's a message/rfc822) use the local line termination
        convention, not CRLF.  This makes text articles get
        saved with the local line terminators.  Since SMTP
        and NNTP mandate the use of CRLF, it is expected that
        the local system will convert that to the local line
        terminator as it is read.
        */
        // mscott - the firstline hack is aimed at making sure we don't write
        // out the dummy header when we are trying to display the message.
        // The dummy header is the From line with the date tag on it.
        if (m_tempMessageFile && TestFlag(MAILBOX_MSG_PARSE_FIRST_LINE))
        {
          PRInt32 count = 0;
          if (line)
            rv = m_tempMessageFile->Write(line, PL_strlen(line),
            &count);
          if (NS_FAILED(rv)) break;
          
          if (canonicalLineEnding)
            rv = m_tempMessageFile->Write(CRLF, 2, &count);
          else
            rv = m_tempMessageFile->Write(MSG_LINEBREAK,
            MSG_LINEBREAK_LEN, &count);
          
          if (NS_FAILED(rv)) break;
        }
        else
          SetFlag(MAILBOX_MSG_PARSE_FIRST_LINE);
      } 
      PR_Free(saveLine);
    }
    while (line && !pauseForMoreData);
  }
  
  SetFlag(MAILBOX_PAUSE_FOR_READ); // wait for more data to become available...
  if (mProgressEventSink)
  {
    PRInt32 contentLength = 0;
    GetContentLength(&contentLength);
    mProgressEventSink->OnProgress(this, m_channelContext, mCurrentProgress, contentLength);
  }
  
  if (NS_FAILED(rv)) return -1;
  
  return 0;
}


/*
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
nsresult nsMailboxProtocol::ProcessProtocolState(nsIURI * url, nsIInputStream * inputStream, PRUint32 offset, PRUint32 length)
{
  nsresult rv = NS_OK;
  PRInt32 status = 0;
  ClearFlag(MAILBOX_PAUSE_FOR_READ); /* already paused; reset */
  
  while(!TestFlag(MAILBOX_PAUSE_FOR_READ))
  {
    
    switch(m_nextState) 
    {
    case MAILBOX_READ_MESSAGE:
      if (inputStream == nsnull)
        SetFlag(MAILBOX_PAUSE_FOR_READ);
      else
        status = ReadMessageResponse(inputStream, offset, length);
      break;
    case MAILBOX_READ_FOLDER:
      if (inputStream == nsnull)
        SetFlag(MAILBOX_PAUSE_FOR_READ);   // wait for file socket to read in the next chunk...
      else
        status = ReadFolderResponse(inputStream, offset, length);
      break;
    case MAILBOX_DONE:
    case MAILBOX_ERROR_DONE:
      {
        nsCOMPtr <nsIMsgMailNewsUrl> anotherUrl = do_QueryInterface(m_runningUrl);
        rv = m_nextState == MAILBOX_DONE ? NS_OK : NS_ERROR_FAILURE;
        anotherUrl->SetUrlState(PR_FALSE, rv);
        m_nextState = MAILBOX_FREE;
      }
      break;
      
    case MAILBOX_FREE:
      // MAILBOX is a one time use connection so kill it if we get here...
      CloseSocket(); 
      return rv; /* final end */
      
    default: /* should never happen !!! */
      m_nextState = MAILBOX_ERROR_DONE;
      break;
    }
    
    /* check for errors during load and call error 
    * state if found
    */
    if(status < 0 && m_nextState != MAILBOX_FREE)
    {
      m_nextState = MAILBOX_ERROR_DONE;
      /* don't exit! loop around again and do the free case */
      ClearFlag(MAILBOX_PAUSE_FOR_READ);
    }
  } /* while(!MAILBOX_PAUSE_FOR_READ) */
  
  return rv;
}

nsresult nsMailboxProtocol::CloseSocket()
{
  // how do you force a release when closing the connection??
  nsMsgProtocol::CloseSocket(); 
  m_runningUrl = nsnull;
  m_mailboxParser = nsnull;
  return 0;
}

// vim: ts=2 sw=2
