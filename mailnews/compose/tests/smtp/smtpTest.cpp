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

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsITransport.h"
#include "nsIURL.h"
#include "nsINetService.h"
#include "nsRepository.h"
#include "nsString.h"

#include "nsSmtpUrl.h"
#include "nsSmtpProtocol.h"

#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib.so"
#define XPCOM_DLL  "libxpcom.so"
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static NS_DEFINE_IID(kIInputStreamIID, NS_IINPUTSTREAM_IID);
static NS_DEFINE_IID(kIURLIID, NS_IURL_IID);
static NS_DEFINE_IID(kISmtpUrlIID, NS_ISMTPURL_IID);

static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

/////////////////////////////////////////////////////////////////////////////////
// Define default values to be used to drive the test
/////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_HOST	"nsmail-2.mcom.com"
#define DEFAULT_PORT	25		/* we get this value from SmtpCore.h */
#define DEFAULT_URL_TYPE  "mailto://"	/* do NOT change this value until netlib re-write is done...*/
#define DEFAULT_RECIPIENT "mscott@netscape.com"
#define DEFAULT_FILE	  "message.eml"
#define	DEFAULT_SENDER	  "qatest03@netscape.com"
#define DEFAULT_PASSWORD  "Ne!sc-pe"

//extern NET_StreamClass *MIME_MessageConverter(int format_out, void *closure, 
//											  URL_Struct *url, MWContext *context);

#ifdef XP_UNIX
extern "C" char *fe_GetConfigDir(void) {
  printf("XXX: return /tmp for fe_GetConfigDir\n");
  return PL_strdup("/tmp");
}
#endif /* XP_UNIX */

/////////////////////////////////////////////////////////////////////////////////
// This function is used to load and prepare an smtp url which can be run by
// a transport instance. For different protocols, you'll have different url
// functions like this one in the test harness...
/////////////////////////////////////////////////////////////////////////////////


nsresult NS_NewSmtpUrl(nsISmtpUrl ** aResult, const nsString urlSpec)
{
	nsresult rv = NS_OK;

	 nsSmtpUrl * smtpUrl = new nsSmtpUrl(nsnull, nsnull);
	 if (smtpUrl)
	 {
		smtpUrl->ParseURL(urlSpec);  // load the spec we were given...
		rv = smtpUrl->QueryInterface(kISmtpUrlIID, (void **) aResult);
	 }

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


//////////////////////////////////////////////////////////////////////////////////
// The nsSmtpTestDriver is a class that I envision could be generalized to form the
// building block of a protocol test harness. To configure it, you would list all of
// the events you know how to handle and when one of those events is triggered, you
// would be asked to process it....right now it is just Smtp specific....
///////////////////////////////////////////////////////////////////////////////////

class nsSmtpTestDriver
{
public:
	nsSmtpTestDriver(nsINetService * pService, PLEventQueue *queue);
	virtual ~nsSmtpTestDriver();

	// run driver initializes the instance, lists the commands, runs the command and when
	// the command is finished, it reads in the next command and continues...theoretically,
	// the client should only ever have to call RunDriver(). It should do the rest of the 
	// work....
	nsresult RunDriver(); 

	// User drive commands
	void InitializeTestDriver(); // will end up prompting the user for things like host, port, etc.
	nsresult ListCommands();   // will list all available commands to the user...i.e. "get groups, get article, etc."
	nsresult ReadAndDispatchCommand(); // reads a command number in from the user and calls the appropriate command generator
	nsresult PromptForUserDataAndBuildUrl(const char * userPrompt);

	// The following are event generators. They convert all of the available user commands into
	// URLs and then run the urls. 
	nsresult OnSendMessageInFile();
	nsresult OnExit(); 
protected:
    PLEventQueue *m_eventQueue;
	char m_urlSpec[200];	// "sockstub://hostname:port" it does not include the command specific data...
	char m_urlString[500];	// string representing the current url being run. Includes host AND command specific data.
	char m_userData[250];	// generic string buffer for storing the current user entered data...

	// host and port info...
	PRUint32	m_port;
	char		m_host[200];		

	nsISmtpUrl * m_url; 
	nsSmtpProtocol * m_SmtpProtocol; // running protocol instance
	nsITransport * m_transport; // a handle on the current transport object being used with the protocol binding...
	nsINetService * m_netService;

	PRBool		m_runningURL;	// are we currently running a url? this flag is set to false on exit...

	void InitializeProtocol(const char * urlSpec);
    nsresult SetupUrl(char *group);
	PRBool m_protocolInitialized; 
};

nsSmtpTestDriver::nsSmtpTestDriver(nsINetService * pNetService,
                                   PLEventQueue *queue)
{
	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_url = nsnull;
	m_protocolInitialized = PR_FALSE;
	m_runningURL = PR_TRUE;
    m_eventQueue = queue;
	m_netService = pNetService;
	NS_IF_ADDREF(m_netService); 
	
	InitializeTestDriver(); // prompts user for initialization information...

	m_transport = nsnull;
	m_SmtpProtocol = nsnull; // we can't create it until we have a url...
}

void nsSmtpTestDriver::InitializeProtocol(const char * urlString)
{
	// this is called when we don't have a url nor a protocol instance yet...
	if (m_url)
		m_url->Release(); // get rid of our old ref count...

	NS_NewSmtpUrl(&m_url, urlString);
	
	NS_IF_RELEASE(m_transport);
	
	// create a transport socket...
	m_netService->CreateSocketTransport(&m_transport, m_port, m_host);

	// now create a protocl instance...
	if (m_SmtpProtocol)
		delete m_SmtpProtocol; // delete our old instance
	
	// now create a new protocol instance...
	m_SmtpProtocol = new nsSmtpProtocol(m_url, m_transport);
	NS_IF_ADDREF(m_SmtpProtocol); 
	m_protocolInitialized = PR_TRUE;
}

nsSmtpTestDriver::~nsSmtpTestDriver()
{
	NS_IF_RELEASE(m_url);
	NS_IF_RELEASE(m_transport);
	NS_IF_RELEASE(m_netService); 
	if (m_SmtpProtocol) delete m_SmtpProtocol;
}

nsresult nsSmtpTestDriver::RunDriver()
{
	nsresult status = NS_OK;


	while (m_runningURL)
	{
		// if we haven't gotten started (and created a protocol) or
		// if the protocol instance is currently not busy, then read in a new command
		// and process it...
		if ((!m_SmtpProtocol) || m_SmtpProtocol->IsRunningUrl() == PR_FALSE) // if we aren't running the url anymore, ask ueser for another command....
		{
			//  SMTP is a connectionless protocol...so kill off our transport and current protocol
			//  each command must create its own transport and protocol instance to run the url...
			NS_IF_RELEASE(m_transport);
			if (m_SmtpProtocol)
			{
				NS_RELEASE(m_SmtpProtocol);
				m_SmtpProtocol = nsnull;
			}

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

void nsSmtpTestDriver::InitializeTestDriver()
{
	// prompt the user for port and host name 
	char portString[20];  // used to read in the port string
	char hostString[200];
	portString[0] = '\0';
	hostString[0] = '\0';
	m_host[0] = '\0';
	m_port = DEFAULT_PORT;

	// load default host name and set the start of the url
	PL_strcpy(m_host, DEFAULT_HOST);
	PL_strcpy(m_urlSpec, DEFAULT_URL_TYPE); // copy "sockstub://" part into url spec...

	// prompt user for port...
	printf("Enter port to use [%d]: ", m_port);
    fgets(portString, sizeof(portString), stdin);
    strip_nonprintable(portString);
	if (portString && *portString)
	{
		m_port = atoi(portString);
	}

	// now prompt for the host name....
	printf("Enter host name to use [%s]: ", m_host);
    fgets(hostString, sizeof(hostString), stdin);
    strip_nonprintable(hostString);
	if(hostString && *hostString)
	{
		PL_strcpy(m_host, hostString);
	}

	PL_strcat(m_urlSpec, m_host);
    // we'll actually build the url (spec + user data) once the user has specified a command they want to try...
}

// prints the userPrompt and then reads in the user data. Assumes urlData has already been allocated.
// it also reconstructs the url string in m_urlString but does NOT reload it....
nsresult nsSmtpTestDriver::PromptForUserDataAndBuildUrl(const char * userPrompt)
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

nsresult nsSmtpTestDriver::ReadAndDispatchCommand()
{
	nsresult status = NS_OK;
	PRInt32 command = 0; 
	char commandString[5];
	commandString[0] = '\0';

	printf("Enter command number: ");
    fgets(commandString, sizeof(commandString), stdin);
    strip_nonprintable(commandString);
	if (commandString && *commandString)
	{
		command = atoi(commandString);
	}

	// now switch on command to the appropriate 
	switch (command)
	{
	case 0:
		status = ListCommands();
		break;
	case 1:
		status = OnSendMessageInFile();
		break;
	default:
		status = OnExit();
		break;
	}

	return status;
}

nsresult nsSmtpTestDriver::ListCommands()
{
	printf("Commands currently available: \n");
	printf("0) List commands. \n");
	printf("1) Send a message in a file. \n");
	printf("9) Exit the test application. \n");
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////
// Begin protocol specific command url generation code...gee that's a mouthful....
///////////////////////////////////////////////////////////////////////////////////

nsresult nsSmtpTestDriver::OnExit()
{
	printf("Terminating Smtp test harness....\n");
	m_runningURL = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}

nsresult nsSmtpTestDriver::OnSendMessageInFile()
{
	nsresult rv = NS_OK; 
	char * fileName = nsnull;
	char * userName = nsnull;
	char * userPassword = nsnull;
	char * displayString = nsnull;
	
	PL_strcpy(m_userData, DEFAULT_FILE);

	displayString = PR_smprintf("Location of message [%s]: ", m_userData);
	rv = PromptForUserDataAndBuildUrl(displayString);
	PR_FREEIF(displayString);

	fileName = PL_strdup(m_userData);

	// now ask the user who to send the message too...
	PL_strcpy(m_userData, DEFAULT_RECIPIENT);
	displayString = PR_smprintf("Recipient of message [%s]: ", m_userData);
	rv = PromptForUserDataAndBuildUrl(displayString);
	PR_FREEIF(displayString);

	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "/");
	PL_strcat(m_urlString, m_userData); // tack on recipient...
	
	// now ask the user what their address is...
	PL_strcpy(m_userData, DEFAULT_SENDER);
	displayString = PR_smprintf("Email address of sender [%s]: ", m_userData);
	rv = PromptForUserDataAndBuildUrl(displayString);
	PR_FREEIF(displayString);

	userName = PL_strdup(m_userData);

	// SMTP is a connectionless protocol...so we always start with a new
	// SMTP protocol instance every time we launch a mailto url...

	InitializeProtocol(m_urlString);  // this creates a transport
	m_url->SetSpec(m_urlString); // reset spec
	m_url->SetHost(DEFAULT_HOST);
	
	// store the file name in the url...
	m_url->SetPostMessageFile(fileName);
	m_url->SetUserEmailAddress(userName);
	m_url->SetUserPassword(DEFAULT_PASSWORD);
	PR_FREEIF(fileName);
	PR_FREEIF(userName);
		
	printf("Running %s\n", m_urlString);
	rv = m_SmtpProtocol->LoadURL(m_url);
	return rv;
}

/////////////////////////////////////////////////////////////////////////////////
// End on command handlers for news
////////////////////////////////////////////////////////////////////////////////

int main()
{
	nsINetService * pNetService;
    PLEventQueue *queue;
    nsresult result;

    nsRepository::RegisterFactory(kNetServiceCID, NETLIB_DLL, PR_FALSE, PR_FALSE);
	nsRepository::RegisterFactory(kEventQueueServiceCID, XPCOM_DLL, PR_FALSE, PR_FALSE);

	// Create the Event Queue for this thread...
    nsIEventQueueService *pEventQService = nsnull;
    result = nsServiceManager::GetService(kEventQueueServiceCID,
                                          kIEventQueueServiceIID,
                                          (nsISupports **)&pEventQService);
	if (NS_SUCCEEDED(result)) {
      // XXX: What if this fails?
      result = pEventQService->CreateThreadEventQueue();
    }

	// ask the net lib service for a nsINetStream:
	result = NS_NewINetService(&pNetService, NULL);
	if (NS_FAILED(result) || !pNetService)
	{
		printf("unable to initialize net serivce. \n");
		return 1;
	}

    result =
        pEventQService->GetThreadEventQueue(PR_GetCurrentThread(),&queue);
    if (NS_FAILED(result) || !queue) {
        printf("unable to get event queue.\n");
        return 1;
    }
    
	// now register a mime converter....
    //	NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_NGLAYOUT, NULL, MIME_MessageConverter);
    //	NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_CACHE_AND_NGLAYOUT, NULL, MIME_MessageConverter);

	// okay, everything is set up, now we just need to create a test driver and run it...
	nsSmtpTestDriver * driver = new nsSmtpTestDriver(pNetService,queue);
	if (driver)
	{
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		delete driver;
	}

	// shut down:
	NS_RELEASE(pNetService);
    
    return 0;
}
