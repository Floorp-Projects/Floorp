/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "msgCore.h"    // precompiled header...

#include "nsPop3Protocol.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "MailNewsTypes.h"
#include "nsString2.h"

#include "nsINetService.h"

#if 0
#include "nsINetSupportDialogService.h"

static NS_DEFINE_CID(kCNetSupportDialog, NS_NETSUPPORTDIALOG_CID);
#endif

static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

char * NET_SACopy (char **destination, const char *source)
{
	if(*destination)
	  {
	    PR_Free(*destination);
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

/* km
 *
 *
 *  Process state: POP3_START_USE_TOP_FOR_FAKE_UIDL
 *
 *	If we get here then UIDL and XTND are not supported by the mail server.
 * 
 *	Now we will walk backwards, from higher message numbers to lower, 
 *  using the TOP command. Along the way, keep track of messages to down load
 *  by POP3_GET_MSG.
 *
 *  Stop when we reach a msg that we knew about before or we run out of
 *  messages.
 *
 *	There is also conditional code to handle:
 *		BIFF
 *		Pop3ConData->only_uidl == true (fetching one message only)
 *      emptying message hash table if all messages are new
 *		Deleting messages that are marked deleted.
 *
 *
*/

/* this function gets called for each hash table entry.  If it finds a message
   marked delete, then we will have to traverse all of the server messages
   with TOP in order to delete those that are marked delete.


   If UIDL or XTND XLST are supported then this function will never get called.
   
   A pointer to this function is passed to XP_Maphash     -km */

static PR_CALLBACK PRIntn
net_pop3_check_for_hash_messages_marked_delete(PLHashEntry* he,
				       						   PRIntn index, 
				       						   void *arg)
{
	char valueChar = (char) (int) he->value;
	if (valueChar == DELETE_CHAR)
	{
		((Pop3ConData *) arg)->delete_server_message_during_top_traversal = PR_TRUE;
		return HT_ENUMERATE_STOP;	/* XP_Maphash will stop traversing hash table now */
	}
	
	return HT_ENUMERATE_NEXT;		/* XP_Maphash will continue traversing the hash */
}

static int
uidl_cmp (const void *obj1, const void *obj2)
{
  PR_ASSERT (obj1 && obj2);
  return PL_strcmp ((char *) obj1, (char *) obj2);
}


static void
put_hash(Pop3UidlHost* host, PLHashTable* table, const char* key, char value)
{
  Pop3AllocedString* tmp;
  int v = value;

  tmp = PR_NEWZAP(Pop3AllocedString);
  if (tmp) {
	tmp->str = PL_strdup(key);
	if (tmp->str) {
	  tmp->next = host->strings;
	  host->strings = tmp;
	  PL_HashTableAdd(table, (const void *)tmp->str, (void*) v);
	} else {
	  PR_Free(tmp);
	}
  }
}

static Pop3UidlHost* 
net_pop3_load_state(const char* searchhost, 
                    const char* searchuser,
                    const char* mailDirectory)
{
  char* buf;
  char* newStr;
  char* host;
  char* user;
  char* uidl;
  char* flags;
  Pop3UidlHost* result = NULL;
  Pop3UidlHost* current = NULL;
  Pop3UidlHost* tmp;

  result = PR_NEWZAP(Pop3UidlHost);
  if (!result) return NULL;
  result->host = PL_strdup(searchhost);
  result->user = PL_strdup(searchuser);
  result->hash = PL_NewHashTable(20, PL_HashString, PL_CompareStrings, PL_CompareValues, nsnull, nsnull);

  if (!result->host || !result->user || !result->hash) {
    PR_FREEIF(result->host);
	PR_FREEIF(result->user);
	if (result->hash) PL_HashTableDestroy(result->hash);
	PR_Free(result);
	return NULL;
  }

  nsFileSpec fileSpec(mailDirectory);
  fileSpec += "popstate.dat";

  nsInputFileStream fileStream(fileSpec);

  buf = (char*)PR_CALLOC(512);
  if (buf) {
	while (!fileStream.eof() && !fileStream.failed() && fileStream.is_open())
  {
      fileStream.readline(buf, 512);
      if (*buf == '#' || *buf == CR || *buf == LF || *buf == 0)
          continue;
	  if (buf[0] == '*') {
        /* It's a host&user line. */
        current = NULL;
        host = nsCRT::strtok(buf + 1, " \t\r\n", &newStr);
        /* XP_FileReadLine uses LF on all platforms */
        user = nsCRT::strtok(newStr, " \t\r\n", &newStr);
        if (host == NULL || user == NULL) continue;
        for (tmp = result ; tmp ; tmp = tmp->next) {
            if (PL_strcmp(host, tmp->host) == 0 &&
			  PL_strcmp(user, tmp->user) == 0) {
                current = tmp;
                break;
            }
        }
        if (!current) {
            current = PR_NEWZAP(Pop3UidlHost);
            if (current) {
                current->host = PL_strdup(host);
                current->user = PL_strdup(user);
                current->hash = PL_NewHashTable(20, PL_HashString, PL_CompareStrings, PL_CompareValues, nsnull, nsnull);
                if (!current->host || !current->user || !current->hash) {
                    PR_FREEIF(current->host);
                    PR_FREEIF(current->user);
                    if (current->hash) PL_HashTableDestroy(current->hash);
                    PR_Free(current);
                } else {
                    current->next = result->next;
                    result->next = current;
                }
            }
        }
	  } else {
        /* It's a line with a UIDL on it. */
        if (current) {
            flags = nsCRT::strtok(buf, " \t\r\n", &newStr);		/* XP_FileReadLine uses LF on all platforms */
            uidl = nsCRT::strtok(newStr, " \t\r\n", &newStr);
            if (flags && uidl) {
                PR_ASSERT((flags[0] == KEEP) || (flags[0] == DELETE_CHAR) ||
                          (flags[0] == TOO_BIG)); 
                if ((flags[0] == KEEP) || (flags[0] == DELETE_CHAR) ||
                    (flags[0] == TOO_BIG)) { 
                    put_hash(current, current->hash, uidl, flags[0]);
                }
            }
        }
	  }
	}

	PR_Free(buf);
  }
  
  return result;
}

static PR_CALLBACK PRIntn
hash_clear_mapper(PLHashEntry* he, PRIntn index, void* arg)
{
#ifdef UNREADY_CODE   // mscott: the compiler won't take this line and I can't figure out why..=(
  PR_FREEIF( (char *)he->key );
#endif
  return HT_ENUMERATE_REMOVE;
}

static PR_CALLBACK PRIntn
hash_empty_mapper(PLHashEntry* he, PRIntn index, void* arg)
{
  *((PRBool*) arg) = PR_FALSE;
  return HT_ENUMERATE_STOP;
}

static PRBool
hash_empty(PLHashTable* hash)
{
  PRBool result = PR_TRUE;
  PL_HashTableEnumerateEntries(hash, hash_empty_mapper, (void *)&result);
  return result;
}


static PR_CALLBACK PRIntn
net_pop3_write_mapper(PLHashEntry* he, PRIntn index, void* arg)
{
  nsOutputFileStream* file = (nsOutputFileStream*) arg;
  PR_ASSERT((he->value == ((void *) (int) KEEP)) ||
			(he->value == ((void *) (int) DELETE_CHAR)) ||
			(he->value == ((void *) (int) TOO_BIG)));
	char* tmpBuffer = PR_smprintf("%c %s" MSG_LINEBREAK, (char)(long)he->value, (char*)
																he->key);
	PR_ASSERT(tmpBuffer);
  *file << tmpBuffer;
	PR_Free(tmpBuffer);
  return HT_ENUMERATE_NEXT;
}

static void
net_pop3_write_state(Pop3UidlHost* host, const char* mailDirectory)
{
  PRInt32 len = 0;

  nsFileSpec fileSpec(mailDirectory);
  fileSpec += "popstate.dat";

  nsOutputFileStream outFileStream(fileSpec, PR_WRONLY | PR_CREATE_FILE |
                                   PR_TRUNCATE);
	char* tmpBuffer = PR_smprintf("%s", "# Netscape POP3 State File" MSG_LINEBREAK
			   "# This is a generated file!  Do not edit." MSG_LINEBREAK MSG_LINEBREAK);
	PR_ASSERT(tmpBuffer);

  outFileStream << tmpBuffer;

 	PR_Free(tmpBuffer);

  for (; host && (len >= 0); host = host->next)
  {
	  if (!hash_empty(host->hash))
	  {
        outFileStream << "*";
        outFileStream << host->host;
        outFileStream << " ";
        outFileStream << host->user;
        outFileStream << MSG_LINEBREAK;
        PL_HashTableEnumerateEntries(host->hash, net_pop3_write_mapper, (void *)&outFileStream);
    }
  }
}

/*
Wrapper routines for POP data. The following routines are used from MSG_FolderInfoMail to
allow deleting of messages that have been kept on a POP3 server due to their size or
a preference to keep the messages on the server. When "deleting" messages we load
our state file, mark any messages we have for deletion and then re-save the state file.
*/
extern char* ReadPopData(char *hostname, char* username, char* maildirectory);
extern void SavePopData(char *data, char* maildirectory);
extern void net_pop3_delete_if_in_server(char *data, char *uidl, PRBool *changed);
extern void KillPopData(char* data);


char* ReadPopData(char *hostname, 
                  char* username, 
                  char* mailDirectory)
{
	Pop3UidlHost *uidlHost = NULL;
	if(!username || !*username)
		return (char*) uidlHost;
	
	uidlHost = net_pop3_load_state(hostname, username, mailDirectory);
	return (char*) uidlHost;
}

void SavePopData(char *data, char* mailDirectory)
{
	Pop3UidlHost *host = (Pop3UidlHost*) data;

	if (!host)
		return;
	net_pop3_write_state(host, mailDirectory);
}

/*
Look for a specific UIDL string in our hash tables, if we have it then we need
to mark the message for deletion so that it can be deleted later. If the uidl of the
message is not found, then the message was downloaded completly and already deleted
from the server. So this only applies to messages kept on the server or too big
for download. */

void net_pop3_delete_if_in_server(char *data, char *uidl, PRBool *changed)
{
	Pop3UidlHost *host = (Pop3UidlHost*) data;
	
	if (!host)
		return;
	if (PL_HashTableLookup (host->hash, (const void*) uidl))
	{
		PL_HashTableAdd(host->hash, uidl, (void*) DELETE_CHAR);
		*changed = PR_TRUE;
	}
}

static void
net_pop3_free_state(Pop3UidlHost* host) 
{
  Pop3UidlHost* h;
  Pop3AllocedString* tmp;
  Pop3AllocedString* next;
  while (host) {
	h = host->next;
	PR_Free(host->host);
	PR_Free(host->user);
	PL_HashTableDestroy(host->hash);
	tmp = host->strings;
	while (tmp) {
	  next = tmp->next;
	  PR_Free(tmp->str);
	  PR_Free(tmp);
	  tmp = next;
	}
	PR_Free(host);
	host = h;
  }
}

void KillPopData(char* data)
{
	if (!data)
		return;
	net_pop3_free_state((Pop3UidlHost*) data);
}

// nsPop3Protocol class implementation

NS_IMPL_ADDREF(nsPop3Protocol)
NS_IMPL_RELEASE(nsPop3Protocol)
NS_IMPL_QUERY_INTERFACE(nsPop3Protocol, nsIStreamListener::GetIID())

NS_IMETHODIMP nsPop3Protocol::OnDataAvailable(nsIURL* aURL, 
                                           nsIInputStream* aInputStream,
                                           PRUint32 aLength)
{
    ProcessPop3State(aURL, aInputStream, aLength);
    return NS_OK;
}

NS_IMETHODIMP nsPop3Protocol::OnStartBinding(nsIURL* aURL, 
                                          const char* aContentType)
{
    return NS_OK; // for now
}

NS_IMETHODIMP nsPop3Protocol::OnStopBinding(nsIURL* aURL, nsresult aStatus,
                                         const PRUnichar* aMsg)
{
    m_isRunning = PR_FALSE;
	// change the state on the url...
	if (m_nsIPop3URL)
		m_nsIPop3URL->SetUrlState(PR_FALSE, aStatus);
    return NS_OK; // for now
}

nsPop3Protocol::nsPop3Protocol(nsIURL* aURL) : nsMsgLineBuffer(NULL, FALSE)
{
	NS_INIT_REFCNT();
	Initialize(aURL);
}

void nsPop3Protocol::Initialize(nsIURL * aURL)
{
	m_username = nsnull;
    m_password = nsnull;
    m_pop3ConData = nsnull;
    m_nsIPop3URL = nsnull;
    m_transport = nsnull;
    m_outputStream = nsnull;
    m_outputConsumer = nsnull;
    m_nsIPop3Sink = nsnull;
    m_isRunning = PR_FALSE;

	m_pop3CapabilityFlags = POP3_AUTH_LOGIN_UNDEFINED |
				            POP3_XSENDER_UNDEFINED |
				            POP3_GURL_UNDEFINED |
                            POP3_UIDL_UNDEFINED |
                            POP3_TOP_UNDEFINED |
				            POP3_XTND_XLST_UNDEFINED;

	m_pop3ConData = (Pop3ConData *)PR_NEWZAP(Pop3ConData);

    PR_ASSERT(m_pop3ConData);
	m_pop3ConData->output_buffer = (char *) PR_MALLOC(OUTPUT_BUFFER_SIZE);
	PR_ASSERT(m_pop3ConData->output_buffer);
		
    m_transport = nsnull;
    m_outputStream = nsnull;
    m_outputConsumer = nsnull;
    m_isRunning = PR_FALSE;
	if (aURL)
	{
		nsresult rv = aURL->QueryInterface(nsIPop3URL::GetIID(), (void **)&m_nsIPop3URL);
		if (NS_SUCCEEDED(rv) && m_nsIPop3URL)
		{
			// extract the file name and create a file transport...
            nsINetService* pNetService;
            rv = nsServiceManager::GetService(kNetServiceCID,
                                              nsINetService::GetIID(),
                                              (nsISupports**)&pNetService);
			if (NS_SUCCEEDED(rv) && pNetService)
			{
				const char * hostName = nsnull;
				PRUint32 port = POP3_PORT;

				m_nsIPop3URL->GetHost(&hostName);
				m_nsIPop3URL->GetHostPort(&port);
				rv = pNetService->CreateSocketTransport(&m_transport, port, hostName);
				if (NS_SUCCEEDED(rv) && m_transport)
				{
					rv = m_transport->GetOutputStream(&m_outputStream);
					NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create an output stream");

					rv = m_transport->GetOutputStreamConsumer(&m_outputConsumer);
					NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create an output consumer");

					// register self as the consumer for the socket...
					rv = m_transport->SetInputStreamConsumer((nsIStreamListener *) this);
					NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register NNTP instance as a consumer on the socket");
				}
                
				(void)nsServiceManager::ReleaseService(kNetServiceCID, pNetService);
			} // if we got a netlib service
		} // if we have a runningUrl
	} // if we got a url...

	m_lineStreamBuffer = new nsMsgLineStreamBuffer(OUTPUT_BUFFER_SIZE, CRLF, PR_TRUE);
}

nsPop3Protocol::~nsPop3Protocol()
{
	if (m_pop3ConData->newuidl) PL_HashTableDestroy(m_pop3ConData->newuidl);
		net_pop3_free_state(m_pop3ConData->uidlinfo);

#if 0
		if (m_pop3ConData->graph_progress_bytes_p) {
				/* Only destroy it if we have initialized it. */
				FE_GraphProgressDestroy(ce->window_id, ce->URL_s,
																m_pop3ConData->cur_msg_size,
																ce->bytes_received);
		}
#endif 

    NS_IF_RELEASE(m_nsIPop3Sink);
    // NS_IF_RELEASE(m_transport);  // not sure who should release the transport
    NS_IF_RELEASE(m_nsIPop3URL);

	FreeMsgInfo();
	PR_FREEIF(m_pop3ConData->only_uidl);
	PR_FREEIF(m_pop3ConData->output_buffer);
	PR_FREEIF(m_pop3ConData->obuffer);
	PR_FREEIF(m_pop3ConData->command_response);
	PR_FREEIF(m_pop3ConData->sender_info);
	PR_Free(m_pop3ConData);

	if (m_lineStreamBuffer)
		delete m_lineStreamBuffer;
}

void
nsPop3Protocol::SetUsername(const char* name)
{
    PR_ASSERT(name);
    PR_FREEIF(m_username);
	if (name)
		m_username = PL_strdup(name);
}

void
nsPop3Protocol::SetPassword(const char* passwd)
{
    PR_ASSERT(passwd);
    PR_FREEIF(m_password);
	
	if (passwd)
		m_password = PL_strdup(passwd);
}


PRInt32 
nsPop3Protocol::Load(nsIURL* aURL, nsISupports * aConsumer)
{
	const char* urlSpec = NULL;
	const char* host = NULL;
	nsresult rv = 0;
    nsIPop3URL *pop3URL = NULL;

    if (aURL)
    {
        rv = aURL->QueryInterface(nsIPop3URL::GetIID(), (void **) &pop3URL);
        if (NS_SUCCEEDED(rv) && pop3URL)
        {
            // replace our old url with the new one...
            
            NS_IF_RELEASE(m_nsIPop3URL);
            
            m_nsIPop3URL = pop3URL;
            
            // okay, now fill in our event sinks...Note that each getter ref counts before
            // it returns the interface to us...we'll release when we are done
        }
        else
            NS_ASSERTION(0, "Invalid url type passed into Pop3 Protocol Handler");
    }
    else
        rv = NS_ERROR_FAILURE;

		// Time to figure out the pop3 account name and password, either from the
		// prefs file or prompt user for them
		// 
		// -*-*-*- To Do:
		// Call SetUsername(accntName);
		// Call SetPassword(aPassword);
		
		PR_ASSERT(aURL);
		rv = aURL->GetSpec(&urlSpec);
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get the url spect");

		if (PL_strcasestr(urlSpec, "?check"))
				m_pop3ConData->only_check_for_new_mail = PR_TRUE;
    else
        m_pop3ConData->only_check_for_new_mail = PR_FALSE;

		if (PL_strcasestr(urlSpec, "?gurl"))
				m_pop3ConData->get_url = PR_TRUE;
    else
        m_pop3ConData->get_url = PR_FALSE;

		// Time to set up the message store event sink
		//
		// -*-*-*- To Do:
		// ???

		if (!m_pop3ConData->only_check_for_new_mail)
		{
				// Pick up pref setting regarding leave messages on server, message size
				// limit, for now do the following
		
				m_pop3ConData->leave_on_server = PR_TRUE;
				m_pop3ConData->size_limit = 50 * 1024;
		}

		// UIDL stuff
		rv = aURL->GetHost(&host);
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get the host from the url");

    NS_IF_RELEASE(m_nsIPop3Sink);
    m_nsIPop3URL->GetPop3Sink(&m_nsIPop3Sink);

   nsIPop3IncomingServer *popServer=nsnull;
    char* mailDirectory = 0;


    m_nsIPop3Sink->GetPopServer(&popServer);
    popServer->GetRootFolderPath(&mailDirectory);
    NS_RELEASE(popServer);

    m_pop3ConData->uidlinfo = net_pop3_load_state(host, GetUsername(), 
                                                  mailDirectory);
    PR_Free(mailDirectory);

    PR_ASSERT(m_pop3ConData->uidlinfo);

	m_pop3ConData->biffstate = MSG_BIFF_NOMAIL;

	const char* uidl = PL_strcasestr(urlSpec, "?uidl=");
    PR_FREEIF(m_pop3ConData->only_uidl);
		
    if (uidl)
	{
		uidl += 6;
		m_pop3ConData->only_uidl = PL_strdup(uidl);
		PR_ASSERT(m_pop3ConData->only_uidl);
	}
	
	m_pop3ConData->next_state = POP3_READ_PASSWORD;
    m_isRunning = PR_TRUE;

    m_transport->Open(aURL);

    return ProcessPop3State(aURL, nsnull, nsnull);
}

void
nsPop3Protocol::FreeMsgInfo()
{
  int i;
  if (m_pop3ConData->msg_info) {
	for (i=0 ; i<m_pop3ConData->number_of_messages ; i++) {
	  if (m_pop3ConData->msg_info[i].uidl)
		  PR_Free(m_pop3ConData->msg_info[i].uidl);
	  m_pop3ConData->msg_info[i].uidl = NULL;
	}
	PR_Free(m_pop3ConData->msg_info);
	m_pop3ConData->msg_info = NULL;
  }
}

PRInt32
nsPop3Protocol::WaitForStartOfConnectionResponse(nsIInputStream* aInputStream,
                                                 PRUint32 length)
{
    char * line = NULL;
	PRUint32 line_length = 0;
	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(aInputStream, line_length, pauseForMoreData);

    if(line_length == 0)
    {
        /* this shouldn't really happen, but...
         */
        m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
        return(0);
    }
    else if(line_length < 0)
    {
        m_pop3ConData->pause_for_read = PR_TRUE;
        return line_length;
    }
    else if(!line)
    {
        return line_length; /* wait for line */
    }

    if(*line == '+')
	{
        m_pop3ConData->command_succeeded = PR_TRUE;
        if(PL_strlen(line) > 4)
            StrAllocCopy(m_pop3ConData->command_response, line+4);
        else
            StrAllocCopy(m_pop3ConData->command_response, line);
        
        m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
	}
    
    return(1);  /* everything ok */
}

PRInt32
nsPop3Protocol::WaitForResponse(nsIInputStream* inputStream, PRUint32 length)
{
    char * line;
    PRUint32 ln = 0;
	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(ln == 0)
    {
        /* this shouldn't really happen, but...
         */
//        m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
        m_pop3ConData->pause_for_read = PR_TRUE; /* don't pause */
        return(0);
    }
    else if(ln < 0)
    {
        m_pop3ConData->pause_for_read = PR_TRUE;
        return ln;
    }
    else if(!line)
    {
        return ln; /* wait for line */
    }

    if(*line == '+')
    {
        m_pop3ConData->command_succeeded = PR_TRUE;
        if(PL_strlen(line) > 4)
            StrAllocCopy(m_pop3ConData->command_response, line+4);
        else
            StrAllocCopy(m_pop3ConData->command_response, line);
    }
    else
	{
        m_pop3ConData->command_succeeded = PR_FALSE;
        if(PL_strlen(line) > 5)
            StrAllocCopy(m_pop3ConData->command_response, line+5);
        else
            StrAllocCopy(m_pop3ConData->command_response, line);
	}

    m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
    m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */

    return(1);  /* everything ok */
}

PRInt32
nsPop3Protocol::Error(int err_code)
{
#if 0
    ce->URL_s->error_msg = NET_ExplainErrorDetails(err_code, 
							m_pop3ConData->command_response ? m_pop3ConData->command_response :
							XP_GetString( XP_NO_ANSWER ) );
#else
#ifdef DEBUG
    printf("Pop3 Protocol Error: %d\n", err_code);
#endif 
#endif

	m_pop3ConData->next_state = POP3_ERROR_DONE;
	m_pop3ConData->pause_for_read = PR_FALSE;

	return(err_code);
}

PRInt32
nsPop3Protocol::SendCommand(const char * command)
{
    PRUint32 write_count = 0;
    nsresult rv = 0;

    PR_ASSERT(command && m_outputStream && m_outputConsumer);

    rv = m_outputStream->Write(command, PL_strlen(command), &write_count);
    if (NS_SUCCEEDED(rv) && write_count == PL_strlen(command))
    {
        nsIInputStream *inputStream = NULL;
        m_outputStream->QueryInterface(nsIInputStream::GetIID(), (void **)
                                       &inputStream);
        if (inputStream)
        {
            m_outputConsumer->OnDataAvailable(m_nsIPop3URL, inputStream,
                                              write_count);
            NS_RELEASE(inputStream);
        }
        m_pop3ConData->pause_for_read = PR_TRUE;
        m_pop3ConData->next_state = POP3_WAIT_FOR_RESPONSE;

        return rv;
    }
    else
    {
        m_pop3ConData->next_state = POP3_ERROR_DONE;
        return rv;
	  }
}

/*
 * POP3 AUTH LOGIN extention
 */

PRInt32
nsPop3Protocol::SendAuth()
{
    if(!m_pop3ConData->command_succeeded)
        return(Error(MK_POP3_SERVER_ERROR));

	nsString2 command("AUTH"CRLF, eOneByte);

    m_pop3ConData->next_state_after_response = POP3_AUTH_RESPONSE;

    return SendCommand(command.GetBuffer());
}

PRInt32
nsPop3Protocol::AuthResponse(nsIInputStream* inputStream, 
                             PRUint32 length)
{
    char * line;
    PRUint32 ln = 0;

    if (POP3_AUTH_LOGIN_UNDEFINED & m_pop3CapabilityFlags)
        m_pop3CapabilityFlags &= ~POP3_AUTH_LOGIN_UNDEFINED;
    
    if (!m_pop3ConData->command_succeeded) 
    {
        /* AUTH command not implemented 
         * no base64 encoded username/password
         */
        m_pop3ConData->command_succeeded = PR_TRUE;
        m_pop3CapabilityFlags &= ~POP3_HAS_AUTH_LOGIN;
        m_pop3ConData->next_state = POP3_SEND_USERNAME;
        return 0;
    }
    
	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(ln == 0) 
    {
        /* this shouldn't really happen, but...
         */
        m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
        return(0);
    }
    else if (ln < 0) 
    {
		/* return TCP error
		 */
        m_pop3ConData->pause_for_read = PR_TRUE; /* don't pause */
        return ln;
    } 
    else if (!line) 
        return ln; /* wait for line */

    if (!PL_strcmp(line, ".")) 
    {
        /* now that we've read all the AUTH responses, decide which 
         * state to go to next 
         */
        if (m_pop3CapabilityFlags & POP3_HAS_AUTH_LOGIN)
            m_pop3ConData->next_state = POP3_AUTH_LOGIN;
        else
            m_pop3ConData->next_state = POP3_SEND_USERNAME;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
    }
    else if (!PL_strcasecmp (line, "LOGIN")) 
        m_pop3CapabilityFlags |= POP3_HAS_AUTH_LOGIN;
    
    return 0;
}

PRInt32
nsPop3Protocol::AuthLogin()
{
    /* check login response */
    if(!m_pop3ConData->command_succeeded) 
	{
        m_pop3CapabilityFlags &= ~POP3_HAS_AUTH_LOGIN;
        return(Error(MK_POP3_SERVER_ERROR));
    }

	nsString2 command("AUTH LOGIN" CRLF, eOneByte);
    m_pop3ConData->next_state_after_response = POP3_AUTH_LOGIN_RESPONSE;

    return SendCommand(command.GetBuffer());
}

PRInt32
nsPop3Protocol::AuthLoginResponse()
{
    if (!m_pop3ConData->command_succeeded) 
    {
        /* sounds like server does not support auth-skey extension
           resume regular logon process */
        /* reset command_succeeded to true */
        m_pop3ConData->command_succeeded = PR_TRUE;
        /* reset auth login state */
        m_pop3CapabilityFlags &= ~POP3_HAS_AUTH_LOGIN;
    }
    else
    {
        m_pop3CapabilityFlags |= POP3_HAS_AUTH_LOGIN;
    }
    m_pop3ConData->next_state = POP3_SEND_USERNAME;
    return 0;
}


PRInt32
nsPop3Protocol::SendUsername()
{
    /* check login response */
    if(!m_pop3ConData->command_succeeded)
        return(Error(MK_POP3_SERVER_ERROR));

    if(!m_username || !*m_username)
        return(Error(MK_POP3_USERNAME_UNDEFINED));

    if (POP3_HAS_AUTH_LOGIN & m_pop3CapabilityFlags) 
	{
#if 0 // not yet
        char * str = 
            NET_Base64Encode(m_username, 
                             PL_strlen(m_username));
#else
        char* str = PL_strdup(m_username);
#endif

        if (str)
        {
            PR_snprintf(m_pop3ConData->output_buffer, OUTPUT_BUFFER_SIZE, 
                        "%.256s" CRLF, str);
            PR_FREEIF(str);
        }
        else
        {
            return (Error(MK_POP3_USERNAME_UNDEFINED));
        }
    }
    else 
	{
        PR_snprintf(m_pop3ConData->output_buffer, 
                    OUTPUT_BUFFER_SIZE, 
                    "USER %.256s" CRLF, m_username);
    }

    m_pop3ConData->next_state_after_response = POP3_SEND_PASSWORD;

    return(SendCommand(m_pop3ConData->output_buffer));
}

PRInt32
nsPop3Protocol::SendPassword()
{
    /* check username response */
    if (!m_pop3ConData->command_succeeded)
        return(Error(MK_POP3_USERNAME_FAILURE));
    
    if (m_password == NULL)
        return Error(MK_POP3_PASSWORD_UNDEFINED);

    if (POP3_HAS_AUTH_LOGIN & m_pop3CapabilityFlags) 
	{
#if 0 // not yet
        char * str = 
            NET_Base64Encode(m_password, PL_strlen(m_password));
#else
        char* str = PL_strdup(m_password);
#endif
        if (str)
        {
            PR_snprintf(m_pop3ConData->output_buffer, 
                        OUTPUT_BUFFER_SIZE, "%.256s" CRLF, str);
            PR_FREEIF(str);
        }
        else
        {
            return (Error(MK_POP3_PASSWORD_UNDEFINED));
        }
    }
    else 
	{
        PR_snprintf(m_pop3ConData->output_buffer, 
                    OUTPUT_BUFFER_SIZE, "PASS %.256s" CRLF, m_password);
    }

    if (m_pop3ConData->get_url)
        m_pop3ConData->next_state_after_response = POP3_SEND_GURL;
    else
        m_pop3ConData->next_state_after_response = POP3_SEND_STAT;

    return(SendCommand(m_pop3ConData->output_buffer));
}

PRInt32
nsPop3Protocol::SendStatOrGurl(PRBool sendStat)
{
    /* check password response */
    if(!m_pop3ConData->command_succeeded)
	  {
        /* The password failed.
           
           Sever the connection and go back to the `read password' state,
           which, upon success, will re-open the connection.  Set a flag
           which causes the prompt to be different that time (to indicate
           that the old password was bogus.)
           
           But if we're just checking for new mail (biff) then don't bother
           prompting the user for a password: just fail silently. */
        if (m_pop3ConData->only_check_for_new_mail)
            return MK_POP3_PASSWORD_UNDEFINED;

        m_pop3ConData->password_failed = PR_TRUE;
        m_pop3ConData->next_state = POP3_ERROR_DONE;	/* close */
        m_pop3ConData->pause_for_read = PR_FALSE;		   /* try again right away */
        m_pop3CapabilityFlags = POP3_AUTH_LOGIN_UNDEFINED | POP3_XSENDER_UNDEFINED |
            POP3_GURL_UNDEFINED;

        // libmsg event sink
        if (m_nsIPop3Sink) {
            m_nsIPop3Sink->SetUserAuthenticated(PR_FALSE);
            m_nsIPop3Sink->SetMailAccountURL(NULL);
        }

        /* clear the bogus password in case 
         * we need to sync with auth smtp password 
         */
        PR_FREEIF(m_password);
        return 0;
	  }
#if 0 // not yet
    else if (net_pop3_password_pending)
    {
        /*
         * First time with this password.  Record it as a keeper.
         * (The user's preference might say to save it.)
         */
        char* host = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);
        FE_RememberPopPassword(ce->window_id, net_pop3_password);
        if (host)
        {
            if (m_pop3ConData->pane)
            {
                char *unMungedPassword = net_get_pop3_password();
                if (unMungedPassword)
                {
                    MSG_SetPasswordForMailHost(MSG_GetMaster(m_pop3ConData->pane), 
                                               host, unMungedPassword);
                    PR_Free(unMungedPassword);
                }
            }
            PR_Free(host);
        }
        net_pop3_password_pending = PR_FALSE;
        if (m_pop3ConData->pane)
            MSG_SetUserAuthenticated(MSG_GetMaster(m_pop3ConData->pane),
                                     PR_TRUE);
    }
#else
    else 
    {
        m_nsIPop3Sink->SetUserAuthenticated(PR_TRUE);
    }
#endif 
    
    if (sendStat) {
        PL_strcpy(m_pop3ConData->output_buffer, "STAT" CRLF);
        m_pop3ConData->next_state_after_response = POP3_GET_STAT;
    }
    else {
        PL_strcpy(m_pop3ConData->output_buffer, "GURL" CRLF);
        m_pop3ConData->next_state_after_response = POP3_GURL_RESPONSE;
    }
    
    return(SendCommand(m_pop3ConData->output_buffer));
}


PRInt32
nsPop3Protocol::SendStat()
{
	return SendStatOrGurl(PR_TRUE);
}


PRInt32
nsPop3Protocol::GetStat()
{
    char *num;
    char* newStr;

    /* check stat response */
    if(!m_pop3ConData->command_succeeded)
        return(Error(MK_POP3_PASSWORD_FAILURE));

    /* stat response looks like:  %d %d
     * The first number is the number of articles
     * The second number is the number of bytes
     *
     *  grab the first and second arg of stat response
     */
    num = nsCRT::strtok(m_pop3ConData->command_response, " ", &newStr);

    m_pop3ConData->number_of_messages = atol(num);

    num = nsCRT::strtok(newStr, " ", &newStr);

    m_pop3ConData->total_folder_size = (PRInt32) atol(num);
    m_pop3ConData->really_new_messages = 0;
    m_pop3ConData->real_new_counter = 1;

    m_pop3ConData->total_download_size = -1; /* Means we need to calculate it, later. */

    if(m_pop3ConData->number_of_messages <= 0) {
        /* We're all done.  We know we have no mail. */
        m_pop3ConData->next_state = POP3_SEND_QUIT;
        PL_HashTableEnumerateEntries(m_pop3ConData->uidlinfo->hash, hash_clear_mapper, nsnull);
        return(0);
    }

    if (m_pop3ConData->only_check_for_new_mail && !m_pop3ConData->leave_on_server &&
        m_pop3ConData->size_limit < 0) {
        /* We're just checking for new mail, and we're not playing any games that
           involve keeping messages on the server.  Therefore, we now know enough
           to finish up.  If we had no messages, that would have been handled
           above; therefore, we know we have some new messages. */
        m_pop3ConData->biffstate = MSG_BIFF_NEWMAIL;
        m_pop3ConData->next_state = POP3_SEND_QUIT;
        return(0);
    }


    if (!m_pop3ConData->only_check_for_new_mail) {
        m_nsIPop3Sink->BeginMailDelivery(&m_pop3ConData->msg_del_started);

        if(!m_pop3ConData->msg_del_started)
        {
            return(Error(MK_POP3_MESSAGE_WRITE_ERROR));
        }
    }

/*
#ifdef POP_ALWAYS_USE_UIDL_FOR_DUPLICATES
	if (net_uidl_command_unimplemented && net_xtnd_xlst_command_unimplemented && net_top_command_unimplemented) 
#else
	if ((net_uidl_command_unimplemented && net_xtnd_xlst_command_unimplemented && net_top_command_unimplemented) ||
		(!m_pop3ConData->only_uidl && !m_pop3ConData->leave_on_server &&
		 (m_pop3ConData->size_limit < 0 || net_top_command_unimplemented)))
#endif */
		 /* We don't need message size or uidl info; go directly to getting
		 messages. */
	/*  m_pop3ConData->next_state = POP3_GET_MSG;
	else */  /* Fix bug 51262 where pop messages kept on server are re-downloaded after unchecking keep on server */
	
    m_pop3ConData->next_state = POP3_SEND_LIST;
    return 0;
}



PRInt32
nsPop3Protocol::SendGurl()
{
    if (m_pop3CapabilityFlags == POP3_CAPABILITY_UNDEFINED ||
        m_pop3CapabilityFlags & POP3_GURL_UNDEFINED ||
        m_pop3CapabilityFlags & POP3_HAS_GURL)
        return SendStatOrGurl(PR_FALSE);
    else 
        return -1;
}


PRInt32
nsPop3Protocol::GurlResponse()
{
    if (POP3_GURL_UNDEFINED & m_pop3CapabilityFlags)
        m_pop3CapabilityFlags &= ~POP3_GURL_UNDEFINED;
    
    if (m_pop3ConData->command_succeeded) {
        m_pop3CapabilityFlags |= POP3_HAS_GURL;
        if (m_nsIPop3Sink)
            m_nsIPop3Sink->SetMailAccountURL(m_pop3ConData->command_response);
    }
    else {
        m_pop3CapabilityFlags &= ~POP3_HAS_GURL;
    }
    m_pop3ConData->next_state = POP3_SEND_QUIT;
    return 0;
}

PRInt32
nsPop3Protocol::SendList()
{
    m_pop3ConData->msg_info = (Pop3MsgInfo *) 
        PR_CALLOC(sizeof(Pop3MsgInfo) *	m_pop3ConData->number_of_messages);
    if (!m_pop3ConData->msg_info)
        return(MK_OUT_OF_MEMORY);

    PL_strcpy(m_pop3ConData->output_buffer, "LIST" CRLF);
    m_pop3ConData->next_state_after_response = POP3_GET_LIST;
    return(SendCommand(m_pop3ConData->output_buffer));
}



PRInt32
nsPop3Protocol::GetList(nsIInputStream* inputStream, 
                        PRUint32 length)
{
    char * line, *token, *newStr;
    PRUint32 ln = 0;
    PRInt32 msg_num;

    /* check list response 
     * This will get called multiple times
     * but it's alright since command_succeeded
     * will remain constant
     */
    if(!m_pop3ConData->command_succeeded)
        return(Error(MK_POP3_LIST_FAILURE));

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(ln== 0)
    {
        /* this shouldn't really happen, but...
         */
        m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
        return(0);
    }
    else if(ln < 0)
    {
        m_pop3ConData->pause_for_read = PR_TRUE;
        return ln;
    }
    else if(!line)
    {
        return ln;
    }
    
    /* parse the line returned from the list command 
     * it looks like
     * #msg_number #bytes
     *
     * list data is terminated by a ".CRLF" line
     */
    if(!PL_strcmp(line, "."))
	  {
        m_pop3ConData->next_state = POP3_SEND_UIDL_LIST;
        m_pop3ConData->pause_for_read = PR_FALSE;
        return(0);
	  }
    
	token = nsCRT::strtok(line, " ", &newStr);
	if (token)
	{
		msg_num = atol(token);

		if(msg_num <= m_pop3ConData->number_of_messages && msg_num > 0)
		{
			token = nsCRT::strtok(newStr, " ", &newStr);
			if (token)
				m_pop3ConData->msg_info[msg_num-1].size = atol(token);
		}
	}

    return(0);
}

PRInt32
nsPop3Protocol::SendFakeUidlTop()
{
    PR_snprintf(m_pop3ConData->output_buffer, 
                OUTPUT_BUFFER_SIZE,  
                "TOP %ld 1" CRLF,
                m_pop3ConData->current_msg_to_top);

    m_pop3ConData->next_state_after_response = POP3_GET_FAKE_UIDL_TOP;
    m_pop3ConData->pause_for_read = PR_TRUE;
    return(SendCommand(m_pop3ConData->output_buffer));
}

PRInt32
nsPop3Protocol::StartUseTopForFakeUidl()
{
    m_pop3ConData->current_msg_to_top = m_pop3ConData->number_of_messages;
    m_pop3ConData->number_of_messages_not_seen_before = 0;
    m_pop3ConData->found_new_message_boundary = PR_FALSE;
    m_pop3ConData->delete_server_message_during_top_traversal = PR_FALSE;
	
    /* may set delete_server_message_during_top_traversal to true */
    PL_HashTableEnumerateEntries(m_pop3ConData->uidlinfo->hash, 
    								net_pop3_check_for_hash_messages_marked_delete,
    								(void *)m_pop3ConData);
	
    return (SendFakeUidlTop());
}


PRInt32
nsPop3Protocol::GetFakeUidlTop(nsIInputStream* inputStream, 
                               PRUint32 length)
{
    char * line, *newStr;
    PRUint32 ln = 0;

    /* check list response 
     * This will get called multiple times
     * but it's alright since command_succeeded
     * will remain constant
     */
    if(!m_pop3ConData->command_succeeded) {
        
        /* UIDL, XTND and TOP are all unsupported for this mail server.
           Tell the user to join the 20th century.
           
           Tell the user this, and refuse to download any messages until they've
           gone into preferences and turned off the `Keep Mail on Server' and
           `Maximum Message Size' prefs.  Some people really get their panties
           in a bunch if we download their mail anyway. (bug 11561)
           */
#if 0
        char *fmt = XP_GetString(XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC);
        char *host = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);
        ce->URL_s->error_msg = PR_smprintf (fmt, (host ? host : "(null)"));
        PR_FREEIF(host);
#endif 
        m_pop3ConData->next_state = POP3_ERROR_DONE;
        m_pop3ConData->pause_for_read = PR_FALSE;
        return -1;
        
    }

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(ln == 0)
    {
        /* this shouldn't really happen, but...
         */
        m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
        return 0;
    }
    else if(ln < 0)
    {
        m_pop3ConData->pause_for_read = PR_TRUE;
        return ln;
    }
    else if(!line)
    {
        return ln; /* wait for line */
    }

    if(!PL_strcmp(line, "."))
    {
        m_pop3ConData->current_msg_to_top--;
        if (!m_pop3ConData->current_msg_to_top || 
            (m_pop3ConData->found_new_message_boundary &&
             !m_pop3ConData->delete_server_message_during_top_traversal))
        {
            /* we either ran out of messages or reached the edge of new
               messages and no messages are marked dele */
            m_pop3ConData->next_state = POP3_GET_MSG;
            m_pop3ConData->pause_for_read = PR_FALSE;
            
            /* if all of the messages are new, toss all hash table entries */
            if (!m_pop3ConData->current_msg_to_top &&
                !m_pop3ConData->found_new_message_boundary)
                PL_HashTableEnumerateEntries(m_pop3ConData->uidlinfo->hash, hash_clear_mapper, nsnull);
        }
        else
        {
            /* this message is done, go to the next */
            m_pop3ConData->next_state = POP3_SEND_FAKE_UIDL_TOP;
            m_pop3ConData->pause_for_read = PR_FALSE;
        }
    }
    else
    {
        /* we are looking for a string of the form
		   Message-Id: <199602071806.KAA14787@neon.netscape.com> */
        char *firstToken = nsCRT::strtok(line, " ", &newStr);
        int state = 0;
        
        if (firstToken && !PL_strcasecmp(firstToken, "MESSAGE-ID:") )
        {
            char *message_id_token = nsCRT::strtok(newStr, " ", &newStr);
            state = (int)PL_HashTableLookup(m_pop3ConData->uidlinfo->hash, message_id_token);
            
            if (!m_pop3ConData->only_uidl && message_id_token && (state == 0))
            {	/* we have not seen this message before */
                
                /* if we are only doing a biff, stop here */
                if (m_pop3ConData->only_check_for_new_mail)
                {
                    m_pop3ConData->biffstate = MSG_BIFF_NEWMAIL;
                    m_pop3ConData->next_state = POP3_SEND_QUIT;
                    m_pop3ConData->pause_for_read = PR_FALSE;
                }
                else	/* we will retrieve it and cache it in GET_MSG */
                {
                    m_pop3ConData->number_of_messages_not_seen_before++;
                    m_pop3ConData->msg_info[m_pop3ConData->current_msg_to_top-1].uidl = 
                        PL_strdup(message_id_token);
                    if (!m_pop3ConData->msg_info[m_pop3ConData->current_msg_to_top-1].uidl)
                        return MK_OUT_OF_MEMORY;
                }
            }
            else if (m_pop3ConData->only_uidl && message_id_token &&
                     !PL_strcmp(m_pop3ConData->only_uidl, message_id_token))
            {
                m_pop3ConData->last_accessed_msg = m_pop3ConData->current_msg_to_top - 1;
                m_pop3ConData->found_new_message_boundary = PR_TRUE;
                m_pop3ConData->msg_info[m_pop3ConData->current_msg_to_top-1].uidl =
                    PL_strdup(message_id_token);
                if (!m_pop3ConData->msg_info[m_pop3ConData->current_msg_to_top-1].uidl)
                    return MK_OUT_OF_MEMORY;
            }
            else if (!m_pop3ConData->only_uidl)
            {	/* we have seen this message and we care about the edge,
                 stop looking for new ones */
                if (m_pop3ConData->number_of_messages_not_seen_before != 0)
                {
                    m_pop3ConData->last_accessed_msg =
                        m_pop3ConData->current_msg_to_top;	/* -1 ? */
                    m_pop3ConData->found_new_message_boundary = PR_TRUE;
                    /* we stay in this state so we can process the rest of the
                       lines in the top message */
                }
                else
                {
                    m_pop3ConData->next_state = POP3_SEND_QUIT;
                    m_pop3ConData->pause_for_read = PR_FALSE;
                }
            }
        }
    }
    return 0;
}


/* km
 *
 *	net_pop3_send_xtnd_xlst_msgid
 *
 *  Process state: POP3_SEND_XTND_XLST_MSGID
 *
 *	If we get here then UIDL is not supported by the mail server.
 *  Some mail servers support a similar command:
 *
 *		XTND XLST Message-Id
 *
 * 	Here is a sample transaction from a QUALCOMM server
 
 >>XTND XLST Message-Id
 <<+OK xlst command accepted; headers coming.
 <<1 Message-ID: <3117E4DC.2699@netscape.com>
 <<2 Message-Id: <199602062335.PAA19215@lemon.mcom.com>
 
 * This function will send the xtnd command and put us into the
 * POP3_GET_XTND_XLST_MSGID state
 *
*/
PRInt32
nsPop3Protocol::SendXtndXlstMsgid()
{
    if (!(m_pop3CapabilityFlags & POP3_HAS_XTND_XLST))
        return StartUseTopForFakeUidl();
    PL_strcpy(m_pop3ConData->output_buffer, "XTND XLST Message-Id" CRLF);
    m_pop3ConData->next_state_after_response = POP3_GET_XTND_XLST_MSGID;
    m_pop3ConData->pause_for_read = PR_TRUE;
    return(SendCommand(m_pop3ConData->output_buffer));
}


/* km
 *
 *	net_pop3_get_xtnd_xlst_msgid
 *
 *  This code was created from the net_pop3_get_uidl_list boiler plate.
 *	The difference is that the XTND reply strings have one more token per
 *  string than the UIDL reply strings do.
 *
 */

PRInt32
nsPop3Protocol::GetXtndXlstMsgid(nsIInputStream* inputStream, 
                                 PRUint32 length)
{
    char * line, *newStr;
    PRUint32 ln = 0;
    PRInt32 msg_num;

    /* check list response 
     * This will get called multiple times
     * but it's alright since command_succeeded
     * will remain constant
     */
    if(!m_pop3ConData->command_succeeded) {
        m_pop3CapabilityFlags &= ~POP3_XTND_XLST_UNDEFINED;
        m_pop3CapabilityFlags &= ~POP3_HAS_XTND_XLST;
        m_pop3ConData->next_state = POP3_START_USE_TOP_FOR_FAKE_UIDL;
        m_pop3ConData->pause_for_read = PR_FALSE;
        return(0);
    }

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(ln == 0)
    {
        /* this shouldn't really happen, but...
         */
        m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
        return 0;
    }
    else if(ln < 0)
    {
        m_pop3ConData->pause_for_read = PR_TRUE;
        return ln;
    }
    else if(!line)
    {
        return ln; /* wait for line */
    }

    /* parse the line returned from the list command 
     * it looks like
     * 1 Message-ID: <3117E4DC.2699@netscape.com>
     *
     * list data is terminated by a ".CRLF" line
     */
    if(!PL_strcmp(line, "."))
	  {
        m_pop3ConData->next_state = POP3_GET_MSG;
        m_pop3ConData->pause_for_read = PR_FALSE;
        return(0);
	  }
    
    msg_num = atol(nsCRT::strtok(line, " ", &newStr));

    if(msg_num <= m_pop3ConData->number_of_messages && msg_num > 0) {
/*	  char *eatMessageIdToken = nsCRT::strtok(newStr, " ", &newStr);	*/
        char *uidl = nsCRT::strtok(newStr, " ", &newStr);/* not really a uidl but a unique token -km */

        if (!uidl)
            /* This is bad.  The server didn't give us a UIDL for this message.
               I've seen this happen when somehow the mail spool has a message
               that contains a header that reads "X-UIDL: \n".  But how that got
               there, I have no idea; must be a server bug.  Or something. */
            uidl = "";
        
        m_pop3ConData->msg_info[msg_num-1].uidl = PL_strdup(uidl);
        if (!m_pop3ConData->msg_info[msg_num-1].uidl)
            return MK_OUT_OF_MEMORY;
    }
    
    return(0);
}


PRInt32
nsPop3Protocol::SendUidlList()
{
    if (!(m_pop3CapabilityFlags & POP3_HAS_XTND_XLST))
        return SendXtndXlstMsgid();

    PL_strcpy(m_pop3ConData->output_buffer, "UIDL" CRLF);
    m_pop3ConData->next_state_after_response = POP3_GET_UIDL_LIST;
    m_pop3ConData->pause_for_read = PR_TRUE;
    return(SendCommand(m_pop3ConData->output_buffer));
}


PRInt32
nsPop3Protocol::GetUidlList(nsIInputStream* inputStream, 
                            PRUint32 length)
{
    char * line, *newStr;
    PRUint32 ln;
    PRInt32 msg_num;

    /* check list response 
     * This will get called multiple times
     * but it's alright since command_succeeded
     * will remain constant
     */
    if(!m_pop3ConData->command_succeeded) {
        m_pop3ConData->next_state = POP3_SEND_XTND_XLST_MSGID;
        m_pop3ConData->pause_for_read = PR_FALSE;
        m_pop3CapabilityFlags &= ~POP3_HAS_UIDL;
        return(0);
        
#if 0  /* this if block shows what UIDL used to do in this case */
        /* UIDL doesn't work so we can't retrieve the message later, and we
         * can't play games notating how to keep it on the server.  Therefore
         * just go download the whole thing, and warn the user.
         */
        char *host, *fmt, *buf;
        
        net_uidl_command_unimplemented = PR_TRUE;
        
        fmt = XP_GetString( XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC );
        
        host = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);
        PR_ASSERT(host);
        if (!host)
            host = "(null)";
        buf = PR_smprintf (fmt, host);
        if (!buf)
            return MK_OUT_OF_MEMORY;
        FE_Alert (ce->window_id, buf);
        PR_Free (buf);
        
        /* Free up the msg_info structure, as we use its presence later to
           decide if we can do UIDL-based games. */
        net_pop3_free_msg_info(ce);
        
        m_pop3ConData->next_state = POP3_GET_MSG;
        return(0);
        
#endif /* 0 */
    }
    
    PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(ln == 0)
    {
        /* this shouldn't really happen, but...
         */
        m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
        return 0;
    }
    else if(ln < 0)
    {
        m_pop3ConData->pause_for_read = PR_TRUE;
        return ln;
    }
    else if(!line)
    {
        return ln; /* wait for line */
    }

    /* parse the line returned from the list command 
     * it looks like
     * #msg_number uidl
     *
     * list data is terminated by a ".CRLF" line
     */
    if(!PL_strcmp(line, "."))
	{
        m_pop3ConData->next_state = POP3_GET_MSG;
        m_pop3ConData->pause_for_read = PR_FALSE;
        return(0);
	}

    msg_num = atol(nsCRT::strtok(line, " ", &newStr));
    
    if(msg_num <= m_pop3ConData->number_of_messages && msg_num > 0) 
	{
        char *uidl = nsCRT::strtok(newStr, " ", &newStr);

        if (!uidl)
            /* This is bad.  The server didn't give us a UIDL for this message.
               I've seen this happen when somehow the mail spool has a message
               that contains a header that reads "X-UIDL: \n".  But how that got
               there, I have no idea; must be a server bug.  Or something. */
            uidl = "";

        m_pop3ConData->msg_info[msg_num-1].uidl = PL_strdup(uidl);
        if (!m_pop3ConData->msg_info[msg_num-1].uidl)
            return MK_OUT_OF_MEMORY;
    }
    
    return(0);
}




/* this function decides if we are going to do a
 * normal RETR or a TOP.  The first time, it also decides the total number
 * of bytes we're probably going to get.
 */
PRInt32 
nsPop3Protocol::GetMsg()
{
    char c;
    int i;
    PRBool prefBool = PR_FALSE;

    if(m_pop3ConData->last_accessed_msg >= m_pop3ConData->number_of_messages) {
        /* Oh, gee, we're all done. */
        m_pop3ConData->next_state = POP3_SEND_QUIT;
        return 0;
    }

    if (m_pop3ConData->total_download_size < 0) {
        /* First time.  Figure out how many bytes we're about to get.
           If we didn't get any message info, then we are going to get
           everything, and it's easy.  Otherwise, if we only want one
           uidl, than that's the only one we'll get.  Otherwise, go
           through each message info, decide if we're going to get that
           message, and add the number of bytes for it. When a message is too
           large (per user's preferences) only add the size we are supposed
           to get. */
        m_pop3ConData->really_new_messages = 0;
        m_pop3ConData->real_new_counter = 1;
        if (m_pop3ConData->msg_info) {
            m_pop3ConData->total_download_size = 0;
            for (i=0 ; i < m_pop3ConData->number_of_messages ; i++) {
                c = 0;
                if (m_pop3ConData->only_uidl) {
                    if (m_pop3ConData->msg_info[i].uidl &&
                        PL_strcmp(m_pop3ConData->msg_info[i].uidl, 
                                  m_pop3ConData->only_uidl) == 0) {
			  /*if (m_pop3ConData->msg_info[i].size > m_pop3ConData->size_limit)
				  m_pop3ConData->total_download_size = m_pop3ConData->size_limit;	*/	/* if more than max, only count max */
			  /*else*/
                        m_pop3ConData->total_download_size =
                            m_pop3ConData->msg_info[i].size;
                        m_pop3ConData->really_new_messages = 1;		/* we are
                                                                   * only
                                                                   * getting
                                                                   * one
                                                                   * message
                                                                   * */ 
                        m_pop3ConData->real_new_counter = 1;
                        break;
                    }
                    continue;
                }
                if (m_pop3ConData->msg_info[i].uidl)
                    c = (char) (int) PL_HashTableLookup(m_pop3ConData->uidlinfo->hash,
                                                m_pop3ConData->msg_info[i].uidl);
                if ((c == KEEP) && !m_pop3ConData->leave_on_server)
                {		/* This message has been downloaded but kept on server, we
                     * no longer want to keep it there */ 
                    if (m_pop3ConData->newuidl == NULL)
                    {
                        m_pop3ConData->newuidl = PL_NewHashTable(20,
                                                                 PL_HashString, 
                                                                 PL_CompareStrings,
                                                                 PL_CompareValues,
                                                                 nsnull,
                                                                 nsnull);
                        if (!m_pop3ConData->newuidl)
                            return MK_OUT_OF_MEMORY;
                    }
                    c = DELETE_CHAR;
                    put_hash(m_pop3ConData->uidlinfo, m_pop3ConData->newuidl,
                             m_pop3ConData->msg_info[i].uidl, DELETE_CHAR);
                    /*Mark message to be deleted in new table */ 
                    put_hash(m_pop3ConData->uidlinfo,
                             m_pop3ConData->uidlinfo->hash,
                             m_pop3ConData->msg_info[i].uidl, DELETE_CHAR);
                    /*and old one too */ 
                }
                if ((c != KEEP) && (c != DELETE_CHAR) && (c != TOO_BIG)) 
                {	/* mesage left on server */
                    /*if (m_pop3ConData->msg_info[i].size > m_pop3ConData->size_limit)
                      m_pop3ConData->total_download_size +=
                      m_pop3ConData->size_limit;	*/	
                    /* if more than max, only count max */
                    /*else*/
                    m_pop3ConData->total_download_size +=
                        m_pop3ConData->msg_info[i].size; 
                    m_pop3ConData->really_new_messages++;		
                    /* a message we will really download */
                }
            }
        } else {
            m_pop3ConData->total_download_size = m_pop3ConData->total_folder_size;
        }
        if (m_pop3ConData->only_check_for_new_mail) {
            if (m_pop3ConData->total_download_size > 0)
                m_pop3ConData->biffstate = MSG_BIFF_NEWMAIL; 
            m_pop3ConData->next_state = POP3_SEND_QUIT;
            return(0);
        }
        /* get the amount of available space on the drive
         * and make sure there is enough
         */	
#if 0 // not yet
        {
            const char* dir = MSG_GetFolderDirectory(MSG_GetPrefs(m_pop3ConData->pane));
            
            /* When checking for disk space available, take in consideration
             * possible database 
             * changes, therefore ask for a little more than what the message
             * size is. Also, due to disk sector sizes, allocation blocks,
             * etc. The space "available" may be greater than the actual space
             * usable. */ 
            if((m_pop3ConData->total_download_size > 0)
               && ((PRUint32)m_pop3ConData->total_download_size + (PRUint32)
                   3096) > FE_DiskSpaceAvailable(ce->window_id, dir))
            {
                return(Error(MK_POP3_OUT_OF_DISK_SPACE));
            }
            
        }
#endif 
    }
    
    
    /* Look at this message, and decide whether to ignore it, get it, just get
       the TOP of it, or delete it. */
    
#if 0 // not yet
    PREF_GetBoolPref("mail.auth_login", &prefBool);
#endif 
    if (prefBool && (m_pop3CapabilityFlags & POP3_HAS_XSENDER ||
                     m_pop3CapabilityFlags & POP3_XSENDER_UNDEFINED))
        m_pop3ConData->next_state = POP3_SEND_XSENDER;
    else
        m_pop3ConData->next_state = POP3_SEND_RETR;
    m_pop3ConData->truncating_cur_msg = PR_FALSE;
    m_pop3ConData->pause_for_read = PR_FALSE;
    if (m_pop3ConData->msg_info) {
        Pop3MsgInfo* info = m_pop3ConData->msg_info + m_pop3ConData->last_accessed_msg;
        if (m_pop3ConData->only_uidl) {
            if (info->uidl == NULL || PL_strcmp(info->uidl, m_pop3ConData->only_uidl))
                m_pop3ConData->next_state = POP3_GET_MSG;
            else
                m_pop3ConData->next_state = POP3_SEND_RETR;
        } else {
            c = 0;
            if (m_pop3ConData->newuidl == NULL) {
                m_pop3ConData->newuidl = PL_NewHashTable(20, PL_HashString, PL_CompareStrings, PL_CompareValues, nsnull, nsnull);
                if (!m_pop3ConData->newuidl)
                    return MK_OUT_OF_MEMORY;
            }
            if (info->uidl) {
                c = (char) (int) PL_HashTableLookup(m_pop3ConData->uidlinfo->hash,
                                            info->uidl);
            }
            m_pop3ConData->truncating_cur_msg = PR_FALSE;
            if (c == DELETE_CHAR) {
                m_pop3ConData->next_state = POP3_SEND_DELE;
            } else if (c == KEEP) {
                m_pop3ConData->next_state = POP3_GET_MSG;
            } else if ((c != TOO_BIG) && (m_pop3ConData->size_limit > 0) &&
                       (info->size > m_pop3ConData->size_limit) && 
                       !(m_pop3CapabilityFlags & POP3_HAS_TOP) &&
                       (m_pop3ConData->only_uidl == NULL)) { 
                /* message is too big */
                m_pop3ConData->truncating_cur_msg = PR_TRUE;
                m_pop3ConData->next_state = POP3_SEND_TOP;
                put_hash(m_pop3ConData->uidlinfo, m_pop3ConData->newuidl,
                         info->uidl, TOO_BIG); 
            } else if (c == TOO_BIG) {	/* message previously left on server,
                                           see if the max download size has
                                           changed, because we may want to
                                           download the message this time
                                           around. Otherwise ignore the
                                           message, we have the header. */
                if ((m_pop3ConData->size_limit > 0) && (info->size <=
                                                        m_pop3ConData->size_limit)) 
                    PL_HashTableRemove (m_pop3ConData->uidlinfo->hash, (void*)
                                info->uidl);
                /* remove from our table, and download */
                else
                {
                    m_pop3ConData->truncating_cur_msg = PR_TRUE;
                    m_pop3ConData->next_state = POP3_GET_MSG;
                    /* ignore this message and get next one */
                    put_hash(m_pop3ConData->uidlinfo, m_pop3ConData->newuidl,
                             info->uidl, TOO_BIG);
                }
            }
        }
        if ((m_pop3ConData->leave_on_server && m_pop3ConData->next_state !=
             POP3_SEND_DELE) || 
            m_pop3ConData->next_state == POP3_GET_MSG ||
            m_pop3ConData->next_state == POP3_SEND_TOP) { 

            /* This is a message we have decided to keep on the server.  Notate
               that now for the future.  (Don't change the popstate file at all
               if only_uidl is set; in that case, there might be brand new messages
               on the server that we *don't* want to mark KEEP; we just want to
               leave them around until the user next does a GetNewMail.) */
            
            if (info->uidl && m_pop3ConData->only_uidl == NULL) {
                if (!m_pop3ConData->truncating_cur_msg)	
                    /* message already marked as too_big */
                    put_hash(m_pop3ConData->uidlinfo, m_pop3ConData->newuidl,
                             info->uidl, KEEP); 
            }
        }
        if (m_pop3ConData->next_state == POP3_GET_MSG) {
            m_pop3ConData->last_accessed_msg++; 
            /* Make sure we check the next message next
									time! */
        }
    }
    return 0;
}


/* start retreiving just the first 20 lines
 */
PRInt32
nsPop3Protocol::SendTop()
{
    PR_ASSERT(!(m_pop3CapabilityFlags & (POP3_HAS_UIDL | POP3_HAS_XTND_XLST |
                                         POP3_HAS_TOP)));

    PR_snprintf(m_pop3ConData->output_buffer, 
                OUTPUT_BUFFER_SIZE,  
                "TOP %ld 20" CRLF,
                m_pop3ConData->last_accessed_msg+1);
    
    m_pop3ConData->next_state_after_response = POP3_TOP_RESPONSE;
    
    m_pop3ConData->cur_msg_size = -1;

    /* zero the bytes received in message in preparation for
     * the next
     */
    m_pop3ConData->bytes_received_in_message = 0;
    
    return(SendCommand(m_pop3ConData->output_buffer));
}

/* send the xsender command
 */
PRInt32
nsPop3Protocol::SendXsender()
{
    PR_snprintf(m_pop3ConData->output_buffer,
                OUTPUT_BUFFER_SIZE,
                "XSENDER %ld" CRLF,
                m_pop3ConData->last_accessed_msg+1);
    
    m_pop3ConData->next_state_after_response = POP3_XSENDER_RESPONSE;
    return SendCommand(m_pop3ConData->output_buffer);
}

PRInt32
nsPop3Protocol::XsenderResponse()
{
    m_pop3ConData->seenFromHeader = PR_FALSE;
    PR_FREEIF(m_pop3ConData->sender_info);
    
    if (POP3_XSENDER_UNDEFINED & m_pop3CapabilityFlags)
        m_pop3CapabilityFlags &= ~POP3_XSENDER_UNDEFINED;

    if (m_pop3ConData->command_succeeded) {
        if (PL_strlen (m_pop3ConData->command_response) > 4)
        {
            StrAllocCopy(m_pop3ConData->sender_info, m_pop3ConData->command_response);
        }
        if (! (POP3_HAS_XSENDER & m_pop3CapabilityFlags))
            m_pop3CapabilityFlags |= POP3_HAS_XSENDER;
    }
    else {
        m_pop3CapabilityFlags &= ~POP3_HAS_XSENDER;
    }
    if (m_pop3ConData->truncating_cur_msg)
        m_pop3ConData->next_state = POP3_SEND_TOP;
    else
        m_pop3ConData->next_state = POP3_SEND_RETR;
    return 0;
}

/* retreive the whole message
 */
PRInt32
nsPop3Protocol::SendRetr()
{
   
    PR_snprintf(m_pop3ConData->output_buffer, 
                OUTPUT_BUFFER_SIZE,  
                "RETR %ld" CRLF,
                m_pop3ConData->last_accessed_msg+1);

    m_pop3ConData->next_state_after_response = POP3_RETR_RESPONSE;
    
    m_pop3ConData->cur_msg_size = -1;

    /* zero the bytes received in message in preparation for
     * the next
     */
    m_pop3ConData->bytes_received_in_message = 0;
    
    if (m_pop3ConData->only_uidl)
	  {
        /* Display bytes if we're only downloading one message. */
        PR_ASSERT(!m_pop3ConData->graph_progress_bytes_p);
#if 0
        if (!m_pop3ConData->graph_progress_bytes_p)
            FE_GraphProgressInit(ce->window_id, ce->URL_s,
                                 m_pop3ConData->total_download_size);
#endif 
        m_pop3ConData->graph_progress_bytes_p = PR_TRUE;
	  }
    else
	  {
#if 0
        PR_snprintf(buf, OUTPUT_BUFFER_SIZE,
                    XP_GetString( XP_RECEIVING_MESSAGE_OF ),
                    /* m_pop3ConData->last_accessed_msg+1,
                       m_pop3ConData->number_of_messages); */ 
                    m_pop3ConData->real_new_counter,
                    m_pop3ConData->really_new_messages);	/* (rb) real
                                                             counts... */ 
        NET_Progress(ce->window_id, buf);
#endif 
	  }
    
    return(SendCommand(m_pop3ConData->output_buffer));
}


static PRInt32 gPOP3parsed_bytes, gPOP3size;
static PRBool gPOP3dotFix, gPOP3AssumedEnd;


/*
	To fix a bug where we think the message is over, check the alleged size of the message
	before we assume that CRLF.CRLF is the end.
	return PR_TRUE if end of message is unlikely. parsed bytes is not accurate since we add
	bytes every now and then.
*/

PRBool NET_POP3TooEarlyForEnd(PRInt32 len)
{
	if (!gPOP3dotFix)
		return PR_FALSE;	/* fix turned off */
	gPOP3parsed_bytes += len;
	if (gPOP3parsed_bytes >= (gPOP3size - 3))
		return PR_FALSE;
	return PR_TRUE;
}



/* digest the message
 */
PRInt32
nsPop3Protocol::RetrResponse(nsIInputStream* inputStream, 
                             PRUint32 length)
{
    char *buffer;
    PRUint32 buffer_size;
    PRInt32 flags = 0;
    char *uidl = NULL;
    char *newStr;
#if 0
    PRInt32 old_bytes_received = ce->bytes_received;
#endif 
    PRBool fix = PR_FALSE;
    PRUint32 status = 0;
	
    if(m_pop3ConData->cur_msg_size == -1)
    {
        /* this is the beginning of a message
         * get the response code and byte size
         */
        if(!m_pop3ConData->command_succeeded)
            return Error(MK_POP3_RETR_FAILURE);
        
        /* a successful RETR response looks like: #num_bytes Junk
           from TOP we only get the +OK and data
           */
        if (m_pop3ConData->truncating_cur_msg)
        { /* TOP, truncated message */
            m_pop3ConData->cur_msg_size = m_pop3ConData->size_limit;
            flags |= MSG_FLAG_PARTIAL;
        }
        else
            m_pop3ConData->cur_msg_size =
                atol(nsCRT::strtok(m_pop3ConData->command_response, " ", &newStr));
        /* RETR complete message */

        if (m_pop3ConData->sender_info)
            flags |= MSG_FLAG_SENDER_AUTHED;
        
        if(m_pop3ConData->cur_msg_size < 0)
            m_pop3ConData->cur_msg_size = 0;

        if (m_pop3ConData->msg_info && 
            m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].uidl)
            uidl = m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].uidl;

        gPOP3parsed_bytes = 0;
        gPOP3size = m_pop3ConData->cur_msg_size;
        gPOP3AssumedEnd = PR_FALSE;
#if 0
        PREF_GetBoolPref("mail.dot_fix", &fix);
#endif 
        gPOP3dotFix = fix;
		
#if 0
        TRACEMSG(("Opening message stream: MSG_IncorporateBegin"));
#endif 
        /* open the message stream so we have someplace
         * to put the data
         */
        m_pop3ConData->real_new_counter++;		
        /* (rb) count only real messages being downloaded */

        m_nsIPop3Sink->IncorporateBegin(uidl, m_nsIPop3URL, flags,
                                        &m_pop3ConData->msg_closure);  

#if 0
        TRACEMSG(("Done opening message stream!"));
#endif
													
        if(!m_pop3ConData->msg_closure)
            return(Error(MK_POP3_MESSAGE_WRITE_ERROR));
    }
    
    m_pop3ConData->pause_for_read = PR_TRUE;

	PRBool pauseForMoreData = PR_FALSE;
	char * line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);
	buffer_size = status;
    
    if(status == 0)  // no bytes read in...
    {
        if (gPOP3dotFix && gPOP3AssumedEnd)
        {
            status =
                m_nsIPop3Sink->IncorporateComplete(m_pop3ConData->msg_closure);
            m_pop3ConData->msg_closure = 0;
            buffer_size = 0;
        }
        else
        {
            m_pop3ConData->pause_for_read = PR_TRUE;
            return (0);
        }
    }
    
    if (m_pop3ConData->msg_closure)	/* not done yet */
    {
        if (!m_pop3ConData->obuffer)
        {
            m_pop3ConData->obuffer = (char *) PR_CALLOC(1024);
            PR_ASSERT(m_pop3ConData->obuffer);
            m_pop3ConData->obuffer_size = 1024;
            m_pop3ConData->obuffer_fp = 0;
        }

		// buffer the line we just read in, and buffer all remaining lines in the stream
		status = buffer_size;
		do
		{
			BufferInput(line, buffer_size);
			BufferInput(CRLF, 2);

			// now read in the next line
			line = m_lineStreamBuffer->ReadNextLine(inputStream, buffer_size, pauseForMoreData);
			status += buffer_size;
		} while (/* !pauseForMoreData && */ line);

		PRUint32 size = 0;
		inputStream->GetLength(&size);
		NS_ASSERTION(size == 0, "hmmm....");
    }

	buffer_size = status;  // status holds # bytes we've actually buffered so far...

#if 0
    if (status < 0)
    {
        if (m_pop3ConData->msg_closure)
        {
            m_nsIPop3Sink->IncorporateAbort(m_pop3ConData->msg_closure,
                                            MK_POP3_MESSAGE_WRITE_ERROR);
            m_pop3ConData->msg_closure = NULL;
        }
        m_nsIPop3Sink->AbortMailDelivery();
        return(Error(MK_POP3_MESSAGE_WRITE_ERROR));
    }
#endif
    
    /* normal read. Yay! */
    if (m_pop3ConData->bytes_received_in_message + buffer_size >
        m_pop3ConData->cur_msg_size) 
        buffer_size = m_pop3ConData->cur_msg_size -
            m_pop3ConData->bytes_received_in_message; 
    
    m_pop3ConData->bytes_received_in_message += buffer_size;
    
#if 0
    ce->bytes_received            += buffer_size;
#endif 
    
    if (!m_pop3ConData->msg_closure) 
        /* meaning _handle_line read ".\r\n" at end-of-msg */
    {
        m_pop3ConData->pause_for_read = PR_FALSE;
        if (m_pop3ConData->truncating_cur_msg ||
            !(m_pop3CapabilityFlags & (POP3_HAS_UIDL | POP3_HAS_XTND_XLST |
                                       POP3_HAS_TOP)))
        {
            /* We've retrieved all or part of this message, but we want to
               keep it on the server.  Go on to the next message. */
            m_pop3ConData->last_accessed_msg++;
            m_pop3ConData->next_state = POP3_GET_MSG;
        } else
        {
            m_pop3ConData->next_state = POP3_SEND_DELE;
        }
        
#if 0
        /* if we didn't get the whole message add the bytes that we didn't get
           to the bytes received part so that the progress percent stays sane.
           */
        if(m_pop3ConData->bytes_received_in_message < m_pop3ConData->cur_msg_size)
            ce->bytes_received += (m_pop3ConData->cur_msg_size -
                                   m_pop3ConData->bytes_received_in_message);
#endif 
    }

#if 0
    if (m_pop3ConData->graph_progress_bytes_p)
        FE_GraphProgress(ce->window_id, ce->URL_s,
                         ce->bytes_received,
                         ce->bytes_received - old_bytes_received,
                         m_pop3ConData->cur_msg_size);
    
    /* set percent done to portion of total bytes of all messages
       that we're going to download. */
    if (m_pop3ConData->total_download_size)
        FE_SetProgressBarPercent(ce->window_id, ((ce->bytes_received*100) /
                                                 m_pop3ConData->total_download_size));
#endif 
    
    return(0);
}


PRInt32
nsPop3Protocol::TopResponse(nsIInputStream* inputStream, PRUint32 length)
{
    if(m_pop3ConData->cur_msg_size == -1 &&  /* first line after TOP command sent */
       !m_pop3ConData->command_succeeded)	/* and TOP command failed */
    {
        /* TOP doesn't work so we can't retrieve the first part of this msg.
           So just go download the whole thing, and warn the user.
           
           Note that the progress bar will not be accurate in this case.
           Oops. #### */
        PRBool prefBool = PR_FALSE;

        m_pop3CapabilityFlags &= ~POP3_HAS_TOP;
        m_pop3ConData->truncating_cur_msg = PR_FALSE;

#if 0
        fmt = XP_GetString( XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_THE_TOP_COMMAND );

        host = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);
        size = PL_strlen(fmt) + PL_strlen(host ? host : "(null)") + 100;
        buf = (char *) PR_CALLOC (size);
        if (!buf) {
            PR_FREEIF(host);
            return MK_OUT_OF_MEMORY;
        }
        PR_snprintf (buf, size, fmt, host ? host : "(null)");
        FE_Alert (ce->window_id, buf);
        PR_Free (buf);
        PR_FREEIF(host);
        
        PREF_GetBoolPref ("mail.auth_login", &prefBool);
#endif 
        if (prefBool && (POP3_XSENDER_UNDEFINED & m_pop3CapabilityFlags ||
                         POP3_HAS_XSENDER & m_pop3CapabilityFlags))
            m_pop3ConData->next_state = POP3_SEND_XSENDER;
        else
            m_pop3ConData->next_state = POP3_SEND_RETR;
        return(0);
	}

  /* If TOP works, we handle it in the same way as RETR. */
  return RetrResponse(inputStream, length);
}


PRInt32
nsPop3Protocol::HandleLine(char *line, PRUint32 line_length)
{
    int status;
    
    PR_ASSERT(m_pop3ConData->msg_closure);
    if (!m_pop3ConData->msg_closure)
        return -1;
    
    if (m_pop3ConData->sender_info && !m_pop3ConData->seenFromHeader)
    {
        if (line_length > 6 && !XP_MEMCMP("From: ", line, 6))
        {
            /* Zzzzz PL_strstr only works with NULL terminated string. Since,
             * the last character of a line is either a carriage return
             * or a linefeed. Temporary setting the last character of the
             * line to NULL and later setting it back should be the right 
             * thing to do. 
             */
            char ch = line[line_length-1];
            line[line_length-1] = 0;
            m_pop3ConData->seenFromHeader = PR_TRUE;
            if (PL_strstr(line, m_pop3ConData->sender_info) == NULL)
                m_nsIPop3Sink->SetSenderAuthedFlag(m_pop3ConData->msg_closure,
                                                     PR_FALSE);
            line[line_length-1] = ch;
        }
    }
    
    status =
        m_nsIPop3Sink->IncorporateWrite(m_pop3ConData->msg_closure, 
                                             line, line_length);
    
    if ((status >= 0) &&
        (line[0] == '.') &&
        ((line[1] == CR) || (line[1] == LF)))
    {
        gPOP3AssumedEnd = PR_TRUE;	/* in case byte count from server is */
                                    /* wrong, mark we may have had the end */ 
        if (!gPOP3dotFix || m_pop3ConData->truncating_cur_msg ||
            (gPOP3parsed_bytes >= (gPOP3size -3))) 
        {
            status = 
                m_nsIPop3Sink->IncorporateComplete(m_pop3ConData->msg_closure);
            m_pop3ConData->msg_closure = 0;
        }
    }
    
    return status;
}

PRInt32
nsPop3Protocol::SendDele()
{
    /* increment the last accessed message since we have now read it
     */
    m_pop3ConData->last_accessed_msg++;
    
    PR_snprintf(m_pop3ConData->output_buffer, 
                OUTPUT_BUFFER_SIZE,  
                "DELE %ld" CRLF, 
                m_pop3ConData->last_accessed_msg);
    
    m_pop3ConData->next_state_after_response = POP3_DELE_RESPONSE;
    
    return(SendCommand(m_pop3ConData->output_buffer));
}

PRInt32
nsPop3Protocol::DeleResponse()
{
    Pop3UidlHost *host = NULL;
	
		host = m_pop3ConData->uidlinfo;

    /* the return from the delete will come here
     */
    if(!m_pop3ConData->command_succeeded)
        return(Error(MK_POP3_DELE_FAILURE));
    
    
    /*	###chrisf
        the delete succeeded.  Write out state so that we
        keep track of all the deletes which have not yet been
        committed on the server.  Flush this state upon successful
        QUIT.
        
        We will do this by adding each successfully deleted message id
        to a list which we will write out to popstate.dat in 
        net_pop3_write_state().
        */
    if (host)
    {
        if (m_pop3ConData->msg_info &&
            m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg-1].uidl)
        { 
            if (m_pop3ConData->newuidl)
                PL_HashTableRemove(m_pop3ConData->newuidl, (void*)
                  m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg-1].uidl); 
                /* kill message in new hash table */
            else
                PL_HashTableRemove(host->hash, 
              (void*) m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg-1].uidl);
        }
    }

    m_pop3ConData->next_state = POP3_GET_MSG;

    m_pop3ConData->pause_for_read = PR_FALSE;
    
    return(0);
}


PRInt32
nsPop3Protocol::CommitState(PRBool remove_last_entry)
{
    if (remove_last_entry)
    {
        /* now, if we are leaving messages on the server, pull out the last
           uidl from the hash, because it might have been put in there before
           we got it into the database. */
        if (m_pop3ConData->msg_info) {
            Pop3MsgInfo* info = m_pop3ConData->msg_info +
                m_pop3ConData->last_accessed_msg; 
            if (info && info->uidl && (m_pop3ConData->only_uidl == NULL) &&
                m_pop3ConData->newuidl) { 
                PRBool val = PL_HashTableRemove (m_pop3ConData->newuidl, info->uidl);
                PR_ASSERT(val);
            }
        }
    }
    
    if (m_pop3ConData->newuidl) {
        PL_HashTableDestroy(m_pop3ConData->uidlinfo->hash);
        m_pop3ConData->uidlinfo->hash = m_pop3ConData->newuidl;
        m_pop3ConData->newuidl = NULL;
    }
    if (!m_pop3ConData->only_check_for_new_mail) {
        char* mailDirectory = 0;
        nsIPop3IncomingServer *popServer = nsnull;
        m_nsIPop3Sink->GetPopServer(&popServer);
        popServer->GetRootFolderPath(&mailDirectory);
        NS_IF_RELEASE(popServer);
        
        net_pop3_write_state(m_pop3ConData->uidlinfo, mailDirectory);
        PR_Free(mailDirectory);
    }
    return 0;
}


/* NET_process_Pop3  will control the state machine that
 * loads messages from a pop3 server
 *
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
PRInt32
nsPop3Protocol::ProcessPop3State (nsIURL* aURL, nsIInputStream* aInputStream,
                                  PRUint32 aLength)
{
    PRInt32 status = 0;

#if 0
    TRACEMSG(("Entering NET_ProcessPop3"));
#endif 

    m_pop3ConData->pause_for_read = PR_FALSE; /* already paused; reset */
    
    if(!m_username || !*m_username)
    {
        // net_pop3_block = PR_FALSE;
        return(Error(MK_POP3_USERNAME_UNDEFINED));
    }

    while(!m_pop3ConData->pause_for_read)
    {
#if 0        
        TRACEMSG(("POP3: Entering state: %d", m_pop3ConData->next_state));
#endif 

        switch(m_pop3ConData->next_state)
        {
        case POP3_READ_PASSWORD:
            /* This is a seperate state so that we're waiting for the
               user to type in a password while we don't actually have
               a connection to the pop server open; this saves us from
               having to worry about the server timing out on us while
               we wait for user input. */
			  {
            /* If we're just checking for new mail (biff) then don't
               prompt the user for a password; just tell him we don't
               know whether he has new mail. */
            if ((m_pop3ConData->only_check_for_new_mail /* ||
                 MSG_Biff_Master_NikiCallingGetNewMail() */) && 
                (!m_password || !m_username)) 
            {
                status = MK_POP3_PASSWORD_UNDEFINED;
                m_pop3ConData->biffstate = MSG_BIFF_UNKNOWN;
                m_nsIPop3Sink->SetBiffStateAndUpdateFE(m_pop3ConData->biffstate);	

                /* update old style biff */
                m_pop3ConData->next_state = POP3_FREE;
                m_pop3ConData->pause_for_read = PR_FALSE;
                break;
            }

#if 0            
            PR_ASSERT(net_pop3_username);
            if (m_pop3ConData->password_failed)
                fmt2 =
                    XP_GetString( XP_THE_PREVIOUSLY_ENTERED_PASSWORD_IS_INVALID_ETC );
            else if (!net_pop3_password)
                fmt1 = 
                    XP_GetString( XP_PASSWORD_FOR_POP3_USER );
            
            if (fmt1 || fmt2)	/* We need to read a password. */
            {
                char *password;
                char *host = NET_ParseURL(ce->URL_s->address,
                                          GET_HOST_PART);
                size_t len = (PL_strlen(fmt1 ? fmt1 : fmt2) +
                              (host ? PL_strlen(host) : 0) + 300) 
                    * sizeof(char);
                char *prompt = (char *) PR_CALLOC (len);
#if defined(CookiesAndSignons)                                        
                char *usernameAndHost=0;
#endif
                if (!prompt) {
                    PR_FREEIF(host);
                    net_pop3_block = PR_FALSE;
                    return MK_OUT_OF_MEMORY;
                }
                if (fmt1)
                    PR_snprintf (prompt, len, fmt1, net_pop3_username, host);
                else
                    PR_snprintf (prompt, len, fmt2,
                                 (m_pop3ConData->command_response
                                  ? m_pop3ConData->command_response
                                  : XP_GetString(XP_NO_ANSWER)),
                                 net_pop3_username, host);
#if defined(CookiesAndSignons)                                        
                StrAllocCopy(usernameAndHost, net_pop3_username);
                StrAllocCat(usernameAndHost, "@");
                StrAllocCat(usernameAndHost, host);
                PR_FREEIF (host);
                
                password = SI_PromptPassword
                    (ce->window_id,
                     prompt, usernameAndHost,
                     PR_FALSE, !m_pop3ConData->password_failed);
                m_pop3ConData->password_failed = PR_FALSE;
                PR_Free(usernameAndHost);
#else
                PR_FREEIF (host);
                m_pop3ConData->password_failed = PR_FALSE;
                password = FE_PromptPassword
                    (ce->window_id, prompt);
#endif
                PR_Free(prompt);
                
                if (password == NULL)
                {
                    net_pop3_block = PR_FALSE;
                    return MK_POP3_PASSWORD_UNDEFINED;
                }
                
                net_set_pop3_password(password);
                memset(password, 0, PL_strlen(password));
                PR_Free(password);
                
                net_pop3_password_pending = PR_TRUE;
            }
#endif 
            
            PR_ASSERT (m_username && m_password);
            if (!m_username || !m_password)
            {
                // net_pop3_block = PR_FALSE;
                return -1;
            }
            
            m_pop3ConData->next_state = POP3_START_CONNECT;
            m_pop3ConData->pause_for_read = PR_FALSE;
            break;
			  }
        
        case POP3_START_CONNECT:
        {
            m_pop3ConData->next_state = POP3_FINISH_CONNECT;
            m_pop3ConData->pause_for_read = PR_FALSE;
            break;
        }

        case POP3_FINISH_CONNECT:
        {
            PRBool prefBool = PR_FALSE;
            
            m_pop3ConData->pause_for_read = PR_TRUE;
            m_pop3ConData->next_state =
                POP3_WAIT_FOR_START_OF_CONNECTION_RESPONSE;
#if 0
            PREF_GetBoolPref ("mail.auth_login", &prefBool);
#endif 
            
            if (prefBool) {
                if (m_pop3CapabilityFlags & POP3_AUTH_LOGIN_UNDEFINED)
                    m_pop3ConData->next_state_after_response = POP3_SEND_AUTH;
                else if (m_pop3CapabilityFlags & POP3_HAS_AUTH_LOGIN)
                    m_pop3ConData->next_state_after_response = POP3_AUTH_LOGIN;
                else
                    m_pop3ConData->next_state_after_response = POP3_SEND_USERNAME;
            }
            else
                m_pop3ConData->next_state_after_response = POP3_SEND_USERNAME;
            break;
        }

        case POP3_WAIT_FOR_RESPONSE:
            status = WaitForResponse(aInputStream, aLength);
            break;
            
        case POP3_WAIT_FOR_START_OF_CONNECTION_RESPONSE:
            WaitForStartOfConnectionResponse(aInputStream, aLength);
            break;
            
        case POP3_SEND_AUTH:
            status = SendAuth();
            break;
            
        case POP3_AUTH_RESPONSE:
            status = AuthResponse(aInputStream, aLength);
            break;
            
        case POP3_AUTH_LOGIN:
            status = AuthLogin();
            break;
            
        case POP3_AUTH_LOGIN_RESPONSE:
            status = AuthLoginResponse();
            break;
            
        case POP3_SEND_USERNAME:
#if 0
            NET_Progress(ce->window_id,
               XP_GetString(XP_CONNECT_HOST_CONTACTED_SENDING_LOGIN_INFORMATION) );
#endif
            status = SendUsername();
            break;
            
        case POP3_SEND_PASSWORD:
            status = SendPassword();
            break;
            
        case POP3_SEND_GURL:
            status = SendGurl();
            break;
            
        case POP3_GURL_RESPONSE:
            status = GurlResponse();
            break;
            
        case POP3_SEND_STAT:
            status = SendStat();
            break;
            
        case POP3_GET_STAT:
            status = GetStat();
            break;
            
        case POP3_SEND_LIST:
            status = SendList();
            break;
            
        case POP3_GET_LIST:
            status = GetList(aInputStream, aLength);
            break;
            
        case POP3_SEND_UIDL_LIST:
            status = SendUidlList();
            break;
            
        case POP3_GET_UIDL_LIST:
            status = GetUidlList(aInputStream, aLength);
            break;
            
        case POP3_SEND_XTND_XLST_MSGID:
            status = SendXtndXlstMsgid();
            break;
            
        case POP3_GET_XTND_XLST_MSGID:
            status = GetXtndXlstMsgid(aInputStream, aLength);
            break;
            
        case POP3_START_USE_TOP_FOR_FAKE_UIDL:
            status = StartUseTopForFakeUidl();
            break;
            
    		case POP3_SEND_FAKE_UIDL_TOP:
            status = SendFakeUidlTop();
            break;
            
    		case POP3_GET_FAKE_UIDL_TOP:
            status = GetFakeUidlTop(aInputStream, aLength);
            break;
            
        case POP3_GET_MSG:
            status = GetMsg();
            break;
            
        case POP3_SEND_TOP:
            status = SendTop();
            break;
            
        case POP3_TOP_RESPONSE:
            status = TopResponse(aInputStream, aLength);
            break;

        case POP3_SEND_XSENDER:
            status = SendXsender();
            break;
            
        case POP3_XSENDER_RESPONSE:
            status = XsenderResponse();
            break;
            
        case POP3_SEND_RETR:
            status = SendRetr();
            break;
            
        case POP3_RETR_RESPONSE:
            status = RetrResponse(aInputStream, aLength);
            break;
            
        case POP3_SEND_DELE:
            status = SendDele();
            break;
            
        case POP3_DELE_RESPONSE:
            status = DeleResponse();
            break;
            
        case POP3_SEND_QUIT:
            /* attempt to send a server quit command.  Since this means
               everything went well, this is a good time to update the
               status file and the FE's biff state.
               */
            if (!m_pop3ConData->only_uidl) 
            {
                if (m_pop3ConData->only_check_for_new_mail)
                    m_nsIPop3Sink->SetBiffStateAndUpdateFE(m_pop3ConData->biffstate);	
                    /* update old style biff */
                else 
                {
                    /* We don't want to pop up a warning message any more (see
                       bug 54116), so instead we put the "no new messages" or
                       "retrieved x new messages" 
                       in the status line.  Unfortunately, this tends to be running
                       in a progress pane, so we try to get the real pane and
                       show the message there. */
#if 0
                    MWContext* context = ce->window_id;
                    if (m_pop3ConData->pane) {
                        MSG_Pane* parentpane = MSG_GetParentPane(m_pop3ConData->pane);
                        if (parentpane)
                            context = MSG_GetContext(parentpane);
                    }
#endif
                    if (m_pop3ConData->total_download_size <= 0) {
#if 0
                        /* There are no new messages.  */
                        if (context)
                            NET_Progress(context,
                                         XP_GetString(MK_POP3_NO_MESSAGES));
#endif 
                    }
                    else {
#if 0
                        /* at least 1 message was queued to download */
                        char *statusTemplate = XP_GetString (MK_MSG_DOWNLOAD_COUNT);
                        char *statusString = PR_smprintf (statusTemplate,
                              m_pop3ConData->real_new_counter - 1,
                              m_pop3ConData->really_new_messages);  
											/* (rb) not real
                         count. m_pop3ConData->last_accessed_msg,
                         m_pop3ConData->number_of_messages); */ 
                        if (context)
                            NET_Progress(context, statusString);
                        PR_FREEIF(statusString);
                        if (context == MSG_GetBiffContext())
#endif 
                            m_nsIPop3Sink->SetBiffStateAndUpdateFE(MSG_BIFF_NEWMAIL);
                    }
                }
            }

            PL_strcpy(m_pop3ConData->output_buffer, "QUIT" CRLF);
            status = SendCommand(m_pop3ConData->output_buffer);
            m_pop3ConData->next_state = POP3_WAIT_FOR_RESPONSE;
#ifdef POP_ALWAYS_USE_UIDL_FOR_DUPLICATES
            m_pop3ConData->next_state_after_response = POP3_QUIT_RESPONSE;
#else
            m_pop3ConData->next_state_after_response = POP3_DONE;
#endif
            break;

#ifdef POP_ALWAYS_USE_UIDL_FOR_DUPLICATES
        case POP3_QUIT_RESPONSE:
            if(m_pop3ConData->command_succeeded)
            {
                /*	the QUIT succeeded.  We can now flush the state in popstate.dat which
                    keeps track of any uncommitted DELE's */
                
                /* here we need to clear the hash of all our 
                   uncommitted deletes */
                /*
                  if (m_pop3ConData->uidlinfo &&
                      m_pop3ConData->uidlinfo->uncommitted_deletes) 
                      XP_Clrhash (m_pop3ConData->uidlinfo->uncommitted_deletes);*/

                m_pop3ConData->next_state = POP3_DONE;

            }
            else
            {
                m_pop3ConData->next_state = POP3_ERROR_DONE;
            }
            break;
#endif
            
        case POP3_DONE:
            CommitState(PR_FALSE);
            
            if(m_pop3ConData->msg_del_started)
                m_nsIPop3Sink->EndMailDelivery();

            if (m_transport)
                m_transport->OnStopBinding(m_nsIPop3URL, 0, nsnull);

			if (m_nsIPop3URL)
				m_nsIPop3URL->SetUrlState(PR_FALSE, NS_OK);

            m_pop3ConData->next_state = POP3_FREE;
            break;

        case POP3_INTERRUPTED:
				{
    				PL_strcpy(m_pop3ConData->output_buffer, "QUIT" CRLF);
            SendCommand(m_pop3ConData->output_buffer);
#if 0
            /*
              nnet_graceful_shutdown(ce->socket, PR_FALSE);
              */ /* make sure QUIT get send before closing down the socket */
#endif 
            m_pop3ConData->pause_for_read = PR_FALSE;
            m_pop3ConData->next_state = POP3_ERROR_DONE;
				}
				break;
        
        case POP3_ERROR_DONE:

            /*  write out the state */
            CommitState(PR_TRUE);
				
            if(m_pop3ConData->msg_closure)
            {
                m_nsIPop3Sink->IncorporateAbort(m_pop3ConData->msg_closure,
                                                MK_POP3_MESSAGE_WRITE_ERROR);
                m_pop3ConData->msg_closure = NULL;
                m_nsIPop3Sink->AbortMailDelivery();
            }
            
            if(m_pop3ConData->msg_del_started)
            {
#if 0
                char *statusTemplate = XP_GetString (MK_MSG_DOWNLOAD_COUNT);
                char *statusString = PR_smprintf (statusTemplate,
                                                  m_pop3ConData->real_new_counter, 
                                                  m_pop3ConData->really_new_messages); 
                                               /* m_pop3ConData->last_accessed_msg,
                                                  m_pop3ConData->number_of_messages); */
                MWContext* context = ce->window_id;
                if (m_pop3ConData->pane) {
                    MSG_Pane* parentpane = MSG_GetParentPane(m_pop3ConData->pane);
                    if (parentpane) {
                        context = MSG_GetContext(parentpane);
                    }
                }
#endif
                PR_ASSERT (!m_pop3ConData->password_failed);
                m_nsIPop3Sink->AbortMailDelivery();
#if 0
                if (context)
                    NET_Progress(context, statusString);
                PR_FREEIF(statusString);
#endif 
            }
            
            if (m_pop3ConData->password_failed)
                /* We got here because the password was wrong, so go
                   read a new one and re-open the connection. */
                m_pop3ConData->next_state = POP3_READ_PASSWORD;
            else
                /* Else we got a "real" error, so finish up. */
                m_pop3ConData->next_state = POP3_FREE;
            
            if (m_transport)
                m_transport->OnStopBinding(m_nsIPop3URL, 0, nsnull);

			if (m_nsIPop3URL)
				m_nsIPop3URL->SetUrlState(PR_FALSE, NS_ERROR_FAILURE);

            m_pop3ConData->pause_for_read = PR_FALSE;
            break;
            
        case POP3_FREE:
            m_isRunning = PR_FALSE;
            /*************** done by destructor
              if (m_pop3ConData->newuidl) PL_HashTableDestroy(m_pop3ConData->newuidl);
              net_pop3_free_state(m_pop3ConData->uidlinfo);
              ***************/
#if 0
            if (m_pop3ConData->graph_progress_bytes_p) {
                /* Only destroy it if we have initialized it. */
                FE_GraphProgressDestroy(ce->window_id, ce->URL_s,
										  m_pop3ConData->cur_msg_size,
										  ce->bytes_received);
            }
#endif 
            /***************** Done by destructor
              FreeMsgInfo();
              PR_FREEIF(m_pop3ConData->only_uidl);
              PR_FREEIF(m_pop3ConData->output_buffer);
              PR_FREEIF(m_pop3ConData->obuffer);
              PR_FREEIF(m_pop3ConData->command_response);
              PR_FREEIF(m_pop3ConData->sender_info);
              PR_Free(ce->con_data);
              ******************/
            
            /* release the semaphore which prevents POP3 from creating more
               connections */ 
            // net_pop3_block = PR_FALSE;
            
            return(-1);
            break;
            
        default:
            PR_ASSERT(0);
            
	      }  /* end switch */

        if((status < 0) && m_pop3ConData->next_state != POP3_FREE)
        {
            m_pop3ConData->pause_for_read = PR_FALSE;
            m_pop3ConData->next_state = POP3_ERROR_DONE;
        }
        
	  }  /* end while */
    
    return(status);
    
}
