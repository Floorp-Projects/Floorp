/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
*/

#include "msgCore.h"
#include "nsLocalUtils.h"
#include "nsIServiceManager.h"
#include "prsystem.h"
#include "nsCOMPtr.h"

// stuff for temporary root folder hack
#include "nsIMsgMailSession.h"
#include "nsIMsgIncomingServer.h"
#include "nsIPop3IncomingServer.h"
#include "nsINoIncomingServer.h"
#include "nsMsgBaseCID.h"

#include "nsMsgUtils.h"


static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

// it would be really cool to:
// - cache the last hostname->path match
// - if no such server exists, behave like an old-style mailbox URL
// (i.e. return the mail.directory preference or something)
static nsresult
nsGetMailboxServer(const char *username, const char *hostname, nsIMsgIncomingServer** aResult)
{
  nsresult rv = NS_OK;

  // retrieve the AccountManager
  NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIMsgAccountManager> accountManager;
  rv = session->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) return rv;

  // find all local mail "no servers" matching the given hostname
  nsCOMPtr<nsIMsgIncomingServer> none_server;
  rv = accountManager->FindServer(username,
                                  hostname,
                                  "none",
                                  getter_AddRefs(none_server));
  if (NS_SUCCEEDED(rv)) {
	 *aResult = none_server;
	  NS_ADDREF(*aResult);
	  return rv;
  }

  // if that fails, look for the pop hosts matching the given hostname
  nsCOMPtr<nsIMsgIncomingServer> pop3_server;
  if (NS_FAILED(rv)) {
	rv = accountManager->FindServer(username,
                                  hostname,
                                  "pop3",
                                  getter_AddRefs(pop3_server));
  }
  if (NS_SUCCEEDED(rv)) {
	 *aResult = pop3_server;
	  NS_ADDREF(*aResult);
	  return rv;
  }

  if (NS_FAILED(rv)) return rv;
}

nsresult
nsLocalURI2Server(const char* uriStr,
                  nsIMsgIncomingServer ** aResult)
{

  nsresult rv;

  // start parsing the uriStr
  const char* curPos = uriStr;
  
  // skip past schema xxx://
  while (*curPos != ':') curPos++;
  curPos++;
  while (*curPos == '/') curPos++;

  // extract userid from userid@hostname...
  // this is so amazingly ugly, please forgive me....
  // I'll fix this post-M7 -alecf@netscape.com
  char *atPos = PL_strchr(curPos, '@');
  NS_ASSERTION(atPos!=nsnull, "URI with no userid!");
  
  int length;
  if (atPos)
    length = (atPos - curPos) + 1;
  else {
    length = 1;
  }

  char *username = new char[length];
  if (!username) return NS_ERROR_OUT_OF_MEMORY;

  if (atPos) {
    PL_strncpyz(username, curPos, length);
    curPos = atPos;
    curPos++;
  }    
  else
    username[0] = '\0';

  // extract hostname
  char *slashPos = PL_strchr(curPos, '/');

  // if there are no more /'s then we just copy the rest of the string
  if (slashPos)
    length = (slashPos - curPos) + 1;
  else
    length = PL_strlen(curPos) + 1;

  char* hostname = new char[length];
  if(!hostname)
	  return NS_ERROR_OUT_OF_MEMORY;

  PL_strncpyz(hostname, curPos, length);

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = nsGetMailboxServer(username, hostname, getter_AddRefs(server));
  delete[] username;
  delete[] hostname;

  *aResult = server;
  NS_IF_ADDREF(*aResult);

  return rv;
}

// given rootURI and rootURI##folder, return on-disk path of folder
nsresult
nsLocalURI2Path(const char* rootURI, const char* uriStr,
                nsFileSpec& pathResult)
{
  nsresult rv;
  
  // verify that rootURI starts with "mailbox:/" or "mailbox_message:/"
  if ((PL_strcmp(rootURI, kMailboxRootURI) != 0) && 
      (PL_strcmp(rootURI, kMailboxMessageRootURI) != 0)) {
    pathResult = nsnull;
    return NS_ERROR_FAILURE;
  }

  // verify that uristr starts with rooturi
  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)
    return NS_ERROR_FAILURE;


  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = nsLocalURI2Server(uriStr, getter_AddRefs(server));
  
  if (NS_FAILED(rv))
  	return rv;
  
  // now ask the server what it's root is
  // and begin pathResult with the mailbox root
  char *localPath;
  rv = server->GetLocalPath(&localPath);
  if (NS_SUCCEEDED(rv)) 
    pathResult = localPath;

  if (localPath) PL_strfree(localPath);

  const char *curPos = uriStr + PL_strlen(rootURI);
  if (curPos) {
    
    // advance past hostname
    while ((*curPos)=='/') curPos++;
    while (*curPos && (*curPos)!='/') curPos++;
    while ((*curPos)=='/') curPos++;

    // get the seperator
    nsAutoString sbdSep;
    rv = nsGetMailFolderSeparator(sbdSep);
    
    // for each token in between the /'s, put a .sbd on the end and
    // append to the path
    char *newStr=nsnull;
    char *temp = PL_strdup(curPos);
    char *token = nsCRT::strtok(temp, "/", &newStr);
    while (token) {
      nsCAutoString dir(token);
      
      // look for next token
      token = nsCRT::strtok(newStr, "/", &newStr);
      
      // check if we're the last entry
      if (token) {
        NS_MsgHashIfNecessary(dir);
        dir += sbdSep;            // no, we're not, so append .sbd
        pathResult += (const char *) dir;
      }
      else {
        NS_MsgHashIfNecessary(dir);
        pathResult += (const char *) dir;
      }
    }
    PL_strfree(temp);
  }
  return NS_OK;
}

/* parses LocalMessageURI
 * mailbox://folder1/folder2#123
 *
 * puts folder path in folderURI
 * message key number in key
 */
nsresult nsParseLocalMessageURI(const char* uri,
                                nsCString& folderURI,
                                PRUint32 *key)
{
	if(!key)
		return NS_ERROR_NULL_POINTER;

	nsCAutoString uriStr = uri;
	PRInt32 keySeparator = uriStr.FindChar('#');
	if(keySeparator != -1)
	{
		nsAutoString folderPath;
		uriStr.Left(folderURI, keySeparator);

		nsCAutoString keyStr;
		uriStr.Right(keyStr, uriStr.Length() - (keySeparator + 1));
		PRInt32 errorCode;
		*key = keyStr.ToInteger(&errorCode);

		return errorCode;
	}
	return NS_ERROR_FAILURE;

}

nsresult nsBuildLocalMessageURI(const char *baseURI, PRUint32 key, char** uri)
{
	
	if(!uri)
		return NS_ERROR_NULL_POINTER;
	// need to convert mailbox://hostname/.. to mailbox_message://hostname/..

	nsAutoString tailURI(baseURI);

	// chop off mailbox:/
	if (tailURI.Find(kMailboxRootURI) == 0)
		tailURI.Cut(0, PL_strlen(kMailboxRootURI));
  
	char *tail = tailURI.ToNewCString();

	*uri = PR_smprintf("%s%s#%d", kMailboxMessageRootURI, tail, key);
	delete[] tail;
	return NS_OK;
}

nsresult
nsGetMailboxHostName(const char *rootURI, const char *uriStr, char **hostName)
{

  if(!hostName)
	  return NS_ERROR_NULL_POINTER;

  nsresult rv;
  
  // verify that rootURI starts with "mailbox:/" or "mailbox_message:/"
  if ((PL_strcmp(rootURI, kMailboxRootURI) != 0) && 
      (PL_strcmp(rootURI, kMailboxMessageRootURI) != 0)) {
    return NS_ERROR_FAILURE;
  }

  // verify that uristr starts with rooturi
  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = nsLocalURI2Server(uriStr, getter_AddRefs(server));
  
  if (NS_FAILED(rv))
  	return rv;
  
  return server->GetHostName(hostName);
}


nsresult nsGetMailboxUserName(const char *rootURI, const char* uriStr,
                              char **userName)
{
  

  if(!userName)
	  return NS_ERROR_NULL_POINTER;

  nsresult rv;
  
  // verify that rootURI starts with "mailbox:/" or "mailbox_message:/"
  if ((PL_strcmp(rootURI, kMailboxRootURI) != 0) && 
      (PL_strcmp(rootURI, kMailboxMessageRootURI) != 0)) {
    return NS_ERROR_FAILURE;
  }

  // verify that uristr starts with rooturi
  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = nsLocalURI2Server(uriStr, getter_AddRefs(server));
  
  if (NS_FAILED(rv))
  	return rv;
  
  return server->GetUsername(userName);
}
