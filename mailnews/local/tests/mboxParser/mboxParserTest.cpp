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
 */


/*=============================================================================
 * This test program is designed to test the berkeley mailbox parser.
 * When the test program starts up, you are prompted for a mailbox name.
 * The code will then generate a stream of data for the mailbox parser.
*===============================================================================*/

#include <stdio.h>
#include <assert.h>

#ifdef XP_PC
#include <windows.h>
#endif

#include "plstr.h"
#include "plevent.h"
#include "prenv.h"

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsITransport.h"
#include "nsIURL.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsString.h"

#include "nsParseMailbox.h"


#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);


/////////////////////////////////////////////////////////////////////////////////
// Define default values to be used to drive the test
/////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_HOST	"nsmail-2.mcom.com"
#define DEFAULT_PORT	POP3_PORT		/* we get this value from nsPop3Protocol.h */
#define DEFAULT_URL_TYPE  "pop3://"

//extern NET_StreamClass *MIME_MessageConverter(int format_out, void *closure, 
//											  URL_Struct *url, MWContext *context);

#ifdef XP_UNIX
extern "C" char *fe_GetConfigDir(void) {
  printf("XXX: return /tmp for fe_GetConfigDir\n");
  return strdup("/tmp");
}
#endif /* XP_UNIX */



//////////////////////////////////////////////////////////////////////////////////
// The nsMailboxParserTestDriver is a class that I envision could be generalized to form the
// building block of a protocol test harness. To configure it, you would list all of
// the events you know how to handle and when one of those events is triggered, you
// would be asked to process it....right now it is just Pop3 specific....
///////////////////////////////////////////////////////////////////////////////////

class nsMailboxParserTestDriver
{
public:
	nsMailboxParserTestDriver();
	virtual ~nsMailboxParserTestDriver();

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
	nsresult OnExit(); 
protected:
	char m_urlSpec[200];	// "sockstub://hostname:port" it does not include the command specific data...
	char m_urlString[500];	// string representing the current url being run. Includes host AND command specific data.
	char m_userData[250];	// generic string buffer for storing the current user entered data...

	// host and port info...
	PRUint32	m_port;
	char		m_host[200];		
    char*       m_username;
    char*       m_password;
    char*       m_mailDirectory;

	PRBool		m_runningURL;	// are we currently running a url? this flag is set to false on exit...

	void InitializeProtocol(const char * urlSpec);
	PRBool m_protocolInitialized; 
	nsMsgMailboxParser	*m_mailboxParser;
};

nsMailboxParserTestDriver::nsMailboxParserTestDriver()
{
	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_protocolInitialized = PR_FALSE;
	m_runningURL = PR_TRUE;
    m_username = PL_strdup("qatest03");
    m_password = PL_strdup("Ne!sc-pe");
    char *env = PR_GetEnv("TEMP");
    if (!env)
        env = PR_GetEnv("TMP");
    if (env)
        m_mailDirectory = PL_strdup(env);
	
	InitializeTestDriver(); // prompts user for initialization information...
	
	// create a transport socket...
}

void nsMailboxParserTestDriver::InitializeProtocol(const char * urlString)
{
	// this is called when we don't have a url nor a protocol instance yet...
}

nsMailboxParserTestDriver::~nsMailboxParserTestDriver()
{
    PR_FREEIF(m_username);
    PR_FREEIF(m_password);
    PR_FREEIF(m_mailDirectory);
}

nsresult nsMailboxParserTestDriver::RunDriver()
{
	nsresult status = NS_OK;

	while (m_runningURL)
	{
		// if we haven't gotten started (and created a protocol) or
		// if the protocol instance is currently not busy, then read in a new command
		// and process it...
	//	if ((!m_pop3Protocol) || m_pop3Protocol->IsRunning() == PR_FALSE) // if we aren't running the url anymore, ask ueser for another command....
		{
			status = ReadAndDispatchCommand();	
		}  // if running url
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

void nsMailboxParserTestDriver::InitializeTestDriver()
{
	// prompt the user for port and host name 
	char fileName[255];
	fileName[0] = '\0';

	// prompt user for mailboxname...
	printf("Enter mailbox to parse  ");
	scanf("%s", fileName);
	// now prompt for the local mail folder directory .....
	printf("Enter local mail folder directory [%s]: ", m_mailDirectory);
	m_mailboxParser = new nsMsgMailboxParser;
}

// prints the userPrompt and then reads in the user data. Assumes urlData has already been allocated.
// it also reconstructs the url string in m_urlString but does NOT reload it....
nsresult nsMailboxParserTestDriver::PromptForUserDataAndBuildUrl(const char * userPrompt)
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

nsresult nsMailboxParserTestDriver::ReadAndDispatchCommand()
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
	default:
		status = OnExit();
		break;
	}

	return status;
}

nsresult nsMailboxParserTestDriver::ListCommands()
{
	printf("Commands currently available: \n");
	printf("0) List available commands. \n");
	printf("1) Check new mail. \n");
	printf("2) Get mail account url. \n");
	printf("3) Uidl. \n");
    printf("4) Get new mail. \n");
	printf("9) Exit the test application. \n");
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////
// Begin protocol specific command url generation code...gee that's a mouthful....
///////////////////////////////////////////////////////////////////////////////////

nsresult nsMailboxParserTestDriver::OnExit()
{
	printf("Terminating Pop3 test harness....\n");
	m_runningURL = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}


nsresult nsMailboxParserTestDriver::OnCheck()
{
	nsresult rv = NS_OK; 

	// no prompt for url data....just append a '*' to the url data and run it...
	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);

	return rv;
}


int main()
{
    nsresult result;


	// now register a mime converter....
    //	NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_NGLAYOUT, NULL, MIME_MessageConverter);
    //	NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_CACHE_AND_NGLAYOUT, NULL, MIME_MessageConverter);

	// okay, everything is set up, now we just need to create a test driver and run it...
	nsMailboxParserTestDriver * driver = new nsMailboxParserTestDriver();
	if (driver)
	{
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		delete driver;
	}

	// shut down:
    
    return 0;
}
