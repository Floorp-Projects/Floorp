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
  // FindServersByHostname is broken or we got some wierd object back
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

  nsCOMPtr<nsIMsgIncomingServer> server;
  
  nsAutoString sep;
  sep += PR_GetDirectorySeparator();

  nsAutoString sbdSep;
  rv = nsGetMailFolderSeparator(sbdSep);
  if (NS_FAILED(rv)) return rv;

  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
    return NS_ERROR_FAILURE;

  // verify that rootURI starts with "mailbox:/" or "mailbox_message:/"
  if ((strcmp(rootURI, kMailboxRootURI) != 0) && 
      (strcmp(rootURI, kMailboxMessageRootURI) != 0)) {
    pathResult = nsnull;
    return NS_ERROR_FAILURE;
  }

  // the server name is the first component of the path, so extract it out
  PRInt32 hostStart;

  hostStart = uri.Find('/');
  if (hostStart <= 0) return NS_ERROR_FAILURE;

  // skip past all //
  while (uri[hostStart]=='/') hostStart++;

  // cut mailbox://hostname/folder -> hostname/folder
  nsAutoString hostname;
  uri.Right(hostname, uri.Length() - hostStart);

  PRInt32 hostEnd = hostname.Find('/');

  // folder comes after the hostname, after the '/'
  nsAutoString folder;
  hostname.Right(folder, hostname.Length() - hostEnd - 1);

  // cut off first '/' and everything following it
  // hostname/folder -> hostname
  if (hostEnd >0) {
    hostname.Truncate(hostEnd);
  }
  
  // local mail case
  // should return a list of all local mail folders? or maybe nothing
  // at all?
  char *hostchar = hostname.ToNewCString();
  rv = nsGetMailboxRoot(hostchar, pathResult);
  printf("nsGetMailboxRoot(%s) = %s\n\tfolder = %s\n",
         hostchar, (const char*)pathResult,
         folder.ToNewCString());
  delete[] hostchar;

  if (NS_FAILED(rv)) {
    pathResult = nsnull;
    return rv;
  }
#if 0
  nsAutoString path="";
  uri.Cut(0, nsCRT::strlen(rootURI));

  PRInt32 uriLen = uri.Length();
  PRInt32 pos;
  while(uriLen > 0) {
    nsAutoString folderName;

    PRInt32 leadingPos;
    // if it's the first character then remove it.
    while ((leadingPos = uri.Find('/')) == 0) {
      uri.Cut(0, 1);
      uriLen--;
    }

    if (uriLen == 0)
      break;

    pos = uri.Find('/');
    if (pos < 0)
      pos = uriLen;


    PRInt32 leftRes = uri.Left(folderName, pos);

    NS_ASSERTION(leftRes == pos,
                 "something wrong with nsString");
	//We only want to add this after the first time around.
	if(path.Length() > 0)
	{
		path += sep;
		path += PR_GetDirectorySeparator();
	}
    // the first time around the separator is special because
    // the root mail folder doesn't end with .sbd
    sep = sbdSep;

    path += folderName;
    uri.Cut(0, pos);
    uriLen -= pos;
  }

  if(path.Length() > 0)
	  pathResult +=path;
#endif

  pathResult += folder;
  return NS_OK;
}

#if 0
static
nsresult
nsPath2LocalURI(const char* rootURI, const nsFileSpec& spec, char **uri)
{
  nsresult rv;


  nsAutoString sep;
  /* sspitzer: is this ok for mail and news? */
  rv = nsGetMailFolderSeparator(sep);
  if (NS_FAILED(rv)) return rv;

  PRUint32 sepLen = sep.Length();

  nsFileSpec root;
  // local mail case
  rv = nsGetMailboxRoot(root);
  if (NS_FAILED(rv)) return rv;

  const char *path = spec;
  nsAutoString pathStr(path);
  path = root;
  nsAutoString rootStr(path);
  
  PRInt32 pos = pathStr.Find(rootStr);
  if (pos != 0)     // if doesn't start with root path
    return NS_ERROR_FAILURE;

  nsAutoString uriStr(rootURI);

  PRUint32 rootStrLen = rootStr.Length();
  pathStr.Cut(0, rootStrLen);
  PRInt32 pathStrLen = pathStr.Length();

  char dirSep = PR_GetDirectorySeparator();
  
  while (pathStrLen > 0) {
    nsAutoString folderName;
    
    PRInt32 leadingPos;
    // if it's the first character then remove it.
    while ((leadingPos = pathStr.Find(dirSep)) == 0) {
      pathStr.Cut(0, 1);
      pathStrLen--;
    }
    if (pathStrLen == 0)
      break;

    pos = pathStr.Find(sep);
    if (pos < 0) 
      pos = pathStrLen;

    PRInt32 leftRes = pathStr.Left(folderName, pos);
    NS_ASSERTION(leftRes == pos,
                 "something wrong with nsString");

    pathStr.Cut(0, pos + sepLen);
    pathStrLen -= pos + sepLen;

    uriStr += '/';
    uriStr += folderName;
  }
  *uri = uriStr.ToNewCString();
  return NS_OK;
}
#endif

nsresult
nsLocalURI2Name(const char* rootURI, char* uriStr, nsString& name)
{
  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
    return NS_ERROR_FAILURE;
  PRInt32 pos = uri.RFind("/");
  PRInt32 length = uri.Length();
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

#ifdef DEBUG_alecf
  printf("nsParseLocalMessageURI(%s..)\n", uri);
#endif

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
  
  const char *tail = tailURI.ToNewCString();
    
	*uri = PR_smprintf("%s%s#%d", kMailboxMessageRootURI, tail, key);
  
	return NS_OK;
}
