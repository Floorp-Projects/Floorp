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
#include "prprf.h"

#include <stdio.h>
#include <assert.h>

#ifdef XP_PC
#include <windows.h>
#endif

#include "plstr.h"
#include "plevent.h"

#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsString.h"

#include "nsIUrlListener.h"

#include "nsIPref.h"
#include "nsIImapUrl.h"
#include "nsIImapProtocol.h"
#include "nsIMsgIdentity.h"
#include "nsIMsgMailSession.h"

#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsFileSpec.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define PREF_DLL	 "xppref32.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib.so"
#define XPCOM_DLL  "libxpcom.so"
#define PREF_DLL	 "pref.so"   // mscott: is this right?
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);
static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

/////////////////////////////////////////////////////////////////////////////////
// Define default values to be used to drive the test
/////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_HOST		"nsmail-2.mcom.com"
#define DEFAULT_PORT		IMAP_PORT
#define DEFAULT_URL_TYPE	"imap://"	/* do NOT change this value until netlib re-write is done...*/

class nsIMAP4TestDriver  : public nsIUrlListener
{
public:
	NS_DECL_ISUPPORTS;

	// nsIUrlListener support
	NS_IMETHOD OnStartRunningUrl(nsIURL * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode);

	nsIMAP4TestDriver(PLEventQueue *queue);
	virtual ~nsIMAP4TestDriver();

	// run driver initializes the instance, lists the commands, runs the command and when
	// the command is finished, it reads in the next command and continues...theoretically,
	// the client should only ever have to call RunDriver(). It should do the rest of the 
	// work....
	nsresult RunDriver(); 

	// User drive commands
	nsresult ListCommands();   // will list all available commands to the user...i.e. "get groups, get article, etc."
	nsresult ReadAndDispatchCommand(); // reads a command number in from the user and calls the appropriate command generator
	nsresult PromptForUserData(const char * userPrompt);

	// command handlers
	nsresult OnCommand();   // send a command to the imap server
	nsresult OnRunIMAPCommand();
	nsresult OnGet();
	nsresult OnIdentityCheck();
	nsresult OnTestUrlParsing();
	nsresult OnExit(); 
protected:
	char m_command[500];	// command to run
	char m_urlString[500];	// string representing the current url being run. Includes host AND command specific data.
	char m_userData[250];	// generic string buffer for storing the current user entered data...
	char m_urlSpec[200];	// "imap://hostname:port/" it does not include the command specific data...

	// host and port info...
	PRUint32	m_port;
	char		m_host[200];		

	nsIImapUrl * m_url; 
	nsIImapProtocol * m_IMAP4Protocol; // running protocol instance

	PRBool	    m_runTestHarness;
	PRBool		m_runningURL;	// are we currently running a url? this flag is set to false when the url finishes...

	nsresult InitializeProtocol(const char * urlSpec);
	PRBool m_protocolInitialized; 
    PLEventQueue *m_eventQueue;

};

nsIMAP4TestDriver::nsIMAP4TestDriver(PLEventQueue *queue)
{
	NS_INIT_REFCNT();
	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_url = nsnull;
	m_protocolInitialized = PR_FALSE;
	m_runTestHarness = PR_TRUE;
	m_runningURL = PR_FALSE;
    m_eventQueue = queue;

	m_IMAP4Protocol = nsnull; // we can't create it until we have a url...

	// until we read from the prefs, just use the default, I guess.
	strcpy(m_urlSpec, DEFAULT_URL_TYPE);
	strcat(m_urlSpec, DEFAULT_HOST);
	strcat(m_urlSpec, "/");
}

NS_IMPL_ISUPPORTS(nsIMAP4TestDriver, nsIUrlListener::GetIID())

nsresult nsIMAP4TestDriver::InitializeProtocol(const char * urlString)
{
	nsresult rv = 0;

	// this is called when we don't have a url nor a protocol instance yet...
	// use service manager to get an imap 4 url...
	rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull, nsIImapUrl::GetIID(), (void **) &m_url);
	// now create a protocol instance...
	if (NS_SUCCEEDED(rv))
		rv = nsComponentManager::CreateInstance(kImapProtocolCID, nsnull, nsIImapProtocol::GetIID(), (void **) &m_IMAP4Protocol);

	if (NS_SUCCEEDED(rv))
		m_protocolInitialized = PR_TRUE;
	return rv;
}

nsIMAP4TestDriver::~nsIMAP4TestDriver()
{
	NS_IF_RELEASE(m_url);
	NS_IF_RELEASE(m_IMAP4Protocol);
}

nsresult nsIMAP4TestDriver::RunDriver()
{
	nsresult status = NS_OK;

	while (m_runTestHarness)
	{
		if (!m_runningURL) // if we aren't running the url anymore, ask ueser for another command....
		{
			NS_IF_RELEASE(m_url); // release the old one before we run a new one...
			status = ReadAndDispatchCommand();
		}  // if running url
#ifdef XP_UNIX

        PL_ProcessPendingEvents(m_eventQueue);

#endif
#ifdef XP_PC	
		MSG msg;
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
#endif

	} // until the user has stopped running the url (which is really the test session.....

	return status;
}

nsresult nsIMAP4TestDriver::ReadAndDispatchCommand()
{
	nsresult status = NS_OK;
	PRInt32 command = 0; 
	char commandString[5];
	commandString[0] = '\0';

	printf("Enter command number: ");
	scanf("%[^\n]", commandString);
	if (commandString && *commandString)
	{
		command = atoi(commandString);
	}
	scanf("%c", commandString);  // eat the extra CR

	// now switch on command to the appropriate 
	switch (command)
	{
	case 0:
		status = ListCommands();
		break;
	case 1:
		status = OnRunIMAPCommand();
		break;
	case 2:
		status = OnIdentityCheck();
		break;
	case 3:
		status = OnTestUrlParsing();
		break;
	default:
		status = OnExit();
		break;
	}

	return status;
}

nsresult nsIMAP4TestDriver::ListCommands()
{
	printf("Commands currently available: \n");
	printf("0) List available commands. \n");
	printf("1) Run IMAP Command. \n");
	printf("2) Check identity information.\n");
	printf("3) Test url parsing. \n");
	printf("9) Exit the test application. \n");
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////
// Begin protocol specific command url generation code...gee that's a mouthful....
///////////////////////////////////////////////////////////////////////////////////

nsresult nsIMAP4TestDriver::OnStartRunningUrl(nsIURL * aUrl)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	m_runningURL = PR_TRUE;
	return NS_OK;
}

nsresult nsIMAP4TestDriver::OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	nsresult rv = NS_OK;
	m_runningURL = PR_FALSE;
	if (aUrl)
	{
		// query it for a mailnews interface for now....
		nsIMsgMailNewsUrl * mailUrl = nsnull;
		rv = aUrl->QueryInterface(nsIMsgMailNewsUrl::GetIID(), (void **) mailUrl);
		if (NS_SUCCEEDED(rv))
		{
			mailUrl->UnRegisterListener(this);
			NS_RELEASE(mailUrl);
		}
	}

	return NS_OK;
}


nsresult nsIMAP4TestDriver::OnExit()
{
	printf("Terminating IMAP4 test harness....\n");
	m_runTestHarness = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

nsresult nsIMAP4TestDriver::OnIdentityCheck()
{
	nsIMsgMailSession * mailSession = nsnull;
	nsresult result = nsServiceManager::GetService(kCMsgMailSessionCID,
												   nsIMsgMailSession::GetIID(),
                                                   (nsISupports **) &mailSession);
	if (NS_SUCCEEDED(result) && mailSession)
	{
		nsIMsgIdentity * msgIdentity = nsnull;
		result = mailSession->GetCurrentIdentity(&msgIdentity);
		if (NS_SUCCEEDED(result) && msgIdentity)
		{
			const char * value = nsnull;
			msgIdentity->GetRootFolderPath(&value);
			printf("Root folder path: %s\n", value ? value : "");
			msgIdentity->GetUserFullName(&value);
			printf("User Name: %s\n", value ? value : "");
			msgIdentity->GetPopServer(&value);
			printf("Pop Server: %s\n", value ? value : "");
			msgIdentity->GetPopPassword(&value);
			printf("Pop Password: %s\n", value ? value : "");
			msgIdentity->GetSmtpServer(&value);
			printf("Smtp Server: %s\n", value ? value : "");

		}
		else
			printf("Unable to retrieve the msgIdentity....\n");

		nsServiceManager::ReleaseService(kCMsgMailSessionCID, mailSession);
	}
	else
		printf("Unable to retrieve the mail session service....\n");

	return result;
}
nsresult nsIMAP4TestDriver::OnRunIMAPCommand()
{
	nsresult rv = NS_OK;

	PL_strcpy(m_command, "LOGON");
	// prompt for the command to run ....
	printf("Enter IMAP command to run [%s]: ", m_command);
	scanf("%[^\n]", m_command);


	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, m_command);

	if (m_protocolInitialized == PR_FALSE)
		rv = InitializeProtocol(m_urlString);
	if (m_url)
		rv = m_url->SetSpec(m_urlString); // reset spec
	
	if (NS_SUCCEEDED(rv))
	{
		rv = m_IMAP4Protocol->LoadUrl(m_url, nsnull /* probably need a consumer... */);
	} // if user provided the data...

	return rv;
}

nsresult nsIMAP4TestDriver::OnGet()
{
	nsresult rv = NS_OK;

	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
#ifdef HAVE_SERVICE_MGR
	nsIIMAP4Service * IMAP4Service = nsnull;
	nsServiceManager::GetService(kIMAP4ServiceCID, nsIIMAP4Service::GetIID(),
                                 (nsISupports **)&IMAP4Service); // XXX probably need shutdown listener here

	if (IMAP4Service)
	{
		IMAP4Service->GetNewMail(nsnull, nsnull);
	}

	nsServiceManager::ReleaseService(kIMAP4ServiceCID, IMAP4Service);
#endif // HAVE_SERVICE_MGR
#if 0

	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);
	else
		rv = m_url->SetSpec(m_urlString); // reset spec
	
	// load the correct newsgroup interface as an event sink...
	if (NS_SUCCEEDED(rv))
	{
		 // before we re-load, assume it is a group command and configure our IMAP4URL correctly...

		rv = m_IMAP4Protocol->Load(m_url);
	} // if user provided the data...
#endif
	return rv;
}


/* strip out non-printable characters */
static void strip_nonprintable(char *string) {
    char *dest, *src;

    dest=src=string;
    while (*src) {
        if (isprint(*src)) {
            (*src)=(*dest);
            src++; dest++;
        } else {
            src++;
        }
    }
    (*dest)='\0';
}


// prints the userPrompt and then reads in the user data. Assumes urlData has already been allocated.
// it also reconstructs the url string in m_urlString but does NOT reload it....
nsresult nsIMAP4TestDriver::PromptForUserData(const char * userPrompt)
{
	char tempBuffer[500];
	tempBuffer[0] = '\0'; 

	if (userPrompt)
		printf(userPrompt);
	else
		printf("Enter data for command: ");

    fgets(tempBuffer, sizeof(tempBuffer), stdin);
    strip_nonprintable(tempBuffer);
	// only replace m_userData if the user actually entered a valid line...
	// this allows the command function to set a default value on m_userData before
	// calling this routine....
	if (tempBuffer && *tempBuffer)
		PL_strcpy(m_userData, tempBuffer);

	return NS_OK;
}


nsresult nsIMAP4TestDriver::OnTestUrlParsing()
{
	nsresult rv = NS_OK; 
	char * hostName = nsnull;
	PRUint32 port = IMAP_PORT;

	char * displayString = nsnull;
	
	PL_strcpy(m_userData, DEFAULT_HOST);

	displayString = PR_smprintf("Enter a host name for the imap url [%s]: ", m_userData);
	rv = PromptForUserData(displayString);
	PR_FREEIF(displayString);

	hostName = PL_strdup(m_userData);

	PL_strcpy(m_userData, "143");
	displayString = PR_smprintf("Enter port number if any for the imap url [%d] (use 0 to skip port field): ", IMAP_PORT);
	rv = PromptForUserData(displayString);
	PR_FREEIF(displayString);
	
	port = atol(m_userData);

	// as we had more functionality to the imap url, we'll probably need to ask for more information than just
	// the host and the port...

	nsIImapUrl * imapUrl = nsnull;
	
	nsComponentManager::CreateInstance(kImapUrlCID, nsnull /* progID */, nsIImapUrl::GetIID(), (void **) &imapUrl);
	if (imapUrl)
	{
		char * urlSpec = nsnull;
		if (m_port > 0) // did the user specify a port? 
			urlSpec = PR_smprintf("imap://%s:%d", hostName, port);
		else
			urlSpec = PR_smprintf("imap://%s", hostName);

		imapUrl->SetSpec("imap://nsmail-2.mcom.com:143/test");
		
		const char * urlHost = nsnull;
		PRUint32 urlPort = 0;

		imapUrl->GetHost(&urlHost);
		imapUrl->GetHostPort(&urlPort);

		printf("Host name test: %s\n", PL_strcmp(urlHost, hostName) == 0 ? "PASSED." : "FAILED!");
		if (port > 0) // did the user try to test the port?
			printf("Port test: %s\n", port == urlPort ? "PASSED." : "FAILED!");

		NS_IF_RELEASE(imapUrl);
	}
	else
		printf("Failure!! Unable to create an imap url. Registration problem? \n");

	PR_FREEIF(hostName);

	return rv;
}


int main()
{
    PLEventQueue *queue;
    nsresult result;

	// register all the components we might need - what's the imap service going to be called?
//    nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
//	nsComponentManager::RegisterComponent(kRDFServiceCID, nsnull, nsnull, RDF_DLL, PR_TRUE, PR_TRUE);
	nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
	// IMAP Service goes here?

	// Create the Event Queue for the test app thread...a standin for the ui thread
    nsIEventQueueService* pEventQService;
    result = nsServiceManager::GetService(kEventQueueServiceCID,
                                          nsIEventQueueService::GetIID(),
                                          (nsISupports**)&pEventQService);
	if (NS_FAILED(result)) return result;

    result = pEventQService->CreateThreadEventQueue();

	if (NS_FAILED(result)) return result;

    pEventQService->GetThreadEventQueue(PR_GetCurrentThread(),&queue);
    if (NS_FAILED(result) || !queue) 
	{
        printf("unable to get event queue.\n");
        return 1;
    }

	nsServiceManager::ReleaseService(kEventQueueServiceCID, pEventQService);

	// okay, everything is set up, now we just need to create a test driver and run it...
	nsIMAP4TestDriver * driver = new nsIMAP4TestDriver(queue);
	if (driver)
	{
		NS_ADDREF(driver);
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		NS_RELEASE(driver);
	}

	// shut down:
    return 0;

}
