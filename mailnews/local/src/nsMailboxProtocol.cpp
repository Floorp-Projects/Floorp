/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "msgCore.h"

#include "nsMailboxProtocol.h"
#include "nscore.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsINetService.h"

#include "nsMailDatabase.h"

#include "rosetta.h"

#include "allxpstr.h"
#include "prtime.h"
#include "prlog.h"
#include "prerror.h"
#include "prprf.h"

static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kIWebShell, NS_IWEB_SHELL_IID);

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

////////////////////////////////////////////////////////////////////////////////////////////
// TEMPORARY HARD CODED FUNCTIONS 
///////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////
// END OF TEMPORARY HARD CODED FUNCTIONS 
///////////////////////////////////////////////////////////////////////////////////////////

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_ADDREF(nsMailboxProtocol)
NS_IMPL_RELEASE(nsMailboxProtocol)
NS_IMPL_QUERY_INTERFACE(nsMailboxProtocol, nsIStreamListener::GetIID()); /* we need to pass in the interface ID of this interface */

nsMailboxProtocol::nsMailboxProtocol(nsIURL * aURL)
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
  Initialize(aURL);
}

nsMailboxProtocol::~nsMailboxProtocol()
{
	// release all of our event sinks
	NS_IF_RELEASE(m_mailboxParser);
	
	// free our local state
	PR_FREEIF(m_dataBuf);

	// free handles on all networking objects...
	NS_IF_RELEASE(m_outputStream); 
	NS_IF_RELEASE(m_outputConsumer);
	NS_IF_RELEASE(m_transport);
}

void nsMailboxProtocol::Initialize(nsIURL * aURL)
{
	NS_PRECONDITION(aURL, "invalid URL passed into MAILBOX Protocol");

	m_flags = 0;

	// query the URL for a nsIMAILBOXUrl
	m_runningUrl = nsnull; // initialize to NULL
	m_transport = nsnull;
	m_displayConsumer = nsnull;

	if (aURL)
	{
		nsresult rv = aURL->QueryInterface(nsIMailboxUrl::GetIID(), (void **)&m_runningUrl);
		if (NS_SUCCEEDED(rv) && m_runningUrl)
		{
			// extract the file name and create a file transport...
			const char * fileName = nsnull;
            nsINetService* pNetService;
            rv = nsServiceManager::GetService(kNetServiceCID,
                                              nsINetService::GetIID(),
                                              (nsISupports**)&pNetService);
			if (NS_SUCCEEDED(rv) && pNetService)
			{
				const nsFileSpec * fileSpec = nsnull;
				m_runningUrl->GetFilePath(&fileSpec);

				nsFilePath filePath(*fileSpec);
				rv = pNetService->CreateFileSocketTransport(&m_transport, filePath);
                (void)nsServiceManager::ReleaseService(kNetServiceCID, pNetService);
			}
		}
	}
	
	m_outputStream = NULL;
	m_outputConsumer = NULL;

	nsresult rv = m_transport->GetOutputStream(&m_outputStream);
	NS_ASSERTION(NS_SUCCEEDED(rv), "ooops, transport layer unable to create an output stream");
	rv = m_transport->GetOutputStreamConsumer(&m_outputConsumer);
	NS_ASSERTION(NS_SUCCEEDED(rv), "ooops, transport layer unable to provide us with an output consumer!");

	// register self as the consumer for the socket...
	rv = m_transport->SetInputStreamConsumer((nsIStreamListener *) this);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register MAILBOX instance as a consumer on the socket");

	m_dataBuf = (char *) PR_Malloc(sizeof(char) * OUTPUT_BUFFER_SIZE);
	m_dataBufSize = OUTPUT_BUFFER_SIZE;
	m_mailboxParser = nsnull;

	m_nextState = MAILBOX_READ_FOLDER;
	m_initialState = MAILBOX_READ_FOLDER;

	m_urlInProgress = PR_FALSE;
	m_socketIsOpen = PR_FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// we suppport the nsIStreamListener interface 
////////////////////////////////////////////////////////////////////////////////////////////

// Whenever data arrives from the connection, core netlib notifices the protocol by calling
// OnDataAvailable. We then read and process the incoming data from the input stream. 
NS_IMETHODIMP nsMailboxProtocol::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
{
	// right now, this really just means turn around and process the url
	ProcessMailboxState(aURL, aIStream, aLength);
	return NS_OK;
}

NS_IMETHODIMP nsMailboxProtocol::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
	// extract the appropriate event sinks from the url and initialize them in our protocol data
	// the URL should be queried for a nsINewsURL. If it doesn't support a news URL interface then
	// we have an error.

	if (m_nextState == MAILBOX_READ_FOLDER && m_mailboxParser)
	{
		// we need to inform our mailbox parser that it's time to start...
		m_mailboxParser->OnStartBinding(aURL, aContentType);

	}
	return NS_OK;

}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsMailboxProtocol::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
	// what can we do? we can close the stream?
	m_urlInProgress = PR_FALSE;  
	m_runningUrl->SetUrlState(PR_FALSE, aStatus);

	if (m_nextState == MAILBOX_READ_FOLDER && m_mailboxParser)
	{
		// we need to inform our mailbox parser that there is no more incoming data...
		m_mailboxParser->OnStopBinding(aURL, 0, nsnull);
	}
	if (m_nextState == MAILBOX_READ_MESSAGE) 
	{
		// and close the article file if it was open....
		if (m_tempMessageFile)
			PR_Close(m_tempMessageFile);

		// mscott: hack alert...now that the file is done...turn around and fire a file url 
		// to display the message....
		char * fileUrl = PR_smprintf("file:///%s", MESSAGE_PATH_URL);
		if (m_displayConsumer)
			m_displayConsumer->LoadURL(nsAutoString(fileUrl), nsnull, PR_TRUE, nsURLReload, 0);
		PR_FREEIF(fileUrl);
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
	printf("Mailbox Done\n");

	return NS_OK;

}

/////////////////////////////////////////////////////////////////////////////////////////////
// End of nsIStreamListenerSupport
//////////////////////////////////////////////////////////////////////////////////////////////

PRInt32 nsMailboxProtocol::ReadLine(nsIInputStream * inputStream, PRUint32 length, char ** line)
{
	// I haven't looked into writing this yet. We have a couple of possibilities:
	// (1) insert ReadLine *yuck* into here or better yet into the nsIInputStream
	// then we can just turn around and call it here. 
	// OR
	// (2) we write "protocol" specific code for news which looks for a CRLF in the incoming
	// stream. If it finds it, that's our new line that we put into @param line. We'd
	// need a buffer (m_dataBuf) to store extra info read in from the stream.....

	// read out everything we've gotten back and return it in line...this won't work for much but it does
	// get us going...

	// XXX: please don't hold this quick "algorithm" against me. I just want to read just one
	// line for the stream. I promise this is ONLY temporary to test out NNTP. We need a generic
	// way to read one line from a stream. For now I'm going to read out one character at a time.
	// (I said it was only temporary =)) and test for newline...

	PRUint32 numBytesToRead = 0;  // MAX # bytes to read from the stream
	PRUint32 numBytesRead = 0;	  // total number bytes we have read from the stream during this call
	inputStream->GetLength(&length); // refresh the length in case it has changed...

	if (length > OUTPUT_BUFFER_SIZE)
		numBytesToRead = OUTPUT_BUFFER_SIZE;
	else
		numBytesToRead = length;

	m_dataBuf[0] = '\0';
	PRUint32 numBytesLastRead = 0;  // total number of bytes read in the last cycle...
	do
	{
		inputStream->Read(m_dataBuf + numBytesRead /* offset into m_dataBuf */, 1 /* read just one byte */, &numBytesLastRead);
		numBytesRead += numBytesLastRead;
	} while (numBytesRead <= numBytesToRead && numBytesLastRead > 0 && m_dataBuf[numBytesRead-1] != '\n');

	m_dataBuf[numBytesRead] = '\0'; // null terminate the string.

	// oops....we also want to eat up the '\n' and the \r'...
	if (numBytesRead > 1 && m_dataBuf[numBytesRead-2] == '\r')
		m_dataBuf[numBytesRead-2] = '\0'; // hit both cr and lf...
	else
		if (numBytesRead > 0 && (m_dataBuf[numBytesRead-1] == '\r' || m_dataBuf[numBytesRead-1] == '\n'))
			m_dataBuf[numBytesRead-1] = '\0';

	if (line)
		*line = m_dataBuf;
	return numBytesRead;
}

/*
 * Writes the data contained in dataBuffer into the current output stream. It also informs
 * the transport layer that this data is now available for transmission.
 * Returns a positive number for success, 0 for failure (not all the bytes were written to the
 * stream, etc). We need to make another pass through this file to install an error system (mscott)
 */

PRInt32 nsMailboxProtocol::SendData(const char * dataBuffer)
{
	PRUint32 writeCount = 0; 
	PRInt32 status = 0; 

	NS_PRECONDITION(m_outputStream && m_outputConsumer, "no registered consumer for our output");
	if (dataBuffer && m_outputStream)
	{
		nsresult rv = m_outputStream->Write(dataBuffer, PL_strlen(dataBuffer), &writeCount);
		if (NS_SUCCEEDED(rv) && writeCount == PL_strlen(dataBuffer))
		{
			// notify the consumer that data has arrived
			// HACK ALERT: this should really be m_runningUrl once we have NNTP url support...
			nsIInputStream *inputStream = NULL;
			m_outputStream->QueryInterface(nsIInputStream::GetIID() , (void **) &inputStream);
			if (inputStream)
			{
				m_outputConsumer->OnDataAvailable(m_runningUrl, inputStream, writeCount);
				NS_RELEASE(inputStream);
			}
			status = 1; // mscott: we need some type of MK_OK? MK_SUCCESS? Arrgghhh
		}
		else // the write failed for some reason, returning 0 trips an error by the caller
			status = 0; // mscott: again, I really want to add an error code here!!
	}

	return status;
}

PRInt32 nsMailboxProtocol::SetupReadMessage()
{
	// (1) create a temp file to write the message into. We need to do this because
	// we don't have pluggable converters yet. We want to let mkfile do the work of 
	// converting the message from RFC-822 to HTML before displaying it...
	// (2) Determine the number of bytes we are going to need to read out of the 
	// mailbox url....

	PR_Delete(MESSAGE_PATH);
	m_tempMessageFile = PR_Open(MESSAGE_PATH, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 00700);

	nsMsgKey messageKey;
	PRUint32 messageSize = 0;
	const nsFileSpec * dbFileSpec = nsnull;
	m_runningUrl->GetFilePath(&dbFileSpec);
	m_runningUrl->GetMessageKey(messageKey);
	if (dbFileSpec)
	{
		nsMailDatabase * mailDb = nsnull;
		nsIMessage * msgHdr = nsnull;
		nsMailDatabase::Open((nsFileSpec&) *dbFileSpec, PR_FALSE, &mailDb);
		if (mailDb) // did we get a db back?
		{
			mailDb->GetMsgHdrForKey(messageKey, &msgHdr);
			if (msgHdr)
			{
				msgHdr->GetMessageSize(&messageSize);
				msgHdr->Release();
			}
			mailDb->Close(PR_TRUE);
		}
	}
	m_runningUrl->SetMessageSize(messageSize);
	return NS_OK;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// Begin protocol state machine functions...
//////////////////////////////////////////////////////////////////////////////////////////////

PRInt32 nsMailboxProtocol::LoadURL(nsIURL * aURL, nsISupports * aConsumer)
{
	nsresult rv = NS_OK;
    PRInt32 status = 0; 
	nsIMailboxUrl * mailboxUrl = nsnull;
	HG77067
	if (aURL)
	{
		rv = aURL->QueryInterface(nsIMailboxUrl::GetIID(), (void **) &mailboxUrl);
		if (NS_SUCCEEDED(rv) && mailboxUrl)
		{
			NS_IF_RELEASE(m_runningUrl);
			m_runningUrl = mailboxUrl; // we have transferred ref cnt contro to m_runningUrl

			if (aConsumer)
				rv = aConsumer->QueryInterface(kIWebShell, (void **) &m_displayConsumer);

			// find out from the url what action we are supposed to perform...
			rv = m_runningUrl->GetMailboxAction(&m_mailboxAction);

			if (NS_SUCCEEDED(rv))
			{
				switch (m_mailboxAction)
				{
				case nsMailboxActionParseMailbox:
					// extract the mailbox parser..
					NS_IF_RELEASE(m_mailboxParser);
					rv = m_runningUrl->GetMailboxParser(&m_mailboxParser);
					m_nextState = MAILBOX_READ_FOLDER;
					break;

				case nsMailboxActionDisplayMessage:
					SetupReadMessage();
					// i may need to modify the url spec to remove the search part and just have a file
					// path...
					m_nextState = MAILBOX_READ_MESSAGE;
					break;

				default:
					break;
				}
			}

	
			// okay now kick us off to the next state...
			// our first state is a process state so drive the state machine...
			PRBool transportOpen = PR_FALSE;
			m_transport->IsTransportOpen(&transportOpen);
			m_urlInProgress = PR_TRUE;
			m_runningUrl->SetUrlState(PR_TRUE, NS_OK);
			if (transportOpen == PR_FALSE)
			{
				m_transport->Open(m_runningUrl);  // opening the url will cause to get notified when the connection is established
			}
			else  // the connection is already open so we should begin processing our new url...
			{
				// mscott - I think mailbox urls always come in fresh for each mailbox protocol connection
				// so we should always be calling m_transport->open(our url)....
				NS_ASSERTION(0, "I don't think we should get here for mailbox urls");
				status = ProcessMailboxState(m_runningUrl, nsnull, 0); 
			}
		} // if we received an MAILBOX url...
	} // if we received a url!

	return status;
}
	
PRInt32 nsMailboxProtocol::ReadFolderResponse(nsIInputStream * inputStream, PRUint32 length)
{
	// okay we are doing a folder read in 8K chunks of a mail folder....
	// this is almost too easy....we can just forward the data in this stream on to our
	// folder parser object!!!

	nsresult rv = NS_OK;

	if (m_mailboxParser)
		rv = m_mailboxParser->OnDataAvailable(m_runningUrl, inputStream, length); // let the parser deal with it...

	if (NS_FAILED(rv))
	{
		m_nextState = MAILBOX_ERROR_DONE; // drop out of the loop....
		return -1;
	}

	// now wait for the next 8K chunk to come in.....
	SetFlag(MAILBOX_PAUSE_FOR_READ);

	// leave our state alone so when the next chunk of the mailbox comes in we jump to this state
	// and repeat....how does this process end? Well when the file is done being read in, core net lib
	// will issue an ::OnStopBinding to us...we'll use that as our sign to drop out of this state and to
	// close the protocol instance...

	return 0; 
}

PRInt32 nsMailboxProtocol::ReadMessageResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char *line;
	PRInt32 status = 0;
	nsresult rv = NS_OK;
	char outputBuffer[OUTPUT_BUFFER_SIZE];
	do
	{
		status = ReadLine(inputStream, length, &line);
		if(status == 0)
		{
			m_nextState = MAILBOX_ERROR_DONE;
			ClearFlag(MAILBOX_PAUSE_FOR_READ);
			m_runningUrl->SetErrorMessage("error message not implemented yet");
			return status;
		}

		if (status > length)
			length = 0;
		else
			length -= status; 

		if(!line)
			return(status);  /* no line yet or error */
	
		if (!line || (line[0] == '.' && line[1] == 0))
		{
			m_nextState = MAILBOX_DONE;
			// and close the article file if it was open....
			if (m_tempMessageFile)
				PR_Close(m_tempMessageFile);

			// mscott: hack alert...now that the file is done...turn around and fire a file url 
			// to display the message....
			char * fileUrl = PR_smprintf("file:///%s", MESSAGE_PATH_URL);
			if (m_displayConsumer)
				m_displayConsumer->LoadURL(nsAutoString(fileUrl), nsnull, PR_TRUE, nsURLReload, 0);

			ClearFlag(MAILBOX_PAUSE_FOR_READ);
		} // otherwise process the line
		else
		{
			if (line[0] == '.')
				PL_strcpy (outputBuffer, line + 1);
			else
				PL_strcpy (outputBuffer, line);

			/* When we're sending this line to a converter (ie,
				it's a message/rfc822) use the local line termination
				convention, not CRLF.  This makes text articles get
				saved with the local line terminators.  Since SMTP
				and NNTP mandate the use of CRLF, it is expected that
				the local system will convert that to the local line
				terminator as it is read.
			*/
		
			PL_strcat (outputBuffer, LINEBREAK);

			if (m_tempMessageFile)
				PR_Write(m_tempMessageFile,(void *) outputBuffer,PL_strlen(outputBuffer));
		} 


	}
	while (length > 0 && status > 0);

	SetFlag(MAILBOX_PAUSE_FOR_READ); // wait for more data to become available...

	return 0;
}


/*
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
 PRInt32 nsMailboxProtocol::ProcessMailboxState(nsIURL * url, nsIInputStream * inputStream, PRUint32 length)
{
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
					status = ReadMessageResponse(inputStream, length);
				break;
			case MAILBOX_READ_FOLDER:
				if (inputStream == nsnull)
					SetFlag(MAILBOX_PAUSE_FOR_READ);   // wait for file socket to read in the next chunk...
				else
					status = ReadFolderResponse(inputStream, length);
				break;
			case MAILBOX_DONE:
			case MAILBOX_ERROR_DONE:
				m_urlInProgress = PR_FALSE;
				m_runningUrl->SetUrlState(PR_FALSE, NS_OK);
	            m_nextState = MAILBOX_FREE;
				break;
        
			case MAILBOX_FREE:
				// MAILBOX is a one time use connection so kill it if we get here...
				CloseConnection(); 
	            return(-1); /* final end */
        
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
    
    return(status);
}

PRInt32	  nsMailboxProtocol::CloseConnection()
{
	return 0;
}

