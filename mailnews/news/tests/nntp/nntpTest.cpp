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

#include <stdio.h>
#include <assert.h>

#ifdef XP_PC
#include <windows.h>
#endif

#include "nsCOMPtr.h"

#include "plstr.h"
#include "plevent.h"

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsITransport.h"
#include "nsIURL.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nntpCore.h"

#include "nsINNTPNewsgroupPost.h"
#include "nsINNTPNewsgroup.h"
#include "nsINNTPHost.h"
#include "nsINntpUrl.h"

#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsIEventQueue.h" 
#include "nsIEventQueueService.h"
#include "nsIUrlListener.h"

#include "nsIPref.h"
#include "nsIFileLocator.h"

#include "nsMsgNewsCID.h"

#ifdef XP_PC
#define NETLIB_DLL	"netlib.dll"
#define XPCOM_DLL	"xpcom32.dll"
#define NEWS_DLL	"msgnews.dll"
#define PREF_DLL	"xppref32.dll"
#define APPSHELL_DLL	"nsappshell.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define NEWS_DLL   "libmsgnews"MOZ_DLL_SUFFIX
#define PREF_DLL   "libpref"MOZ_DLL_SUFFIX
#define APPSHELL_DLL "libnsappshell"MOZ_DLL_SUFFIX
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kNntpUrlCID, NS_NNTPURL_CID);
static NS_DEFINE_CID(kNNTPProtocolCID, NS_NNTPPROTOCOL_CID);
static NS_DEFINE_CID(kNNTPNewsgroupPostCID, NS_NNTPNEWSGROUPPOST_CID);
static NS_DEFINE_CID(kNNTPNewsgroupCID, NS_NNTPNEWSGROUP_CID);

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);

/////////////////////////////////////////////////////////////////////////////////
// Define default values to be used to drive the test
/////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_HOST	"news.mozilla.org"
#define DEFAULT_PORT	NEWS_PORT		/* we get this value from nntpCore.h */
#define DEFAULT_URL_TYPE  "news://"	/* do NOT change this value until netlib re-write is done...*/
#define DEFAULT_ARTICLE "37099AC5.8D0EB52@netscape.com"

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

class nsNntpTestDriver : public nsIUrlListener
{
public:
	nsNntpTestDriver(nsINetService * pService, nsIEventQueue *queue);
	virtual ~nsNntpTestDriver();
	
	NS_DECL_ISUPPORTS

	// nsIUrlListener support
	NS_IMETHOD OnStartRunningUrl(nsIURL * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode);

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
	nsresult OnRunURL();
	nsresult OnExit(); 
protected:
    nsCOMPtr <nsIEventQueue> m_eventQueue;
	char m_urlSpec[200];	// "sockstub://hostname:port" it does not include the command specific data...
	char m_urlString[500];	// string representing the current url being run. Includes host AND command specific data.
	char m_userData[250];	// generic string buffer for storing the current user entered data...

	// host and port info...
	PRUint32	m_port;
	char		m_host[200];		

	nsCOMPtr <nsINntpUrl> m_url; 
	nsCOMPtr <nsINNTPProtocol> m_nntpProtocol; // running protocol instance
	nsCOMPtr <nsITransport> m_transport; // a handle on the current transport object being used with the protocol binding...

	PRBool		m_runningURL;
	PRBool		m_runTestHarness;

	nsresult InitializeProtocol(const char * urlSpec);
    nsresult SetupUrl(char *group);
	PRBool m_protocolInitialized; 
};

nsNntpTestDriver::nsNntpTestDriver(nsINetService * pNetService,
                                   nsIEventQueue *queue)
{
	NS_INIT_REFCNT();

	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_protocolInitialized = PR_FALSE;
	m_runTestHarness = PR_TRUE;
	m_runningURL = PR_FALSE;
    m_eventQueue = queue;
	NS_IF_ADDREF(queue);
	
	InitializeTestDriver(); // prompts user for initialization information...
	
	// create a transport socket...
	pNetService->CreateSocketTransport(getter_AddRefs(m_transport), m_port, m_host);
}

nsresult 
nsNntpTestDriver::InitializeProtocol(const char * urlString)
{
    nsresult rv = NS_OK;

	// this is called when we don't have a url nor a protocol instance yet...
	rv = nsComponentManager::CreateInstance(kNntpUrlCID, nsnull, nsINntpUrl::GetIID(), getter_AddRefs(m_url));

	if (NS_FAILED(rv)) { 
        printf("InitializeProtocol failed\n");
        m_protocolInitialized = PR_FALSE;
        return rv;
    }

    m_url->SetSpec(urlString);

	m_url->RegisterListener(this);

	// now create a protocol instance...
    rv = nsComponentManager::CreateInstance(kNNTPProtocolCID, nsnull, nsINNTPProtocol::GetIID(), getter_AddRefs(m_nntpProtocol));
    if (NS_FAILED(rv)) {
        m_protocolInitialized = PR_FALSE;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        rv = m_nntpProtocol->Initialize(m_url, m_transport);
        if (NS_FAILED(rv)) {
            m_protocolInitialized = PR_FALSE;
        }
        else {
            m_protocolInitialized = PR_TRUE;
        }
        return rv;
    }
}

nsNntpTestDriver::~nsNntpTestDriver()
{
	if (m_url)
		m_url->UnRegisterListener(this);
}

NS_IMPL_ISUPPORTS(nsNntpTestDriver, nsIUrlListener::GetIID())

NS_IMETHODIMP nsNntpTestDriver::OnStartRunningUrl(nsIURL * aUrl)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	m_runningURL = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsNntpTestDriver::OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	nsresult rv = NS_OK;
	m_runningURL = PR_FALSE;
	return rv;
}

nsresult nsNntpTestDriver::RunDriver()
{
	nsresult status = NS_OK;

	while (m_runTestHarness)
	{
		if (m_runningURL == PR_FALSE) // can we run and dispatch another command?
		{
			status = ReadAndDispatchCommand();	 // run a new url
		}

	 // if running url
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
	printf("5) Perform Search. (not working)\n");
	printf("6) Read NewsRC file. (not working)\n");
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
	m_runTestHarness = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}


nsresult nsNntpTestDriver::OnListAllGroups()
{
	nsresult rv = NS_OK;
    PRInt32 status = 0;
    printf("Listing all groups..\n");
	// no prompt for url data....just append a '*' to the url data and run it...
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "/");
	PL_strcat(m_urlString, "*");

	if (m_protocolInitialized == PR_FALSE){
        rv = InitializeProtocol(m_urlString);
        if (NS_FAILED(rv) || !m_url) {
            return rv;
        }
    }

	m_url->SetSpec(m_urlString); // reset spec
    printf("Running %s\n", m_urlString);
	rv = m_nntpProtocol->LoadUrl(m_url, nsnull);
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
        PRInt32 status = 0;
        rv = m_nntpProtocol->LoadUrl(m_url, nsnull);
    }

	return rv;
}

nsresult nsNntpTestDriver::OnListArticle()
{
	nsresult rv = NS_OK;

	// first, prompt the user for the name of the group to fetch
	// prime article number with a default value...
	m_userData[0] = '\0';
	PL_strcpy(m_userData, DEFAULT_ARTICLE);
	rv = PromptForUserDataAndBuildUrl("Article to Fetch (note, the default ("DEFAULT_ARTICLE") only lives on "DEFAULT_HOST"): ");
	// no prompt for url data....just append a '*' to the url data and run it...
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "/");
	PL_strcat(m_urlString, m_userData);

	if (NS_SUCCEEDED(rv)) {
        PRInt32 status = 0;
        SetupUrl(m_userData);
        printf("Running %s\n", m_urlString);
        rv = m_nntpProtocol->LoadUrl(m_url, nsnull);
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
        PRInt32 status = 0 ;
        SetupUrl(m_userData);
        printf("Running %s\n", m_urlString);
        rv = m_nntpProtocol->LoadUrl(m_url, nsnull);
    }
    
	return rv;
	

}

nsresult
nsNntpTestDriver::OnPostMessage()
{
    PRInt32 status = 0;
    nsresult rv = NS_OK;
    char *subject;
    char *message;
    char *newsgroup;
    
    rv = PromptForUserDataAndBuildUrl("Newsgroup: ");
    newsgroup = PL_strdup(m_userData);
    
    m_urlString[0] = '\0';
    PL_strcpy(m_urlString, m_urlSpec);
    PL_strcat(m_urlString, "/");
    PL_strcat(m_urlString, m_userData);

    // now we need to attach a message

    
    rv = PromptForUserDataAndBuildUrl("Subject: ");
    subject = PL_strdup(m_userData);
    printf("Enter your message below. End with a line with a dot (.)\n");
    rv = PromptForUserDataAndBuildUrl("");
    int messagelen = 0;
    message = NULL;
    while (m_userData[0]) {
		if (m_userData[0] == '.') {
			break;
		}

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
    printf("Message:\n%s\n", message);
    
    SetupUrl(m_userData);

    nsCOMPtr<nsINNTPNewsgroupPost> post;
    rv = nsComponentManager::CreateInstance(kNNTPNewsgroupPostCID, nsnull, nsINNTPNewsgroupPost::GetIID(), getter_AddRefs(post));
    
    if (NS_SUCCEEDED(rv)) {
        post->AddNewsgroup(newsgroup);
        post->SetBody(message);
        post->SetSubject(subject);

        // fake out these headers so that it's a valid post
		// todo: get this from the the account manager
        post->SetFrom("userid@somewhere.com");
    }
    
    m_url->SetMessageToPost(post);
    
    printf("Running %s\n", m_urlString);
    rv = m_nntpProtocol->LoadUrl(m_url, nsnull);

	return rv;
}

nsresult nsNntpTestDriver::OnGetGroup()
{
	nsresult rv = NS_OK;
    PRInt32 status = 0;
    
	// first, prompt the user for the name of the group to fetch
	rv = PromptForUserDataAndBuildUrl("Group to fetch: ");
	// no prompt for url data....just append a '*' to the url data and run it...
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "/");
	PL_strcat(m_urlString, m_userData);

	if (m_protocolInitialized == PR_FALSE) {
		rv = InitializeProtocol(m_urlString);
        if (NS_FAILED(rv) || !m_url) {
            return rv;
        }
    }
	else
		m_url->SetSpec(m_urlString); // reset spec

	nsCOMPtr<nsINNTPNewsgroup> newsgroup;
    rv = nsComponentManager::CreateInstance(kNNTPNewsgroupCID, nsnull, nsINNTPNewsgroup::GetIID(), getter_AddRefs(newsgroup));
    
    if (NS_SUCCEEDED(rv)) {
        newsgroup->Initialize(nsnull /* line */, nsnull /* set */, PR_FALSE /* subscribed */);
        newsgroup->SetName(m_userData);
    }
	else {
		return rv;
	}

    m_url->SetNewsgroup(newsgroup);

    if (NS_SUCCEEDED(rv)) {
        SetupUrl(m_userData);
        printf("Running %s\n", m_urlString);
		rv = m_nntpProtocol->LoadUrl(m_url, nsnull);
	} // if user provided the data...

	return rv;
}


nsresult nsNntpTestDriver::OnReadNewsRC()
{
    PRInt32 status = 0;
	nsresult rv = NS_OK;
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);

	if (m_protocolInitialized == PR_FALSE) {
		rv = InitializeProtocol(m_urlString);
        if (NS_FAILED(rv) || !m_url) {
            return rv;
        }
    }
	else
		m_url->SetSpec(m_urlString); // reset spec


	// a read newsrc url is of the form: news://
	// or news://HOST
    printf("Running %s\n", m_urlString);
	rv = m_nntpProtocol->LoadUrl(m_url, nsnull);
	return rv;
}

nsresult nsNntpTestDriver::SetupUrl(char *groupname)
{
    nsresult rv = NS_OK;
    
	if (m_protocolInitialized == PR_FALSE) {
		rv = InitializeProtocol(m_urlString);
        if (NS_FAILED(rv) || !m_url) {
            return rv;
        }
    }
	else
		rv = m_url->SetSpec(m_urlString); // reset spec
    
    // before we re-load, assume it is a group command and configure our nntpurl correctly...
    nsCOMPtr <nsINNTPHost> host;
    nsCOMPtr <nsINNTPNewsgroup> group;
    nsCOMPtr <nsINNTPNewsgroupList> list;
    rv = m_url->GetNntpHost(getter_AddRefs(host));
    if (NS_SUCCEEDED(rv) && host) {
        rv = host->FindGroup(groupname, getter_AddRefs(group));
        
        if (NS_SUCCEEDED(rv) && group) {
            group->GetNewsgroupList(getter_AddRefs(list));
        }
        else {
#ifdef DEBUG_sspitzer
            printf("failed to find group %s\n",groupname);
#endif
			return rv;
        }

        rv = m_url->SetNewsgroup(group);
		if (NS_FAILED(rv)) {
#ifdef DEBUG_sspitzer
			printf("setnewsgroup failed\n");
#endif
			return rv;
		}
        rv = m_url->SetNewsgroupList(list);
		if (NS_FAILED(rv)) {
#ifdef DEBUG_sspitzer
			printf("setnewsgrouplist failed\n");
#endif
			return rv;
		}
    }
    else {
#ifdef DEBUG_sspitzer
		printf("failed to get nntp host\n");
#endif
		return rv;
    }

	return rv;
    
} // if user provided the data...


/////////////////////////////////////////////////////////////////////////////////
// End on command handlers for news
////////////////////////////////////////////////////////////////////////////////

int main()
{
	nsINetService * pNetService;
    nsCOMPtr<nsIEventQueue> queue;
    nsresult result;

    nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kEventQueueCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kNntpUrlCID, NULL, NULL, NEWS_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kNNTPProtocolCID, NULL, NULL, NEWS_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
	// make sure prefs get initialized and loaded..
	// mscott - this is just a bad bad bad hack right now until prefs
	// has the ability to take nsnull as a parameter. Once that happens,
	// prefs will do the work of figuring out which prefs file to load...
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &result); 
    if (NS_FAILED(result) || !prefs) {
        exit(result);
    }

	// Create the Event Queue for this thread...
	NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &result);
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
        pEventQService->GetThreadEventQueue(PR_GetCurrentThread(),getter_AddRefs(queue));
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
		NS_ADDREF(driver);
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		NS_RELEASE(driver);
	}

	// shut down:
	NS_RELEASE(pNetService);
    
    return 0;
}
