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
#include "nsIOutputStream.h"
#include "nsINetService.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgHdr.h"
#include "nsMsgLineBuffer.h"
#include "nsIMessage.h"
#include "nsMsgDBCID.h"

#include "rosetta.h"

#include "allxpstr.h"
#include "prtime.h"
#include "prlog.h"
#include "prerror.h"
#include "prprf.h"
#include "nsFileStream.h"

#define ENABLE_SMOKETEST  1

static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kIWebShell, NS_IWEB_SHELL_IID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);


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

nsMailboxProtocol::nsMailboxProtocol(nsIURL * aURL)
{
	Initialize(aURL);
}

nsMailboxProtocol::~nsMailboxProtocol()
{
	// free our local state 
	delete m_lineStreamBuffer;
}

void nsMailboxProtocol::Initialize(nsIURL * aURL)
{
	NS_PRECONDITION(aURL, "invalid URL passed into MAILBOX Protocol");
	if (aURL)
	{
		nsresult rv = aURL->QueryInterface(nsIMailboxUrl::GetIID(), (void **) getter_AddRefs(m_runningUrl));
		if (NS_SUCCEEDED(rv) && m_runningUrl)
		{
			const nsFileSpec * fileSpec = nsnull;
			m_runningUrl->GetFilePath(&fileSpec);
			rv = OpenFileSocket(aURL, fileSpec);
		}
	}

	m_lineStreamBuffer = new nsMsgLineStreamBuffer(OUTPUT_BUFFER_SIZE, MSG_LINEBREAK, PR_TRUE);

	m_nextState = MAILBOX_READ_FOLDER;
	m_initialState = MAILBOX_READ_FOLDER;

	nsFileSpec fileSpec(MESSAGE_PATH);
	NS_NewFileSpecWithSpec(fileSpec, getter_AddRefs(m_tempMessageFile));
}

/////////////////////////////////////////////////////////////////////////////////////////////
// we suppport the nsIStreamListener interface 
////////////////////////////////////////////////////////////////////////////////////////////

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
	else if(m_mailboxCopyHandler) 
		m_mailboxCopyHandler->OnStartBinding(aURL, aContentType); 

	return NS_OK;

}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsMailboxProtocol::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
	// what can we do? we can close the stream?
	nsMsgProtocol::OnStopBinding(aURL, aStatus, aMsg);

	if (m_nextState == MAILBOX_READ_FOLDER && m_mailboxParser)
	{
		// we need to inform our mailbox parser that there is no more incoming data...
		m_mailboxParser->OnStopBinding(aURL, 0, nsnull);
	}
	else if (m_mailboxCopyHandler) 
		m_mailboxCopyHandler->OnStopBinding(aURL, 0, nsnull); 
	else if (m_nextState == MAILBOX_READ_MESSAGE) 
	{
		DoneReadingMessage();
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
#ifdef ENABLE_SMOKETEST
	nsFileSpec smokeFile("MailSmokeTest.txt");
	nsOutputFileStream smokeTestFile(smokeFile, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE);
	smokeTestFile << "Mailbox Done\n";
#endif

	// when on stop binding is called, we as the protocol are done...let's close down the connection
	// releasing all of our interfaces. It's important to remember that this on stop binding call
	// is coming from netlib so they are never going to ping us again with on data available. This means
	// we'll never be going through the Process loop...
	return CloseSocket(); 
}

/////////////////////////////////////////////////////////////////////////////////////////////
// End of nsIStreamListenerSupport
//////////////////////////////////////////////////////////////////////////////////////////////

PRInt32 nsMailboxProtocol::DoneReadingMessage()
{
	nsresult rv = NS_OK;
	// and close the article file if it was open....
	if (m_tempMessageFile)
		rv = m_tempMessageFile->closeStream();

	// disply hack: run a file url on the temp file
	if (m_mailboxAction == nsMailboxActionDisplayMessage && m_displayConsumer)
	{
		nsFilePath filePath(MESSAGE_PATH);
		nsFileURL  fileURL(filePath);
		char * message_path_url = PL_strdup(fileURL.GetAsString());

		rv = m_displayConsumer->LoadURL(nsAutoString(message_path_url).GetUnicode(), nsnull, PR_TRUE, nsURLReload, 0);

		PR_FREEIF(message_path_url);

		// now mark the message as read
		nsCOMPtr<nsIMsgDBHdr> msgHdr;

		rv = m_runningUrl->GetMessageHeader(getter_AddRefs(msgHdr));
		if (NS_SUCCEEDED(rv))
			msgHdr->MarkRead(PR_TRUE);
	}

	return rv;
}

PRInt32 nsMailboxProtocol::SetupMessageExtraction()
{
	// Determine the number of bytes we are going to need to read out of the 
	// mailbox url....
	nsCOMPtr<nsIMsgDBHdr> msgHdr;
	nsresult rv = NS_OK;
	
	rv = m_runningUrl->GetMessageHeader(getter_AddRefs(msgHdr));
	if (NS_SUCCEEDED(rv))
	{
		PRUint32 messageSize = 0;
		msgHdr->GetMessageSize(&messageSize);
		m_runningUrl->SetMessageSize(messageSize);
	}
	
	return rv;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Begin protocol state machine functions...
//////////////////////////////////////////////////////////////////////////////////////////////

nsresult nsMailboxProtocol::LoadUrl(nsIURL * aURL, nsISupports * aConsumer)
{
	nsresult rv = NS_OK;
	HG77067
	if (aURL)
	{
		m_runningUrl = do_QueryInterface(aURL);
		if (m_runningUrl)
		{
			if (aConsumer)
				m_displayConsumer = do_QueryInterface(aConsumer);

			// find out from the url what action we are supposed to perform...
			rv = m_runningUrl->GetMailboxAction(&m_mailboxAction);

			if (NS_SUCCEEDED(rv))
			{
				switch (m_mailboxAction)
				{
				case nsMailboxActionParseMailbox:
					// extract the mailbox parser..
					rv = m_runningUrl->GetMailboxParser(getter_AddRefs(m_mailboxParser));
					m_nextState = MAILBOX_READ_FOLDER;
					break;
				case nsMailboxActionSaveMessageToDisk:
					// ohhh, display message already writes a msg to disk (as part of a hack)
					// so we can piggy back off of that!! We just need to change m_tempMessageFile
					// to be the name of our save message to disk file. Since save message to disk
					// urls are run without a webshell to display the msg into, we won't be trying
					// to display the message after we write it to disk...
					m_runningUrl->GetMessageFile(getter_AddRefs(m_tempMessageFile));
				case nsMailboxActionDisplayMessage:
					// create a temp file to write the message into. We need to do this because
					// we don't have pluggable converters yet. We want to let mkfile do the work of 
					// converting the message from RFC-822 to HTML before displaying it...
				
					m_tempMessageFile->openStreamForWriting();
					SetupMessageExtraction();
					m_nextState = MAILBOX_READ_MESSAGE;
					break;

				case nsMailboxActionCopyMessage:
				case nsMailboxActionMoveMessage:
					rv = m_runningUrl->GetMailboxCopyHandler(getter_AddRefs(m_mailboxCopyHandler));
					SetupMessageExtraction();
					m_nextState = MAILBOX_READ_MESSAGE;
					break;

				default:
					break;
				}
			}

			rv = nsMsgProtocol::LoadUrl(aURL);

		} // if we received an MAILBOX url...
	} // if we received a url!

	return rv;
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
	char *line = nsnull;
	PRUint32 status = 0;
	nsresult rv = NS_OK;

	// if we are doing a move or a copy, forward the data onto the copy handler...
	// if we want to display the message then parse the incoming data...

	if (m_mailboxAction == nsMailboxActionCopyMessage || m_mailboxAction == nsMailboxActionMoveMessage) 
	{
		if (m_mailboxCopyHandler)
			rv = m_mailboxCopyHandler->OnDataAvailable(m_runningUrl, inputStream, length);
	}
	else
	{
		PRBool pauseForMoreData = PR_FALSE;
		do
		{
			line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);
		
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

				if (m_tempMessageFile)
				{
					PRInt32 count = 0;
					if (line)
						m_tempMessageFile->write(line, PL_strlen(line), &count);
					m_tempMessageFile->write(MSG_LINEBREAK, MSG_LINEBREAK_LEN, &count);
				}
			} 
		}
		while (line && !pauseForMoreData);
	}

	SetFlag(MAILBOX_PAUSE_FOR_READ); // wait for more data to become available...

	return 0;
}


/*
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
nsresult nsMailboxProtocol::ProcessProtocolState(nsIURL * url, nsIInputStream * inputStream, PRUint32 length)
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
				m_runningUrl->SetUrlState(PR_FALSE, NS_OK);
	            m_nextState = MAILBOX_FREE;
				break;
        
			case MAILBOX_FREE:
				// MAILBOX is a one time use connection so kill it if we get here...
				CloseSocket(); 
	            return NS_OK; /* final end */
        
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
    
    return NS_OK;
}

nsresult nsMailboxProtocol::CloseSocket()
{
	// how do you force a release when closing the connection??
	nsMsgProtocol::CloseSocket(); 
	m_runningUrl = null_nsCOMPtr();
	m_mailboxParser = null_nsCOMPtr();
	return 0;
}
