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

///////////////////////////////////////////////////////////////////////////////
// This is at test harness for mailbox urls. Currently the only supported
// mailbox url is for parsing mail box folders. This will change as we add
// more functionality to the mailbox protocol code....
//
// For command (1) running a parse mailbox url:
//		You are prompted for the name of a mail box file you wish to parse
//		CURRENTLY this file name must be in the form of a file url:
//		i.e. "D|/mozilla/MailboxFile". We'll look into changing this after
//		I talk to folks about deriving our own file spec classes (david b.?)
//		Once this is done, we'll get rid of the file in the form of a url
//		requirement.....
//
// Currently I use a stub mailbox parser instance (nsMsgMailboxParserStub)
// which simply acknowledges that it is getting data from the mailbox protocol.
// It supports the stream listener interface. When you want to hook this up to
// a real mailbox parser simply create your mail box parser and pass it into the
// constructor of the mailbox test driver. This line change would occurr in the main
// when the mailbox test driver is created.
//
// Oh one more thing. Data is passed from the mailbox protocol to the mailbox parse
// through calls to ::OnDataAvailable which provides the mailbox parser with the url,
// stream and the number of bytes available to be read.
// A call to ::OnStopBinding is a signal to the mailbox parser that you've reached
// the end of the file.
//////////////////////////////////////////////////////////////////////////////////

#include "msgCore.h"
#include "prprf.h"

#include <stdio.h>
#include <assert.h>

#ifdef XP_PC
#include <windows.h>
#endif

#include "nsFileSpec.h"
#include "nsIPref.h"
#include "plstr.h"
#include "plevent.h"
#include "nsIServiceManager.h"
#include "nsIMailboxService.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsITransport.h"
#include "nsIURL.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsString.h"

#include "nsIMailboxUrl.h"
#include "nsMailboxUrl.h"
#include "nsMailboxProtocol.h"
#include "nsParseMailbox.h"
#include "nsMailDatabase.h"
#include "nsIUrlListener.h"
#include "nsIUrlListenerManager.h"

#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsRDFCID.h"

#ifdef XP_PC
#define NETLIB_DLL   "netlib.dll"
#define XPCOM_DLL    "xpcom32.dll"
#define LOCAL_DLL    "msglocal.dll"
#define RDF_DLL      "rdf.dll"
#define PREF_DLL	 "xppref32.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL   "libnetlib.so"
#define XPCOM_DLL    "libxpcom.so"
#define LOCAL_DLL	 "libmsglocal.so"
#define RDF_DLL      "librdf.so"
#define PREF_DLL	 "libpref.so"  
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_CID(kCUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);

static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static NS_DEFINE_IID(kIInputStreamIID, NS_IINPUTSTREAM_IID);
static NS_DEFINE_IID(kIURLIID, NS_IURL_IID);

static NS_DEFINE_CID(kCMailboxServiceCID, NS_MAILBOXSERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCMailboxParser, NS_MAILBOXPARSER_CID);

static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

/////////////////////////////////////////////////////////////////////////////////
// Define default values to be used to drive the test
/////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_URL_TYPE  "mailbox://"	
#define DEFAULT_MAILBOX "TestFolder"


#ifdef XP_UNIX
extern "C" char *fe_GetConfigDir(void) {
  printf("XXX: return /tmp for fe_GetConfigDir\n");
  return PL_strdup("/tmp");
}
#endif /* XP_UNIX */

////////////////////////////////////////////////////////////////////////////////
// This function is used to load and prepare a mailbox url which can be run by
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
// The nsMailboxTestDriver is a class that I envision could be generalized to form the
// building block of a protocol test harness. To configure it, you would list all of
// the events you know how to handle and when one of those events is triggered, you
// would be asked to process it....right now it is just Mailbox specific....
///////////////////////////////////////////////////////////////////////////////////

class nsMailboxTestDriver : public nsIUrlListener
{
public:
	NS_DECL_ISUPPORTS

	// nsIUrlListener support
	NS_IMETHOD OnStartRunningUrl(nsIURL * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode);

	nsMailboxTestDriver(PLEventQueue *queue, nsIStreamListener * aMailboxParser);
	virtual ~nsMailboxTestDriver();

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
	nsresult OpenMailbox();
	nsresult OnDisplayMessage();
	nsresult OnExit(); 

protected:
    PLEventQueue *m_eventQueue;
	char m_urlSpec[200];	// "sockstub://hostname:port" it does not include the command specific data...
	char m_urlString[800];	// string representing the current url being run. Includes host AND command specific data.
	char m_userData[500];	// generic string buffer for storing the current user entered data...

	nsIMailboxUrl		*m_url; 
	nsIStreamListener	*m_mailboxParser;

	nsFileSpec	m_folderSpec;

	PRBool		m_runningURL;	// are we currently running a url? this flag is set to false on exit...
	PRBool	    m_runTestHarness;

	void InitializeProtocol(const char * urlSpec);
    nsresult SetupUrl(char *group); 
	nsMailDatabase * OpenDB(nsFileSpec filePath);
};

nsresult nsMailboxTestDriver::OnStartRunningUrl(nsIURL * aUrl)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	m_runningURL = PR_TRUE;
	return NS_OK;
}

nsresult nsMailboxTestDriver::OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode)
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

NS_IMPL_ISUPPORTS(nsMailboxTestDriver, nsIUrlListener::GetIID())

nsMailboxTestDriver::nsMailboxTestDriver(PLEventQueue *queue, nsIStreamListener * aMailboxParser) : m_folderSpec("")
{
	NS_INIT_REFCNT();
	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_url = nsnull;
	m_runningURL = PR_FALSE;
	m_runTestHarness = PR_TRUE;
    m_eventQueue = queue;
	
	InitializeTestDriver(); // prompts user for initialization information...

	if (aMailboxParser)
	{
		NS_ADDREF(aMailboxParser);
		m_mailboxParser = aMailboxParser;
	}
	else
		m_mailboxParser = nsnull;
}

nsMailboxTestDriver::~nsMailboxTestDriver()
{
	NS_IF_RELEASE(m_mailboxParser);
}

nsresult nsMailboxTestDriver::RunDriver()
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

static const char kMsgRootFolderPref[] = "mail.rootFolder";

void nsMailboxTestDriver::InitializeTestDriver()
{
	PL_strcpy(m_urlSpec, DEFAULT_URL_TYPE); // copy "mailbox://" part into url spec...
	// read in the default mail folder path...

	// propogating bienvenu's preferences hack.....
	#define ROOT_PATH_LENGTH 128 
	char rootPath[ROOT_PATH_LENGTH];
	int rootLen = ROOT_PATH_LENGTH;
	nsIPref* prefs;
	nsresult rv;
	rv = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&prefs);
    if (prefs && NS_SUCCEEDED(rv))
	{
		prefs->Startup("prefs50.js");
		rv = prefs->GetCharPref(kMsgRootFolderPref, rootPath, &rootLen);
		nsServiceManager::ReleaseService(kPrefCID, prefs);
	}

	if (rootLen > 0) // how many bytes did they write into our buffer?
	{
		m_folderSpec = rootPath;
	}
}

// prints the userPrompt and then reads in the user data. Assumes urlData has already been allocated.
// it also reconstructs the url string in m_urlString but does NOT reload it....
nsresult nsMailboxTestDriver::PromptForUserDataAndBuildUrl(const char * userPrompt)
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

nsresult nsMailboxTestDriver::ReadAndDispatchCommand()
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
		status = OpenMailbox();
		break;
	case 2:
		status = OnDisplayMessage();
		break;
	default:
		status = OnExit();
		break;
	}

	return status;
}

nsresult nsMailboxTestDriver::ListCommands()
{
	printf("Commands currently available: \n");
	printf("0) List commands. \n");
	printf("1) Open a mailbox folder. \n");
	printf("2) Display a message from a folder. \n");
	printf("9) Exit the test application. \n");
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////
// Begin protocol specific command url generation code...gee that's a mouthful....
///////////////////////////////////////////////////////////////////////////////////

nsresult nsMailboxTestDriver::OnExit()
{
	printf("Terminating Mailbox test harness....\n");
	m_runTestHarness = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}

nsMailDatabase * nsMailboxTestDriver::OpenDB(nsFileSpec filePath)
{
	nsMailDatabase * mailDb = nsnull;
	nsresult rv = NS_OK;
	rv = nsMailDatabase::Open(filePath, PR_FALSE, &mailDb);
	return mailDb;
}

nsresult nsMailboxTestDriver::OnDisplayMessage()
{
	nsresult rv = NS_OK; 
	char * displayString = nsnull;
	nsMsgKeyArray msgKeys;

	PL_strcpy(m_userData, (const char *) m_folderSpec);

	displayString = PR_smprintf("Location of mailbox folders [%s]: ", m_userData);
	rv = PromptForUserDataAndBuildUrl(displayString);
	PR_FREEIF(displayString);
 
	char * folderPath = PL_strdup(m_userData);

	// now ask for the mailbox name...
	PL_strcpy(m_userData, DEFAULT_MAILBOX);
	displayString = PR_smprintf("Mailbox to get message from [%s]: ", m_userData);
	rv = PromptForUserDataAndBuildUrl(displayString);
	PR_FREEIF(displayString);

	// concatenate folder name onto folder path....
	char * fullFolderPath = PR_smprintf("%s%c%s", folderPath ? folderPath : "",PR_DIRECTORY_SEPARATOR, m_userData);
	PR_FREEIF(folderPath);
	// now turn this into a nsFilePath...
	nsFileSpec filePath(fullFolderPath);
	PR_FREEIF(fullFolderPath);

	nsMailDatabase * mailDb = OpenDB(filePath);
	if (mailDb)
	{
		// extract the message key array
		mailDb->ListAllKeys(msgKeys);
		PRUint32 numKeys = msgKeys.GetSize();
		// ask the user which message they want to display...We'll do this by asking the message number
		// and then looking up in the array for the message key associated with that message.
		PL_strcpy(m_userData, "0");
		displayString = PR_smprintf("Enter message numbeer between %d and %d to display [%s]: ", 0, numKeys-1, m_userData);
		rv = PromptForUserDataAndBuildUrl(displayString);
		PR_FREEIF(displayString);

		PRUint32 index = atol(m_userData);
		nsMsgKey msgKey = msgKeys[index];

		// okay, we have the msgKey so let's get rid of our db state...
		mailDb->Close(PR_TRUE);

		// now ask the mailbox service to parse this mailbox...
		nsIMailboxService * mailboxService = nsnull;
		rv = nsServiceManager::GetService(kCMailboxServiceCID, nsIMailboxService::GetIID(), (nsISupports **) &mailboxService);
		if (NS_SUCCEEDED(rv) && mailboxService)
		{
			nsIURL * url = nsnull;
			mailboxService->DisplayMessage(filePath, msgKey, nsnull, nsnull, this, nsnull);
			if (url)
				url->QueryInterface(nsIMailboxUrl::GetIID(), (void **) &m_url);
			NS_IF_RELEASE(url);

			nsServiceManager::ReleaseService(kCMailboxServiceCID, mailboxService);
		}
	}
	else
	{
		printf("We were unable to open a database associated with this mailbox folder.\n");
		printf("Try parsing the mailbox folder first (command 1). \n");
	}

	return rv;
}


nsresult nsMailboxTestDriver::OpenMailbox()
{
	nsresult rv = NS_OK; 
	char * displayString = nsnull;

	PL_strcpy(m_userData, (const char *) m_folderSpec);

	// ask for path to local mailbox folders
	displayString = PR_smprintf("Location of mailbox folders [%s]: ", m_userData);
	rv = PromptForUserDataAndBuildUrl(displayString);
	PR_FREEIF(displayString);

	char * folderPath = PL_strdup(m_userData);

	// now ask for the mailbox name...
	PL_strcpy(m_userData, DEFAULT_MAILBOX);
	displayString = PR_smprintf("Mailbox to parse [%s]: ", m_userData);
	rv = PromptForUserDataAndBuildUrl(displayString);
	PR_FREEIF(displayString);

	// concatenate folder name onto folder path....
	char * fullFolderPath = PR_smprintf("%s%c%s", folderPath ? folderPath : "",PR_DIRECTORY_SEPARATOR, m_userData);
	PR_FREEIF(folderPath);
	// now turn this into a nsFileSpec...
	nsFileSpec filePath(fullFolderPath);
	PR_FREEIF(fullFolderPath);

	// now ask the mailbox service to parse this mailbox...
    nsIMailboxService* mailboxService;
    rv = nsServiceManager::GetService(kCMailboxServiceCID,
                                      nsIMailboxService::GetIID(),
                                      (nsISupports**)&mailboxService);
	if (NS_SUCCEEDED(rv) && mailboxService)
	{
		nsIURL * url = nsnull;
		mailboxService->ParseMailbox(filePath, m_mailboxParser, this /* register self as url listener */, &url);
		if (url)
			url->QueryInterface(nsIMailboxUrl::GetIID(), (void **) &m_url);
		NS_IF_RELEASE(url);
        (void)nsServiceManager::ReleaseService(kCMailboxServiceCID, mailboxService);
	}
	else
		NS_ASSERTION(PR_FALSE, "unable to acquire a mailbox service...registration problem?");
	return rv;
}

/////////////////////////////////////////////////////////////////////////////////
// End on command handlers for mailbox
/////////////////////////////////////////////////////////////////////////////////


int main()
{
	nsINetService * pNetService;
    PLEventQueue *queue;
    nsresult result;

    nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kRDFServiceCID, nsnull, nsnull, RDF_DLL, PR_TRUE, PR_TRUE);
	nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
	nsComponentManager::RegisterComponent(kCMailboxServiceCID, nsnull, nsnull, LOCAL_DLL, PR_TRUE, PR_TRUE);

	// Create the Event Queue for this thread...
    nsIEventQueueService* pEventQService;
    result = nsServiceManager::GetService(kEventQueueServiceCID,
                                          nsIEventQueueService::GetIID(),
                                          (nsISupports**)&pEventQService);
	if (NS_FAILED(result)) return result;

    result = pEventQService->CreateThreadEventQueue();
	if (NS_FAILED(result)) return result;

	// ask the net lib service for a nsINetStream:
	result = NS_NewINetService(&pNetService, NULL);
	if (NS_FAILED(result) || !pNetService)
	{
		printf("unable to initialize net serivce. \n");
		return 1;
	}

    pEventQService->GetThreadEventQueue(PR_GetCurrentThread(),&queue);
    if (NS_FAILED(result) || !queue) {
        printf("unable to get event queue.\n");
        return 1;
    }

	// bienvenu -- this is where you replace my creation of a stub mailbox parser with one of your own
	// that gets passed into the mailbox test driver and it binds your parser to the mailbox url you run
	// through the driver.
	nsIStreamListener * mailboxParser = nsnull;
	nsComponentManager::CreateInstance(kCMailboxParser, nsnull, nsIStreamListener::GetIID(), (void **) &mailboxParser);
	// NS_NewMsgParser(&mailboxParser);
    
	// okay, everything is set up, now we just need to create a test driver and run it...
	nsMailboxTestDriver * driver = new nsMailboxTestDriver(queue, mailboxParser);
	if (driver)
	{
		NS_ADDREF(driver);
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		NS_RELEASE(driver);
	}

	// shut down:
	NS_IF_RELEASE(mailboxParser);
	NS_IF_RELEASE(pNetService);
    
    return 0;
}
