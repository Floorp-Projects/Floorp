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
#include "plbase64.h"

PRLogModuleInfo *IMAP;

// netlib required files
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsINetService.h"

#include "nsString2.h"

#include "nsIMsgIncomingServer.h"

#define ONE_SECOND ((PRUint32)1000)    // one second

const char *kImapTrashFolderName = "Trash"; // **** needs to be localized ****

static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

#define OUTPUT_BUFFER_SIZE (4096*2) // mscott - i should be able to remove this if I can use nsMsgLineBuffer???

#define IMAP_DB_HEADERS "From To Cc Subject Date Priority X-Priority Message-ID References Newsgroups"

static const PRInt32 kImapSleepTime = 1000000;
static PRInt32 gPromoteNoopToCheckCount = 0;

// **** helper class for downloading line ****
TLineDownloadCache::TLineDownloadCache()
{
    fLineInfo = (msg_line_info *) PR_CALLOC(sizeof( msg_line_info));
    fLineInfo->adoptedMessageLine = fLineCache;
    fLineInfo->uidOfMessage = 0;
    fBytesUsed = 0;
}

TLineDownloadCache::~TLineDownloadCache()
{
    PR_FREEIF( fLineInfo);
}

PRUint32 TLineDownloadCache::CurrentUID()
{
    return fLineInfo->uidOfMessage;
}

PRUint32 TLineDownloadCache::SpaceAvailable()
{
    return kDownLoadCacheSize - fBytesUsed;
}

msg_line_info *TLineDownloadCache::GetCurrentLineInfo()
{
    return fLineInfo;
}
    
void TLineDownloadCache::ResetCache()
{
    fBytesUsed = 0;
}
    
PRBool TLineDownloadCache::CacheEmpty()
{
    return fBytesUsed == 0;
}

void TLineDownloadCache::CacheLine(const char *line, PRUint32 uid)
{
    PRUint32 lineLength = PL_strlen(line);
    NS_ASSERTION((lineLength + 1) <= SpaceAvailable(), 
                 "Oops... line length greater than space available");
    
    fLineInfo->uidOfMessage = uid;
    
    PL_strcpy(fLineInfo->adoptedMessageLine + fBytesUsed, line);
    fBytesUsed += lineLength;
}

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
    m_parser(*this), m_currentCommand(eOneByte, 0), m_flagState(kImapFlagAndUidStateSize, PR_FALSE)
{
	NS_INIT_REFCNT();
	m_flags = 0;
	m_runningUrl = nsnull; // initialize to NULL
	m_transport = nsnull;
	m_outputStream = nsnull;
	m_inputStream = nsnull;
	m_outputConsumer = nsnull;
	m_urlInProgress = PR_FALSE;
	m_socketIsOpen = PR_FALSE;
    m_connectionStatus = 0;
	m_hostSessionList = nsnull;
    
    // ***** Thread support *****
    m_sinkEventQueue = nsnull;
    m_eventQueue = nsnull;
    m_thread = nsnull;
    m_dataAvailableMonitor = nsnull;
	m_urlReadyToRunMonitor = nsnull;
	m_pseudoInterruptMonitor = nsnull;
	m_dataMemberMonitor = nsnull;
	m_threadDeathMonitor = nsnull;
    m_eventCompletionMonitor = nsnull;
	m_waitForBodyIdsMonitor = nsnull;
	m_fetchMsgListMonitor = nsnull;
    m_imapThreadIsRunning = PR_FALSE;
    m_consumer = nsnull;
	m_currentServerCommandTagNumber = 0;
	m_active = PR_FALSE;
	m_threadShouldDie = PR_FALSE;
	m_pseudoInterrupted = PR_FALSE;

    // imap protocol sink interfaces
    m_server = nsnull;
    m_imapLog = nsnull;
    m_imapMailFolder = nsnull;
    m_imapMessage = nsnull;
    m_imapExtension = nsnull;
    m_imapMiscellaneous = nsnull;
    
    m_trackingTime = PR_FALSE;
    m_startTime = 0;
    m_endTime = 0;
    m_tooFastTime = 0;
    m_idealTime = 0;
    m_chunkAddSize = 0;
    m_chunkStartSize = 0;
    m_maxChunkSize = 0;
    m_fetchByChunks = PR_FALSE;
    m_chunkSize = 0;
    m_chunkThreshold = 0;
    m_fromHeaderSeen = PR_FALSE;
    m_closeNeededBeforeSelect = PR_FALSE;
	m_needNoop = PR_FALSE;
	m_noopCount = 0;
	m_promoteNoopToCheckCount = 0;
	m_mailToFetch = PR_FALSE;
	m_fetchMsgListIsNew = PR_FALSE;

	m_checkForNewMailDownloadsHeaders = PR_TRUE;	// this should be on by default
    m_hierarchyNameState = kNoOperationInProgress;
    m_onlineBaseFolderExists = PR_FALSE;
    m_discoveryStatus = eContinue;

	// m_dataOutputBuf is used by Send Data
	m_dataOutputBuf = (char *) PR_CALLOC(sizeof(char) * OUTPUT_BUFFER_SIZE);
	m_allocatedSize = OUTPUT_BUFFER_SIZE;

	// used to buffer incoming data by ReadNextLineFromInput
	m_dataInputBuf = (char *) PR_CALLOC(sizeof(char) * OUTPUT_BUFFER_SIZE);

	m_currentBiffState = nsMsgBiffState_Unknown;

	// where should we do this? Perhaps in the factory object?
	if (!IMAP)
		IMAP = PR_NewLogModule("IMAP");
}

nsresult nsImapProtocol::Initialize(nsIImapHostSessionList * aHostSessionList, PLEventQueue * aSinkEventQueue)
{
	NS_PRECONDITION(aSinkEventQueue && aHostSessionList, 
             "oops...trying to initalize with a null sink event queue!");
	if (!aSinkEventQueue || !aHostSessionList)
        return NS_ERROR_NULL_POINTER;

    m_sinkEventQueue = aSinkEventQueue;
    m_hostSessionList = aHostSessionList;
    m_parser.SetHostSessionList(aHostSessionList);
    m_parser.SetFlagState(&m_flagState);
    NS_ADDREF (m_hostSessionList);

	// Now initialize the thread for the connection and create appropriate monitors
	if (m_thread == nsnull)
    {
        m_dataAvailableMonitor = PR_NewMonitor();
		m_urlReadyToRunMonitor = PR_NewMonitor();
		m_pseudoInterruptMonitor = PR_NewMonitor();
		m_dataMemberMonitor = PR_NewMonitor();
		m_threadDeathMonitor = PR_NewMonitor();
        m_eventCompletionMonitor = PR_NewMonitor();
		m_waitForBodyIdsMonitor = PR_NewMonitor();
		m_fetchMsgListMonitor = PR_NewMonitor();

        m_thread = PR_CreateThread(PR_USER_THREAD, ImapThreadMain, (void*)
                                   this, PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
                                   PR_UNJOINABLE_THREAD, 0);
        NS_ASSERTION(m_thread, "Unable to create imap thread.\n");
    }
	return NS_OK;
}

nsImapProtocol::~nsImapProtocol()
{
	// free handles on all networking objects...
	NS_IF_RELEASE(m_outputStream); 
	NS_IF_RELEASE(m_inputStream);
	NS_IF_RELEASE(m_outputConsumer);
	NS_IF_RELEASE(m_transport);
    NS_IF_RELEASE(m_server);
    NS_IF_RELEASE(m_imapLog);
    NS_IF_RELEASE(m_imapMailFolder);
    NS_IF_RELEASE(m_imapMessage);
    NS_IF_RELEASE(m_imapExtension);
    NS_IF_RELEASE(m_imapMiscellaneous);
	NS_IF_RELEASE(m_hostSessionList);

	PR_FREEIF(m_dataOutputBuf);
	PR_FREEIF(m_dataInputBuf);

    // **** We must be out of the thread main loop function
    NS_ASSERTION(m_imapThreadIsRunning == PR_FALSE, "Oops, thread is still running.\n");
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

	if (m_urlReadyToRunMonitor)
	{
		PR_DestroyMonitor(m_urlReadyToRunMonitor);
		m_urlReadyToRunMonitor = nsnull;
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
	if (m_waitForBodyIdsMonitor)
	{
		PR_DestroyMonitor(m_waitForBodyIdsMonitor);
		m_waitForBodyIdsMonitor = nsnull;
	}
	if (m_fetchMsgListMonitor)
	{
		PR_DestroyMonitor(m_fetchMsgListMonitor);
		m_fetchMsgListMonitor = nsnull;
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

char*
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
                
        if (!m_imapMailFolder)
        {
            nsIImapMailFolder *aImapMailFolder;
            res = m_runningUrl->GetImapMailFolder(&aImapMailFolder);
            if (NS_SUCCEEDED(res) && aImapMailFolder)
            {
                m_imapMailFolder = new nsImapMailFolderProxy(aImapMailFolder,
                                                             this,
                                                             m_sinkEventQueue,
                                                             m_thread);
                NS_IF_ADDREF(m_imapMailFolder);
                NS_RELEASE(aImapMailFolder);
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

// Setup With Url is intended to set up data which is held on a PER URL basis and not
// a per connection basis. If you have data which is independent of the url we are currently
// running, then you should put it in Initialize(). 
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
        PR_EnterMonitor(m_urlReadyToRunMonitor);

        PR_Wait(m_urlReadyToRunMonitor, PR_INTERVAL_NO_TIMEOUT);

        ProcessCurrentURL();

        PR_ExitMonitor(m_urlReadyToRunMonitor);
    }
}

void nsImapProtocol::EstablishServerConnection()
{
	// mscott: I really haven't worried about how much of this establish server connection
	// stuff we *REALLY* need. For now I just want to read out the greeting so I never bother
	// the parser with it...
	char * serverResponse = CreateNewLineFromSocket(); // read in the greeting
	PR_FREEIF(serverResponse); // we don't care about the greeting yet...

	// record the fact that we've received a greeting for this connection so we don't ever
	// try to do it again..
	SetFlag(IMAP_RECEIVED_GREETING);

#ifdef UNREADY_CODE // mscott: I don't think we need to care about this stuff...
    while (!DeathSignalReceived() && (GetConnectionStatus() == MK_WAITING_FOR_CONNECTION))
    {
        TImapFEEvent *feFinishConnectionEvent = 
            new TImapFEEvent(FinishIMAPConnection,  // function to call
                             this,                  // for access to current
                                                    // entry and monitor
                             nil,
							 TRUE);
        
        fFEEventQueue->AdoptEventToEnd(feFinishConnectionEvent);
        IMAP_YIELD(PR_INTERVAL_NO_WAIT);
        
        // wait here for the connection finish io to finish
        WaitForFEEventCompletion();
		if (!DeathSignalReceived() && (GetConnectionStatus() == MK_WAITING_FOR_CONNECTION))
			WaitForIOCompletion();
        
    }       
    
	if (GetConnectionStatus() == MK_CONNECTED)
    {
        // get the one line response from the IMAP server
        char *serverResponse = CreateNewLineFromSocket();
        if (serverResponse)
		{
			if (!XP_STRNCASECMP(serverResponse, "* OK", 4))
			{
				SetConnectionStatus(0);
				preAuthSucceeded = FALSE;
				//if (!XP_STRNCASECMP(serverResponse, "* OK Netscape IMAP4rev1 Service 3.0", 35))
				//	GetServerStateParser().SetServerIsNetscape30Server();
			}
			else if (!XP_STRNCASECMP(serverResponse, "* PREAUTH", 9))
			{
				// we've been pre-authenticated.
				// we can skip the whole password step, right into the
				// kAuthenticated state
				GetServerStateParser().PreauthSetAuthenticatedState();

				// tell the master that we're authenticated
				XP_Bool loginSucceeded = TRUE;
				TImapFEEvent *alertEvent = 
					new TImapFEEvent(msgSetUserAuthenticated,  // function to call
							 this,                // access to current entry
							 (void *)loginSucceeded,
							 TRUE);            
				if (alertEvent)
				{
					fFEEventQueue->AdoptEventToEnd(alertEvent);
				}
				else
					HandleMemoryFailure();

				preAuthSucceeded = TRUE;
           		if (fCurrentBiffState == MSG_BIFF_Unknown)
           		{
           			fCurrentBiffState = MSG_BIFF_NoMail;
               		SendSetBiffIndicatorEvent(fCurrentBiffState);
           		}

				if (GetServerStateParser().GetCapabilityFlag() ==
					kCapabilityUndefined)
					Capability();

				if ( !(GetServerStateParser().GetCapabilityFlag() & 
						(kIMAP4Capability | 
						 kIMAP4rev1Capability | 
						 kIMAP4other) ) )
				{
					AlertUserEvent_UsingId(MK_MSG_IMAP_SERVER_NOT_IMAP4);
					SetCurrentEntryStatus(-1);			
					SetConnectionStatus(-1);        // stop netlib
					preAuthSucceeded = FALSE;
				}
				else
				{
					ProcessAfterAuthenticated();
					// the connection was a success
					SetConnectionStatus(0);
				}
			}
			else
			{
				preAuthSucceeded = FALSE;
				SetConnectionStatus(MK_BAD_CONNECT);
			}

			fNeedGreeting = FALSE;
			FREEIF(serverResponse);
		}
        else
            SetConnectionStatus(MK_BAD_CONNECT);
    }

    if ((GetConnectionStatus() < 0) && !DeathSignalReceived())
	{
		if (!MSG_Biff_Master_NikiCallingGetNewMail())
    		AlertUserEvent_UsingId(MK_BAD_CONNECT);
	}
#endif   
}

void nsImapProtocol::ProcessCurrentURL()
{
	PRBool	logonFailed = FALSE;
    
	// we used to check if the current running url was 
	// Reinitialize the parser
	GetServerStateParser().InitializeState();
	GetServerStateParser().SetConnected(PR_TRUE);

	// mscott - I believe whenever we get here that we will also have
	// a connection. Why? Because when we load the url we open the 
	// connection if it isn't open already....However, if we haven't received
	// the greeting yet, we need to make sure we strip it out of the input
	// before we start to do useful things...
	if (!TestFlag(IMAP_RECEIVED_GREETING))
		EstablishServerConnection();

	// Update the security status for this URL
#ifdef UNREADY_CODE
	UpdateSecurityStatus();
#endif

	// Step 1: If we have not moved into the authenticated state yet then do so
	// by attempting to logon.
	if (!DeathSignalReceived() && (GetConnectionStatus() >= 0) &&
            (GetServerStateParser().GetIMAPstate() == 
			 nsImapServerResponseParser::kNonAuthenticated))
    {
		/* if we got here, the server's greeting should not have been PREAUTH */
		if (GetServerStateParser().GetCapabilityFlag() == kCapabilityUndefined)
				Capability();
			
		if ( !(GetServerStateParser().GetCapabilityFlag() & (kIMAP4Capability | kIMAP4rev1Capability | 
					 kIMAP4other) ) )
		{
#ifdef UNREADY_CODE
			AlertUserEvent_UsingId(MK_MSG_IMAP_SERVER_NOT_IMAP4);

			SetCurrentEntryStatus(-1);			
			SetConnectionStatus(-1);        // stop netlib
#endif
		}
		else
		{
			logonFailed = !TryToLogon();
	    }
    } // if death signal not received

    if (!DeathSignalReceived() && (GetConnectionStatus() >= 0))
    {
#if 0
		FindMailboxesIfNecessary();
#endif
		nsIImapUrl::nsImapState imapState;
		m_runningUrl->GetRequiredImapState(&imapState);
		if (imapState == nsIImapUrl::nsImapAuthenticatedState)
			ProcessAuthenticatedStateURL();
	    else    // must be a url that requires us to be in the selected stae 
			ProcessSelectedStateURL();

		// The URL has now been processed
        if (!logonFailed && GetConnectionStatus() < 0)
            HandleCurrentUrlError();
#ifdef UNREADY_CODE
		if (GetServerStateParser().LastCommandSuccessful())
			SetCurrentEntryStatus(0);
#endif
	    SetConnectionStatus(-1);        // stop netlib
        if (DeathSignalReceived())
           	HandleCurrentUrlError();
	}
    else if (!logonFailed)
        HandleCurrentUrlError(); 

	m_runningUrl->SetUrlState(PR_FALSE, NS_OK);  // we are done with this url.
	PseudoInterrupt(FALSE);	// clear this, because we must be done interrupting?
}


#if 0
void nsImapProtocol::ProcessCurrentURL()
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
            m_imapLog->HandleImapLogData(m_dataOutputBuf);
            // WaitForFEEventCompletion();

            // we are done running the imap log url so mark the url as done...
            // set change in url state...
            m_runningUrl->SetUrlState(PR_FALSE, NS_OK); 
        }
    }
}
#endif

void nsImapProtocol::ParseIMAPandCheckForNewMail(char* commandString)
{
    if (commandString)
        GetServerStateParser().ParseIMAPServerResponse(commandString);
    else
        GetServerStateParser().ParseIMAPServerResponse(
            m_currentCommand.GetBuffer());
    // **** fix me for new mail biff state *****
}


/////////////////////////////////////////////////////////////////////////////////////////////
// we suppport the nsIStreamListener interface 
////////////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsImapProtocol::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
{
    PR_CEnterMonitor(this);

    nsIImapUrl *aImapUrl;
    nsresult res = aURL->QueryInterface(nsIImapUrl::GetIID(), (void**)&aImapUrl);

    if(NS_SUCCEEDED(res) && aLength > 0)
    {
        NS_PRECONDITION( m_runningUrl->Equals(aImapUrl), "Oops... running a different imap url. Hmmm...");
		// make sure m_inputStream is set to the right input stream...
		if (m_inputStream == nsnull)
		{
			m_inputStream = aIStream;
			NS_IF_ADDREF(aIStream);
		}

		NS_ASSERTION(m_inputStream == aIStream, "hmm somehow we got a different stream than we were expecting");

        // if we received data, we need to signal the data available monitor...
		// Read next line from input will actually read the data out of the stream
		PR_EnterMonitor(m_dataAvailableMonitor);
        PR_Notify(m_dataAvailableMonitor);
		PR_ExitMonitor(m_dataAvailableMonitor);
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
        Log("SendData", nsnull, dataBuffer);
		nsresult rv = m_outputStream->Write(dataBuffer, PL_strlen(dataBuffer), &writeCount);
		if (NS_SUCCEEDED(rv) && writeCount == PL_strlen(dataBuffer))
		{
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

// LoadUrl takes a url, initializes all of our url specific data by calling SetupUrl.
// If we don't have a connection yet, we open the connection. Finally, we signal the 
// url to run monitor to let the imap main thread loop process the current url (it is waiting
// on this monitor). There is a contract that the imap thread has already been started b4 we
// attempt to load a url....
nsresult nsImapProtocol::LoadUrl(nsIURL * aURL, nsISupports * aConsumer)
{
	nsresult rv = NS_OK;
	nsIImapUrl * imapUrl = nsnull;
	if (aURL)
	{
		if (m_transport == nsnull) // i.e. we haven't been initialized yet....
			SetupWithUrl(aURL); 

		 SetupSinkProxy(); // generate proxies for all of the event sinks in the url
		
		if (m_transport && m_runningUrl)
		{
			PRBool transportOpen = PR_FALSE;
			m_transport->IsTransportOpen(&transportOpen);
			if (transportOpen == PR_FALSE)
			{
                // m_urlInProgress = PR_TRUE;
				rv = m_transport->Open(m_runningUrl);  // opening the url will cause to get notified when the connection is established
			}
			else  // the connection is already open so we should signal the monitor that a new url is ready to be processed.
			{
				// mscott: the following code segment is just a hack to test imap commands from the test
				// harnesss. It should eventually be replaced by the code in the else clause which just signals
				// the monitor for processing the url 
                // ********** jefft ********* okay let's use ? search string
                // for passing the raw command now.
                m_urlInProgress = PR_TRUE;
                const char *search = nsnull;
                aURL->GetSearch(&search);
                char *tmpBuffer = nsnull;
                if (search && PL_strlen(search))
                {
				    IncrementCommandTagNumber();
                    tmpBuffer = PR_smprintf("%s %s\r\n", GetServerCommandTag(), search);
                    if (tmpBuffer)
                    {
                        SendData(tmpBuffer);
                        PR_Free(tmpBuffer);
                    }
                }
					 
			} // if the connection was already open

			// We now have a url to run so signal the monitor for url ready to be processed...
			PR_EnterMonitor(m_urlReadyToRunMonitor);
			PR_Notify(m_urlReadyToRunMonitor);
			PR_ExitMonitor(m_urlReadyToRunMonitor);

		} // if we have an imap url and a transport
        
		if (aConsumer)
		{
            m_consumer = aConsumer;
			NS_ADDREF(m_consumer);
		}
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

void nsImapProtocol::ProcessSelectedStateURL()
{
    char *mailboxName = nsnull;
	nsIImapUrl::nsImapAction imapAction; 
	PRBool					bMessageIdsAreUids = PR_TRUE;
	imapMessageFlagsType	msgFlags = 0;
	const char					*hostName = nsnull;
	nsString2				urlHost(eOneByte);

	// this can't fail, can it?
	nsresult res = m_runningUrl->GetImapAction(&imapAction);
	m_runningUrl->MessageIdsAreUids(&bMessageIdsAreUids);
	m_runningUrl->GetMsgFlags(&msgFlags);
	m_runningUrl->GetHost(&hostName);

	res = m_runningUrl->CreateServerSourceFolderPathString(&mailboxName);
    if (mailboxName && NS_SUCCEEDED(res))
    {
    	char *convertedName = CreateUtf7ConvertedString(mailboxName, TRUE);
    	PR_Free(mailboxName);
    	mailboxName = convertedName;
    }
    if (mailboxName && !DeathSignalReceived())
    {
		// OK, code here used to check explicitly for multiple connections to the inbox,
		// but the connection pool stuff should handle this now.
    	PRBool selectIssued = FALSE;
        if (GetServerStateParser().GetIMAPstate() == nsImapServerResponseParser::kFolderSelected)
        {
            if (GetServerStateParser().GetSelectedMailboxName() && 
                PL_strcmp(GetServerStateParser().GetSelectedMailboxName(),
                          mailboxName))
            {       // we are selected in another folder
            	if (m_closeNeededBeforeSelect)
                	Close();
                if (GetServerStateParser().LastCommandSuccessful()) 
                {
                	selectIssued = TRUE;
					AutoSubscribeToMailboxIfNecessary(mailboxName);
                    SelectMailbox(mailboxName);
                }
            }
            else if (!GetServerStateParser().GetSelectedMailboxName())
            {       // why are we in the selected state with no box name?
                SelectMailbox(mailboxName);
                selectIssued = TRUE;
            }
            else
            {
            	// get new message counts, if any, from server
//            	ProgressEventFunction_UsingId (MK_IMAP_STATUS_SELECTING_MAILBOX);
				if (m_needNoop)
				{
					m_noopCount++;
					if (gPromoteNoopToCheckCount > 0 && (m_noopCount % gPromoteNoopToCheckCount) == 0)
						Check();
					else
            			Noop();	// I think this is needed when we're using a cached connection
					m_needNoop = FALSE;
				}
            }
        }
        else
        {
            // go to selected state
			AutoSubscribeToMailboxIfNecessary(mailboxName);
            SelectMailbox(mailboxName);
            selectIssued = TRUE;
        }

		if (selectIssued)
		{
//			RefreshACLForFolderIfNecessary(mailboxName);
		}
        
        PRBool uidValidityOk = PR_TRUE;
        if (GetServerStateParser().LastCommandSuccessful() && selectIssued && 
           (imapAction != nsIImapUrl::nsImapSelectFolder) && (imapAction != nsIImapUrl::nsImapLiteSelectFolder))
        {
        	uid_validity_info *uidStruct = (uid_validity_info *) PR_Malloc(sizeof(uid_validity_info));
        	if (uidStruct)
        	{
        		uidStruct->returnValidity = kUidUnknown;
				uidStruct->hostName = hostName;
        		m_runningUrl->CreateCanonicalSourceFolderPathString(&uidStruct->canonical_boxname);
				if (m_imapMiscellaneous)
				{
					m_imapMiscellaneous->GetStoredUIDValidity(this, uidStruct);

			        WaitForFEEventCompletion();
			        
			        // error on the side of caution, if the fe event fails to set uidStruct->returnValidity, then assume that UIDVALIDITY
			        // did not role.  This is a common case event for attachments that are fetched within a browser context.
					if (!DeathSignalReceived())
						uidValidityOk = (uidStruct->returnValidity == kUidUnknown) || (uidStruct->returnValidity == GetServerStateParser().FolderUID());
			    }
			    else
			        HandleMemoryFailure();
			    
			    PR_FREEIF(uidStruct->canonical_boxname);
			    PR_Free(uidStruct);
        	}
        	else
        		HandleMemoryFailure();
        }
            
        if (GetServerStateParser().LastCommandSuccessful() && !DeathSignalReceived() && (uidValidityOk || imapAction == nsIImapUrl::nsImapDeleteAllMsgs))
        {

			switch (imapAction)
			{
			case nsIImapUrl::nsImapLiteSelectFolder:
				if (GetServerStateParser().LastCommandSuccessful() && m_imapMiscellaneous)
				{
					m_imapMiscellaneous->LiteSelectUIDValidity(this, GetServerStateParser().FolderUID());

					WaitForFEEventCompletion();
					// need to update the mailbox count - is this a good place?
					ProcessMailboxUpdate(FALSE);	// handle uidvalidity change
				}
				break;
            case nsIImapUrl::nsImapMsgFetch:
            {
                char *messageIdString;
                m_runningUrl->CreateListOfMessageIdsString(&messageIdString);
                if (messageIdString)
                {
#ifdef HAVE_PORT
					// we dont want to send the flags back in a group
					// GetServerStateParser().ResetFlagInfo(0);
					if (HandlingMultipleMessages(messageIdString))
					{
						// multiple messages, fetch them all
						m_progressStringId =  XP_FOLDER_RECEIVING_MESSAGE_OF;
						
						m_progressIndex = 0;
						m_progressCount = CountMessagesInIdString(messageIdString);
						
	                    FetchMessage(messageIdString, 
									 kEveryThingRFC822Peek,
									 bMessageIdsAreUids);
						m_progressStringId = 0;
					}
					else
					{
						// A single message ID

						// First, let's see if we're requesting a specific MIME part
						char *imappart = nsnull;
						m_runningUrl->GetImapPartToFetch(&imappart);
						if (imappart)
						{
							if (bMessageIdsAreUids)
							{
								// We actually want a specific MIME part of the message.
								// The Body Shell will generate it, even though we haven't downloaded it yet.

								IMAP_ContentModifiedType modType = GetShowAttachmentsInline() ? 
									IMAP_CONTENT_MODIFIED_VIEW_INLINE :
									IMAP_CONTENT_MODIFIED_VIEW_AS_LINKS;

								nsIMAPBodyShell *foundShell = TIMAPHostInfo::FindShellInCacheForHost(m_runningUrl->GetUrlHost(),
									GetServerStateParser().GetSelectedMailboxName(), messageIdString, modType);
								if (!foundShell)
								{
									// The shell wasn't in the cache.  Deal with this case later.
									Log("SHELL",NULL,"Loading part, shell not found in cache!");
									//PR_LOG(IMAP, out, ("BODYSHELL: Loading part, shell not found in cache!"));
									// The parser will extract the part number from the current URL.
									SetContentModified(modType);
									Bodystructure(messageIdString, bMessageIdsAreUids);
								}
								else
								{
									Log("SHELL", NULL, "Loading Part, using cached shell.");
									//PR_LOG(IMAP, out, ("BODYSHELL: Loading part, using cached shell."));
									SetContentModified(modType);
									foundShell->SetConnection(this);
									GetServerStateParser().UseCachedShell(foundShell);
									foundShell->Generate(imappart);
									GetServerStateParser().UseCachedShell(NULL);
								}
							}
							else
							{
								// Message IDs are not UIDs.
								NS_ASSERTION(PR_FALSE, "message ids aren't uids");
							}
							PR_Free(imappart);
						}
						else
						{
							// downloading a single message: try to do it by bodystructure, and/or do it by chunks
							PRUint32 messageSize = GetMessageSize(messageIdString,
								bMessageIdsAreUids);
							// We need to check the format_out bits to see if we are allowed to leave out parts,
							// or if we are required to get the whole thing.  Some instances where we are allowed
							// to do it by parts:  when viewing a message, or its source
							// Some times when we're NOT allowed:  when forwarding a message, saving it, moving it, etc.
							XP_Bool allowedToBreakApart = (ce  && !DeathSignalReceived()) ? ce->URL_s->allow_content_change : FALSE;

							if (gMIMEOnDemand &&
								allowedToBreakApart && 
								!GetShouldFetchAllParts() &&
								GetServerStateParser().ServerHasIMAP4Rev1Capability() &&
								(messageSize > (uint32) gMIMEOnDemandThreshold) &&
								!m_runningUrl->MimePartSelectorDetected())	// if a ?part=, don't do BS.
							{
								// OK, we're doing bodystructure

								// Before fetching the bodystructure, let's check our body shell cache to see if
								// we already have it around.
								nsIMAPBodyShell *foundShell = NULL;
								IMAP_ContentModifiedType modType = GetShowAttachmentsInline() ? 
									IMAP_CONTENT_MODIFIED_VIEW_INLINE :
									IMAP_CONTENT_MODIFIED_VIEW_AS_LINKS;

								SetContentModified(modType);  // This will be looked at by the cache
								if (bMessageIdsAreUids)
								{
									foundShell = TIMAPHostInfo::FindShellInCacheForHost(m_runningUrl->GetUrlHost(),
										GetServerStateParser().GetSelectedMailboxName(), messageIdString, modType);
									if (foundShell)
									{
										Log("SHELL",NULL,"Loading message, using cached shell.");
										//PR_LOG(IMAP, out, ("BODYSHELL: Loading message, using cached shell."));
										foundShell->SetConnection(this);
										GetServerStateParser().UseCachedShell(foundShell);
										foundShell->Generate(NULL);
										GetServerStateParser().UseCachedShell(NULL);
									}
								}

								if (!foundShell)
									Bodystructure(messageIdString, bMessageIdsAreUids);
							}
							else
							{
								// Not doing bodystructure.  Fetch the whole thing, and try to do
								// it in chunks.
								SetContentModified(IMAP_CONTENT_NOT_MODIFIED);
								FetchTryChunking(messageIdString, TIMAP4BlockingConnection::kEveryThingRFC822,
									bMessageIdsAreUids, NULL, messageSize, TRUE);
							}
						}
					}
                    PR_FREEIF( messageIdString);
#endif // HAVE_PORT
                }
                else
                    HandleMemoryFailure();
            }
			break;
			case nsIImapUrl::nsImapExpungeFolder:
				Expunge();
				// note fall through to next cases.
			case nsIImapUrl::nsImapSelectFolder:
			case nsIImapUrl::nsImapSelectNoopFolder:
            	ProcessMailboxUpdate(TRUE);
				break;
            case nsIImapUrl::nsImapMsgHeader:
            {
                char *messageIdString = nsnull;
                m_runningUrl->CreateListOfMessageIdsString(&messageIdString);
				nsString2 messageIds(messageIdString, eOneByte);

                if (messageIdString)
                {
                    // we don't want to send the flags back in a group
            //        GetServerStateParser().ResetFlagInfo(0);
                    FetchMessage(messageIds, 
                                 kHeadersRFC822andUid,
                                 bMessageIdsAreUids);
                    PR_FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
            }
			break;
            case nsIImapUrl::nsImapSearch:
            {
#ifdef HAVE_PORT                        
                char *searchCriteriaString = 
                    m_runningUrl->CreateSearchCriteriaString();
                if (searchCriteriaString)
                {
                    Search(searchCriteriaString, bMessageIdsAreUids);
                        // drop the results on the floor for now
                    PR_FREEIF( searchCriteriaString);
                }
                else
                    HandleMemoryFailure();
#endif
            }
			break;
            case nsIImapUrl::nsImapDeleteMsg:
            {
#ifdef HAVE_PORT                        
                char *messageIdString = 
                    m_runningUrl->CreateListOfMessageIdsString();
                if (messageIdString)
                {
					if (HandlingMultipleMessages(messageIdString))
						ProgressEventFunction_UsingId (XP_IMAP_DELETING_MESSAGES);
					else
						ProgressEventFunction_UsingId(XP_IMAP_DELETING_MESSAGE);
                    Store(messageIdString, "+FLAGS (\\Deleted)", 
                          bMessageIdsAreUids);
                    
                    if (GetServerStateParser().LastCommandSuccessful())
                    {
                    	struct delete_message_struct *deleteMsg = (struct delete_message_struct *) XP_ALLOC (sizeof(struct delete_message_struct));

						// convert name back from utf7
						utf_name_struct *nameStruct = (utf_name_struct *) XP_ALLOC(sizeof(utf_name_struct));
						char *convertedCanonicalName = NULL;
						if (nameStruct)
						{
    						nameStruct->toUtf7Imap = FALSE;
							nameStruct->sourceString = (unsigned char *) GetServerStateParser().GetSelectedMailboxName();
							nameStruct->convertedString = NULL;
							ConvertImapUtf7(nameStruct, NULL);
							if (nameStruct->convertedString)
								convertedCanonicalName = m_runningUrl->AllocateCanonicalPath((char *) nameStruct->convertedString);
						}

						deleteMsg->onlineFolderName = convertedCanonicalName;
						deleteMsg->deleteAllMsgs = FALSE;
                    	deleteMsg->msgIdString   = messageIdString;	// storage adopted, do not delete
                    	messageIdString = nil;		// deleting nil is ok
                    	
					    TImapFEEvent *deleteEvent = 
					        new TImapFEEvent(NotifyMessageDeletedEvent,       // function to call
					                        (void *) this,
					                        (void *) deleteMsg, TRUE);

					    if (deleteEvent)
					        fFEEventQueue->AdoptEventToEnd(deleteEvent);
					    else
					        HandleMemoryFailure();
                    }
                    FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
#endif // HAVE_PORT
            }
			break;
            case nsIImapUrl::nsImapDeleteAllMsgs:
            {
#ifdef HAVE_PORT
            	uint32 numberOfMessages = GetServerStateParser().NumberOfMessages();
            	if (numberOfMessages)
				{
					char messageIdString[100];	// enough for bazillion msgs
					sprintf(messageIdString, "1:*");     /* Reviewed 4.51 safe use of sprintf */
                    
					Store(messageIdString, "+FLAGS.SILENT (\\Deleted)", FALSE);	// use sequence #'s 
                
					if (GetServerStateParser().LastCommandSuccessful())
						Expunge();      // expunge messages with deleted flag
					if (GetServerStateParser().LastCommandSuccessful())
					{
						struct delete_message_struct *deleteMsg = (struct delete_message_struct *) XP_ALLOC (sizeof(struct delete_message_struct));

						// convert name back from utf7
						utf_name_struct *nameStruct = (utf_name_struct *) XP_ALLOC(sizeof(utf_name_struct));
						char *convertedCanonicalName = NULL;
						if (nameStruct)
						{
    						nameStruct->toUtf7Imap = FALSE;
							nameStruct->sourceString = (unsigned char *) GetServerStateParser().GetSelectedMailboxName();
							nameStruct->convertedString = NULL;
							ConvertImapUtf7(nameStruct, NULL);
							if (nameStruct->convertedString)
								convertedCanonicalName = m_runningUrl->AllocateCanonicalPath((char *) nameStruct->convertedString);
						}

						deleteMsg->onlineFolderName = convertedCanonicalName;
						deleteMsg->deleteAllMsgs = TRUE;
						deleteMsg->msgIdString   = nil;

						TImapFEEvent *deleteEvent = 
							new TImapFEEvent(NotifyMessageDeletedEvent,       // function to call
											(void *) this,
											(void *) deleteMsg, TRUE);

						if (deleteEvent)
							fFEEventQueue->AdoptEventToEnd(deleteEvent);
						else
							HandleMemoryFailure();
					}
                
				}
                DeleteSubFolders(mailboxName);
#endif // HAVE_PORT
            }
			break;
			case nsIImapUrl::nsImapAppendMsgFromFile:
			{
#ifdef HAVE_PORT // evil evil!
				char *sourceMessageFile =
					XP_STRDUP(GetActiveEntry()->URL_s->post_data);
				char *mailboxName =  
					m_runningUrl->CreateServerSourceFolderPathString();
				
				if (mailboxName)
				{
					char *convertedName = CreateUtf7ConvertedString(mailboxName, TRUE);
					XP_FREE(mailboxName);
					mailboxName = convertedName;
				}
				if (mailboxName)
				{
					UploadMessageFromFile(sourceMessageFile, mailboxName,
										  kImapMsgSeenFlag);
				}
				else
					HandleMemoryFailure();
				PR_FREEIF( sourceMessageFile );
				PR_FREEIF( mailboxName );
#endif
			}
			break;
            case nsIImapUrl::nsImapAddMsgFlags:
            {
                char *messageIdString = nsnull;
                m_runningUrl->CreateListOfMessageIdsString(&messageIdString);
                        
                if (messageIdString)
                {       
                    ProcessStoreFlags(messageIdString, bMessageIdsAreUids,
                                      msgFlags, PR_TRUE);
                    
					PR_FREEIF( messageIdString);
                    /*
                    if ( !DeathSignalReceived() &&
                    	  GetServerStateParser().Connected() &&
                         !GetServerStateParser().SyntaxError())
                    {
                        //if (msgFlags & kImapMsgDeletedFlag)
                        //    Expunge();      // expunge messages with deleted flag
                        Check();        // flush servers flag state
                    }
                    */
                }
                else
                    HandleMemoryFailure();
            }
			break;
            case nsIImapUrl::nsImapSubtractMsgFlags:
            {
                char *messageIdString = nsnull;
                m_runningUrl->CreateListOfMessageIdsString(&messageIdString);
                        
                if (messageIdString)
                {
                    ProcessStoreFlags(messageIdString, bMessageIdsAreUids,
                                      msgFlags, FALSE);
                    
                    PR_FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
            }
			break;
            case nsIImapUrl::nsImapSetMsgFlags:
            {
                char *messageIdString = nsnull;
                m_runningUrl->CreateListOfMessageIdsString(&messageIdString);
                        
                if (messageIdString)
                {
                    ProcessStoreFlags(messageIdString, bMessageIdsAreUids,
                                      msgFlags, TRUE);
                    ProcessStoreFlags(messageIdString, bMessageIdsAreUids,
                                      ~msgFlags, FALSE);
                    
                    PR_FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
            }
			break;
            case nsIImapUrl::nsImapBiff:
#ifdef HAVE_PORT
                PeriodicBiff(); 
#endif
			break;
            case nsIImapUrl::nsImapOnlineCopy:
            case nsIImapUrl::nsImapOnlineMove:
            {
#ifdef HAVE_PORT
                char *messageIdString = nsnull;
                m_runningUrl->CreateListOfMessageIdsString(&messageIdString);
                char *destinationMailbox = 
                    m_runningUrl->CreateServerDestinationFolderPathString();
			    if (destinationMailbox)
			    {
			    	char *convertedName = CreateUtf7ConvertedString(destinationMailbox, TRUE);
			    	XP_FREE(destinationMailbox);
			    	destinationMailbox = convertedName;
			    }

                if (messageIdString && destinationMailbox)
                {
					if (m_runningUrl->GetIMAPurlType() == TIMAPUrl::kOnlineMove) {
						if (HandlingMultipleMessages(messageIdString))
							ProgressEventFunction_UsingIdWithString (XP_IMAP_MOVING_MESSAGES_TO, destinationMailbox);
						else
							ProgressEventFunction_UsingIdWithString (XP_IMAP_MOVING_MESSAGE_TO, destinationMailbox); 
					}
					else {
						if (HandlingMultipleMessages(messageIdString))
							ProgressEventFunction_UsingIdWithString (XP_IMAP_COPYING_MESSAGES_TO, destinationMailbox);
						else
							ProgressEventFunction_UsingIdWithString (XP_IMAP_COPYING_MESSAGE_TO, destinationMailbox); 
					}

                    Copy(messageIdString, destinationMailbox, bMessageIdsAreUids);
                    FREEIF( destinationMailbox);
					ImapOnlineCopyState copyState;
					if (DeathSignalReceived())
						copyState = kInterruptedState;
					else
						copyState = GetServerStateParser().LastCommandSuccessful() ? 
                                kSuccessfulCopy : kFailedCopy;
                    OnlineCopyCompleted(copyState);
                    
                    if (GetServerStateParser().LastCommandSuccessful() &&
                        (m_runningUrl->GetIMAPurlType() == TIMAPUrl::kOnlineMove))
                    {
                        Store(messageIdString, "+FLAGS (\\Deleted)", bMessageIdsAreUids);
                        
						PRBool storeSuccessful = GetServerStateParser().LastCommandSuccessful();
                        	
                        OnlineCopyCompleted( storeSuccessful ? kSuccessfulDelete : kFailedDelete);
                    }
                    FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
#endif
            }
			break;
            case nsIImapUrl::nsImapOnlineToOfflineCopy:
			case nsIImapUrl::nsImapOnlineToOfflineMove:
            {
#ifdef HAVE_PORT
                char *messageIdString = nsnull;
                m_runningUrl->CreateListOfMessageIdsString(&messageIdString);
                if (messageIdString)
                {
					fProgressStringId =  XP_FOLDER_RECEIVING_MESSAGE_OF;
					fProgressIndex = 0;
					fProgressCount = CountMessagesInIdString(messageIdString);
					
					FetchMessage(messageIdString, 
                                 kEveryThingRFC822Peek,
                                 bMessageIdsAreUids);
                      
                    fProgressStringId = 0;           
                    OnlineCopyCompleted(
                        GetServerStateParser().LastCommandSuccessful() ? 
                                kSuccessfulCopy : kFailedCopy);

                    if (GetServerStateParser().LastCommandSuccessful() &&
                        (m_runningUrl->GetIMAPurlType() == TIMAPUrl::kOnlineToOfflineMove))
                    {
                        Store(messageIdString, "+FLAGS (\\Deleted)", bMessageIdsAreUids);
						PRBool storeSuccessful = GetServerStateParser().LastCommandSuccessful();

                        OnlineCopyCompleted( storeSuccessful ? kSuccessfulDelete : kFailedDelete);
                    }
                    
                    FREEIF( messageIdString);
                }
                else
                    HandleMemoryFailure();
#endif
            }
			default:
				if (GetServerStateParser().LastCommandSuccessful() && !uidValidityOk)
        			ProcessMailboxUpdate(FALSE);	// handle uidvalidity change
				break;
			}
        }
        PR_FREEIF( mailboxName);
    }
    else if (!DeathSignalReceived())
        HandleMemoryFailure();

}

void nsImapProtocol::AutoSubscribeToMailboxIfNecessary(const char *mailboxName)
{
#ifdef HAVE_PORT
	if (fFolderNeedsSubscribing)	// we don't know about this folder - we need to subscribe to it / list it.
	{
		fHierarchyNameState = kListingForInfoOnly;
		List(mailboxName, FALSE);
		fHierarchyNameState = kNoOperationInProgress;

		// removing and freeing it as we go.
		TIMAPMailboxInfo *mb = NULL;
		int total = XP_ListCount(fListedMailboxList);
		do
		{
			mb = (TIMAPMailboxInfo *) XP_ListRemoveTopObject(fListedMailboxList);
			delete mb;
		} while (mb);

		// if the mailbox exists (it was returned from the LIST response)
		if (total > 0)
		{
			// Subscribe to it, if the pref says so
			if (TIMAPHostInfo::GetHostIsUsingSubscription(fCurrentUrl->GetUrlHost()) && fAutoSubscribeOnOpen)
			{
				XP_Bool lastReportingErrors = GetServerStateParser().GetReportingErrors();
				GetServerStateParser().SetReportingErrors(FALSE);
				Subscribe(mailboxName);
				GetServerStateParser().SetReportingErrors(lastReportingErrors);
			}

			// Always LIST it anyway, to get it into the folder lists,
			// so that we can continue to perform operations on it, at least
			// for this session.
			fHierarchyNameState = kNoOperationInProgress;
			List(mailboxName, FALSE);
		}

		// We should now be subscribed to it, and have it in our folder lists
		// and panes.  Even if something failed, we don't want to try this again.
		fFolderNeedsSubscribing = FALSE;

	}
#endif
}


void nsImapProtocol::BeginMessageDownLoad(
    PRUint32 total_message_size, // for user, headers and body
    const char *content_type)
{
	char *sizeString = PR_smprintf("OPEN Size: %ld", total_message_size);
	Log("STREAM",sizeString,"Begin Message Download Stream");
	PR_FREEIF(sizeString);
    //total_message_size)); 
	StreamInfo *si = (StreamInfo *) PR_CALLOC (sizeof (StreamInfo));		// This will be freed in the event
	if (si)
	{
		si->size = total_message_size;
		si->content_type = PL_strdup(content_type);
		if (si->content_type)
		{
            if (m_imapMessage) 
            {
                m_imapMessage->SetupMsgWriteStream(this, si);
				WaitForFEEventCompletion();
			}
            PL_strfree(si->content_type);
        }
		else
			HandleMemoryFailure();
        PR_Free(si);
	}
	else
		HandleMemoryFailure();
}

PRBool
nsImapProtocol::GetShouldDownloadArbitraryHeaders()
{
    // *** allocate instead of using local variable to be thread save ***
    GenericInfo *aInfo = (GenericInfo*) PR_CALLOC(sizeof(GenericInfo));
    const char *hostName = nsnull;
    PRBool rv;
    aInfo->rv = PR_TRUE;         // default
    m_runningUrl->GetHost(&hostName);
    aInfo->hostName = PL_strdup (hostName);
    if (m_imapMiscellaneous)
    {
        m_imapMiscellaneous->GetShouldDownloadArbitraryHeaders(this, aInfo);
        WaitForFEEventCompletion();
    }
    rv = aInfo->rv;
    if (aInfo->hostName)
        PL_strfree(aInfo->hostName);
    if (aInfo->c)
        PL_strfree(aInfo->c);
    PR_Free(aInfo);
    return rv;
}

char*
nsImapProtocol::GetArbitraryHeadersToDownload()
{
    // *** allocate instead of using local variable to be thread save ***
    GenericInfo *aInfo = (GenericInfo*) PR_CALLOC(sizeof(GenericInfo));
    const char *hostName = nsnull;
    char *rv = nsnull;
    aInfo->rv = PR_TRUE;         // default
    m_runningUrl->GetHost(&hostName);
    aInfo->hostName = PL_strdup (hostName);
    if (m_imapMiscellaneous)
    {
        m_imapMiscellaneous->GetShouldDownloadArbitraryHeaders(this, aInfo);
        WaitForFEEventCompletion();
    }
    if (aInfo->hostName)
        PL_strfree(aInfo->hostName);
    rv = aInfo->c;
    PR_Free(aInfo);
    return rv;
}

void
nsImapProtocol::AdjustChunkSize()
{
	m_endTime = PR_Now();
	m_trackingTime = FALSE;
	PRTime t = m_endTime - m_startTime;
	if (t < 0)
		return;							// bogus for some reason
	if (t <= m_tooFastTime) {
		m_chunkSize += m_chunkAddSize;
		m_chunkThreshold = m_chunkSize + (m_chunkSize / 2);
		if (m_chunkSize > m_maxChunkSize)
			m_chunkSize = m_maxChunkSize;
	}
	else if (t <= m_idealTime)
		return;
	else {
		if (m_chunkSize > m_chunkStartSize)
			m_chunkSize = m_chunkStartSize;
		else if (m_chunkSize > (m_chunkAddSize * 2))
			m_chunkSize -= m_chunkAddSize;
		m_chunkThreshold = m_chunkSize + (m_chunkSize / 2);
	}
}

// authenticated state commands 
// escape any backslashes or quotes.  Backslashes are used a lot with our NT server
char *nsImapProtocol::CreateEscapedMailboxName(const char *rawName)
{
	nsString2 escapedName(rawName, eOneByte);

	for (PRInt32 strIndex = 0; *rawName; strIndex++)
	{
		char currentChar = *rawName++;
		if ((currentChar == '\\') || (currentChar == '\"'))
		{
			escapedName.Insert('\\', strIndex++);
		}
	}
	return escapedName.ToNewCString();
}

void nsImapProtocol::SelectMailbox(const char *mailboxName)
{
//    ProgressEventFunction_UsingId (MK_IMAP_STATUS_SELECTING_MAILBOX);
    IncrementCommandTagNumber();
    
    m_closeNeededBeforeSelect = PR_FALSE;		// initial value
	GetServerStateParser().ResetFlagInfo(0);    
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    nsString2 commandBuffer(GetServerCommandTag(), eOneByte);
	commandBuffer.Append(" select \"");
	commandBuffer.Append(escapedName);
	commandBuffer.Append("\"" CRLF);
            
    PR_FREEIF( escapedName);
           
    int                 ioStatus = SendData(commandBuffer.GetBuffer());
	ParseIMAPandCheckForNewMail();

	PRInt32 numOfMessagesInFlagState = 0;
	nsIImapUrl::nsImapAction imapAction; 
	m_flagState.GetNumberOfMessages(&numOfMessagesInFlagState);
	nsresult res = m_runningUrl->GetImapAction(&imapAction);
	// if we've selected a mailbox, and we're not going to do an update because of the
	// url type, but don't have the flags, go get them!
	if (NS_SUCCEEDED(res) &&
		imapAction != nsIImapUrl::nsImapSelectFolder && imapAction != nsIImapUrl::nsImapExpungeFolder 
		&& imapAction != nsIImapUrl::nsImapLiteSelectFolder &&
		imapAction != nsIImapUrl::nsImapDeleteAllMsgs && 
		((GetServerStateParser().NumberOfMessages() != numOfMessagesInFlagState) && (numOfMessagesInFlagState == 0))) 
	{
	   	ProcessMailboxUpdate(PR_FALSE);	
	}
}

// Please call only with a single message ID
void nsImapProtocol::Bodystructure(const char *messageId, PRBool idIsUid)
{
    IncrementCommandTagNumber();
    
    nsString2 commandString(GetServerCommandTag(), eOneByte);
    if (idIsUid)
    	commandString.Append(" UID");
   	commandString.Append(" fetch ");

	commandString.Append(messageId);
	commandString.Append("  (BODYSTRUCTURE)" CRLF);

	            
    int ioStatus = SendData(commandString.GetBuffer());
	    

	ParseIMAPandCheckForNewMail(commandString.GetBuffer());
}

void nsImapProtocol::PipelinedFetchMessageParts(const char *uid, nsIMAPMessagePartIDArray *parts)
{
	// assumes no chunking

	// build up a string to fetch
	nsString2 stringToFetch(eOneByte), what(eOneByte);
	int32 currentPartNum = 0;
	while ((parts->GetNumParts() > currentPartNum) && !DeathSignalReceived())
	{
		nsIMAPMessagePartID *currentPart = parts->GetPart(currentPartNum);
		if (currentPart)
		{
			// Do things here depending on the type of message part
			// Append it to the fetch string
			if (currentPartNum > 0)
				stringToFetch.Append(" ");

			switch (currentPart->GetFields())
			{
			case kMIMEHeader:
				what = "BODY[";
				what.Append(currentPart->GetPartNumberString());
				what.Append(".MIME]");
				stringToFetch.Append(what);
				break;
			case kRFC822HeadersOnly:
				if (currentPart->GetPartNumberString())
				{
					what = "BODY[";
					what.Append(currentPart->GetPartNumberString());
					what.Append(".HEADER]");
					stringToFetch.Append(what);
				}
				else
				{
					// headers for the top-level message
					stringToFetch.Append("BODY[HEADER]");
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

		nsString2 commandString(GetServerCommandTag(), eOneByte); 
		commandString.Append(" UID fetch ");
		commandString.Append(uid, 10);
		commandString.Append(" (");
		commandString.Append(stringToFetch);
		commandString.Append(")" CRLF);
		int ioStatus = SendData(commandString.GetBuffer());
		ParseIMAPandCheckForNewMail(commandString.GetBuffer());
		PR_Free(stringToFetch);
	}
}




// this routine is used to fetch a message or messages, or headers for a
// message...

void
nsImapProtocol::FetchMessage(nsString2 &messageIds, 
                             nsIMAPeFetchFields whatToFetch,
                             PRBool idIsUid,
                             PRUint32 startByte, PRUint32 endByte,
                             char *part)
{
    IncrementCommandTagNumber();
    
    nsString2 commandString;
    if (idIsUid)
    	commandString = "%s UID fetch";
    else
    	commandString = "%s fetch";
    
    switch (whatToFetch) {
        case kEveryThingRFC822:
			if (m_trackingTime)
				AdjustChunkSize();			// we started another segment
			m_startTime = PR_Now();			// save start of download time
			m_trackingTime = TRUE;
			if (GetServerStateParser().ServerHasIMAP4Rev1Capability())
			{
				if (GetServerStateParser().GetCapabilityFlag() & kHasXSenderCapability)
					commandString.Append(" %s (XSENDER UID RFC822.SIZE BODY[]");
				else
					commandString.Append(" %s (UID RFC822.SIZE BODY[]");
			}
			else
			{
				if (GetServerStateParser().GetCapabilityFlag() & kHasXSenderCapability)
					commandString.Append(" %s (XSENDER UID RFC822.SIZE RFC822");
				else
					commandString.Append(" %s (UID RFC822.SIZE RFC822");
			}
			if (endByte > 0)
			{
				// if we are retrieving chunks
				char *byterangeString = PR_smprintf("<%ld.%ld>",startByte,endByte);
				if (byterangeString)
				{
					commandString.Append(byterangeString);
					PR_Free(byterangeString);
				}
			}
			commandString.Append(")");
            break;

        case kEveryThingRFC822Peek:
        	{
	        	char *formatString = "";
	        	PRUint32 server_capabilityFlags = GetServerStateParser().GetCapabilityFlag();
	        	
	        	if (server_capabilityFlags & kIMAP4rev1Capability)
	        	{
	        		// use body[].peek since rfc822.peek is not in IMAP4rev1
	        		if (server_capabilityFlags & kHasXSenderCapability)
	        			formatString = " %s (XSENDER UID RFC822.SIZE BODY.PEEK[])";
	        		else
	        			formatString = " %s (UID RFC822.SIZE BODY.PEEK[])";
	        	}
	        	else
	        	{
	        		if (server_capabilityFlags & kHasXSenderCapability)
	        			formatString = " %s (XSENDER UID RFC822.SIZE RFC822.peek)";
	        		else
	        			formatString = " %s (UID RFC822.SIZE RFC822.peek)";
	        	}
	        
				commandString.Append(formatString);
			}
            break;
        case kHeadersRFC822andUid:
			if (GetServerStateParser().ServerHasIMAP4Rev1Capability())
			{
				PRBool useArbitraryHeaders = GetShouldDownloadArbitraryHeaders();	// checks filter headers, etc.
				if (/***** Fix me *** gOptimizedHeaders &&	*/// preference -- able to turn it off
					useArbitraryHeaders)	// if it's ok -- no filters on any header, etc.
				{
					char *headersToDL = NULL;
					char *arbitraryHeaders = GetArbitraryHeadersToDownload();
					if (arbitraryHeaders)
					{
						headersToDL = PR_smprintf("%s %s",IMAP_DB_HEADERS,arbitraryHeaders);
						PR_Free(arbitraryHeaders);
					}
					else
					{
						headersToDL = PR_smprintf("%s",IMAP_DB_HEADERS);
					}
					if (headersToDL)
					{
						char *what = PR_smprintf(" BODY.PEEK[HEADER.FIELDS (%s)])", headersToDL);
						if (what)
						{
							commandString.Append(" %s (UID RFC822.SIZE FLAGS");
							commandString.Append(what);
							PR_Free(what);
						}
						else
						{
							commandString.Append(" %s (UID RFC822.SIZE BODY.PEEK[HEADER] FLAGS)");
						}
						PR_Free(headersToDL);
					}
					else
					{
						commandString.Append(" %s (UID RFC822.SIZE BODY.PEEK[HEADER] FLAGS)");
					}
				}
				else
					commandString.Append(" %s (UID RFC822.SIZE BODY.PEEK[HEADER] FLAGS)");
			}
			else
				commandString.Append(" %s (UID RFC822.SIZE RFC822.HEADER FLAGS)");
            break;
        case kUid:
			commandString.Append(" %s (UID)");
            break;
        case kFlags:
			commandString.Append(" %s (FLAGS)");
            break;
        case kRFC822Size:
			commandString.Append(" %s (RFC822.SIZE)");
			break;
		case kRFC822HeadersOnly:
			if (GetServerStateParser().ServerHasIMAP4Rev1Capability())
			{
				if (part)
				{
					commandString.Append(" %s (BODY[");
					char *what = PR_smprintf("%s.HEADER])", part);
					if (what)
					{
						commandString.Append(what);
						PR_Free(what);
					}
					else
						HandleMemoryFailure();
				}
				else
				{
					// headers for the top-level message
					commandString.Append(" %s (BODY[HEADER])");
				}
			}
			else
				commandString.Append(" %s (RFC822.HEADER)");
			break;
		case kMIMEPart:
			commandString.Append(" %s (BODY[%s]");
			if (endByte > 0)
			{
				// if we are retrieving chunks
				char *byterangeString = PR_smprintf("<%ld.%ld>",startByte,endByte);
				if (byterangeString)
				{
					commandString.Append(byterangeString);
					PR_Free(byterangeString);
				}
			}
			commandString.Append(")");
			break;
		case kMIMEHeader:
			commandString.Append(" %s (BODY[%s.MIME])");
    		break;
	};

	commandString.Append(CRLF);

		// since messageIds can be infinitely long, use a dynamic buffer rather than the fixed one
	const char *commandTag = GetServerCommandTag();
	int protocolStringSize = commandString.Length() + messageIds.Length() + PL_strlen(commandTag) + 1 +
		(part ? PL_strlen(part) : 0);
	char *protocolString = (char *) PR_CALLOC( protocolStringSize );
    
    if (protocolString)
    {
		char *cCommandStr = commandString.ToNewCString();
		char *cMessageIdsStr = messageIds.ToNewCString();

		if ((whatToFetch == kMIMEPart) ||
			(whatToFetch == kMIMEHeader))
		{
			PR_snprintf(protocolString,                                      // string to create
					protocolStringSize,                                      // max size
					cCommandStr,                                   // format string
					commandTag,                          // command tag
					cMessageIdsStr,
					part);
		}
		else
		{
			PR_snprintf(protocolString,                                      // string to create
					protocolStringSize,                                      // max size
					cCommandStr,                                   // format string
					commandTag,                          // command tag
					cMessageIdsStr);
		}
	            
	    int                 ioStatus = SendData(protocolString);
	    
		delete [] cCommandStr;
		delete [] cMessageIdsStr;

		ParseIMAPandCheckForNewMail(protocolString);
	    PR_Free(protocolString);
   	}
    else
        HandleMemoryFailure();
}

void nsImapProtocol::FetchTryChunking(nsString2 &messageIds,
                                            nsIMAPeFetchFields whatToFetch,
                                            PRBool idIsUid,
											char *part,
											PRUint32 downloadSize)
{
	GetServerStateParser().SetTotalDownloadSize(downloadSize);
	if (m_fetchByChunks &&
        GetServerStateParser().ServerHasIMAP4Rev1Capability() &&
		(downloadSize > (PRUint32) m_chunkThreshold))
	{
		PRUint32 startByte = 0;
		GetServerStateParser().ClearLastFetchChunkReceived();
		while (!DeathSignalReceived() && !GetPseudoInterrupted() && 
			!GetServerStateParser().GetLastFetchChunkReceived() &&
			GetServerStateParser().ContinueParse())
		{
			PRUint32 sizeToFetch = startByte + m_chunkSize > downloadSize ?
				downloadSize - startByte : m_chunkSize;
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
}


void nsImapProtocol::PipelinedFetchMessageParts(nsString2 &uid, nsIMAPMessagePartIDArray *parts)
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

		char *commandString = PR_smprintf("%s UID fetch %s (%s)%s",
                                          GetServerCommandTag(), uid,
                                          stringToFetch, CRLF);

		if (commandString)
		{
			int ioStatus = SendData(commandString);
			ParseIMAPandCheckForNewMail(commandString);
			PR_Free(commandString);
		}
		else
			HandleMemoryFailure();

		PR_Free(stringToFetch);
	}
}

void
nsImapProtocol::AddXMozillaStatusLine(uint16 /* flags */) // flags not use now
{
	static char statusLine[] = "X-Mozilla-Status: 0201\r\n";
	HandleMessageDownLoadLine(statusLine, FALSE);
}

void
nsImapProtocol::PostLineDownLoadEvent(msg_line_info *downloadLineDontDelete)
{
    NS_ASSERTION(downloadLineDontDelete, 
                 "Oops... null msg line info not good");
    if (m_imapMessage && downloadLineDontDelete)
        m_imapMessage->ParseAdoptedMsgLine(this, downloadLineDontDelete);

    // ***** We need to handle the psuedo interrupt here *****
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
    char *localMessageLine = (char *) PR_CALLOC(strlen(line) + 3);
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

	const char *xSenderInfo = GetServerStateParser().GetXSenderInfo();

	if (xSenderInfo && *xSenderInfo && !m_fromHeaderSeen)
	{
		if (!PL_strncmp("From: ", localMessageLine, 6))
		{
			m_fromHeaderSeen = TRUE;
			if (PL_strstr(localMessageLine, xSenderInfo) != NULL)
				AddXMozillaStatusLine(0);
			GetServerStateParser().FreeXSenderInfo();
		}
	}
    // if this line is for a different message, or the incoming line is too big
    if (((m_downloadLineCache.CurrentUID() != GetServerStateParser().CurrentResponseUID()) && !m_downloadLineCache.CacheEmpty()) ||
        (m_downloadLineCache.SpaceAvailable() < (PL_strlen(localMessageLine) + 1)) )
    {
		if (!m_downloadLineCache.CacheEmpty())
		{
			msg_line_info *downloadLineDontDelete = m_downloadLineCache.GetCurrentLineInfo();
			PostLineDownLoadEvent(downloadLineDontDelete);
		}
		m_downloadLineCache.ResetCache();
	}
     
    // so now the cache is flushed, but this string might still be to big
    if (m_downloadLineCache.SpaceAvailable() < (PL_strlen(localMessageLine) + 1) )
    {
        // has to be dynamic to pass to other win16 thread
		msg_line_info *downLoadInfo = (msg_line_info *) PR_CALLOC(sizeof(msg_line_info));
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
		m_downloadLineCache.CacheLine(localMessageLine, GetServerStateParser().CurrentResponseUID());

	PR_FREEIF( localMessageLine);
}



void nsImapProtocol::NormalMessageEndDownload()
{
	Log("STREAM", "CLOSE", "Normal Message End Download Stream");

	if (m_trackingTime)
		AdjustChunkSize();
	if (!m_downloadLineCache.CacheEmpty())
	{
	    msg_line_info *downloadLineDontDelete = m_downloadLineCache.GetCurrentLineInfo();
	    PostLineDownLoadEvent(downloadLineDontDelete);
	    m_downloadLineCache.ResetCache();
    }

    if (m_imapMessage)
        m_imapMessage->NormalEndMsgWriteStream(this);

}

void nsImapProtocol::AbortMessageDownLoad()
{
	Log("STREAM", "CLOSE", "Abort Message  Download Stream");

	if (m_trackingTime)
		AdjustChunkSize();
	if (!m_downloadLineCache.CacheEmpty())
	{
	    msg_line_info *downloadLineDontDelete = m_downloadLineCache.GetCurrentLineInfo();
	    PostLineDownLoadEvent(downloadLineDontDelete);
	    m_downloadLineCache.ResetCache();
    }

    if (m_imapMessage)
        m_imapMessage->AbortMsgWriteStream(this);

}


void nsImapProtocol::ProcessMailboxUpdate(PRBool handlePossibleUndo)
{
	if (DeathSignalReceived())
		return;
    // fetch the flags and uids of all existing messages or new ones
    if (!DeathSignalReceived() && GetServerStateParser().NumberOfMessages())
    {
    	if (handlePossibleUndo)
    	{
	    	// undo any delete flags we may have asked to
	    	char *undoIds;
			
			GetCurrentUrl()->CreateListOfMessageIdsString(&undoIds);
	    	if (undoIds && *undoIds)
	    	{
				nsString2 undoIds2(undoIds + 1);

	    		// if this string started with a '-', then this is an undo of a delete
	    		// if its a '+' its a redo
	    		if (*undoIds == '-')
					Store(undoIds2, "-FLAGS (\\Deleted)", TRUE);	// most servers will fail silently on a failure, deal with it?
	    		else  if (*undoIds == '+')
					Store(undoIds2, "+FLAGS (\\Deleted)", TRUE);	// most servers will fail silently on a failure, deal with it?
				else 
					NS_ASSERTION(FALSE, "bogus undo Id's");
			}
			PR_FREEIF(undoIds);
		}
    	
        // make the parser record these flags
		nsString2 fetchStr;
		PRInt32 added = 0, deleted = 0;

		nsresult res = m_flagState.GetNumberOfMessages(&added);
		deleted = m_flagState.GetNumberOfDeletedMessages();

		if (!added || (added == deleted))
		{
			nsString2 idsToFetch("1:*");
			FetchMessage(idsToFetch, kFlags, TRUE);	// id string shows uids
			// lets see if we should expunge during a full sync of flags.
			if (!DeathSignalReceived())	// only expunge if not reading messages manually and before fetching new
			{
				// ### TODO read gExpungeThreshhold from prefs.
				if ((m_flagState.GetNumberOfDeletedMessages() >= 20/* gExpungeThreshold */)  /*&& 
					GetDeleteIsMoveToTrash() */)
					Expunge();	// might be expensive, test for user cancel
			}

		}
		else {
			fetchStr.Append(GetServerStateParser().HighestRecordedUID() + 1, 10);
			fetchStr.Append(":*");

			// sprintf(fetchStr, "%ld:*", GetServerStateParser().HighestRecordedUID() + 1);
			FetchMessage(fetchStr, kFlags, TRUE);			// only new messages please
		}
    }
    else if (!DeathSignalReceived())
    	GetServerStateParser().ResetFlagInfo(0);
        
    mailbox_spec *new_spec = GetServerStateParser().CreateCurrentMailboxSpec();
	if (new_spec && !DeathSignalReceived())
	{
		if (!DeathSignalReceived())
		{
			nsIImapUrl::nsImapAction imapAction; 
			nsresult res = m_runningUrl->GetImapAction(&imapAction);
			if (NS_SUCCEEDED(res) && imapAction == nsIImapUrl::nsImapExpungeFolder)
				new_spec->box_flags |= kJustExpunged;
			PR_EnterMonitor(m_waitForBodyIdsMonitor);
			UpdatedMailboxSpec(new_spec);
		}
	}
	else if (!new_spec)
		HandleMemoryFailure();
    
    // Block until libmsg decides whether to download headers or not.
    PRUint32 *msgIdList = NULL;
    PRUint32 msgCount = 0;
    
	if (!DeathSignalReceived())
	{
		WaitForPotentialListOfMsgsToFetch(&msgIdList, msgCount);

		if (new_spec)
			PR_ExitMonitor(m_waitForBodyIdsMonitor);

		if (msgIdList && !DeathSignalReceived() && GetServerStateParser().LastCommandSuccessful())
		{
			FolderHeaderDump(msgIdList, msgCount);
			PR_FREEIF( msgIdList);
		}
			// this might be bogus, how are we going to do pane notification and stuff when we fetch bodies without
			// headers!
	}
    // wait for a list of bodies to fetch. 
    if (!DeathSignalReceived() && GetServerStateParser().LastCommandSuccessful())
    {
        WaitForPotentialListOfMsgsToFetch(&msgIdList, msgCount);
        if ( msgCount && !DeathSignalReceived() && GetServerStateParser().LastCommandSuccessful())
    	{
    		FolderMsgDump(msgIdList, msgCount, kEveryThingRFC822Peek);
    		PR_FREEIF(msgIdList);
    	}
	}
	if (DeathSignalReceived())
		GetServerStateParser().ResetFlagInfo(0);
}

void nsImapProtocol::UpdatedMailboxSpec(mailbox_spec *aSpec)
{
	m_imapMailFolder->UpdateImapMailboxInfo(this, aSpec);
}

void nsImapProtocol::FolderHeaderDump(PRUint32 *msgUids, PRUint32 msgCount)
{
	FolderMsgDump(msgUids, msgCount, kHeadersRFC822andUid);
	
    if (GetServerStateParser().NumberOfMessages())
        HeaderFetchCompleted();
}

void nsImapProtocol::FolderMsgDump(PRUint32 *msgUids, PRUint32 msgCount, nsIMAPeFetchFields fields)
{
#if 0
	// lets worry about this progress stuff later.
	switch (fields) {
	case TIMAP4BlockingConnection::kHeadersRFC822andUid:
		fProgressStringId =  XP_RECEIVING_MESSAGE_HEADERS_OF;
		break;
	case TIMAP4BlockingConnection::kFlags:
		fProgressStringId =  XP_RECEIVING_MESSAGE_FLAGS_OF;
		break;
	default:
		fProgressStringId =  XP_FOLDER_RECEIVING_MESSAGE_OF;
		break;
	}
	
	fProgressIndex = 0;
	fProgressCount = msgCount;
#endif // 0
	FolderMsgDumpLoop(msgUids, msgCount, fields);
	
//	fProgressStringId = 0;
}

void nsImapProtocol::WaitForPotentialListOfMsgsToFetch(PRUint32 **msgIdList, PRUint32 &msgCount)
{
	PRIntervalTime sleepTime = kImapSleepTime;

    PR_EnterMonitor(m_fetchMsgListMonitor);
    while(!m_fetchMsgListIsNew && !DeathSignalReceived())
        PR_Wait(m_fetchMsgListMonitor, sleepTime);
    m_fetchMsgListIsNew = FALSE;

    *msgIdList = m_fetchMsgIdList;
    msgCount   = m_fetchCount;
    
    PR_ExitMonitor(m_fetchMsgListMonitor);
}

#if 0

void nsImapProtocol::NotifyKeyList(PRUint32 *keys, PRUint32 keyCount)
#endif

// libmsg uses this to notify a running imap url about the message headers it should
// download while opening a folder. Generally, the imap thread will be blocked 
// in WaitForPotentialListOfMsgsToFetch waiting for this notification.
NS_IMETHODIMP nsImapProtocol::NotifyHdrsToDownload(PRUint32 *keys, PRUint32 keyCount)
{
    PR_EnterMonitor(m_fetchMsgListMonitor);
    m_fetchMsgIdList = keys;
    m_fetchCount  	= keyCount;
    m_fetchMsgListIsNew = TRUE;
    PR_Notify(m_fetchMsgListMonitor);
    PR_ExitMonitor(m_fetchMsgListMonitor);
	return NS_OK;
}

void nsImapProtocol::FolderMsgDumpLoop(PRUint32 *msgUids, PRUint32 msgCount, nsIMAPeFetchFields fields)
{
//   	PastPasswordCheckEvent();

	PRUint32 msgCountLeft = msgCount;
	PRUint32 msgsDownloaded = 0;
	do 
	{
		nsString2 idString;

		PRUint32 msgsToDownload = (msgCountLeft > 200) ? 200 : msgCountLeft;
   		AllocateImapUidString(msgUids + msgsDownloaded, msgsToDownload, idString);	// 20 * 200

		// except I don't think this works ### DB
		FetchMessage(idString,  fields, TRUE);                  // msg ids are uids                 

		msgsDownloaded += msgsToDownload;
		msgCountLeft -= msgsDownloaded;

   	}
	while (msgCountLeft > 0);
}   	
   	

void nsImapProtocol::HeaderFetchCompleted()
{
    if (m_imapMiscellaneous)
        m_imapMiscellaneous->HeaderFetchCompleted(this);
	WaitForFEEventCompletion();
}


// Use the noop to tell the server we are still here, and therefore we are willing to receive
// status updates. The recent or exists response from the server could tell us that there is
// more mail waiting for us, but we need to check the flags of the mail and the high water mark
// to make sure that we do not tell the user that there is new mail when perhaps they have
// already read it in another machine.

void nsImapProtocol::PeriodicBiff()
{
	
	nsMsgBiffState startingState = m_currentBiffState;
	
	if (GetServerStateParser().GetIMAPstate() == nsImapServerResponseParser::kFolderSelected)
    {
		Noop();	// check the latest number of messages
		PRInt32 numMessages = 0;
		m_flagState.GetNumberOfMessages(&numMessages);
        if (GetServerStateParser().NumberOfMessages() != numMessages)
        {
			PRUint32 id = GetServerStateParser().HighestRecordedUID() + 1;
			nsString2 fetchStr(eOneByte);						// only update flags
			PRUint32 added = 0, deleted = 0;
			
			deleted = m_flagState.GetNumberOfDeletedMessages();
			added = numMessages;
			if (!added || (added == deleted))	// empty keys, get them all
				id = 1;

			//sprintf(fetchStr, "%ld:%ld", id, id + GetServerStateParser().NumberOfMessages() - fFlagState->GetNumberOfMessages());
			fetchStr.Append(id, 10);
			fetchStr.Append(":*"); 
			FetchMessage(fetchStr, kFlags, TRUE);

			if (((PRUint32) m_flagState.GetHighestNonDeletedUID() >= id) && m_flagState.IsLastMessageUnseen())
				m_currentBiffState = nsMsgBiffState_NewMail;
			else
				m_currentBiffState = nsMsgBiffState_NoMail;
        }
        else
            m_currentBiffState = nsMsgBiffState_NoMail;
    }
    else
    	m_currentBiffState = nsMsgBiffState_Unknown;
    
    if (startingState != m_currentBiffState)
    	SendSetBiffIndicatorEvent(m_currentBiffState);
}

void nsImapProtocol::SendSetBiffIndicatorEvent(nsMsgBiffState newState)
{
    m_imapMiscellaneous->SetBiffStateAndUpdate(this, newState);

 	if (newState == nsMsgBiffState_NewMail)
		m_mailToFetch = TRUE;
	else
		m_mailToFetch = FALSE;
    WaitForFEEventCompletion();
}



// We get called to see if there is mail waiting for us at the server, even if it may have been
// read elsewhere. We just want to know if we should download headers or not.

PRBool nsImapProtocol::CheckNewMail()
{
	return m_checkForNewMailDownloadsHeaders;
}



void nsImapProtocol::AllocateImapUidString(PRUint32 *msgUids, PRUint32 msgCount, nsString2 &returnString)
{
	int blocksAllocated = 1;
	
	PRInt32 startSequence = (msgCount > 0) ? msgUids[0] : -1;
	PRInt32 curSequenceEnd = startSequence;
	PRUint32 total = msgCount;

	for (PRUint32 keyIndex=0; keyIndex < total; keyIndex++)
	{
		PRInt32 curKey = msgUids[keyIndex];
		PRInt32 nextKey = (keyIndex + 1 < total) ? msgUids[keyIndex + 1] : -1;
		PRBool lastKey = (nextKey == -1);

		if (lastKey)
			curSequenceEnd = curKey;
		if (nextKey == curSequenceEnd + 1 && !lastKey)
		{
			curSequenceEnd = nextKey;
			continue;
		}
		else if (curSequenceEnd > startSequence)
		{
			returnString.Append(startSequence, 10);
			returnString += ':';
			returnString.Append(curSequenceEnd, 10);
			if (!lastKey)
				returnString += ',';
//			sprintf(currentidString, "%ld:%ld,", startSequence, curSequenceEnd);
			startSequence = nextKey;
			curSequenceEnd = startSequence;
		}
		else
		{
			startSequence = nextKey;
			curSequenceEnd = startSequence;
			returnString.Append(msgUids[keyIndex], 10);
			if (!lastKey)
				returnString += ',';
//			sprintf(currentidString, "%ld,", msgUids[keyIndex]);
		}
	}
}

// log info including current state...
void nsImapProtocol::Log(const char *logSubName, const char *extraInfo, const char *logData)
{
	static char *nonAuthStateName = "NA";
	static char *authStateName = "A";
	static char *selectedStateName = "S";
	static char *waitingStateName = "W";
	char *stateName = NULL;
    const char *hostName = "";  // initilize to empty string
    if (m_runningUrl)
        m_runningUrl->GetHost(&hostName);
	switch (GetServerStateParser().GetIMAPstate())
	{
	case nsImapServerResponseParser::kFolderSelected:
		if (m_runningUrl)
		{
			if (extraInfo)
				PR_LOG(IMAP, PR_LOG_ALWAYS, ("%s:%s-%s:%s:%s: %s", hostName,selectedStateName, GetServerStateParser().GetSelectedMailboxName(), logSubName, extraInfo, logData));
			else
				PR_LOG(IMAP, PR_LOG_ALWAYS, ("%s:%s-%s:%s: %s", hostName,selectedStateName, GetServerStateParser().GetSelectedMailboxName(), logSubName, logData));
		}
		return;
		break;
	case nsImapServerResponseParser::kNonAuthenticated:
		stateName = nonAuthStateName;
		break;
	case nsImapServerResponseParser::kAuthenticated:
		stateName = authStateName;
		break;
#if 0 // *** this isn't a server state; its a status ***
	case nsImapServerResponseParser::kWaitingForMoreClientInput:
		stateName = waitingStateName;
		break;
#endif 
	}

	if (m_runningUrl)
	{
		if (extraInfo)
			PR_LOG(IMAP, PR_LOG_ALWAYS, ("%s:%s:%s:%s: %s", hostName,stateName,logSubName,extraInfo,logData));
		else
			PR_LOG(IMAP, PR_LOG_ALWAYS, ("%s:%s:%s: %s",hostName,stateName,logSubName,logData));
	}
}

// In 4.5, this posted an event back to libmsg and blocked until it got a response.
// We may still have to do this.It would be nice if we could preflight this value,
// but we may not always know when we'll need it.
PRUint32 nsImapProtocol::GetMessageSize(nsString2 &messageId, 
                                        PRBool idsAreUids)
{
	MessageSizeInfo *sizeInfo = 
        (MessageSizeInfo *)PR_CALLOC(sizeof(MessageSizeInfo));
	if (sizeInfo)
	{
		const char *folderFromParser =
            GetServerStateParser().GetSelectedMailboxName(); 
		if (folderFromParser)
		{
			sizeInfo->id = (char *)PR_CALLOC(messageId.Length() + 1);
			PL_strcpy(sizeInfo->id, messageId.GetBuffer());
			sizeInfo->idIsUid = idsAreUids;

			nsIMAPNamespace *nsForMailbox = nsnull;
            char *userName = GetImapUserName();
            m_hostSessionList->GetNamespaceForMailboxForHost(
                GetImapHostName(), userName, folderFromParser,
                nsForMailbox);
            PR_FREEIF(userName);

			char *nonUTF7ConvertedName = CreateUtf7ConvertedString(folderFromParser, FALSE);
			if (nonUTF7ConvertedName)
				folderFromParser = nonUTF7ConvertedName;

			if (nsForMailbox)
                m_runningUrl->AllocateCanonicalPath(
                    folderFromParser, nsForMailbox->GetDelimiter(),
                    &sizeInfo->folderName);
			else
                 m_runningUrl->AllocateCanonicalPath(
                    folderFromParser,kOnlineHierarchySeparatorUnknown,
                    &sizeInfo->folderName);
			PR_FREEIF(nonUTF7ConvertedName);

			if (sizeInfo->id && sizeInfo->folderName)
			{
                if (m_imapMessage)
                {
                    m_imapMessage->GetMessageSizeFromDB(this, sizeInfo);
                    WaitForFEEventCompletion();
                }

			}
            PR_FREEIF(sizeInfo->id);
            PR_FREEIF(sizeInfo->folderName);

			int32 rv = 0;
			if (!DeathSignalReceived())
				rv = sizeInfo->size;
			PR_Free(sizeInfo);
			return rv;
		}
    }
	else
	{
		HandleMemoryFailure();
	}
    return 0;
}


PRBool	nsImapProtocol::GetShowAttachmentsInline()
{
    PRBool *rv = (PRBool*) new PRBool(PR_FALSE);

    if (m_imapMiscellaneous && rv)
    {
        m_imapMiscellaneous->GetShowAttachmentsInline(this, rv);
        WaitForFEEventCompletion();
        return *rv;
    }
    return PR_FALSE;
}

/* static */PRBool nsImapProtocol::HandlingMultipleMessages(char *messageIdString)
{
	return (PL_strchr(messageIdString,',') != nsnull ||
		    PL_strchr(messageIdString,':') != nsnull);
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
    nsIMAPACLRightsInfo *aclRightsInfo = new nsIMAPACLRightsInfo();
	if (aclRightsInfo)
	{
		nsIMAPNamespace *namespaceForFolder = nsnull;
        char *userName = GetImapUserName();
        NS_ASSERTION (m_hostSessionList, "fatal ... null host session list");
        if (m_hostSessionList)
            m_hostSessionList->GetNamespaceForMailboxForHost(
                GetImapHostName(), userName, mailboxName,
                namespaceForFolder);
		PR_FREEIF(userName);

		aclRightsInfo->hostName = PL_strdup(GetImapHostName());

		char *nonUTF7ConvertedName = CreateUtf7ConvertedString(mailboxName, FALSE);
		if (nonUTF7ConvertedName)
			mailboxName = nonUTF7ConvertedName;

		if (namespaceForFolder)
            m_runningUrl->AllocateCanonicalPath(
                mailboxName,
                namespaceForFolder->GetDelimiter(), 
                &aclRightsInfo->mailboxName);
		else
            m_runningUrl->AllocateCanonicalPath(mailboxName,
                          kOnlineHierarchySeparatorUnknown, 
                          &aclRightsInfo->mailboxName);

		PR_FREEIF(nonUTF7ConvertedName);
		if (userName)
			aclRightsInfo->userName = PL_strdup(userName);
		else
			aclRightsInfo->userName = NULL;
		aclRightsInfo->rights = PL_strdup(rights);
		

		if (aclRightsInfo->hostName && 
            aclRightsInfo->mailboxName && aclRightsInfo->rights && 
			userName ? (aclRightsInfo->userName != NULL) : TRUE)
		{
            if (m_imapExtension)
            {
                m_imapExtension->AddFolderRights(this, aclRightsInfo);
                WaitForFEEventCompletion();
            }
        }
        PR_FREEIF(aclRightsInfo->hostName);
        PR_FREEIF(aclRightsInfo->mailboxName);
        PR_FREEIF(aclRightsInfo->rights);
        PR_FREEIF(aclRightsInfo->userName);

		delete aclRightsInfo;
	}
	else
		HandleMemoryFailure();
}


void nsImapProtocol::CommitNamespacesForHostEvent()
{
    if (m_imapMiscellaneous)
    {
        m_imapMiscellaneous->CommitNamespaces(this, GetImapHostName());
        WaitForFEEventCompletion();
    }
}

// notifies libmsg that we have new capability data for the current host
void nsImapProtocol::CommitCapabilityForHostEvent()
{
    if (m_imapMiscellaneous)
    {
        m_imapMiscellaneous->CommitCapabilityForHost(this, GetImapHostName());
        WaitForFEEventCompletion();
    }
}

// rights is a single string of rights, as specified by RFC2086, the IMAP ACL extension.
// Clears all rights for a given folder, for all users.
void nsImapProtocol::ClearAllFolderRights(const char *mailboxName,
                                          nsIMAPNamespace *nsForMailbox)
{
	NS_ASSERTION (nsForMailbox, "Oops ... null name space");
    nsIMAPACLRightsInfo *aclRightsInfo = new nsIMAPACLRightsInfo();
	if (aclRightsInfo)
	{
		char *nonUTF7ConvertedName = CreateUtf7ConvertedString(mailboxName, FALSE);
		if (nonUTF7ConvertedName)
			mailboxName = nonUTF7ConvertedName;

        const char *hostName = "";
        m_runningUrl->GetHost(&hostName);

		aclRightsInfo->hostName = PL_strdup(hostName);
		if (nsForMailbox)
            m_runningUrl->AllocateCanonicalPath(mailboxName,
                                                nsForMailbox->GetDelimiter(),
                                                &aclRightsInfo->mailboxName); 
		else
            m_runningUrl->AllocateCanonicalPath(
                mailboxName, kOnlineHierarchySeparatorUnknown,
                &aclRightsInfo->mailboxName);

		PR_FREEIF(nonUTF7ConvertedName);

		aclRightsInfo->rights = NULL;
		aclRightsInfo->userName = NULL;

		if (aclRightsInfo->hostName && aclRightsInfo->mailboxName)
		{
            if (m_imapExtension)
            {
                m_imapExtension->ClearFolderRights(this, aclRightsInfo);
		        WaitForFEEventCompletion();
            }
        }
        PR_FREEIF(aclRightsInfo->hostName);
        PR_FREEIF(aclRightsInfo->mailboxName);

		delete aclRightsInfo;
	}
	else
		HandleMemoryFailure();
}

// the design for this method has an inherit bug: if the length of the line is greater than the size of aDataBufferSize, 
// then we'll never find the next line because we can't hold the whole line in memory. 
char * nsImapProtocol::ReadNextLineFromInput(char * aDataBuffer, PRUint32 aDataBufferSize, nsIInputStream * aInputStream)
{
	// try to extract a line from m_inputBuffer. If we don't have an entire line, 
	// then read more bytes out from the stream. If the stream is empty then wait
	// on the monitor for more data to come in.

	do
	{
		char * endOfLine = nsnull;
		PRUint32 numBytesInBuffer = PL_strlen(aDataBuffer);

		if (numBytesInBuffer > 0) // any data in our internal buffer?
		{
			endOfLine = PL_strstr(aDataBuffer, CRLF); // see if we already have a line ending...
		}

		// it's possible that we got here before the first time we receive data from the server
		// so m_inputStream will be nsnull...
		if (!endOfLine && m_inputStream) // get some more data from the server
		{
			PRUint32 numBytesInStream = 0;
			PRUint32 numBytesCopied = 0;
			m_inputStream->GetLength(&numBytesInStream);
			PRUint32 numBytesToCopy = PR_MIN(aDataBufferSize - numBytesInBuffer - 1, numBytesInStream);
			// read the data into the end of our data buffer
			if (numBytesToCopy > 0)
			{
				m_inputStream->Read(aDataBuffer + numBytesInBuffer, numBytesToCopy, &numBytesCopied);
				Log("OnDataAvailable", nsnull, aDataBuffer + numBytesInBuffer);
				aDataBuffer[numBytesCopied] = '\0';
			}

			// okay, now that we've tried to read in more data from the stream, look for another end of line 
			// character 
			endOfLine = PL_strstr(aDataBuffer, CRLF);
		}

		// okay, now check again for endOfLine.
		if (endOfLine)
		{
			endOfLine += 2; // count for CRLF
			// PR_CALLOC zeros out the allocated line
			char* newLine = (char*) PR_CALLOC(endOfLine-aDataBuffer+1);
			if (!newLine)
				return nsnull;

			nsCRT::memcpy(newLine, aDataBuffer, endOfLine-aDataBuffer); // copy the string into the new line buffer
			// now we need to shift the rest of the data in the buffer down into the base
			if (PL_strlen(endOfLine) <= 0) // if no more data in the buffer, then just zero out the buffer...
				aDataBuffer[0] = '\0';
			else
			{
				nsCRT::memcpy(aDataBuffer, endOfLine, PL_strlen(endOfLine)); 
				aDataBuffer[PL_strlen(endOfLine)] = '\0';
			}
			return newLine;
		}
		else // we were unable to extract the next line and we now need to wait for data!
		{

			// mscott: this is bogus....fortunately we haven't gotten in this spot yet.
			// we need to be pumping events here instead of just blocking on the on data available call.
			// If we don't pump events how will we get the on data available signal?

			// wait on the data available monitor!!
			PR_EnterMonitor(m_dataAvailableMonitor);
			// wait for data arrival
			PR_Wait(m_dataAvailableMonitor, PR_INTERVAL_NO_TIMEOUT);
			PR_ExitMonitor(m_dataAvailableMonitor);
			// once data has arrived, recursively 
		}
	} while (!DeathSignalReceived()); // mscott - i'd like to have some way of dropping out of here if we aren't going to find a new line...

	return nsnull; // if we somehow got here....
}

char* 
nsImapProtocol::CreateNewLineFromSocket()
{
	char * newLine = ReadNextLineFromInput(m_dataInputBuf, OUTPUT_BUFFER_SIZE, m_inputStream);
	SetConnectionStatus(newLine ? PL_strlen(newLine) : 0);
	return newLine;
#if 0
    NS_PRECONDITION(m_curReadIndex < m_totalDataSize && m_dataOutputBuf, 
                    "Oops ... excceeding total data size");
    if (!m_dataOutputBuf || m_curReadIndex >= m_totalDataSize)
        return nsnull;

    char* startOfLine = m_dataOutputBuf + m_curReadIndex;
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
    m_curReadIndex = endOfLine - m_dataOutputBuf;

    SetConnectionStatus(endOfLine-startOfLine);
    return newLine;
#endif
}

PRInt32
nsImapProtocol::GetConnectionStatus()
{
    // ***?? I am not sure we really to guard with monitor for 5.0 ***
    PRInt32 status;
	// mscott -- do we need these monitors? as i was debuggin this I continually
	// locked when entering this monitor...control would never return from this
	// function...
//    PR_CEnterMonitor(this);
    status = m_connectionStatus;
//    PR_CExitMonitor(this);
    return status;
}

void
nsImapProtocol::SetConnectionStatus(PRInt32 status)
{
//    PR_CEnterMonitor(this);
    m_connectionStatus = status;
//    PR_CExitMonitor(this);
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

void nsImapProtocol::SetMailboxDiscoveryStatus(EMailboxDiscoverStatus status)
{
    PR_EnterMonitor(GetDataMemberMonitor());
	m_discoveryStatus = status;
    PR_ExitMonitor(GetDataMemberMonitor());
}

EMailboxDiscoverStatus nsImapProtocol::GetMailboxDiscoveryStatus( )
{
	EMailboxDiscoverStatus returnStatus;
    PR_EnterMonitor(GetDataMemberMonitor());
	returnStatus = m_discoveryStatus;
    PR_ExitMonitor(GetDataMemberMonitor());
    
    return returnStatus;
}

PRBool
nsImapProtocol::GetSubscribingNow()
{
    // ***** code me *****
    return PR_TRUE;// ***** for now
}

void
nsImapProtocol::DiscoverMailboxSpec(mailbox_spec * adoptedBoxSpec)  
{
	// IMAP_LoadTrashFolderName(); **** needs to work on localization issues

	nsIMAPNamespace *ns = nsnull;
    const char* hostName = GetImapHostName();
    char *userName = GetImapUserName();

    NS_ASSERTION (m_hostSessionList, "fatal null host session list");
    if (!m_hostSessionList) return;

    m_hostSessionList->GetDefaultNamespaceOfTypeForHost(
        hostName, userName, kPersonalNamespace, ns);
	const char *nsPrefix = ns ? ns->GetPrefix() : 0;

	nsString2 canonicalSubDir(eOneByte, 0);
	if (nsPrefix)
	{
        PRUnichar slash = '/';
		canonicalSubDir = nsPrefix;
		if (canonicalSubDir.Length() && canonicalSubDir.Last() == slash)
            canonicalSubDir.SetCharAt((PRUnichar)0, canonicalSubDir.Length());
	}
		
    switch (m_hierarchyNameState)
	{
	case kNoOperationInProgress:
	case kDiscoverTrashFolderInProgress:
	case kListingForInfoAndDiscovery:
		{
            if (canonicalSubDir.Length() &&
                PL_strstr(adoptedBoxSpec->allocatedPathName,
                          canonicalSubDir.GetBuffer()))
                m_onlineBaseFolderExists = TRUE;

            if (ns && nsPrefix)	// if no personal namespace, there can be no Trash folder
			{
                PRBool onlineTrashFolderExists = PR_FALSE;
                if (m_hostSessionList)
                    m_hostSessionList->GetOnlineTrashFolderExistsForHost(
                        hostName, userName, onlineTrashFolderExists);

				if (GetDeleteIsMoveToTrash() &&	// don't set the Trash flag if not using the Trash model
					!onlineTrashFolderExists && 
                    PL_strstr(adoptedBoxSpec->allocatedPathName, 
                              kImapTrashFolderName))
				{
					PRBool trashExists = PR_FALSE;
                    nsString2 trashMatch(eOneByte,0);
                    trashMatch = nsPrefix;
                    trashMatch += kImapTrashFolderName;
					{
						char *serverTrashName = nsnull;
                        m_runningUrl->AllocateCanonicalPath(
                            trashMatch.GetBuffer(),
                            ns->GetDelimiter(), &serverTrashName); 
						if (serverTrashName)
						{
							if (!PL_strcasecmp(nsPrefix, "INBOX."))	// need to special-case this because it should be case-insensitive
							{
#ifdef DEBUG
								NS_ASSERTION (PL_strlen(serverTrashName) > 6,
                                              "Oops.. less that 6");
#endif
								trashExists = ((PL_strlen(serverTrashName) > 6 /* XP_STRLEN("INBOX.") */) &&
									(PL_strlen(adoptedBoxSpec->allocatedPathName) > 6 /* XP_STRLEN("INBOX.") */) &&
									!PL_strncasecmp(adoptedBoxSpec->allocatedPathName, serverTrashName, 6) &&	/* "INBOX." */
									!PL_strcmp(adoptedBoxSpec->allocatedPathName + 6, serverTrashName + 6));
							}
							else
							{
								trashExists = (PL_strcmp(serverTrashName, adoptedBoxSpec->allocatedPathName) == 0);
							}
                            if (m_hostSessionList)
							m_hostSessionList->
                                SetOnlineTrashFolderExistsForHost(
                                    hostName, userName, trashExists);
							PR_Free(serverTrashName);
						}
					}
					
					if (trashExists)
	                	adoptedBoxSpec->box_flags |= kImapTrash;
				}
			}

			// Discover the folder (shuttle over to libmsg, yay)
			// Do this only if the folder name is not empty (i.e. the root)
			if (adoptedBoxSpec->allocatedPathName&&
                *adoptedBoxSpec->allocatedPathName)
			{
                nsString2 boxNameCopy (eOneByte, 0);
                
				boxNameCopy = adoptedBoxSpec->allocatedPathName;

                if (m_imapMailFolder)
                {
                    m_imapMailFolder->PossibleImapMailbox(this,
                                                          adoptedBoxSpec);
                    WaitForFEEventCompletion();
                
                    PRBool useSubscription = PR_FALSE;

                    if (m_hostSessionList)
                        m_hostSessionList->GetHostIsUsingSubscription(
                            hostName, userName,
                            useSubscription);

					if ((GetMailboxDiscoveryStatus() != eContinue) && 
						(GetMailboxDiscoveryStatus() != eContinueNew) &&
						(GetMailboxDiscoveryStatus() != eListMyChildren))
					{
                    	SetConnectionStatus(-1);
					}
					else if (boxNameCopy.Length() && 
                             (GetMailboxDiscoveryStatus() == 
                              eListMyChildren) &&
                             (!useSubscription || GetSubscribingNow()))
					{
						NS_ASSERTION (FALSE, 
                                      "we should never get here anymore");
                    	SetMailboxDiscoveryStatus(eContinue);
					}
					else if (GetMailboxDiscoveryStatus() == eContinueNew)
					{
						if (m_hierarchyNameState ==
                            kListingForInfoAndDiscovery &&
                            boxNameCopy.Length() && 
                            !adoptedBoxSpec->folderIsNamespace)
						{
							// remember the info here also
							nsIMAPMailboxInfo *mb = new
                                nsIMAPMailboxInfo(boxNameCopy.GetBuffer(),
                                    adoptedBoxSpec->hierarchySeparator); 
                            m_listedMailboxList.AppendElement((void*) mb);
						}
						SetMailboxDiscoveryStatus(eContinue);
					}
				}
			}
        }
        break;
    case kDiscoverBaseFolderInProgress:
        {
            if (canonicalSubDir.Length() &&
                PL_strstr(adoptedBoxSpec->allocatedPathName,
                          canonicalSubDir.GetBuffer()))
                m_onlineBaseFolderExists = TRUE;
        }
        break;
    case kDeleteSubFoldersInProgress:
        {
            m_deletableChildren.AppendElement((void*)
                adoptedBoxSpec->allocatedPathName);
			delete adoptedBoxSpec->flagState;
            PR_FREEIF( adoptedBoxSpec);
        }
        break;
	case kListingForInfoOnly:
		{
			//UpdateProgressWindowForUpgrade(adoptedBoxSpec->allocatedPathName);
			ProgressEventFunctionUsingIdWithString(
                /***** fix me **** MK_MSG_IMAP_DISCOVERING_MAILBOX */ -1,
                adoptedBoxSpec->allocatedPathName);
			nsIMAPMailboxInfo *mb = new
                nsIMAPMailboxInfo(adoptedBoxSpec->allocatedPathName,
                                  adoptedBoxSpec->hierarchySeparator);
			m_listedMailboxList.AppendElement((void*) mb);
            PR_FREEIF(adoptedBoxSpec->allocatedPathName);
            PR_FREEIF(adoptedBoxSpec);
		}
		break;
	case kDiscoveringNamespacesOnly:
		{
            PR_FREEIF(adoptedBoxSpec->allocatedPathName);
            PR_FREEIF(adoptedBoxSpec);
		}
		break;
    default:
        NS_ASSERTION (FALSE, "we aren't supposed to be here");
        break;
	}
    PR_FREEIF(userName);
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
    // ***** temporary **** Fix me ****
    if (aSourceString)
        return PL_strdup(aSourceString);
    else
        return nsnull;
}

	// imap commands issued by the parser
void
nsImapProtocol::Store(nsString2 &messageList, const char * messageData,
                      PRBool idsAreUid)
{
   IncrementCommandTagNumber();
    
    char *formatString;
    if (idsAreUid)
        formatString = "%s uid store %s %s\015\012";
    else
        formatString = "%s store %s %s\015\012";
        
    // we might need to close this mailbox after this
	m_closeNeededBeforeSelect = GetDeleteIsMoveToTrash() && (PL_strcasestr(messageData, "\\Deleted"));

	const char *commandTag = GetServerCommandTag();
	int protocolStringSize = PL_strlen(formatString) + PL_strlen(messageList.GetBuffer()) + PL_strlen(messageData) + PL_strlen(commandTag) + 1;
	char *protocolString = (char *) PR_CALLOC( protocolStringSize );

	if (protocolString)
	{
	    PR_snprintf(protocolString,                              // string to create
	            protocolStringSize,                              // max size
	            formatString,                                   // format string
	            commandTag,                  					// command tag
	            messageList,
	            messageData);
	            
	    int                 ioStatus = SendData(protocolString);
	    
		ParseIMAPandCheckForNewMail(protocolString); // ??? do we really need this
	    PR_Free(protocolString);
    }
    else
    	HandleMemoryFailure();
}

void
nsImapProtocol::Expunge()
{
    ProgressEventFunctionUsingId (/***** fix me **** MK_IMAP_STATUS_EXPUNGING_MAILBOX */ -1);
    IncrementCommandTagNumber();

	nsString2 command(GetServerCommandTag(), eOneByte);
    command.Append(" expunge"CRLF);
    
	SendData(command.GetBuffer());
	ParseIMAPandCheckForNewMail(); // ??? do we really need to do this
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

void nsImapProtocol::HandleCurrentUrlError()
{
#ifdef UNREADY_CODE
	if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kSelectFolder)
	{
		// let the front end know the select failed so they
		// don't leave the view without a database.
		mailbox_spec *notSelectedSpec = (mailbox_spec *) XP_CALLOC(1, sizeof( mailbox_spec));
		if (notSelectedSpec)
		{
			notSelectedSpec->allocatedPathName = fCurrentUrl->CreateCanonicalSourceFolderPathString();
			notSelectedSpec->hostName = fCurrentUrl->GetUrlHost();
			notSelectedSpec->folderSelected = FALSE;
			notSelectedSpec->flagState = NULL;
			notSelectedSpec->onlineVerified = FALSE;
			UpdatedMailboxSpec(notSelectedSpec);
		}
	}
	else if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kOfflineToOnlineMove)
	{
		OnlineCopyCompleted(kFailedCopy);
	}
#endif
}

void nsImapProtocol::Capability()
{

//    ProgressEventFunction_UsingId (MK_IMAP_STATUS_CHECK_COMPAT);
    IncrementCommandTagNumber();
	nsString2 command(GetServerCommandTag(), eOneByte);

    command.Append(" capability" CRLF);
            
    int ioStatus = SendData(command.GetBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::InsecureLogin(const char *userName, const char *password)
{

#ifdef UNREADY_CODE
    ProgressEventFunction_UsingId (MK_IMAP_STATUS_SENDING_LOGIN);
#endif
    IncrementCommandTagNumber();
	nsString2 command (GetServerCommandTag(), eOneByte);
	command.Append(" login \"");
	command.Append(userName);
	command.Append("\" \"");
	command.Append(password);
	command.Append("\""CRLF);

	SendData(command.GetBuffer());

    
//    PR_snprintf(m_dataOutputBuf,  OUTPUT_BUFFER_SIZE, "%s login \"%s\" \"%s\"" CRLF,
//				GetServerCommandTag(), userName,  password);

//	SendData(m_dataOutputBuf);
	ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::AuthLogin(const char *userName, const char *password, eIMAPCapabilityFlag flag)
{
#ifdef UNREADY_CODE
	ProgressEventFunction_UsingId (MK_IMAP_STATUS_SENDING_AUTH_LOGIN);
#endif
    IncrementCommandTagNumber();

	const char *currentTagToken = GetServerCommandTag();
	char * currentCommand;
    
	if (flag & kHasAuthPlainCapability)
	{
		PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s authenticate plain" CRLF, GetServerCommandTag());
		SendData(m_dataOutputBuf);

		currentCommand = PL_strdup(m_dataOutputBuf); /* StrAllocCopy(currentCommand, GetOutputBuffer()); */
		ParseIMAPandCheckForNewMail();
		if (GetServerStateParser().LastCommandSuccessful()) 
		{
			char plainstr[512]; // placeholder for "<NUL>userName<NUL>password"
			int len = 1; // count for first <NUL> char
			nsCRT::memset(plainstr, 0, 512);
			PR_snprintf(&plainstr[1], 510, "%s", userName);
			len += PL_strlen(userName);
			len++;	// count for second <NUL> char
			PR_snprintf(&plainstr[len], 511-len, "%s", password);
			len += PL_strlen(password);
		
			char *base64Str = PL_Base64Encode(plainstr, len, nsnull);
			if (base64Str)
			{
				PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s" CRLF, base64Str);
				PR_Free(base64Str);
				SendData(m_dataOutputBuf);
				ParseIMAPandCheckForNewMail(currentCommand);
				if (GetServerStateParser().LastCommandSuccessful())
				{
					PR_FREEIF(currentCommand);
					return;
				} // if the last command succeeded
			} // if we got a base 64 encoded string
		} // if the last command succeeded
	} // if auth plain capability

	else if (flag & kHasAuthLoginCapability)
	{
		PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s authenticate login" CRLF, GetServerCommandTag());
		SendData(m_dataOutputBuf);

		currentCommand = PL_strdup(m_dataOutputBuf);
		ParseIMAPandCheckForNewMail();

		if (GetServerStateParser().LastCommandSuccessful()) 
		{
			char *base64Str = PL_Base64Encode(userName, PL_strlen(userName), nsnull);
			if(base64Str)
			{
				PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s" CRLF, base64Str);
				PR_Free(base64Str);
				SendData(m_dataOutputBuf);
				ParseIMAPandCheckForNewMail(currentCommand);
			}
			if (GetServerStateParser().LastCommandSuccessful()) 
			{
				base64Str = PL_Base64Encode((char*)password, PL_strlen(password), nsnull);
				PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s" CRLF, base64Str);
				PR_FREEIF(base64Str);
				SendData(m_dataOutputBuf);
				ParseIMAPandCheckForNewMail(currentCommand);
				if (GetServerStateParser().LastCommandSuccessful())
				{
					PR_FREEIF(currentCommand);
					return;
				}
			} // if last command successful
		} // if last command successful
	} // if has auth login capability

	// fall back to use InsecureLogin()
	InsecureLogin(userName, password);
	PR_FREEIF(currentCommand);
}

void nsImapProtocol::OnLSubFolders()
{
	// **** use to find out whether Drafts, Sent, & Templates folder
	// exists or not even the user didn't subscribe to it
	char *mailboxName = nsnull;
	m_runningUrl->CreateServerSourceFolderPathString(&mailboxName);
	if (mailboxName)
	{
		char *convertedName = CreateUtf7ConvertedString(mailboxName, PR_TRUE);
		if (convertedName)
		{
			PR_Free(mailboxName);
			mailboxName = convertedName;
		}
#ifdef UNREADY_CODE
		ProgressEventFunction_UsingId(MK_IMAP_STATUS_LOOKING_FOR_MAILBOX);
#endif
		IncrementCommandTagNumber();
		PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE,"%s list \"\" \"%s\"" CRLF, GetServerCommandTag(), mailboxName);
		SendData(m_dataOutputBuf);
#ifdef UNREADY_CODE
		TimeStampListNow();
#endif
		ParseIMAPandCheckForNewMail();	
		PR_Free(mailboxName);
	}
	else
	{
		HandleMemoryFailure();
	}

}

void nsImapProtocol::OnGetMailAccount()
{
	NS_ASSERTION(0, "unimplemented feature");
#ifdef UNREADY_CODE
	if (GetServerStateParser().GetCapabilityFlag() & kHasXNetscapeCapability) 
	{
		Netscape();
		if (GetServerStateParser().LastCommandSuccessful()) 
		{
			TImapFEEvent *alertEvent = 
				new TImapFEEvent(msgSetMailAccountURL,  // function to call
								 this,                // access to current entry
								 (void *) fCurrentUrl->GetUrlHost(),
								 PR_TRUE);
			if (alertEvent)
			{
				fFEEventQueue->AdoptEventToEnd(alertEvent);
				// WaitForFEEventCompletion();
			}
			else
				HandleMemoryFailure();
		}
	}
#endif
}

void nsImapProtocol::OnOfflineToOnlineMove()
{
	NS_ASSERTION(0, "unimplemented feature");
#ifdef UNREADY_CODE
    char *destinationMailbox = fCurrentUrl->CreateServerDestinationFolderPathString();
    
	if (destinationMailbox)
	{
		char *convertedName = CreateUtf7ConvertedString(destinationMailbox, TRUE);
		XP_FREE(destinationMailbox);
		destinationMailbox = convertedName;
	}
    if (destinationMailbox)
    {
        uint32 appendSize = 0;
        do {
            WaitForNextAppendMessageSize();
            appendSize = GetAppendSize();
            if (!DeathSignalReceived() && appendSize)
            {
                char messageSizeString[100];
                sprintf(messageSizeString, "%ld",(long) appendSize);
                AppendMessage(destinationMailbox, messageSizeString, GetAppendFlags());
            }
        } while (appendSize && GetServerStateParser().LastCommandSuccessful());
        FREEIF( destinationMailbox);
    }
    else
        HandleMemoryFailure();
#endif
}

void nsImapProtocol::OnAppendMsgFromFile()
{
	NS_ASSERTION(0, "unimplemented feature");
#ifdef UNREADY_CODE
	char *sourceMessageFile =
		XP_STRDUP(GetActiveEntry()->URL_s->post_data);
    char *mailboxName =  
        fCurrentUrl->CreateServerSourceFolderPathString();
    
	if (mailboxName)
	{
		char *convertedName = CreateUtf7ConvertedString(mailboxName, TRUE);
		XP_FREE(mailboxName);
		mailboxName = convertedName;
	}
    if (mailboxName)
    {
		UploadMessageFromFile(sourceMessageFile, mailboxName,
							  kImapMsgSeenFlag);
    }
    else
        HandleMemoryFailure();
	FREEIF( sourceMessageFile );
    FREEIF( mailboxName );
#endif
}

//caller must free using PR_Free
char * nsImapProtocol::OnCreateServerSourceFolderPathString()
{
	char *sourceMailbox = nsnull;
	m_runningUrl->CreateServerSourceFolderPathString(&sourceMailbox);
	if (sourceMailbox)
	{
		char *convertedName = CreateUtf7ConvertedString(sourceMailbox, PR_TRUE);
		PR_Free(sourceMailbox);
		sourceMailbox = convertedName;
	}

	return sourceMailbox;
}

void nsImapProtocol::OnCreateFolder(const char * aSourceMailbox)
{
	NS_ASSERTION(0, "on create folder is not implemented yet");
#ifdef UNREADY_CODE
           PR_Bool created = CreateMailboxRespectingSubscriptions(aSourceMailbox);
            if (created)
			{
				List(aSourceMailbox, PR_FALSE);
			}
			else
				FolderNotCreated(aSourceMailbox);
#endif
}

void nsImapProtocol::OnSubscribe(const char * aSourceMailbox)
{
	NS_ASSERTION(0, "subscribe is not implemented yet");
#ifdef UNREADY_CODE
	Subscribe(sourceMailbox);
#endif
}

void nsImapProtocol::OnUnsubscribe(const char * aSourceMailbox)
{
	// When we try to auto-unsubscribe from \Noselect folders,
	// some servers report errors if we were already unsubscribed
	// from them.
#ifdef UNREADY_CODE
	PRBool lastReportingErrors = GetServerStateParser().GetReportingErrors();
	GetServerStateParser().SetReportingErrors(FALSE);
	Unsubscribe(sourceMailbox);
	GetServerStateParser().SetReportingErrors(lastReportingErrors);
#endif
}

void nsImapProtocol::OnRefreshACLForFolder(const char *mailboxName)
{
    IncrementCommandTagNumber();

	nsString2 command(GetServerCommandTag(), eOneByte);
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    command.Append(" getacl \"");
	command.Append(escapedName);
	command.Append("\"" CRLF);
            
    PR_FREEIF( escapedName);
            
    int ioStatus = SendData(command.GetBuffer());
    
    ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::OnRefreshAllACLs()
{
	// mscott - haven't ported this yet
}

// any state commands
void nsImapProtocol::Logout()
{
//	ProgressEventFunction_UsingId (MK_IMAP_STATUS_LOGGING_OUT);
	IncrementCommandTagNumber();

	nsString2 command(GetServerCommandTag(), eOneByte);

	command.Append("logout" CRLF);

	int ioStatus = SendData(command.GetBuffer());

	// the socket may be dead before we read the response, so drop it.
	ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Noop()
{
    //ProgressUpdateEvent("noop...");
    IncrementCommandTagNumber();
	nsString2 command(GetServerCommandTag(), eOneByte);
    
	command.Append("noop" CRLF);
            
    int ioStatus = SendData(command.GetBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::XServerInfo()
{

//    ProgressEventFunction_UsingId (MK_IMAP_GETTING_SERVER_INFO);
    IncrementCommandTagNumber();
  	nsString2 command(GetServerCommandTag(), eOneByte);
  
	command.Append(" XSERVERINFO MANAGEACCOUNTURL MANAGELISTSURL MANAGEFILTERSURL" CRLF);
          
    int ioStatus = SendData(command.GetBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::XMailboxInfo(const char *mailboxName)
{

//    ProgressEventFunction_UsingId (MK_IMAP_GETTING_MAILBOX_INFO);
    IncrementCommandTagNumber();
  	nsString2 command(GetServerCommandTag(), eOneByte);

	command.Append(" XMAILBOXINFO \"");
	command.Append(mailboxName);
	command.Append("\"  MANAGEURL POSTURL" CRLF);
            
    int ioStatus = SendData(command.GetBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Namespace()
{

//    ProgressEventFunction_UsingId (MK_IMAP_STATUS_GETTING_NAMESPACE);
    IncrementCommandTagNumber();
    
	nsString2 command(GetServerCommandTag(), eOneByte);
	command.Append(" namespace" CRLF);
            
    int ioStatus = SendData(command.GetBuffer());
    
	ParseIMAPandCheckForNewMail();
}


void nsImapProtocol::MailboxData()
{
    IncrementCommandTagNumber();

	nsString2 command(GetServerCommandTag(), eOneByte);
	command.Append(" mailboxdata" CRLF);
            
    int ioStatus = SendData(command.GetBuffer());
	ParseIMAPandCheckForNewMail();
}


void nsImapProtocol::GetMyRightsForFolder(const char *mailboxName)
{
    IncrementCommandTagNumber();

	nsString2 command(GetServerCommandTag(), eOneByte);
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
	command.Append(" myrights \"");
	command.Append(escapedName);
	command.Append("\"" CRLF);
            
    PR_FREEIF( escapedName);
            
    int ioStatus = SendData(command.GetBuffer());
    
    ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::OnStatusForFolder(const char *mailboxName)
{
    IncrementCommandTagNumber();

	nsString2 command(GetServerCommandTag(), eOneByte);
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
	command.Append(" STATUS \"");
	command.Append(escapedName);
	command.Append("\" (UIDNEXT MESSAGES UNSEEN)" CRLF);
            
    PR_FREEIF( escapedName);
            
    int ioStatus = SendData(command.GetBuffer());
    
    ParseIMAPandCheckForNewMail();

    mailbox_spec *new_spec = GetServerStateParser().CreateCurrentMailboxSpec(mailboxName);
#ifdef HAVE_PORT
	if (new_spec)
		UpdateMailboxStatus(new_spec);

	if (new_spec && m_imapMailFolder)
		m_imapMailFolder->UpdateImapMailboxStatus(this, new_spec);
#endif
}


void nsImapProtocol::OnListFolder(const char * aSourceMailbox, PRBool aBool)
{
#ifdef UNREADY_CODE
	ListFolder();
#endif
}

void nsImapProtocol::OnUpgradeToSubscription()
{
#ifdef UNREADY_CODE
	UpgradeToSubscription();
#endif
}

void nsImapProtocol::OnDeleteFolder(const char * aSourceMailbox)
{
#ifdef UNREADY_CODE
    PRBool deleted = DeleteSubFolders(sourceMailbox);
    if (deleted)
        deleted = DeleteMailboxRespectingSubscriptions(sourceMailbox);
	if (deleted)
		FolderDeleted(sourceMailbox);
#endif
}

void nsImapProtocol::OnRenameFolder(const char * aSourceMailbox)
{
#ifdef UNREADY_CODE
	char *destinationMailbox = nsnull;
    m_runningUrl->CreateServerDestinationFolderPathString(&destinationMailbox);
	if (destinationMailbox)
	{
		char *convertedName = CreateUtf7ConvertedString(destinationMailbox, PR_TRUE);
		PR_Free(destinationMailbox);
		destinationMailbox = convertedName;
	}

	if (destinationMailbox)
	{
		PRBool renamed = RenameHierarchyByHand(sourceMailbox, destinationMailbox);
		if (renamed)
			FolderRenamed(sourceMailbox, destinationMailbox);
        
		PR_Free( destinationMailbox);
	}
	else
		HandleMemoryFailure();
#endif
}

void nsImapProtocol::OnMoveFolderHierarchy(const char * aSourceMailbox)
{
#ifdef UNREADY_CODE
	char *destinationMailbox = nsnull;
	m_runningUrl->CreateServerDestinationFolderPathString(&destinationMailbox);
    if (destinationMailbox)
    {
		char *convertedName = CreateUtf7ConvertedString(destinationMailbox, PR_TRUE);
		PR_Free(destinationMailbox);
		destinationMailbox = convertedName;

        // worst case length
        char *newBoxName = (char *) PR_ALLOC(PL_Strlen(destinationMailbox) + PL_Strlen(sourceMailbox) + 2);
        if (newBoxName)
        {
            char onlineDirSeparator = fCurrentUrl->GetOnlineSubDirSeparator();
            XP_STRCPY(newBoxName, destinationMailbox);
            
            char *leafSeparator = XP_STRRCHR(sourceMailbox, onlineDirSeparator);
            if (!leafSeparator)
                leafSeparator = sourceMailbox;	// this is a root level box
            else
                leafSeparator++;
            
            if ( *newBoxName && ( *(newBoxName + XP_STRLEN(newBoxName) - 1) != onlineDirSeparator))
            {
                char separatorStr[2];
                separatorStr[0] = onlineDirSeparator;
                separatorStr[1] = 0;
                XP_STRCAT(newBoxName, separatorStr);
            }
            XP_STRCAT(newBoxName, leafSeparator);
			XP_Bool renamed = RenameHierarchyByHand(sourceMailbox, newBoxName);
	        if (renamed)
	            FolderRenamed(sourceMailbox, newBoxName);
        }
    }
    else
    	HandleMemoryFailure();
#endif
}

void nsImapProtocol::ProcessAuthenticatedStateURL()
{
	nsIImapUrl::nsImapAction imapAction;
	char * sourceMailbox = nsnull;
	m_runningUrl->GetImapAction(&imapAction);

	// switch off of the imap url action and take an appropriate action
	switch (imapAction)
	{
		case nsIImapUrl::nsImapLsubFolders:
			OnLSubFolders();
			return;
			break;
		case nsIImapUrl::nsImapGetMailAccountUrl:
			OnGetMailAccount();
			return;
			break;
		default: 
			break;
	}

	// all of the other states require the following extra step...
    // even though we don't have to to be legal protocol, Close any select mailbox
    // so we don't miss any info these urls might change for the selected folder
    // (e.g. msg count after append)
	if (GetServerStateParser().GetIMAPstate() == nsImapServerResponseParser::kFolderSelected)
	{
		// now we should be avoiding an implicit Close because it performs an implicit Expunge
		// authenticated state urls should not use a cached connection that is in the selected state
		// However, append message can happen either in a selected state or authenticated state
		if (imapAction != nsIImapUrl::nsImapAppendMsgFromFile /* kAppendMsgFromFile */)
		{
			PR_ASSERT(FALSE);
		}
	}

	switch (imapAction)
	{
		case nsIImapUrl::nsImapOfflineToOnlineMove:
			OnOfflineToOnlineMove();
			break;
		case nsIImapUrl::nsImapAppendMsgFromFile:
			OnAppendMsgFromFile();
			break;
		case nsIImapUrl::nsImapDiscoverAllBoxesUrl:
#ifdef UNREADY_CODE
			PR_ASSERT(!GetSubscribingNow());	// should not get here from subscribe UI
			DiscoverMailboxList();
#endif
			break;
		case nsIImapUrl::nsImapDiscoverAllAndSubscribedBoxesUrl:
#ifdef UNREADY_CODE
			PR_ASSERT(GetSubscribingNow());
			DiscoverAllAndSubscribedBoxes();
#endif
			break;
		case nsIImapUrl::nsImapCreateFolder:
			sourceMailbox = OnCreateServerSourceFolderPathString();
			OnCreateFolder(sourceMailbox);
			break;
		case nsIImapUrl::nsImapDiscoverChildrenUrl:
#ifdef UNREADY_CODE
            char *canonicalParent = fCurrentUrl->CreateServerSourceFolderPathString();
            if (canonicalParent)
            {
				NthLevelChildList(canonicalParent, 2);
                PR_Free(canonicalParent);
            }
#endif
			break;
		case nsIImapUrl::nsImapDiscoverLevelChildrenUrl:
#ifdef UNREADY_CODE
            char *canonicalParent = fCurrentUrl->CreateServerSourceFolderPathString();
			int depth = fCurrentUrl->GetChildDiscoveryDepth();
   			if (canonicalParent)
			{
				NthLevelChildList(canonicalParent, depth);
				if (GetServerStateParser().LastCommandSuccessful())
					ChildDiscoverySucceeded();
				XP_FREE(canonicalParent);
			}
#endif
			break;
		case nsIImapUrl::nsImapSubscribe:
			sourceMailbox = OnCreateServerSourceFolderPathString();
			OnSubscribe(sourceMailbox); // used to be called subscribe
			break;
		case nsIImapUrl::nsImapUnsubscribe:	
			sourceMailbox = OnCreateServerSourceFolderPathString();			
			OnUnsubscribe(sourceMailbox);
			break;
		case nsIImapUrl::nsImapRefreshACL:
			sourceMailbox = OnCreateServerSourceFolderPathString();	
			OnRefreshACLForFolder(sourceMailbox);
			break;
		case nsIImapUrl::nsImapRefreshAllACLs:
			OnRefreshAllACLs();
			break;
		case nsIImapUrl::nsImapListFolder:
			sourceMailbox = OnCreateServerSourceFolderPathString();	
			OnListFolder(sourceMailbox, PR_FALSE);
			break;
		case nsIImapUrl::nsImapUpgradeToSubscription:
			OnUpgradeToSubscription();
			break;
		case nsIImapUrl::nsImapFolderStatus:
			sourceMailbox = OnCreateServerSourceFolderPathString();	
			OnStatusForFolder(sourceMailbox);
			break;
		case nsIImapUrl::nsImapRefreshFolderUrls:
			sourceMailbox = OnCreateServerSourceFolderPathString();
#ifdef UNREADY_CODE
			XMailboxInfo(sourceMailbox);
			if (GetServerStateParser().LastCommandSuccessful()) 
				InitializeFolderUrl(sourceMailbox);
#endif
			break;
		case nsIImapUrl::nsImapDeleteFolder:
			sourceMailbox = OnCreateServerSourceFolderPathString();
			OnDeleteFolder(sourceMailbox);
			break;
		case nsIImapUrl::nsImapRenameFolder:
			sourceMailbox = OnCreateServerSourceFolderPathString();
			OnRenameFolder(sourceMailbox);
			break;
		case nsIImapUrl::nsImapMoveFolderHierarchy:
			sourceMailbox = OnCreateServerSourceFolderPathString();
			OnMoveFolderHierarchy(sourceMailbox);
			break;
		default:
			break;
	}

	PR_FREEIF(sourceMailbox);
}

void nsImapProtocol::ProcessAfterAuthenticated()
{
	// mscott: ignore admin url stuff for now...
#ifdef UNREADY_CODE
	// if we're a netscape server, and we haven't got the admin url, get it
	if (!TIMAPHostInfo::GetHostHasAdminURL(fCurrentUrl->GetUrlHost()))
	{
		if (GetServerStateParser().GetCapabilityFlag() & kXServerInfoCapability)
		{
			XServerInfo();
			if (GetServerStateParser().LastCommandSuccessful()) 
			{
				TImapFEEvent *alertEvent = 
					new TImapFEEvent(msgSetMailServerURLs,  // function to call
									 this,                // access to current entry
									 (void *) fCurrentUrl->GetUrlHost(),
									 TRUE);            
				if (alertEvent)
				{
					fFEEventQueue->AdoptEventToEnd(alertEvent);
					// WaitForFEEventCompletion();
				}
				else
					HandleMemoryFailure();
			}
		}
		else if (GetServerStateParser().GetCapabilityFlag() & kHasXNetscapeCapability)
		{
			Netscape();
			if (GetServerStateParser().LastCommandSuccessful()) 
			{
				TImapFEEvent *alertEvent = 
					new TImapFEEvent(msgSetMailAccountURL,  // function to call
									 this,                // access to current entry
									 (void *) fCurrentUrl->GetUrlHost(),
									 TRUE);
				if (alertEvent)
				{
					fFEEventQueue->AdoptEventToEnd(alertEvent);
					// WaitForFEEventCompletion();
				}
				else
					HandleMemoryFailure();
			}
		}
	}
#endif

	if (GetServerStateParser().ServerHasNamespaceCapability())
	{
		PRBool nameSpacesOverridable = PR_FALSE;
		PRBool haveNameSpacesForHost = PR_FALSE;
		m_hostSessionList->GetNamespacesOverridableForHost(GetImapHostName(), GetImapUserName(), nameSpacesOverridable);
		m_hostSessionList->GetGotNamespacesForHost(GetImapHostName(), GetImapUserName(), haveNameSpacesForHost); 

	// mscott: VERIFY THIS CLAUSE!!!!!!!
		if (nameSpacesOverridable && !haveNameSpacesForHost)
		{
			Namespace();
		}
	}
}

void nsImapProtocol::ProcessStoreFlags(const char *messageIds,
                                                 PRBool idsAreUids,
                                                 imapMessageFlagsType flags,
                                                 PRBool addFlags)
{
	if (!flags)
		return;
		
	nsString2  messageIdsString(messageIds, eOneByte);
    nsString2 flagString(eOneByte);

	uint16 userFlags = GetServerStateParser().SupportsUserFlags();
	uint16 settableFlags = GetServerStateParser().SettablePermanentFlags();

	if (!(flags & userFlags) && !(flags & settableFlags))
		return;					// if cannot set any of the flags bail out
    
    if (addFlags)
        flagString = "+Flags (";
    else
        flagString = "-Flags (";
    
    if (flags & kImapMsgSeenFlag && kImapMsgSeenFlag & settableFlags)
        flagString .Append("\\Seen ");
    if (flags & kImapMsgAnsweredFlag && kImapMsgAnsweredFlag & settableFlags)
        flagString .Append("\\Answered ");
    if (flags & kImapMsgFlaggedFlag && kImapMsgFlaggedFlag & settableFlags)
        flagString .Append("\\Flagged ");
    if (flags & kImapMsgDeletedFlag && kImapMsgDeletedFlag & settableFlags)
        flagString .Append("\\Deleted ");
    if (flags & kImapMsgDraftFlag && kImapMsgDraftFlag & settableFlags)
        flagString .Append("\\Draft ");
	if (flags & kImapMsgForwardedFlag && kImapMsgSupportForwardedFlag & userFlags)
        flagString .Append("$Forwarded ");	// if supported
	if (flags & kImapMsgMDNSentFlag && kImapMsgSupportMDNSentFlag & userFlags)
        flagString .Append("$MDNSent ");	// if supported

    // replace the final space with ')'
    flagString.SetCharAt(flagString.Length() - 1, ')');;
    
    Store(messageIdsString, flagString.GetBuffer(), idsAreUids);
}


void nsImapProtocol::Close()
{
    IncrementCommandTagNumber();

	nsString2 command(GetServerCommandTag(), eOneByte);
	command.Append(" close" CRLF);

//    ProgressEventFunction_UsingId (MK_IMAP_STATUS_CLOSE_MAILBOX);

	GetServerStateParser().ResetFlagInfo(0);
    
    int ioStatus = SendData(command.GetBuffer());
    
	ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Check()
{
    //ProgressUpdateEvent("Checking mailbox...");
    IncrementCommandTagNumber();
    
	nsString2 command(GetServerCommandTag(), eOneByte);
	command.Append(" check" CRLF);
            
    int ioStatus = SendData(command.GetBuffer());
    
	ParseIMAPandCheckForNewMail();
}

PRBool nsImapProtocol::TryToLogon()
{

	// mscott - for right now, I'm assuming the user name and password are in the
	// incoming server...no prompt for password dialog yet...

	PRInt32 logonTries = 0;
	PRBool loginSucceeded = PR_FALSE;
	char * password = nsnull;
	char * userName = nsnull;

	// get the password and user name for the current incoming server...
	nsIMsgIncomingServer * server = nsnull;
	nsresult rv = m_runningUrl->GetServer(&server);
	if (NS_SUCCEEDED(rv) && server)
	{
		rv = server->GetPassword(&password);
		rv = server->GetUserName(&userName);

	}

	do
	{
	    PRBool imapPasswordIsNew = PR_FALSE;
	    PRBool setUserAuthenticatedIsSafe = PR_FALSE;

	    if (userName && password)
	    {
			PRBool prefBool = PR_TRUE;

			PRBool lastReportingErrors = GetServerStateParser().GetReportingErrors();
			GetServerStateParser().SetReportingErrors(FALSE);	// turn off errors - we'll put up our own.
#ifdef UNREADY_CODE
			PREF_GetBoolPref("mail.auth_login", &prefBool);
#endif
			if (prefBool) 
			{
				if (GetServerStateParser().GetCapabilityFlag() == kCapabilityUndefined)
					Capability();

				if (GetServerStateParser().GetCapabilityFlag() & kHasAuthPlainCapability)
				{
					AuthLogin (userName, password, kHasAuthPlainCapability);
					logonTries++;
				}
				else if (GetServerStateParser().GetCapabilityFlag() & kHasAuthLoginCapability)
				{
					AuthLogin (userName, password,  kHasAuthLoginCapability); 
					logonTries++;	// I think this counts against most
									// servers as a logon try 
				}
				else
					InsecureLogin(userName, password);
			}
			else
				InsecureLogin(userName, password);

			if (!GetServerStateParser().LastCommandSuccessful())
	        {
	            // login failed!
	            // if we failed because of an interrupt, then do not bother the user
	            if (!DeathSignalReceived())
	            {
#ifdef UNREADY_CODE
					AlertUserEvent_UsingId(XP_MSG_IMAP_LOGIN_FAILED);
#endif
					// if we did get a password then remember so we don't have to prompt
					// the user for it again
		            if (password != nsnull)
		            {
						m_hostSessionList->SetPasswordForHost(GetImapHostName(), 
															  GetImapUserName(), password);
						PR_FREEIF(password);
		            }
#ifdef UNREADY_CODE
		            fCurrentBiffState = MSG_BIFF_Unknown;
		            SendSetBiffIndicatorEvent(fCurrentBiffState);
#endif
				} // if we didn't receive the death signal...
			} // if login failed
	        else	// login succeeded
	        {
				rv = m_hostSessionList->SetPasswordForHost(GetImapHostName(), 
						  								   GetImapUserName(), password);
				rv = m_hostSessionList->GetPasswordVerifiedOnline(GetImapHostName(), GetImapUserName(), imapPasswordIsNew);
				if (NS_SUCCEEDED(rv) && imapPasswordIsNew)
					m_hostSessionList->SetPasswordVerifiedOnline(GetImapHostName(), GetImapUserName());
#ifdef UNREADY_CODE
				NET_SetPopPassword2(passwordForHost); // bug 53380
#endif
				if (imapPasswordIsNew) 
				{
#ifdef UNREADY_CODE
	                if (fCurrentBiffState == MSG_BIFF_Unknown)
	                {
	                	fCurrentBiffState = MSG_BIFF_NoMail;
	                    SendSetBiffIndicatorEvent(fCurrentBiffState);
	                }
	                LIBNET_LOCK();
	                if (!DeathSignalReceived())
	                {
	                	setUserAuthenticatedIsSafe = GetActiveEntry()->URL_s->msg_pane != NULL;
						MWContext *context = GetActiveEntry()->window_id;
						if (context)
							FE_RememberPopPassword(context, SECNAV_MungeString(passwordForHost));
					}
					LIBNET_UNLOCK();
#endif
				}

				loginSucceeded = PR_TRUE;
			} // else if login succeeded
			
			GetServerStateParser().SetReportingErrors(lastReportingErrors);  // restore it

			if (loginSucceeded && imapPasswordIsNew)
			{
				// let's record the user as authenticated.
				m_imapExtension->SetUserAuthenticated(this, PR_TRUE);
			}

			if (loginSucceeded)
			{
				ProcessAfterAuthenticated();
			}
	    }
	    else
	    {
			// The user hit "Cancel" on the dialog box
	        //AlertUserEvent("Login cancelled.");
			HandleCurrentUrlError();
#ifdef UNREADY_CODE
			SetCurrentEntryStatus(-1);
	        SetConnectionStatus(-1);        // stop netlib
#endif
			break;
	    }
	}

	while (!loginSucceeded && ++logonTries < 4);

	PR_FREEIF(password);

	if (!loginSucceeded)
	{
#ifdef UNREADY_CODE
		fCurrentBiffState = MSG_BIFF_Unknown;
		SendSetBiffIndicatorEvent(fCurrentBiffState);
#endif
		HandleCurrentUrlError();
		SetConnectionStatus(-1);        // stop netlib
	}

	return loginSucceeded;
}

PRBool
nsImapProtocol::GetDeleteIsMoveToTrash()
{
    PRBool rv = PR_FALSE;
    char *userName = GetImapUserName();
    NS_ASSERTION (m_hostSessionList, "fatal... null host session list");
    if (m_hostSessionList)
        m_hostSessionList->GetDeleteIsMoveToTrashForHost(GetImapHostName(),
                                                         userName, rv);
    PR_FREEIF(userName);
    return rv;
}

nsIMAPMailboxInfo::nsIMAPMailboxInfo(const char *name, char delimiter) :
    m_mailboxName(eOneByte,0)
{
	m_mailboxName = name;
	m_delimiter = delimiter;
	m_childrenListed = FALSE;
}

nsIMAPMailboxInfo::~nsIMAPMailboxInfo()
{
}
