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
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Jason Eager <jce2@po.cwru.edu>
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 * 06/07/2000       Jason Eager    Added check for out of disk space
 */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif 

#include "nscore.h"
#include "msgCore.h"    // precompiled header...
#include "nsNetUtil.h"
#include "nspr.h"
#include "nsCRT.h"
#include "plbase64.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsPop3Protocol.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "MailNewsTypes.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIPrompt.h"
#include "nsIMsgIncomingServer.h"
#include "nsLocalStringBundle.h"
#include "nsTextFormatter.h"
#include "nsCOMPtr.h"
#include "nsIPref.h" 
#include "nsIMsgWindow.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...
#include "nsIDocShell.h"

#define EXTRA_SAFETY_SPACE 3096

static PRLogModuleInfo *POP3LOGMODULE = nsnull;

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID); 

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

static PRIntn PR_CALLBACK
net_pop3_check_for_hash_messages_marked_delete(PLHashEntry* he,
				       						   PRIntn msgindex, 
				       						   void *arg)
{
	char valueChar = (char) NS_PTR_TO_INT32(he->value);
	if (valueChar == DELETE_CHAR)
	{
		((Pop3ConData *) arg)->delete_server_message_during_top_traversal = PR_TRUE;
		return HT_ENUMERATE_STOP;	/* XP_Maphash will stop traversing hash table now */
	}
	
	return HT_ENUMERATE_NEXT;		/* XP_Maphash will continue traversing the hash */
}

static PRIntn PR_CALLBACK
net_pop3_remove_messages_marked_delete(PLHashEntry* he,
			  						   PRIntn msgindex, 
			   						   void *arg)
{
	char valueChar = (char)(long)he->value;
	if (valueChar == DELETE_CHAR)
	  return HT_ENUMERATE_REMOVE;
    else
      return HT_ENUMERATE_NEXT;     /* XP_Maphash will continue traversing the hash */
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
                    nsIFileSpec *mailDirectory)
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

  nsFileSpec fileSpec;
  mailDirectory->GetFileSpec(&fileSpec);
  fileSpec += "popstate.dat";

  nsInputFileStream fileStream(fileSpec);

  buf = (char*)PR_CALLOC(512);
  if (buf) {
	while (!fileStream.eof() && !fileStream.failed() && fileStream.is_open())
  {
      fileStream.readline(buf, 512);
      if (*buf == '#' || *buf == nsCRT::CR || *buf == nsCRT::LF || *buf == 0)
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
  if (fileStream.is_open())
   fileStream.close();

  return result;
}

static PRIntn PR_CALLBACK
hash_clear_mapper(PLHashEntry* he, PRIntn msgindex, void* arg)
{
#ifdef UNREADY_CODE   // mscott: the compiler won't take this line and I can't figure out why..=(
  PR_FREEIF( (char *)he->key );
#endif
  return HT_ENUMERATE_REMOVE;
}

static PRIntn PR_CALLBACK
hash_empty_mapper(PLHashEntry* he, PRIntn msgindex, void* arg)
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


static PRIntn PR_CALLBACK
net_pop3_write_mapper(PLHashEntry* he, PRIntn msgindex, void* arg)
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
net_pop3_write_state(Pop3UidlHost* host, nsIFileSpec *mailDirectory)
{
  PRInt32 len = 0;

  nsFileSpec fileSpec;
  mailDirectory->GetFileSpec(&fileSpec);
  fileSpec += "popstate.dat";

  nsOutputFileStream outFileStream(fileSpec, PR_WRONLY | PR_CREATE_FILE |
                                   PR_TRUNCATE);
	const char tmpBuffer[] =
        "# POP3 State File" MSG_LINEBREAK
        "# This is a generated file!  Do not edit." MSG_LINEBREAK
        MSG_LINEBREAK;

  outFileStream << tmpBuffer;

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
  if (outFileStream.is_open())
  {
    outFileStream.flush();
    outFileStream.close();
  }
}

/*
Wrapper routines for POP data. The following routines are used from MSG_FolderInfoMail to
allow deleting of messages that have been kept on a POP3 server due to their size or
a preference to keep the messages on the server. When "deleting" messages we load
our state file, mark any messages we have for deletion and then re-save the state file.
*/
extern char* ReadPopData(const char *hostname, const char* username, nsIFileSpec* maildirectory);
extern void SavePopData(char *data, nsIFileSpec* maildirectory);
extern void net_pop3_delete_if_in_server(char *data, char *uidl, PRBool *changed);
extern void KillPopData(char* data);
static void net_pop3_free_state(Pop3UidlHost* host);


char* ReadPopData(const char *hostname, 
                  const char* username, 
                  nsIFileSpec* mailDirectory)
{
	Pop3UidlHost *uidlHost = NULL;
	if(!username || !*username)
		return (char*) uidlHost;
	
	uidlHost = net_pop3_load_state(hostname, username, mailDirectory);
	return (char*) uidlHost;
}

void SavePopData(char *data, nsIFileSpec* mailDirectory)
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

void KillPopData(char* data)
{
	if (!data)
		return;
	net_pop3_free_state((Pop3UidlHost*) data);
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

// nsPop3Protocol class implementation

nsPop3Protocol::nsPop3Protocol(nsIURI* aURL)
    : nsMsgProtocol(aURL),
	nsMsgLineBuffer(NULL, PR_FALSE),
	m_bytesInMsgReceived(0), 
	m_totalFolderSize(0),    
	m_totalDownloadSize(0),
	m_totalBytesReceived(0),
	m_lineStreamBuffer(nsnull),
	m_pop3ConData(nsnull)
{
	SetLookingForCRLF(MSG_LINEBREAK_LEN == 2);
}

nsresult nsPop3Protocol::Initialize(nsIURI * aURL)
{
  nsresult rv = NS_OK;

  m_pop3ConData = (Pop3ConData *)PR_NEWZAP(Pop3ConData);
  PR_ASSERT(m_pop3ConData);

  if(!m_pop3ConData)
	  return NS_ERROR_OUT_OF_MEMORY;

  m_totalBytesReceived = 0;
  m_bytesInMsgReceived = 0; 
  m_totalFolderSize = 0;    
  m_totalDownloadSize = 0;
  m_totalBytesReceived = 0;

  if (aURL)
  {
    PRBool isSecure = PR_FALSE;

    // extract out message feedback if there is any.
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aURL);
    if (mailnewsUrl)
    {
        nsCOMPtr<nsIMsgIncomingServer> server;
      mailnewsUrl->GetStatusFeedback(getter_AddRefs(m_statusFeedback));
      mailnewsUrl->GetServer(getter_AddRefs(server));
      NS_ENSURE_TRUE(server, NS_MSG_INVALID_OR_MISSING_SERVER);

      rv = server->GetIsSecure(&isSecure);
      NS_ENSURE_SUCCESS(rv,rv);

      m_pop3Server = do_QueryInterface(server);
      if (m_pop3Server)
        m_pop3Server->GetPop3CapabilityFlags(&m_pop3ConData->capability_flags);
    }

    m_url = do_QueryInterface(aURL);

    nsCOMPtr<nsIInterfaceRequestor> ir;
    nsCOMPtr<nsIMsgWindow> msgwin;
    mailnewsUrl->GetMsgWindow(getter_AddRefs(msgwin));
    if (msgwin) {
        nsCOMPtr<nsIDocShell> docshell;
        msgwin->GetRootDocShell(getter_AddRefs(docshell));
        ir = do_QueryInterface(docshell);
    }

    PRInt32 port = 0;
    nsXPIDLCString hostName;
    aURL->GetPort(&port);
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
    if (server)
      server->GetRealHostName(getter_Copies(hostName));

    if (isSecure)
      rv = OpenNetworkSocketWithInfo(hostName.get(), port, "ssl-forcehandshake", ir);
    else
      rv = OpenNetworkSocketWithInfo(hostName.get(), port, nsnull, ir);

	if(NS_FAILED(rv))
		return rv;
  } // if we got a url...
  
  if (!POP3LOGMODULE)
      POP3LOGMODULE = PR_NewLogModule("POP3");

  m_lineStreamBuffer = new nsMsgLineStreamBuffer(OUTPUT_BUFFER_SIZE, PR_TRUE);
  if(!m_lineStreamBuffer)
	  return NS_ERROR_OUT_OF_MEMORY;

  mStringService = do_GetService(NS_MSG_POPSTRINGSERVICE_CONTRACTID);
  return rv;
}

nsPop3Protocol::~nsPop3Protocol()
{
	if (m_pop3ConData->newuidl) PL_HashTableDestroy(m_pop3ConData->newuidl);
		net_pop3_free_state(m_pop3ConData->uidlinfo);

	UpdateProgressPercent(0, 0);

	FreeMsgInfo();
	PR_FREEIF(m_pop3ConData->only_uidl);
	PR_Free(m_pop3ConData);

	if (m_lineStreamBuffer)
		delete m_lineStreamBuffer;
}

void nsPop3Protocol::UpdateStatus(PRInt32 aStatusID)
{
	if (m_statusFeedback)
	{
        PRUnichar * statusString = nsnull;
        mStringService->GetStringByID(aStatusID, &statusString);
		UpdateStatusWithString(statusString);
		nsCRT::free(statusString);
	}
}

void nsPop3Protocol::UpdateStatusWithString(const PRUnichar * aStatusString)
{
    nsresult rv;
    if (mProgressEventSink) {
        rv = mProgressEventSink->OnStatus(this, m_channelContext, NS_OK, aStatusString);      // XXX i18n message
        NS_ASSERTION(NS_SUCCEEDED(rv), "dropping error result");
    }
}

void nsPop3Protocol::UpdateProgressPercent (PRUint32 totalDone, PRUint32 total)
{
  if (mProgressEventSink)
    mProgressEventSink->OnProgress(this, m_channelContext, (PRInt32) totalDone, (PRInt32) total); 
}

// note:  SetUsername() expects an unescaped string
// do not pass in an escaped string
void nsPop3Protocol::SetUsername(const char* name)
{
	NS_ASSERTION(name, "no name specified!");
	if (name) {
		m_username = name;
    }
}

nsresult nsPop3Protocol::GetPassword(char ** aPassword, PRBool *okayValue)
{
	nsresult rv = NS_OK;
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
    
	if (server)
	{
        // clear the password if the last one failed
		if (TestFlag(POP3_PASSWORD_FAILED))
		{
			// if we've already gotten a password and it wasn't correct..clear
			// out the password and try again.
			rv = server->SetPassword("");
		}

		// first, figure out the correct prompt text to use...
        nsXPIDLCString hostName;
        nsXPIDLCString userName;
        PRUnichar *passwordPromptString =nsnull;
        
        server->GetRealHostName(getter_Copies(hostName));
        server->GetRealUsername(getter_Copies(userName));
        nsXPIDLString passwordTemplate;
        // if the last prompt got us a bad password then show a special dialog
        if (TestFlag(POP3_PASSWORD_FAILED))
        { 
            rv = server->ForgetPassword();
            if (NS_FAILED(rv)) return rv;
            mStringService->GetStringByID(POP3_PREVIOUSLY_ENTERED_PASSWORD_IS_INVALID_ETC, getter_Copies(passwordTemplate));
        } // otherwise this is the first time we've asked about the server's password so show a first time prompt
        else
            mStringService->GetStringByID(POP3_ENTER_PASSWORD_PROMPT, getter_Copies(passwordTemplate));
        if (passwordTemplate)
          passwordPromptString = nsTextFormatter::smprintf(passwordTemplate, (const char *) userName, (const char *) hostName);
        // now go get the password!!!!
        nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIMsgWindow> aMsgWindow;
        rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(aMsgWindow));
        if (NS_FAILED(rv)) return rv;
        nsXPIDLString passwordTitle;
        mStringService->GetStringByID(POP3_ENTER_PASSWORD_PROMPT_TITLE, getter_Copies(passwordTitle));
        if (passwordPromptString)
        {
          if (passwordTitle)
            rv =  server->GetPasswordWithUI(passwordPromptString, passwordTitle.get(),
                                          aMsgWindow, okayValue, aPassword);
          nsTextFormatter::smprintf_free(passwordPromptString);
        }

        ClearFlag(POP3_PASSWORD_FAILED);
        if (NS_FAILED(rv))
            m_pop3ConData->next_state = POP3_ERROR_DONE;
    } // if we have a server
	else
		rv = NS_MSG_INVALID_OR_MISSING_SERVER;

	return rv;
}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsPop3Protocol::OnStopRequest(nsIRequest *request, nsISupports * aContext, nsresult aStatus)
{
	nsresult rv = nsMsgProtocol::OnStopRequest(request, aContext, aStatus);
	// turn off the server busy flag on stop request - we know we're done, right?
	if (m_pop3Server)
	{
		nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
		if (server)
			server->SetServerBusy(PR_FALSE); // the server is now busy
	}
	CommitState(PR_TRUE);
	return rv;
}

NS_IMETHODIMP nsPop3Protocol::Cancel(nsresult status)  // handle stop button
{
  if(m_pop3ConData->msg_closure)
  {
      m_nsIPop3Sink->IncorporateAbort(m_pop3ConData->only_uidl != nsnull);
      m_pop3ConData->msg_closure = NULL;
  }
  // need this to close the stream on the inbox.
  m_nsIPop3Sink->AbortMailDelivery();
  return nsMsgProtocol::Cancel(NS_BINDING_ABORTED);
}


nsresult nsPop3Protocol::LoadUrl(nsIURI* aURL, nsISupports * /* aConsumer */)
{
	nsresult rv = 0;

    if (aURL)
		m_url = do_QueryInterface(aURL);
    else
        return NS_ERROR_FAILURE;

	// Time to figure out the pop3 account name and password, either from the
	// prefs file or prompt user for them
	// 
	// -*-*-*- To Do:
	// Call SetUsername(accntName);
	// Call SetPassword(aPassword);

	nsCOMPtr<nsIURL> url = do_QueryInterface(aURL, &rv);
	if (NS_FAILED(rv)) return rv;

    PRInt32 port;
    rv = url->GetPort(&port);
    if (NS_FAILED(rv))
        return rv;

    rv = NS_CheckPortSafety(port, "pop");
    if (NS_FAILED(rv))
        return rv;

	nsXPIDLCString queryPart;
	rv = url->GetQuery(getter_Copies(queryPart));
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get the url spect");

	if (PL_strcasestr(queryPart, "check"))
		m_pop3ConData->only_check_for_new_mail = PR_TRUE;
  else
    m_pop3ConData->only_check_for_new_mail = PR_FALSE;

	if (PL_strcasestr(queryPart, "gurl"))
		m_pop3ConData->get_url = PR_TRUE;
  else
    m_pop3ConData->get_url = PR_FALSE;

	if (!m_pop3ConData->only_check_for_new_mail)
	{
		// Pick up pref setting regarding leave messages on server, message
        // size limit

        m_pop3Server->GetLeaveMessagesOnServer(&m_pop3ConData->leave_on_server);
        PRBool limitMessageSize = PR_FALSE;
        nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
        if (server)
        {
          server->GetLimitOfflineMessageSize(&limitMessageSize);
          if (limitMessageSize)
          {
              PRInt32 max_size = 0;
              server->GetMaxMessageSize(&max_size);
              if (max_size == 0) // default value
                  m_pop3ConData->size_limit = 50 * 1024;
              else
                  m_pop3ConData->size_limit = max_size * 1024;
          }
        }
	}

	// UIDL stuff
  nsCOMPtr<nsIPop3URL> pop3Url = do_QueryInterface(m_url);
  if (pop3Url)
      pop3Url->GetPop3Sink(getter_AddRefs(m_nsIPop3Sink));
    
  nsCOMPtr<nsIFileSpec> mailDirectory;
    
  nsXPIDLCString hostName;
  nsXPIDLCString userName;

  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
  if (server)
	{
    rv = server->GetLocalPath(getter_AddRefs(mailDirectory));
		server->SetServerBusy(PR_TRUE); // the server is now busy
    server->GetHostName(getter_Copies(hostName));
    server->GetUsername(getter_Copies(userName));
	}

  m_pop3ConData->uidlinfo = net_pop3_load_state(hostName, userName, mailDirectory);
	m_pop3ConData->biffstate = nsIMsgFolder::nsMsgBiffState_NoMail;

	const char* uidl = PL_strcasestr(queryPart, "uidl=");
  PR_FREEIF(m_pop3ConData->only_uidl);
		
  if (uidl)
	{
		uidl += 5;
		m_pop3ConData->only_uidl = PL_strdup(uidl);
    mSuppressListenerNotifications = PR_TRUE; // suppress on start and on stop because this url won't have any content to display
	}
	
	// m_pop3ConData->next_state = POP3_READ_PASSWORD;
	m_pop3ConData->next_state = POP3_START_CONNECT;
	m_pop3ConData->next_state_after_response = POP3_FINISH_CONNECT;
	if (NS_SUCCEEDED(rv))
		return nsMsgProtocol::LoadUrl(aURL);
	else
		return rv;
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

    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

    if(pauseForMoreData || !line)
    {
        m_pop3ConData->pause_for_read = PR_TRUE; /* pause */
        PR_FREEIF(line);
        return(line_length);
    }

    if(*line == '+')
    {
        m_pop3ConData->command_succeeded = PR_TRUE;
        if(PL_strlen(line) > 4)
			m_commandResponse = line+4;
        else
			m_commandResponse = line;
         
        m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
    }
	PR_FREEIF(line);
    return(1);  /* everything ok */
}

PRInt32
nsPop3Protocol::WaitForResponse(nsIInputStream* inputStream, PRUint32 length)
{
    char * line;
    PRUint32 ln = 0;
	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(pauseForMoreData || !line)
    {
	    m_pop3ConData->pause_for_read = PR_TRUE; /* don't pause */
		PR_FREEIF(line);
        return(ln);
    }

    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

    if(*line == '+')
    {
        m_pop3ConData->command_succeeded = PR_TRUE;
        if(PL_strlen(line) > 4)
		{
			if(!PL_strncasecmp(line, "+OK", 3))
 				m_commandResponse = line + 4;
			else if(PL_strncasecmp(m_commandResponse.get(), "Invalid login", 13))
				m_commandResponse = "+";
		}
	
        else
			m_commandResponse = line;
    }
    else
	{
        m_pop3ConData->command_succeeded = PR_FALSE;
        if(PL_strlen(line) > 5)
			m_commandResponse = line + 5;
        else
			m_commandResponse  = line;
	}

    m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
    m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */

	PR_FREEIF(line);
    return(1);  /* everything ok */
}

PRInt32
nsPop3Protocol::Error(PRInt32 err_code)
{
	// the error code is just the resource id for the error string...
	// so print out that error message!
	nsresult rv = NS_OK;
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIMsgWindow> msgWindow;
        nsCOMPtr<nsIPrompt> dialog;
        rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
	    NS_ASSERTION(NS_SUCCEEDED(rv) && msgWindow, "no msg window");
        if (NS_SUCCEEDED(rv) && msgWindow)
        {
            rv = msgWindow->GetPromptDialog(getter_AddRefs(dialog));
            if (NS_SUCCEEDED(rv))
            {
              nsXPIDLString alertString;
              mStringService->GetStringByID(err_code, getter_Copies(alertString));
              if (m_pop3ConData->command_succeeded)  //not a server error message
                dialog->Alert(nsnull, alertString.get()); 
              else
              {
                nsXPIDLString serverSaidPrefix;
                mStringService->GetStringByID(POP3_SERVER_SAID,getter_Copies(serverSaidPrefix));
                nsAutoString message(alertString + NS_LITERAL_STRING(" ") +
                                     serverSaidPrefix + NS_LITERAL_STRING(" ") +
                                     NS_ConvertASCIItoUCS2(m_commandResponse));
                dialog->Alert(nsnull,message.get()); 
              }
            }
        }
    }
    m_pop3ConData->next_state = POP3_ERROR_DONE;
    m_pop3ConData->pause_for_read = PR_FALSE;

    return -1;
}

PRInt32 nsPop3Protocol::SendData(nsIURI * aURL, const char * dataBuffer, PRBool aSuppressLogging)
{
  	PRInt32 result = nsMsgProtocol::SendData(aURL, dataBuffer);

    if (!aSuppressLogging) {
        PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS, ("SEND: %s", dataBuffer));        
    }
    else {
        PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS, ("Logging suppressed for this command (it probably contained authentication information)"));
    }

	if (result >= 0) // yeah this sucks...i need an error code....
	{
	    m_pop3ConData->pause_for_read = PR_TRUE;
        m_pop3ConData->next_state = POP3_WAIT_FOR_RESPONSE;
	}
	else
		m_pop3ConData->next_state = POP3_ERROR_DONE;

	return 0;
}

/*
 * POP3 AUTH LOGIN extention
 */

PRInt32 nsPop3Protocol::SendAuth()
{
    if(!m_pop3ConData->command_succeeded)
        return(Error(POP3_SERVER_ERROR));

	nsCAutoString command("AUTH"CRLF);

  m_pop3ConData->next_state_after_response = POP3_AUTH_RESPONSE;
	return SendData(m_url, command.get());
}

PRInt32 nsPop3Protocol::AuthResponse(nsIInputStream* inputStream, 
                             PRUint32 length)
{
    char * line;
    PRUint32 ln = 0;

    if (POP3_AUTH_LOGIN_UNDEFINED & m_pop3ConData->capability_flags)
    {
        m_pop3ConData->capability_flags &= ~POP3_AUTH_LOGIN_UNDEFINED;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }
    
    if (!m_pop3ConData->command_succeeded) 
    {
        /* AUTH command not implemented 
         * no base64 encoded username/password
         */
        m_pop3ConData->command_succeeded = PR_TRUE;
        m_pop3ConData->capability_flags &= ~POP3_HAS_AUTH_LOGIN;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
        m_pop3ConData->next_state = POP3_SEND_USERNAME;
        return 0;
    }
    
    PRBool pauseForMoreData = PR_FALSE;
    line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(pauseForMoreData || !line) 
    {
        m_pop3ConData->pause_for_read = PR_TRUE; /* don't pause */
        PR_FREEIF(line);
        return(0);
    }
    
    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

    if (!PL_strcmp(line, ".")) 
    {
        /* now that we've read all the AUTH responses, decide which 
        * state to go to next 
        */
        if (m_pop3ConData->capability_flags & POP3_HAS_AUTH_LOGIN)
            m_pop3ConData->next_state = POP3_AUTH_LOGIN;
        else
            m_pop3ConData->next_state = POP3_SEND_USERNAME;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
    }
    else if (!PL_strcasecmp (line, "LOGIN")) 
    {
        m_pop3ConData->capability_flags |= POP3_HAS_AUTH_LOGIN;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }

	PR_FREEIF(line);
    return 0;
}

PRInt32 nsPop3Protocol::AuthLogin()
{
    /* check login response */
    if(!m_pop3ConData->command_succeeded) 
    {
        m_pop3ConData->capability_flags &= ~POP3_HAS_AUTH_LOGIN;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
        return(Error(POP3_SERVER_ERROR));
    }
    
    nsCAutoString command("AUTH LOGIN" CRLF);
    m_pop3ConData->next_state_after_response = POP3_AUTH_LOGIN_RESPONSE;
    return SendData(m_url, command.get());
}

PRInt32 nsPop3Protocol::AuthLoginResponse()
{
    if (!m_pop3ConData->command_succeeded) 
    {
        /* sounds like server does not support auth-skey extension
           resume regular logon process */
        /* reset command_succeeded to true */
        m_pop3ConData->command_succeeded = PR_TRUE;
        /* reset auth login state */
        m_pop3ConData->capability_flags &= ~POP3_HAS_AUTH_LOGIN;
    }
    else
    {
        m_pop3ConData->capability_flags |= POP3_HAS_AUTH_LOGIN;
    }
    m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    m_pop3ConData->next_state = POP3_SEND_USERNAME;
    return 0;
}


PRInt32 nsPop3Protocol::SendUsername()
{
    /* check login response */
    if(!m_pop3ConData->command_succeeded)
        return(Error(POP3_SERVER_ERROR));

    if(m_username.IsEmpty())
        return(Error(POP3_USERNAME_UNDEFINED));

    nsCAutoString cmd;

    if (POP3_HAS_AUTH_LOGIN & m_pop3ConData->capability_flags) 
	{
        char * str =
            PL_Base64Encode(m_username.get(), m_username.Length(), nsnull);
        cmd = str;
        PR_FREEIF(str);
    }
    else 
	{
        cmd = "USER ";
        cmd += m_username;
    }
    cmd += CRLF;

    m_pop3ConData->next_state_after_response = POP3_SEND_PASSWORD;

    return SendData(m_url, cmd.get());
}

PRInt32 nsPop3Protocol::SendPassword()
{
    /* check username response */
    if (!m_pop3ConData->command_succeeded)
        return(Error(POP3_USERNAME_FAILURE));
    nsXPIDLCString password;
    PRBool okayValue = PR_TRUE;
	nsresult rv = GetPassword(getter_Copies(password), &okayValue);
    if (NS_SUCCEEDED(rv) && !okayValue)
    {
    // user has canceled the password prompt
        m_pop3ConData->next_state = POP3_ERROR_DONE;
        return NS_ERROR_ABORT;
    }
    else if (NS_FAILED(rv) || !password)
    {
      return Error(POP3_PASSWORD_UNDEFINED);
    }
    nsCAutoString cmd;

    if (POP3_HAS_AUTH_LOGIN & m_pop3ConData->capability_flags) 
	{
        char * str = 
            PL_Base64Encode(password, PL_strlen(password), nsnull);
        cmd = str;
        PR_FREEIF(str);
    }
    else
    {
        cmd = "PASS ";
        cmd += (const char *) password;    
    }
    cmd += CRLF;

    if (m_pop3ConData->get_url)
        m_pop3ConData->next_state_after_response = POP3_SEND_GURL;
    else
        m_pop3ConData->next_state_after_response = POP3_SEND_STAT;

    return SendData(m_url, cmd.get(), PR_TRUE);
}

PRInt32 nsPop3Protocol::SendStatOrGurl(PRBool sendStat)
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
        Error(POP3_PASSWORD_FAILURE);

        SetFlag(POP3_PASSWORD_FAILED);

        // libmsg event sink
        if (m_nsIPop3Sink) 
        {
            m_nsIPop3Sink->SetUserAuthenticated(PR_FALSE);
            m_nsIPop3Sink->SetMailAccountURL(NULL);
        }

        /* clear the bogus password in case 
         * we need to sync with auth smtp password 
         */
        return 0;
    }
    else 
    {
        m_nsIPop3Sink->SetUserAuthenticated(PR_TRUE);
    }

    
	nsCAutoString cmd;
    if (sendStat) 
	{
		cmd  = "STAT" CRLF;
        m_pop3ConData->next_state_after_response = POP3_GET_STAT;
    }
    else 
	{
		cmd = "GURL" CRLF;
        m_pop3ConData->next_state_after_response = POP3_GURL_RESPONSE;
    }
    return SendData(m_url, cmd.get());
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
	char* oldStr;

    /* check stat response */
    if(!m_pop3ConData->command_succeeded)
      return(Error(POP3_STAT_FAILURE));

    /* stat response looks like:  %d %d
     * The first number is the number of articles
     * The second number is the number of bytes
     *
     *  grab the first and second arg of stat response
     */
	oldStr = ToNewCString(m_commandResponse);
    num = nsCRT::strtok(oldStr, " ", &newStr);

    m_pop3ConData->number_of_messages = atol(num);

    num = nsCRT::strtok(newStr, " ", &newStr);
	m_commandResponse = newStr;
	
    m_totalFolderSize = (PRInt32) atol(num);
	PR_FREEIF(oldStr);
    m_pop3ConData->really_new_messages = 0;
    m_pop3ConData->real_new_counter = 1;

    m_totalDownloadSize = -1; /* Means we need to calculate it, later. */

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
        m_pop3ConData->biffstate = nsIMsgFolder::nsMsgBiffState_NewMail;
        m_pop3ConData->next_state = POP3_SEND_QUIT;
        return(0);
    }


    if (!m_pop3ConData->only_check_for_new_mail) {
        // The following was added to prevent the loss of Data when we try and
        // write to somewhere we dont have write access error to (See bug 62480)
        // (Note: This is only a temp hack until the underlying XPCOM is fixed
        // to return errors)
        nsresult rv;
        rv = m_nsIPop3Sink->BeginMailDelivery(m_pop3ConData->only_uidl != nsnull,
                                                      &m_pop3ConData->msg_del_started);
        if (NS_FAILED(rv))
          if (rv == NS_MSG_FOLDER_BUSY)
            return(Error(POP3_MESSAGE_FOLDER_BUSY));
          else
            return(Error(POP3_MESSAGE_WRITE_ERROR));
        if(!m_pop3ConData->msg_del_started)
        {
            return(Error(POP3_MESSAGE_WRITE_ERROR));
        }
    }

    m_pop3ConData->next_state = POP3_SEND_LIST;
    return 0;
}



PRInt32
nsPop3Protocol::SendGurl()
{
    if (m_pop3ConData->capability_flags == POP3_CAPABILITY_UNDEFINED ||
        m_pop3ConData->capability_flags & POP3_GURL_UNDEFINED ||
        m_pop3ConData->capability_flags & POP3_HAS_GURL)
        return SendStatOrGurl(PR_FALSE);
    else 
        return -1;
}


PRInt32
nsPop3Protocol::GurlResponse()
{
    if (POP3_GURL_UNDEFINED & m_pop3ConData->capability_flags)
        m_pop3ConData->capability_flags &= ~POP3_GURL_UNDEFINED;
    
    if (m_pop3ConData->command_succeeded) {
        m_pop3ConData->capability_flags |= POP3_HAS_GURL;
		// mscott - trust me, this cast to a char * IS SAFE!! There is a bug in 
		/// the xpidl file which is preventing SetMailAccountURL from taking
		// const char *. When that is fixed, we can remove this cast.
        if (m_nsIPop3Sink)
            m_nsIPop3Sink->SetMailAccountURL(m_commandResponse.get());
    }
    else {
        m_pop3ConData->capability_flags &= ~POP3_HAS_GURL;
    }
    m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    m_pop3ConData->next_state = POP3_SEND_QUIT;

    return 0;
}

PRInt32 nsPop3Protocol::SendList()
{
    m_pop3ConData->msg_info = (Pop3MsgInfo *) 
        PR_CALLOC(sizeof(Pop3MsgInfo) *	m_pop3ConData->number_of_messages);
    if (!m_pop3ConData->msg_info)
        return(MK_OUT_OF_MEMORY);
    m_pop3ConData->next_state_after_response = POP3_GET_LIST;
    return SendData(m_url, "LIST"CRLF);
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
        return(Error(POP3_LIST_FAILURE));

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(pauseForMoreData || !line)
    {
		m_pop3ConData->pause_for_read = PR_TRUE;
		PR_FREEIF(line);
        return(ln);
    }
    
    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

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
		PR_FREEIF(line);
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

	PR_FREEIF(line);
    return(0);
}

PRInt32 nsPop3Protocol::SendFakeUidlTop()
{
	char * cmd = PR_smprintf("TOP %ld 1" CRLF, m_pop3ConData->current_msg_to_top);
	PRInt32 status = -1;
	if (cmd)
	{
		m_pop3ConData->next_state_after_response = POP3_GET_FAKE_UIDL_TOP;
		m_pop3ConData->pause_for_read = PR_TRUE;
		status = SendData(m_url, cmd);
	}

	PR_FREEIF(cmd);
	return status;
}

PRInt32 nsPop3Protocol::StartUseTopForFakeUidl()
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


PRInt32 nsPop3Protocol::GetFakeUidlTop(nsIInputStream* inputStream, 
                               PRUint32 length)
{
    char * line, *newStr;
    PRUint32 ln = 0;

    /* check list response 
     * This will get called multiple times
     * but it's alright since command_succeeded
     * will remain constant
     */
    if(!m_pop3ConData->command_succeeded) 
	{
        nsresult rv;
        
        /* UIDL, XTND and TOP are all unsupported for this mail server.
           Tell the user to join the 20th century.
           
           Tell the user this, and refuse to download any messages until they've
           gone into preferences and turned off the `Keep Mail on Server' and
           `Maximum Message Size' prefs.  Some people really get their panties
           in a bunch if we download their mail anyway. (bug 11561)
           */
        
        // set up status first, so if the rest fails, state is ok
        m_pop3ConData->next_state = POP3_ERROR_DONE;
        m_pop3ConData->pause_for_read = PR_FALSE;
        
        // get the hostname first, convert to unicode
        nsXPIDLCString hostName;
        m_url->GetHost(getter_Copies(hostName));
        
        nsAutoString hostNameUnicode;
        hostNameUnicode.AssignWithConversion(hostName);
    
        const PRUnichar *formatStrings[] =
        {
            hostNameUnicode.get(),
        };

        // get the strings for the format
        nsCOMPtr<nsIStringBundle> bundle;
        rv = mStringService->GetBundle(getter_AddRefs(bundle));
        NS_ENSURE_SUCCESS(rv, -1);
        
        nsXPIDLString statusString;
        rv = bundle->FormatStringFromID(POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC,
                                        formatStrings, 1,
                                        getter_Copies(statusString));
        NS_ENSURE_SUCCESS(rv, -1);
        

        UpdateStatusWithString(statusString);

        return -1;
        
    }

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(pauseForMoreData || !line)
    {
		m_pop3ConData->pause_for_read = PR_TRUE;
		PR_FREEIF(line);
        return 0;
    }

    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

    if(!PL_strcmp(line, "."))
    {
        m_pop3ConData->current_msg_to_top--;
        if (!m_pop3ConData->current_msg_to_top || 
            (m_pop3ConData->found_new_message_boundary &&
             !m_pop3ConData->delete_server_message_during_top_traversal))
        {
            /* we either ran out of messages or reached the edge of new
               messages and no messages are marked dele */
			if (m_pop3ConData->only_check_for_new_mail)
			{
				m_pop3ConData->biffstate = nsIMsgFolder::nsMsgBiffState_NewMail;
				m_pop3ConData->next_state = POP3_SEND_QUIT;
			}
			else
			{
				m_pop3ConData->next_state = POP3_GET_MSG;
			}
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
            state = NS_PTR_TO_INT32(PL_HashTableLookup(m_pop3ConData->uidlinfo->hash, message_id_token));
            
            if (!m_pop3ConData->only_uidl && message_id_token && (state == 0))
            {	/* we have not seen this message before */
                
				m_pop3ConData->number_of_messages_not_seen_before++;
				m_pop3ConData->msg_info[m_pop3ConData->current_msg_to_top-1].uidl = 
					PL_strdup(message_id_token);
				if (!m_pop3ConData->msg_info[m_pop3ConData->current_msg_to_top-1].uidl)
				{
					PR_FREEIF(line);
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
								{
									PR_FREEIF(line);
                  return MK_OUT_OF_MEMORY;
								}
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

	PR_FREEIF(line);
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
PRInt32 nsPop3Protocol::SendXtndXlstMsgid()
{
    if ((m_pop3ConData->capability_flags & POP3_HAS_XTND_XLST) ||
        (m_pop3ConData->capability_flags & POP3_XTND_XLST_UNDEFINED))
    {
        m_pop3ConData->next_state_after_response = POP3_GET_XTND_XLST_MSGID;
        m_pop3ConData->pause_for_read = PR_TRUE;
        return SendData(m_url, "XTND XLST Message-Id" CRLF);
    }
    else
        return StartUseTopForFakeUidl();
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
    if (m_pop3ConData->capability_flags & POP3_XTND_XLST_UNDEFINED)
        m_pop3ConData->capability_flags &= ~POP3_XTND_XLST_UNDEFINED;

    if(!m_pop3ConData->command_succeeded) {
        m_pop3ConData->capability_flags &= ~POP3_HAS_XTND_XLST;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
        m_pop3ConData->next_state = POP3_START_USE_TOP_FOR_FAKE_UIDL;
        m_pop3ConData->pause_for_read = PR_FALSE;
        return(0);
    }
    else
    {
        m_pop3ConData->capability_flags |= POP3_HAS_XTND_XLST;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }        

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(pauseForMoreData || !line)
    {
		m_pop3ConData->pause_for_read = PR_TRUE;
		PR_FREEIF(line);
        return ln;
    }

    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

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
		PR_FREEIF(line);
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
		{
			PR_FREEIF(line);
            return MK_OUT_OF_MEMORY;
		}
    }
    
	PR_FREEIF(line);
    return(0);
}


PRInt32 nsPop3Protocol::SendUidlList()
{
    if ((m_pop3ConData->capability_flags & POP3_HAS_UIDL) ||
        (m_pop3ConData->capability_flags & POP3_UIDL_UNDEFINED))
    {
        m_pop3ConData->next_state_after_response = POP3_GET_UIDL_LIST;
        m_pop3ConData->pause_for_read = PR_TRUE;
        return SendData(m_url,"UIDL" CRLF);
    }
    else
        return SendXtndXlstMsgid();
}


PRInt32 nsPop3Protocol::GetUidlList(nsIInputStream* inputStream, 
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
    if (m_pop3ConData->capability_flags & POP3_UIDL_UNDEFINED)
        m_pop3ConData->capability_flags &= ~POP3_UIDL_UNDEFINED;

    if(!m_pop3ConData->command_succeeded) 
    {
        m_pop3ConData->next_state = POP3_SEND_XTND_XLST_MSGID;
        m_pop3ConData->pause_for_read = PR_FALSE;
        m_pop3ConData->capability_flags &= ~POP3_HAS_UIDL;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
        return(0);
    }
    else
    {
        m_pop3ConData->capability_flags |= POP3_HAS_UIDL;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }
    
    PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData);

    if(pauseForMoreData || !line)
    {
		PR_FREEIF(line);
		m_pop3ConData->pause_for_read = PR_TRUE;
        return ln;
    }

    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

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
		PR_FREEIF(line);
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
		{
			PR_FREEIF(line);
            return MK_OUT_OF_MEMORY;
		}
    }
    PR_FREEIF(line);
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

    if (m_totalDownloadSize < 0) {
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
            m_totalDownloadSize = 0;
            for (i=0 ; i < m_pop3ConData->number_of_messages ; i++) {
                c = 0;
                if (m_pop3ConData->only_uidl) {
                    if (m_pop3ConData->msg_info[i].uidl &&
                        PL_strcmp(m_pop3ConData->msg_info[i].uidl, 
                                  m_pop3ConData->only_uidl) == 0) {
			  /*if (m_pop3ConData->msg_info[i].size > m_pop3ConData->size_limit)
				  m_totalDownloadSize = m_pop3ConData->size_limit;	*/	/* if more than max, only count max */
			  /*else*/
                        m_totalDownloadSize =
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
                    c = (char) NS_PTR_TO_INT32(PL_HashTableLookup(m_pop3ConData->uidlinfo->hash,
                                                m_pop3ConData->msg_info[i].uidl));
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
                      m_totalDownloadSize +=
                      m_pop3ConData->size_limit;	*/	
                    /* if more than max, only count max */
                    /*else*/
                    m_totalDownloadSize +=
                        m_pop3ConData->msg_info[i].size; 
                    m_pop3ConData->really_new_messages++;		
                    /* a message we will really download */
                }
            }
        } else {
            m_totalDownloadSize = m_totalFolderSize;
        }
        if (m_pop3ConData->only_check_for_new_mail) {
            if (m_totalDownloadSize > 0)
                m_pop3ConData->biffstate = nsIMsgFolder::nsMsgBiffState_NewMail; 
            m_pop3ConData->next_state = POP3_SEND_QUIT;
            return(0);
        }
        /* get the amount of available space on the drive
         * and make sure there is enough
         */	
        if(m_totalDownloadSize > 0) // skip all this if there aren't any messages
        {
			      nsresult rv;
            PRInt64 mailboxSpaceLeft = LL_Zero();
            nsCOMPtr <nsIMsgFolder> folder;
            nsCOMPtr <nsIFileSpec> path;

            // Get the path to the current mailbox
            // 	
            NS_ENSURE_TRUE(m_nsIPop3Sink, NS_ERROR_UNEXPECTED); 
            rv = m_nsIPop3Sink->GetFolder(getter_AddRefs(folder));
			      if (NS_FAILED(rv)) return rv;
            rv = folder->GetPath(getter_AddRefs(path));
            if (NS_FAILED(rv)) return rv;
                
			      // call GetDiskSpaceAvailable
            rv = path->GetDiskSpaceAvailable(&mailboxSpaceLeft);
            if (NS_FAILED(rv))
            {
            	// The call to GetDiskSpaceAvailable FAILED!
            	// This will happen on certain platforms where GetDiskSpaceAvailable
            	// is not implimented. Since people on those platforms still need
            	// to check mail, we will simply bypass the disk-space check.
            	// 
            	// We'll leave a debug message to warn people.

                #ifdef DEBUG
                printf("Call to GetDiskSpaceAvailable FAILED! \n");
                #endif
            }
            else
            {
				#ifdef DEBUG
				printf("GetDiskSpaceAvailable returned: %d bytes\n", mailboxSpaceLeft);
				#endif

            	// Original comment from old implimentation follows...
            	/* When checking for disk space available, take in consideration
             	* possible database 
             	* changes, therefore ask for a little more than what the message
             	* size is. Also, due to disk sector sizes, allocation blocks,
             	* etc. The space "available" may be greater than the actual space
             	* usable. */

            	// The big if statement            	
            	PRInt64 llResult;
            	PRInt64 llExtraSafetySpace;
            	PRInt64 llTotalDownloadSize;
            	LL_I2L(llExtraSafetySpace, EXTRA_SAFETY_SPACE);
            	LL_I2L(llTotalDownloadSize, m_totalDownloadSize);
            	
            	LL_ADD(llResult, llTotalDownloadSize, llExtraSafetySpace);
            	if (LL_CMP(llResult, >, mailboxSpaceLeft))            	
            	{
            		// Not enough disk space!
					#ifdef DEBUG
					printf("Not enough disk space! Raising error! \n");
					#endif
            		// Should raise an error at this point.
            		// First, we need to delete our references to the two interfaces..
            		return MK_POP3_OUT_OF_DISK_SPACE;
            	}
            }
			  // Delete our references to the two interfaces..
       }
    }
    
    
    /* Look at this message, and decide whether to ignore it, get it, just get
       the TOP of it, or delete it. */
    
    m_pop3Server->GetAuthLogin(&prefBool);

    if (prefBool && (m_pop3ConData->capability_flags & POP3_HAS_XSENDER ||
                     m_pop3ConData->capability_flags & POP3_XSENDER_UNDEFINED))
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
                c = (char) NS_PTR_TO_INT32(PL_HashTableLookup(m_pop3ConData->uidlinfo->hash,
                                            info->uidl));
            }
            m_pop3ConData->truncating_cur_msg = PR_FALSE;
            if (c == DELETE_CHAR) {
                m_pop3ConData->next_state = POP3_SEND_DELE;
            } else if (c == KEEP) {
                m_pop3ConData->next_state = POP3_GET_MSG;
            } else if ((c != TOO_BIG) && (m_pop3ConData->size_limit > 0) &&
                       (info->size > m_pop3ConData->size_limit) && 
                       (m_pop3ConData->capability_flags & POP3_TOP_UNDEFINED
                        || (m_pop3ConData->capability_flags & POP3_HAS_TOP)) &&
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
        if ((m_pop3ConData->next_state != POP3_SEND_DELE) || 
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
	char * cmd = PR_smprintf( "TOP %ld 20" CRLF,
                              m_pop3ConData->last_accessed_msg+1);
	PRInt32 status = -1;
	if (cmd)
	{
		m_pop3ConData->next_state_after_response = POP3_TOP_RESPONSE;    
		m_pop3ConData->cur_msg_size = -1;

		/* zero the bytes received in message in preparation for
		* the next
		*/
		m_bytesInMsgReceived = 0;
		status = SendData(m_url,cmd);
	}
	PR_FREEIF(cmd);
	return status;
}

/* send the xsender command
 */
PRInt32 nsPop3Protocol::SendXsender()
{
	char * cmd = PR_smprintf("XSENDER %ld" CRLF, m_pop3ConData->last_accessed_msg+1);
	PRInt32 status = -1;
	if (cmd)
	{  
		m_pop3ConData->next_state_after_response = POP3_XSENDER_RESPONSE;
		status = SendData(m_url, cmd);
	}
	PR_FREEIF(cmd);
	return status;
}

PRInt32 nsPop3Protocol::XsenderResponse()
{
    m_pop3ConData->seenFromHeader = PR_FALSE;
	m_senderInfo = "";
    
    if (POP3_XSENDER_UNDEFINED & m_pop3ConData->capability_flags)
        m_pop3ConData->capability_flags &= ~POP3_XSENDER_UNDEFINED;

    if (m_pop3ConData->command_succeeded) {
        if (m_commandResponse.Length() > 4)
        {
			m_senderInfo = m_commandResponse;
        }
        if (! (POP3_HAS_XSENDER & m_pop3ConData->capability_flags))
            m_pop3ConData->capability_flags |= POP3_HAS_XSENDER;
    }
    else {
        m_pop3ConData->capability_flags &= ~POP3_HAS_XSENDER;
    }
    m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);

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

   	char * cmd = PR_smprintf("RETR %ld" CRLF, m_pop3ConData->last_accessed_msg+1);
	PRInt32 status = -1;
	if (cmd)
	{
		m_pop3ConData->next_state_after_response = POP3_RETR_RESPONSE;    
		m_pop3ConData->cur_msg_size = -1;


		/* zero the bytes received in message in preparation for
		* the next
		*/
		m_bytesInMsgReceived = 0;
    
		if (m_pop3ConData->only_uidl)
		{
			/* Display bytes if we're only downloading one message. */
			PR_ASSERT(!m_pop3ConData->graph_progress_bytes_p);
			UpdateProgressPercent(0, m_totalDownloadSize);
#if 0
			if (!m_pop3ConData->graph_progress_bytes_p)
				FE_GraphProgressInit(ce->window_id, ce->URL_s,
                                 m_totalDownloadSize);
#endif 
			m_pop3ConData->graph_progress_bytes_p = PR_TRUE;
		}
		else
		{
            nsresult rv;
            
            nsAutoString realNewString;
            realNewString.AppendInt(m_pop3ConData->real_new_counter);

            nsAutoString reallyNewMessages;
            reallyNewMessages.AppendInt(m_pop3ConData->really_new_messages);

            nsCOMPtr<nsIStringBundle> bundle;
            rv = mStringService->GetBundle(getter_AddRefs(bundle));
            NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't get bundle");

            if (bundle)
            {
              const PRUnichar *formatStrings[] = {
                  realNewString.get(),
                  reallyNewMessages.get(),
              };

              nsXPIDLString finalString;
              rv = bundle->FormatStringFromID(LOCAL_STATUS_RECEIVING_MESSAGE_OF,
                                              formatStrings, 2,
                                              getter_Copies(finalString));
              NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't format string");

              if (m_statusFeedback)
                m_statusFeedback->ShowStatusString(finalString);
            }
            
		}

		status = SendData(m_url, cmd);
	} // if cmd
	PR_FREEIF(cmd);
    return status;
}

/* digest the message
 */
PRInt32
nsPop3Protocol::RetrResponse(nsIInputStream* inputStream, 
                             PRUint32 length)
{
    PRUint32 buffer_size;
    PRInt32 flags = 0;
    char *uidl = NULL;
    char *newStr;
    nsresult rv;
#if 0
    PRInt32 old_bytes_received = m_totalBytesReceived;
#endif
    PRUint32 status = 0;
	
    if(m_pop3ConData->cur_msg_size == -1)
    {
        /* this is the beginning of a message
         * get the response code and byte size
         */
        if(!m_pop3ConData->command_succeeded)
            return Error(POP3_RETR_FAILURE);
        
        /* a successful RETR response looks like: #num_bytes Junk
           from TOP we only get the +OK and data
           */
        if (m_pop3ConData->truncating_cur_msg)
        { /* TOP, truncated message */
            m_pop3ConData->cur_msg_size = m_pop3ConData->size_limit;
            flags |= MSG_FLAG_PARTIAL;
        }
        else
		{
			char * oldStr = ToNewCString(m_commandResponse);
	        m_pop3ConData->cur_msg_size =
                atol(nsCRT::strtok(oldStr, " ", &newStr));
          m_commandResponse = newStr;
			PR_FREEIF(oldStr);
		}
        /* RETR complete message */

        if (!m_senderInfo.IsEmpty())
            flags |= MSG_FLAG_SENDER_AUTHED;
        
        if(m_pop3ConData->cur_msg_size < 0)
            m_pop3ConData->cur_msg_size = 0;

        if (m_pop3ConData->msg_info && 
            m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].uidl)
            uidl = m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].uidl;

        m_pop3ConData->parsed_bytes = 0;
        m_pop3ConData->pop3_size = m_pop3ConData->cur_msg_size;
        m_pop3ConData->assumed_end = PR_FALSE;
        
        m_pop3Server->GetDotFix(&m_pop3ConData->dot_fix);
		

        PR_LOG(POP3LOGMODULE,PR_LOG_ALWAYS, 
               ("Opening message stream: MSG_IncorporateBegin"));

        /* open the message stream so we have someplace
         * to put the data
         */
        m_pop3ConData->real_new_counter++;		
        /* (rb) count only real messages being downloaded */
        rv = m_nsIPop3Sink->IncorporateBegin(uidl, m_url, flags,
                                        &m_pop3ConData->msg_closure); 

        PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS, ("Done opening message stream!"));
													
        if(!m_pop3ConData->msg_closure || NS_FAILED(rv))
            return(Error(POP3_MESSAGE_WRITE_ERROR));
    }
    
    m_pop3ConData->pause_for_read = PR_TRUE;

	PRBool pauseForMoreData = PR_FALSE;
	char * line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);
    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));
	buffer_size = status;
    
    if(status == 0 && !line)  // no bytes read in...
    {
        if (m_pop3ConData->dot_fix && m_pop3ConData->assumed_end && m_pop3ConData->msg_closure)
        {
          nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
          nsCOMPtr<nsIMsgWindow> msgWindow;
          if (NS_SUCCEEDED(rv))
            rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));

            rv =
                m_nsIPop3Sink->IncorporateComplete(msgWindow);
            // The following was added to prevent the loss of Data when we try
            // and write to somewhere we dont have write access error to (See 
            // bug 62480)
            // (Note: This is only a temp hack until the underlying XPCOM is
            // fixed to return errors)
            
            if(NS_FAILED(rv))
                return(Error(POP3_MESSAGE_WRITE_ERROR));

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
		// buffer the line we just read in, and buffer all remaining lines in the stream
		status = buffer_size;
		do
		{
			PRInt32 res = BufferInput(line, buffer_size);
            if (res < 0) return(Error(POP3_MESSAGE_WRITE_ERROR));
			// BufferInput(CRLF, 2);
            res = BufferInput(MSG_LINEBREAK, MSG_LINEBREAK_LEN);
            if (res < 0) return(Error(POP3_MESSAGE_WRITE_ERROR));

            m_pop3ConData->parsed_bytes += (buffer_size+2); // including CRLF
    
			// now read in the next line
			PR_FREEIF(line);
		    line = m_lineStreamBuffer->ReadNextLine(inputStream, buffer_size,
                                                    pauseForMoreData);
            PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));
			status += (buffer_size+2); // including CRLF
		} while (/* !pauseForMoreData && */ line);
    }

	buffer_size = status;  // status holds # bytes we've actually buffered so far...
  
    /* normal read. Yay! */
    if ((PRInt32) (m_bytesInMsgReceived + buffer_size) >
        m_pop3ConData->cur_msg_size) 
        buffer_size = m_pop3ConData->cur_msg_size -
            m_bytesInMsgReceived; 
    
    m_bytesInMsgReceived += buffer_size;
    m_totalBytesReceived            += buffer_size;

    // *** jefft in case of the message size that server tells us is different
    // from the actual message size
    if (pauseForMoreData && m_pop3ConData->dot_fix &&
        m_pop3ConData->assumed_end && m_pop3ConData->msg_closure)
    {
        nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
        nsCOMPtr<nsIMsgWindow> msgWindow;
        if (NS_SUCCEEDED(rv))
          rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
        rv = 
            m_nsIPop3Sink->IncorporateComplete(msgWindow);

        // The following was added to prevent the loss of Data when we try
        // and write to somewhere we dont have write access error to (See
        // bug 62480)
        // (Note: This is only a temp hack until the underlying XPCOM is
        // fixed to return errors)

        if(NS_FAILED(rv))
            return(Error(POP3_MESSAGE_WRITE_ERROR));

        m_pop3ConData->msg_closure = 0;
    }
    
    if (!m_pop3ConData->msg_closure) 
        /* meaning _handle_line read ".\r\n" at end-of-msg */
    {
        m_pop3ConData->pause_for_read = PR_FALSE;
        if (m_pop3ConData->truncating_cur_msg ||
            m_pop3ConData->leave_on_server )
        {
            /* We've retrieved all or part of this message, but we want to
               keep it on the server.  Go on to the next message. */
            m_pop3ConData->last_accessed_msg++;
            m_pop3ConData->next_state = POP3_GET_MSG;
        } else
        {
            m_pop3ConData->next_state = POP3_SEND_DELE;
        }
        
        /* if we didn't get the whole message add the bytes that we didn't get
           to the bytes received part so that the progress percent stays sane.
           */
        if(m_bytesInMsgReceived < m_pop3ConData->cur_msg_size)
            m_totalBytesReceived += (m_pop3ConData->cur_msg_size -
                                   m_bytesInMsgReceived);
    }

    /* set percent done to portion of total bytes of all messages
       that we're going to download. */
    if (m_totalDownloadSize)
		UpdateProgressPercent(m_totalBytesReceived, m_totalDownloadSize);

	PR_FREEIF(line);    
    return(0);
}


PRInt32
nsPop3Protocol::TopResponse(nsIInputStream* inputStream, PRUint32 length)
{
    if (m_pop3ConData->capability_flags & POP3_TOP_UNDEFINED)
    {
        m_pop3ConData->capability_flags &= ~POP3_TOP_UNDEFINED;
        if (m_pop3ConData->command_succeeded)
            m_pop3ConData->capability_flags |= POP3_HAS_TOP;
        else
            m_pop3ConData->capability_flags &= ~POP3_HAS_TOP;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }
    
    if(m_pop3ConData->cur_msg_size == -1 &&  /* first line after TOP command sent */
       !m_pop3ConData->command_succeeded)	/* and TOP command failed */
    {
        /* TOP doesn't work so we can't retrieve the first part of this msg.
           So just go download the whole thing, and warn the user.
           
           Note that the progress bar will not be accurate in this case.
           Oops. #### */
        PRBool prefBool = PR_FALSE;
        m_pop3ConData->truncating_cur_msg = PR_FALSE;

		PRUnichar * statusTemplate = nsnull;
    mStringService->GetStringByID(POP3_SERVER_DOES_NOT_SUPPORT_THE_TOP_COMMAND, &statusTemplate);
		if (statusTemplate)
		{
			nsXPIDLCString hostName;
			PRUnichar * statusString = nsnull;
			m_url->GetHost(getter_Copies(hostName));

			if (hostName)
				statusString = nsTextFormatter::smprintf(statusTemplate, (const char *) hostName);  
			else
				statusString = nsTextFormatter::smprintf(statusTemplate, "(null)"); 
			UpdateStatusWithString(statusString);
			nsTextFormatter::smprintf_free(statusString);
			nsCRT::free(statusTemplate);
		}

        m_pop3Server->GetAuthLogin(&prefBool);

        if (prefBool && 
            (POP3_XSENDER_UNDEFINED & m_pop3ConData->capability_flags ||
             POP3_HAS_XSENDER & m_pop3ConData->capability_flags))
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
    nsresult rv;
    
    NS_ASSERTION(m_pop3ConData->msg_closure, "m_pop3ConData->msg_closure is null in nsPop3Protocol::HandleLine()");
    if (!m_pop3ConData->msg_closure)
        return -1;
    
    if (!m_senderInfo.IsEmpty() && !m_pop3ConData->seenFromHeader)
    {
        if (line_length > 6 && !PL_strncasecmp("From: ", line, 6))
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
            if (PL_strstr(line, m_senderInfo.get()) == NULL)
                m_nsIPop3Sink->SetSenderAuthedFlag(m_pop3ConData->msg_closure,
                                                     PR_FALSE);
            line[line_length-1] = ch;
        }
    }

    if ((line[0] == '.') &&
        ((line[1] == nsCRT::CR) || (line[1] == nsCRT::LF)))
    {
        m_pop3ConData->assumed_end = PR_TRUE;	/* in case byte count from server is */
                                    /* wrong, mark we may have had the end */ 
        if (!m_pop3ConData->dot_fix || m_pop3ConData->truncating_cur_msg ||
            (m_pop3ConData->parsed_bytes >= (m_pop3ConData->pop3_size -3))) 
        {
          nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
          nsCOMPtr<nsIMsgWindow> msgWindow;
          if (NS_SUCCEEDED(rv))
            rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
          rv = m_nsIPop3Sink->IncorporateComplete(msgWindow);

            // The following was added to prevent the loss of Data when we try
            // and write to somewhere we dont have write access error to (See
            // bug 62480)
            // (Note: This is only a temp hack until the underlying XPCOM is
            // fixed to return errors)

            if(NS_FAILED(rv))
                return(Error(POP3_MESSAGE_WRITE_ERROR));

            m_pop3ConData->msg_closure = 0;
            return 0;
        }
    }
      /*When examining a multi-line response, the client checks
      to see if the line begins with the termination octet.  If so and if
      octets other than CRLF follow, the first octet of the line (the
      termination octet) is stripped away.*/
    else if (line_length > 1  && line[0] == '.' && line[1] == '.' ) 
    {
        PRUint32 i=0;
        while ( i < line_length -1 ){
           line[i] = line[i+1];
           i++;
        }
        line[i] = '\0';
        line_length -= 1;
    
    }
    rv = m_nsIPop3Sink->IncorporateWrite(line, line_length);
    if(NS_FAILED(rv))
      return(Error(POP3_MESSAGE_WRITE_ERROR));
    
    return 0;
}

PRInt32 nsPop3Protocol::SendDele()
{
    /* increment the last accessed message since we have now read it
     */
    m_pop3ConData->last_accessed_msg++;
    char * cmd = PR_smprintf("DELE %ld" CRLF, m_pop3ConData->last_accessed_msg);
	PRInt32 status = -1;
	if (cmd)
	{
   		m_pop3ConData->next_state_after_response = POP3_DELE_RESPONSE;
		status = SendData(m_url, cmd);
	}
	PR_FREEIF(cmd);
	return status;
}

PRInt32 nsPop3Protocol::DeleResponse()
{
    Pop3UidlHost *host = NULL;
	
		host = m_pop3ConData->uidlinfo;

    /* the return from the delete will come here
     */
    if(!m_pop3ConData->command_succeeded)
        return(Error(POP3_DELE_FAILURE));
    
    
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
              if (m_pop3ConData->leave_on_server)
                PL_HashTableRemove(m_pop3ConData->newuidl, (void*)
                  m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg-1].uidl); 
              else
                PL_HashTableAdd(m_pop3ConData->newuidl, (void*)
                  m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg-1].uidl, (void*)DELETE_CHAR);   
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
        if (m_pop3ConData->msg_info && m_pop3ConData->last_accessed_msg < m_pop3ConData->number_of_messages) {
            Pop3MsgInfo* info = m_pop3ConData->msg_info +
                m_pop3ConData->last_accessed_msg; 
            if (info && info->uidl && (m_pop3ConData->only_uidl == NULL) &&
                m_pop3ConData->newuidl && m_pop3ConData->newuidl->nentries > 0) 
            {  
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
        nsresult rv;
        nsCOMPtr<nsIFileSpec> mailDirectory;

        // get the mail directory
        nsCOMPtr<nsIMsgIncomingServer> server =
            do_QueryInterface(m_pop3Server, &rv);
        if (NS_FAILED(rv)) return -1;
                
        rv = server->GetLocalPath(getter_AddRefs(mailDirectory));
        if (NS_FAILED(rv)) return -1;

        // write the state in the mail directory
        net_pop3_write_state(m_pop3ConData->uidlinfo,
                             mailDirectory);
        
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
nsresult nsPop3Protocol::ProcessProtocolState(nsIURI * url, nsIInputStream * aInputStream, 
									      PRUint32 sourceOffset, PRUint32 aLength)
{
    PRInt32 status = 0;
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_url);

    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS, ("Entering NET_ProcessPop3 %d",
                                          aLength));

    m_pop3ConData->pause_for_read = PR_FALSE; /* already paused; reset */
    
    if(m_username.IsEmpty())
    {
        // net_pop3_block = PR_FALSE;
        return(Error(POP3_USERNAME_UNDEFINED));
    }

    while(!m_pop3ConData->pause_for_read)
    {
        PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS, 
               ("POP3: Entering state: %d", m_pop3ConData->next_state));

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
			nsXPIDLCString password;
            PRBool okayValue;
			GetPassword(getter_Copies(password), &okayValue);
			const char * pwd = (const char *) password;
            if (!password || m_username.IsEmpty()) 
            {
                status = MK_POP3_PASSWORD_UNDEFINED;
                m_pop3ConData->biffstate = nsIMsgFolder::nsMsgBiffState_Unknown;
                m_nsIPop3Sink->SetBiffStateAndUpdateFE(m_pop3ConData->biffstate, 0);	

                /* update old style biff */
                m_pop3ConData->next_state = POP3_FREE;
                m_pop3ConData->pause_for_read = PR_FALSE;
                break;
            }

#if 0
            PR_ASSERT(net_pop3_username);
            if (TestFlag(POP3_PASSWORD_FAILED))
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
                                 (m_commandResponse.Length()
                                  ? m_commandResponse.get()
                                  : XP_GetString(XP_NO_ANSWER)),
                                 net_pop3_username, host);
#if defined(CookiesAndSignons)                                        
                NS_MsgSACopy(&usernameAndHost, net_pop3_username);
                NS_MsgSACat(&usernameAndHost, "@");
                NS_MsgSACat(&usernameAndHost, host);
                PR_FREEIF (host);
                
                password = SI_PromptPassword
                    (ce->window_id,
                     prompt, usernameAndHost,
                     nsIPrompt::SAVE_PASSWORD_PERMANENTLY, !TestFlag(POP3_PASSWORD_FAILED));
				ClearFlag(POP3_PASSWORD_FAILED);
                PR_Free(usernameAndHost);
#else
                PR_FREEIF (host);
                ClearFlag(POP3_PASSWORD_FAILED);
                password = FE_PromptPassword
                    (ce->window_id, prompt);
#endif
                PR_Free(prompt);
                
                if (password == NULL)
                {
                    net_pop3_block = PR_FALSE;
                    return NS_OK;
                }
                
                net_set_pop3_password(password);
                memset(password, 0, PL_strlen(password));
                PR_Free(password);
                
                net_pop3_password_pending = PR_TRUE;
            }
#endif 
           if (m_username.IsEmpty() || !pwd )
           {
                m_pop3ConData->next_state = POP3_ERROR_DONE;
                m_pop3ConData->pause_for_read = PR_FALSE;
           }
           else
           {
              //we are already connected so just go on and send the username
              PRBool prefBool = PR_FALSE; 
              m_pop3ConData->pause_for_read = PR_FALSE;
              m_pop3Server->GetAuthLogin(&prefBool);
            
              if (prefBool) 
              {
                if (m_pop3ConData->capability_flags & POP3_AUTH_LOGIN_UNDEFINED)
                  m_pop3ConData->next_state = POP3_SEND_AUTH;
                else if (m_pop3ConData->capability_flags & POP3_HAS_AUTH_LOGIN)
                  m_pop3ConData->next_state = POP3_AUTH_LOGIN;
                else
                  m_pop3ConData->next_state = POP3_SEND_USERNAME;
              }
              else
                m_pop3ConData->next_state = POP3_SEND_USERNAME;
           }
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
            
            m_pop3ConData->pause_for_read = PR_FALSE;
			// m_pop3ConData->next_state = POP3_SEND_USERNAME;
            m_pop3ConData->next_state =
                POP3_WAIT_FOR_START_OF_CONNECTION_RESPONSE;

            m_pop3Server->GetAuthLogin(&prefBool);
            
            if (prefBool) {
                if (m_pop3ConData->capability_flags & POP3_AUTH_LOGIN_UNDEFINED)
                    m_pop3ConData->next_state_after_response = POP3_SEND_AUTH;
                else if (m_pop3ConData->capability_flags & POP3_HAS_AUTH_LOGIN)
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
			UpdateStatus(POP3_CONNECT_HOST_CONTACTED_SENDING_LOGIN_INFORMATION);
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
                    m_nsIPop3Sink->SetBiffStateAndUpdateFE(m_pop3ConData->biffstate, m_pop3ConData->number_of_messages_not_seen_before);	
                    /* update old style biff */
                else 
                {
                    /* We don't want to pop up a warning message any more (see
                       bug 54116), so instead we put the "no new messages" or
                       "retrieved x new messages" 
                       in the status line.  Unfortunately, this tends to be running
                       in a progress pane, so we try to get the real pane and
                       show the message there. */

                    if (m_totalDownloadSize <= 0) 
					{
						UpdateStatus(POP3_NO_MESSAGES);
                        /* There are no new messages.  */
                    }
                    else 
					{
						PRUnichar * statusTemplate = nsnull;
            mStringService->GetStringByID(POP3_DOWNLOAD_COUNT, &statusTemplate);
						if (statusTemplate)
						{
							PRUnichar * statusString = nsTextFormatter::smprintf(statusTemplate, 
                              m_pop3ConData->real_new_counter - 1,
                              m_pop3ConData->really_new_messages);  
							UpdateStatusWithString(statusString);
							nsTextFormatter::smprintf_free(statusString);
							nsCRT::free(statusTemplate);

						}
                        m_nsIPop3Sink->SetBiffStateAndUpdateFE(nsIMsgFolder::nsMsgBiffState_NewMail, m_pop3ConData->number_of_messages_not_seen_before);
                    }
                }
            }
			
			status = SendData(mailnewsurl, "QUIT" CRLF);
            m_pop3ConData->next_state = POP3_WAIT_FOR_RESPONSE;
            m_pop3ConData->next_state_after_response = POP3_QUIT_RESPONSE;
            break;

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
              //delete the uidl because deletes are committed
              if (!m_pop3ConData->leave_on_server && m_pop3ConData->newuidl){  
                PL_HashTableEnumerateEntries(m_pop3ConData->newuidl, 
    								net_pop3_remove_messages_marked_delete,
    								(void *)m_pop3ConData);   
              }


                m_pop3ConData->next_state = POP3_DONE;

            }
            else
            {
                m_pop3ConData->next_state = POP3_ERROR_DONE;
            }
            break;
            
        case POP3_DONE:
            CommitState(PR_FALSE);
            
            if(m_pop3ConData->msg_del_started)
                m_nsIPop3Sink->EndMailDelivery();

			if (mailnewsurl)
				mailnewsurl->SetUrlState(PR_FALSE, NS_OK);

            m_pop3ConData->next_state = POP3_FREE;
            break;

        case POP3_INTERRUPTED:
			SendData(mailnewsurl, "QUIT" CRLF);
            m_pop3ConData->pause_for_read = PR_FALSE;
            m_pop3ConData->next_state = POP3_ERROR_DONE;
			break;
        
        case POP3_ERROR_DONE:

            /*  write out the state */
            CommitState(PR_TRUE);
				
            if(m_pop3ConData->msg_closure)
            {
                m_nsIPop3Sink->IncorporateAbort(m_pop3ConData->only_uidl != nsnull);
                m_pop3ConData->msg_closure = NULL;
                m_nsIPop3Sink->AbortMailDelivery();
            }

           
            if(m_pop3ConData->msg_del_started)
            {

				PRUnichar * statusTemplate = nsnull;
        mStringService->GetStringByID(POP3_DOWNLOAD_COUNT, &statusTemplate);
				if (statusTemplate)
				{
					PRUnichar * statusString = nsTextFormatter::smprintf(statusTemplate, 
                             m_pop3ConData->real_new_counter - 1,
                              m_pop3ConData->really_new_messages);  
					UpdateStatusWithString(statusString);
					nsTextFormatter::smprintf_free(statusString);
					nsCRT::free(statusTemplate);

				}

                PR_ASSERT (!TestFlag(POP3_PASSWORD_FAILED));
                m_nsIPop3Sink->AbortMailDelivery();
            }

            if (TestFlag(POP3_PASSWORD_FAILED))
			{
                /* We got here because the password was wrong, so go
                   read a new one and re-open the connection. */
                m_pop3ConData->next_state = POP3_READ_PASSWORD;
				m_pop3ConData->command_succeeded = PR_TRUE;
				status = 0;
				break;
			}
            else
                /* Else we got a "real" error, so finish up. */
                m_pop3ConData->next_state = POP3_FREE;       

			if (mailnewsurl)
				mailnewsurl->SetUrlState(PR_FALSE, NS_ERROR_FAILURE);

            m_pop3ConData->pause_for_read = PR_FALSE;
            break;
            
        case POP3_FREE:
			UpdateProgressPercent(0,0); // clear out the progress meter
			if (m_nsIPop3Sink)
			{
				nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
				if (server)
					server->SetServerBusy(PR_FALSE); // the server is now not busy
			}

			CloseSocket();
            return NS_OK;
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
    
    return NS_OK;
    
}

nsresult nsPop3Protocol::CloseSocket()
{
	nsresult rv = nsMsgProtocol::CloseSocket();
	m_url = nsnull;
	return rv;
}
