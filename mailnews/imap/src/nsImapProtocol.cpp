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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"  // for pre-compiled headers
#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsImapCore.h"
#include "nsImapProtocol.h"
#include "nscore.h"
#include "nsImapProxyEvent.h"
#include "nsIMAPHostSessionList.h"
#include "nsIMAPBodyShell.h"
#include "nsImapServerResponseParser.h"
#include "nspr.h"

PRLogModuleInfo *IMAP;

// netlib required files
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsINetService.h"

#include "nsString2.h"

#include "nsIMsgIncomingServer.h"

#define ONE_SECOND ((PRUint32)1000)    // one second

static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

#define OUTPUT_BUFFER_SIZE (4096*2) // mscott - i should be able to remove this if I can use nsMsgLineBuffer???

// **************?????***********????*************????***********************
// ***** IMPORTANT **** jefft -- this is a temporary implementation for the
// testing purpose. Eventually, we will have a host service object in
// controlling the host session list.
// Remove the following when the host service object is in place.
// **************************************************************************
extern nsIMAPHostSessionList*gImapHostSessionList;

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_THREADSAFE_ADDREF(nsImapProtocol)
NS_IMPL_THREADSAFE_RELEASE(nsImapProtocol)

NS_IMETHODIMP nsImapProtocol::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr)
    return NS_ERROR_NULL_POINTER;
        
  *aInstancePtr = NULL;
                                                                         
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID); 
  static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID); 

  if (aIID.Equals(nsIStreamListener::GetIID())) 
  {
	  *aInstancePtr = (nsIStreamListener *) this;                                                   
	  NS_ADDREF_THIS();
	  return NS_OK;
  }
  if (aIID.Equals(nsIImapProtocol::GetIID()))
  {
	  *aInstancePtr = (nsIImapProtocol *) this;
	  NS_ADDREF_THIS();
	  return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) 
  {
	  *aInstancePtr = (void*) ((nsISupports*)this);
	  NS_ADDREF_THIS();
    return NS_OK;                                                        
  }                                                                      
  
  if (aIID.Equals(kIsThreadsafeIID)) 
  {
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsImapProtocol::nsImapProtocol() : 
    m_parser(*this), m_currentCommand(eOneByte, 0)
{
	NS_INIT_REFCNT();
	m_runningUrl = nsnull; // initialize to NULL
	m_transport = nsnull;
	m_outputStream = nsnull;
	m_outputConsumer = nsnull;
	m_urlInProgress = PR_FALSE;
	m_socketIsOpen = PR_FALSE;
	m_dataBuf = nsnull;
    m_connectionStatus = 0;
    
    // ***** Thread support *****
    m_sinkEventQueue = nsnull;
    m_eventQueue = nsnull;
    m_thread = nsnull;
    m_dataAvailableMonitor = nsnull;
	m_pseudoInterruptMonitor = nsnull;
	m_dataMemberMonitor = nsnull;
	m_threadDeathMonitor = nsnull;
    m_eventCompletionMonitor = nsnull;
    m_imapThreadIsRunning = PR_FALSE;
    m_consumer = nsnull;
	m_currentServerCommandTagNumber = 0;
	m_active = PR_FALSE;
	m_threadShouldDie = PR_FALSE;
	m_pseudoInterrupted = PR_FALSE;

    // imap protocol sink interfaces
    m_server = nsnull;
    m_imapLog = nsnull;
    m_imapMailfolder = nsnull;
    m_imapMessage = nsnull;
    m_imapExtension = nsnull;
    m_imapMiscellaneous = nsnull;

	// where should we do this? Perhaps in the factory object?
	if (!IMAP)
		IMAP = PR_NewLogModule("IMAP");
}

nsresult nsImapProtocol::Initialize(nsIImapHostSessionList * aHostSessionList, PLEventQueue * aSinkEventQueue)
{
	NS_PRECONDITION(aSinkEventQueue, "oops...trying to initalize with a null sink event queue!");
	if (!aSinkEventQueue)
        return NS_ERROR_NULL_POINTER;

    m_sinkEventQueue = aSinkEventQueue;
	return NS_OK;
}

nsImapProtocol::~nsImapProtocol()
{
	// free handles on all networking objects...
	NS_IF_RELEASE(m_outputStream); 
	NS_IF_RELEASE(m_outputConsumer);
	NS_IF_RELEASE(m_transport);
    NS_IF_RELEASE(m_server);
    NS_IF_RELEASE(m_imapLog);
    NS_IF_RELEASE(m_imapMailfolder);
    NS_IF_RELEASE(m_imapMessage);
    NS_IF_RELEASE(m_imapExtension);
    NS_IF_RELEASE(m_imapMiscellaneous);

	PR_FREEIF(m_dataBuf);

    // **** We must be out of the thread main loop function
    NS_ASSERTION(m_imapThreadIsRunning == PR_FALSE, 
                 "Oops, thread is still running.\n");
    if (m_eventQueue)
    {
        PL_DestroyEventQueue(m_eventQueue);
        m_eventQueue = nsnull;
    }

    if (m_dataAvailableMonitor)
    {
        PR_DestroyMonitor(m_dataAvailableMonitor);
        m_dataAvailableMonitor = nsnull;
    }
	if (m_pseudoInterruptMonitor)
	{
		PR_DestroyMonitor(m_pseudoInterruptMonitor);
		m_pseudoInterruptMonitor = nsnull;
	}
	if (m_dataMemberMonitor)
	{
		PR_DestroyMonitor(m_dataMemberMonitor);
		m_dataMemberMonitor = nsnull;
	}
	if (m_threadDeathMonitor)
	{
		PR_DestroyMonitor(m_threadDeathMonitor);
		m_threadDeathMonitor = nsnull;
	}
    if (m_eventCompletionMonitor)
    {
        PR_DestroyMonitor(m_eventCompletionMonitor);
        m_eventCompletionMonitor = nsnull;
    }
}

const char*
nsImapProtocol::GetImapHostName()
{
    const char* hostName = nsnull;
	// mscott - i wonder if we should be getting the host name from the url
	// or from m_server....
    if (m_runningUrl)
        m_runningUrl->GetHost(&hostName);
    return hostName;
}

const char*
nsImapProtocol::GetImapUserName()
{
    char* userName = nsnull;
	nsIMsgIncomingServer * server = m_server;
    if (server)
		server->GetUserName(&userName);
    return userName;
}

void
nsImapProtocol::SetupSinkProxy()
{
    if (m_runningUrl)
    {
        NS_ASSERTION(m_sinkEventQueue && m_thread, 
                     "fatal... null sink event queue or thread");
        nsresult res;

        if (!m_server)
            m_runningUrl->GetServer(&m_server);
        if (!m_imapLog)
        {
            nsIImapLog *aImapLog;
            res = m_runningUrl->GetImapLog(&aImapLog);
            if (NS_SUCCEEDED(res) && aImapLog)
            {
                m_imapLog = new nsImapLogProxy (aImapLog, this,
                                                m_sinkEventQueue, m_thread);
                NS_IF_ADDREF (m_imapLog);
                NS_RELEASE (aImapLog);
            }
        }
                
        if (!m_imapMailfolder)
        {
            nsIImapMailfolder *aImapMailfolder;
            res = m_runningUrl->GetImapMailfolder(&aImapMailfolder);
            if (NS_SUCCEEDED(res) && aImapMailfolder)
            {
                m_imapMailfolder = new nsImapMailfolderProxy(aImapMailfolder,
                                                             this,
                                                             m_sinkEventQueue,
                                                             m_thread);
                NS_IF_ADDREF(m_imapMailfolder);
                NS_RELEASE(aImapMailfolder);
            }
        }
        if (!m_imapMessage)
        {
            nsIImapMessage *aImapMessage;
            res = m_runningUrl->GetImapMessage(&aImapMessage);
            if (NS_SUCCEEDED(res) && aImapMessage)
            {
                m_imapMessage = new nsImapMessageProxy(aImapMessage,
                                                       this,
                                                       m_sinkEventQueue,
                                                       m_thread);
                NS_IF_ADDREF (m_imapMessage);
                NS_RELEASE(aImapMessage);
            }
        }
        if (!m_imapExtension)
        {
            nsIImapExtension *aImapExtension;
            res = m_runningUrl->GetImapExtension(&aImapExtension);
            if(NS_SUCCEEDED(res) && aImapExtension)
            {
                m_imapExtension = new nsImapExtensionProxy(aImapExtension,
                                                           this,
                                                           m_sinkEventQueue,
                                                           m_thread);
                NS_IF_ADDREF(m_imapExtension);
                NS_RELEASE(aImapExtension);
            }
        }
        if (!m_imapMiscellaneous)
        {
            nsIImapMiscellaneous *aImapMiscellaneous;
            res = m_runningUrl->GetImapMiscellaneous(&aImapMiscellaneous);
            if (NS_SUCCEEDED(res) && aImapMiscellaneous)
            {
                m_imapMiscellaneous = new
                    nsImapMiscellaneousProxy(aImapMiscellaneous,
                                             this,
                                             m_sinkEventQueue,
                                             m_thread);
                NS_IF_ADDREF(m_imapMiscellaneous);
                NS_RELEASE(aImapMiscellaneous);
            }
        }
    }
}

void nsImapProtocol::SetupWithUrl(nsIURL * aURL)
{
	NS_PRECONDITION(aURL, "null URL passed into Imap Protocol");

	m_flags = 0;

	// query the URL for a nsIImapUrl
	m_runningUrl = nsnull; // initialize to NULL
	m_transport = nsnull;

	if (aURL)
	{
		nsresult rv = aURL->QueryInterface(nsIImapUrl::GetIID(), (void **)&m_runningUrl);
		if (NS_SUCCEEDED(rv) && m_runningUrl)
		{
			// extract the file name and create a file transport...
			const char * hostName = nsnull;
			PRUint32 port = IMAP_PORT;

			m_runningUrl->GetHost(&hostName);
			m_runningUrl->GetHostPort(&port);

			/*JT - Should go away when netlib registers itself! */
			nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, 
												  "netlib.dll", PR_FALSE, PR_FALSE); 
            nsINetService* pNetService;
            rv = nsServiceManager::GetService(kNetServiceCID,
                                              nsINetService::GetIID(),
                                              (nsISupports**)&pNetService);
			if (NS_SUCCEEDED(rv) && pNetService)
			{
				rv = pNetService->CreateSocketTransport(&m_transport, port, hostName);
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
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register Imap instance as a consumer on the socket");

	m_urlInProgress = PR_FALSE;
	m_socketIsOpen = PR_FALSE;

	// m_dataBuf is used by ReadLine and SendData
	m_dataBuf = (char *) PR_Malloc(sizeof(char) * OUTPUT_BUFFER_SIZE);
	m_allocatedSize = OUTPUT_BUFFER_SIZE;

    // ******* Thread support *******
    if (m_thread == nsnull)
    {
        m_dataAvailableMonitor = PR_NewMonitor();
		m_pseudoInterruptMonitor = PR_NewMonitor();
		m_dataMemberMonitor = PR_NewMonitor();
		m_threadDeathMonitor = PR_NewMonitor();
        m_eventCompletionMonitor = PR_NewMonitor();

        m_thread = PR_CreateThread(PR_USER_THREAD, ImapThreadMain, (void*)
                                   this, PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
                                   PR_UNJOINABLE_THREAD, 0);
        NS_ASSERTION(m_thread, "Unable to create imap thread.\n");
    }

    // *** setting up the sink proxy if needed
    SetupSinkProxy();
}

void
nsImapProtocol::ImapThreadMain(void *aParm)
{
    nsImapProtocol *me = (nsImapProtocol *) aParm;
    NS_ASSERTION(me, "Yuk, me is null.\n");
    
    PR_CEnterMonitor(aParm);
    NS_ASSERTION(me->m_imapThreadIsRunning == PR_FALSE, 
                 "Oh. oh. thread is already running. What's wrong here?");
    if (me->m_imapThreadIsRunning)
    {
        PR_CExitMonitor(me);
        return;
    }
    me->m_eventQueue = PL_CreateEventQueue("ImapProtocolThread",
                                       PR_GetCurrentThread());
    NS_ASSERTION(me->m_eventQueue, 
                 "Unable to create imap thread event queue.\n");
    if (!me->m_eventQueue)
    {
        PR_CExitMonitor(me);
        return;
    }
    me->m_imapThreadIsRunning = PR_TRUE;
    PR_CExitMonitor(me);

    // call the platform specific main loop ....
    me->ImapThreadMainLoop();

    PR_DestroyEventQueue(me->m_eventQueue);
    me->m_eventQueue = nsnull;

    // ***** Important need to remove the connection out from the connection
    // pool - nsImapService **********
    delete me;
}

PRBool
nsImapProtocol::ImapThreadIsRunning()
{
    PRBool retValue = PR_FALSE;
    PR_CEnterMonitor(this);
    retValue = m_imapThreadIsRunning;
    PR_CExitMonitor(this);
    return retValue;
}

NS_IMETHODIMP
nsImapProtocol::GetThreadEventQueue(PLEventQueue **aEventQueue)
{
    // *** should subclassing PLEventQueue and ref count it ***
    // *** need to find a way to prevent dangling pointer ***
    // *** a callback mechanism or a semaphor control thingy ***
    PR_CEnterMonitor(this);
    if (aEventQueue)
        *aEventQueue = m_eventQueue;
    PR_CExitMonitor(this);
    return NS_OK;
}

NS_IMETHODIMP 
nsImapProtocol::SetMessageDownloadOutputStream(nsIOutputStream *aOutputStream)
{
    NS_PRECONDITION(aOutputStream, "Yuk, null output stream");
    if (!aOutputStream)
        return NS_ERROR_NULL_POINTER;
    PR_CEnterMonitor(this);
    if(m_messageDownloadOutputStream)
        NS_RELEASE(m_messageDownloadOutputStream);
    m_messageDownloadOutputStream = aOutputStream;
    NS_ADDREF(m_messageDownloadOutputStream);
    PR_CExitMonitor(this);
    return NS_OK;
}

NS_IMETHODIMP 
nsImapProtocol::NotifyFEEventCompletion()
{
    PR_EnterMonitor(m_eventCompletionMonitor);
    PR_Notify(m_eventCompletionMonitor);
    PR_ExitMonitor(m_eventCompletionMonitor);
    return NS_OK;
}

void
nsImapProtocol::WaitForFEEventCompletion()
{
    PR_EnterMonitor(m_eventCompletionMonitor);
    PR_Wait(m_eventCompletionMonitor, PR_INTERVAL_NO_TIMEOUT);
    PR_ExitMonitor(m_eventCompletionMonitor);
}

void
nsImapProtocol::ImapThreadMainLoop()
{
    // ****** please implement PR_LOG 'ing ******
    while (ImapThreadIsRunning())
    {
        PR_EnterMonitor(m_dataAvailableMonitor);

        PR_Wait(m_dataAvailableMonitor, PR_INTERVAL_NO_TIMEOUT);

        ProcessCurrentURL();

        PR_ExitMonitor(m_dataAvailableMonitor);
    }
}

void
nsImapProtocol::ProcessCurrentURL()
{
    nsresult res;

    if (!m_urlInProgress)
    {
        // **** we must be just successfully connected to the sever; we
        // haven't got a chance to run the url yet; let's call load the url
        // again
        res = LoadUrl((nsIURL*)m_runningUrl, m_consumer);
        return;
    }
    else 
    {
        GetServerStateParser().ParseIMAPServerResponse(m_currentCommand.GetBuffer());
        // **** temporary for now
        if (m_imapLog)
        {
            m_imapLog->HandleImapLogData(m_dataBuf);
            // WaitForFEEventCompletion();

            // we are done running the imap log url so mark the url as done...
            // set change in url state...
            m_runningUrl->SetUrlState(PR_FALSE, NS_OK); 
        }
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////
// we suppport the nsIStreamListener interface 
////////////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsImapProtocol::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
{
    PR_CEnterMonitor(this);

	// we would read a line from the stream and then parse it.....I think this function can
	// effectively replace ReadLineFromSocket...
    nsIImapUrl *aImapUrl;
    nsresult res = aURL->QueryInterface(nsIImapUrl::GetIID(),
                                        (void**)&aImapUrl);
    if (aLength >= m_allocatedSize)
    {
        m_dataBuf = (char*) PR_Realloc(m_dataBuf, aLength+1);
        if (m_dataBuf)
            m_allocatedSize = aLength+1;
        else
            m_allocatedSize = 0;
    }

    if(NS_SUCCEEDED(res))
    {
        NS_PRECONDITION( m_runningUrl->Equals(aImapUrl), 
                         "Oops... running a different imap url. Hmmm...");
            
        res = aIStream->Read(m_dataBuf, aLength, &m_totalDataSize);
        if (NS_SUCCEEDED(res))
        {
            m_dataBuf[m_totalDataSize] = 0;
            m_curReadIndex = 0;
			PR_EnterMonitor(m_dataAvailableMonitor);
            PR_Notify(m_dataAvailableMonitor);
			PR_ExitMonitor(m_dataAvailableMonitor);
        }
        NS_RELEASE(aImapUrl);
    }

    PR_CExitMonitor(this);

	return res;
}

NS_IMETHODIMP nsImapProtocol::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
	m_runningUrl->SetUrlState(PR_TRUE, NS_OK);
	return NS_OK;
}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsImapProtocol::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
	m_runningUrl->SetUrlState(PR_FALSE, aStatus); // set change in url state...
	return NS_OK;

}

/////////////////////////////////////////////////////////////////////////////////////////////
// End of nsIStreamListenerSupport
//////////////////////////////////////////////////////////////////////////////////////////////

PRInt32 nsImapProtocol::ReadLine(nsIInputStream * inputStream, PRUint32 length, char ** line)
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

PRInt32 nsImapProtocol::SendData(const char * dataBuffer)
{
	PRUint32 writeCount = 0; 
	PRInt32 status = 0; 

	NS_PRECONDITION(m_outputStream && m_outputConsumer, "no registered consumer for our output");
	if (dataBuffer && m_outputStream)
	{
        m_currentCommand = dataBuffer;
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

/////////////////////////////////////////////////////////////////////////////////////////////
// Begin protocol state machine functions...
//////////////////////////////////////////////////////////////////////////////////////////////

nsresult nsImapProtocol::LoadUrl(nsIURL * aURL, nsISupports * aConsumer)
{
	nsresult rv = NS_OK;
	nsIImapUrl * imapUrl = nsnull;
	if (aURL)
	{
		// mscott: I used to call SetupWithUrl from the constructor but we really need the url
		// in order to properly intialize the protocol instance with a transport / host / port
		// etc. So I'm going to experiment by saying every time we load a Url, we look to see
		// if we need to initialize the protocol....I really need to check to make sure the host
		// and port haven't changed if they have then we need to re-initialize in order to pick up
		// a new connection to the imap server...

		if (m_transport == nsnull) // i.e. we haven't been initialized yet....
			SetupWithUrl(aURL);
		
		if (m_transport && m_runningUrl)
		{
			PRBool transportOpen = PR_FALSE;
			m_transport->IsTransportOpen(&transportOpen);
			if (transportOpen == PR_FALSE)
			{
                // m_urlInProgress = PR_TRUE;
				rv = m_transport->Open(m_runningUrl);  // opening the url will cause to get notified when the connection is established
			}
			else  // the connection is already open so we should begin processing our new url...
			{
				// mscott - I think Imap urls always come in fresh for each Imap protocol connection
				// so we should always be calling m_transport->open(our url)....
				// NS_ASSERTION(0, "I don't think we should get here for imap
                // urls");
                // ********** jefft ********* okay let's use ? search string
                // for passing the raw command now.
                m_urlInProgress = PR_TRUE;
                const char *search = nsnull;
                aURL->GetSearch(&search);
                char *tmpBuffer = nsnull;
                if (search && PL_strlen(search))
                {
                    tmpBuffer = PR_smprintf("%s\r\n", search);
                    if (tmpBuffer)
                    {
                        SendData(tmpBuffer);
                        PR_Free(tmpBuffer);
                    }
                }
			}
		} // if we have an imap url and a transport
        if (aConsumer)
            m_consumer = aConsumer;
	} // if we received a url!

	return rv;
}

// ***** Beginning of ported stuf from 4.5 *******

// Command tag handling stuff
void nsImapProtocol::IncrementCommandTagNumber()
{
    sprintf(m_currentServerCommandTag,"%ld", (long) ++m_currentServerCommandTagNumber);
}

char *nsImapProtocol::GetServerCommandTag()
{
    return m_currentServerCommandTag;
}

void nsImapProtocol::BeginMessageDownLoad(
                                      PRUint32 total_message_size, // for user, headers and body
                                      const char *content_type)
{
	char *sizeString = PR_smprintf("OPEN Size: %ld", total_message_size);
	Log("STREAM",sizeString,"Begin Message Download Stream");
	PR_FREEIF(sizeString);
#if 0 // here's the old code ...
	//PR_LOG(IMAP, out, ("STREAM: Begin Message Download Stream.  Size: %ld", total_message_size));
	StreamInfo *si = (StreamInfo *) PR_Malloc (sizeof (StreamInfo));		// This will be freed in the event
	if (si)
	{
		si->size = total_message_size;
		si->content_type = PL_strdup(content_type);
		if (si->content_type)
		{
			TImapFEEvent *setupStreamEvent = 
				new TImapFEEvent(SetupMsgWriteStream,   // function to call
								this,                                   // access to current entry
								(void *) si,
								PR_TRUE);		// ok to run when interrupted because si is FREE'd in the event

			if (setupStreamEvent)
			{
				fFEEventQueue->AdoptEventToEnd(setupStreamEvent);
				WaitForFEEventCompletion();
			}
			else
				HandleMemoryFailure();
			fFromHeaderSeen = PR_FALSE;
		}
		else
			HandleMemoryFailure();
	}
	else
		HandleMemoryFailure();
#endif
}



// this routine is used to fetch a message or messages, or headers for a message...
void nsImapProtocol::FetchTryChunking(const char *messageIds,
                                            nsIMAPeFetchFields whatToFetch,
                                            PRBool idIsUid,
											char *part,
											PRUint32 downloadSize)
{
#if 0
	GetServerStateParser().SetTotalDownloadSize(downloadSize);
	if (fFetchByChunks && GetServerStateParser().ServerHasIMAP4Rev1Capability() &&
		(downloadSize > (PRUint32) fChunkThreshold))
	{
		PRUint32 startByte = 0;
		GetServerStateParser().ClearLastFetchChunkReceived();
		while (!DeathSignalReceived() && !GetPseudoInterrupted() && 
			!GetServerStateParser().GetLastFetchChunkReceived() &&
			GetServerStateParser().ContinueParse())
		{
			PRUint32 sizeToFetch = startByte + fChunkSize > downloadSize ?
				downloadSize - startByte : fChunkSize;
			FetchMessage(messageIds, 
						 whatToFetch,
						 idIsUid,
						 startByte, sizeToFetch,
						 part);
			startByte += sizeToFetch;
		}

		// Only abort the stream if this is a normal message download
		// Otherwise, let the body shell abort the stream.
		if ((whatToFetch == kEveryThingRFC822)
			&&
			((startByte > 0 && (startByte < downloadSize) &&
			(DeathSignalReceived() || GetPseudoInterrupted())) ||
			!GetServerStateParser().ContinueParse()))
		{
			AbortMessageDownLoad();
			PseudoInterrupt(FALSE);
		}
	}
	else
	{
		// small message, or (we're not chunking and not doing bodystructure),
		// or the server is not rev1.
		// Just fetch the whole thing.
		FetchMessage(messageIds, whatToFetch,idIsUid, 0, 0, part);
	}
#endif
}


void nsImapProtocol::PipelinedFetchMessageParts(const char *uid, nsIMAPMessagePartIDArray *parts)
{
	// assumes no chunking

	// build up a string to fetch
	nsString2 stringToFetch;
	nsString2 what;

	int32 currentPartNum = 0;
	while ((parts->GetNumParts() > currentPartNum) && !DeathSignalReceived())
	{
		nsIMAPMessagePartID *currentPart = parts->GetPart(currentPartNum);
		if (currentPart)
		{
			// Do things here depending on the type of message part
			// Append it to the fetch string
			if (currentPartNum > 0)
				stringToFetch += " ";

			switch (currentPart->GetFields())
			{
			case kMIMEHeader:
				what = "BODY[";
				what += currentPart->GetPartNumberString();
				what += ".MIME]";
				stringToFetch += what;
				break;
			case kRFC822HeadersOnly:
				if (currentPart->GetPartNumberString())
				{
					what = "BODY[";
					what += currentPart->GetPartNumberString();
					what += ".HEADER]";
					stringToFetch += what;
				}
				else
				{
					// headers for the top-level message
					stringToFetch += "BODY[HEADER]";
				}
				break;
			default:
				NS_ASSERTION(FALSE, "we should only be pipelining MIME headers and Message headers");
				break;
			}

		}
		currentPartNum++;
	}

	// Run the single, pipelined fetch command
	if ((parts->GetNumParts() > 0) && !DeathSignalReceived() && !GetPseudoInterrupted() && stringToFetch)
	{
	    IncrementCommandTagNumber();

		char *commandString = PR_smprintf("%s UID fetch %s (%s)%s", GetServerCommandTag(), uid, stringToFetch, CRLF);
#if 0	// ### DMB - hook up writing commands and parsing response.
		if (commandString)
		{
			int ioStatus = WriteLineToSocket(commandString);
			ParseIMAPandCheckForNewMail(commandString);
			PR_Free(commandString);
		}
		else
			HandleMemoryFailure();
#endif // 0
		PR_Free(stringToFetch);
	}
}


// well, this is what the old code used to look like to handle a line seen by the parser.
// I'll leave it mostly #ifdef'ed out, but I suspect it will look a lot like this.
// Perhaps we'll send the line to a messageSink...
void nsImapProtocol::HandleMessageDownLoadLine(const char *line, PRBool chunkEnd)
{
    // when we duplicate this line, whack it into the native line
    // termination.  Do not assume that it really ends in CRLF
    // to start with, even though it is supposed to be RFC822

	// If we are fetching by chunks, we can make no assumptions about
	// the end-of-line terminator, and we shouldn't mess with it.
    
    // leave enough room for two more chars. (CR and LF)
    char *localMessageLine = (char *) PR_Malloc(strlen(line) + 3);
    if (localMessageLine)
        strcpy(localMessageLine,line);
    char *endOfLine = localMessageLine + strlen(localMessageLine);

	if (!chunkEnd)
	{
#if (LINEBREAK_LEN == 1)
		if ((endOfLine - localMessageLine) >= 2 &&
			 endOfLine[-2] == CR &&
			 endOfLine[-1] == LF)
			{
			  /* CRLF -> CR or LF */
			  endOfLine[-2] = LINEBREAK[0];
			  endOfLine[-1] = '\0';
			}
		  else if (endOfLine > localMessageLine + 1 &&
				   endOfLine[-1] != LINEBREAK[0] &&
			   ((endOfLine[-1] == CR) || (endOfLine[-1] == LF)))
			{
			  /* CR -> LF or LF -> CR */
			  endOfLine[-1] = LINEBREAK[0];
			}
		else // no eol characters at all
		  {
		    endOfLine[0] = LINEBREAK[0]; // CR or LF
		    endOfLine[1] = '\0';
		  }
#else
		  if (((endOfLine - localMessageLine) >= 2 && endOfLine[-2] != CR) ||
			   ((endOfLine - localMessageLine) >= 1 && endOfLine[-1] != LF))
			{
			  if ((endOfLine[-1] == CR) || (endOfLine[-1] == LF))
			  {
				  /* LF -> CRLF or CR -> CRLF */
				  endOfLine[-1] = LINEBREAK[0];
				  endOfLine[0]  = LINEBREAK[1];
				  endOfLine[1]  = '\0';
			  }
			  else	// no eol characters at all
			  {
				  endOfLine[0] = LINEBREAK[0];	// CR
				  endOfLine[1] = LINEBREAK[1];	// LF
				  endOfLine[2] = '\0';
			  }
			}
#endif
	}
#if 0
	const char *xSenderInfo = GetServerStateParser().GetXSenderInfo();

	if (xSenderInfo && *xSenderInfo && !fFromHeaderSeen)
	{
		if (!PL_strncmp("From: ", localMessageLine, 6))
		{
			fFromHeaderSeen = TRUE;
			if (PL_strstr(localMessageLine, xSenderInfo) != NULL)
				AddXMozillaStatusLine(0);
			GetServerStateParser().FreeXSenderInfo();
		}
	}
    // if this line is for a different message, or the incoming line is too big
    if (((fDownLoadLineCache.CurrentUID() != GetServerStateParser().CurrentResponseUID()) && !fDownLoadLineCache.CacheEmpty()) ||
        (fDownLoadLineCache.SpaceAvailable() < (PL_strlen(localMessageLine) + 1)) )
    {
		if (!fDownLoadLineCache.CacheEmpty())
		{
			msg_line_info *downloadLineDontDelete = fDownLoadLineCache.GetCurrentLineInfo();
			PostLineDownLoadEvent(downloadLineDontDelete);
		}
		fDownLoadLineCache.ResetCache();
	}
     
    // so now the cache is flushed, but this string might still be to big
    if (fDownLoadLineCache.SpaceAvailable() < (PL_strlen(localMessageLine) + 1) )
    {
        // has to be dynamic to pass to other win16 thread
		msg_line_info *downLoadInfo = (msg_line_info *) PR_Malloc(sizeof(msg_line_info));
        if (downLoadInfo)
        {
          	downLoadInfo->adoptedMessageLine = localMessageLine;
          	downLoadInfo->uidOfMessage = GetServerStateParser().CurrentResponseUID();
          	PostLineDownLoadEvent(downLoadInfo);
          	if (!DeathSignalReceived())
          		PR_Free(downLoadInfo);
          	else
          	{
          		// this is very rare, interrupt while waiting to display a huge single line
          		// Net_InterruptIMAP will read this line so leak the downLoadInfo
          		
          		// set localMessageLine to NULL so the FREEIF( localMessageLine) leaks also
          		localMessageLine = NULL;
          	}
        }
	}
    else
		fDownLoadLineCache.CacheLine(localMessageLine, GetServerStateParser().CurrentResponseUID());
#endif // 1
	PR_FREEIF( localMessageLine);
}



void nsImapProtocol::NormalMessageEndDownload()
{
	Log("STREAM", "CLOSE", "Normal Message End Download Stream");
#if 0
	if (fTrackingTime)
		AdjustChunkSize();
	if (!fDownLoadLineCache.CacheEmpty())
	{
	    msg_line_info *downloadLineDontDelete = fDownLoadLineCache.GetCurrentLineInfo();
	    PostLineDownLoadEvent(downloadLineDontDelete);
	    fDownLoadLineCache.ResetCache();
    }

    TImapFEEvent *endEvent = 
        new TImapFEEvent(NormalEndMsgWriteStream,	// function to call
                        this,						// access to current entry
                        nil,						// unused
						TRUE);

    if (endEvent)
        fFEEventQueue->AdoptEventToEnd(endEvent);
    else
        HandleMemoryFailure();
#endif // 0
}

void nsImapProtocol::AbortMessageDownLoad()
{
	Log("STREAM", "CLOSE", "Abort Message  Download Stream");
#if 0
	//PR_LOG(IMAP, out, ("STREAM: Abort Message Download Stream"));
	if (fTrackingTime)
		AdjustChunkSize();
	if (!fDownLoadLineCache.CacheEmpty())
	{
	    msg_line_info *downloadLineDontDelete = fDownLoadLineCache.GetCurrentLineInfo();
	    PostLineDownLoadEvent(downloadLineDontDelete);
	    fDownLoadLineCache.ResetCache();
    }

    TImapFEEvent *endEvent = 
        new TImapFEEvent(AbortMsgWriteStream,	// function to call
                        this,					// access to current entry
                        nil,					// unused
						TRUE);

    if (endEvent)
        fFEEventQueue->AdoptEventToEnd(endEvent);
    else
        HandleMemoryFailure();
#endif // 0
}


// log info including current state...
void nsImapProtocol::Log(const char *logSubName, const char *extraInfo, const char *logData)
{
	static char *nonAuthStateName = "NA";
	static char *authStateName = "A";
	static char *selectedStateName = "S";
	static char *waitingStateName = "W";
	char *stateName = NULL;
#if 0
	switch (GetServerStateParser().GetIMAPstate())
	{
	case TImapServerState::kFolderSelected:
		if (fCurrentUrl)
		{
			if (extraInfo)
				PR_LOG(IMAP, out, ("%s:%s-%s:%s:%s: %s", fCurrentUrl->GetUrlHost(),selectedStateName, GetServerStateParser().GetSelectedMailboxName(), logSubName, extraInfo, logData));
			else
				PR_LOG(IMAP, out, ("%s:%s-%s:%s: %s", fCurrentUrl->GetUrlHost(),selectedStateName, GetServerStateParser().GetSelectedMailboxName(), logSubName, logData));
		}
		return;
		break;
	case TImapServerState::kNonAuthenticated:
		stateName = nonAuthStateName;
		break;
	case TImapServerState::kAuthenticated:
		stateName = authStateName;
		break;
	case TImapServerState::kWaitingForMoreClientInput:
		stateName = waitingStateName;
		break;
	}

	if (fCurrentUrl)
	{
		if (extraInfo)
			PR_LOG(IMAP, out, ("%s:%s:%s:%s: %s",fCurrentUrl->GetUrlHost(),stateName,logSubName,extraInfo,logData));
		else
			PR_LOG(IMAP, out, ("%s:%s:%s: %s",fCurrentUrl->GetUrlHost(),stateName,logSubName,logData));
	}
#endif
}

// In 4.5, this posted an event back to libmsg and blocked until it got a response.
// We may still have to do this.It would be nice if we could preflight this value,
// but we may not always know when we'll need it.
PRUint32 nsImapProtocol::GetMessageSize(const char *messageId, PRBool idsAreUids)
{
	NS_ASSERTION(FALSE, "not implemented yet");
	return 0;
}


PRBool	nsImapProtocol::GetShowAttachmentsInline()
{
	return PR_FALSE;	// need to check the preference "mail.inline_attachments"
						// perhaps the pref code is thread safe? If not ??? ### DMB
}

PRMonitor *nsImapProtocol::GetDataMemberMonitor()
{
    return m_dataMemberMonitor;
}

// It would be really nice not to have to use this method nearly as much as we did
// in 4.5 - we need to think about this some. Some of it may just go away in the new world order
PRBool nsImapProtocol::DeathSignalReceived()
{
	PRBool returnValue;
	PR_EnterMonitor(m_threadDeathMonitor);
	returnValue = m_threadShouldDie;
	PR_ExitMonitor(m_threadDeathMonitor);
	
	return returnValue;
}


PRBool nsImapProtocol::GetPseudoInterrupted()
{
	PRBool rv = FALSE;
	PR_EnterMonitor(m_pseudoInterruptMonitor);
	rv = m_pseudoInterrupted;
	PR_ExitMonitor(m_pseudoInterruptMonitor);
	return rv;
}

void nsImapProtocol::PseudoInterrupt(PRBool the_interrupt)
{
	PR_EnterMonitor(m_pseudoInterruptMonitor);
	m_pseudoInterrupted = the_interrupt;
	if (the_interrupt)
	{
		Log("CONTROL", NULL, "PSEUDO-Interrupted");
		//PR_LOG(IMAP, out, ("PSEUDO-Interrupted"));
	}
	PR_ExitMonitor(m_pseudoInterruptMonitor);
}

void	nsImapProtocol::SetActive(PRBool active)
{
	PR_EnterMonitor(GetDataMemberMonitor());
	m_active = active;
	PR_ExitMonitor(GetDataMemberMonitor());
}

PRBool	nsImapProtocol::GetActive()
{
	PRBool ret;
	PR_EnterMonitor(GetDataMemberMonitor());
	ret = m_active;
	PR_ExitMonitor(GetDataMemberMonitor());
	return ret;
}

void nsImapProtocol::SetContentModified(PRBool modified)
{
	// ### DMB this used to poke the content_modified member of the url struct...
}


PRInt32 nsImapProtocol::OpenTunnel (PRInt32 maxNumberOfBytesToRead)
{
	return 0;
}

PRInt32 nsImapProtocol::GetTunnellingThreshold()
{
	return 0;
//	return gTunnellingThreshold;
}

PRBool nsImapProtocol::GetIOTunnellingEnabled()
{
	return PR_FALSE;
//	return gIOTunnelling;
}

// Adds a set of rights for a given user on a given mailbox on the current host.
// if userName is NULL, it means "me," or MYRIGHTS.
void nsImapProtocol::AddFolderRightsForUser(const char *mailboxName, const char *userName, const char *rights)
{
}


void nsImapProtocol::CommitNamespacesForHostEvent()
{
}

// notifies libmsg that we have new capability data for the current host
void nsImapProtocol::CommitCapabilityForHostEvent()
{
}

// rights is a single string of rights, as specified by RFC2086, the IMAP ACL extension.
// Clears all rights for a given folder, for all users.
void nsImapProtocol::ClearAllFolderRights(const char *mailboxName)
{
}

char* 
nsImapProtocol::CreateNewLineFromSocket()
{
    NS_PRECONDITION(m_curReadIndex < m_totalDataSize && m_dataBuf, 
                    "Oops ... excceeding total data size");
    if (!m_dataBuf || m_curReadIndex >= m_totalDataSize)
        return nsnull;

    char* startOfLine = m_dataBuf + m_curReadIndex;
    char* endOfLine = PL_strstr(startOfLine, CRLF);
    
    // *** must have a canonical line format from the imap server ***
    if (!endOfLine)
        return nsnull;
    endOfLine += 2; // count for CRLF
    // PR_CALLOC zeros out the allocated line
    char* newLine = (char*) PR_CALLOC(endOfLine-startOfLine+1);
    if (!newLine)
        return nsnull;

    memcpy(newLine, startOfLine, endOfLine-startOfLine);
    // set the current read index
    m_curReadIndex = endOfLine - m_dataBuf;

    SetConnectionStatus(endOfLine-startOfLine);

    return newLine;
}

PRInt32
nsImapProtocol::GetConnectionStatus()
{
    // ***?? I am not sure we really to guard with monitor for 5.0 ***
    PRInt32 status;
    PR_CEnterMonitor(this);
    status = m_connectionStatus;
    PR_CExitMonitor(this);
    return status;
}

void
nsImapProtocol::SetConnectionStatus(PRInt32 status)
{
    PR_CEnterMonitor(this);
    m_connectionStatus = status;
    PR_CExitMonitor(this);
}

void
nsImapProtocol::NotifyMessageFlags(imapMessageFlagsType flags, nsMsgKey key)
{
    FlagsKeyStruct aKeyStruct;
    aKeyStruct.flags = flags;
    aKeyStruct.key = key;
    if (m_imapMessage)
        m_imapMessage->NotifyMessageFlags(this, &aKeyStruct);
}

void
nsImapProtocol::NotifySearchHit(const char * hitLine)
{
    if (m_imapMiscellaneous)
        m_imapMiscellaneous->AddSearchResult(this, hitLine);
}

	// Event handlers for the imap parser. 
void
nsImapProtocol::DiscoverMailboxSpec(mailbox_spec * adoptedBoxSpec)
{
}

void
nsImapProtocol::AlertUserEventUsingId(PRUint32 aMessageId)
{
    if (m_imapMiscellaneous)
        m_imapMiscellaneous->FEAlert(this, 
                          "**** Fix me with real string ****\r\n");
}

void
nsImapProtocol::AlertUserEvent(const char * message)
{
    if (m_imapMiscellaneous)
        m_imapMiscellaneous->FEAlert(this, message);
}

void
nsImapProtocol::AlertUserEventFromServer(const char * aServerEvent)
{
    if (m_imapMiscellaneous)
        m_imapMiscellaneous->FEAlertFromServer(this, aServerEvent);
}

void
nsImapProtocol::ShowProgress()
{
    ProgressInfo aProgressInfo;

    aProgressInfo.message = "*** Fix me!! ***\r\n";
    aProgressInfo.percent = 0;

    if (m_imapMiscellaneous)
        m_imapMiscellaneous->PercentProgress(this, &aProgressInfo);
}

void
nsImapProtocol::ProgressEventFunctionUsingId(PRUint32 aMsgId)
{
    if (m_imapMiscellaneous)
        m_imapMiscellaneous->ProgressStatus(this, "*** Fix me!! ***\r\n");
}

void
nsImapProtocol::ProgressEventFunctionUsingIdWithString(PRUint32 aMsgId, const
                                                       char * aExtraInfo)
{
    if (m_imapMiscellaneous)
        m_imapMiscellaneous->ProgressStatus(this, "*** Fix me!! ***\r\n");
}

void
nsImapProtocol::PercentProgressUpdateEvent(char *message, PRInt32 percent)
{
    ProgressInfo aProgressInfo;
    aProgressInfo.message = message;
    aProgressInfo.percent = percent;
    if (m_imapMiscellaneous)
        m_imapMiscellaneous->PercentProgress(this, &aProgressInfo);
}

	// utility function calls made by the server
char*
nsImapProtocol::CreateUtf7ConvertedString(const char * aSourceString, PRBool
                                          aConvertToUtf7Imap)
{
    return nsnull;
}

	// imap commands issued by the parser
void
nsImapProtocol::Store(const char * aMessageList, const char * aMessageData,
                      PRBool aIdsAreUid)
{
}

void
nsImapProtocol::Expunge()
{
}

void
nsImapProtocol::HandleMemoryFailure()
{
    PR_CEnterMonitor(this);
    // **** jefft fix me!!!!!! ******
    // m_imapThreadIsRunning = PR_FALSE;
    // SetConnectionStatus(-1);
    PR_CExitMonitor(this);
}
