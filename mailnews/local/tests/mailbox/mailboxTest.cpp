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
// A call to ::OnStopRequest is a signal to the mailbox parser that you've reached
// the end of the file.
//////////////////////////////////////////////////////////////////////////////////

#include "msgCore.h"
#include "prprf.h"
#include "nspr.h"
#include <stdio.h>
#include <assert.h>

#ifdef XP_PC
#include <windows.h>
#endif

#include "nsIFileSpec.h"
#include "nsFileSpec.h"
#include "nsIPref.h"
#include "plstr.h"
#include "plevent.h"
#include "nsIServiceManager.h"
#include "nsIMailboxService.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsMsgDBCID.h"
#include "nsIMailboxUrl.h"
#include "nsParseMailbox.h"
#include "nsIMsgDatabase.h"
#include "nsIUrlListener.h"
#include "nsIUrlListenerManager.h"

#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsRDFCID.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"
#include "nsIFileLocator.h"
#include "nsMsgUtils.h"
#include "nsLocalUtils.h"

#ifdef XP_PC
#define XPCOM_DLL    "xpcom32.dll"
#define PREF_DLL	 "xppref32.dll"
#define APPSHELL_DLL "appshell.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define XPCOM_DLL    "libxpcom"MOZ_DLL_SUFFIX
#define LOCAL_DLL	 "libmsglocal"MOZ_DLL_SUFFIX
#define RDF_DLL      "librdf"MOZ_DLL_SUFFIX
#define PREF_DLL	 "libpref"MOZ_DLL_SUFFIX
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_CID(kCUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static NS_DEFINE_CID(kCMailboxServiceCID, NS_MAILBOXSERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_CID(kCMailboxParser, NS_MAILBOXPARSER_CID);

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);

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

class nsMailboxTestDriver : public nsIStreamListener, nsIUrlListener
{
public:
	NS_DECL_ISUPPORTS

	// nsIUrlListener support
	NS_IMETHOD OnStartRunningUrl(nsIURI * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode);

	nsMailboxTestDriver(nsIEventQueue *queue, nsIStreamListener * aMailboxParser);
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
	nsresult OnDisplayMessage(PRBool aCopyMessage = PR_FALSE); // this function can display or copy a message
	nsresult OnCopyMessage();
	nsresult OnExit(); 

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIStreamListener interface --> primarily to test copy, move and delete
	////////////////////////////////////////////////////////////////////////////////////////
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER

	////////////////////////////////////////////////////////////////////////////////////////
	// End of nsIStreamListenerSupport
	////////////////////////////////////////////////////////////////////////////////////////

protected:
    nsIEventQueue *m_eventQueue;
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
	nsIMsgDatabase * OpenDB(nsFileSpec filePath);
};

nsresult nsMailboxTestDriver::OnStartRunningUrl(nsIURI * aUrl)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	m_runningURL = PR_TRUE;
	return NS_OK;
}

nsresult nsMailboxTestDriver::OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	nsresult rv = NS_OK;
	m_runningURL = PR_FALSE;
	if (aUrl)
	{
		// query it for a mailnews interface for now....
		nsIMsgMailNewsUrl * mailUrl = nsnull;
		rv = aUrl->QueryInterface(NS_GET_IID(nsIMsgMailNewsUrl), (void **) mailUrl);
		if (NS_SUCCEEDED(rv))
		{
			mailUrl->UnRegisterListener(this);
			NS_RELEASE(mailUrl);
		}
	}

	return NS_OK;
}

NS_IMPL_ADDREF(nsMailboxTestDriver);
NS_IMPL_RELEASE(nsMailboxTestDriver);

NS_IMETHODIMP nsMailboxTestDriver::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;
 
    if (aIID.Equals(NS_GET_IID(nsIStreamListener)) || aIID.Equals(NS_GET_IID(nsISupports))) 
	{
        *aInstancePtr = (void*) ((nsIStreamListener*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(NS_GET_IID(nsIUrlListener))) 
	{
        *aInstancePtr = (void*) ((nsIUrlListener*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
 
    return NS_NOINTERFACE;
}

nsMailboxTestDriver::nsMailboxTestDriver(nsIEventQueue *queue, nsIStreamListener * aMailboxParser) : m_folderSpec("")
{
	NS_INIT_REFCNT();
	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_url = nsnull;
	m_runningURL = PR_FALSE;
	m_runTestHarness = PR_TRUE;
    m_eventQueue = queue;
	NS_IF_ADDREF(queue);
	
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
	NS_IF_RELEASE(m_eventQueue);
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

        m_eventQueue->ProcessPendingEvents();

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
	m_folderSpec = "c:\\Program Files\\Netscape\\Users\\mscott\\mail";
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
	case 3:
		status = OnCopyMessage();
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
	printf("3) On copy message. \n");
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

nsIMsgDatabase * nsMailboxTestDriver::OpenDB(nsFileSpec filePath)
{
	nsIMsgDatabase * db;
	nsCOMPtr<nsIMsgDatabase> mailDB;
	nsresult rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), (void **) getter_AddRefs(mailDB));
	if (NS_SUCCEEDED(rv) && mailDB)
	{
		nsCOMPtr <nsIFileSpec> dbFileSpec;
		NS_NewFileSpecWithSpec(filePath, getter_AddRefs(dbFileSpec));
		rv = mailDB->Open(dbFileSpec, PR_TRUE, PR_FALSE, &db);
	}

	return db;
}

nsresult nsMailboxTestDriver::OnCopyMessage()
{
	return OnDisplayMessage(PR_TRUE);
}

nsresult nsMailboxTestDriver::OnDisplayMessage(PRBool copyMessage)
{
	// if copyMessage is TRUE then this function is a copy message test. if false, then this 
	// function is supposed to display a message.

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
	char * fullFolderPath = PR_smprintf("%s%c%s", folderPath ? folderPath : "", '\\', m_userData);
	char * mailboxName = PL_strdup(m_userData);
	PR_FREEIF(folderPath);
	// now turn this into a nsFilePath...
	nsFileSpec filePath(fullFolderPath);
	PR_FREEIF(fullFolderPath);

	nsIMsgDatabase * mailDb = OpenDB(filePath);
	if (mailDb)
	{
		// extract the message key array
		mailDb->ListAllKeys(msgKeys);
		PRUint32 numKeys = msgKeys.GetSize();
		// ask the user which message they want to display...We'll do this by asking the message number
		// and then looking up in the array for the message key associated with that message.
		PL_strcpy(m_userData, "0");
		displayString = PR_smprintf("Enter message number between %d and %d to display [%s]: ", 0, numKeys-1, m_userData);
		rv = PromptForUserDataAndBuildUrl(displayString);
		PR_FREEIF(displayString);

		PRUint32 index = atol(m_userData);
		nsMsgKey msgKey = msgKeys[index];

		// okay, we have the msgKey so let's get rid of our db state...
		mailDb->Close(PR_TRUE);

		// mscott - hacky....sprintf up a mailbox URI to represent the message.
		char * uri = nsnull;
		nsCString uriStr;
		char * testString = PR_smprintf("%s%s", "mailbox_message://mscott@dredd.mcom.com/", mailboxName);
		rv = nsBuildLocalMessageURI(/* (const char *) filePath */ testString, msgKey, uriStr);
		uri = uriStr.ToNewCString();

		if (NS_SUCCEEDED(rv))
		{
			nsIMsgMessageService * messageService = nsnull;
			rv = GetMessageServiceFromURI(uri, &messageService);

			if (NS_SUCCEEDED(rv) && messageService)
			{
				nsISupports * asupport = nsnull;
				QueryInterface(NS_GET_IID(nsISupports), (void **) asupport);
				messageService->DisplayMessage(uri, asupport, nsnull, nsnull, nsnull, nsnull);
				ReleaseMessageServiceFromURI(uri, messageService);
			}
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
	char * fullFolderPath = PR_smprintf("%s%c%s", folderPath ? folderPath : "",'\\', m_userData);
	PR_FREEIF(folderPath);
	// now turn this into a nsFileSpec...
	nsFileSpec filePath(fullFolderPath);
	PR_FREEIF(fullFolderPath);

	// now ask the mailbox service to parse this mailbox...
	NS_WITH_SERVICE(nsIMailboxService, mailboxService, kCMailboxServiceCID, &rv);

	if (NS_SUCCEEDED(rv) && mailboxService)
	{
		nsIURI * url = nsnull;
		mailboxService->ParseMailbox(nsnull, filePath, m_mailboxParser, this /* register self as url listener */, &url);
		if (url)
			url->QueryInterface(NS_GET_IID(nsIMailboxUrl), (void **) &m_url);
		NS_IF_RELEASE(url);
	}
	else
		NS_ASSERTION(PR_FALSE, "unable to acquire a mailbox service...registration problem?");
	return rv;
}

/////////////////////////////////////////////////////////////////////////////////
// End on command handlers for mailbox
/////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsMailboxTestDriver::OnDataAvailable(nsIChannel * /* aChannel */, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 aLength)
{
	// read the data out and print it to the screen....
	if (aLength > 0)
	{
		char * buffer = (char *) PR_Malloc(sizeof(char) * aLength + 1);
		if (buffer && inStr)
		{
			inStr->Read(buffer, aLength, nsnull);
			printf(buffer);
		}

		PR_FREEIF(buffer);
	}
	return NS_OK;
}

NS_IMETHODIMP nsMailboxTestDriver::OnStartRequest(nsIChannel * aChannel, nsISupports *ctxt)
{
	nsCOMPtr<nsIMailboxUrl> mailboxUrl = do_QueryInterface(ctxt);
	if (mailboxUrl)
	{
		nsMsgKey msgKey;
		mailboxUrl->GetMessageKey(&msgKey);
		printf ("Begin Copying Message with message key %d.\n", msgKey);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMailboxTestDriver::OnStopRequest(nsIChannel * /* aChannel */, nsISupports *ctxt, nsresult aStatus, const PRUnichar* aMsg)
{
	nsCOMPtr<nsIMailboxUrl> mailboxUrl = do_QueryInterface(ctxt);

	if (mailboxUrl)
	{
		nsMsgKey msgKey;
		mailboxUrl->GetMessageKey(&msgKey);
		printf ("\nEnd Copying Message with message key %d.\n", msgKey);
	}
	return NS_OK;
}


int main()
{
    nsCOMPtr<nsIEventQueue> queue;
    nsresult result;

	nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
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
    nsIEventQueueService* pEventQService;
    result = nsServiceManager::GetService(kEventQueueServiceCID,
                                          NS_GET_IID(nsIEventQueueService),
                                          (nsISupports**)&pEventQService);
	if (NS_FAILED(result)) return result;

    result = pEventQService->CreateThreadEventQueue();
	if (NS_FAILED(result)) return result;

    pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,getter_AddRefs(queue));
    if (NS_FAILED(result) || !queue) {
        printf("unable to get event queue.\n");
        return 1;
    }

	// bienvenu -- this is where you replace my creation of a stub mailbox parser with one of your own
	// that gets passed into the mailbox test driver and it binds your parser to the mailbox url you run
	// through the driver.
	nsCOMPtr<nsIStreamListener> mailboxParser = nsnull;
	nsComponentManager::CreateInstance(kCMailboxParser, nsnull, NS_GET_IID(nsIStreamListener), getter_AddRefs(mailboxParser));
   
	// okay, everything is set up, now we just need to create a test driver and run it...
	nsMailboxTestDriver * driver = new nsMailboxTestDriver(queue, mailboxParser);
	if (driver)
	{
		NS_ADDREF(driver);
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		NS_RELEASE(driver);
	}

    return 0;
}
