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

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "nsCOMPtr.h"

#if defined(XP_PC) && !defined(XP_OS2)
#include <windows.h>
#endif

#include "plstr.h"
#include "plevent.h"

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsIPref.h"
#include "nsIFileLocator.h"
#include "nsIPop3Service.h"
#include "nsIMsgMailNewsUrl.h"

// include the event sinks for the protocol you are testing

#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"

#include "nsIMsgIdentity.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgIncomingServer.h"
#include "nsIPop3IncomingServer.h"
#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"

#ifdef XP_PC
#define XPCOM_DLL  "xpcom32.dll"
#define PREF_DLL   "xppref32.dll"
#define APPSHELL_DLL "appshell.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
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
static NS_DEFINE_CID(kPop3ServiceCID, NS_POP3SERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);

/////////////////////////////////////////////////////////////////////////////////
// Define default values to be used to drive the test
/////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////
// The nsPop3TestDriver is a class that I envision could be generalized to form the
// building block of a protocol test harness. To configure it, you would list all of
// the events you know how to handle and when one of those events is triggered, you
// would be asked to process it....right now it is just Pop3 specific....
///////////////////////////////////////////////////////////////////////////////////

class nsPop3TestDriver : public nsIUrlListener
{
public:
	nsPop3TestDriver(nsIEventQueue *queue);
	virtual ~nsPop3TestDriver();
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
	nsresult OnCheck();   // lists all the groups on the host
	nsresult OnGUrl();
	nsresult OnUidl();		// lists the status of the user specified group...
    nsresult OnGet();
	nsresult OnExit(); 
	nsresult OnIdentityCheck();

protected:
    nsIEventQueue *m_eventQueue;
	char m_urlSpec[200];	// "sockstub://hostname:port" it does not include the command specific data...
	char m_urlString[500];	// string representing the current url being run. Includes host AND command specific data.
	char m_userData[250];	// generic string buffer for storing the current user entered data...

	PRBool		m_runTestHarness;
	PRBool		m_runningURL;	// are we currently running a url? this flag is set to false on exit...
};

NS_IMPL_ISUPPORTS(nsPop3TestDriver, NS_GET_IID(nsIUrlListener))

nsPop3TestDriver::nsPop3TestDriver(nsIEventQueue *queue)
{
	NS_INIT_REFCNT();
	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_runningURL = PR_FALSE;
	m_runTestHarness = PR_TRUE;
	m_eventQueue = queue;
	NS_IF_ADDREF(queue);

	InitializeTestDriver(); // prompts user for initialization information...
}

nsPop3TestDriver::~nsPop3TestDriver()
{
	NS_IF_RELEASE(m_eventQueue);
}

nsresult nsPop3TestDriver::OnStartRunningUrl(nsIURI * aUrl)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	m_runningURL = PR_TRUE;
	return NS_OK;
}

nsresult nsPop3TestDriver::OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	nsresult rv = NS_OK;
	m_runningURL = PR_FALSE;
	if (aUrl)
	{
		// query it for a mailnews interface for now....
		nsIMsgMailNewsUrl * mailUrl = nsnull;
		rv = aUrl->QueryInterface(NS_GET_IID(nsIMsgMailNewsUrl), (void **) &mailUrl);
		if (NS_SUCCEEDED(rv))
		{
			mailUrl->UnRegisterListener(this);
			NS_RELEASE(mailUrl);
		}
	}

	return NS_OK;
}

nsresult nsPop3TestDriver::RunDriver()
{
	nsresult status = NS_OK;

	while (m_runTestHarness)
	{
		// if we haven't gotten started (and created a protocol) or
		// if the protocol instance is currently not busy, then read in a new command
		// and process it...
		if (m_runningURL == PR_FALSE) // if we aren't running the url anymore, ask ueser for another command....
		{
			status = ReadAndDispatchCommand();	
		}  // if running url
#ifdef XP_UNIX
        printf(".");
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

void nsPop3TestDriver::InitializeTestDriver()
{
}

// prints the userPrompt and then reads in the user data. Assumes urlData has already been allocated.
// it also reconstructs the url string in m_urlString but does NOT reload it....
nsresult nsPop3TestDriver::PromptForUserDataAndBuildUrl(const char * userPrompt)
{
	char tempBuffer[500];
	tempBuffer[0] = '\0'; 

	if (userPrompt && *userPrompt)
		printf(userPrompt);
	else
		printf("Enter data for command: ");
	 
	scanf("%[^\n]", tempBuffer);
	if (*tempBuffer)
	{
		if (tempBuffer[0])  // kill off any CR or LFs...
		{
			PRUint32 length = PL_strlen(tempBuffer);
			if (length > 0 && tempBuffer[length-1] == '\r')
				tempBuffer[length-1] = '\0';

			// okay, user gave us a valid line so copy it into the user data field..o.t. leave user
			// data field untouched. This allows us to use default values for things...
			m_userData[0] = '\0';
			PL_strcpy(m_userData, tempBuffer);
		}
		
	}
	
	char buffer[2];
	scanf("%c", buffer);  // eat up the CR that is still in the input stream...

	return NS_OK;
}

nsresult nsPop3TestDriver::ReadAndDispatchCommand()
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
		status = OnCheck();
		break;
	case 2:
		status = OnGUrl();
		break;
	case 3:
		status = OnUidl();
		break;
    case 4:
        status = OnGet();
        break;
	case 5:
		status = OnIdentityCheck();
		break;
	default:
		status = OnExit();
		break;
	}

	return status;
}

nsresult nsPop3TestDriver::ListCommands()
{
	printf("Commands currently available: \n");
	printf("0) List available commands. \n");
	printf("1) Check new mail. \n");
	printf("2) Get mail account url. \n");
	printf("3) Uidl. \n");
    printf("4) Get new mail. \n");
	printf("5) Check identity information.\n");
	printf("9) Exit the test application. \n");
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////
// Begin protocol specific command url generation code...gee that's a mouthful....
///////////////////////////////////////////////////////////////////////////////////

nsresult nsPop3TestDriver::OnExit()
{
	printf("Terminating Pop3 test harness....\n");
	m_runTestHarness = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}

nsresult nsPop3TestDriver::OnIdentityCheck()
{
	nsresult result = NS_OK;
	NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                    NS_MSGACCOUNTMANAGER_CONTRACTID, &result);
    
	if (NS_SUCCEEDED(result) && accountManager)
	{
		// mscott: we really don't check an identity, we check
		// for an outgoing
        nsCOMPtr<nsIMsgAccount> account;
        accountManager->GetDefaultAccount(getter_AddRefs(account));

        nsCOMPtr<nsIMsgIncomingServer> incomingServer;
        result = account->GetIncomingServer(getter_AddRefs(incomingServer));
		if (NS_SUCCEEDED(result) && incomingServer)
		{
			char * value = nsnull;
            PRUnichar* uvalue;
			incomingServer->GetPrettyName(&uvalue);
            nsString val(uvalue);
			printf("Server pretty name: %s\n", val.ToNewCString());
			incomingServer->GetUsername(&value);
			printf("User Name: %s\n", value ? value : "");
			incomingServer->GetHostName(&value);
			printf("Pop Server: %s\n", value ? value : "");
			incomingServer->GetPassword(&value);
			printf("Pop Password: %s\n", value ? value : "");

		}
		else
			printf("Unable to retrieve the outgoing server interface....\n");
	}
	else
		printf("Unable to retrieve the mail session service....\n");

	return result;
}

nsresult nsPop3TestDriver::OnCheck()
{
	nsresult rv = NS_OK;

    
	NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                    NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIMsgAccount> account;
    accountManager->GetDefaultAccount(getter_AddRefs(account));

    nsCOMPtr<nsIMsgIncomingServer> incomingServer;
    rv = account->GetIncomingServer(getter_AddRefs(incomingServer));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIPop3IncomingServer> popServer =
        do_QueryInterface(incomingServer);

	NS_WITH_SERVICE(nsIPop3Service, pop3Service, kPop3ServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

	if (pop3Service)
	{
		pop3Service->CheckForNewMail(nsnull, this, nsnull, popServer, nsnull);
		m_runningURL = PR_TRUE;
	}

	return rv;
}

nsresult nsPop3TestDriver::OnGUrl()
{
	nsresult rv = NS_OK;

#if 0 
	// no prompt for url data....just append a '*' to the url data and run it...
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "?gurl");

	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);
	else
		rv = m_url->SetSpec(m_urlString); // reset spec
	
	if (NS_SUCCEEDED(rv))
	{
		 // before we re-load, assume it is a group command and configure our Pop3URL correctly...

		rv = m_pop3Protocol->Load(m_url);
	} // if user provided the data...
#endif

	printf ("No longer implemented -- mscott\n");
	return rv;
}

nsresult nsPop3TestDriver::OnUidl()
{
	nsresult rv = NS_OK;
#if 0
	// first, prompt the user for the name of the group to fetch
	// prime article number with a default value...
	m_userData[0] = '\0';
	PL_strcpy(m_userData, "25-910436378");
	rv = PromptForUserDataAndBuildUrl("Message uidl to Fetch [25-910436378]: ");
	// no prompt for url data....just append a '*' to the url data and run
    // it...

	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "?uidl=");
    PL_strcat(m_urlString, m_userData);

	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);
	else
		m_url->SetSpec(m_urlString); // reset spec

	if (NS_SUCCEEDED(rv))
	{
		 // before we re-load, assume it is a group command and configure our Pop3URL correctly...

		rv = m_pop3Protocol->Load(m_url);
	} // if user provided the data...
#endif
	printf("No longer implemented -- mscott. \n");
	return rv;
}

nsresult nsPop3TestDriver::OnGet()
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                    NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE(nsIPop3Service, pop3Service, kPop3ServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIMsgAccount> account;
    accountManager->GetDefaultAccount(getter_AddRefs(account));

    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = account->GetIncomingServer(getter_AddRefs(server));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIPop3IncomingServer> popServer =
        do_QueryInterface(server);

	if (pop3Service)
	{
		pop3Service->GetNewMail(nsnull, this, nsnull, popServer, nsnull);
		m_runningURL = PR_TRUE;
	}
	return rv;
}

int main()
{
    nsCOMPtr<nsIEventQueue> queue;
    nsresult result;

	nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kEventQueueCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
	nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NS_FILELOCATOR_CONTRACTID, APPSHELL_DLL, PR_FALSE, PR_FALSE);

	// make sure prefs get initialized and loaded..
	// mscott - this is just a bad bad bad hack right now until prefs
	// has the ability to take nsnull as a parameter. Once that happens,
	// prefs will do the work of figuring out which prefs file to load...
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &result); 
    if (NS_FAILED(result) || !prefs) {
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

    result = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,getter_AddRefs(queue));
    if (NS_FAILED(result) || !queue) {
        printf("unable to get event queue.\n");
        return 1;
    }

	// okay, everything is set up, now we just need to create a test driver and run it...
	nsPop3TestDriver * driver = new nsPop3TestDriver(queue);
	if (driver)
	{
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		delete driver;
	}
    
    return 0;
}
