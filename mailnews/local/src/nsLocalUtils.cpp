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


static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

// it would be really cool to:
// - cache the last hostname->path match
// - if no such server exists, behave like an old-style mailbox URL
// (i.e. return the mail.directory preference or something)
nsresult
nsGetMailboxRoot(const char *hostname, nsFileSpec &result)
{
  nsresult rv = NS_OK;

  // retrieve the AccountManager
  NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIMsgAccountManager> accountManager;
  rv = session->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) return rv;

  // find all pop hosts matching the given hostname
  nsCOMPtr<nsISupportsArray> hosts;
  rv = accountManager->FindServersByHostname(hostname,
                                             nsIPop3IncomingServer::GetIID(),
                                             getter_AddRefs(hosts));
  
  if (NS_FAILED(rv)) return rv;

  // use enumeration function to find the first Pop server
  nsISupports *serverSupports = hosts->ElementAt(0);

#ifdef DEBUG_alecf
  if (hosts->Count() <= 0)
    fprintf(stderr, "Augh, no pop server named %s?\n", hostname);
  if (!serverSupports)
    fprintf(stderr, "Huh, serverSupports returned nsnull\n");
#endif

  // if there are no pop servers, how did we get here?
  if (! serverSupports) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(serverSupports);

  // this had better be a nsIMsgIncomingServer, otherwise
  // FindServersByHostname is broken or we got some weird object back
  PR_ASSERT(server);
  
  // now ask the server what it's root is
  char *localPath;
  rv = server->GetLocalPath(&localPath);
  if (NS_SUCCEEDED(rv)) 
    result = localPath;

  return rv;
}

// given rootURI and rootURI##folder, return on-disk path of folder
nsresult
nsLocalURI2Path(const char* rootURI, const char* uriStr,
                nsFileSpec& pathResult)
{
  nsresult rv;
  nsAutoString sbdSep;
  rv = nsGetMailFolderSeparator(sbdSep);
  if (NS_FAILED(rv)) return rv;

  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
    return NS_ERROR_FAILURE;

  // verify that rootURI starts with "mailbox:/" or "mailbox_message:/"
  if ((PL_strcmp(rootURI, kMailboxRootURI) != 0) && 
      (PL_strcmp(rootURI, kMailboxMessageRootURI) != 0)) {
    pathResult = nsnull;
    return NS_ERROR_FAILURE;
  }

  // start parsing the uriStr
  const char* curPos = uriStr;
  
  // skip past schema 
  while (*curPos != ':') curPos++;
  curPos++;
  while (*curPos == '/') curPos++;

  char *slashPos = PL_strchr(curPos, '/');
  int length;

  // if there are no more /'s then we just copy the rest of the string
  if (slashPos)
    length = (slashPos - curPos) + 1;
  else
    length = PL_strlen(curPos) + 1;

  char* hostname = new char[length];
  PL_strncpyz(hostname, curPos, length);

  // begin pathResult with the mailbox root
  rv = nsGetMailboxRoot(hostname, pathResult);
  delete[] hostname;

  if (slashPos) {
    // advance past hostname
    curPos=slashPos;
    curPos++;

    // for each token in between the /'s, put a .sbd on the end and
    // append to the path
    char *newStr=nsnull;
    char *temp = PL_strdup(curPos);
    char *token = nsCRT::strtok(temp, "/", &newStr);
    while (token) {
      nsAutoString dir(token);
      
      // look for next token
      token = nsCRT::strtok(newStr, "/", &newStr);
      
      // check if we're the last entry
      if (token)
        dir += sbdSep;            // no, we're not, so append .sbd
      
      pathResult += dir;
    }
    PL_strfree(temp);
  }
  return NS_OK;
}

nsresult
nsLocalURI2Name(const char* rootURI, char* uriStr, nsString& name)
{
  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
    return NS_ERROR_FAILURE;
  PRInt32 pos = uri.RFind('/');
  PRInt32 length = uri.Length();

  // if the last character is a /, chop it off and search again
  if (pos == (length-1)) {
    uri.Truncate(length-1);     // chop the last character
    length--;
    pos = uri.RFind('/');
  }

  PRInt32 count = length - (pos + 1);
  return uri.Right(name, count);
}

/* parses LocalMessageURI
 * mailbox://folder1/folder2#123
 *
 * puts folder path in folderURI
 * message key number in key
 */
nsresult nsParseLocalMessageURI(const char* uri,
                                nsString& folderURI,
                                PRUint32 *key)
{
	if(!key)
		return NS_ERROR_NULL_POINTER;

	nsAutoString uriStr = uri;
	PRInt32 keySeparator = uriStr.Find('#');
	if(keySeparator != -1)
	{
		nsAutoString folderPath;
		uriStr.Left(folderURI, keySeparator);

		nsAutoString keyStr;
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
