/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "nsLocalUtils.h"
#include "nsIServiceManager.h"
#include "prsystem.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsEscape.h"
#include "nsIFileSpec.h"

// stuff for temporary root folder hack
#include "nsIMsgAccountManager.h"
#include "nsIMsgIncomingServer.h"
#include "nsIPop3IncomingServer.h"
#include "nsINoIncomingServer.h"
#include "nsMsgBaseCID.h"

#include "nsMsgUtils.h"


// it would be really cool to:
// - cache the last hostname->path match
// - if no such server exists, behave like an old-style mailbox URL
// (i.e. return the mail.directory preference or something)
static nsresult
nsGetMailboxServer(const char *username, const char *hostname, nsIMsgIncomingServer** aResult)
{
  nsresult rv = NS_OK;

  nsUnescape(NS_CONST_CAST(char*,username));
  nsUnescape(NS_CONST_CAST(char*,hostname));
  // retrieve the AccountManager
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
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

#ifdef HAVE_MOVEMAIL
  // find all movemail "servers" matching the given hostname
  nsCOMPtr<nsIMsgIncomingServer> movemail_server;
  rv = accountManager->FindServer(username,
                                  hostname,
                                  "movemail",
                                  getter_AddRefs(movemail_server));
  if (NS_SUCCEEDED(rv)) {
	 *aResult = movemail_server;
	  NS_ADDREF(*aResult);
	  return rv;
  }
#endif /* HAVE_MOVEMAIL */

  // if that fails, look for the pop hosts matching the given hostname
  nsCOMPtr<nsIMsgIncomingServer> server;
  if (NS_FAILED(rv)) 
  {
	  rv = accountManager->FindServer(username,
                                    hostname,
                                    "pop3",
                                    getter_AddRefs(server));

    // if we can't find a pop server, maybe it's a local message 
    // in an imap hiearchy. look for an imap server.
    if (NS_FAILED(rv)) 
    {
	  rv = accountManager->FindServer(username,
                                    hostname,
                                    "imap",
                                    getter_AddRefs(server));
    }
  }
	if (NS_SUCCEEDED(rv)) 
  {
		*aResult = server;
		NS_ADDREF(*aResult);
		return rv;
	}

  // if you fail after looking at all "pop3", "movemail" and "none" servers, you fail.
  return rv;
}

static nsresult
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
  nsCAutoString uri(uriStr);
  if (uri.Find(rootURI) != 0)
    return NS_ERROR_FAILURE;


  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = nsLocalURI2Server(uriStr, getter_AddRefs(server));

  if (NS_FAILED(rv))
  	return rv;
  
  // now ask the server what it's root is
  // and begin pathResult with the mailbox root
  nsCOMPtr<nsIFileSpec> localPath;
  rv = server->GetLocalPath(getter_AddRefs(localPath));
  if (NS_SUCCEEDED(rv)) 
    localPath->GetFileSpec(&pathResult);

  const char *curPos = uriStr + PL_strlen(rootURI);
  if (curPos) {
    
    // advance past hostname
    while ((*curPos)=='/') curPos++;
    while (*curPos && (*curPos)!='/') curPos++;

    // get the seperator
    nsAutoString sbdSep;
    rv = nsGetMailFolderSeparator(sbdSep);
    
       
    nsCAutoString newPath("");
    char *unescaped = nsCRT::strdup(curPos);  
    // Unescape folder name
    if (unescaped) {
      nsUnescape(unescaped);
      NS_MsgCreatePathStringFromFolderURI(unescaped, newPath);
      PR_Free(unescaped);
    }
    else
      NS_MsgCreatePathStringFromFolderURI(curPos, newPath);

    pathResult+=newPath.get();
  }

  return NS_OK;
}

/* parses LocalMessageURI
 * mailbox_message://folder1/folder2#123?header=none or
 * mailbox_message://folder1/folder2#1234&part=1.2
 *
 * puts folder URI in folderURI (mailbox://folder1/folder2)
 * message key number in key
 */
nsresult nsParseLocalMessageURI(const char* uri,
                                nsCString& folderURI,
                                PRUint32 *key)
{
	if(!key)
		return NS_ERROR_NULL_POINTER;

	nsCAutoString uriStr(uri);
	PRInt32 keySeparator = uriStr.FindChar('#');
	if(keySeparator != -1)
	{
    PRInt32 keyEndSeparator = uriStr.FindCharInSet("?&", 
                                                   keySeparator); 
		nsAutoString folderPath;
		uriStr.Left(folderURI, keySeparator);
        folderURI.Cut(7, 8);    // cut out the _message part of mailbox_message:

		nsCAutoString keyStr;
    if (keyEndSeparator != -1)
        uriStr.Mid(keyStr, keySeparator+1, 
                   keyEndSeparator-(keySeparator+1));
    else
        uriStr.Right(keyStr, uriStr.Length() - (keySeparator + 1));
		PRInt32 errorCode;
		*key = keyStr.ToInteger(&errorCode);

		return errorCode;
	}
	return NS_ERROR_FAILURE;

}

nsresult nsBuildLocalMessageURI(const char *baseURI, PRUint32 key, nsCString& uri)
{
	
	// need to convert mailbox://hostname/.. to mailbox_message://hostname/..

	uri.Append(baseURI);
	uri.Append('#');
	uri.AppendInt(key);
	return NS_OK;
}

nsresult nsCreateLocalBaseMessageURI(const char *baseURI, char **baseMessageURI)
{
	if(!baseMessageURI)
		return NS_ERROR_NULL_POINTER;

	nsCAutoString tailURI(baseURI);

	// chop off mailbox:/
	if (tailURI.Find(kMailboxRootURI) == 0)
		tailURI.Cut(0, PL_strlen(kMailboxRootURI));
	
	nsCAutoString baseURIStr(kMailboxMessageRootURI);
	baseURIStr += tailURI;

	*baseMessageURI = ToNewCString(baseURIStr);
	if(!*baseMessageURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}
