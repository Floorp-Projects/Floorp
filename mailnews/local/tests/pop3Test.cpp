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
#include "nsRepository.h"
#include "nsString.h"

#include "nsPop3Protocol.h"
#include "nsPop3URL.h"
#include "nsPop3Sink.h"

// include the event sinks for the protocol you are testing

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
static NS_DEFINE_IID(kIPop3UrlIID, NS_IPOP3URL_IID);
static NS_DEFINE_IID(kIPop3SinkIID, NS_IPOP3SINK_IID);

static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

/////////////////////////////////////////////////////////////////////////////////
// Define default values to be used to drive the test
/////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_HOST	"nsmail-2.mcom.com"
#define DEFAULT_PORT	POP3_PORT		/* we get this value from nsPop3Protocol.h */
#define DEFAULT_URL_TYPE  "sockstub://"	/* do NOT change this value until netlib re-write is done...*/

//extern NET_StreamClass *MIME_MessageConverter(int format_out, void *closure, 
//											  URL_Struct *url, MWContext *context);

#ifdef XP_UNIX
extern "C" char *fe_GetConfigDir(void) {
  printf("XXX: return /tmp for fe_GetConfigDir\n");
  return strdup("/tmp");
}
#endif /* XP_UNIX */

extern NET_POP3TooEarlyForEnd(PRInt32 len);

/*
 * This function takes an error code and associated error data
 * and creates a string containing a textual description of
 * what the error is and why it happened.
 *
 * The returned string is allocated and thus should be freed
 * once it has been used.
 *
 * This function is defined in mkmessag.c.
 */
char * NET_ExplainErrorDetails (int code, ...)
{
	char * rv = PR_smprintf("%s", "Error descriptions not implemented yet");
	return rv;
}

char * NET_SACopy (char **destination, const char *source)
{
	if(*destination)
	  {
	    XP_FREE(*destination);
		*destination = 0;
	  }
    if (! source)
	  {
        *destination = NULL;
	  }
    else 
	  {
        *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
        if (*destination == NULL) 
 	        return(NULL);

        PL_strcpy (*destination, source);
      }
    return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
char * NET_SACat (char **destination, const char *source)
{
    if (source && *source)
      {
        if (*destination)
          {
            int length = PL_strlen (*destination);
            *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
            if (*destination == NULL)
            return(NULL);

            PL_strcpy (*destination + length, source);
          }
        else
          {
            *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
            if (*destination == NULL)
                return(NULL);

             PL_strcpy (*destination, source);
          }
      }
    return *destination;
}

char *MSG_UnEscapeSearchUrl (const char *commandSpecificData)
{
	char *result = (char*) PR_Malloc (PL_strlen(commandSpecificData) + 1);
	if (result)
	{
		char *resultPtr = result;
		while (1)
		{
			char ch = *commandSpecificData++;
			if (!ch)
				break;
			if (ch == '\\')
			{
				char scratchBuf[3];
				scratchBuf[0] = (char) *commandSpecificData++;
				scratchBuf[1] = (char) *commandSpecificData++;
				scratchBuf[2] = '\0';
				int accum = 0;
				sscanf (scratchBuf, "%X", &accum);
				*resultPtr++ = (char) accum;
			}
			else
				*resultPtr++ = ch;
		}
		*resultPtr = '\0';
	}
	return result;
}



/* Buffer management.
   Why do I feel like I've written this a hundred times before?
 */
int
msg_GrowBuffer (PRUint32 desired_size, PRUint32 element_size, PRUint32 quantum,
				char **buffer, PRUint32 *size)
{
  if (*size <= desired_size)
	{
	  char *new_buf;
	  PRUint32 increment = desired_size - *size;
	  if (increment < quantum) /* always grow by a minimum of N bytes */
		increment = quantum;

#ifdef TESTFORWIN16
	  if (((*size + increment) * (element_size / sizeof(char))) >= 64000)
		{
		  /* Make sure we don't choke on WIN16 */
		  XP_ASSERT(0);
		  return MK_OUT_OF_MEMORY;
		}
#endif /* DEBUG */

	  new_buf = (*buffer
				 ? (char *) PR_REALLOC (*buffer, (*size + increment)
										* (element_size / sizeof(char)))
				 : (char *) PR_MALLOC ((*size + increment)
									  * (element_size / sizeof(char))));
	  if (! new_buf)
		return MK_OUT_OF_MEMORY;
	  *buffer = new_buf;
	  *size += increment;
	}
  return 0;
}


/* Take the given buffer, tweak the newlines at the end if necessary, and
   send it off to the given routine.  We are guaranteed that the given
   buffer has allocated space for at least one more character at the end. */
static PRInt32
msg_convert_and_send_buffer(char* buf, PRUint32 length, PRBool convert_newlines_p,
							PRInt32 (*per_line_fn) (char *line,
                                                     PRUint32 line_length,
                                                     void *closure),
							void *closure)
{
    /* Convert the line terminator to the native form.
     */
    char* newline;
    
    PR_ASSERT(buf && length > 0);
    if (!buf || length <= 0) return -1;
    newline = buf + length;
    
    PR_ASSERT(newline[-1] == CR || newline[-1] == LF);
    if (newline[-1] != CR && newline[-1] != LF) return -1;
    
    /* update count of bytes parsed adding/removing CR or LF*/
    NET_POP3TooEarlyForEnd(length);	
    
    if (!convert_newlines_p)
	{
	}
#if (LINEBREAK_LEN == 1)
    else if ((newline - buf) >= 2 &&
             newline[-2] == CR &&
             newline[-1] == LF)
	{
        /* CRLF -> CR or LF */
        buf [length - 2] = LINEBREAK[0];
        length--;
	}
    else if (newline > buf + 1 &&
             newline[-1] != LINEBREAK[0])
	{
        /* CR -> LF or LF -> CR */
        buf [length - 1] = LINEBREAK[0];
	}
#else
    else if (((newline - buf) >= 2 && newline[-2] != CR) ||
             ((newline - buf) >= 1 && newline[-1] != LF))
	{
        /* LF -> CRLF or CR -> CRLF */
        length++;
        buf[length - 2] = LINEBREAK[0];
        buf[length - 1] = LINEBREAK[1];
	}
#endif
    
    return (*per_line_fn)(buf, length, closure);
}


/* SI::BUFFERED-STREAM-MIXIN
   Why do I feel like I've written this a hundred times before?
   */

PRInt32 msg_LineBuffer (const char *net_buffer, PRInt32 net_buffer_size,
                        char **bufferP, PRUint32 *buffer_sizeP,
                        PRUint32 *buffer_fpP,
                        PRBool convert_newlines_p,
                        PRInt32 (*per_line_fn) (char *line, PRUint32
                                            line_length, void *closure),
                        void *closure)
{
    int status = 0;
    if (*buffer_fpP > 0 && *bufferP && (*bufferP)[*buffer_fpP - 1] == CR &&
        net_buffer_size > 0 && net_buffer[0] != LF) {
        /* The last buffer ended with a CR.  The new buffer does not start
           with a LF.  This old buffer should be shipped out and discarded. */
        PR_ASSERT(*buffer_sizeP > *buffer_fpP);
        if (*buffer_sizeP <= *buffer_fpP) return -1;
        status = msg_convert_and_send_buffer(*bufferP, *buffer_fpP,
                                             convert_newlines_p,
                                             per_line_fn, closure);
        if (status < 0) return status;
        *buffer_fpP = 0;
    }
    while (net_buffer_size > 0)
	{
        const char *net_buffer_end = net_buffer + net_buffer_size;
        const char *newline = 0;
        const char *s;
        
        
        for (s = net_buffer; s < net_buffer_end; s++)
		{
            /* Move forward in the buffer until the first newline.
               Stop when we see CRLF, CR, or LF, or the end of the buffer.
               *But*, if we see a lone CR at the *very end* of the buffer,
               treat this as if we had reached the end of the buffer without
               seeing a line terminator.  This is to catch the case of the
               buffers splitting a CRLF pair, as in "FOO\r\nBAR\r" "\nBAZ\r\n".
		   */
            if (*s == CR || *s == LF)
			{
                newline = s;
                if (newline[0] == CR)
				{
                    if (s == net_buffer_end - 1)
					{
                        /* CR at end - wait for the next character. */
                        newline = 0;
                        break;
					}
                    else if (newline[1] == LF)
                        /* CRLF seen; swallow both. */
                        newline++;
				}
                newline++;
			  break;
			}
		}
        
        /* Ensure room in the net_buffer and append some or all of the current
           chunk of data to it. */
        {
            const char *end = (newline ? newline : net_buffer_end);
            PRUint32 desired_size = (end - net_buffer) + (*buffer_fpP) + 1;
            
            if (desired_size >= (*buffer_sizeP))
            {
                status = msg_GrowBuffer (desired_size, sizeof(char), 1024,
                                         bufferP, buffer_sizeP);
                if (status < 0) return status;
            }
            memcpy ((*bufferP) + (*buffer_fpP), net_buffer, (end - net_buffer));
            (*buffer_fpP) += (end - net_buffer);
        }
        
        /* Now *bufferP contains either a complete line, or as complete
           a line as we have read so far.
           
           If we have a line, process it, and then remove it from `*bufferP'.
           Then go around the loop again, until we drain the incoming data.
           */
        if (!newline)
            return 0;
        
        status = msg_convert_and_send_buffer(*bufferP, *buffer_fpP,
                                             convert_newlines_p,
                                             per_line_fn, closure);
        if (status < 0) return status;
        
        net_buffer_size -= (newline - net_buffer);
        net_buffer = newline;
        (*buffer_fpP) = 0;
	}
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////
// This function is used to load and prepare an pop3 url which can be run by
// a transport instance. For different protocols, you'll have different url
// functions like this one in the test harness...
/////////////////////////////////////////////////////////////////////////////////
nsresult NS_NewPop3URL(nsIPop3URL ** aResult, const nsString urlSpec)
{
	nsIURL * pUrl = NULL;
	nsresult rv = NS_OK;

	 nsPop3URL * pop3URL = new nsPop3URL(nsnull, nsnull);
	 if (pop3URL)
	 {
		pop3URL->ParseURL(urlSpec);  // load the spec we were given...
		rv = pop3URL->QueryInterface(kIPop3UrlIID, (void **) aResult);
	 }

	 return rv;
}

//////////////////////////////////////////////////////////////////////////////////
// The nsPop3TestDriver is a class that I envision could be generalized to form the
// building block of a protocol test harness. To configure it, you would list all of
// the events you know how to handle and when one of those events is triggered, you
// would be asked to process it....right now it is just Pop3 specific....
///////////////////////////////////////////////////////////////////////////////////

class nsPop3TestDriver
{
public:
	nsPop3TestDriver(nsINetService * pService);
	virtual ~nsPop3TestDriver();

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

	nsIPop3URL * m_url; 
	nsPop3Protocol * m_pop3Protocol; // running protocol instance
	nsITransport * m_transport; // a handle on the current transport object being used with the protocol binding...

	PRBool		m_runningURL;	// are we currently running a url? this flag is set to false on exit...

	void InitializeProtocol(const char * urlSpec);
	PRBool m_protocolInitialized; 
};

nsPop3TestDriver::nsPop3TestDriver(nsINetService * pNetService)
{
	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_url = nsnull;
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
	pNetService->CreateSocketTransport(&m_transport, m_port, m_host);
	m_pop3Protocol = nsnull; // we can't create it until we have a url...
}

void nsPop3TestDriver::InitializeProtocol(const char * urlString)
{
	// this is called when we don't have a url nor a protocol instance yet...
	NS_NewPop3URL(&m_url, urlString);
	// now create a protocl instance...
	m_pop3Protocol = new nsPop3Protocol(m_url, m_transport);
    m_pop3Protocol->SetUsername(m_username);
    m_pop3Protocol->SetPassword(m_password);
    
	m_protocolInitialized = PR_TRUE;

    nsPop3Sink* aPop3Sink = new nsPop3Sink;
    if (aPop3Sink)
    {
        aPop3Sink->SetMailDirectory(m_mailDirectory);
        m_url->SetPop3Sink(aPop3Sink);
    }
}

nsPop3TestDriver::~nsPop3TestDriver()
{
	NS_IF_RELEASE(m_url);
	NS_IF_RELEASE(m_transport);
    PR_FREEIF(m_username);
    PR_FREEIF(m_password);
    PR_FREEIF(m_mailDirectory);
	delete m_pop3Protocol;
}

nsresult nsPop3TestDriver::RunDriver()
{
	nsresult status = NS_OK;

	while (m_runningURL)
	{
		// if we haven't gotten started (and created a protocol) or
		// if the protocol instance is currently not busy, then read in a new command
		// and process it...
		if ((!m_pop3Protocol) || m_pop3Protocol->IsRunning() == PR_FALSE) // if we aren't running the url anymore, ask ueser for another command....
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

void nsPop3TestDriver::InitializeTestDriver()
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
	scanf("%[^\n]", portString);
	if (portString && *portString)
	{
		m_port = atoi(portString);
	}
	scanf("%c", portString);  // eat the extra CR

	// now prompt for the host name....
	printf("Enter host name to use [%s]: ", m_host);
	scanf("%[^\n]", hostString);
	scanf("%c", portString);  // eat the extra CR
	if(hostString && *hostString)
	{
		PL_strcpy(m_host, hostString);
	}

	PL_strcat(m_urlSpec, m_host);
    // we'll actually build the url (spec + user data) once the user has specified a command they want to try...

	// now prompt for the mail account name....
    hostString[0] = 0;
	printf("Enter mail account name [%s]: ", m_username);
	scanf("%[^\n]", hostString);
	scanf("%c", portString);  // eat the extra CR
	if(hostString && *hostString)
	{
        PR_FREEIF(m_username);
		m_username = PL_strdup(hostString);
	}

	// now prompt for the mail account password .....
    hostString[0] = 0;
	printf("Enter mail account password [%s]: ", m_password);
	scanf("%[^\n]", hostString);
	scanf("%c", portString);  // eat the extra CR
	if(hostString && *hostString)
	{
        PR_FREEIF(m_password);
		m_password = PL_strdup(hostString);
	}

	// now prompt for the local mail folder directory .....
    hostString[0] = 0;
	printf("Enter local mail folder directory [%s]: ", m_mailDirectory);
	scanf("%[^\n]", hostString);
	scanf("%c", portString);  // eat the extra CR
	if(hostString && *hostString)
	{
        PR_FREEIF(m_mailDirectory);
		m_mailDirectory = PL_strdup(hostString);
	}
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
	printf("9) Exit the test application. \n");
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////
// Begin protocol specific command url generation code...gee that's a mouthful....
///////////////////////////////////////////////////////////////////////////////////

nsresult nsPop3TestDriver::OnExit()
{
	printf("Terminating Pop3 test harness....\n");
	m_runningURL = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}


nsresult nsPop3TestDriver::OnCheck()
{
	nsresult rv = NS_OK; 

	// no prompt for url data....just append a '*' to the url data and run it...
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "?check");
	
	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);

	m_url->SetSpec(m_urlString); // reset spec
	rv = m_pop3Protocol->Load(m_url);
	return rv;
}

nsresult nsPop3TestDriver::OnGUrl()
{
	nsresult rv = NS_OK;
		// no prompt for url data....just append a '*' to the url data and run it...
	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "?gurl");

	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);
	else
		rv = m_url->SetSpec(m_urlString); // reset spec
	
	// load the correct newsgroup interface as an event sink...
	if (NS_SUCCEEDED(rv))
	{
		 // before we re-load, assume it is a group command and configure our Pop3URL correctly...

		rv = m_pop3Protocol->Load(m_url);
	} // if user provided the data...

	return rv;
}

nsresult nsPop3TestDriver::OnUidl()
{
	nsresult rv = NS_OK;

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

	return rv;
}

nsresult nsPop3TestDriver::OnGet()
{
	nsresult rv = NS_OK;

	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);

	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);
	else
		rv = m_url->SetSpec(m_urlString); // reset spec
	
	// load the correct newsgroup interface as an event sink...
	if (NS_SUCCEEDED(rv))
	{
		 // before we re-load, assume it is a group command and configure our Pop3URL correctly...

		rv = m_pop3Protocol->Load(m_url);
	} // if user provided the data...

	return rv;
}

int main()
{
	nsINetService * pNetService;
    nsresult result;
    nsIURL * pURL = NULL;

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

	// now register a mime converter....
    //	NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_NGLAYOUT, NULL, MIME_MessageConverter);
    //	NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_CACHE_AND_NGLAYOUT, NULL, MIME_MessageConverter);

	// okay, everything is set up, now we just need to create a test driver and run it...
	nsPop3TestDriver * driver = new nsPop3TestDriver(pNetService);
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
