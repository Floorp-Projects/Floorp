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


/*=============================================================================
 * This test program is designed to test netlib's implementation of nsITransport.
 * In particular, it is currently geared towards testing their socket implemnation.
 * When the test program starts up, you are prompted for a port and domain 
 * (I may have these hard coded right now to be nsmail-2 and port 143).
 * After entering this information, we'll build a connection to the host name.
 * You can then enter raw protocol text (i.e. "1 capability") and watch the data
 * that comes back from the socket. After data is returned, you can enter another
 * line of protocol.
*===============================================================================*/

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
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsNNTPNewsgroupPost.h"

#include "nntpCore.h"
#include "nsNNTPProtocol.h"
#include "nsNntpUrl.h"

// include the event sinks for the protocol you are testing
#include "nsINNTPHost.h"

#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define NEWS_DLL   "msgnews.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib.so"
#define XPCOM_DLL  "libxpcom.so"
#define NEWS_DLL   "libmsgnews.so"
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kNntpUrlCID, NS_NNTPURL_CID);

/////////////////////////////////////////////////////////////////////////////////
// Define default values to be used to drive the test
/////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_HOST	"zia.mcom.com"
#define DEFAULT_PORT	NEWS_PORT		/* we get this value from nntpCore.h */
#define DEFAULT_URL_TYPE  "news://"	/* do NOT change this value until netlib re-write is done...*/

//extern NET_StreamClass *MIME_MessageConverter(int format_out, void *closure, 
//											  URL_Struct *url, MWContext *context);

#ifdef XP_UNIX
extern "C" char *fe_GetConfigDir(void) {
  printf("XXX: return /tmp for fe_GetConfigDir\n");
  return PL_strdup("/tmp");
}
#endif /* XP_UNIX */

// temporary hack...we don't have escape search url in the new world yet....
char *MSG_EscapeSearchUrl (const char *nntpCommand)
{
	char *result = NULL;
	// max escaped length is two extra characters for every character in the cmd.
	char *scratchBuf = (char*) PR_Malloc (3*PL_strlen(nntpCommand) + 1);
	if (scratchBuf)
	{
		char *scratchPtr = scratchBuf;
		while (1)
		{
			char ch = *nntpCommand++;
			if (!ch)
				break;
			if (ch == '#' || ch == '?' || ch == '@' || ch == '\\')
			{
				*scratchPtr++ = '\\';
				sprintf (scratchPtr, "%X", ch);
				scratchPtr += 2;
			}
			else
				*scratchPtr++ = ch;
		}
		*scratchPtr = '\0';
		result = PL_strdup (scratchBuf); // realloc down to smaller size
		PR_Free(scratchBuf);
	}
	return result;
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
// The nsNntpTestDriver is a class that I envision could be generalized to form the
// building block of a protocol test harness. To configure it, you would list all of
// the events you know how to handle and when one of those events is triggered, you
// would be asked to process it....right now it is just NNTP specific....
///////////////////////////////////////////////////////////////////////////////////

class nsNntpTestDriver
{
public:
	nsNntpTestDriver(nsINetService * pService, PLEventQueue *queue);
	virtual ~nsNntpTestDriver();

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
	nsresult OnListAllGroups();   // lists all the groups on the host
	nsresult OnListIDs();
	nsresult OnGetGroup();		// lists the status of the user specified group...
	nsresult OnListArticle();
	nsresult OnSearch();
	nsresult OnReadNewsRC();
    nsresult OnPostMessage();
	nsresult OnExit(); 
protected:
    PLEventQueue *m_eventQueue;
	char m_urlSpec[200];	// "sockstub://hostname:port" it does not include the command specific data...
	char m_urlString[500];	// string representing the current url being run. Includes host AND command specific data.
	char m_userData[250];	// generic string buffer for storing the current user entered data...

	// host and port info...
	PRUint32	m_port;
	char		m_host[200];		

	nsINntpUrl * m_url; 
	nsNNTPProtocol * m_nntpProtocol; // running protocol instance
	nsITransport * m_transport; // a handle on the current transport object being used with the protocol binding...

	PRBool		m_runningURL;	// are we currently running a url? this flag is set to false on exit...

	void InitializeProtocol(const char * urlSpec);
    nsresult SetupUrl(char *group);
	PRBool m_protocolInitialized; 
};

nsNntpTestDriver::nsNntpTestDriver(nsINetService * pNetService,
                                   PLEventQueue *queue)
{
	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_url = nsnull;
	m_protocolInitialized = PR_FALSE;
	m_runningURL = PR_TRUE;
    m_eventQueue = queue;
	
	InitializeTestDriver(); // prompts user for initialization information...
	
	// create a transport socket...
	pNetService->CreateSocketTransport(&m_transport, m_port, m_host);
	m_nntpProtocol = nsnull; // we can't create it until we have a url...
}

void nsNntpTestDriver::InitializeProtocol(const char * urlString)
{
    nsresult rv = NS_OK;

	// this is called when we don't have a url nor a protocol instance yet...
    rv = nsServiceManager::GetService(kNntpUrlCID,
                                      nsINntpUrl::GetIID(),
                                      (nsISupports**)&m_url);
	if (NS_FAILED(rv)) return rv;

	// now create a protocl instance...
	m_nntpProtocol = new nsNNTPProtocol(m_url, m_transport);
	m_protocolInitialized = PR_TRUE;
}

nsNntpTestDriver::~nsNntpTestDriver()
{
	NS_IF_RELEASE(m_url);
	NS_IF_RELEASE(m_transport);
	if (m_nntpProtocol) delete m_nntpProtocol;
}

nsresult nsNntpTestDriver::RunDriver()
{
	nsresult status = NS_OK;


	while (m_runningURL)
	{
		// if we haven't gotten started (and created a protocol) or
		// if the protocol instance is currently not busy, then read in a new command
		// and process it...
		if ((!m_nntpProtocol) || m_nntpProtocol->IsRunningUrl() == PR_FALSE) // if we aren't running the url anymore, ask ueser for another command....
		{
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

void nsNntpTestDriver::InitializeTestDriver()
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
nsresult nsNntpTestDriver::PromptForUserDataAndBuildUrl(const char * userPrompt)
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

nsresult nsNntpTestDriver::ReadAndDispatchCommand()
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
		status = OnListAllGroups();
		break;
	case 2:
		status = OnGetGroup();
		break;
	case 3:
		status = OnListIDs();
		break;
	case 4:
		status = OnListArticle();
		break;
	case 5:
		status = OnSearch();
		break;
	case 6:
		status = OnReadNewsRC();
		break;
    case 7:
        status = OnPostMessage();
        break;
	default:
		status = OnExit();
		break;
	}

	return status;
}

nsresult nsNntpTestDriver::ListCommands()
{
	printf("Commands currently available: \n");
	printf("0) List available commands. \n");
	printf("1) List all news groups. \n");
	printf("2) Get (and subscribe) to a group. \n");
	printf("3) List ids. \n");
	printf("4) Get an article. \n");
	printf("5) Perform Search. \n");
	printf("6) Read NewsRC file. \n");
    printf("7) Post a message. \n");
	printf("9) Exit the test application. \n");
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////
// Begin protocol specific command url generation code...gee that's a mouthful....
///////////////////////////////////////////////////////////////////////////////////

nsresult nsNntpTestDriver::OnExit()
{
	printf("Terminating NNTP test harness....\n");
	m_runningURL = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}


nsresult nsNntpTestDriver::OnListAllGroups()
{
	nsresult rv = NS_OK; 
    printf("Listing all groups..\n");
	// no prompt for url data....just append a '*' to the url data and run it...
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "/");
	PL_strcat(m_urlString, "*");

	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);

	m_url->SetSpec(m_urlString); // reset spec
    printf("Running %s\n", m_urlString);
	rv = m_nntpProtocol->LoadURL(m_url, nsnull /* display stream */);
	return rv;
}

nsresult nsNntpTestDriver::OnListIDs()
{
	nsresult rv = NS_OK;
		// no prompt for url data....just append a '*' to the url data and run it...
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "/");


	rv = PromptForUserDataAndBuildUrl("Group to fetch IDs for: ");
	PL_strcat(m_urlString, m_userData);
	PL_strcat(m_urlString, "?list-ids");
	
	
	// load the correct newsgroup interface as an event sink...
    if (NS_SUCCEEDED(rv)) {
        SetupUrl(m_userData);
        printf("Running %s\n", m_urlString);
        rv = m_nntpProtocol->LoadURL(m_url, nsnull /* display stream */);
    }

	return rv;
}

nsresult nsNntpTestDriver::OnListArticle()
{
	nsresult rv = NS_OK;

	// first, prompt the user for the name of the group to fetch
	// prime article number with a default value...
	m_userData[0] = '\0';
	PL_strcpy(m_userData, "35D8A048.3C0F0C7A@zia.mcom.com");
	rv = PromptForUserDataAndBuildUrl("Article Number to Fetch: ");
	// no prompt for url data....just append a '*' to the url data and run it...
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "/");
	PL_strcat(m_urlString, m_userData);

	if (NS_SUCCEEDED(rv)) {
        SetupUrl(m_userData);
        printf("Running %s\n", m_urlString);
        rv = m_nntpProtocol->LoadURL(m_url);
    }
    
	return rv;
}

nsresult nsNntpTestDriver::OnSearch()
{
	nsresult rv = NS_OK;

	// first, prompt the user for the name of the group to fetch
	rv = PromptForUserDataAndBuildUrl("Group to search: ");
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "/");
	PL_strcat(m_urlString, m_userData);

	// now append "?search" to the end...
	PL_strcat(m_urlString, "?search/");
	rv = PromptForUserDataAndBuildUrl("Search criteria: ");
	if (m_userData[0])  // did the user enter in something???
	{
		char * escapedBuffer = MSG_EscapeSearchUrl(m_userData);
		PL_strcat(m_urlString, escapedBuffer);
	}

	
	if (NS_SUCCEEDED(rv)) {
        SetupUrl(m_userData);
        printf("Running %s\n", m_urlString);
        rv = m_nntpProtocol->LoadURL(m_url, nsnull /* display stream */);
    }
    
	return rv;
	

}

nsresult
nsNntpTestDriver::OnPostMessage()
{
    nsresult rv = NS_OK;
    char *subject;
    char *message;
    char *newsgroup;
    
    rv = PromptForUserDataAndBuildUrl("Newsgroup: ");
    newsgroup =PL_strdup(m_userData);
    
    m_urlString[0] = '\0';
    PL_strcpy(m_urlString, m_urlSpec);
    PL_strcat(m_urlString, "/");
    PL_strcat(m_urlString, m_userData);

    // now we need to attach a message

    
    rv = PromptForUserDataAndBuildUrl("Subject: ");
    subject = PL_strdup(m_userData);
    printf("Enter your message below. End with a blank line.\n");
    rv = PromptForUserDataAndBuildUrl("");
    int messagelen = 0;
    message = NULL;
    while (m_userData[0]) {
        int linelen = PL_strlen(m_userData);
        char *newMessage = (char *)PR_Malloc(linelen+messagelen+2);
        messagelen = linelen+messagelen+2;
        
        newMessage[0]='\0';
        if (message) PL_strcpy(newMessage, message);
        PL_strcat(newMessage, m_userData);
        PL_strcat(newMessage, "\n");
        PR_FREEIF(message);
        message = newMessage;
        rv = PromptForUserDataAndBuildUrl("");
    }

    printf("Ready to post the message:\n");
    printf("Subject: %s\n", subject);
    printf("Message:\n %s\n", message);
    
    SetupUrl(m_userData);

    nsINNTPNewsgroupPost *post;
    rv = NS_NewNewsgroupPost(&post);
    
    if (NS_SUCCEEDED(rv)) {
        post->AddNewsgroup(newsgroup);
        post->SetBody(message);
        post->SetSubject(subject);

        // fake out these headers so that it's a valid post
        post->SetFrom("userid@somewhere.com");
    }
    
    m_url->SetMessageToPost(post);
    
    printf("Running %s\n", m_urlString);
    rv = m_nntpProtocol->LoadURL(m_url, nsnull /* display stream */);

	return rv;
}

nsresult nsNntpTestDriver::OnGetGroup()
{
	nsresult rv = NS_OK;

	// first, prompt the user for the name of the group to fetch
	rv = PromptForUserDataAndBuildUrl("Group to fetch: ");
	// no prompt for url data....just append a '*' to the url data and run it...
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "/");
	PL_strcat(m_urlString, m_userData);

	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);
	else
		m_url->SetSpec(m_urlString); // reset spec

    if (NS_SUCCEEDED(rv)) {
        SetupUrl(m_userData);
        printf("Running %s\n", m_urlString);
		rv = m_nntpProtocol->LoadURL(m_url, nsnull /* displayStream */);
	} // if user provided the data...

	return rv;
}


nsresult nsNntpTestDriver::OnReadNewsRC()
{
	nsresult rv = NS_OK;
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);

	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);
	else
		m_url->SetSpec(m_urlString); // reset spec


	// a read newsrc url is of the form: news://
	// or news://HOST
    printf("Running %s\n", m_urlString);
	rv = m_nntpProtocol->LoadURL(m_url);
	return rv;
}

nsresult nsNntpTestDriver::SetupUrl(char *groupname)
{
    nsresult rv = NS_OK;
    
	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);
	else
		rv = m_url->SetSpec(m_urlString); // reset spec
    
    // before we re-load, assume it is a group command and configure our nntpurl correctly...
    nsINNTPHost * host = nsnull;
    nsINNTPNewsgroup * group = nsnull;
    nsINNTPNewsgroupList * list = nsnull;
    rv = m_url->GetNntpHost(&host);
    if (host)
        {
			rv = host->FindGroup(groupname, &group);
			if (group)
				group->GetNewsgroupList(&list);
            
			rv = m_url->SetNewsgroup(group);
			rv = m_url->SetNewsgroupList(list);
			NS_IF_RELEASE(group);
			NS_IF_RELEASE(list);
			NS_IF_RELEASE(host);
        }

	return rv;
    
} // if user provided the data...


/////////////////////////////////////////////////////////////////////////////////
// End on command handlers for news
////////////////////////////////////////////////////////////////////////////////

int main()
{
	nsINetService * pNetService;
    PLEventQueue *queue;
    nsresult result;

    nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kNntpUrlCID, NULL, NULL, NEWS_DLL, PR_FALSE, PR_FALSE);

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
	nsNntpTestDriver * driver = new nsNntpTestDriver(pNetService,queue);
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
