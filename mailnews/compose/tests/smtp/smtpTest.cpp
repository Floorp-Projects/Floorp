/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "msgCore.h"
#include "nsCOMPtr.h"
#include "prprf.h"

#include <stdio.h>
#include <assert.h>

#if defined(XP_PC) && !defined(XP_OS2)
#include <windows.h>
#endif

#include "plstr.h"
#include "plevent.h"

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsIComponentManager.h"
#include "nsString.h"

#include "nsISmtpService.h"
#include "nsISmtpUrl.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIUrlListener.h"

#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsIPref.h"
#include "nsIFileLocator.h"
#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsMsgCompCID.h"
#include "nsIMsgIdentity.h"

#ifdef XP_PC
#define XPCOM_DLL  "xpcom32.dll"
#define PREF_DLL   "xppref32.dll"
#define APPSHELL_DLL "appshell.dll"
#else
#ifdef XP_MAC
#else
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define PREF_DLL   "libpref"MOZ_DLL_SUFFIX
#define APPSHELL_DLL "libnsappshell"MOZ_DLL_SUFFIX
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);

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

class nsSmtpTestDriver : public nsIUrlListener
{
public:
	nsSmtpTestDriver(nsIEventQueue *queue);
	virtual ~nsSmtpTestDriver();

	NS_DECL_ISUPPORTS

	// nsIUrlListener support
	NS_IMETHOD OnStartRunningUrl(nsIURI * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode);

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
    nsIEventQueue *m_eventQueue;
	char m_urlSpec[200];	// "sockstub://hostname:port" it does not include the command specific data...
	char m_urlString[500];	// string representing the current url being run. Includes host AND command specific data.
	char m_userData[250];	// generic string buffer for storing the current user entered data...

	// host and port info...
	PRUint32	m_port;
	char		m_host[200];		
	PRBool		m_runningURL;
	PRBool		m_runTestHarness;

	nsISmtpService			*m_smtpService;
	nsCOMPtr<nsISmtpUrl>	 m_smtpUrl;
    
	nsresult SetupUrl(char *group);
	PRBool m_protocolInitialized; 
};

nsresult nsSmtpTestDriver::OnStartRunningUrl(nsIURI * aUrl)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	m_runningURL = PR_TRUE;
	return NS_OK;
}

nsresult nsSmtpTestDriver::OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	//nsresult rv = NS_OK;
	m_runningURL = PR_FALSE;
	if (aUrl)
	{
		// query it for a mailnews interface for now....
		nsCOMPtr<nsIMsgMailNewsUrl> mailUrl = do_QueryInterface(aUrl);
		if (mailUrl)
			mailUrl->UnRegisterListener(this);
	}

	///////////////////////////////////////////////////////////////////////////////////////////
	// the following strings are used by QA as part of the smoketest. DO NOT REMOVE THEM!!!!!!!
	///////////////////////////////////////////////////////////////////////////////////////////
	if (NS_SUCCEEDED(aExitCode))
		printf("\nMessage Sent: PASSED\n");
	else
		printf("\nMessage Sent: FAILED!\n");

	return NS_OK;
}

nsSmtpTestDriver::nsSmtpTestDriver(nsIEventQueue *queue)
{
	NS_INIT_REFCNT();
	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_protocolInitialized = PR_FALSE;
	m_runningURL = PR_FALSE;
	m_runTestHarness = PR_TRUE;
	m_eventQueue = queue;
	NS_IF_ADDREF(m_eventQueue);

	InitializeTestDriver(); // prompts user for initialization information...

	m_smtpService = nsnull;
	nsServiceManager::GetService(kSmtpServiceCID, NS_GET_IID(nsISmtpService),
                                 (nsISupports **)&m_smtpService); // XXX probably need shutdown listener here
}


nsSmtpTestDriver::~nsSmtpTestDriver()
{
	NS_IF_RELEASE(m_eventQueue);
	nsServiceManager::ReleaseService(kSmtpServiceCID, m_smtpService); // XXX probably need shutdown listener here
}

NS_IMPL_ISUPPORTS(nsSmtpTestDriver, NS_GET_IID(nsIUrlListener))

nsresult nsSmtpTestDriver::RunDriver()
{
	nsresult status = NS_OK;

	while (m_runTestHarness)
	{
		if (m_runningURL == PR_FALSE) // can we run and dispatch another command?
			status = ReadAndDispatchCommand();	 // run a new url

	 // if running url
#ifdef XP_UNIX

        m_eventQueue->ProcessPendingEvents();

#endif
#if defined(XP_PC) && !defined(XP_OS2)
	
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
	m_runTestHarness = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}

nsresult nsSmtpTestDriver::OnSendMessageInFile()
{
	nsresult rv = NS_OK; 
	char * fileName = nsnull;
	char * userName = nsnull;
	//char * userPassword = nsnull;
	char * displayString = nsnull;
	char * recipients = nsnull;
	
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

	recipients = PL_strdup(m_userData);

	// SMTP is a connectionless protocol...so we always start with a new
	// SMTP protocol instance every time we launch a mailto url...

	nsFilePath filePath (fileName);
	nsFileSpec aFileSpec (fileName);
	nsCOMPtr<nsIFileSpec> aIFileSpec;
	NS_NewFileSpecWithSpec(aFileSpec, getter_AddRefs(aIFileSpec));


	nsIURI * url = nsnull;
	nsCOMPtr <nsIMsgIdentity> senderIdentity;
	senderIdentity = null_nsCOMPtr();
	
	printf("passing in null for the sender identity will cause the send to fail, but at least it builds.  I need to talk to rhp / mscott about this.\n");

	m_smtpService->SendMailMessage(aIFileSpec, recipients, senderIdentity, this, nsnull, &url);
	if (url)
		m_smtpUrl = do_QueryInterface(url);

	NS_IF_RELEASE(url);


	PR_FREEIF(fileName);
	PR_FREEIF(userName);
	PR_FREEIF(recipients);
	return rv;
}

/////////////////////////////////////////////////////////////////////////////////
// End on command handlers for news
////////////////////////////////////////////////////////////////////////////////

int main()
{
    nsIEventQueue *queue;
    nsresult result;

	nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kEventQueueCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
	result = nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
	nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NS_FILELOCATOR_CONTRACTID, APPSHELL_DLL, PR_FALSE, PR_FALSE);

	result = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);

	// make sure prefs get initialized and loaded..
	// mscott - this is just a bad bad bad hack right now until prefs
	// has the ability to take nsnull as a parameter. Once that happens,
	// prefs will do the work of figuring out which prefs file to load...
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &result); 
    if (NS_FAILED(result) || (prefs == nsnull)) {
        exit(result);
    }
	prefs->StartUp();
    if (NS_FAILED(prefs->ReadUserPrefs()))
    {
      printf("Failed on reading user prefs!\n");
      exit(-1);
    }

	// Create the Event Queue for this thread...
	NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &result); 

	if (NS_FAILED(result)) return result;

    result = pEventQService->CreateThreadEventQueue();
	if (NS_FAILED(result)) return result;

    result = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,&queue);
    if (NS_FAILED(result) || !queue) {
        printf("unable to get event queue.\n");
        return 1;
    }

	// okay, everything is set up, now we just need to create a test driver and run it...
	nsSmtpTestDriver * driver = new nsSmtpTestDriver(queue);
	if (driver)
	{
		NS_ADDREF(driver);
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		NS_RELEASE(driver);
	}
	NS_IF_RELEASE(queue);
    return NS_OK;
}
